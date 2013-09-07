//
//  huecomm.hpp
//  vdcd
//
//  Created by Lukas Zeller on 07.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
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
  typedef enum {
    HueCommErrorOK,
    HueCommErrorUuidNotFound, ///< bridge specified by ssdpUuid was not found
    HueCommErrorInvalidToken, ///< bridge did not accept the apiToken
    HueCommErrorNoRegistration, ///< could not register with a bridge
  } HueCommErrors;

  class HueCommError : public Error
  {
  public:
    static const char *domain() { return "HueComm"; }
    virtual const char *getErrorDomain() const { return HueCommError::domain(); };
    HueCommError(HueCommErrors aError) : Error(ErrorCode(aError)) {};
    HueCommError(HueCommErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };
  

  class BridgeFinder;
  typedef boost::intrusive_ptr<BridgeFinder> BridgeFinderPtr;

  class HueComm;

  typedef boost::intrusive_ptr<HueComm> HueCommPtr;
  class HueComm : public OperationQueue
  {
    typedef OperationQueue inherited;

    BridgeFinderPtr bridgeFinder;

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

    /// find an already known bridge again (might have different IP in DHCP environment)
    /// @param aFindHandler called to deliver find result
    /// @note ssdpUuid and apiToken member variables must be set to the pre-know bridge's parameters before calling this
    void refindBridge(HueBridgeFindCB aFindHandler);
  };
  
} // namespace p44

#endif /* defined(__vdcd__huecomm__) */
