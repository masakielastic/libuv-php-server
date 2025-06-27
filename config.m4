PHP_ARG_ENABLE([aurora],
  [whether to enable Aurora HTTP server support],
  [AS_HELP_STRING([--enable-aurora],
    [Enable Aurora HTTP server support])],
  [no])

if test "$PHP_AURORA" != "no"; then
  dnl Check for libuv
  AC_MSG_CHECKING([for libuv])
  PKG_CHECK_MODULES([LIBUV], [libuv >= 1.0.0], [
    AC_MSG_RESULT([yes])
    PHP_EVAL_INCLINE($LIBUV_CFLAGS)
    PHP_EVAL_LIBLINE($LIBUV_LIBS, AURORA_SHARED_LIBADD)
  ], [
    AC_MSG_ERROR([libuv is required but not found])
  ])

  dnl Check for OpenSSL
  AC_MSG_CHECKING([for OpenSSL])
  PKG_CHECK_MODULES([OPENSSL], [openssl >= 1.1.0], [
    AC_MSG_RESULT([yes])
    PHP_EVAL_INCLINE($OPENSSL_CFLAGS)
    PHP_EVAL_LIBLINE($OPENSSL_LIBS, AURORA_SHARED_LIBADD)
  ], [
    AC_MSG_ERROR([OpenSSL is required but not found])
  ])

  dnl Check for llhttp
  AC_MSG_CHECKING([for llhttp])
  if test -f "$srcdir/llhttp.h" && test -f "$srcdir/llhttp.c"; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_ERROR([llhttp source files not found])
  fi

  dnl Define extension
  PHP_NEW_EXTENSION(aurora, aurora.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
  PHP_SUBST(AURORA_SHARED_LIBADD)
  
  dnl Add compiler flags
  PHP_ADD_BUILD_DIR($ext_builddir)
fi