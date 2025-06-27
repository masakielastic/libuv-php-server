#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_smart_string.h"
#include "php_aurora.h"

#include <uv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <time.h>
#include <sys/stat.h>
#include "main/php_output.h"
#include "main/php_streams.h"
#include "Zend/zend_compile.h"
#include "Zend/zend_execute.h"
#include "llhttp.h"

/* Module globals */
ZEND_DECLARE_MODULE_GLOBALS(aurora)

/* Argument info */
ZEND_BEGIN_ARG_INFO_EX(arginfo_aurora_serve, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, options, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aurora_register_handler, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, handler, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aurora_stop, 0, 0, 0)
ZEND_END_ARG_INFO()

/* Function entries */
const zend_function_entry aurora_functions[] = {
    PHP_FE(aurora_serve, arginfo_aurora_serve)
    PHP_FE(aurora_register_handler, arginfo_aurora_register_handler)
    PHP_FE(aurora_stop, arginfo_aurora_stop)
    PHP_FE_END
};

/* Module entry */
zend_module_entry aurora_module_entry = {
    STANDARD_MODULE_HEADER,
    "aurora",
    aurora_functions,
    PHP_MINIT(aurora),
    PHP_MSHUTDOWN(aurora),
    PHP_RINIT(aurora),
    PHP_RSHUTDOWN(aurora),
    PHP_MINFO(aurora),
    PHP_AURORA_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_AURORA
ZEND_GET_MODULE(aurora)
#endif

/* Forward declarations for TLS functions */
static int aurora_handle_tls_handshake(aurora_client_t* client);
static void aurora_flush_tls_data(aurora_client_t* client);

/* HTTP parser callbacks */
static int aurora_on_message_begin(llhttp_t* parser) {
    aurora_client_t* client = (aurora_client_t*)parser->data;
    client->headers_complete = 0;
    client->message_complete = 0;
    return 0;
}

static int aurora_on_url(llhttp_t* parser, const char* at, size_t length) {
    aurora_client_t* client = (aurora_client_t*)parser->data;
    if (client->url) {
        zend_string_release(client->url);
    }
    client->url = zend_string_init(at, length, 0);
    return 0;
}

static int aurora_on_header_field(llhttp_t* parser, const char* at, size_t length) {
    aurora_client_t* client = (aurora_client_t*)parser->data;
    if (client->current_header_field) {
        zend_string_release(client->current_header_field);
    }
    client->current_header_field = zend_string_init(at, length, 0);
    return 0;
}

static int aurora_on_header_value(llhttp_t* parser, const char* at, size_t length) {
    aurora_client_t* client = (aurora_client_t*)parser->data;
    if (client->current_header_field) {
        zend_string* header_value = zend_string_init(at, length, 0);
        zval header_zval;
        ZVAL_STR(&header_zval, header_value);
        zend_hash_update(client->headers, client->current_header_field, &header_zval);
        zend_string_release(client->current_header_field);
        client->current_header_field = NULL;
    }
    return 0;
}

static int aurora_on_headers_complete(llhttp_t* parser) {
    aurora_client_t* client = (aurora_client_t*)parser->data;
    client->headers_complete = 1;
    
    // Get method
    if (client->method) {
        zend_string_release(client->method);
    }
    const char* method_str = llhttp_method_name(llhttp_get_method(parser));
    client->method = zend_string_init(method_str, strlen(method_str), 0);
    
    // Get HTTP version
    if (client->http_version) {
        zend_string_release(client->http_version);
    }
    char version_buf[16];
    snprintf(version_buf, sizeof(version_buf), "%d.%d", 
             parser->http_major, parser->http_minor);
    client->http_version = zend_string_init(version_buf, strlen(version_buf), 0);
    
    return 0;
}

static int aurora_on_body(llhttp_t* parser, const char* at, size_t length) {
    aurora_client_t* client = (aurora_client_t*)parser->data;
    if (client->body) {
        // Append to existing body
        size_t old_len = ZSTR_LEN(client->body);
        client->body = zend_string_extend(client->body, old_len + length, 0);
        memcpy(ZSTR_VAL(client->body) + old_len, at, length);
        ZSTR_VAL(client->body)[old_len + length] = '\0';
    } else {
        client->body = zend_string_init(at, length, 0);
    }
    return 0;
}

// Forward declaration moved above aurora_on_message_complete
static int aurora_on_message_complete_direct(aurora_client_t* client);
static void aurora_on_write(uv_write_t* req, int status);

static int aurora_on_message_complete(llhttp_t* parser) {
    aurora_client_t* client = (aurora_client_t*)parser->data;
    client->message_complete = 1;
    
    // Process the complete HTTP request
    aurora_on_message_complete_direct(client);
    return 0;
}

// Response callback function for PHP handlers
void aurora_response_callback(aurora_client_t* client, int status, HashTable* headers, zend_string* body) {
    client->response_status = status;
    if (client->response_body) {
        zend_string_release(client->response_body);
    }
    client->response_body = body ? zend_string_copy(body) : zend_string_init("Hello, World!", 13, 0);
    
    if (headers && client->response_headers) {
        zend_hash_destroy(client->response_headers);
        FREE_HASHTABLE(client->response_headers);
    }
    if (headers) {
        ALLOC_HASHTABLE(client->response_headers);
        zend_hash_init(client->response_headers, zend_hash_num_elements(headers), NULL, ZVAL_PTR_DTOR, 0);
        zend_hash_copy(client->response_headers, headers, (copy_ctor_func_t)zval_add_ref);
    }
    
    client->response_sent = 1;
    
    // Build HTTP response using simple string concatenation
    char response_header[4096];
    snprintf(response_header, sizeof(response_header), 
        "HTTP/1.1 %d OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
        status, ZSTR_LEN(client->response_body));
    
    // Calculate total response size
    size_t header_len = strlen(response_header);
    size_t total_len = header_len + ZSTR_LEN(client->response_body);
    
    // Allocate response buffer
    char* response_buf = emalloc(total_len);
    memcpy(response_buf, response_header, header_len);
    memcpy(response_buf + header_len, ZSTR_VAL(client->response_body), ZSTR_LEN(client->response_body));
    
    // Send response
    if (client->ssl && client->handshake_complete) {
        // Send via SSL
        int bytes_written = SSL_write(client->ssl, response_buf, total_len);
        if (bytes_written > 0) {
            printf("Sent HTTPS response (%d bytes)\n", bytes_written);
            
            // SSL shutdown
            SSL_shutdown(client->ssl);
            client->shutdown_sent = 1;
            
            aurora_flush_tls_data(client);
        } else {
            int err = SSL_get_error(client->ssl, bytes_written);
            if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                php_error_docref(NULL, E_WARNING, "SSL_write failed: %d", err);
            }
        }
        efree(response_buf);
        
        // Close connection after SSL response
        uv_close((uv_handle_t*)client, (uv_close_cb)aurora_client_free);
    } else {
        // Send via plain TCP
        uv_write_t* req = emalloc(sizeof(uv_write_t));
        req->data = response_buf; // Store buffer pointer for cleanup
        uv_buf_t buf = uv_buf_init(response_buf, total_len);
        uv_write(req, (uv_stream_t*)client, &buf, 1, aurora_on_write);
    }
}

