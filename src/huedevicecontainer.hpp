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
#include "httpcomm.hpp"

#include "deviceclasscontainer.hpp"

using namespace std;

namespace p44 {


  class HueDeviceContainer;
  typedef boost::intrusive_ptr<HueDeviceContainer> HueDeviceContainerPtr;
  class HueDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;

    SsdpSearchPtr bridgeSearcher;
    CompletedCB collectedHandler;

    HttpComm bridgeAPIComm;

    /// @name persistent parameters
    /// @{

    string ssdpUuid; ///< the UUID for searching the hue bridge via SSDP
    string apiToken; ///< the API token

    /// @}

    // volatile vars
    string baseURL;

  public:
    HueDeviceContainer(int aInstanceNumber);

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aExhaustive);

    void setLearnMode(bool aEnableLearning);

  private:

    void bridgeDiscoveryHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError);
    void bridgeRefindHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError);

    void handleBridgeAnswer(ErrorPtr aError);

    // TODO: remove, experimental only %%%
    void threadfunc(ChildThreadWrapper &aThread);
    void threadsignalfunc(SyncIOMainLoop &aMainLoop, ChildThreadWrapper &aChildThread, ThreadSignals aSignalCode);


  };

} // namespace p44

#endif /* defined(__vdcd__huedevicecontainer__) */
