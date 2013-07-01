AC_INIT([vdcd], [0.1], [luz@plan44.ch], [vdcd], [http://www.plan44.ch/])
AC_PREREQ([2.59])

AC_CONFIG_AUX_DIR(configure_aux)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.10 no-define foreign])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CXX

AC_CONFIG_FILES([Makefile])

AC_CHECK_LIB(m, atan2)
AC_CHECK_LIB(sqlite3, sqlite3_open)
AC_CHECK_LIB(json, json_tokener_parse)

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


AC_OUTPUT
