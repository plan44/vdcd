//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

#include "jsonrpccomm.hpp"


using namespace p44;


JsonRpcComm::JsonRpcComm(SyncIOMainLoop &aMainLoop) :
  inherited(aMainLoop),
  requestIdCounter(0),
  reportAllErrors(false)
{
  // set myself as handler of incoming JSON objects (which are supposed to be JSON-RPC 2.0
  setMessageHandler(boost::bind(&JsonRpcComm::gotJson, this, _1, _2));
}


JsonRpcComm::~JsonRpcComm()
{
}


void JsonRpcComm::setRequestHandler(JsonRpcRequestCB aJsonRpcRequestHandler)
{
  jsonRequestHandler = aJsonRpcRequestHandler;
}


static JsonObjectPtr jsonRPCObj()
{
  JsonObjectPtr obj = JsonObject::newObj();
  // the mandatory version string all objects need to have
  obj->add("jsonrpc", JsonObject::newString("2.0"));
  return obj;
}



#pragma mark - sending outgoing requests and responses


ErrorPtr JsonRpcComm::sendRequest(const char *aMethod, JsonObjectPtr aParams, JsonRpcResponseCB aResponseHandler)
{
  JsonObjectPtr request = jsonRPCObj();
  // the method or notification name
  request->add("method", JsonObject::newString(aMethod));
  // the optional parameters
  if (aParams) {
    request->add("params", aParams);
  }
  // in any case, count this call (even if it is a notification)
  requestIdCounter++;
  // in case this is a method call (i.e. a answer handler is specified), add the call ID
  if (aResponseHandler) {
    // add the ID so the callee can include it in the response
    request->add("id", JsonObject::newInt32(requestIdCounter));
    // remember it in our map
    pendingAnswers[requestIdCounter] = aResponseHandler;
  }
  // now send
  DBGLOG(LOG_DEBUG,"Sending JSON-RPC 2.0 request message:\n  %s\n", request->c_strValue());
  return sendMessage(request);
}


ErrorPtr JsonRpcComm::sendResult(const char *aJsonRpcId, JsonObjectPtr aResult)
{
  JsonObjectPtr response = jsonRPCObj();
  // add the result, can be NULL
  response->add("result", aResult);
  // add the ID so the caller can associate with a previous request
  response->add("id", JsonObject::newString(aJsonRpcId));
  // now send
  DBGLOG(LOG_DEBUG,"Sending JSON-RPC 2.0 result message:\n  %s\n", response->c_strValue());
  return sendMessage(response);
}


ErrorPtr JsonRpcComm::sendError(const char *aJsonRpcId, uint32_t aErrorCode, const char *aErrorMessage, JsonObjectPtr aErrorData)
{
  JsonObjectPtr response = jsonRPCObj();
  // create the error object
  JsonObjectPtr errorObj = JsonObject::newObj();
  errorObj->add("code", JsonObject::newInt32(aErrorCode));
  string errMsg;
  if (aErrorMessage) {
    errMsg = aErrorMessage;
  }
  else {
    errMsg = string_format("Error code %d (0x%X)", aErrorCode, aErrorCode);
  }
  errorObj->add("message", JsonObject::newString(errMsg));
  // add the data object if any
  if (aErrorData) {
    errorObj->add("data", aErrorData);
  }
  // add the error object
  response->add("error", errorObj);
  // add the ID so the caller can associate with a previous request
  response->add("id", aJsonRpcId ? JsonObject::newString(aJsonRpcId) : JsonObjectPtr());
  // now send
  DBGLOG(LOG_DEBUG,"Sending JSON-RPC 2.0 error message:\n  %s\n", response->c_strValue());
  return sendMessage(response);
}


ErrorPtr JsonRpcComm::sendError(const char *aJsonRpcId, ErrorPtr aErrorToSend)
{
  if (!Error::isOK(aErrorToSend)) {
    return sendError(aJsonRpcId, (uint32_t)aErrorToSend->getErrorCode(), aErrorToSend->getErrorMessage());
  }
  return ErrorPtr();
}



#pragma mark - handling incoming requests and responses


