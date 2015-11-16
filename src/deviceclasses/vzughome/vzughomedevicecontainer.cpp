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

#include "vzughomedevicecontainer.hpp"

#if ENABLE_VZUGHOME

#include "vzughomedevice.hpp"

using namespace p44;



#pragma mark - DB and initialisation


VZugHomeDeviceContainer::VZugHomeDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP, aTag)
{
}


void VZugHomeDeviceContainer::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
  // done
  aCompletedCB(ErrorPtr());
}



bool VZugHomeDeviceContainer::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("vzughome", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


// device class name
const char *VZugHomeDeviceContainer::deviceClassIdentifier() const
{
  return "VZugHome_Device_Container";
}



#if !DEBUG
#error TESTHACK
#else
//#define TESTHACK 1
#endif

void VZugHomeDeviceContainer::collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings)
{
  // incrementally collecting static devices makes no sense. The devices are "static"!
  if (!aIncremental) {
    removeDevices(aClearSettings);
  }
  VZugHomeDiscoveryPtr discovery = VZugHomeDiscoveryPtr(new VZugHomeDiscovery);
  #ifdef TESTHACK
  discovery->baseURLs.clear();
  discovery->baseURLs.push_back("http://localhost/vzug.php");
  discoveryStatusHandler(discovery, aCompletedCB, ErrorPtr());
  return;
  #endif
  discovery->discover(boost::bind(&VZugHomeDeviceContainer::discoveryStatusHandler, this, discovery, aCompletedCB, _1), 15*Second);
}



void VZugHomeDeviceContainer::discoveryStatusHandler(VZugHomeDiscoveryPtr aDiscovery, StatusCB aCompletedCB, ErrorPtr aError)
{
  StringList::iterator pos = aDiscovery->baseURLs.begin();
  if (Error::isOK(aError)) {
    addNextDevice(aDiscovery, pos, aCompletedCB);
    return;
  }
  // report error
  aCompletedCB(aError);
}



void VZugHomeDeviceContainer::addNextDevice(VZugHomeDiscoveryPtr aDiscovery, StringList::iterator aNext, StatusCB aCompletedCB)
{
  if (aNext!=aDiscovery->baseURLs.end()) {
    LOG(LOG_NOTICE, "V-Zug home device with API at %s", aNext->c_str());
    VZugHomeDevicePtr newDev = VZugHomeDevicePtr(new VZugHomeDevice(this, *aNext));
    newDev->queryDeviceInfos(boost::bind(&VZugHomeDeviceContainer::gotDeviceInfos, this, newDev, aDiscovery, aNext, aCompletedCB));
  }
  else {
    // all collected
    aCompletedCB(ErrorPtr());
  }
}


void VZugHomeDeviceContainer::gotDeviceInfos(VZugHomeDevicePtr aNewDev, VZugHomeDiscoveryPtr aDiscovery, StringList::iterator aNext, StatusCB aCompletedCB)
{
  if (aNewDev) {
    if (addDevice(aNewDev)) {
      // actually added, no duplicate
    }
  }
  aNext++;
  addNextDevice(aDiscovery, aNext, aCompletedCB);
}





ErrorPtr VZugHomeDeviceContainer::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
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

#endif // ENABLE_VZUGHOME



