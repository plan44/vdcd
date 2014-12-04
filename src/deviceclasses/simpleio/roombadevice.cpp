//
//  Copyright (c) 2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
#define FOCUSLOGLEVEL 5


#include "roombadevice.hpp"

#include "fnv.hpp"

#include "buttonbehaviour.hpp"


using namespace p44;


#pragma mark - RoombaDevice

RoombaDevice::RoombaDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  StaticDevice((DeviceClassContainer *)aClassContainerP),
  roombaJSON(MainLoop::currentMainLoop())
{
  // config must be: ip address of RooWifi
  size_t i = aDeviceConfig.find(":");
  roombaIPAddress = aDeviceConfig;
  // Simulate relay device
  // - defaults to black (app)
  primaryGroup = group_black_joker;
  // - standard device settings with scene table
  installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*this)));
  // - add generic output behaviour
  OutputBehaviourPtr o = OutputBehaviourPtr(new OutputBehaviour(*this));
  o->setHardwareOutputConfig(outputFunction_switch, usage_undefined, false, -1);
  o->setGroupMembership(group_black_joker, true); // put into joker group by default
  // digital channel
  o->addChannel(ChannelBehaviourPtr(new DigitalChannel(*o)));
  addBehaviour(o);
  // dsuid
	deriveDsUid();
}



//bool RoombaDevice::roombaApiCall(JsonWebClientCB aResponseCB, string aArgs)
//{
//  string url = string_format("https://api.spark.io/v1/devices/%s/vdsd", sparkCoreID.c_str());
//  string data;
//  HttpComm::appendFormValue(data, "access_token", sparkCoreToken);
//  HttpComm::appendFormValue(data, "args", aArgs);
//  LOG(LOG_DEBUG,"sparkApiCall to %s - data = %s\n", url.c_str(), data.c_str());
//  return sparkCloudComm.jsonReturningRequest(url.c_str(), aResponseCB, "POST", data);
//}



void RoombaDevice::initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset)
{
  // %%% for now
  // get params
  string url = string_format("http://%s/roomba.json",roombaIPAddress.c_str());
  roombaJSON.jsonRequest(url.c_str(), boost::bind(&RoombaDevice::statusReceived, this, aCompletedCB, aFactoryReset, _1, _2));

//  // get vdsd API version
//  if (!sparkApiCall(boost::bind(&SparkIoDevice::apiVersionReceived, this, aCompletedCB, aFactoryReset, _1, _2), "version")) {
//    // could not even issue request, init complete
//    inherited::initializeDevice(aCompletedCB, aFactoryReset);
//  }
}




void RoombaDevice::statusReceived(CompletedCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  string s = aJsonResponse->stringValue();
  FOCUSLOG("****** Roomba Status: %s", s.c_str());
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}





void RoombaDevice::checkPresence(PresenceCB aPresenceResultHandler)
{
  aPresenceResultHandler(true);
}



void RoombaDevice::applyChannelValues(DoneCB aDoneCB, bool aForDimming)
{
  // get value to set
  ChannelBehaviourPtr ch = output->getChannelByIndex(0);
  bool shouldRun = ch->getChannelValue()>50;
  // get current status
  string url = string_format("http://%s/roomba.json",roombaIPAddress.c_str());
  roombaJSON.jsonRequest(url.c_str(), boost::bind(&RoombaDevice::runStatusReceived, this, aDoneCB, shouldRun, _1, _2));


//  // %%% for now
//  // confirm done
//  if (aDoneCB) aDoneCB();

//  channelValuesSent(aDoneCB, JsonObjectPtr(), ErrorPtr());
//  // error, wrong API
//  channelValuesSent(sl, aDoneCB, JsonObjectPtr(), ErrorPtr(new WebError(415)));
}



void RoombaDevice::runStatusReceived(DoneCB aDoneCB, bool aShouldRun, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  string s = aJsonResponse->stringValue();
  FOCUSLOG("****** Response: %s\n", s.c_str());
  JsonObjectPtr response = aJsonResponse->get("response");
  JsonObjectPtr r18 = response->get("r16");
  JsonObjectPtr o = r18->get("value");
  int v = o->int32Value();
  bool runsNow = v<-500;
  bool docked = v>=1000;
  FOCUSLOG("****** r16, current: %d -> considers roomba %s\n", v, runsNow ? "running" : "docked/stopped");
  if (runsNow) {
    string url = string_format("http://%s/roomba.cgi?button=%s",roombaIPAddress.c_str(), aShouldRun ? "CLEAN" : "DOCK");
    FOCUSLOG("****** sending first command %s\n", url.c_str());
    roombaJSON.jsonRequest(url.c_str(), boost::bind(&RoombaDevice::firstCommandSent, this, aDoneCB, aShouldRun, _1, _2));
  }
  else {
    sendFinalCommand(aDoneCB, aShouldRun);
  }
}


void RoombaDevice::firstCommandSent(DoneCB aDoneCB, bool aShouldRun, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  // wait a bit
  FOCUSLOG("****** first command set, waiting 2 seconds\n");
  MainLoop::currentMainLoop().executeOnce(boost::bind(&RoombaDevice::sendFinalCommand, this, aDoneCB, aShouldRun), 2*Second);
}


void RoombaDevice::sendFinalCommand(DoneCB aDoneCB, bool aShouldRun)
{
  string url = string_format("http://%s/roomba.cgi?button=%s",roombaIPAddress.c_str(), aShouldRun ? "CLEAN" : "DOCK");
  FOCUSLOG("****** sending final command %s\n", url.c_str());
  roombaJSON.jsonRequest(url.c_str(), boost::bind(&RoombaDevice::finalCommandSent, this, aDoneCB, aShouldRun));
}


void RoombaDevice::finalCommandSent(DoneCB aDoneCB, bool aShouldRun)
{
  FOCUSLOG("****** final command done\n");
  inherited::applyChannelValues(aDoneCB, false);
}




void RoombaDevice::syncChannelValues(DoneCB aDoneCB)
{
//  // query light attributes and state
//  sparkApiCall(boost::bind(&SparkIoDevice::channelValuesReceived, this, aDoneCB, _1, _2), "state");
  inherited::syncChannelValues(aDoneCB);
}



bool RoombaDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (output && getIcon("roomba", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


//void RoombaDevice::channelValuesReceived(DoneCB aDoneCB, JsonObjectPtr aJsonResponse, ErrorPtr aError)
//{
//  if (Error::isOK(aError) && aJsonResponse) {
//    JsonObjectPtr o = aJsonResponse->get("return_value");
//    SparkLightBehaviourPtr sl = boost::dynamic_pointer_cast<SparkLightBehaviour>(output);
//    if (o && sl) {
//      uint32_t state = o->int32Value();
//      uint8_t mode = (state>>24) & 0xFF;
//      sl->sparkmode->syncChannelValue(mode);
//      if (mode==3) {
//        // RGB lamp
//        double r = (state>>16) & 0xFF;
//        double g = (state>>8) & 0xFF;
//        double b = state & 0xFF;
//        sl->setRGB(r, g, b, 255);
//      }
//      else {
//        // brightness only
//        sl->brightness->syncChannelValue(state & 0xFF);
//      }
//    }
//  }
//  // done
//  inherited::syncChannelValues(aDoneCB);
//}



void RoombaDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::SparkCoreID
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  s += "::" + roombaIPAddress;
  dSUID.setNameInSpace(s, vdcNamespace);
}



string RoombaDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "- digitalSTROM dev days 2014 roomba hack device\n");
  return s;
}
