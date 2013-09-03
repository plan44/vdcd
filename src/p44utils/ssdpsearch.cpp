//
//  ssdpsearch.cpp
//  p44utils
//
//  Created by Lukas Zeller on 02.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "ssdpsearch.hpp"

using namespace p44;

SsdpSearch::SsdpSearch(SyncIOMainLoop *aMainLoopP) :
  inherited(aMainLoopP)
{
  setReceiveHandler(boost::bind(&SsdpSearch::gotData, this, _2));
}


SsdpSearch::~SsdpSearch()
{
}


// Search request:
//  M-SEARCH * HTTP/1.1
//  HOST: 239.255.255.250:1900
//  MAN: "ssdp:discover"
//  MX: 10
//  ST: upnp:rootdevice

#define SSDP_BROADCAST_ADDR "239.255.255.250"
#define SSDP_PORT "1900"
#define SSDP_MX 5


void SsdpSearch::startSearch(const string &aSearchTarget, SsdpSearchCB aSearchResultHandler)
{
  // save params
  searchTarget = aSearchTarget;
  searchResultHandler = aSearchResultHandler;
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
  if (Error::isOK(aError)) {
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
  }
  else {
    // error starting search
    if (searchResultHandler) {
      searchResultHandler(this, aError);
    }
  }
}




// Notify responses
//  NOTIFY * HTTP/1.1
//  HOST: 239.255.255.250:1900
//  CACHE-CONTROL: max-age=100
//  LOCATION: http://192.168.59.107:80/description.xml
//  SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/0.1
//  NTS: ssdp:alive
//  NT: urn:schemas-upnp-org:device:basic:1
//  USN: uuid:2f402f80-da50-11e1-9b23-0017880979ae



void SsdpSearch::gotData(ErrorPtr aError)
{
  string s;
  if (Error::isOK(receiveString(s))) {
    puts(s.c_str());
  }
}
