//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of vdcd.
//
//  vdcd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  vdcd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with vdcd. If not, see <http://www.gnu.org/licenses/>.
//

#include "vdcapi.hpp"

using namespace p44;


#pragma mark - VdcApiServer

VdcApiServer::VdcApiServer() :
  inherited(SyncIOMainLoop::currentMainLoop())
{
}


void VdcApiServer::start()
{
  inherited::startServer(boost::bind(&VdcApiServer::serverConnectionHandler, this, _1), 3);
}


void VdcApiServer::setConnectionStatusHandler(VdcApiConnectionCB aConnectionCB)
{
  apiConnectionStatusHandler = aConnectionCB;
}


SocketCommPtr VdcApiServer::serverConnectionHandler(SocketComm *aServerSocketCommP)
{
  // create new connection
  VdcApiConnectionPtr apiConnection = newConnection();
  SocketCommPtr socketComm = apiConnection->socketConnection();
  socketComm->relatedObject = apiConnection; // bind object to connection
  socketComm->setConnectionStatusHandler(boost::bind(&VdcApiServer::connectionStatusHandler, this, _1, _2));
  // return the socketComm object which handles this connection
  return socketComm;
}


void VdcApiServer::connectionStatusHandler(SocketComm *aSocketComm, ErrorPtr aError)
{
  if (apiConnectionStatusHandler) {
    // get connection object
    VdcApiConnectionPtr apiConnection = boost::dynamic_pointer_cast<VdcApiConnection>(aSocketComm->relatedObject);
    if (apiConnection) {
      apiConnectionStatusHandler(apiConnection, aError);
    }
  }
  if (!Error::isOK(aError)) {
    // connection failed/closed and we don't support reconnect yet
    aSocketComm->relatedObject.reset(); // detach connection object
  }
}



#pragma mark - VdcApiConnection


void VdcApiConnection::setRequestHandler(VdcApiRequestCB aApiRequestHandler)
{
  apiRequestHandler = aApiRequestHandler;
}


void VdcApiConnection::closeConnection()
{
  if (socketConnection()) {
    socketConnection()->closeConnection();
  }
}


#pragma mark - VdcApiRequest

ErrorPtr VdcApiRequest::sendError(ErrorPtr aErrorToSend)
{
  if (!Error::isOK(aErrorToSend)) {
    return sendError((uint32_t)aErrorToSend->getErrorCode(), aErrorToSend->getErrorMessage());
  }
  return ErrorPtr();
}





