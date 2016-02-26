//
//  Copyright (c) 2013-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
  consoleIoType(consoleio_unknown)
{
  // Config is:
  //  <name>:<behaviour type>
  //  - where first character of name (and possibly next in alphabeth) is also used as console key for inputs
  size_t i = aDeviceConfig.find(":");
  string name = aDeviceConfig;
  if (i!=string::npos) {
    name = aDeviceConfig.substr(0,i);
    string mode = aDeviceConfig.substr(i+1,string::npos);
    if (mode=="button")
      consoleIoType = consoleio_button;
    else if (mode=="rocker")
      consoleIoType = consoleio_rocker;
    else if (mode=="input")
      consoleIoType = consoleio_input;
    else if (mode=="sensor")
      consoleIoType = consoleio_sensor;
    else if (mode=="dimmer")
      consoleIoType = consoleio_dimmer;
    else if (mode=="colordimmer")
      consoleIoType = consoleio_colordimmer;
    else if (mode=="valve")
      consoleIoType = consoleio_valve;
    else {
      LOG(LOG_ERR, "unknown console IO type: %s", mode.c_str());
    }
  }
  else {
    LOG(LOG_ERR, "missing console IO type");
  }
  // assign name for showing on console and for creating dSUID from
  consoleName = name;
  // create I/O
  if (consoleIoType==consoleio_button) {
    // Simulate Button device
    // - defaults to black (generic button)
    primaryGroup = group_black_joker;
    // - standard device settings without scene table
    installSettings();
    // - console key input as button
    consoleKey1 = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(name[0], name.c_str());
    consoleKey1->setConsoleKeyHandler(boost::bind(&ConsoleDevice::buttonHandler, this, 0, _1, _2));
    // - create one button input
    ButtonBehaviourPtr b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_undefined, buttonElement_center, false, 0, false); // mode not restricted
    b->setGroup(group_yellow_light); // pre-configure for light
    b->setHardwareName(string_format("console key '%c'",name[0]));
    addBehaviour(b);
  }
  if (consoleIoType==consoleio_rocker) {
    // Simulate Two-way Rocker Button device
    // - defaults to black (generic button)
    primaryGroup = group_black_joker;
    // - standard device settings without scene table
    installSettings();
    // - create down button (index 0)
    consoleKey1 = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(name[0], name.c_str());
    consoleKey1->setConsoleKeyHandler(boost::bind(&ConsoleDevice::buttonHandler, this, 0, _1, _2));
    ButtonBehaviourPtr b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_2way, buttonElement_down, false, 1, true); // counterpart up-button has buttonIndex 1, fixed mode
    b->setGroup(group_yellow_light); // pre-configure for light
    b->setHardwareName(string_format("console key '%c'",consoleKey1->getKeyCode()));
    addBehaviour(b);
    // - create up button (index 1)
    consoleKey2 = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(name[0]+1, name.c_str());
    consoleKey2->setConsoleKeyHandler(boost::bind(&ConsoleDevice::buttonHandler, this, 1, _1, _2));
    b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_2way, buttonElement_down, false, 0, true); // counterpart down-button has buttonIndex 0, fixed mode
    b->setGroup(group_yellow_light); // pre-configure for light
    b->setHardwareName(string_format("console key '%c'",consoleKey2->getKeyCode()));
    addBehaviour(b);
  }
  else if (consoleIoType==consoleio_input) {
    // Standard device settings without scene table
    primaryGroup = group_black_joker;
    installSettings();
    // Digital input as binary input (AKM, automation block type)
    consoleKey1 = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(name[0], name.c_str());
    consoleKey1->setConsoleKeyHandler(boost::bind(&ConsoleDevice::binaryInputHandler, this, _1, _2));
    // - create one binary input
    BinaryInputBehaviourPtr b = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*this));
    b->setHardwareInputConfig(binInpType_none, usage_undefined, true, Never);
    addBehaviour(b);
  }
  else if (consoleIoType==consoleio_sensor) {
    // Standard device settings without scene table
    primaryGroup = group_black_joker;
    installSettings();
    // Analog sensor value, which can be changed up and down with two keys
    consoleKey1 = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(name[0], name.c_str());
    consoleKey1->setConsoleKeyHandler(boost::bind(&ConsoleDevice::sensorHandler, this, 0, _1, _2)); // down
    consoleKey2 = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(name[0]+1, name.c_str());
    consoleKey2->setConsoleKeyHandler(boost::bind(&ConsoleDevice::sensorHandler, this, 1, _1, _2)); // up
    // - create one sensor input
    SensorBehaviourPtr s = SensorBehaviourPtr(new SensorBehaviour(*this));
    s->setHardwareSensorConfig(sensorType_none, usage_undefined, 0, 50, 1, 30*Second, 300*Second);
    s->setHardwareName("Console simulated Sensor 0..50");
    addBehaviour(s);
  }
  else if (consoleIoType==consoleio_valve) {
    // simulate heating valve with lo bat (like thermokon SAB02,SAB05 or Kieback+Peter MD15-FTL)
    // - is heating
    primaryGroup = group_blue_heating;
    // - standard device settings with scene table
    installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*this)));
    // - create climate control outout
    OutputBehaviourPtr ob = OutputBehaviourPtr(new ClimateControlBehaviour(*this));
    ob->setGroupMembership(group_roomtemperature_control, true); // put into room temperature control group by default, NOT into standard blue)
    ob->setHardwareOutputConfig(outputFunction_bipolar_positional, outputmode_gradual, usage_room, false, 0);
    ob->setHardwareName("Simulated valve, -100..100");
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
    consoleKey1 = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(name[0], name.c_str());
    consoleKey1->setConsoleKeyHandler(boost::bind(&ConsoleDevice::binaryInputHandler, this, _1, _2));
    // - add console keys for changing sensor
    consoleKey2 = ConsoleKeyManager::sharedKeyManager()->newConsoleKey('$', "random sensor simulation change");
    consoleKey2->setConsoleKeyHandler(boost::bind(&ConsoleDevice::sensorJitter, this, _1, _2));
  }
  else if (consoleIoType==consoleio_dimmer) {
    // Simple single-channel light
    // - defaults to yellow (light)
    primaryGroup = group_yellow_light;
    // - use light settings, which include a scene table
    installSettings(DeviceSettingsPtr(new LightDeviceSettings(*this)));
    // - add simple single-channel light behaviour
    LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
    l->setHardwareOutputConfig(outputFunction_dimmer, outputmode_gradual, usage_undefined, true, -1);
    addBehaviour(l);
  }
  else if (consoleIoType==consoleio_colordimmer) {
    // Color light
    // - defaults to yellow (light)
    primaryGroup = group_yellow_light;
    // - use color light settings, which include a color scene table
    installSettings(DeviceSettingsPtr(new ColorLightDeviceSettings(*this)));
    // - add multi-channel color light behaviour (which adds a number of auxiliary channels)
    ColorLightBehaviourPtr l = ColorLightBehaviourPtr(new ColorLightBehaviour(*this));
    addBehaviour(l);
  }
  deriveDsUid();
}


