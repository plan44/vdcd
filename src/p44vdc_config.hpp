//  SPDX-License-Identifier: GPL-3.0-or-later
//
//  Copyright (c) 2014-2023 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of vdcd.
//
//  vdcd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  vdcd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with vdcd. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __p44vdc__config__
#define __p44vdc__config__


#if !P44_BUILD_DIGI && !P44_BUILD_RPI && !P44_BUILD_OW
  // non official release platform
  #undef ALWAYS_DEBUG // override per-file ALWAYS_DEBUG
  #define ALWAYS_DEBUG 1 // if set, DBGLOG statements will always be included, even in non-debug builds (but can be silenced via loglevel)
#endif

// auto-disable some features depending on platform
// - No i2c on Mac or DigiESP, but possible on RaspberryPi and OpenWrt
#if (defined(__APPLE__) || P44_BUILD_DIGI) && !P44_BUILD_RPI && !P44_BUILD_OW
  #define DISABLE_I2C 1 // No i2c
#endif
#if defined(__APPLE__)
  #define DISABLE_DISCOVERY 1 // Avahi usually makes no sense on Mac (but compiles with Avahi core available)
  #define USE_AVAHI_CORE 0 // in case we want avahi, use avahi core
  #define BUTTON_NOT_AVAILABLE_AT_START 1 // as in newer xx2 devices
  #define ENABLE_LOCALCONTROLLER 1
  #define ENABLE_SETTINGS_FROM_FILES 1 // enabled to keep it compiling (but no real use any more)
  #define ENABLE_ENOCEAN_SECURE 1
  #define SELFTESTING_ENABLED 1
  #define ENABLE_JSONCFGAPI 1
  #define ENABLE_JSONBRIDGEAPI 1
  #define ENABLE_LEGACY_P44CFGAPI 0
  #define ENABLE_UBUS 0
  #define ENABLE_OLA 1
  #define ENABLE_MODBUS 1
  #define ENABLE_UWSC 1
  #define REDUCED_FOOTPRINT 0 // general flag to leave away stuff not urgently needed when footprint is a concern
  #define HAVE_JSONC_VERSION_013 1 // unlike in many linux distros, brew had json-c >=0.13 for a while (2021: 0.15)
#endif

// general defaults
#define ENABLE_LOCAL_BEHAVIOUR 1 // enabled local (minimalistic) light/button operations when no vdsm is connected

// Default build settings for different targets
#define REDUCED_FOOTPRINT_TEST 0
#if P44_BUILD_DIGI || (defined(__APPLE__) && REDUCED_FOOTPRINT_TEST)
  // DigiConnect based P44-DSB-DEH
  #define ENABLE_DALI 1
  #define ENABLE_DALI_INPUTS 0 // disabled because of DALI bridge restrictions
  #define ENABLE_ENOCEAN 1
  #define ENABLE_ENOCEAN_SECURE 0 // disabled because of footprint and insufficient modem firmware
  #define ENABLE_ENOCEAN_SHADOW 0 // disabled because of footprint and insufficient modem firmware
  #define ENABLE_HUE 1
  #define ENABLE_STATIC 0 // disabled because of footprint
  #define ENABLE_FCU_SUPPORT 0 // disabled because of footprint
  #define ENABLE_EXTERNAL 0 // disabled because of footprint
  #define ENABLE_SCRIPTED 0 // disabled because of footprint and lack of scripting in general
  #define ENABLE_CUSTOM_EXOTIC 0 // disabled because of footprint
  #define ENABLE_CUSTOM_SINGLEDEVICE 0 // disabled because of footprint
  #define ENABLE_EVALUATORS 1
  #define ENABLE_PROXYDEVICES 0 // disabled because of footprint
  #define ENABLE_LEDCHAIN 0
  #define ENABLE_LOCALCONTROLLER 0 // not supported at all
  #define ENABLE_P44LRGRAPHICS 0 // not needed, no ledchains anyway
  #define ENABLE_MODBUS 0 // not needed
  #define ENABLE_MIDI 0 // not needed
  #define ENABLE_JSONBRIDGEAPI 0 // disabled because of footprint
  #define P44SCRIPT_FULL_SUPPORT 0 // disabled because of footprint
  #define SCRIPTING_JSON_SUPPORT 0 // disabled because of footprint
  #define ENABLE_SCENE_SCRIPT 0 // disabled because of footprint
  #define ENABLE_SETTINGS_FROM_FILES 0 // disabled because not needed and adding to footprint
  #define ENABLE_HTTP_SCRIPT_FUNCS 0 // disabled because not needed and adding to footprint
  #define ENABLE_DNSSD_SCRIPT_FUNCS 0 // disabled because not needed and adding to footprint
  #define ENABLE_SOCKET_SCRIPT_FUNCS 0 // disabled because not needed and adding to footprint
  #define ENABLE_WEBSOCKET_SCRIPT_FUNCS 0 // disabled because not needed and adding to footprint
  #define ENABLE_ANIMATOR_SCRIPT_FUNCS 0 // disabled because not needed and adding to footprint
  #define ENABLE_DIGITALIO_SCRIPT_FUNCS 0 // disabled because not needed and adding to footprint
  #define ENABLE_ANALOGIO_SCRIPT_FUNCS 0 // disabled because not needed and adding to footprint
  #define ENABLE_ANALOGIO_COLOR_SUPPORT 0 // disabled because not needed and adding to footprint
  #define ENABLE_DCMOTOR_SCRIPT_FUNCS 0 // disabled because not needed and adding to footprint
  #define ENABLE_MIDI_SCRIPT_FUNCS 0 // disabled because not needed and adding to footprint
  #define USE_AVAHI_CORE 1 // use direct avahi-code functions (good for small embedded targets, not recommended for desktops)
  #define SELFTESTING_ENABLED 0 // no longer needed, no new units will be produced any more
  #define REDUCED_FOOTPRINT 1 // general flag to leave away stuff not urgently needed when footprint is a concern
  #define ENABLE_LOG_COLORS 0 // to save a bit space and performance
