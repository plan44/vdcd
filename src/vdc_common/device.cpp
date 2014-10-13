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
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 0


#include "device.hpp"

#include "buttonbehaviour.hpp"
#include "binaryinputbehaviour.hpp"
#include "outputbehaviour.hpp"
#include "sensorbehaviour.hpp"

using namespace p44;


#pragma mark - Device


Device::Device(DeviceClassContainer *aClassContainerP) :
  progMode(false),
  isDimming(false),
  dimHandlerTicket(0),
  dimTimeoutTicket(0),
  currentDimMode(dimmode_stop),
  currentDimChannel(channeltype_default),
  classContainerP(aClassContainerP),
  DsAddressable(&aClassContainerP->getDeviceContainer()),
  primaryGroup(group_black_joker),
  applyInProgress(false),
  missedApplyAttempts(0),
  updateInProgress(false),
  serializerWatchdogTicket(0)
{
}


string Device::modelUID()
{
  // combine basic device type identifier, primary group, behaviours and model features and make UUID based dSUID of it
  DsUid vdcNamespace(DSUID_P44VDC_MODELUID_UUID);
  string s = string_format("%s:%d:", deviceTypeIdentifier(), primaryGroup);
  // behaviours
  for (BehaviourVector::iterator pos = buttons.begin(); pos!=buttons.end(); ++pos) s += (*pos)->behaviourTypeIdentifier();
  for (BehaviourVector::iterator pos = binaryInputs.begin(); pos!=binaryInputs.end(); ++pos) s += (*pos)->behaviourTypeIdentifier();
  for (BehaviourVector::iterator pos = sensors.begin(); pos!=sensors.end(); ++pos) s += (*pos)->behaviourTypeIdentifier();
  if (output) s += output->behaviourTypeIdentifier();
  // model features
  for (int f=0; f<numModelFeatures; f++) {
    s += hasModelFeature((DsModelFeatures)f) ? 'T' : 'F';
  }
  // now make UUIDv5 type dSUID out of it
  DsUid modelUID;
  modelUID.setNameInSpace(s, vdcNamespace);
  return modelUID.getString();
}


Device::~Device()
{
  buttons.clear();
  binaryInputs.clear();
  sensors.clear();
  output.reset();
}



void Device::setName(const string &aName)
{
  if (aName!=getName()) {
    // has changed
    inherited::setName(aName);
    // make sure it will be saved
    if (deviceSettings) {
      deviceSettings->markDirty();
    }
  }
}


void Device::setPrimaryGroup(DsGroup aColorGroup)
{
  primaryGroup = aColorGroup;
}


DsGroup Device::getDominantGroup()
{
  DsGroup group = group_variable;
  if (output) {
    // lowest group of output determines dominant color
    for (int i = group_yellow_light; i<=group_windows; i++) {
      if (output->isMember((DsGroup)i)) {
        group = (DsGroup)i;
        break;
      }
    }
  }
  if (group==group_variable && buttons.size()>0) {
    // second priority: color of first button
    ButtonBehaviourPtr btn = boost::dynamic_pointer_cast<ButtonBehaviour>(buttons[0]);
    group =  btn->buttonGroup;
  }
  if (group==group_variable && sensors.size()>0) {
    // third priority: color of first sensor
    SensorBehaviourPtr sns = boost::dynamic_pointer_cast<SensorBehaviour>(sensors[0]);
    group =  sns->sensorGroup;
  }
  if (group==group_variable && binaryInputs.size()>0) {
    // fourth priority: color of first binary input
    BinaryInputBehaviourPtr bin = boost::dynamic_pointer_cast<BinaryInputBehaviour>(binaryInputs[0]);
    group =  bin->binInputGroup;
  }
  if (group==group_variable) {
    // still undefined -> use primary color
    group = primaryGroup;
  }
  return group;
}


bool Device::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getGroupColoredIcon("vdsd", getDominantGroup(), aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}



void Device::installSettings(DeviceSettingsPtr aDeviceSettings)
{
  if (aDeviceSettings) {
    deviceSettings = aDeviceSettings;
  }
  else {
    // use standard settings
    deviceSettings = DeviceSettingsPtr(new DeviceSettings(*this));
  }
}




void Device::addBehaviour(DsBehaviourPtr aBehaviour)
{
  if (aBehaviour) {
    switch (aBehaviour->getType()) {
      case behaviour_button:
        aBehaviour->index = buttons.size();
        buttons.push_back(aBehaviour);
        break;
      case behaviour_binaryinput:
        aBehaviour->index = binaryInputs.size();
        binaryInputs.push_back(aBehaviour);
        break;
      case behaviour_sensor:
        aBehaviour->index = sensors.size();
        sensors.push_back(aBehaviour);
        break;
      case behaviour_output: {
        aBehaviour->index = 0;
        output = boost::dynamic_pointer_cast<OutputBehaviour>(aBehaviour);
        break;
      }
      default:
        LOG(LOG_ERR,"Device::addBehaviour: unknown behaviour type\n");
    }
  }
  else {
    LOG(LOG_ERR,"Device::addBehaviour: NULL behaviour passed\n");
  }
}

#pragma mark - model features

static const char *modelFeatureNames[numModelFeatures] = {
  "dontcare",
  "blink",
  "ledauto",
  "leddark",
  "transt",
  "outmode",
  "outvalue8",
  "pushbutton",
  "pushbdevice",
  "pushbarea",
  "pushbadvanced",
  "pushbcombined",
  "modelFeature_shadeprops",
  "shadeposition",
  "motiontimefins",
  "optypeconfig",
  "shadebladeang",
  "highlevel",
  "consumption",
  "jokerconfig",
  "akmsensor",
  "akminput",
  "akmdelay",
  "twowayconfig",
  "outputchannels",
  "heatinggroup",
  "heatingoutmode",
  "heatingprops",
  "pwmvalue"
};


bool Device::hasModelFeature(DsModelFeatures aFeatureIndex)
{
  // ask output first, might have more specific info
  if (output) {
    bool hasFeature = output->hasModelFeature(aFeatureIndex);
    if (hasFeature) return true; // output has the feature, no need to check at device level
  }
  // now check for device level features
  switch (aFeatureIndex) {
    case modelFeature_dontcare:
      // Assumption: all devices with scene table have this
      return boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings)!=NULL;
    case modelFeature_blink:
      // Assumption: all devices with an output have this, except heating
      return output && primaryGroup!=group_blue_heating;
    case modelFeature_ledauto:
      // TODO: check this, assuming no virtual device has a LED at all
      return false;
    case modelFeature_leddark:
      // TODO: check this, assuming no virtual device has a LED at all
      return false;
    case modelFeature_outmode:
      // Assuming light, access and alarm output devices have an output mode
      return (output && (primaryGroup==group_yellow_light || primaryGroup==group_green_access || primaryGroup==group_red_security));
    case modelFeature_outvalue8:
      // TODO: this is a tough one, no idea how to derive this. As light is most important, and always has outvalue8 set, return it for now for all outputs
      return output!=NULL;
    case modelFeature_pushbutton:
    case modelFeature_pushbarea:
    case modelFeature_pushbadvanced:
      // Assumption: any device with a buttonInputBehaviour has these props
      return buttons.size()>0;
    case modelFeature_pushbdevice:
      // Assumption: no device has a local button
      return false;
    case modelFeature_pushbcombined:
    case modelFeature_twowayconfig:
      // Assumption: devices with more than single button input are combined up/down (or even 4-way and more) buttons, and need two-way config
      return buttons.size()>1;
    case modelFeature_highlevel:
    case modelFeature_jokerconfig:
      // Assumption: black joker devices can have a high-level (app) functionality and need joker config
      return primaryGroup==group_black_joker;
    default:
      return false; // not known
  }
}


