//
//  socketclient.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "socketcomm.hpp"

#include <sys/ioctl.h>


using namespace p44;

SocketComm::SocketComm(SyncIOMainLoop *aMainLoopP) :
  connectionOpen(false),
  connectionFd(-1),
  mainLoopP(aMainLoopP)
{
}


SocketComm::~SocketComm()
{
  closeConnection();
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


ErrorPtr SocketComm::openConnection()
{
  int res;

  if (!connectionOpen && !hostNameOrAddress.empty()) {
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
      return ErrorPtr(new SocketCommError(SocketCommErrorCannotResolveHost, string_format("getaddrinfo error %d: %s", res, gai_strerror(res))));
    }
    // try to create a connection
    // TODO: also make connect non-blocking
    int socketFD = -1;
    struct addrinfo *aiP;
    for (aiP = addressInfoP; aiP!=NULL; aiP = aiP->ai_next) {
      socketFD = socket(aiP->ai_family, aiP->ai_socktype, aiP->ai_protocol);
      if (socketFD==-1)
        continue;
      if (connect(socketFD, aiP->ai_addr, aiP->ai_addrlen)!=-1)
        break;
      close(socketFD);
    }
    freeaddrinfo(addressInfoP);
    if (aiP==NULL) {
      // none of the addresses succeeded
      return ErrorPtr(new SocketCommError(SocketCommErrorCannotConnect, "cannot connect socket"));
    }
    connectionFd = socketFD;
    // make non-blocking
    int flags;
    if ((flags = fcntl(connectionFd, F_GETFL, 0))==-1)
      flags = 0;
    fcntl(connectionFd, F_SETFL, flags | O_NONBLOCK);
    // register with main loop
    mainLoopP->registerSyncIOHandlers(
      connectionFd,
      boost::bind(&SocketComm::readyForRead, this, _1, _2, _3),
      boost::bind(&SocketComm::readyForWrite, this, _1, _2, _3),
      boost::bind(&SocketComm::errorOccurred, this, _1, _2, _3)
    );
    // connected
    connectionOpen = true;
  }
  return ErrorPtr(); // no error
}


void SocketComm::closeConnection()
{
  // unregister from main loop
  if (connectionOpen) {
    mainLoopP->unregisterSyncIOHandlers(connectionFd);
    close(connectionFd);
    connectionFd = -1;
  }
}


void SocketComm::setReceiveHandler(SocketCommCB aReceiveHandler)
{
  receiveHandler = aReceiveHandler;
}


void SocketComm::setTransmitHandler(SocketCommCB aTransmitHandler)
{
  transmitHandler = aTransmitHandler;
}


size_t SocketComm::transmitBytes(size_t aNumBytes, const uint8_t *aBytes, ErrorPtr &aError)
{
  ErrorPtr err = openConnection();
  if (!Error::isOK(err)) {
    aError = err;
    return 0; // nothing transmitted
  }
  // connection is open
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
  size_t numBytes;
  ioctl(connectionFd, FIONREAD, &numBytes);
  return numBytes<0 ? 0 : numBytes;
}



bool SocketComm::readyForRead(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD)
{
  if (receiveHandler) {
    receiveHandler(this, ErrorPtr());
  }
  return true;
}


bool SocketComm::readyForWrite(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD)
{
  if (transmitHandler) {
    transmitHandler(this, ErrorPtr());
  }
  return true;
}


bool SocketComm::errorOccurred(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD)
{
  if (receiveHandler) {
    receiveHandler(this, ErrorPtr(new SocketCommError(SocketCommErrorFDErr,"Async socket error")));
  }
  return true;
}



