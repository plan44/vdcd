//
//  huedevicecontainer.hpp
//  vdcd
//
//  Created by Lukas Zeller on 02.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__huedevicecontainer__
#define __vdcd__huedevicecontainer__

#include "vdcd_common.hpp"

#include "ssdpsearch.hpp"
#include "jsonwebclient.hpp"

#include "deviceclasscontainer.hpp"

using namespace std;

namespace p44 {


  class HueDeviceContainer;
  typedef boost::intrusive_ptr<HueDeviceContainer> HueDeviceContainerPtr;
  class HueDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;

    CompletedCB collectedHandler;

    // discovery
    SsdpSearchPtr bridgeSearcher;
    typedef map<string, string> StringStringMap;
    StringStringMap bridgeCandiates; ///< possible candidates for hue bridges, key=uuid, value=description URL
    StringStringMap::iterator currentBridgeCandidate; ///< next candidate for bridge
    StringStringMap authCandidates; ///< bridges to try auth with, key=uuid, value=baseURL
    StringStringMap::iterator currentAuthCandidate; ///< next auth candiate

    // HTTP communication object
    JsonWebClient bridgeAPIComm;

    /// @name persistent parameters
    /// @{

    string ssdpUuid; ///< the UUID for searching the hue bridge via SSDP
    string apiToken; ///< the API token

    /// @}

    // volatile vars
    string baseURL; ///< base URL for API calls

  public:
    HueDeviceContainer(int aInstanceNumber);

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aExhaustive);

    void setLearnMode(bool aEnableLearning);

  private:

    void bridgeDiscoveryHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError);
    void bridgeRefindHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError);

    void processCurrentBridgeCandidate();
    void handleBridgeDescriptionAnswer(const string &aResponse, ErrorPtr aError);

    void processCurrentAuthCandidate();
    void handleBridgeAuthAnswer(JsonObjectPtr aJsonResponse, ErrorPtr aError);


  };

} // namespace p44

#endif /* defined(__vdcd__huedevicecontainer__) */
