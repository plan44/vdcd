//
//  dsaddressable.cpp
//  vdcd
//
//  Created by Lukas Zeller on 14.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "dsaddressable.hpp"

#include "devicecontainer.hpp"

using namespace p44;



DsAddressable::DsAddressable(DeviceContainer *aDeviceContainerP) :
  deviceContainerP(aDeviceContainerP)
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



ErrorPtr DsAddressable::checkParam(JsonObjectPtr aParams, const char *aParamName, JsonObjectPtr &aParam)
{
  ErrorPtr err;
  bool exists = false;
  aParam.reset();
  if (aParams)
    exists = aParams->get(aParamName, aParam);
  if (!exists)
    err = ErrorPtr(new JsonRpcError(JSONRPC_INVALID_PARAMS, string_format("Invalid Parameters - missing '%s'",aParamName)));
  return err;
}


ErrorPtr DsAddressable::checkStringParam(JsonObjectPtr aParams, const char *aParamName, string &aParamValue)
{
  ErrorPtr err;
  JsonObjectPtr o;
  err = checkParam(aParams, aParamName, o);
  if (Error::isOK(err)) {
    aParamValue = o->stringValue();
  }
  return err;
}



ErrorPtr DsAddressable::handleMethod(const string &aMethod, const string &aJsonRpcId, JsonObjectPtr aParams)
{
  ErrorPtr respErr;
  string name;
  int arrayIndex = 0;
  int rangeSize = PROP_ARRAY_SIZE; // entire array by default if no "index" or "offset"/"count" is given
  JsonObjectPtr o;
  if (aMethod=="getProperty") {
    // name must be present
    if (Error::isOK(respErr = checkStringParam(aParams, "name", name))) {
      // get optional index
      o = aParams->get("index");
      if (o) {
        arrayIndex = o->int32Value();
        rangeSize = 0; // single element
      }
      else {
        o = aParams->get("offset");
        if (o) {
          // range access
          arrayIndex = o->int32Value(); // same as index for lower level property access mechanism
          // - check optional max count of elements
          o = aParams->get("count");
          if (o)
            rangeSize = o->int32Value();
        }
      }
      // now read
      JsonObjectPtr result;
      respErr = accessProperty(false, result, name, VDC_API_DOMAIN, arrayIndex, rangeSize);
      if (Error::isOK(respErr)) {
        // send back property result
        sendResult(aJsonRpcId, result);
      }
    }
  }
  else if (aMethod=="setProperty") {
    // name must be present
    if (Error::isOK(respErr = checkStringParam(aParams, "name", name))) {
      // value must be present
      JsonObjectPtr value;
      if (Error::isOK(respErr = checkParam(aParams, "value", value))) {
        // get optional index
        o = aParams->get("index");
        if (o) {
          arrayIndex = o->int32Value();
          rangeSize = 1; // single array element, no repetition 
        }
        else {
          o = aParams->get("offset");
          if (o) {
            // range access
            arrayIndex = o->int32Value(); // same as index for lower level property access mechanism
            // - check optional max count of elements
            o = aParams->get("count");
            if (o)
              rangeSize = o->int32Value();
          }
          else {
            // neither "index" nor "offset" -> must be non-array
            rangeSize = 1; // no repetition
          }
        }
        // now write (possibly batch to multiple elements if rangeSize>1. rangeSize==0 is single element write)
        if (rangeSize==PROP_ARRAY_SIZE)
          respErr = ErrorPtr(new JsonRpcError(JSONRPC_INVALID_PARAMS, "array batch write needs offset AND count"));
        else {
          do {
            respErr = accessProperty(true, value, name, VDC_API_DOMAIN, arrayIndex, 0);
            arrayIndex++;
          } while (--rangeSize>0 && Error::isOK(respErr));
        }
        if (Error::isOK(respErr)) {
          // send back OK if write was successful
          sendResult(aJsonRpcId, JsonObjectPtr());
        }
      }
    }
  }
  else {
    respErr = ErrorPtr(new JsonRpcError(JSONRPC_METHOD_NOT_FOUND, "unknown method"));
  }
  return respErr;
}


