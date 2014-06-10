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

#include "consoledevice.hpp"

#include "fnv.hpp"

#include "buttonbehaviour.hpp"
#include "lightbehaviour.hpp"
#include "colorlightbehaviour.hpp"

using namespace p44;


ConsoleDevice::ConsoleDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  Device((DeviceClassContainer *)aClassContainerP),
  hasButton(false),
  hasOutput(false),
  hasColor(false),
  outputValue(0)
{
  size_t i = aDeviceConfig.find_first_of(':');
  string name = aDeviceConfig;
  if (i!=string::npos) {
    name = aDeviceConfig.substr(0,i);
    string mode = aDeviceConfig.substr(i+1,string::npos);
    if (mode=="in")
      hasButton = true;
    else if (mode=="out")
      hasOutput = true;
    else if (mode=="io") {
      hasButton = true;
      hasOutput = true;
    }
    else if (mode=="color") {
      hasOutput = true;
      hasColor = true;
    }
  }
  // assign name
  initializeName(name);
  // create I/O
  if (hasOutput) {
    // Simulate light device
    // - defaults to yellow (light)
    primaryGroup = group_yellow_light;
    // - create output(s)
    if (hasColor) {
      // Color light
      // - use color light settings, which include a color scene table
      deviceSettings = DeviceSettingsPtr(new ColorLightDeviceSettings(*this));
      // - add multi-channel color light behaviour (which adds a number of auxiliary channels)
      ColorLightBehaviourPtr l = ColorLightBehaviourPtr(new ColorLightBehaviour(*this));
      addBehaviour(l);
    }
    else {
      // Simple single-channel light
      // - use light settings, which include a scene table
      deviceSettings = DeviceSettingsPtr(new LightDeviceSettings(*this));
      // - add simple single-channel light behaviour
      LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
      l->setHardwareOutputConfig(outputFunction_dimmer, usage_undefined, true, -1);
      addBehaviour(l);
    }
  }
  else if (hasButton) {
    // Simulate Button device
    // - defaults to black (generic button)
    primaryGroup = group_black_joker;
    // - console key input as button
    consoleKey = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(name[0], name.c_str());
    consoleKey->setConsoleKeyHandler(boost::bind(&ConsoleDevice::buttonHandler, this, _1, _2));
    // - create one button input
    ButtonBehaviourPtr b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_single, buttonElement_center, false, 0);
    b->setHardwareName(string_format("console key '%c'",name[0]));
    addBehaviour(b);
  }
	deriveDsUid();
}


void ConsoleDevice::buttonHandler(bool aState, MLMicroSeconds aTimestamp)
{
	ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(buttons[0]);
	if (b) {
		b->buttonAction(aState);
	}
}


void ConsoleDevice::updateChannelValue(ChannelBehaviour &aChannelBehaviour)
{
  if (aChannelBehaviour.isPrimary()) {
    outputValue = aChannelBehaviour.valueForHardware();
    printf(
      ">>> Console device %s: output set to %d, transition time = %0.3f Seconds\n",
      getName().c_str(), outputValue,
      (double)aChannelBehaviour.transitionTimeForHardware()/Second
    );
    aChannelBehaviour.channelValueApplied(); // confirm having applied the value
  }
  else
    return inherited::updateChannelValue(aChannelBehaviour); // let superclass handle this
}



void ConsoleDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::consoledevicename
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  s += "::" + getName();
  dSUID.setNameInSpace(s, vdcNamespace);
}


string ConsoleDevice::description()
{
  string s = inherited::description();
  if (hasOutput)
    string_format_append(s, "- has output printing value to console\n");
  if (hasButton)
    string_format_append(s, "- has button which can be switched via console keypresses\n");
  return s;
}
