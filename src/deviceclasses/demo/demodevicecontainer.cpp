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

#include "demodevicecontainer.hpp"

#include "demodevice.hpp"

using namespace p44;


DemoDeviceContainer::DemoDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP)
{
}


// device class name
const char *DemoDeviceContainer::deviceClassIdentifier() const
{
  return "Demo_Device_Container";
}


/// collect devices from this device class
void DemoDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive)
{
  // incrementally collecting Demo devices makes no sense, they are statically created at startup
  if (!aIncremental) {
    // non-incremental, re-collect all devices
    removeDevices(false);
    // create one single demo device
    DevicePtr newDev = DevicePtr(new DemoDevice(this));
    // add to container
    addDevice(newDev);
  }
  // assume ok
  aCompletedCB(ErrorPtr());
}

