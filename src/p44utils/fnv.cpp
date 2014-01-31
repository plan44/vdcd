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

#include "fnv.hpp"


// Using the FNV hash: http://www.isthe.com/chongo/tech/comp/fnv/

// There is a minor variation of the FNV hash algorithm known as FNV-1a:
// hash = offset_basis
// for each octet_of_data to be hashed
//   hash = hash xor octet_of_data
//   hash = hash * FNV_prime
// return hash
//
// For 32bit FNV hash:
//  FNV_prime = 16777619
//  offset_basis = 2166136261

static const uint32_t FNV32_prime = 16777619ul;
static const uint32_t FNV32_offset_basis = 2166136261ul;

using namespace p44;


Fnv32::Fnv32()
{
  reset();
}


void Fnv32::reset()
{
  hash = FNV32_offset_basis;
}

void Fnv32::addByte(uint8_t aByte)
{
  hash ^= aByte;
  hash *= FNV32_prime;
}


void Fnv32::addBytes(size_t aNumBytes, const uint8_t *aBytesP)
{
  for (size_t i=0; i<aNumBytes; ++i) {
    addByte(aBytesP[i]);
  }
}


void Fnv32::addCStr(const char *aCStr)
{
  while (char c = *aCStr++)
    addByte(c);
}


uint32_t Fnv32::getHash() const
{
  return hash;
}



static const uint64_t FNV64_prime = 1099511628211ull;
static const uint64_t FNV64_offset_basis = 14695981039346656037ull;


Fnv64::Fnv64()
{
  reset();
}


void Fnv64::reset()
{
  hash = FNV64_offset_basis;
}

void Fnv64::addByte(uint8_t aByte)
{
  hash ^= aByte;
  hash *= FNV64_prime;
}


void Fnv64::addBytes(size_t aNumBytes, const uint8_t *aBytesP)
{
  for (size_t i=0; i<aNumBytes; ++i) {
    addByte(aBytesP[i]);
  }
}


void Fnv64::addCStr(const char *aCStr)
{
  while (char c = *aCStr++)
    addByte(c);
}



uint64_t Fnv64::getHash() const
{
  return hash;
}


#pragma mark - folded down variants with less bits

// for non 2^x bit sized hashes, recommened practice is x-or folding:

// If you need an x-bit hash where x is not a power of 2, then we recommend
// that you compute the FNV hash that is just larger than x-bits and xor-fold
// the result down to x-bits. By xor-folding we mean shift the excess high order
// bits down and xor them with the lower x-bits.

#define MASK_48 (((uint64_t)1<<48)-1) /* i.e., (uint64_t)0xffffffffffff */

uint64_t Fnv64::getHash48() const
{
  return (hash>>48) ^ (hash & MASK_48);
}

#define MASK_36 (((uint64_t)1<<36)-1) /* i.e., (uint64_t)0xffffffff */

uint64_t Fnv64::getHash36() const
{
  return (hash>>36) ^ (hash & MASK_36);
}


#define MASK_32 (((uint64_t)1<<32)-1) /* i.e., (uint64_t)0xfffffff */

uint64_t Fnv64::getHash32() const
{
  return (hash>>32) ^ (hash & MASK_32);
}

#define MASK_28 (((uint64_t)1<<28)-1) /* i.e., (uint64_t)0xffffffffffff */

uint32_t Fnv32::getHash28() const
{
  return (hash>>28) ^ (hash & MASK_28);
}

uint32_t Fnv64::getHash28() const
{
  return (uint32_t)((hash>>28) ^ (hash & MASK_28));
}
