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

#ifndef __vdcd__vzughomecomm__
#define __vdcd__vzughomecomm__

#include "vdcd_common.hpp"

#if ENABLE_VZUGHOME

#include "socketcomm.hpp"
#include "operationqueue.hpp"
#include "jsonwebclient.hpp"

using namespace std;

namespace p44 {

  typedef std::list<string> StringList;

  class VZugHomeDiscovery : public SocketComm
  {
    typedef SocketComm inherited;

    long timeoutTicket;
    StatusCB statusCB; // discopvery status callback

  public:
    /// create driver Voxnet
    VZugHomeDiscovery();

    /// destructor
    ~VZugHomeDiscovery();

    /// discover V-ZUG devices
    void discover(StatusCB aStatusCB, MLMicroSeconds aTimeout);

    /// stop discovery early
    void stopDiscovery();

    /// list of API base URLs
    StringList baseURLs;

  private:

    void socketStatusHandler(MLMicroSeconds aTimeout, ErrorPtr aError);
    void searchTimedOut();
    void gotData(ErrorPtr aError);

  };
  typedef boost::intrusive_ptr<VZugHomeDiscovery> VZugHomeDiscoveryPtr;



  /// will be called to deliver api result
  /// @param aResult the result in case of success.
  /// @param aError error in case of failure
  typedef boost::function<void (JsonObjectPtr aResult, ErrorPtr aError)> VZugHomeResultCB;

  class VZugHomeComm;

  class VZugHomeOperation : public Operation
  {
    typedef Operation inherited;

    VZugHomeComm &vzugHomeComm;
    string url;
    JsonObjectPtr data;
    bool completed;
    bool hasJSONresult;
    ErrorPtr error;
    VZugHomeResultCB resultHandler;

    void processJsonAnswer(JsonObjectPtr aJsonResponse, ErrorPtr aError);
    void processPlainAnswer(const string &aResponse, ErrorPtr aError);

  public:

    VZugHomeOperation(VZugHomeComm &aHueComm, const char* aUrl, JsonObjectPtr aData, bool aHasJSONResult, VZugHomeResultCB aResultHandler);
    virtual ~VZugHomeOperation();

    virtual bool initiate();
    virtual bool hasCompleted();
    virtual OperationPtr finalize(p44::OperationQueue *aQueueP);
    virtual void abortOperation(ErrorPtr aError);

  };
  typedef boost::intrusive_ptr<VZugHomeOperation> VZugHomeOperationPtr;



  class VZugHomeComm : public OperationQueue
  {
    typedef OperationQueue inherited;


  public:
    /// create driver Voxnet
    VZugHomeComm();

    /// destructor
    ~VZugHomeComm();

    // HTTP communication object
    JsonWebClient apiComm;

    // volatile vars
    string baseURL; ///< base URL for API calls

    /// Send API command/value
    /// @param aToInterface if set, command goes to interface, otherwise to device
    /// @param aCommand the command name
    /// @param aValue the value, depending on the command
    /// @param aHasJSONResult if true, action expects a JSON result, if false the action returns a plain string, which is reported
    ///   back to the caller as a JSON string object
    /// @param aResultHandler will be called with the result
    void apiCommand(bool aToInterface, const char* aCommand, const char* aValue, bool aHasJSONResult, VZugHomeResultCB aResultHandler);

    /// Send request to the API
    /// @param aUrlSuffix the suffix to append to the baseURL+userName (including leading slash, including URLencoded parameters)
    /// @param aData the data for the action to perform (JSON POST body of the request, if any - NOTE: current VZug API does not do POST)
    /// @param aHasJSONResult if true, action expects a JSON result, if false the action returns a plain string, which is reported
    ///   back to the caller as a JSON string object
    /// @param aResultHandler will be called with the result
    void apiRequest(const char* aUrlSuffix, JsonObjectPtr aData, bool aHasJSONResult, VZugHomeResultCB aResultHandler);

  };
  typedef boost::intrusive_ptr<VZugHomeComm> VZugHomeCommPtr;


} // namespace p44

#endif // ENABLE_VZUGHOME

#endif /* defined(__vdcd__vzughomecomm__) */

