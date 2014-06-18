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

#include "fnv.hpp"

#include "buttonbehaviour.hpp"
#include "lightbehaviour.hpp"

using namespace p44;


DigitalIODevice::DigitalIODevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  Device((DeviceClassContainer *)aClassContainerP)
{
  size_t i = aDeviceConfig.find_first_of(':');
  string ioname = aDeviceConfig;
  bool inverted = false;
  bool output = false;
  if (i!=string::npos) {
    ioname = aDeviceConfig.substr(0,i);
    string mode = aDeviceConfig.substr(i+1,string::npos);
    if (mode[0]=='!') {
      inverted = true;
      mode.erase(0,1);
    }
    if (mode=="in") {
      output = false;
    }
    else if (mode=="out") {
      output = true;
    }
  }
  // basically act as black device so we can configure colors
  primaryGroup = group_black_joker;
  if (output) {
    // Digital output as light on/off switch
    indicatorOutput = IndicatorOutputPtr(new IndicatorOutput(ioname.c_str(), inverted, false));
    // Simulate light device
    LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
    l->setHardwareOutputConfig(outputFunction_switch, usage_undefined, false, -1);
    addBehaviour(l);
  }
  else {
    // Digital input as button
    buttonInput = ButtonInputPtr(new ButtonInput(ioname.c_str(), inverted));
    buttonInput->setButtonHandler(boost::bind(&DigitalIODevice::buttonHandler, this, _1, _2), true);
    // - create one button input
    ButtonBehaviourPtr b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_single, buttonElement_center, false, 0);
    addBehaviour(b);
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


void DigitalIODevice::applyChannelValues(CompletedCB aCompletedCB)
{
  // light device
  LightBehaviourPtr lightBehaviour = boost::dynamic_pointer_cast<LightBehaviour>(output);
  if (lightBehaviour && lightBehaviour->brightnessNeedsApplying()) {
    indicatorOutput->set(lightBehaviour->brightnessForHardware());
    lightBehaviour->brightnessApplied(); // confirm having applied the value
  }
  inherited::applyChannelValues(aCompletedCB);
}


void DigitalIODevice::deriveDsUid()
{
  Fnv64 hash;

  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::ioname[:ioname ...]
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  s += ':';
  if (buttonInput) s += ':' + buttonInput->getName();
  if (indicatorOutput) s += ':' + indicatorOutput->getName();
  dSUID.setNameInSpace(s, vdcNamespace);
}


string DigitalIODevice::modelName()
{
  if (buttonInput)
    return string_format("Digital Input @ %s", buttonInput->getName());
  else if (indicatorOutput)
    return string_format("Digital Output @ %s", indicatorOutput->getName());
  return "Digital I/O";
}





string DigitalIODevice::description()
{
  string s = inherited::description();
  if (buttonInput)
    string_format_append(s, "- Button at Digital IO '%s'\n", buttonInput->getName());
  if (indicatorOutput)
    string_format_append(s, "- On/Off Lamp at Digital IO '%s'\n", indicatorOutput->getName());
  return s;
}
