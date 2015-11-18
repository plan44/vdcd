//
//  Copyright (c) 2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "vzughomedevice.hpp"

#if ENABLE_VZUGHOME


#include "outputbehaviour.hpp"
#include "sensorbehaviour.hpp"


using namespace p44;

#define POLL_INTERVAL (10*Second)
#define ERROR_RETRY_INTERVAL (30*Second)


#pragma mark - VZugHomeDevice

VZugHomeDevice::VZugHomeDevice(VZugHomeDeviceContainer *aClassContainerP, const string aBaseURL) :
  inherited(aClassContainerP),
  deviceModel(model_unknown),
  mostRecentPush(0)
{
  vzugHomeComm.baseURL = aBaseURL;
  setPrimaryGroup(group_black_joker); // TODO: what is the correct color for whiteware?
  installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*this)));
  // - set the output behaviour
  OutputBehaviourPtr o = OutputBehaviourPtr(new OutputBehaviour(*this));
  o->setHardwareOutputConfig(outputFunction_switch, outputmode_binary, usage_undefined, false, -1);
  o->setGroupMembership(group_black_joker, true); // TODO: what is the correct color for whiteware?
  o->addChannel(ChannelBehaviourPtr(new DigitalChannel(*o)));
  addBehaviour(o);
}


void VZugHomeDevice::queryDeviceInfos(StatusCB aCompletedCB)
{
  // query model ID
  vzugHomeComm.apiAction("/hh?command=getModel", JsonObjectPtr(), false, boost::bind(&VZugHomeDevice::gotModelId, this, aCompletedCB, _1, _2));
}


void VZugHomeDevice::gotModelId(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aResult) {
      modelId = aResult->stringValue();
      if (modelId=="MSLQ") {
        deviceModel = model_MSLQ;
      }
      vzugHomeComm.apiAction("/hh?command=getModelDescription", JsonObjectPtr(), false, boost::bind(&VZugHomeDevice::gotModelDescription, this, aCompletedCB, _1, _2));
      return;
    }
    aError = TextError::err("no model ID");
  }
  // early fail
  aCompletedCB(aError);
}


void VZugHomeDevice::gotModelDescription(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aResult) {
      modelDesc = aResult->stringValue();
      vzugHomeComm.apiAction("/hh?command=getSerialNumber", JsonObjectPtr(), false, boost::bind(&VZugHomeDevice::gotSerialNumber, this, aCompletedCB, _1, _2));
      return;
    }
    aError = TextError::err("no model description");
  }
  // early fail
  aCompletedCB(aError);
}


void VZugHomeDevice::gotSerialNumber(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aResult) {
      serialNo = aResult->stringValue();
      deriveDsUid(); // dSUID bases on modelId and serial number
      vzugHomeComm.apiAction("/hh?command=getDeviceName", JsonObjectPtr(), false, boost::bind(&VZugHomeDevice::gotDeviceName, this, aCompletedCB, _1, _2));
      return;
    }
    aError = TextError::err("no serial number");
  }
  // early fail
  aCompletedCB(aError);
}


void VZugHomeDevice::gotDeviceName(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aResult) {
      initializeName(aResult->stringValue());
    }
  }
  // end of init in all cases
  aCompletedCB(aError);
}


void VZugHomeDevice::willBeAdded()
{
  inherited::willBeAdded();
  // now add inputs according to model
  if (deviceModel==model_MSLQ) {
    // - add two temperature sensors
    SensorBehaviourPtr s1 = SensorBehaviourPtr(new SensorBehaviour(*this));
    s1->setHardwareSensorConfig(sensorType_temperature, usage_undefined, 0, 500, 1, POLL_INTERVAL, Never);
    s1->setHardwareName("Gartemperatur");
    addBehaviour(s1);
    SensorBehaviourPtr s2 = SensorBehaviourPtr(new SensorBehaviour(*this));
    s2->setHardwareSensorConfig(sensorType_temperature, usage_undefined, 0, 500, 1, POLL_INTERVAL, Never);
    s2->setHardwareName("Garraumtemperatur");
    addBehaviour(s2);
  }
}



