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

#include "dsaddressable.hpp"

#include "devicecontainer.hpp"

using namespace p44;



DsAddressable::DsAddressable(DeviceContainer *aDeviceContainerP) :
  deviceContainerP(aDeviceContainerP),
  announced(Never),
  announcing(Never)
{
}


DsAddressable::~DsAddressable()
{
}


void DsAddressable::setName(const string &aName)
{
  // TODO: for now dsm API truncates names to 20 bytes. Therefore,
  //   we prevent replacing a long name with a truncated version
  if (name!=aName && (name.length()<20 || name.substr(0,20)!=aName)) {
    name = aName;
  }
}


void DsAddressable::initializeName(const string &aName)
{
  // just assign
  name = aName;
}



#pragma mark - vDC API



ErrorPtr DsAddressable::checkParam(ApiValuePtr aParams, const char *aParamName, ApiValuePtr &aParam)
{
  ErrorPtr err;
  if (aParams)
    aParam = aParams->get(aParamName);
  else
    aParam.reset();
  if (!aParam)
    err = ErrorPtr(new VdcApiError(400, string_format("Invalid Parameters - missing '%s'",aParamName)));
  return err;
}


ErrorPtr DsAddressable::checkStringParam(ApiValuePtr aParams, const char *aParamName, string &aParamValue)
{
  ErrorPtr err;
  ApiValuePtr o;
  err = checkParam(aParams, aParamName, o);
  if (Error::isOK(err)) {
    aParamValue = o->stringValue();
  }
  return err;
}



ErrorPtr DsAddressable::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  string name;
  int arrayIndex = 0; // default to start at first element
  int rangeSize = PROP_ARRAY_SIZE; // entire array by default if no "index" or "offset"/"count" is given
  ApiValuePtr o;
  if (aMethod=="getProperty") {
    // name must be present
    if (Error::isOK(respErr = checkStringParam(aParams, "name", name))) {
      // get optional index
      o = aParams->get("index");
      if (o) {
        arrayIndex = o->int32Value();
        rangeSize = 0; // single element by default, unless count is explicitly specified
      }
      else {
        o = aParams->get("offset");
        if (o) {
          // range access
          arrayIndex = o->int32Value(); // same as index for lower level property access mechanism
        }
      }
      // check optional max count of elements
      o = aParams->get("count");
      if (o) {
        rangeSize = o->int32Value();
      }
      // now read
      ApiValuePtr result = aRequest->connection()->newApiValue();
      respErr = accessProperty(false, result, name, VDC_API_DOMAIN, arrayIndex, rangeSize);
      if (Error::isOK(respErr)) {
        // send back property result
        aRequest->sendResult(result);
      }
    }
  }
  else if (aMethod=="setProperty") {
    // name must be present
    if (Error::isOK(respErr = checkStringParam(aParams, "name", name))) {
      // value must be present
      ApiValuePtr value;
      if (Error::isOK(respErr = checkParam(aParams, "value", value))) {
        // get optional index
        rangeSize = 1; // default to single element access
        arrayIndex = 0; // default to first element
        o = aParams->get("index");
        if (!o) o = aParams->get("offset");
        if (o) {
          arrayIndex = o->int32Value();
          // - check optional max count of elements to fill with SAME VALUE
          o = aParams->get("count");
          if (o) {
            rangeSize = o->int32Value();
          }
        }
        // now write (possibly batch to multiple elements if rangeSize>1. rangeSize==0 && rangeSize==1 is single element write)
        if (rangeSize==PROP_ARRAY_SIZE)
          respErr = ErrorPtr(new VdcApiError(400, "array batch write needs offset AND count"));
        else {
          do {
            respErr = accessProperty(true, value, name, VDC_API_DOMAIN, arrayIndex, 0);
            arrayIndex++;
          } while (--rangeSize>0 && Error::isOK(respErr));
        }
        if (Error::isOK(respErr)) {
          // send back OK if write was successful
          aRequest->sendResult(ApiValuePtr());
        }
      }
    }
  }
  else if (aMethod=="getUserProperty") {
    // Shortcut access to some specific "user" properties
    // TODO: maybe remove this once real named properties are implemented in vdSM/ds485
    ApiValuePtr propindex;
    if (Error::isOK(respErr = checkParam(aParams, "index", propindex))) {
      int userPropIndex = propindex->int32Value();
      // look up name and index of real property
      if (Error::isOK(respErr = getUserPropertyMapping(userPropIndex, name, arrayIndex))) {
        // this dsAdressable supports this user property index, read it
        ApiValuePtr result;
        respErr = accessProperty(false, result, name, VDC_API_DOMAIN, arrayIndex, 0); // always single element
        if (Error::isOK(respErr)) {
          // send back property result
          aRequest->sendResult(result);
        }
      }
    }
  }
  else if (aMethod=="setUserProperty") {
    // Shortcut access to some specific "user" properties
    // TODO: maybe remove this once real named properties are implemented in vdSM/ds485
    ApiValuePtr propindex;
    ApiValuePtr value;
    if (Error::isOK(respErr = checkParam(aParams, "value", value))) {
      if (Error::isOK(respErr = checkParam(aParams, "index", propindex))) {
        int userPropIndex = propindex->int32Value();
        // look up name and index of real property
        if (Error::isOK(respErr = getUserPropertyMapping(userPropIndex, name, arrayIndex))) {
          // this dsAdressable supports this user property index, write it
          respErr = accessProperty(true, value, name, VDC_API_DOMAIN, arrayIndex, 0);
          if (Error::isOK(respErr)) {
            // send back OK if write was successful
            aRequest->sendResult(ApiValuePtr());
          }
        }
      }
    }
  }
  else {
    respErr = ErrorPtr(new VdcApiError(405, "unknown method"));
  }
  return respErr;
}


