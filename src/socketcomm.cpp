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
  addressInfoList(NULL),
  currentAddressInfo(NULL),
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
    freeAddressInfo();
    if (hostNameOrAddress.empty()) {
      err = ErrorPtr(new SocketCommError(SocketCommErrorNoParams,"Missing connection parameters"));
      goto done;
    }
    // try to resolve host name
    struct addrinfo hint;
    memset(&hint, 0, sizeof(addrinfo));
    hint.ai_flags = 0; // no flags
    hint.ai_family = protocolFamily;
    hint.ai_socktype = socketType;
    hint.ai_protocol = protocol;
    res = getaddrinfo(hostNameOrAddress.c_str(), serviceOrPortNo.c_str(), &hint, &addressInfoList);
    if (res!=0) {
      // error
      err = ErrorPtr(new SocketCommError(SocketCommErrorCannotResolveHost, string_format("getaddrinfo error %d: %s", res, gai_strerror(res))));
      goto done;
    }
    // now try all addresses in the list
    // - init iterator pointer
    currentAddressInfo = addressInfoList;
    // - try connecting first address
    LOG(LOG_DEBUG, "Initiating connection to %s:%s\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str());
    err = connectNextAddress();
  }
done:
  if (!Error::isOK(err) && connectionStatusHandler) {
    connectionStatusHandler(this, err);
  }
  // return it
  return err;
}


void SocketComm::freeAddressInfo()
{
  if (!currentAddressInfo && addressInfoList) {
    // entire list consumed, free it
    freeaddrinfo(addressInfoList);
    addressInfoList = NULL;
  }
}


ErrorPtr SocketComm::connectNextAddress()
{
  int res;
  ErrorPtr err;

  // close possibly not fully open connection FD
  internalCloseConnection();
  // try to create a socket
  int socketFD = -1;
  // as long as we have more addresses to check and not already connecting
  bool connectingAgain = false;
  while (currentAddressInfo && !connectingAgain) {
    err.reset();
    socketFD = socket(currentAddressInfo->ai_family, currentAddressInfo->ai_socktype, currentAddressInfo->ai_protocol);
    if (socketFD==-1) {
      err = SysError::errNo("Cannot create socket: ");
    }
    else {
      // usable address found, socket created
      // - make socket non-blocking
      int flags;
      if ((flags = fcntl(socketFD, F_GETFL, 0))==-1)
        flags = 0;
      fcntl(socketFD, F_SETFL, flags | O_NONBLOCK);
      // - initiate connection
      res = connect(socketFD, currentAddressInfo->ai_addr, currentAddressInfo->ai_addrlen);
      LOG(LOG_DEBUG, "- Attempting connection with address family = %d, protocol = %d\n", currentAddressInfo->ai_family, currentAddressInfo->ai_protocol);
      if (res==0 || errno==EINPROGRESS) {
        // connection initiated (or already open, but connectionMonitorHandler will take care in both cases)
        connectingAgain = true;
      }
      else {
        // immediate error connecting
        err = SysError::errNo("Cannot connect: ");
      }
    }
    // advance to next address
    currentAddressInfo = currentAddressInfo->ai_next;
  }
  if (!connectingAgain) {
    // exhausted addresses without starting to connect
    if (!err) err = ErrorPtr(new SocketCommError(SocketCommErrorNoConnection, "No connection could be established"));
    LOG(LOG_DEBUG, "Cannot initiate connection to %s:%s\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str(), err->description().c_str());
  }
  else {
    // connection in progress
    isConnecting = true;
    // - save FD
    connectionFd = socketFD;
    // - install callback for when FD becomes writable (or errors out)
    mainLoopP->registerPollHandler(
      connectionFd,
      POLLOUT,
      boost::bind(&SocketComm::connectionMonitorHandler, this, _1, _2, _3, _4)
    );
  }
  // clean up if list processed
  freeAddressInfo();
  // return status
  return err;
}



ErrorPtr SocketComm::connectionError()
{
  ErrorPtr err;
  int result;
  socklen_t result_len = sizeof(result);
  if (getsockopt(connectionFd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
    // error, fail somehow, close socket
    err = SysError::errNo("Cant get socket error status: ");
  }
  else {
    err = SysError::err(result, "Socket Error status: ");
  }
  return err;
}



bool SocketComm::connectionMonitorHandler(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD, int aPollFlags)
{
  ErrorPtr err;
  if ((aPollFlags & POLLOUT) && isConnecting) {
    // became writable, check status
    err = connectionError();
  }
  else if (aPollFlags & POLLHUP) {
    err = ErrorPtr(new SocketCommError(SocketCommErrorHungUp, "Connection HUP while opening (= connection rejected)"));
  }
  else if (aPollFlags & POLLERR) {
    err = connectionError();
  }
  // now check if successful
  if (Error::isOK(err)) {
    // successfully connected
    connectionOpen = true;
    isConnecting = false;
    currentAddressInfo = NULL; // no more addresses to check
    freeAddressInfo();
    LOG(LOG_DEBUG, "Connection to %s:%s established\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str());
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
    // this attempt has failed, try next (if any)
    LOG(LOG_DEBUG, "- Connection attempt failed: %s\n", err->description().c_str());
    // this will return no error if we have another address to try
    err = connectNextAddress();
    if (err) {
      // no next attempt started, report error
      LOG(LOG_WARNING, "Connection to %s:%s failed: %s\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str(), err->description().c_str());
      internalCloseConnection();
      if (connectionStatusHandler) {
        connectionStatusHandler(this, err);
      }
      freeAddressInfo();
    }
  }
  // handled
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
  else if (aPollFlags & POLLERR) {
    // error
    ErrorPtr err = connectionError();
    LOG(LOG_WARNING, "Connection to %s:%s reported error: %s\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str(), err->description().c_str());
    // - shut down
    internalCloseConnection();
    if (connectionStatusHandler) {
      // report reason for closing
      connectionStatusHandler(this, err);
    }
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
      LOG(LOG_NOTICE, "Connection to %s:%s explicitly closed\n", hostNameOrAddress.c_str(), serviceOrPortNo.c_str());
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

