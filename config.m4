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

  dnl Build our static library
  HTTPSERVER_DIR=$srcdir/libhttpserver
  AC_MSG_CHECKING([for http server sources in $HTTPSERVER_DIR])
  if test -d "$HTTPSERVER_DIR"; then
    AC_MSG_RESULT([found])
  else
    AC_MSG_ERROR([not found])
  fi

  PHP_ADD_INCLUDE($HTTPSERVER_DIR)
  AURORA_SHARED_LIBADD="-Wl,--whole-archive -L$HTTPSERVER_DIR -lhttpserver -Wl,--no-whole-archive $AURORA_SHARED_LIBADD"

  dnl Define extension
  PHP_NEW_EXTENSION(aurora, aurora.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
  PHP_SUBST(AURORA_SHARED_LIBADD)
  
  dnl Add compiler flags
  PHP_ADD_BUILD_DIR($ext_builddir)

  dnl Add a rule to build the static library
  PHP_ADD_MAKEFILE_FRAGMENT($srcdir/Makefile.frag)
fi