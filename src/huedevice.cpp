//
//  huedevice.cpp
//  vdcd
//
//  Created by Lukas Zeller on 02.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "huedevice.hpp"
#include "huedevicecontainer.hpp"

#include "fnv.hpp"

#include "lightbehaviour.hpp"

using namespace p44;


HueDevice::HueDevice(HueDeviceContainer *aClassContainerP, int aLampNumber) :
  inherited(aClassContainerP),
  lampNumber(aLampNumber)
{
}


void HueDevice::updateOutputValue(OutputBehaviour &aOutputBehaviour)
{
  return inherited::updateOutputValue(aOutputBehaviour); // let superclass handle this
}



void HueDevice::deriveDSID()
{
  Fnv64 hash;
	
	// we have no unqiquely defining device information, construct something as reproducible as possible
	// - use class container's ID
	string s = classContainerP->deviceClassContainerInstanceIdentifier();
	hash.addBytes(s.size(), (uint8_t *)s.c_str());
	// - add-in the console device name
  #warning "TEST ONLY: lampNumber is only semi-clever basis for a hash!!!"
  hash.addBytes(sizeof(lampNumber), (uint8_t *)&lampNumber);
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


string HueDevice::description()
{
  string s = inherited::description();
  return s;
}
