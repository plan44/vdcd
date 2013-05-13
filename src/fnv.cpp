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


void Fnv32::addBytes(size_t aNumBytes, uint8_t *aBytesP)
{
  for (size_t i=0; i<aNumBytes; ++i) {
    addByte(aBytesP[i]);
  }
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


void Fnv64::addBytes(size_t aNumBytes, uint8_t *aBytesP)
{
  for (size_t i=0; i<aNumBytes; ++i) {
    addByte(aBytesP[i]);
  }
}


uint64_t Fnv64::getHash() const
{
  return hash;
}


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
