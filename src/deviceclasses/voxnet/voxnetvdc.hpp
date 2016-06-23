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

#ifndef __vdcd__voxnetvdc__
#define __vdcd__voxnetvdc__

#include "vdcd_common.hpp"

#if ENABLE_VOXNET

#include "vdc.hpp"
#include "device.hpp"

#include "voxnetcomm.hpp"


using namespace std;

namespace p44 {

  class VoxnetVdc;
  class VoxnetDevice;
  typedef boost::intrusive_ptr<VoxnetDevice> VoxnetDevicePtr;
  typedef boost::intrusive_ptr<VoxnetVdc> VoxnetVdcPtr;

  class VoxnetVdc : public Vdc
  {
    typedef Vdc inherited;
    friend class VoxnetDevice;

    typedef map<string, VoxnetDevicePtr> VoxnetDeviceMap;

    VoxnetDeviceMap voxnetDevices;

  public:

    VoxnetCommPtr voxnetComm;

    VoxnetVdc(int aInstanceNumber, VdcHost *aVdcHostP, int aTag);

    void initialize(StatusCB aCompletedCB, bool aFactoryReset);

    virtual const char *vdcClassIdentifier() const;

    virtual void collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings);

    /// some containers (statically defined devices for example) should be invisible for the dS system when they have no
    /// devices.
    /// @return if true, this vDC should not be announced towards the dS system when it has no devices
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
    virtual string vdcModelSuffix() { return "Voxnet"; }

  private:

    VoxnetDevicePtr addVoxnetDevice(const string aID, const string aName);
    bool voxnetStatusHandler(const string aVoxnetID, const string aVoxnetStatus);


  };

} // namespace p44

#endif // ENABLE_VOXNET
#endif // __vdcd__voxnetvdc__
