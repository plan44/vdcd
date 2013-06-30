AC_INIT([vdcd], [0.1], [luz@plan44.ch], [vdcd], [http://www.plan44.ch/])
AC_PREREQ([2.59])
AM_INIT_AUTOMAKE([1.10 no-define foreign])
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_AUX_DIR(configure_aux)
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CXX

AC_CONFIG_FILES([Makefile])

AC_CHECK_LIB(m, atan2)
AC_CHECK_LIB(sqlite3, sqlite3_open)
AC_CHECK_LIB(json, json_tokener_parse)

AC_OUTPUT