#pragma mark - Channels


int Device::numChannels()
{
  if (output)
    return (int)output->numChannels();
  else
    return 0;
}


ChannelBehaviourPtr Device::getChannelByIndex(size_t aChannelIndex, bool aPendingApplyOnly)
{
  if (!output) return ChannelBehaviourPtr();
  return output->getChannelByIndex(aChannelIndex, aPendingApplyOnly);
}


ChannelBehaviourPtr Device::getChannelByType(DsChannelType aChannelType, bool aPendingApplyOnly)
{
  if (!output) return ChannelBehaviourPtr();
  return output->getChannelByType(aChannelType, aPendingApplyOnly);
}



#pragma mark - Device level vDC API


ErrorPtr Device::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  if (aMethod=="x-p44-removeDevice") {
    if (isSoftwareDisconnectable()) {
      // confirm first, because device will get deleted in the process
      aRequest->sendResult(ApiValuePtr());
      // Remove this device from the installation, forget the settings
      hasVanished(true);
      // now device does not exist any more, so only thing that may happen is return
    }
    else {
      respErr = ErrorPtr(new WebError(403, "device cannot be removed with this method"));
    }
  }
  else {
    respErr = inherited::handleMethod(aRequest, aMethod, aParams);
  }
  return respErr;
}



#define MOC_DIM_STEP_TIMEOUT (5*Second)
#define LEGACY_DIM_STEP_TIMEOUT (400*MilliSecond)


void Device::handleNotification(const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr err;
  if (aMethod=="callScene") {
    // call scene
    ApiValuePtr o;
    if (Error::isOK(err = checkParam(aParams, "scene", o))) {
      SceneNo sceneNo = (SceneNo)o->int32Value();
      bool force = false;
      // check for force flag
      if (Error::isOK(err = checkParam(aParams, "force", o))) {
        force = o->boolValue();
        // now call
        callScene(sceneNo, force);
      }
    }
    if (!Error::isOK(err)) {
      LOG(LOG_WARNING, "callScene error: %s\n", err->description().c_str());
    }
  }
  else if (aMethod=="saveScene") {
    // save scene
    ApiValuePtr o;
    if (Error::isOK(err = checkParam(aParams, "scene", o))) {
      SceneNo sceneNo = (SceneNo)o->int32Value();
      // now save
      saveScene(sceneNo);
    }
    if (!Error::isOK(err)) {
      LOG(LOG_WARNING, "saveScene error: %s\n", err->description().c_str());
    }
  }
  else if (aMethod=="undoScene") {
    // save scene
    ApiValuePtr o;
    if (Error::isOK(err = checkParam(aParams, "scene", o))) {
      SceneNo sceneNo = (SceneNo)o->int32Value();
      // now save
      undoScene(sceneNo);
    }
    if (!Error::isOK(err)) {
      LOG(LOG_WARNING, "undoScene error: %s\n", err->description().c_str());
    }
  }
  else if (aMethod=="setLocalPriority") {
    // set local priority
    ApiValuePtr o;
    if (Error::isOK(err = checkParam(aParams, "scene", o))) {
      SceneNo sceneNo = (SceneNo)o->int32Value();
      // now save
      setLocalPriority(sceneNo);
    }
    if (!Error::isOK(err)) {
      LOG(LOG_WARNING, "setLocalPriority error: %s\n", err->description().c_str());
    }
  }
  else if (aMethod=="setControlValue") {
    // set control value
    ApiValuePtr o;
    if (Error::isOK(err = checkParam(aParams, "name", o))) {
      string controlValueName = o->stringValue();
      if (Error::isOK(err = checkParam(aParams, "value", o))) {
        // get value
        double value = o->doubleValue();
        // now process the value (updates channel values, but does not yet apply them)
        LOG(LOG_NOTICE, "processControlValue(%s, %f) in device %s:\n", controlValueName.c_str(), value, shortDesc().c_str());
        processControlValue(controlValueName, value);
        // apply the values
        requestApplyingChannels(NULL, false);
      }
    }
    if (!Error::isOK(err)) {
      LOG(LOG_WARNING, "setControlValue error: %s\n", err->description().c_str());
    }
  }
  else if (aMethod=="callSceneMin") {
    // switch device on with minimum output level if not already on (=prepare device for dimming from zero)
    ApiValuePtr o;
    if (Error::isOK(err = checkParam(aParams, "scene", o))) {
      SceneNo sceneNo = (SceneNo)o->int32Value();
      // now call
      callSceneMin(sceneNo);
    }
    if (!Error::isOK(err)) {
      LOG(LOG_WARNING, "callSceneMin error: %s\n", err->description().c_str());
    }
  }
  else if (aMethod=="dimChannel") {
    // start or stop dimming a channel
    ApiValuePtr o;
    if (Error::isOK(err = checkParam(aParams, "channel", o))) {
      DsChannelType channel = (DsChannelType)o->int32Value();
      if (Error::isOK(err = checkParam(aParams, "mode", o))) {
        int mode = o->int32Value();
        int area = 0;
        o = aParams->get("area");
        if (o) {
          area = o->int32Value();
        }
        // start/stop dimming
        dimChannelForArea(channel,mode==0 ? dimmode_stop : (mode<0 ? dimmode_down : dimmode_up), area, MOC_DIM_STEP_TIMEOUT);
      }
    }
    if (!Error::isOK(err)) {
      LOG(LOG_WARNING, "callSceneMin error: %s\n", err->description().c_str());
    }
  }
  else if (aMethod=="setOutputChannelValue") {
    // set output channel value (alias for setProperty outputStates)
    ApiValuePtr o;
    if (Error::isOK(err = checkParam(aParams, "channel", o))) {
      DsChannelType channel = (DsChannelType)o->int32Value();
      if (Error::isOK(err = checkParam(aParams, "value", o))) {
        double value = o->doubleValue();
        // check optional apply_now flag
        bool apply_now = true; // non-buffered write by default
        o = aParams->get("apply_now");
        if (o) {
          apply_now = o->boolValue();
        }
        // reverse build the correctly structured property value: { channelStates: { <channel>: { value:<value> } } }
        // - value
        o = aParams->newObject();
        o->add("value", o->newDouble(value));
        // - channel id
        ApiValuePtr ch = o->newObject();
        ch->add(string_format("%d",channel), o);
        // - channelStates
        ApiValuePtr propValue = ch->newObject();
        propValue->add("channelStates", ch);
        // now access the property
        err = accessProperty(apply_now ? access_write : access_write_preload, propValue, ApiValuePtr(), VDC_API_DOMAIN, PropertyDescriptorPtr());
      }
    }
    if (!Error::isOK(err)) {
      LOG(LOG_WARNING, "setOutputChannelValue error: %s\n", err->description().c_str());
    }
  }
  else if (aMethod=="identify") {
    // identify to user
    LOG(LOG_NOTICE, "Identify in device %s:\n", shortDesc().c_str());
    identifyToUser();
  }
  else {
    inherited::handleNotification(aMethod, aParams);
  }
}


