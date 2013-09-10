//
//  httpcomm.hpp
//  p44utils
//
//  Created by Lukas Zeller on 04.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__httpcomm__
#define __vdcd__httpcomm__

#include "p44_common.hpp"

#include "mongoose.h"

using namespace std;

namespace p44 {


  // Errors
  typedef uint16_t HttpCommErrors;

  enum {
    HttpCommError_invalidParameters = 10000,
    HttpCommError_noConnection = 10001,
    HttpCommError_read = 10002,
    HttpCommError_write = 10003,
    HttpCommError_mongooseError = 20000
  };

  class HttpCommError : public Error
  {
  public:
    static const char *domain() { return "HttpComm"; }
    virtual const char *getErrorDomain() const { return HttpCommError::domain(); };
    HttpCommError(HttpCommErrors aError) : Error(ErrorCode(aError)) {};
    HttpCommError(HttpCommErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };



  class HttpComm;

  typedef boost::intrusive_ptr<HttpComm> HttpCommPtr;

  /// callback for returning response data or reporting error
  /// @param aHttpCommP the HttpComm object this callback comes from
  /// @param aResponse the response string
  /// @param aError an error object if an error occurred, empty pointer otherwise
  typedef boost::function<void (HttpComm &aHttpComm, const string &aResponse, ErrorPtr aError)> HttpCommCB;


  /// wrapper for non-blocking http client communication
  /// @note this class' implementation is not suitable for handling huge http requests and answers. It is
  ///   intended for accessing web APIs with short messages.
  class HttpComm : public P44Obj
  {
    typedef P44Obj inherited;

    HttpCommCB responseCallback;

    // vars used in subthread, only access when !requestInProgress
    string requestURL;
    string method;
    string contentType;
    string requestBody;
    struct mg_connection *mgConn; // mongoose connection

  protected:

    SyncIOMainLoop &mainLoop;

    bool requestInProgress; ///< set when request is in progress and no new request can be issued

    // vars used in subthread, only access when !requestInProgress
    ChildThreadWrapperPtr childThread;
    string response;
    ErrorPtr requestError;

  public:

    HttpComm(SyncIOMainLoop &aMainLoop);
    virtual ~HttpComm();

    /// send a HTTP or HTTPS request
    /// @param aURL the http or https URL to access
    /// @param responseCallback will be called when request completes, returning response or error
    /// @param aMethod the HTTP method to use (defaults to "GET")
    /// @param aRequestBody a C string containing the request body to send, or NULL if none
    /// @param aContentType the content type for the body to send, or NULL to use default
    /// @return false if no request could be initiated (already busy with another request).
    ///   If false, aHttpCallback will not be called
    bool httpRequest(const char *aURL, HttpCommCB aResponseCallback, const char *aMethod = "GET", const char* aRequestBody = NULL, const char *aContentType = NULL);

    /// cancel request
    void cancelRequest();

  protected:
    virtual const char *defaultContentType() { return "text/html"; };

    virtual void requestThreadSignal(SyncIOMainLoop &aMainLoop, ChildThreadWrapper &aChildThread, ThreadSignals aSignalCode);

  private:
    void requestThread(ChildThreadWrapper &aThread);

  };

  
} // namespace p44


#endif /* defined(__vdcd__httpcomm__) */
