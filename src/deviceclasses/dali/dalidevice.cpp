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


// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set DEBUGFOCUS to 1 to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define DEBUGFOCUS 0


#include "dalidevice.hpp"
#include "dalidevicecontainer.hpp"

#include "fnv.hpp"

#include "lightbehaviour.hpp"

#include <math.h>

using namespace p44;


DaliDevice::DaliDevice(DaliDeviceContainer *aClassContainerP) :
  Device((DeviceClassContainer *)aClassContainerP),
  transitionTime(Infinite), // invalid
  dimPerMS(0), // none
  dimRepeaterTicket(0),
  fadeRate(0xFF), fadeTime(0xFF) // unlikely values
{
  // DALI devices are always light (in this implementation, at least)
  setPrimaryGroup(group_yellow_light);
}

DaliDeviceContainer &DaliDevice::daliDeviceContainer()
{
  return *(static_cast<DaliDeviceContainer *>(classContainerP));
}


void DaliDevice::setDeviceInfo(DaliDeviceInfo aDeviceInfo)
{
  // store the info record
  deviceInfo = aDeviceInfo; // copy
  // derive the dSUID
  deriveDsUid();
  // use light settings, which include a scene table
  deviceSettings = DeviceSettingsPtr(new LightDeviceSettings(*this));
  // set the behaviour
  LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
  l->setHardwareOutputConfig(outputFunction_dimmer, usage_undefined, true, 160); // DALI ballasts are always dimmable, // TODO: %%% somewhat arbitrary 2*8=W max wattage
  l->setHardwareName(string_format("DALI %d",deviceInfo.shortAddress));
  addBehaviour(l);
}


bool DaliDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("dali_dimmer", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}




void DaliDevice::initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset)
{
  // query actual arc power level
  daliDeviceContainer().daliComm->daliSendQuery(
    deviceInfo.shortAddress,
    DALICMD_QUERY_ACTUAL_LEVEL,
    boost::bind(&DaliDevice::queryActualLevelResponse,this, aCompletedCB, aFactoryReset, _1, _2, _3)
  );
}


void DaliDevice::queryActualLevelResponse(CompletedCB aCompletedCB, bool aFactoryReset, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  if (Error::isOK(aError) && !aNoOrTimeout) {
    // this is my current arc power, save it as brightness for dS system side queries
    Brightness bri = arcpowerToBrightness(aResponse);
    output->getChannelByIndex(0)->syncChannelValue(bri);
    LOG(LOG_DEBUG, "DaliDevice: updated brightness cache from actual device value: arc power = %d, brightness = %0.1f\n", aResponse, bri);
  }
  // query the minimum dimming level
  daliDeviceContainer().daliComm->daliSendQuery(
    deviceInfo.shortAddress,
    DALICMD_QUERY_MIN_LEVEL,
    boost::bind(&DaliDevice::queryMinLevelResponse,this, aCompletedCB, aFactoryReset, _1, _2, _3)
  );
}


void DaliDevice::queryMinLevelResponse(CompletedCB aCompletedCB, bool aFactoryReset, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  Brightness minLevel = 0; // default to 0
  if (Error::isOK(aError) && !aNoOrTimeout) {
    // this is my current arc power, save it as brightness for dS system side queries
    minLevel = arcpowerToBrightness(aResponse);
    LOG(LOG_DEBUG, "DaliDevice: retrieved minimum dimming level: arc power = %d, brightness = %0.1f\n", aResponse, minLevel);
  }
  // initialize the light behaviour with the minimal dimming level
  LightBehaviourPtr l = boost::static_pointer_cast<LightBehaviour>(output);
  l->initMinBrightness(minLevel);
  // let superclass initialize as well
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}



void DaliDevice::setTransitionTime(MLMicroSeconds aTransitionTime)
{
  LightBehaviourPtr l = boost::static_pointer_cast<LightBehaviour>(output);
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
      daliDeviceContainer().daliComm->daliSendDtrAndConfigCommand(deviceInfo.shortAddress, DALICMD_STORE_DTR_AS_FADE_TIME, tr);
      fadeTime = tr;
    }
    transitionTime = aTransitionTime;
  }
}



void DaliDevice::checkPresence(PresenceCB aPresenceResultHandler)
{
  // query the device
  daliDeviceContainer().daliComm->daliSendQuery(
    deviceInfo.shortAddress, DALICMD_QUERY_CONTROL_GEAR,
    boost::bind(&DaliDevice::checkPresenceResponse, this, aPresenceResultHandler, _1, _2, _3)
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
      aDisconnectResultHandler(false);
    }
  }
}


