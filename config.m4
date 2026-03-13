dnl config.m4 for extension qpack

PHP_ARG_ENABLE([qpack],
  [whether to enable qpack support],
  [AS_HELP_STRING([--enable-qpack],
    [Enable qpack support])],
  [no])

PHP_ARG_WITH([nghttp3],
  [whether to use nghttp3 for QPACK],
  [AS_HELP_STRING([--with-nghttp3],
    [Use nghttp3 library for QPACK (optional)])],
  [no],
  [no])

if test "$PHP_QPACK" != "no"; then
  if test "$PHP_NGHTTP3" != "no"; then
    AC_CHECK_HEADER([nghttp3/nghttp3.h], [
      PHP_CHECK_LIBRARY(nghttp3, nghttp3_qpack_encoder_new,
        [
          PHP_ADD_LIBRARY(nghttp3, 1, QPACK_SHARED_LIBADD)
          AC_DEFINE(HAVE_NGHTTP3, 1, [Have nghttp3 library])
        ],
        [AC_MSG_WARN([libnghttp3 not found, using built-in QPACK implementation])])
    ], [AC_MSG_WARN([nghttp3 headers not found, using built-in QPACK implementation])])
  fi

  PHP_SUBST(QPACK_SHARED_LIBADD)
  PHP_NEW_EXTENSION(qpack, qpack.c, $ext_shared)
fi
