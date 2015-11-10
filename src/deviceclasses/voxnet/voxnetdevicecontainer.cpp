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

#include "vzughomedevice.hpp"

using namespace p44;



#pragma mark - DB and initialisation


VoxnetDeviceContainer::VoxnetDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP, aTag)
{
  voxnetComm = VoxnetCommPtr(new VoxnetComm);
}


void VoxnetDeviceContainer::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
  // initialize the communication
  voxnetComm->initialize(aCompletedCB);
}



bool VoxnetDeviceContainer::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("vdc_voxnet", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


// device class name
const char *VoxnetDeviceContainer::deviceClassIdentifier() const
{
  return "Voxnet_Device_Container";
}


void VoxnetDeviceContainer::collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings)
{
  // incrementally collecting static devices makes no sense. The devices are "static"!
  if (!aIncremental) {
    //    // non-incremental, re-collect all devices
    //    removeDevices(aClearSettings);
    //    // then add those from the DB
    //    sqlite3pp::query qry(db);
    //    if (qry.prepare("SELECT firstLED, numLEDs, deviceconfig, rowid FROM devConfigs ORDER BY firstLED")==SQLITE_OK) {
    //      for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
    //        LedChainDevicePtr dev =addLedChainDevice(i->get<int>(0), i->get<int>(1), i->get<string>(2));
    //        dev->ledChainDeviceRowID = i->get<int>(3);
    //      }
    //    }
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



