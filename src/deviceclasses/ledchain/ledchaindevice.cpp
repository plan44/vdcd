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

#include "ledchaindevice.hpp"

#if !DISABLE_LEDCHAIN


#include "lightbehaviour.hpp"
#include "colorlightbehaviour.hpp"


using namespace p44;


#pragma mark - LedChainDevice


LedChainDevice::LedChainDevice(LedChainDeviceContainer *aClassContainerP, uint16_t aFirstLED, uint16_t aNumLEDs, const string &aDeviceConfig) :
  inherited(aClassContainerP),
  firstLED(aFirstLED),
  numLEDs(aNumLEDs),
  transitionTicket(0),
  startSoftEdge(0),
  endSoftEdge(0),
  r(0), g(0), b(0)
{
  // type:config_for_type
  // Where:
  //  with type=segment
  //  config=b:e
  //   b:0..n size of softedge at beginning
  //   e:0..n size of softedge at end
  // evaluate config
  string config = aDeviceConfig;
  string mode, s;
  size_t i = config.find(":");
  ledchainType = ledchain_unknown;
  bool configOK = false;
  if (i!=string::npos) {
    mode = config.substr(0,i);
    config.erase(0,i+1);
  }
  if (mode=="segment") {
    ledchainType = ledchain_softsegment;
    i = config.find(":");
    if (i!=string::npos) {
      s = config.substr(0,i);
      config.erase(0,i+1);
      if (sscanf(s.c_str(), "%hd", &startSoftEdge)==1) {
        if (sscanf(config.c_str(), "%hd", &endSoftEdge)==1) {
          // complete config
          if (startSoftEdge+endSoftEdge<=numLEDs) {
            // correct config
            configOK = true;
          }
        }
      }
    }
  }
  if (!configOK) {
    LOG(LOG_ERR, "invalid LedChain device config: %s", aDeviceConfig.c_str());
  }
  // - is RGB
  primaryGroup = group_yellow_light;
  // just color light settings, which include a color scene table
  installSettings(DeviceSettingsPtr(new ColorLightDeviceSettings(*this)));
  // - add multi-channel color light behaviour (which adds a number of auxiliary channels)
  RGBColorLightBehaviourPtr l = RGBColorLightBehaviourPtr(new RGBColorLightBehaviour(*this));
  addBehaviour(l);
  // - create dSUID
  deriveDsUid();
}



bool LedChainDevice::isSoftwareDisconnectable()
{
  return true; // these are always software disconnectable
}

LedChainDeviceContainer &LedChainDevice::getLedChainDeviceContainer()
{
  return *(static_cast<LedChainDeviceContainer *>(classContainerP));
}


void LedChainDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  // clear learn-in data from DB
  if (ledChainDeviceRowID) {
    getLedChainDeviceContainer().db.executef("DELETE FROM devConfigs WHERE rowid=%d", ledChainDeviceRowID);
  }
  // disconnection is immediate, so we can call inherited right now
  inherited::disconnect(aForgetParams, aDisconnectResultHandler);
}


#define TRANSITION_STEP_TIME (10*MilliSecond)

void LedChainDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  MLMicroSeconds transitionTime = 0;
  // abort previous transition
  MainLoop::currentMainLoop().cancelExecutionTicket(transitionTicket);
  // full color device
  RGBColorLightBehaviourPtr cl = boost::dynamic_pointer_cast<RGBColorLightBehaviour>(output);
  if (cl) {
    if (needsToApplyChannels()) {
      // needs update
      // - derive (possibly new) color mode from changed channels
      cl->deriveColorMode();
      // - calculate and start transition
      //   TODO: depending to what channel has changed, take transition time from that channel. For now always using brightness transition time
      transitionTime = cl->transitionTimeToNewBrightness();
      cl->colorTransitionStep(); // init
      applyChannelValueSteps(aForDimming, transitionTime==0 ? 1 : (double)TRANSITION_STEP_TIME/transitionTime);
    }
    // consider applied
    cl->appliedColorValues();
  }
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void LedChainDevice::applyChannelValueSteps(bool aForDimming, double aStepSize)
{
  // RGB, RGBW or RGBWA dimmer
  RGBColorLightBehaviourPtr cl = boost::dynamic_pointer_cast<RGBColorLightBehaviour>(output);
  // RGB lamp, get components for rendering loop
  cl->getRGB(r, g, b, 255); // get brightness per R,G,B channel
  // trigger rendering the LEDs soon
  getLedChainDeviceContainer().triggerRenderingRange(firstLED, numLEDs);
  // next step
  if (cl->colorTransitionStep(aStepSize)) {
    ALOG(LOG_DEBUG, "LED chain transitional values R=%d, G=%d, B=%d", (int)r, (int)g, (int)b);
    // not yet complete, schedule next step
    transitionTicket = MainLoop::currentMainLoop().executeOnce(
      boost::bind(&LedChainDevice::applyChannelValueSteps, this, aForDimming, aStepSize),
      TRANSITION_STEP_TIME
    );
    return; // will be called later again
  }
  if (!aForDimming) {
    ALOG(LOG_INFO, "LED chain final values R=%d, G=%d, B=%d", (int)r, (int)g, (int)b);
  }
}


double LedChainDevice::getLEDColor(uint16_t aLedNumber, uint8_t &aRed, uint8_t &aGreen, uint8_t &aBlue)
{
  // index relative to beginning of my segment
  uint16_t i = aLedNumber-firstLED;
  if (i<0 || i>=numLEDs)
    return 0; // no color at this point
  // color at this point
  aRed = r; aGreen = g; aBlue = b;
  // for soft edges
  if (i>=startSoftEdge && i<=numLEDs-endSoftEdge) {
    // not withing soft edge range, full opacity
    return 1;
  }
  else {
    if (i<startSoftEdge) {
      // zero point is LED *before* first LED!
      return 1.0/(startSoftEdge+1)*(i+1);
    }
    else {
      // zero point is LED *after* last LED!
      return 1.0/(endSoftEdge+1)*(numLEDs-i);
    }
  }
}


void LedChainDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::ledchainType:firstLED:lastLED
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  string_format_append(s, "%d:%d:%d", ledchainType, firstLED, numLEDs);
  dSUID.setNameInSpace(s, vdcNamespace);
}


string LedChainDevice::modelName()
{
  if (ledchainType==ledchain_softsegment)
    return "LED Chain Segment";
  return "LedChain device";
}



bool LedChainDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  const char *iconName = "rgbchain";
  if (iconName && getIcon(iconName, aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


string LedChainDevice::getExtraInfo()
{
  string s;
  s = string_format("Led Chain Color Light from LED #%d..%d", firstLED, firstLED+numLEDs-1);
  return s;
}



string LedChainDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "\n- Led Chain Color Light from LED #%d..%d", firstLED, firstLED+numLEDs-1);
  return s;
}


#endif // !DISABLE_LEDCHAIN



