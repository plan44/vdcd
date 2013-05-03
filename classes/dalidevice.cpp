//
//  dalidevice.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "dalidevice.hpp"

#include "fnv.hpp"


DaliDevice::DaliDevice(DaliDeviceContainer *aClassContainerP) :
  Device((DeviceClassContainer *)aClassContainerP)
{
}


void DaliDevice::setDeviceInfo(DaliDeviceInfo aDeviceInfo)
{
  // store the info record
  deviceInfo = aDeviceInfo; // copy
  // derive the dSID
  deriveDSID();
}


void DaliDevice::deriveDSID()
{
  dsid.setObjectClass(DSID_OBJECTCLASS_MACADDRESS); // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
  Fnv64 hash;
  if (deviceInfo.uniquelyIdentifiing()) {
    // Valid device info
    // - add GTIN (6 bytes = 48bits, MSB to LSB)
    hash.addByte((deviceInfo.gtin>>40) & 0xFF);
    hash.addByte((deviceInfo.gtin>>32) & 0xFF);
    hash.addByte((deviceInfo.gtin>>24) & 0xFF);
    hash.addByte((deviceInfo.gtin>>16) & 0xFF);
    hash.addByte((deviceInfo.gtin>>8) & 0xFF);
    hash.addByte((deviceInfo.gtin) & 0xFF);
    // - add Serial number (all 8 bytes, usually only last 4 are used)
    hash.addByte((deviceInfo.serialNo>>56) & 0xFF);
    hash.addByte((deviceInfo.serialNo>>52) & 0xFF);
    hash.addByte((deviceInfo.serialNo>>48) & 0xFF);
    hash.addByte((deviceInfo.serialNo>>40) & 0xFF);
    hash.addByte((deviceInfo.serialNo>>32) & 0xFF);
    hash.addByte((deviceInfo.serialNo>>16) & 0xFF);
    hash.addByte((deviceInfo.serialNo>>8) & 0xFF);
    hash.addByte((deviceInfo.serialNo) & 0xFF);
  }
  else {
    // no unqiquely defining device information, construct something as reproducible as possible
    // - use class container's ID
    string s = classContainerP->deviceClassContainerInstanceIdentifier();
    hash.addBytes(s.size(), (uint8_t *)s.c_str());
    // - and add the DALI short address
    hash.addByte(deviceInfo.shortAddress);
  }
  // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
  dsid.setSerialNo(0x7000000000000ll+hash.getHash48());
}


string DaliDevice::description()
{
  string s = inherited::description();
  s.append(deviceInfo.description());
  return s;
}
