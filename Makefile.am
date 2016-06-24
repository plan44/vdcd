AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

if P44_BUILD_RPI
bin_PROGRAMS = vdcd olavdcd
else
bin_PROGRAMS = vdcd
endif

# common stuff for protobuf - NOTE: need to "make all" to get BUILT_SOURCES made

PROTOBUF_GENERATED = \
  src/libp44vdc/pbuf/gen/messages.pb-c.h \
  src/libp44vdc/pbuf/gen/messages.pb-c.c \
  src/libp44vdc/pbuf/gen/vdcapi.pb-c.h \
  src/libp44vdc/pbuf/gen/vdcapi.pb-c.c

#dist_noinst_DATA = messages.proto vdcapi.proto

MOSTLYCLEANFILES = $(PROTOBUF_GENERATED)

BUILT_SOURCES = $(PROTOBUF_GENERATED)

src/libp44vdc/pbuf/gen/messages.pb-c.h src/libp44vdc/pbuf/gen/messages.pb-c.c:	src/libp44vdc/pbuf/messages.proto
	$(PROTOC) --proto_path=src/libp44vdc/pbuf --c_out=src/libp44vdc/pbuf/gen src/libp44vdc/pbuf/messages.proto

src/libp44vdc/pbuf/gen/vdcapi.pb-c.h src/libp44vdc/pbuf/gen/vdcapi.pb-c.c:	src/libp44vdc/pbuf/vdcapi.proto
	$(PROTOC) --proto_path=src/libp44vdc/pbuf --c_out=src/libp44vdc/pbuf/gen src/libp44vdc/pbuf/vdcapi.proto


# vdcd

if DEBUG
vdcd_DEBUG = -D DEBUG=1
else
vdcd_DEBUG =
endif

if P44_BUILD_RPI
vdcd_PLATFORM = -D P44_BUILD_RPI=1
else
if P44_BUILD_OW
vdcd_PLATFORM = -D P44_BUILD_OW=1
else
vdcd_PLATFORM =
endif
endif


# enable some extras
vdcd_EXTRAS = -D ENABLE_VOXNET=1 -D ENABLE_VZUGHOME=1


nodist_vdcd_SOURCES = $(PROTOBUF_GENERATED)


if P44_BUILD_RPI

# hacks required for current cross-build environment for RPi, which is not a proper buildroot yet

# This is an ugly hack, but I could not find a way to refer to the sysroot prefix for the -L path.
# Note: the entire library situation is ugly, as toolchain is not complete and autoconf lib macros dont work.
TOOLCHAIN_SYSROOT = /Volumes/xtools/arm-none-linux-gnueabi/arm-none-linux-gnueabi/sysroot
vdcd_LDADD = $(PTHREAD_LIBS) -lmongoose -lavahi-core  -lprotobuf-c -lsqlite3 -ljson-c -ldl -L${TOOLCHAIN_SYSROOT}/${libdir}/arm-linux-gnueabihf -lcrypto -lz
# JSONC_CFLAGS is not automatically set in RPi build because config check is disabled, set include path here
# AVAHI_CFLAGS is not automatically set in RPi build because config check is disabled, set include path here
vdcd_EXTRACFLAGS = \
  -I${TOOLCHAIN_SYSROOT}/usr/local/include/json-c \
  -I ${srcdir}/../libavahi-core


# sources only supported on RPI
LEDCHAIN_SRC = \
  src/libp44vdc/thirdparty/rpi_ws281x/clk.h \
  src/libp44vdc/thirdparty/rpi_ws281x/gpio.h \
  src/libp44vdc/thirdparty/rpi_ws281x/dma.h \
  src/libp44vdc/thirdparty/rpi_ws281x/dma.c \
  src/libp44vdc/thirdparty/rpi_ws281x/pwm.h \
  src/libp44vdc/thirdparty/rpi_ws281x/pwm.c \
  src/libp44vdc/thirdparty/rpi_ws281x/board_info.h \
  src/libp44vdc/thirdparty/rpi_ws281x/board_info.c \
  src/libp44vdc/thirdparty/rpi_ws281x/mailbox.h \
  src/libp44vdc/thirdparty/rpi_ws281x/mailbox.c \
  src/libp44vdc/thirdparty/rpi_ws281x/ws2811.h \
  src/libp44vdc/thirdparty/rpi_ws281x/ws2811.c