// PHP function wrapper for response callback
static void aurora_php_respond(INTERNAL_FUNCTION_PARAMETERS) {
    zend_long status;
    HashTable* headers = NULL;
    zend_string* body;
    aurora_client_t* client;
    
    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_LONG(status)
        Z_PARAM_ARRAY_HT(headers)
        Z_PARAM_STR(body)
    ZEND_PARSE_PARAMETERS_END();
    
    // Get client from closure context (simplified for now)
    // In a full implementation, this would be passed through closure binding
    
    // For now, just return - the actual implementation needs closure context
}

static int aurora_on_message_complete_direct(aurora_client_t* client) {
    // Populate superglobals and handle request
    aurora_populate_superglobals(client);
    
    const char* url = client->url ? ZSTR_VAL(client->url) : "/";
    const char* method = client->method ? ZSTR_VAL(client->method) : "GET";
    
    // Resolve file path
    char* file_path = aurora_resolve_file_path(client->server, url);
    if (!file_path) {
        // Send 404
        zend_string* body = zend_string_init("<h1>404 Not Found</h1><p>The requested file was not found.</p>", 64, 0);
        aurora_response_callback(client, 404, NULL, body);
        zend_string_release(body);
        return 0;
    }
    
    // Check if file exists
    struct stat st;
    if (stat(file_path, &st) != 0) {
        efree(file_path);
        zend_string* body = zend_string_init("<h1>404 Not Found</h1><p>File not found.</p>", 46, 0);
        aurora_response_callback(client, 404, NULL, body);
        zend_string_release(body);
        return 0;
    }
    
    // Check file extension
    char* ext = strrchr(file_path, '.');
    if (ext && strcmp(ext, ".php") == 0) {
        // Execute PHP file
        zend_string* output = aurora_execute_php_file(client, file_path);
        aurora_response_callback(client, 200, NULL, output);
        if (output) zend_string_release(output);
    } else {
        // Serve static file
        const char* mime_type;
        zend_string* content = aurora_serve_static_file(file_path, &mime_type);
        
        // TODO: Set proper mime type in headers
        aurora_response_callback(client, 200, NULL, content);
        if (content) zend_string_release(content);
    }
    
    efree(file_path);
    return 0;
}

