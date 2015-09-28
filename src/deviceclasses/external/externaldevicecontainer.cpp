//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "externaldevicecontainer.hpp"

#include "movinglightbehaviour.hpp"
#include "shadowbehaviour.hpp"
#include "climatecontrolbehaviour.hpp"
#include "binaryinputbehaviour.hpp"
#include "sensorbehaviour.hpp"


using namespace p44;



#pragma mark - External Device


ExternalDevice::ExternalDevice(DeviceClassContainer *aClassContainerP, JsonCommPtr aDeviceConnection) :
  Device(aClassContainerP),
  deviceConnection(aDeviceConnection),
  simpletext(false), // default to JSON
  useMovement(false), // no movement by default
  querySync(false), // no sync query by default
  configured(false)
{
  // relate the device to the connection, so it keeps living until connection closes
  deviceConnection->relatedObject = this;
  // install handlers on device connection
  deviceConnection->setConnectionStatusHandler(boost::bind(&ExternalDevice::handleDeviceConnectionStatus, this, _2));
  deviceConnection->setMessageHandler(boost::bind(&ExternalDevice::handleDeviceApiJsonMessage, this, _1, _2));
  deviceConnection->setClearHandlersAtClose(); // close must break retain cycles so this object won't cause a mem leak
}


ExternalDevice::~ExternalDevice()
{
  LOG(LOG_DEBUG,"external device %s -> destructed\n", shortDesc().c_str());
}



bool ExternalDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getGroupColoredIcon("ext", getDominantGroup(), aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}



ExternalDeviceContainer &ExternalDevice::getExternalDeviceContainer()
{
  return *(static_cast<ExternalDeviceContainer *>(classContainerP));
}



void ExternalDevice::handleDeviceConnectionStatus(ErrorPtr aError)
{
  if (!Error::isOK(aError)) {
    closeConnection();
    LOG(LOG_NOTICE,"external device %s: connection closed (%s) -> disconnecting\n", shortDesc().c_str(), aError->description().c_str());
    // device has vanished for now, but will keep parameters in case it reconnects later
    hasVanished(false);
  }
}


void ExternalDevice::closeConnection()
{
  // prevent further connection status callbacks
  deviceConnection->setConnectionStatusHandler(NULL);
  // error, close connection
  deviceConnection->closeConnection();
  // release the connection
  // Note: this should cause the connection to get deleted, which in turn also releases the relatedObject,
  //   so the device is only kept by the container (or not at all if it has not yet registered)
  deviceConnection.reset();
}




void ExternalDevice::handleDeviceApiJsonMessage(ErrorPtr aError, JsonObjectPtr aMessage)
{
  // device API request
  if (Error::isOK(aError)) {
    // not JSON level error, try to process
    LOG(LOG_INFO,"device -> externalDeviceContainer (JSON) message received: %s\n", aMessage->c_strValue());
    // extract message type
    JsonObjectPtr o = aMessage->get("message");
    if (o) {
      aError = processJsonMessage(o->stringValue(), aMessage);
    }
    else {
      sendDeviceApiStatusMessage(TextError::err("missing 'message' field"));
    }
  }
  // if error or explicit OK, send response now. Otherwise, request processing will create and send the response
  if (aError) {
    sendDeviceApiStatusMessage(aError);
  }
  if (!configured) {
    // disconnect if we are not configured now
    deviceConnection->closeAfterSend(); // close connection afterwards
  }
}


void ExternalDevice::handleDeviceApiSimpleMessage(ErrorPtr aError, string aMessage)
{
  // device API request
  if (Error::isOK(aError)) {
    // not JSON level error, try to process
    aMessage = trimWhiteSpace(aMessage);
    LOG(LOG_INFO,"device -> externalDeviceContainer (simple) message received: %s\n", aMessage.c_str());
    // extract message type
    string msg;
    string val;
    if (keyAndValue(aMessage, msg, val, '=')) {
      aError = processSimpleMessage(msg,val);
    }
    else {
      aError = processSimpleMessage(aMessage,"");
    }
  }
  // if error or explicit OK, send response now. Otherwise, request processing will create and send the response
  if (aError) {
    sendDeviceApiStatusMessage(aError);
  }
  if (!configured) {
    // disconnect if we are not configured now
    deviceConnection->closeAfterSend(); // close connection afterwards
  }
}


