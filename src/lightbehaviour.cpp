  //
//  lightbehaviour.cpp
//  vdcd
//
//  Created by Lukas Zeller on 19.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "lightbehaviour.hpp"

using namespace p44;



#pragma mark - LightScene


LightScene::LightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inherited(aSceneDeviceSettings, aSceneNo)
{
  sceneBrightness = 0;
  specialBehaviour = false;
  flashing = false;
  dimTimeSelector = 0;
}


#pragma mark - Light Scene persistence

const char *LightScene::tableName()
{
  return "LightScenes";
}

// data field definitions

static const size_t numSceneFields = 3;

size_t LightScene::numFieldDefs()
{
  return inherited::numFieldDefs()+numSceneFields;
}


const FieldDefinition *LightScene::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numSceneFields] = {
    { "brightness", SQLITE_INTEGER },
    { "lightFlags", SQLITE_INTEGER },
    { "dimTimeSelector", SQLITE_INTEGER }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numSceneFields)
    return &dataDefs[aIndex];
  return NULL;
}


enum {
  lightflag_specialBehaviour = 0x0001,
  lightflag_flashing = 0x0002
};


/// load values from passed row
void LightScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  sceneBrightness = aRow->get<int>(aIndex++);
  int lightflags = aRow->get<int>(aIndex++);
  dimTimeSelector = aRow->get<int>(aIndex++);
  // decode the flags
  specialBehaviour = lightflags & lightflag_specialBehaviour;
  flashing = lightflags & lightflag_flashing;
}


/// bind values to passed statement
void LightScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // encode the flags
  int lightflags = 0;
  if (specialBehaviour) lightflags |= lightflag_specialBehaviour;
  if (flashing) lightflags |= lightflag_flashing;
  // bind the fields
  aStatement.bind(aIndex++, sceneBrightness);
  aStatement.bind(aIndex++, lightflags);
  aStatement.bind(aIndex++, dimTimeSelector);
}


#pragma mark - Light scene property access


static char lightscene_key;

enum {
  value_key,
  flashing_key,
  dimTimeSelector_key,
  numLightSceneProperties
};


int LightScene::numProps(int aDomain)
{
  return inherited::numProps(aDomain)+numLightSceneProperties;
}


const PropertyDescriptor *LightScene::getPropertyDescriptor(int aPropIndex, int aDomain)
{
  static const PropertyDescriptor properties[numLightSceneProperties] = {
    { "value", ptype_int8, false, value_key, &lightscene_key },
    { "flashing", ptype_bool, false, flashing_key, &lightscene_key },
    { "dimTimeSelector", ptype_int8, false, dimTimeSelector_key, &lightscene_key },
  };
  int n = inherited::numProps(aDomain);
  if (aPropIndex<n)
    return inherited::getPropertyDescriptor(aPropIndex, aDomain); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return &properties[aPropIndex];
}


bool LightScene::accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&lightscene_key) {
    if (!aForWrite) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        case value_key:
          aPropValue = JsonObject::newInt32(sceneBrightness);
          return true;
        case flashing_key:
          aPropValue = JsonObject::newBool(flashing);
          return true;
        case dimTimeSelector_key:
          aPropValue = JsonObject::newInt32(dimTimeSelector);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor.accessKey) {
        case value_key:
          sceneBrightness = (Brightness)aPropValue->int32Value();
          markDirty();
          return true;
        case flashing_key:
          flashing = aPropValue->boolValue();
          markDirty();
          return true;
        case dimTimeSelector_key:
          dimTimeSelector = aPropValue->int32Value();
          markDirty();
          return true;
      }
    }
  }
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
}




#pragma mark - LightDeviceSettings with default light scenes factory


LightDeviceSettings::LightDeviceSettings(Device &aDevice) :
  inherited(aDevice)
{
};


