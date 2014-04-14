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
  localPriority(false),
  progMode(false),
  lastDimSceneNo(T0_S0),
  classContainerP(aClassContainerP),
  DsAddressable(&aClassContainerP->getDeviceContainer()),
  primaryGroup(group_black_joker)
{
}


Device::~Device()
{
  buttons.clear();
  binaryInputs.clear();
  outputs.clear();
  sensors.clear();
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



DsGroupMask Device::behaviourGroups()
{
  // or together all group memberships of all behaviours
  DsGroupMask groups = 0;
  for (BehaviourVector::iterator pos = buttons.begin(); pos!=buttons.end(); ++pos) groups |= 1ll<<(*pos)->getGroup();
  for (BehaviourVector::iterator pos = binaryInputs.begin(); pos!=binaryInputs.end(); ++pos) groups |= 1ll<<(*pos)->getGroup();
  for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) groups |= 1ll<<(*pos)->getGroup();
  for (BehaviourVector::iterator pos = sensors.begin(); pos!=sensors.end(); ++pos) groups |= 1ll<<(*pos)->getGroup();
  return groups;
}



bool Device::isMember(DsGroup aColorGroup)
{
  return
    aColorGroup==primaryGroup || // is always member of primary group
    (behaviourGroups() & 0x1ll<<aColorGroup)!=0 || // plus of all groups of all behaviours
    (deviceSettings && (deviceSettings->extraGroups & 0x1ll<<aColorGroup)!=0); // explicit extra membership flag set
}


void Device::setGroupMembership(DsGroup aColorGroup, bool aIsMember)
{
  if (deviceSettings) {
    DsGroupMask newExtraGroups = deviceSettings->extraGroups;
    if (aIsMember) {
      // make explicitly member of a group
      newExtraGroups |= (0x1ll<<aColorGroup);
    }
    else {
      // not explicitly member
      newExtraGroups &= ~(0x1ll<<aColorGroup);
    }
    if (newExtraGroups!=deviceSettings->extraGroups) {
      deviceSettings->extraGroups = newExtraGroups;
      deviceSettings->markDirty();
    }
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
      case behaviour_output:
        aBehaviour->index = outputs.size();
        outputs.push_back(aBehaviour);
        break;
      default:
        LOG(LOG_ERR,"Device::addBehaviour: unknown behaviour type\n");
    }
  }
  else {
    LOG(LOG_ERR,"Device::addBehaviour: NULL behaviour passed\n");
  }
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
        // now process
        LOG(LOG_NOTICE, "%s: processControlValue(%s, %f):\n", shortDesc().c_str(), controlValueName.c_str(), value);
        processControlValue(controlValueName, value);
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
        dimChannel(channel,mode,area);
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
    aDisconnectResultHandler(dev, true);
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


void Device::dimChannel(DsChannelType aChannel, int aDimMode, int aArea)
{
  #warning "%%% to be implemented"
}