LEDCHAIN_INCLUDES = \
  -I ${srcdir}/src/libp44vdc/thirdparty/rpi_ws281x

else

# proper buildroot and autotools

vdcd_LDADD = $(PTHREAD_LIBS) $(JSONC_LIBS) $(SQLITE3_LIBS) $(PROTOBUFC_LIBS) $(AVAHI_LIBS)

# build mongoose into vdcd, no library
MONGOOSE_SRC = src/libp44vdc/thirdparty/mongoose/mongoose.c src/libp44vdc/thirdparty/mongoose/mongoose.h

endif


vdcd_CXXFLAGS = \
  -I ${srcdir}/src/p44utils \
  -I ${srcdir}/src \
  -I ${srcdir}/src/libp44vdc/thirdparty/mongoose \
  -I ${srcdir}/src/libp44vdc/thirdparty \
  -I ${srcdir}/src/libp44vdc/pbuf/gen \
  -I ${srcdir}/src/libp44vdc/vdc_common \
  -I ${srcdir}/src/libp44vdc/behaviours \
  -I ${srcdir}/src/libp44vdc/deviceclasses/simpleio \
  -I ${srcdir}/src/libp44vdc/deviceclasses/external \
  -I ${srcdir}/src/libp44vdc/deviceclasses/evaluator \
  -I ${srcdir}/src/libp44vdc/deviceclasses/enocean \
  -I ${srcdir}/src/libp44vdc/deviceclasses/dali \
  -I ${srcdir}/src/libp44vdc/deviceclasses/hue \
  -I ${srcdir}/src/libp44vdc/deviceclasses/ledchain \
  -I ${srcdir}/src/deviceclasses/vzughome \
  -I ${srcdir}/src/deviceclasses/voxnet \
  ${LEDCHAIN_INCLUDES} \
  ${BOOST_CPPFLAGS} \
  ${PTHREAD_CFLAGS} \
  ${JSONC_CFLAGS} \
  ${SQLITE3_CFLAGS} \
  ${PROTOBUFC_CFLAGS} \
  ${AVAHI_CFLAGS} \
  ${vdcd_EXTRACFLAGS} \
  ${vdcd_DEBUG} \
  ${vdcd_PLATFORM} \
  ${vdcd_EXTRAS} \
  -D ENABLE_OLA=0