typedef struct {
  Brightness brightness; ///< output value for this scene
  uint8_t dimTimeSelector; ///< 0: use current DIM time, 1-3 use DIMTIME0..2
  bool flashing; ///< flashing active for this scene
  bool ignoreLocalPriority; ///< if set, local priority is ignored when calling this scene
  bool dontCare; ///< if set, applying this scene does not change the output value
  bool specialBehaviour; ///< special behaviour active
} DefaultSceneParams;

#define NUMDEFAULTSCENES 80 ///< Number of default scenes

// General rules

//  Rule 1 A digitalSTROM Ready Device has to be preconfigured in the right functional group. This is essential to ensure that all electrical devices in one functional group can be orchestrated together.
//  Rule 2 A digitalSTROM Ready Device must be configured for exactly one digitalSTROM functional group. The assigned functional group must be non- ambiguous and is part of the static device configuration (see Function-ID 9.4).
//  Rule 3 The function of a devices output is the basis of its group member- ship. For devices without actuator the target function of the switch button decides about the group membership.
//  Rule 4 digitalSTROM Devices have to implement a default behavior for all 128 scene commands. The system behavior and default values are defined in the particular documents for each functional group.
//  Rule 5 When applications send a scene command to a set of digitalSTROM Devices with more than one target device they have to use scene calls di- rected to a group, splitting into multiple calls to single devices has to be avoided due to latency and statemachine consistency issues.

//  Rule 6 digitalSTROM Ready Devices must ignore stepping commands if their output value is zero.

//  Rule 7 digitalSTROM Device have to complete the identification action on the command Programming Mode Start within 4 seconds.
//  Rule 8 Application processes that do automatic cyclic reads or writes of device parameters are subject to a request limit: at maximum one request per minute and circuit is allowed.
//  Rule 9 Application processes that do automatic cyclic reads of measured values are subject to a request limit: at maximum one request per minute and circuit is allowed.
//  Rule 10 The action command "SetOutputValue" must not be used for other than device configuration purposes.
//  Rule 11 digitalSTROM Ready Devices must not send upstream events continously and must stop sending Low-Level-Event data even if the event is still or repeatedly valid. Transmission of pushbutton events must be abondoned after a maximum time of 2.5 minutes. Automatically genereated events must not exceed a rate limit of 5 events per 5 minutes.
//  Rule 12 Applications shall use the digitalSTROM Server webservice inter- face for communication with the digitalSTROM system. Directly interfacing the dSM-API shall be avoided because it is an internal interface and its API may change in the future.

//  Rule 13 Applications that automatically generate Call Scene action commands (see 5.1.1) must not execute the action commands at a rate faster than one request per second.


// Light rules

//  Rule 3 If a digitalSTROM Device is in local priority state, a scene call is ignored.
//  Rule 4 All devices which are turned on and not in local priority state take part in the dimming process.

