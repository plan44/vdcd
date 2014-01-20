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

#ifndef __p44utils__socketcomm__
#define __p44utils__socketcomm__

#include "p44_common.hpp"

#include "fdcomm.hpp"

// unix I/O and network
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
    SocketCommErrorUnsupported, ///< unsupported mode/feature
    SocketCommErrorCannotResolve, ///< host or service name cannot be resolved
    SocketCommErrorNoConnection, ///< no connection could be established (none of the addresses worked)
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

  typedef boost::intrusive_ptr<SocketComm> SocketCommPtr;
  typedef std::list<SocketCommPtr> SocketCommList;

  /// callback for signalling ready for receive or transmit, or error
  typedef boost::function<void (SocketComm *aSocketCommP, ErrorPtr aError)> SocketCommCB;
  /// callback for accepting new server connections
  /// @return must return a new SocketComm connection object which will handle the connection
  typedef boost::function<SocketCommPtr (SocketComm *aServerSocketCommP)> ServerConnectionCB;


  /// A class providing socket communication (client and server)
  class SocketComm : public FdComm
  {
    typedef FdComm inherited;

    // connection parameter
    string hostNameOrAddress;
    string serviceOrPortNo;
    int protocolFamily;
    int socketType;
    int protocol;
    bool nonLocal;
    bool connectionLess;
    // connection making fd (for server to listen, for clients or server handlers for opening connection)
    int connectionFd;
    // client connection internals
    struct addrinfo *addressInfoList; ///< list of possible connection addresses
    struct addrinfo *currentAddressInfo; ///< address currently connecting to
    struct sockaddr *currentSockAddrP; ///< address info as currently in use by open connection
    socklen_t currentSockAddrLen; ///< length of current sockAddr struct
    bool isConnecting; ///< in progress of opening connection
    bool isClosing; ///< in progress of closing connection
    bool connectionOpen; ///< regular data connection is open
    bool serving; ///< is serving socket
    SocketCommCB connectionStatusHandler;
    // server connection internals
    int maxServerConnections;
    ServerConnectionCB serverConnectionHandler;
    SocketCommList clientConnections;
    SocketComm *serverConnection;
  public:

    SocketComm(SyncIOMainLoop &aMainLoop);
    virtual ~SocketComm();

    /// Set parameters for connection (client and server)
    /// @param aHostNameOrAddress host name/address (1.2.3.4 or xxx.yy) - client only
    /// @param aServiceOrPort port number or service name
    /// @param aSocketType defaults to SOCK_STREAM (TCP)
    /// @param aProtocolFamily defaults to AF_UNSPEC (means that address family is derived from host name lookup)
    /// @param aProtocol defaults to 0
    void setConnectionParams(const char* aHostNameOrAddress, const char* aServiceOrPort, int aSocketType = SOCK_STREAM, int aProtocolFamily = AF_UNSPEC, int aProtocol = 0);

    /// get host name we are connected to (useful for server to query connecting client's address)
    /// @return name or IP address of host (for server: actually connected, for client: as set with setConnectionParams())
    const char *getHost() { return hostNameOrAddress.c_str(); };

    /// get port or service name
    /// @return port (for server: actually connected, for client: as set with setConnectionParams())
    const char *getPort() { return serviceOrPortNo.c_str(); };

    /// Set if server may accept non-local connections
    /// @param aAllow if set, server accepts non-local connections
    void setAllowNonlocalConnections(bool aAllow) { nonLocal = aAllow; };

    /// start the server
    /// @param aConnectionStatusHandler will be called when a server connection is accepted
    ///   The SocketComm object passed in the handler is a new SocketComm object for that particular connection
    /// @param aMaxConnections max number of simultaneous server connections
    /// @param aNonLocal if set, connections from other hosts are allowed. Default is false, which means only
    ///   local connections are accepted
    ErrorPtr startServer(ServerConnectionCB aServerConnectionHandler, int aMaxConnections);

    /// initiate the connection (non-blocking)
    /// This starts the connection process
    /// @return if no error is returned, this means the connection could be initiated
    ///   (but actual connection might still fail)
    /// @note can be called multiple times, initiates connection only if not already open or initiated
    ///   When connection status changes, the connectionStatusHandler (if set) will be called
    /// @note if connectionStatusHandler is set, it will be called when initiation fails with the same error
    ///   code as returned by initiateConnection itself.
    ErrorPtr initiateConnection();

    /// close the current connection, if any, or stop the server and close all client connections in case of a server
    /// @note can be called multiple times, closes connection if a connection is open (or connecting)
    void closeConnection();

    /// set connection status handler
    /// @param aConnectionStatusHandler will be called when connection status changes.
    ///   If callback is called without error, connection was established. Otherwise, error signals
    ///   why connection was closed
    void setConnectionStatusHandler(SocketCommCB aConnectionStatusHandler);

    /// check if parameters set so connection could be initiated
    /// @return true if connection can be initiated
    bool connectable();

    /// check if connection in progress
    /// @return true if connection initiated and in progress.
    /// @note checking connecting does not automatically try to establish a connection
    bool connecting();

    /// check if connected
    /// @return true if connected.
    /// @note checking connected does not automatically try to establish a connection
    /// @note for connectionless sockets, connected() means "ready to transmit/receive data"
    bool connected();

    /// write data (non-blocking)
    /// @param aNumBytes number of bytes to transfer
    /// @param aBytes pointer to buffer to be sent
    /// @param aError reference to ErrorPtr. Will be left untouched if no error occurs
    /// @return number ob bytes actually written, can be 0 (e.g. if connection is still in process of opening)
    /// @note for UDP, the host/port specified in setConnectionParams() will be used to send datagrams to
    virtual size_t transmitBytes(size_t aNumBytes, const uint8_t *aBytes, ErrorPtr &aError);


  private:
    void freeAddressInfo();
    ErrorPtr socketError(int aSocketFd);
    ErrorPtr connectNextAddress();
    bool connectionMonitorHandler(SyncIOMainLoop &aMainLoop, MLMicroSeconds aCycleStartTime, int aFd, int aPollFlags);
    void internalCloseConnection();
    virtual void dataExceptionHandler(int aFd, int aPollFlags);

    bool connectionAcceptHandler(SyncIOMainLoop &aMainLoop, MLMicroSeconds aCycleStartTime, int aFd, int aPollFlags);
    void passClientConnection(int aFD, SocketComm *aServerConnectionP); // used by listening SocketComm to pass accepted client connection to child SocketComm
    SocketCommPtr returnClientConnection(SocketComm *aClientConnectionP); // used to notify listening SocketComm when client connection ends

  };
  
} // namespace p44


#endif /* defined(__p44utils__socketcomm__) */