void ExternalDevice::sendDeviceApiJsonMessage(JsonObjectPtr aMessage)
{
  LOG(LOG_INFO,"device <- externalDeviceContainer (JSON) message sent: %s\n", aMessage->c_strValue());
  deviceConnection->sendMessage(aMessage);
}


void ExternalDevice::sendDeviceApiSimpleMessage(string aMessage)
{
  LOG(LOG_INFO,"device <- externalDeviceContainer (simple) message sent: %s\n", aMessage.c_str());
  aMessage += "\n";
  deviceConnection->sendRaw(aMessage);
}



void ExternalDevice::sendDeviceApiStatusMessage(ErrorPtr aError)
{
  if (simpletext) {
    // simple text message
    string msg;
    if (Error::isOK(aError))
      msg = "OK";
    else
      msg = string_format("ERROR=%s",aError->getErrorMessage());
    // send it
    sendDeviceApiSimpleMessage(msg);
  }
  else {
    // create JSON response
    JsonObjectPtr message = JsonObject::newObj();
    message->add("message", JsonObject::newString("status"));
    if (!Error::isOK(aError)) {
      LOG(LOG_INFO,"device API error: %s\n", aError->description().c_str());
      // error, return error response
      message->add("status", JsonObject::newString("error"));
      message->add("errorcode", JsonObject::newInt32((int32_t)aError->getErrorCode()));
      message->add("errormessage", JsonObject::newString(aError->getErrorMessage()));
      message->add("errordomain", JsonObject::newString(aError->getErrorDomain()));
    }
    else {
      // no error, return result (if any)
      message->add("status", JsonObject::newString("ok"));
    }
    // send it
    sendDeviceApiJsonMessage(message);
  }
}



ErrorPtr ExternalDevice::processJsonMessage(string aMessageType, JsonObjectPtr aMessage)
{
  ErrorPtr err;
  if (aMessageType=="bye") {
    configured = false; // cause disconnect
    err = Error::ok(); // explicit ok
  }
  else if (aMessageType=="init") {
    // create new device
    err = configureDevice(aMessage);
  }
  else {
    if (configured) {
      if (aMessageType=="synced") {
        // device confirms having reported all channel states (in response to "sync" command)
        if (syncedCB) syncedCB();
        syncedCB = NULL;
        return ErrorPtr(); // no answer
      }
      else if (aMessageType=="button") {
        err = processInputJson('B', aMessage);
      }
      else if (aMessageType=="input") {
        err = processInputJson('I', aMessage);
      }
      else if (aMessageType=="sensor") {
        err = processInputJson('S', aMessage);
      }
      else if (aMessageType=="channel") {
        err = processInputJson('C', aMessage);
      }
      else {
        err = TextError::err("Unknown message '%s'", aMessageType.c_str());
      }
    }
    else {
      err = TextError::err("Device must be sent 'init' message first", aMessageType.c_str());
    }
  }
  return err;
}


ErrorPtr ExternalDevice::processSimpleMessage(string aMessageType, string aValue)
{
  if (aMessageType=="BYE") {
    configured = false; // cause disconnect
    return Error::ok(); // explicit ok
  }
  else if (aMessageType=="SYNCED") {
    // device confirms having reported all channel states (in response to "SYNC" command)
    if (syncedCB) syncedCB();
    syncedCB = NULL;
    return ErrorPtr(); // no answer
  }
  else if (aMessageType.size()>0) {
    // none of the other commands, try inputs
    char iotype = aMessageType[0];
    int index = 0;
    if (sscanf(aMessageType.c_str()+1, "%d", &index)==1) {
      double value = 0;
      sscanf(aValue.c_str(), "%lf", &value);
      return processInput(iotype, index, value);
    }
  }
  return TextError::err("Unknown message '%s'", aMessageType.c_str());
}



ErrorPtr ExternalDevice::processInputJson(char aInputType, JsonObjectPtr aParams)
{
  uint32_t index = 0;
  JsonObjectPtr o = aParams->get("index");
  if (o) index = o->int32Value();
  o = aParams->get("value");
  if (o) {
    double value = o->doubleValue();
    return processInput(aInputType, index, value);
  }
  else {
    return TextError::err("missing value");
  }
  return ErrorPtr();
}


#pragma mark - process input