static const DefaultSceneParams defaultScenes[NUMDEFAULTSCENES+1] = {
  // group related scenes
  // { brightness, dimTimeSelector, flashing, ignoreLocalPriority, dontCare, specialBehaviour }
  { 0, 1, false, false, false, false }, // 0 : Preset 0 - T0_S0
  { 0, 1, false, true, false, false }, // 1 : Area 1 Off - T1_S0
  { 0, 1, false, true, false, false }, // 2 : Area 2 Off - T2_S0
  { 0, 1, false, true, false, false }, // 3 : Area 3 Off - T3_S0
  { 0, 1, false, true, false, false }, // 4 : Area 4 Off - T4_S0
  { 255, 1, false, false, false, false }, // 5 : Preset 1 - T0_S1
  { 255, 1, false, true, false, false }, // 6 : Area 1 On - T1_S1
  { 255, 1, false, true, false, false }, // 7 : Area 2 On - T1_S1
  { 255, 1, false, true, false, false }, // 8 : Area 3 On - T1_S1
  { 255, 1, false, true, false, false }, // 9 : Area 4 On - T1_S1
  { 0, 1, false, true, false, false }, // 10 : Area Stepping continue - T1234_CONT
  { 0, 1, false, false, false, false }, // 11 : Decrement - DEC_S
  { 0, 1, false, false, false, false }, // 12 : Increment - INC_S
  { 0, 1, false, true, false, false }, // 13 : Minimum - MIN_S
  { 255, 1, false, true, false, false }, // 14 : Maximum - MAX_S
  { 0, 1, false, true, false, false }, // 15 : Stop - STOP_S
  { 0, 1, false, false, true, false }, // 16 : Reserved
  { 192, 1, false, false, false, false }, // 17 : Preset 2 - T0_S2
  { 128, 1, false, false, false, false }, // 18 : Preset 3 - T0_S3
  { 64, 1, false, false, false, false }, // 19 : Preset 4 - T0_S4
  { 192, 1, false, false, false, false }, // 20 : Preset 12 - T1_S2
  { 128, 1, false, false, false, false }, // 21 : Preset 13 - T1_S3
  { 64, 1, false, false, false, false }, // 22 : Preset 14 - T1_S4
  { 192, 1, false, false, false, false }, // 23 : Preset 22 - T2_S2
  { 168, 1, false, false, false, false }, // 24 : Preset 23 - T2_S3
  { 64, 1, false, false, false, false }, // 25 : Preset 24 - T2_S4
  { 192, 1, false, false, false, false }, // 26 : Preset 32 - T3_S2
  { 168, 1, false, false, false, false }, // 27 : Preset 33 - T3_S3
  { 64, 1, false, false, false, false }, // 28 : Preset 34 - T3_S4
  { 192, 1, false, false, false, false }, // 29 : Preset 42 - T4_S2
  { 168, 1, false, false, false, false }, // 30 : Preset 43 - T4_S3
  { 64, 1, false, false, false, false }, // 31 : Preset 44 - T4_S4
  { 0, 1, false, false, false, false }, // 32 : Preset 10 - T1E_S0
  { 255, 1, false, false, false, false }, // 33 : Preset 11 - T1E_S1
  { 0, 1, false, false, false, false }, // 34 : Preset 20 - T2E_S0
  { 255, 1, false, false, false, false }, // 35 : Preset 21 - T2E_S1
  { 0, 1, false, false, false, false }, // 36 : Preset 30 - T3E_S0
  { 255, 1, false, false, false, false }, // 37 : Preset 31 - T3E_S1
  { 0, 1, false, false, false, false }, // 38 : Preset 40 - T4E_S0
  { 255, 1, false, false, false, false }, // 39 : Preset 41 - T4E_S1
  { 0, 1, false, false, true, false }, // 40 : Reserved
  { 0, 1, false, false, true, false }, // 41 : Reserved
  { 0, 1, false, true, false, false }, // 42 : Area 1 Decrement - T1_DEC
  { 0, 1, false, true, false, false }, // 43 : Area 1 Increment - T1_INC
  { 0, 1, false, true, false, false }, // 44 : Area 2 Decrement - T2_DEC
  { 0, 1, false, true, false, false }, // 45 : Area 2 Increment - T2_INC
  { 0, 1, false, true, false, false }, // 46 : Area 3 Decrement - T3_DEC
  { 0, 1, false, true, false, false }, // 47 : Area 3 Increment - T3_INC
  { 0, 1, false, true, false, false }, // 48 : Area 4 Decrement - T4_DEC
  { 0, 1, false, true, false, false }, // 49 : Area 4 Increment - T4_INC
  { 0, 1, false, true, false, false }, // 50 : Device (Local Button) on : LOCAL_OFF
  { 255, 1, false, true, false, false }, // 51 : Device (Local Button) on : LOCAL_ON
  { 0, 1, false, true, false, false }, // 52 : Area 1 Stop - T1_STOP_S
  { 0, 1, false, true, false, false }, // 53 : Area 2 Stop - T2_STOP_S
  { 0, 1, false, true, false, false }, // 54 : Area 3 Stop - T3_STOP_S
  { 0, 1, false, true, false, false }, // 55 : Area 4 Stop - T4_STOP_S
  { 0, 1, false, false, true, false }, // 56 : Reserved
  { 0, 1, false, false, true, false }, // 57 : Reserved
  { 0, 1, false, false, true, false }, // 58 : Reserved
  { 0, 1, false, false, true, false }, // 59 : Reserved
  { 0, 1, false, false, true, false }, // 60 : Reserved
  { 0, 1, false, false, true, false }, // 61 : Reserved
  { 0, 1, false, false, true, false }, // 62 : Reserved
  { 0, 1, false, false, true, false }, // 63 : Reserved
  // global, appartment-wide, group independent scenes
  { 0, 2, false, true, false, false }, // 64 : Auto Standby - AUTO_STANDBY
  { 255, 1, false, true, false, false }, // 65 : Panic - SIG_PANIC
  { 0, 1, false, false, true, false }, // 66 : Reserved (ENERGY_OL)
  { 0, 1, false, true, false, false }, // 67 : Standby - STANDBY
  { 0, 1, false, true, false, false }, // 68 : Deep Off - DEEP_OFF
  { 0, 1, false, true, false, false }, // 69 : Sleeping - SLEEPING
  { 255, 1, false, true, true, false }, // 70 : Wakeup - WAKE_UP
  { 255, 1, false, true, true, false }, // 71 : Present - PRESENT
  { 0, 1, false, true, false, false }, // 72 : Absent - ABSENT
  { 0, 1, false, true, true, false }, // 73 : Door Bell - SIG_BELL
  { 0, 1, false, false, true, false }, // 74 : Reserved (SIG_ALARM)
  { 255, 1, false, false, true, false }, // 75 : Zone Active
  { 255, 1, false, false, true, false }, // 76 : Reserved
  { 255, 1, false, false, true, false }, // 77 : Reserved
  { 0, 1, false, false, true, false }, // 78 : Reserved
  { 0, 1, false, false, true, false }, // 79 : Reserved
  // all other scenes equal or higher
  { 0, 1, false, false, true, false }, // 80..n : Reserved
};


