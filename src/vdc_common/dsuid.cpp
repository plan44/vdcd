//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "dsuid.hpp"

#include <openssl/sha.h>

#include "fnv.hpp"

using namespace p44;


// create empty

void DsUid::internalInit()
{
  idType = idtype_undefined;
  memset(raw, 0, sizeof(raw));
}



DsUid::DsUid()
{
  internalInit();
}


bool DsUid::empty() const
{
  return idType==idtype_undefined;
}


void DsUid::clear()
{
  internalInit();
}




// Byte offset         0 1 2 3  4 5  6 7  8 9 101112131415 16
// dSUID with UUID  : xxxxxxxx-xxxx-Vxxx-Txxx-xxxxxxxxxxxx ii   (V=version, T=type/variant, ii=subdevice index)
// dSUID with EPC96 : ssssssss ssss 0000 0000 ssssssssssss ii   (ii=subdevice index)

void DsUid::setIdType(DsUidType aIdType)
{
  if (aIdType!=idType) {
    // new type, reset
    idType = aIdType;
    memset(raw, 0, sizeof(raw));
    switch (idType) {
      // classic
      case idtype_classic:
        idBytes = dsidBytes;
        // header byte
        raw[0] = GID96Header;
        // constant manager number
        raw[1] = (ManagerNo>>20) & 0xFF;
        raw[2] = (ManagerNo>>12) & 0xFF;
        raw[3] = (ManagerNo>>4) & 0xFF;
        raw[4] = (ManagerNo<<4) & 0xF0;
        break;
      // dSUID
      case idtype_sgtin:
        raw[0] = SGTIN96Header;
        // fall through
      case idtype_uuid:
      case idtype_gid:
        idBytes = dsuidBytes;
        break;
      default:
        idBytes = 0; // no content
        break;
    }
  }
}


void DsUid::setSubdeviceIndex(uint8_t aSubDeviceIndex)
{
  if (idBytes==dsuidBytes) {
    // is a dSUID, can set subdevice index
    raw[16] = aSubDeviceIndex;
  }
}



#pragma mark - set SGTIN based dSUID from parameters

// 1. vDC can determine GTIN and SerialNumber of Device → combine GTIN and SerialNumber to a SGTIN

// SGTIN96 binary:
//      hhhhhhhh fffpppgg gggggggg gggggggg gggggggg gggggggg gggggggg ggssssss ssssssss ssssssss ssssssss ssssssss
//      00110000 001ppp<--------- 44 bit binary GCP+ItemRef ------------><------- 38 bit serial number ----------->
// dSUID Byte index:
//         0        1         2        3       4        5        10        11      12       13       14       15


// translation table to get GCP bit length for partition value
// Note: Partition Value + 1 = number of decimal digits for item reference including indicator/pad digit
static uint8_t gcpBitLength[7] = { 40, 37, 34, 30, 27, 24, 20 };

void DsUid::setGTIN(uint64_t aGCP, uint64_t aItemRef, uint8_t aPartition)
{
  // setting GTIN switches to sgtin dSUID
  setIdType(idtype_sgtin);
  // total bit length for CGP + itemRef combined are 44bits
  uint64_t binaryGtin = aGCP<<(44-gcpBitLength[aPartition]) | aItemRef;
  // now put into bytes
  // - filter (fixed to 1), partition and upper 2 bits of binaryGtin go into raw[1]
  raw[1] = (0x1<<5) | ((aPartition&0x07)<<2) | (binaryGtin>>42);
  // - raw[2..5]
  raw[2] = (binaryGtin>>34) & 0xFF;
  raw[3] = (binaryGtin>>26) & 0xFF;
  raw[4] = (binaryGtin>>18) & 0xFF;
  raw[5] = (binaryGtin>>10) & 0xFF;
  // - raw[6..9] are left 0 to mark it as non-UUID
  // - raw[10..11] contain more GTIN information
  raw[10] = (binaryGtin>>2) & 0xFF;
  raw[11] = (raw[11] & 0x3F) | ((binaryGtin & 0x03)<<6); // combine lowest 2 bits of GTIN with highest 6 of serial
}


