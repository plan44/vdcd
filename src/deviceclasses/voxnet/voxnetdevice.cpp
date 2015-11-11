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

#include "voxnetdevice.hpp"

#if ENABLE_VOXNET


#include "audiobehaviour.hpp"


using namespace p44;


#pragma mark - VoxnetDevice


VoxnetDevice::VoxnetDevice(VoxnetDeviceContainer *aClassContainerP, const string aVoxnetRoomID) :
  inherited(aClassContainerP),
  voxnetRoomID(aVoxnetRoomID)
{
  // audio device
  primaryGroup = group_cyan_audio;
  // just color light settings, which include a color scene table
  installSettings(DeviceSettingsPtr(new AudioDeviceSettings(*this)));
  // - add audio device behaviour
  AudioBehaviourPtr a = AudioBehaviourPtr(new AudioBehaviour(*this));
  addBehaviour(a);
  // - create dSUID
  deriveDsUid();
}



VoxnetDeviceContainer &VoxnetDevice::getVoxnetDeviceContainer()
{
  return *(static_cast<VoxnetDeviceContainer *>(classContainerP));
}


void VoxnetDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  // disconnection is immediate, so we can call inherited right now
  inherited::disconnect(aForgetParams, aDisconnectResultHandler);
}


void VoxnetDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void VoxnetDevice::processVoxnetStatus(const string aVoxnetStatus)
{
  ALOG(LOG_INFO, "Status: %s", aVoxnetStatus.c_str());
}




void VoxnetDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = voxnetRoomID (which is MAC-derived and should be globally unique)
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  dSUID.setNameInSpace(voxnetRoomID, vdcNamespace);
}


string VoxnetDevice::hardwareGUID()
{
  return string_format("voxnetdeviceid:%s", voxnetRoomID.c_str());
}


string VoxnetDevice::vendorName()
{
  return "Revox";
}



string VoxnetDevice::modelName()
{
  return "Voxnet device";
}



bool VoxnetDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  const char *iconName = "voxnet";
  if (iconName && getIcon(iconName, aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


string VoxnetDevice::getExtraInfo()
{
  string s;
  s = string_format("Voxnet device %s", voxnetRoomID.c_str());
  return s;
}



string VoxnetDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "\n- Voxnet device %s", voxnetRoomID.c_str());
  return s;
}


#endif // ENABLE_VOXNET



