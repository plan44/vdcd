//
//  dalidevice.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "dalidevice.hpp"
#include "dalidevicecontainer.hpp"

#include "fnv.hpp"

#include "lightbehaviour.hpp"

#include <math.h>

using namespace p44;


DaliDevice::DaliDevice(DaliDeviceContainer *aClassContainerP) :
  Device((DeviceClassContainer *)aClassContainerP),
  cachedBrightness(0)
{
}

DaliDeviceContainer *DaliDevice::daliDeviceContainerP()
{
  return (DaliDeviceContainer *)classContainerP;
}


void DaliDevice::setDeviceInfo(DaliDeviceInfo aDeviceInfo)
{
  // store the info record
  deviceInfo = aDeviceInfo; // copy
  // derive the dSID
  deriveDSID();
  // set the behaviour
  LightBehaviour *l = new LightBehaviour(this);
  setDSBehaviour(l);
  #ifdef DEBUG
//  #warning // TODO: %%%% remove this q&d debug hack
//  if (deviceInfo.shortAddress==0) {
//    l->setBehaviourParam("MINDIM", 0, 20);
//    // get scene
//    l->setBehaviourParam("SCE", T0_S1, 78);
//  }
  #endif
}


void DaliDevice::initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset)
{
  // query actual arc power level
  daliDeviceContainerP()->daliComm.daliSendQuery(
    deviceInfo.shortAddress,
    DALICMD_QUERY_ACTUAL_LEVEL,
    boost::bind(&DaliDevice::queryActualLevelResponse,this, aCompletedCB, aFactoryReset, _2, _3, _4)
  );
}


void DaliDevice::queryActualLevelResponse(CompletedCB aCompletedCB, bool aFactoryReset, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  cachedBrightness = 0; // default to 0
  if (Error::isOK(aError) && !aNoOrTimeout) {
    // this is my current arc power, save it as brightness for dS system side queries
    cachedBrightness = arcpowerToBrightness(aResponse);
    LOG(LOG_DEBUG, "DaliDevice: updated brightness cache: arc power = %d, brightness = %d\n", aResponse, cachedBrightness);
  }
  // initialize the light behaviour with the current output value
  static_cast<LightBehaviour *>(getDSBehaviour())->setLogicalBrightness(cachedBrightness);
  // let superclass initialize as well
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}



void DaliDevice::ping()
{
  // query the device
  daliDeviceContainerP()->daliComm.daliSendQuery(
    deviceInfo.shortAddress, DALICMD_QUERY_CONTROL_GEAR,
    boost::bind(&DaliDevice::pingResponse, this, _2, _3, _4)
  );
}


void DaliDevice::pingResponse(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  if (DaliComm::isYes(aNoOrTimeout, aResponse, aError, false)) {
    // proper YES (without collision) received
    pong();
  }
}


int16_t DaliDevice::getOutputValue(int aChannel)
{
  if (aChannel==0)
    return cachedBrightness;
  else
    return inherited::getOutputValue(aChannel); // let superclass handle this
}



void DaliDevice::setOutputValue(int aChannel, int16_t aValue)
{
  if (aChannel==0) {
    cachedBrightness = aValue;
    // update actual dimmer value
    uint8_t power = brightnessToArcpower(cachedBrightness);
    LOG(LOG_DEBUG, "DaliDevice: set new brightness = %d, arc power = %d\n", cachedBrightness, power);
    daliDeviceContainerP()->daliComm.daliSendDirectPower(deviceInfo.shortAddress, power);
  }
  else
    return inherited::setOutputValue(aChannel, aValue); // let superclass handle this
}



uint8_t DaliDevice::brightnessToArcpower(Brightness aBrightness)
{
  double intensity = (double)aBrightness/255;
  if (intensity<0) intensity = 0;
  if (intensity>1) intensity = 1;
  return log10((intensity*9)+1)*254;
}



Brightness DaliDevice::arcpowerToBrightness(int aArcpower)
{
  double intensity = (pow(10, aArcpower/254.0)-1)/9;
  return intensity*255;
}




void DaliDevice::deriveDSID()
{
  // create a hash
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
  #if FAKE_REAL_DSD_IDS
  dsid.setObjectClass(DSID_OBJECTCLASS_DSDEVICE);
  dsid.setSerialNo(hash.getHash32());
  #warning "TEST ONLY: faking digitalSTROM device addresses, possibly colliding with real devices"
  #else
  // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
  dsid.setObjectClass(DSID_OBJECTCLASS_MACADDRESS); // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
  dsid.setSerialNo(0x7000000000000ll+hash.getHash48());
  #endif
}


string DaliDevice::description()
{
  string s = inherited::description();
  s.append(deviceInfo.description());
  return s;
}
