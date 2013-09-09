//
//  huedevice.cpp
//  vdcd
//
//  Created by Lukas Zeller on 02.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "huedevice.hpp"
#include "huedevicecontainer.hpp"

#include "fnv.hpp"

#include "lightbehaviour.hpp"

using namespace p44;


HueDevice::HueDevice(HueDeviceContainer *aClassContainerP, const string &aLightID) :
  inherited(aClassContainerP),
  lightID(aLightID)
{
  // hue devices are lights
  setPrimaryGroup(group_yellow_light);
  // derive the dSID
  deriveDSID();
  // use light settings, which include a scene table
  deviceSettings = DeviceSettingsPtr(new LightDeviceSettings(*this));
  // set the behaviour
  LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
  l->setHardwareOutputConfig(outputFunction_dimmer, true, 8.5); // hue lights are always dimmable, one hue = 8.5W
  l->setHardwareName(string_format("brightness of hue light #%s", lightID.c_str()));
  l->initBrightnessParams(1,255); // brightness range is 1..255
  addBehaviour(l);
}



HueDeviceContainer &HueDevice::hueDeviceContainer()
{
  return *(static_cast<HueDeviceContainer *>(classContainerP));
}



void HueDevice::initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset)
{
  // query light attributes and state
  string url = string_format("/lights/%s", lightID.c_str());
  hueDeviceContainer().hueComm.apiQuery(url.c_str(), boost::bind(&HueDevice::deviceStateReceived, this, aCompletedCB, aFactoryReset, _2, _3));
}


void HueDevice::deviceStateReceived(CompletedCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aDeviceInfo, ErrorPtr aError)
{
  if (Error::isOK(aError) && aDeviceInfo) {
    JsonObjectPtr o;
    // get model name from device
    hueModel.clear();
    o = aDeviceInfo->get("type");
    if (o) {
      hueModel = o->stringValue();
    }
    o = aDeviceInfo->get("modelid");
    if (o) {
      hueModel += ": " + o->stringValue();
    }
    // get current brightness
    JsonObjectPtr state = aDeviceInfo->get("state");
    Brightness bri = 0;
    if (state) {
      o = state->get("on");
      if (o && o->boolValue()) {
        // lamp is on
        bri = 255; // default to full brightness
        o = state->get("bri");
        if (o) {
          bri = o->int32Value();
        }
        // set current brightness
        boost::static_pointer_cast<LightBehaviour>(outputs[0])->initOutputValue(bri);
      }
    }
  }
  // let superclass initialize as well
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}



void HueDevice::checkPresence(PresenceCB aPresenceResultHandler)
{
  // query the device
  string url = string_format("/lights/%s", lightID.c_str());
  hueDeviceContainer().hueComm.apiQuery(url.c_str(), boost::bind(&HueDevice::presenceStateReceived, this, aPresenceResultHandler, _2, _3));
}



void HueDevice::presenceStateReceived(PresenceCB aPresenceResultHandler, JsonObjectPtr aDeviceInfo, ErrorPtr aError)
{
  bool reachable = false;
  if (Error::isOK(aError) && aDeviceInfo) {
    JsonObjectPtr state = aDeviceInfo->get("state");
    if (state) {
      // Note: 2012 hue bridge firmware always returns 1 for this.
      JsonObjectPtr o = state->get("reachable");
      reachable = o && o->boolValue();
    }
  }
  aPresenceResultHandler(reachable);
}



void HueDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  checkPresence(boost::bind(&HueDevice::disconnectableHandler, this, aForgetParams, aDisconnectResultHandler, _1));
}


void HueDevice::disconnectableHandler(bool aForgetParams, DisconnectCB aDisconnectResultHandler, bool aPresent)
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




void HueDevice::updateOutputValue(OutputBehaviour &aOutputBehaviour)
{
  if (aOutputBehaviour.getIndex()==0) {
    string url = string_format("/lights/%s/state", lightID.c_str());
    JsonObjectPtr newState = JsonObject::newObj();
    Brightness b = aOutputBehaviour.valueForHardware();
    if (b==0) {
      // light off
      newState->add("on", JsonObject::newBool(false));
    }
    else {
      // light on
      newState->add("on", JsonObject::newBool(true));
      newState->add("bri", JsonObject::newInt32(b)); // 0..255
    }
    // for on and off, set transition time (1/10 second resolution)
    newState->add("transitiontime", JsonObject::newInt64(aOutputBehaviour.transitionTimeForHardware()/(100*MilliSecond)));
    LOG(LOG_DEBUG, "HueDevice: setting new brightness = %d\n", b);
    hueDeviceContainer().hueComm.apiAction(httpMethodPUT, string_format("/lights/%s/state", lightID.c_str()).c_str(), newState, NULL);
  }
  else
    return inherited::updateOutputValue(aOutputBehaviour); // let superclass handle this
}



void HueDevice::deriveDSID()
{
  Fnv64 hash;
	
	// we have no unqiquely defining device information, construct something as reproducible as possible
	// - use class container's ID
	string s = classContainerP->deviceClassContainerInstanceIdentifier();
	hash.addBytes(s.size(), (uint8_t *)s.c_str());
	// - add-in the console device name
  #warning "TEST ONLY: lightID is only semi-clever basis for a hash!!!"
  hash.addBytes(lightID.length(), (uint8_t *)lightID.c_str());
  #if FAKE_REAL_DSD_IDS
  dsid.setObjectClass(DSID_OBJECTCLASS_DSDEVICE);
  dsid.setSerialNo(hash.getHash28()<<4); // leave lower 4 bits for input number
  #warning "TEST ONLY: faking digitalSTROM device addresses, possibly colliding with real devices"
  #else
  dsid.setObjectClass(DSID_OBJECTCLASS_MACADDRESS); // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
  dsid.setSerialNo(0x7000000000000ll+hash.getHash48());
  #endif
  // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
}


string HueDevice::description()
{
  string s = inherited::description();
  return s;
}