ErrorPtr ExternalDevice::processInput(char aInputType, uint32_t aIndex, double aValue)
{
  switch (aInputType) {
    case 'B': {
      if (aIndex<buttons.size()) {
        ButtonBehaviourPtr bb = boost::dynamic_pointer_cast<ButtonBehaviour>(buttons[aIndex]);
        if (bb) {
          if (aValue>2) {
            // simulate a keypress of defined length in milliseconds
            bb->buttonAction(true);
            MainLoop::currentMainLoop().executeOnce(boost::bind(&ExternalDevice::releaseButton, this, bb), aValue*MilliSecond);
          }
          else {
            bb->buttonAction(aValue!=0);
          }
        }
      }
      break;
    }
    case 'I': {
      if (aIndex<binaryInputs.size()) {
        BinaryInputBehaviourPtr ib = boost::dynamic_pointer_cast<BinaryInputBehaviour>(binaryInputs[aIndex]);
        if (ib) {
          ib->updateInputState(aValue!=0);
        }
      }
      break;
    }
    case 'S': {
      if (aIndex<sensors.size()) {
        SensorBehaviourPtr sb = boost::dynamic_pointer_cast<SensorBehaviour>(sensors[aIndex]);
        if (sb) {
          sb->updateSensorValue(aValue);
        }
      }
      break;
    }
    case 'C': {
      ChannelBehaviourPtr cb = getChannelByIndex(aIndex);
      if (cb) {
        cb->syncChannelValue(aValue, true);
      }
      break;
    }
    default:
      break;
  }
  return ErrorPtr(); // no feedback for input processing
}


void ExternalDevice::releaseButton(ButtonBehaviourPtr aButtonBehaviour)
{
  aButtonBehaviour->buttonAction(false);
}



#pragma mark - output control


void ExternalDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  // special behaviour for shadow behaviour
  ShadowBehaviourPtr sb = boost::dynamic_pointer_cast<ShadowBehaviour>(output);
  if (sb && useMovement) {
    // ask shadow behaviour to start movement sequence on default channel
    sb->applyBlindChannels(boost::bind(&ExternalDevice::changeChannelMovement, this, 0, _1, _2), aDoneCB, aForDimming);
  }
  else {
    // generic channel apply
    for (size_t i=0; i<numChannels(); i++) {
      ChannelBehaviourPtr cb = getChannelByIndex(i);
      if (cb->needsApplying()) {
        // send channel value message
        if (simpletext) {
          string m = string_format("C%d=%lf", i, cb->getChannelValue());
          sendDeviceApiSimpleMessage(m);
        }
        else {
          JsonObjectPtr message = JsonObject::newObj();
          message->add("message", JsonObject::newString("channel"));
          message->add("index", JsonObject::newInt32((int)i));
          message->add("type", JsonObject::newInt32(cb->getChannelType())); // informational
          message->add("value", JsonObject::newDouble(cb->getChannelValue()));
          sendDeviceApiJsonMessage(message);
        }
        cb->channelValueApplied();
      }
    }
  }
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void ExternalDevice::dimChannel(DsChannelType aChannelType, DsDimMode aDimMode)
{
  // start dimming
  ShadowBehaviourPtr sb = boost::dynamic_pointer_cast<ShadowBehaviour>(output);
  if (sb && useMovement) {
    // no channel check, there's only global dimming of the blind, no separate position/angle
    sb->dimBlind(boost::bind(&ExternalDevice::changeChannelMovement, this, 0, _1, _2), aDimMode);
  }
  else if (useMovement) {
    // not shadow, but still use movement for dimming
    ChannelBehaviourPtr cb = getChannelByType(aChannelType);
    if (cb) {
      changeChannelMovement(cb->getChannelIndex(), NULL, aDimMode);
    }
  }
  else {
    inherited::dimChannel(aChannelType, aDimMode);
  }
}


void ExternalDevice::changeChannelMovement(size_t aChannelIndex, SimpleCB aDoneCB, int aNewDirection)
{
  if (simpletext) {
    string m = string_format("MV%d=%d", aChannelIndex, aNewDirection);
    sendDeviceApiSimpleMessage(m);
  }
  else {
    JsonObjectPtr message = JsonObject::newObj();
    message->add("message", JsonObject::newString("move"));
    message->add("index", JsonObject::newInt32((int)aChannelIndex));
    message->add("direction", JsonObject::newInt32(aNewDirection));
    sendDeviceApiJsonMessage(message);
  }
  if (aDoneCB) aDoneCB();
}


