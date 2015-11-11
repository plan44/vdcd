//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
  analogIOType(analogio_unknown),
  transitionTicket(0)
{
  string ioname = aDeviceConfig;
  string mode = "dimmer"; // default to dimmer
  size_t i = aDeviceConfig.find(":");
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
    LOG(LOG_ERR, "unknown analog IO type: %s", mode.c_str());
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
    l->setHardwareOutputConfig(outputFunction_dimmer, outputmode_gradual_positive, usage_undefined, false, -1);
    addBehaviour(l);
  }
  else if (analogIOType==analogio_rgbdimmer) {
    // - is light
    primaryGroup = group_yellow_light;
    // - need 3 IO names for R,G,B, optional fourth for W
    size_t p;
    p = ioname.find("|");
    if (p!=string::npos) {
      // at least 2 pins specified
      // - create red output
      analogIO = AnalogIoPtr(new AnalogIo(ioname.substr(0,p).c_str(), true, 0));
      ioname.erase(0,p+1);
      p = ioname.find("|");
      if (p!=string::npos) {
        // 3 pins specified
        // - create green output
        analogIO2 = AnalogIoPtr(new AnalogIo(ioname.substr(0,p).c_str(), true, 0));
        ioname.erase(0,p+1);
        p = ioname.find("|");
        if (p!=string::npos) {
          // extra 4th pin for white specified
          // - create white output from rest
          analogIO4 = AnalogIoPtr(new AnalogIo(ioname.substr(p+1).c_str(), true, 0));
          ioname.erase(p); // remove specification of white channel
        }
        // - create blue output from rest
        analogIO3 = AnalogIoPtr(new AnalogIo(ioname.c_str(), true, 0));
        // Complete set of outputs, now create RGB light (with optional white channel)
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
    ob->setHardwareOutputConfig(outputFunction_positional, outputmode_gradual_positive, usage_room, false, 0);
    ob->setHardwareName("Valve, 0..100");
    addBehaviour(ob);
  }
	deriveDsUid();
}





#define TRANSITION_STEP_TIME (10*MilliSecond)

void AnalogIODevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  MLMicroSeconds transitionTime = 0;
  // abort previous transition
  MainLoop::currentMainLoop().cancelExecutionTicket(transitionTicket);
  // generic device, show changed channels
  if (analogIOType==analogio_dimmer) {
    // single channel PWM dimmer
    LightBehaviourPtr l = boost::dynamic_pointer_cast<LightBehaviour>(output);
    if (l && l->brightnessNeedsApplying()) {
      transitionTime = l->transitionTimeToNewBrightness();
      l->brightnessTransitionStep(); // init
      applyChannelValueSteps(aForDimming, transitionTime==0 ? 1 : (double)TRANSITION_STEP_TIME/transitionTime);
    }
    // consider applied
    l->brightnessApplied();
  }
  else if (analogIOType==analogio_rgbdimmer) {
    // three channel RGB PWM dimmer
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
      } // if needs update
      // consider applied
      cl->appliedColorValues();
    }
  }
  else {
    // direct single channel PWM output, no smooth transitions
    ChannelBehaviourPtr ch = getChannelByIndex(0);
    if (ch && ch->needsApplying()) {
      double chVal = ch->getTransitionalValue()-ch->getMin();
      double chSpan = ch->getMax()-ch->getMin();
      analogIO->setValue(chVal/chSpan*100); // 0..100%
      ch->channelValueApplied(); // confirm having applied the value
    }
  }
  // always consider apply done, even if transition is still running
  inherited::applyChannelValues(aDoneCB, aForDimming);
}



