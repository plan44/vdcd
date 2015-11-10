//
//  Copyright (c) 2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "voxnetcomm.hpp"

#if ENABLE_VOXNET

#define VOXNET_TEXT_SERVICE "11244"
#define VOXNET_CONNECTION_RETRY_INTERVAL 30

using namespace p44;


VoxnetComm::VoxnetComm() :
  inherited(MainLoop::currentMainLoop()),
  commState(commState_unknown)
{
}


VoxnetComm::~VoxnetComm()
{
  closeConnection();
}


void VoxnetComm::setConnectionSpecification(const char *aVoxnetHost)
{
  setConnectionParams(aVoxnetHost, VOXNET_TEXT_SERVICE, SOCK_STREAM);
}


void VoxnetComm::initialize(StatusCB aCompletedCB)
{
  start();
  if (aCompletedCB) aCompletedCB(ErrorPtr());
}


void VoxnetComm::start()
{
  setConnectionStatusHandler(boost::bind(&VoxnetComm::connectionStatusHandler, this, _2));
  setReceiveHandler(boost::bind(&VoxnetComm::dataHandler, this, _1), '\n'); // line-by-line text interface
  initiateConnection();
}


void VoxnetComm::connectionStatusHandler(ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // opened successfully, start quering interface
    LOG(LOG_DEBUG, "Voxnet Text connection opened successfully, expecting menu now");
    commState = commState_menuwait;
  }
  else {
    // connection level error
    LOG(LOG_ERR, "Voxnet Text connection error: %s -> closing + reconnecting in %d seconds", aError->description().c_str(), VOXNET_CONNECTION_RETRY_INTERVAL);
    closeConnection();
    // retry later
    MainLoop::currentMainLoop().executeOnce(boost::bind(&VoxnetComm::start, this), VOXNET_CONNECTION_RETRY_INTERVAL*Second);
  }
}


void VoxnetComm::dataHandler(ErrorPtr aError)
{
  string line;
  receiveDelimitedString(line);
  LOG(LOG_DEBUG, "Voxnet Text: %s", line.c_str());
  switch (commState) {
    case commState_menuwait:
      // skip menu text until empty line is found
      if (trimWhiteSpace(line).size()==0) {
        commState = commState_servicesread;
        // initiate reading list of services
        sendString("2\r");
      }
      break;
    case commState_servicesread:
      // skip menu text until empty line is found
      break;
    default:
      break;
  }


}


#endif // ENABLE_VOXNET

