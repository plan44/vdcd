//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__lightbehaviour__
#define __vdcd__lightbehaviour__

#include "device.hpp"
#include "simplescene.hpp"
#include "outputbehaviour.hpp"

using namespace std;

namespace p44 {

  typedef uint8_t DimmingTime; ///< dimming time with bits 0..3 = mantissa in 6.666mS, bits 4..7 = exponent (# of bits to shift left)
  typedef double Brightness;

  class BrightnessChannel : public ChannelBehaviour
  {
    typedef ChannelBehaviour inherited;
    double minDim;

  public:
    BrightnessChannel(OutputBehaviour &aOutput) : inherited(aOutput)
    {
      resolution = 1.0/256*100; // light defaults to historic dS 1/256 of full scale resolution
      minDim = getMin()+1; // min dimming level defaults to one unit above zero
    };

    void setDimMin(double aMinDim) { minDim = aMinDim; };

    virtual DsChannelType getChannelType() { return channeltype_brightness; }; ///< the dS channel type
    virtual const char *getName() { return "brightness"; };
    virtual double getMin() { return 0; }; // dS brightness goes from 0 to 100%
    virtual double getMax() { return 100; };
    virtual double getDimPerMS() { return 11.0/256*100/300; }; // dimming is 11 steps(1/256) per 300mS (as per ds-light.pdf specification) = 255/11*300 = 7 seconds full scale
    virtual double getMinDim() { return minDim; };

  };
  typedef boost::intrusive_ptr<BrightnessChannel> BrightnessChannelPtr;



  /// A concrete class implementing the Scene object for a simple (single channel = brightness) light device
  /// @note subclasses can implement more parameters, like for exampe ColorLightScene for color lights.
  class LightScene : public SimpleScene
  {
    typedef SimpleScene inherited;