void VZugHomeDevice::initializeDevice(StatusCB aCompletedCB, bool aFactoryReset)
{
  // start regular polling of status and push messages
  getDeviceState();
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}


void VZugHomeDevice::getDeviceState()
{
  vzugHomeComm.apiAction("/hh?command=getCurrentStatus", JsonObjectPtr(), false, boost::bind(&VZugHomeDevice::gotCurrentStatus, this, _1, _2));
}



void VZugHomeDevice::gotCurrentStatus(JsonObjectPtr aResult, ErrorPtr aError)
{
  if (!Error::isOK(aError) || !aResult) {
    scheduleNextStatePoll(aError);
    return;
  }
  // Status:
  if (deviceModel==model_MSLQ) {
    // for MSLQ something like
    //  "Gartemperatur: 45°\nGarraumtemperatur: 120°"
    string s = aResult->stringValue();
    ALOG(LOG_DEBUG, "Status: %s", s.c_str());
    size_t i = 0;
    int sensorIndex = 0;
    while (i<s.size() && sensorIndex<2) {
      size_t e = s.find('\n',i);
      if (e==string::npos) e=s.size();
      // extract temperature
      size_t ne = s.find_last_of("0123456789",e);
      size_t ns = s.find_last_of(' ',ne);
      if (ns<ne && ns!=string::npos) {
        int temp;
        if (sscanf(s.c_str()+ns+1, "%d", &temp)==1) {
          // update sensor
          boost::dynamic_pointer_cast<SensorBehaviour>(sensors[sensorIndex])->updateSensorValue(temp);
        }
      }
      sensorIndex++;
    }
  }
  // query current program
  vzugHomeComm.apiAction("/hh?command=getCurrentProgram", JsonObjectPtr(), false, boost::bind(&VZugHomeDevice::gotCurrentProgram, this, _1, _2));
}


void VZugHomeDevice::gotCurrentProgram(JsonObjectPtr aResult, ErrorPtr aError)
{
  if (!Error::isOK(aError) || !aResult) {
    scheduleNextStatePoll(aError);
    return;
  }
  // Status:
  string s = aResult->stringValue();
  ALOG(LOG_DEBUG, "Program: %s", s.c_str());
  // query current program end
  vzugHomeComm.apiAction("/hh?command=getCurrentProgramEnd", JsonObjectPtr(), true, boost::bind(&VZugHomeDevice::gotCurrentProgramEnd, this, _1, _2));
}


void VZugHomeDevice::gotCurrentProgramEnd(JsonObjectPtr aResult, ErrorPtr aError)
{
  if (!Error::isOK(aError) || !aResult) {
    scheduleNextStatePoll(aError);
    return;
  }
  // Status:
  ALOG(LOG_DEBUG, "Program End: %s", aResult->json_c_str());
  // query activity status
  vzugHomeComm.apiAction("/hh?command=isActive", JsonObjectPtr(), false, boost::bind(&VZugHomeDevice::gotIsActive, this, _1, _2));
}


void VZugHomeDevice::gotIsActive(JsonObjectPtr aResult, ErrorPtr aError)
{
  if (!Error::isOK(aError) || !aResult) {
    scheduleNextStatePoll(aError);
    return;
  }
  // Active?
  string s = aResult->stringValue();
  ALOG(LOG_DEBUG, "isActive: %s", s.c_str());
  int value = 0;
  if (s=="true") value = 100;
  output->getChannelByType(channeltype_default)->syncChannelValue(value);
  // query last push messages
  vzugHomeComm.apiAction("/ai?command=getLastPUSHNotifications", JsonObjectPtr(), true, boost::bind(&VZugHomeDevice::gotLastPUSHNotifications, this, _1, _2));
}



