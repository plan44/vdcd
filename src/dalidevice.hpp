//
//  dalidevice.hpp
//  vdcd
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__dalidevice__
#define __vdcd__dalidevice__

#include "device.hpp"

#include "dalicomm.hpp"
#include "lightbehaviour.hpp"

using namespace std;

namespace p44 {

  class DaliDeviceContainer;
  class DaliDevice;
  typedef boost::intrusive_ptr<DaliDevice> DaliDevicePtr;
  class DaliDevice : public Device
  {
    typedef Device inherited;

    /// the device info
    DaliDeviceInfo deviceInfo;

    /// cached brightness
    Brightness cachedBrightness;

    /// currently set transition time
    MLMicroSeconds transitionTime;

    /// currently set DALI fade rate
    uint8_t fadeTime;

  public:
    DaliDevice(DaliDeviceContainer *aClassContainerP);

    /// get typed container reference
    DaliDeviceContainer &daliDeviceContainer();

    void setDeviceInfo(DaliDeviceInfo aDeviceInfo);

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


    /// disconnect device. For DALI, we'll check if the device is still present on the bus, and only if not
    /// we allow disconnection
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDisconnectResultHandler will be called to report true if device could be disconnected,
    ///   false in case it is certain that the device is still connected to this and only this vDC
    virtual void disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler);

    /// get currently set output value from device
    /// @param aChannel the output channel. Traditional dS devices have one single output only, but future devices might have many
    virtual int16_t getOutputValue(OutputBehaviour &aOutputBehaviour);

    /// set new output value on device
    /// @param aChannel the output channel. Traditional dS devices have one single output only, but future devices might have many
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness (if possible in hardware)
    virtual void setOutputValue(OutputBehaviour &aOutputBehaviour, int16_t aValue, MLMicroSeconds aTransitionTime=0);

    /// get current output error/status
    /// @param aOutputBehaviour the output behaviour to get the hardware error status for
    virtual DsOutputError getOutputError(OutputBehaviour &aOutputBehaviour);

    /// @}

    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "plan44 DALI device"; }

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    virtual string hardwareGUID();

    /// @return OEM GUID in URN format to identify hardware as uniquely as possible
    virtual string oemGUID();
    
    /// @}


  protected:

    /// derive the dsid from collected device info
    void deriveDSID();

    /// convert dS brightness value to DALI arc power
    /// @param aBrightness 0..255
    /// @return arcpower 0..254
    uint8_t brightnessToArcpower(Brightness aBrightness);

    /// convert DALI arc power to dS brightness value
    /// @param aArcpower 0..254
    /// @return brightness 0..255
    Brightness arcpowerToBrightness(int aArcpower);

    /// set transition time for subsequent brightness changes
    /// @param aTransitionTime time for transition
    void setTransitionTime(MLMicroSeconds aTransitionTime);


  private:

    void checkPresenceResponse(PresenceCB aPresenceResultHandler, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);

    void queryActualLevelResponse(CompletedCB aCompletedCB, bool aFactoryReset, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);
    void queryMinLevelResponse(CompletedCB aCompletedCB, bool aFactoryReset, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);

    void disconnectableHandler(bool aForgetParams, DisconnectCB aDisconnectResultHandler, bool aPresent);

  };

} // namespace p44

#endif /* defined(__vdcd__dalidevice__) */
