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
vdcd_PLATFORM = -D P44_BUILD_OW=1
else
vdcd_PLATFORM =
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

OLA_SOURCES = \
  src/p44vdc/deviceclasses/ola/oladevice.cpp \
  src/p44vdc/deviceclasses/ola/oladevice.hpp \
  src/p44vdc/deviceclasses/ola/olavdc.cpp \
  src/p44vdc/deviceclasses/ola/olavdc.hpp
OLA_FLAGS = \
  -D ENABLE_OLA=1 \
  -I ${srcdir}/src/p44vdc/deviceclasses/ola
OLA_LIBS = \
  -lola -lolacommon -lprotobuf

else

OLA_FLAGS = -D ENABLE_OLA=0

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



# Note: no programmatic SSL lib loading in civetweb (NO_SSL_DL)
vdcd_LDADD = ${PTHREAD_CFLAGS} ${PTHREAD_LIBS} ${JSONC_LIBS} ${SQLITE3_LIBS} ${PROTOBUFC_LIBS} ${AVAHI_LIBS} ${OLA_LIBS} ${PNG_LIBS} ${UBUS_LIBS} ${RPIWS281X_LIBS} ${RRDB_LIBS}
vdcd_EXTRACFLAGS = -D NO_SSL_DL=1


vdcd_CPPFLAGS = \
  -I ${srcdir}/src \
  -I ${srcdir}/src/p44utils \
  -I ${srcdir}/src/p44lrgraphics \
  -I ${srcdir}/src/p44utils/thirdparty/civetweb \
  -I ${srcdir}/src/p44utils/thirdparty/slre \
  -I ${srcdir}/src/p44utils/thirdparty \
  -I ${srcdir}/src/p44vdc/pbuf/gen \
  -I ${srcdir}/src/p44vdc/vdc_common \
  -I ${srcdir}/src/p44vdc/behaviours \
  -I ${srcdir}/src/p44vdc/deviceclasses/simpleio \
  -I ${srcdir}/src/p44vdc/deviceclasses/external \
  -I ${srcdir}/src/p44vdc/deviceclasses/evaluator \
  -I ${srcdir}/src/p44vdc/deviceclasses/enocean \
  -I ${srcdir}/src/p44vdc/deviceclasses/eldat \
  -I ${srcdir}/src/p44vdc/deviceclasses/zf \
  -I ${srcdir}/src/p44vdc/deviceclasses/dali \
  -I ${srcdir}/src/p44vdc/deviceclasses/hue \
  -I ${srcdir}/src/p44vdc/deviceclasses/ledchain \
  ${RRDB_FLAGS} \
  ${RPIWS281X_FLAGS} \
  ${OLA_FLAGS} \
  ${PNG_FLAGS} \
  ${UBUS_FLAGS} \
  ${BOOST_CPPFLAGS} \
  ${PTHREAD_CFLAGS} \
  ${JSONC_CFLAGS} \
  ${PNG_CFLAGS} \
  ${SQLITE3_CFLAGS} \
  ${PROTOBUFC_CFLAGS} \
  ${AVAHI_CFLAGS} \
  ${vdcd_EXTRACFLAGS} \
  ${vdcd_DEBUG} \
  ${vdcd_PLATFORM}


