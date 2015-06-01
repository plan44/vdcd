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

#ifndef __vdcd__channelbehaviour__
#define __vdcd__channelbehaviour__

#include "device.hpp"
#include "dsbehaviour.hpp"

using namespace std;

namespace p44 {


  class OutputBehaviour;


  /// how long dimming through the full scale of a channel should take by default
  /// @note this is derived from dS-light spec: 11 brightness (1/256) steps per 300mS -> ~7 seconds for full  range
  #define FULL_SCALE_DIM_TIME_MS 7000

  /// represents a single channel of the output
  /// @note this class is not meant to be derived. Device specific channel functionality should
  ///   be implemented in derived Device classes' methods which are passed channels to process.
  ///   The ChannelBehaviour objects only represent the dS interface to channels, not the
  ///   device specific interface from dS channels to actual device hardware.
  class ChannelBehaviour : public PropertyContainer
  {
    typedef PropertyContainer inherited;
    friend class OutputBehaviour;

  protected:

    OutputBehaviour &output;

    /// @name hardware derived parameters (constant during operation)
    /// @{
    uint8_t channelIndex; ///< the index of the channel within the device
    double resolution; ///< actual resolution within the device
    /// @}

    /// @name persistent settings
    /// @{

    /// @}

    /// @name internal volatile state
    /// @{
    bool channelUpdatePending; ///< set if cachedOutputValue represents a value to be transmitted to the hardware
    double cachedChannelValue; ///< the cached channel value
    double previousChannelValue; ///< the previous channel value, can be used for performing transitions
    double transitionProgress; ///< how much the transition has progressed so far (0..1)
    MLMicroSeconds channelLastSync; ///< Never if the cachedChannelValue is not yet applied to the hardware or retrieved from hardware, otherwise when it was last synchronized
    MLMicroSeconds nextTransitionTime; ///< the transition time to use for the next channel value change
    /// @}

  public:

    ChannelBehaviour(OutputBehaviour &aOutput);


    /// @name Fixed channel properties, partly from dS specs
    /// @{

    virtual DsChannelType getChannelType() = 0; ///< the dS channel type
    virtual const char *getName() = 0; ///< descriptive channel name
    virtual double getMin() = 0; ///< min value
    virtual double getMax() = 0; ///< max value
    virtual double getDimPerMS() { return (getMax()-getMin())/7000; }; ///< value to step up or down per Millisecond when dimming (default = 7sec for full scale)
    virtual double getMinDim() { return getMin(); }; ///< dimming min value defaults to same value as min
    virtual bool wrapsAround() { return false; }; ///< if true, dimming is wrap around i.e. dimming below getMin()->getMax() and vice versa. Off by default

    /// @}


    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// set the fixed channel configuration: identification (defined by the device's hardware)
    /// @param aResolution actual resolution (smallest step) of the connected hardware
    void setResolution(double aResolution);

    /// set actual current output value as read from the device on startup, or before saving scenes
    /// to sync local cache value
    /// @param aActualChannelValue the value as read from the device
    /// @param aAlwaysSync if set, value is synchronized even if current value is still pending to be applied
    /// @note only used to get the actual value FROM the hardware.
    ///   NOT to be used to change the hardware output value!
    void syncChannelValue(double aActualChannelValue, bool aAlwaysSync=false);

    /// set new channel value and transition time to be applied with next device-level applyChannelValues()
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new channel value
    /// @param aAlwaysApply if set, new value will be applied to hardware even if not different from currently known value
    void setChannelValue(double aNewValue, MLMicroSeconds aTransitionTime=0, bool aAlwaysApply=false);

    /// set new channel value and separate transition times for increasing/decreasing value at applyChannelValues()
    /// @param aValue the new output value
    /// @param aTransitionTimeUp time in microseconds to be spent on transition from current to higher channel value
    /// @param aTransitionTimeDown time in microseconds to be spent on transition from current to lower channel value
    /// @param aAlwaysApply if set, new value will be applied to hardware even if not different from currently known value
    void setChannelValue(double aNewValue, MLMicroSeconds aTransitionTimeUp, MLMicroSeconds aTransitionTimeDown, bool aAlwaysApply);

    /// convenience variant of setChannelValue, which also checks the associated dontCare flag from the scene passed
    /// and only assigns the new value if the dontCare flags is NOT set.
    /// @param aAlwaysApply if set, new value will be applied to hardware even if not different from currently known value
    void setChannelValueIfNotDontCare(DsScenePtr aScene, double aNewValue, MLMicroSeconds aTransitionTimeUp, MLMicroSeconds aTransitionTimeDown, bool aAlwaysApply);

    /// dim channel value up or down, preventing going below getMinDim().
    /// @param aIncrement how much to increment/decrement the value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new channel value
    /// @return new channel value after increment/decrement
    double dimChannelValue(double aIncrement, MLMicroSeconds aTransitionTime);