void Device::callScene(SceneNo aSceneNo, bool aForce)
{
  // see if we have a scene table at all
  SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
  if (scenes) {
    DsScenePtr scene;
    // check special scene numbers first
    SceneNo dimSceneNo = 0;
    if (aSceneNo==T1234_CONT) {
      if (lastDimSceneNo) {
        // re-use last dim scene
        aSceneNo = lastDimSceneNo;
      }
      else {
        // this device was not part of area dimming, ignore T1234_CONT
        LOG(LOG_DEBUG, "- dimming was not started in this device, ignore T1234_CONT\n");
        return;
      }
    }
    // see if it is a dim scene and normalize to INC_S/DEC_S/STOP_S
    dimSceneNo = mainDimScene(aSceneNo);
    if (dimSceneNo==0) {
      LOG(LOG_NOTICE, "%s: callScene(%d) (non-dimming!):\n", shortDesc().c_str(), aSceneNo);
    }
    lastDimSceneNo = 0; // reset for now (set again if it turns out to be area dimming)
    // check for area
    int area = areaFromScene(aSceneNo);
    // filter area scene calls via area main scene's (area x on, Tx_S1) dontCare flag
    if (area) {
      LOG(LOG_DEBUG, "callScene(%d): is area #%d scene\n", aSceneNo, area);
      // check if device is in area (criteria used is dontCare flag OF THE AREA ON SCENE (other don't care flags are irrelevant!)
      scene = scenes->getScene(mainSceneForArea(area));
      if (scene->dontCare) {
        LOG(LOG_DEBUG, "- area main scene(%d) is dontCare -> suppress\n", mainSceneForArea(area));
        return; // not in this area, suppress callScene entirely
      }
      // call applies, if it is area off it resets localPriority
      if (aSceneNo>=T1_S0 && aSceneNo<=T4_S0) {
        // area is switched off -> end local priority
        LOG(LOG_DEBUG, "- is area off scene -> ends localPriority now\n");
        localPriority = false;
      }
    }
    // get the scene to apply to output
    if (dimSceneNo) {
      // dimming, use normalized dim scene (INC_S/DEC_S/STOP_S) in all cases, including area dimming
      scene = scenes->getScene(dimSceneNo);
      // if area dimming, remember last are dimming scene for possible subsequent T1234_CONT
      if (area)
        lastDimSceneNo = aSceneNo;
    }
    else {
      // not dimming, use scene as passed
      scene = scenes->getScene(aSceneNo);
    }
    if (scene) {
      LOG(LOG_DEBUG, "- effective normalized scene to apply to output is %d, dontCare=%d\n", scene->sceneNo, scene->dontCare);
      if (!scene->dontCare) {
        // Scene found and dontCare not set, check details
        // - check local priority
        if (!area && localPriority) {
          // non-area scene call, but device is in local priority and scene does not ignore local priority
          if (!aForce && !scene->ignoreLocalPriority) {
            // not forced nor localpriority ignored, localpriority prevents applying non-area scene
            LOG(LOG_DEBUG, "- Non-area scene, localPriority set, scene does not ignore local prio and not forced -> suppressed\n");
            return; // suppress scene call entirely
          }
          else {
            // forced or scene ignores local priority, scene is applied anyway, and also clears localPriority
            localPriority = false;
          }
        }
        // - make sure we have the lastState pseudo-scene for undo (but not for dimming scenes)
        if (dimSceneNo==0) {
          if (!previousState)
            previousState = scenes->newDefaultScene(aSceneNo);
          else
            previousState->sceneNo = aSceneNo; // we remember the scene for which these are undo values in sceneNo of the pseudo scene
        }
        // - now apply to all of our outputs
        for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) {
          OutputBehaviourPtr output = boost::dynamic_pointer_cast<OutputBehaviour>(*pos);
          if (output) {
            if (dimSceneNo) {
              // Dimming scene: apply right now
              output->applyScene(scene);
              // Note: no special actions are performed on dimming scene
            }
            else {
              // Non-dimming scene: have output save its current state into the previousState pseudo scene
              // Note: the actual updating might happen later (when the hardware responds) but
              //   implementations must make sure access to the hardware is serialized such that
              //   the values are captured before values from applyScene() below are applied.
              output->captureScene(previousState, boost::bind(&Device::outputUndoStateSaved,this,output,scene)); // apply only after capture is complete
            }
          } // if output
        } // for
      } // not dontCare
      else {
        // do other scene actions now, as dontCare prevented applying scene above
        for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) {
          OutputBehaviourPtr output = boost::dynamic_pointer_cast<OutputBehaviour>(*pos);
          if (output) {
            output->performSceneActions(scene);
          } // if output
        } // for
      }
    } // scene found
  } // device with scenes
}


// deferred applying of state, after current state has been captured for this output
void Device::outputUndoStateSaved(DsBehaviourPtr aOutput, DsScenePtr aScene)
{
  OutputBehaviourPtr output = boost::dynamic_pointer_cast<OutputBehaviour>(aOutput);
  if (output) {
    output->applyScene(aScene);
    output->performSceneActions(aScene);
  }
}




void Device::undoScene(SceneNo aSceneNo)
{
  LOG(LOG_NOTICE, "%s: undoScene(%d):\n", shortDesc().c_str(), aSceneNo);
  if (previousState && previousState->sceneNo==aSceneNo) {
    // there is an undo pseudo scene we can apply
    // scene found, now apply to all of our outputs
    for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) {
      OutputBehaviourPtr output = boost::dynamic_pointer_cast<OutputBehaviour>(*pos);
      if (output) {
        // now apply the pseudo state
        output->applyScene(previousState);
      }
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
    if (scene && !scene->dontCare) {
      LOG(LOG_DEBUG, "setLocalPriority(%d): localPriority set\n", aSceneNo);
      localPriority = true;
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
    if (scene && !scene->dontCare) {
      for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) {
        OutputBehaviourPtr output = boost::dynamic_pointer_cast<OutputBehaviour>(*pos);
        if (output) {
          output->onAtMinBrightness();
        }
      }
    }
  }
}




