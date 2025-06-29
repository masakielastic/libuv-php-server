#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_smart_string.h"
#include "php_uvhttp.h"

#include "main/php_output.h"
#include "Zend/zend_compile.h"
#include "Zend/zend_execute.h"
#include <sys/stat.h>

#include "libuvhttp/uvhttp.h"

/* PHP-Faced functions */
PHP_FUNCTION(uvhttp_serve);
PHP_FUNCTION(uvhttp_stop);

/* Module lifecycle functions */
PHP_MINIT_FUNCTION(uvhttp);
PHP_MSHUTDOWN_FUNCTION(uvhttp);
PHP_RINIT_FUNCTION(uvhttp);
PHP_RSHUTDOWN_FUNCTION(uvhttp);
PHP_MINFO_FUNCTION(uvhttp);

/* Module globals */
ZEND_DECLARE_MODULE_GLOBALS(uvhttp)


// Forward declarations
static void uvhttp_request_handler(http_request_t* request);
static zend_string* uvhttp_execute_php_file(const char* file_path, http_request_t* request);
static zend_string* uvhttp_serve_static_file(const char* file_path);
static char* uvhttp_resolve_file_path(const char* docroot, const char* url);
static const char* uvhttp_get_mime_type(const char* file_path);
static void uvhttp_populate_superglobals(http_request_t* request, const char* docroot, const char* script_name);

/* Argument info */
ZEND_BEGIN_ARG_INFO_EX(arginfo_uvhttp_serve, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, options, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_uvhttp_stop, 0, 0, 0)
ZEND_END_ARG_INFO()

/* Function entries */
const zend_function_entry uvhttp_functions[] = {
    PHP_FE(uvhttp_serve, arginfo_uvhttp_serve)
    PHP_FE(uvhttp_stop, arginfo_uvhttp_stop)
    PHP_FE_END
};