    /// get current value of channel. This is always the target value, even if channel is still in transition
    /// @note does not trigger a device read, but returns chached value
    //   (initialized from actual value only at startup via initChannelValue(), updated when using setChannelValue)
    double getChannelValue();

    /// get current value of channel, which might be a calculated intermediate value between a previous value and getChannelValue()
    /// @note does not trigger a device read, but returns chached value
    //   (initialized from actual value only at startup via initChannelValue(), updated when using setChannelValue)
    double getTransitionalValue();

    /// step through transitions
    /// @param aStepSize how much to step. Default is zero and means starting transition
    /// @return true if there's another step to take, false if end of transition already reached
    bool transitionStep(double aStepSize=0);

    /// set transition progress
    /// @param aProgress progress between 0 (just started) to 1 (completed).
    void setTransitionProgress(double aProgress);

    /// set transition progress from intermediate value (instead of 0..1 progress as with setTransitionProgress())
    /// @param aCurrentValue value actually reached in transition right now, will update internal transition progress accordingly
    /// @param aIsInitial if set, this is considered the start value of the transition
    ///   (rather than as intermediate value between previously established start and target value)
    void setTransitionValue(double aCurrentValue, bool aIsInitial);

    /// check if in transition
    /// @return true if transition not complete and getTransitionalValue() will return a intermediate value
    bool inTransition();

    /// get time of last sync with hardware (applied or synchronized back)
    /// @return time of last sync, p44::Never if value never synchronized
    MLMicroSeconds getLastSync() { return channelLastSync; };

    /// get current value of this channel - and calculate it if it is not set in the device, but must be calculated from other channels
    virtual double getChannelValueCalculated() { return getChannelValue(); /* no calculated channels in base class */ };

    /// the transition time to use to change value in the hardware
    /// @return time to be used to transition to new value
    MLMicroSeconds transitionTimeToNewValue() { return nextTransitionTime; };

    /// check if channel value needs to be sent to device hardware
    /// @return true if the cached channel value was changed and should be applied to hardware via device's applyChannelValues()
    bool needsApplying() { return channelUpdatePending; }

    /// to be called when channel value has been successfully applied to hardware
    /// @param aAnyWay if true, lastSent state will be set even if channel was not in needsApplying() state
    void channelValueApplied(bool aAnyWay = false);

    /// @}


    /// @name interaction with digitalSTROM system
    /// @{

    /// get the channel index
    /// @return the channel index (0..N, 0=primary)
    size_t getChannelIndex() { return channelIndex; };

    /// get the resolution this channel has in the hardware of this particular device
    /// @return resolution of channel value (size of smallest step output can take, LSB)
    double getResolution() { return resolution; }; ///< actual resolution of the hardware

    /// check if this is the primary channel
    /// @return true if this is the primary (default) channel of a device
    bool isPrimary();

    /// call to make update pending
    /// @param aTransitionTime if >=0, sets new transition time (useful when re-applying values)
    void setNeedsApplying(MLMicroSeconds aTransitionTime = -1) { channelUpdatePending = true; if (aTransitionTime>=0) nextTransitionTime = aTransitionTime; }

    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();


  protected:

    // property access implementation
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

  };

  typedef boost::intrusive_ptr<ChannelBehaviour> ChannelBehaviourPtr;

  typedef vector<ChannelBehaviourPtr> ChannelBehaviourVector;



  /// digital switch channel
  class DigitalChannel : public ChannelBehaviour
  {
    typedef ChannelBehaviour inherited;

  public:
    DigitalChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 100; /* on or off */ };
    virtual DsChannelType getChannelType() { return channeltype_default; }; ///< no real dS channel type
    virtual const char *getName() { return "switch"; };
    virtual double getMin() { return 0; }; // compatible with brightness: 0 to 100%
    virtual double getMax() { return 100; };
  };
  typedef boost::intrusive_ptr<DigitalChannel> DigitalChannelPtr;


  /// custom channel with instance variables defining the properties
  class CustomChannel : public ChannelBehaviour
  {
    typedef ChannelBehaviour inherited;

    DsChannelType channelType;
    string name;
    double min;
    double max;
    double resolution;
    double dimPerMS;
    double minDim;

  public:
    CustomChannel(
      OutputBehaviour &aOutput,
      DsChannelType aChannelType,
      const char *aName,
      double aMin,
      double aMax,
      double aResolution,
      double aDimPerMS,
      double aMinDim
    ) :
      inherited(aOutput),
      channelType(aChannelType),
      name(aName),
      min(aMin),
      max(aMax),
      dimPerMS(aDimPerMS),
      minDim(aMinDim)
    {};

    virtual DsChannelType getChannelType() { return channelType; };
    virtual const char *getName() { return name.c_str(); };
    virtual double getMin() { return min; };
    virtual double getMax() { return max; };
    virtual double getDimPerMS() { return dimPerMS; };
    virtual double getMinDim() { return minDim; };
  };



} // namespace p44

#endif /* defined(__vdcd__channelbehaviour__) */