void VZugHomeDevice::gotLastPUSHNotifications(JsonObjectPtr aResult, ErrorPtr aError)
{
  if (!Error::isOK(aError) || !aResult) {
    scheduleNextStatePoll(aError);
    return;
  }
  // Figure out if there are new push notifications
  ALOG(LOG_DEBUG, "Last Push notifications: %s", aResult->json_c_str());
  // TODO: implement it
  int i = 0;
  uint64_t newMostRecent = mostRecentPush;
  while (aResult->arrayLength()>i) {
    JsonObjectPtr push = aResult->arrayGet(i);
    // {"date":"2012-11-29T14:59:53+01:00","message":"Türe geoeffnet."}
    JsonObjectPtr o = push->get("date");
    if (o) {
      string date = o->stringValue();
      short int Y,M,D,h,m,s;
      if (sscanf(date.c_str(), "%04hd-%02hd-%02hdT%02hd:%02hd:%02hd", &Y, &M, &D, &h, &m, &s)==6) {
        uint64_t timeOrder = (((((((((Y*12)+M)*31)+D)*24)+h)*60)+m)*60)+s;
        o = push->get("message");
        if (o) {
          // check if we need to publish this
          // Note: we suppress processing any messages when the device is polling this for the first time now,
          //  because we would likely replay old messages. We just remember the timestamp of the most recent
          //  message present NOW, and will process pushes that appear LATER
          if (mostRecentPush!=0 && timeOrder>mostRecentPush) {
            // newer than what we've seen in last poll -> process it
            processPushMessage(o->stringValue());
          }
          if (timeOrder>newMostRecent) {
            newMostRecent = timeOrder;
          }
        }
      }
    }
    i++;
  }
  // update to newest message processed
  mostRecentPush = newMostRecent;
  // Schedule next poll, no error
  scheduleNextStatePoll(ErrorPtr());
}



void VZugHomeDevice::processPushMessage(const string aMessage)
{
  ALOG(LOG_NOTICE, "Push Message: %s", aMessage.c_str());
}




void VZugHomeDevice::scheduleNextStatePoll(ErrorPtr aError)
{
  if (!Error::isOK(aError)) {
    ALOG(LOG_INFO, "VZug API error '%s' while polling -> retrying again later", aError->description().c_str());
  }
  MainLoop::currentMainLoop().executeOnce(boost::bind(&VZugHomeDevice::getDeviceState, this), aError ? ERROR_RETRY_INTERVAL : POLL_INTERVAL);
}



VZugHomeDeviceContainer &VZugHomeDevice::getVZugHomeDeviceContainer()
{
  return *(static_cast<VZugHomeDeviceContainer *>(classContainerP));
}


void VZugHomeDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  //  // clear learn-in data from DB
  //  if (ledChainDeviceRowID) {
  //    getLedChainDeviceContainer().db.executef("DELETE FROM devConfigs WHERE rowid=%d", ledChainDeviceRowID);
  //  }
  // disconnection is immediate, so we can call inherited right now
  inherited::disconnect(aForgetParams, aDisconnectResultHandler);
}


void VZugHomeDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void VZugHomeDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  // UUIDv5 with name = deviceClassIdentifier:modelid:serialno
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassIdentifier();
  string_format_append(s, "%s:%s", modelId.c_str(), serialNo.c_str());
  dSUID.setNameInSpace(s, vdcNamespace);
}


string VZugHomeDevice::vendorName()
{
  return "V-Zug";
}



string VZugHomeDevice::modelName()
{
  return modelDesc;
}





bool VZugHomeDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  const char *iconName = "vzughome";
  if (iconName && getIcon(iconName, aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


string VZugHomeDevice::getExtraInfo()
{
  string s;
  s = string_format("V-Zug Home device");
  return s;
}



string VZugHomeDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "\n- V-Zug Home device");
  return s;
}


#endif // ENABLE_VZUGHOME