DsScenePtr LightDeviceSettings::newDefaultScene(SceneNo aSceneNo)
{
  // fetch from defaults
  if (aSceneNo>NUMDEFAULTSCENES)
    aSceneNo = NUMDEFAULTSCENES; // last entry in the table is the default for all higher scene numbers
  const DefaultSceneParams &p = defaultScenes[aSceneNo];
  LightScenePtr lightScene = LightScenePtr(new LightScene(*this, aSceneNo));
  // now set default values
  // - common scene flags
  lightScene->dontCare = p.dontCare;
  lightScene->ignoreLocalPriority = p.ignoreLocalPriority;
  // - light scene specifics
  lightScene->sceneBrightness = p.brightness;
  lightScene->dimTimeSelector = p.dimTimeSelector;
  lightScene->flashing = p.flashing;
  lightScene->specialBehaviour = p.specialBehaviour;
  // return it
  return lightScene;
}




#pragma mark - LightBehaviour


LightBehaviour::LightBehaviour(Device &aDevice, size_t aIndex) :
  inherited(aDevice, aIndex),
  // hardware derived parameters
  // persistent settings
  onThreshold(128),
  minBrightness(1),
  maxBrightness(255),
  // volatile state
  localPriority(false),
  isLocigallyOn(false),
  logicalBrightness(0)
{
  // should always be a member of the light group
  setGroup(group_yellow_light);
  // persistent settings
  dimTimeUp[0] = 0x0F; // 100mS
  dimTimeUp[1] = 0x3F; // 800mS
  dimTimeUp[2] = 0x2F; // 400mS
  dimTimeDown[0] = 0x0F; // 100mS
  dimTimeDown[1] = 0x3F; // 800mS
  dimTimeDown[2] = 0x2F; // 400mS
}


