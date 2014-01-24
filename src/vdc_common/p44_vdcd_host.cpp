//
//  Copyright (c) 2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "p44_vdcd_host.hpp"

#include "deviceclasscontainer.hpp"
#include "device.hpp"

#include "jsonvdcapi.hpp"

using namespace p44;


#pragma mark - P44JsonApiRequest


P44JsonApiRequest::P44JsonApiRequest(JsonCommPtr aJsonComm)
{
  jsonComm = aJsonComm;
}



ErrorPtr P44JsonApiRequest::sendResult(ApiValuePtr aResult)
{
  LOG(LOG_INFO,"cfg <- vdcd (JSON) result sent: result=%s\n", aResult ? aResult->description().c_str() : "<none>");
  JsonApiValuePtr result = boost::dynamic_pointer_cast<JsonApiValue>(aResult);
  P44VdcHost::sendCfgApiResponse(jsonComm, result->jsonObject(), ErrorPtr());
  return ErrorPtr();
}



ErrorPtr P44JsonApiRequest::sendError(uint32_t aErrorCode, string aErrorMessage, ApiValuePtr aErrorData)
{
  LOG(LOG_INFO,"cfg <- vdcd (JSON) error sent: error=%d (%s)\n", aErrorCode, aErrorMessage.c_str());
  ErrorPtr err = ErrorPtr(new Error(aErrorCode, aErrorMessage));
  P44VdcHost::sendCfgApiResponse(jsonComm, JsonObjectPtr(), err);
  return ErrorPtr();
}


ApiValuePtr P44JsonApiRequest::newApiValue()
{
  return ApiValuePtr(new JsonApiValue);
}


#pragma mark - P44VdcHost


P44VdcHost::P44VdcHost() :
  configApiServer(SyncIOMainLoop::currentMainLoop()),
  learnTicket(0)
{
}


void P44VdcHost::startConfigApi()
{
  configApiServer.startServer(boost::bind(&P44VdcHost::configApiConnectionHandler, this, _1), 3);
}



SocketCommPtr P44VdcHost::configApiConnectionHandler(SocketComm *aServerSocketCommP)
{
  JsonCommPtr conn = JsonCommPtr(new JsonComm(SyncIOMainLoop::currentMainLoop()));
  conn->setMessageHandler(boost::bind(&P44VdcHost::configApiRequestHandler, this, _1, _2, _3));
  return conn;
}


void P44VdcHost::configApiRequestHandler(JsonComm *aJsonCommP, ErrorPtr aError, JsonObjectPtr aJsonObject)
{
  ErrorPtr err;
  // when coming from mg44, requests have the following form
  // - for GET requests like http://localhost:8080/api/json/myuri?foo=bar&this=that
  //   {"method":"GET","uri":"myuri","uri_params":{"foo":"bar","this":"that"}}
  // - for POST requests like
  //   curl "http://localhost:8080/api/json/myuri?foo=bar&this=that" --data-ascii "{ \"content\":\"data\", \"important\":false }"
  //   {"method":"POST","uri":"myuri","uri_params":{"foo":"bar","this":"that"},"data":{"content":"data","important":false}}
  //   curl "http://localhost:8080/api/json/myuri" --data-ascii "{ \"content\":\"data\", \"important\":false }"
  //   {"method":"POST","uri":"myuri","data":{"content":"data","important":false}}
  // processing:
  // - a JSON request must be either specified in the URL or in the POST data, not both
  // - if POST data ("data" member in the incoming request) is present, "uri_params" is ignored
  // - "uri" selects one of possibly multiple APIs
  if (Error::isOK(aError)) {
    // not JSON level error, try to process
    LOG(LOG_DEBUG,"Config API request: %s\n", aJsonObject->c_strValue());
    // find out which one is our actual JSON request
    // - try POST data first
    JsonObjectPtr request = aJsonObject->get("data");
    if (!request) {
      // no POST data, try uri_params
      request = aJsonObject->get("uri_params");
    }
    if (!request) {
      // empty query, that's an error
      aError = ErrorPtr(new P44VdcError(415, "empty request"));
    }
    else {
      // have the request processed
      string apiselector;
      JsonObjectPtr uri = aJsonObject->get("uri");
      if (uri) apiselector = uri->stringValue();
      // dispatch according to API
      if (apiselector=="vdc") {
        // process request that basically is a vdc API request, but as simple webbish JSON, not as JSON-RPC 2.0
        // and without the need to start a vdc session
        // Notes:
        // - if dSUID is specified invalid or empty, the vdc host itself is addressed.
        // - use x-p44-vdcs and x-p44-devices properties to find dsuids
        aError = processVdcRequest(JsonCommPtr(aJsonCommP), request);
      }
      else if (apiselector=="p44") {
        // process p44 specific requests
        aError = processP44Request(JsonCommPtr(aJsonCommP), request);
      }
      else {
        // unknown API selector
        aError = ErrorPtr(new P44VdcError(400, "invalid URI, unknown API"));
      }
    }
  }
  // if error or explicit OK, send response now. Otherwise, request processing will create and send the response
  if (aError) {
    sendCfgApiResponse(JsonCommPtr(aJsonCommP), JsonObjectPtr(), aError);
  }
}


