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


using namespace p44;


#pragma mark - VZugHomeDeviceSettings + VZugHomeScene

DsScenePtr VZugHomeDeviceSettings::newDefaultScene(SceneNo aSceneNo)
{
  VZugHomeScenePtr vzugHomeScene = VZugHomeScenePtr(new VZugHomeScene(*this, aSceneNo));
  vzugHomeScene->setDefaultSceneValues(aSceneNo);
  // return it
  return vzugHomeScene;
}


void VZugHomeScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  // set the common simple scene defaults
  inherited::setDefaultSceneValues(aSceneNo);
  // Add special audio scene behaviour
  switch (aSceneNo) {
    // all alarms turn off the device
    case SIG_PANIC:
    case FIRE:
    case SMOKE:
    case WATER:
    case ALARM1:
    case ALARM2:
    case ALARM3:
    case ALARM4:
    case GAS:
    // off scenes as well
    case STANDBY:
    case DEEP_OFF:
    case T0_S0:
      value = 0;
      setDontCare(false);
      command.clear();
      break;
    case SLEEPING:
    case ABSENT:
      // no operation for these by default
      value = 0; // if action, then it would be turning off
      setDontCare(true); // but no action by default
      command.clear();
      break;
    default:
      // no operation by default for all other scenes
      setDontCare(true);
      break;
  }
  markClean(); // default values are always clean
}




#pragma mark - VZugHomeDevice

#define POLL_INTERVAL (10*Second)
#define ERROR_RETRY_INTERVAL (30*Second)


#define DEVICE_COLOR group_magenta_video

VZugHomeDevice::VZugHomeDevice(VZugHomeDeviceContainer *aClassContainerP, const string aBaseURL) :
  inherited(aClassContainerP),
  deviceModel(model_unknown),
  mostRecentPush(0),
  programTemp(0),
  currentIsActive(false)
{
  vzugHomeComm.baseURL = aBaseURL;
  setPrimaryGroup(DEVICE_COLOR);
  installSettings(DeviceSettingsPtr(new VZugHomeDeviceSettings(*this)));
  // - set the output behaviour
  OutputBehaviourPtr o = OutputBehaviourPtr(new OutputBehaviour(*this));
  o->setHardwareOutputConfig(outputFunction_positional, outputmode_gradual, usage_undefined, false, -1);
  o->setGroupMembership(DEVICE_COLOR, true);
  o->setGroupMembership(group_magenta_video, true);
  DialChannelPtr dc = DialChannelPtr(new DialChannel(*o));
  dc->setMax(230); // TODO: parametrize this - for now, the max 230 degrees
  o->addChannel(dc);
  addBehaviour(o);
  // - set the pseudo-button behaviour needed for direct scene calls
  actionButton = ButtonBehaviourPtr(new ButtonBehaviour(*this));
  actionButton->setHardwareButtonConfig(0, buttonType_undefined, buttonElement_center, false, 0, true);
  actionButton->setHardwareName("status");
  addBehaviour(actionButton);
  #if STATUS_BINARY_INPUTS
  // - add some "message" binary inputs
  for (int i=0; i<NUM_MESSAGE_BINARY_INPUTS; i++) {
    BinaryInputBehaviourPtr bi = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*this));
    bi->setHardwareInputConfig((DsBinaryInputType)(i+25), usage_undefined, true, POLL_INTERVAL);
    bi->setHardwareName(string_format("event %d", i+25));
    addBehaviour(bi);
  }
  #endif
}


void VZugHomeDevice::queryDeviceInfos(StatusCB aCompletedCB)
{
  // query model ID
  vzugHomeComm.apiCommand(false, "getModel", NULL, false, boost::bind(&VZugHomeDevice::gotModelId, this, aCompletedCB, _1, _2));
}