Brightness LightBehaviour::getLogicalBrightness()
{
  return logicalBrightness;
}


void LightBehaviour::setLogicalBrightness(Brightness aBrightness, MLMicroSeconds aTransitionTime)
{
  if (aBrightness>255) aBrightness = 255;
  logicalBrightness = aBrightness;
  if (isLocigallyOn) {
    // device is logically ON
    if (isDimmable()) {
      // dimmable, 0=off, 1..255=brightness
      device.setOutputValue(*this, logicalBrightness, aTransitionTime);
    }
    else {
      // not dimmable, on if logical brightness is above threshold
      device.setOutputValue(*this, logicalBrightness>=onThreshold ? 255 : 0, aTransitionTime);
    }
  }
  else {
    // off is off
    device.setOutputValue(*this, 0, aTransitionTime);
  }
}


void LightBehaviour::initBrightnessParams(Brightness aCurrent, Brightness aMin, Brightness aMax)
{
  // save current brightness
  logicalBrightness = aCurrent;
  if (aCurrent>0) isLocigallyOn = true; // logically on if physically on

  if (aMin!=minBrightness || aMax!=maxBrightness) {
    maxBrightness = aMax;
    minBrightness = aMin>0 ? aMin : 1; // never below 1
    markDirty();
  }
}



#pragma mark - behaviour interaction with digitalSTROM system


// apply scene
void LightBehaviour::applyScene(DsScenePtr aScene)
{
  // we can only handle light scenes
  LightScenePtr lightScene = boost::dynamic_pointer_cast<LightScene>(aScene);
  if (lightScene) {
    SceneNo sceneNo = lightScene->sceneNo;
    if (sceneNo==DEC_S || sceneNo==INC_S) {
      // dimming up/down special scenes
      //  Rule 4: All devices which are turned on and not in local priority state take part in the dimming process.
      if (isLocigallyOn && !localPriority) {
        Brightness b = getLogicalBrightness();
        Brightness nb = b;
        if (sceneNo==DEC_S) {
          // dim down
          // Rule 5: Decrement commands only reduce the output value down to a minimum value, but not to zero.
          // If a digitalSTROM Device reaches one of its limits, it stops its ongoing dimming process.
          nb = nb>11 ? nb-11 : 1; // never below 1
          // also make sure we don't go below minDim
          if (nb<minBrightness)
            nb = minBrightness;
        }
        else {
          // dim up
          nb = nb<255-11 ? nb+11 : 255;
          // also make sure we don't go above maxDim
          if (nb>maxBrightness)
            nb = maxBrightness;
        }
        if (nb!=b) {
          isLocigallyOn = nb!=0; // TODO: is this correct?
          // TODO: pass correct transition time
          setLogicalBrightness(nb, 300*MilliSecond); // up commands arrive approx every 250mS, give it some extra to avoid stutter
          LOG(LOG_NOTICE,"- CallScene DIM: Dimmed to new value %d\n", nb);
        }
      }
    }
    else if (sceneNo==STOP_S) {
      // stop dimming
      // TODO: when fine tuning dimming, we'll need to actually stop ongoing DALI dimming. For now, it's just a NOP
    }
    else if (sceneNo==MIN_S) {
      // TODO: this is a duplicate implementation of "callscenemin"
      Brightness b = minBrightness;
      isLocigallyOn = true; // mindim always turns on light
      // TODO: pass correct transition time
      setLogicalBrightness(b, 0);
      LOG(LOG_NOTICE,"- CallScene(MIN_S): setting minDim %d\n", b);
    }
    else {
      if (!lightScene->dontCare && (!localPriority || lightScene->ignoreLocalPriority)) {
        // apply to output
        Brightness b = lightScene->sceneBrightness;
        isLocigallyOn = b!=0; // TODO: is this correct?
        // TODO: pass correct transition time
        setLogicalBrightness(b, 0);
        LOG(LOG_NOTICE,"- CallScene: Applied output value from scene %d : %d\n", sceneNo, b);
      }
    }
  }
}


