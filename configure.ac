AC_INIT([vdcd], [0.1], [luz@plan44.ch], [vdcd], [http://www.plan44.ch/])
AC_PREREQ([2.59])

AC_CONFIG_AUX_DIR(configure_aux)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.10 no-define foreign])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CXX

AC_CONFIG_FILES([Makefile])

PKG_CHECK_MODULES([JSONC], [json], [],
    [
        AC_MSG_ERROR([Missing json-c / libjson0])
    ])

AC_CHECK_LIB(m, atan2, [], [AC_MSG_ERROR([Could not find math lib (m)])])
AC_CHECK_LIB(sqlite3, sqlite3_open, [], [AC_MSG_ERROR([Could not find sqlite3])])
AC_CHECK_LIB(crypto, SHA1_Init, [], [AC_MSG_ERROR([Could not find libcrypto])])
#AC_CHECK_LIB(ssl, SSLv2_client_method, [], [AC_MSG_ERROR([Could not find libssl])])
AC_CHECK_LIB(rt, clock_gettime, [], [AC_MSG_ERROR([Could not find rt lib for clock_gettime])])

AC_CHECK_LIB(dl, dlopen, [], [AC_MSG_ERROR([Could not find libdl])])

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([JSONC], [json >= 0.10 ], [],
    [
        AC_MSG_ERROR([$JSON_PKG_ERRORS])
    ])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h sys/resource.h], [], [AC_MSG_ERROR([required system header not found])])


ASSUMED_BOOST_VER=146

BOOST_REQUIRE([1.46],
    [
        unset boost_cv_version
        unset boost_cv_inc_path
        BOOST_REQUIRE([1.35],
            [
                AC_MSG_ERROR([could not find boost on your system])
            ])
        ASSUMED_BOOST_VER=135
    ])

if test $ASSUMED_BOOST_VER -eq 135; then
    AC_DEFINE([BOOST_VERSION_135], [1], [boost 1.35 or higher])
else
    AC_DEFINE([BOOST_VERSION_146], [1], [boost 1.46 or higher])
fi

BOOST_BIND
BOOST_FOREACH
BOOST_FUNCTION
BOOST_SMART_PTR

AX_PTHREAD([], [ AC_MSG_ERROR([required pthread library not found]) ])

AC_OUTPUT