void VZugHomeDevice::gotModelId(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aResult) {
      modelId = aResult->stringValue();
      if (modelId=="CSTMSLQ" || modelId=="MSLQ") {
        deviceModel = model_MSLQ;
      }
      vzugHomeComm.apiCommand(false,"getModelDescription", NULL, false, boost::bind(&VZugHomeDevice::gotModelDescription, this, aCompletedCB, _1, _2));
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
      vzugHomeComm.apiCommand(false, "getSerialNumber", NULL, false, boost::bind(&VZugHomeDevice::gotSerialNumber, this, aCompletedCB, _1, _2));
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
      vzugHomeComm.apiCommand(false, "getDeviceName", NULL, false, boost::bind(&VZugHomeDevice::gotDeviceName, this, aCompletedCB, _1, _2));
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
    ovenTemp = SensorBehaviourPtr(new SensorBehaviour(*this));
    ovenTemp->setHardwareSensorConfig(sensorType_temperature, usage_undefined, 0, 500, 1, POLL_INTERVAL, Never, 10*Minute);
    ovenTemp->setHardwareName("Garraumtemperatur");
    addBehaviour(ovenTemp);
    foodTemp = SensorBehaviourPtr(new SensorBehaviour(*this));
    foodTemp->setHardwareSensorConfig(sensorType_temperature, usage_undefined, 0, 500, 1, POLL_INTERVAL, Never, 10*Minute);
    foodTemp->setHardwareName("Garsensortemperatur");
    addBehaviour(foodTemp);
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
  vzugHomeComm.apiCommand(false, "getCurrentStatus", NULL, false, boost::bind(&VZugHomeDevice::gotCurrentStatus, this, _1, _2));
}



void VZugHomeDevice::gotCurrentStatus(JsonObjectPtr aResult, ErrorPtr aError)
{
  if (!Error::isOK(aError) || !aResult) {
    scheduleNextStatePoll(aError);
    return;
  }
  // Status:
  string s = aResult->stringValue();
  currentStatus = s;
  if (deviceModel==model_MSLQ) {
    // for MSLQ something like
    // (sensor) xx°, (garraum) xx°\n(unknown) 0(unknown unit)

    // EE 83 B1 = degree sign
    // EE 84 B2 = unknown unit of 3rd value

    // EE 85 81 = Garraum
    // EE 84 86 = Garsensor

    // EE 83 B5 = 3rd value, not visible on display so far

    size_t i = string::npos;
    ALOG(LOG_DEBUG, "Status: %s", s.c_str());
    // search for UTF-8 char designating Garraum
    i = s.find("\xEE\x85\x81");
    if (i!=string::npos) {
      // find start of temperature value
      i = s.find_first_of("0123456789",i);
      int temp;
      if (sscanf(s.c_str()+i, "%d", &temp)==1) {
        // update Garraumtemperatur
        ovenTemp->updateSensorValue(temp);
      }
    }
    else {
      ovenTemp->invalidateSensorValue();
    }
    // search for UTF-8 char designating Gargutsensor
    i = s.find("\xEE\x84\x86");
    if (i!=string::npos) {
      // find start of temperature value
      i = s.find_first_of("0123456789",i);
      int temp;
      if (sscanf(s.c_str()+i, "%d", &temp)==1) {
        // update Garsensortemperatur
        foodTemp->updateSensorValue(temp);
      }
    }
    else {
      foodTemp->invalidateSensorValue();
    }
  }
  // query current program
  vzugHomeComm.apiCommand(false, "getCurrentProgram", NULL, false, boost::bind(&VZugHomeDevice::gotCurrentProgram, this, _1, _2));
}


void VZugHomeDevice::gotCurrentProgram(JsonObjectPtr aResult, ErrorPtr aError)
{
  if (!Error::isOK(aError) || !aResult) {
    scheduleNextStatePoll(aError);
    return;
  }
  // Status:
  string s = aResult->stringValue();
  currentProgram = s;
  // Extract program setting
  // \xEE\x85\x81 = temperature symbol
  // Program:  Dämpfen,  30
  size_t i = s.find("\xEE\x85\x81");
  if (i!=string::npos) {
    // find start of temperature value
    i = s.find_first_of("0123456789",i);
    sscanf(s.c_str()+i, "%d", &programTemp);
  }
  ALOG(LOG_DEBUG, "Program: %s", s.c_str());
  // query current program end
  vzugHomeComm.apiCommand(false, "getCurrentProgramEnd", NULL, true, boost::bind(&VZugHomeDevice::gotCurrentProgramEnd, this, _1, _2));
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
  vzugHomeComm.apiCommand(false, "isActive", NULL, false, boost::bind(&VZugHomeDevice::gotIsActive, this, _1, _2));
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
  bool isActive = false;
  if (s=="true") isActive = true;
  if (currentIsActive!=isActive) {
    currentIsActive = isActive;
    output->getChannelByType(channeltype_default)->syncChannelValue(isActive ? (programTemp>0 ? programTemp : 1) : 0); // update channel value
    processPushMessage(isActive ? "GOT_ACTIVE" : "GOT_INACTIVE");
  }
  // query last push messages
  vzugHomeComm.apiCommand(true, "getLastPUSHNotifications", NULL, true, boost::bind(&VZugHomeDevice::gotLastPUSHNotifications, this, _1, _2));
}



