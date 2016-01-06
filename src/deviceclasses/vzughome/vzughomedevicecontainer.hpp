//
//  Copyright (c) 2015-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__vzughomedevicecontainer__
#define __vdcd__vzughomedevicecontainer__

#include "vdcd_common.hpp"

#if ENABLE_VZUGHOME

#include "deviceclasscontainer.hpp"
#include "device.hpp"

#include "vzughomecomm.hpp"


using namespace std;

namespace p44 {



  class VZugHomeDeviceContainer;
  class VZugHomeDevice;
  typedef boost::intrusive_ptr<VZugHomeDevice> VZugHomeDevicePtr;
  typedef boost::intrusive_ptr<VZugHomeDeviceContainer> VZugHomeDeviceContainerPtr;

  class VZugHomeDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;
    friend class VZugHomeDevice;

    StringList baseURLs;

  public:
  
    VZugHomeDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag);

    void initialize(StatusCB aCompletedCB, bool aFactoryReset);

    /// Switch to manual API URL specification (disables discovery)
    /// @param aVzugApiBaseURLs one or multiple semicolon separated VZug home device API base URLs
    void addVzugApiBaseURLs(const string aVzugApiBaseURLs);


    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings);

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
    virtual string vdcModelSuffix() { return "V-Zug Home"; }

  private:

    void discoveryStatusHandler(VZugHomeDiscoveryPtr aDiscovery, StatusCB aCompletedCB, ErrorPtr aError);
    void addNextDevice(StringList::iterator aNext, StatusCB aCompletedCB);
    void gotDeviceInfos(VZugHomeDevicePtr aNewDev, StringList::iterator aNext, StatusCB aCompletedCB);

  };

} // namespace p44

#endif // ENABLE_VZUGHOME

#endif /* defined(__vdcd__vzughomedevicecontainer__) */
