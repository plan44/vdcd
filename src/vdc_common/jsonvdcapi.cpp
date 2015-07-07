//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "jsonvdcapi.hpp"

using namespace p44;



#pragma mark - JsonApiValue

JsonApiValue::JsonApiValue()
{
}


ApiValuePtr JsonApiValue::newValue(ApiValueType aObjectType)
{
  ApiValuePtr newVal = ApiValuePtr(new JsonApiValue);
  newVal->setType(aObjectType);
  return newVal;
}



void JsonApiValue::clear()
{
  switch (getType()) {
    case apivalue_object:
      // just assign new object and forget old one
      jsonObj = JsonObject::newObj();
      break;
    case apivalue_array:
      jsonObj = JsonObject::newArray();
      break;
    // for unstuctured values, the json obj will be created on assign, so clear it now
    default:
      jsonObj.reset();
      break;
  }
}


bool JsonApiValue::setStringValue(const string &aString)
{
  if (getType()==apivalue_string || getType()==apivalue_binary) {
    jsonObj = JsonObject::newString(aString, false);
    return true;
  }
  else
    return inherited::setStringValue(aString);
};


void JsonApiValue::setBinaryValue(const string &aBinary)
{
  // represent as hex string in JSON
  setStringValue(binaryToHexString(aBinary));
}


string JsonApiValue::binaryValue()
{
  // parse binary string as hex
  return hexToBinaryString(stringValue().c_str());
}





void JsonApiValue::setJsonObject(JsonObjectPtr aJsonObject)
{
  jsonObj = aJsonObject;
  // derive type
  if (!jsonObj) {
    setType(apivalue_null);
  }
  else {
    switch (jsonObj->type()) {
      case json_type_boolean: objectType = apivalue_bool; break;
      case json_type_double: objectType = apivalue_double; break;
      case json_type_int: objectType = apivalue_int64; break;
      case json_type_object: objectType = apivalue_object; break;
      case json_type_array: objectType = apivalue_array; break;
      case json_type_string: objectType = apivalue_string; break;
      case json_type_null:
      default:
        setType(apivalue_null);
        break;
    }
  }
}


void JsonApiValue::operator=(ApiValue &aApiValue)
{
  JsonApiValue *javP = dynamic_cast<JsonApiValue *>(&aApiValue);
  if (javP)
    setJsonObject(javP->jsonObj);
  else
    setNull(); // not assignable
}



ApiValuePtr JsonApiValue::newValueFromJson(JsonObjectPtr aJsonObject)
{
  JsonApiValue *javP = new JsonApiValue;
  javP->setJsonObject(aJsonObject);
  return ApiValuePtr(javP);
}



#pragma mark - VdcJsonApiServer


VdcApiConnectionPtr VdcJsonApiServer::newConnection()
{
  // create the right kind of API connection
  return VdcApiConnectionPtr(static_cast<VdcApiConnection *>(new VdcJsonApiConnection()));
}



#pragma mark - VdcJsonApiRequest


VdcJsonApiRequest::VdcJsonApiRequest(VdcJsonApiConnectionPtr aConnection, const char *aJsonRpcId)
{
  jsonConnection = aConnection;
  jsonRpcId = aJsonRpcId ? aJsonRpcId : ""; // empty if none passed
}


VdcApiConnectionPtr VdcJsonApiRequest::connection()
{
  return jsonConnection;
}



ErrorPtr VdcJsonApiRequest::sendResult(ApiValuePtr aResult)
{
  LOG(LOG_INFO,"vdSM <- vDC (JSON) result sent: requestid='%s', result=%s\n", requestId().c_str(), aResult ? aResult->description().c_str() : "<none>");
  JsonApiValuePtr result = boost::dynamic_pointer_cast<JsonApiValue>(aResult);
  return jsonConnection->jsonRpcComm->sendResult(requestId().c_str(), result ? result->jsonObject() : NULL);
}


ErrorPtr VdcJsonApiRequest::sendError(uint32_t aErrorCode, string aErrorMessage, ApiValuePtr aErrorData)
{
  LOG(LOG_INFO,"vdSM <- vDC (JSON) error sent: requestid='%s', error=%d (%s)\n", requestId().c_str(), aErrorCode, aErrorMessage.c_str());
  JsonApiValuePtr errorData;
  if (aErrorData)
    errorData = boost::dynamic_pointer_cast<JsonApiValue>(aErrorData);
  return jsonConnection->jsonRpcComm->sendError(requestId().c_str(), aErrorCode, aErrorMessage.size()>0 ? aErrorMessage.c_str() : NULL, errorData ? errorData->jsonObject() : JsonObjectPtr());
}



