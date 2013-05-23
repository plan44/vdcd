//
//  staticdevicecontainer.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "staticdevicecontainer.hpp"

#include "gpiodevice.hpp"

using namespace p44;


StaticDeviceContainer::StaticDeviceContainer(int aInstanceNumber, DeviceConfigMap aDeviceConfigs) :
  DeviceClassContainer(aInstanceNumber),
	deviceConfigs(aDeviceConfigs)
{
}



// device class name
const char *StaticDeviceContainer::deviceClassIdentifier() const
{
  return "Static_Device_Container";
}



/// collect devices from this device class
/// @param aCompletedCB will be called when device scan for this device class has been completed
void StaticDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aExhaustive)
{
  forgetDevices();
	// create devices from configs
	for (DeviceConfigMap::iterator pos = deviceConfigs.begin(); pos!=deviceConfigs.end(); ++pos) {
		// create device of appropriate class
		DevicePtr newDev;
		if (pos->first=="gpio") {
			// GPIO based device
			newDev = DevicePtr(new GpioDevice(this, pos->second));
		}
		if (newDev) {
			// add to container
			addDevice(newDev);
		}
	}
}