// capture scene
void LightBehaviour::captureScene(DsScenePtr aScene)
{
  // we can only handle light scenes
  LightScenePtr lightScene = boost::dynamic_pointer_cast<LightScene>(aScene);
  if (lightScene) {
    // just capture the output value
    if (lightScene->sceneBrightness != getLogicalBrightness()) {
      lightScene->sceneBrightness = getLogicalBrightness();
      lightScene->markDirty();
    }
  }
}



#define BLINK_HALF_PERIOD (500*MilliSecond)

void LightBehaviour::nextBlink()
{
  Brightness b = getLogicalBrightness();
  if (b<128)
    b = 255;
  else {
    b = 1;
    // one complete period
    blinkCounter--;
  }
  isLocigallyOn = b!=0; // TODO: is this correct?
  setLogicalBrightness(b,0);
  // schedule next
  if (blinkCounter>0) {
    // schedule next blink
    MainLoop::currentMainLoop()->executeOnce(boost::bind(&LightBehaviour::nextBlink, this), BLINK_HALF_PERIOD);
  }
}


#pragma mark - persistence implementation


const char *LightBehaviour::tableName()
{
  return "LightOutputSettings";
}


// data field definitions

static const size_t numFields = 5;

size_t LightBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *LightBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "onThreshold", SQLITE_INTEGER },
    { "minDim", SQLITE_INTEGER },
    { "maxDim", SQLITE_INTEGER },
    { "dimUpTimes", SQLITE_INTEGER },
    { "dimDownTimes", SQLITE_INTEGER },
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void LightBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  onThreshold = aRow->get<int>(aIndex++);
  minBrightness = aRow->get<int>(aIndex++);
  maxBrightness = aRow->get<int>(aIndex++);
  uint32_t du = aRow->get<int>(aIndex++);
  uint32_t dd = aRow->get<int>(aIndex++);
  // dissect dimming times
  dimTimeUp[0] = du & 0xFF;
  dimTimeUp[1] = (du>>8) & 0xFF;
  dimTimeUp[2] = (du>>16) & 0xFF;
  dimTimeDown[0] = dd & 0xFF;
  dimTimeDown[1] = (dd>>8) & 0xFF;
  dimTimeDown[2] = (dd>>16) & 0xFF;
}


// bind values to passed statement
void LightBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // create dimming time fields
  uint32_t du =
    dimTimeUp[0] |
    (dimTimeUp[1]<<8) |
    (dimTimeUp[2]<<16);
  uint32_t dd =
    dimTimeDown[0] |
    (dimTimeDown[1]<<8) |
    (dimTimeDown[2]<<16);
  // bind the fields
  aStatement.bind(aIndex++, onThreshold);
  aStatement.bind(aIndex++, minBrightness);
  aStatement.bind(aIndex++, maxBrightness);
  aStatement.bind(aIndex++, (int)du);
  aStatement.bind(aIndex++, (int)dd);
}



#pragma mark - property access


static char light_key;

// settings properties

enum {
  onThreshold_key,
  minBrightness_key,
  maxBrightness_key,
  dimTimeUp_key,
  dimTimeDown_key,
  dimTimeUpAlt1_key,
  dimTimeDownAlt1_key,
  dimTimeUpAlt2_key,
  dimTimeDownAlt2_key,
  numSettingsProperties
};