/* Memory allocation callback */
static void aurora_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = emalloc(suggested_size);
    buf->len = suggested_size;
}

/* Write callback */
static void aurora_on_write(uv_write_t* req, int status) {
    aurora_client_t* client = (aurora_client_t*)req->handle;
    
    // Free the response buffer if allocated
    if (req->data) {
        efree(req->data);
    }
    efree(req);
    
    if (status) {
        php_error_docref(NULL, E_WARNING, "Write error: %s", uv_strerror(status));
    }
    
    // Close connection after response
    uv_close((uv_handle_t*)client, (uv_close_cb)aurora_client_free);
}

/* Read callback */
static void aurora_on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    aurora_client_t* client = (aurora_client_t*)stream;
    
    if (nread < 0) {
        if (nread != UV_EOF) {
            php_error_docref(NULL, E_WARNING, "Read error: %s", uv_err_name(nread));
        }
        uv_close((uv_handle_t*)client, (uv_close_cb)aurora_client_free);
        goto cleanup;
    }
    
    if (nread == 0) {
        goto cleanup;
    }
    
    // Handle SSL/TLS data
    if (client->ssl) {
        // Write received data to SSL read BIO
        BIO_write(client->read_bio, buf->base, nread);
        
        // Handle handshake if not complete
        if (!client->handshake_complete) {
            int hs_result = aurora_handle_tls_handshake(client);
            if (hs_result < 0) {
                uv_close((uv_handle_t*)client, (uv_close_cb)aurora_client_free);
                goto cleanup;
            }
            aurora_flush_tls_data(client);
            if (!client->handshake_complete) {
                goto cleanup;
            }
        }
        
        // Read decrypted HTTP data
        if (client->handshake_complete && !client->shutdown_sent) {
            char http_buf[4096];
            int bytes = SSL_read(client->ssl, http_buf, sizeof(http_buf) - 1);
            if (bytes > 0) {
                http_buf[bytes] = '\0';
                printf("Received HTTPS request (first 100 chars): %.100s\n", http_buf);
                
                // Parse decrypted HTTP data with llhttp
                enum llhttp_errno err = llhttp_execute(&client->parser, http_buf, bytes);
                if (err != HPE_OK) {
                    php_error_docref(NULL, E_WARNING, "HTTP parse error: %s", llhttp_errno_name(err));
                    uv_close((uv_handle_t*)client, (uv_close_cb)aurora_client_free);
                    goto cleanup;
                }
            } else {
                int err = SSL_get_error(client->ssl, bytes);
                if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                    if (err != SSL_ERROR_ZERO_RETURN) {
                        php_error_docref(NULL, E_WARNING, "SSL_read error: %d", err);
                    }
                    uv_close((uv_handle_t*)client, (uv_close_cb)aurora_client_free);
                    goto cleanup;
                }
            }
            aurora_flush_tls_data(client);
        }
    } else {
        // Plain HTTP processing with llhttp
        enum llhttp_errno err = llhttp_execute(&client->parser, buf->base, nread);
        if (err != HPE_OK) {
            php_error_docref(NULL, E_WARNING, "HTTP parse error: %s", llhttp_errno_name(err));
            uv_close((uv_handle_t*)client, (uv_close_cb)aurora_client_free);
            goto cleanup;
        }
    }

cleanup:
    if (buf->base) {
        efree(buf->base);
    }
}

