//
//  dsid.cpp
//  vdcd
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "dsid.hpp"

#include <openssl/sha.h>


using namespace p44;


// create empty

void dSID::internalInit()
{
  idType = dsidtype_undefined;
}



dSID::dSID()
{
  internalInit();
}



void dSID::setIdType(DsIdType aIdType)
{
  if (aIdType!=idType) {
    // new type, reset
    idType = aIdType;
    memset(raw.raw, 0, rawBytes);
    switch (idType) {
      case dsidtype_classic:
        idBytes = classicBytes;
        // header byte
        raw.classic[0] = GID96Header;
        // constant manager number
        raw.classic[1] = (ManagerNo>>20) & 0xFF;
        raw.classic[2] = (ManagerNo>>12) & 0xFF;
        raw.classic[3] = (ManagerNo>>4) & 0xFF;
        raw.classic[4] = (ManagerNo<<4) & 0xF0;
        break;
      case dsidtype_sgtin:
        idBytes = sgtinBytes;
        break;
      case dsidtype_uuid:
        idBytes = uuidBytes;
        break;
      default:
        idBytes = 0; // no content
        break;
    }
  }
}



#pragma mark - set SGTIN based dSID from parameters

// 1. vDC can determine GTIN and SerialNumber of Device → combine GTIN and SerialNumber to a SGTIN


void dSID::setGTIN(uint64_t aGTIN, uint8_t aPartition)
{
  // setting GTIN switches to sgtin dsid
  setIdType(dsidtype_sgtin);
 // TODO: implement
}


void dSID::setSerial(uint64_t aSerial)
{
  // setting GTIN switches to sgtin dsid
  setIdType(dsidtype_sgtin);
}



#pragma mark - set UUID based dSID from parameters


// 2. vDC can determine an existing UUID of Device → use existing UUID
// 3. vDC can determine a unique ID of the Device within the kind of the device → generate name based UUIDv5 with unique ID and predefined name space UUID
// 4. vDC can determine a locally unique ID: generate UUIDv5 with local ID and vDC UUID as name space
// 5. vDC can determine MAC address of Device: generate UUIDv1 (MAC based)

// Device kind Name spaces (UUID v4, randomly generated):
//   EnOcean: DSID_ENOCEAN_NAMESPACE_UUID (0ba94a7b-7c92-4dab-b8e3-5fe09e83d0f3)


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


void dSID::setNameInSpace(const string &aName, const dSID &aNameSpace)
{
  uint8_t sha1[SHA_DIGEST_LENGTH]; // buffer for calculating SHA1
  SHA_CTX sha_context;

  // setting name in namespace switches to UUID dsid
  setIdType(dsidtype_uuid);
  // calculate the hash used as basis for a UUIDv5
  SHA1_Init(&sha_context);
  // - hash the name space UUID
  SHA1_Update(&sha_context, &aNameSpace.raw.uuid, uuidBytes);
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
  memcpy(raw.uuid, sha1, uuidBytes);
  // Then:
  // - Set the four most significant bits (bits 12 through 15) of the time_hi_and_version field to the appropriate 4-bit version number from Section 4.1.3.
  // ...means: set the UUID version, is 0x5 here
  raw.uuid[6] = (raw.uuid[6] & 0x0F) | (0x5<<4);
  // - Set the two most significant bits (bits 6 and 7) of the clock_seq_hi_and_reserved to zero and one, respectively.
  // ...means: mark the UUID as RFC4122 type/variant
  raw.uuid[8] = (raw.uuid[8] & 0xC0) | (0x2<<6);
}





#pragma mark - set dSID from classic GID96 class/serial


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


void dSID::setObjectClass(ObjectClass aObjectClass)
{
  // setting object class switches to classic dsid
  setIdType(dsidtype_classic);
  // first nibble of object class shares byte 4 with last nibble of ManagerNo
  raw.classic[4] |= (aObjectClass>>20) & 0x0F; // or in
  // object class 0xFFxxxx is special, contains bits 32..47 of MAC address
  if ((aObjectClass & OBJECTCLASS_MSB_MASK)==DSID_OBJECTCLASS_MACADDRESS) {
    // MAC address object class
    // serialNo can be up to 52 bits (lower 48 reserved for MAC address)
    // Note: bits 48..51 of aSerialNo are mapped into bits 32..35 of the dsid (as a 4 bit extension of the MAC address mapping)
    raw.classic[5] = ((aObjectClass>>12) & 0xF0);
  }
  else {
    // Regular object class
    raw.classic[5] = (aObjectClass>>12) & 0xFF;
    raw.classic[6] = (aObjectClass>>4) & 0xFF;
    // lowest 4 bits of object class combined with highest 4 bit of 36bit aSerialNo
    raw.classic[7] = (raw.classic[7] & 0x0F) | ((aObjectClass<<4) & 0xF0);
  }
}


