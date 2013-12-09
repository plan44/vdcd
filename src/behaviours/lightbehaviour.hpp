//
//  Copyright (c) 2013 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
#include "dsscene.hpp"
#include "outputbehaviour.hpp"

using namespace std;

namespace p44 {

  typedef uint8_t Brightness;
  typedef uint8_t DimmingTime; ///< dimming time with bits 0..3 = mantissa in 6.666mS, bits 4..7 = exponent (# of bits to shift left)


  class LightScene : public DsScene
  {
    typedef DsScene inherited;
  public:
    LightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo); ///< constructor, sets values according to dS specs' default values

    /// @name light scene specific values
    /// @{

    Brightness sceneBrightness; ///< saved brightness value for this scene
    bool specialBehaviour; ///< special behaviour active
    bool flashing; ///< flashing active for this scene
    uint8_t dimTimeSelector; ///< 0: use current DIM time, 1-3 use DIMTIME0..2

    /// @}

    /// Set default scene values for a specified scene number
    /// @param aSceneNo the scene number to set default values
    virtual void setDefaultSceneValues(SceneNo aSceneNo);

  protected:


    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);

    // property access implementation
    virtual int numProps(int aDomain);
    virtual const PropertyDescriptor *getPropertyDescriptor(int aPropIndex, int aDomain);
    virtual bool accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);

  };
  typedef boost::intrusive_ptr<LightScene> LightScenePtr;
  typedef map<SceneNo, LightScenePtr> LightSceneMap;




  /// the persistent parameters of a light scene device (including scene table)
  class LightDeviceSettings : public SceneDeviceSettings
  {
    typedef SceneDeviceSettings inherited;

  public:
    LightDeviceSettings(Device &aDevice);

  protected:

    /// factory method to create the correct subclass type of DsScene
    /// @param aSceneNo the scene number to create a scene object for.
    /// @note setDefaultSceneValues() must be called to set default scene values
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);

  };



  class LightBehaviour : public OutputBehaviour
  {
    typedef OutputBehaviour inherited;


    /// @name hardware derived parameters (constant during operation)
    /// @{
    /// @}


    /// @name persistent settings
    /// @{
    Brightness onThreshold; ///< if !isDimmable, output will be on when output value is >= the threshold
    Brightness minBrightness; ///< minimal brightness, dimming down will not go below this
    Brightness maxBrightness; ///< maximal brightness, dimming down will not go below this
    DimmingTime dimTimeUp[3]; ///< dimming up time
    DimmingTime dimTimeDown[3]; ///< dimming down time
    /// @}


    /// @name internal volatile state
    /// @{
    int blinkCounter; ///< for generation of blink sequence
    long fadeDownTicket; ///< for slow fading operations
    Brightness logicalBrightness; ///< current internal brightness value. For non-dimmables, output is on only if outputValue>onThreshold
    /// @}

  public:
    LightBehaviour(Device &aDevice);

    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// Get the current logical brightness
    /// @return 0..255, linear brightness as perceived by humans (half value = half brightness)
    Brightness getLogicalBrightness();

    /// @return true if device is dimmable
    bool isDimmable() { return outputFunction==outputFunction_dimmer && outputMode==outputmode_gradual; };

    /// set new brightness
    /// @param aBrightness 0..255, linear brightness as perceived by humans (half value = half brightness)
    /// @param aTransitionTimeUp time in microseconds to be spent on transition from current to higher new logical brightness
    /// @param aTransitionTimeDown time in microseconds to be spent on transition from current to lower new logical brightness
    ///   if not specified or <0, aTransitionTimeUp is used for both directions
    void setLogicalBrightness(Brightness aBrightness, MLMicroSeconds aTransitionTimeUp, MLMicroSeconds aTransitionTimeDown=-1);

    /// update logical brightness from actual output state
    void updateLogicalBrightnessFromOutput();

    /// initialize behaviour with actual device's brightness parameters
    /// @param aMin minimal brightness that can be set
    /// @param aMax maximal brightness that can be set
    /// @note brightness: 0..255, linear brightness as perceived by humans (half value = half brightness)
    void initBrightnessParams(Brightness aMin, Brightness aMax);

    /// @}


    /// @name interaction with digitalSTROM system
    /// @{

    /// apply scene to output
    /// @param aScene the scene to apply to the output
    virtual void applyScene(DsScenePtr aScene);

    /// perform special scene actions (like flashing) which are independent of dontCare flag.
    /// @param aScene the scene that was called (if not dontCare, applyScene() has already been called)
    virtual void performSceneActions(DsScenePtr aScene);

    /// capture current state into passed scene object
    /// @param aScene the scene object to update
    /// @param aDoneCB will be called when capture is complete
    /// @note call markDirty on aScene in case it is changed (otherwise captured values will not be saved)
    virtual void captureScene(DsScenePtr aScene, DoneCB aDoneCB);

    /// blink the light (for identifying it)
    /// @param aDuration how long the light should blink
    /// @param aBlinkPeriod how fast the blinking should be
    /// @param aOnRatioPercent how many percents of aBlinkPeriod the indicator should be on
    void blink(MLMicroSeconds aDuration, MLMicroSeconds aBlinkPeriod = 600*MilliSecond, int aOnRatioPercent = 50);

    /// switch on at minimum brightness if not already on (needed for callSceneMin), only relevant for lights
    virtual void onAtMinBrightness();

    /// @}

    /// @param aDimTime : dimming time specification in dS format (Bit 7..4 = exponent, Bit 3..0 = 1/150 seconds, i.e. 0x0F = 100mS)
    static MLMicroSeconds transitionTimeFromDimTime(uint8_t aDimTime);

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  protected:

    /// called by applyScene to actually recall a scene from the scene table
    /// This allows lights with more parameters than just brightness (e.g. color lights) to recall
    /// additional values that were saved as captureScene()
    virtual void recallScene(LightScenePtr aLightScene);


    // property access implementation for descriptor/settings/states
    //virtual int numDescProps();
    //virtual const PropertyDescriptor *getDescDescriptor(int aPropIndex);
    virtual int numSettingsProps();
    virtual const PropertyDescriptor *getSettingsDescriptor(int aPropIndex);
    //virtual int numStateProps();
    //virtual const PropertyDescriptor *getStateDescriptor(int aPropIndex);
    // combined field access for all types of properties
    virtual bool accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);

  private:
  
    void blinkHandler(MLMicroSeconds aEndTime, bool aState, MLMicroSeconds aOnTime, MLMicroSeconds aOffTime, Brightness aOrigBrightness);
    void fadeDownHandler(MLMicroSeconds aFadeStepTime, Brightness aBrightness);

  };

  typedef boost::intrusive_ptr<LightBehaviour> LightBehaviourPtr;

} // namespace p44

#endif /* defined(__vdcd__lightbehaviour__) */
