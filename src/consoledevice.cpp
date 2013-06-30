//
//  consoledevice.cpp
//  p44bridged
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
  name = aDeviceConfig;
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
  if (hasOutput) {
    // Simulate light device
    LightBehaviour *l = new LightBehaviour(this);
    l->setHardwareDimmer(true); // Simulation
    setDSBehaviour(l);
  }
  else if (hasButton) {
    // GPIO input as button
    consoleKey = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(name[0], name.c_str());
    consoleKey->setConsoleKeyHandler(boost::bind(&ConsoleDevice::buttonHandler, this, _2, _3));
    // set the behaviour
    ButtonBehaviour *b = new ButtonBehaviour(this);
    b->setHardwareButtonType(hwbuttontype_1way, false);
    #warning default to SW-TKM for now
    b->setDeviceColor(group_black_joker);
    setDSBehaviour(b);
  }
	deriveDSID();
}


void ConsoleDevice::buttonHandler(bool aNewState, MLMicroSeconds aTimestamp)
{
	ButtonBehaviour *b = dynamic_cast<ButtonBehaviour *>(getDSBehaviour());
	if (b) {
		b->buttonAction(aNewState, false);
	}
}



int16_t ConsoleDevice::getOutputValue(int aChannel)
{
  if (aChannel==0)
    return outputValue;
  else
    return inherited::getOutputValue(aChannel); // let superclass handle this
}



void ConsoleDevice::setOutputValue(int aChannel, int16_t aValue, MLMicroSeconds aTransitionTime)
{
  if (aChannel==0) {
    outputValue = aValue;
    printf(">>> Console device %s: output set to %d\n", name.c_str(), outputValue);
  }
  else
    return inherited::setOutputValue(aChannel, aValue); // let superclass handle this
}



void ConsoleDevice::deriveDSID()
{
  Fnv64 hash;
	
	// we have no unqiquely defining device information, construct something as reproducible as possible
	// - use class container's ID
	string s = classContainerP->deviceClassContainerInstanceIdentifier();
	hash.addBytes(s.size(), (uint8_t *)s.c_str());
	// - add-in the GPIO name
  hash.addCStr(name.c_str());
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
