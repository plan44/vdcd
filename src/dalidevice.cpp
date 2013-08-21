//
//  dalidevice.cpp
//  vdcd
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
  cachedBrightness(0),
  transitionTime(Infinite) // invalid
{
}

DaliDeviceContainer &DaliDevice::daliDeviceContainer()
{
  return *(static_cast<DaliDeviceContainer *>(classContainerP));
}


void DaliDevice::setDeviceInfo(DaliDeviceInfo aDeviceInfo)
{
  // store the info record
  deviceInfo = aDeviceInfo; // copy
  // derive the dSID
  deriveDSID();
  // set the behaviour
  LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this,outputs.size()));
  l->setHardwareDimmer(true); // DALI ballasts are always dimmable
  l->setHardwareName(string_format("DALI %d",deviceInfo.shortAddress));
  outputs.push_back(l);
}


void DaliDevice::initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset)
{
  // query actual arc power level
  daliDeviceContainer().daliComm.daliSendQuery(
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
    LOG(LOG_DEBUG, "DaliDevice: updated brightness cache from actual device value: arc power = %d, brightness = %d\n", aResponse, cachedBrightness);
  }
  // query the minimum dimming level
  daliDeviceContainer().daliComm.daliSendQuery(
    deviceInfo.shortAddress,
    DALICMD_QUERY_MIN_LEVEL,
    boost::bind(&DaliDevice::queryMinLevelResponse,this, aCompletedCB, aFactoryReset, _2, _3, _4)
  );
}


void DaliDevice::queryMinLevelResponse(CompletedCB aCompletedCB, bool aFactoryReset, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  Brightness minLevel = 0; // default to 0
  if (Error::isOK(aError) && !aNoOrTimeout) {
    // this is my current arc power, save it as brightness for dS system side queries
    minLevel = arcpowerToBrightness(aResponse);
    LOG(LOG_DEBUG, "DaliDevice: retrieved minimum dimming level: arc power = %d, brightness = %d\n", aResponse, minLevel);
  }
  // initialize the light behaviour with the minimal dimming level
  boost::static_pointer_cast<LightBehaviour>(outputs[0])->initBrightnessParams(cachedBrightness,minLevel,255);
  // let superclass initialize as well
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}


// Fade rate: R = 506/SQRT(2^X) [steps/second] -> x = ln2((506/R)^2) : R=44 [steps/sec] -> x = 7

void DaliDevice::setTransitionTime(MLMicroSeconds aTransitionTime)
{
  if (transitionTime==Infinite || transitionTime!=aTransitionTime) {
    uint8_t tr = 0; // default to 0
    if (aTransitionTime>0) {
      // Fade time: T = 0.5 * SQRT(2^X) [seconds] -> x = ln2((T/0.5)^2) : T=0.25 [sec] -> x = -2, T=10 -> 8.64
      double h = (((double)aTransitionTime/Second)/0.5);
      h = h*h;
      h = log(h)/log(2);
      tr = h>1 ? (uint8_t)h : 1;
      LOG(LOG_DEBUG, "DaliDevice: new transition time = %ld, calculated FADE_TIME setting = %f (rounded %d)\n", aTransitionTime, h, tr);
    }
    if (tr!=fadeTime || transitionTime==Infinite) {
      LOG(LOG_DEBUG, "DaliDevice: setting DALI FADE_TIME to %d\n", tr);
      daliDeviceContainer().daliComm.daliSendDtrAndConfigCommand(deviceInfo.shortAddress, DALICMD_STORE_DTR_AS_FADE_TIME, tr);
      fadeTime = tr;
    }
    transitionTime = aTransitionTime;
  }
}



void DaliDevice::checkPresence(PresenceCB aPresenceResultHandler)
{
  // query the device
  daliDeviceContainer().daliComm.daliSendQuery(
    deviceInfo.shortAddress, DALICMD_QUERY_CONTROL_GEAR,
    boost::bind(&DaliDevice::checkPresenceResponse, this, aPresenceResultHandler, _2, _3, _4)
  );
}


void DaliDevice::checkPresenceResponse(PresenceCB aPresenceResultHandler, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  // present if a proper YES (without collision) received
  aPresenceResultHandler(DaliComm::isYes(aNoOrTimeout, aResponse, aError, false));
}


void DaliDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  checkPresence(boost::bind(&DaliDevice::disconnectableHandler, this, aForgetParams, aDisconnectResultHandler, _1));
}

void DaliDevice::disconnectableHandler(bool aForgetParams, DisconnectCB aDisconnectResultHandler, bool aPresent)
{
  if (!aPresent) {
    // call inherited disconnect
    inherited::disconnect(aForgetParams, aDisconnectResultHandler);
  }
  else {
    // not disconnectable
    if (aDisconnectResultHandler) {
      aDisconnectResultHandler(classContainerP->getDevicePtrForInstance(this), false);
    }
  }
}





int16_t DaliDevice::getOutputValue(int aChannel)
{
  if (aChannel==0)
    return cachedBrightness;
  else
    return inherited::getOutputValue(aChannel); // let superclass handle this
}



void DaliDevice::setOutputValue(int aChannel, int16_t aValue, MLMicroSeconds aTransitionTime)
{
  if (aChannel==0) {
    setTransitionTime(aTransitionTime);
    cachedBrightness = aValue;
    // update actual dimmer value
    uint8_t power = brightnessToArcpower(cachedBrightness);
    LOG(LOG_DEBUG, "DaliDevice: setting new brightness = %d, arc power = %d\n", cachedBrightness, power);
    daliDeviceContainer().daliComm.daliSendDirectPower(deviceInfo.shortAddress, power);
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


string DaliDevice::hardwareGUID()
{
  // GTIN is 24bit company prefix + 20bit item reference, SGTIN adds a 48bit serial number as third element: urn:epc:id:sgtin:COMPANYPREFIX.ITEMREF.SERIALNO
  if (deviceInfo.gtin==0)
    return ""; // none
  return string_format("urn:epc:id:sgtin:%ld.%ld.%lld", (deviceInfo.gtin>>20)&0xFFFFFF, deviceInfo.gtin&0xFFFFF, deviceInfo.serialNo);
}


string DaliDevice::oemGUID()
{
  if (deviceInfo.oem_gtin==0)
    return ""; // none
  // GTIN is 24bit company prefix + 20bit item reference, SGTIN adds a 48bit serial number as third element: urn:epc:id:sgtin:COMPANYPREFIX.ITEMREF.SERIALNO
  return string_format("urn:epc:id:sgtin:%ld.%ld.%lld", (deviceInfo.oem_gtin>>20)&0xFFFFFF, deviceInfo.oem_gtin&0xFFFFF, deviceInfo.oem_serialNo);
}


string DaliDevice::description()
{
  string s = inherited::description();
  s.append(deviceInfo.description());
  return s;
}
