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

#ifndef __vdcd__digitaliodevice__
#define __vdcd__digitaliodevice__

#include "device.hpp"

#include "digitalio.hpp"

using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class DigitalIODevice;
  typedef boost::intrusive_ptr<DigitalIODevice> DigitalIODevicePtr;
  class DigitalIODevice : public Device
  {
    typedef Device inherited;
		ButtonInputPtr buttonInput;
    IndicatorOutputPtr indicatorOutput;

  public:
    DigitalIODevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);
    
    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// set new channel value on device
    /// @param aChannelBehaviour the channel behaviour which has a new output value to be sent to the hardware output
    /// @note depending on how the actual device communication works, the implementation might need to consult all
    ///   channel behaviours to collect data for an outgoing message.
    virtual void updateChannelValue(ChannelBehaviour &aChannelBehaviour);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName();

    /// @}

  protected:

    void deriveDsUid();
		
	private:

    void buttonHandler(bool aNewState, MLMicroSeconds aTimestamp);
		
  };

} // namespace p44

#endif /* defined(__vdcd__digitaliodevice__) */
