AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

if RASPBERRYPI
bin_PROGRAMS = vdcd olavdcd
else
bin_PROGRAMS = vdcd
endif

# common stuff for protobuf - NOTE: need to "make all" to get BUILT_SOURCES made

PROTOBUF_GENERATED = \
  src/pbuf/gen/messages.pb-c.h \
  src/pbuf/gen/messages.pb-c.c \
  src/pbuf/gen/vdcapi.pb-c.h \
  src/pbuf/gen/vdcapi.pb-c.c

#dist_noinst_DATA = messages.proto vdcapi.proto

MOSTLYCLEANFILES = $(PROTOBUF_GENERATED)

BUILT_SOURCES = $(PROTOBUF_GENERATED)

src/pbuf/gen/messages.pb-c.h src/pbuf/gen/messages.pb-c.c:	src/pbuf/messages.proto
	$(PROTOC) --proto_path=src/pbuf --c_out=src/pbuf/gen src/pbuf/messages.proto

src/pbuf/gen/vdcapi.pb-c.h src/pbuf/gen/vdcapi.pb-c.c:	src/pbuf/vdcapi.proto
	$(PROTOC) --proto_path=src/pbuf --c_out=src/pbuf/gen src/pbuf/vdcapi.proto


# vdcd

if DEBUG
vdcd_DEBUG = -D DEBUG=1
else
vdcd_DEBUG =
endif

if RASPBERRYPI
vdcd_PLATFORM = -D RASPBERRYPI=1
else
vdcd_PLATFORM =
endif

# enable some extras
vdcd_EXTRAS = -D ENABLE_VOXNET=1 -D ENABLE_VZUGHOME=1


nodist_vdcd_SOURCES = $(PROTOBUF_GENERATED)


if RASPBERRYPI
# This is an ugly hack, but I could not find a way to refer to the sysroot prefix for the -L path.
# Note: the entire library situation is ugly, as toolchain is not complete and autoconf lib macros dont work.
TOOLCHAIN_SYSROOT = /Volumes/xtools/arm-none-linux-gnueabi/arm-none-linux-gnueabi/sysroot
vdcd_LDADD = $(PTHREAD_LIBS) -lmongoose -lavahi-core  -lprotobuf-c -lsqlite3 -ljson-c -ldl -L${TOOLCHAIN_SYSROOT}/${libdir}/arm-linux-gnueabihf -lcrypto -lz
# sources only supported on RPI
LEDCHAIN_SRC = \
  src/thirdparty/rpi_ws281x/clk.h \
  src/thirdparty/rpi_ws281x/gpio.h \
  src/thirdparty/rpi_ws281x/dma.h \
  src/thirdparty/rpi_ws281x/dma.c \
  src/thirdparty/rpi_ws281x/pwm.h \
  src/thirdparty/rpi_ws281x/pwm.c \
  src/thirdparty/rpi_ws281x/board_info.h \
  src/thirdparty/rpi_ws281x/board_info.c \
  src/thirdparty/rpi_ws281x/mailbox.h \
  src/thirdparty/rpi_ws281x/mailbox.c \
  src/thirdparty/rpi_ws281x/ws2811.h \
  src/thirdparty/rpi_ws281x/ws2811.c \
  src/deviceclasses/ledchain/ws281xcomm.hpp \
  src/deviceclasses/ledchain/ws281xcomm.cpp \
  src/deviceclasses/ledchain/ledchaindevice.hpp \
  src/deviceclasses/ledchain/ledchaindevice.cpp \
  src/deviceclasses/ledchain/ledchaindevicecontainer.hpp \
  src/deviceclasses/ledchain/ledchaindevicecontainer.cpp
LEDCHAIN_INCLUDES = \
  -I ${srcdir}/src/deviceclasses/ledchain \
  -I ${srcdir}/src/thirdparty/rpi_ws281x

