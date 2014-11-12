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

#ifndef __vdcd__oladevicecontainer__
#define __vdcd__oladevicecontainer__

#include "vdcd_common.hpp"

#if !DISABLE_OLA

#include "deviceclasscontainer.hpp"
#include "device.hpp"

#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/client/StreamingClient.h>

using namespace std;

namespace p44 {

  class OlaDeviceContainer;
  class OlaDevice;
  typedef boost::intrusive_ptr<OlaDevice> OlaDevicePtr;

  /// persistence for ola device container
  class OlaDevicePersistence : public SQLite3Persistence  {
    typedef SQLite3Persistence inherited;
  protected:
    /// Get DB Schema creation/upgrade SQL statements
    virtual string dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion);
  };


	typedef std::multimap<string, string> DeviceConfigMap;
	
  typedef boost::intrusive_ptr<OlaDeviceContainer> OlaDeviceContainerPtr;
  class OlaDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;
    friend class OlaDevice;

    OlaDevicePersistence db;

    ola::DmxBuffer dmxBuffer;
    ola::client::StreamingClient olaClient;
    long dmxSenderTicket;
    

  public:
    OlaDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag);

    void initialize(CompletedCB aCompletedCB, bool aFactoryReset);

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive);

    /// some containers (statically defined devices for example) should be invisible for the dS system when they have no
    /// devices.
    /// @return if true, this device class should not be announced towards the dS system when it has no devices
    virtual bool invisibleWhenEmpty() { return true; }

    /// vdc level methods (p44 specific, JSON only, for configuring static devices)
    virtual ErrorPtr handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams);

    /// Get icon data or name
    /// @param aIcon string to put result into (when method returns true)
    /// - if aWithData is set, binary PNG icon data for given resolution prefix is returned
    /// - if aWithData is not set, only the icon name (without file extension) is returned
    /// @param aWithData if set, PNG data is returned, otherwise only name
    /// @return true if there is an icon, false if not
    virtual bool getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix);

    /// @return human readable, language independent suffix to explain vdc functionality.
    ///   Will be appended to product name to create modelName() for vdcs
    virtual string vdcModelSuffix() { return "OLA/DMX512"; }

  private:

    OlaDevicePtr addOlaDevice(string aDeviceType, string aDeviceConfig);

    void dmxSend();

  };

} // namespace p44

#endif // !DISABLE_OLA
#endif /* defined(__vdcd__oladevicecontainer__) */
