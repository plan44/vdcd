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

  typedef std::map<string,string> HttpHeaderMap;
  typedef boost::shared_ptr<HttpHeaderMap> HttpHeaderMapPtr;


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
    int responseDataFd;
    struct mg_connection *mgConn; // mongoose connection

  public:

    HttpHeaderMapPtr responseHeaders; ///< the response headers when httpRequest is called with aSaveHeaders

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
    /// @param aResponseCallback will be called when request completes, returning response or error
    /// @param aMethod the HTTP method to use (defaults to "GET")
    /// @param aRequestBody a C string containing the request body to send, or NULL if none
    /// @param aContentType the content type for the body to send, or NULL to use default
    /// @param aResponseDataFd if>=0, response data will be written to that file descriptor
    /// @param aSaveHeaders if true, responseHeaders will be set to a string,string map containing the headers
    /// @return false if no request could be initiated (already busy with another request).
    ///   If false, aHttpCallback will not be called
    bool httpRequest(
      const char *aURL,
      HttpCommCB aResponseCallback,
      const char *aMethod = "GET",
      const char* aRequestBody = NULL,
      const char *aContentType = NULL,
      int aResponseDataFd = -1,
      bool aSaveHeaders = false
    );

    /// cancel request
    void cancelRequest();

    // Utilities
    static string urlEncode(const string &aString, bool aFormURLEncoded);
    static void appendFormValue(string &aDataString, const string &aFieldname, const string &aValue);


  protected:
    virtual const char *defaultContentType() { return "text/html"; };

    virtual void requestThreadSignal(SyncIOMainLoop &aMainLoop, ChildThreadWrapper &aChildThread, ThreadSignals aSignalCode);

  private:
    void requestThread(ChildThreadWrapper &aThread);

  };

  
} // namespace p44


#endif /* defined(__vdcd__httpcomm__) */