void Device::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  // remove from container management
  DevicePtr dev = DevicePtr(this);
  classContainerP->removeDevice(dev, aForgetParams);
  // that's all for the base class
  if (aDisconnectResultHandler)
    aDisconnectResultHandler(true);
}


void Device::hasVanished(bool aForgetParams)
{
  // have device send a vanish message
  sendRequest("vanish", ApiValuePtr());
  // then disconnect it in software
  // Note that disconnect() might delete the Device object (so 'this' gets invalid)
  disconnect(aForgetParams, NULL);
}



// returns 0 for non-area scenes, area number for area scenes
static int areaFromScene(SceneNo aSceneNo)
{
  int area = 0;
  switch(aSceneNo) {
    case T1_S0:
    case T1_S1:
    case T1_S2:
    case T1_S3:
    case T1_S4:
    case T1_INC:
    case T1_DEC:
    case T1_STOP_S:
    case T1E_S0:
    case T1E_S1:
      area = 1;
      break;
    case T2_S0:
    case T2_S1:
    case T2_S2:
    case T2_S3:
    case T2_S4:
    case T2_INC:
    case T2_DEC:
    case T2_STOP_S:
    case T2E_S0:
    case T2E_S1:
      area = 2;
      break;
    case T3_S0:
    case T3_S1:
    case T3_S2:
    case T3_S3:
    case T3_S4:
    case T3_INC:
    case T3_DEC:
    case T3_STOP_S:
    case T3E_S0:
    case T3E_S1:
      area = 3;
      break;
    case T4_S0:
    case T4_S1:
    case T4_S2:
    case T4_S3:
    case T4_S4:
    case T4_INC:
    case T4_DEC:
    case T4_STOP_S:
    case T4E_S0:
    case T4E_S1:
      area = 4;
      break;
  }
  return area;
}


static SceneNo mainSceneForArea(int aArea)
{
  switch (aArea) {
    case 1: return T1_S1;
    case 2: return T2_S1;
    case 3: return T3_S1;
    case 4: return T4_S1;
  }
  return T0_S1; // no area, main scene for room
}


static SceneNo offSceneForArea(int aArea)
{
  switch (aArea) {
    case 1: return T1_S0;
    case 2: return T2_S0;
    case 3: return T3_S0;
    case 4: return T4_S0;
  }
  return T0_S0; // no area, off scene for room
}



#pragma mark - dimming


// implementation of "dimChannel" vDC API command and legacy dimming
// Note: ensures dimming only continues for at most aAutoStopAfter
void Device::dimChannelForArea(DsChannelType aChannel, DsDimMode aDimMode, int aArea, MLMicroSeconds aAutoStopAfter)
{
  if (aArea!=0) {
    SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
    if (scenes) {
      // check area first
      SceneNo areaScene = mainSceneForArea(aArea);
      DsScenePtr scene = scenes->getScene(areaScene);
      if (scene->isDontCare()) {
        LOG(LOG_DEBUG, "- area main scene(%d) is dontCare -> suppress dimChannel for Area %d\n", areaScene, aArea);
        return; // not in this area, suppress dimming
      }
    }
  }
  // requested dimming this device, no area suppress active
  if (aDimMode!=currentDimMode || aChannel!=currentDimChannel) {
    // mode changes
    if (aDimMode!=dimmode_stop) {
      // start or change direction
      if (currentDimMode==dimmode_stop) {
        // start dimming from stopped state: install timeout
        dimTimeoutTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&Device::dimAutostopHandler, this, aChannel), aAutoStopAfter);
      }
      else {
        // change dimming direction or channel
        // - stop previous dimming operation
        dimChannel(currentDimChannel, dimmode_stop);
        // - start new
        MainLoop::currentMainLoop().rescheduleExecutionTicket(dimTimeoutTicket, aAutoStopAfter);
      }
    }
    else {
      // stop
      MainLoop::currentMainLoop().cancelExecutionTicket(dimTimeoutTicket);
    }
    // actually execute
    dimChannel(aChannel, aDimMode);
    currentDimMode = aDimMode;
    currentDimChannel = aChannel;
  }
  else {
    // same dim mode, just retrigger if dimming right now
    if (aDimMode!=dimmode_stop) {
      MainLoop::currentMainLoop().rescheduleExecutionTicket(dimTimeoutTicket, aAutoStopAfter);
    }
  }
}



// returns main dim scene INC_S/DEC_S/STOP_S for any type of dim scene
// returns 0 for non-dim scenes
static SceneNo mainDimScene(SceneNo aSceneNo)
{
  SceneNo dimScene = 0;
  switch (aSceneNo) {
    case INC_S:
    case T1_INC:
    case T2_INC:
    case T3_INC:
    case T4_INC:
      dimScene = INC_S;
      break;
    case DEC_S:
    case T1_DEC:
    case T2_DEC:
    case T3_DEC:
    case T4_DEC:
      dimScene = DEC_S;
      break;
    case STOP_S:
    case T1_STOP_S:
    case T2_STOP_S:
    case T3_STOP_S:
    case T4_STOP_S:
      dimScene = STOP_S;
      break;
  }
  return dimScene;
}



// autostop handler (for both dimChannel and legacy dimming)
void Device::dimAutostopHandler(DsChannelType aChannel)
{
  // timeout: stop dimming immediately
  dimChannel(aChannel, dimmode_stop);
  currentDimMode = dimmode_stop; // stopped now
}



#define DIM_STEP_INTERVAL_MS 300.0
#define DIM_STEP_INTERVAL (DIM_STEP_INTERVAL_MS*MilliSecond)

// actual dimming implementation, usually overridden by subclasses to provide more optimized/precise dimming
void Device::dimChannel(DsChannelType aChannelType, DsDimMode aDimMode)
{
  DBGLOG(LOG_INFO, "dimChannel: channel=%d %s\n", aChannelType, aDimMode==dimmode_stop ? "STOPS dimming" : (aDimMode==dimmode_up ? "starts dimming UP" : "starts dimming DOWN"));
  // Simple base class implementation just increments/decrements channel values periodically (and skips steps when applying values is too slow)
  if (aDimMode==dimmode_stop) {
    // stop dimming
    isDimming = false;
    MainLoop::currentMainLoop().cancelExecutionTicket(dimHandlerTicket);
  }
  else {
    // start dimming
    ChannelBehaviourPtr ch = getChannelByType(aChannelType);
    if (ch) {
      // make sure the start point is calculated if needed
      ch->getChannelValueCalculated();
      ch->setNeedsApplying(0); // force re-applying start point, no transition time
      // calculate increment
      double increment = (aDimMode==dimmode_up ? DIM_STEP_INTERVAL_MS : -DIM_STEP_INTERVAL_MS) * ch->getDimPerMS();
      // start ticking
      isDimming = true;
      // wait for all apply operations to really complete before starting to dim
      DoneCB dd = boost::bind(&Device::dimDoneHandler, this, ch, increment, MainLoop::now());
      waitForApplyComplete(boost::bind(&Device::requestApplyingChannels, this, dd, false));
    }
  }
}


