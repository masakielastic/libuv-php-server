#ifndef PHP_UVHTTP_H
#define PHP_UVHTTP_H

extern zend_module_entry uvhttp_module_entry;
#define phpext_uvhttp_ptr &uvhttp_module_entry

#define PHP_UVHTTP_VERSION "0.2.0"

#if defined(ZTS) && defined(COMPILE_DL_UVHTTP)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

ZEND_BEGIN_MODULE_GLOBALS(uvhttp)
    void* server;
    char* docroot;
ZEND_END_MODULE_GLOBALS(uvhttp)

#define UVHTTP_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(uvhttp, v)

#endif	/* PHP_UVHTTP_H */