void ExternalDevice::syncChannelValues(SimpleCB aDoneCB)
{
  if (querySync) {
    // save callback, to be called when "synced" message confirms sync done
    syncedCB = aDoneCB;
    // send sync command
    if (simpletext) {
      sendDeviceApiSimpleMessage("SYNC");
    }
    else {
      JsonObjectPtr message = JsonObject::newObj();
      message->add("message", JsonObject::newString("sync"));
      sendDeviceApiJsonMessage(message);
    }
  }
  else {
    inherited::syncChannelValues(aDoneCB);
  }
}



#pragma mark - external device configuration


ErrorPtr ExternalDevice::configureDevice(JsonObjectPtr aInitParams)
{
  JsonObjectPtr o;
  // get protocol type for further communication
  if (aInitParams->get("protocol", o)) {
    string p = o->stringValue();
    if (p=="json")
      simpletext = false;
    else if (p=="simple")
      simpletext = true;
    else
      return TextError::err("unknown protocol '%s'", p.c_str());
  }
  // options
  if (aInitParams->get("sync", o)) querySync = o->boolValue();
  if (aInitParams->get("move", o)) useMovement = o->boolValue();
  // get unique ID
  if (!aInitParams->get("uniqueid", o)) {
    return TextError::err("missing 'uniqueid'");
  }
  // - try it natively (can be a dSUID or a UUID)
  if (!dSUID.setAsString(o->stringValue())) {
    // not suitable dSUID or UUID syntax, create hashed dSUID
    DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
    //   UUIDv5 with name = classcontainerinstanceid::uniqueid
    string s = classContainerP->deviceClassContainerInstanceIdentifier();
    s += ':';
    s += o->stringValue();
    dSUID.setNameInSpace(s, vdcNamespace);
  }
  // Output
  // - get group (overridden for some output types)
  primaryGroup = group_variable; // none set so far
  if (aInitParams->get("group", o)) {
    primaryGroup = (DsGroup)o->int32Value(); // custom primary group
  }
  // - get output type
  string outputType;
  if (aInitParams->get("output", o)) {
    outputType = o->stringValue();
  }
  // - get hardwarename
  string hardwareName = outputType; // default to output type
  if (aInitParams->get("hardwarename", o)) {
    hardwareName = o->stringValue();
  }
  // - basic output behaviour
  DsOutputFunction outputFunction = outputFunction_dimmer; // dimmable by default
  if (aInitParams->get("dimmable", o)) {
    if (!o->boolValue()) outputFunction = outputFunction_switch;
  }
  // - create appropriate output behaviour
  if (outputType=="light") {
    if (primaryGroup==group_variable) primaryGroup = group_yellow_light;
    // - use light settings, which include a scene table
    installSettings(DeviceSettingsPtr(new LightDeviceSettings(*this)));
    // - add simple single-channel light behaviour
    LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
    l->setHardwareOutputConfig(outputFunction, usage_undefined, false, -1);
    l->setHardwareName(hardwareName);
    addBehaviour(l);
  }
  else if (outputType=="colorlight") {
    if (primaryGroup==group_variable) primaryGroup = group_yellow_light;
    // - use color light settings, which include a color scene table
    installSettings(DeviceSettingsPtr(new ColorLightDeviceSettings(*this)));
    // - add multi-channel color light behaviour (which adds a number of auxiliary channels)
    RGBColorLightBehaviourPtr l = RGBColorLightBehaviourPtr(new RGBColorLightBehaviour(*this));
    l->setHardwareName(hardwareName);
    addBehaviour(l);
  }
  else if (outputType=="movinglight") {
    if (primaryGroup==group_variable) primaryGroup = group_yellow_light;
    // - use moving light settings, which include a color+position scene table
    installSettings(DeviceSettingsPtr(new MovingLightDeviceSettings(*this)));
    // - add moving color light behaviour
    MovingLightBehaviourPtr ml = MovingLightBehaviourPtr(new MovingLightBehaviour(*this));
    ml->setHardwareName(hardwareName);
    addBehaviour(ml);
  }
  else if (outputType=="heatingvalve") {
    if (primaryGroup==group_variable) primaryGroup = group_blue_heating;
    // - standard device settings with scene table
    installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*this)));
    // - create climate control outout
    OutputBehaviourPtr cb = OutputBehaviourPtr(new ClimateControlBehaviour(*this));
    cb->setGroupMembership(group_roomtemperature_control, true); // put into room temperature control group by default, NOT into standard blue)
    cb->setHardwareOutputConfig(outputFunction_positional, usage_room, false, 0);
    cb->setHardwareName(hardwareName);
    addBehaviour(cb);
  }
  else if (outputType=="shadow") {
    if (primaryGroup==group_variable) primaryGroup = group_grey_shadow;
    // - use shadow scene settings
    installSettings(DeviceSettingsPtr(new ShadowDeviceSettings(*this)));
    // - add shadow behaviour
    ShadowBehaviourPtr sb = ShadowBehaviourPtr(new ShadowBehaviour(*this));
    sb->setHardwareOutputConfig(outputFunction_positional, usage_undefined, false, -1);
    sb->setHardwareName(hardwareName);
    sb->setDeviceParams(shadowdevice_jalousie, 0, 0, 0); // no restrictions for move times so far
    sb->position->syncChannelValue(100); // assume fully up at beginning
    sb->angle->syncChannelValue(100); // assume fully open at beginning
    addBehaviour(sb);
  }
  else if (outputType=="basic") {
    if (primaryGroup==group_variable) primaryGroup = group_black_joker;
    // - use simple scene settings
    installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*this)));
    // - add generic output behaviour
    OutputBehaviourPtr o = OutputBehaviourPtr(new OutputBehaviour(*this));
    o->setHardwareOutputConfig(outputFunction_switch, usage_undefined, false, -1);
    o->setHardwareName(hardwareName);
    o->setGroupMembership(primaryGroup, true); // put into primary group
    o->addChannel(ChannelBehaviourPtr(new DigitalChannel(*o)));
    addBehaviour(o);
  }
  else {
    // no output, just install minimal settings without scenes
    installSettings();
  }
  // set primary group to black if group is not yet defined so far
  if (primaryGroup==group_variable) primaryGroup = group_black_joker;
  // check for groups definition, will override anything set so far
  if (aInitParams->get("groups", o) && output) {
    output->resetGroupMembership(); // clear all
    for (int i=0; i<o->arrayLength(); i++) {
      JsonObjectPtr o2 = o->arrayGet(i);
      DsGroup g = (DsGroup)o2->int32Value();
      output->setGroupMembership(g, true);
    }
  }
  // check for buttons
  if (aInitParams->get("buttons", o)) {
    for (int i=0; i<o->arrayLength(); i++) {
      JsonObjectPtr o2 = o->arrayGet(i);
      JsonObjectPtr o3;
      // set defaults
      int buttonId = 0;
      DsButtonType buttonType = buttonType_single;
      DsButtonElement buttonElement = buttonElement_center;
      DsGroup group = primaryGroup; // default group is same as primary
      string buttonName;
      // - optional params
      if (o2->get("id", o3)) buttonId = o3->int32Value();
      if (o2->get("buttontype", o3)) buttonType = (DsButtonType)o3->int32Value();
      if (o2->get("element", o3)) buttonElement = (DsButtonElement)o3->int32Value();
      if (o2->get("group", o3)) group = (DsGroup)o3->int32Value();
      if (o2->get("hardwarename", o3)) buttonName = o3->stringValue(); else buttonName = string_format("button_id%d_el%d", buttonId, buttonElement);
      // - create behaviour
      ButtonBehaviourPtr bb = ButtonBehaviourPtr(new ButtonBehaviour(*this));
      bb->setHardwareButtonConfig(buttonId, buttonType, buttonElement, false, buttonElement_down ? 1 : 0, true); // fixed mode
      bb->setGroup(group);
      bb->setHardwareName(buttonName);
      addBehaviour(bb);
    }
  }
  // check for binary inputs
  if (aInitParams->get("inputs", o)) {
    for (int i=0; i<o->arrayLength(); i++) {
      JsonObjectPtr o2 = o->arrayGet(i);
      JsonObjectPtr o3;
      // set defaults
      DsBinaryInputType inputType = binInpType_none;
      DsUsageHint usage = usage_undefined;
      DsGroup group = primaryGroup; // default group is same as primary
      MLMicroSeconds updateInterval = Never; // unknown
      string inputName;
      // - optional params
      if (o2->get("inputtype", o3)) inputType = (DsBinaryInputType)o3->int32Value();
      if (o2->get("usage", o3)) usage = (DsUsageHint)o3->int32Value();
      if (o2->get("group", o3)) group = (DsGroup)o3->int32Value();
      if (o2->get("updateinterval", o3)) updateInterval = o3->doubleValue()*Second;
      if (o2->get("hardwarename", o3)) inputName = o3->stringValue(); else inputName = string_format("input_ty%d", inputType);
      // - create behaviour
      BinaryInputBehaviourPtr ib = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*this));
      ib->setHardwareInputConfig(inputType, usage, true, updateInterval);
      ib->setGroup(group);
      ib->setHardwareName(inputName);
      addBehaviour(ib);
    }
  }
  // check for sensors
  if (aInitParams->get("sensors", o)) {
    for (int i=0; i<o->arrayLength(); i++) {
      JsonObjectPtr o2 = o->arrayGet(i);
      JsonObjectPtr o3;
      // set defaults
      DsSensorType sensorType = sensorType_none;
      DsUsageHint usage = usage_undefined;
      DsGroup group = primaryGroup; // default group is same as primary
      double min = 0;
      double max = 100;
      double resolution = 1;
      MLMicroSeconds updateInterval = 5*Second; // assume mostly up-to-date
      string sensorName;
      // - optional params
      if (o2->get("sensortype", o3)) sensorType = (DsSensorType)o3->int32Value();
      if (o2->get("usage", o3)) usage = (DsUsageHint)o3->int32Value();
      if (o2->get("group", o3)) group = (DsGroup)o3->int32Value();
      if (o2->get("updateinterval", o3)) updateInterval = o3->doubleValue()*Second;
      if (o2->get("hardwarename", o3)) sensorName = o3->stringValue(); else sensorName = string_format("sensor_ty%d", sensorType);
      if (o2->get("min", o3)) min = o3->doubleValue();
      if (o2->get("max", o3)) max = o3->doubleValue();
      if (o2->get("resolution", o3)) resolution = o3->doubleValue();
      // - create behaviour
      SensorBehaviourPtr sb = SensorBehaviourPtr(new SensorBehaviour(*this));
      sb->setHardwareSensorConfig(sensorType, usage, min, max, resolution, updateInterval, 5*Minute, 5*Minute);
      sb->setGroup(group);
      sb->setHardwareName(sensorName);
      addBehaviour(sb);
    }
  }
  // check for default name
  if (aInitParams->get("name", o)) {
    initializeName(o->stringValue());
  }
  // switch message decoder if we have simpletext
  if (simpletext) {
    deviceConnection->setRawMessageHandler(boost::bind(&ExternalDevice::handleDeviceApiSimpleMessage, this, _1, _2));
  }
  // device configured, add it now
  configured = getExternalDeviceContainer().addDevice(DevicePtr(this));
  // explicit ok
  return Error::ok();
}


