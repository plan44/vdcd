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

#include "huedevice.hpp"
#include "huedevicecontainer.hpp"

#include "fnv.hpp"


using namespace p44;


// hue API conversion factors


// - hue: brightness: Brightness of the light. This is a scale from the minimum brightness the light is capable of, 0,
//        to the maximum capable brightness, 255. Note a brightness of 0 is not off.
// - dS: brightness: 0..100
#define HUEAPI_OFFSET_BRIGHTNESS 0.4 // hue brightness starts at 0 (lowest value, but not off), corresonds with one dS step = 0.4
#define HUEAPI_FACTOR_BRIGHTNESS (255.0/(100-HUEAPI_OFFSET_BRIGHTNESS)) // dS has 99.6 (254/255) not-off brightness steps (0.4..100), 0 is reserved for off

// - hue: hue: Wrapping value between 0 and 65535. Both 0 and 65535 are red, 25500 is green and 46920 is blue.
// - dS: hue: 0..358.6 degrees
#define HUEAPI_FACTOR_HUE (65535.0/360)

// - hue: Saturation: 255 is the most saturated (colored) and 0 is the least saturated (white)
// - dS: 0..100%
#define HUEAPI_FACTOR_SATURATION (255.0/100)

// - hue: color temperature: 153..500 mired for 2012's hue bulbs
// - dS: color temperature: 100..10000 mired

// - CIE x,y: hue and dS use 0..1 for x and y



#pragma mark - HueDevice


HueDevice::HueDevice(HueDeviceContainer *aClassContainerP, const string &aLightID, bool aIsColor) :
  inherited(aClassContainerP),
  lightID(aLightID),
  pendingApplyCB(NULL),
  repeatApplyAtEnd(false)
{
  // hue devices are lights
  setPrimaryGroup(group_yellow_light);
  if (aIsColor) {
    // color lamp
    // - use color light settings, which include a color scene table
    installSettings(DeviceSettingsPtr(new ColorLightDeviceSettings(*this)));
    // - set the behaviour
    ColorLightBehaviourPtr cl = ColorLightBehaviourPtr(new ColorLightBehaviour(*this));
    cl->setHardwareOutputConfig(outputFunction_colordimmer, usage_undefined, true, 8.5); // hue lights are always dimmable, one hue = 8.5W
    cl->setHardwareName(string_format("color light #%s", lightID.c_str()));
    cl->initMinBrightness(0.4); // min brightness is roughly 1/256
    addBehaviour(cl);
  }
  else {
    // dimmable lamp
    // - use normal light settings
    installSettings(DeviceSettingsPtr(new LightDeviceSettings(*this)));
    // - set the behaviour
    LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
    l->setHardwareOutputConfig(outputFunction_dimmer, usage_undefined, true, 8.5); // hue lights are always dimmable, one hue = 8.5W
    l->setHardwareName(string_format("monochrome light #%s", lightID.c_str()));
    l->initMinBrightness(0.4); // min brightness is roughly 1/256
    addBehaviour(l);
  }
  // derive the dSUID
  deriveDsUid();
}


string HueDevice::getExtraInfo()
{
  return string_format("Light #%s", lightID.c_str());
}



HueDeviceContainer &HueDevice::hueDeviceContainer()
{
  return *(static_cast<HueDeviceContainer *>(classContainerP));
}


HueComm &HueDevice::hueComm()
{
  return (static_cast<HueDeviceContainer *>(classContainerP))->hueComm;
}



void HueDevice::setName(const string &aName)
{
  string oldname = getName();
  inherited::setName(aName);
  if (getName()!=oldname) {
    // really changed, propagate to hue
    JsonObjectPtr params = JsonObject::newObj();
    params->add("name", JsonObject::newString(getName()));
    string url = string_format("/lights/%s", lightID.c_str());
    hueComm().apiAction(httpMethodPUT, url.c_str(), params, NULL);
  }
}



void HueDevice::initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset)
{
  // query light attributes and state
  string url = string_format("/lights/%s", lightID.c_str());
  hueComm().apiQuery(url.c_str(), boost::bind(&HueDevice::deviceStateReceived, this, aCompletedCB, aFactoryReset, _1, _2));
}


