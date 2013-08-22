//
//  device.cpp
//  vdcd
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "device.hpp"

#include "dsparams.hpp"

using namespace p44;


#pragma mark - Device


Device::Device(DeviceClassContainer *aClassContainerP) :
  announced(Never),
  announcing(Never),
  classContainerP(aClassContainerP),
  DsAddressable(&aClassContainerP->getDeviceContainer())
{
}


Device::~Device()
{
  buttons.clear();
  binaryInputs.clear();
  outputs.clear();
  sensors.clear();
}


bool Device::isPublicDS()
{
  // base class assumes that all devices are public
  return true;
}


void Device::setPrimaryGroup(DsGroup aColorGroup)
{
  primaryGroup = aColorGroup;
}


bool Device::isMember(DsGroup aColorGroup)
{
  return
    aColorGroup==primaryGroup || // is always member of primary group
    ((groupMembership & 0x1ll<<aColorGroup)!=0); // additional membership flag set
}


void Device::setGroupMembership(DsGroup aColorGroup, bool aIsMember)
{
  if (aIsMember) {
    groupMembership |= (0x1ll<<aColorGroup);
  }
  else {
    groupMembership &= ~(0x1ll<<aColorGroup);
  }
}


#pragma mark - Device level vDC API


ErrorPtr Device::handleMethod(const string &aMethod, const string &aJsonRpcId, JsonObjectPtr aParams)
{
  ErrorPtr respErr;
//  if (aMethod=="Gugus") {
//    // Do something
//  }
//  else
  {
    respErr = inherited::handleMethod(aMethod, aJsonRpcId, aParams);
  }
  return respErr;
}



void Device::handleNotification(const string &aMethod, JsonObjectPtr aParams)
{
  ErrorPtr err;
  if (aMethod=="callScene") {
    // call scene
    JsonObjectPtr o;
    if (Error::isOK(err = checkParam(aParams, "scene", o))) {
      SceneNo sceneNo = (SceneNo)o->int32Value();
      bool force = false;
      // check for force flag
      if (Error::isOK(err = checkParam(aParams, "force", o))) {
        force = o->boolValue();
      }
      // now call
      callScene(sceneNo, force);
    }
    if (!Error::isOK(err)) {
      LOG(LOG_WARNING, "callScene error: %s\n", err->description().c_str());
    }
  }
  else if (aMethod=="saveScene") {
    // save scene
    JsonObjectPtr o;
    if (Error::isOK(err = checkParam(aParams, "scene", o))) {
      SceneNo sceneNo = (SceneNo)o->int32Value();
      // now save
      saveScene(sceneNo);
    }
    if (!Error::isOK(err)) {
      LOG(LOG_WARNING, "saveScene error: %s\n", err->description().c_str());
    }
  }
  else {
    inherited::handleNotification(aMethod, aParams);
  }
}


void Device::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  // remove from container management
  DevicePtr dev = classContainerP->getDevicePtrForInstance(this);
  classContainerP->removeDevice(dev, aForgetParams);
  // that's all for the base class
  if (aDisconnectResultHandler)
    aDisconnectResultHandler(dev, true);
}


void Device::hasVanished(bool aForgetParams)
{
  // have device send a vanish message
  sendRequest("vanish", JsonObjectPtr());
  // then disconnect it in software
  // Note that disconnect() might delete the Device object (so 'this' gets invalid)
  disconnect(aForgetParams, NULL);
}



void Device::callScene(SceneNo aSceneNo, bool aForce)
{
  // see if we have a scene table at all
  SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
  if (scenes) {
    // we have a device-wide scene table, get the scene object
    DsScenePtr scene = scenes->getScene(aSceneNo);
    if (scene) {
      // scene found, now apply to all of our outputs
      for (BehaviourVector::iterator pos = outputs.begin(); pos!=outputs.end(); ++pos) {
        OutputBehaviourPtr output = boost::dynamic_pointer_cast<OutputBehaviour>(*pos);
        if (output) {
          output->applyScene(scene);
        }
      }
    }
  }
}


void Device::saveScene(SceneNo aSceneNo)
{
  // see if we have a scene table at all
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
          output->captureScene(scene);
        }
      }
      // save updated scene if any modifications found
      if (scene->isDirty()) {
        scenes->updateScene(scene);
      }
    }
  }
}



#pragma mark - persistent device params