else
# automatic libs does not work right now due to commented out checks in autoconf.ac, so specify -l directly
# vdcd_LDADD = $(JSONC_LIBS) $(PTHREAD_LIBS) $(SQLITE3_LIBS) $(PROTOBUFC_LIBS)
vdcd_LDADD = $(PTHREAD_LIBS) -lavahi-core -lavahi-common -lprotobuf-c -lsqlite3 -ljson -ldl -lcrypto -lz
# build mongoose into vdcd, no library
MONGOOSE_SRC = src/thirdparty/mongoose/mongoose.c src/thirdparty/mongoose/mongoose.h
endif


vdcd_CXXFLAGS = \
  -I ${srcdir}/src/p44utils \
  -I ${srcdir}/src \
  -I ${srcdir}/src/thirdparty/mongoose \
  -I ${srcdir}/src/thirdparty \
  -I ${srcdir}/src/pbuf/gen \
  -I ${srcdir}/src/vdc_common \
  -I ${srcdir}/src/behaviours \
  -I ${srcdir}/src/deviceclasses/simpleio \
  -I ${srcdir}/src/deviceclasses/external \
  -I ${srcdir}/src/deviceclasses/evaluator \
  -I ${srcdir}/src/deviceclasses/enocean \
  -I ${srcdir}/src/deviceclasses/dali \
  -I ${srcdir}/src/deviceclasses/hue \
  -I ${srcdir}/src/deviceclasses/vzughome \
  -I ${srcdir}/src/deviceclasses/voxnet \
  -I ${srcdir}/../libavahi-core \
  ${LEDCHAIN_INCLUDES} \
  ${BOOST_CPPFLAGS} \
  ${JSONC_CFLAGS} \
  ${PTHREAD_CFLAGS} \
  ${SQLITE3_CFLAGS} \
  ${PROTOBUFC_CFLAGS} \
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
  src/p44utils/httpcomm.cpp\
  src/p44utils/httpcomm.hpp\
  src/p44utils/jsonwebclient.cpp\
  src/p44utils/jsonwebclient.hpp\
  src/p44utils/p44_common.hpp \
  src/thirdparty/sqlite3pp/sqlite3pp.cpp \
  src/thirdparty/sqlite3pp/sqlite3pp.h \
  src/thirdparty/sqlite3pp/sqlite3ppext.cpp \
  src/thirdparty/sqlite3pp/sqlite3ppext.h \
  src/vdc_common/dsbehaviour.cpp \
  src/vdc_common/dsbehaviour.hpp \
  src/vdc_common/outputbehaviour.cpp \
  src/vdc_common/outputbehaviour.hpp \
  src/vdc_common/channelbehaviour.cpp \
  src/vdc_common/channelbehaviour.hpp \
  src/vdc_common/dsscene.cpp \
  src/vdc_common/dsscene.hpp \
  src/vdc_common/simplescene.cpp \
  src/vdc_common/simplescene.hpp \
  src/vdc_common/device.cpp \
  src/vdc_common/device.hpp \
  src/vdc_common/devicesettings.cpp \
  src/vdc_common/devicesettings.hpp \
  src/vdc_common/valuesource.cpp \
  src/vdc_common/valuesource.hpp \
  src/vdc_common/propertycontainer.cpp \
  src/vdc_common/propertycontainer.hpp \
  src/vdc_common/pbufvdcapi.cpp \
  src/vdc_common/pbufvdcapi.hpp \
  src/vdc_common/jsonvdcapi.cpp \
  src/vdc_common/jsonvdcapi.hpp \
  src/vdc_common/vdcapi.cpp \
  src/vdc_common/vdcapi.hpp \
  src/vdc_common/apivalue.cpp \
  src/vdc_common/apivalue.hpp \
  src/vdc_common/dsaddressable.cpp \
  src/vdc_common/dsaddressable.hpp \
  src/vdc_common/deviceclasscontainer.cpp \
  src/vdc_common/deviceclasscontainer.hpp \
  src/vdc_common/devicecontainer.cpp \
  src/vdc_common/devicecontainer.hpp \
  src/vdc_common/discovery.cpp \
  src/vdc_common/discovery.hpp \
  src/vdc_common/p44_vdcd_host.cpp \
  src/vdc_common/p44_vdcd_host.hpp \
  src/vdc_common/dsdefs.h \
  src/vdc_common/dsuid.cpp \
  src/vdc_common/dsuid.hpp \
  src/vdc_common/vdcd_common.hpp \
  src/behaviours/climatecontrolbehaviour.cpp \
  src/behaviours/climatecontrolbehaviour.hpp \
  src/behaviours/audiobehaviour.cpp \
  src/behaviours/audiobehaviour.hpp \
  src/behaviours/shadowbehaviour.cpp \
  src/behaviours/shadowbehaviour.hpp \
  src/behaviours/buttonbehaviour.hpp \
  src/behaviours/buttonbehaviour.cpp \
  src/behaviours/sensorbehaviour.hpp \
  src/behaviours/sensorbehaviour.cpp \
  src/behaviours/binaryinputbehaviour.hpp \
  src/behaviours/binaryinputbehaviour.cpp \
  src/behaviours/lightbehaviour.cpp \
  src/behaviours/lightbehaviour.hpp \
  src/behaviours/colorlightbehaviour.cpp \
  src/behaviours/colorlightbehaviour.hpp \
  src/behaviours/movinglightbehaviour.cpp \
  src/behaviours/movinglightbehaviour.hpp \
  src/deviceclasses/simpleio/consoledevice.cpp \
  src/deviceclasses/simpleio/consoledevice.hpp \
  src/deviceclasses/simpleio/sparkiodevice.cpp \
  src/deviceclasses/simpleio/sparkiodevice.hpp \
  src/deviceclasses/simpleio/digitaliodevice.cpp \
  src/deviceclasses/simpleio/digitaliodevice.hpp \
  src/deviceclasses/simpleio/analogiodevice.cpp \
  src/deviceclasses/simpleio/analogiodevice.hpp \
  src/deviceclasses/simpleio/staticdevicecontainer.cpp \
  src/deviceclasses/simpleio/staticdevicecontainer.hpp \
  src/deviceclasses/external/externaldevicecontainer.cpp \
  src/deviceclasses/external/externaldevicecontainer.hpp \
  src/deviceclasses/evaluator/evaluatordevicecontainer.cpp \
  src/deviceclasses/evaluator/evaluatordevicecontainer.hpp \
  src/deviceclasses/evaluator/evaluatordevice.cpp \
  src/deviceclasses/evaluator/evaluatordevice.hpp \
  src/deviceclasses/enocean/enoceancomm.cpp \
  src/deviceclasses/enocean/enoceancomm.hpp \
  src/deviceclasses/enocean/enoceandevice.cpp \
  src/deviceclasses/enocean/enoceandevice.hpp \
  src/deviceclasses/enocean/enoceanremotecontrol.cpp \
  src/deviceclasses/enocean/enoceanremotecontrol.hpp \
  src/deviceclasses/enocean/enoceansensorhandler.cpp \
  src/deviceclasses/enocean/enoceansensorhandler.hpp \
  src/deviceclasses/enocean/enoceanrps.cpp \
  src/deviceclasses/enocean/enoceanrps.hpp \
  src/deviceclasses/enocean/enocean4bs.cpp \
  src/deviceclasses/enocean/enocean4bs.hpp \
  src/deviceclasses/enocean/enocean1bs.cpp \
  src/deviceclasses/enocean/enocean1bs.hpp \
  src/deviceclasses/enocean/enoceanvld.cpp \
  src/deviceclasses/enocean/enoceanvld.hpp \
  src/deviceclasses/enocean/enoceandevicecontainer.cpp \
  src/deviceclasses/enocean/enoceandevicecontainer.hpp \
  src/deviceclasses/dali/dalicomm.cpp \
  src/deviceclasses/dali/dalicomm.hpp \
  src/deviceclasses/dali/dalidefs.h \
  src/deviceclasses/dali/dalidevice.cpp \
  src/deviceclasses/dali/dalidevice.hpp \
  src/deviceclasses/dali/dalidevicecontainer.cpp \
  src/deviceclasses/dali/dalidevicecontainer.hpp \
  src/deviceclasses/hue/huecomm.cpp \
  src/deviceclasses/hue/huecomm.hpp \
  src/deviceclasses/hue/huedevicecontainer.cpp \
  src/deviceclasses/hue/huedevicecontainer.hpp \
  src/deviceclasses/hue/huedevice.cpp \
  src/deviceclasses/hue/huedevice.hpp \
  src/deviceclasses/vzughome/vzughomecomm.cpp \
  src/deviceclasses/vzughome/vzughomecomm.hpp \
  src/deviceclasses/vzughome/vzughomedevice.cpp \
  src/deviceclasses/vzughome/vzughomedevice.hpp \
  src/deviceclasses/vzughome/vzughomedevicecontainer.cpp \
  src/deviceclasses/vzughome/vzughomedevicecontainer.hpp \
  src/deviceclasses/voxnet/voxnetcomm.cpp \
  src/deviceclasses/voxnet/voxnetcomm.hpp \
  src/deviceclasses/voxnet/voxnetdevice.cpp \
  src/deviceclasses/voxnet/voxnetdevice.hpp \
  src/deviceclasses/voxnet/voxnetdevicecontainer.cpp \
  src/deviceclasses/voxnet/voxnetdevicecontainer.hpp \
  src/p44_vdcd_main.cpp


