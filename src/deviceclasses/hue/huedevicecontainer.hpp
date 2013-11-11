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

#include "huecomm.hpp"
#include "deviceclasscontainer.hpp"

using namespace std;

namespace p44 {


  class HueDeviceContainer;
  class HueDevice;

  /// persistence for enocean device container
  class HuePersistence : public SQLite3Persistence
  {
    typedef SQLite3Persistence inherited;
  protected:
    /// Get DB Schema creation/upgrade SQL statements
    virtual string dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion);
  };


  typedef boost::intrusive_ptr<HueDeviceContainer> HueDeviceContainerPtr;
  class HueDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;
    friend class HueDevice;

    HueComm hueComm;
    HuePersistence db;

    CompletedCB collectedHandler;

    /// @name persistent parameters
    /// @{

    string bridgeUuid; ///< the UUID for searching the hue bridge via SSDP
    string bridgeUserName; ///< the user name registered with the bridge

    /// @}

  public:
    HueDeviceContainer(int aInstanceNumber);

		void initialize(CompletedCB aCompletedCB, bool aFactoryReset);

    virtual const char *deviceClassIdentifier() const;

    /// collect and add devices to the container
    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive);

    /// set container learn mode
    /// @param aEnableLearning true to enable learning mode
    /// @note learn events (new devices found or devices removed) must be reported by calling reportLearnEvent() on DeviceContainer.
    void setLearnMode(bool aEnableLearning);

  private:

    void refindResultHandler(ErrorPtr aError);
    void searchResultHandler(ErrorPtr aError);
    void collectLights();
    void collectedLightsHandler(JsonObjectPtr aResult, ErrorPtr aError);

  };

} // namespace p44

#endif /* defined(__vdcd__huedevicecontainer__) */
