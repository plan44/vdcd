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

#ifndef __vdcd__demodevicecontainer__
#define __vdcd__demodevicecontainer__

#include "vdcd_common.hpp"

#include "deviceclasscontainer.hpp"

using namespace std;

namespace p44 {

  class DemoDeviceContainer;
  typedef boost::intrusive_ptr<DemoDeviceContainer> DemoDeviceContainerPtr;
  class DemoDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;

  public:
    DemoDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag);

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive);

    /// @return human readable model name/short description
    virtual string modelName() { return "Experimental Demo vDC"; }

  };

} // namespace p44


#endif /* defined(__vdcd__demodevicecontainer__) */