void AnalogIODevice::applyChannelValueSteps(bool aForDimming, double aStepSize)
{
  // generic device, show changed channels
  if (analogIOType==analogio_dimmer) {
    // single channel PWM dimmer
    LightBehaviourPtr l = boost::dynamic_pointer_cast<LightBehaviour>(output);
    double w = l->brightnessForHardware();
    double pwm = l->brightnessToPWM(w, 100);
    analogIO->setValue(pwm);
    // next step
    if (l->brightnessTransitionStep(aStepSize)) {
      ALOG(LOG_DEBUG, "AnalogIO transitional PWM value: %.2f", w);
      // not yet complete, schedule next step
      transitionTicket = MainLoop::currentMainLoop().executeOnce(
        boost::bind(&AnalogIODevice::applyChannelValueSteps, this, aForDimming, aStepSize),
        TRANSITION_STEP_TIME
      );
      return; // will be called later again
    }
    if (!aForDimming) ALOG(LOG_INFO, "AnalogIO final PWM value: %.2f", w);
  }
  else if (analogIOType==analogio_rgbdimmer) {
    // three channel RGB PWM dimmer
    RGBColorLightBehaviourPtr cl = boost::dynamic_pointer_cast<RGBColorLightBehaviour>(output);
    // RGB lamp, get components
    double r, g, b, pwm;
    double w = 0;
    if (analogIO4) {
      // RGBW lamp
      cl->getRGBW(r, g, b, w, 100); // get brightness for R,G,B,W channels
      pwm = cl->brightnessToPWM(w, 100);
      analogIO4->setValue(pwm);
    }
    else {
      // RGB only
      cl->getRGB(r, g, b, 100); // get brightness for R,G,B channels
    }
    // - red
    pwm = cl->brightnessToPWM(r, 100);
    analogIO->setValue(pwm);
    // - green
    pwm = cl->brightnessToPWM(g, 100);
    analogIO2->setValue(pwm);
    // - blue
    pwm = cl->brightnessToPWM(b, 100);
    analogIO3->setValue(pwm);
    // next step
    if (cl->colorTransitionStep(aStepSize)) {
      ALOG(LOG_DEBUG, "AnalogIO transitional RGBW values: R=%.2f G=%.2f, B=%.2f, W=%.2f", r, g, b, w);
      // not yet complete, schedule next step
      transitionTicket = MainLoop::currentMainLoop().executeOnce(
        boost::bind(&AnalogIODevice::applyChannelValueSteps, this, aForDimming, aStepSize),
        TRANSITION_STEP_TIME
      );
      return; // will be called later again
    }
    if (!aForDimming) ALOG(LOG_INFO, "AnalogIO final RGBW values: R=%.2f G=%.2f, B=%.2f, W=%.2f", r, g, b, w);
  }
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
  if (analogIO4) { s += ":"; s += analogIO4->getName(); }
  dSUID.setNameInSpace(s, vdcNamespace);
}


string AnalogIODevice::modelName()
{
  if (analogIOType==analogio_dimmer)
    return "Dimmer output";
  if (analogIOType==analogio_rgbdimmer)
    return "RGB(W) dimmer outputs";
  else if (analogIOType==analogio_valve)
    return "Heating Valve output";
  return "Analog I/O";
}


string AnalogIODevice::getExtraInfo()
{
  if (analogIOType==analogio_rgbdimmer)
    return string_format("RGB Outputs:%s, %s, %s; White:%s", analogIO->getName().c_str(), analogIO2->getName().c_str(), analogIO3->getName().c_str(), analogIO4 ? analogIO4->getName().c_str() : "none");
  else if (analogIOType==analogio_dimmer || analogIOType==analogio_valve)
    return string_format("Output: %s", analogIO->getName().c_str());
  return "Analog I/O";
}



string AnalogIODevice::description()
{
  string s = inherited::description();
  if (analogIOType==analogio_dimmer)
    string_format_append(s, "\n- Dimmer at Analog output '%s'", analogIO->getName().c_str());
  if (analogIOType==analogio_rgbdimmer)
    string_format_append(s, "\n- Color Dimmer with RGB outputs '%s', '%s', '%s'; White: '%s'", analogIO->getName().c_str(), analogIO2->getName().c_str(), analogIO3->getName().c_str(), analogIO4 ? analogIO4->getName().c_str() : "none");
  if (analogIOType==analogio_valve)
    string_format_append(s, "\n- Heating Valve @ '%s'", analogIO->getName().c_str());
  return s;
}

