//
//  Copyright (c) 2014-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#if ENABLE_STATIC

#include "buttonbehaviour.hpp"
#include "lightbehaviour.hpp"


using namespace p44;


#pragma mark - SparkLightScene

SparkLightScene::SparkLightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inherited(aSceneDeviceSettings, aSceneNo)
{
  extendedState = 0;
}


#pragma mark - spark scene values/channels


double SparkLightScene::sceneValue(size_t aChannelIndex)
{
  ChannelBehaviourPtr cb = getDevice().getChannelByIndex(aChannelIndex);
  switch (cb->getChannelType()) {
    case channeltype_sparkmode: return extendedState;
    default: return inherited::sceneValue(aChannelIndex);
  }
  return 0;
}


void SparkLightScene::setSceneValue(size_t aChannelIndex, double aValue)
{
  ChannelBehaviourPtr cb = getDevice().getChannelByIndex(aChannelIndex);
  switch (cb->getChannelType()) {
    case channeltype_sparkmode: setPVar(extendedState, (uint32_t)aValue); break;
    default: inherited::setSceneValue(aChannelIndex, aValue); break;
  }
}


#pragma mark - scene persistence

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
void SparkLightScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the fields
  extendedState = aRow->get<int>(aIndex++);
}


/// bind values to passed statement
void SparkLightScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, (int)extendedState);
}



#pragma mark - default scene values


void SparkLightScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  // init default brightness
  inherited::setDefaultSceneValues(aSceneNo);
  // default to mode 3 = RGB lamp
  extendedState = 3; // mode 3 = lamp
  markClean(); // default values are always clean
}





#pragma mark - SparkLightBehaviour


SparkLightBehaviour::SparkLightBehaviour(Device &aDevice) :
  inherited(aDevice)
{
  // add special spark mode channel
  sparkmode = ChannelBehaviourPtr(new SparkModeChannel(*this));
  addChannel(sparkmode);
}


void SparkLightBehaviour::loadChannelsFromScene(DsScenePtr aScene)
{
  // now load spark specific scene information
  SparkLightScenePtr sparkLightScene = boost::dynamic_pointer_cast<SparkLightScene>(aScene);
  if (sparkLightScene) {
    sparkmode->setChannelValueIfNotDontCare(sparkLightScene, sparkLightScene->extendedState, 0, 0, true);
  }
  // load basic light scene info
  inherited::loadChannelsFromScene(aScene);
}


void SparkLightBehaviour::saveChannelsToScene(DsScenePtr aScene)
{
  // save basic light scene info
  inherited::saveChannelsToScene(aScene);
  // now save color specific scene information
  SparkLightScenePtr sparkLightScene = boost::dynamic_pointer_cast<SparkLightScene>(aScene);
  if (sparkLightScene) {
    sparkLightScene->setPVar(sparkLightScene->extendedState, (uint32_t)sparkmode->getChannelValue());
    sparkLightScene->setSceneValueFlags(sparkmode->getChannelIndex(), valueflags_dontCare, false);
  }
}


