//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
    HueDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag);

		void initialize(CompletedCB aCompletedCB, bool aFactoryReset);

    virtual const char *deviceClassIdentifier() const;

    /// collect and add devices to the container
    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive);

    /// set container learn mode
    /// @param aEnableLearning true to enable learning mode
    /// @note learn events (new devices found or devices removed) must be reported by calling reportLearnEvent() on DeviceContainer.
    void setLearnMode(bool aEnableLearning);

    /// @return human readable model name/short description
    virtual string modelName() { return "hue vDC"; }

  private:

    void refindResultHandler(ErrorPtr aError);
    void searchResultHandler(ErrorPtr aError);
    void collectLights();
    void collectedLightsHandler(JsonObjectPtr aResult, ErrorPtr aError);

  };

} // namespace p44

#endif /* defined(__vdcd__huedevicecontainer__) */