bool DsAddressable::pushProperty(const string &aName, int aDomain, int aIndex)
{
  // get the value
  JsonObjectPtr value;
  ErrorPtr err = accessProperty(false, value, aName, aDomain, aIndex<0 ? 0 : aIndex, 0);
  if (Error::isOK(err)) {
    JsonObjectPtr pushParams = JsonObject::newObj();
    pushParams->add("name", JsonObject::newString(aName));
    if (aIndex>=0) {
      // array property push
      pushParams->add("index", JsonObject::newInt32(aIndex));
    }
    pushParams->add("value", value);
    return sendRequest("pushProperty", pushParams);
  }
  return false;
}



void DsAddressable::handleNotification(const string &aMethod, JsonObjectPtr aParams)
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


bool DsAddressable::sendRequest(const char *aMethod, JsonObjectPtr aParams, JsonRpcResponseCB aResponseHandler)
{
  if (!aParams) {
    // create params object because we need it for the dSID
    aParams = JsonObject::newObj();
  }
  aParams->add("dSUID", JsonObject::newString(dSUID.getString()));
  return getDeviceContainer().sendApiRequest(aMethod, aParams, aResponseHandler);
}


bool DsAddressable::sendResult(const string &aJsonRpcId, JsonObjectPtr aResult)
{
  return getDeviceContainer().sendApiResult(aJsonRpcId, aResult);
}


bool DsAddressable::sendError(const string &aJsonRpcId, ErrorPtr aErrorToSend)
{
  return getDeviceContainer().sendApiError(aJsonRpcId, aErrorToSend);
}




void DsAddressable::presenceResultHandler(bool aIsPresent)
{
  if (aIsPresent) {
    // send back Pong notification
    LOG(LOG_INFO,"ping: %s is present -> sending pong\n", shortDesc().c_str());
    sendRequest("pong", JsonObjectPtr());
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
  dSID_key,
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
    { "dSUID", ptype_charptr, false, dSID_key, &dsAddressable_key },
    { "model", ptype_charptr, false, model_key, &dsAddressable_key },
    { "hardwareVersion", ptype_charptr, false, hardwareVersion_key, &dsAddressable_key },
    { "hardwareGuid", ptype_charptr, false, hardwareGUID_key, &dsAddressable_key },
    { "numDevicesInHW", ptype_int32, false, numDevicesInHW_key, &dsAddressable_key },
    { "deviceIndexInHW", ptype_int32, false, deviceIndexInHW_key, &dsAddressable_key },
    { "oemGuid", ptype_charptr, false, oemGUID_key, &dsAddressable_key },
    { "name", ptype_charptr, false, name_key, &dsAddressable_key }
  };
  int n = inherited::numProps(aDomain);
  if (aPropIndex<n)
    return inherited::getPropertyDescriptor(aPropIndex, aDomain); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return &properties[aPropIndex];
}


bool DsAddressable::accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&dsAddressable_key) {
    if (aForWrite) {
      switch (aPropertyDescriptor.accessKey) {
        case name_key: setName(aPropValue->stringValue()); return true;
      }
    }
    else {
      switch (aPropertyDescriptor.accessKey) {
        case dSID_key: aPropValue = JsonObject::newString(dSUID.getString()); return true;
        case model_key: aPropValue = JsonObject::newString(modelName()); return true;
        case hardwareVersion_key: aPropValue = JsonObject::newString(hardwareVersion(), true); return true;
        case hardwareGUID_key: aPropValue = JsonObject::newString(hardwareGUID(), true); return true;
        case oemGUID_key: aPropValue = JsonObject::newString(oemGUID(), true); return true;
        case name_key: aPropValue = JsonObject::newString(getName()); return true;
        // conditionally available
        case numDevicesInHW_key:
          if (numDevicesInHW()>=0) {
            aPropValue = JsonObject::newInt32((int)numDevicesInHW());
            return true;
          }
          break; // no such property
        case deviceIndexInHW_key:
          if (deviceIndexInHW()>=0) {
            aPropValue = JsonObject::newInt32((int)deviceIndexInHW());
            return true;
          }
          break; // no such property
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
  return string_format("DsAddressable %s", shortDesc().c_str());
}
