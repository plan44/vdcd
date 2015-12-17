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

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 6


#include "voxnetcomm.hpp"

#if ENABLE_VOXNET

#define VOXNET_TEXT_SERVICE "11244"
#define VOXNET_CONNECTION_RETRY_INTERVAL 30

using namespace p44;


VoxnetComm::VoxnetComm() :
  inherited(MainLoop::currentMainLoop()),
  searchTimeoutTicket(0),
  statusRequestTicket(0),
  commState(commState_unknown)
{
}


VoxnetComm::~VoxnetComm()
{
  closeConnection();
}


#pragma mark - discovery

#define VOXNET_DISCOVERY_BROADCASTADDR "255.255.255.255"
#define VOXNET_DISCOVERY_PORT "11224"
#define VOXNET_DISCOVERY_REQUEST "FOXNET:DISCOVER:REQUEST"
#define VOXNET_DISCOVERY_RESPONSE_PREFIX "FOXNET:DISCOVER:ANSWER:"

#define VOXNET_DISCOVERY_TIMEOUT (5*Second)
#define VOXNET_DISCOVERY_RETRY_INTERVAL (60*Second)


void VoxnetComm::initialize(StatusCB aCompletedCB)
{
  initializedCB = aCompletedCB;
  discoverAndStart();
}


void VoxnetComm::voxnetInitialized(ErrorPtr aError)
{
  if (initializedCB) {
    StatusCB cb = initializedCB;
    initializedCB = NULL;
    cb(aError);
  }
}


void VoxnetComm::setConnectionSpecification(const char *aVoxnetHost)
{
  manualServerIP = nonNullCStr(aVoxnetHost);
}




void VoxnetComm::discoverAndStart()
{
  // close current socket
  closeConnection();
  // check for fixed IP
  if (!manualServerIP.empty()) {
    // start server TCP/IP connection
    setConnectionParams(manualServerIP.c_str(), VOXNET_TEXT_SERVICE, SOCK_STREAM);
    start();
    return;
  }
  // setup new UDP socket
  setConnectionParams(VOXNET_DISCOVERY_BROADCASTADDR, VOXNET_DISCOVERY_PORT, SOCK_DGRAM, AF_INET);
  enableBroadcast(true);
  setConnectionStatusHandler(boost::bind(&VoxnetComm::searchSocketStatusHandler, this, _2));
  setReceiveHandler(boost::bind(&VoxnetComm::searchDataHandler, this, _1)); // line-by-line text interface
  // prepare socket for usage
  initiateConnection();
}


void VoxnetComm::searchSocketStatusHandler(ErrorPtr aError)
{
  FOCUSLOG("Voxnet discovery socket status: %s", aError ? aError->description().c_str() : "<no error>");
  if (Error::isOK(aError)) {
    FOCUSLOG("### sending Voxnet discovery request");
    // unregister socket status handler (or we'll get called when connection closes)
    setConnectionStatusHandler(NULL);
    // send search request
    transmitString(VOXNET_DISCOVERY_REQUEST);
    // start timer
    searchTimeoutTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&VoxnetComm::searchTimedOut, this), VOXNET_DISCOVERY_TIMEOUT);
  }
  else {
    // error starting search, try again later
    voxnetInitialized(aError); // failed starting discovery
  }
}


void VoxnetComm::stopDiscovery()
{
  MainLoop::currentMainLoop().cancelExecutionTicket(searchTimeoutTicket);
  closeConnection();
}



void VoxnetComm::searchDataHandler(ErrorPtr aError)
{
  string response;
  if (Error::isOK(receiveIntoString(response))) {
    FOCUSLOG("Voxnet discovery response: %s", response.c_str());
    // check if this is a Voxnet discovery response
    if (response.find(VOXNET_DISCOVERY_RESPONSE_PREFIX)==0) {
      string reportedIP = response.substr(strlen(VOXNET_DISCOVERY_RESPONSE_PREFIX));
      LOG(LOG_NOTICE, "Found Voxnet server at %s", reportedIP.c_str());
      // stop search
      stopDiscovery();
      // start server TCP/IP connection
      setConnectionParams(reportedIP.c_str(), VOXNET_TEXT_SERVICE, SOCK_STREAM);
      start();
    }
  }
  else {
    FOCUSLOG("Voxnet: error reading discovery data: %s", aError->description().c_str());
  }
}



