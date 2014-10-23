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

#include "analogiodevice.hpp"

#include "lightbehaviour.hpp"
#include "colorlightbehaviour.hpp"
#include "climatecontrolbehaviour.hpp"

using namespace p44;


AnalogIODevice::AnalogIODevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  StaticDevice((DeviceClassContainer *)aClassContainerP),
  analogIOType(analogio_unknown)
{
  string ioname = aDeviceConfig;
  string mode = "dimmer"; // default to dimmer
  size_t i = aDeviceConfig.find_first_of(':');
  if (i!=string::npos) {
    ioname = aDeviceConfig.substr(0,i);
    mode = aDeviceConfig.substr(i+1,string::npos);
  }
  if (mode=="dimmer")
    analogIOType = analogio_dimmer;
  else if (mode=="rgbdimmer")
    analogIOType = analogio_rgbdimmer;
  else if (mode=="valve")
    analogIOType = analogio_valve;
  else {
    LOG(LOG_ERR,"unknown analog IO type: %s\n", mode.c_str());
  }
  // by default, act as black device so we can configure colors
  primaryGroup = group_black_joker;
  if (analogIOType==analogio_dimmer) {
    // Analog output as dimmer
    analogIO = AnalogIoPtr(new AnalogIo(ioname.c_str(), true, 0));
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
  else if (analogIOType==analogio_rgbdimmer) {
    // - is light
    primaryGroup = group_yellow_light;
    // - need 3 IO names for R,G,B
    size_t p;
    p = ioname.find_first_of('|');
    if (p!=string::npos) {
      // at least 2 pins specified
      // - create red output
      analogIO = AnalogIoPtr(new AnalogIo(ioname.substr(0,p).c_str(), true, 0));
      ioname.erase(0,p+1);
      p = ioname.find_first_of('|');
      if (p!=string::npos) {
        // 3 pins specified
        // - create green output
        analogIO2 = AnalogIoPtr(new AnalogIo(ioname.substr(0,p).c_str(), true, 0));
        // - create blue output from rest
        analogIO3 = AnalogIoPtr(new AnalogIo(ioname.substr(p+1).c_str(), true, 0));
        // Complete set of outputs, now create RGB light
        // - use color light settings, which include a color scene table
        installSettings(DeviceSettingsPtr(new ColorLightDeviceSettings(*this)));
        // - add multi-channel color light behaviour (which adds a number of auxiliary channels)
        RGBColorLightBehaviourPtr l = RGBColorLightBehaviourPtr(new RGBColorLightBehaviour(*this));
        addBehaviour(l);
      }
    }
  }
  else if (analogIOType==analogio_valve) {
    // Analog output as valve controlling output
    analogIO = AnalogIoPtr(new AnalogIo(ioname.c_str(), true, 0));
    // - is heating
    primaryGroup = group_blue_heating;
    // - standard device settings with scene table
    installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*this)));
    // - create climate control outout
    OutputBehaviourPtr ob = OutputBehaviourPtr(new ClimateControlBehaviour(*this));
    ob->setGroupMembership(group_roomtemperature_control, true); // put into room temperature control group by default, NOT into standard blue)
    ob->setHardwareOutputConfig(outputFunction_positional, usage_room, false, 0);
    ob->setHardwareName("Valve, 0..100");
    addBehaviour(ob);
  }
	deriveDsUid();
}


void AnalogIODevice::applyChannelValues(DoneCB aDoneCB, bool aForDimming)
{
  // generic device, show changed channels
  if (analogIOType==analogio_dimmer) {
    // single channel PWM dimmer
    LightBehaviourPtr l = boost::dynamic_pointer_cast<LightBehaviour>(output);
    if (l && l->brightnessNeedsApplying()) {
      double pwm = l->brightnessToPWM(l->brightnessForHardware(), 100);
      analogIO->setValue(pwm);
      l->brightnessApplied(); // confirm having applied the new brightness
    }
  }
  else if (analogIOType==analogio_rgbdimmer) {
    // three channel RGB PWM dimmer
    RGBColorLightBehaviourPtr cl = boost::dynamic_pointer_cast<RGBColorLightBehaviour>(output);
    if (cl) {
      if (needsToApplyChannels()) {
        // needs update
        // - derive (possibly new) color mode from changed channels
        cl->deriveColorMode();
        // RGB lamp, get components
        double r,g,b;
        cl->getRGB(r, g, b, 100); // get brightness per R,G,B channel
        // transfer to outputs
        // - red
        double pwm = cl->brightnessToPWM(r, 100);
        analogIO->setValue(pwm);
        // - green
        pwm = cl->brightnessToPWM(g, 100);
        analogIO2->setValue(pwm);
        // - red
        pwm = cl->brightnessToPWM(b, 100);
        analogIO3->setValue(pwm);
      } // if needs update
      // anyway, applied now
      cl->appliedRGB();
    }
  }
  else {
    // direct single channel PWM output
    ChannelBehaviourPtr ch = getChannelByIndex(0);
    if (ch && ch->needsApplying()) {
      double chVal = ch->getChannelValue()-ch->getMin();
      double chSpan = ch->getMax()-ch->getMin();
      analogIO->setValue(chVal/chSpan*100); // 0..100%
      ch->channelValueApplied(); // confirm having applied the value
    }
  }
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void AnalogIODevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::ioname[:ioname ...]
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  string_format_append(s, ":%d:", (int)analogIOType);
  if (analogIO) { s += ":"; s += analogIO->getName(); }
  if (analogIO2) { s += ":"; s += analogIO2->getName(); }
  if (analogIO3) { s += ":"; s += analogIO3->getName(); }
  dSUID.setNameInSpace(s, vdcNamespace);
}


string AnalogIODevice::modelName()
{
  if (analogIOType==analogio_dimmer)
    return "Dimmer output";
  if (analogIOType==analogio_rgbdimmer)
    return "RGB dimmer outputs";
  else if (analogIOType==analogio_valve)
    return "Heating Valve output";
  return "Analog I/O";
}


string AnalogIODevice::getExtraInfo()
{
  if (analogIOType==analogio_rgbdimmer)
    return string_format("RGB Outputs: %s, %s, %s", analogIO->getName(), analogIO2->getName(), analogIO3->getName());
  else if (analogIOType==analogio_dimmer || analogIOType==analogio_valve)
    return string_format("Output: %s", analogIO->getName());
  return "Analog I/O";
}



string AnalogIODevice::description()
{
  string s = inherited::description();
  if (analogIOType==analogio_dimmer)
    string_format_append(s, "- Dimmer at Analog output '%s'\n", analogIO->getName());
  if (analogIOType==analogio_rgbdimmer)
    string_format_append(s, "- Color Dimmer at Analog outputs '%s', '%s', '%s'\n", analogIO->getName(), analogIO2->getName(), analogIO3->getName());
  else if (analogIOType==analogio_valve)
    return string_format("Heating Valve @ '%s'\n", analogIO->getName());
  return s;
}

