//
//  staticdevicecontainer.cpp
//  vdcd
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "staticdevicecontainer.hpp"

#include "digitaliodevice.hpp"
#include "consoledevice.hpp"

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
  removeDevices(false);
	// create devices from configs
	for (DeviceConfigMap::iterator pos = deviceConfigs.begin(); pos!=deviceConfigs.end(); ++pos) {
		// create device of appropriate class
		DevicePtr newDev;
		if (pos->first=="digitalio") {
			// Digital IO based device
			newDev = DevicePtr(new DigitalIODevice(this, pos->second));
		}
    else if (pos->first=="console") {
      // console based simulated device
			newDev = DevicePtr(new ConsoleDevice(this, pos->second));
    }
		if (newDev) {
			// add to container
			addDevice(newDev);
		}
	}
  // assume ok
  aCompletedCB(ErrorPtr());
}