void Device::dimHandler(ChannelBehaviourPtr aChannel, double aIncrement, MLMicroSeconds aNow)
{
  // increment channel value
  aChannel->dimChannelValue(aIncrement, DIM_STEP_INTERVAL);
  // apply to hardware
  requestApplyingChannels(boost::bind(&Device::dimDoneHandler, this, aChannel, aIncrement, aNow+DIM_STEP_INTERVAL), true); // apply in dimming mode
}


void Device::dimDoneHandler(ChannelBehaviourPtr aChannel, double aIncrement, MLMicroSeconds aNextDimAt)
{
  // keep up with actual dim time
  MLMicroSeconds now = MainLoop::now();
  while (aNextDimAt<now) {
    // missed this step - simply increment channel and target time, but do not cause re-apply
    DBGLOG(LOG_DEBUG, "dimChannel: applyChannelValues() was too slow while dimming channel=%d -> skipping next dim step\n", aChannel->getChannelType());
    aChannel->dimChannelValue(aIncrement, DIM_STEP_INTERVAL);
    aNextDimAt += DIM_STEP_INTERVAL;
  }
  if (isDimming) {
    // now schedule next inc/update step
    dimHandlerTicket = MainLoop::currentMainLoop().executeOnceAt(boost::bind(&Device::dimHandler, this, aChannel, aIncrement, _1), aNextDimAt);
  }
}


#pragma mark - high level serialized hardware access

#define SERIALIZER_WATCHDOG 1
#define SERIALIZER_WATCHDOG_TIMEOUT (20*Second)

void Device::requestApplyingChannels(DoneCB aAppliedOrSupersededCB, bool aForDimming)
{
  FOCUSLOG("requestApplyingChannels entered in device %s\n", shortDesc().c_str());
  // Caller wants current channel values applied to hardware
  // Three possible cases:
  // a) hardware is busy applying new values already -> confirm previous request to apply as superseded
  // b) hardware is busy updating values -> wait until this is done
  // c) hardware is not busy -> start apply right now
  if (applyInProgress) {
    FOCUSLOG("- requestApplyingChannels called while apply already running\n");
    // case a) confirm previous request because superseded
    if (appliedOrSupersededCB) {
      FOCUSLOG("- confirming previous (superseded) apply request\n");
      DoneCB cb = appliedOrSupersededCB;
      appliedOrSupersededCB = aAppliedOrSupersededCB; // in case current callback should request another change, callback is already installed
      cb(); // call back now, values have been superseded
      FOCUSLOG("- previous (superseded) apply request confirmed\n");
    }
    else {
      appliedOrSupersededCB = aAppliedOrSupersededCB;
    }
    // - when previous request actually terminates, we need another update to make sure finally settled values are correct
    missedApplyAttempts++;
    FOCUSLOG("- missed requestApplyingChannels requests now %d\n", missedApplyAttempts);
  }
  else if (updateInProgress) {
    FOCUSLOG("- requestApplyingChannels called while update running -> postpone apply\n");
    // case b) cannot execute until update finishes
    missedApplyAttempts++;
    appliedOrSupersededCB = aAppliedOrSupersededCB;
    applyInProgress = true;
  }
  else {
    // case c) applying is not currently in progress, start updating hardware now
    FOCUSLOG("- ready, calling applyChannelValues() in device %s\n", shortDesc().c_str());
    #if SERIALIZER_WATCHDOG
    // - start watchdog
    MainLoop::currentMainLoop().cancelExecutionTicket(serializerWatchdogTicket); // cancel old
    serializerWatchdogTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&Device::serializerWatchdog, this), 10*Second); // new
    FOCUSLOG("+++++ Serializer watchdog started for apply with ticket #%ld\n", serializerWatchdogTicket);
    #endif
    // - start applying
    appliedOrSupersededCB = aAppliedOrSupersededCB;
    applyInProgress = true;
    applyChannelValues(boost::bind(&Device::applyingChannelsComplete, this), aForDimming);
  }
}


void Device::waitForApplyComplete(DoneCB aApplyCompleteCB)
{
  if (!applyInProgress) {
    // not applying anything, immediately call back
    FOCUSLOG("- waitForApplyComplete() called while no apply in progress -> immediately call back\n");
    aApplyCompleteCB();
  }
  else {
    // apply in progress, save callback, will be called once apply is complete
    if (applyCompleteCB) {
      // already regeistered, chain it
      FOCUSLOG("- waitForApplyComplete() called while apply in progress and another callback already set -> install callback fork\n");
      applyCompleteCB = boost::bind(&Device::forkDoneCB, this, applyCompleteCB, aApplyCompleteCB);
    }
    else {
      FOCUSLOG("- waitForApplyComplete() called while apply in progress and no callback already set -> install callback\n");
      applyCompleteCB = aApplyCompleteCB;
    }
  }
}


void Device::forkDoneCB(DoneCB aOriginalCB, DoneCB aNewCallback)
{
  FOCUSLOG("- forking applyCompleteCB\n");
  aOriginalCB();
  aNewCallback();
}



void Device::serializerWatchdog()
{
  #if SERIALIZER_WATCHDOG
  FOCUSLOG("##### Serializer watchdog ticket #%ld expired\n", serializerWatchdogTicket);
  if (applyInProgress) {
    LOG(LOG_WARNING, "##### Serializer watchdog force-ends apply with %d missed attempts in device %s\n", missedApplyAttempts, shortDesc().c_str());
    missedApplyAttempts = 0;
    applyingChannelsComplete();
    FOCUSLOG("##### Force-ending apply complete\n");
  }
  if (updateInProgress) {
    LOG(LOG_WARNING, "##### Serializer watchdog force-ends update in device %s\n", shortDesc().c_str());
    updatingChannelsComplete();
    FOCUSLOG("##### Force-ending complete\n");
  }
  #endif
}


bool Device::checkForReapply()
{
  LOG(LOG_DEBUG, "checkForReapply in device %s - missed %d apply attempts in between\n", shortDesc().c_str(), missedApplyAttempts);
  if (missedApplyAttempts>0) {
    // request applying again to make sure final values are applied
    // - re-use callback of most recent requestApplyingChannels(), will be called once this attempt has completed (or superseded again)
    FOCUSLOG("- checkForReapply now requesting final channel apply\n");
    missedApplyAttempts = 0; // clear missed
    applyInProgress = false; // must be cleared for requestApplyingChannels() to actually do something
    requestApplyingChannels(appliedOrSupersededCB, false); // final apply after missing other apply commands may not optimize for dimming
    // - done for now
    return true; // reapply needed and started
  }
  return false; // no repply pending
}



// hardware has completed applying values
void Device::applyingChannelsComplete()
{
  FOCUSLOG("applyingChannelsComplete entered in device %s\n", shortDesc().c_str());
  #if SERIALIZER_WATCHDOG
  if (serializerWatchdogTicket) {
    FOCUSLOG("----- Serializer watchdog ticket #%ld cancelled - apply complete\n", serializerWatchdogTicket);
  }
  #endif
  MainLoop::currentMainLoop().cancelExecutionTicket(serializerWatchdogTicket); // cancel watchdog
  applyInProgress = false;
  // if more apply request have happened in the meantime, we need to reapply now
  if (!checkForReapply()) {
    // apply complete and no final re-apply pending
    // - confirm because finally applied
    FOCUSLOG("- applyingChannelsComplete - really completed\n");
    if (appliedOrSupersededCB) {
      FOCUSLOG("- confirming apply (really) finalized\n");
      DoneCB cb = appliedOrSupersededCB;
      appliedOrSupersededCB = NULL; // ready for possibly taking new callback in case current callback should request another change
      cb(); // call back now, values have been superseded
      // check for independent operation waiting for apply complete
      if (applyCompleteCB) {
        FOCUSLOG("- confirming apply (really) finalized to waitForApplyComplete() client\n");
        cb = applyCompleteCB;
        applyCompleteCB = NULL;
        cb();
      }
      FOCUSLOG("- confirmed apply (really) finalized\n");
    }
  }
}