#pragma mark - VdcJsonApiConnection


VdcJsonApiConnection::VdcJsonApiConnection()
{
  jsonRpcComm = JsonRpcCommPtr(new JsonRpcComm(MainLoop::currentMainLoop()));
  // install JSON request handler locally
  jsonRpcComm->setRequestHandler(boost::bind(&VdcJsonApiConnection::jsonRequestHandler, this, _1, _2, _3));
}



void VdcJsonApiConnection::jsonRequestHandler(const char *aMethod, const char *aJsonRpcId, JsonObjectPtr aParams)
{
  ErrorPtr respErr;
  if (apiRequestHandler) {
    // create params API value
    ApiValuePtr params = JsonApiValue::newValueFromJson(aParams);
    VdcApiRequestPtr request;
    // create request object in case this request expects an answer
    if (aJsonRpcId) {
      // Method
      request = VdcJsonApiRequestPtr(new VdcJsonApiRequest(VdcJsonApiConnectionPtr(this), aJsonRpcId));
      LOG(LOG_INFO,"vdSM -> vDC (JSON) method call '%s' received: requestid='%s', params=%s\n", aMethod, request->requestId().c_str(), params ? params->description().c_str() : "<none>");
    }
    else {
      // Notification
      LOG(LOG_INFO,"vdSM -> vDC (JSON) notification '%s' received: params=%s\n", aMethod, params ? params->description().c_str() : "<none>");
    }
    // call handler
    apiRequestHandler(VdcJsonApiConnectionPtr(this), request, aMethod, params);
  }
}


void VdcJsonApiConnection::closeAfterSend()
{
  jsonRpcComm->closeAfterSend();
}


ApiValuePtr VdcJsonApiConnection::newApiValue()
{
  return ApiValuePtr(new JsonApiValue);
}


ErrorPtr VdcJsonApiConnection::sendRequest(const string &aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler)
{
  JsonApiValuePtr params = boost::dynamic_pointer_cast<JsonApiValue>(aParams);
  ErrorPtr err;
  if (aResponseHandler) {
    // method call expecting response
    err = jsonRpcComm->sendRequest(aMethod.c_str(), params->jsonObject(), boost::bind(&VdcJsonApiConnection::jsonResponseHandler, this, aResponseHandler, _1, _2, _3));
    LOG(LOG_INFO,"vdSM <- vDC (JSON) method call sent: requestid='%d', method='%s', params=%s\n", jsonRpcComm->lastRequestId(), aMethod.c_str(), aParams ? aParams->description().c_str() : "<none>");
  }
  else {
    // notification
    err = jsonRpcComm->sendRequest(aMethod.c_str(), params->jsonObject(), NULL);
    LOG(LOG_INFO,"vdSM <- vDC (JSON) notification sent: method='%s', params=%s\n", aMethod.c_str(), aParams ? aParams->description().c_str() : "<none>");
  }
  return err;
}


void VdcJsonApiConnection::jsonResponseHandler(VdcApiResponseCB aResponseHandler, int32_t aResponseId, ErrorPtr &aError, JsonObjectPtr aResultOrErrorData)
{
  if (aResponseHandler) {
    // create request object just to hold the response ID
    string respId = string_format("%d", aResponseId);
    ApiValuePtr resultOrErrorData = JsonApiValue::newValueFromJson(aResultOrErrorData);
    VdcApiRequestPtr request = VdcJsonApiRequestPtr(new VdcJsonApiRequest(VdcJsonApiConnectionPtr(this), respId.c_str()));
    if (Error::isOK(aError)) {
      LOG(LOG_INFO,"vdSM -> vDC (JSON) result received: id='%s', result=%s\n", request->requestId().c_str(), resultOrErrorData ? resultOrErrorData->description().c_str() : "<none>");
    }
    else {
      LOG(LOG_INFO,"vdSM -> vDC (JSON) error received: id='%s', error=%s, errordata=%s\n", request->requestId().c_str(), aError->description().c_str(), resultOrErrorData ? resultOrErrorData->description().c_str() : "<none>");
    }
    aResponseHandler(VdcApiConnectionPtr(this), request, aError, resultOrErrorData);
  }
}