// TODO: once hue bridge 1.3 is common, this information could be read from the collection result
void HueDevice::deviceStateReceived(CompletedCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aDeviceInfo, ErrorPtr aError)
{
  if (Error::isOK(aError) && aDeviceInfo) {
    JsonObjectPtr o;
    // get model name from device (note: with 1.3 bridge and later this could be read at collection, but pre-1.3 needs this separate call)
    hueModel.clear();
    o = aDeviceInfo->get("type");
    if (o) {
      hueModel = o->stringValue();
    }
    o = aDeviceInfo->get("modelid");
    if (o) {
      hueModel += ": " + o->stringValue();
    }
    // now look at state
    JsonObjectPtr state = aDeviceInfo->get("state");
    Brightness bri = 0;
    if (state) {
      // get current brightness
      o = state->get("on");
      if (o && o->boolValue()) {
        // lamp is on
        bri = 100; // default to full brightness
        o = state->get("bri");
        if (o) {
          bri = o->int32Value()/HUEAPI_FACTOR_BRIGHTNESS+HUEAPI_OFFSET_BRIGHTNESS;
        }
        // set current brightness
        output->getChannelByType(channeltype_brightness)->syncChannelValue(bri);
      }
    }
  }
  // let superclass initialize as well
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}


bool HueDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (output && getIcon(output->getOutputFunction()==outputFunction_colordimmer ? "hue" : "hue_lux", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}




void HueDevice::checkPresence(PresenceCB aPresenceResultHandler)
{
  // query the device
  string url = string_format("/lights/%s", lightID.c_str());
  hueComm().apiQuery(url.c_str(), boost::bind(&HueDevice::presenceStateReceived, this, aPresenceResultHandler, _1, _2));
}



void HueDevice::presenceStateReceived(PresenceCB aPresenceResultHandler, JsonObjectPtr aDeviceInfo, ErrorPtr aError)
{
  bool reachable = false;
  if (Error::isOK(aError) && aDeviceInfo) {
    JsonObjectPtr state = aDeviceInfo->get("state");
    if (state) {
      // Note: 2012 hue bridge firmware always returns 1 for this.
      JsonObjectPtr o = state->get("reachable");
      reachable = o && o->boolValue();
    }
  }
  aPresenceResultHandler(reachable);
}



void HueDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  checkPresence(boost::bind(&HueDevice::disconnectableHandler, this, aForgetParams, aDisconnectResultHandler, _1));
}


void HueDevice::disconnectableHandler(bool aForgetParams, DisconnectCB aDisconnectResultHandler, bool aPresent)
{
  if (!aPresent) {
    // call inherited disconnect
    inherited::disconnect(aForgetParams, aDisconnectResultHandler);
  }
  else {
    // not disconnectable
    if (aDisconnectResultHandler) {
      aDisconnectResultHandler(false);
    }
  }
}