/// request that channel values are updated by reading them back from the device's hardware
/// @param aUpdatedOrCachedCB will be called when values are updated with actual hardware values
///   or pending values are in process to be applied to the hardware and thus these cached values can be considered current.
/// @note this method is only called at startup and before saving scenes to make sure changes done to the outputs directly (e.g. using
///   a direct remote control for a lamp) are included. Just reading a channel state does not call this method.
void Device::requestUpdatingChannels(DoneCB aUpdatedOrCachedCB)
{
  FOCUSLOG("requestUpdatingChannels entered in device %s\n", shortDesc().c_str());
  // Caller wants current values from hardware
  // Three possible cases:
  // a) hardware is busy updating values already -> serve previous callback (with stale values) and install new callback
  // b) hardware is busy applying new values -> consider cache most recent
  // c) hardware is not busy -> start reading values
  if (updateInProgress) {
    // case a) serialize updates: terminate previous callback with stale values and install new one
    if (updatedOrCachedCB) {
      FOCUSLOG("- confirming channels updated for PREVIOUS request with stale values (as asked again)\n");
      DoneCB cb = updatedOrCachedCB;
      updatedOrCachedCB = aUpdatedOrCachedCB; // install new
      cb(); // execute old
      FOCUSLOG("- confirmed channels updated for PREVIOUS request with stale values (as asked again)\n");
    }
    else {
      updatedOrCachedCB = aUpdatedOrCachedCB; // install new
    }
    // done, actual results will serve most recent request for values
  }
  else if (applyInProgress) {
    // case b) no update pending, but applying values right now: return current values as hardware values are in
    //   process of being overwritten by those
    if (aUpdatedOrCachedCB) {
      FOCUSLOG("- confirming channels already up-to-date (as HW update is in progress)\n");
      aUpdatedOrCachedCB(); // execute old
      FOCUSLOG("- confirmed channels already up-to-date (as HW update is in progress)\n");
    }
  }
  else {
    // case c) hardware is not busy, start reading back current values
    FOCUSLOG("requestUpdatingChannels: hardware ready, calling syncChannelValues() in device %s\n", shortDesc().c_str());
    updatedOrCachedCB = aUpdatedOrCachedCB; // install new callback
    updateInProgress = true;
    #if SERIALIZER_WATCHDOG
    // - start watchdog
    MainLoop::currentMainLoop().cancelExecutionTicket(serializerWatchdogTicket); // cancel old
    serializerWatchdogTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&Device::serializerWatchdog, this), SERIALIZER_WATCHDOG_TIMEOUT);
    FOCUSLOG("+++++ Serializer watchdog started for update with ticket #%ld\n", serializerWatchdogTicket);
    #endif
    // - trigger querying hardware
    syncChannelValues(boost::bind(&Device::updatingChannelsComplete, this));
  }
}


void Device::updatingChannelsComplete()
{
  #if SERIALIZER_WATCHDOG
  if (serializerWatchdogTicket) {
    FOCUSLOG("----- Serializer watchdog ticket #%ld cancelled - update complete\n", serializerWatchdogTicket);
  }
  #endif
  if (updateInProgress) {
    FOCUSLOG("endUpdatingChannels in device %s, actually waiting for these result\n", shortDesc().c_str());
    updateInProgress = false;
    if (updatedOrCachedCB) {
      FOCUSLOG("- confirming channels updated from hardware\n");
      DoneCB cb = updatedOrCachedCB;
      updatedOrCachedCB = NULL; // ready for possibly taking new callback in case current callback should request another change
      cb(); // call back now, cached values are either updated from hardware or superseded by pending updates TO hardware
      FOCUSLOG("- confirmed channels updated from hardware\n");
    }
  }
  else {
    FOCUSLOG("UNEXPECTED endUpdatingChannels in device %s -> discarded\n", shortDesc().c_str());
  }
  // if we have got apply requests in the meantime, we need to do a reapply now
  checkForReapply();
}



#pragma mark - scene operations

void Device::callScene(SceneNo aSceneNo, bool aForce)
{
  // see if we have a scene table at all
  SceneDeviceSettingsPtr scenes = getScenes();
  if (scenes) {
    DsScenePtr scene;
    // check special scene numbers first
    SceneNo dimSceneNo = 0;
    if (aSceneNo==T1234_CONT) {
      // area dimming continuation
      // - reschedule dimmer timeout (=keep dimming)
      MainLoop::currentMainLoop().rescheduleExecutionTicket(dimTimeoutTicket, LEGACY_DIM_STEP_TIMEOUT);
      // - otherwise: NOP
      return;
    }
    // see if it is a dim scene and normalize to INC_S/DEC_S/STOP_S
    dimSceneNo = mainDimScene(aSceneNo);
    if (dimSceneNo!=0) {
      // Start/Stop legacy dimming via INC_S/DEC_S/STOP_S
      dimChannelForArea(
        channeltype_default,
        dimSceneNo==STOP_S ? dimmode_stop : (dimSceneNo==INC_S ? dimmode_up : dimmode_down),
        areaFromScene(aSceneNo),
        LEGACY_DIM_STEP_TIMEOUT
      );
      return;
    }
    LOG(LOG_NOTICE, "CallScene(%d) (non-dimming!) in device %s:\n", aSceneNo, shortDesc().c_str());
    // check for area
    int area = areaFromScene(aSceneNo);
    // make sure dimming stops for any non-dimming scene call
    if (currentDimMode!=dimmode_stop) {
      // any non-dimming scene call stops dimming
      LOG(LOG_NOTICE, "- interrupts dimming in progress\n");
      dimChannelForArea(currentDimChannel, dimmode_stop, area, 0);
    }
    // filter area scene calls via area main scene's (area x on, Tx_S1) dontCare flag
    if (area) {
      LOG(LOG_DEBUG, "- callScene(%d): is area #%d scene\n", aSceneNo, area);
      // check if device is in area (criteria used is dontCare flag OF THE AREA ON SCENE (other don't care flags are irrelevant!)
      scene = scenes->getScene(mainSceneForArea(area));
      if (scene->isDontCare()) {
        LOG(LOG_DEBUG, "- area main scene(%d) is dontCare -> suppress\n", mainSceneForArea(area));
        return; // not in this area, suppress callScene entirely
      }
      // call applies, if it is area off it resets localPriority
      if (aSceneNo>=T1_S0 && aSceneNo<=T4_S0) {
        // area is switched off -> end local priority
        LOG(LOG_DEBUG, "- is area off scene -> ends localPriority now\n");
        output->setLocalPriority(false);
      }
    }
    // not dimming, use scene as passed
    scene = scenes->getScene(aSceneNo);
    if (scene) {
      LOG(LOG_DEBUG, "- effective normalized scene to apply to output is %d, dontCare=%d\n", scene->sceneNo, scene->isSceneValueFlagSet(0, valueflags_dontCare));
      if (!scene->isDontCare()) {
        // Scene found and dontCare not set, check details
        // - check local priority
        if (!area && output->hasLocalPriority()) {
          // non-area scene call, but device is in local priority and scene does not ignore local priority
          if (!aForce && !scene->ignoresLocalPriority()) {
            // not forced nor localpriority ignored, localpriority prevents applying non-area scene
            LOG(LOG_DEBUG, "- Non-area scene, localPriority set, scene does not ignore local prio and not forced -> suppressed\n");
            return; // suppress scene call entirely
          }
          else {
            // forced or scene ignores local priority, scene is applied anyway, and also clears localPriority
            output->setLocalPriority(false);
          }
        }
        // - make sure we have the lastState pseudo-scene for undo
        if (!previousState)
          previousState = scenes->newDefaultScene(aSceneNo);
        else
          previousState->sceneNo = aSceneNo; // we remember the scene for which these are undo values in sceneNo of the pseudo scene
        // - now apply to output
        if (output) {
          // Non-dimming scene: have output save its current state into the previousState pseudo scene
          // Note: the actual updating might happen later (when the hardware responds) but
          //   implementations must make sure access to the hardware is serialized such that
          //   the values are captured before values from applyScene() below are applied.
          output->captureScene(previousState, true, boost::bind(&Device::outputUndoStateSaved,this,output,scene)); // apply only after capture is complete
        } // if output
      } // not dontCare
      else {
        // do other scene actions now, as dontCare prevented applying scene above
        if (output) {
          output->performSceneActions(scene, boost::bind(&Device::sceneActionsComplete, this, scene));
        } // if output
      }
    } // scene found
  } // device with scenes
}



