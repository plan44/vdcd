//
//  Copyright (c) 2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 7

#include "mystromdevice.hpp"

#if ENABLE_STATIC

#include "lightbehaviour.hpp"


using namespace p44;


MyStromDevice::MyStromDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  StaticDevice((DeviceClassContainer *)aClassContainerP),
  myStromComm(MainLoop::currentMainLoop())
{
  // config must be: mystromdevicehost[:token]:(light|relay)
  size_t i = aDeviceConfig.rfind(":");
  bool isLight = false;
  if (i!=string::npos) {
    string mode = aDeviceConfig.substr(i+1,string::npos);
    isLight = (mode=="light");
    deviceHostName = aDeviceConfig.substr(0,i);
  }
  else {
    deviceHostName = aDeviceConfig;
  }
  // now see if hostname includes token
  i = deviceHostName.find(":");
  if (i!=string::npos) {
    // split
    deviceToken = deviceHostName.substr(i+1,string::npos);
    deviceHostName.erase(i,string::npos);
  }
  // configure device now
  if (isLight) {
    // light device
    primaryGroup = group_yellow_light;
    installSettings(DeviceSettingsPtr(new LightDeviceSettings(*this)));
    LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
    l->setHardwareOutputConfig(outputFunction_switch, outputmode_binary, usage_undefined, false, -1);
    l->setHardwareName("on/off light");
    addBehaviour(l);
  }
  else {
    // general purpose relay
    primaryGroup = group_black_joker;
    installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*this)));
    OutputBehaviourPtr o = OutputBehaviourPtr(new OutputBehaviour(*this));
    o->setHardwareOutputConfig(outputFunction_switch, outputmode_binary, usage_undefined, false, -1);
    o->setHardwareName("on/off switch");
    o->setGroupMembership(group_black_joker, true); // put into joker group by default
    o->addChannel(ChannelBehaviourPtr(new DigitalChannel(*o)));
    addBehaviour(o);
  }
  // dsuid
	deriveDsUid();
}



bool MyStromDevice::myStromApiQuery(JsonWebClientCB aResponseCB, string aPathAndArgs)
{
  string url = string_format("http://%s/%s", deviceHostName.c_str(), aPathAndArgs.c_str());
  FOCUSLOG("myStromApiQuery: %s", url.c_str());
  return myStromComm.jsonReturningRequest(url.c_str(), aResponseCB, "GET");
}


bool MyStromDevice::myStromApiAction(HttpCommCB aResponseCB, string aPathAndArgs)
{
  string url = string_format("http://%s/%s", deviceHostName.c_str(), aPathAndArgs.c_str());
  FOCUSLOG("myStromApiAction: %s", url.c_str());
  return myStromComm.httpRequest(url.c_str(), aResponseCB, "GET");
}




void MyStromDevice::initializeDevice(StatusCB aCompletedCB, bool aFactoryReset)
{
  // get current state of the switch
  if (!myStromApiQuery(boost::bind(&MyStromDevice::initialStateReceived, this, aCompletedCB, aFactoryReset, _1, _2), "report")) {
    // could not even issue request, init complete
    inherited::initializeDevice(aCompletedCB, aFactoryReset);
  }
}


void MyStromDevice::initialStateReceived(StatusCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  if (Error::isOK(aError) && aJsonResponse) {
    JsonObjectPtr o = aJsonResponse->get("relay");
    if (o) {
      output->getChannelByIndex(0)->syncChannelValue(o->boolValue() ? 100 : 0);
    }
  }
  // anyway, consider initialized
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}


void MyStromDevice::checkPresence(PresenceCB aPresenceResultHandler)
{
  if (applyInProgress) {
    // cannot query now, update in progress, assume still present
    aPresenceResultHandler(true);
    return;
  }
  // query the device
  myStromApiQuery(boost::bind(&MyStromDevice::presenceStateReceived, this, aPresenceResultHandler, _1, _2), "report");
}



void MyStromDevice::presenceStateReceived(PresenceCB aPresenceResultHandler, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  bool reachable = false;
  if (Error::isOK(aError) && aJsonResponse) {
    reachable = true;
  }
  aPresenceResultHandler(reachable);
}



void MyStromDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  bool sendState = false;
  bool newState;
  LightBehaviourPtr l = boost::dynamic_pointer_cast<LightBehaviour>(output);
  if (l) {
    // light
    if (l->brightnessNeedsApplying()) {
      // need to update switch state
      sendState = true;
      newState = l->brightnessForHardware()>0;
    }
  }
  else {
    // standard output
    ChannelBehaviourPtr ch = output->getChannelByIndex(0);
    if (ch->needsApplying()) {
      sendState = true;
      newState = ch->getChannelValueBool();
    }
  }
  if (sendState) {
    myStromApiAction(boost::bind(&MyStromDevice::channelValuesSent, this, aDoneCB, _1, _2), string_format("relay?state=%d", newState ? 1 : 0));
    return;
  }
  // no other operation for this call
  if (aDoneCB) aDoneCB();
  return;
}


void MyStromDevice::channelValuesSent(SimpleCB aDoneCB, string aResponse, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    output->getChannelByIndex(0)->channelValueApplied();
  }
  else {
    FOCUSLOG("myStrom API error: %s", aError->description().c_str());
  }
  // confirm done
  if (aDoneCB) aDoneCB();
}



void MyStromDevice::syncChannelValues(SimpleCB aDoneCB)
{
  // query switch state
  myStromApiQuery(boost::bind(&MyStromDevice::channelValuesReceived, this, aDoneCB, _1, _2), "report");
}



void MyStromDevice::channelValuesReceived(SimpleCB aDoneCB, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  if (Error::isOK(aError) && aJsonResponse) {
    JsonObjectPtr o = aJsonResponse->get("relay");
    if (o) {
      output->getChannelByIndex(0)->syncChannelValueBool(o->boolValue());
    }
  }
  // done
  inherited::syncChannelValues(aDoneCB);
}



void MyStromDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::mystromhost_xxxx where xxxx=IP address or host name
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  s += "::mystromhost_" + deviceHostName;
  dSUID.setNameInSpace(s, vdcNamespace);
}



string MyStromDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "\n- myStrom Switch");
  return s;
}


#endif // ENABLE_STATIC