  public:
    LightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) : inherited(aSceneDeviceSettings, aSceneNo) {}; ///< constructor, sets values according to dS specs' default values

  };
  typedef boost::intrusive_ptr<LightScene> LightScenePtr;



  /// the persistent parameters of a light scene device (including scene table)
  /// @note subclasses can implement more parameters, like for example ColorLightDeviceSettings for color lights.
  class LightDeviceSettings : public SceneDeviceSettings
  {
    typedef SceneDeviceSettings inherited;

  public:
    LightDeviceSettings(Device &aDevice);

    /// factory method to create the correct subclass type of DsScene
    /// @param aSceneNo the scene number to create a scene object for.
    /// @note setDefaultSceneValues() must be called to set default scene values
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);

  };


  /// Implements the behaviour of a digitalSTROM Light device, such as maintaining the logical brightness,
  /// dimming and alert (blinking) functions.
  class LightBehaviour : public OutputBehaviour
  {
    typedef OutputBehaviour inherited;


    /// @name hardware derived parameters (constant during operation)
    /// @{
    /// @}


    /// @name persistent settings
    /// @{
    Brightness onThreshold; ///< if !isDimmable, output will be on when output value is >= the threshold
    DimmingTime dimTimeUp[3]; ///< dimming up time
    DimmingTime dimTimeDown[3]; ///< dimming down time
    double dimCurveExp; ///< exponent for logarithmic curve (1=linear, 2=quadratic, 3=cubic, ...)
    /// @}


    /// @name internal volatile state
    /// @{
    long blinkTicket; ///< when blinking
    SimpleCB blinkDoneHandler; ///< called when blinking done
    LightScenePtr blinkRestoreScene; ///< scene to restore
    long fadeDownTicket; ///< for slow fading operations
    bool hardwareHasSetMinDim; ///< if set, hardware has set minDim (prevents loading from DB)
    /// @}


  public:
    LightBehaviour(Device &aDevice);

    /// device type identifier
    /// @return constant identifier for this type of behaviour
    virtual const char *behaviourTypeIdentifier() { return "light"; };

    /// the brightness channel
    BrightnessChannelPtr brightness;

    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// @return true if device is dimmable
    bool isDimmable() { return outputFunction!=outputFunction_switch && actualOutputMode()!=outputmode_binary; };

    /// initialize behaviour with actual device's brightness parameters
    /// @param aMin minimal brightness that can be set
    /// @note brightness: 0..100%, linear brightness as perceived by humans (half value = half brightness)
    void initMinBrightness(Brightness aMin);

    /// Apply output-mode specific output value transformation
    /// @param aChannelValue channel value
    /// @param aChannelIndex channel index (might be different transformation depending on type)
    /// @return output value limited/transformed according to outputMode
    /// @note subclasses might implement behaviour-specific output transformations
    virtual double outputValueAccordingToMode(double aChannelValue, size_t aChannelIndex);

    /// return the brightness to be applied to hardware
    /// @return brightness
    /// @note this is to allow lights to have switching behaviour - when brightness channel value is
    ///   above onThreshold, brightnessForHardware() will return the max channel value and 0 otherwise.
    Brightness brightnessForHardware();

    /// sync channel brightness from actual hardware value
    /// @param aBrightness current brightness value read back from hardware
    /// @note this wraps the dimmable/switch functionality (does not change channel value when onThreshold
    ///   condition is already met to allow saving virtual brightness to scenes)
    void syncBrightnessFromHardware(Brightness aBrightness, bool aAlwaysSync=false);

    /// wrapper to confirm having applied brightness
    bool brightnessNeedsApplying() { return brightness->needsApplying(); };

    /// step through transitions
    /// @param aStepSize how much to step. Default is zero and means starting transition
    /// @return true if there's another step to take, false if end of transition already reached
    bool brightnessTransitionStep(double aStepSize = 0) { return brightness->transitionStep(aStepSize); };

    /// wrapper to confirm having applied brightness
    void brightnessApplied() { brightness->channelValueApplied(); };

    /// wrapper to get brightness' transition time
    MLMicroSeconds transitionTimeToNewBrightness() { return brightness->transitionTimeToNewValue(); };

    /// @}


    /// @name interaction with digitalSTROM system
    /// @{

    /// check for presence of model feature (flag in dSS visibility matrix)
    /// @param aFeatureIndex the feature to check for
    /// @return yes if this output behaviour has the feature, no if (explicitly) not, undefined if asked entity does not know
    virtual Tristate hasModelFeature(DsModelFeatures aFeatureIndex);

    /// apply scene to output channels
    /// @param aScene the scene to apply to output channels
    /// @return true if apply is complete, i.e. everything ready to apply to hardware outputs.
    ///   false if scene cannot yet be applied to hardware, and will be performed later
    /// @note this derived class' applyScene only implements special hard-wired behaviour specific scenes
    ///   basic scene apply functionality is provided by base class' implementation already.
    virtual bool applyScene(DsScenePtr aScene);

    /// perform special scene actions (like flashing) which are independent of dontCare flag.
    /// @param aScene the scene that was called (if not dontCare, applyScene() has already been called)
    /// @param aDoneCB will be called when scene actions have completed (but not necessarily when stopped by stopSceneActions())
    virtual void performSceneActions(DsScenePtr aScene, SimpleCB aDoneCB);

    /// will be called to stop all ongoing actions before next callScene etc. is issued.
    /// @note this must stop all ongoing actions such that applying another scene or action right afterwards
    ///   cannot mess up things.
    virtual void stopSceneActions();

    /// switch on at minimum brightness if not already on (needed for callSceneMin), only relevant for lights
    /// @param aScene the scene to take all other channel values from, except brightness which is set to light's minDim
    virtual void onAtMinBrightness(DsScenePtr aScene);

    /// check if this channel of this device is allowed to dim now (for lights, this will prevent dimming lights that are off)
    /// @param aChannelType the channel to check
    virtual bool canDim(DsChannelType aChannelType);

    /// identify the device to the user in a behaviour-specific way
    /// @note implemented as blinking for LightBehaviour
    virtual void identifyToUser();

    /// @}


    /// @name services for implementing functionality
    /// @{

    /// blink the light (for identifying it, or alerting special system states)
    /// @param aDuration how long the light should blink
    /// @param aParamScene if not NULL, this scene might provide parameters for blinking
    /// @param aDoneCB will be called when scene actions have completed
    /// @param aBlinkPeriod how fast the blinking should be
    /// @param aOnRatioPercent how many percents of aBlinkPeriod the indicator should be on
    void blink(MLMicroSeconds aDuration, LightScenePtr aParamScene, SimpleCB aDoneCB, MLMicroSeconds aBlinkPeriod = 600*MilliSecond, int aOnRatioPercent = 50);

    /// stop blinking immediately
    virtual void stopBlink();

    /// get transition time in microseconds from given scene effect
    /// @param aEffect the scene effect
    /// @param aDimUp true when dimming up, false when dimming down
    MLMicroSeconds transitionTimeFromSceneEffect(DsSceneEffect aEffect, bool aDimUp);


    /// get PWM value for brightness (from brightness channel) according to dim curve
    /// @param aPWM will receive the PWM value corresponding to current brightness from 0..aMax
    /// @param aMax max PWM duty cycle value
    double brightnessToPWM(Brightness aBrightness, double aMaxPWM);

    /// set PWM value from lamp (to update brightness channel) according to dim curve
    /// @param aPWM PWM value to be converted back to brightness
    /// @param aMax max PWM duty cycle value
    Brightness PWMToBrightness(double aPWM, double aMaxPWM);


    /// @}


    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  protected:

    /// called by applyScene to load channel values from a scene.
    /// @param aScene the scene to load channel values from
    /// @note Scenes don't have 1:1 representation of all channel values for footprint and logic reasons, so this method
    ///   is implemented in the specific behaviours according to the scene layout for that behaviour.
    virtual void loadChannelsFromScene(DsScenePtr aScene);

    /// called by captureScene to save channel values to a scene.
    /// @param aScene the scene to save channel values to
    /// @note Scenes don't have 1:1 representation of all channel values for footprint and logic reasons, so this method
    ///   is implemented in the specific behaviours according to the scene layout for that behaviour.
    /// @note call markDirty on aScene in case it is changed (otherwise captured values will not be saved)
    virtual void saveChannelsToScene(DsScenePtr aScene);


    // property access implementation for descriptor/settings/states
    //virtual int numDescProps();
    //virtual const PropertyDescriptorPtr getDescDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    virtual int numSettingsProps();
    virtual const PropertyDescriptorPtr getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    //virtual int numStateProps();
    //virtual const PropertyDescriptorPtr getStateDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    // combined field access for all types of properties
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

  private:

    void beforeBlinkStateSavedHandler(MLMicroSeconds aDuration, LightScenePtr aParamScene, MLMicroSeconds aBlinkPeriod, int aOnRatioPercent);
    void blinkHandler(MLMicroSeconds aEndTime, bool aState, MLMicroSeconds aOnTime, MLMicroSeconds aOffTime);
    void fadeDownHandler(MLMicroSeconds aFadeStepTime);

  };

  typedef boost::intrusive_ptr<LightBehaviour> LightBehaviourPtr;

} // namespace p44

#endif /* defined(__vdcd__lightbehaviour__) */
