//
//  Copyright (c) 2013 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__huecomm__
#define __vdcd__huecomm__

#include "vdcd_common.hpp"

#include "ssdpsearch.hpp"
#include "jsonwebclient.hpp"
#include "operationqueue.hpp"

using namespace std;

namespace p44 {


  // Errors
  enum {
    HueCommErrorOK,
    HueCommErrorReservedForBridge = 1, ///< 1..999 are native bridge error codes
    HueCommErrorUuidNotFound = 1000, ///< bridge specified by uuid was not found
    HueCommErrorApiNotReady, ///< API not ready (bridge not yet found, no bridge paired)
    HueCommErrorDescription, ///< SSDP by uuid did find a device, but XML description was inaccessible or invalid
    HueCommErrorInvalidUser, ///< bridge did not allow accessing the API with the username
    HueCommErrorNoRegistration, ///< could not register with a bridge
    HueCommErrorInvalidResponse, ///< invalid response from bridge (malformed JSON)
  };

  typedef int HueCommErrors;

  class HueCommError : public Error
  {
  public:
    static const char *domain() { return "HueComm"; }
    virtual const char *getErrorDomain() const { return HueCommError::domain(); };
    HueCommError(HueCommErrors aError) : Error(ErrorCode(aError)) {};
    HueCommError(HueCommErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };


  typedef enum {
    httpMethodGET,
    httpMethodPOST,
    httpMethodPUT,
    httpMethodDELETE
  } HttpMethods;


  class BridgeFinder;
  typedef boost::intrusive_ptr<BridgeFinder> BridgeFinderPtr;

  class HueComm;
  typedef boost::intrusive_ptr<HueComm> HueCommPtr;


  /// will be called to deliver api result
  /// @param aHueComm the HueComm object
  /// @param the result in case of success.
  /// - In case of PUT, POST and DELETE requests, it is the contents of the "success" response object
  /// - In case of GET requests, it is the entire answer object
  /// @param aError error in case of failure, error code is either a HueCommErrors enum or the error code as
  ///   delivered by the hue brigde itself.
  typedef boost::function<void (HueComm &aHueComm, JsonObjectPtr aResult, ErrorPtr aError)> HueApiResultCB;


  class HueApiOperation : public Operation
  {
    typedef Operation inherited;

    HueComm &hueComm;
    HttpMethods method;
    string url;
    JsonObjectPtr data;
    bool completed;
    ErrorPtr error;
    HueApiResultCB resultHandler;

    void processAnswer(JsonObjectPtr aJsonResponse, ErrorPtr aError);

  public:

    HueApiOperation(HueComm &aHueComm, HttpMethods aMethod, const char* aUrl, JsonObjectPtr aData, HueApiResultCB aResultHandler);
    virtual ~HueApiOperation();

    virtual bool initiate();
    virtual bool hasCompleted();
    virtual OperationPtr finalize(p44::OperationQueue *aQueueP);
    virtual void abortOperation(ErrorPtr aError);

  };
  typedef boost::intrusive_ptr<HueApiOperation> HueApiOperationPtr;


  class BridgeFinder;

  class HueComm : public OperationQueue
  {
    typedef OperationQueue inherited;
    friend class BridgeFinder;

    bool findInProgress;
    bool apiReady;

  public:

    HueComm();
    virtual ~HueComm();

    // HTTP communication object
    JsonWebClient bridgeAPIComm;

    // volatile vars
    string baseURL; ///< base URL for API calls

    /// @name settings
    /// @{

    string uuid; ///< the UUID for searching the hue bridge via SSDP
    string userName; ///< the user name

    /// @}

    /// @name executing regular API calls
    /// @{

    /// Query information from the API
    /// @param aUrlSuffix the suffix to append to the baseURL+userName (including leading slash)
    /// @param aResultHandler will be called with the result
    void apiQuery(const char* aUrlSuffix, HueApiResultCB aResultHandler);

    /// Send information to the API
    /// @param aMethod the HTTP method to use
    /// @param aUrlSuffix the suffix to append to the baseURL+userName (including leading slash)
    /// @param aData the data for the action to perform (JSON body of the request)
    /// @param aResultHandler will be called with the result
    /// @param aNoAutoURL if set, aUrlSuffix must be the complete URL (baseURL and userName will not be used automatically)
    void apiAction(HttpMethods aMethod, const char* aUrlSuffix, JsonObjectPtr aData, HueApiResultCB aResultHandler, bool aNoAutoURL = false);

    /// @}


    /// @name discovery and pairing
    /// @{

    /// will be called when findBridge completes
    /// @param aHueComm the HueComm object
    /// @param aError error if find/learn was not successful. If no error, HueComm is now ready to
    ///   send API commands
    typedef boost::function<void (HueComm &aHueComm, ErrorPtr aError)> HueBridgeFindCB;

    /// find and try to pair new hue bridge
    /// @param aUserName the suggested user name for the hue bridge. Identifier without spaces and funny characters
    ///   if bridge is not happy with the user name suggested, it will assign a hex string
    /// @param aDeviceType a short description to identify the type of device/software accessing the hue bridge
    /// @param aAuthTimeWindow how long we should look for hue bridges with link button pressed among the candidates
    /// @note on success, the ssdpUuid, apiToken and baseURL string member variables will be set (when aFindHandler is called)
    void findNewBridge(const char *aUserName, const char *aDeviceType, MLMicroSeconds aAuthTimeWindow, HueBridgeFindCB aFindHandler);

    /// stop finding bridges
    void stopFind();

    /// find an already known bridge again (might have different IP in DHCP environment)
    /// @param aFindHandler called to deliver find result
    /// @note ssdpUuid and apiToken member variables must be set to the pre-know bridge's parameters before calling this
    void refindBridge(HueBridgeFindCB aFindHandler);

  };
  
} // namespace p44

#endif /* defined(__vdcd__huecomm__) */
