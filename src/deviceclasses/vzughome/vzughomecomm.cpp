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
#define FOCUSLOGLEVEL 5

#include "vzughomecomm.hpp"

#if ENABLE_VZUGHOME

#include "socketcomm.hpp"

using namespace p44;

#pragma mark - VZugHomeDiscovery

VZugHomeDiscovery::VZugHomeDiscovery() :
  inherited(MainLoop::currentMainLoop()),
  timeoutTicket(0),
  statusCB(NULL)
{
  setReceiveHandler(boost::bind(&VZugHomeDiscovery::gotData, this, _1));
}


/// destructor
VZugHomeDiscovery::~VZugHomeDiscovery()
{
  stopDiscovery();
}



// Search request:
//  M-SEARCH * HTTP/1.1
//  HOST: 239.255.255.250:1900
//  MAN: "ssdp:discover"
//  MX: 5
//  ST: upnp:rootdevice

#define VZUGHOME_DISCOVERY_BROADCASTADDR "255.255.255.255"
#define VZUGHOME_DISCOVERY_PORT "2047"
#define VZUGHOME_DISCOVERY_REQUEST "DISCOVERY_LAN_INTERFACE_REQUEST"
#define VZUGHOME_DISCOVERY_RESPONSE "DISCOVERY_LAN_INTERFACE_RESPONSE"


void VZugHomeDiscovery::discover(StatusCB aStatusCB, MLMicroSeconds aTimeout)
{
  // forget previous connection specifications
  baseURLs.clear();
  // save callback
  statusCB = aStatusCB;
  // close current socket
  closeConnection();
  // setup new UDP socket
  setConnectionParams(VZUGHOME_DISCOVERY_BROADCASTADDR, VZUGHOME_DISCOVERY_PORT, SOCK_DGRAM, AF_INET);
  enableBroadcast(true);
  setConnectionStatusHandler(boost::bind(&VZugHomeDiscovery::socketStatusHandler, this, aTimeout, _2));
  // prepare socket for usage
  initiateConnection();
}


void VZugHomeDiscovery::socketStatusHandler(MLMicroSeconds aTimeout, ErrorPtr aError)
{
  FOCUSLOG("V-Zug home discovery socket status: %s", aError ? aError->description().c_str() : "<no error>");
  if (Error::isOK(aError)) {
    FOCUSLOG("### sending V-Zug home discovery request");
    // unregister socket status handler (or we'll get called when connection closes)
    setConnectionStatusHandler(NULL);
    // send search request
    transmitString(VZUGHOME_DISCOVERY_REQUEST);
    // start timer (wait 1.5 the MX for answers)
    timeoutTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&VZugHomeDiscovery::searchTimedOut, this), aTimeout);
  }
  else {
    // error starting search
    if (statusCB) {
      statusCB(aError);
    }
  }
}


void VZugHomeDiscovery::stopDiscovery()
{
  MainLoop::currentMainLoop().cancelExecutionTicket(timeoutTicket);
  closeConnection();
}



void VZugHomeDiscovery::searchTimedOut()
{
  stopDiscovery();
  if (statusCB) {
    // successful end of discovery
    statusCB(ErrorPtr());
  }
}


void VZugHomeDiscovery::gotData(ErrorPtr aError)
{
  string response;
  if (Error::isOK(receiveIntoString(response))) {
    FOCUSLOG("V-Zug home discovery response: %s", response.c_str());
    // check if this is a VZUG discovery response
    if (response==VZUGHOME_DISCOVERY_RESPONSE) {
      string a,p;
      getDatagramOrigin(a,p);
      LOG(LOG_DEBUG, "V-Zug home device responds from %s:%s", a.c_str(), p.c_str());
      // add to connection base URLs
      string url = string_format("http://%s", a.c_str());
      for (StringList::iterator pos=baseURLs.begin(); pos!=baseURLs.end(); ++pos) {
        if (*pos==url) {
          // already in list -> done
          return;
        }
      }
      // not a duplicate, save in list
      baseURLs.push_back(url);
    }
  }
  else {
    FOCUSLOG("V-Zug home discovery error: %s", aError->description().c_str());
  }
}


#pragma mark - VZugHomeOperation

VZugHomeOperation::VZugHomeOperation(VZugHomeComm &aVzugHomeComm, const char* aUrl, JsonObjectPtr aData, bool aHasJSONResult, VZugHomeResultCB aResultHandler) :
  vzugHomeComm(aVzugHomeComm),
  url(aUrl),
  data(aData),
  hasJSONresult(aHasJSONResult),
  resultHandler(aResultHandler),
  completed(false)
{
}



VZugHomeOperation::~VZugHomeOperation()
{

}



bool VZugHomeOperation::initiate()
{
  if (!canInitiate())
    return false;
  // initiate the web request
  const char *methodStr;
  if (data) {
    // has a body, needs to be sent via POST
    methodStr = "POST";
  }
  else {
    // assume GET
    methodStr = "GET";
  }
  if (hasJSONresult) {
    // will return a JSON result
    vzugHomeComm.apiComm.jsonRequest(url.c_str(), boost::bind(&VZugHomeOperation::processJsonAnswer, this, _1, _2), methodStr, data);
  }
  else {
    // will return a plain string
    vzugHomeComm.apiComm.httpRequest(url.c_str(), boost::bind(&VZugHomeOperation::processPlainAnswer, this, _1, _2), methodStr, NULL);
  }
  // executed
  return inherited::initiate();
}



void VZugHomeOperation::processJsonAnswer(JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  error = aError;
  if (Error::isOK(error)) {
    data = aJsonResponse;
  }
  // done
  completed = true;
  // have queue reprocessed
  vzugHomeComm.processOperations();
}


void VZugHomeOperation::processPlainAnswer(const string &aResponse, ErrorPtr aError)
{
  error = aError;
  if (Error::isOK(error)) {
    // return as a JSON string object
    data = JsonObject::newString(aResponse);
  }
  // done
  completed = true;
  // have queue reprocessed
  vzugHomeComm.processOperations();
}




bool VZugHomeOperation::hasCompleted()
{
  return completed;
}



OperationPtr VZugHomeOperation::finalize(p44::OperationQueue *aQueueP)
{
  if (resultHandler) {
    resultHandler(data, error);
    resultHandler = NULL; // call once only
  }
  return OperationPtr(); // no operation to insert
}



void VZugHomeOperation::abortOperation(ErrorPtr aError)
{
  if (!aborted) {
    if (!completed) {
      vzugHomeComm.apiComm.cancelRequest();
    }
    if (resultHandler) {
      resultHandler(JsonObjectPtr(), aError);
      resultHandler = NULL; // call once only
    }
  }
}




#pragma mark - VZugHomeComm

VZugHomeComm::VZugHomeComm() :
  inherited(MainLoop::currentMainLoop()),
  apiComm(MainLoop::currentMainLoop())
{
}


VZugHomeComm::~VZugHomeComm()
{
}


void VZugHomeComm::apiAction(const char* aUrlSuffix, JsonObjectPtr aData, bool aHasJSONResult, VZugHomeResultCB aResultHandler)
{
  string url = baseURL + nonNullCStr(aUrlSuffix);
  VZugHomeOperationPtr op = VZugHomeOperationPtr(new VZugHomeOperation(*this, url.c_str(), aData, aHasJSONResult, aResultHandler));
  queueOperation(op);
  // process operations
  processOperations();
}








#endif // ENABLE_VZUGHOME

