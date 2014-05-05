//
//  Copyright (c) 2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "crc32.hpp"


// Using the CRC32 calculation as shown on: http://www.zorc.breitbandkatze.de/crc.html

// Configured CRC32 to match BSD/OSX's "cksum -o3" and Digi's "update_flash":
// - CRC order = 32
// - CRC polynominal = 0x4C11DB7
// - Initial value = 0xFFFFFFFF (direct)
// - Final XOR value = 0xFFFFFFFF
// - reverse data bytes, reverse CRC result before Final XOR

// Optimized for this case
static const uint32_t CRC32_polynominal = 0xEDB88320; // is bit reversal of CRC32 polynominal 0x4C11DB7ul;

using namespace p44;


Crc32::Crc32()
{
  reset();
}


void Crc32::reset()
{
  crc = 0xFFFFFFFF;
}


void Crc32::addByte(uint8_t aByte)
{
  for (uint8_t j=0x01; j; j<<=1) {
    bool bit = crc & 1;
    crc>>= 1;
    if (aByte & j) bit^= 1;
    if (bit) crc^= CRC32_polynominal;
  }
}


void Crc32::addBytes(size_t aNumBytes, const uint8_t *aBytesP)
{
  for (size_t i=0; i<aNumBytes; ++i) {
    addByte(aBytesP[i]);
  }
}


void Crc32::addCStr(const char *aCStr)
{
  while (char c = *aCStr++)
    addByte(c);
}


uint32_t Crc32::getCRC() const
{
  return ~crc;
}


