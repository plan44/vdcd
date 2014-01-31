//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __p44utils__fnv__
#define __p44utils__fnv__

#include "p44_common.hpp"

using namespace std;

namespace p44 {


  class Fnv32  : public P44Obj
  {
    uint32_t hash;
  public:
    Fnv32();
    void reset();
    void addByte(uint8_t aByte);
    void addBytes(size_t aNumBytes, const uint8_t *aBytesP);
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
    void addBytes(size_t aNumBytes, const uint8_t *aBytesP);
		void addCStr(const char *aCStr);
    uint64_t getHash() const;
    uint32_t getHash28() const; ///< get hash "xor folded down" to 28bits
    uint64_t getHash32() const; ///< get hash "xor folded down" to 32bits
    uint64_t getHash36() const; ///< get hash "xor folded down" to 36bits
    uint64_t getHash48() const; ///< get hash "xor folded down" to 48bits
  };

} // namespace p44


#endif /* defined(__p44utils__fnv__) */
