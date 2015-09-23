//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "socketcomm.hpp"

#include <sys/ioctl.h>
#include <sys/poll.h>

using namespace p44;

SocketComm::SocketComm(MainLoop &aMainLoop) :
  FdComm(aMainLoop),
  connectionOpen(false),
  isConnecting(false),
  isClosing(false),
  serving(false),
  clearHandlersAtClose(false),
  addressInfoList(NULL),
  currentAddressInfo(NULL),
  currentSockAddrP(NULL),
  maxServerConnections(1),
  serverConnection(NULL),
  connectionFd(-1)
{
}


SocketComm::~SocketComm()
{
  if (!isClosing) {
    internalCloseConnection();
  }
}


void SocketComm::setConnectionParams(const char* aHostNameOrAddress, const char* aServiceOrPortOrSocket, int aSocketType, int aProtocolFamily, int aProtocol)
{
  closeConnection();
  hostNameOrAddress = nonNullCStr(aHostNameOrAddress);
  serviceOrPortOrSocket = nonNullCStr(aServiceOrPortOrSocket);
  protocolFamily = aProtocolFamily;
  socketType = aSocketType;
  protocol = aProtocol;
  connectionLess = socketType==SOCK_DGRAM;
}


#pragma mark - becoming a server

ErrorPtr SocketComm::startServer(ServerConnectionCB aServerConnectionHandler, int aMaxConnections)
{
  ErrorPtr err;

  struct sockaddr *saP = NULL;
  socklen_t saLen = 0;
  int proto = IPPROTO_IP;
  int one = 1;
  int socketFD = -1;

  maxServerConnections = aMaxConnections;
  // check for protocolfamily auto-choice
  if (protocolFamily==PF_UNSPEC) {
    // not specified, choose default
    if (serviceOrPortOrSocket.size()>1 && serviceOrPortOrSocket[0]=='/')
      protocolFamily = PF_LOCAL; // absolute paths are considered local sockets
    else
      protocolFamily = PF_INET; // otherwise, default to IPv4 for now
  }
  // now start server
  if (protocolFamily==PF_INET) {
    // IPv4 socket
    struct servent *pse;
    // - create suitable socket address
    struct sockaddr_in *sinP = new struct sockaddr_in;
    saLen = sizeof(struct sockaddr_in);
    saP = (struct sockaddr *)sinP;
    memset((char *)saP, 0, saLen);
    // - set listening socket address
    sinP->sin_family = (sa_family_t)protocolFamily;
    if (nonLocal)
      sinP->sin_addr.s_addr = htonl(INADDR_ANY);
    else
      sinP->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    // get service / port
    if ((pse = getservbyname(serviceOrPortOrSocket.c_str(), NULL)) != NULL)
      sinP->sin_port = htons(ntohs((in_port_t)pse->s_port));
    else if ((sinP->sin_port = htons((in_port_t)atoi(serviceOrPortOrSocket.c_str()))) == 0) {
      err = ErrorPtr(new SocketCommError(SocketCommErrorCannotResolve,"Unknown service/port name"));
    }
    // - protocol derived from socket type
    if (protocol==0) {
      // determine protocol automatically from socket type
      if (socketType==SOCK_STREAM)
        proto = IPPROTO_TCP;
      else
        proto = IPPROTO_UDP;
    }
    else
      proto = protocol;
  }
  else if (protocolFamily==PF_LOCAL) {
    // Local (UNIX) socket
    // - create suitable socket address
    struct sockaddr_un *sunP = new struct sockaddr_un;
    saLen = sizeof(struct sockaddr_un);
    saP = (struct sockaddr *)sunP;
    memset((char *)saP, 0, saLen);
    // - set socket address
    sunP->sun_family = (sa_family_t)protocolFamily;
    strncpy(sunP->sun_path, serviceOrPortOrSocket.c_str(), sizeof (sunP->sun_path));
    sunP->sun_path[sizeof (sunP->sun_path) - 1] = '\0'; // emergency terminator
    // - protocol for local socket is not specific
    proto = 0;
  }
  else {
    // TODO: implement other portocol families, in particular PF_INET6
    err = ErrorPtr(new SocketCommError(SocketCommErrorUnsupported,"Unsupported protocol family"));
  }
  // now create and configure socket
  if (Error::isOK(err)) {
    socketFD = socket(protocolFamily, socketType, proto);
    if (socketFD<0) {
      err = SysError::errNo("Cannot create server socket: ");
    }
    else {
      // socket created, set options
      if (setsockopt(socketFD,SOL_SOCKET,SO_REUSEADDR,(char *)&one,(int)sizeof(one)) == -1) {
        err = SysError::errNo("Cannot SETSOCKOPT SO_REUSEADDR: ");
      }
      else {
        // options ok, bind to address
        if (::bind(socketFD, saP, saLen) < 0) {
          err = SysError::errNo("Cannot bind socket (server already running?): ");
        }
      }
    }
  }
  // listen
  if (Error::isOK(err)) {
    if (socketType==SOCK_STREAM && listen(socketFD, maxServerConnections) < 0) {
      err = SysError::errNo("Cannot listen on socket: ");
    }
    else {
      // listen ok or not needed, make non-blocking
      makeNonBlocking(socketFD);
      // now socket is ready, register in mainloop to receive connections
      connectionFd = socketFD;
      serving = true;
      serverConnectionHandler = aServerConnectionHandler;
      // - install callback for when FD becomes writable (or errors out)
      mainLoop.registerPollHandler(
        connectionFd,
        POLLIN,
        boost::bind(&SocketComm::connectionAcceptHandler, this, _1, _2, _3)
      );
    }
  }
  if (saP) {
    delete saP; saP = NULL;
  }
  return err;
}


