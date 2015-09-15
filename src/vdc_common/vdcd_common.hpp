//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__common__
#define __vdcd__common__

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



#endif /* defined(__vdcd__common__) */
