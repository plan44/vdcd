AC_INIT([vdcd], [2.0.1.1], [luz@plan44.ch], [vdcd], [http://www.plan44.ch/])
AC_PREREQ([2.59])

AC_CONFIG_AUX_DIR(configure_aux)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.10 no-define foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CXX

AC_CONFIG_FILES([Makefile])

##### OPTIONS

AC_ARG_ENABLE(
  [debug],
  [AC_HELP_STRING([--enable-debug], [Debug build with extra debugging code])]
)
AM_CONDITIONAL([DEBUG], [test "x$enable_debug" = xyes])

AC_ARG_ENABLE(
  [p44buildrpi],
  [AC_HELP_STRING([--enable-p44buildrpi], [plan44 special legacy cross-compiling build for Raspberry Pi])]
)
AM_CONDITIONAL([P44_BUILD_RPI], [test "x$enable_p44buildrpi" = xyes])

AC_ARG_ENABLE(
  [p44buildow],
  [AC_HELP_STRING([--enable-p44buildow], [plan44 build for openwrt targets])]
)
AM_CONDITIONAL([P44_BUILD_OW], [test "x$enable_p44buildow" = xyes])

AC_ARG_ENABLE(
  [ola],
  [AS_HELP_STRING([--enable-ola], [Enable OLA support (requires libola)])]
)
AM_CONDITIONAL([ENABLE_OLA], [test "x$enable_ola" = xyes])

AC_ARG_ENABLE(
  [rpiws281x],
  [AS_HELP_STRING([--enable-rpiws281x], [Enable WS281x Ledchain support on RaspberryPi ])]
)
AM_CONDITIONAL([ENABLE_RPIWS281X], [test "x$enable_rpiws281x" = xyes])

AC_ARG_ENABLE(
  [rrdb],
  [AS_HELP_STRING([--enable-rrdb], [Enable librrd usage for logging sensor data])]
)
AM_CONDITIONAL([ENABLE_RRDB], [test "x$enable_rrdb" = xyes])



AC_CHECK_LIB(m, atan2, [], [AC_MSG_ERROR([Could not find math lib (m) with atan2])])
AC_CHECK_LIB(rt, clock_gettime, [], [AC_MSG_ERROR([Could not find rt lib with clock_gettime])])
AC_CHECK_LIB(dl, dlopen, [], [AC_MSG_ERROR([Could not find libdl with dlopen])])

AM_COND_IF([ENABLE_OLA], [
  AC_CHECK_LIB(ola, ola_new_dmxbuffer, [], [AC_MSG_ERROR([Could not find libola with ola_new_dmxbuffer])])
], [])

AM_COND_IF([ENABLE_RRDB], [
  AC_CHECK_LIB(rrd, rrd_create, [], [AC_MSG_ERROR([Could not find librrd with rrd_create])])
], [])



AM_COND_IF([P44_BUILD_RPI], [], [
  dnl # can check libs only if not P44_BUILD_RPI
  AC_CHECK_LIB(json-c, json_tokener_get_error, [], [AC_MSG_ERROR([Could not find JSON-C / libjson0 with json_tokener_get_error supported (>=0.10)])])
  AC_CHECK_LIB(ssl, TLSv1_client_method, [], [AC_MSG_ERROR([Could not find libssl with TLSv1_client_method])])
  AC_CHECK_LIB(crypto, SHA1_Init, [], [AC_MSG_ERROR([Could not find libcrypto with SHA1_Init])])
  AC_CHECK_LIB(sqlite3, sqlite3_open, [], [AC_MSG_ERROR([Could not find sqlite3 with sqlite3_open])])
  AC_CHECK_LIB(protobuf-c, protobuf_c_enum_descriptor_get_value_by_name, [], [AC_MSG_ERROR([Could not find libprotobuf-c with protobuf_c_enum_descriptor_get_value_by_name])])
])

PKG_PROG_PKG_CONFIG

AM_COND_IF([P44_BUILD_RPI], [], [
  dnl # can check modules only if not P44_BUILD_RPI

  PKG_CHECK_MODULES([JSONC], [json-c >= 0.10 ], [], [
    AC_MSG_ERROR([$JSON_PKG_ERRORS])
  ])

  PKG_CHECK_MODULES([AVAHI], [avahi-client], [], [
    AC_MSG_ERROR([$AVAHI_PKG_ERRORS])
  ])

  PKG_CHECK_MODULES([SQLITE3], [sqlite3], [], [
    AC_MSG_ERROR([$SQLITE3_PKG_ERRORS])
  ])

  PKG_CHECK_MODULES([PROTOBUFC], [libprotobuf-c], [], [
    AC_CHECK_HEADERS([google/protobuf-c/protobuf-c.h], [], [
      AC_MSG_ERROR([coult not find protobuf-c headers])
    ])

    AC_CHECK_LIB([protobuf-c], [protobuf_c_message_unpack], [], [
      AC_MSG_ERROR([coult not find protobuf-c library])
    ])
  ])

])


AC_PATH_PROG(PROTOC, protoc-c, "")
if (test -z "$PROTOC"); then
    AC_MSG_ERROR([Can not build protobuf API, protoc not found])
fi
AC_SUBST(PROTOC)


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