#elif P44_BUILD_OW
  // P44-DSB-xx2 and P44-DSB-Rpi,Rpi-2,Rpi-3
  #define ENABLE_DALI 1
  #define ENABLE_DALI_INPUTS 1
  #define ENABLE_ENOCEAN 1
  #define ENABLE_ENOCEAN_SECURE 1
  #define ENABLE_ENOCEAN_SHADOW 1
  #define ENABLE_HUE 1
  #define ENABLE_LEDCHAIN 1
  #define ENABLE_ELDAT 1
  #define ENABLE_ZF 1
  #define ENABLE_STATIC 1
  #define ENABLE_FCU_SUPPORT 1
  #define ENABLE_EXTERNAL 1
  #define ENABLE_SCRIPTED 1
  #define ENABLE_CUSTOM_SINGLEDEVICE 1
  #define ENABLE_EVALUATORS 1
  #define ENABLE_PROXYDEVICES 1
  #define ENABLE_LOCALCONTROLLER 1
  #define ENABLE_SETTINGS_FROM_FILES 0 // disabled because not needed
  #define ENABLE_JSONCFGAPI 1 // FIXME: %%% for now, we still want to have the JSON CFG API in any case, even if built with ENABLE_UBUS
  #define ENABLE_JSONBRIDGEAPI 1
  #define USE_AVAHI_CORE 0 // use dbus version of avahi via libavahi-client so other daemons can use it as well
  #define BUTTON_NOT_AVAILABLE_AT_START 1 // button has uboot function at system startup, so use alternative factory reset
  #define SELFTESTING_ENABLED 1
#elif P44_BUILD_RB
  // Raspian build (debian package, usually)
  #define ENABLE_ENOCEAN 1
  #define ENABLE_ENOCEAN_SECURE 1
  #define ENABLE_ENOCEAN_SHADOW 1
  #define ENABLE_HUE 1
  #define ENABLE_LEDCHAIN 1
  #define ENABLE_ELDAT 1
  #define ENABLE_STATIC 1
  #define ENABLE_FCU_SUPPORT 1
  #define ENABLE_EXTERNAL 1
  #define ENABLE_SCRIPTED 1
  #define ENABLE_CUSTOM_SINGLEDEVICE 1
  #define ENABLE_EVALUATORS 1
  #define ENABLE_PROXYDEVICES 1
  #define ENABLE_LOCALCONTROLLER 1
  #define ENABLE_MODBUS 1
  #define ENABLE_SETTINGS_FROM_FILES 0 // disabled because not needed
  #define ENABLE_JSONCFGAPI 1
  #define ENABLE_JSONBRIDGEAPI 1
  #define USE_AVAHI_CORE 0 // use dbus version of avahi via libavahi-client so other daemons can use it as well
#else
  // Default build options unless defined otherwise already
  #ifndef ENABLE_HUE
    #define ENABLE_HUE 1
  #endif
  #ifndef ENABLE_STATIC
    #define ENABLE_STATIC 1
  #endif
  #ifndef ENABLE_EXTERNAL
    #define ENABLE_EXTERNAL 1
  #endif
  #ifndef ENABLE_SCRIPTED
    #define ENABLE_SCRIPTED 1
  #endif
  #ifndef ENABLE_EVALUATORS
    #define ENABLE_EVALUATORS 1
  #endif
  #ifndef ENABLE_PROXYDEVICES
    #define ENABLE_PROXYDEVICES 1
  #endif
  #ifndef ENABLE_JSONCFGAPI
    #define ENABLE_JSONCFGAPI 1
  #endif
#endif


// dependencies
#if ENABLE_EVALUATORS || ENABLE_LOCALCONTROLLER
  #if defined(ENABLE_P44SCRIPT) && !ENABLE_P44SCRIPT
    #error "ENABLE_EVALUATORS needs ENABLE_P44SCRIPT"
  #endif
  #if !defined(ENABLE_P44SCRIPT)
    #define ENABLE_P44SCRIPT 1
  #endif
#endif



#endif /* defined(__p44vdc__config__) */
