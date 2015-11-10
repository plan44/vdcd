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

#include "vzughomedevice.hpp"

#if ENABLE_VZUGHOME


#include "audiobehaviour.hpp"


using namespace p44;


#pragma mark - LedChainDevice


VZugHomeDevice::VZugHomeDevice(VZugHomeDeviceContainer *aClassContainerP) :
  inherited(aClassContainerP)
{
  // - create dSUID
  deriveDsUid();
}



VZugHomeDeviceContainer &VZugHomeDevice::getVZugHomeDeviceContainer()
{
  return *(static_cast<VZugHomeDeviceContainer *>(classContainerP));
}


void VZugHomeDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  //  // clear learn-in data from DB
  //  if (ledChainDeviceRowID) {
  //    getLedChainDeviceContainer().db.executef("DELETE FROM devConfigs WHERE rowid=%d", ledChainDeviceRowID);
  //  }
  // disconnection is immediate, so we can call inherited right now
  inherited::disconnect(aForgetParams, aDisconnectResultHandler);
}


void VZugHomeDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void VZugHomeDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //  //   UUIDv5 with name = classcontainerinstanceid::ledchainType:firstLED:lastLED
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  string_format_append(s, "%s", %%%deviceid);
  dSUID.setNameInSpace(s, vdcNamespace);
}


string VZugHomeDevice::modelName()
{
  return "V-Zug Home device";
}



bool VZugHomeDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  const char *iconName = "vzughome";
  if (iconName && getIcon(iconName, aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


string VZugHomeDevice::getExtraInfo()
{
  string s;
  s = string_format("V-Zug Home device");
  return s;
}



string VZugHomeDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "- V-Zug Home device\n");
  return s;
}


#endif // ENABLE_VZUGHOME