void Device::identifyToUser()
{
  LOG(LOG_INFO,"***** device 'identify' called (for device with no real identify implementation) *****\n");
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
      for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) {
        OutputBehaviourPtr output = boost::dynamic_pointer_cast<OutputBehaviour>(*pos);
        if (output) {
          // capture value from this output
          output->captureScene(scene, boost::bind(&Device::outputSceneValueSaved, this, scene));
        }
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
  for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) {
    OutputBehaviourPtr ob = boost::dynamic_pointer_cast<OutputBehaviour>(*pos);
    if (ob) {
      ob->processControlValue(aName, aValue);
    }
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
  for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) (*pos)->load();
  for (BehaviourVector::iterator pos = sensors.begin(); pos!=sensors.end(); ++pos) (*pos)->load();
  return ErrorPtr();
}


ErrorPtr Device::save()
{
  // save the device settings
  if (deviceSettings) deviceSettings->saveToStore(dSUID.getString().c_str());
  // save the behaviours
  for (BehaviourVector::iterator pos = buttons.begin(); pos!=buttons.end(); ++pos) (*pos)->save();
  for (BehaviourVector::iterator pos = binaryInputs.begin(); pos!=binaryInputs.end(); ++pos) (*pos)->save();
  for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) (*pos)->save();
  for (BehaviourVector::iterator pos = sensors.begin(); pos!=sensors.end(); ++pos) (*pos)->save();
  return ErrorPtr();
}


ErrorPtr Device::forget()
{
  // delete the device settings
  if (deviceSettings) deviceSettings->deleteFromStore();
  // delete the behaviours
  for (BehaviourVector::iterator pos = buttons.begin(); pos!=buttons.end(); ++pos) (*pos)->forget();
  for (BehaviourVector::iterator pos = binaryInputs.begin(); pos!=binaryInputs.end(); ++pos) (*pos)->forget();
  for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) (*pos)->forget();
  for (BehaviourVector::iterator pos = sensors.begin(); pos!=sensors.end(); ++pos) (*pos)->forget();
  return ErrorPtr();
}

#pragma mark - property access

enum {
  // device level simple parameters
  primaryGroup_key,
  isMember_key,
  zoneID_key,
  localPriority_key,
  progMode_key,
  idBlockSize_key,
  // the behaviour arrays
  buttonInputDescriptions_key,
  buttonInputSettings_key,
  buttonInputStates_key,
  binaryInputDescriptions_key,
  binaryInputSettings_key,
  binaryInputStates_key,
  sensorDescriptions_key,
  sensorSettings_key,
  sensorStates_key,
  outputDescriptions_key,
  outputSettings_key,
  outputStates_key,
  channelDescriptions_key,
  channelSettings_key,
  channelStates_key,
  outputScenes_key,
  channelScenes_key,
  undoState_key,
  numDeviceProperties
};


static char device_key;

int Device::numProps(int aDomain)
{
  return inherited::numProps(aDomain)+numDeviceProperties;
}


