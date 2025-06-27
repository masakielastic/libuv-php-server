#ifndef PHP_AURORA_H
#define PHP_AURORA_H

extern zend_module_entry aurora_module_entry;
#define phpext_aurora_ptr &aurora_module_entry

#define PHP_AURORA_VERSION "0.1.0"

#ifdef PHP_WIN32
# define PHP_AURORA_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
# define PHP_AURORA_API __attribute__ ((visibility("default")))
#else
# define PHP_AURORA_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#include <uv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "llhttp.h"

/* Server structure */
typedef struct _aurora_server {
    uv_loop_t* loop;
    uv_tcp_t tcp_server;
    SSL_CTX* ssl_ctx;
    zend_string* docroot;
    zend_string* bind_addr;
    int port;
    int ssl_enabled;
    zval handler_callback;
    HashTable* mime_types;
} aurora_server_t;

/* Client connection structure */
typedef struct _aurora_client {
    uv_tcp_t tcp;
    aurora_server_t* server;
    SSL* ssl;
    BIO* read_bio;
    BIO* write_bio;
    llhttp_t parser;
    llhttp_settings_t parser_settings;
    int handshake_complete;
    int shutdown_sent;
    zend_string* method;
    zend_string* url;
    zend_string* http_version;
    HashTable* headers;
    zend_string* body;
    zend_string* current_header_field;
    int headers_complete;
    int message_complete;
    zend_string* response_body;
    int response_status;
    HashTable* response_headers;
    int response_sent;
} aurora_client_t;

/* Module globals */
ZEND_BEGIN_MODULE_GLOBALS(aurora)
    aurora_server_t* server;
    zend_bool is_running;
ZEND_END_MODULE_GLOBALS(aurora)

#ifdef ZTS
#define AURORA_G(v) TSRMG(aurora_globals_id, zend_aurora_globals *, v)
#else
#define AURORA_G(v) (aurora_globals.v)
#endif

/* Function declarations */
PHP_MINIT_FUNCTION(aurora);
PHP_MSHUTDOWN_FUNCTION(aurora);
PHP_RINIT_FUNCTION(aurora);
PHP_RSHUTDOWN_FUNCTION(aurora);
PHP_MINFO_FUNCTION(aurora);

/* PHP functions */
PHP_FUNCTION(aurora_serve);
PHP_FUNCTION(aurora_register_handler);
PHP_FUNCTION(aurora_stop);

/* Internal functions */
int aurora_init_ssl(aurora_server_t* server, const char* cert_file, const char* key_file);
void aurora_cleanup_ssl(aurora_server_t* server);
aurora_client_t* aurora_client_create(aurora_server_t* server);
void aurora_client_free(aurora_client_t* client);
int aurora_parse_http_request(aurora_client_t* client, const char* data, size_t len);
void aurora_populate_superglobals(aurora_client_t* client);
void aurora_send_response(aurora_client_t* client, int status, HashTable* headers, const char* body, size_t body_len);
void aurora_response_callback(aurora_client_t* client, int status, HashTable* headers, zend_string* body);
zend_string* aurora_execute_php_file(aurora_client_t* client, const char* file_path);
zend_string* aurora_serve_static_file(const char* file_path, const char** mime_type);
const char* aurora_get_mime_type(const char* file_path);
char* aurora_resolve_file_path(aurora_server_t* server, const char* url);

#endif /* PHP_AURORA_H */