void VoxnetComm::searchTimedOut()
{
  // No server found now
  stopDiscovery();
  // end initialisation for now
  if (initializedCB) {
    initializedCB(ErrorPtr());
    initializedCB = NULL;
  }
}




#pragma mark - server communication


void VoxnetComm::start()
{
  rooms.clear(); // start with empty list
  users.clear(); // start with empty list
  sources.clear(); // start with empty list
  setConnectionStatusHandler(boost::bind(&VoxnetComm::connectionStatusHandler, this, _2));
  setReceiveHandler(boost::bind(&VoxnetComm::dataHandler, this, _1), '\n'); // line-by-line text interface
  initiateConnection();
}


void VoxnetComm::connectionStatusHandler(ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // opened successfully, start quering interface
    FOCUSLOG("Voxnet Text connection opened successfully, expecting menu now");
    commState = commState_menuwait;
  }
  else {
    // connection level error
    LOG(LOG_ERR, "Voxnet Text connection error: %s -> closing + reconnecting in %d seconds", aError->description().c_str(), VOXNET_CONNECTION_RETRY_INTERVAL);
    closeConnection();
    // keep trying, but report ok in case this was initialisation
    voxnetInitialized(ErrorPtr());
    // retry later
    MainLoop::currentMainLoop().executeOnce(boost::bind(&VoxnetComm::start, this), VOXNET_CONNECTION_RETRY_INTERVAL*Second);
  }
}



void VoxnetComm::sendVoxnetText(const string aVoxNetText)
{
  LOG(LOG_NOTICE, "Voxnet <- vDC: %s", aVoxNetText.c_str());
  sendString(aVoxNetText + "\r");
}


void VoxnetComm::requestStatus()
{
  sendVoxnetText("8");
}



void VoxnetComm::resolveVoxnetRef(string &aVoxNetRef)
{
  if (aVoxNetRef.size()>0) {
    if (aVoxNetRef[0]=='$') {
      // alias reference, get ID
      StringStringMap::iterator pos = aliases.find(aVoxNetRef);
      if (pos!=aliases.end()) {
        aVoxNetRef = pos->second;
      }
      else {
        aVoxNetRef.clear(); // alias not found
      }
    }
  }
}


//      id              alias             name                      type
//  -------------------------------------------------------------------------------
//  1   #P00113220A2A40 $P00113220A2A40   Proxy 1                   SYN.00.proxy
//  5   #X00113220A2A40 $X00113220A2A40   Timer 1                   SYN.00.timer
//  9   #S00113220A2A40 $S00113220A2A40   My Music 1                SYN.00.server
//  10  #S00113220A2A41 $S00113220A2A41   My Music 2                SYN.00.server
//  11  #U00113220A2A40 $U00113220A2A40   User 1                    SYN.00.user
//  12  #U00113220A2A41 $U00113220A2A41   User 2                    SYN.00.user
//  13  #R001EC0DD0B1D0 $r.zone2          Zone 2                    219.80.room
//  14  #S001EC0DD0B1D0 $s.zone2          Inputs Zone 2             219.80.local
//  15  #T001EC0DD0B1D0 $t.zone2          T001EC0DD0B1D0            219.80.trigger
//  16  #P001EC0DD0B1D0 $P001EC0DD0B1D0   P001EC0DD0B1D0            219.80.proxy
//  19  #X001EC0DD0B1D0 $X001EC0DD0B1D0   Timer                     219.80.timer


