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


#include "outputbehaviour.hpp"


using namespace p44;


#pragma mark - VZugHomeDevice


// MARK: gugus

VZugHomeDevice::VZugHomeDevice(VZugHomeDeviceContainer *aClassContainerP, const string aBaseURL) :
  inherited(aClassContainerP)
{
  vzugHomeComm.baseURL = aBaseURL;
  setPrimaryGroup(group_black_joker); // TODO: what is the correct color for whiteware?
  installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*this)));
  // - set the behaviour
  OutputBehaviourPtr o = OutputBehaviourPtr(new OutputBehaviour(*this));
  o->setHardwareOutputConfig(outputFunction_switch, outputmode_binary, usage_undefined, false, -1);
  o->setGroupMembership(group_black_joker, true); // TODO: what is the correct color for whiteware?
  o->addChannel(ChannelBehaviourPtr(new DigitalChannel(*o)));
  addBehaviour(o);
}


void VZugHomeDevice::queryDeviceInfos(StatusCB aCompletedCB)
{
  // query model ID
  vzugHomeComm.apiAction("/hh?command=getModel", JsonObjectPtr(), false, boost::bind(&VZugHomeDevice::gotModelId, this, aCompletedCB, _1, _2));
}


void VZugHomeDevice::gotModelId(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aResult) {
      modelId = aResult->stringValue();
      vzugHomeComm.apiAction("/hh?command=getModelDescription", JsonObjectPtr(), false, boost::bind(&VZugHomeDevice::gotModelDescription, this, aCompletedCB, _1, _2));
      return;
    }
    aError = TextError::err("no model ID");
  }
  // early fail
  aCompletedCB(aError);
}


void VZugHomeDevice::gotModelDescription(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aResult) {
      modelDesc = aResult->stringValue();
      vzugHomeComm.apiAction("/hh?command=getSerialNumber", JsonObjectPtr(), false, boost::bind(&VZugHomeDevice::gotSerialNumber, this, aCompletedCB, _1, _2));
      return;
    }
    aError = TextError::err("no model description");
  }
  // early fail
  aCompletedCB(aError);
}


void VZugHomeDevice::gotSerialNumber(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aResult) {
      serialNo = aResult->stringValue();
      deriveDsUid(); // dSUID bases on modelId and serial number
      vzugHomeComm.apiAction("/hh?command=getDeviceName", JsonObjectPtr(), false, boost::bind(&VZugHomeDevice::gotDeviceName, this, aCompletedCB, _1, _2));
      return;
    }
    aError = TextError::err("no serial number");
  }
  // early fail
  aCompletedCB(aError);
}


void VZugHomeDevice::gotDeviceName(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aResult) {
      initializeName(aResult->stringValue());
    }
  }
  // end of init in all cases
  aCompletedCB(aError);
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
  // UUIDv5 with name = deviceClassIdentifier:modelid:serialno
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassIdentifier();
  string_format_append(s, "%s:%s", modelId.c_str(), serialNo.c_str());
  dSUID.setNameInSpace(s, vdcNamespace);
}


string VZugHomeDevice::vendorName()
{
  return "V-Zug";
}



string VZugHomeDevice::modelName()
{
  return modelDesc;
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
  string_format_append(s, "\n- V-Zug Home device");
  return s;
}


#endif // ENABLE_VZUGHOME



