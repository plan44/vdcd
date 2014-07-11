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

#ifndef __vdcd__dalidevicecontainer__
#define __vdcd__dalidevicecontainer__

#include "vdcd_common.hpp"

#include "deviceclasscontainer.hpp"

#include "dalicomm.hpp"

using namespace std;

namespace p44 {

  class DaliDeviceContainer;
  typedef boost::intrusive_ptr<DaliDeviceContainer> DaliDeviceContainerPtr;
  class DaliDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;
  public:
    DaliDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag);

    // the DALI communication object
    DaliCommPtr daliComm;

    virtual const char *deviceClassIdentifier() const;

    /// perform self test
    /// @param aCompletedCB will be called when self test is done, returning ok or error
    virtual void selfTest(CompletedCB aCompletedCB);

    /// collect and add devices to the container
    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive);

    /// @return human readable model name/short description
    virtual string modelName() { return "DALI vDC"; }

  private:

    void testScanDone(CompletedCB aCompletedCB, DaliComm::ShortAddressListPtr aShortAddressListPtr, ErrorPtr aError);
    void testRW(CompletedCB aCompletedCB, DaliAddress aShortAddr, uint8_t aTestByte);
    void testRWResponse(CompletedCB aCompletedCB, DaliAddress aShortAddr, uint8_t aTestByte, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);

  };

} // namespace p44


#endif /* defined(__vdcd__dalidevicecontainer__) */