#pragma mark - external device container



ExternalDeviceContainer::ExternalDeviceContainer(int aInstanceNumber, const string &aSocketPathOrPort, bool aNonLocal, DeviceContainer *aDeviceContainerP, int aTag) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP, aTag)
{
  // create device API server and set connection specifications
  externalDeviceApiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
  externalDeviceApiServer->setConnectionParams(NULL, aSocketPathOrPort.c_str(), SOCK_STREAM, PF_UNSPEC);
  externalDeviceApiServer->setAllowNonlocalConnections(aNonLocal);
}


void ExternalDeviceContainer::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
  // start device API server
  ErrorPtr err = externalDeviceApiServer->startServer(boost::bind(&ExternalDeviceContainer::deviceApiConnectionHandler, this, _1), 10);
  aCompletedCB(err); // return status of starting server
}


SocketCommPtr ExternalDeviceContainer::deviceApiConnectionHandler(SocketCommPtr aServerSocketCommP)
{
  JsonCommPtr conn = JsonCommPtr(new JsonComm(MainLoop::currentMainLoop()));
  // new connection means new device (which will add itself to container once it has received a proper init message
  ExternalDevicePtr extDev = ExternalDevicePtr(new ExternalDevice(this, conn));
  return conn;
}



// device class name
const char *ExternalDeviceContainer::deviceClassIdentifier() const
{
  return "External_Device_Container";
}


void ExternalDeviceContainer::collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings)
{
  // we have no real collecting process (devices just connect when possibl),
  // but we force all devices to re-connect when a exhaustive collect is requested (mainly for debug purposes)
  if (aExhaustive) {
    // remove all, so they will need to reconnect
    removeDevices(aClearSettings);
  }
  // assume ok
  aCompletedCB(ErrorPtr());
}


void ExternalDeviceContainer::removeDevices(bool aForget)
{
  // cancel all connections
  for (DeviceVector::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
    ExternalDevicePtr dev = boost::dynamic_pointer_cast<ExternalDevice>(*pos);
    // disconnect
    dev->closeConnection();
  }
  // actually remove devices
  inherited::removeDevices(aForget);
}







