//
//  Copyright (c) 2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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


#ifndef __vdcd__sparkiodevice__
#define __vdcd__sparkiodevice__

#include "device.hpp"

#include "jsonwebclient.hpp"


using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class SparkIoDevice;
  typedef boost::intrusive_ptr<SparkIoDevice> SparkIoDevicePtr;
  class SparkIoDevice : public Device
  {
    typedef Device inherited;
    int32_t outputValue;
    string sparkCoreID;
    string sparkCoreToken;
    JsonWebClient sparkCloudComm;
    int apiVersion;
    bool outputChangePending;

  public:
    SparkIoDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// initializes the physical device for being used
    /// @param aFactoryReset if set, the device will be inititalized as thoroughly as possible (factory reset, default settings etc.)
    /// @note this is called before interaction with dS system starts (usually just after collecting devices)
    /// @note implementation should call inherited when complete, so superclasses could chain further activity
    virtual void initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset);

    /// set new output value on device
    /// @param aOutputBehaviour the output behaviour which has a new output value to be sent to the hardware output
    /// @note depending on how the actual device communication works, the implementation might need to consult all
    ///   output behaviours to collect data for an outgoing message.
    virtual void updateOutputValue(OutputBehaviour &aOutputBehaviour);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "spark core based device"; }

    /// @}

  protected:

    void deriveDsUid();

  private:

    void apiVersionReceived(CompletedCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aJsonResponse, ErrorPtr aError);
    void postOutputValue(OutputBehaviour &aOutputBehaviour);
    void outputChanged(OutputBehaviour &aOutputBehaviour, JsonObjectPtr aJsonResponse, ErrorPtr aError);

  };
  
} // namespace p44

#endif /* defined(__vdcd__sparkiodevice__) */
