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

#include "sparkiodevice.hpp"


#include "fnv.hpp"

#include "buttonbehaviour.hpp"
#include "lightbehaviour.hpp"


using namespace p44;


#pragma mark - SparkLightScene

SparkLightScene::SparkLightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inherited(aSceneDeviceSettings, aSceneNo)
{
  extendedState = 0;
}


#pragma mark - HueLight Scene persistence

const char *SparkLightScene::tableName()
{
  return "SparkLightScenes";
}

// data field definitions

static const size_t numSparkSceneFields = 1;

size_t SparkLightScene::numFieldDefs()
{
  return inherited::numFieldDefs()+numSparkSceneFields;
}


const FieldDefinition *SparkLightScene::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numSparkSceneFields] = {
    { "extendedState", SQLITE_INTEGER }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numSparkSceneFields)
    return &dataDefs[aIndex];
  return NULL;
}



/// load values from passed row
void SparkLightScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  extendedState = aRow->get<int>(aIndex++);
}


/// bind values to passed statement
void SparkLightScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // bind the fields
  aStatement.bind(aIndex++, (int)extendedState);
}


#pragma mark - Light scene property access


static char sparklightscene_key;

enum {
  extendedState_key,
  numSparkLightSceneProperties
};


int SparkLightScene::numProps(int aDomain)
{
  return inherited::numProps(aDomain)+numSparkLightSceneProperties;
}


const PropertyDescriptor *SparkLightScene::getPropertyDescriptor(int aPropIndex, int aDomain)
{
  static const PropertyDescriptor properties[numSparkLightSceneProperties] = {
    { "x-p44-extendedState", apivalue_uint64, false, extendedState_key, &sparklightscene_key }
  };
  int n = inherited::numProps(aDomain);
  if (aPropIndex<n)
    return inherited::getPropertyDescriptor(aPropIndex, aDomain); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return &properties[aPropIndex];
}


bool SparkLightScene::accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&sparklightscene_key) {
    if (!aForWrite) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        case extendedState_key:
          aPropValue->setUint32Value(extendedState);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor.accessKey) {
        case extendedState_key:
          extendedState = aPropValue->uint32Value();
          markDirty();
          return true;
      }
    }
  }
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
}


#pragma mark - default scene values


void SparkLightScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  // init default brightness
  inherited::setDefaultSceneValues(aSceneNo);
  // default to no extended state
  extendedState = 0;
}





#pragma mark - SparkLightBehaviour


SparkLightBehaviour::SparkLightBehaviour(Device &aDevice) :
  LightBehaviour(aDevice)
{
}


void SparkLightBehaviour::recallScene(LightScenePtr aLightScene)
{
  SparkLightScenePtr sparkScene = boost::dynamic_pointer_cast<SparkLightScene>(aLightScene);
  if (sparkScene) {
    // prepare next color values in device
    SparkIoDevice *devP = dynamic_cast<SparkIoDevice *>(&device);
    if (devP) {
      devP->pendingSparkScene = sparkScene;
      outputUpdatePending = true; // we need an output update, even if main output value (brightness) has not changed in new scene
    }
  }
  // let base class update logical brightness, which will in turn update the output, which will then
  // catch the colors from pendingColorScene
  inherited::recallScene(aLightScene);
}



// capture scene
void SparkLightBehaviour::captureScene(DsScenePtr aScene, DoneCB aDoneCB)
{
  SparkLightScenePtr sparkScene = boost::dynamic_pointer_cast<SparkLightScene>(aScene);
  if (sparkScene) {
    // query light attributes and state
    SparkIoDevice *devP = dynamic_cast<SparkIoDevice *>(&device);
    if (devP) {
      devP->sparkApiCall(boost::bind(&SparkLightBehaviour::sceneStateReceived, this, sparkScene, aDoneCB, _2, _3), "state0");
    }
  }
}


void SparkLightBehaviour::sceneStateReceived(SparkLightScenePtr aSparkScene, DoneCB aDoneCB, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  if (Error::isOK(aError) && aJsonResponse) {
    JsonObjectPtr o = aJsonResponse->get("return_value");
    if (o) {
      uint32_t state = o->int32Value();
      // state is brightness + extended state: 0xssssssbb, ssssss=extended state, bb=brightness
      Brightness bri = state & 0xFF;
      uint32_t extendedState = (state & 0xFFFFFF)>>8;
      initOutputValue(bri); // update basic light behaviour brightness state
      aSparkScene->extendedState = extendedState; // capture extended state in scene
      aSparkScene->markDirty();
    }
  }
  // anyway, let base class capture brightness
  inherited::captureScene(aSparkScene, aDoneCB);
}



#pragma mark - SparkDeviceSettings

SparkDeviceSettings::SparkDeviceSettings(Device &aDevice) :
  inherited(aDevice)
{
};


