//
//  lightbehaviour.hpp
//  vdcd
//
//  Created by Lukas Zeller on 19.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
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
    virtual bool accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);

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

    /// factory method to create the correct subclass type of DsScene with default values
    /// @param aSceneNo the scene number to create a scene object with proper default values for.
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
    bool localPriority; ///< if set, device is in local priority, i.e. ignores scene calls
    bool isLocigallyOn; ///< if set, device is logically ON (but may be below threshold to enable the output)
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


    /// @return true if device is logically on
    bool getLogicallyOn() { return isLocigallyOn; };

    /// set new brightness
    /// @param aBrightness 0..255, linear brightness as perceived by humans (half value = half brightness)
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness
    void setLogicalBrightness(Brightness aBrightness, MLMicroSeconds aTransitionTime=0);

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

    /// capture current state into passed scene object
    /// @param aScene the scene object to update
    /// @note call markDirty on aScene in case it is changed (otherwise captured values will not be saved)
    virtual void captureScene(DsScenePtr aScene);
    
    /// @}

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
    virtual bool accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);


  private:

    void nextBlink();
  };

  typedef boost::intrusive_ptr<LightBehaviour> LightBehaviourPtr;

} // namespace p44

#endif /* defined(__vdcd__lightbehaviour__) */
