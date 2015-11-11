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
#define FOCUSLOGLEVEL 7


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
  initializedCB = aCompletedCB;
  start();
}


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



void VoxnetComm::voxnetInitialized(ErrorPtr aError)
{
  if (initializedCB) {
    StatusCB cb = initializedCB;
    initializedCB = NULL;
    cb(aError);
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
    FOCUSLOG("Voxnet Text: %s", line.c_str());
    switch (commState) {
      case commState_menuwait: {
        // skip menu text until empty line is found
        if (trimWhiteSpace(line).size()==0) {
          commState = commState_servicesread;
          // initiate reading list of services
          sendString("2\r");
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
          sendString("8\r");
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
                    FOCUSLOG("Voxnet Text: Extracted ID=%s, alias=%s, name='%s'", id.c_str(), alias.c_str(), name.c_str());
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
          if (ref.size()>0) {
            if (ref[0]=='$') {
              // alias reference, get ID
              StringStringMap::iterator pos = aliases.find(ref);
              if (pos!=aliases.end()) {
                ref = pos->second;
              }
              else {
                ref.clear(); // alias not found
              }
            }
          }
          if (ref.size()>0 && ref[0]=='#') {
            // ID reference
            // - now check for status
            e = line.find_first_of(":",i);
            if (e!=string::npos) {
              string cmd;
              cmd.assign(line, i, e-i); // copy command
              i = e+1;
              if (cmd=="status") {
                // call back
                if (voxnetStatusHandler) {
                  voxnetStatusHandler(ref, line.substr(i));
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

