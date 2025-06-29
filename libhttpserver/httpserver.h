#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <uv.h>

// Forward declarations
struct http_server_s;
struct http_request_s;
struct http_response_s;
struct http_server_config_s;

// Request handler callback type
typedef void (*http_request_handler_t)(struct http_request_s* request);

// HTTP server structure (opaque)
typedef struct http_server_s http_server_t;

// HTTP request structure (opaque)
typedef struct http_request_s http_request_t;

// HTTP response structure (opaque)
typedef struct http_response_s http_response_t;

// HTTP server configuration structure
typedef struct http_server_config_s {
    const char* host;
    int port;
    http_request_handler_t handler;
    int tls_enabled;
    const char* cert_file;
    const char* key_file;
} http_server_config_t;

// Server management functions
http_server_t* http_server_create(const http_server_config_t* config);
int http_server_listen(http_server_t* server);
void http_server_destroy(http_server_t* server);
uv_loop_t* http_server_loop(http_server_t* server);


// Request functions
const char* http_request_method(http_request_t* request);
const char* http_request_target(http_request_t* request);
const char* http_request_header(http_request_t* request, const char* name);
const char* http_request_body(http_request_t* request);
size_t http_request_body_length(http_request_t* request);
void* http_request_get_user_data(http_request_t* request);
void http_request_set_user_data(http_request_t* request, void* user_data);


// Response functions
http_response_t* http_response_init(void);
void http_response_status(http_response_t* response, int status);
void http_response_header(http_response_t* response, const char* name, const char* value);
void http_response_body(http_response_t* response, const char* body, size_t length);
int http_respond(http_request_t* request, http_response_t* response);
void http_response_destroy(http_response_t* response);

#ifdef __cplusplus
}
#endif

#endif // HTTPSERVER_H


#ifdef HTTPSERVER_IMPL

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include "llhttp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_HEADERS 64

// Internal structures
struct http_server_s {
    uv_tcp_t tcp;
    uv_loop_t* loop;
    SSL_CTX* ssl_ctx;
    http_request_handler_t handler;
    const char* host;
    int port;
    int tls_enabled;
};

typedef struct {
    uv_tcp_t tcp;
    SSL* ssl;
    BIO* read_bio;
    BIO* write_bio;
    llhttp_t parser;
    llhttp_settings_t parser_settings;
    int handshake_complete;
    http_server_t* server;
    
    char* method;
    char* url;
    char* headers[MAX_HEADERS][2];
    int header_count;
    char* current_header_field;
    char* body;
    size_t body_length;
    size_t body_capacity;

    http_response_t* pending_response;
    void* user_data;
} http_connection_t;

struct http_request_s {
    http_connection_t* connection;
};

struct http_response_s {
    int status_code;
    char* headers[MAX_HEADERS][2];
    int header_count;
    char* body;
    size_t body_length;
};

// Forward declarations for internal functions
static void on_new_connection(uv_stream_t* server_stream, int status);
static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
static void on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
static void on_final_write_cb(uv_write_t* req, int status);
static void on_transient_write_cb(uv_write_t* req, int status);
static void on_close(uv_handle_t* handle);

static int on_message_begin(llhttp_t* parser);
static int on_url(llhttp_t* parser, const char* at, size_t length);
static int on_header_field(llhttp_t* parser, const char* at, size_t length);
static int on_header_value(llhttp_t* parser, const char* at, size_t length);
static int on_headers_complete(llhttp_t* parser);
static int on_body(llhttp_t* parser, const char* at, size_t length);
static int on_message_complete(llhttp_t* parser);

static void flush_write_bio(http_connection_t* conn, uv_write_cb cb);
static int handle_tls_handshake(http_connection_t* conn);

// --- Function Implementations ---

const char* http_request_method(http_request_t* request) { return request->connection->method; }
const char* http_request_target(http_request_t* request) { return request->connection->url; }
const char* http_request_body(http_request_t* request) { return request->connection->body; }
size_t http_request_body_length(http_request_t* request) { return request->connection->body_length; }
void* http_request_get_user_data(http_request_t* request) { return request->connection->user_data; }
void http_request_set_user_data(http_request_t* request, void* user_data) { request->connection->user_data = user_data; }

const char* http_request_header(http_request_t* request, const char* name) {
    for (int i = 0; i < request->connection->header_count; i++) {
        if (strcasecmp(request->connection->headers[i][0], name) == 0) {
            return request->connection->headers[i][1];
        }
    }
    return NULL;
}

http_response_t* http_response_init(void) {
    http_response_t* response = (http_response_t*)calloc(1, sizeof(http_response_t));
    response->status_code = 200;
    return response;
}

void http_response_status(http_response_t* response, int status) { response->status_code = status; }

void http_response_header(http_response_t* response, const char* name, const char* value) {
    if (response->header_count < MAX_HEADERS) {
        response->headers[response->header_count][0] = strdup(name);
        response->headers[response->header_count][1] = strdup(value);
        response->header_count++;
    }
}

void http_response_body(http_response_t* response, const char* body, size_t length) {
    response->body = (char*)malloc(length);
    memcpy(response->body, body, length);
    response->body_length = length;
}

void http_response_destroy(http_response_t* response) {
    for (int i = 0; i < response->header_count; i++) {
        free(response->headers[i][0]);
        free(response->headers[i][1]);
    }
    if (response->body) free(response->body);
    free(response);
}

int http_respond(http_request_t* request, http_response_t* response) {
    http_connection_t* conn = request->connection;
    
    char status_line[128];
    snprintf(status_line, sizeof(status_line),