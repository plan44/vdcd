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

#ifndef __vdcd__staticdevicecontainer__
#define __vdcd__staticdevicecontainer__

#include "vdcd_common.hpp"

#include "deviceclasscontainer.hpp"
#include "device.hpp"

using namespace std;

namespace p44 {

  class StaticDeviceContainer;

  /// persistence for static device container
  class StaticDevicePersistence : public SQLite3Persistence  {
    typedef SQLite3Persistence inherited;
  protected:
    /// Get DB Schema creation/upgrade SQL statements
    virtual string dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion);
  };


  class StaticDevice : public Device
  {
    typedef Device inherited;
    friend class StaticDeviceContainer;

    long long staticDeviceRowID; ///< the ROWID this device was created from (0=none)

  public:

    StaticDevice(DeviceClassContainer *aClassContainerP);

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "static"; };

    StaticDeviceContainer &getStaticDeviceContainer();

    /// check if device can be disconnected by software (i.e. Web-UI)
    /// @return true if device might be disconnectable by the user via software (i.e. web UI)
    /// @note devices returning true here might still refuse disconnection on a case by case basis when
    ///   operational state does not allow disconnection.
    /// @note devices returning false here might still be disconnectable using disconnect() triggered
    ///   by vDC API "remove" method.
    virtual bool isSoftwareDisconnectable();

    /// disconnect device. For static device, this means removing the config from the container's DB. Note that command line
    /// static devices cannot be disconnected.
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDisconnectResultHandler will be called to report true if device could be disconnected,
    ///   false in case it is certain that the device is still connected to this and only this vDC
    virtual void disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler);

  };
  typedef boost::intrusive_ptr<StaticDevice> StaticDevicePtr;


	typedef std::multimap<string, string> DeviceConfigMap;
	
  typedef boost::intrusive_ptr<StaticDeviceContainer> StaticDeviceContainerPtr;
  class StaticDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;
    friend class StaticDevice;

		DeviceConfigMap deviceConfigs;

    StaticDevicePersistence db;

  public:
    StaticDeviceContainer(int aInstanceNumber, DeviceConfigMap aDeviceConfigs, DeviceContainer *aDeviceContainerP, int aTag);

    void initialize(CompletedCB aCompletedCB, bool aFactoryReset);

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive);

    /// some containers (statically defined devices for example) should be invisible for the dS system when they have no
    /// devices.
    /// @return if true, this device class should not be announced towards the dS system when it has no devices
    virtual bool invisibleWhenEmpty() { return true; }

    /// vdc level methods (p44 specific, JSON only, for configuring static devices)
    virtual ErrorPtr handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams);

    /// @return human readable, language independent suffix to explain vdc functionality.
    ///   Will be appended to product name to create modelName() for vdcs
    virtual string vdcModelSuffix() { return "GPIO,I2C,console"; }

  private:

    StaticDevicePtr addStaticDevice(string aDeviceType, string aDeviceConfig);

  };

} // namespace p44


#endif /* defined(__vdcd__staticdevicecontainer__) */