ErrorPtr DsAddressable::getUserPropertyMapping(int aUserPropertyIndex, string &aName, int &aIndex)
{
  // base class implements no user properties
  return ErrorPtr(new VdcApiError(400, string_format("Unknown user property index %d", aUserPropertyIndex)));
}



bool DsAddressable::pushProperty(const string &aName, int aDomain, int aIndex)
{
  VdcApiConnectionPtr api = getDeviceContainer().getSessionConnection();
  if (api) {
    // get the value
    ApiValuePtr value = api->newApiValue();
    ErrorPtr err = accessProperty(false, value, aName, aDomain, aIndex<0 ? 0 : aIndex, 0);
    if (Error::isOK(err)) {
      ApiValuePtr pushParams = api->newApiValue();
      pushParams->setType(apivalue_object);
      pushParams->add("name", pushParams->newString(aName));
      if (aIndex>=0) {
        // array property push
        pushParams->add("index", pushParams->newInt64(aIndex));
      }
      pushParams->add("value", value);
      return sendRequest("pushProperty", pushParams);
    }
  }
  return false;
}



void DsAddressable::handleNotification(const string &aMethod, ApiValuePtr aParams)
{
  if (aMethod=="ping") {
    // issue device ping (which will issue a pong when device is reachable)
    LOG(LOG_INFO,"ping: %s -> checking presence...\n", shortDesc().c_str());
    checkPresence(boost::bind(&DsAddressable::presenceResultHandler, this, _1));
  }
  else {
    // unknown notification
    LOG(LOG_WARNING, "unknown notification '%s' for '%s'\n", aMethod.c_str(), shortDesc().c_str());
  }
}


bool DsAddressable::sendRequest(const char *aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler)
{
  VdcApiConnectionPtr api = getDeviceContainer().getSessionConnection();
  if (api) {
    if (!aParams) {
      // create params object because we need it for the dSUID
      aParams = api->newApiValue();
      aParams->setType(apivalue_object);
    }
    aParams->add("dSUID", aParams->newString(dSUID.getString()));
    return getDeviceContainer().sendApiRequest(aMethod, aParams, aResponseHandler);
  }
  return false; // no connection
}



