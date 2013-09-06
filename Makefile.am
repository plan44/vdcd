AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

bin_PROGRAMS = vdcd jsonrpctool

vdcd_CPPFLAGS = -I src/p44utils -I src -I src/mongoose

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
  src/p44utils/p44_common.hpp \
  src/sqlite3pp/sqlite3pp.cpp \
  src/sqlite3pp/sqlite3pp.h \
  src/sqlite3pp/sqlite3ppext.cpp \
  src/sqlite3pp/sqlite3ppext.h \
  src/dsbehaviour.cpp \
  src/dsbehaviour.hpp \
  src/dsscene.cpp \
  src/dsscene.hpp \
  src/outputbehaviour.cpp \
  src/outputbehaviour.hpp \
  src/buttonbehaviour.cpp \
  src/sensorbehaviour.hpp \
  src/sensorbehaviour.cpp \
  src/binaryinputbehaviour.hpp \
  src/binaryinputbehaviour.cpp \
  src/buttonbehaviour.hpp \
  src/lightbehaviour.cpp \
  src/lightbehaviour.hpp \
  src/consoledevice.cpp \
  src/consoledevice.hpp \
  src/dalicomm.cpp \
  src/dalicomm.hpp \
  src/dalidefs.h \
  src/dalidevice.cpp \
  src/dalidevice.hpp \
  src/dalidevicecontainer.cpp \
  src/dalidevicecontainer.hpp \
  src/device.cpp \
  src/device.hpp \
  src/devicesettings.cpp \
  src/devicesettings.hpp \
  src/propertycontainer.cpp \
  src/propertycontainer.hpp \
  src/dsaddressable.cpp \
  src/dsaddressable.hpp \
  src/deviceclasscontainer.cpp \
  src/deviceclasscontainer.hpp \
  src/devicecontainer.cpp \
  src/devicecontainer.hpp \
  src/dsdefs.h \
  src/dsid.cpp \
  src/dsid.hpp \
  src/enoceancomm.cpp \
  src/enoceancomm.hpp \
  src/enoceandevice.cpp \
  src/enoceandevice.hpp \
  src/enoceanrps.cpp \
  src/enoceanrps.hpp \
  src/enocean4bs.cpp \
  src/enocean4bs.hpp \
  src/enoceandevicecontainer.cpp \
  src/enoceandevicecontainer.hpp \
  src/digitaliodevice.cpp \
  src/digitaliodevice.hpp \
  src/staticdevicecontainer.cpp \
  src/staticdevicecontainer.hpp \
  src/huedevicecontainer.cpp \
  src/huedevicecontainer.hpp \
  src/huedevice.cpp \
  src/huedevice.hpp \
  src/mongoose/mongoose.c\
  src/mongoose/mongoose.h\
  src/p44utils/httpcomm.cpp\
  src/p44utils/httpcomm.hpp\
  src/vdcd_common.hpp \
  src/main.cpp

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
