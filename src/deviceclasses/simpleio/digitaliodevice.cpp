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

#include "digitaliodevice.hpp"

#if ENABLE_STATIC

#include "buttonbehaviour.hpp"
#include "binaryinputbehaviour.hpp"
#include "lightbehaviour.hpp"
#include "shadowbehaviour.hpp"

using namespace p44;

#define INPUT_DEBOUNCE_TIME (25*MilliSecond)

DigitalIODevice::DigitalIODevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  StaticDevice((DeviceClassContainer *)aClassContainerP),
  digitalIoType(digitalio_unknown)
{
  // Config is:
  //  <pin(s) specification>:<behaviour mode>
  //  - where pin specification describes the actual I/Os to be used (see DigitialIO)
  // last : separates behaviour from pin specification (so pins specs containing colons are possible, such as OW LEDs)
  size_t i = aDeviceConfig.rfind(":");
  string ioname = aDeviceConfig;
  string upName;
  string downName;
  if (i!=string::npos) {
    ioname = aDeviceConfig.substr(0,i);
    string mode = aDeviceConfig.substr(i+1,string::npos);
    // Still handle old-style inverting with !-prefixed mode (because Web-UI created those, we don't want to break them)
    if (mode[0]=='!') {
      ioname.insert(0, "/");
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
    } else if (mode=="blind") {
      // ioname = "<upPinSpec>:<downPinSpec>"
      size_t t = ioname.find(":");
      if (t != string::npos) {
        digitalIoType = digitalio_blind;
        upName = ioname.substr(0, t);
        downName = ioname.substr(t+1, string::npos);
      } else {
        LOG(LOG_ERR, "Illegal output specification for blinds: %s", ioname.c_str());
      }
    }
    else {
      LOG(LOG_ERR, "unknown digital IO type: %s", mode.c_str());
    }
  }
  // basically act as black device so we can configure colors
  if (digitalIoType==digitalio_button) {
    primaryGroup = group_black_joker;
    // Standard device settings without scene table
    installSettings();
    // Digital input as button
    buttonInput = ButtonInputPtr(new ButtonInput(ioname.c_str()));
    buttonInput->setButtonHandler(boost::bind(&DigitalIODevice::buttonHandler, this, _1, _2), true);
    // - create one button input
    ButtonBehaviourPtr b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_undefined, buttonElement_center, false, 0, false); // mode not restricted
    b->setHardwareName("digitalin");
    b->setGroup(group_yellow_light); // pre-configure for light
    addBehaviour(b);
  }
  else if (digitalIoType==digitalio_input) {
    primaryGroup = group_black_joker;
    // Standard device settings without scene table
    installSettings();
    // Digital input as binary input (AKM, automation block type)
    digitalInput = DigitalIoPtr(new DigitalIo(ioname.c_str(), false));
    digitalInput->setInputChangedHandler(boost::bind(&DigitalIODevice::inputHandler, this, _1), INPUT_DEBOUNCE_TIME, 0); // edge detection if possible, mainloop idle poll otherwise
    // - create one binary input
    BinaryInputBehaviourPtr b = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*this));
    b->setHardwareInputConfig(binInpType_none, usage_undefined, true, Never);
    b->setHardwareName("digitalin");
    addBehaviour(b);
  }
  else if (digitalIoType==digitalio_light) {
    // Digital output as light on/off switch
    primaryGroup = group_yellow_light;
    indicatorOutput = IndicatorOutputPtr(new IndicatorOutput(ioname.c_str(), false));
    // - use light settings, which include a scene table
    installSettings(DeviceSettingsPtr(new LightDeviceSettings(*this)));
    // - add simple single-channel light behaviour
    LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
    l->setHardwareOutputConfig(outputFunction_switch, outputmode_binary, usage_undefined, false, -1);
    l->setHardwareName("digitalout");
    addBehaviour(l);
  }
  else if (digitalIoType==digitalio_relay) {
    primaryGroup = group_black_joker;
    // - standard device settings with scene table
    installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*this)));
    // Digital output
    indicatorOutput = IndicatorOutputPtr(new IndicatorOutput(ioname.c_str(), false));
    // - add generic output behaviour
    OutputBehaviourPtr o = OutputBehaviourPtr(new OutputBehaviour(*this));
    o->setHardwareOutputConfig(outputFunction_switch, outputmode_binary, usage_undefined, false, -1);
    o->setHardwareName("digitalout");
    o->setGroupMembership(group_black_joker, true); // put into joker group by default
    o->addChannel(ChannelBehaviourPtr(new DigitalChannel(*o)));
    addBehaviour(o);
  }
  else if (digitalIoType==digitalio_blind) {
    primaryGroup = group_grey_shadow;
    installSettings(DeviceSettingsPtr(new ShadowDeviceSettings(*this)));
    blindsOutputUp = DigitalIoPtr(new DigitalIo(upName.c_str(), true, false));
    blindsOutputDown = DigitalIoPtr(new DigitalIo(downName.c_str(), true, false));
    ShadowBehaviourPtr s = ShadowBehaviourPtr(new ShadowBehaviour(*this));
    s->setHardwareName("dual_digitalout");
    s->setHardwareOutputConfig(outputFunction_positional, outputmode_gradual, usage_room, false, -1);
    s->setDeviceParams(shadowdevice_rollerblind, false, 500*MilliSecond);
    s->position->setFullRangeTime(40*Second);
    s->position->syncChannelValue(100); // assume fully up at beginning
    addBehaviour(s);
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


void DigitalIODevice::inputHandler(bool aNewState)
{
  BinaryInputBehaviourPtr b = boost::dynamic_pointer_cast<BinaryInputBehaviour>(binaryInputs[0]);
  if (b) {
    b->updateInputState(aNewState);
  }
}


void DigitalIODevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  LightBehaviourPtr lightBehaviour = boost::dynamic_pointer_cast<LightBehaviour>(output);
  ShadowBehaviourPtr shadowBehaviour = boost::dynamic_pointer_cast<ShadowBehaviour>(output);
  if (lightBehaviour) {
    // light
    if (lightBehaviour->brightnessNeedsApplying()) {
      indicatorOutput->set(lightBehaviour->brightnessForHardware());
      lightBehaviour->brightnessApplied(); // confirm having applied the value
    }
  }
  else if (shadowBehaviour) {
    // ask shadow behaviour to start movement sequence
    shadowBehaviour->applyBlindChannels(boost::bind(&DigitalIODevice::changeMovement, *this, _1, _2), aDoneCB, aForDimming);
    return;
  }
  else if (output) {
    // simple switch output, activates at 50% of possible output range
    ChannelBehaviourPtr ch = output->getChannelByIndex(0);
    if (ch->needsApplying()) {
      indicatorOutput->set(ch->getChannelValue() >= (ch->getMax()-ch->getMin())/2);
      ch->channelValueApplied();
    }
  }
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void DigitalIODevice::syncChannelValues(SimpleCB aDoneCB)
{
  ShadowBehaviourPtr sb = boost::dynamic_pointer_cast<ShadowBehaviour>(output);
  if (sb) {
    sb->syncBlindState();
  }
  if (aDoneCB) aDoneCB();
}


void DigitalIODevice::dimChannel(DsChannelType aChannelType, DsDimMode aDimMode)
{
  // start dimming
  ShadowBehaviourPtr sb = boost::dynamic_pointer_cast<ShadowBehaviour>(output);
  if (sb) {
    // no channel check, there's only global dimming of the blind, no separate position/angle
    sb->dimBlind(boost::bind(&DigitalIODevice::changeMovement, *this, _1, _2), aDimMode);
  } else {
    inherited::dimChannel(aChannelType, aDimMode);
  }
}


void DigitalIODevice::changeMovement(SimpleCB aDoneCB, int aNewDirection)
{
  if (aNewDirection == 0) {
    // stop
    blindsOutputUp->set(false);
    blindsOutputDown->set(false);
  } else if (aNewDirection > 0) {
    blindsOutputDown->set(false);
    blindsOutputUp->set(true);
  } else {
    blindsOutputUp->set(false);
    blindsOutputDown->set(true);
  }
  if (aDoneCB) {
    aDoneCB();
  }
}

string DigitalIODevice::blindsName() const
{
  if (blindsOutputUp && blindsOutputDown) {
    return string_format("%s+%s", blindsOutputUp->getName().c_str(), blindsOutputDown->getName().c_str());
  }
  return "";
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
  if (digitalInput) { s += ":"; s += digitalInput->getName(); }
  if (blindsOutputUp && blindsOutputDown) { s += ":"; s += blindsName(); }
  dSUID.setNameInSpace(s, vdcNamespace);
}


string DigitalIODevice::modelName()
{
  switch (digitalIoType) {
    case digitalio_button: return "Button digital input";
    case digitalio_input: return "Binary digital input";
    case digitalio_light: return "Light controlling output";
    case digitalio_relay: return "Relay controlling output";
    case digitalio_blind: return "Blind controlling output";
    default: return "Digital I/O";
  }
}


string DigitalIODevice::getExtraInfo()
{
  if (buttonInput)
    return string_format("Button: %s\n", buttonInput->getName().c_str());
  else if (digitalInput)
    return string_format("Input: %s\n", digitalInput->getName().c_str());
  else if (indicatorOutput)
    return string_format("Output: %s\n", indicatorOutput->getName().c_str());
  else if (blindsOutputUp && blindsOutputDown)
    return string_format("Outputs: %s\n", blindsName().c_str());
  else
    return "?";
}



string DigitalIODevice::description()
{
  string s = inherited::description();
  if (buttonInput)
    string_format_append(s, "\n- Button at Digital IO '%s'", buttonInput->getName().c_str());
  if (digitalInput)
    string_format_append(s, "\n- Input at Digital IO '%s'", digitalInput->getName().c_str());
  if (indicatorOutput)
    string_format_append(s, "\n- Switch output at Digital IO '%s'", indicatorOutput->getName().c_str());
  if (blindsOutputUp && blindsOutputDown)
    string_format_append(s, "\n Blinds output at Digital IO %s", blindsName().c_str());
  return s;
}


#endif // ENABLE_STATIC