// deferred applying of state, after current state has been captured for this output
void Device::outputUndoStateSaved(DsBehaviourPtr aOutput, DsScenePtr aScene)
{
  OutputBehaviourPtr output = boost::dynamic_pointer_cast<OutputBehaviour>(aOutput);
  if (output) {
    // apply scene logically
    if (output->applyScene(aScene)) {
      // now apply values to hardware
      requestApplyingChannels(boost::bind(&Device::sceneValuesApplied, this, aScene), false);
    }
  }
}


void Device::sceneValuesApplied(DsScenePtr aScene)
{
  // now perform scene special actions such as blinking
  output->performSceneActions(aScene, boost::bind(&Device::sceneActionsComplete, this, aScene));
}


void Device::sceneActionsComplete(DsScenePtr aScene)
{
  // now perform scene special actions such as blinking
  LOG(LOG_DEBUG, "- scene actions for scene %d complete\n", aScene->sceneNo);
}




void Device::undoScene(SceneNo aSceneNo)
{
  LOG(LOG_NOTICE, "UndoScene(%d) in device %s:\n", aSceneNo, shortDesc().c_str());
  if (previousState && previousState->sceneNo==aSceneNo) {
    // there is an undo pseudo scene we can apply
    // scene found, now apply it to the output (if any)
    if (output) {
      // now apply the pseudo state
      output->applyScene(previousState);
      // apply the values now, not dimming
      requestApplyingChannels(NULL, false);
    }
  }
}


void Device::setLocalPriority(SceneNo aSceneNo)
{
  SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
  if (scenes) {
    LOG(LOG_NOTICE, "SetLocalPriority(%d) in device %s:\n", aSceneNo, shortDesc().c_str());
    // we have a device-wide scene table, get the scene object
    DsScenePtr scene = scenes->getScene(aSceneNo);
    if (scene && !scene->isDontCare()) {
      LOG(LOG_DEBUG, "- setLocalPriority(%d): localPriority set\n", aSceneNo);
      output->setLocalPriority(true);
    }
  }
}


void Device::callSceneMin(SceneNo aSceneNo)
{
  SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
  if (scenes) {
    LOG(LOG_NOTICE, "CallSceneMin(%d) in device %s:\n", aSceneNo, shortDesc().c_str());
    // we have a device-wide scene table, get the scene object
    DsScenePtr scene = scenes->getScene(aSceneNo);
    if (scene && !scene->isDontCare()) {
      if (output) {
        output->onAtMinBrightness();
        // apply the values now, not dimming
        requestApplyingChannels(NULL, false);
      }
    }
  }
}




void Device::identifyToUser()
{
  if (output) {
    output->identifyToUser(); // pass on to behaviour by default
  }
  else {
    LOG(LOG_INFO,"***** device 'identify' called (for device with no real identify implementation) *****\n");
  }
}



void Device::saveScene(SceneNo aSceneNo)
{
  // see if we have a scene table at all
  LOG(LOG_NOTICE, "SaveScene(%d) in device:\n", aSceneNo, shortDesc().c_str());
  SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
  if (scenes) {
    // we have a device-wide scene table, get the scene object
    DsScenePtr scene = scenes->getScene(aSceneNo);
    if (scene) {
      // scene found, now capture to all of our outputs
      if (output) {
        // capture value from this output, reading from device (if possible) to catch e.g. color changes applied via external means (hue remote app etc.)
        output->captureScene(scene, true, boost::bind(&Device::outputSceneValueSaved, this, scene));
      }
    }
  }
}


void Device::outputSceneValueSaved(DsScenePtr aScene)
{
  // Check special area scene case: dontCare need to be updated depending on brightness (if zero, set don't care)
  SceneNo sceneNo = aScene->sceneNo;
  int area = areaFromScene(sceneNo);
  if (area) {
    // detail check - set don't care when saving Area On-Scene
    if (sceneNo==mainSceneForArea(area)) {
      // saving Main ON scene - set dontCare flag when brightness is zero, otherwise clear dontCare
      ChannelBehaviourPtr ch = output->getChannelByType(channeltype_brightness);
      if (ch) {
        bool mustBeDontCare = ch->getChannelValue()==0;
        // update this main scene's dontCare
        aScene->setDontCare(mustBeDontCare);
        // also update the off scene's dontCare
        SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
        DsScenePtr offScene = scenes->getScene(offSceneForArea(area));
        if (offScene) {
          offScene->setDontCare(mustBeDontCare);
          // update scene in scene table and DB if dirty
          updateScene(offScene);
        }
      }
    }
  }
  // update scene in scene table and DB if dirty
  updateScene(aScene);
}


void Device::updateScene(DsScenePtr aScene)
{
  SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
  if (scenes && aScene->isDirty()) {
    scenes->updateScene(aScene);
  }
}



void Device::processControlValue(const string &aName, double aValue)
{
  // default base class behaviour is letting know all output behaviours
  if (output) {
    output->processControlValue(aName, aValue);
  }
}



#pragma mark - persistent device params