void dSID::setDsSerialNo(DsSerialNo aSerialNo)
{
  // setting dS serial number switches to classic dsid
  setIdType(dsidtype_classic);
  // object class 0xFFxxxx is special, contains bits 32..47 of MAC address
  if ((((raw.classic[4] & 0x0F)<<4) | ((raw.classic[5] & 0xF0)>>4))==MACADDRESSCLASS_MSB) {
    // MAC address object class
    // serialNo can be up to 52 bits (lower 48 reserved for MAC address)
    // Note: bits 48..51 of aSerialNo are mapped into bits 32..35 of the dsid (as a 4 bit extension of the MAC address mapping)
    raw.classic[5] = (raw.classic[5] & 0xF0) | ((aSerialNo>>44) & 0x0F);
    raw.classic[6] = (aSerialNo>>36) & 0xFF;
    raw.classic[7] = ((aSerialNo>>28) & 0xF0) | ((aSerialNo>>48) & 0x0F);
  }
  else {
    // lowest 4 bits of object class combined with highest 4 bit of 36bit aSerialNo
    raw.classic[7] = (raw.classic[7] & 0xF0) | ((aSerialNo>>32) & 0x0F);
  }
  // lower 4 bytes are always bits 0..31 of aSerialNo
  raw.classic[8] = (aSerialNo>>24) & 0xFF;
  raw.classic[9] = (aSerialNo>>16) & 0xFF;
  raw.classic[10] = (aSerialNo>>8) & 0xFF;
  raw.classic[11] = aSerialNo & 0xFF;
}




#pragma mark - set/create dSID from string representation


dSID::dSID(const string &aString)
{
  internalInit();
  setAsString(aString);
}


dSID::dSID(const char *aString)
{
  internalInit();
  setAsString(aString);
}



bool dSID::setAsString(const string &aString)
{
  // detect type
  // - separate SGTIN by detecting period
  // TODO: verify actual format used
  size_t p = aString.find('.');
  bool success = false;
  if (p!=string::npos) {
    // is a SGTIN
    success = setAsSGTIN(aString);
  }
  else {
    p = aString.find('-');
    if (p!=string::npos) {
      // is a UUID
      success = setAsUUID(aString);
    }
    else {
      // must be a classic GID96 dsid
      success = setAsClassic(aString);
    }
  }
  if (!success) setIdType(dsidtype_undefined); // clear
  return success;
}


// internal
bool dSID::setAsClassic(const string &aString)
{
  setIdType(dsidtype_classic);
  return setAsHex(aString);
}


// internal
bool dSID::setAsUUID(const string &aString)
{
  setIdType(dsidtype_uuid);
  return setAsHex(aString);
}


// internal: set as string of hex digits (dashes ignored, everything else non-hex ends parsing)
// number of hex bytes must exactly match idBytes
bool dSID::setAsHex(const string &aString)
{
  const char *p = aString.c_str();
  int byteIndex = 0;
  uint8_t b = 0;
  bool firstNibble = true;
  char c;
  while ((c = *p++)!=0 && byteIndex<idBytes) {
    if (c=='-') continue; // dashes allowed in UUID
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
      raw.raw[byteIndex++]=b;
      firstNibble = true;
    }
  }
  // valid if all bytes read
  return byteIndex==idBytes;
}


// set as SGTIN
bool dSID::setAsSGTIN(const string &aString)
{
  // TODO: implement
  return false;
}





#pragma mark - getting dSID string representation



string dSID::getString() const
{
  string s;
  switch (idType) {
    case dsidtype_classic: {
      for (int i=0; i<classicBytes; i++) {
        string_format_append(s, "%02X", raw.classic[i]);
      }
      break;
    }
    case dsidtype_sgtin: {
      // TODO: implement
      s = "SGTIN%%%";
      break;
    }
    case dsidtype_uuid: {
      const int8_t numSegs = 5;
      const uint8_t uuidsegments[numSegs] = { 4,2,2,2,6 };
      int8_t i = 0;
      for (uint8_t seg=0; seg<numSegs; seg++) {
        if (seg>0) s += '-'; // not first segment, separate
        for (uint8_t j=0; j<uuidsegments[seg]; ++j) {
          string_format_append(s, "%02X", raw.classic[i]);
          ++i;
        }
      }
      break;
    }
    default:
      break;
  }
  return s;
}


#pragma mark - comparison


bool dSID::operator== (const dSID &aDSID) const
{
  if (idType!=aDSID.idType) return false;
  return memcmp(raw.raw, aDSID.raw.raw, idBytes)==0;
}


bool dSID::operator< (const dSID &aDSID) const
{
  if (idType==aDSID.idType)
    return memcmp(raw.raw, aDSID.raw.raw, idBytes)<0;
  else
    return idType<aDSID.idType;
}