void DsUid::setSerial(uint64_t aSerial)
{
  // setting GTIN switches to sgtin dSUID
  setIdType(idtype_sgtin);
  raw[11] = (raw[11] & 0xC0) | ((aSerial>>32)&0x3F); // combine lowest 2 bits of GTIN with highest 6 of serial
  raw[12] = (aSerial>>24)&0xFF;
  raw[13] = (aSerial>>16)&0xFF;
  raw[14] = (aSerial>>8)&0xFF;
  raw[15] = aSerial&0xFF;
}



#pragma mark - set UUID based DsUid from parameters


// 2. vDC can determine an existing UUID of Device → use existing UUID
// 3. vDC can determine a unique ID of the Device within the kind of the device → generate name based UUIDv5 with unique ID and predefined name space UUID
// 4. vDC can determine a locally unique ID: generate UUIDv5 with local ID and vDC UUID as name space
// 5. vDC can determine MAC address of Device: generate UUIDv1 (MAC based)

// Device kind Name spaces (UUID v4, randomly generated):
//   EnOcean: DSUID_ENOCEAN_NAMESPACE_UUID (0ba94a7b-7c92-4dab-b8e3-5fe09e83d0f3)


// UUID format (see RFC 4122 for details):
//
// 0ba94a7b-7c92-4dab-b8e3-5fe09e83d0f3  // example, enOcean namespace UUID (v4, random)
//
// tltltltl-tmtm-thth-chcl-nononononono
// xxxxxxxx-xxxx-Vxxx-Txxx-xxxxxxxxxxxx
//
// where
//  tl : time_low[4]
//  tm : time_mid[2];
//  th : time_hi_and_version[2];
//  ch : clock_seq_hi_and_reserved[1];
//  cl : clock_seq_low[1];
//  no : node[6];
//
//  xxxxx : actual ID bits
//  V : 4 bit version number (v1, v4, v5 etc.)
//  T : type/variant, 2 upper bits must be 0b10 (i.e. Bit7 of clock_seq_hi_and_reserved = 1, Bit6 of clock_seq_hi_and_reserved = 0)



// Note: We do *not* store the fields above in machine byte order. Internal
//   representation in raw.uuid[] is always network byte order, so it
//   can be directly fed into hashing algorithms (RFC demands hashes are
//   always calculated over network byte order representation).


void DsUid::setNameInSpace(const string &aName, const DsUid &aNameSpace)
{
  uint8_t sha1[SHA_DIGEST_LENGTH]; // buffer for calculating SHA1
  SHA_CTX sha_context;

  // setting name in namespace switches to UUID dSUID
  setIdType(idtype_uuid);
  // calculate the hash used as basis for a UUIDv5
  SHA1_Init(&sha_context);
  // - hash the name space UUID
  SHA1_Update(&sha_context, &aNameSpace.raw, uuidBytes);
  // - hash the name
  SHA1_Update(&sha_context, aName.c_str(), aName.size());
  SHA1_Final(sha1, &sha_context);
  // Now make UUID of it
  // - Set octets zero through 3 of the time_low field to octets zero through 3 of the hash.
  // - Set octets zero and one of the time_mid field to octets 4 and 5 of the hash.
  // - Set octets zero and one of the time_hi_and_version field to octets 6 and 7 of the hash.
  // - Set the clock_seq_hi_and_reserved field to octet 8 of the hash.
  // - Set the clock_seq_low field to octet 9 of the hash.
  // - Set octets zero through five of the node field to octets 10 through 15 of the hash.
  // ...which means: copy byte 0..15 of the sha1 into the UUID bytes 0..15
  memcpy(raw, sha1, uuidBytes);
  // Then:
  // - Set the four most significant bits (bits 12 through 15) of the time_hi_and_version field to the appropriate 4-bit version number from Section 4.1.3.
  // ...means: set the UUID version, is 0x5 here
  raw[6] = (raw[6] & 0x0F) | (0x5<<4);
  // - Set the two most significant bits (bits 6 and 7) of the clock_seq_hi_and_reserved to zero and one, respectively.
  // ...means: mark the UUID as RFC4122 type/variant
  raw[8] = (raw[8] & 0xC0) | (0x2<<6);
}





