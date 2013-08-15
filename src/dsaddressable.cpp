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
  int rangeSize = 0; // no range
  JsonObjectPtr o;
  if (aMethod=="getProperty") {
    // name must be present
    if (Error::isOK(respErr = checkStringParam(aParams, "name", name))) {
      // get optional index
      o = aParams->get("index");
      if (o)
        arrayIndex = o->int32Value();
      else {
        o = aParams->get("offset");
        if (o) {
          // range access
          arrayIndex = o->int32Value(); // same as index for lower level property access mechanism
          // - check optional max count of elements
          o = aParams->get("count");
          if (o)
            rangeSize = o->int32Value();
          else
            rangeSize = PROP_ARRAY_SIZE; // entire array
        }
      }
      // now read
      JsonObjectPtr result;
      respErr = accessProperty(false, result, name, arrayIndex, rangeSize);
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
      if (Error::isOK(respErr = checkParam(aParams, "name", value))) {
        // get optional index
        o = aParams->get("index");
        if (o)
          arrayIndex = o->int32Value();
        // now write
        respErr = accessProperty(true, value, name, arrayIndex);
        if (Error::isOK(respErr)) {
          // send back OK if write was successful
          sendResult(aJsonRpcId, NULL);
        }
      }
    }
  }
  else {
    respErr = ErrorPtr(new JsonRpcError(JSONRPC_METHOD_NOT_FOUND, "unknown method"));
  }
  return respErr;
}


void DsAddressable::handleNotification(const string &aMethod, JsonObjectPtr aParams)
{
  if (aMethod=="ping") {
    // issue device ping (which will issue a pong when device is reachable)
    checkPresence(boost::bind(&DsAddressable::presenceResultHandler, this, _1));
  }
  else {
    // unknown notification
    LOG(LOG_WARNING, "unknown method for '%s'", shortDesc().c_str());
  }
}


bool DsAddressable::sendRequest(const char *aMethod, JsonObjectPtr aParams, JsonRpcResponseCB aResponseHandler)
{
  if (!aParams) {
    // create params object because we need it for the dSID
    aParams = JsonObject::newObj();
  }
  aParams->add("dSID", JsonObject::newString(dsid.getString()));
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
    sendRequest("pong", JsonObjectPtr());
  }
  else {
    LOG(LOG_NOTICE,"ping: %s is not present -> no Pong sent\n", shortDesc().c_str());
  }
}



#pragma mark - interaction with subclasses


void DsAddressable::checkPresence(PresenceCB aPresenceResultHandler)
{
  // base class just assumes being present
  aPresenceResultHandler(true);
}




#pragma mark - description/shortDesc

string DsAddressable::shortDesc()
{
  // short description is dsid
  return dsid.getString();
}



string DsAddressable::description()
{
  return string_format("DsAddressable %s", shortDesc().c_str());
}
