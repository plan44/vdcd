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

#ifndef __vdcd__ledchaindevicecontainer__
#define __vdcd__ledchaindevicecontainer__

#include "vdcd_common.hpp"

#if !DISABLE_LEDCHAIN

#include "deviceclasscontainer.hpp"
#include "device.hpp"

#include "ws281xcomm.hpp"


using namespace std;

namespace p44 {

  class LedChainDeviceContainer;
  class LedChainDevice;
  typedef boost::intrusive_ptr<LedChainDevice> LedChainDevicePtr;


  /// persistence for LedChain device container
  class LedChainDevicePersistence : public SQLite3Persistence  {
    typedef SQLite3Persistence inherited;
  protected:
    /// Get DB Schema creation/upgrade SQL statements
    virtual string dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion);
  };


	typedef std::multimap<string, string> DeviceConfigMap;
	
  typedef boost::intrusive_ptr<LedChainDeviceContainer> LedChainDeviceContainerPtr;
  class LedChainDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;
    friend class LedChainDevice;

    LedChainDevicePersistence db;

    int numLedsInChain;
    uint8_t maxOutValue;
    WS281xCommPtr ws281xcomm;

    typedef std::list<LedChainDevicePtr> LedChainDeviceList;

    LedChainDeviceList sortedSegments; ///< list of devices, ordered by firstLED
    uint16_t renderStart; ///< first LED needing rendering (valid if renderTicket!=0)
    uint16_t renderEnd; ///< end of rendering range = first LED not needing rendering (valid if renderTicket!=0)
    long renderTicket;

  public:
  
    LedChainDeviceContainer(int aInstanceNumber, int aNumLedsInChain, DeviceContainer *aDeviceContainerP, int aTag);

    void initialize(StatusCB aCompletedCB, bool aFactoryReset);

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings);

    void removeDevice(DevicePtr aDevice, bool aForget);

    /// set max output value to send to WS2812 LEDs. This is like a global brightness limit, to prevent LED chain
    /// power supply overload
    /// @param aMaxOutValue max output value, 0..255.
    void setMaxOutValue(uint8_t aMaxOutValue) { maxOutValue = aMaxOutValue; };

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
    virtual string vdcModelSuffix() { return "WS281x LED Chains"; }

  private:

    static bool segmentCompare(LedChainDevicePtr aFirst, LedChainDevicePtr aSecond);
    LedChainDevicePtr addLedChainDevice(uint16_t aFirstLED, uint16_t aNumLEDs, string aDeviceConfig);

    void triggerRenderingRange(uint16_t aFirst, uint16_t aNum);
    void render();

  };

} // namespace p44

#endif // !DISABLE_LEDCHAIN

#endif /* defined(__vdcd__ledchaindevicecontainer__) */
