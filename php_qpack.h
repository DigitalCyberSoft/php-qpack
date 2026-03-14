#ifndef PHP_QPACK_H
#define PHP_QPACK_H

extern zend_module_entry qpack_module_entry;
#define phpext_qpack_ptr &qpack_module_entry

#define PHP_QPACK_VERSION "1.0.1"

#ifdef PHP_WIN32
# define PHP_QPACK_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
# define PHP_QPACK_API __attribute__ ((visibility("default")))
#else
# define PHP_QPACK_API
#endif

PHP_MINIT_FUNCTION(qpack);
PHP_MSHUTDOWN_FUNCTION(qpack);
PHP_MINFO_FUNCTION(qpack);

#endif /* PHP_QPACK_H */