void VoxnetComm::dataHandler(ErrorPtr aError)
{
  string line;
  if (receiveDelimitedString(line)) {
    FOCUSLOG("Voxnet -> vDC: %s", line.c_str());
    switch (commState) {
      case commState_menuwait: {
        // skip menu text until empty line is found
        if (trimWhiteSpace(line).size()==0) {
          commState = commState_servicesread;
          // initiate reading list of services
          sendVoxnetText("2");
        }
        break;
      }
      case commState_servicesread: {
        // analyze services list
        if (line.size()==0) {
          // end of services
          FOCUSLOG("Voxnet Text: services list processing complete");
          commState = commState_idle;
          voxnetInitialized(ErrorPtr());
          // initiate sending status
          requestStatus();
          break;
        }
        // - skip header lines
        if (line[0]==' ' || line[0]=='-') break;
        // - list line, split into bits
        size_t e = 0;
        string id;
        string name;
        string alias;
        // line number
        size_t i = 0;
        e = line.find_first_of(" \t", i); // end of line number
        if (e!=string::npos) {
          // - line number unused
          // ID
          i = line.find_first_not_of(" \t", e); // start of ID
          if (i!=string::npos) {
            e = line.find_first_of(" \t", i); // end of ID
            if (e!=string::npos) {
              id.assign(line, i, e-i); // copy ID
              // alias
              i = line.find_first_not_of(" \t", e); // start of alias
              if (i!=string::npos) {
                e = line.find_first_of(" \t", i); // end of alias
                if (e!=string::npos) {
                  alias.assign(line, i, e-i); // copy alias
                  // name
                  i = line.find_first_not_of(" \t", e); // start of name
                  if (i!=string::npos) {
                    name.assign(line, i, 25); // copy name, max 25 chars
                    name = trimWhiteSpace(name);
                    //FOCUSLOG("Voxnet Text: Extracted ID=%s, alias=%s, name='%s'", id.c_str(), alias.c_str(), name.c_str());
                    if (id.size()>=2) {
                      // map alias to ID
                      aliases[alias] = id;
                      // add to list
                      switch (id[1]) {
                        case 'R': rooms[id] = name; break;
                        case 'U': users[id] = name; break;
                        case 'S': sources[id] = name; break;
                      }
                    }
                  }
                }
              }
            }
          }
        }
        break;
      }
      case commState_idle: {
        // watch for status
        size_t i = 0;
        size_t e = line.find_first_of(":",i);
        if (e!=string::npos) {
          string ref;
          ref.assign(line,0,e);
          i = e+1;
          resolveVoxnetRef(ref);
          if (ref.size()>0 && ref[0]=='#') {
            // ID reference
            // - now check for status
            e = line.find_first_of(":",i);
            if (e!=string::npos) {
              string cmd;
              cmd.assign(line, i, e-i); // copy command
              i = e+1;
              if (
                cmd=="status" || // status command
                (cmd.substr(0,1)=="$" && line[i]=='[') // or music source confirm command
              ) {
                // status:
                //   $MyMusic1:status:streaming=radio:info_1=SRF 3
                //   :info_2=ANGUS AND JULIA STONE - GRIZZLY BEAR:info_3=:info_4=Buffer Level\: 90%
                // music source confirm:
                //   $MyMusic1:$M00113220A2A41:[next]:ok
                //   $MyMusic1:$M00113220A2A40:[play\:4]:ok
                //   $r.zone1:$A0011324822231:[room\:off]:ok
                // call back
                if (voxnetStatusHandler) {
                  if (voxnetStatusHandler(ref, line.substr(i))) {
                    if (!MainLoop::currentMainLoop().rescheduleExecutionTicket(statusRequestTicket, 1*Second)) {
                      MainLoop::currentMainLoop().executeOnce(boost::bind(&VoxnetComm::requestStatus, this), 1*Second);
                    }
                  }
                }
              }
            }
          }
        }
        break;
      }
      default: {
        break;
      }
    }
  }
}


#endif // ENABLE_VOXNET

