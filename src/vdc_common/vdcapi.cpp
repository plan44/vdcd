//
//  vdcapi.cpp
//  vdcd
//
//  Created by Lukas Zeller on 04.12.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "vdcapi.hpp"

using namespace p44;


#pragma mark - VdcApiServer

VdcApiServer::VdcApiServer() :
  inherited(SyncIOMainLoop::currentMainLoop())
{
}


void VdcApiServer::start()
{
  inherited::startServer(boost::bind(&VdcApiServer::serverConnectionHandler, this, _1), 3);
}


void VdcApiServer::setConnectionStatusHandler(VdcApiConnectionCB aConnectionCB)
{
  apiConnectionStatusHandler = aConnectionCB;
}


SocketCommPtr VdcApiServer::serverConnectionHandler(SocketComm *aServerSocketCommP)
{
  // create new connection
  VdcApiConnectionPtr apiConnection = newConnection();
  SocketCommPtr socketComm = apiConnection->socketConnection();
  socketComm->relatedObject = apiConnection; // bind object to connection
  socketComm->setConnectionStatusHandler(boost::bind(&VdcApiServer::connectionStatusHandler, this, _1, _2));
  // return the socketComm object which handles this connection
  return socketComm;
}


void VdcApiServer::connectionStatusHandler(SocketComm *aSocketComm, ErrorPtr aError)
{
  if (apiConnectionStatusHandler) {
    // get connection object
    VdcApiConnectionPtr apiConnection = boost::dynamic_pointer_cast<VdcApiConnection>(aSocketComm->relatedObject);
    if (apiConnection) {
      apiConnectionStatusHandler(apiConnection, aError);
    }
  }
  if (!Error::isOK(aError)) {
    // connection failed/closed and we don't support reconnect yet
    aSocketComm->relatedObject.reset(); // detach connection object
  }
}



#pragma mark - VdcApiConnection


void VdcApiConnection::setRequestHandler(VdcApiRequestCB aApiRequestHandler)
{
  apiRequestHandler = aApiRequestHandler;
}


void VdcApiConnection::closeConnection()
{
  if (socketConnection()) {
    socketConnection()->closeConnection();
    socketConnection().reset(); // break connection
  }
}


ErrorPtr VdcApiConnection::sendError(VdcApiRequestPtr aForRequest, ErrorPtr aErrorToSend)
{
  if (!Error::isOK(aErrorToSend)) {
    return sendError(aForRequest, (uint32_t)aErrorToSend->getErrorCode(), aErrorToSend->getErrorMessage());
  }
  return ErrorPtr();
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




#pragma mark - VdcJsonApiConnection


VdcJsonApiConnection::VdcJsonApiConnection()
{
  jsonRpcComm = JsonRpcCommPtr(new JsonRpcComm(SyncIOMainLoop::currentMainLoop()));
  // install JSON request handler locally
  jsonRpcComm->setRequestHandler(boost::bind(&VdcJsonApiConnection::jsonRequestHandler, this, _1, _2, _3, _4));
}



void VdcJsonApiConnection::jsonRequestHandler(JsonRpcComm *aJsonRpcComm, const char *aMethod, const char *aJsonRpcId, JsonObjectPtr aParams)
{
  ErrorPtr respErr;
  if (apiRequestHandler) {
    // create params API value
    ApiValuePtr params = JsonApiValue::newValueFromJson(aParams);
    VdcApiRequestPtr request;
    // create request object in case this request expects an answer
    if (aJsonRpcId) {
      request = VdcJsonApiRequestPtr(new VdcJsonApiRequest(VdcJsonApiConnectionPtr(this), aJsonRpcId));
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
  return jsonRpcComm->sendRequest(aMethod.c_str(), params->jsonObject(), boost::bind(&VdcJsonApiConnection::jsonResponseHandler, this, aResponseHandler, _2, _3, _4));
}


void VdcJsonApiConnection::jsonResponseHandler(VdcApiResponseCB aResponseHandler, int32_t aResponseId, ErrorPtr &aError, JsonObjectPtr aResultOrErrorData)
{
  if (aResponseHandler) {
    // create request object just to hold the response ID
    string respId = string_format("%d", aResponseId);
    ApiValuePtr resultOrErrorData = JsonApiValue::newValueFromJson(aResultOrErrorData);
    VdcApiRequestPtr request = VdcJsonApiRequestPtr(new VdcJsonApiRequest(VdcJsonApiConnectionPtr(this), respId.c_str()));
    aResponseHandler(VdcApiConnectionPtr(this), request, aError, resultOrErrorData);
  }
}


ErrorPtr VdcJsonApiConnection::sendResult(VdcApiRequestPtr aForRequest, ApiValuePtr aResult)
{
  JsonApiValuePtr result = boost::dynamic_pointer_cast<JsonApiValue>(aResult);
  return jsonRpcComm->sendResult(aForRequest->requestId().c_str(), result->jsonObject());
}


ErrorPtr VdcJsonApiConnection::sendError(VdcApiRequestPtr aForRequest, uint32_t aErrorCode, string aErrorMessage, ApiValuePtr aErrorData)
{
  JsonApiValuePtr errorData;
  if (aErrorData)
    errorData = boost::dynamic_pointer_cast<JsonApiValue>(aErrorData);
  return jsonRpcComm->sendError(aForRequest->requestId().c_str(), aErrorCode, aErrorMessage.size()>0 ? aErrorMessage.c_str() : NULL, errorData->jsonObject());
}


