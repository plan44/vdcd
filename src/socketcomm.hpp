//
//  socketcomm.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__socketclient__
#define __p44bridged__socketclient__

#include "p44bridged_common.hpp"

// unix I/O and network
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>


using namespace std;

namespace p44 {

  // Errors
  typedef enum {
    SocketCommErrorOK,
    SocketCommErrorNoParams, ///< parameters missing to even try initiating connection
    SocketCommErrorCannotResolveHost, ///< host cannot be resolved
    SocketCommErrorConnecting, ///< connection could not be established
    SocketCommErrorHungUp, ///< other side closed connection (hung up, HUP)
    SocketCommErrorClosed, ///< closed from my side
    SocketCommErrorFDErr, ///< error on file descriptor
  } SocketCommErrors;

  class SocketCommError : public Error
  {
  public:
    static const char *domain() { return "SocketComm"; }
    virtual const char *getErrorDomain() const { return SocketCommError::domain(); };
    SocketCommError(SocketCommErrors aError) : Error(ErrorCode(aError)) {};
    SocketCommError(SocketCommErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };


  class SocketComm;

  /// callback for signalling ready for receive or transmit, or error
  typedef boost::function<void (SocketComm *aSocketCommP, ErrorPtr aError)> SocketCommCB;

  typedef boost::shared_ptr<SocketComm> SocketCommPtr;
  /// A class providing low level access to the DALI bus
  class SocketComm 
  {
    // mainloop
    SyncIOMainLoop *mainLoopP;
    // connection parameter
    string hostNameOrAddress;
    string serviceOrPortNo;
    int protocolFamily;
    int socketType;
    int protocol;
    // connection internals
    bool isConnecting; ///< in progress of opening connection
    bool connectionOpen;
    int connectionFd;
    SocketCommCB receiveHandler;
    SocketCommCB transmitHandler;
    SocketCommCB connectionStatusHandler;
  public:

    SocketComm(SyncIOMainLoop *aMainLoopP);
    virtual ~SocketComm();

    /// Set parameters for making a client connection
    /// @param aHostNameOrAddress host name/address (1.2.3.4 or xxx.yy)
    /// @param aServiceOrPort port number or service name
    /// @param aProtocolFamily defaults to AF_UNSPEC (means that address family is derived from host name lookup)
    /// @param aSocketType defaults to SOCK_STREAM (TCP)
    /// @param aProtocol defaults to 0
    void setClientConnection(const char* aHostNameOrAddress, const char* aServiceOrPort, int aSocketType = SOCK_STREAM, int aProtocolFamily = AF_UNSPEC, int aProtocol = 0);

    /// initiate the connection (non-blocking)
    /// This starts the connection process
    /// @return if no error is returned, this means the connection could be initiated
    ///   (but actual connection might still fail)
    /// @note can be called multiple times, initiates connection only if not already open or initiated
    ///   When connection status changes, the connectionStatusHandler (if set) will be called
    /// @note if connectionStatusHandler is set, it will be called when initiation fails with the same error
    ///   code as returned by initiateConnection itself.
    ErrorPtr initiateConnection();

    /// close the current connection, if any
    /// @note can be called multiple times, closes connection if a connection is open (or connecting)
    void closeConnection();

    /// set connection status handler
    /// @param aConnectionStatusHandler will be called when connection status changes.
    ///   If callback is called without error, connection was established. Otherwise, error signals
    ///   why connection was closed
    void setConnectionStatusHandler(SocketCommCB aConnectionStatusHandler);

    /// check if connection in progress
    /// @return true if connection initiated and in progress.
    /// @note checking connecting does not automatically try to establish a connection
    bool connecting();

    /// check if connected
    /// @return true if connected.
    /// @note checking connected does not automatically try to establish a connection
    bool connected();


    /// write data (non-blocking)
    /// @param aNumBytes number of bytes to transfer
    /// @param aBytes pointer to buffer to be sent
    /// @param aError reference to ErrorPtr. Will be left untouched if no error occurs
    /// @return number ob bytes actually written, can be 0 (e.g. if connection is still in process of opening)
    size_t transmitBytes(size_t aNumBytes, const uint8_t *aBytes, ErrorPtr &aError);

    /// @return number of bytes ready for read
    size_t numBytesReady();

    /// read data (non-blocking)
    /// @param aMaxBytes max number of bytes to receive
    /// @param aBytes pointer to buffer to store received bytes
    /// @param aError reference to ErrorPtr. Will be left untouched if no error occurs
    /// @return number ob bytes actually read
    size_t receiveBytes(size_t aNumBytes, uint8_t *aBytes, ErrorPtr &aError);

    /// install callback for data becoming ready to read
    /// @param aCallBack will be called when data is ready for reading (receiveBytes()) or an asynchronous error occurs on the connection
    void setReceiveHandler(SocketCommCB aReceiveHandler);

    /// install callback for connection ready for accepting new data to send
    /// @param aCallBack will be called when connection is ready to transmit more data (using transmitBytes())
    void setTransmitHandler(SocketCommCB aTransmitHandler);


  private:
    bool readyForRead(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD, int aPollFlags);
    bool readyForWrite(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD, int aPollFlags);
    bool errorOccurred(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD, int aPollFlags);

    bool connectionMonitorHandler(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD, int aPollFlags);
    void internalCloseConnection();

  };
  
} // namespace p44


#endif /* defined(__p44bridged__socketclient__) */
