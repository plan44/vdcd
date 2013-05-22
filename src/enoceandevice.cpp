//
//  enoceandevice.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 07.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "enoceandevice.hpp"


using namespace p44;


EnoceanDevice::EnoceanDevice(EnoceanDeviceContainer *aClassContainerP) :
  Device((DeviceClassContainer *)aClassContainerP),
  eeProfile(eep_profile_unknown),
  eeManufacturer(manufacturer_unknown)
{
}


EnoceanAddress EnoceanDevice::getEnoceanAddress()
{
  return enoceanAddress;
}


void EnoceanDevice::setEnoceanAddress(EnoceanAddress aAddress)
{
  enoceanAddress = aAddress;
  deriveDSID();
}


void EnoceanDevice::setEEPInfo(EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer)
{
  eeProfile = aEEProfile;
  eeManufacturer = aEEManufacturer;
}


EnoceanProfile EnoceanDevice::getEEProfile()
{
  return eeProfile;
}


EnoceanManufacturer EnoceanDevice::getEEManufacturer()
{
  return eeManufacturer;
}



void EnoceanDevice::deriveDSID()
{
  dsid.setObjectClass(DSID_OBJECTCLASS_MACADDRESS);
  // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 6
  dsid.setSerialNo(0x6000000000000ll+getEnoceanAddress());
}



string EnoceanDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "Enocean Address = 0x%08lX\n", enoceanAddress);
  return s;
}