void DsAddressable::presenceResultHandler(bool aIsPresent)
{
  if (aIsPresent) {
    // send back Pong notification
    LOG(LOG_INFO,"ping: %s is present -> sending pong\n", shortDesc().c_str());
    sendRequest("pong", ApiValuePtr());
  }
  else {
    LOG(LOG_NOTICE,"ping: %s is NOT present -> no Pong sent\n", shortDesc().c_str());
  }
}



#pragma mark - interaction with subclasses


void DsAddressable::checkPresence(PresenceCB aPresenceResultHandler)
{
  // base class just assumes being present
  aPresenceResultHandler(true);
}


#pragma mark - property access

enum {
  dSUID_key,
  model_key,
  hardwareVersion_key,
  hardwareGUID_key,
  numDevicesInHW_key,
  deviceIndexInHW_key,
  oemGUID_key,
  name_key,
  numDsAddressableProperties
};


static char dsAddressable_key;

int DsAddressable::numProps(int aDomain)
{
  return inherited::numProps(aDomain)+numDsAddressableProperties;
}


const PropertyDescriptor *DsAddressable::getPropertyDescriptor(int aPropIndex, int aDomain)
{
  static const PropertyDescriptor properties[numDsAddressableProperties] = {
    { "dSUID", apivalue_string, false, dSUID_key, &dsAddressable_key },
    { "model", apivalue_string, false, model_key, &dsAddressable_key },
    { "hardwareVersion", apivalue_string, false, hardwareVersion_key, &dsAddressable_key },
    { "hardwareGuid", apivalue_string, false, hardwareGUID_key, &dsAddressable_key },
    { "numDevicesInHW", apivalue_uint64, false, numDevicesInHW_key, &dsAddressable_key },
    { "deviceIndexInHW", apivalue_uint64, false, deviceIndexInHW_key, &dsAddressable_key },
    { "oemGuid", apivalue_string, false, oemGUID_key, &dsAddressable_key },
    { "name", apivalue_string, false, name_key, &dsAddressable_key }
  };
  int n = inherited::numProps(aDomain);
  if (aPropIndex<n)
    return inherited::getPropertyDescriptor(aPropIndex, aDomain); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return &properties[aPropIndex];
}


bool DsAddressable::accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&dsAddressable_key) {
    if (aForWrite) {
      switch (aPropertyDescriptor.accessKey) {
        case name_key: setName(aPropValue->stringValue()); return true;
      }
    }
    else {
      switch (aPropertyDescriptor.accessKey) {
        case dSUID_key: aPropValue->setStringValue(dSUID.getString()); return true;
        case model_key: aPropValue->setStringValue(modelName()); return true;
        case hardwareVersion_key: if (hardwareVersion().size()>0) { aPropValue->setStringValue(hardwareVersion()); return true; } else return false;
        case hardwareGUID_key: if (hardwareGUID().size()>0) { aPropValue->setStringValue(hardwareGUID()); return true; } else return false;
        case oemGUID_key: if (oemGUID().size()>0) { aPropValue->setStringValue(oemGUID()); return true; } else return false;
        case name_key: aPropValue->setStringValue(getName()); return true;
        // conditionally available
        case numDevicesInHW_key:
          if (numDevicesInHW()>=0) {
            aPropValue->setUint16Value((int)numDevicesInHW());
            return true;
          }
          return false; // no such property
        case deviceIndexInHW_key:
          if (deviceIndexInHW()>=0) {
            aPropValue->setUint16Value((int)deviceIndexInHW());
            return true;
          }
          return false; // no such property
      }
      return true;
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex); // let base class handle it
}




#pragma mark - description/shortDesc

string DsAddressable::shortDesc()
{
  // short description is dSUID
  return dSUID.getString();
}



string DsAddressable::description()
{
  string s = string_format("%s %s", entityType(), shortDesc().c_str());
  if (getName().length()>0)
    string_format_append(s, " named '%s'", getName().c_str());
  if (announced!=Never)
    string_format_append(s, " (Announced %lld)", announced);
  else
    s.append(" (not yet announced)");
  s.append("\n");
  return s;
}