void VZugHomeDevice::gotLastPUSHNotifications(JsonObjectPtr aResult, ErrorPtr aError)
{
  if (!Error::isOK(aError) || !aResult) {
    scheduleNextStatePoll(aError);
    return;
  }
  // Figure out if there are new push notifications
  ALOG(LOG_DEBUG, "Last Push notifications: %s", aResult->json_c_str());
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


ErrorPtr VZugHomeDevice::load()
{
  // first do normal load, including loadings scenes and device settings from CSV files
  ErrorPtr err = inherited::load();
  // load triggers
  pushMessageTriggers.clear();
  string dir = getDeviceContainer().getPersistentDataDir();
  const int numLevels = 3;
  string levelids[numLevels];
  // Level strategy: most specialized will be active, unless lower levels specify explicit override
  // - Level 0 are triggers related to the device instance (dSUID)
  // - Level 1 are triggers related to the device type (deviceTypeIdentifier())
  levelids[0] = getDsUid().getString();
  levelids[1] = string(deviceTypeIdentifier());
  for(int i=0; i<numLevels; ++i) {
    // try to open config file
    string fn = dir+"actiontriggers_"+levelids[i]+".csv";
    string line;
    FILE *file = fopen(fn.c_str(), "r");
    if (!file) {
      int syserr = errno;
      if (syserr!=ENOENT) {
        // file not existing is ok, all other errors must be reported
        LOG(LOG_ERR, "failed opening file %s - %s", fn.c_str(), strerror(syserr));
      }
      // NOP
    }
    else {
      // file opened
      while (string_fgetline(file, line)) {
        const char *p = line.c_str();
        string text,action;
        while (nextCSVField(p, text)) {
          if (!nextCSVField(p, action)) break;
          pushMessageTriggers[text] = action;
        }
      }
      fclose(file);
      ALOG(LOG_INFO, "Loaded direct action triggers from file %s", fn.c_str());
    }

  }
  return err;
}



void VZugHomeDevice::processPushMessage(const string aMessage)
{
  lastPushMessage = aMessage;
  ALOG(LOG_NOTICE, "Push Message: %s", aMessage.c_str());
  // look up
  for (StringStringMap::iterator pos = pushMessageTriggers.begin(); pos!=pushMessageTriggers.end(); ++pos) {
    if (aMessage.find(pos->first)!=string::npos) {
      // matching trigger -> execute direct call
      ALOG(LOG_NOTICE, "Trigger text '%s' detected -> executing signalling commands: %s", pos->first.c_str(), pos->second.c_str());
      // parse call: syntax:
      //   <action>[,<action>...]
      //   action = <directscenecall>|<inputchange>
      //   directscenecall = [-|!]n, n=scene number, "!" for force call, "-" for undo
      //   inputchange = >[-|+]n, n=input index, "+" to set input, "-" to clear input, no "+"/"-" to pulse input
      const char *p = pos->second.c_str();
      while (*p) {
        #if STATUS_BINARY_INPUTS
        if (*p=='>') {
          // input change
          bool on = false;
          bool off = false;
          p++;
          if (*p=='+') {
            on = true;
            p++;
          }
          else if (*p=='-') {
            off = true;
            p++;
          }
          int i;
          if (sscanf(p, "%d", &i)==1 && i>=0 && i<binaryInputs.size()) {
            BinaryInputBehaviourPtr bi = boost::dynamic_pointer_cast<BinaryInputBehaviour>(binaryInputs[i]);
            if (bi) {
              if (on) bi->updateInputState(true);
              else if (off) bi->updateInputState(false);
              else {
                // pulse 1 second
                bi->updateInputState(true);
                MainLoop::currentMainLoop().executeOnce(boost::bind(&BinaryInputBehaviour::updateInputState, bi, false), 1*Second);
              }
            }
          }
        }
        else
        #endif
        {
          // direct scene call
          DsButtonActionMode m = buttonActionMode_normal;
          if (*p=='!') { p++; m = buttonActionMode_force; }
          else if (*p=='-') { p++; m = buttonActionMode_undo; }
          int a;
          if (sscanf(p, "%d", &a)==1) {
            actionButton->sendAction(m, a);
          }
        }
        // skip number, check for more commands
        while (*p>='0' && *p<='9') p++;
        if (*p!=',') break;
        p++;
      }
    }
  }
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
  inherited::disconnect(aForgetParams, aDisconnectResultHandler);
}



bool VZugHomeDevice::prepareSceneCall(DsScenePtr aScene)
{
  SimpleCmdScenePtr cs = boost::dynamic_pointer_cast<SimpleCmdScene>(aScene);
  bool continueApply = true;
  if (cs) {
    // execute custom scene commands
    if (!cs->command.empty()) {
      string subcmd, params;
      if (keyAndValue(cs->command, subcmd, params, ':')) {
        if (subcmd=="vzughome") {
          // Syntax: vzughome:hh|ai:command[:value]
          // direct execution of vzughome commands
          // "value" will be scanned for @{sceneno} and @{channel...} placeholders
          string dest;
          string vzcmd;
          if (keyAndValue(params, dest, vzcmd)) {
            string cmd;
            const char *p = vzcmd.c_str();
            if (nextPart(p, cmd, ':')) {
              string vsubst;
              if (*p) {
                // rest is value, substitute
                vsubst = p;
                cs->substitutePlaceholders(vsubst);
              }
              // issue
              vzugHomeComm.apiCommand(dest=="ai", cmd.c_str(), vsubst.empty() ? NULL : vsubst.c_str(), false, boost::bind(&VZugHomeDevice::sceneCmdSent, this, _1, _2));
            }
          }
        }
        else {
          ALOG(LOG_ERR, "Unknown scene command: %s", cs->command.c_str());
        }
      }
    }
  }
  // prepared ok
  return continueApply;
}


void VZugHomeDevice::sceneCmdSent(JsonObjectPtr aResult, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    string s = aResult->stringValue();
    ALOG(LOG_INFO, "VZug device reply for scene command: %s", s.c_str());
  }
  else {
    ALOG(LOG_ERR, "Error sending command to VZug device: %s", aError->description().c_str());
  }
}




void VZugHomeDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  ChannelBehaviourPtr ch = getChannelByType(channeltype_default);
  if (ch && ch->needsApplying()) {
    // we can turn off the device
    if (ch->getChannelValue()==0) {
      // send off command if not already inactive
      if (currentIsActive) {
        ALOG(LOG_INFO, "Main channel set to 0 -> send off command");
        vzugHomeComm.apiCommand(false, "doTurnOff", NULL, false, boost::bind(&VZugHomeDevice::sentTurnOff, this, aDoneCB, aForDimming, _2));
        ch->channelValueApplied(); // value is "applied" (saved in request)
        return; // wait for actual apply
      }
    }
    else {
      // setting the channel to another value than 0 is NOP, or just for saving it into a scene that will use it together with a program
    }
  }
  // let inherited handle and callback right now
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void VZugHomeDevice::sentTurnOff(SimpleCB aDoneCB, bool aForDimming, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    ALOG(LOG_INFO, "Successfully sent off command");
  }
  else {
    ALOG(LOG_INFO, "Error turning off device: %s", aError->description().c_str());
  }
  // applied, let inherited handle and callback
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


string VZugHomeDevice::hardwareModelGUID()
{
  return "vzughomemodel:" + modelId;
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
  s = string_format("Last Push: %s | Program: %s | Status: %s", lastPushMessage.c_str(), currentProgram.c_str(), currentStatus.c_str());
  return s;
}



string VZugHomeDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "\n- V-Zug Home device");
  return s;
}


#endif // ENABLE_VZUGHOME



