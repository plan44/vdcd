//
//  socketclient.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "socketcomm.hpp"

#include <sys/ioctl.h>
#include <sys/poll.h>

using namespace p44;

SocketComm::SocketComm(SyncIOMainLoop *aMainLoopP) :
  connectionOpen(false),
  isConnecting(false),
  connectionFd(-1),
  mainLoopP(aMainLoopP)
{
}


SocketComm::~SocketComm()
{
  internalCloseConnection();
}


void SocketComm::setClientConnection(const char* aHostNameOrAddress, const char* aServiceOrPort, int aSocketType, int aProtocolFamily, int aProtocol)
{
  closeConnection();
  hostNameOrAddress = aHostNameOrAddress;
  serviceOrPortNo = aServiceOrPort;
  protocolFamily = aProtocolFamily;
  socketType = aSocketType;
  protocol = aProtocol;
}


ErrorPtr SocketComm::initiateConnection()
{
  int res;
  ErrorPtr err;

  if (!connectionOpen && !isConnecting) {
    if (hostNameOrAddress.empty()) {
      err = ErrorPtr(new SocketCommError(SocketCommErrorNoParams,"Missing connection parameters"));
      goto done;
    }
    // try to resolve host name
    struct addrinfo *addressInfoP;
    struct addrinfo hint;
    memset(&hint, 0, sizeof(addrinfo));
    hint.ai_flags = 0; // no flags
    hint.ai_family = protocolFamily;
    hint.ai_socktype = socketType;
    hint.ai_protocol = protocol;
    res = getaddrinfo(hostNameOrAddress.c_str(), serviceOrPortNo.c_str(), &hint, &addressInfoP);
    if (res!=0) {
      // error
      err = ErrorPtr(new SocketCommError(SocketCommErrorCannotResolveHost, string_format("getaddrinfo error %d: %s", res, gai_strerror(res))));
      goto done;
    }
    // try to create a connection
    int socketFD = -1;
    struct addrinfo *aiP;
    for (aiP = addressInfoP; aiP!=NULL; aiP = aiP->ai_next) {
      socketFD = socket(aiP->ai_family, aiP->ai_socktype, aiP->ai_protocol);
      if (socketFD==-1)
        continue; // can't create socket with this address
      // socket created
      // - make socket non-blocking
      int flags;
      if ((flags = fcntl(socketFD, F_GETFL, 0))==-1)
        flags = 0;
      fcntl(socketFD, F_SETFL, flags | O_NONBLOCK);
      // - initiate connection
      res = connect(socketFD, aiP->ai_addr, aiP->ai_addrlen);
      if (res==0 || errno==EINPROGRESS) {
        // connection initiated (or already open, but connectionMonitorHandler will take care in both cases)
        LOG(LOG_NOTICE, "Connection to %s:%s initiated\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str());
        isConnecting = true;
        break;
      }
      // remember error...
      err = SysError::errNo("SocketComm: cannot initiate socket connection: ");
      close(socketFD);
      // ..but retry other addresses before giving up
    }
    freeaddrinfo(addressInfoP);
    if (isConnecting) {
      // connecting
      // - save FD
      connectionFd = socketFD;
      // - register callbacks to get notified when connection is established
      mainLoopP->registerPollHandler(
        connectionFd,
        POLLOUT|POLLERR,
        boost::bind(&SocketComm::connectionMonitorHandler, this, _1, _2, _3, _4)
      );
    }
    else {
      // not connecting, initiation failed
      LOG(LOG_NOTICE, "Failed initiating connection to %s:%s : %s\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str(), err->description().c_str());
    }
  }
done:
  if (!Error::isOK(err) && connectionStatusHandler) {
    connectionStatusHandler(this, err);
  }
  // return it
  return err;
}



bool SocketComm::connectionMonitorHandler(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD, int aPollFlags)
{
  if ((aPollFlags & POLLOUT) && isConnecting) {
    // connecting, became ready for write: connection is open now
    connectionOpen = true;
    isConnecting = false;
    LOG(LOG_NOTICE, "Connection to %s:%s established\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str());
    // call handler if defined
    if (connectionStatusHandler) {
      // connection ok
      connectionStatusHandler(this, ErrorPtr());
    }
    // register handlers for operating open connection
    mainLoopP->registerPollHandler(
      connectionFd,
      (receiveHandler ? POLLIN : 0) | // report ready to read if we have a handler
      (transmitHandler ? POLLOUT : 0), // report ready to transmit if we have a handler
      boost::bind(&SocketComm::dataMonitorHandler, this, _1, _2, _3, _4)
    );
  }
  else {
    // must be error, close connection
    ErrorPtr err;
    if (aPollFlags & POLLHUP) {
      err = ErrorPtr(new SocketCommError(SocketCommErrorHungUp, "Connection HUP while opening (= connection rejected)"));
    }
    else if (aPollFlags & POLLIN) {
      // try to get real error code
      if (read(connectionFd,NULL,0)<0)
        err = SysError::errNo("Connection attempt failed: ");
    }
    if (!err) {
      // no specific error, use general one
      err = ErrorPtr(new SocketCommError(SocketCommErrorConnecting, "Connection attempt failed"));
    }
    LOG(LOG_WARNING, "Connection attempt to %s:%s failed: %s\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str(), err->description().c_str());
    internalCloseConnection();
    if (connectionStatusHandler) {
      connectionStatusHandler(this, err);
    }
  }
  return true;
}



bool SocketComm::dataMonitorHandler(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD, int aPollFlags)
{
  if (aPollFlags & POLLHUP) {
    // other end has closed connection
    // - close my end
    internalCloseConnection();
    if (connectionStatusHandler) {
      // report reason for closing
      connectionStatusHandler(this, ErrorPtr(new SocketCommError(SocketCommErrorHungUp,"Connection closed (HUP)")));
    }
  }
  else if ((aPollFlags & POLLIN) && receiveHandler) {
    receiveHandler(this, ErrorPtr());
  }
  else if ((aPollFlags & POLLOUT) && transmitHandler) {
    transmitHandler(this, ErrorPtr());
  }
  else {
    // other event, error
    ErrorPtr err = ErrorPtr(new SocketCommError(SocketCommErrorFDErr,"Async socket error"));
    if (receiveHandler) {
      // report error
      receiveHandler(this, err);
    }
    LOG(LOG_WARNING, "Connection to %s:%s reported error: %s\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str(), err->description().c_str());
  }
  // handled
  return true;
}



void SocketComm::setConnectionStatusHandler(SocketCommCB aConnectedHandler)
{
  // set handler
  connectionStatusHandler = aConnectedHandler;
}



void SocketComm::closeConnection()
{
  if (connectionOpen) {
    // close the connection
    internalCloseConnection();
    // report to handler
    if (connectionStatusHandler) {
      // connection ok
      ErrorPtr err = ErrorPtr(new SocketCommError(SocketCommErrorClosed, "Connection closed"));
      connectionStatusHandler(this, err);
      LOG(LOG_WARNING, "Connection to %s:%s explicitly closed\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str());
    }
  }
}


void SocketComm::internalCloseConnection()
{
  // unregister from main loop
  if (connectionOpen || isConnecting) {
    mainLoopP->unregisterPollHandler(connectionFd);
    close(connectionFd);
    connectionFd = -1;
    connectionOpen = false;
    isConnecting = false;
  }
}


bool SocketComm::connected()
{
  return connectionOpen;
}


bool SocketComm::connecting()
{
  return isConnecting;
}



void SocketComm::setReceiveHandler(SocketCommCB aReceiveHandler)
{
  if (receiveHandler.empty()!=aReceiveHandler.empty()) {
    receiveHandler = aReceiveHandler;
    if (connectionOpen) {
      // If connected already, update poll flags to include data-ready-to-read
      // (otherwise, flags will be set when connection opens)
      if (receiveHandler.empty())
        mainLoopP->changePollFlags(connectionFd, 0, POLLIN); // clear POLLIN
      else
        mainLoopP->changePollFlags(connectionFd, POLLIN, 0); // set POLLIN
    }
  }
  receiveHandler = aReceiveHandler;
}


void SocketComm::setTransmitHandler(SocketCommCB aTransmitHandler)
{
  if (transmitHandler.empty()!=aTransmitHandler.empty()) {
    transmitHandler = aTransmitHandler;
    if (connectionOpen) {
      // If connected already, update poll flags to include ready-for-transmit
      // (otherwise, flags will be set when connection opens)
      if (transmitHandler.empty())
        mainLoopP->changePollFlags(connectionFd, 0, POLLOUT); // clear POLLOUT
      else
        mainLoopP->changePollFlags(connectionFd, POLLOUT, 0); // set POLLOUT
    }
  }
}


size_t SocketComm::transmitBytes(size_t aNumBytes, const uint8_t *aBytes, ErrorPtr &aError)
{
  if (!connectionOpen) {
    ErrorPtr err = initiateConnection();
    if (!Error::isOK(err)) {
      // initiation failed already
      aError = err;
      return 0; // nothing transmitted
    }
    // connection initiation ok
  }
  // if not connected now, we can't write
  if (!connectionOpen) {
    // waiting for connection to open
    return 0; // cannot transmit data yet
  }
  // connection is open, write now
  ssize_t res = write(connectionFd,aBytes,aNumBytes);
  if (res<0) {
    aError = SysError::errNo("SocketComm::transmitBytes: ");
    return 0; // nothing transmitted
  }
  return res;
}


size_t SocketComm::receiveBytes(size_t aNumBytes, uint8_t *aBytes, ErrorPtr &aError)
{
  if (connectionOpen) {
		// read
    ssize_t res = 0;
		if (aNumBytes>0) {
			res = read(connectionFd,aBytes,aNumBytes); // read
      if (res<0) {
        if (errno==EWOULDBLOCK)
          return 0; // nothing received
        else {
          aError = SysError::errNo("SocketComm::receiveBytes: ");
          return 0; // nothing transmitted
        }
      }
      return res;
    }
  }
	return 0; // connection not open, nothing to read
}


size_t SocketComm::numBytesReady()
{
  // get number of bytes ready for reading
	int numBytes; // must be int!! FIONREAD defines parameter as *int
  int res = ioctl(connectionFd, FIONREAD, &numBytes);
  return res!=0 ? 0 : numBytes;
}

