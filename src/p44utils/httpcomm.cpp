//
//  httpcomm.cpp
//  p44utils
//
//  Created by Lukas Zeller on 04.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "httpcomm.hpp"


using namespace p44;

HttpComm::HttpComm(SyncIOMainLoop *aMainLoopP) :
  mainLoopP(aMainLoopP),
  mgConn(NULL)
{
}


HttpComm::~HttpComm()
{
  closeConnection();
}


//  I used mg_download to establish the http connection and then mg_read to get the http response.
//  You will have to create your HTTP header in the mg_download line:
//
//  connection = mg_download (
//                 host,
//                 port,
//                 0,
//                 ebuf,
//                 sizeof(ebuf),
//                 "GET /pgrest/%s HTTP/1.1\r\nHost: %s\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n",
//                 queryUrl,
//                 host
//              );
//
//  Then you can use the connection returned by mg_download in the mg_read().
//  Others have used this to enable streaming also.



// TODO: %%% remove, experimental
void HttpComm::threadfunc(ChildThreadWrapper &aThread)
{
  sleep(10);
  aThread.signalParentThread(threadSignalUserSignal);
}

// TODO: %%% remove, experimental
void HttpComm::threadsignalfunc(SyncIOMainLoop &aMainLoop, ChildThreadWrapper &aChildThread, ThreadSignals aSignalCode)
{
  DBGLOG(LOG_DEBUG,"Received signal from child thread: %d", aSignalCode);
}



void HttpComm::initiateRequest(const char *aURL, HttpCommCB aHttpCallback)
{
  string protocol, hostSpec, host, doc;
  uint16_t port;
  ErrorPtr err;

//  // TODO: %%% remove, experimental
//  SyncIOMainLoop::currentMainLoop()->executeInThread(boost::bind(&HttpComm::threadfunc, this, _1), boost::bind(&HttpComm::threadsignalfunc, this, _1, _2, _3));

  httpCallback = aHttpCallback;
  splitURL(aURL, &protocol, &hostSpec, &doc, NULL, NULL);
  bool useSSL;
  if (protocol=="http") {
    port = 80;
    useSSL = false;
  }
  else if (protocol=="https") {
    port = 443;
    useSSL = true;
  }
  else {
    err = ErrorPtr(new HttpCommError(HttpCommError_invalidParameters, "invalid protocol"));
  }
  splitHost(hostSpec.c_str(), &host, &port);
  if (Error::isOK(err)) {
    // now issue request
    const size_t ebufSz = 100;
    char ebuf[ebufSz];
    mgConn = mg_download(
      host.c_str(),
      port,
      useSSL,
      ebuf, ebufSz,
      "GET %s HTTP/1.1\r\nHost: %s\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n",
      doc.c_str(),
      host.c_str()
    );
    if (!mgConn) {
      err = ErrorPtr(new HttpCommError(HttpCommError_mongooseError, ebuf));
    }
    else {
      // successfully initiated connection, ready for mg_read
      if (httpCallback) httpCallback(this, err);
    }
  }
  if (!Error::isOK(err)) {
    // abort early
    if (httpCallback) httpCallback(this, err);
  }
}



size_t HttpComm::numBytesReady()
{
  return 0;
}


size_t HttpComm::receiveBytes(size_t aNumBytes, uint8_t *aBytes, ErrorPtr &aError)
{
  ssize_t res = 0;
  if (mgConn) {
    res = mg_read(mgConn, aBytes, aNumBytes);
    if (res==0) {
      // connection has closed, all bytes read
      closeConnection();
      return 0;
    }
    else if (res<0) {
      // read error
      aError = ErrorPtr(new HttpCommError(HttpCommError_read));
      closeConnection();
      return 0;
    }
    return res;
  }
  else {
    aError = ErrorPtr(new HttpCommError(HttpCommError_noConnection));
  }
  return res;
}



void HttpComm::closeConnection()
{
  if (mgConn) {
    mg_close_connection(mgConn);
    mgConn = NULL;
  }
}



