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
  primaryGroup(group_black_joker)
{
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
//  if (aMethod=="Gugus") {
//    // Do something
//  }
//  else
  {
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
        LOG(LOG_NOTICE, "%s: processControlValue(%s, %f):\n", shortDesc().c_str(), controlValueName.c_str(), value);
        processControlValue(controlValueName, value);
        // apply the values
        applyChannelValues(NULL, false);
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
  else if (aMethod=="identify") {
    // identify to user
    LOG(LOG_NOTICE, "%s: identify:\n", shortDesc().c_str());
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



#pragma mark - dimming


// implementation of "dimChannel" vDC API command and legacy dimming
// Note: ensures dimming only continues for at most aAutoStopAfter
void Device::dimChannelForArea(DsChannelType aChannel, DsDimMode aDimMode, int aArea, MLMicroSeconds aAutoStopAfter)
{
  // TODO: maybe optimize: area check could be omitted when dimming is running already
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
        dimChannel(aChannel, dimmode_stop);
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
      // apply start point (non-dimming), then call dim handler
      applyChannelValues(boost::bind(&Device::dimDoneHandler, this, ch, increment, MainLoop::now()), false);
    }
  }
}


void Device::dimHandler(ChannelBehaviourPtr aChannel, double aIncrement, MLMicroSeconds aNow)
{
  // increment channel value
  aChannel->dimChannelValue(aIncrement, DIM_STEP_INTERVAL);
  // apply to hardware
  applyChannelValues(boost::bind(&Device::dimDoneHandler, this, aChannel, aIncrement, aNow+DIM_STEP_INTERVAL), true); // apply in dimming mode
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
    }
    // see if it is a dim scene and normalize to INC_S/DEC_S/STOP_S
    dimSceneNo = mainDimScene(aSceneNo);
    if (dimSceneNo!=0) {
      // Legacy dimming via INC_S/DEC_S/STOP_S
      dimChannelForArea(
        channeltype_default,
        dimSceneNo==STOP_S ? dimmode_stop : (dimSceneNo==INC_S ? dimmode_up : dimmode_down),
        areaFromScene(aSceneNo),
        LEGACY_DIM_STEP_TIMEOUT
      );
      return;
    }
    LOG(LOG_NOTICE, "%s: callScene(%d) (non-dimming!):\n", shortDesc().c_str(), aSceneNo);
    // check for area
    int area = areaFromScene(aSceneNo);
    // filter area scene calls via area main scene's (area x on, Tx_S1) dontCare flag
    if (area) {
      LOG(LOG_DEBUG, "callScene(%d): is area #%d scene\n", aSceneNo, area);
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
      applyChannelValues(boost::bind(&Device::sceneValuesApplied, this, aScene), false);
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
  LOG(LOG_NOTICE, "%s: undoScene(%d):\n", shortDesc().c_str(), aSceneNo);
  if (previousState && previousState->sceneNo==aSceneNo) {
    // there is an undo pseudo scene we can apply
    // scene found, now apply it to the output (if any)
    if (output) {
      // now apply the pseudo state
      output->applyScene(previousState);
      // apply the values now, not dimming
      applyChannelValues(NULL, false);
    }
  }
}


void Device::setLocalPriority(SceneNo aSceneNo)
{
  SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
  if (scenes) {
    LOG(LOG_NOTICE, "%s: setLocalPriority(%d):\n", shortDesc().c_str(), aSceneNo);
    // we have a device-wide scene table, get the scene object
    DsScenePtr scene = scenes->getScene(aSceneNo);
    if (scene && !scene->isDontCare()) {
      LOG(LOG_DEBUG, "setLocalPriority(%d): localPriority set\n", aSceneNo);
      output->setLocalPriority(true);
    }
  }
}


void Device::callSceneMin(SceneNo aSceneNo)
{
  SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
  if (scenes) {
    LOG(LOG_NOTICE, "%s: callSceneMin(%d):\n", shortDesc().c_str(), aSceneNo);
    // we have a device-wide scene table, get the scene object
    DsScenePtr scene = scenes->getScene(aSceneNo);
    if (scene && !scene->isDontCare()) {
      if (output) {
        output->onAtMinBrightness();
        // apply the values now, not dimming
        applyChannelValues(NULL, false);
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
  LOG(LOG_NOTICE, "%s: saveScene(%d):\n", shortDesc().c_str(), aSceneNo);
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
  // if we don't have device settings at this point (created by subclass)
  // create standard base class settings.
  if (!deviceSettings)
    deviceSettings = DeviceSettingsPtr(new DeviceSettings(*this));
  // load the device settings
  if (deviceSettings)
    deviceSettings->loadFromStore(dSUID.getString().c_str());
  // load the behaviours
  for (BehaviourVector::iterator pos = buttons.begin(); pos!=buttons.end(); ++pos) (*pos)->load();
  for (BehaviourVector::iterator pos = binaryInputs.begin(); pos!=binaryInputs.end(); ++pos) (*pos)->load();
  for (BehaviourVector::iterator pos = sensors.begin(); pos!=sensors.end(); ++pos) (*pos)->load();
  if (output) output->load();
  return ErrorPtr();
}


ErrorPtr Device::save()
{
  // save the device settings
  if (deviceSettings) deviceSettings->saveToStore(dSUID.getString().c_str());
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
  // output
  output_description_key, // output is not array!
  output_settings_key, // output is not array!
  output_state_key, // output is not array!
  // the scenes + undo
  scenes_key,
  undoState_key,
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


int Device::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (!aParentDescriptor) {
    // Accessing properties at the Device (root) level
    return inherited::numProps(aDomain, aParentDescriptor)+numDeviceProperties;
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
  static const PropertyDescription properties[numDeviceProperties] = {
    // common device properties
    { "primaryGroup", apivalue_uint64, primaryGroup_key, OKEY(device_key) },
    { "zoneID", apivalue_uint64, zoneID_key, OKEY(device_key) },
    { "progMode", apivalue_bool, progMode_key, OKEY(device_key) },
    { "idBlockSize", apivalue_uint64, idBlockSize_key, OKEY(device_key) },
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
  return PropertyDescriptorPtr();
}



PropertyDescriptorPtr Device::getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (aParentDescriptor && aParentDescriptor->isArrayContainer()) {
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
    applyChannelValues(NULL, false);
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
