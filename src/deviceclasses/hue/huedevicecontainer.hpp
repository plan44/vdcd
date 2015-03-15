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

    HuePersistence db;

    CompletedCB collectedHandler;

    /// @name persistent parameters
    /// @{

    string bridgeUuid; ///< the UUID for searching the hue bridge via SSDP
    string bridgeUserName; ///< the user name registered with the bridge

    /// @}

  public:

    HueDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag);

    HueComm hueComm;

		void initialize(CompletedCB aCompletedCB, bool aFactoryReset);

    virtual const char *deviceClassIdentifier() const;

    /// get supported rescan modes for this device class
    /// @return a combination of rescanmode_xxx bits
    virtual int getRescanModes() const;

    /// collect and add devices to the container
    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings);

    /// set container learn mode
    /// @param aEnableLearning true to enable learning mode
    /// @param aDisableProximityCheck true to disable proximity check (e.g. minimal RSSI requirement for some EnOcean devices)
    /// @note learn events (new devices found or devices removed) must be reported by calling reportLearnEvent() on DeviceContainer.
    void setLearnMode(bool aEnableLearning, bool aDisableProximityCheck);

    /// @return human readable, language independent suffix to explain vdc functionality.
    ///   Will be appended to product name to create modelName() for vdcs
    virtual string vdcModelSuffix() { return "hue"; }

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    /// - uuid:UUUUUUU = UUID
    virtual string hardwareGUID() { return string_format("uuid:%s", bridgeUuid.c_str()); };

    /// Get icon data or name
    /// @param aIcon string to put result into (when method returns true)
    /// - if aWithData is set, binary PNG icon data for given resolution prefix is returned
    /// - if aWithData is not set, only the icon name (without file extension) is returned
    /// @param aWithData if set, PNG data is returned, otherwise only name
    /// @return true if there is an icon, false if not
    virtual bool getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix);

    /// Get extra info (plan44 specific) to describe the addressable in more detail
    /// @return string, single line extra info describing aspects of the device not visible elsewhere
    virtual string getExtraInfo();

  private:

    void refindResultHandler(ErrorPtr aError);
    void searchResultHandler(ErrorPtr aError);
    void collectLights();
    void collectedLightsHandler(JsonObjectPtr aResult, ErrorPtr aError);

  };

} // namespace p44

#endif /* defined(__vdcd__huedevicecontainer__) */