const PropertyDescriptor *Device::getPropertyDescriptor(int aPropIndex, int aDomain)
{
  static const PropertyDescriptor properties[numDeviceProperties] = {
    // common device properties
    { "primaryGroup", apivalue_uint64, false, primaryGroup_key, &device_key },
    { "outputIsMemberOfGroup", apivalue_bool, true, isMember_key, &device_key },
    { "zoneID", apivalue_uint64, false, zoneID_key, &device_key },
    { "outputLocalPriority", apivalue_bool, false, localPriority_key, &device_key },
    { "progMode", apivalue_bool, false, progMode_key, &device_key },
    { "idBlockSize", apivalue_uint64, false, idBlockSize_key, &device_key },
    // the behaviour arrays
    // Note: the prefixes for xxxDescriptions, xxxSettings and xxxStates must match
    //   getTypeName() of the behaviours.
    { "buttonInputDescriptions", apivalue_object, true, buttonInputDescriptions_key, &device_key },
    { "buttonInputSettings", apivalue_object, true, buttonInputSettings_key, &device_key },
    { "buttonInputStates", apivalue_object, true, buttonInputStates_key, &device_key },
    { "binaryInputDescriptions", apivalue_object, true, binaryInputDescriptions_key, &device_key },
    { "binaryInputSettings", apivalue_object, true, binaryInputSettings_key, &device_key },
    { "binaryInputStates", apivalue_object, true, binaryInputStates_key, &device_key },
    { "sensorDescriptions", apivalue_object, true, sensorDescriptions_key, &device_key },
    { "sensorSettings", apivalue_object, true, sensorSettings_key, &device_key },
    { "sensorStates", apivalue_object, true, sensorStates_key, &device_key },
    { "outputDescriptions", apivalue_object, true, outputDescriptions_key, &device_key },
    { "outputSettings", apivalue_object, true, outputSettings_key, &device_key },
    { "outputStates", apivalue_object, true, outputStates_key, &device_key },
    { "channelDescriptions", apivalue_object, true, channelDescriptions_key, &device_key },
    { "channelSettings", apivalue_object, true, channelSettings_key, &device_key },
    { "channelStates", apivalue_object, true, channelStates_key, &device_key },
    // the scenes array
    { "outputScenes", apivalue_object, true, outputScenes_key, &device_key },
    { "channelScenes", apivalue_object, true, channelScenes_key, &device_key },
    { "undoState", apivalue_object, false, undoState_key, &device_key },
  };
  int n = inherited::numProps(aDomain);
  if (aPropIndex<n)
    return inherited::getPropertyDescriptor(aPropIndex, aDomain); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return &properties[aPropIndex];
}



PropertyContainerPtr Device::getContainer(const PropertyDescriptor &aPropertyDescriptor, int &aDomain, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&device_key) {
    switch (aPropertyDescriptor.accessKey) {
      // Note: domain is adjusted to differentiate between descriptions, settings and states of the same object
      // buttons
      case buttonInputDescriptions_key:
        aDomain = VDC_API_BHVR_DESC;
        goto buttons;
      case buttonInputSettings_key:
        aDomain = VDC_API_BHVR_SETTINGS;
        goto buttons;
      case buttonInputStates_key:
        aDomain = VDC_API_BHVR_STATES;
      buttons:
        if (aIndex<buttons.size()) return buttons[aIndex];
        break;
      // binaryInputs
      case binaryInputDescriptions_key:
        aDomain = VDC_API_BHVR_DESC;
        goto binaryInputs;
      case binaryInputSettings_key:
        aDomain = VDC_API_BHVR_SETTINGS;
        goto binaryInputs;
      case binaryInputStates_key:
        aDomain = VDC_API_BHVR_STATES;
      binaryInputs:
        if (aIndex<binaryInputs.size()) return binaryInputs[aIndex];
        break;
      // outputs by index
      case outputDescriptions_key:
        aDomain = VDC_API_BHVR_DESC;
        goto outputs;
      case outputSettings_key:
        aDomain = VDC_API_BHVR_SETTINGS;
        goto outputs;
      case outputStates_key:
        aDomain = VDC_API_BHVR_STATES;
      outputs:
        if (aIndex<outputs.size()) return outputs[aIndex];
        break;
      // outputs by channel
      case channelDescriptions_key:
        aDomain = VDC_API_BHVR_DESC;
        goto channels;
      case channelSettings_key:
        aDomain = VDC_API_BHVR_SETTINGS;
        goto channels;
      case channelStates_key:
        aDomain = VDC_API_BHVR_STATES;
      channels: {
        // look up the output by channel
        size_t i=0;
        for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) {
          OutputBehaviourPtr o = boost::dynamic_pointer_cast<OutputBehaviour>(*pos);
          if (o->getChannel()==aIndex) {
            return outputs[i];
          }
          // next
          i++;
        }
        // no channel found
        break;
      }
      // sensors
      case sensorDescriptions_key:
        aDomain = VDC_API_BHVR_DESC;
        goto sensors;
      case sensorSettings_key:
        aDomain = VDC_API_BHVR_SETTINGS;
        goto sensors;
      case sensorStates_key:
        aDomain = VDC_API_BHVR_STATES;
      sensors:
        if (aIndex<sensors.size()) return sensors[aIndex];
        break;
      // scenes
      case outputScenes_key:
        aDomain = VDC_API_OUTPUT_SCENES;
        goto scenes;
      case channelScenes_key:
        aDomain = VDC_API_CHANNEL_SCENES;
      scenes: {
        SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
        if (scenes) {
          return scenes->getScene(aIndex);
        }
      }
      // pseudo scene which saves the values before last scene call, used for undoScene
      case undoState_key: {
        if (previousState) {
          return previousState;
        }
      }
    }
  }
  // unknown here
  return NULL;
}


