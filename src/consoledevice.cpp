//
//  consoledevice.cpp
//  vdcd
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "consoledevice.hpp"

#include "fnv.hpp"

#include "buttonbehaviour.hpp"
#include "lightbehaviour.hpp"

using namespace p44;


ConsoleDevice::ConsoleDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  Device((DeviceClassContainer *)aClassContainerP),
  hasButton(false),
  hasOutput(false),
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
  }
  // assign name
  initializeName(name);
  // create I/O
  if (hasOutput) {
    // Simulate light device
    // - defaults to yellow (light)
    primaryGroup = group_yellow_light;
    // - use light settings, which include a scene table
    deviceSettings = DeviceSettingsPtr(new LightDeviceSettings(*this));
    // - create one output
    LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
    l->setHardwareOutputConfig(outputFunction_dimmer, usage_undefined, true, -1);
    l->setHardwareName("console output");
    addBehaviour(l);
  }
  else if (hasButton) {
    // Simulate Button device
    // - defaults to black (generic button)
    primaryGroup = group_black_joker;
    // - console key input as button
    consoleKey = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(name[0], name.c_str());
    consoleKey->setConsoleKeyHandler(boost::bind(&ConsoleDevice::buttonHandler, this, _2, _3));
    // - create one button input
    ButtonBehaviourPtr b = ButtonBehaviourPtr(new ButtonBehaviour(*this));
    b->setHardwareButtonConfig(0, buttonType_single, buttonElement_center, false, 0);
    b->setHardwareName(string_format("console key '%c'",name[0]));
    addBehaviour(b);
  }
	deriveDSID();
}


void ConsoleDevice::buttonHandler(bool aState, MLMicroSeconds aTimestamp)
{
	ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(buttons[0]);
	if (b) {
		b->buttonAction(aState);
	}
}



void ConsoleDevice::updateOutputValue(OutputBehaviour &aOutputBehaviour)
{
  if (aOutputBehaviour.getIndex()==0) {
    outputValue = aOutputBehaviour.valueForHardware();
    printf(
      ">>> Console device %s: output set to %d, transition time = %0.3f Seconds\n",
      getName().c_str(), outputValue,
      (double)aOutputBehaviour.transitionTimeForHardware()/Second
    );
  }
  else
    return inherited::updateOutputValue(aOutputBehaviour); // let superclass handle this
}



void ConsoleDevice::deriveDSID()
{
  Fnv64 hash;
	
	// we have no unqiquely defining device information, construct something as reproducible as possible
	// - use class container's ID
	string s = classContainerP->deviceClassContainerInstanceIdentifier();
	hash.addBytes(s.size(), (uint8_t *)s.c_str());
	// - add-in the console device name
  hash.addCStr(getName().c_str());
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


string ConsoleDevice::description()
{
  string s = inherited::description();
  if (hasOutput)
    string_format_append(s, "- has output printing value to console\n");
  if (hasButton)
    string_format_append(s, "- has button which can be switched via console keypresses\n");
  return s;
}
