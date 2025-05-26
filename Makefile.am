AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

bin_PROGRAMS = vdcd

EXTRA_PROGRAMS = tests

# common stuff for protobuf - NOTE: need to "make all" to get BUILT_SOURCES made

PROTOBUF_GENERATED = \
  src/p44vdc/pbuf/gen/messages.pb-c.h \
  src/p44vdc/pbuf/gen/messages.pb-c.c \
  src/p44vdc/pbuf/gen/vdcapi.pb-c.h \
  src/p44vdc/pbuf/gen/vdcapi.pb-c.c

#dist_noinst_DATA = messages.proto vdcapi.proto

MOSTLYCLEANFILES = ${PROTOBUF_GENERATED}

BUILT_SOURCES = ${PROTOBUF_GENERATED}

src/p44vdc/pbuf/gen/messages.pb-c.h src/p44vdc/pbuf/gen/messages.pb-c.c:	${top_srcdir}/src/p44vdc/pbuf/messages.proto
	${PROTOC} --proto_path=${top_srcdir}/src/p44vdc/pbuf --c_out=src/p44vdc/pbuf/gen ${top_srcdir}/src/p44vdc/pbuf/messages.proto

src/p44vdc/pbuf/gen/vdcapi.pb-c.h src/p44vdc/pbuf/gen/vdcapi.pb-c.c:	${top_srcdir}/src/p44vdc/pbuf/vdcapi.proto
	${PROTOC} --proto_path=${top_srcdir}/src/p44vdc/pbuf --c_out=src/p44vdc/pbuf/gen ${top_srcdir}/src/p44vdc/pbuf/vdcapi.proto


# vdcd

if DEBUG
vdcd_DEBUG = -D DEBUG=1
else
vdcd_DEBUG =
endif

if P44_BUILD_OW
vdcd_PLATFORM = -D P44_BUILD_OW=1 -std=gnu++17
else
if P44_BUILD_RB
vdcd_PLATFORM = -D P44_BUILD_RB=1
else !P44_BUILD_RB
vdcd_PLATFORM = -D DISABLE_I2C=1
endif !P44_BUILD_RB
endif !P44_BUILD_OW

if PLATFORM_HAS_SHORTBREAK
vdcd_SHORTBREAK = -D PLATFORM_HAS_SHORTBREAK=1
else
vdcd_SHORTBREAK =
endif


if WITH_APPLICATION_VERSION
vdcd_VERSION = -DP44_APPLICATION_VERSION=\"$(APPLICATION_VERSION)\"
endif


nodist_vdcd_SOURCES = ${PROTOBUF_GENERATED}


# WS281x LED chain support on Raspberry Pi (direct PWM/PCM hardware access)
if ENABLE_RPIWS281X

RPIWS281X_SOURCES = \
  src/p44utils/thirdparty/rpi_ws281x/clk.h \
  src/p44utils/thirdparty/rpi_ws281x/gpio.h \
  src/p44utils/thirdparty/rpi_ws281x/dma.h \
  src/p44utils/thirdparty/rpi_ws281x/dma.c \
  src/p44utils/thirdparty/rpi_ws281x/pcm.h \
  src/p44utils/thirdparty/rpi_ws281x/pcm.c \
  src/p44utils/thirdparty/rpi_ws281x/pwm.h \
  src/p44utils/thirdparty/rpi_ws281x/pwm.c \
  src/p44utils/thirdparty/rpi_ws281x/rpihw.h \
  src/p44utils/thirdparty/rpi_ws281x/rpihw.c \
  src/p44utils/thirdparty/rpi_ws281x/mailbox.h \
  src/p44utils/thirdparty/rpi_ws281x/mailbox.c \
  src/p44utils/thirdparty/rpi_ws281x/ws2811.h \
  src/p44utils/thirdparty/rpi_ws281x/ws2811.c
RPIWS281X_FLAGS = \
  -D ENABLE_RPIWS281X=1 \
  -I ${srcdir}/src/p44utils/thirdparty/rpi_ws281x

else

RPIWS281X_FLAGS = -D ENABLE_RPIWS281X=0

endif


# OLA support via libola
if ENABLE_OLA

OLA_FLAGS = \
  -D ENABLE_OLA=1
OLA_LIBS = \
  -lola -lolacommon -lprotobuf

else

OLA_FLAGS = -D ENABLE_OLA=0

endif


# ubus support via libubus
if ENABLE_DS485

DS485SUPP_GENERATED = \
	${builddir}/include/digitalSTROM/dsuid.h \
	${builddir}/include/digitalSTROM/ds485-client-interface.h \
	${builddir}/include/digitalSTROM/ds485n-packet.h \
	${builddir}/include/digitalSTROM/ds485-socket-server.h \
	${builddir}/include/digitalSTROM/ds485-socket-interface.h

BUILT_SOURCES += ${DS485SUPP_GENERATED}

DS485SUPP_SOURCES = \
  src/p44vdc/thirdparty/ds485-stack/libdsuid/src/dsuid.c \
  src/p44vdc/thirdparty/ds485-stack/ds485-client/base/log.c \
  src/p44vdc/thirdparty/ds485-stack/ds485-client/base/socket_client.c \
  src/p44vdc/thirdparty/ds485-stack/ds485-client/base/socket_send.c \
  src/p44vdc/thirdparty/ds485-stack/ds485-client/src/client-socket.c \
  src/p44vdc/thirdparty/ds485-stack/ds485-client/src/ds485-client.c \
  src/p44vdc/thirdparty/ds485-stack/ds485-client/src/shim.c \
  src/p44vdc/thirdparty/ds485-stack/ds485-client/src/util.c \
  src/p44vdc/thirdparty/ds485-stack/ds485-client/src/waitqueue.c