/* New connection callback */
static void aurora_on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        php_error_docref(NULL, E_WARNING, "New connection error: %s", uv_strerror(status));
        return;
    }

    aurora_server_t* aurora_server = (aurora_server_t*)server->data;
    aurora_client_t* client = aurora_client_create(aurora_server);
    
    if (uv_accept(server, (uv_stream_t*)client) == 0) {
        // Initialize SSL if enabled
        if (aurora_server->ssl_enabled && aurora_server->ssl_ctx) {
            client->ssl = SSL_new(aurora_server->ssl_ctx);
            if (!client->ssl) {
                php_error_docref(NULL, E_WARNING, "Failed to create SSL object");
                aurora_client_free(client);
                return;
            }
            
            client->read_bio = BIO_new(BIO_s_mem());
            client->write_bio = BIO_new(BIO_s_mem());
            
            if (!client->read_bio || !client->write_bio) {
                php_error_docref(NULL, E_WARNING, "Failed to create BIO objects");
                aurora_client_free(client);
                return;
            }
            
            SSL_set_bio(client->ssl, client->read_bio, client->write_bio);
            SSL_set_accept_state(client->ssl);
            
            printf("New HTTPS connection accepted, starting TLS handshake\n");
        } else {
            printf("New HTTP connection accepted\n");
        }
        
        uv_read_start((uv_stream_t*)client, aurora_alloc_buffer, aurora_on_read);
    } else {
        php_error_docref(NULL, E_WARNING, "Failed to accept connection");
        aurora_client_free(client);
    }
}

/* Client management functions */
aurora_client_t* aurora_client_create(aurora_server_t* server) {
    aurora_client_t* client = ecalloc(1, sizeof(aurora_client_t));
    
    uv_tcp_init(server->loop, &client->tcp);
    client->tcp.data = client;
    client->server = server;
    
    // Initialize headers hash table
    ALLOC_HASHTABLE(client->headers);
    zend_hash_init(client->headers, 0, NULL, ZVAL_PTR_DTOR, 0);
    
    // Initialize llhttp parser
    llhttp_settings_init(&client->parser_settings);
    client->parser_settings.on_message_begin = aurora_on_message_begin;
    client->parser_settings.on_url = aurora_on_url;
    client->parser_settings.on_header_field = aurora_on_header_field;
    client->parser_settings.on_header_value = aurora_on_header_value;
    client->parser_settings.on_headers_complete = aurora_on_headers_complete;
    client->parser_settings.on_body = aurora_on_body;
    client->parser_settings.on_message_complete = aurora_on_message_complete;
    
    llhttp_init(&client->parser, HTTP_REQUEST, &client->parser_settings);
    client->parser.data = client;
    
    // Initialize response fields
    client->response_status = 200;
    client->response_body = NULL;
    client->response_headers = NULL;
    client->response_sent = 0;
    client->message_complete = 0;
    
    return client;
}

void aurora_client_free(aurora_client_t* client) {
    if (client->method) zend_string_release(client->method);
    if (client->url) zend_string_release(client->url);
    if (client->http_version) zend_string_release(client->http_version);
    if (client->body) zend_string_release(client->body);
    if (client->current_header_field) zend_string_release(client->current_header_field);
    if (client->response_body) zend_string_release(client->response_body);
    if (client->headers) {
        zend_hash_destroy(client->headers);
        FREE_HASHTABLE(client->headers);
    }
    if (client->response_headers) {
        zend_hash_destroy(client->response_headers);
        FREE_HASHTABLE(client->response_headers);
    }
    if (client->ssl) SSL_free(client->ssl);
    efree(client);
}

// Simple HTTP parser (temporary replacement for llhttp)
int aurora_parse_http_request(aurora_client_t* client, const char* data, size_t len) {
    // Simple parsing - look for first line: METHOD URL HTTP/1.1
    char* line_end = strstr(data, "\r\n");
    if (!line_end) {
        return 0; // Need more data
    }
    
    // Extract first line
    size_t line_len = line_end - data;
    char first_line[512];
    if (line_len >= sizeof(first_line)) line_len = sizeof(first_line) - 1;
    memcpy(first_line, data, line_len);
    first_line[line_len] = '\0';
    
    // Parse METHOD URL HTTP/1.1
    char method[16], url[256], version[16];
    if (sscanf(first_line, "%15s %255s %15s", method, url, version) == 3) {
        if (client->method) zend_string_release(client->method);
        if (client->url) zend_string_release(client->url);
        if (client->http_version) zend_string_release(client->http_version);
        
        client->method = zend_string_init(method, strlen(method), 0);
        client->url = zend_string_init(url, strlen(url), 0);
        client->http_version = zend_string_init(version, strlen(version), 0);
        
        // Simulate message complete
        client->message_complete = 1;
        
        // Call our message complete handler
        return 0;
    }
    
    return -1;
}

