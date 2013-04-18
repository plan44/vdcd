//
//  fnv.h
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__fnv__
#define __p44bridged__fnv__

#include "p44bridged_common.hpp"

class Fnv32 {
  uint32_t hash;
public:
  Fnv32();
  void reset();
  void addByte(uint8_t aByte);
  void addBytes(size_t aNumBytes, uint8_t *aBytesP);
  uint32_t getFNV32();
};


#endif /* defined(__p44bridged__fnv__) */