DS485SUPP_FLAGS = \
  -D ENABLE_DS485DEVICES=1 \
  -I ${srcdir}/src/p44vdc/thirdparty/ds485-stack/libdsuid/src \
  -I ${srcdir}/src/p44vdc/thirdparty/ds485-stack/ds485-client \
  -I ${srcdir}/src/p44vdc/thirdparty/ds485-stack/ds485-client/src \
  -I ${srcdir}/src/p44vdc/thirdparty/ds485-stack/prebuilt/dsm-api-v2 \
  -I ${builddir}/include/digitalSTROM \
  -isystem ${builddir}/include

copy-ds485-headers:
	mkdir -p ${builddir}/include/digitalSTROM
	cp ${srcdir}/src/p44vdc/thirdparty/ds485-stack/ds485-core/include/digitalSTROM/*.h ${builddir}/include/digitalSTROM
	cp ${srcdir}/src/p44vdc/thirdparty/ds485-stack/ds485-netlib/include/*.h ${builddir}/include/digitalSTROM
	cp ${srcdir}/src/p44vdc/thirdparty/ds485-stack/ds485-client/include/*.h ${builddir}/include/digitalSTROM
	cp ${srcdir}/src/p44vdc/thirdparty/ds485-stack/libdsuid/src/*.h ${builddir}/include/digitalSTROM

${builddir}/include/digitalSTROM/%.h: copy-ds485-headers
	@true

.PHONY: copy-ds485-headers

else

DS485SUPP_FLAGS = -D ENABLE_DS485DEVICES=0

endif


# ubus support via libubus
if ENABLE_MODBUS

MODBUSSUPP_SOURCES = \
  src/p44utils/modbus.cpp \
  src/p44utils/modbus.hpp
MODBUSSUPP_FLAGS = -D ENABLE_MODBUS=1

else

MODBUSSUPP_FLAGS = -D ENABLE_MODBUS=0

endif


# i2c support via libi2c
if ENABLE_I2C

I2C_LIBS = -li2c
I2C_FLAGS = -D DISABLE_I2C=0

else

I2C_FLAGS = -D DISABLE_I2C=1

endif


# embedded libmodbus
if EMBEDDED_LIBMODBUS

LIBMODBUS_SOURCES = \
  src/p44utils/thirdparty/libmodbus/src/modbus-data.c \
  src/p44utils/thirdparty/libmodbus/src/modbus-private.h \
  src/p44utils/thirdparty/libmodbus/src/modbus-rtu-private.h \
  src/p44utils/thirdparty/libmodbus/src/modbus-rtu.c \
  src/p44utils/thirdparty/libmodbus/src/modbus-rtu.h \
  src/p44utils/thirdparty/libmodbus/src/modbus-tcp-private.h \
  src/p44utils/thirdparty/libmodbus/src/modbus-tcp.c \
  src/p44utils/thirdparty/libmodbus/src/modbus-tcp.h \
  src/p44utils/thirdparty/libmodbus/src/modbus-version.h \
  src/p44utils/thirdparty/libmodbus/src/modbus.c \
  src/p44utils/thirdparty/libmodbus/src/modbus.h

LIBMODBUS_CFLAGS = -I ${srcdir}/src/p44utils/thirdparty/libmodbus/src -D EMBEDDED_LIBMODBUS=1

else

LIBMODBUS_CFLAGS = ${MODBUS_CFLAGS}
LIBMODBUS_LIBS = ${MODBUS_LIBS}

endif


# ubus support via libubus
if ENABLE_UBUS

UBUS_SOURCES = \
  src/p44utils/ubus.cpp \
  src/p44utils/ubus.hpp
UBUS_FLAGS = -D ENABLE_UBUS=1

else

UBUS_FLAGS = -D ENABLE_UBUS=0

endif


# websocket support via libuwsc
if ENABLE_UWSC

UWSC_SOURCES = \
  src/p44utils/websocket.cpp \
  src/p44utils/websocket.hpp
UWSC_FLAGS = -D ENABLE_UWSC=1

else

UWSC_FLAGS = -D ENABLE_UWSC=0

endif


# libev based mainloop
if ENABLE_EV

EV_FLAGS = -D MAINLOOP_LIBEV_BASED=1

endif


# PNG support via libpng
if ENABLE_PNG

PNG_FLAGS = \
  -D ENABLE_PNG=1

else

PNG_FLAGS = -D ENABLE_PNG=0

endif


# rrdb support for logging sensor data
if ENABLE_RRDB

RRDB_FLAGS = -D ENABLE_RRDB=1 ${RRD_FLAGS}
RRDB_LIBS = ${RRD_LIBS}

else

RRDB_FLAGS = -D ENABLE_RRDB=0

endif


# p44features special purpose hardware support
if ENABLE_P44FEATURES

P44FEATURES_SOURCES = \
  src/p44features/dispmatrix.cpp \
  src/p44features/dispmatrix.hpp \
  src/p44features/feature.cpp \
  src/p44features/feature.hpp \
  src/p44features/featureapi.cpp \
  src/p44features/featureapi.hpp \
  src/p44features/hermel.cpp \
  src/p44features/hermel.hpp \
  src/p44features/indicators.cpp \
  src/p44features/indicators.hpp \
  src/p44features/inputs.cpp \
  src/p44features/inputs.hpp \
  src/p44features/keyevents.cpp \
  src/p44features/keyevents.hpp \
  src/p44features/light.cpp \
  src/p44features/light.hpp \
  src/p44features/mixloop.cpp \
  src/p44features/mixloop.hpp \
  src/p44features/neuron.cpp \
  src/p44features/neuron.hpp \
  src/p44features/p44features_common.hpp \
  src/p44features/rfids.cpp \
  src/p44features/rfids.hpp \
  src/p44features/splitflaps.cpp \
  src/p44features/splitflaps.hpp \
  src/p44features/wifitrack.cpp \
  src/p44features/wifitrack.hpp \
  src/p44utils/rfid.cpp \
  src/p44utils/rfid.hpp
P44FEATURES_FLAGS = \
  -D ENABLE_P44FEATURES=1 \
  -I ${srcdir}/src/p44features

else

P44FEATURES_FLAGS = -D ENABLE_P44FEATURES=0

endif


# lvgl support
if ENABLE_LVGL

LVGL_SOURCES = \
  src/lv_conf.h \
  src/lv_drv_conf.h \
  src/lvgl/src/lv_core/lv_debug.c \
  src/lvgl/src/lv_core/lv_disp.c \
  src/lvgl/src/lv_core/lv_group.c \
  src/lvgl/src/lv_core/lv_indev.c \
  src/lvgl/src/lv_core/lv_obj.c \
  src/lvgl/src/lv_core/lv_refr.c \
  src/lvgl/src/lv_core/lv_style.c \
  src/lvgl/src/lv_draw/lv_draw_arc.c \
  src/lvgl/src/lv_draw/lv_draw_basic.c \
  src/lvgl/src/lv_draw/lv_draw_img.c \
  src/lvgl/src/lv_draw/lv_draw_label.c \
  src/lvgl/src/lv_draw/lv_draw_line.c \
  src/lvgl/src/lv_draw/lv_draw_rect.c \
  src/lvgl/src/lv_draw/lv_draw_triangle.c \
  src/lvgl/src/lv_draw/lv_draw.c \
  src/lvgl/src/lv_draw/lv_img_cache.c \
  src/lvgl/src/lv_draw/lv_img_decoder.c \
  src/lvgl/src/lv_font/lv_font_fmt_txt.c \
  src/lvgl/src/lv_font/lv_font_roboto_12.c \
  src/lvgl/src/lv_font/lv_font_roboto_16.c \
  src/lvgl/src/lv_font/lv_font_roboto_22.c \
  src/lvgl/src/lv_font/lv_font_roboto_28.c \
  src/lvgl/src/lv_font/lv_font_unscii_8.c \
  src/lvgl/src/lv_font/lv_font.c \
  src/lvgl/src/lv_hal/lv_hal_disp.c \
  src/lvgl/src/lv_hal/lv_hal_indev.c \
  src/lvgl/src/lv_hal/lv_hal_tick.c \
  src/lvgl/src/lv_misc/lv_anim.c \
  src/lvgl/src/lv_misc/lv_area.c \
  src/lvgl/src/lv_misc/lv_async.c \
  src/lvgl/src/lv_misc/lv_circ.c \
  src/lvgl/src/lv_misc/lv_color.c \
  src/lvgl/src/lv_misc/lv_fs.c \
  src/lvgl/src/lv_misc/lv_gc.c \
  src/lvgl/src/lv_misc/lv_ll.c \
  src/lvgl/src/lv_misc/lv_log.c \
  src/lvgl/src/lv_misc/lv_math.c \
  src/lvgl/src/lv_misc/lv_mem.c \
  src/lvgl/src/lv_misc/lv_printf.c \
  src/lvgl/src/lv_misc/lv_task.c \
  src/lvgl/src/lv_misc/lv_txt.c \
  src/lvgl/src/lv_misc/lv_utils.c \
  src/lvgl/src/lv_objx/lv_arc.c \
  src/lvgl/src/lv_objx/lv_bar.c \
  src/lvgl/src/lv_objx/lv_btn.c \
  src/lvgl/src/lv_objx/lv_btnm.c \
  src/lvgl/src/lv_objx/lv_calendar.c \
  src/lvgl/src/lv_objx/lv_canvas.c \
  src/lvgl/src/lv_objx/lv_cb.c \
  src/lvgl/src/lv_objx/lv_chart.c \
  src/lvgl/src/lv_objx/lv_cont.c \
  src/lvgl/src/lv_objx/lv_ddlist.c \
  src/lvgl/src/lv_objx/lv_gauge.c \
  src/lvgl/src/lv_objx/lv_img.c \
  src/lvgl/src/lv_objx/lv_imgbtn.c \
  src/lvgl/src/lv_objx/lv_kb.c \
  src/lvgl/src/lv_objx/lv_label.c \
  src/lvgl/src/lv_objx/lv_led.c \
  src/lvgl/src/lv_objx/lv_line.c \
  src/lvgl/src/lv_objx/lv_list.c \
  src/lvgl/src/lv_objx/lv_lmeter.c \
  src/lvgl/src/lv_objx/lv_mbox.c \
  src/lvgl/src/lv_objx/lv_objx_templ.c \
  src/lvgl/src/lv_objx/lv_page.c \
  src/lvgl/src/lv_objx/lv_preload.c \
  src/lvgl/src/lv_objx/lv_roller.c \
  src/lvgl/src/lv_objx/lv_slider.c \
  src/lvgl/src/lv_objx/lv_spinbox.c \
  src/lvgl/src/lv_objx/lv_sw.c \
  src/lvgl/src/lv_objx/lv_ta.c \
  src/lvgl/src/lv_objx/lv_table.c \
  src/lvgl/src/lv_objx/lv_tabview.c \
  src/lvgl/src/lv_objx/lv_tileview.c \
  src/lvgl/src/lv_objx/lv_win.c \
  src/lvgl/src/lv_themes/lv_theme_alien.c \
  src/lvgl/src/lv_themes/lv_theme_default.c \
  src/lvgl/src/lv_themes/lv_theme_material.c \
  src/lvgl/src/lv_themes/lv_theme_mono.c \
  src/lvgl/src/lv_themes/lv_theme_nemo.c \
  src/lvgl/src/lv_themes/lv_theme_night.c \
  src/lvgl/src/lv_themes/lv_theme_templ.c \
  src/lvgl/src/lv_themes/lv_theme_zen.c \
  src/lvgl/src/lv_themes/lv_theme.c \
  src/lv_drivers/indev/FT5406EE8.c \
  src/lv_drivers/indev/keyboard.c \
  src/lv_drivers/indev/mouse.c \
  src/lv_drivers/indev/mousewheel.c \
  src/lv_drivers/indev/evdev.c \
  src/lv_drivers/indev/XPT2046.c \
  src/lv_drivers/display/fbdev.c \
  src/lv_drivers/display/monitor.c \
  src/lv_drivers/display/R61581.c \
  src/lv_drivers/display/SSD1963.c \
  src/lv_drivers/display/ST7565.c \
  src/p44utils/lvgl.cpp \
  src/p44utils/lvgl.hpp \
  src/p44utils/lvglui.cpp \
  src/p44utils/lvglui.hpp


LVGL_FLAGS = \
  -D ENABLE_LVGL=1 \
  -I ${srcdir}/src/lvgl/src/lv_core \
  -I ${srcdir}/src/lvgl/src/lv_misc \
  -I ${srcdir}/src/lvgl/src/lv_themes \
  -I ${srcdir}/src/lvgl/src/lv_font \
  -I ${srcdir}/src/lvgl/src/lv_hal \
  -I ${srcdir}/src/lvgl/src/lv_draw \
  -I ${srcdir}/src/lvgl/src/lv_objx \
  -I ${srcdir}/src/lv_drivers \
  -I ${srcdir}/src/lv_drivers/indev \
  -I ${srcdir}/src/lv_drivers/display \
  -I ${srcdir}/src/lv_examples/lv_apps/demo

else

LVGL_FLAGS = -D ENABLE_LVGL=0

endif


# main program

vdcd_LDADD = \
  ${PTHREAD_CFLAGS} \
  ${PTHREAD_LIBS} \
  ${JSONC_LIBS} \
  ${SQLITE3_LIBS} \
  ${PROTOBUFC_LIBS} \
  ${AVAHI_LIBS} \
  ${OLA_LIBS} \
  ${PNG_LIBS} \
  ${UBUS_LIBS} \
  ${I2C_LIBS} \
  ${LIBMODBUS_LIBS} \
  ${EV_LIBS} \
  ${UWSC_LIBS} \
  ${RPIWS281X_LIBS} \
  ${RRDB_LIBS}

# Note: no programmatic SSL lib loading in civetweb (NO_SSL_DL)
# Note: we still use old-style global placeholders (BOOST_BIND_GLOBAL_PLACEHOLDERS)
vdcd_EXTRACFLAGS = -D NO_SSL_DL=1 -D BOOST_BIND_GLOBAL_PLACEHOLDERS=1


vdcd_CPPFLAGS = \
  -I ${srcdir}/src \
  -I ${srcdir}/src/p44utils \
  -I ${srcdir}/src/p44lrgraphics \
  -I ${srcdir}/src/p44utils/thirdparty/civetweb \
  -I ${srcdir}/src/p44utils/thirdparty/slre \
  -idirafter ${srcdir}/src/p44utils/thirdparty \
  -I ${srcdir}/src/p44vdc/pbuf/gen \
  -I ${srcdir}/src/p44vdc/vdc_common \
  -I ${srcdir}/src/p44vdc/behaviours \
  -I ${srcdir}/src/p44vdc/deviceclasses/simpleio \
  -I ${srcdir}/src/p44vdc/deviceclasses/bridge \
  -I ${srcdir}/src/p44vdc/deviceclasses/proxy \
  -I ${srcdir}/src/p44vdc/deviceclasses/custom \
  -I ${srcdir}/src/p44vdc/deviceclasses/evaluator \
  -I ${srcdir}/src/p44vdc/deviceclasses/enocean \
  -I ${srcdir}/src/p44vdc/deviceclasses/dmx \
  -I ${srcdir}/src/p44vdc/deviceclasses/ds485 \
  -I ${srcdir}/src/p44vdc/deviceclasses/eldat \
  -I ${srcdir}/src/p44vdc/deviceclasses/zf \
  -I ${srcdir}/src/p44vdc/deviceclasses/dali \
  -I ${srcdir}/src/p44vdc/deviceclasses/hue \
  -I ${srcdir}/src/p44vdc/deviceclasses/ledchain \
  ${RRDB_FLAGS} \
  ${P44FEATURES_FLAGS} \
  ${RPIWS281X_FLAGS} \
  ${OLA_FLAGS} \
  ${PNG_FLAGS} \
  ${UBUS_FLAGS} \
  ${I2C_FLAGS} \
  ${MODBUSSUPP_FLAGS} \
  ${DS485SUPP_FLAGS} \
  ${EV_FLAGS} \
  ${UWSC_FLAGS} \
  ${LVGL_FLAGS} \
  ${BOOST_CPPFLAGS} \
  ${PTHREAD_CFLAGS} \
  ${JSONC_CFLAGS} \
  ${PNG_CFLAGS} \
  ${LIBMODBUS_CFLAGS} \
  ${SQLITE3_CFLAGS} \
  ${PROTOBUFC_CFLAGS} \
  ${AVAHI_CFLAGS} \
  ${vdcd_EXTRACFLAGS} \
  ${vdcd_DEBUG} \
  ${vdcd_VERSION} \
  ${vdcd_PLATFORM} \
  ${vdcd_SHORTBREAK}


vdcd_SOURCES = \
  ${P44FEATURES_SOURCES} \
  ${RPIWS281X_SOURCES} \
  ${UBUS_SOURCES} \
  ${LIBMODBUS_SOURCES} \
  ${UWSC_SOURCES} \
  ${LVGL_SOURCES} \
  ${MODBUSSUPP_SOURCES} \
  ${DS485SUPP_SOURCES} \
  src/p44utils/analogio.cpp \
  src/p44utils/analogio.hpp \
  src/p44utils/application.cpp \
  src/p44utils/application.hpp \
  src/p44utils/colorutils.cpp \
  src/p44utils/colorutils.hpp \
  src/p44utils/consolekey.cpp \
  src/p44utils/consolekey.hpp \
  src/p44utils/crc32.cpp \
  src/p44utils/crc32.hpp \
  src/p44utils/dcmotor.cpp \
  src/p44utils/dcmotor.hpp \
  src/p44utils/digitalio.cpp \
  src/p44utils/digitalio.hpp \
  src/p44utils/dnssd.cpp \
  src/p44utils/dnssd.hpp \
  src/p44utils/error.cpp \
  src/p44utils/error.hpp \
  src/p44utils/extutils.cpp \
  src/p44utils/extutils.hpp \
  src/p44utils/fdcomm.cpp \
  src/p44utils/fdcomm.hpp \
  src/p44utils/fixpoint_macros.h \
  src/p44utils/fnv.cpp \
  src/p44utils/fnv.hpp \
  src/p44utils/gpio.cpp \
  src/p44utils/gpio.h \
  src/p44utils/gpio.hpp \
  src/p44utils/httpcomm.cpp\
  src/p44utils/httpcomm.hpp\
  src/p44utils/i2c.cpp \
  src/p44utils/i2c.hpp \
  src/p44utils/igmp.cpp \
  src/p44utils/igmp.hpp \
  src/p44utils/iopin.cpp \
  src/p44utils/iopin.hpp \
  src/p44utils/jsoncomm.cpp \
  src/p44utils/jsoncomm.hpp \
  src/p44utils/jsonobject.cpp \
  src/p44utils/jsonobject.hpp \
  src/p44utils/jsonrpccomm.cpp \
  src/p44utils/jsonrpccomm.hpp \
  src/p44utils/jsonwebclient.cpp\
  src/p44utils/jsonwebclient.hpp\
  src/p44utils/ledchaincomm.cpp \
  src/p44utils/ledchaincomm.hpp \
  src/p44utils/logger.cpp \
  src/p44utils/logger.hpp \
  src/p44utils/macaddress.cpp \
  src/p44utils/macaddress.hpp \
  src/p44utils/mainloop.cpp \
  src/p44utils/mainloop.hpp \
  src/p44utils/midi.cpp \
  src/p44utils/midi.hpp \
  src/p44utils/operationqueue.cpp \
  src/p44utils/operationqueue.hpp \
  src/p44utils/p44obj.cpp \
  src/p44utils/p44obj.hpp \
  src/p44utils/p44script.cpp \
  src/p44utils/p44script.hpp \
  src/p44utils/p44utils_common.hpp \
  src/p44utils/p44utils_defs.hpp \
  src/p44utils/p44utils_minimal.hpp \
  src/p44utils/persistentparams.cpp \
  src/p44utils/persistentparams.hpp \
  src/p44utils/pwm.cpp \
  src/p44utils/pwm.hpp \
  src/p44utils/regexp.cpp \
  src/p44utils/regexp.hpp \
  src/p44utils/serialcomm.cpp \
  src/p44utils/serialcomm.hpp \
  src/p44utils/serialqueue.cpp \
  src/p44utils/serialqueue.hpp \
  src/p44utils/socketcomm.cpp \
  src/p44utils/socketcomm.hpp \
  src/p44utils/spi.cpp \
  src/p44utils/spi.hpp \
  src/p44utils/sqlite3persistence.cpp \
  src/p44utils/sqlite3persistence.hpp \
  src/p44utils/ssdpsearch.cpp \
  src/p44utils/ssdpsearch.hpp \
  src/p44utils/timeutils.cpp \
  src/p44utils/timeutils.hpp \
  src/p44utils/tlv.cpp \
  src/p44utils/tlv.hpp \
  src/p44utils/utils.cpp \
  src/p44utils/utils.hpp \
  src/p44utils/valueanimator.cpp \
  src/p44utils/valueanimator.hpp \
  src/p44utils/valueunits.cpp \
  src/p44utils/valueunits.hpp \
  src/p44utils/thirdparty/civetweb/civetweb.c \
  src/p44utils/thirdparty/civetweb/civetweb.h \
  src/p44utils/thirdparty/civetweb/handle_form.inl \
  src/p44utils/thirdparty/civetweb/md5.inl \
  src/p44utils/thirdparty/civetweb/sha1.inl \
  src/p44utils/thirdparty/slre/slre.c \
  src/p44utils/thirdparty/slre/slre.h \
  src/p44utils/thirdparty/sqlite3pp/sqlite3pp.cpp \
  src/p44utils/thirdparty/sqlite3pp/sqlite3pp.h \
  src/p44utils/thirdparty/sqlite3pp/sqlite3ppext.cpp \
  src/p44utils/thirdparty/sqlite3pp/sqlite3ppext.h \
  src/p44lrgraphics/blocksview.cpp \
  src/p44lrgraphics/blocksview.hpp \
  src/p44lrgraphics/canvasview.cpp \
  src/p44lrgraphics/canvasview.hpp \
  src/p44lrgraphics/coloreffectview.cpp \
  src/p44lrgraphics/coloreffectview.hpp \
  src/p44lrgraphics/epxview.cpp \
  src/p44lrgraphics/epxview.hpp \
  src/p44lrgraphics/fonts.cpp \
  src/p44lrgraphics/fonts.hpp \
  src/p44lrgraphics/imageview.cpp \
  src/p44lrgraphics/imageview.hpp \
  src/p44lrgraphics/lifeview.cpp \
  src/p44lrgraphics/lifeview.hpp \
  src/p44lrgraphics/lightspotview.cpp \
  src/p44lrgraphics/lightspotview.hpp \
  src/p44lrgraphics/p44lrg_common.hpp \
  src/p44lrgraphics/p44view.cpp \
  src/p44lrgraphics/p44view.hpp \
  src/p44lrgraphics/textview.cpp \
  src/p44lrgraphics/textview.hpp \
  src/p44lrgraphics/torchview.cpp \
  src/p44lrgraphics/torchview.hpp \
  src/p44lrgraphics/viewfactory.cpp \
  src/p44lrgraphics/viewfactory.hpp \
  src/p44lrgraphics/viewscroller.cpp \
  src/p44lrgraphics/viewscroller.hpp \
  src/p44lrgraphics/viewsequencer.cpp \
  src/p44lrgraphics/viewsequencer.hpp \
  src/p44lrgraphics/viewstack.cpp \
  src/p44lrgraphics/viewstack.hpp \
  src/p44lrgraphics/fonts/font_3x3.cpp \
  src/p44lrgraphics/fonts/font_3x4.cpp \
  src/p44lrgraphics/fonts/font_5x5.cpp \
  src/p44lrgraphics/fonts/font_5x7.cpp \
  src/p44lrgraphics/fonts/font_5x8.cpp \
  src/p44lrgraphics/fonts/font_bios.cpp \
  src/p44lrgraphics/fonts/font_m3x6.cpp \
  src/p44lrgraphics/fonts/font_m5x7.cpp \
  src/p44lrgraphics/fonts/font_m6x11.cpp \
  src/p44lrgraphics/fonts/font_sixtyfour.cpp \
  src/p44lrgraphics/fonts/font_vcr_osd_mono.cpp \
  src/p44vdc/vdc_common/dsbehaviour.cpp \
  src/p44vdc/vdc_common/dsbehaviour.hpp \
  src/p44vdc/vdc_common/outputbehaviour.cpp \
  src/p44vdc/vdc_common/outputbehaviour.hpp \
  src/p44vdc/vdc_common/channelbehaviour.cpp \
  src/p44vdc/vdc_common/channelbehaviour.hpp \
  src/p44vdc/vdc_common/dsscene.cpp \
  src/p44vdc/vdc_common/dsscene.hpp \
  src/p44vdc/vdc_common/simplescene.cpp \
  src/p44vdc/vdc_common/simplescene.hpp \
  src/p44vdc/vdc_common/device.cpp \
  src/p44vdc/vdc_common/device.hpp \
  src/p44vdc/vdc_common/singledevice.cpp \
  src/p44vdc/vdc_common/singledevice.hpp \
  src/p44vdc/vdc_common/devicesettings.cpp \
  src/p44vdc/vdc_common/devicesettings.hpp \
  src/p44vdc/vdc_common/valuesource.cpp \
  src/p44vdc/vdc_common/valuesource.hpp \
  src/p44vdc/vdc_common/valuedescriptor.cpp \
  src/p44vdc/vdc_common/valuedescriptor.hpp \
  src/p44vdc/vdc_common/propertycontainer.cpp \
  src/p44vdc/vdc_common/propertycontainer.hpp \
  src/p44vdc/vdc_common/pbufvdcapi.cpp \
  src/p44vdc/vdc_common/pbufvdcapi.hpp \
  src/p44vdc/vdc_common/jsonvdcapi.cpp \
  src/p44vdc/vdc_common/jsonvdcapi.hpp \
  src/p44vdc/vdc_common/vdcapi.cpp \
  src/p44vdc/vdc_common/vdcapi.hpp \
  src/p44vdc/vdc_common/apivalue.cpp \
  src/p44vdc/vdc_common/apivalue.hpp \
  src/p44vdc/vdc_common/dsaddressable.cpp \
  src/p44vdc/vdc_common/dsaddressable.hpp \
  src/p44vdc/vdc_common/vdc.cpp \
  src/p44vdc/vdc_common/vdc.hpp \
  src/p44vdc/vdc_common/vdchost.cpp \
  src/p44vdc/vdc_common/vdchost.hpp \
  src/p44vdc/vdc_common/localcontroller.cpp \
  src/p44vdc/vdc_common/localcontroller.hpp \
  src/p44vdc/vdc_common/discovery.cpp \
  src/p44vdc/vdc_common/discovery.hpp \
  src/p44vdc/vdc_common/dsdefs.h \
  src/p44vdc/vdc_common/dsuid.cpp \
  src/p44vdc/vdc_common/dsuid.hpp \
  src/p44vdc/vdc_common/p44_vdcd_host.cpp \
  src/p44vdc/vdc_common/p44_vdcd_host.hpp \
  src/p44vdc/vdc_common/p44vdc_common.hpp \
  src/p44vdc/behaviours/climatecontrolbehaviour.cpp \
  src/p44vdc/behaviours/climatecontrolbehaviour.hpp \
  src/p44vdc/behaviours/ventilationbehaviour.cpp \
  src/p44vdc/behaviours/ventilationbehaviour.hpp \
  src/p44vdc/behaviours/audiobehaviour.cpp \
  src/p44vdc/behaviours/audiobehaviour.hpp \
  src/p44vdc/behaviours/videobehaviour.cpp \
  src/p44vdc/behaviours/videobehaviour.hpp \
  src/p44vdc/behaviours/shadowbehaviour.cpp \
  src/p44vdc/behaviours/shadowbehaviour.hpp \
  src/p44vdc/behaviours/buttonbehaviour.hpp \
  src/p44vdc/behaviours/buttonbehaviour.cpp \
  src/p44vdc/behaviours/sensorbehaviour.hpp \
  src/p44vdc/behaviours/sensorbehaviour.cpp \
  src/p44vdc/behaviours/binaryinputbehaviour.hpp \
  src/p44vdc/behaviours/binaryinputbehaviour.cpp \
  src/p44vdc/behaviours/lightbehaviour.cpp \
  src/p44vdc/behaviours/lightbehaviour.hpp \
  src/p44vdc/behaviours/colorlightbehaviour.cpp \
  src/p44vdc/behaviours/colorlightbehaviour.hpp \
  src/p44vdc/behaviours/movinglightbehaviour.cpp \
  src/p44vdc/behaviours/movinglightbehaviour.hpp \
  src/p44vdc/deviceclasses/simpleio/consoledevice.cpp \
  src/p44vdc/deviceclasses/simpleio/consoledevice.hpp \
  src/p44vdc/deviceclasses/simpleio/mystromdevice.cpp \
  src/p44vdc/deviceclasses/simpleio/mystromdevice.hpp \
  src/p44vdc/deviceclasses/simpleio/digitaliodevice.cpp \
  src/p44vdc/deviceclasses/simpleio/digitaliodevice.hpp \
  src/p44vdc/deviceclasses/simpleio/analogiodevice.cpp \
  src/p44vdc/deviceclasses/simpleio/analogiodevice.hpp \
  src/p44vdc/deviceclasses/simpleio/staticvdc.cpp \
  src/p44vdc/deviceclasses/simpleio/staticvdc.hpp \
  src/p44vdc/deviceclasses/custom/customdevice.cpp \
  src/p44vdc/deviceclasses/custom/customdevice.hpp \
  src/p44vdc/deviceclasses/custom/externalvdc.cpp \
  src/p44vdc/deviceclasses/custom/externalvdc.hpp \
  src/p44vdc/deviceclasses/custom/scriptedvdc.cpp \
  src/p44vdc/deviceclasses/custom/scriptedvdc.hpp \
  src/p44vdc/deviceclasses/evaluator/evaluatorvdc.cpp \
  src/p44vdc/deviceclasses/evaluator/evaluatorvdc.hpp \
  src/p44vdc/deviceclasses/evaluator/evaluatordevice.cpp \
  src/p44vdc/deviceclasses/evaluator/evaluatordevice.hpp \
  src/p44vdc/deviceclasses/bridge/bridgevdc.cpp \
  src/p44vdc/deviceclasses/bridge/bridgevdc.hpp \
  src/p44vdc/deviceclasses/bridge/bridgedevice.cpp \
  src/p44vdc/deviceclasses/bridge/bridgedevice.hpp \
  src/p44vdc/deviceclasses/dmx/dmxdevice.cpp \
  src/p44vdc/deviceclasses/dmx/dmxdevice.hpp \
  src/p44vdc/deviceclasses/dmx/dmxvdc.cpp \
  src/p44vdc/deviceclasses/dmx/dmxvdc.hpp \
  src/p44vdc/deviceclasses/ds485/ds485comm.cpp \
  src/p44vdc/deviceclasses/ds485/ds485comm.hpp \
  src/p44vdc/deviceclasses/ds485/ds485device.cpp \
  src/p44vdc/deviceclasses/ds485/ds485device.hpp \
  src/p44vdc/deviceclasses/ds485/ds485vdc.cpp \
  src/p44vdc/deviceclasses/ds485/ds485vdc.hpp \
  src/p44vdc/deviceclasses/proxy/proxyvdc.cpp \
  src/p44vdc/deviceclasses/proxy/proxyvdc.hpp \
  src/p44vdc/deviceclasses/proxy/proxydevice.cpp \
  src/p44vdc/deviceclasses/proxy/proxydevice.hpp \
  src/p44vdc/deviceclasses/proxy/p44bridgeapi.cpp \
  src/p44vdc/deviceclasses/proxy/p44bridgeapi.hpp \
  src/p44vdc/deviceclasses/enocean/enoceancomm.cpp \
  src/p44vdc/deviceclasses/enocean/enoceancomm.hpp \
  src/p44vdc/deviceclasses/enocean/enoceandevice.cpp \
  src/p44vdc/deviceclasses/enocean/enoceandevice.hpp \
  src/p44vdc/deviceclasses/enocean/enoceanremotecontrol.cpp \
  src/p44vdc/deviceclasses/enocean/enoceanremotecontrol.hpp \
  src/p44vdc/deviceclasses/enocean/enoceaninputhandler.cpp \
  src/p44vdc/deviceclasses/enocean/enoceaninputhandler.hpp \
  src/p44vdc/deviceclasses/enocean/enoceanrps.cpp \
  src/p44vdc/deviceclasses/enocean/enoceanrps.hpp \
  src/p44vdc/deviceclasses/enocean/enocean4bs.cpp \
  src/p44vdc/deviceclasses/enocean/enocean4bs.hpp \
  src/p44vdc/deviceclasses/enocean/enocean1bs.cpp \
  src/p44vdc/deviceclasses/enocean/enocean1bs.hpp \
  src/p44vdc/deviceclasses/enocean/enoceanvld.cpp \
  src/p44vdc/deviceclasses/enocean/enoceanvld.hpp \
  src/p44vdc/deviceclasses/enocean/enoceanvdc.cpp \
  src/p44vdc/deviceclasses/enocean/enoceanvdc.hpp \
  src/p44vdc/deviceclasses/eldat/eldatcomm.cpp \
  src/p44vdc/deviceclasses/eldat/eldatcomm.hpp \
  src/p44vdc/deviceclasses/eldat/eldatdevice.cpp \
  src/p44vdc/deviceclasses/eldat/eldatdevice.hpp \
  src/p44vdc/deviceclasses/eldat/eldatvdc.cpp \
  src/p44vdc/deviceclasses/eldat/eldatvdc.hpp \
  src/p44vdc/deviceclasses/zf/zfcomm.cpp \
  src/p44vdc/deviceclasses/zf/zfcomm.hpp \
  src/p44vdc/deviceclasses/zf/zfdevice.cpp \
  src/p44vdc/deviceclasses/zf/zfdevice.hpp \
  src/p44vdc/deviceclasses/zf/zfvdc.cpp \
  src/p44vdc/deviceclasses/zf/zfvdc.hpp \
  src/p44vdc/deviceclasses/dali/dalicomm.cpp \
  src/p44vdc/deviceclasses/dali/dalicomm.hpp \
  src/p44vdc/deviceclasses/dali/dalidefs.h \
  src/p44vdc/deviceclasses/dali/dalidevice.cpp \
  src/p44vdc/deviceclasses/dali/dalidevice.hpp \
  src/p44vdc/deviceclasses/dali/dalivdc.cpp \
  src/p44vdc/deviceclasses/dali/dalivdc.hpp \
  src/p44vdc/deviceclasses/hue/huecomm.cpp \
  src/p44vdc/deviceclasses/hue/huecomm.hpp \
  src/p44vdc/deviceclasses/hue/huevdc.cpp \
  src/p44vdc/deviceclasses/hue/huevdc.hpp \
  src/p44vdc/deviceclasses/hue/huedevice.cpp \
  src/p44vdc/deviceclasses/hue/huedevice.hpp \
  src/p44vdc/deviceclasses/ledchain/ledchaindevice.hpp \
  src/p44vdc/deviceclasses/ledchain/ledchaindevice.cpp \
  src/p44vdc/deviceclasses/ledchain/ledchainvdc.hpp \
  src/p44vdc/deviceclasses/ledchain/ledchainvdc.cpp \
  src/p44utils_config.hpp \
  src/p44vdc_config.hpp \
  src/p44_vdcd_main.cpp


# tests

tests_LDADD = ${PTHREAD_CFLAGS} ${PTHREAD_LIBS} ${JSONC_LIBS} ${SQLITE3_LIBS} ${PROTOBUFC_LIBS} ${AVAHI_LIBS}

tests_CPPFLAGS = \
  -I ${srcdir}/src/p44utils \
  -I ${srcdir}/src/p44utils/tests \
  -I ${srcdir}/src/p44utils/thirdparty/civetweb \
  -I ${srcdir}/src/p44utils/thirdparty \
  ${BOOST_CPPFLAGS} \
  ${PTHREAD_CFLAGS} \
  ${JSONC_CFLAGS} \
  ${SQLITE3_CFLAGS} \
  ${PROTOBUFC_CFLAGS} \
  ${AVAHI_CFLAGS}

tests_SOURCES = \
  src/p44utils/tests/catch_amalgamated.cpp \
  src/p44utils/tests/catch_amalgamated.hpp \
  src/p44utils/utils.cpp \
  src/p44utils/utils.hpp \
  src/p44utils/tests/test_utils.cpp \
  src/p44utils/macaddress.cpp \
  src/p44utils/macaddress.hpp \
  src/p44utils/tests/test_macaddress.cpp