// load device settings - beaviours + scenes
ErrorPtr Device::load()
{
  ErrorPtr err;
  // if we don't have device settings at this point (created by subclass)
  // create standard base class settings.
  if (!deviceSettings) {
    LOG(LOG_ERR, "***** Device %s does not have settings at load() time! -> probably misconfigured\n", shortDesc().c_str());
    return ErrorPtr(new WebError(500,"missing settings"));
  }
  // load the device settings
  if (deviceSettings) {
    err = deviceSettings->loadFromStore(dSUID.getString().c_str());
    if (!Error::isOK(err)) LOG(LOG_ERR,"Error loading settings for device %s: %s", shortDesc().c_str(), err->description().c_str());
  }
  // load the behaviours
  for (BehaviourVector::iterator pos = buttons.begin(); pos!=buttons.end(); ++pos) (*pos)->load();
  for (BehaviourVector::iterator pos = binaryInputs.begin(); pos!=binaryInputs.end(); ++pos) (*pos)->load();
  for (BehaviourVector::iterator pos = sensors.begin(); pos!=sensors.end(); ++pos) (*pos)->load();
  if (output) output->load();
  return ErrorPtr();
}


ErrorPtr Device::save()
{
  ErrorPtr err;
  // save the device settings
  if (deviceSettings) err = deviceSettings->saveToStore(dSUID.getString().c_str(), false); // only one record per device
  if (!Error::isOK(err)) LOG(LOG_ERR,"Error saving settings for device %s: %s", shortDesc().c_str(), err->description().c_str());
  // save the behaviours
  for (BehaviourVector::iterator pos = buttons.begin(); pos!=buttons.end(); ++pos) (*pos)->save();
  for (BehaviourVector::iterator pos = binaryInputs.begin(); pos!=binaryInputs.end(); ++pos) (*pos)->save();
  for (BehaviourVector::iterator pos = sensors.begin(); pos!=sensors.end(); ++pos) (*pos)->save();
  if (output) output->save();
  return ErrorPtr();
}


ErrorPtr Device::forget()
{
  // delete the device settings
  if (deviceSettings) deviceSettings->deleteFromStore();
  // delete the behaviours
  for (BehaviourVector::iterator pos = buttons.begin(); pos!=buttons.end(); ++pos) (*pos)->forget();
  for (BehaviourVector::iterator pos = binaryInputs.begin(); pos!=binaryInputs.end(); ++pos) (*pos)->forget();
  for (BehaviourVector::iterator pos = sensors.begin(); pos!=sensors.end(); ++pos) (*pos)->forget();
  if (output) output->forget();
  return ErrorPtr();
}

#pragma mark - property access

enum {
  // device level simple parameters
  primaryGroup_key,
  zoneID_key,
  progMode_key,
  idBlockSize_key,
  deviceType_key,
  softwareRemovable_key,
  // output
  output_description_key, // output is not array!
  output_settings_key, // output is not array!
  output_state_key, // output is not array!
  // the scenes + undo
  scenes_key,
  undoState_key,
  // model features
  modelFeatures_key,
  numDeviceFieldKeys
};


const int numBehaviourArrays = 4; // buttons, binaryInputs, Sensors, Channels
const int numDeviceProperties = numDeviceFieldKeys+3*numBehaviourArrays;



static char device_key;
static char device_output_key;

static char device_buttons_key;
static char device_inputs_key;
static char device_sensors_key;
static char device_channels_key;

static char device_scenes_key;

static char device_modelFeatures_key;


int Device::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (!aParentDescriptor) {
    // Accessing properties at the Device (root) level
    return inherited::numProps(aDomain, aParentDescriptor)+numDeviceProperties;
  }
  else if (aParentDescriptor->hasObjectKey(device_modelFeatures_key)) {
    // model features
    return numModelFeatures;
  }
  else if (aParentDescriptor->hasObjectKey(device_buttons_key)) {
    return (int)buttons.size();
  }
  else if (aParentDescriptor->hasObjectKey(device_inputs_key)) {
    return (int)binaryInputs.size();
  }
  else if (aParentDescriptor->hasObjectKey(device_sensors_key)) {
    return (int)sensors.size();
  }
  else if (aParentDescriptor->hasObjectKey(device_channels_key)) {
    return numChannels(); // if no output, this returns 0
  }
  else if (aParentDescriptor->hasObjectKey(device_scenes_key)) {
    SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
    if (scenes)
      return MAX_SCENE_NO;
    else
      return 0; // device with no scenes
  }
  return 0; // none
}



PropertyDescriptorPtr Device::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  // device level properties
  static const PropertyDescription properties[numDeviceProperties] = {
    // common device properties
    { "primaryGroup", apivalue_uint64, primaryGroup_key, OKEY(device_key) },
    { "zoneID", apivalue_uint64, zoneID_key, OKEY(device_key) },
    { "progMode", apivalue_bool, progMode_key, OKEY(device_key) },
    { "idBlockSize", apivalue_uint64, idBlockSize_key, OKEY(device_key) },
    { "x-p44-deviceType", apivalue_string, deviceType_key, OKEY(device_key) },
    { "x-p44-softwareRemovable", apivalue_bool, softwareRemovable_key, OKEY(device_key) },
    // the behaviour arrays
    // Note: the prefixes for xxxDescriptions, xxxSettings and xxxStates must match
    //   getTypeName() of the behaviours.
    { "buttonInputDescriptions", apivalue_object+propflag_container, descriptions_key_offset, OKEY(device_buttons_key) },
    { "buttonInputSettings", apivalue_object+propflag_container, settings_key_offset, OKEY(device_buttons_key) },
    { "buttonInputStates", apivalue_object+propflag_container, states_key_offset, OKEY(device_buttons_key) },
    { "binaryInputDescriptions", apivalue_object+propflag_container, descriptions_key_offset, OKEY(device_inputs_key) },
    { "binaryInputSettings", apivalue_object+propflag_container, settings_key_offset, OKEY(device_inputs_key) },
    { "binaryInputStates", apivalue_object+propflag_container, states_key_offset, OKEY(device_inputs_key) },
    { "sensorDescriptions", apivalue_object+propflag_container, descriptions_key_offset, OKEY(device_sensors_key) },
    { "sensorSettings", apivalue_object+propflag_container, settings_key_offset, OKEY(device_sensors_key) },
    { "sensorStates", apivalue_object+propflag_container, states_key_offset, OKEY(device_sensors_key) },
    { "channelDescriptions", apivalue_object+propflag_container, descriptions_key_offset, OKEY(device_channels_key) },
    { "channelSettings", apivalue_object+propflag_container, settings_key_offset, OKEY(device_channels_key) },
    { "channelStates", apivalue_object+propflag_container, states_key_offset, OKEY(device_channels_key) },
    // the single output
    { "outputDescription", apivalue_object, descriptions_key_offset, OKEY(device_output_key) },
    { "outputSettings", apivalue_object, settings_key_offset, OKEY(device_output_key) },
    { "outputState", apivalue_object, states_key_offset, OKEY(device_output_key) },
    // the scenes array
    { "scenes", apivalue_object+propflag_container, scenes_key, OKEY(device_scenes_key) },
    { "undoState", apivalue_object, undoState_key, OKEY(device_key) },
    // the modelFeatures (row from former dSS visibility matrix, controlling what is shown in the UI)
    { "modelFeatures", apivalue_object+propflag_container, modelFeatures_key, OKEY(device_modelFeatures_key) }
  };
  // C++ object manages different levels, check aParentObjectKey
  if (!aParentDescriptor) {
    // root level - accessing properties on the Device level
    int n = inherited::numProps(aDomain, aParentDescriptor);
    if (aPropIndex<n)
      return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
    aPropIndex -= n; // rebase to 0 for my own first property
    return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
  }
  else if (aParentDescriptor->hasObjectKey(device_modelFeatures_key)) {
    // model features - distinct set of boolean flags
    if (aPropIndex<numModelFeatures) {
      DynamicPropertyDescriptor *descP = new DynamicPropertyDescriptor(aParentDescriptor);
      descP->propertyName = modelFeatureNames[aPropIndex];
      descP->propertyType = apivalue_bool;
      descP->propertyFieldKey = aPropIndex;
      descP->propertyObjectKey = OKEY(device_modelFeatures_key);
      return descP;
    }
  }
  return PropertyDescriptorPtr();
}



