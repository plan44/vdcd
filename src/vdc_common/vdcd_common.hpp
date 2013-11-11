/*
 * vdcd_common.hpp
 *
 *  Created on: Apr 10, 2013
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2013 by plan44.ch/luz
 */

#ifndef P44BRIDGED_COMMON_HPP_
#define P44BRIDGED_COMMON_HPP_

#define FAKE_REAL_DSD_IDS 1 // if set to 1, dsids will be made look like aizo devices (class 0)

#ifndef DIGI_ESP
  #define ALWAYS_DEBUG 1 // if set, logger statements will always be included, even in non-debug builds (but can be silenced via loglevel)
#endif

#include "p44_common.hpp"

#include "application.hpp"



#endif /* P44BRIDGED_COMMON_HPP_ */