void P44VdcHost::sendCfgApiResponse(JsonCommPtr aJsonComm, JsonObjectPtr aResult, ErrorPtr aError)
{
  // create response
  JsonObjectPtr response = JsonObject::newObj();
  if (!Error::isOK(aError)) {
    // error, return error response
    response->add("error", JsonObject::newInt32((int32_t)aError->getErrorCode()));
    response->add("errormessage", JsonObject::newString(aError->getErrorMessage()));
    response->add("errordomain", JsonObject::newString(aError->getErrorDomain()));
  }
  else {
    // no error, return result (if any)
    response->add("result", aResult);
  }
  LOG(LOG_DEBUG,"Config API response: %s\n", response->c_strValue());
  aJsonComm->sendMessage(response);
}


// access to vdc API methods and notifications via web requests
ErrorPtr P44VdcHost::processVdcRequest(JsonCommPtr aJsonComm, JsonObjectPtr aRequest)
{
  ErrorPtr err;
  string cmd;
  bool isMethod = false;
  // get method/notification and params
  JsonObjectPtr m = aRequest->get("method");
  if (m) {
    // is a method call, expects answer
    isMethod = true;
  }
  else {
    // not method, may be notification
    m = aRequest->get("notification");
  }
  if (!m) {
    err = ErrorPtr(new P44VdcError(400, "invalid request, must specify 'method' or 'notification'"));
  }
  else {
    // get method/notification name
    cmd = m->stringValue();
    // get params
    // Note: the "method" or "notification" param will also be in the params, but should not cause any problem
    ApiValuePtr params = JsonApiValue::newValueFromJson(aRequest);
    string dsuidstring;
    if (Error::isOK(err = checkStringParam(params, "dSUID", dsuidstring))) {
      // operation method
      DsUid dsuid = DsUid(dsuidstring);
      if (isMethod) {
        // create request
        P44JsonApiRequestPtr request = P44JsonApiRequestPtr(new P44JsonApiRequest(aJsonComm));
        // handle method
        err = handleMethodForDsUid(cmd, request, dsuid, params);
        // methods send results themselves
        if (Error::isOK(err)) {
          err.reset(); // even if we get a ErrorOK, make sure we return NULL to the caller, meaning NO answer is needed
        }
      }
      else {
        // handle notification
        handleNotificationForDsUid(cmd, dsuid, params);
        // notifications are always successful
        err = ErrorPtr(new Error(ErrorOK));
      }
    }
  }
  // returning NULL means caller should not do anything more
  // returning an Error object (even ErrorOK) means caller should return status
  return err;
}


// access to plan44 extras that are not part of the vdc API
ErrorPtr P44VdcHost::processP44Request(JsonCommPtr aJsonComm, JsonObjectPtr aRequest)
{
  ErrorPtr err;
  JsonObjectPtr m = aRequest->get("method");
  if (!m) {
    err = ErrorPtr(new P44VdcError(400, "missing 'method'"));
  }
  else {
    string method = m->stringValue();
    if (method=="learn") {
      // get timeout
      JsonObjectPtr o = aRequest->get("seconds");
      int seconds = 30; // default to 30
      if (o) seconds = o->int32Value();
      if (seconds==0) {
        // end learning
        stopLearning();
        learnHandler(aJsonComm, false, ErrorPtr());
      }
      else {
        // start learning
        startLearning(boost::bind(&P44VdcHost::learnHandler, this, aJsonComm, _1, _2));
        learnTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&P44VdcHost::learnHandler, this, aJsonComm, false, ErrorPtr(new P44VdcError(408, "learn timeout"))), seconds*Second);
      }
    }
    else {
      err = ErrorPtr(new P44VdcError(400, "unknown method"));
    }
  }
  return err;
}


void P44VdcHost::learnHandler(JsonCommPtr aJsonComm, bool aLearnIn, ErrorPtr aError)
{
  MainLoop::currentMainLoop().cancelExecutionTicket(learnTicket);
  sendCfgApiResponse(aJsonComm, JsonObject::newBool(aLearnIn), aError);
}

