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

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 7

#include "ssdpsearch.hpp"

using namespace p44;

SsdpSearch::SsdpSearch(MainLoop &aMainLoop) :
  inherited(aMainLoop)
{
  setReceiveHandler(boost::bind(&SsdpSearch::gotData, this, _1));
}


SsdpSearch::~SsdpSearch()
{
  stopSearch();
}


void SsdpSearch::startSearch(SsdpSearchCB aSearchResultHandler, const char *aUuidToFind, bool aVerifyUUID)
{
  string searchTarget;
  bool singleTarget = false;
  bool mustMatch = false;
  if (!aUuidToFind) {
    // find anything
    searchTarget = "upnp:rootdevice";
  }
  else {
    // find specific UUID
    singleTarget = true;
    searchTarget = string_format("uuid:%s",aUuidToFind);
    mustMatch = aVerifyUUID;
  }
  startSearchForTarget(aSearchResultHandler, searchTarget.c_str(), singleTarget, mustMatch);
}



// Search request:
//  M-SEARCH * HTTP/1.1
//  HOST: 239.255.255.250:1900
//  MAN: "ssdp:discover"
//  MX: 5
//  ST: upnp:rootdevice

#define SSDP_BROADCAST_ADDR "239.255.255.250"
#define SSDP_PORT "1900"
#define SSDP_MX 3 // should be sufficient (5 is max allowed)


void SsdpSearch::startSearchForTarget(SsdpSearchCB aSearchResultHandler, const char *aSearchTarget, bool aSingleTarget, bool aTargetMustMatch)
{
  // save params
  singleTargetSearch = aSingleTarget;
  searchTarget = aSearchTarget;
  searchResultHandler = aSearchResultHandler;
  targetMustMatch = aTargetMustMatch;
  // close current socket
  closeConnection();
  // setup new UDP socket
  setConnectionParams(SSDP_BROADCAST_ADDR, SSDP_PORT, SOCK_DGRAM, AF_INET);
  setConnectionStatusHandler(boost::bind(&SsdpSearch::socketStatusHandler, this, _2));
  // prepare socket for usage
  initiateConnection();
}


void SsdpSearch::socketStatusHandler(ErrorPtr aError)
{
  FOCUSLOG("SSDP socket status: %s\n", aError ? aError->description().c_str() : "<no error>");
  if (Error::isOK(aError)) {
    FOCUSLOG("### sending UDP M-SEARCH\n");
    // unregister socket status handler (or we'll get called when connection closes)
    setConnectionStatusHandler(NULL);
    // send search request
    string ssdpSearch = string_format(
      "M-SEARCH * HTTP/1.1\n"
      "HOST: %s:%s\n"
      "MAN: \"ssdp:discover\"\n"
      "MX: %d\n"
      "ST: %s\n",
      SSDP_BROADCAST_ADDR,
      SSDP_PORT,
      SSDP_MX,
      searchTarget.c_str()
    );
    transmitString(ssdpSearch);
    // start timer (wait 1.5 the MX for answers)
    timeoutTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&SsdpSearch::searchTimedOut, this), SSDP_MX*1500*MilliSecond);
  }
  else {
    // error starting search
    if (searchResultHandler) {
      searchResultHandler(this, aError);
    }
  }
}


void SsdpSearch::searchTimedOut()
{
  stopSearch();
  if (searchResultHandler) {
    searchResultHandler(this, ErrorPtr(new SsdpError(SsdpErrorTimeout, "SSDP search timed out with no (more) results")));
  }
}



void SsdpSearch::stopSearch()
{
  MainLoop::currentMainLoop().cancelExecutionTicket(timeoutTicket);
  closeConnection();
}



// M-SEARCH response
//  HTTP/1.1 200 OK
//  CACHE-CONTROL: max-age=100
//  EXT:
//  LOCATION: http://192.168.59.107:80/description.xml
//  SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/0.1
//  ST: urn:schemas-upnp-org:device:basic:1
//  USN: uuid:2f402f80-da50-11e1-9b23-0017880979ae



void SsdpSearch::gotData(ErrorPtr aError)
{
  if (Error::isOK(receiveString(response))) {
    // extract uuid and location
    const char *p = response.c_str();
    FOCUSLOG("### received UDP answer: %s\n", p);
    string line;
    bool locFound = false;
    bool uuidFound = false;
    bool serverFound = false;
    bool stFound = false;
    while (nextLine(p, line)) {
      string key, value;
      if (keyAndValue(line, key, value)) {
        if (key=="LOCATION") {
          locationURL = value;
          locFound = true;
          FOCUSLOG("Location: %s\n", locationURL.c_str());
        }
        else if (key=="ST") {
          if (targetMustMatch) {
            if (searchTarget!=value) {
              // wrong ST, discard
              LOG(LOG_INFO,"Received notify from %s, but wrong ST (%s, expected: %s) -> ignored\n", locationURL.c_str(), value.c_str(), searchTarget.c_str());
              // no more action, wait for response with correct ST
              return;
            }
          }
          stFound = true;
        }
        else if (key=="USN") {
          FOCUSLOG("USN: %s\n", value.c_str());
          // extract the UUID
          string k,v;
          if (keyAndValue(value, k, v)) {
            if (k=="uuid") {
              size_t i = v.find("::");
              if (i!=string::npos)
                uuid = v.substr(0,i);
              else
                uuid = v;
              uuidFound = true;
            }
          }
        }
        else if (key=="CACHE-CONTROL") {
          size_t pos = value.find("max-age =");
          if (pos!=string::npos) {
            sscanf(value.c_str()+9, "%d", &maxAge);
          }
        }
        else if (key=="SERVER") {
          server = value;
          serverFound = true;
          FOCUSLOG("SERVER: %s\n", server.c_str());
        }
      }
    }
    if (locFound && uuidFound && serverFound && stFound) {
      // complete response -> call back
      if (singleTargetSearch) {
        stopSearch();
      }
      if (searchResultHandler) {
        searchResultHandler(this, ErrorPtr());
      }
    }
    else {
      // invalid answer, just ignore it
      FOCUSLOG("Received invalid SEARCH response (or unrelated SSDP packet) -> ignored\n");
    }
  }
  else {
    // receiving problem, report it
    searchResultHandler(this, aError);
  }
}
