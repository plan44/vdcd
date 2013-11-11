AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

bin_PROGRAMS = vdcd jsonrpctool

vdcd_CPPFLAGS = \
  -I src/p44utils \
  -I src -I src/thirdparty/mongoose \
  -I src/thirdparty \
  -I src/vdc_common \
  -I src/behaviours \
  -I src/deviceclasses/simpleio \
  -I src/deviceclasses/enocean \
  -I src/deviceclasses/dali \
  -I src/deviceclasses/hue \
  -D DEBUG=1

vdcd_SOURCES = \
  src/p44utils/p44obj.cpp \
  src/p44utils/p44obj.hpp \
  src/p44utils/application.cpp \
  src/p44utils/application.hpp \
  src/p44utils/consolekey.cpp \
  src/p44utils/consolekey.hpp \
  src/p44utils/digitalio.cpp \
  src/p44utils/digitalio.hpp \
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
  src/p44utils/macaddress.cpp \
  src/p44utils/macaddress.hpp \
  src/p44utils/httpcomm.cpp\
  src/p44utils/httpcomm.hpp\
  src/p44utils/jsonwebclient.cpp\
  src/p44utils/jsonwebclient.hpp\
  src/p44utils/p44_common.hpp \
  src/thirdparty/mongoose/mongoose.c\
  src/thirdparty/mongoose/mongoose.h\
  src/thirdparty/sqlite3pp/sqlite3pp.cpp \
  src/thirdparty/sqlite3pp/sqlite3pp.h \
  src/thirdparty/sqlite3pp/sqlite3ppext.cpp \
  src/thirdparty/sqlite3pp/sqlite3ppext.h \
  src/vdc_common/dsbehaviour.cpp \
  src/vdc_common/dsbehaviour.hpp \
  src/vdc_common/dsscene.cpp \
  src/vdc_common/dsscene.hpp \
  src/vdc_common/device.cpp \
  src/vdc_common/device.hpp \
  src/vdc_common/devicesettings.cpp \
  src/vdc_common/devicesettings.hpp \
  src/vdc_common/propertycontainer.cpp \
  src/vdc_common/propertycontainer.hpp \
  src/vdc_common/dsaddressable.cpp \
  src/vdc_common/dsaddressable.hpp \
  src/vdc_common/deviceclasscontainer.cpp \
  src/vdc_common/deviceclasscontainer.hpp \
  src/vdc_common/devicecontainer.cpp \
  src/vdc_common/devicecontainer.hpp \
  src/vdc_common/dsdefs.h \
  src/vdc_common/dsuid.cpp \
  src/vdc_common/dsuid.hpp \
  src/vdc_common/vdcd_common.hpp \
  src/behaviours/outputbehaviour.cpp \
  src/behaviours/outputbehaviour.hpp \
  src/behaviours/climatecontrolbehaviour.cpp \
  src/behaviours/climatecontrolbehaviour.hpp \
  src/behaviours/buttonbehaviour.cpp \
  src/behaviours/sensorbehaviour.hpp \
  src/behaviours/sensorbehaviour.cpp \
  src/behaviours/binaryinputbehaviour.hpp \
  src/behaviours/binaryinputbehaviour.cpp \
  src/behaviours/buttonbehaviour.hpp \
  src/behaviours/lightbehaviour.cpp \
  src/behaviours/lightbehaviour.hpp \
  src/deviceclasses/simpleio/consoledevice.cpp \
  src/deviceclasses/simpleio/consoledevice.hpp \
  src/deviceclasses/simpleio/digitaliodevice.cpp \
  src/deviceclasses/simpleio/digitaliodevice.hpp \
  src/deviceclasses/simpleio/staticdevicecontainer.cpp \
  src/deviceclasses/simpleio/staticdevicecontainer.hpp \
  src/deviceclasses/enocean/enoceancomm.cpp \
  src/deviceclasses/enocean/enoceancomm.hpp \
  src/deviceclasses/enocean/enoceandevice.cpp \
  src/deviceclasses/enocean/enoceandevice.hpp \
  src/deviceclasses/enocean/enoceanrps.cpp \
  src/deviceclasses/enocean/enoceanrps.hpp \
  src/deviceclasses/enocean/enocean4bs.cpp \
  src/deviceclasses/enocean/enocean4bs.hpp \
  src/deviceclasses/enocean/enocean1bs.cpp \
  src/deviceclasses/enocean/enocean1bs.hpp \
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
  src/p44_vdcd_main.cpp

jsonrpctool_CPPFLAGS = -I src/p44utils -I src

jsonrpctool_SOURCES = \
  src/p44utils/p44obj.cpp \
  src/p44utils/p44obj.hpp \
  src/p44utils/application.cpp \
  src/p44utils/application.hpp \
  src/p44utils/error.cpp \
  src/p44utils/error.hpp \
  src/p44utils/jsoncomm.cpp \
  src/p44utils/jsoncomm.hpp \
  src/p44utils/jsonrpccomm.cpp \
  src/p44utils/jsonrpccomm.hpp \
  src/p44utils/jsonobject.cpp \
  src/p44utils/jsonobject.hpp \
  src/p44utils/logger.cpp \
  src/p44utils/logger.hpp \
  src/p44utils/mainloop.cpp \
  src/p44utils/mainloop.hpp \
  src/p44utils/operationqueue.cpp \
  src/p44utils/operationqueue.hpp \
  src/p44utils/serialcomm.cpp \
  src/p44utils/serialcomm.hpp \
  src/p44utils/serialqueue.cpp \
  src/p44utils/serialqueue.hpp \
  src/p44utils/fdcomm.cpp \
  src/p44utils/fdcomm.hpp \
  src/p44utils/socketcomm.cpp \
  src/p44utils/socketcomm.hpp \
  src/p44utils/utils.cpp \
  src/p44utils/utils.hpp \
  src/p44utils/p44_common.hpp \
  src/jsonrpctool.cpp