vdcd_SOURCES = \
  ${MONGOOSE_SRC} \
  ${LEDCHAIN_SRC} \
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
  src/p44utils/i2c.cpp \
  src/p44utils/i2c.hpp \
  src/p44utils/spi.cpp \
  src/p44utils/spi.hpp \
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
  src/p44utils/p44_common.hpp \
  src/libp44vdc/thirdparty/sqlite3pp/sqlite3pp.cpp \
  src/libp44vdc/thirdparty/sqlite3pp/sqlite3pp.h \
  src/libp44vdc/thirdparty/sqlite3pp/sqlite3ppext.cpp \
  src/libp44vdc/thirdparty/sqlite3pp/sqlite3ppext.h \
  src/libp44vdc/vdc_common/dsbehaviour.cpp \
  src/libp44vdc/vdc_common/dsbehaviour.hpp \
  src/libp44vdc/vdc_common/outputbehaviour.cpp \
  src/libp44vdc/vdc_common/outputbehaviour.hpp \
  src/libp44vdc/vdc_common/channelbehaviour.cpp \
  src/libp44vdc/vdc_common/channelbehaviour.hpp \
  src/libp44vdc/vdc_common/dsscene.cpp \
  src/libp44vdc/vdc_common/dsscene.hpp \
  src/libp44vdc/vdc_common/simplescene.cpp \
  src/libp44vdc/vdc_common/simplescene.hpp \
  src/libp44vdc/vdc_common/device.cpp \
  src/libp44vdc/vdc_common/device.hpp \
  src/libp44vdc/vdc_common/devicesettings.cpp \
  src/libp44vdc/vdc_common/devicesettings.hpp \
  src/libp44vdc/vdc_common/valuesource.cpp \
  src/libp44vdc/vdc_common/valuesource.hpp \
  src/libp44vdc/vdc_common/propertycontainer.cpp \
  src/libp44vdc/vdc_common/propertycontainer.hpp \
  src/libp44vdc/vdc_common/pbufvdcapi.cpp \
  src/libp44vdc/vdc_common/pbufvdcapi.hpp \
  src/libp44vdc/vdc_common/jsonvdcapi.cpp \
  src/libp44vdc/vdc_common/jsonvdcapi.hpp \
  src/libp44vdc/vdc_common/vdcapi.cpp \
  src/libp44vdc/vdc_common/vdcapi.hpp \
  src/libp44vdc/vdc_common/apivalue.cpp \
  src/libp44vdc/vdc_common/apivalue.hpp \
  src/libp44vdc/vdc_common/dsaddressable.cpp \
  src/libp44vdc/vdc_common/dsaddressable.hpp \
  src/libp44vdc/vdc_common/vdc.cpp \
  src/libp44vdc/vdc_common/vdc.hpp \
  src/libp44vdc/vdc_common/vdchost.cpp \
  src/libp44vdc/vdc_common/vdchost.hpp \
  src/libp44vdc/vdc_common/discovery.cpp \
  src/libp44vdc/vdc_common/discovery.hpp \
  src/libp44vdc/vdc_common/dsdefs.h \
  src/libp44vdc/vdc_common/dsuid.cpp \
  src/libp44vdc/vdc_common/dsuid.hpp \
  src/libp44vdc/vdc_common/vdcd_common.hpp \
  src/libp44vdc/behaviours/climatecontrolbehaviour.cpp \
  src/libp44vdc/behaviours/climatecontrolbehaviour.hpp \
  src/libp44vdc/behaviours/audiobehaviour.cpp \
  src/libp44vdc/behaviours/audiobehaviour.hpp \
  src/libp44vdc/behaviours/shadowbehaviour.cpp \
  src/libp44vdc/behaviours/shadowbehaviour.hpp \
  src/libp44vdc/behaviours/buttonbehaviour.hpp \
  src/libp44vdc/behaviours/buttonbehaviour.cpp \
  src/libp44vdc/behaviours/sensorbehaviour.hpp \
  src/libp44vdc/behaviours/sensorbehaviour.cpp \
  src/libp44vdc/behaviours/binaryinputbehaviour.hpp \
  src/libp44vdc/behaviours/binaryinputbehaviour.cpp \
  src/libp44vdc/behaviours/lightbehaviour.cpp \
  src/libp44vdc/behaviours/lightbehaviour.hpp \
  src/libp44vdc/behaviours/colorlightbehaviour.cpp \
  src/libp44vdc/behaviours/colorlightbehaviour.hpp \
  src/libp44vdc/behaviours/movinglightbehaviour.cpp \
  src/libp44vdc/behaviours/movinglightbehaviour.hpp \
  src/libp44vdc/deviceclasses/simpleio/consoledevice.cpp \
  src/libp44vdc/deviceclasses/simpleio/consoledevice.hpp \
  src/libp44vdc/deviceclasses/simpleio/sparkiodevice.cpp \
  src/libp44vdc/deviceclasses/simpleio/sparkiodevice.hpp \
  src/libp44vdc/deviceclasses/simpleio/mystromdevice.cpp \
  src/libp44vdc/deviceclasses/simpleio/mystromdevice.hpp \
  src/libp44vdc/deviceclasses/simpleio/digitaliodevice.cpp \
  src/libp44vdc/deviceclasses/simpleio/digitaliodevice.hpp \
  src/libp44vdc/deviceclasses/simpleio/analogiodevice.cpp \
  src/libp44vdc/deviceclasses/simpleio/analogiodevice.hpp \
  src/libp44vdc/deviceclasses/simpleio/staticvdc.cpp \
  src/libp44vdc/deviceclasses/simpleio/staticvdc.hpp \
  src/libp44vdc/deviceclasses/external/externalvdc.cpp \
  src/libp44vdc/deviceclasses/external/externalvdc.hpp \
  src/libp44vdc/deviceclasses/evaluator/evaluatorvdc.cpp \
  src/libp44vdc/deviceclasses/evaluator/evaluatorvdc.hpp \
  src/libp44vdc/deviceclasses/evaluator/evaluatordevice.cpp \
  src/libp44vdc/deviceclasses/evaluator/evaluatordevice.hpp \
  src/libp44vdc/deviceclasses/enocean/enoceancomm.cpp \
  src/libp44vdc/deviceclasses/enocean/enoceancomm.hpp \
  src/libp44vdc/deviceclasses/enocean/enoceandevice.cpp \
  src/libp44vdc/deviceclasses/enocean/enoceandevice.hpp \
  src/libp44vdc/deviceclasses/enocean/enoceanremotecontrol.cpp \
  src/libp44vdc/deviceclasses/enocean/enoceanremotecontrol.hpp \
  src/libp44vdc/deviceclasses/enocean/enoceansensorhandler.cpp \
  src/libp44vdc/deviceclasses/enocean/enoceansensorhandler.hpp \
  src/libp44vdc/deviceclasses/enocean/enoceanrps.cpp \
  src/libp44vdc/deviceclasses/enocean/enoceanrps.hpp \
  src/libp44vdc/deviceclasses/enocean/enocean4bs.cpp \
  src/libp44vdc/deviceclasses/enocean/enocean4bs.hpp \
  src/libp44vdc/deviceclasses/enocean/enocean1bs.cpp \
  src/libp44vdc/deviceclasses/enocean/enocean1bs.hpp \
  src/libp44vdc/deviceclasses/enocean/enoceanvld.cpp \
  src/libp44vdc/deviceclasses/enocean/enoceanvld.hpp \
  src/libp44vdc/deviceclasses/enocean/enoceanvdc.cpp \
  src/libp44vdc/deviceclasses/enocean/enoceanvdc.hpp \
  src/libp44vdc/deviceclasses/dali/dalicomm.cpp \
  src/libp44vdc/deviceclasses/dali/dalicomm.hpp \
  src/libp44vdc/deviceclasses/dali/dalidefs.h \
  src/libp44vdc/deviceclasses/dali/dalidevice.cpp \
  src/libp44vdc/deviceclasses/dali/dalidevice.hpp \
  src/libp44vdc/deviceclasses/dali/dalivdc.cpp \
  src/libp44vdc/deviceclasses/dali/dalivdc.hpp \
  src/libp44vdc/deviceclasses/hue/huecomm.cpp \
  src/libp44vdc/deviceclasses/hue/huecomm.hpp \
  src/libp44vdc/deviceclasses/hue/huevdc.cpp \
  src/libp44vdc/deviceclasses/hue/huevdc.hpp \
  src/libp44vdc/deviceclasses/hue/huedevice.cpp \
  src/libp44vdc/deviceclasses/hue/huedevice.hpp \
  src/libp44vdc/deviceclasses/ledchain/ws281xcomm.hpp \
  src/libp44vdc/deviceclasses/ledchain/ws281xcomm.cpp \
  src/libp44vdc/deviceclasses/ledchain/ledchaindevice.hpp \
  src/libp44vdc/deviceclasses/ledchain/ledchaindevice.cpp \
  src/libp44vdc/deviceclasses/ledchain/ledchainvdc.hpp \
  src/libp44vdc/deviceclasses/ledchain/ledchainvdc.cpp \
  src/deviceclasses/vzughome/vzughomecomm.cpp \
  src/deviceclasses/vzughome/vzughomecomm.hpp \
  src/deviceclasses/vzughome/vzughomedevice.cpp \
  src/deviceclasses/vzughome/vzughomedevice.hpp \
  src/deviceclasses/vzughome/vzughomevdc.cpp \
  src/deviceclasses/vzughome/vzughomevdc.hpp \
  src/deviceclasses/voxnet/voxnetcomm.cpp \
  src/deviceclasses/voxnet/voxnetcomm.hpp \
  src/deviceclasses/voxnet/voxnetdevice.cpp \
  src/deviceclasses/voxnet/voxnetdevice.hpp \
  src/deviceclasses/voxnet/voxnetvdc.cpp \
  src/deviceclasses/voxnet/voxnetvdc.hpp \
  src/p44_vdcd_host.cpp \
  src/p44_vdcd_host.hpp \
  src/p44_vdcd_main.cpp


