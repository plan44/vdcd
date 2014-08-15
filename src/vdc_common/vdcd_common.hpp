/*
 * vdcd_common.hpp
 *
 *  Created on: Apr 10, 2013
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2014 by plan44.ch/luz
 */

#ifndef P44VDCD_COMMON_HPP_
#define P44VDCD_COMMON_HPP_

#define VDCS_PSEUDO_CLASSIC_DSID 1 // if set to 1, vdcs will expose pseudo-classic IDs to the vdsm


#if !defined(DIGI_ESP) && !defined(RASPBERRYPI)
  // non official release platform
  #define ALWAYS_DEBUG 1 // if set, DBGLOG statements will always be included, even in non-debug builds (but can be silenced via loglevel)
#endif

#include "p44_common.hpp"

#include "application.hpp"



#endif /* P44VDCD_COMMON_HPP_ */
