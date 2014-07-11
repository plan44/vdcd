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

#ifndef __vdcd__ssdpsearch__
#define __vdcd__ssdpsearch__

#include "socketcomm.hpp"

using namespace std;

namespace p44 {

  // Errors
  typedef enum {
    SsdpErrorOK,
    SsdpErrorInvalidAnswer,
    SsdpErrorTimeout,
  } SsdpErrors;

  class SsdpError : public Error
  {
  public:
    static const char *domain() { return "Ssdp"; }
    virtual const char *getErrorDomain() const { return SsdpError::domain(); };
    SsdpError(SsdpErrors aError) : Error(ErrorCode(aError)) {};
    SsdpError(SsdpErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };


  class SsdpSearch;

  typedef boost::intrusive_ptr<SsdpSearch> SsdpSearchPtr;

  /// callback for delivering a Ssdp result
  typedef boost::function<void (SsdpSearchPtr aSsdpSearch, ErrorPtr aError)> SsdpSearchCB;

  /// A class providing basic Ssdp service discovery
  class SsdpSearch : public SocketComm
  {
    typedef SocketComm inherited;

    // parameters
    bool targetMustMatch;
    SsdpSearchCB searchResultHandler;
    string searchTarget;
    bool singleTargetSearch;
    long timeoutTicket;

  public:

    // results
    string response; ///< will be set to the entire response string
    string locationURL; ///< will be set to the location of the result
    string uuid; ///< will be set to the uuid (extracted from USN header) of the result
    string server; ///< will be set to the SERVER header
    int maxAge; ///< will be set to the max-age value

    SsdpSearch(SyncIOMainLoop &aMainLoop);
    virtual ~SsdpSearch();

    /// start a SSDP search for a specific UUID or all root devices
    /// @param aSearchResultHandler will be called whenever a Ssdp search result has been received, or a search times out
    /// @param aUuidToFind the uuid to search for, or NULL to search for all root devices (target: "upnp:rootdevice")
    /// @param aVerifyUUID verify that UUID matches before delivering result via callback (default)
    void startSearch(SsdpSearchCB aSearchResultHandler, const char *aUuidToFind = NULL, bool aVerifyUUID = true);

    /// start a SSDP search
    /// @param aSearchResultHandler will be called whenever a Ssdp search result has been received, or a search times out
    /// @param aSearchTarget search target string, like "ssdp:all" or "upnp:rootdevice"
    /// @param aSingleTarget searching for single target, stop search once we got an answer
    /// @param aTargetMustMatch if set, only direct responses to our search are returned
    void startSearchForTarget(SsdpSearchCB aSearchResultHandler, const char *aSearchTarget, bool aSingleTarget, bool aTargetMustMatch);


    /// stop SSDP search - result handler
    void stopSearch();


  private:
    void gotData(ErrorPtr aError);
    void socketStatusHandler(ErrorPtr aError);
    void searchTimedOut();
  };
  
} // namespace p44


#endif /* defined(__vdcd__ssdpsearch__) */