void aurora_populate_superglobals(aurora_client_t* client) {
    zval server_array;
    
    // Initialize $_SERVER
    array_init(&server_array);
    
    if (client->method) {
        add_assoc_str(&server_array, "REQUEST_METHOD", zend_string_copy(client->method));
    }
    if (client->url) {
        add_assoc_str(&server_array, "REQUEST_URI", zend_string_copy(client->url));
        
        // Parse query string
        char* query_pos = strchr(ZSTR_VAL(client->url), '?');
        if (query_pos) {
            zend_string* query_string = zend_string_init(query_pos + 1, strlen(query_pos + 1), 0);
            add_assoc_str(&server_array, "QUERY_STRING", query_string);
            
            // Set PATH_INFO (URL without query string)
            size_t path_len = query_pos - ZSTR_VAL(client->url);
            zend_string* path_info = zend_string_init(ZSTR_VAL(client->url), path_len, 0);
            add_assoc_str(&server_array, "PATH_INFO", path_info);
        } else {
            add_assoc_str(&server_array, "QUERY_STRING", zend_string_init("", 0, 0));
            add_assoc_str(&server_array, "PATH_INFO", zend_string_copy(client->url));
        }
    }
    
    if (client->http_version) {
        add_assoc_str(&server_array, "SERVER_PROTOCOL", zend_string_copy(client->http_version));
    }
    
    // Add server info
    add_assoc_string(&server_array, "SERVER_NAME", "localhost");
    add_assoc_long(&server_array, "SERVER_PORT", client->server->port);
    add_assoc_string(&server_array, "SERVER_SOFTWARE", "Aurora/0.1.0");
    add_assoc_string(&server_array, "SCRIPT_NAME", "/");
    add_assoc_string(&server_array, "DOCUMENT_ROOT", client->server->docroot ? ZSTR_VAL(client->server->docroot) : ".");
    
    // Set $_SERVER superglobal
    zend_hash_str_update(&EG(symbol_table), "_SERVER", sizeof("_SERVER") - 1, &server_array);
}

void aurora_send_response(aurora_client_t* client, int status, HashTable* headers, const char* body, size_t body_len) {
    uv_write_t* req = emalloc(sizeof(uv_write_t));
    uv_buf_t buf = uv_buf_init((char*)body, body_len);
    uv_write(req, (uv_stream_t*)client, &buf, 1, aurora_on_write);
}

/* PHP Functions */
PHP_FUNCTION(aurora_serve) {
    HashTable* options = NULL;
    
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(options)
    ZEND_PARSE_PARAMETERS_END();
    
    if (AURORA_G(is_running)) {
        php_error_docref(NULL, E_WARNING, "Aurora server is already running");
        RETURN_FALSE;
    }
    
    // Create server
    aurora_server_t* server = ecalloc(1, sizeof(aurora_server_t));
    server->loop = uv_default_loop();
    server->bind_addr = zend_string_init("0.0.0.0", 7, 0);
    server->port = 8080;
    server->ssl_enabled = 0;
    
    // Parse options
    if (options) {
        zval* val;
        if ((val = zend_hash_str_find(options, "port", 4)) != NULL) {
            server->port = (int)zval_get_long(val);
        }
        if ((val = zend_hash_str_find(options, "host", 4)) != NULL) {
            zend_string_release(server->bind_addr);
            server->bind_addr = zval_get_string(val);
        }
        if ((val = zend_hash_str_find(options, "docroot", 7)) != NULL) {
            server->docroot = zval_get_string(val);
        }
        
        // TLS options
        zval* tls_cert = zend_hash_str_find(options, "tls_cert", 8);
        zval* tls_key = zend_hash_str_find(options, "tls_key", 7);
        
        if (tls_cert && tls_key) {
            zend_string* cert_str = zval_get_string(tls_cert);
            zend_string* key_str = zval_get_string(tls_key);
            
            if (aurora_init_ssl(server, ZSTR_VAL(cert_str), ZSTR_VAL(key_str))) {
                printf("SSL/TLS enabled with certificate: %s\n", ZSTR_VAL(cert_str));
            } else {
                php_error_docref(NULL, E_WARNING, "Failed to initialize SSL/TLS");
                zend_string_release(cert_str);
                zend_string_release(key_str);
                efree(server);
                RETURN_FALSE;
            }
            
            zend_string_release(cert_str);
            zend_string_release(key_str);
        }
    }
    
    // Initialize server
    uv_tcp_init(server->loop, &server->tcp_server);
    server->tcp_server.data = server;
    
    struct sockaddr_in addr;
    uv_ip4_addr(ZSTR_VAL(server->bind_addr), server->port, &addr);
    
    int r = uv_tcp_bind(&server->tcp_server, (const struct sockaddr*)&addr, 0);
    if (r) {
        php_error_docref(NULL, E_ERROR, "Bind error: %s", uv_strerror(r));
        RETURN_FALSE;
    }
    
    r = uv_listen((uv_stream_t*)&server->tcp_server, 128, aurora_on_new_connection);
    if (r) {
        php_error_docref(NULL, E_ERROR, "Listen error: %s", uv_strerror(r));
        RETURN_FALSE;
    }
    
    AURORA_G(server) = server;
    AURORA_G(is_running) = 1;
    
    php_printf("Aurora server listening on %s:%d\n", ZSTR_VAL(server->bind_addr), server->port);
    
    // Run event loop
    uv_run(server->loop, UV_RUN_DEFAULT);
    
    RETURN_TRUE;
}