string ConsoleDevice::modelName()
{
  string m = "Console ";
  switch (consoleIoType) {
    case consoleio_button: string_format_append(m, "button key:'%c'", consoleKey1->getKeyCode()); break;
    case consoleio_rocker: string_format_append(m, "rocker keys:'%c'/'%c'", consoleKey1->getKeyCode(), consoleKey2->getKeyCode()); break;
    case consoleio_input: string_format_append(m, "binary input key:'%c'", consoleKey1->getKeyCode()); break;
    case consoleio_sensor: string_format_append(m, "sensor tunable with keys:'%c'/'%c'", consoleKey1->getKeyCode(), consoleKey2->getKeyCode()); break;
    case consoleio_valve: m += "valve"; break;
    case consoleio_dimmer: m += "dimmer"; break;
    case consoleio_colordimmer: m += "color dimmer"; break;
    default: break;
  }
  return m;
}


void ConsoleDevice::buttonHandler(int aButtonIndex, bool aState, MLMicroSeconds aTimestamp)
{
	ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(buttons[aButtonIndex]);
	if (b) {
		b->buttonAction(aState);
	}
}


void ConsoleDevice::sensorHandler(int aButtonIndex, bool aState, MLMicroSeconds aTimestamp)
{
  if (aState) {
    // pressed
    SensorBehaviourPtr s = boost::dynamic_pointer_cast<SensorBehaviour>(sensors[0]);
    if (s) {
      double v = s->getCurrentValue();
      if (aButtonIndex==0)
        v -= s->getResolution(); // down
      else
        v += s->getResolution(); // down
      // limit
      if (v>s->getMax()) v = s->getMax();
      if (v<s->getMin()) v = s->getMin();
      // post
      s->updateSensorValue(v);
    }
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





void ConsoleDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
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
  s += "::" + consoleName;
  dSUID.setNameInSpace(s, vdcNamespace);
}


string ConsoleDevice::description()
{
  string s = inherited::description();
  if (consoleIoType==consoleio_dimmer || consoleIoType==consoleio_colordimmer)
    string_format_append(s, "\n- has output printing channel value(s) to console");
  if (consoleIoType==consoleio_button)
    string_format_append(s, "\n- has button which can be switched via console keypresses");
  if (consoleIoType==consoleio_rocker)
    string_format_append(s, "\n- has 2-way-rocker which can be switched via console keypresses");
  if (consoleIoType==consoleio_input)
    string_format_append(s, "\n- has binary input can be switched via console keypresses");
  if (consoleIoType==consoleio_sensor)
    string_format_append(s, "\n- has sensor input that can be tuned up/down via console keypresses");
  if (consoleIoType==consoleio_valve)
    string_format_append(s, "\n- has valve actuator shown on console, pseudo temperature, battery low via console keypress");
  return s;
}
