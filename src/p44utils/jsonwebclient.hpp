//
//  jsonwebclient.hpp
//  p44utils
//
//  Created by Lukas Zeller on 06.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__jsonwebclient__
#define __vdcd__jsonwebclient__

#include "p44_common.hpp"

#include "jsonobject.hpp"
#include "httpcomm.hpp"

using namespace std;

namespace p44 {

  class JsonWebClient;

  typedef boost::intrusive_ptr<JsonWebClient> JsonWebClientPtr;

  /// callback for returning response data or reporting error
  /// @param aJsonWebClientP the JsonWebClient object this callback comes from
  /// @param aResponse the response string
  /// @param aError an error object if an error occurred, empty pointer otherwise
  typedef boost::function<void (JsonWebClient &aJsonWebClient, JsonObjectPtr aJsonResponse, ErrorPtr aError)> JsonWebClientCB;


  /// wrapper for non-blocking http client communication
  /// @note this class' implementation is not suitable for handling huge http requests and answers. It is
  ///   intended for accessing web APIs with short messages.
  class JsonWebClient : public HttpComm
  {
    typedef HttpComm inherited;

    JsonWebClientCB jsonResponseCallback;

  public:

    JsonWebClient(SyncIOMainLoop *aMainLoopP);
    virtual ~JsonWebClient();

    /// send a JSON request via HTTP or HTTPS
    /// @param aURL the http or https URL to send JSON request to
    /// @param responseCallback will be called when request completes, returning response or error
    /// @param aMethod the HTTP method to use (defaults to "GET")
    /// @param aJsonRequest the JSON request to send (defaults to none)
    /// @return false if no request could be initiated (already busy with another request).
    ///   If false, aHttpCallback will not be called
    bool jsonRequest(const char *aURL, JsonWebClientCB aResponseCallback, const char *aMethod = "GET", JsonObjectPtr aJsonRequest = JsonObjectPtr());

  protected:

    virtual void requestThreadSignal(SyncIOMainLoop &aMainLoop, ChildThreadWrapper &aChildThread, ThreadSignals aSignalCode);

  };

} // namespace p44

#endif /* defined(__vdcd__jsonwebclient__) */
