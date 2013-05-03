//
//  dsid.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "dsid.hpp"


#pragma mark - creating dSIDs

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

// create empty

void dSID::internalInit()
{
  // 8 bit header
  memset(raw.bytes, 0, dsidBytes);
  // constant manager number
  raw.bytes[0] = GID96Header;
  raw.bytes[1] = (ManagerNo>>20) & 0xFF;
  raw.bytes[2] = (ManagerNo>>12) & 0xFF;
  raw.bytes[3] = (ManagerNo>>4) & 0xFF;
  raw.bytes[4] = (ManagerNo<<4) & 0xF0;
}


dSID::dSID()
{
  internalInit();
}


// set from object class and serial number

void dSID::setObjectClass(ObjectClass aObjectClass)
{
  // first nibble of object class shares byte 4 with last nibble of ManagerNo
  raw.bytes[4] |= (aObjectClass>>20) & 0x0F; // or in
  // object class 0xFFxxxx is special, contains bits 32..47 of MAC address
  if ((aObjectClass & OBJECTCLASS_MSB_MASK)==DSID_OBJECTCLASS_MACADDRESS) {
    // MAC address object class
    // serialNo can be up to 52 bits (lower 48 reserved for MAC address)
    // Note: bits 48..51 of aSerialNo are mapped into bits 32..35 of the dsid (as a 4 bit extension of the MAC address mapping)
    raw.bytes[5] = ((aObjectClass>>12) & 0xF0);
  }
  else {
    // Regular object class
    raw.bytes[5] = (aObjectClass>>12) & 0xFF;
    raw.bytes[6] = (aObjectClass>>4) & 0xFF;
    // lowest 4 bits of object class combined with highest 4 bit of 36bit aSerialNo
    raw.bytes[7] = (raw.bytes[7] & 0x0F) | ((aObjectClass<<4) & 0xF0);
  }
}


void dSID::setSerialNo(SerialNo aSerialNo)
{
  // object class 0xFFxxxx is special, contains bits 32..47 of MAC address
  if (getObjectClassMSB()==MACADDRESSCLASS_MSB) {
    // MAC address object class
    // serialNo can be up to 52 bits (lower 48 reserved for MAC address)
    // Note: bits 48..51 of aSerialNo are mapped into bits 32..35 of the dsid (as a 4 bit extension of the MAC address mapping)
    raw.bytes[5] = (raw.bytes[5] & 0xF0) | ((aSerialNo>>44) & 0x0F);
    raw.bytes[6] = (aSerialNo>>36) & 0xFF;
    raw.bytes[7] = ((aSerialNo>>28) & 0xF0) | ((aSerialNo>>48) & 0x0F);
  }
  else {
    // lowest 4 bits of object class combined with highest 4 bit of 36bit aSerialNo
    raw.bytes[7] = (raw.bytes[7] & 0xF0) | ((aSerialNo>>32) & 0x0F);
  }
  // lower 4 bytes are always bits 0..31 of aSerialNo
  raw.bytes[8] = (aSerialNo>>24) & 0xFF;
  raw.bytes[9] = (aSerialNo>>16) & 0xFF;
  raw.bytes[10] = (aSerialNo>>8) & 0xFF;
  raw.bytes[11] = aSerialNo & 0xFF;
}



// create from strings

dSID::dSID(string &aString)
{
  internalInit();
  setAsString(aString);
}


bool dSID::setAsString(string &aString)
{
  const char *p = aString.c_str();
  int byteIndex = 0;
  uint8_t b = 0;
  bool firstNibble = true;
  char c;
  while ((c = *p++)!=0 && byteIndex<dsidBytes) {
    if (c=='-') continue;
    c = toupper(c)-'0';
    if (c>9) c -= ('A'-'9'+1);
    if (c<0 || c>0xF)
      break; // invalid char, done
    if (firstNibble) {
      b = c<<4;
      firstNibble = false;
    }
    else {
      b |= c;
      raw.bytes[byteIndex++]=b;
      firstNibble = true;
    }
  }
  // valid if all bytes read
  return byteIndex==dsidBytes;
}


#pragma mark - getting dSIDs representations

dSID::RawID dSID::getRawId() const
{
  // copy of the raw bytes
  return raw;
}


const uint8_t *dSID::getBytesP() const
{
  // direct pointer to raw bytes
  return &(raw.bytes[0]);
}

/// get upper 8 bit of object class
uint8_t dSID::getObjectClassMSB() const
{
  return
    ((raw.bytes[4] & 0x0F)<<4) |
    ((raw.bytes[5] & 0xF0)>>4);
}


dSID::ObjectClass dSID::getObjectClass() const
{
  uint8_t cc = getObjectClassMSB();
  if (cc==MACADDRESSCLASS_MSB)
    return (ObjectClass)cc<<16; // don't extract lower bits, these belong to serial
  // return all 24 bits
  return
    ((ObjectClass)cc<<16) |
    (((ObjectClass)raw.bytes[5] & 0x0F)<<12) |
    (((ObjectClass)raw.bytes[6] & 0xFF)<<4) |
    (((ObjectClass)raw.bytes[7] & 0xF0)>>4);
}


dSID::SerialNo dSID::getSerialNo() const
{
  SerialNo s =
    raw.bytes[0] |
    ((SerialNo)raw.bytes[1]<<8) |
    ((SerialNo)raw.bytes[2]<<16) |
    ((SerialNo)raw.bytes[3]<<24);
  if (getObjectClassMSB()==MACADDRESSCLASS_MSB) {
    // extract lower 16 bits of object class field for bits 32..47
    s |=
      (((SerialNo)raw.bytes[7] & 0xF0)<<28) |
      (((SerialNo)raw.bytes[6] & 0xFF)<<20) |
      (((SerialNo)raw.bytes[8] & 0x0F)<<12);
    // extract bits 32..35 of serial field for bits 48..51
    s |=
      (((SerialNo)raw.bytes[7] & 0x0F)<<48);
  }
  else {
    // use bits 32..35 as bits 32..35 of serial number
    s |= ((SerialNo)raw.bytes[4]<<32) & 0x0F;
  }
  return s;
}


string dSID::getString() const
{
  string s;
  for (int i=0; i<dSID::dsidBytes; i++) {
    string_format_append(s, "%02X", raw.bytes[i]);
  }
  return s;
}


#pragma mark - comparison

bool dSID::operator== (const dSID &aDSID) const
{
  return memcmp(raw.bytes, aDSID.getBytesP(), dsidBytes)==0;
}


bool dSID::operator< (const dSID &aDSID) const
{
  return memcmp(raw.bytes, aDSID.getBytesP(), dsidBytes)<0;
}