if P44_BUILD_RPI

# olavdcd - inherits everything from regular vdcd except compiling with OLA enabled and additionally linking libola+libolacommon+libprotobuf

olavdcd_DEBUG = ${vdcd_DEBUG}

nodist_olavdcd_SOURCES = ${nodist_vdcd_SOURCES}

olavdcd_LDADD = ${vdcd_LDADD} -lola -lolacommon -lprotobuf

olavdcd_CXXFLAGS = \
  -I ${srcdir}/src/p44utils \
  -I ${srcdir}/src \
  -I ${srcdir}/src/libp44vdc/thirdparty/mongoose \
  -I ${srcdir}/src/libp44vdc/thirdparty \
  -I ${srcdir}/src/libp44vdc/pbuf/gen \
  -I ${srcdir}/src/libp44vdc/vdc_common \
  -I ${srcdir}/src/libp44vdc/behaviours \
  -I ${srcdir}/src/libp44vdc/deviceclasses/simpleio \
  -I ${srcdir}/src/libp44vdc/deviceclasses/external \
  -I ${srcdir}/src/libp44vdc/deviceclasses/evaluator \
  -I ${srcdir}/src/libp44vdc/deviceclasses/enocean \
  -I ${srcdir}/src/libp44vdc/deviceclasses/dali \
  -I ${srcdir}/src/libp44vdc/deviceclasses/hue \
  -I ${srcdir}/src/libp44vdc/deviceclasses/ledchain \
  -I ${srcdir}/src/libp44vdc/deviceclasses/ola \
  -I ${srcdir}/src/deviceclasses/vzughome \
  -I ${srcdir}/src/deviceclasses/voxnet \
  ${LEDCHAIN_INCLUDES} \
  ${BOOST_CPPFLAGS} \
  ${JSONC_CFLAGS} \
  ${PTHREAD_CFLAGS} \
  ${SQLITE3_CFLAGS} \
  ${PROTOBUFC_CFLAGS} \
  ${AVAHI_CFLAGS} \
  ${vdcd_EXTRACFLAGS} \
  ${vdcd_DEBUG} \
  ${vdcd_PLATFORM} \
  ${vdcd_EXTRAS} \
  -D ENABLE_OLA=1

olavdcd_SOURCES = \
  ${vdcd_SOURCES} \
  src/libp44vdc/deviceclasses/ola/oladevice.cpp \
  src/libp44vdc/deviceclasses/ola/oladevice.hpp \
  src/libp44vdc/deviceclasses/ola/olavdc.cpp \
  src/libp44vdc/deviceclasses/ola/olavdc.hpp

endif