/* Module entry */
zend_module_entry uvhttp_module_entry = {
    STANDARD_MODULE_HEADER,
    "uvhttp",
    uvhttp_functions,
    PHP_MINIT(uvhttp),
    PHP_MSHUTDOWN(uvhttp),
    PHP_RINIT(uvhttp),
    PHP_RSHUTDOWN(uvhttp),
    PHP_MINFO(uvhttp),
    PHP_UVHTTP_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_UVHTTP
ZEND_GET_MODULE(uvhttp)
#endif

static void php_uvhttp_init_globals(zend_uvhttp_globals* uvhttp_globals)
{
    uvhttp_globals->server = NULL;
    uvhttp_globals->docroot = NULL;
}

/* Main request handler called by libhttpserver */
static void uvhttp_request_handler(http_request_t* request) {
    const char* url = http_request_target(request);
    char* file_path = uvhttp_resolve_file_path(UVHTTP_G(docroot), url);

    if (!file_path) {
        http_response_t* response = http_response_init();
        http_response_status(response, 404);
        http_response_body(response, "<h1>404 Not Found</h1>", sizeof("<h1>404 Not Found</h1>") - 1);
        http_respond(request, response);
        http_response_destroy(response);
        return;
    }

    struct stat st;
    if (stat(file_path, &st) != 0) {
        http_response_t* response = http_response_init();
        http_response_status(response, 404);
        http_response_body(response, "<h1>404 Not Found</h1><p>File not found.</p>", sizeof("<h1>404 Not Found</h1><p>File not found.</p>")-1);
        http_respond(request, response);
        http_response_destroy(response);
        efree(file_path);
        return;
    }

    zend_string* output = NULL;
    const char* mime_type = "text/html";
    int status_code = 200;

    char* ext = strrchr(file_path, '.');
    if (ext && strcmp(ext, ".php") == 0) {
        output = uvhttp_execute_php_file(file_path, request);
    } else {
        output = uvhttp_serve_static_file(file_path);
        mime_type = uvhttp_get_mime_type(file_path);
    }

    if (!output) {
        status_code = 404;
        output = zend_string_init("<h1>404 Not Found</h1>", sizeof("<h1>404 Not Found</h1>") - 1, 0);
    }

    http_response_t* response = http_response_init();
    http_response_status(response, status_code);
    http_response_header(response, "Content-Type", mime_type);
    http_response_body(response, ZSTR_VAL(output), ZSTR_LEN(output));
    http_respond(request, response);
    
    http_response_destroy(response);
    zend_string_release(output);
    efree(file_path);
}


/* PHP Functions */
PHP_FUNCTION(uvhttp_serve) {
    HashTable* options = NULL;
    
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(options)
    ZEND_PARSE_PARAMETERS_END();
    
    if (UVHTTP_G(server)) {
        php_error_docref(NULL, E_WARNING, "uvhttp server is already running");
        RETURN_FALSE;
    }

    http_server_config_t config = {0};
    config.host = "127.0.0.1";
    config.port = 8080;
    config.handler = uvhttp_request_handler;

    char* docroot_str = getcwd(NULL, 0);
    UVHTTP_G(docroot) = docroot_str;

    if (options) {
        zval* val;
        if ((val = zend_hash_str_find(options, "port", 4)) != NULL) {
            config.port = (int)zval_get_long(val);
        }
        if ((val = zend_hash_str_find(options, "host", 4)) != NULL) {
            config.host = zval_get_string(val)->val;
        }
        if ((val = zend_hash_str_find(options, "docroot", 7)) != NULL) {
            free(docroot_str); // free the cwd default
            UVHTTP_G(docroot) = zval_get_string(val)->val;
        }
        
        zval* tls_cert = zend_hash_str_find(options, "tls_cert", 8);
        zval* tls_key = zend_hash_str_find(options, "tls_key", 7);
        
        if (tls_cert && tls_key) {
            config.tls_enabled = 1;
            config.cert_file = zval_get_string(tls_cert)->val;
            config.key_file = zval_get_string(tls_key)->val;
        }
    }
    
    UVHTTP_G(server) = http_server_create(&config);
    if (!UVHTTP_G(server)) {
        php_error_docref(NULL, E_ERROR, "Failed to create server");
        free(docroot_str);
        UVHTTP_G(docroot) = NULL;
        RETURN_FALSE;
    }
    
    http_server_listen(UVHTTP_G(server));

    // Cleanup after listen returns
    http_server_destroy(UVHTTP_G(server));
    UVHTTP_G(server) = NULL;
    free(docroot_str);
    UVHTTP_G(docroot) = NULL;

    RETURN_TRUE;
}

PHP_FUNCTION(uvhttp_stop) {
    if (UVHTTP_G(server)) {
        uv_stop(http_server_loop(UVHTTP_G(server)));
        RETURN_TRUE;
    }
    RETURN_FALSE;
}

/* Module lifecycle functions */
PHP_MINIT_FUNCTION(uvhttp) {
    ZEND_INIT_MODULE_GLOBALS(uvhttp, php_uvhttp_init_globals, NULL);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(uvhttp) {
    return SUCCESS;
}

PHP_RINIT_FUNCTION(uvhttp) {
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(uvhttp) {
    if (UVHTTP_G(server)) {
        http_server_destroy(UVHTTP_G(server));
        UVHTTP_G(server) = NULL;
    }
    if (UVHTTP_G(docroot)) {
        free(UVHTTP_G(docroot));
        UVHTTP_G(docroot) = NULL;
    }
    return SUCCESS;
}

PHP_MINFO_FUNCTION(uvhttp) {
    php_info_print_table_start();
    php_info_print_table_header(2, "uvhttp HTTP Server", "enabled");
    php_info_print_table_row(2, "Version", PHP_UVHTTP_VERSION);
    php_info_print_table_row(2, "Server Library", "libhttpserver (internal)");
    php_info_print_table_end();
}

/* Helper Functions */

static void uvhttp_populate_superglobals(http_request_t* request, const char* docroot, const char* script_name) {
    zval server_array;
    array_init(&server_array);

    const char* method = http_request_method(request);
    const char* url = http_request_target(request);
    const char* body = http_request_body(request);
    size_t body_len = http_request_body_length(request);

    add_assoc_string(&server_array, "REQUEST_METHOD", (char*)method);
    add_assoc_string(&server_array, "REQUEST_URI", (char*)url);
    
    char* query_pos = strchr(url, '?');
    if (query_pos) {
        add_assoc_string(&server_array, "QUERY_STRING", query_pos + 1);
        add_assoc_stringl(&server_array, "PATH_INFO", (char*)url, query_pos - url);
    } else {
        add_assoc_string(&server_array, "QUERY_STRING", "");
        add_assoc_string(&server_array, "PATH_INFO", (char*)url);
    }

    add_assoc_string(&server_array, "SERVER_PROTOCOL", "HTTP/1.1");
    add_assoc_string(&server_array, "SERVER_SOFTWARE", "uvhttp/0.2.0");
    add_assoc_string(&server_array, "DOCUMENT_ROOT", (char*)docroot);
    add_assoc_string(&server_array, "SCRIPT_NAME", (char*)script_name);
    add_assoc_string(&server_array, "SCRIPT_FILENAME", (char*)script_name);

    zend_hash_str_update(&EG(symbol_table), "_SERVER", sizeof("_SERVER") - 1, &server_array);

    // Populate $_GET
    zval get_array;
    array_init(&get_array);
    if (query_pos) {
        // Basic GET parsing, not fully robust
        // A real implementation should handle url decoding etc.
    }
    zend_hash_str_update(&EG(symbol_table), "_GET", sizeof("_GET") - 1, &get_array);

    // Populate $_POST
    zval post_array;
    array_init(&post_array);
    if (strcmp(method, "POST") == 0 && body && body_len > 0) {
       // Basic POST parsing, not fully robust
    }
    zend_hash_str_update(&EG(symbol_table), "_POST", sizeof("_POST") - 1, &post_array);
}

static zend_string* uvhttp_execute_php_file(const char* file_path, http_request_t* request) {
    zend_file_handle file_handle;
    zend_string *result;

    if (php_output_start_default() != SUCCESS) {
        return zend_string_init("Failed to start output buffering", sizeof("Failed to start output buffering")-1, 0);
    }

    uvhttp_populate_superglobals(request, UVHTTP_G(docroot), file_path);

    zend_stream_init_filename(&file_handle, file_path);
    if (zend_execute_scripts(ZEND_REQUIRE, NULL, 1, &file_handle) != SUCCESS) {
        while (php_output_get_level() > 0) {
            php_output_discard();
        }
        return zend_string_init("<h1>500 Internal Server Error</h1>", sizeof("<h1>500 Internal Server Error</h1>")-1, 0);
    }

    zval output_zval;
    php_output_get_contents(&output_zval);
    result = zval_get_string(&output_zval);
    zval_ptr_dtor(&output_zval);

    while (php_output_get_level() > 0) {
        php_output_discard();
    }

    return result;
}

static zend_string* uvhttp_serve_static_file(const char* file_path) {
    FILE* file = fopen(file_path, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return zend_string_init("", 0, 0);
    }

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

static char* uvhttp_resolve_file_path(const char* docroot, const char* url) {
    const char* path = url;
    if (strcmp(url, "/") == 0) {
        path = "/index.php";
    }
    
    char* query_pos = strchr(path, '?');
    size_t path_len = query_pos ? (size_t)(query_pos - path) : strlen(path);

    size_t docroot_len = strlen(docroot);
    char* file_path = emalloc(docroot_len + path_len + 1);
    strcpy(file_path, docroot);
    strncat(file_path, path, path_len);
    file_path[docroot_len + path_len] = '\0';
    
    return file_path;
}

static const char* uvhttp_get_mime_type(const char* file_path) {
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
