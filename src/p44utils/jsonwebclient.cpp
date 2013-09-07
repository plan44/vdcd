//
//  jsonwebclient.cpp
//  p44utils
//
//  Created by Lukas Zeller on 06.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "jsonwebclient.hpp"



using namespace p44;

JsonWebClient::JsonWebClient(SyncIOMainLoop &aMainLoop) :
  HttpComm(aMainLoop)
{
}


JsonWebClient::~JsonWebClient()
{
}


void JsonWebClient::requestThreadSignal(SyncIOMainLoop &aMainLoop, ChildThreadWrapper &aChildThread, ThreadSignals aSignalCode)
{
  if (jsonResponseCallback) {
    // only if we have a json callback, we need to parse the response at all
    if (aSignalCode==threadSignalCompleted) {
      requestInProgress = false; // thread completed
      JsonObjectPtr message;
      if (Error::isOK(requestError)) {
        // try to decode JSON
        struct json_tokener* tokener = json_tokener_new();
        struct json_object *o = json_tokener_parse_ex(tokener, response.c_str(), (int)response.size());
        if (o==NULL) {
          // error (or incomplete JSON, which is fine)
          JsonErrors err = json_tokener_get_error(tokener);
          if (err!=json_tokener_continue) {
            // real error
            requestError = ErrorPtr(new JsonError(err));
            json_tokener_reset(tokener);
          }
        }
        else {
          // got JSON object
          message = JsonObject::newObj(o);
        }
      }
      // call back with result of request
      // Note: this callback might initiate another request already
      if (jsonResponseCallback)
        jsonResponseCallback(*this, message, requestError);
      // release child thread object now
      jsonResponseCallback.clear();
      childThread.reset();
    }
  }
  else {
    // no JSON callback, let inherited handle this
    inherited::requestThreadSignal(aMainLoop, aChildThread, aSignalCode);
  }
}



bool JsonWebClient::jsonRequest(const char *aURL, JsonWebClientCB aResponseCallback, const char *aMethod, JsonObjectPtr aJsonRequest)
{
  // set callback
  jsonResponseCallback = aResponseCallback;
  // encode JSON, if any
  string jsonstring;
  if (aJsonRequest) {
    jsonstring = aJsonRequest->json_c_str();
  }
  DBGLOG(LOG_DEBUG,"JsonWebClient: -> sending JSON request:\n%s", jsonstring.c_str());
  return httpRequest(aURL, NULL, aMethod, jsonstring.c_str());
}