bool SocketComm::connectionAcceptHandler(MLMicroSeconds aCycleStartTime, int aFd, int aPollFlags)
{
  ErrorPtr err;
  if (aPollFlags & POLLIN) {
    // server socket has data, means connection waiting to get accepted
    socklen_t fsinlen;
    struct sockaddr fsin;
    int clientFD = -1;
    fsinlen = sizeof(fsin);
    clientFD = accept(connectionFd, (struct sockaddr *) &fsin, &fsinlen);
    if (clientFD>0) {
      // get address and port of incoming connection
      char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
      if (protocolFamily==PF_LOCAL) {
        // no real address and port
        strcpy(hbuf,"local");
        strcpy(sbuf,"local_socket");
      }
      else {
        int s = getnameinfo(
          &fsin, fsinlen,
          hbuf, sizeof hbuf,
          sbuf, sizeof sbuf,
          NI_NUMERICHOST | NI_NUMERICSERV
        );
        if (s!=0) {
          strcpy(hbuf,"<unknown>");
          strcpy(sbuf,"<unknown>");
        }
      }
      // actually accepted
      // - call handler to create child connection
      SocketCommPtr clientComm;
      if (serverConnectionHandler) {
        clientComm = serverConnectionHandler(this);
      }
      if (clientComm) {
        // - set host/port
        clientComm->hostNameOrAddress = hbuf;
        clientComm->serviceOrPortOrSocket = sbuf;
        // - remember
        clientConnections.push_back(clientComm);
        LOG(LOG_DEBUG, "New client connection accepted from %s:%s (now %d connections)\n", hostNameOrAddress.c_str(), serviceOrPortOrSocket.c_str(), clientConnections.size());
        // - pass connection to child
        clientComm->passClientConnection(clientFD, this);
      }
      else {
        // can't handle connection, close immediately
        LOG(LOG_NOTICE, "Connection not accepted from %s:%s - shut down\n", hbuf, sbuf);
        shutdown(clientFD, SHUT_RDWR);
        close(clientFD);
      }
    }
  }
  // handled
  return true;
}


void SocketComm::passClientConnection(int aFd, SocketCommPtr aServerConnection)
{
  // make non-blocking
  makeNonBlocking(aFd);
  // save and mark open
  serverConnection = aServerConnection;
  // set Fd and let FdComm base class install receive & transmit handlers
  setFd(aFd);
  // save fd for my own use
  connectionFd = aFd;
  isConnecting = false;
  connectionOpen = true;
  // call handler if defined
  if (connectionStatusHandler) {
    // connection ok
    connectionStatusHandler(this, ErrorPtr());
  }
}



