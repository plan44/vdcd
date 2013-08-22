//
//  dsparams.hpp
//  vdcd
//
//  Created by Lukas Zeller on 31.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
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
