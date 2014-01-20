//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "staticdevicecontainer.hpp"

#include "digitaliodevice.hpp"
#include "consoledevice.hpp"
#include "sparkiodevice.hpp"

using namespace p44;


StaticDeviceContainer::StaticDeviceContainer(int aInstanceNumber, DeviceConfigMap aDeviceConfigs, DeviceContainer *aDeviceContainerP) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP),
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
void StaticDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive)
{
  // incrementally collecting static devices makes no sense. The devices are "static"!
  if (!aIncremental) {
    // non-incremental, re-collect all devices
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
      else if (pos->first=="spark") {
        // spark core based device
        newDev = DevicePtr(new SparkIoDevice(this, pos->second));
      }
      if (newDev) {
        // add to container
        addDevice(newDev);
      }
    }
  }
  // assume ok
  aCompletedCB(ErrorPtr());
}

