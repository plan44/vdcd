AC_INIT([vdcd], [2.8.2.19], [luz@plan44.ch], [vdcd], [http://www.plan44.ch/])
AC_PREREQ([2.59])

AC_CONFIG_AUX_DIR(configure_aux)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.10 no-define foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CXX

AC_CONFIG_FILES([Makefile])

##### OPTIONS

AC_ARG_VAR(APPLICATION_VERSION, [specify to inject application version from build environment])
AM_CONDITIONAL([WITH_APPLICATION_VERSION], [test -n "$APPLICATION_VERSION"])

AC_ARG_ENABLE(
  [debug],
  [AC_HELP_STRING([--enable-debug], [Debug build with extra debugging code])]
)
AM_CONDITIONAL([DEBUG], [test "x$enable_debug" = xyes])

AC_ARG_ENABLE(
  [p44buildow],
  [AC_HELP_STRING([--enable-p44buildow], [plan44 build for openwrt targets])]
)
AM_CONDITIONAL([P44_BUILD_OW], [test "x$enable_p44buildow" = xyes])

AC_ARG_ENABLE(
  [p44buildrb],
  [AC_HELP_STRING([--enable-p44buildrb], [plan44 build for Raspbian targets])]
)
AM_CONDITIONAL([P44_BUILD_RB], [test "x$enable_p44buildrb" = xyes])


AC_ARG_ENABLE(
  [ola],
  [AS_HELP_STRING([--enable-ola], [Enable OLA support (requires libola)])]
)
AM_CONDITIONAL([ENABLE_OLA], [test "x$enable_ola" = xyes])

AC_ARG_ENABLE(
  [ubus],
  [AS_HELP_STRING([--enable-ubus], [Enable ubus API support (requires libubus)])]
)
AM_CONDITIONAL([ENABLE_UBUS], [test "x$enable_ubus" = xyes])

AC_ARG_ENABLE(
  [i2c],
  [AS_HELP_STRING([--enable-i2c], [Enable i2c direct access (requires libi2c)])]
)
AM_CONDITIONAL([ENABLE_I2C], [test "x$enable_i2c" = xyes])

AC_ARG_ENABLE(
  [rpiws281x],
  [AS_HELP_STRING([--enable-rpiws281x], [Enable WS281x Ledchain support on RaspberryPi])]
)
AM_CONDITIONAL([ENABLE_RPIWS281X], [test "x$enable_rpiws281x" = xyes])

AC_ARG_ENABLE(
  [png],
  [AS_HELP_STRING([--enable-png], [Enable PNG image support for ledchains])]
)
AM_CONDITIONAL([ENABLE_PNG], [test "x$enable_png" = xyes])

AC_ARG_ENABLE(
  [rrdb],
  [AS_HELP_STRING([--enable-rrdb], [Enable librrd usage for logging sensor data])]
)
AM_CONDITIONAL([ENABLE_RRDB], [test "x$enable_rrdb" = xyes])

AC_ARG_ENABLE(
  [uwsc],
  [AS_HELP_STRING([--enable-uwsc], [Enable libuwsc usage for websockets])]
)
AM_CONDITIONAL([ENABLE_UWSC], [test "x$enable_uwsc" = xyes])

AC_ARG_ENABLE(
  [ev],
  [AS_HELP_STRING([--enable-ev], [Enable libev usage for mainloop])]
)
AM_CONDITIONAL([ENABLE_EV], [test "x$enable_ev" = xyes])

AC_ARG_ENABLE(
  [p44features],
  [AS_HELP_STRING([--enable-p44features], [Enable p44features special purpose hardware support])]
)
AM_CONDITIONAL([ENABLE_P44FEATURES], [test "x$enable_p44features" = xyes])

AC_ARG_ENABLE(
  [modbus],
  [AS_HELP_STRING([--enable-modbus], [Enable modbus support])]
)
AM_CONDITIONAL([ENABLE_MODBUS], [test "x$enable_modbus" = xyes])

AC_ARG_ENABLE(
  [embeddedlibmodbus],
  [AS_HELP_STRING([--enable-embeddedlibmodbus], [Enable embedded libmodbus (instead of using standard package)])]
)
AM_CONDITIONAL([EMBEDDED_LIBMODBUS], [test "x$enable_embeddedlibmodbus" = xyes])

AC_ARG_ENABLE(
  [ds485],
  [AS_HELP_STRING([--enable-ds485], [Enable ds485 support])]
)
AM_CONDITIONAL([ENABLE_DS485], [test "x$enable_ds485" = xyes])

AC_ARG_ENABLE(
  [lvgl],
  [AS_HELP_STRING([--enable-lvgl], [Enable lvgl graphics support])]
)
AM_CONDITIONAL([ENABLE_LVGL], [test "x$enable_lvgl" = xyes])

AC_ARG_ENABLE(
  [platformshortbreak],
  [AS_HELP_STRING([--enable-platformshortbreak], [Enable usage of platform's ability to produce short breaks on UART])]
)
AM_CONDITIONAL([PLATFORM_HAS_SHORTBREAK], [test "x$enable_platformshortbreak" = xyes])




AC_CHECK_LIB(m, atan2, [], [AC_MSG_ERROR([Could not find math lib (m) with atan2])])
AC_CHECK_LIB(rt, clock_gettime, [], [AC_MSG_ERROR([Could not find rt lib with clock_gettime])])
AC_CHECK_LIB(dl, dlopen, [], [AC_MSG_ERROR([Could not find libdl with dlopen])])

AM_COND_IF([ENABLE_OLA], [
  AC_CHECK_LIB(ola, ola_new_dmxbuffer, [], [AC_MSG_ERROR([Could not find libola with ola_new_dmxbuffer])])
], [])

AM_COND_IF([ENABLE_UBUS], [
  AC_CHECK_LIB(ubus, ubus_connect_ctx, [], [AC_MSG_ERROR([Could not find libubus with ubus_connect_ctx])])
  AC_CHECK_LIB(ubox, uloop_init, [], [AC_MSG_ERROR([Could not find libubox with uloop_init])])
  AC_CHECK_LIB(blobmsg_json, blobmsg_add_object, [], [AC_MSG_ERROR([Could not find libblobmsg_json with blobmsg_add_object])])
], [])

AM_COND_IF([ENABLE_I2C], [
  AC_CHECK_LIB(i2c, i2c_smbus_write_byte, [], [AC_MSG_ERROR([Could not find libi2c with i2c_smbus_write_byte])])
], [])

AM_COND_IF([ENABLE_RRDB], [
  AC_CHECK_LIB(rrd, rrd_create, [], [AC_MSG_ERROR([Could not find librrd with rrd_create])])
], [])

AM_COND_IF([ENABLE_UWSC], [
  AC_CHECK_LIB(uwsc, uwsc_init, [], [AC_MSG_ERROR([Could not find libuwsc with uwsc_init])])
], [])

AM_COND_IF([ENABLE_EV], [
  AC_CHECK_LIB(ev, ev_time, [], [AC_MSG_ERROR([Could not find libuwsc with ev_time])])
], [])

AM_COND_IF([ENABLE_MODBUS], [
  AM_COND_IF([EMBEDDED_LIBMODBUS], [], [
    AC_CHECK_LIB(modbus, modbus_rtu_set_custom_rts_ex, [], [AC_MSG_ERROR([Could not find libmodbus with modbus_rtu_set_custom_rts_ex - plan44 enhanced version!])])
  ])
], [])

AC_CHECK_LIB(json-c, json_tokener_get_error, [], [AC_MSG_ERROR([Could not find JSON-C / libjson0 with json_tokener_get_error supported (>=0.10)])])
AC_CHECK_LIB(ssl, TLSv1_client_method, [], [AC_MSG_ERROR([Could not find libssl with TLSv1_client_method])])
AC_CHECK_LIB(crypto, SHA1_Init, [], [AC_MSG_ERROR([Could not find libcrypto with SHA1_Init])])
AC_CHECK_LIB(sqlite3, sqlite3_open, [], [AC_MSG_ERROR([Could not find sqlite3 with sqlite3_open])])
AC_CHECK_LIB(protobuf-c, protobuf_c_enum_descriptor_get_value_by_name, [], [AC_MSG_ERROR([Could not find libprotobuf-c with protobuf_c_enum_descriptor_get_value_by_name])])


PKG_PROG_PKG_CONFIG


PKG_CHECK_MODULES([JSONC], [json-c >= 0.13 ],
  [AC_DEFINE([HAVE_JSONC_VERSION_013], [1], [We have JSONC version 0.13 or later])],
  [PKG_CHECK_MODULES([JSONC], [json-c >= 0.10 ], [], [
    AC_MSG_ERROR([$JSON_PKG_ERRORS])
  ])]
)

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

AM_COND_IF([ENABLE_PNG], [
  PKG_CHECK_MODULES([PNG], [libpng >= 1.6], [], [
    AC_MSG_ERROR([$PNG_PKG_ERRORS])
  ])
], [])



AC_PATH_PROG(PROTOC, protoc-c, "")
if (test -z "$PROTOC"); then
    AC_MSG_ERROR([Can not build protobuf API, protoc not found])
fi
AC_SUBST(PROTOC)


# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h sys/resource.h], [], [AC_MSG_ERROR([required system header not found])])


ASSUMED_BOOST_VER=146

BOOST_REQUIRE([1.46], [
  unset boost_cv_version
  unset boost_cv_inc_path
  BOOST_REQUIRE([1.35], [
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
