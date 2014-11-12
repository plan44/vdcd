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

#include "oladevice.hpp"

#if !DISABLE_OLA

#include "lightbehaviour.hpp"
#include "colorlightbehaviour.hpp"


using namespace p44;


#pragma mark - OlaDevice


static bool nextChannelSpec(string &aConfig, size_t &aStartPos, char &aChannelType, DmxChannel &aChannelNo)
{
  // check for channel spec
  // syntax is: C=n[,C=n,...] where C=channel type character, n=channel number
  size_t i = aConfig.find_first_of('=', aStartPos);
  if (i==string::npos || i==0) return false;
  // first char before = is channel type
  aChannelType = aConfig[aStartPos];
  // after =, there must be a channel number
  int n;
  if (sscanf(aConfig.c_str()+i+1, "%d", &n)!=1) return false;
  aChannelNo = n;
  // advance until after next colon
  i = aConfig.find_first_of(',', i);
  if (i==string::npos)
    aStartPos = aConfig.size();
  else
    aStartPos = i+1; // next item after ,
  return true;
}


OlaDevice::OlaDevice(OlaDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  inherited(aClassContainerP),
  olaType(ola_unknown),
  whiteChannel(dmxNone),
  redChannel(dmxNone),
  greenChannel(dmxNone),
  blueChannel(dmxNone),
  amberChannel(dmxNone)
{
  string config = aDeviceConfig;
  string mode = "dimmer"; // default to dimmer
  size_t i = aDeviceConfig.find_first_of(':');
  if (i!=string::npos) {
    mode = aDeviceConfig.substr(0,i);
    config = aDeviceConfig.substr(i+1,string::npos);
  }
  if (mode=="dimmer")
    olaType = ola_dimmer;
  else if (mode=="tunablewhite")
    olaType = ola_tunablewhitedimmer;
  else if (mode=="color")
    olaType = ola_fullcolordimmer;
  else {
    LOG(LOG_ERR,"unknown OLA device type: %s\n", mode.c_str());
  }
  // by default, act as black device so we can configure colors
  primaryGroup = group_black_joker;
  if (olaType==ola_dimmer) {
    // Single channel DMX512 dimmer
    int channel;
    if (sscanf(config.c_str(), "%d", &channel)) {
      whiteChannel = channel;
    }
    // - is light
    primaryGroup = group_yellow_light;
    // - use light settings, which include a scene table
    installSettings(DeviceSettingsPtr(new LightDeviceSettings(*this)));
    // - add simple single-channel light behaviour
    LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
    l->setHardwareOutputConfig(outputFunction_dimmer, usage_undefined, false, -1);
    l->setGroupMembership(group_yellow_light, true); // put into light group by default
    addBehaviour(l);
  }
  else if (olaType==ola_fullcolordimmer) {
    // - is RGB
    primaryGroup = group_yellow_light;
    // - get DMX channels for colors
    char channelType;
    DmxChannel channelNo;
    size_t p = 0;
    while (nextChannelSpec(config, p, channelType, channelNo)) {
      switch (channelType) {
        case 'W' : whiteChannel = channelNo; break;
        case 'R' : redChannel = channelNo; break;
        case 'G' : greenChannel = channelNo; break;
        case 'B' : blueChannel = channelNo; break;
        case 'A' : amberChannel = channelNo; break;
      }
    }
    if (redChannel!=dmxNone && greenChannel!=dmxNone && blueChannel!=dmxNone) {
      // Complete set of outputs, now create RGB light
      // - use color light settings, which include a color scene table
      installSettings(DeviceSettingsPtr(new ColorLightDeviceSettings(*this)));
      // - add multi-channel color light behaviour (which adds a number of auxiliary channels)
      RGBColorLightBehaviourPtr l = RGBColorLightBehaviourPtr(new RGBColorLightBehaviour(*this));
      addBehaviour(l);
    }
  }
  deriveDsUid();
}



bool OlaDevice::isSoftwareDisconnectable()
{
  return olaDeviceRowID>0; // disconnectable by software if it was created from DB entry (and not on the command line)
}

OlaDeviceContainer &OlaDevice::getOlaDeviceContainer()
{
  return *(static_cast<OlaDeviceContainer *>(classContainerP));
}


void OlaDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  // clear learn-in data from DB
  if (olaDeviceRowID) {
    getOlaDeviceContainer().db.executef("DELETE FROM devConfigs WHERE rowid=%d", olaDeviceRowID);
  }
  // disconnection is immediate, so we can call inherited right now
  inherited::disconnect(aForgetParams, aDisconnectResultHandler);
}


void OlaDevice::setDMXChannel(DmxChannel aChannel, DmxValue aChannelValue)
{
  if (aChannel>=1 && aChannel<=512) {
    getOlaDeviceContainer().dmxBuffer.SetChannel(aChannel-1, aChannelValue);
  }
}



