//
//  fdcomm.cpp
//  p44utils
//
//  Created by Lukas Zeller on 09.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "fdcomm.hpp"

#include <sys/ioctl.h>
#include <sys/poll.h>

using namespace p44;

FdComm::FdComm(SyncIOMainLoop *aMainLoopP) :
  dataFd(-1),
  mainLoopP(aMainLoopP)
{
}


FdComm::~FdComm()
{
  // unregister handlers
  setFd(-1);
}


void FdComm::setFd(int aFd)
{
  if (dataFd!=aFd) {
    if (dataFd>=0) {
      // unregister previous fd
      mainLoopP->unregisterPollHandler(dataFd);
      dataFd = -1;
    }
    dataFd = aFd;
    if (dataFd>=0) {
      // register new fd
      mainLoopP->registerPollHandler(
        dataFd,
        (receiveHandler ? POLLIN : 0) | // report ready to read if we have a handler
        (transmitHandler ? POLLOUT : 0), // report ready to transmit if we have a handler
        boost::bind(&FdComm::dataMonitorHandler, this, _1, _2, _3, _4)
      );
    }
  }
}


void FdComm::dataExceptionHandler(int aFd, int aPollFlags)
{
  DBGLOG(LOG_DEBUG, "FdComm::dataExceptionHandler(fd==%d, pollflags==0x%X)\n", aFd, aPollFlags);
}



bool FdComm::dataMonitorHandler(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFd, int aPollFlags)
{
  //DBGLOG(LOG_DEBUG, "FdComm::dataMonitorHandler(time==%lld, fd==%d, pollflags==0x%X)\n", aCycleStartTime, aFd, aPollFlags);
  if (aPollFlags & POLLHUP) {
    // other end has closed connection
    dataExceptionHandler(aFd, aPollFlags);
  }
  else if ((aPollFlags & POLLIN) && receiveHandler) {
    // Note: on linux a socket closed server side does not return POLLHUP, but POLLIN
    if (numBytesReady()>0)
      receiveHandler(this, ErrorPtr());
    else {
      // alerted for read, but nothing to read any more - is also an exception
      dataExceptionHandler(aFd, aPollFlags);
    }
  }
  else if ((aPollFlags & POLLOUT) && transmitHandler) {
    transmitHandler(this, ErrorPtr());
  }
  else if (aPollFlags & POLLERR) {
    // error
    dataExceptionHandler(aFd, aPollFlags);
  }
  // handled
  return true;
}




void FdComm::setReceiveHandler(FdCommCB aReceiveHandler)
{
  if (receiveHandler.empty()!=aReceiveHandler.empty()) {
    receiveHandler = aReceiveHandler;
    if (dataFd>=0) {
      // If connected already, update poll flags to include data-ready-to-read
      // (otherwise, flags will be set when connection opens)
      if (receiveHandler.empty())
        mainLoopP->changePollFlags(dataFd, 0, POLLIN); // clear POLLIN
      else
        mainLoopP->changePollFlags(dataFd, POLLIN, 0); // set POLLIN
    }
  }
  receiveHandler = aReceiveHandler;
}


void FdComm::setTransmitHandler(FdCommCB aTransmitHandler)
{
  if (transmitHandler.empty()!=aTransmitHandler.empty()) {
    transmitHandler = aTransmitHandler;
    if (dataFd>=0) {
      // If connected already, update poll flags to include ready-for-transmit
      // (otherwise, flags will be set when connection opens)
      if (transmitHandler.empty())
        mainLoopP->changePollFlags(dataFd, 0, POLLOUT); // clear POLLOUT
      else
        mainLoopP->changePollFlags(dataFd, POLLOUT, 0); // set POLLOUT
    }
  }
}


size_t FdComm::transmitBytes(size_t aNumBytes, const uint8_t *aBytes, ErrorPtr &aError)
{
  // if not connected now, we can't write
  if (dataFd<0) {
    // waiting for connection to open
    return 0; // cannot transmit data yet
  }
  // connection is open, write now
  ssize_t res = write(dataFd,aBytes,aNumBytes);
  if (res<0) {
    aError = SysError::errNo("FdComm::transmitBytes: ");
    return 0; // nothing transmitted
  }
  return res;
}


bool FdComm::transmitString(string &aString)
{
  ErrorPtr err;
  size_t res = transmitBytes(aString.length(), (uint8_t *)aString.c_str(), err);
  return Error::isOK(err) && res==aString.length(); // ok if no error and all bytes sent
}




size_t FdComm::receiveBytes(size_t aNumBytes, uint8_t *aBytes, ErrorPtr &aError)
{
  if (dataFd>=0) {
		// read
    ssize_t res = 0;
		if (aNumBytes>0) {
			res = read(dataFd,aBytes,aNumBytes); // read
      if (res<0) {
        if (errno==EWOULDBLOCK)
          return 0; // nothing received
        else {
          aError = SysError::errNo("FdComm::receiveBytes: ");
          return 0; // nothing received
        }
      }
      return res;
    }
  }
	return 0; // no fd set, nothing to read
}


ErrorPtr FdComm::receiveAndAppendToString(string &aString, ssize_t aMaxBytes)
{
  ErrorPtr err;
  size_t max = numBytesReady();
  if (aMaxBytes>0 && max>aMaxBytes) max = aMaxBytes;
  uint8_t *buf = new uint8_t[max];
  size_t b = receiveBytes(max, buf, err);
  if (Error::isOK(err)) {
    // received
    aString.append((char *)buf, b);
  }
  delete[] buf;
  return err;
}


ErrorPtr FdComm::receiveString(string &aString, ssize_t aMaxBytes)
{
  aString.erase();
  return receiveAndAppendToString(aString, aMaxBytes);
}



size_t FdComm::numBytesReady()
{
  if (dataFd>=0) {
    // get number of bytes ready for reading
    int numBytes; // must be int!! FIONREAD defines parameter as *int
    int res = ioctl(dataFd, FIONREAD, &numBytes);
    return res!=0 ? 0 : numBytes;
  }
	return 0; // no fd set, nothing to read
}


void FdComm::makeNonBlocking(int aFd)
{
  if (aFd<0) aFd = dataFd;
  int flags;
  if ((flags = fcntl(aFd, F_GETFL, 0))==-1)
    flags = 0;
  fcntl(aFd, F_SETFL, flags | O_NONBLOCK);
}


