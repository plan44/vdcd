//
//  Copyright (c) 2013 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__dsparams__
#define __vdcd__dsparams__

#include "vdcd_common.hpp"

using namespace std;

namespace p44 {

  typedef struct {
    uint16_t offset;
    uint8_t size;
    const char *name;
  } paramEntry;


  typedef struct {
    uint8_t bankNo;
    uint8_t variant;
    const paramEntry *bankParams;
  } bankEntry;


  class DsParams : public P44Obj
  {

    static const paramEntry *paramEntryForOffset(const paramEntry *aBankTable, uint8_t aOffset);

  public:

    static const paramEntry *paramEntryForBankOffset(uint8_t aBank, uint8_t aOffset, uint8_t aVariant = 0);

    static const paramEntry *paramEntryForName(const char *aName, uint8_t aVariant = 0);

  };

}


#endif /* defined(__vdcd__dsparams__) */