// load device settings - beaviours + scenes
ErrorPtr Device::load()
{
  // load the device settings
  if (deviceSettings) deviceSettings->loadFromStore(dsid.getString().c_str());
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
  if (deviceSettings) deviceSettings->saveToStore(dsid.getString().c_str());
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
  dsProfileVersion_key,
  primaryGroup_key,
  isMember_key,
  progMode_key,
  // the behaviour arrays
  buttonInputDescriptions_key,
  buttonInputSettings_key,
  buttonInputStates_key,
  binaryInputDescriptions_key,
  binaryInputSettings_key,
  binaryInputStates_key,
  outputDescriptions_key,
  outputSettings_key,
  outputStates_key,
  sensorDescriptions_key,
  sensorSettings_key,
  sensorStates_key,
  scenes_key,
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
    { "dsProfileVersion", ptype_int32, false, dsProfileVersion_key, &device_key },
    { "primaryGroup", ptype_int8, false, primaryGroup_key, &device_key },
    { "isMember", ptype_bool, true, isMember_key, &device_key },
    { "progMode", ptype_bool, false, progMode_key, &device_key },
    // the behaviour arrays
    { "buttonInputDescriptions", ptype_object, true, buttonInputDescriptions_key, &device_key },
    { "buttonInputSettings", ptype_object, true, buttonInputSettings_key, &device_key },
    { "buttonInputStates", ptype_object, true, buttonInputStates_key, &device_key },
    { "binaryInputDescriptions", ptype_object, true, binaryInputDescriptions_key, &device_key },
    { "binaryInputSettings", ptype_object, true, binaryInputSettings_key, &device_key },
    { "binaryInputStates", ptype_object, true, binaryInputStates_key, &device_key },
    { "outputDescriptions", ptype_object, true, outputDescriptions_key, &device_key },
    { "outputSettings", ptype_object, true, outputSettings_key, &device_key },
    { "outputStates", ptype_object, true, outputStates_key, &device_key },
    { "sensorDescriptions", ptype_object, true, sensorDescriptions_key, &device_key },
    { "sensorSettings", ptype_object, true, sensorSettings_key, &device_key },
    { "sensorStates", ptype_object, true, sensorStates_key, &device_key },
    // the scenes array
    { "scenes", ptype_object, true, scenes_key, &device_key },
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
      // outputs
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
      case scenes_key: {
        SceneDeviceSettingsPtr scenes = boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings);
        if (scenes) {
          return scenes->getScene(aIndex);
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
      case scenes_key: {
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




bool Device::accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&device_key) {
    if (aIndex==PROP_ARRAY_SIZE && !aForWrite) {
      // array size query
      switch (aPropertyDescriptor.accessKey) {
        // the isMember pseudo-array
        case isMember_key:
          aPropValue = JsonObject::newInt32(64); // max 64 groups
          return true;
        // the behaviour arrays
        case buttonInputDescriptions_key:
        case buttonInputSettings_key:
        case buttonInputStates_key:
          aPropValue = JsonObject::newInt32((int)buttons.size());
          return true;
        case binaryInputDescriptions_key:
        case binaryInputSettings_key:
        case binaryInputStates_key:
          aPropValue = JsonObject::newInt32((int)binaryInputs.size());
          return true;
        case outputDescriptions_key:
        case outputSettings_key:
        case outputStates_key:
          aPropValue = JsonObject::newInt32((int)outputs.size());
          return true;
        case sensorDescriptions_key:
        case sensorSettings_key:
        case sensorStates_key:
          aPropValue = JsonObject::newInt32((int)sensors.size());
          return true;
        case scenes_key:
          if (boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings))
            aPropValue = JsonObject::newInt32(MAX_SCENE_NO);
          else
            aPropValue = JsonObject::newInt32(0); // no scene table
          return true;
      }
    }
    else if (!aForWrite) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        case dsProfileVersion_key:
          aPropValue = JsonObject::newInt32(dsProfileVersion());
          return true;
        case primaryGroup_key:
          aPropValue = JsonObject::newInt32(primaryGroup);
          return true;
        case isMember_key:
          // test group bit
          aPropValue = JsonObject::newBool(isMember((DsGroup)aIndex));
          return true;
        case progMode_key:
          aPropValue = JsonObject::newBool(progMode);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor.accessKey) {
        case isMember_key:
          setGroupMembership((DsGroup)aIndex, aPropValue->boolValue());
          return true;
        case progMode_key:
          progMode = aPropValue->boolValue();
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
}

#pragma mark - Device description/shortDesc


string Device::description()
{
  string s = string_format("Device %s", shortDesc().c_str());
  if (announced!=Never)
    string_format_append(s, " (Announced %lld)", announced);
  else
    s.append(" (not yet announced)");
  s.append("\n");
  if (buttons.size()>0) s.append(" Buttons: %d\n", buttons.size());
  if (binaryInputs.size()>0) s.append(" Binary Inputs: %d\n", binaryInputs.size());
  if (outputs.size()>0) s.append(" Outputs: %d\n", outputs.size());
  if (sensors.size()>0) s.append(" Sensors: %d\n", sensors.size());
  return s;
}
