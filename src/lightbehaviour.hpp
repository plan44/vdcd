//
//  lightbehaviour.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 19.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__lightbehaviour__
#define __p44bridged__lightbehaviour__

#include "device.hpp"

#include "persistentparams.hpp"

using namespace std;

namespace p44 {

  typedef uint8_t DimmingTime; ///< dimming time with bits 0..3 = mantissa in 6.666mS, bits 4..7 = exponent (# of bits to shift left)

  class LightSettings;

  class LightScene : public PersistentParams
  {
    typedef PersistentParams inherited;
  public:
    LightScene(ParamStore &aParamStore, SceneNo aSceneNo); ///< constructor, sets values according to dS specs' default values

    SceneNo sceneNo; ///< scene number
    Brightness sceneValue; ///< output value for this scene
    bool dontCare; ///< if set, applying this scene does not change the output value
    bool ignoreLocalPriority; ///< if set, local priority is ignored when calling this scene
    bool specialBehaviour; ///< special behaviour active
    bool flashing; ///< flashing active for this scene
    uint8_t dimTimeSelector; ///< 0: use current DIM time, 1-3 use DIMTIME0..2

    /// Legacy: get scene flags encoded as SCECON byte
    uint8_t getSceCon();
    /// Legacy: set scene flags encoded as SCECON byte
    void setSceCon(uint8_t aSceCon);


    /// @name PersistentParams methods which implement actual storage
    /// @{

    /// SQLIte3 table name to store these parameters to
    virtual const char *tableName();
    /// primary key field definitions
    virtual const FieldDefinition *getKeyDefs();
    /// data field definitions
    virtual const FieldDefinition *getFieldDefs();
    /// load values from passed row
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    /// bind values to passed statement
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);

    /// @}
  };
  typedef boost::shared_ptr<LightScene> LightScenePtr;
  typedef map<SceneNo, LightScenePtr> LightSceneMap;



  /// the persistent parameters of a device with light behaviour
  class LightSettings : public PersistentParams
  {
    typedef PersistentParams inherited;
    friend class LightScene;
    LightSceneMap scenes; ///< the user defined light scenes (default scenes will be created on the fly)
  public:
    LightSettings(ParamStore &aParamStore);
    bool isDimmable; ///< if set, device can be dimmed
    Brightness onThreshold; ///< if !isDimmable, output will be on when output value is >= the threshold
    Brightness minDim; ///< minimal dimming value, dimming down will not go below this
    Brightness maxDim; ///< maximum dimming value, dimming up will not go above this
    DimmingTime dimUpTime[3]; ///< dimming up time
    DimmingTime dimDownTime[3]; ///< dimming down time
    Brightness dimUpStep; ///< size of dim up steps
    Brightness dimDownStep; ///< size of dim down steps

    /// get the parameters for the scene
    LightScenePtr getScene(SceneNo aSceneNo);

    /// update scene (mark dirty, add to list of non-default scene objects)
    void updateScene(LightScenePtr aScene);

    /// Get output MODE
    uint8_t getOutputMode();
    /// Set output MODE
    void setOutputMode(uint8_t aOutputMode);

    /// @name PersistentParams methods which implement actual storage
    /// @{

    /// SQLIte3 table name to store these parameters to
    virtual const char *tableName();
    /// data field definitions
    virtual const FieldDefinition *getFieldDefs();
    /// load values from passed row
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    /// bind values to passed statement
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);
    /// load child parameters (if any)
    virtual ErrorPtr loadChildren();
    /// save child parameters (if any)
    virtual ErrorPtr saveChildren();
    /// delete child parameters (if any)
    virtual ErrorPtr deleteChildren();

    /// @}
  };


  class LightBehaviour : public DSBehaviour
  {
    typedef DSBehaviour inherited;

    bool localPriority; ///< if set, device is in local priority, i.e. ignores scene calls
    bool isLocigallyOn; ///< if set, device is logically ON (but may be below threshold to enable the output)
    Brightness logicalBrightness; ///< current internal brightness value. For non-dimmables, output is on only if outputValue>onThreshold
    LightSettings lightSettings; ///< the persistent params of this lighting device

  public:
    LightBehaviour(Device *aDeviceP);

    /// Get the current logical brightness
    /// @return 0..255, linear brightness as perceived by humans (half value = half brightness)
    Brightness getLogicalBrightness();

    /// set new brightness
    /// @param aBrightness 0..255, linear brightness as perceived by humans (half value = half brightness)
    void setLogicalBrightness(Brightness aBrightness);

    /// set min brightness the lamp can provide
    /// @param aBrightness 0..255, linear brightness as perceived by humans (half value = half brightness)
    void setMinimalBrightness(Brightness aBrightness);



    /// @name functional identification for digitalSTROM system
    /// @{

    virtual uint16_t functionId();
    virtual uint16_t productId();
    virtual uint16_t groupMemberShip();
    virtual uint8_t ltMode();
    virtual uint8_t outputMode();
    virtual uint8_t buttonIdGroup();
    virtual uint16_t version();

    /// @}

    /// @name interaction with digitalSTROM system
    /// @{

    /// handle message from vdSM
    /// @param aOperation the operation keyword
    /// @param aParams the parameters object, or NULL if none
    /// @return Error object if message generated an error
    virtual ErrorPtr handleMessage(string &aOperation, JsonObjectPtr aParams);

    /// get behaviour-specific parameter
    /// @param aParamName name of the parameter
    /// @param aArrayIndex index of the parameter if the parameter is an array
    /// @param aValue will receive the current value
    virtual ErrorPtr getBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t &aValue);

    /// set behaviour-specific parameter
    /// @param aParamName name of the parameter
    /// @param aArrayIndex index of the parameter if the parameter is an array
    /// @param aValue the new value to set
    virtual ErrorPtr setBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t aValue);

    /// load behaviour parameters from persistent DB
    /// @note this is usually called from the device container when device is added (detected)
    virtual ErrorPtr load();

    /// save unsaved behaviourparameters to persistent DB
    /// @note this is usually called from the device container in regular intervals
    virtual ErrorPtr save();

    /// forget any behaviour parameters stored in persistent DB
    virtual ErrorPtr forget();
    
    /// @}

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();
  };

}

#endif /* defined(__p44bridged__lightbehaviour__) */