void OlaDevice::applyChannelValues(DoneCB aDoneCB, bool aForDimming)
{
  // generic device, show changed channels
  if (olaType==ola_dimmer) {
    // single channel dimmer
    LightBehaviourPtr l = boost::dynamic_pointer_cast<LightBehaviour>(output);
    if (l && l->brightnessNeedsApplying()) {
      double w = l->brightnessForHardware()*255/100;
      setDMXChannel(whiteChannel,(DmxValue)w);
      if (!aForDimming) LOG(LOG_INFO, "OLA device %s: DMX512 channel %d=%d\n", shortDesc().c_str(), whiteChannel, (int)w);
      l->brightnessApplied(); // confirm having applied the new brightness
    }
  }
  else if (olaType==ola_fullcolordimmer) {
    // for now: only RGB, later: als support RGBW and RGBWA
    // %%%three channel RGB dimmer
    RGBColorLightBehaviourPtr cl = boost::dynamic_pointer_cast<RGBColorLightBehaviour>(output);
    if (cl) {
      if (needsToApplyChannels()) {
        // needs update
        // - derive (possibly new) color mode from changed channels
        cl->deriveColorMode();
        // RGB lamp, get components
        double r,g,b;
        double w = 0;
        double a = 0;
        if (whiteChannel!=dmxNone) {
          if (amberChannel!=dmxNone) {
            // RGBW
            cl->getRGBWA(r, g, b, w, a, 255);
            setDMXChannel(amberChannel,(DmxValue)a);
          }
          else {
            // RGBW
            cl->getRGBW(r, g, b, w, 255);
          }
          setDMXChannel(whiteChannel,(DmxValue)w);
        }
        else {
          // RGB
          cl->getRGB(r, g, b, 255); // get brightness per R,G,B channel
        }
        // There's always RGB
        setDMXChannel(redChannel,(DmxValue)r);
        setDMXChannel(greenChannel,(DmxValue)g);
        setDMXChannel(blueChannel,(DmxValue)b);
        if (!aForDimming) LOG(LOG_INFO,
          "OLA device %s: set DMX512 channels R(%hd)=%d, G(%hd)=%d, B(%hd)=%d, W(%hd)=%d, A(%hd)=%d\n",
          shortDesc().c_str(),
          redChannel, (int)r, greenChannel, (int)g, blueChannel, (int)b, whiteChannel, (int)w, amberChannel, (int)a
        );
      } // if needs update
      // anyway, applied now
      cl->appliedRGB();
    }
  }
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void OlaDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::olaType:white[:red:green:blue][:amber]
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  string_format_append(s, ":%d:%d", (int)olaType, whiteChannel);
  if (olaType==ola_fullcolordimmer)
    string_format_append(s, ":%d:%d:%d", redChannel, greenChannel, blueChannel);
  if (amberChannel!=dmxNone)
    string_format_append(s, ":%d", amberChannel);
  dSUID.setNameInSpace(s, vdcNamespace);
}


string OlaDevice::modelName()
{
  if (olaType==ola_dimmer)
    return "DMX512 Dimmer";
  else if (olaType==ola_tunablewhitedimmer)
    return "DMX512 Tunable white";
  else if (olaType==ola_fullcolordimmer)
    return "DMX512 Full color";
  return "DMX512 device";
}



bool OlaDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  const char *iconName = NULL;
  switch (olaType) {
    case ola_dimmer: iconName = "ola_dimmer"; break;
    case ola_tunablewhitedimmer: iconName = "ola_ct"; break;
    case ola_fullcolordimmer: iconName = "ola_color"; break;
    default: break;
  }
  if (iconName && getIcon(iconName, aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


string OlaDevice::getExtraInfo()
{
  if (olaType==ola_dimmer)
    return string_format("DMX512 Dimmer: brightness=%d", whiteChannel);
  else if (olaType==ola_tunablewhitedimmer)
    return string_format("DMX512 Tunable white dimmer: white=%d, amber=%d", whiteChannel, amberChannel);
  else if (olaType==ola_fullcolordimmer)
    return string_format("DMX512 Full color dimmer: RGB=%d,%d,%d, white=%d, amber=%d", redChannel, greenChannel, blueChannel, whiteChannel, amberChannel);
  return "DMX512 device";
}



string OlaDevice::description()
{
  string s = inherited::description();
  if (olaType==ola_dimmer)
    string_format_append(s, "- DMX512 Dimmer: brightness=%d\n", whiteChannel);
  else if (olaType==ola_tunablewhitedimmer)
    string_format_append(s, "- DMX512 Tunable white dimmer: white=%d, amber=%d\n", whiteChannel, amberChannel);
  else if (olaType==ola_fullcolordimmer)
    string_format_append(s, "- DMX512 Full color dimmer: RGB=%d,%d,%d, white=%d, amber=%d\n", redChannel, greenChannel, blueChannel, whiteChannel, amberChannel);
  return s;
}

#endif // !DISABLE_OLA



