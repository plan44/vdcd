cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/p44-app-version.txt" P44_APP_VERSION LIMIT_COUNT 1)
string(STRIP "${P44_APP_VERSION}" P44_APP_VERSION)
if(NOT P44_APP_VERSION MATCHES "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?(\\.[0-9]+)?$")
  message(FATAL_ERROR "p44-app-version.txt must contain a numeric CMake project version, got '${P44_APP_VERSION}'")
endif()

project(vdcd VERSION ${P44_APP_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

option(DEBUG "Debug build with extra debugging code" OFF)
option(P44_BUILD_OW "plan44 build for OpenWrt targets" OFF)
option(P44_BUILD_RB "plan44 build for Raspbian targets" OFF)
option(ENABLE_OLA "Enable OLA support" OFF)
option(ENABLE_UBUS "Enable ubus API support" OFF)
option(ENABLE_I2C "Enable i2c direct access" OFF)
option(ENABLE_RPIWS281X "Enable WS281x Ledchain support on RaspberryPi" OFF)
option(ENABLE_PNG "Enable PNG image support for ledchains" OFF)
option(ENABLE_RRDB "Enable librrd usage for logging sensor data" OFF)
option(ENABLE_UWSC "Enable libuwsc usage for websockets" OFF)
option(ENABLE_GPIOD "Enable libgpiod usage for gpios" OFF)
option(ENABLE_EV "Enable libev usage for mainloop" OFF)
option(ENABLE_P44FEATURES "Enable p44features special purpose hardware support" OFF)
option(ENABLE_MODBUS "Enable modbus support" OFF)
option(EMBEDDED_LIBMODBUS "Use embedded libmodbus instead of system libmodbus" OFF)
option(ENABLE_DS485 "Enable dS485 support" OFF)
option(ENABLE_WBF "Enable Wiser-by-Feller support" OFF)
option(ENABLE_MCTRL "Enable Matter controller support" OFF)
option(ENABLE_LVGL "Enable LVGL graphics support" OFF)
option(ENABLE_LVGL_DRM "Use DRM instead of fbdev for LVGL" OFF)
option(PLATFORM_HAS_SHORTBREAK "Platform can produce short UART breaks" OFF)

set(P44_EXPECTED_APP_VERSION "" CACHE STRING "Expected application version from the packaging/build environment")
set(APPLICATION_VERSION "" CACHE STRING "Unsupported legacy version override")
set(LVGL_COLOR_DEPTH "" CACHE STRING "LVGL color depth")

if(APPLICATION_VERSION)
  message(FATAL_ERROR "APPLICATION_VERSION is no longer supported as a version override; set P44_EXPECTED_APP_VERSION for package version checks instead")
endif()

if(P44_EXPECTED_APP_VERSION AND NOT P44_EXPECTED_APP_VERSION STREQUAL PROJECT_VERSION)
  message(FATAL_ERROR "Expected application version ${P44_EXPECTED_APP_VERSION} does not match source version ${PROJECT_VERSION}")
endif()

include(CheckLibraryExists)
include(CheckIncludeFile)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost 1.35 REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(JSONC REQUIRED IMPORTED_TARGET json-c>=0.10)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules(PROTOBUFC REQUIRED IMPORTED_TARGET libprotobuf-c)

if(NOT APPLE)
  pkg_check_modules(AVAHI REQUIRED IMPORTED_TARGET avahi-client)
endif()

if(ENABLE_PNG)
  pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng>=1.6)
endif()

if(ENABLE_GPIOD)
  pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod>=1.6.3)
endif()

if(ENABLE_LVGL_DRM)
  pkg_check_modules(DRM REQUIRED IMPORTED_TARGET libdrm>=2.4)
endif()

if(ENABLE_LVGL AND APPLE)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
endif()

if(ENABLE_RRDB)
  pkg_check_modules(RRD QUIET IMPORTED_TARGET librrd)
  if(NOT RRD_FOUND)
    find_path(RRD_INCLUDE_DIR rrd.h REQUIRED)
    find_library(RRD_LIBRARY rrd REQUIRED)
  endif()
endif()

if(ENABLE_EV)
  find_library(EV_LIBRARY ev REQUIRED)
endif()

if(ENABLE_UWSC AND NOT APPLE)
  find_library(UWSC_LIBRARY uwsc REQUIRED)
endif()

if(ENABLE_OLA)
  find_library(OLA_LIBRARY ola REQUIRED)
  find_library(OLACOMMON_LIBRARY olacommon REQUIRED)
  find_library(PROTOBUF_LIBRARY protobuf REQUIRED)