int LightBehaviour::numSettingsProps() { return numSettingsProperties; }
const PropertyDescriptor *LightBehaviour::getSettingsDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numSettingsProperties] = {
    { "onThreshold", ptype_int8, false, onThreshold_key+settings_key_offset, &light_key },
    { "minBrightness", ptype_int8, false, minBrightness_key+settings_key_offset, &light_key },
    { "maxBrightness", ptype_int8, false, maxBrightness_key+settings_key_offset, &light_key },
    { "dimTimeUp", ptype_int8, false, dimTimeUp_key+settings_key_offset, &light_key },
    { "dimTimeUpAlt1", ptype_int8, false, dimTimeUpAlt1_key+settings_key_offset, &light_key },
    { "dimTimeUpAlt2", ptype_int8, false, dimTimeUpAlt2_key+settings_key_offset, &light_key },
    { "dimTimeDown", ptype_int8, false, dimTimeDown_key+settings_key_offset, &light_key },
    { "dimTimeDownAlt1", ptype_int8, false, dimTimeDownAlt1_key+settings_key_offset, &light_key },
    { "dimTimeDownAlt2", ptype_int8, false, dimTimeDownAlt2_key+settings_key_offset, &light_key },
  };
  return &properties[aPropIndex];
}


// access to all fields

bool LightBehaviour::accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&light_key) {
    if (!aForWrite) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        // Settings properties
        case onThreshold_key+settings_key_offset:
          aPropValue = JsonObject::newInt32(onThreshold);
          return true;
        case minBrightness_key+settings_key_offset:
          aPropValue = JsonObject::newInt32(minBrightness);
          return true;
        case maxBrightness_key+settings_key_offset:
          aPropValue = JsonObject::newInt32(maxBrightness);
          return true;
        case dimTimeUp_key+settings_key_offset:
        case dimTimeUpAlt1_key+settings_key_offset:
        case dimTimeUpAlt2_key+settings_key_offset:
          aPropValue = JsonObject::newInt32(dimTimeUp[aPropertyDescriptor.accessKey-(dimTimeUp_key+settings_key_offset)]);
          return true;
        case dimTimeDown_key+settings_key_offset:
        case dimTimeDownAlt1_key+settings_key_offset:
        case dimTimeDownAlt2_key+settings_key_offset:
          aPropValue = JsonObject::newInt32(dimTimeDown[aPropertyDescriptor.accessKey-(dimTimeDown_key+settings_key_offset)]);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor.accessKey) {
          // Settings properties
        case onThreshold_key+settings_key_offset:
          onThreshold = (Brightness)aPropValue->int32Value();
          markDirty();
          return true;
        case minBrightness_key+settings_key_offset:
          minBrightness = (Brightness)aPropValue->int32Value();
          markDirty();
          return true;
        case maxBrightness_key+settings_key_offset:
          minBrightness = (Brightness)aPropValue->int32Value();
          markDirty();
          return true;
        case dimTimeUp_key+settings_key_offset:
        case dimTimeUpAlt1_key+settings_key_offset:
        case dimTimeUpAlt2_key+settings_key_offset:
          dimTimeUp[aPropertyDescriptor.accessKey-(dimTimeUp_key+settings_key_offset)] = (DimmingTime)aPropValue->int32Value();
          return true;
        case dimTimeDown_key+settings_key_offset:
        case dimTimeDownAlt1_key+settings_key_offset:
        case dimTimeDownAlt2_key+settings_key_offset:
          dimTimeDown[aPropertyDescriptor.accessKey-(dimTimeDown_key+settings_key_offset)] = (DimmingTime)aPropValue->int32Value();
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
}


#pragma mark - description/shortDesc


string LightBehaviour::shortDesc()
{
  return string("Light");
}


string LightBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- logical brightness = %d, logical on = %d, localPriority = %d\n", logicalBrightness, isLocigallyOn, localPriority);
  string_format_append(s, "- dimmable: %d, mindim=%d, maxdim=%d, onThreshold=%d\n", isDimmable(), minBrightness, maxBrightness, onThreshold);
  s.append(inherited::description());
  return s;
}







