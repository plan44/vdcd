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

#include "digitaliodevice.hpp"

#include "buttonbehaviour.hpp"
#include "binaryinputbehaviour.hpp"
#include "lightbehaviour.hpp"

using namespace p44;


DigitalIODevice::DigitalIODevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  StaticDevice((DeviceClassContainer *)aClassContainerP),
  digitalIoType(digitalio_unknown)
{
  // last : separates behaviour from pin specification
  size_t i = aDeviceConfig.rfind(":");
  string ioname = aDeviceConfig;
  bool inverted = false;
  if (i!=string::npos) {
    ioname = aDeviceConfig.substr(0,i);
    string mode = aDeviceConfig.substr(i+1,string::npos);
    if (mode[0]=='!') {
      inverted = true;
      mode.erase(0,1);
    }
    if (mode=="button")
      digitalIoType = digitalio_button;
    else if (mode=="input")
      digitalIoType = digitalio_input;
    else if (mode=="light")
      digitalIoType = digitalio_light;
    else if (mode=="relay") {
      digitalIoType = digitalio_relay;
      LOG(LOG_ERR,"relay type output not yet supported %%%\n");
    }
    else {
      LOG(LOG_ERR,"unknown digital IO type: %s\n", mode.c_str());
    }
  }
  // basically act as black device so we can configure colors
  if (digitalIoType==digitalio_button) {
    primaryGroup = group_black_joker;
    // Standard device settings without scene table
    installSettings();
    // Digital input as button
    buttonInput = ButtonInputPtr(new ButtonInput(ioname.c_str(), inverted));
    buttonInput->setButtonHandler(boost::bind(&DigitalIODevice::buttonHandler, this, _1, _2), true);
    // - create one button input
    ButtonBehaviourPtr b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_single, buttonElement_center, false, 0);
    addBehaviour(b);
  }
  else if (digitalIoType==digitalio_input) {
    primaryGroup = group_black_joker;
    // Standard device settings without scene table
    installSettings();
    // Digital input as binary input (AKM, automation block type)
    buttonInput = ButtonInputPtr(new ButtonInput(ioname.c_str(), inverted));
    buttonInput->setButtonHandler(boost::bind(&DigitalIODevice::inputHandler, this, _1, _2), true);
    // - create one binary input
    BinaryInputBehaviourPtr b = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*this));
    b->setHardwareInputConfig(binInpType_none, usage_undefined, true, Never);
    addBehaviour(b);
  }
  else if (digitalIoType==digitalio_light) {
    // Digital output as light on/off switch
    primaryGroup = group_yellow_light;
    indicatorOutput = IndicatorOutputPtr(new IndicatorOutput(ioname.c_str(), inverted, false));
    // - use light settings, which include a scene table
    installSettings(DeviceSettingsPtr(new LightDeviceSettings(*this)));
    // - add simple single-channel light behaviour
    LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
    l->setHardwareOutputConfig(outputFunction_switch, usage_undefined, false, -1);
    l->setGroupMembership(group_yellow_light, true); // put into light group by default
    addBehaviour(l);
  }
  else if (digitalIoType==digitalio_relay) {
    primaryGroup = group_black_joker;
    // - standard device settings with scene table
    installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*this)));
    // Digital output
    indicatorOutput = IndicatorOutputPtr(new IndicatorOutput(ioname.c_str(), inverted, false));
    // - add generic output behaviour
    OutputBehaviourPtr o = OutputBehaviourPtr(new OutputBehaviour(*this));
    o->setHardwareOutputConfig(outputFunction_switch, usage_undefined, false, -1);
    o->setGroupMembership(group_black_joker, true); // put into joker group by default
    o->addChannel(ChannelBehaviourPtr(new DigitalChannel(*o)));
    addBehaviour(o);
  }
	deriveDsUid();
}


void DigitalIODevice::buttonHandler(bool aNewState, MLMicroSeconds aTimestamp)
{
	ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(buttons[0]);
	if (b) {
		b->buttonAction(aNewState);
	}
}


void DigitalIODevice::inputHandler(bool aNewState, MLMicroSeconds aTimestamp)
{
  BinaryInputBehaviourPtr b = boost::dynamic_pointer_cast<BinaryInputBehaviour>(binaryInputs[0]);
  if (b) {
    b->updateInputState(aNewState);
  }
}



void DigitalIODevice::applyChannelValues(DoneCB aDoneCB, bool aForDimming)
{
  LightBehaviourPtr lightBehaviour = boost::dynamic_pointer_cast<LightBehaviour>(output);
  if (lightBehaviour) {
    // light
    if (lightBehaviour->brightnessNeedsApplying()) {
      indicatorOutput->set(lightBehaviour->brightnessForHardware());
      lightBehaviour->brightnessApplied(); // confirm having applied the value
    }
  }
  else if (output) {
    // simple switch output, activates at 50% of possible output range
    ChannelBehaviourPtr ch = output->getChannelByIndex(0);
    indicatorOutput->set(ch->getChannelValue() >= (ch->getMax()-ch->getMin())/2);
  }
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void DigitalIODevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::ioname[:ioname ...]
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  s += ':';
  if (buttonInput) { s += ":"; s += buttonInput->getName(); }
  if (indicatorOutput) { s += ":"; s += indicatorOutput->getName(); }
  dSUID.setNameInSpace(s, vdcNamespace);
}


string DigitalIODevice::modelName()
{
  switch (digitalIoType) {
    case digitalio_button: return "Button digital input";
    case digitalio_input: return "Binary digital input";
    case digitalio_light: return "Light controlling output";
    case digitalio_relay: return "Relay controlling output";
    default: return "Digital I/O";
  }
}


string DigitalIODevice::getExtraInfo()
{
  if (buttonInput)
    return string_format("Input: %s\n", buttonInput->getName());
  else if (indicatorOutput)
    return string_format("Output: %s\n", indicatorOutput->getName());
  else
    return "?";
}



string DigitalIODevice::description()
{
  string s = inherited::description();
  if (buttonInput)
    string_format_append(s, "- Button at Digital IO '%s'\n", buttonInput->getName());
  if (indicatorOutput)
    string_format_append(s, "- Switch output at Digital IO '%s'\n", indicatorOutput->getName());
  return s;
}