void DaliDevice::applyChannelValues(DoneCB aDoneCB, bool aForDimming)
{
  LightBehaviourPtr lightBehaviour = boost::dynamic_pointer_cast<LightBehaviour>(output);
  if (lightBehaviour && lightBehaviour->brightnessNeedsApplying()) {
    setTransitionTime(lightBehaviour->transitionTimeToNewBrightness());
    // update actual dimmer value
    uint8_t power = brightnessToArcpower(lightBehaviour->brightnessForHardware());
    LOG(LOG_INFO, "DaliDevice: setting new brightness = %0.0f, arc power = %d\n", lightBehaviour->brightnessForHardware(), power);
    daliDeviceContainer().daliComm->daliSendDirectPower(deviceInfo.shortAddress, power);
    lightBehaviour->brightnessApplied(); // confirm having applied the value
  }
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


// optimized DALI dimming implementation
void DaliDevice::dimChannel(DsChannelType aChannelType, DsDimMode aDimMode)
{
  // start dimming
  if (aChannelType==channeltype_brightness) {
    ChannelBehaviourPtr ch = getChannelByType(aChannelType);
    DBGFLOG(LOG_INFO, "DALI dimChannel for brightness %s\n", aDimMode==dimmode_stop ? "STOPS dimming" : (aDimMode==dimmode_up ? "starts dimming UP" : "starts dimming DOWN"));
    MainLoop::currentMainLoop().cancelExecutionTicket(dimRepeaterTicket); // stop and previous dimming activity
    // Use DALI UP/DOWN dimming commands
    if (aDimMode==dimmode_stop) {
      // stop dimming - send MASK
      daliDeviceContainer().daliComm->daliSendDirectPower(deviceInfo.shortAddress, DALIVALUE_MASK);
    }
    else {
      // start dimming
      // - configure new fade rate if current does not match
      if (dimPerMS!=ch->getDimPerMS()) {
        dimPerMS = ch->getDimPerMS();
        //   Fade rate: R = 506/SQRT(2^X) [steps/second] -> x = ln2((506/R)^2) : R=44 [steps/sec] -> x = 7
        double h = 506.0/(dimPerMS*1000);
        h = log(h*h)/log(2);
        uint8_t fr = h>0 ? (uint8_t)h : 0;
        LOG(LOG_DEBUG, "DaliDevice: new dimming rate = %f Steps/second, calculated FADE_RATE setting = %f (rounded %d)\n", dimPerMS*1000, h, fr);
        if (fr!=fadeRate) {
          LOG(LOG_DEBUG, "DaliDevice: setting DALI FADE_RATE to %d\n", fr);
          daliDeviceContainer().daliComm->daliSendDtrAndConfigCommand(deviceInfo.shortAddress, DALICMD_STORE_DTR_AS_FADE_RATE, fr);
          fadeRate = fr;
        }
      }
      // - use repeated UP and DOWN commands
      dimRepeater(deviceInfo.shortAddress, aDimMode==dimmode_up ? DALICMD_UP : DALICMD_DOWN, MainLoop::now());
    }
  }
  else {
    // not my channel, use standard implementation
    inherited::dimChannel(aChannelType, aDimMode);
  }
}


void DaliDevice::dimRepeater(DaliAddress aDaliAddress, uint8_t aCommand, MLMicroSeconds aCycleStartTime)
{
  daliDeviceContainer().daliComm->daliSendCommand(aDaliAddress, aCommand);
  // schedule next command
  // - DALI UP and DOWN run 200mS, but can be repeated earlier, so we use 150mS to make sure we don't have hickups
  //   Note: DALI bus speed limits commands to 120Bytes/sec max, i.e. about 20 per 150mS, i.e. max 10 lamps dimming
  dimRepeaterTicket = MainLoop::currentMainLoop().executeOnceAt(boost::bind(&DaliDevice::dimRepeater, this, aDaliAddress, aCommand, _1), aCycleStartTime+200*MilliSecond);
}



#warning "// TODO: add error status polling and use DsBehaviour::setHardwareError() to report it"


uint8_t DaliDevice::brightnessToArcpower(Brightness aBrightness)
{
  double intensity = (double)aBrightness/255;
  if (intensity<0) intensity = 0;
  if (intensity>1) intensity = 1;
  return log10((intensity*9)+1)*254; // 0..254, 255 is MASK and is reserved to stop fading
}



Brightness DaliDevice::arcpowerToBrightness(int aArcpower)
{
  double intensity = (pow(10, aArcpower/254.0)-1)/9;
  return intensity*255;
}




void DaliDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s;
  if (deviceInfo.uniquelyIdentifying()) {
    // uniquely identified by GTIN+Serial, but unknown partition value:
    // - Proceed according to dS rule 2:
    //   "vDC can determine GTIN and serial number of Device → combine GTIN and
    //    serial number to form a GS1-128 with Application Identifier 21:
    //    "(01)<GTIN>(21)<serial number>" and use the resulting string to
    //    generate a UUIDv5 in the GS1-128 name space"
    s = string_format("(01)%llu(21)%llu", deviceInfo.gtin, deviceInfo.serialNo);
  }
  else {
    // not uniquely identified by itself:
    // - generate id in vDC namespace
    //   UUIDv5 with name = classcontainerinstanceid::daliShortAddrDecimal
    s = classContainerP->deviceClassContainerInstanceIdentifier();
    string_format_append(s, "::%d", deviceInfo.shortAddress);
  }
  dSUID.setNameInSpace(s, vdcNamespace);
}


string DaliDevice::hardwareGUID()
{
  if (deviceInfo.gtin==0)
    return ""; // none
  // return as GS1 element strings
  return string_format("gs1:(01)%llu(21)%llu", deviceInfo.gtin, deviceInfo.serialNo);
}


string DaliDevice::modelGUID()
{
  if (deviceInfo.oem_gtin==0)
    return ""; // none
  // return as GS1 element strings with Application Identifier 01=GTIN
  return string_format("gs1:(01)%llu", deviceInfo.gtin);
}


string DaliDevice::oemGUID()
{
  if (deviceInfo.oem_gtin==0)
    return ""; // none
  // return as GS1 element strings with Application Identifiers 01=GTIN and 21=Serial
  return string_format("gs1:(01)%llu(21)%llu", deviceInfo.oem_gtin, deviceInfo.oem_serialNo);
}


string DaliDevice::description()
{
  string s = inherited::description();
  s.append(deviceInfo.description());
  return s;
}
