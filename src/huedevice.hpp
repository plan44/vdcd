//
//  huedevice.hpp
//  vdcd
//
//  Created by Lukas Zeller on 02.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__huedevice__
#define __vdcd__huedevice__

#include "device.hpp"

using namespace std;

namespace p44 {

  class HueDeviceContainer;
  class HueDevice;
  typedef boost::intrusive_ptr<HueDevice> HueDevicePtr;
  class HueDevice : public Device
  {
    typedef Device inherited;

    string lightID; ///< the ID as used in the hue bridge

    // information from the device itself
    string hueModel;

  public:
    HueDevice(HueDeviceContainer *aClassContainerP, const string &aLightID);

    HueDeviceContainer &hueDeviceContainer();

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

    /// check presence of this addressable
    /// @param aPresenceResultHandler will be called to report presence status
    virtual void checkPresence(PresenceCB aPresenceResultHandler);

    /// disconnect device. For hue, we'll check if the device is still reachable via the bridge, and only if not
    /// we allow disconnection
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDisconnectResultHandler will be called to report true if device could be disconnected,
    ///   false in case it is certain that the device is still connected to this and only this vDC
    virtual void disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler);

    /// set new output value on device
    /// @param aOutputBehaviour the output behaviour which has a new output value to be sent to the hardware output
    /// @note depending on how the actual device communication works, the implementation might need to consult all
    ///   output behaviours to collect data for an outgoing message.
    virtual void updateOutputValue(OutputBehaviour &aOutputBehaviour);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return hueModel; };

    /// @}


  protected:

    void deriveDSID();

  private:

    void deviceStateReceived(CompletedCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aDeviceInfo, ErrorPtr aError);
    void presenceStateReceived(PresenceCB aPresenceResultHandler, JsonObjectPtr aDeviceInfo, ErrorPtr aError);
    void disconnectableHandler(bool aForgetParams, DisconnectCB aDisconnectResultHandler, bool aPresent);

  };
  
} // namespace p44


#endif /* defined(__vdcd__huedevice__) */