void HueDevice::applyChannelValues(DoneCB aDoneCB, bool aForDimming)
{
  // Update of light state needed
  LightBehaviourPtr l = boost::dynamic_pointer_cast<LightBehaviour>(output);
  if (l) {
    if (!needsToApplyChannels()) {
      // NOP for this call
      channelValuesSent(l, aDoneCB, JsonObjectPtr(), ErrorPtr());
      return;
    }
    ColorLightBehaviourPtr cl = boost::dynamic_pointer_cast<ColorLightBehaviour>(l);
    MLMicroSeconds transitionTime = 0; // undefined so far
    // build hue API light state
    string url = string_format("/lights/%s/state", lightID.c_str());
    JsonObjectPtr newState = JsonObject::newObj();
    // brightness is always re-applied unless it's dimming
    if (!aForDimming || l->brightness->needsApplying()) {
      Brightness b = l->brightnessForHardware();
      transitionTime = l->transitionTimeToNewBrightness();
      if (b==0) {
        // light off, no other parameters
        newState->add("on", JsonObject::newBool(false));
      }
      else {
        // light on
        newState->add("on", JsonObject::newBool(true));
        newState->add("bri", JsonObject::newInt32((b-HUEAPI_OFFSET_BRIGHTNESS)*HUEAPI_FACTOR_BRIGHTNESS+0.5)); // 1..100 -> 0..255
      }
      l->brightness->channelValueApplied(true); // confirm early, as subsequent request might set new value again
    }
    // for color lights, also check color
    if (cl) {
      // derive (possibly new) color mode from changed channels
      cl->deriveColorMode();
      // add color in case it was set (by scene call)
      switch (cl->colorMode) {
        case colorLightModeHueSaturation: {
          // for dimming, only actually changed component (hue or saturation)
          if (!aForDimming || cl->hue->needsApplying()) {
            if (transitionTime==0) transitionTime = cl->hue->transitionTimeToNewValue();
            newState->add("hue", JsonObject::newInt32(cl->hue->getChannelValue()*HUEAPI_FACTOR_HUE+0.5));
            cl->hue->channelValueApplied(true); // confirm early, as subsequent request might set new value again
          }
          if (!aForDimming || cl->saturation->needsApplying()) {
            if (transitionTime==0) transitionTime = cl->saturation->transitionTimeToNewValue();
            newState->add("sat", JsonObject::newInt32(cl->saturation->getChannelValue()*HUEAPI_FACTOR_SATURATION+0.5));
            cl->saturation->channelValueApplied(true); // confirm early, as subsequent request might set new value again
          }
          break;
        }
        case colorLightModeXY: {
          // x,y are always applied together
          if (cl->cieX->needsApplying() || cl->cieY->needsApplying()) {
            if (transitionTime==0) transitionTime = cl->cieX->transitionTimeToNewValue();
            if (transitionTime==0) transitionTime = cl->cieY->transitionTimeToNewValue();
            JsonObjectPtr xyArr = JsonObject::newArray();
            xyArr->arrayAppend(JsonObject::newDouble(cl->cieX->getChannelValue()));
            xyArr->arrayAppend(JsonObject::newDouble(cl->cieY->getChannelValue()));
            newState->add("xy", xyArr);
            cl->cieX->channelValueApplied(true); // confirm early, as subsequent request might set new value again
            cl->cieY->channelValueApplied(true); // confirm early, as subsequent request might set new value again
          }
          break;
        }
        case colorLightModeCt: {
          if (cl->ct->needsApplying()) {
            if (transitionTime==0) transitionTime = cl->ct->transitionTimeToNewValue();
            newState->add("ct", JsonObject::newInt32(cl->ct->getChannelValue()));
            cl->ct->channelValueApplied(true); // confirm early, as subsequent request might set new value again
          }
          break;
        }
        default:
          break;
      }
    }
    if (!aForDimming) {
      LOG(LOG_INFO, "hue device %s: sending new light state: brightness = %0.0f\n", shortDesc().c_str(), l->brightness->getChannelValue());
      if (cl) {
        LOG(LOG_INFO, "- colorMode=%d\n", cl->colorMode);
      }
    }
    // use transition time from (1/10 = 100mS second resolution)
    newState->add("transitiontime", JsonObject::newInt64(transitionTime/(100*MilliSecond)));
    // TODO: use result to sync channel value
    hueComm().apiAction(httpMethodPUT, url.c_str(), newState, boost::bind(&HueDevice::channelValuesSent, this, l, aDoneCB, _1, _2));
  }
}