#pragma mark - set classic dSID from classic GID96 class/serial


// Sample real dsids
// 35 04 17 5F E0 00 00 10 00 00 14 d9  dSM
// 35 04 17 5F E0 00 00 00 00 00 b4 c1	SW-TKM210


// Standard dSID fields:
// - h: 8 bit  : constant header byte 0x35
// - m: 28 bit : constant manager number for Aizo 0x04175FE
// - c: 24 bit : object class
// - d: 36 bit : device serial
// 00 01 02 03 04 05 06 07 08 09 10 11
// hh mm mm mm mc cc cc cd dd dd dd dd

// Class 0xFFxxxx field usage
// - h: 8 bit  : constant header byte 0x35
// - m: 28 bit : constant manager number for Aizo 0x04175FE
// - c: 8 bit  : object class upper 8 bits = 0xFF
// - M: 16 bit : object class lower 16 bits used for MAC address first two bytes
// - X: 4 bit  : device serial upper 4 bits: 0x0 for MAC address, we map bits 48..51 of aSerialNo here
// - N: 32 bit : device serial lower 32 bits used for MAC address last four bytes
// 00 01 02 03 04 05 06 07 08 09 10 11
// hh mm mm mm mc cM MM MX NN NN NN NN


#define MACADDRESSCLASS_MSB (DSID_OBJECTCLASS_MACADDRESS>>16)
#define OBJECTCLASS_MSB_MASK 0xFF0000


void DsUid::setObjectClass(ObjectClass aObjectClass)
{
  // setting object class switches to classic dSUID
  setIdType(idtype_classic);
  // first nibble of object class shares byte 4 with last nibble of ManagerNo
  raw[4] |= (aObjectClass>>20) & 0x0F; // or in
  // object class 0xFFxxxx is special, contains bits 32..47 of MAC address
  if ((aObjectClass & OBJECTCLASS_MSB_MASK)==DSID_OBJECTCLASS_MACADDRESS) {
    // MAC address object class
    // serialNo can be up to 52 bits (lower 48 reserved for MAC address)
    // Note: bits 48..51 of aSerialNo are mapped into bits 32..35 of the dSUID (as a 4 bit extension of the MAC address mapping)
    raw[5] = ((aObjectClass>>12) & 0xF0);
  }
  else {
    // Regular object class
    raw[5] = (aObjectClass>>12) & 0xFF;
    raw[6] = (aObjectClass>>4) & 0xFF;
    // lowest 4 bits of object class combined with highest 4 bit of 36bit aSerialNo
    raw[7] = (raw[7] & 0x0F) | ((aObjectClass<<4) & 0xF0);
  }
}


void DsUid::setDsSerialNo(DsSerialNo aSerialNo)
{
  // setting dS serial number switches to classic dSUID
  setIdType(idtype_classic);
  // object class 0xFFxxxx is special, contains bits 32..47 of MAC address
  if ((((raw[4] & 0x0F)<<4) | ((raw[5] & 0xF0)>>4))==MACADDRESSCLASS_MSB) {
    // MAC address object class
    // serialNo can be up to 52 bits (lower 48 reserved for MAC address)
    // Note: bits 48..51 of aSerialNo are mapped into bits 32..35 of the dSUID (as a 4 bit extension of the MAC address mapping)
    raw[5] = (raw[5] & 0xF0) | ((aSerialNo>>44) & 0x0F);
    raw[6] = (aSerialNo>>36) & 0xFF;
    raw[7] = ((aSerialNo>>28) & 0xF0) | ((aSerialNo>>48) & 0x0F);
  }
  else {
    // lowest 4 bits of object class combined with highest 4 bit of 36bit aSerialNo
    raw[7] = (raw[7] & 0xF0) | ((aSerialNo>>32) & 0x0F);
  }
  // lower 4 bytes are always bits 0..31 of aSerialNo
  raw[8] = (aSerialNo>>24) & 0xFF;
  raw[9] = (aSerialNo>>16) & 0xFF;
  raw[10] = (aSerialNo>>8) & 0xFF;
  raw[11] = aSerialNo & 0xFF;
}




