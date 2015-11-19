//
//  Copyright (c) 2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "voxnetdevicecontainer.hpp"

#if ENABLE_VOXNET

#include "voxnetdevice.hpp"

using namespace p44;



#pragma mark - DB and initialisation


VoxnetDeviceContainer::VoxnetDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP, aTag)
{
  voxnetComm = VoxnetCommPtr(new VoxnetComm);
}


void VoxnetDeviceContainer::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
  // set the status handler
  voxnetComm->setVoxnetStatusHandler(boost::bind(&VoxnetDeviceContainer::voxnetStatusHandler, this, _1, _2));
  // initialize the communication
  voxnetComm->initialize(aCompletedCB);
}


void VoxnetDeviceContainer::voxnetStatusHandler(const string aVoxnetID, const string aVoxnetStatus)
{
  // dispatch status to all devices
  for (VoxnetDeviceMap::iterator pos = voxnetDevices.begin(); pos!=voxnetDevices.end(); ++pos) {
    // have device process the status
    pos->second->processVoxnetStatus(aVoxnetID, aVoxnetStatus);
  }
}




bool VoxnetDeviceContainer::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("voxnet", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


// device class name
const char *VoxnetDeviceContainer::deviceClassIdentifier() const
{
  return "Voxnet_Device_Container";
}



VoxnetDevicePtr VoxnetDeviceContainer::addVoxnetDevice(const string aID, const string aName)
{
  VoxnetDevicePtr newDev = VoxnetDevicePtr(new VoxnetDevice(this, aID));
  if (newDev) {
    newDev->initializeName(aName);
    // add to container
    addDevice(newDev);
    // add to my own list
    voxnetDevices[aID] = newDev;
    return newDev;
  }
  // none added
  return VoxnetDevicePtr();
}


void VoxnetDeviceContainer::collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings)
{
  // incrementally collecting devices makes no sense.
  if (!aIncremental) {
    // remove all
    removeDevices(aClearSettings);
    voxnetDevices.clear();
    // then create devices from rooms
    for (VoxnetComm::StringStringMap::iterator pos=voxnetComm->rooms.begin(); pos!=voxnetComm->rooms.end(); ++pos) {
      VoxnetDevicePtr newDev = VoxnetDevicePtr(new VoxnetDevice(this, pos->first));
      newDev->initializeName(pos->second);
      voxnetDevices[pos->first] = newDev;
      addDevice(newDev);
    }
  }
  // assume ok
  aCompletedCB(ErrorPtr());
}


ErrorPtr VoxnetDeviceContainer::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  //  if (aMethod=="x-p44-addDevice") {
  //  }
  //  else
  {
    respErr = inherited::handleMethod(aRequest, aMethod, aParams);
  }
  return respErr;
}

#endif // ENABLE_VOXNET