void JsonRpcComm::gotJson(ErrorPtr aError, JsonObjectPtr aJsonObject)
{
  JsonRpcCommPtr keepMeAlive(this); // make sure this object lives until routine terminates
  ErrorPtr respErr;
  bool safeError = false; // set when reporting error is safe (i.e. not error possibly generated by malformed error, to prevent error loops)
  JsonObjectPtr idObj;
  const char *idString = NULL;
  if (Error::isOK(aError)) {
    // received proper JSON, now check JSON-RPC specifics
    DBGLOG(LOG_DEBUG,"Received JSON message:\n  %s\n", aJsonObject->c_strValue());
    if (aJsonObject->isType(json_type_array)) {
      respErr = ErrorPtr(new JsonRpcError(JSONRPC_INVALID_REQUEST, "Invalid Request - batch mode not supported by this implementation"));
    }
    else if (!aJsonObject->isType(json_type_object)) {
      respErr = ErrorPtr(new JsonRpcError(JSONRPC_INVALID_REQUEST, "Invalid Request - request must be JSON object"));
    }
    else {
      // check request object fields
      const char *method = NULL;
      JsonObjectPtr o = aJsonObject->get("jsonrpc");
      if (!o)
        respErr = ErrorPtr(new JsonRpcError(JSONRPC_INVALID_REQUEST, "Invalid Request - missing 'jsonrpc'"));
      else if (o->stringValue()!="2.0")
        respErr = ErrorPtr(new JsonRpcError(JSONRPC_INVALID_REQUEST, "Invalid Request - wrong version in 'jsonrpc'"));
      else {
        // get ID param (must be present for all messages except notification)
        idObj = aJsonObject->get("id");
        if (idObj) idString = idObj->c_strValue();
        JsonObjectPtr paramsObj = aJsonObject->get("params");
        // JSON-RPC version is correct, check other params
        method = aJsonObject->getCString("method");
        if (method) {
          // this is a request (responses don't have the method member)
          safeError = idObj!=NULL; // reporting error is safe if this is a method call. Other errors are reported only when reportAllErrors is set
          if (*method==0)
            respErr = ErrorPtr(new JsonRpcError(JSONRPC_INVALID_REQUEST, "Invalid Request - empty 'method'"));
          else {
            // looks like a valid method or notification call
            if (!jsonRequestHandler) {
              // no handler -> method cannot be executed
              respErr = ErrorPtr(new JsonRpcError(JSONRPC_METHOD_NOT_FOUND, "Method not found"));
            }
            else {
              if (paramsObj && !paramsObj->isType(json_type_array) && !paramsObj->isType(json_type_object)) {
                // invalid param object
                respErr = ErrorPtr(new JsonRpcError(JSONRPC_INVALID_REQUEST, "Invalid Request - 'params' must be object or array"));
              }
              else {
                // call handler to execute method or notification
                jsonRequestHandler(method, idString, paramsObj);
              }
            }
          }
        }
        else {
          // this is a response (requests always have a method member)
          // - check if result or error
          JsonObjectPtr respObj;
          if (!aJsonObject->get("result", respObj)) {
            // must be error, need further decoding
            respObj = aJsonObject->get("error");
            if (!respObj)
              respErr = ErrorPtr(new JsonRpcError(JSONRPC_INTERNAL_ERROR, "Internal JSON-RPC error - response with neither 'result' nor 'error'"));
            else {
              // dissect error object
              ErrorCode errCode = JSONRPC_INTERNAL_ERROR; // Internal RPC error
              const char *errMsg = "malformed Error response";
              // - try to get error code
              JsonObjectPtr o = respObj->get("code");
              if (o) errCode = o->int32Value();
              // - try to get error message
              o = respObj->get("message");
              if (o) errMsg = o->c_strValue();
              // compose error object from this
              respErr = ErrorPtr(new JsonRpcError(errCode, errMsg));
              // also get optional data element
              respObj = respObj->get("data");
            }
          }
          // Now we have either result or error.data in respObj, and respErr is Ok or contains the error code + message
          if (!idObj) {
            // errors without ID cannot be associated with calls made earlier, so just log the error
            LOG(LOG_WARNING,"JSON-RPC 2.0 warning: Received response with no or NULL 'id' that cannot be dispatched:\n  %s\n", aJsonObject->c_strValue());
          }
          else {
            // dispatch by ID
            uint32_t requestId = idObj->int32Value();
            PendingAnswerMap::iterator pos = pendingAnswers.find(requestId);
            if (pos==pendingAnswers.end()) {
              // errors without ID cannot be associated with calls made earlier, so just log the error
              LOG(LOG_WARNING,"JSON-RPC 2.0 error: Received response with unknown 'id'=%d : %s\n", requestId, aJsonObject->c_strValue());
            }
            else {
              // found callback
              JsonRpcResponseCB cb = pos->second;
              pendingAnswers.erase(pos); // erase
              cb(requestId, respErr, respObj); // call
            }
            respErr.reset(); // handled
          }
        }
      }
    }
  }
  else {
    // no proper JSON received, create error response
    if (aError->isDomain(JsonError::domain())) {
      // some kind of parsing error
      respErr = ErrorPtr(new JsonRpcError(JSONRPC_PARSE_ERROR, aError->description()));
    }
    else {
      // some other type of server error
      respErr = ErrorPtr(new JsonRpcError(JSONRPC_SERVER_ERROR, aError->description()));
    }
  }
  // auto-generate error response for internally created errors
  if (!Error::isOK(respErr)) {
    if (safeError || reportAllErrors)
      sendError(idString, respErr);
    else
      LOG(LOG_WARNING,"Received data that generated error which can't be sent back: Code=%d, Message='%s'\n", respErr->getErrorCode(), respErr->description().c_str());
  }
}












