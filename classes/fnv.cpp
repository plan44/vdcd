//
//  fnv.cpp
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
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

static const uint32_t FNV32_prime = 16777619;
static const uint32_t FNV32_offset_basis = 2166136261;


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


void Fnv32::addBytes(size_t aNumBytes, uint8_t *aBytesP)
{
  for (size_t i=0; i<aNumBytes; ++i) {
    addByte(aBytesP[i]);
  }
}


uint32_t Fnv32::getFNV32() const
{
  return hash;
}