ErrorPtr Device::writtenProperty(const PropertyDescriptor &aPropertyDescriptor, int aDomain, int aIndex, PropertyContainerPtr aContainer)
{
  if (aPropertyDescriptor.objectKey==&device_key) {
    switch (aPropertyDescriptor.accessKey) {
      case outputScenes_key:
      case channelScenes_key: {
        // scene was written, update needed if dirty
        DsScenePtr scene = boost::dynamic_pointer_cast<DsScene>(aContainer);
        SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
        if (scenes && scene && scene->isDirty()) {
          scenes->updateScene(scene);
          return ErrorPtr();
        }
      }
    }
  }
  return inherited::writtenProperty(aPropertyDescriptor, aDomain, aIndex, aContainer);
}




bool Device::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&device_key) {
    if (aIndex==PROP_ARRAY_SIZE) {
      if (aMode!=access_read) {
        return false;
      }
      else {
        // array size query
        switch (aPropertyDescriptor.accessKey) {
          // the isMember pseudo-array
          case isMember_key:
            aPropValue->setUint16Value(64); // max 64 groups
            return true;
          // the behaviour arrays
          case buttonInputDescriptions_key:
          case buttonInputSettings_key:
          case buttonInputStates_key:
            aPropValue->setUint16Value((int)buttons.size());
            return true;
          case binaryInputDescriptions_key:
          case binaryInputSettings_key:
          case binaryInputStates_key:
            aPropValue->setUint16Value((int)binaryInputs.size());
            return true;
          case sensorDescriptions_key:
          case sensorSettings_key:
          case sensorStates_key:
            aPropValue->setUint16Value((int)sensors.size());
            return true;
          case outputDescriptions_key:
          case outputSettings_key:
          case outputStates_key:
            aPropValue->setUint16Value((int)outputs.size());
            return true;
          case channelDescriptions_key:
          case channelSettings_key:
          case channelStates_key:
            aPropValue->setUint16Value(numChannelTypes); // fixed number of possible channels
            return true;
          case outputScenes_key:
          case channelScenes_key:
            if (boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings))
              aPropValue->setUint16Value(MAX_SCENE_NO);
            else
              aPropValue->setUint16Value(0); // no scene table
            return true;
        }
      }
    }
    else if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        case primaryGroup_key:
          aPropValue->setUint16Value(primaryGroup);
          return true;
        case isMember_key:
          // test group bit
          aPropValue->setBoolValue(isMember((DsGroup)aIndex));
          return true;
        case zoneID_key:
          if (deviceSettings)
            aPropValue->setUint16Value(deviceSettings->zoneID);
          return true;
        case localPriority_key:
          aPropValue->setBoolValue(localPriority);
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
      switch (aPropertyDescriptor.accessKey) {
        case isMember_key:
          setGroupMembership((DsGroup)aIndex, aPropValue->boolValue());
          return true;
        case zoneID_key:
          if (deviceSettings) {
            deviceSettings->zoneID = aPropValue->int32Value();
            deviceSettings->markDirty();
          }
          return true;
        case localPriority_key:
          localPriority = aPropValue->boolValue();
          return true;
        case progMode_key:
          progMode = aPropValue->boolValue();
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor, aIndex);
}

#pragma mark - Device description/shortDesc


string Device::description()
{
  string s = inherited::description(); // DsAdressable
  if (buttons.size()>0) string_format_append(s, "- Buttons: %d\n", buttons.size());
  if (binaryInputs.size()>0) string_format_append(s, "- Binary Inputs: %d\n", binaryInputs.size());
  if (outputs.size()>0) string_format_append(s, "- Outputs: %d\n", outputs.size());
  if (sensors.size()>0) string_format_append(s, "- Sensors: %d\n", sensors.size());
  return s;
}