vdcd_SOURCES = \
  ${RPIWS281X_SOURCES} \
  ${OLA_SOURCES} \
  ${UBUS_SOURCES} \
  src/p44utils/p44obj.cpp \
  src/p44utils/p44obj.hpp \
  src/p44utils/application.cpp \
  src/p44utils/application.hpp \
  src/p44utils/consolekey.cpp \
  src/p44utils/consolekey.hpp \
  src/p44utils/digitalio.cpp \
  src/p44utils/digitalio.hpp \
  src/p44utils/analogio.cpp \
  src/p44utils/analogio.hpp \
  src/p44utils/error.cpp \
  src/p44utils/error.hpp \
  src/p44utils/fnv.cpp \
  src/p44utils/fnv.hpp \
  src/p44utils/gpio.cpp \
  src/p44utils/gpio.h \
  src/p44utils/gpio.hpp \
  src/p44utils/pwm.cpp \
  src/p44utils/pwm.hpp \
  src/p44utils/i2c.cpp \
  src/p44utils/i2c.hpp \
  src/p44utils/spi.cpp \
  src/p44utils/spi.hpp \
  src/p44utils/ledchaincomm.cpp \
  src/p44utils/ledchaincomm.hpp \
  src/p44utils/iopin.cpp \
  src/p44utils/iopin.hpp \
  src/p44utils/jsoncomm.cpp \
  src/p44utils/jsoncomm.hpp \
  src/p44utils/jsonobject.cpp \
  src/p44utils/jsonobject.hpp \
  src/p44utils/jsonrpccomm.cpp \
  src/p44utils/jsonrpccomm.hpp \
  src/p44utils/logger.cpp \
  src/p44utils/logger.hpp \
  src/p44utils/mainloop.cpp \
  src/p44utils/mainloop.hpp \
  src/p44utils/operationqueue.cpp \
  src/p44utils/operationqueue.hpp \
  src/p44utils/persistentparams.cpp \
  src/p44utils/persistentparams.hpp \
  src/p44utils/serialcomm.cpp \
  src/p44utils/serialcomm.hpp \
  src/p44utils/serialqueue.cpp \
  src/p44utils/serialqueue.hpp \
  src/p44utils/fdcomm.cpp \
  src/p44utils/fdcomm.hpp \
  src/p44utils/socketcomm.cpp \
  src/p44utils/socketcomm.hpp \
  src/p44utils/ssdpsearch.cpp \
  src/p44utils/ssdpsearch.hpp \
  src/p44utils/sqlite3persistence.cpp \
  src/p44utils/sqlite3persistence.hpp \
  src/p44utils/utils.cpp \
  src/p44utils/utils.hpp \
  src/p44utils/timeutils.cpp \
  src/p44utils/timeutils.hpp \
  src/p44utils/regexp.cpp \
  src/p44utils/regexp.hpp \
  src/p44utils/expressions.cpp \
  src/p44utils/expressions.hpp \
  src/p44utils/valueunits.cpp \
  src/p44utils/valueunits.hpp \
  src/p44utils/colorutils.cpp \
  src/p44utils/colorutils.hpp \
  src/p44utils/macaddress.cpp \
  src/p44utils/macaddress.hpp \
  src/p44utils/igmp.cpp \
  src/p44utils/igmp.hpp \
  src/p44utils/httpcomm.cpp\
  src/p44utils/httpcomm.hpp\
  src/p44utils/jsonwebclient.cpp\
  src/p44utils/jsonwebclient.hpp\
  src/p44utils/p44utils_common.hpp \
  src/p44utils/thirdparty/slre/slre.c \
  src/p44utils/thirdparty/slre/slre.h \
  src/p44utils/thirdparty/sqlite3pp/sqlite3pp.cpp \
  src/p44utils/thirdparty/sqlite3pp/sqlite3pp.h \
  src/p44utils/thirdparty/sqlite3pp/sqlite3ppext.cpp \
  src/p44utils/thirdparty/sqlite3pp/sqlite3ppext.h \
  src/p44utils/thirdparty/civetweb/civetweb.c \
  src/p44utils/thirdparty/civetweb/civetweb.h \
  src/p44utils/thirdparty/civetweb/handle_form.inl \
  src/p44utils/thirdparty/civetweb/md5.inl \
  src/p44utils/thirdparty/civetweb/sha1.inl \
  src/p44utils/thirdparty/civetweb/hostcheck.inl \
  src/p44utils/thirdparty/civetweb/openssl_hostname_validation.inl \
  src/p44lrgraphics/imageview.cpp \
  src/p44lrgraphics/imageview.hpp \
  src/p44lrgraphics/coloreffectview.cpp \
  src/p44lrgraphics/coloreffectview.hpp \
  src/p44lrgraphics/lightspotview.cpp \
  src/p44lrgraphics/lightspotview.hpp \
  src/p44lrgraphics/lifeview.cpp \
  src/p44lrgraphics/lifeview.hpp \
  src/p44lrgraphics/torchview.cpp \
  src/p44lrgraphics/torchview.hpp \
  src/p44lrgraphics/p44view.cpp \
  src/p44lrgraphics/p44view.hpp \
  src/p44lrgraphics/textview.cpp \
  src/p44lrgraphics/textview.hpp \
  src/p44lrgraphics/viewstack.cpp \
  src/p44lrgraphics/viewstack.hpp \
  src/p44lrgraphics/viewscroller.cpp \
  src/p44lrgraphics/viewscroller.hpp \
  src/p44lrgraphics/viewfactory.cpp \
  src/p44lrgraphics/viewfactory.hpp \
  src/p44lrgraphics/viewanimator.cpp \
  src/p44lrgraphics/viewanimator.hpp \
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
  src/p44vdc/deviceclasses/simpleio/sparkiodevice.cpp \
  src/p44vdc/deviceclasses/simpleio/sparkiodevice.hpp \
  src/p44vdc/deviceclasses/simpleio/mystromdevice.cpp \
  src/p44vdc/deviceclasses/simpleio/mystromdevice.hpp \
  src/p44vdc/deviceclasses/simpleio/digitaliodevice.cpp \
  src/p44vdc/deviceclasses/simpleio/digitaliodevice.hpp \
  src/p44vdc/deviceclasses/simpleio/analogiodevice.cpp \
  src/p44vdc/deviceclasses/simpleio/analogiodevice.hpp \
  src/p44vdc/deviceclasses/simpleio/staticvdc.cpp \
  src/p44vdc/deviceclasses/simpleio/staticvdc.hpp \
  src/p44vdc/deviceclasses/external/externalvdc.cpp \
  src/p44vdc/deviceclasses/external/externalvdc.hpp \
  src/p44vdc/deviceclasses/evaluator/evaluatorvdc.cpp \
  src/p44vdc/deviceclasses/evaluator/evaluatorvdc.hpp \
  src/p44vdc/deviceclasses/evaluator/evaluatordevice.cpp \
  src/p44vdc/deviceclasses/evaluator/evaluatordevice.hpp \
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
  src/p44utils/tests/catch.hpp \
  src/p44utils/utils.cpp \
  src/p44utils/utils.hpp \
  src/p44utils/tests/test_utils.cpp \
  src/p44utils/macaddress.cpp \
  src/p44utils/macaddress.hpp \
  src/p44utils/tests/test_macaddress.cpp \
  src/tests/vdcd_tester.cpp

