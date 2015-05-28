/*
 * vdcd_common.hpp
 *
 *  Created on: Apr 10, 2013
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2014 by plan44.ch/luz
 */

#ifndef P44VDCD_COMMON_HPP_
#define P44VDCD_COMMON_HPP_

#if !defined(DIGI_ESP) && !defined(RASPBERRYPI)
  // non official release platform
  #undef ALWAYS_DEBUG // override per-file ALWAYS_DEBUG
  #define ALWAYS_DEBUG 1 // if set, DBGLOG statements will always be included, even in non-debug builds (but can be silenced via loglevel)
#endif

// auto-disable some features depending on platform
// - No i2c on Mac or DigiESP, but always on RaspberryPi
#if (defined(__APPLE__) || defined(DIGI_ESP)) && !defined(RASPBERRYPI)
  #define DISABLE_I2C 1
#endif
// - No OLA or LED Chains on DIGI_ESP
#if defined(DIGI_ESP)
  #define DISABLE_OLA 1
  #define DISABLE_LEDCHAIN 1
#endif
#if !defined(RASPBERRYPI)
  #define DISABLE_LEDCHAIN 1
#endif


#include "p44_common.hpp"

#include "application.hpp"



#endif /* P44VDCD_COMMON_HPP_ */