endif()

if(ENABLE_UBUS)
  find_library(UBUS_LIBRARY ubus REQUIRED)
  find_library(UBOX_LIBRARY ubox REQUIRED)
  find_library(BLOBMSG_JSON_LIBRARY blobmsg_json REQUIRED)
endif()

if(ENABLE_I2C)
  find_library(I2C_LIBRARY i2c REQUIRED)
endif()

if(ENABLE_MODBUS AND NOT EMBEDDED_LIBMODBUS)
  find_library(MODBUS_LIBRARY modbus REQUIRED)
  check_library_exists("${MODBUS_LIBRARY}" modbus_rtu_set_custom_rts_ex "" HAVE_PLAN44_MODBUS)
  if(NOT HAVE_PLAN44_MODBUS)
    message(FATAL_ERROR "System libmodbus does not provide modbus_rtu_set_custom_rts_ex")
  endif()
endif()

find_program(PROTOC_C protoc-c REQUIRED)

add_library(vdcd_config INTERFACE)
target_include_directories(vdcd_config INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_compile_definitions(vdcd_config INTERFACE
  PACKAGE_VERSION="${PROJECT_VERSION}"
  P44_APPLICATION_VERSION="${PROJECT_VERSION}"
  NO_SSL_DL=1
  BOOST_BIND_GLOBAL_PLACEHOLDERS=1
  BOOST_VERSION_146=1
  $<$<BOOL:${DEBUG}>:DEBUG=1>
  $<$<BOOL:${P44_BUILD_OW}>:P44_BUILD_OW=1;USE_IPV6=1>
  $<$<BOOL:${P44_BUILD_RB}>:P44_BUILD_RB=1>
  $<$<BOOL:${PLATFORM_HAS_SHORTBREAK}>:PLATFORM_HAS_SHORTBREAK=1>
  ENABLE_OLA=$<IF:$<BOOL:${ENABLE_OLA}>,1,0>
  ENABLE_UBUS=$<IF:$<BOOL:${ENABLE_UBUS}>,1,0>
  ENABLE_RPIWS281X=$<IF:$<BOOL:${ENABLE_RPIWS281X}>,1,0>
  ENABLE_PNG=$<IF:$<BOOL:${ENABLE_PNG}>,1,0>
  ENABLE_RRDB=$<IF:$<BOOL:${ENABLE_RRDB}>,1,0>
  ENABLE_UWSC=$<IF:$<BOOL:${ENABLE_UWSC}>,1,0>
  ENABLE_GPIOD=$<IF:$<BOOL:${ENABLE_GPIOD}>,1,0>
  ENABLE_P44FEATURES=$<IF:$<BOOL:${ENABLE_P44FEATURES}>,1,0>
  ENABLE_MODBUS=$<IF:$<BOOL:${ENABLE_MODBUS}>,1,0>
  ENABLE_DS485DEVICES=$<IF:$<BOOL:${ENABLE_DS485}>,1,0>
  ENABLE_WBF=$<IF:$<BOOL:${ENABLE_WBF}>,1,0>
  ENABLE_MATTER=$<IF:$<BOOL:${ENABLE_MCTRL}>,1,0>
  ENABLE_LVGL=$<IF:$<BOOL:${ENABLE_LVGL}>,1,0>
  ENABLE_LVGL_DRM=$<IF:$<BOOL:${ENABLE_LVGL_DRM}>,1,0>
  DISABLE_I2C=$<IF:$<BOOL:${ENABLE_I2C}>,0,1>
  MAINLOOP_LIBEV_BASED=$<IF:$<BOOL:${ENABLE_EV}>,1,0>
)

if(LVGL_COLOR_DEPTH)
  target_compile_definitions(vdcd_config INTERFACE LVGL_COLOR_DEPTH=${LVGL_COLOR_DEPTH})
endif()

if(JSONC_VERSION VERSION_GREATER_EQUAL 0.13)
  target_compile_definitions(vdcd_config INTERFACE HAVE_JSONC_VERSION_013=1)
endif()

target_link_libraries(vdcd_config INTERFACE
  Threads::Threads
  PkgConfig::JSONC
  PkgConfig::SQLITE3
  PkgConfig::PROTOBUFC
  OpenSSL::SSL
  OpenSSL::Crypto
)

if(TARGET Boost::headers)
  target_link_libraries(vdcd_config INTERFACE Boost::headers)
elseif(TARGET Boost::boost)
  target_link_libraries(vdcd_config INTERFACE Boost::boost)
else()
  target_include_directories(vdcd_config INTERFACE ${Boost_INCLUDE_DIRS})
endif()

if(NOT APPLE)
  target_link_libraries(vdcd_config INTERFACE PkgConfig::AVAHI)
endif()

if(APPLE)
  target_link_libraries(vdcd_config INTERFACE "-framework CoreFoundation" "-framework IOKit")
endif()

if(ENABLE_PNG)
  target_link_libraries(vdcd_config INTERFACE PkgConfig::PNG)
endif()
if(ENABLE_GPIOD)
  target_link_libraries(vdcd_config INTERFACE PkgConfig::GPIOD)
endif()
if(ENABLE_LVGL_DRM)
  target_link_libraries(vdcd_config INTERFACE PkgConfig::DRM)
endif()
if(ENABLE_LVGL AND APPLE)
  target_link_libraries(vdcd_config INTERFACE PkgConfig::SDL2)
endif()
if(ENABLE_RRDB)
  if(TARGET PkgConfig::RRD)
    target_link_libraries(vdcd_config INTERFACE PkgConfig::RRD)
  else()
    target_include_directories(vdcd_config INTERFACE "${RRD_INCLUDE_DIR}")
    target_link_libraries(vdcd_config INTERFACE "${RRD_LIBRARY}")
  endif()
endif()
if(ENABLE_EV)
  target_link_libraries(vdcd_config INTERFACE "${EV_LIBRARY}")
endif()
if(ENABLE_UWSC)
  if(APPLE)
  else()
    target_link_libraries(vdcd_config INTERFACE "${UWSC_LIBRARY}")
  endif()
endif()
if(ENABLE_OLA)
  target_link_libraries(vdcd_config INTERFACE "${OLA_LIBRARY}" "${OLACOMMON_LIBRARY}" "${PROTOBUF_LIBRARY}")
endif()
if(ENABLE_UBUS)
  target_link_libraries(vdcd_config INTERFACE "${UBUS_LIBRARY}" "${UBOX_LIBRARY}" "${BLOBMSG_JSON_LIBRARY}")
endif()
if(ENABLE_I2C)
  target_link_libraries(vdcd_config INTERFACE "${I2C_LIBRARY}")
endif()
if(ENABLE_MODBUS AND NOT EMBEDDED_LIBMODBUS)
  target_link_libraries(vdcd_config INTERFACE "${MODBUS_LIBRARY}")
endif()

if(ENABLE_LVGL)
  set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/lv_conf.h" CACHE STRING "" FORCE)
  set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
  set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
  add_subdirectory(src/lvgl)
  target_link_libraries(lvgl PUBLIC vdcd_config)
  if(APPLE)
    target_link_libraries(lvgl PUBLIC PkgConfig::SDL2)
    set(SDL2_COMPAT_INCLUDE_DIRS)
    foreach(SDL2_INCLUDE_DIR IN LISTS SDL2_INCLUDE_DIRS)
      get_filename_component(SDL2_INCLUDE_NAME "${SDL2_INCLUDE_DIR}" NAME)
      if(SDL2_INCLUDE_NAME STREQUAL "SDL2")
        get_filename_component(SDL2_INCLUDE_PARENT "${SDL2_INCLUDE_DIR}" DIRECTORY)
        list(APPEND SDL2_COMPAT_INCLUDE_DIRS "${SDL2_INCLUDE_PARENT}")
      endif()
    endforeach()
    target_include_directories(lvgl SYSTEM PUBLIC ${SDL2_COMPAT_INCLUDE_DIRS})
  endif()
endif()

set(P44UTILS_SOURCES
  analogio.cpp
  application.cpp
  artnetcomm.cpp
  backlight.cpp
  colorutils.cpp
  consolekey.cpp
  crc32.cpp
  dcmotor.cpp
  digitalio.cpp
  dmxhandler.cpp
  error.cpp
  extutils.cpp
  fdcomm.cpp
  fnv.cpp
  gpio.cpp
  httpcomm.cpp
  i2c.cpp
  igmp.cpp
  iopin.cpp
  jsoncomm.cpp
  jsonobject.cpp
  jsonrpccomm.cpp
  jsonwebclient.cpp
  ledchaincomm.cpp
  logger.cpp
  macaddress.cpp
  mainloop.cpp
  midi.cpp
  operationqueue.cpp
  p44obj.cpp
  p44script.cpp
  persistentparams.cpp
  pwm.cpp
  regexp.cpp
  serialcomm.cpp
  serialqueue.cpp
  socketcomm.cpp
  spi.cpp
  sqlite3persistence.cpp
  ssdpsearch.cpp
  timeutils.cpp
  tlv.cpp
  utils.cpp
  valueanimator.cpp
  valueunits.cpp
  thirdparty/civetweb/civetweb.c
  thirdparty/slre/slre.c
  thirdparty/sqlite3pp/sqlite3pp.cpp
  thirdparty/sqlite3pp/sqlite3ppext.cpp
)

if(NOT APPLE)
  list(APPEND P44UTILS_SOURCES dnssd.cpp)
endif()

if(ENABLE_UBUS)
  list(APPEND P44UTILS_SOURCES ubus.cpp)
endif()

if(ENABLE_UWSC)
  list(APPEND P44UTILS_SOURCES websocket.cpp)
  if(APPLE)
    list(APPEND P44UTILS_SOURCES
      thirdparty/uwsc/uwsc.c
      thirdparty/uwsc/log.c
      thirdparty/uwsc/sha1.c
      thirdparty/uwsc/ssl.c
      thirdparty/uwsc/utils.c
      thirdparty/uwsc/buffer/buffer.c
      thirdparty/uwsc/ssl/openssl.c
      thirdparty/compat_glue/src/socket.c
    )
  endif()
endif()

if(ENABLE_MODBUS)
  list(APPEND P44UTILS_SOURCES modbus.cpp)
endif()

if(ENABLE_MODBUS AND EMBEDDED_LIBMODBUS)
  list(APPEND P44UTILS_SOURCES
    thirdparty/libmodbus/src/modbus-data.c
    thirdparty/libmodbus/src/modbus-rtu.c
    thirdparty/libmodbus/src/modbus-tcp.c
    thirdparty/libmodbus/src/modbus.c
  )
endif()

if(ENABLE_RPIWS281X)
  list(APPEND P44UTILS_SOURCES
    thirdparty/rpi_ws281x/dma.c
    thirdparty/rpi_ws281x/pcm.c
    thirdparty/rpi_ws281x/pwm.c
    thirdparty/rpi_ws281x/rpihw.c
    thirdparty/rpi_ws281x/mailbox.c
    thirdparty/rpi_ws281x/ws2811.c
  )
endif()

if(ENABLE_LVGL)
  list(APPEND P44UTILS_SOURCES lvgl.cpp lvglui.cpp)
endif()

if(ENABLE_P44FEATURES)
  list(APPEND P44UTILS_SOURCES rfid.cpp)
endif()

add_subdirectory(src/p44utils)
add_subdirectory(src/p44lrgraphics)

if(ENABLE_P44FEATURES)
  add_subdirectory(src/p44features)
endif()

add_subdirectory(src/p44vdc)

add_executable(vdcd src/p44_vdcd_main.cpp)

set(VDCD_LOCAL_LIBS p44vdc p44utils p44lrgraphics)
if(ENABLE_P44FEATURES)
  list(APPEND VDCD_LOCAL_LIBS p44features)
endif()

function(p44_enable_runtime_asserts target_name)
  target_compile_options(${target_name} PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-UNDEBUG>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-UNDEBUG>
    $<$<COMPILE_LANG_AND_ID:C,MSVC>:/UNDEBUG>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/UNDEBUG>
  )
endfunction()

foreach(VDCD_ASSERT_TARGET IN LISTS VDCD_LOCAL_LIBS)
  p44_enable_runtime_asserts(${VDCD_ASSERT_TARGET})
endforeach()
p44_enable_runtime_asserts(vdcd)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT APPLE)
  target_link_libraries(vdcd PRIVATE
    "-Wl,--whole-archive"
    ${VDCD_LOCAL_LIBS}
    "-Wl,--no-whole-archive"
  )
elseif(APPLE)
  target_link_libraries(vdcd PRIVATE ${VDCD_LOCAL_LIBS})
  foreach(VDCD_LOCAL_LIB IN LISTS VDCD_LOCAL_LIBS)
    target_link_libraries(vdcd PRIVATE "-Wl,-force_load,$<TARGET_FILE:${VDCD_LOCAL_LIB}>")
    add_dependencies(vdcd ${VDCD_LOCAL_LIB})
  endforeach()
else()
  target_link_libraries(vdcd PRIVATE ${VDCD_LOCAL_LIBS})
endif()

target_link_libraries(vdcd PRIVATE vdcd_config)

if(ENABLE_LVGL)
  target_link_libraries(vdcd PRIVATE lvgl)
endif()

install(TARGETS vdcd RUNTIME DESTINATION bin)
