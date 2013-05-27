//
//  gpiodevice.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "gpiodevice.hpp"

#include "fnv.hpp"

#include "buttonbehaviour.hpp"

using namespace p44;


GpioDevice::GpioDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  Device((DeviceClassContainer *)aClassContainerP)
{
	buttonInput = ButtonInputPtr(new ButtonInput(aDeviceConfig.c_str(), false));
	buttonInput->setButtonHandler(boost::bind(&GpioDevice::buttonHandler, this, _2, _3), true);
	// set the behaviour
  ButtonBehaviour *b = new ButtonBehaviour(this);
  b->setKeyId(ButtonBehaviour::key_1way); // one-way key
	setDSBehaviour(b);
	deriveDSID();
}


void GpioDevice::buttonHandler(bool aNewState, MLMicroSeconds aTimestamp)
{
	ButtonBehaviour *b = dynamic_cast<ButtonBehaviour *>(getDSBehaviour());
	if (b) {
		b->buttonAction(aNewState);
	}
}


void GpioDevice::deriveDSID()
{
  dsid.setObjectClass(DSID_OBJECTCLASS_MACADDRESS); // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
  Fnv64 hash;
	
	// we have no unqiquely defining device information, construct something as reproducible as possible
	// - use class container's ID
	string s = classContainerP->deviceClassContainerInstanceIdentifier();
	hash.addBytes(s.size(), (uint8_t *)s.c_str());
	// - add-in the GPIO name
	hash.addCStr(buttonInput->getName());
  // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
  dsid.setSerialNo(0x7000000000000ll+hash.getHash48());
}


string GpioDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "- Button at GPIO %s\n", buttonInput->getName());
  return s;
}