string SparkLightBehaviour::shortDesc()
{
  return string("SparkRGBLight");
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
  StaticDevice((DeviceClassContainer *)aClassContainerP),
  sparkCloudComm(MainLoop::currentMainLoop()),
  apiVersion(0)
{
  // config must be: sparkCoreId:accessToken
  size_t i = aDeviceConfig.find(":");
  if (i!=string::npos) {
    sparkCoreID = aDeviceConfig.substr(0,i);
    sparkCoreToken = aDeviceConfig.substr(i+1,string::npos);
  }
  // Simulate light device
  // - defaults to yellow (light)
  primaryGroup = group_yellow_light;
  // - use Spark settings, which include a scene table with extendedState for mode
  installSettings(DeviceSettingsPtr(new SparkDeviceSettings(*this)));
  // set the behaviour
  SparkLightBehaviourPtr sl = SparkLightBehaviourPtr(new SparkLightBehaviour(*this));
  sl->setHardwareOutputConfig(outputFunction_colordimmer, outputmode_gradual, usage_undefined, true, 70); // spark light can draw 70 Watts with 240 WS2812 connected
  sl->setHardwareName("SparkCore based RGB light");
  sl->initMinBrightness(1); // min brightness is 1
  addBehaviour(sl);
  // dsuid
	deriveDsUid();
}



bool SparkIoDevice::sparkApiCall(JsonWebClientCB aResponseCB, string aArgs)
{
  string url = string_format("https://api.spark.io/v1/devices/%s/vdsd", sparkCoreID.c_str());
  string data;
  HttpComm::appendFormValue(data, "access_token", sparkCoreToken);
  HttpComm::appendFormValue(data, "args", aArgs);
  LOG(LOG_DEBUG, "sparkApiCall to %s - data = %s", url.c_str(), data.c_str());
  return sparkCloudComm.jsonReturningRequest(url.c_str(), aResponseCB, "POST", data);
}



void SparkIoDevice::initializeDevice(StatusCB aCompletedCB, bool aFactoryReset)
{
  // get vdsd API version
  if (!sparkApiCall(boost::bind(&SparkIoDevice::apiVersionReceived, this, aCompletedCB, aFactoryReset, _1, _2), "version")) {
    // could not even issue request, init complete
    inherited::initializeDevice(aCompletedCB, aFactoryReset);
  }
}


void SparkIoDevice::apiVersionReceived(StatusCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  if (Error::isOK(aError) && aJsonResponse) {
    if (apiVersion==0) {
      // only set if unknown so far to avoid other out-of-sequence responses from cloud to change the API version
      JsonObjectPtr o = aJsonResponse->get("return_value");
      if (o) {
        apiVersion = o->int32Value();
      }
      o = aJsonResponse->get("name");
      if (o) {
        initializeName(o->stringValue());
      }
    }
  }
  // anyway, consider initialized
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}


void SparkIoDevice::checkPresence(PresenceCB aPresenceResultHandler)
{
  if (applyInProgress) {
    // cannot query now, update in progress, assume still present
    aPresenceResultHandler(true);
    return;
  }
  // query the device
  sparkApiCall(boost::bind(&SparkIoDevice::presenceStateReceived, this, aPresenceResultHandler, _1, _2), "version");
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



void SparkIoDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  SparkLightBehaviourPtr sl = boost::dynamic_pointer_cast<SparkLightBehaviour>(output);
  if (sl) {
    if (!needsToApplyChannels()) {
      // NOP for this call
      channelValuesSent(sl, aDoneCB, JsonObjectPtr(), ErrorPtr());
      return;
    }
    // needs update
    // - derive (possibly new) color mode from changed channels
    sl->deriveColorMode();
    // - process according to spark mode
    uint32_t stateWord;
    uint8_t mode = sl->sparkmode->getTransitionalValue();
    if (mode==3) {
      // RGB lamp
      double r,g,b;
      sl->getRGB(r, g, b, 255); // Spark RGB values are scaled 0..255
      stateWord =
        (mode << 24) |
        ((int)r << 16) |
        ((int)g << 8) |
        (int)b;
      LOG(LOG_DEBUG, "Spark vdsd: Update state to mode=%d, RGB=%d,%d,%d, stateWord=0x%08X / %d", mode, (int)r, (int)g, (int)b, stateWord, stateWord);
    }
    else {
      // brightness only
      double br = sl->brightness->getTransitionalValue()*255/sl->brightness->getMax();
      stateWord =
        (mode << 24) |
        ((int)br & 0xFF);
      LOG(LOG_DEBUG, "Spark vdsd: Update state to mode=%d, Brightness=%d, stateWord=0x%08X / %d", mode, (int)br, stateWord, stateWord);
    }
    // set output value
    if (apiVersion==2) {
      string args = string_format("state=%u", stateWord);
      // posting might fail if done too early
      if (!sparkApiCall(boost::bind(&SparkIoDevice::channelValuesSent, this, sl, aDoneCB, _1, _2), args)) {
        // retry after a while
        MainLoop::currentMainLoop().executeOnce(boost::bind(&SparkIoDevice::applyChannelValues, this, aDoneCB, aForDimming), 1*Second);
      }
    }
    else {
      // error, wrong API
      channelValuesSent(sl, aDoneCB, JsonObjectPtr(), ErrorPtr(new WebError(415)));
    }
  }
}


void SparkIoDevice::channelValuesSent(SparkLightBehaviourPtr aSparkLightBehaviour, SimpleCB aDoneCB, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    aSparkLightBehaviour->appliedColorValues();
  }
  else {
    LOG(LOG_DEBUG, "Spark API error: %s", aError->description().c_str());
  }
  // confirm done
  if (aDoneCB) aDoneCB();
}



void SparkIoDevice::syncChannelValues(SimpleCB aDoneCB)
{
  // query light attributes and state
  sparkApiCall(boost::bind(&SparkIoDevice::channelValuesReceived, this, aDoneCB, _1, _2), "state");
}



void SparkIoDevice::channelValuesReceived(SimpleCB aDoneCB, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  if (Error::isOK(aError) && aJsonResponse) {
    JsonObjectPtr o = aJsonResponse->get("return_value");
    SparkLightBehaviourPtr sl = boost::dynamic_pointer_cast<SparkLightBehaviour>(output);
    if (o && sl) {
      uint32_t state = o->int32Value();
      uint8_t mode = (state>>24) & 0xFF;
      sl->sparkmode->syncChannelValue(mode);
      if (mode==3) {
        // RGB lamp
        double r = (state>>16) & 0xFF;
        double g = (state>>8) & 0xFF;
        double b = state & 0xFF;
        sl->setRGB(r, g, b, 255);
      }
      else {
        // brightness only
        sl->brightness->syncChannelValue(state & 0xFF);
      }
    }
  }
  // done
  inherited::syncChannelValues(aDoneCB);
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
  string_format_append(s, "\n- MessageTorch RGB light controlled via spark cloud API");
  return s;
}


#endif // ENABLE_STATIC
