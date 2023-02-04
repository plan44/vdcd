//  SPDX-License-Identifier: GPL-3.0-or-later
//
//  Copyright (c) 2016-2023 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44features.
//
//  p44lrgraphics is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44lrgraphics is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44lrgraphics. If not, see <http://www.gnu.org/licenses/>.
//


#ifndef __p44features__config__
#define __p44features__config__

// Default build options unless defined otherwise already

#ifndef ENABLE_P44FEATURES
  #define ENABLE_P44FEATURES 0 // disabled by default
#endif

#if ENABLE_P44FEATURES
  // Features:
  // - generic use
  #ifndef ENABLE_FEATURE_LIGHT
    #define ENABLE_FEATURE_LIGHT 1
  #endif
  #ifndef ENABLE_FEATURE_DISPMATRIX
    #define ENABLE_FEATURE_DISPMATRIX 1
  #endif
  #ifndef ENABLE_FEATURE_RFIDS
    #define ENABLE_FEATURE_RFIDS 1
  #endif
  #ifndef ENABLE_FEATURE_INDICATORS
    #define ENABLE_FEATURE_INDICATORS 1
  #endif
  #ifndef ENABLE_FEATURE_INPUTS
    #define ENABLE_FEATURE_INPUTS 1
  #endif
  #ifndef ENABLE_FEATURE_KEYEVENTS
    #define ENABLE_FEATURE_KEYEVENTS 1
  #endif
  #ifndef ENABLE_FEATURE_SPLITFLAPS
    #define ENABLE_FEATURE_SPLITFLAPS 1
  #endif
  // - specific application
  #ifndef ENABLE_FEATURE_WIFITRACK
    #define ENABLE_FEATURE_WIFITRACK 1
  #endif
  // - very specific hardware related stuff
  #ifndef ENABLE_FEATURE_NEURON
    #define ENABLE_FEATURE_NEURON 0
  #endif
  #ifndef ENABLE_FEATURE_HERMEL
    #define ENABLE_FEATURE_HERMEL 0
  #endif
  #ifndef ENABLE_FEATURE_MIXLOOP
    #define ENABLE_FEATURE_MIXLOOP 0
  #endif

  // options
  #ifndef ENABLE_FEATURE_COMMANDLINE
    #define ENABLE_FEATURE_COMMANDLINE 1
  #endif
  #define ENABLE_LEGACY_FEATURE_SCRIPTS 0


  // dependencies
  #define ENABLE_LEDARRANGEMENT (ENABLE_FEATURE_DISPMATRIX || ENABLE_FEATURE_INDICATORS)
  #if ENABLE_FEATURE_RFIDS
    #define ENABLE_RFID 1
  #endif
#endif // ENABLE_P44FEATURES

#endif // __p44features__config__
