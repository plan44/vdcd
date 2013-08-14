//
//  fnv.h
//  p44utils
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44utils__fnv__
#define __p44utils__fnv__

#include "p44_common.hpp"

using namespace std;

namespace p44 {


  class Fnv32 {
    uint32_t hash;
  public:
    Fnv32();
    void reset();
    void addByte(uint8_t aByte);
    void addBytes(size_t aNumBytes, uint8_t *aBytesP);
		void addCStr(const char *aCStr);
    uint32_t getHash() const;
    uint32_t getHash28() const; ///< get hash "xor folded down" to 28bits
  };


  class Fnv64 {
    uint64_t hash;
  public:
    Fnv64();
    void reset();
    void addByte(uint8_t aByte);
    void addBytes(size_t aNumBytes, uint8_t *aBytesP);
		void addCStr(const char *aCStr);
    uint64_t getHash() const;
    uint32_t getHash28() const; ///< get hash "xor folded down" to 28bits
    uint64_t getHash32() const; ///< get hash "xor folded down" to 32bits
    uint64_t getHash36() const; ///< get hash "xor folded down" to 36bits
    uint64_t getHash48() const; ///< get hash "xor folded down" to 48bits
  };

} // namespace p44


#endif /* defined(__p44utils__fnv__) */