#pragma mark - set/create DsUid from string representation


DsUid::DsUid(const string &aString)
{
  internalInit();
  setAsString(aString);
}


DsUid::DsUid(const char *aString)
{
  internalInit();
  setAsString(aString);
}



bool DsUid::setAsString(const string &aString)
{
  const char *p = aString.c_str();
  int byteIndex = 0;
  uint8_t b = 0;
  bool firstNibble = true;
  bool hasDashes = false;
  char c;
  while ((c = *p++)!=0 && byteIndex<dsuidBytes) {
    if (c=='-') {
      hasDashes = true; // a dash has occurred, might be a pure UUID (without 17th byte)
      continue; // dashes allowed but ignored
    }
    c = toupper(c)-'0';
    if (c>9) c -= ('A'-'9'-1);
    if (c<0 || c>0xF)
      break; // invalid char, done
    if (firstNibble) {
      b = c<<4;
      firstNibble = false;
    }
    else {
      b |= c;
      raw[byteIndex++]=b;
      firstNibble = true;
    }
  }
  // determine type of dSUID
  if (byteIndex==dsidBytes && raw[0]==GID96Header) {
    // must be a classic dSUID (pure GID96)
    idType = idtype_classic;
    idBytes = dsidBytes;
  }
  else if (byteIndex==dsuidBytes || (hasDashes && byteIndex==uuidBytes)) {
    // must be a dSUID (when read with dashes, it can also be a pure UUID without the subdevice index byte)
    idType = idtype_other;
    idBytes = dsuidBytes;
    // - determine subtype
    if (raw[6]==0 && raw[7]==0 && raw[8]==0 && raw[9]==0) {
      // EPC96, check which one
      if (raw[0]==SGTIN96Header)
        idType = idtype_sgtin;
      else if (raw[0]==GID96Header)
        idType = idtype_gid;
    }
    else {
      // UUID
      idType = idtype_uuid;
    }
    if (byteIndex==uuidBytes)
      raw[16] = 0; // specified as pure UUID, set subdevice index == 0
  }
  else {
    // unknown format
    setIdType(idtype_undefined);
    return false;
  }
  return true;
}

#pragma mark - getting derived classic equivalent of a dSUID (FNV hashing method)


DsUid DsUid::getDerivedClassicId(ObjectClass aObjectClass) const
{
  if (idType==idtype_classic) {
    // already classic, just return myself
    return *this;
  }
  // all other ID types: generate a hashed derivate
  DsUid classicId;
  // Calculation of serial (i.e serial number part of classic dsid)
  Fnv32 hash;
  hash.addBytes(idBytes-1, raw);
  uint32_t serial = hash.getHash();
  serial ^= (serial>>28); // fold 4 MSBits into 4 LSBits, so hash has now 28 bits of length
  serial <<= 3; // reserve three LSBits for enumerated devices
  serial |= raw[idBytes-1] & 0x07; // add in last three bits of dSUID's enumeration byte
  serial |= 0x80000000; // Always set MSBit to make sure these hashed IDs cannot collide with real terminal blocks
  // create ID now
  classicId.setObjectClass(aObjectClass);
  classicId.setDsSerialNo(serial);
  return classicId;
}



#pragma mark - getting dSUID string representation



string DsUid::getString() const
{
  string s;
  for (int i=0; i<idBytes; i++) {
    string_format_append(s, "%02X", raw[i]);
  }
  return s;
}


#pragma mark - comparison


bool DsUid::operator== (const DsUid &aDsUid) const
{
  if (idType!=aDsUid.idType) return false;
  return memcmp(raw, aDsUid.raw, idBytes)==0;
}


bool DsUid::operator< (const DsUid &aDsUid) const
{
  if (idType==aDsUid.idType)
    return memcmp(raw, aDsUid.raw, idBytes)<0;
  else
    return idType<aDsUid.idType;
}















