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
    SocketCommErrorNoParams,
    SocketCommErrorCannotResolveHost,
    SocketCommErrorCannotConnect,
    SocketCommErrorFDErr,
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
    bool connectionOpen;
    int connectionFd;
    SocketCommCB receiveHandler;
    SocketCommCB transmitHandler;
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


    /// open the connection (possibly blocking)
    /// @note can be called multiple times, opens connection only if not already open
    ErrorPtr openConnection();

    /// close the current connection, if any
    void closeConnection();

    /// check if connected
    /// @return true if connected.
    /// @note checking connected does not automatically try to establish a connection
    bool connected();


    /// write data (non-blocking)
    /// @param aNumBytes number of bytes to transfer
    /// @param aBytes pointer to buffer to be sent
    /// @param aError reference to ErrorPtr. Will be left untouched if no error occurs
    /// @return number ob bytes actually written
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


  protected:
    bool readyForRead(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD);
    bool readyForWrite(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD);
    bool errorOccurred(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD);


  };
  
} // namespace p44


#endif /* defined(__p44bridged__socketclient__) */