PropertyDescriptorPtr Device::getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (
    aParentDescriptor && aParentDescriptor->isArrayContainer() &&
    !aParentDescriptor->hasObjectKey(device_modelFeatures_key) // these are handled by base class via getDescriptorByIndex
  ) {
    // array-like container
    PropertyDescriptorPtr propDesc;
    bool numericName = getNextPropIndex(aPropMatch, aStartIndex);
    if (numericName && aParentDescriptor->hasObjectKey(device_channels_key)) {
      // specific channel addressed by ID, look up index for it
      DsChannelType ct = (DsChannelType)aStartIndex;
      aStartIndex = PROPINDEX_NONE; // default: not found
      // there is an output
      ChannelBehaviourPtr cb = getChannelByType(ct);
      if (cb) {
        aStartIndex = (int)cb->getChannelIndex();
      }
    }
    int n = numProps(aDomain, aParentDescriptor);
    if (aStartIndex!=PROPINDEX_NONE && aStartIndex<n) {
      // within range, create descriptor
      DynamicPropertyDescriptor *descP = new DynamicPropertyDescriptor(aParentDescriptor);
      if (aParentDescriptor->hasObjectKey(device_channels_key)) {
        if (numericName) {
          // query specified a channel number -> return same number in result (to return "0" when default channel "0" was explicitly queried)
          descP->propertyName = aPropMatch; // query = name of object
        }
        else {
          // wildcard, result object is named after channelType
          ChannelBehaviourPtr cb = getChannelByIndex(aStartIndex);
          if (cb) {
            descP->propertyName = string_format("%d", cb->getChannelType());
          }
        }
      }
      else {
        // by index
        descP->propertyName = string_format("%d", aStartIndex);
      }
      descP->propertyType = aParentDescriptor->type();
      descP->propertyFieldKey = aStartIndex;
      descP->propertyObjectKey = aParentDescriptor->objectKey();
      propDesc = PropertyDescriptorPtr(descP);
      // advance index
      aStartIndex++;
    }
    if (aStartIndex>=n || numericName) {
      // no more descriptors OR specific descriptor accessed -> no "next" descriptor
      aStartIndex = PROPINDEX_NONE;
    }
    return propDesc;
  }
  // None of the containers within Device - let base class handle Device-Level properties
  return inherited::getDescriptorByName(aPropMatch, aStartIndex, aDomain, aParentDescriptor);
}



PropertyContainerPtr Device::getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain)
{
  // might be virtual container
  if (aPropertyDescriptor->isArrayContainer()) {
    // one of the local containers
    return PropertyContainerPtr(this); // handle myself
  }
  // containers are elements from the behaviour arrays
  else if (aPropertyDescriptor->hasObjectKey(device_buttons_key)) {
    return buttons[aPropertyDescriptor->fieldKey()];
  }
  else if (aPropertyDescriptor->hasObjectKey(device_inputs_key)) {
    return binaryInputs[aPropertyDescriptor->fieldKey()];
  }
  else if (aPropertyDescriptor->hasObjectKey(device_sensors_key)) {
    return sensors[aPropertyDescriptor->fieldKey()];
  }
  else if (aPropertyDescriptor->hasObjectKey(device_channels_key)) {
    if (!output) return PropertyContainerPtr(); // none
    return output->getChannelByIndex(aPropertyDescriptor->fieldKey());
  }
  else if (aPropertyDescriptor->hasObjectKey(device_scenes_key)) {
    SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
    if (scenes) {
      return scenes->getScene(aPropertyDescriptor->fieldKey());
    }
  }
  else if (aPropertyDescriptor->hasObjectKey(device_output_key)) {
    return output; // only single output or none
  }
  else if (aPropertyDescriptor->hasObjectKey(device_key)) {
    // device level object properties
    if (aPropertyDescriptor->fieldKey()==undoState_key) {
      return previousState;
    }
  }
  // unknown here
  return NULL;
}



bool Device::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(device_key)) {
    // Device level field properties
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        case primaryGroup_key:
          aPropValue->setUint16Value(primaryGroup);
          return true;
        case zoneID_key:
          if (deviceSettings)
            aPropValue->setUint16Value(deviceSettings->zoneID);
          return true;
        case progMode_key:
          aPropValue->setBoolValue(progMode);
          return true;
        case idBlockSize_key:
          aPropValue->setUint16Value(idBlockSize());
          return true;
        case deviceType_key:
          aPropValue->setStringValue(deviceTypeIdentifier());
          return true;
        case softwareRemovable_key:
          aPropValue->setBoolValue(isSoftwareDisconnectable()); return true;
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        case zoneID_key:
          if (deviceSettings) {
            deviceSettings->zoneID = aPropValue->int32Value();
            deviceSettings->markDirty();
          }
          return true;
        case progMode_key:
          progMode = aPropValue->boolValue();
          return true;
      }
    }
  }
  else if (aPropertyDescriptor->hasObjectKey(device_modelFeatures_key)) {
    // model features
    if (aMode==access_read) {
      if (hasModelFeature((DsModelFeatures)aPropertyDescriptor->fieldKey())) {
        aPropValue->setBoolValue(true);
        return true;
      }
      else {
        return false;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


ErrorPtr Device::writtenProperty(PropertyAccessMode aMode, PropertyDescriptorPtr aPropertyDescriptor, int aDomain, PropertyContainerPtr aContainer)
{
  if (aPropertyDescriptor->hasObjectKey(device_scenes_key)) {
    // a scene was written, update needed if dirty
    DsScenePtr scene = boost::dynamic_pointer_cast<DsScene>(aContainer);
    SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
    if (scenes && scene && scene->isDirty()) {
      scenes->updateScene(scene);
      return ErrorPtr();
    }
  }
  else if (
    aPropertyDescriptor->hasObjectKey(device_channels_key) && // one or multiple channel's...
    aPropertyDescriptor->fieldKey()==states_key_offset && // ...state(s)...
    aMode==access_write // ...got a non-preload write
  ) {
    // apply new channel values to hardware, not dimming
    requestApplyingChannels(NULL, false);
  }
  return inherited::writtenProperty(aMode, aPropertyDescriptor, aDomain, aContainer);
}


#pragma mark - Device description/shortDesc


string Device::description()
{
  string s = inherited::description(); // DsAdressable
  if (buttons.size()>0) string_format_append(s, "- Buttons: %d\n", buttons.size());
  if (binaryInputs.size()>0) string_format_append(s, "- Binary Inputs: %d\n", binaryInputs.size());
  if (sensors.size()>0) string_format_append(s, "- Sensors: %d\n", sensors.size());
  if (numChannels()>0) string_format_append(s, "- Output Channels: %d\n", numChannels());
  return s;
}
