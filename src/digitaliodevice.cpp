//
//  digitaliodevice.cpp
//  vdcd
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
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
    // Digital output as on/off switch
    indicatorOutput = IndicatorOutputPtr(new IndicatorOutput(ioname.c_str(), inverted, false));
    // Simulate light device
    // - create one output
    LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
    l->setHardwareOutputConfig(outputFunction_switch, usage_undefined, false, -1);
    addBehaviour(l);
  }
  else {
    // Digital input as button
    buttonInput = ButtonInputPtr(new ButtonInput(ioname.c_str(), inverted));
    buttonInput->setButtonHandler(boost::bind(&DigitalIODevice::buttonHandler, this, _2, _3), true);
    // - create one button input
    ButtonBehaviourPtr b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_single, buttonElement_center, false, 0);
    addBehaviour(b);
  }
	deriveDSID();
}


void DigitalIODevice::buttonHandler(bool aNewState, MLMicroSeconds aTimestamp)
{
	ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(buttons[0]);
	if (b) {
		b->buttonAction(aNewState);
	}
}



void DigitalIODevice::updateOutputValue(OutputBehaviour &aOutputBehaviour)
{
  if (aOutputBehaviour.getIndex()==0 && indicatorOutput) {
    indicatorOutput->set(aOutputBehaviour.valueForHardware()>0);
  }
  else
    return inherited::updateOutputValue(aOutputBehaviour); // let superclass handle this
}



void DigitalIODevice::deriveDSID()
{
  Fnv64 hash;
	
	// we have no unqiquely defining device information, construct something as reproducible as possible
	// - use class container's ID
	string s = classContainerP->deviceClassContainerInstanceIdentifier();
	hash.addBytes(s.size(), (uint8_t *)s.c_str());
	// - add-in the DigitalIO name
  if (buttonInput)
    hash.addCStr(buttonInput->getName());
  if (indicatorOutput)
    hash.addCStr(indicatorOutput->getName());
  #if FAKE_REAL_DSD_IDS
  dsid.setObjectClass(DSID_OBJECTCLASS_DSDEVICE);
  dsid.setSerialNo(hash.getHash28()<<4); // leave lower 4 bits for input number
  #warning "TEST ONLY: faking digitalSTROM device addresses, possibly colliding with real devices"
  #else
  dsid.setObjectClass(DSID_OBJECTCLASS_MACADDRESS); // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
  dsid.setSerialNo(0x7000000000000ll+hash.getHash48());
  #endif
  // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
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