if RASPBERRYPI

# olavdcd - inherits everything from regular vdcd except compiling with OLA enabled and additionally linking libola+libolacommon+libprotobuf

olavdcd_DEBUG = ${vdcd_DEBUG}

nodist_olavdcd_SOURCES = ${nodist_vdcd_SOURCES}

olavdcd_LDADD = ${vdcd_LDADD} -lola -lolacommon -lprotobuf

olavdcd_CXXFLAGS = \
  -I ${srcdir}/src/p44utils \
  -I ${srcdir}/src \
  -I ${srcdir}/src/thirdparty/mongoose \
  -I ${srcdir}/src/thirdparty \
  -I ${srcdir}/src/pbuf/gen \
  -I ${srcdir}/src/vdc_common \
  -I ${srcdir}/src/behaviours \
  -I ${srcdir}/src/deviceclasses/simpleio \
  -I ${srcdir}/src/deviceclasses/external \
  -I ${srcdir}/src/deviceclasses/evaluator \
  -I ${srcdir}/src/deviceclasses/enocean \
  -I ${srcdir}/src/deviceclasses/dali \
  -I ${srcdir}/src/deviceclasses/hue \
  -I ${srcdir}/src/deviceclasses/ola \
  -I ${srcdir}/src/deviceclasses/vzughome \
  -I ${srcdir}/src/deviceclasses/voxnet \
  -I ${srcdir}/../libavahi-core \
  ${LEDCHAIN_INCLUDES} \
  ${BOOST_CPPFLAGS} \
  ${JSONC_CFLAGS} \
  ${PTHREAD_CFLAGS} \
  ${SQLITE3_CFLAGS} \
  ${PROTOBUFC_CFLAGS} \
  ${vdcd_DEBUG} \
  ${vdcd_PLATFORM} \
  ${vdcd_EXTRAS} \
  -D ENABLE_OLA=1

olavdcd_SOURCES = \
  ${vdcd_SOURCES} \
  src/deviceclasses/ola/oladevice.cpp \
  src/deviceclasses/ola/oladevice.hpp \
  src/deviceclasses/ola/oladevicecontainer.cpp \
  src/deviceclasses/ola/oladevicecontainer.hpp

endif

