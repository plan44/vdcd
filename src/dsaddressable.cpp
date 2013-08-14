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


ErrorPtr DsAddressable::checkStringParam(JsonObjectPtr aParams, const char *aParamName, string &aParamValue)
{
  ErrorPtr err;
  JsonObjectPtr o;
  if (aParams)
    o = aParams->get(aParamName);
  if (!o)
    err = ErrorPtr(new JsonRpcError(JSONRPC_INVALID_PARAMS, string_format("Invalid Parameters - missing '%s'",aParamName)));
  else
    aParamValue = o->stringValue();
  return err;
}



ErrorPtr DsAddressable::handleMethod(const string &aMethod, const string &aJsonRpcId, JsonObjectPtr aParams)
{
  ErrorPtr respErr = ErrorPtr(new JsonRpcError(JSONRPC_METHOD_NOT_FOUND, "unknown method"));
  return respErr;
}


void DsAddressable::handleNotification(const string &aMethod, JsonObjectPtr aParams)
{
  if (aMethod=="Ping") {
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
    sendRequest("Pong", JsonObjectPtr());
  }
  else {
    LOG(LOG_NOTICE,"Ping: %s is not present -> no Pong sent\n", shortDesc().c_str());
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
