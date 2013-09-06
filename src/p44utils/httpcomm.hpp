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

  /// callback for signalling ready for receive or transmit, or error
  typedef boost::function<void (HttpComm *aHttpCommP, ErrorPtr aError)> HttpCommCB;


  /// wrapper for non-blocking http client communication
  class HttpComm : public P44Obj
  {
    HttpCommCB httpCallback;

    // mongoose
    struct mg_connection *mgConn;

  protected:

    SyncIOMainLoop *mainLoopP;

  public:

    HttpComm(SyncIOMainLoop *aMainLoopP);
    virtual ~HttpComm();

    /// send a HTTP or HTTPS request
    /// @param aURL the http or https URL to access
    /// @param aHttpCallback will be called when data is ready to read or write, and on error
    void initiateRequest(const char *aURL, HttpCommCB aHttpCallback);

    /// @return number of bytes ready for read
    size_t numBytesReady();

    /// read data (non-blocking)
    /// @param aNumBytes max number of bytes to receive
    /// @param aBytes pointer to buffer to store received bytes
    /// @param aError reference to ErrorPtr. Will be left untouched if no error occurs
    /// @return number ob bytes actually read
    size_t receiveBytes(size_t aNumBytes, uint8_t *aBytes, ErrorPtr &aError);

    /// close the connection
    void closeConnection();


  private:
    // TODO: remove, experimental only %%%
    void threadfunc(ChildThreadWrapper &aThread);
    void threadsignalfunc(SyncIOMainLoop &aMainLoop, ChildThreadWrapper &aChildThread, ThreadSignals aSignalCode);



  };
  
} // namespace p44


#endif /* defined(__vdcd__httpcomm__) */