SocketCommPtr SocketComm::returnClientConnection(SocketCommPtr aClientConnection)
{
  SocketCommPtr endingConnection;
  // remove the client connection from the list
  for (SocketCommList::iterator pos = clientConnections.begin(); pos!=clientConnections.end(); ++pos) {
    if (pos->get()==aClientConnection.get()) {
      // found, keep around until really done with everything
      endingConnection = *pos;
      // remove from list
      clientConnections.erase(pos);
      break;
    }
  }
  LOG(LOG_DEBUG, "Client connection terminated (now %d connections)\n", clientConnections.size());
  // return connection object to prevent premature deletion
  return endingConnection;
}




#pragma mark - connecting to a client


bool SocketComm::connectable()
{
  return !hostNameOrAddress.empty();
}



ErrorPtr SocketComm::initiateConnection()
{
  int res;
  ErrorPtr err;

  if (!connectionOpen && !isConnecting && !serverConnection) {
    freeAddressInfo();
    if (protocolFamily==PF_LOCAL) {
      // local socket -> just connect, no lists to try
      LOG(LOG_DEBUG, "Initiating local socket %s connection\n", serviceOrPortOrSocket.c_str());
      hostNameOrAddress = "local"; // set it for log display
      // synthesize address info for unix socket, because standard UN*X getaddrinfo() call usually does not handle PF_LOCAL
      addressInfoList = new struct addrinfo;
      memset(addressInfoList, 0, sizeof(addrinfo));
      addressInfoList->ai_family = protocolFamily;
      addressInfoList->ai_socktype = socketType;
      addressInfoList->ai_protocol = protocol;
      struct sockaddr_un *sunP = new struct sockaddr_un;
      addressInfoList->ai_addr = (struct sockaddr *)sunP;
      addressInfoList->ai_addrlen = sizeof(struct sockaddr_un);
      memset((char *)sunP, 0, addressInfoList->ai_addrlen);
      sunP->sun_family = (sa_family_t)protocolFamily;
      strncpy(sunP->sun_path, serviceOrPortOrSocket.c_str(), sizeof (sunP->sun_path));
      sunP->sun_path[sizeof (sunP->sun_path) - 1] = '\0'; // emergency terminator
    }
    else {
      // assume internet connection -> get list of possible addresses and try them
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
      res = getaddrinfo(hostNameOrAddress.c_str(), serviceOrPortOrSocket.c_str(), &hint, &addressInfoList);
      if (res!=0) {
        // error
        err = ErrorPtr(new SocketCommError(SocketCommErrorCannotResolve, string_format("getaddrinfo error %d: %s", res, gai_strerror(res))));
        DBGLOG(LOG_DEBUG, "SocketComm: getaddrinfo failed: %s\n", err->description().c_str());
        goto done;
      }
    }
    // now try all addresses in the list
    // - init iterator pointer
    currentAddressInfo = addressInfoList;
    // - try connecting first address
    LOG(LOG_DEBUG, "Initiating connection to %s:%s\n", hostNameOrAddress.c_str(), serviceOrPortOrSocket.c_str());
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
  bool startedConnecting = false;
  while (currentAddressInfo && !startedConnecting) {
    err.reset();
    socketFD = socket(currentAddressInfo->ai_family, currentAddressInfo->ai_socktype, currentAddressInfo->ai_protocol);
    if (socketFD==-1) {
      err = SysError::errNo("Cannot create client socket: ");
    }
    else {
      // usable address found, socket created
      // - make socket non-blocking
      makeNonBlocking(socketFD);
      // Now we have a socket
      if (connectionLess) {
        // UDP: no connect phase
        startedConnecting = true;
        // save valid address info for later use (UDP needs it to send datagrams)
        if (currentSockAddrP)
          free(currentSockAddrP);
        currentSockAddrLen = currentAddressInfo->ai_addrlen;
        currentSockAddrP = (sockaddr *)malloc(currentSockAddrLen);
        memcpy(currentSockAddrP, currentAddressInfo->ai_addr, currentAddressInfo->ai_addrlen);
      }
      else {
        // TCP: initiate connection
        res = connect(socketFD, currentAddressInfo->ai_addr, currentAddressInfo->ai_addrlen);
        LOG(LOG_DEBUG, "- Attempting connection with address family = %d, protocol = %d\n", currentAddressInfo->ai_family, currentAddressInfo->ai_protocol);
        if (res==0 || errno==EINPROGRESS) {
          // connection initiated (or already open, but connectionMonitorHandler will take care in both cases)
          startedConnecting = true;
        }
        else {
          // immediate error connecting
          err = SysError::errNo("Cannot connect: ");
        }
      }
    }
    // advance to next address
    currentAddressInfo = currentAddressInfo->ai_next;
  }
  if (!startedConnecting) {
    // exhausted addresses without starting to connect
    if (!err) err = ErrorPtr(new SocketCommError(SocketCommErrorNoConnection, "No connection could be established"));
    LOG(LOG_DEBUG, "Cannot initiate connection to %s:%s\n", hostNameOrAddress.c_str(), serviceOrPortOrSocket.c_str(), err->description().c_str());
  }
  else {
    if (!connectionLess) {
      // connection in progress
      isConnecting = true;
      // - save FD
      connectionFd = socketFD;
      // - install callback for when FD becomes writable (or errors out)
      mainLoop.registerPollHandler(
        connectionFd,
        POLLOUT,
        boost::bind(&SocketComm::connectionMonitorHandler, this, _1, _2, _3)
      );
    }
    else {
      // UDP socket successfully created
      LOG(LOG_DEBUG, "Connectionless socket ready for address family = %d, protocol = %d\n", protocolFamily, protocol);
      connectionOpen = true;
      isConnecting = false;
      currentAddressInfo = NULL; // no more addresses to check
      // immediately use socket for I/O
      setFd(socketFD);
      // call handler if defined
      if (connectionStatusHandler) {
        // connection ok
        connectionStatusHandler(this, ErrorPtr());
      }
    }
  }
  // clean up if list processed
  freeAddressInfo();
  // return status
  return err;
}


#pragma mark - general connection handling


ErrorPtr SocketComm::socketError(int aSocketFd)
{
  ErrorPtr err;
  int result;
  socklen_t result_len = sizeof(result);
  if (getsockopt(aSocketFd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
    // error, fail somehow, close socket
    err = SysError::errNo("Cant get socket error status: ");
  }
  else {
    err = SysError::err(result, "Socket Error status: ");
  }
  return err;
}



bool SocketComm::connectionMonitorHandler(MLMicroSeconds aCycleStartTime, int aFd, int aPollFlags)
{
  ErrorPtr err;
  if ((aPollFlags & POLLOUT) && isConnecting) {
    // became writable, check status
    err = socketError(aFd);
  }
  else if (aPollFlags & POLLHUP) {
    err = ErrorPtr(new SocketCommError(SocketCommErrorHungUp, "Connection HUP while opening (= connection rejected)"));
  }
  else if (aPollFlags & POLLERR) {
    err = socketError(aFd);
  }
  // now check if successful
  if (Error::isOK(err)) {
    // successfully connected
    connectionOpen = true;
    isConnecting = false;
    currentAddressInfo = NULL; // no more addresses to check
    freeAddressInfo();
    LOG(LOG_DEBUG, "Connection to %s:%s established\n", hostNameOrAddress.c_str(), serviceOrPortOrSocket.c_str());
    // call handler if defined
    if (connectionStatusHandler) {
      // connection ok
      connectionStatusHandler(this, ErrorPtr());
    }
    // let FdComm base class operate open connection (will install handlers)
    setFd(aFd);
  }
  else {
    // this attempt has failed, try next (if any)
    LOG(LOG_DEBUG, "- Connection attempt failed: %s\n", err->description().c_str());
    // this will return no error if we have another address to try
    err = connectNextAddress();
    if (err) {
      // no next attempt started, report error
      LOG(LOG_WARNING, "Connection to %s:%s failed: %s\n", hostNameOrAddress.c_str(), serviceOrPortOrSocket.c_str(), err->description().c_str());
      if (connectionStatusHandler) {
        connectionStatusHandler(this, err);
      }
      freeAddressInfo();
      internalCloseConnection();
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
  if (connectionOpen && !isClosing) {
    isClosing = true; // prevent doing it more than once due to handlers called
    // report to handler
    LOG(LOG_DEBUG, "Connection with %s:%s explicitly closing\n", hostNameOrAddress.c_str(), serviceOrPortOrSocket.c_str());
    if (connectionStatusHandler) {
      // connection ok
      ErrorPtr err = ErrorPtr(new SocketCommError(SocketCommErrorClosed, "Connection closed"));
      connectionStatusHandler(this, err);
    }
    // close the connection
    internalCloseConnection();
  }
}


void SocketComm::internalCloseConnection()
{
  isClosing = true; // prevent doing it more than once due to handlers called
  if (serving) {
    // serving socket
    // - close listening socket
    mainLoop.unregisterPollHandler(connectionFd);
    close(connectionFd);
    connectionFd = -1;
    serving = false;
    // - close all child connections (closing will remove them from the list)
    while (clientConnections.size()>0) {
      SocketCommPtr conn = *clientConnections.begin();
      conn->closeConnection();
      conn->clearCallbacks(); // clear callbacks to break possible retain cycles
    }
  }
  else if (connectionOpen || isConnecting) {
    // stop monitoring data connection
    setFd(-1);
    // to make sure, also unregister handler for connectionFd (in case FdComm had no fd set yet)
    mainLoop.unregisterPollHandler(connectionFd);
    if (serverConnection) {
      shutdown(connectionFd, SHUT_RDWR);
    }
    close(connectionFd);
    connectionFd = -1;
    connectionOpen = false;
    isConnecting = false;
    // if this was a client connection to our server, let server know
    if (serverConnection) {
      serverConnection->returnClientConnection(this);
      serverConnection = NULL;
    }
  }
  // free the address info
  if (currentSockAddrP) {
    free(currentSockAddrP);
    currentSockAddrP = NULL;
  }
  // now clear handlers if requested
  if (clearHandlersAtClose) {
    clearCallbacks();
  }
  isClosing = false;
}


bool SocketComm::connected()
{
  return connectionOpen && !isClosing;
}


bool SocketComm::connecting()
{
  return isConnecting;
}


#pragma mark - connectionless data exchange


size_t SocketComm::transmitBytes(size_t aNumBytes, const uint8_t *aBytes, ErrorPtr &aError)
{
  if (connectionLess) {
    if (dataFd<0)
      return 0; // not ready yet
    ssize_t res = sendto(dataFd, aBytes, aNumBytes, 0, currentSockAddrP, currentSockAddrLen);
    if (res<0) {
      aError = SysError::errNo("SocketComm::transmitBytes (connectionless): ");
      return 0; // nothing transmitted
    }
    return res;
  }
  else {
    return inherited::transmitBytes(aNumBytes, aBytes, aError);
  }
}



#pragma mark - handling data exception


void SocketComm::dataExceptionHandler(int aFd, int aPollFlags)
{
  SocketCommPtr keepMyselfAlive(this);
  DBGLOG(LOG_DEBUG, "SocketComm::dataExceptionHandler(fd==%d, pollflags==0x%X)\n", aFd, aPollFlags);
  if (!isClosing) {
    if (aPollFlags & POLLHUP) {
      // other end has closed connection
      // - report
      if (connectionStatusHandler) {
        // report reason for closing
        connectionStatusHandler(this, ErrorPtr(new SocketCommError(SocketCommErrorHungUp,"Connection closed (HUP)")));
      }
    }
    else if (aPollFlags & POLLIN) {
      // Note: on linux a socket closed server side does not return POLLHUP, but POLLIN with no data
      // alerted for read, but nothing to read any more: assume connection closed
      ErrorPtr err = socketError(aFd);
      if (Error::isOK(err))
        err = ErrorPtr(new SocketCommError(SocketCommErrorHungUp,"Connection closed (POLLIN but no data -> interpreted as HUP)"));
      DBGLOG(LOG_DEBUG, "Connection to %s:%s has POLLIN but no data; error: %s\n", hostNameOrAddress.c_str(), serviceOrPortOrSocket.c_str(), err->description().c_str());
      // - report
      if (connectionStatusHandler) {
        // report reason for closing
        connectionStatusHandler(this, err);
      }
    }
    else if (aPollFlags & POLLERR) {
      // error
      ErrorPtr err = socketError(aFd);
      LOG(LOG_WARNING, "Connection to %s:%s reported error: %s\n", hostNameOrAddress.c_str(), serviceOrPortOrSocket.c_str(), err->description().c_str());
      // - report
      if (connectionStatusHandler) {
        // report reason for closing
        connectionStatusHandler(this, err);
      }
    }
    else {
      // NOP
      return;
    }
    // - shut down (Note: if nobody else retains the connection except the server SocketComm, this will delete the connection)
    internalCloseConnection();
  }
}

