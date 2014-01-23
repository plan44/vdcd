//
//  Copyright (c) 2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "p44_vdcd_host.hpp"

#include "deviceclasscontainer.hpp"
#include "device.hpp"


using namespace p44;


P44VdcHost::P44VdcHost() :
  configApiServer(SyncIOMainLoop::currentMainLoop())
{
}


void P44VdcHost::startConfigApi()
{
  configApiServer.startServer(boost::bind(&P44VdcHost::configApiConnectionHandler, this, _1), 3);
}



SocketCommPtr P44VdcHost::configApiConnectionHandler(SocketComm *aServerSocketCommP)
{
  JsonCommPtr conn = JsonCommPtr(new JsonComm(SyncIOMainLoop::currentMainLoop()));
  conn->setMessageHandler(boost::bind(&P44VdcHost::configApiRequestHandler, this, _1, _2, _3));
  return conn;
}


void P44VdcHost::configApiRequestHandler(JsonComm *aJsonCommP, ErrorPtr aError, JsonObjectPtr aJsonObject)
{
  ErrorPtr err;
  // TODO: actually do something
  JsonObjectPtr json = JsonObject::newObj();
  if (Error::isOK(aError)) {
    // %%% just show
    LOG(LOG_DEBUG,"Config API request: %s", aJsonObject->c_strValue());
    // %%% and return dummy response
    json->add("Echo", aJsonObject);
  }
  else {
    LOG(LOG_DEBUG,"Invalid JSON request");
    json->add("Error", JsonObject::newString(aError->description()));
  }
  aJsonCommP->sendMessage(json);
}