void HueDevice::channelValuesSent(LightBehaviourPtr aLightBehaviour, DoneCB aDoneCB, JsonObjectPtr aResult, ErrorPtr aError)
{
  // synchronize actual channel values as hue delivers them back
  if (aResult) {
    ColorLightBehaviourPtr cl = boost::dynamic_pointer_cast<ColorLightBehaviour>(aLightBehaviour);
    // [{"success":{"\/lights\/1\/state\/transitiontime":1}},{"success":{"\/lights\/1\/state\/on":true}},{"success":{"\/lights\/1\/state\/hue":0}},{"success":{"\/lights\/1\/state\/sat":255}},{"success":{"\/lights\/1\/state\/bri":255}}]
    for (int i=0; i<aResult->arrayLength(); i++) {
      JsonObjectPtr staObj = HueComm::getSuccessItem(aResult, i);
      if (staObj) {
        // dispatch results
        staObj->resetKeyIteration();
        string key;
        JsonObjectPtr val;
        bool blockBrightness = false;
        if (staObj->nextKeyValue(key, val)) {
          // match path
          string param = key.substr(key.find_last_of('/')+1);
          if (cl && param=="hue") {
            cl->hue->syncChannelValue(val->int32Value()/HUEAPI_FACTOR_HUE);
          }
          else if (cl && param=="sat") {
            cl->saturation->syncChannelValue(val->int32Value()/HUEAPI_FACTOR_SATURATION);
          }
          else if (cl && param=="xy") {
            JsonObjectPtr e = val->arrayGet(0);
            if (e) cl->cieX->syncChannelValue(e->doubleValue());
            e = val->arrayGet(1);
            if (e) cl->cieY->syncChannelValue(e->doubleValue());
          }
          else if (cl && param=="ct") {
            cl->ct->syncChannelValue(val->int32Value());
          }
          else if (param=="on") {
            if (!val->boolValue()) {
              aLightBehaviour->syncBrightnessFromHardware(0);
              blockBrightness = true; // prevent syncing brightness
            }
          }
          else if (param=="bri" && !blockBrightness) {
            aLightBehaviour->syncBrightnessFromHardware(val->int32Value()/HUEAPI_FACTOR_BRIGHTNESS+HUEAPI_OFFSET_BRIGHTNESS);
          }
        } // status data key/val
      } // status item found
    } // all success items
  }
  // confirm done
  if (aDoneCB) aDoneCB();
}



void HueDevice::syncChannelValues(DoneCB aDoneCB)
{
  // query light attributes and state
  string url = string_format("/lights/%s", lightID.c_str());
  hueComm().apiQuery(url.c_str(), boost::bind(&HueDevice::channelValuesReceived, this, aDoneCB, _1, _2));
}



void HueDevice::channelValuesReceived(DoneCB aDoneCB, JsonObjectPtr aDeviceInfo, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // assign the channel values
    JsonObjectPtr o;
    // get current color settings
    JsonObjectPtr state = aDeviceInfo->get("state");
    if (state) {
      LightBehaviourPtr l = boost::dynamic_pointer_cast<LightBehaviour>(output);
      if (l) {
        // on with brightness or off
        o = state->get("on");
        if (o && o->boolValue()) {
          // lamp is on, get brightness
          o = state->get("bri");
          if (o) l->syncBrightnessFromHardware(o->int32Value()/HUEAPI_FACTOR_BRIGHTNESS+HUEAPI_OFFSET_BRIGHTNESS); // 0..255 -> 0.4..100
        }
        else {
          l->syncBrightnessFromHardware(0); // off
        }
        ColorLightBehaviourPtr cl = boost::dynamic_pointer_cast<ColorLightBehaviour>(l);
        if (cl) {
          // color information
          o = state->get("colormode");
          if (o) {
            string mode = o->stringValue();
            if (mode=="hs") {
              cl->colorMode = colorLightModeHueSaturation;
              o = state->get("hue");
              if (o) cl->hue->syncChannelValue(o->int32Value()/HUEAPI_FACTOR_HUE);
              o = state->get("sat");
              if (o) cl->saturation->syncChannelValue(o->int32Value()/HUEAPI_FACTOR_SATURATION);
            }
            else if (mode=="xy") {
              cl->colorMode = colorLightModeXY;
              o = state->get("xy");
              if (o) {
                JsonObjectPtr e = o->arrayGet(0);
                if (e) cl->cieX->syncChannelValue(e->doubleValue());
                e = o->arrayGet(1);
                if (e) cl->cieY->syncChannelValue(e->doubleValue());
              }
            }
            else if (mode=="ct") {
              cl->colorMode = colorLightModeCt;
              o = state->get("ct");
              if (o) cl->ct->syncChannelValue(o->int32Value());
            }
            else {
              cl->colorMode = colorLightModeNone;
            }
          }
        } // color
      } // light
    } // state
  } // no error
  // done
  if (aDoneCB) aDoneCB();
}



void HueDevice::deriveDsUid()
{
  // NOTE: lightID is not exactly a stable ID. But the hue API does not provide anything better at this time
  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::bridgeUUID:huelightid
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  s += "::" + hueDeviceContainer().bridgeUuid;
  s += ":" + lightID;
  dSUID.setNameInSpace(s, vdcNamespace);
}


string HueDevice::description()
{
  string s = inherited::description();
  return s;
}
