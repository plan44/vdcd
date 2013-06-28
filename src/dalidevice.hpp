//
//  dalidevice.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__dalidevice__
#define __p44bridged__dalidevice__

#include "device.hpp"

#include "dalicomm.hpp"

using namespace std;

namespace p44 {

  class DaliDeviceContainer;
  class DaliDevice;
  typedef boost::shared_ptr<DaliDevice> DaliDevicePtr;
  class DaliDevice : public Device
  {
    typedef Device inherited;

    /// the device info
    DaliDeviceInfo deviceInfo;

    /// cached brightness
    Brightness cachedBrightness;

    /// currently set transition time
    MLMicroSeconds transitionTime;

  public:
    DaliDevice(DaliDeviceContainer *aClassContainerP);

    DaliDeviceContainer *daliDeviceContainerP();

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

    /// "pings" the device. Device should respond by sending back a "pong" shortly after (using pong())
    /// base class just sends the pong, but derived classes which can actually ping their hardware should
    /// do so and send the pong only if the hardware actually responds.
    virtual void ping();

    /// get currently set output value from device
    /// @param aChannel the output channel. Traditional dS devices have one single output only, but future devices might have many
    virtual int16_t getOutputValue(int aChannel);

    /// set new output value on device
    /// @param aChannel the output channel. Traditional dS devices have one single output only, but future devices might have many
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness (if possible in hardware)
    virtual void setOutputValue(int aChannel, int16_t aValue, MLMicroSeconds aTransitionTime=0);
    
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

    void pingResponse(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);

    void queryActualLevelResponse(CompletedCB aCompletedCB, bool aFactoryReset, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);

    void queryMinLevelResponse(CompletedCB aCompletedCB, bool aFactoryReset, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);


  };

} // namespace p44

#endif /* defined(__p44bridged__dalidevice__) */
