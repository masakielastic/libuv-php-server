#ifndef PHP_AURORA_H
#define PHP_AURORA_H

extern zend_module_entry aurora_module_entry;
#define phpext_aurora_ptr &aurora_module_entry

#define PHP_AURORA_VERSION "0.2.0"

#if defined(ZTS) && defined(COMPILE_DL_AURORA)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

ZEND_BEGIN_MODULE_GLOBALS(aurora)
    void* server;
    char* docroot;
ZEND_END_MODULE_GLOBALS(aurora)

#define AURORA_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(aurora, v)

#endif	/* PHP_AURORA_H */