DsScenePtr SparkDeviceSettings::newDefaultScene(SceneNo aSceneNo)
{
  SparkLightScenePtr lightScene = SparkLightScenePtr(new SparkLightScene(*this, aSceneNo));
  lightScene->setDefaultSceneValues(aSceneNo);
  // return it
  return lightScene;
}



#pragma mark - SparkIoDevice

SparkIoDevice::SparkIoDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  Device((DeviceClassContainer *)aClassContainerP),
  sparkCloudComm(SyncIOMainLoop::currentMainLoop()),
  apiVersion(0),
  outputValue(0),
  outputChangePending(false)
{
  // config must be: sparkCoreId:accessToken
  size_t i = aDeviceConfig.find_first_of(':');
  if (i!=string::npos) {
    sparkCoreID = aDeviceConfig.substr(0,i);
    sparkCoreToken = aDeviceConfig.substr(i+1,string::npos);
  }
  // Simulate light device
  // - defaults to yellow (light)
  primaryGroup = group_yellow_light;
  // - use light settings, which include a scene table
  deviceSettings = DeviceSettingsPtr(new SparkDeviceSettings(*this));
  // - create one output
  SparkLightBehaviourPtr l = SparkLightBehaviourPtr(new SparkLightBehaviour(*this));
  l->setHardwareOutputConfig(outputFunction_dimmer, channeltype_brightness, usage_undefined, true, -1);
  l->setHardwareName("spark core output");
  addBehaviour(l);
  // dsuid
	deriveDsUid();
}



bool SparkIoDevice::sparkApiCall(JsonWebClientCB aResponseCB, string aArgs)
{
  string url = string_format("https://api.spark.io/v1/devices/%s/vdsd", sparkCoreID.c_str());
  string data;
  HttpComm::appendFormValue(data, "access_token", sparkCoreToken);
  HttpComm::appendFormValue(data, "args", aArgs);
  return sparkCloudComm.jsonReturningRequest(url.c_str(), aResponseCB, "POST", data);
}



void SparkIoDevice::initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset)
{
  // get vdsd API version
  if (!sparkApiCall(boost::bind(&SparkIoDevice::apiVersionReceived, this, aCompletedCB, aFactoryReset, _2, _3), "version")) {
    // could not even issue request, init complete
    inherited::initializeDevice(aCompletedCB, aFactoryReset);
  }
}


void SparkIoDevice::apiVersionReceived(CompletedCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  if (Error::isOK(aError) && aJsonResponse) {
    JsonObjectPtr o = aJsonResponse->get("return_value");
    if (o) {
      apiVersion = o->int32Value();
    }
    o = aJsonResponse->get("name");
    if (o) {
      initializeName(o->stringValue());
    }
  }
  // anyway, consider initialized
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}


void SparkIoDevice::checkPresence(PresenceCB aPresenceResultHandler)
{
  // query the device
  sparkApiCall(boost::bind(&SparkIoDevice::presenceStateReceived, this, aPresenceResultHandler, _2, _3), "version");
}



void SparkIoDevice::presenceStateReceived(PresenceCB aPresenceResultHandler, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  bool reachable = false;
  if (Error::isOK(aError) && aJsonResponse) {
    JsonObjectPtr connected = aJsonResponse->get("connected");
    if (connected)
      reachable = connected->boolValue();
  }
  aPresenceResultHandler(reachable);
}




void SparkIoDevice::updateOutputValue(OutputBehaviour &aOutputBehaviour)
{
  if (aOutputBehaviour.getIndex()==0) {
    outputValue = aOutputBehaviour.valueForHardware();
    // set output value
    postOutputValue(aOutputBehaviour);
  }
  else
    return inherited::updateOutputValue(aOutputBehaviour); // let superclass handle this
}


void SparkIoDevice::postOutputValue(OutputBehaviour &aOutputBehaviour)
{
  if (apiVersion==1) {
    string args;
    if (pendingSparkScene && pendingSparkScene->extendedState!=0) {
      args = string_format("state0=%d", (outputValue & 0xFF) | (pendingSparkScene->extendedState<<8));
    }
    else {
      // brightness only
      args = string_format("output0=%d", outputValue);
    }
    // posting might fail if done too early
    if (!sparkApiCall(boost::bind(&SparkIoDevice::outputChanged, this, aOutputBehaviour, _2, _3), args)) {
      outputChangePending = true; // retry when previous request done
    }
  }
}



void SparkIoDevice::outputChanged(OutputBehaviour &aOutputBehaviour, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (outputChangePending) {
      outputChangePending = false;
      postOutputValue(aOutputBehaviour); // one more change pending
    }
    else {
      aOutputBehaviour.outputValueApplied(); // confirm having applied the value
      outputChangePending = false;
    }
  }
}



void SparkIoDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::SparkCoreID
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  s += "::" + sparkCoreID;
  dSUID.setNameInSpace(s, vdcNamespace);
}



string SparkIoDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "- has output controlling brightness via spark cloud API\n");
  return s;
}
