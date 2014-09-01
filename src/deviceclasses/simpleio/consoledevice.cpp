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
#include "sensorbehaviour.hpp"
#include "binaryinputbehaviour.hpp"
#include "climatecontrolbehaviour.hpp"
#include "colorlightbehaviour.hpp"

using namespace p44;


ConsoleDevice::ConsoleDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  StaticDevice((DeviceClassContainer *)aClassContainerP),
  hasButton(false),
  hasOutput(false),
  hasColor(false),
  isValve(false)
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
    else if (mode=="valve") {
      isValve = true;
    }
  }
  // assign name
  initializeName(name);
  // create I/O
  // - special cases first
  if (isValve) {
    // simulate heating valve with lo bat (like thermokon SAB02,SAB05 or Kieback+Peter MD15-FTL)
    // - is heating
    primaryGroup = group_blue_heating;
    // - create climate control outout
    OutputBehaviourPtr ob = OutputBehaviourPtr(new ClimateControlBehaviour(*this));
    ob->setGroupMembership(group_roomtemperature_control, true); // also put into room temperature control group by default (besides standard blue)
    ob->setGroupMembership(group_blue_heating, true);
    ob->setHardwareOutputConfig(outputFunction_positional, usage_room, false, 0);
    ob->setHardwareName("Simulated valve, 0..100");
    addBehaviour(ob);
    // - create feedback sensor input
    SensorBehaviourPtr sb = SensorBehaviourPtr(new SensorBehaviour(*this));
    sb->setHardwareSensorConfig(sensorType_temperature, usage_room, 0, 40, 40.0/255, 100*Second, 5*Minute);
    sb->setGroup(group_blue_heating);
    sb->setHardwareName("Simulated Temperature 0..40 °C");
    sb->updateSensorValue(21); // default to 21 degrees
    addBehaviour(sb);
    // - create low battery binary input
    BinaryInputBehaviourPtr bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*this));
    bb->setHardwareInputConfig(binInpType_lowBattery, usage_room, true, 100*Second);
    bb->setGroup(group_blue_heating);
    bb->setHardwareName("Simulated Low Battery Indicator");
    addBehaviour(bb);
    // Simulation
    // - add console key for low battery
    consoleKey = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(name[0], name.c_str());
    consoleKey->setConsoleKeyHandler(boost::bind(&ConsoleDevice::binaryInputHandler, this, _1, _2));
    // - add console keys for changing sensor
    consoleKey = ConsoleKeyManager::sharedKeyManager()->newConsoleKey('$', "random sensor simulation change");
    consoleKey->setConsoleKeyHandler(boost::bind(&ConsoleDevice::sensorJitter, this, _1, _2));
  }
  else if (hasOutput) {
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
  // TODO: assuming SINGLE button per device here - fix it when we need to simulate multi-button devices
	ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(buttons[0]);
	if (b) {
		b->buttonAction(aState);
	}
}



void ConsoleDevice::binaryInputHandler(bool aState, MLMicroSeconds aTimeStamp)
{
  // TODO: assuming SINGLE binary input per device here - fix it when we need to simulate multi-input devices
	BinaryInputBehaviourPtr b = boost::dynamic_pointer_cast<BinaryInputBehaviour>(binaryInputs[0]);
	if (b) {
		b->updateInputState(aState);
	}
}



void ConsoleDevice::sensorJitter(bool aState, MLMicroSeconds aTimeStamp)
{
  if (aState) {
    // pressed, make a small random sensor value change
    SensorBehaviourPtr s = boost::dynamic_pointer_cast<SensorBehaviour>(sensors[0]);
    if (s) {
      double val = s->getCurrentValue();
      double inc = (s->getMax()-s->getMin())/2560*(random() & 0xFF); // change 1/10 of the range max
      if (random() & 0x01) inc = -1*inc;
      val += inc;
      if (val>s->getMax()) val = s->getMax();
      if (val<s->getMin()) val = s->getMin();
      s->updateSensorValue(val);
    }
  }
}



void ConsoleDevice::sensorValueHandler(double aValue, MLMicroSeconds aTimeStamp)
{
  // TODO: assuming SINGLE sensor per device here - fix it when we need to simulate multi-sensor devices
	SensorBehaviourPtr s = boost::dynamic_pointer_cast<SensorBehaviour>(sensors[0]);
	if (s) {
		s->updateSensorValue(aValue);
	}
}





void ConsoleDevice::applyChannelValues(DoneCB aDoneCB, bool aForDimming)
{
  // generic device, show changed channels
  for (int i = 0; i<numChannels(); i++) {
    ChannelBehaviourPtr ch = getChannelByIndex(i);
    if (ch && ch->needsApplying()) {
      double chVal = ch->getChannelValue();
      // represent full scale as 0..50 hashes
      string bar;
      double v = ch->getMin();
      double step = (ch->getMax()-ch->getMin())/50;
      while (v<chVal) {
        bar += '#';
        v += step;
      }
      // show
      printf(
         ">>> Console device %s: channel %s %s to %4.2f, transition time = %2.3f Seconds: %s\n",
         getName().c_str(), ch->getName(),
         aForDimming ? "dimmed" : "set",
         chVal, (double)ch->transitionTimeToNewValue()/Second,
         bar.c_str()
      );
      ch->channelValueApplied(); // confirm having applied the value
    }
  }
  inherited::applyChannelValues(aDoneCB, aForDimming);
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
  if (isValve)
    string_format_append(s, "- has valve actuator shown on console, pseudo temperature, battery low via console keypress\n");
  return s;
}
