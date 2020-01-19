AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

bin_PROGRAMS = p44utilsdevice

if DEBUG
p44utilsdevice_DEBUG = -D DEBUG=1
else
p44utilsdevice_DEBUG =
endif

if P44_BUILD_RPI
p44utilsdevice_PLATFORM = -D P44_BUILD_RPI=1
else
p44utilsdevice_PLATFORM =
endif


if P44_BUILD_RPI
# This is an ugly hack, but I could not find a way to refer to the sysroot prefix for the -L path.
# Note: the entire library situation is ugly, as toolchain is not complete and autoconf lib macros dont work.
TOOLCHAIN_SYSROOT = /Volumes/xtools/arm-none-linux-gnueabi/arm-none-linux-gnueabi/sysroot
p44utilsdevice_LDADD = $(PTHREAD_LIBS) -ljson-c -ldl -L${TOOLCHAIN_SYSROOT}/${libdir}/arm-linux-gnueabihf
# JSONC_CFLAGS is not automatically set in RPi build because config check is disabled, set include path here
p44utilsdevice_EXTRACFLAGS = \
  -I${TOOLCHAIN_SYSROOT}/usr/local/include/json-c
else
p44utilsdevice_LDADD = $(PTHREAD_LIBS) $(JSONC_LIBS) -ldl
endif


p44utilsdevice_CXXFLAGS = \
  -I ${srcdir}/src/p44utils \
  -I ${srcdir}/src \
  ${BOOST_CPPFLAGS} \
  ${JSONC_CFLAGS} \
  ${PTHREAD_CFLAGS} \
  ${p44utilsdevice_EXTRACFLAGS} \
  ${p44utilsdevice_DEBUG} \
  ${p44utilsdevice_PLATFORM}

p44utilsdevice_SOURCES = \
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
  src/p44utils/logger.cpp \
  src/p44utils/logger.hpp \
  src/p44utils/mainloop.cpp \
  src/p44utils/mainloop.hpp \
  src/p44utils/fdcomm.cpp \
  src/p44utils/fdcomm.hpp \
  src/p44utils/socketcomm.cpp \
  src/p44utils/socketcomm.hpp \
  src/p44utils/serialcomm.cpp \
  src/p44utils/serialcomm.hpp \
  src/p44utils/utils.cpp \
  src/p44utils/utils.hpp \
  src/p44utils/p44utils_common.hpp \
  src/p44utils_config.hpp \
  src/main.cpp
