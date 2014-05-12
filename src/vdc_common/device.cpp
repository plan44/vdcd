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
      case behaviour_output: {
        aBehaviour->index = outputs.size();
        outputs.push_back(aBehaviour);
        // give behaviour chance to add auxiliary channels
        OutputBehaviourPtr o = boost::dynamic_pointer_cast<OutputBehaviour>(aBehaviour);
        if (o) o->createAuxChannels();
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
      if (scene->isSceneValueFlagSet(0, valueflags_dontCare)) {
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
      LOG(LOG_DEBUG, "- effective normalized scene to apply to output is %d, dontCare=%d\n", scene->sceneNo, scene->isSceneValueFlagSet(0, valueflags_dontCare));
      if (!scene->isSceneValueFlagSet(0, valueflags_dontCare)) {
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
    if (scene && !scene->isSceneValueFlagSet(0, valueflags_dontCare)) {
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
    if (scene && !scene->isSceneValueFlagSet(0, valueflags_dontCare)) {
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
  // the scenes + undo
  scenes_key,
  undoState_key,
  numDeviceFieldKeys
};


const int numBehaviourArrays = 5; // buttons, binaryInputs, Sensors, Outputs, Channels
const int numDeviceProperties = numDeviceFieldKeys+3*numBehaviourArrays;



static char device_key;
static char device_groups_key;

static char device_buttons_key;
static char device_inputs_key;
static char device_sensors_key;
static char device_outputs_key;
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
  else if (aParentDescriptor->hasObjectKey(device_outputs_key)) {
    return (int)outputs.size();
  }
  else if (aParentDescriptor->hasObjectKey(device_channels_key)) {
    // Note: represent every output as a channel. For devices with no default channel IDs or
    //   improperly configured channels, we might get multiple channels with same name (=ID)
    return (int)outputs.size();
  }
  else if (aParentDescriptor->hasObjectKey(device_scenes_key)) {
    SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
    if (scenes)
      return MAX_SCENE_NO;
    else
      return 0; // device with no scenes
  }
  else if (aParentDescriptor->hasObjectKey(device_groups_key)) {
    return 64; // group mask has 64 bits for now
  }
  return 0; // none
}



PropertyDescriptorPtr Device::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numDeviceProperties] = {
    // common device properties
    { "primaryGroup", apivalue_uint64, primaryGroup_key, OKEY(device_key) },
    { "outputIsMemberOfGroup", apivalue_bool+propflag_container, isMember_key, OKEY(device_groups_key) },
    { "zoneID", apivalue_uint64, zoneID_key, OKEY(device_key) },
    { "outputLocalPriority", apivalue_bool, localPriority_key, OKEY(device_key) },
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
    { "outputDescriptions", apivalue_object+propflag_container, descriptions_key_offset, OKEY(device_outputs_key) },
    { "outputSettings", apivalue_object+propflag_container, settings_key_offset, OKEY(device_outputs_key) },
    { "outputStates", apivalue_object+propflag_container, states_key_offset, OKEY(device_outputs_key) },
    { "channelDescriptions", apivalue_object+propflag_container, descriptions_key_offset, OKEY(device_channels_key) },
    { "channelSettings", apivalue_object+propflag_container, settings_key_offset, OKEY(device_channels_key) },
    { "channelStates", apivalue_object+propflag_container, states_key_offset, OKEY(device_channels_key) },
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
      int i=0;
      int channelIndex=-1;
      for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) {
        OutputBehaviourPtr o = boost::dynamic_pointer_cast<OutputBehaviour>(*pos);
        if (o->getChannel()==aStartIndex) {
          channelIndex = i;
          break;
        }
        i++; // next
      }
      aStartIndex = channelIndex>=0 ? channelIndex : PROPINDEX_NONE;
    }
    int n = numProps(aDomain, aParentDescriptor);
    if (aStartIndex!=PROPINDEX_NONE && aStartIndex<n) {
      // within range, create descriptor
      DynamicPropertyDescriptor *descP = new DynamicPropertyDescriptor(aParentDescriptor);
      if (aParentDescriptor->hasObjectKey(device_channels_key)) {
        OutputBehaviourPtr o = boost::dynamic_pointer_cast<OutputBehaviour>(outputs[aStartIndex]);
        descP->propertyName = string_format("%d", o->getChannel());
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
    if (aStartIndex>=n)
      aStartIndex = PROPINDEX_NONE;
    return propDesc;
  }
  // None of the containers within Device - let base class handle Device-Level properties
  return inherited::getDescriptorByName(aPropMatch, aStartIndex, aDomain, aParentDescriptor);
}



PropertyContainerPtr Device::getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain)
{
  // might be virtual container
  if (aPropertyDescriptor->isArrayContainer()) {
    // on of the local containers
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
  else if (aPropertyDescriptor->hasObjectKey(device_outputs_key)) {
    return outputs[aPropertyDescriptor->fieldKey()];
  }
  else if (aPropertyDescriptor->hasObjectKey(device_channels_key)) {
    return outputs[aPropertyDescriptor->fieldKey()];
  }
  else if (aPropertyDescriptor->hasObjectKey(device_scenes_key)) {
    SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
    if (scenes) {
      return scenes->getScene(aPropertyDescriptor->fieldKey());
    }
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
  if (aPropertyDescriptor->hasObjectKey(device_groups_key)) {
    if (aMode==access_read) {
      // read group
      aPropValue->setBoolValue(isMember((DsGroup)aPropertyDescriptor->fieldKey()));
      return true;
    }
    else {
      // write group
      setGroupMembership((DsGroup)aPropertyDescriptor->fieldKey(), aPropValue->boolValue());
      return true;
    }
  }
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
      switch (aPropertyDescriptor->fieldKey()) {
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
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


ErrorPtr Device::writtenProperty(PropertyDescriptorPtr aPropertyDescriptor, int aDomain, PropertyContainerPtr aContainer)
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
  return inherited::writtenProperty(aPropertyDescriptor, aDomain, aContainer);
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