PHP_FUNCTION(aurora_register_handler) {
    zval* handler;
    
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(handler)
    ZEND_PARSE_PARAMETERS_END();
    
    if (!zend_is_callable(handler, 0, NULL)) {
        php_error_docref(NULL, E_WARNING, "Handler must be callable");
        RETURN_FALSE;
    }
    
    if (AURORA_G(server)) {
        ZVAL_COPY(&AURORA_G(server)->handler_callback, handler);
        RETURN_TRUE;
    }
    
    RETURN_FALSE;
}

PHP_FUNCTION(aurora_stop) {
    if (AURORA_G(is_running) && AURORA_G(server)) {
        uv_stop(AURORA_G(server)->loop);
        AURORA_G(is_running) = 0;
        RETURN_TRUE;
    }
    RETURN_FALSE;
}

/* Module lifecycle functions */
PHP_MINIT_FUNCTION(aurora) {
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(aurora) {
    EVP_cleanup();
    return SUCCESS;
}

PHP_RINIT_FUNCTION(aurora) {
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(aurora) {
    if (AURORA_G(server)) {
        if (AURORA_G(server)->docroot) zend_string_release(AURORA_G(server)->docroot);
        if (AURORA_G(server)->bind_addr) zend_string_release(AURORA_G(server)->bind_addr);
        efree(AURORA_G(server));
        AURORA_G(server) = NULL;
    }
    AURORA_G(is_running) = 0;
    return SUCCESS;
}

PHP_MINFO_FUNCTION(aurora) {
    php_info_print_table_start();
    php_info_print_table_header(2, "Aurora HTTP Server", "enabled");
    php_info_print_table_row(2, "Version", PHP_AURORA_VERSION);
    php_info_print_table_row(2, "libuv Support", "enabled");
    php_info_print_table_row(2, "OpenSSL Support", "enabled");
    php_info_print_table_row(2, "llhttp Support", "enabled");
    php_info_print_table_end();
}

/* SSL initialization */
int aurora_init_ssl(aurora_server_t* server, const char* cert_file, const char* key_file) {
    server->ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!server->ssl_ctx) {
        return 0;
    }
    
    SSL_CTX_set_options(server->ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    
    if (cert_file && key_file) {
        if (SSL_CTX_use_certificate_file(server->ssl_ctx, cert_file, SSL_FILETYPE_PEM) != 1) {
            return 0;
        }
        if (SSL_CTX_use_PrivateKey_file(server->ssl_ctx, key_file, SSL_FILETYPE_PEM) != 1) {
            return 0;
        }
        if (SSL_CTX_check_private_key(server->ssl_ctx) != 1) {
            return 0;
        }
    }
    
    server->ssl_enabled = 1;
    return 1;
}

void aurora_cleanup_ssl(aurora_server_t* server) {
    if (server->ssl_ctx) {
        SSL_CTX_free(server->ssl_ctx);
        server->ssl_ctx = NULL;
    }
    server->ssl_enabled = 0;
}

/* File path resolution */
char* aurora_resolve_file_path(aurora_server_t* server, const char* url) {
    const char* docroot = server->docroot ? ZSTR_VAL(server->docroot) : ".";
    size_t docroot_len = strlen(docroot);
    size_t url_len = strlen(url);
    
    // Handle root URL
    if (strcmp(url, "/") == 0) {
        url = "/index.php";
        url_len = 10;
    }
    
    // Remove query string for file path resolution
    char* query_pos = strchr(url, '?');
    if (query_pos) {
        url_len = query_pos - url;
    }
    
    // Allocate and build file path
    char* file_path = emalloc(docroot_len + url_len + 1);
    strcpy(file_path, docroot);
    strncat(file_path, url, url_len);
    file_path[docroot_len + url_len] = '\0';
    
    return file_path;
}

/* MIME type detection */
const char* aurora_get_mime_type(const char* file_path) {
    char* ext = strrchr(file_path, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    
    return "application/octet-stream";
}

/* Static file serving */
zend_string* aurora_serve_static_file(const char* file_path, const char** mime_type) {
    *mime_type = aurora_get_mime_type(file_path);
    
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return zend_string_init("", 0, 0);
    }
    
    // Read file content
    zend_string* content = zend_string_alloc(file_size, 0);
    size_t bytes_read = fread(ZSTR_VAL(content), 1, file_size, file);
    fclose(file);
    
    if (bytes_read != file_size) {
        zend_string_release(content);
        return NULL;
    }
    
    ZSTR_VAL(content)[file_size] = '\0';
    ZSTR_LEN(content) = file_size;
    
    return content;
}

/* PHP file execution - very simplified approach for demo */
zend_string* aurora_execute_php_file(aurora_client_t* client, const char* file_path) {
    // For now, return a placeholder showing that PHP file was found
    // In a full implementation, this would use zend_eval_string or similar
    
    char demo_response[1024];
    const char* url = client->url ? ZSTR_VAL(client->url) : "/";
    
    snprintf(demo_response, sizeof(demo_response),
        "<!DOCTYPE html>"
        "<html><head><title>Aurora - PHP File Execution</title></head><body>"
        "<h1>🚀 Aurora PHP File Execution</h1>"
        "<p><strong>File Path:</strong> %s</p>"
        "<p><strong>URL:</strong> %s</p>"
        "<p><strong>Status:</strong> PHP file detected and ready for execution!</p>"
        "<p><strong>Note:</strong> This demonstrates file routing and detection. "
        "Full PHP execution requires additional Zend Engine integration.</p>"
        "<hr><p><em>Aurora HTTP Server v0.1.0 - Built with libuv + PHP 8.4</em></p>"
        "</body></html>",
        file_path, url);
    
    return zend_string_init(demo_response, strlen(demo_response), 0);
}

/* TLS handshake handling */
static int aurora_handle_tls_handshake(aurora_client_t* client) {
    if (!client->ssl) return 1; // Not SSL connection
    
    int result = SSL_do_handshake(client->ssl);
    
    if (result == 1) {
        if (!client->handshake_complete) {
            client->handshake_complete = 1;
            printf("TLS handshake completed successfully\n");
        }
        return 1;
    } else {
        int ssl_error = SSL_get_error(client->ssl, result);
        if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
            // Handshake in progress
            return 0;
        } else {
            char err_buf[256];
            ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
            php_error_docref(NULL, E_WARNING, "TLS handshake failed: %s", err_buf);
            return -1;
        }
    }
}

/* Flush TLS data to socket */
static void aurora_flush_tls_data(aurora_client_t* client) {
    if (!client->ssl || !client->write_bio) return;
    
    char buffer[16384];
    int pending = BIO_pending(client->write_bio);
    
    if (pending > 0) {
        int bytes = BIO_read(client->write_bio, buffer, sizeof(buffer));
        if (bytes > 0) {
            uv_write_t* req = emalloc(sizeof(uv_write_t));
            char* buf_data = emalloc(bytes);
            memcpy(buf_data, buffer, bytes);
            req->data = buf_data;
            
            uv_buf_t buf = uv_buf_init(buf_data, bytes);
            uv_write(req, (uv_stream_t*)client, &buf, 1, aurora_on_write);
        }
    }
}