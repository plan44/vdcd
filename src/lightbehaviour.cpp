  //
//  lightbehaviour.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 19.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "lightbehaviour.hpp"

using namespace p44;



#pragma mark - LightScene

typedef struct {
  Brightness sceneValue; ///< output value for this scene
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
  // { sceneValue, dimTimeSelector, flashing, ignoreLocalPriority, dontCare, specialBehaviour }
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



LightScene::LightScene(ParamStore &aParamStore, SceneNo aSceneNo) :
  inherited(aParamStore),
  sceneNo(aSceneNo)
{
  // fetch from defaults
  if (aSceneNo>NUMDEFAULTSCENES)
    aSceneNo = NUMDEFAULTSCENES; // last entry in the table is the default for all higher scene numbers
  const DefaultSceneParams &p = defaultScenes[aSceneNo];
  sceneValue = p.sceneValue;
  dimTimeSelector = p.dimTimeSelector;
  flashing = p.flashing;
  ignoreLocalPriority = p.ignoreLocalPriority;
  dontCare = p.dontCare;
  specialBehaviour = p.specialBehaviour;
}


// SQLIte3 table name to store these parameters to
const char *LightScene::tableName()
{
  return "dsLightScenes";
}


// primary key field definitions
const FieldDefinition *LightScene::getKeyDefs()
{
  static const FieldDefinition keyDefs[] = {
    { "parentID", SQLITE_INTEGER }, // not included in loadFromRow() and bindToStatement()
    { "sceneNo", SQLITE_INTEGER }, // first field included in loadFromRow() and bindToStatement()
    { NULL, 0 },
  };
  return keyDefs;
}


/// data field definitions
const FieldDefinition *LightScene::getFieldDefs()
{
  static const FieldDefinition dataDefs[] = {
    { "sceneValue", SQLITE_INTEGER },
    { "sceneFlags", SQLITE_INTEGER },
    { "dimTimeSelector", SQLITE_INTEGER },
    { NULL, 0 },
  };
  return dataDefs;
}


enum {
  LightSceneFlag_dontCare = 0x0001,
  LightSceneFlag_ignoreLocalPriority = 0x0002,
  LightSceneFlag_specialBehaviour = 0x0004,
  LightSceneFlag_flashing = 0x0008,
  LightSceneFlag_slowTransition = 0x0010,
};


/// load values from passed row
void LightScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  sceneNo = aRow->get<int>(aIndex++);
  sceneValue = aRow->get<int>(aIndex++);
  int flags = aRow->get<int>(aIndex++);
  dimTimeSelector = aRow->get<int>(aIndex++);
  // decode the flags
  dontCare = flags & LightSceneFlag_dontCare;
  ignoreLocalPriority = flags & LightSceneFlag_ignoreLocalPriority;
  specialBehaviour = flags & LightSceneFlag_specialBehaviour;
  flashing = flags & LightSceneFlag_flashing;
}


/// bind values to passed statement
void LightScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // encode the flags
  int flags = 0;
  if (dontCare) flags |= LightSceneFlag_dontCare;
  if (ignoreLocalPriority) flags |= LightSceneFlag_ignoreLocalPriority;
  if (specialBehaviour) flags |= LightSceneFlag_specialBehaviour;
  if (flashing) flags |= LightSceneFlag_flashing;
  // bind the fields
  aStatement.bind(aIndex++, sceneNo);
  aStatement.bind(aIndex++, sceneValue);
  aStatement.bind(aIndex++, flags);
  aStatement.bind(aIndex++, dimTimeSelector);
}


// SCECON
//  Bit0: Don't care flag. Wenn 1 wird der Ausgang nicht verändert
//  Bit1: Lokale Priorisierung ignorieren
//  Bit2: Spezialverhalten aktiv (INC/DEC/STOP/meinCLICK)
//  Bit3: Flashen für diese Szene aktiv
//  Bit4-5 : 0 LED Konfiguration nicht verändern, 1-3 LEDCON0..2 verwenden
//  Bit6-7 : 0 DIMTIME Konfiguration nicht verändern 1-3 DIMTIME0..2 verwenden

uint8_t LightScene::getSceCon()
{
  return
    (dontCare ? 0x01 : 0) +
    (ignoreLocalPriority ? 0x02 : 0) +
    (specialBehaviour ? 0x04 : 0) +
    (flashing ? 0x08 : 0) +
    (dimTimeSelector<<6) & 0xC0;
}


void LightScene::setSceCon(uint8_t aSceCon)
{
  dontCare = aSceCon & 0x01;
  ignoreLocalPriority = aSceCon & 0x02;
  specialBehaviour = aSceCon & 0x04;
  flashing = aSceCon & 0x08;
  dimTimeSelector = (aSceCon>>6) & 0x03;
}


#pragma mark - LightSettings


LightSettings::LightSettings(ParamStore &aParamStore) :
  inherited(aParamStore),
  isDimmable(false),
  onThreshold(128),
  minDim(1),
  maxDim(255)
{
  dimUpTime[0] = 0x0F; // 100mS
  dimUpTime[1] = 0x3F; // 800mS
  dimUpTime[2] = 0x2F; // 400mS
  dimDownTime[0] = 0x0F; // 100mS
  dimDownTime[1] = 0x3F; // 800mS
  dimDownTime[2] = 0x2F; // 400mS
  dimUpStep = 11;
  dimDownStep = 11;
}



LightScenePtr LightSettings::getScene(SceneNo aSceneNo)
{
  // check if we have modified from default data
  LightSceneMap::iterator pos = scenes.find(aSceneNo);
  if (pos!=scenes.end()) {
    // found scene params in map
    return pos->second;
  }
  else {
    // just return default values for this scene
    return LightScenePtr(new LightScene(paramStore, aSceneNo));
  }
}


void LightSettings::updateScene(LightScenePtr aScene)
{
  if (aScene->rowid==0) {
    // unstored so far, add to map of non-default scenes
    scenes[aScene->sceneNo] = aScene;
  }
  // anyway, mark scene dirty
  aScene->markDirty();
  // as we need the ROWID of the lightsettings as parentID, make sure we get saved if we don't have one
  if (rowid==0) markDirty();
}



// SQLIte3 table name to store these parameters to
const char *LightSettings::tableName()
{
  return "dsLight";
}

/// data field definitions
const FieldDefinition *LightSettings::getFieldDefs()
{
  static const FieldDefinition dataDefs[] = {
    { "isDimmable", SQLITE_INTEGER },
    { "onThreshold", SQLITE_INTEGER },
    { "minDim", SQLITE_INTEGER },
    { "maxDim", SQLITE_INTEGER },
    { NULL, 0 },
  };
  return dataDefs;
}


/// load values from passed row
void LightSettings::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  isDimmable = aRow->get<bool>(aIndex++);
  onThreshold = aRow->get<int>(aIndex++);
  minDim = aRow->get<int>(aIndex++);
  maxDim = aRow->get<int>(aIndex++);
}


// bind values to passed statement
void LightSettings::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // bind the fields
  aStatement.bind(aIndex++, isDimmable);
  aStatement.bind(aIndex++, onThreshold);
  aStatement.bind(aIndex++, minDim);
  aStatement.bind(aIndex++, maxDim);
}


// load child parameters (scenes)
ErrorPtr LightSettings::loadChildren()
{
  ErrorPtr err;
  // my own ROWID is the parent key for the children
  string parentID = string_format("%d",rowid);
  // create a template
  LightScenePtr scene = LightScenePtr(new LightScene(paramStore, 0));
  // get the query
  sqlite3pp::query * queryP = scene->newLoadAllQuery(parentID.c_str());
  if (queryP==NULL) {
    // real error preparing query
    err = paramStore.error();
  }
  else {
    for (sqlite3pp::query::iterator row = queryP->begin(); row!=queryP->end(); ++row) {
      // got record
      // - load record fields into scene object
      int index = 0;
      scene->loadFromRow(row, index);
      // - put scene into map of non-default scenes
      scenes[scene->sceneNo] = scene;
      // - fresh object for next row
      scene = LightScenePtr(new LightScene(paramStore, 0));
    }
  }
  return err;
}

// save child parameters (scenes)
ErrorPtr LightSettings::saveChildren()
{
  ErrorPtr err;
  // Cannot save children before I have my own rowID
  if (rowid!=0) {
    // my own ROWID is the parent key for the children
    string parentID = string_format("%d",rowid);
    // save all elements of the map (only dirty ones will be actually stored to DB
    for (LightSceneMap::iterator pos = scenes.begin(); pos!=scenes.end(); ++pos) {
      err = pos->second->saveToStore(parentID.c_str());
    }
  }
  return err;
}


// save child parameters (scenes)
ErrorPtr LightSettings::deleteChildren()
{
  ErrorPtr err;
  for (LightSceneMap::iterator pos = scenes.begin(); pos!=scenes.end(); ++pos) {
    err = pos->second->deleteFromStore();
  }
  return err;
}


DsOutputModes LightSettings::getOutputMode()
{
  return isDimmable ? outputmode_dim_phase_trailing_char : outputmode_switch;
}


void LightSettings::setOutputMode(DsOutputModes aOutputMode)
{
  isDimmable = aOutputMode>=outputmode_dim_eff && aOutputMode<=outputmode_dim_pwm_char;
}



#pragma mark - LightBehaviour

LightBehaviour::LightBehaviour(Device *aDeviceP) :
  inherited(aDeviceP),
  lightSettings(aDeviceP->getDeviceContainer().getDsParamStore()),
  localPriority(false),
  isLocigallyOn(false),
  logicalBrightness(0)
{
  deviceColorGroup = group_yellow_light;
}


void LightBehaviour::setHardwareDimmer(bool aAvailable)
{
  hasDimmer = aAvailable;
  // default to dimming mode if we have a dimmer, not dimming otherwise
  lightSettings.isDimmable = hasDimmer;
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
    if (lightSettings.isDimmable && hasDimmer) {
      // dimmable, 0=off, 1..255=brightness
      deviceP->setOutputValue(0, logicalBrightness, aTransitionTime);
    }
    else {
      // not dimmable, on if logical brightness is above threshold
      deviceP->setOutputValue(0, logicalBrightness>=lightSettings.onThreshold ? 255 : 0, aTransitionTime);
    }
  }
  else {
    // off is off
    deviceP->setOutputValue(0, 0, aTransitionTime);
  }
}


void LightBehaviour::initBrightnessParams(Brightness aCurrent, Brightness aMin, Brightness aMax)
{
  // save current brightness
  logicalBrightness = aCurrent;
  if (aCurrent>0) isLocigallyOn = true; // logically on if physically on

  if (aMin!=lightSettings.minDim || aMax!=lightSettings.maxDim) {
    lightSettings.maxDim = aMax;
    lightSettings.minDim = aMin>0 ? aMin : 1; // never below 1
    lightSettings.markDirty();
  }
}




#pragma mark - functional identification for digitalSTROM system

// Standard group
//  0 variable (all)
//  1 Light (yellow)
//  2 Blinds (grey)
//  3 Climate (blue)
//  4 Audio (cyan)
//  5 Video (magenta)
//  6 Security (red)
//  7 Access (green)
//  8 Joker (black)

// Function ID:
//
//  1111 11
//  5432 1098 76 543210
//  gggg.cccc cc.xxxxxx
//
//  - gggg   : device group (color, class), 0..15
//  - cccccc : device subclass
//             - 000100 : dS-Standard R105 (current dS standard)
//  - xxxxxx : class specific config
//
//  Light:
//  - Xxxxxx : Bit 5 : if set, ramp time is variable and can be set in RAMPTIMEMAX
//  - xXxxxx : Bit 4 : if set, device has a power output
//  - xxXxxx : Bit 3 : if set, device has extra hardware features like extra binary inputs, sensors etc.
//  - xxxXxx : Bit 2 : reserved
//  - xxxxXX : Bit 0..1 : 0 = no button, 1 = one button, 2 = two buttons, 3 = four buttons

//  Name,          FunctionId  ProductId,  ltMode, outputMode,   buttonIdGroup
//  "GE-KM200",    0x1111,     200,        0,      16,           0x10
//  "GE-TKM210",   0x1111,     1234,       0,      16,           0x15
//  "GE-TKM220",   0x1101,     1244,       0,      0,            0x15
//  "GE-TKM230",   0x1102,     1254,       0,      0,            0x15
//  "GE-KL200",    0x1111,     3272,       0,      35,           0x10
//  "GE-KL210",    0x1111,     5320,       0,      35,           0x10
//  "GE-SDM200",   0x1111,     2248,       0,      16,           0x10
//  "GE-SDS200",   0x1119,     6344,       0,      16,           0x10
//  "GR-KL200",    0x2131,     3272,       0,      33,           0x20
//  "GR-KL210",    0x2131,     3282,       0,      33,           0x20
//  "GR-KL220",    0x2131,     3292,       0,      42,           0x20
//  "GR-TKM200",   0x2101,     1224,       0,      0,            0x25
//  "GR-TKM210",   0x2101,     1234,       0,      0,            0x25
//  "RT-TKM200",   0x6001,     1224,       0,      16,           0
//  "RT-SDM200",   0x6001,     2248,       0,      16,           0
//  "GN-TKM200",   0x7050,     1224,       0,      16,           0
//  "GN-TKM210",   0x6001,     1234,       0,      16,           0
//  "GN-KM200",    0x6001,     200,        0,      16,           70
//  "SW-KL200",    0x8111,     5320,       0,      41,           0
//  "SW-KL210",    0x8111,     3273,       0,      40,           0
//  "SW-TKM210",   0x8102,     1234,       0,      0,            0
//  "SW-TKM200",   0x8103,     1224,       0,      0,            0



uint16_t LightBehaviour::functionId()
{
  int i = deviceP->getNumButtons();
  return
    (group_yellow_light<<12) + // always light
    (0x04 << 6) + // DS Standard R105
    (0 << 5) + // no variable ramp time
    (0 << 5) + // no variable ramp time
    (1 << 4) + // light always has power output
    (i>3 ? 3 : i); // 0 = no inputs, 1..2 = 1..2 inputs, 3 = 4 inputs
}



uint16_t LightBehaviour::productId()
{
  return 200;
}


uint16_t LightBehaviour::groupMemberShip()
{
  return group_yellow_light; // fixed to Light
}


uint16_t LightBehaviour::version()
{
  #warning // TODO: just faking a real GE-KM200's version for now
  return 0x0314;
}



uint8_t LightBehaviour::ltMode()
{
  return buttonmode_inactive; // TODO: Really parametrize this
}


uint8_t LightBehaviour::outputMode()
{
  return lightSettings.getOutputMode();
}



uint8_t LightBehaviour::buttonIdGroup()
{
  return group_yellow_light<<4 + buttonfunc_device; // TODO: Really parametrize this
}




#pragma mark - interaction with digitalSTROM system


// handle message from vdSM
ErrorPtr LightBehaviour::handleMessage(string &aOperation, JsonObjectPtr aParams)
{
  ErrorPtr err;
  if (aOperation=="setoutval") {
    JsonObjectPtr o = aParams->get("Outval");
    if (o==NULL) {
      err = ErrorPtr(new vdSMError(
        vdSMErrorMissingParameter,
        string_format("missing parameter 'Outval'")
      ));
    }
    else {
      Brightness b = o->int32Value();
      isLocigallyOn = b!=0; // TODO: is this correct?
      setLogicalBrightness(b);
    }
  }
  else if (aOperation=="callscene") {
    JsonObjectPtr o = aParams->get("Scene");
    if (o==NULL) {
      err = ErrorPtr(new vdSMError(
        vdSMErrorMissingParameter,
        string_format("missing parameter 'Scene'")
      ));
    }
    else {
      SceneNo sceneNo = o->int32Value();
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
            if (nb<lightSettings.minDim)
              nb = lightSettings.minDim;
          }
          else {
            // dim up
            nb = nb<255-11 ? nb+11 : 255;
            // also make sure we don't go above maxDim
            if (nb>lightSettings.maxDim)
              nb = lightSettings.maxDim;
          }
          if (nb!=b) {
            isLocigallyOn = nb!=0; // TODO: is this correct?
            // TODO: pass correct transition time
            setLogicalBrightness(nb, 300*MilliSecond); // up commands arrive approx every 250mS, give it some extra to avoid stutter
            LOG(LOG_NOTICE,"- CallScene DIM: Dimmed to new value %d\n", nb);
          }
        }
      }
      else {
        LightScenePtr scene = lightSettings.getScene(sceneNo);
        if (!scene->dontCare && (!localPriority || scene->ignoreLocalPriority)) {
          // apply to output
          Brightness b = scene->sceneValue;
          isLocigallyOn = b!=0; // TODO: is this correct?
          // TODO: pass correct transition time
          setLogicalBrightness(b, 0);
          LOG(LOG_NOTICE,"- CallScene: Applied output value from scene %d : %d\n", sceneNo, b);
        }
      }
    }
  }
  else if (aOperation=="callscenemin") {
    Brightness b = lightSettings.minDim;
    isLocigallyOn = true; // mindim always turns on light
    // TODO: pass correct transition time
    setLogicalBrightness(b, 0);
    LOG(LOG_NOTICE,"- CallSceneMin: setting minDim %d\n", b);
  }
  else if (aOperation=="undoscenenumber") {
    // TODO: implement undoscenenumber
    LOG(LOG_NOTICE,"Called unimplemented %s on behaviour %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="undoscene") {
    // TODO: implement undoscene
    LOG(LOG_NOTICE,"Called unimplemented %s on behaviour %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="blink") {
    blinkCounter = 3;
    setLogicalBrightness(1,0); // not entirely off to avoid multiple ignition
    nextBlink();
  }
  else {
    err = inherited::handleMessage(aOperation, aParams);
  }
  return err;
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


// get behaviour-specific parameter
ErrorPtr LightBehaviour::getBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t &aValue)
{
  if (aParamName=="MODE")
    aValue = lightSettings.getOutputMode();
  else if (aParamName=="VAL") // current logical brightness value (actual output value might be different for non-dimmables)
    aValue = getLogicalBrightness();
  else if (aParamName=="MINDIM")
    aValue = lightSettings.minDim;
  else if (aParamName=="MAXDIM")
    aValue = lightSettings.maxDim;
  else if (aParamName=="SW_THR")
    aValue = lightSettings.onThreshold;
  else if (aParamName=="SW_THR")
    aValue = lightSettings.onThreshold;
  else if (aParamName=="DIMTIME0_UP")
    aValue = lightSettings.dimUpTime[0];
  else if (aParamName=="DIMTIME1_UP")
    aValue = lightSettings.dimUpTime[1];
  else if (aParamName=="DIMTIME2_UP")
    aValue = lightSettings.dimUpTime[2];
  else if (aParamName=="DIMTIME0_DOWN")
    aValue = lightSettings.dimDownTime[0];
  else if (aParamName=="DIMTIME1_DOWN")
    aValue = lightSettings.dimDownTime[1];
  else if (aParamName=="DIMTIME2_DOWN")
    aValue = lightSettings.dimDownTime[2];
  else if (aParamName=="STEP0_UP")
    aValue = lightSettings.dimUpStep;
  else if (aParamName=="STEP0_DOWN")
    aValue = lightSettings.dimDownStep;
  else if (aParamName=="SCE")
    aValue = lightSettings.getScene(aArrayIndex)->sceneValue;
  else if (aParamName=="SCECON")
    aValue = lightSettings.getScene(aArrayIndex)->getSceCon();
  else
    return inherited::getBehaviourParam(aParamName, aArrayIndex, aValue); // none of my params, let parent handle it
  // done
  return ErrorPtr();
}


// set behaviour-specific parameter
ErrorPtr LightBehaviour::setBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t aValue)
{
  if (aParamName=="MODE")
    lightSettings.setOutputMode((DsOutputModes)aValue);
  else if (aParamName=="VAL") // set current logical brightness value. // TODO: this is redunant with SetOutval operation
    setLogicalBrightness(aValue);
  else if (aParamName=="MINDIM")
    lightSettings.minDim = aValue;
  else if (aParamName=="MAXDIM")
    lightSettings.maxDim = aValue;
  else if (aParamName=="SW_THR")
    lightSettings.onThreshold = aValue;
  else if (aParamName=="DIMTIME0_UP")
    lightSettings.dimUpTime[0] = aValue;
  else if (aParamName=="DIMTIME1_UP")
    lightSettings.dimUpTime[1] = aValue;
  else if (aParamName=="DIMTIME2_UP")
    lightSettings.dimUpTime[2] = aValue;
  else if (aParamName=="DIMTIME0_DOWN")
    lightSettings.dimDownTime[0] = aValue;
  else if (aParamName=="DIMTIME1_DOWN")
    lightSettings.dimDownTime[1] = aValue;
  else if (aParamName=="DIMTIME2_DOWN")
    lightSettings.dimDownTime[2] = aValue;
  else if (aParamName=="STEP0_UP")
    lightSettings.dimUpStep = aValue;
  else if (aParamName=="STEP0_DOWN")
    lightSettings.dimDownStep = aValue;
  else if (aParamName=="SCE") {
    LightScenePtr ls = lightSettings.getScene(aArrayIndex);
    ls->sceneValue = aValue;
    lightSettings.updateScene(ls);
  }
  else if (aParamName=="SCECON") {
    LightScenePtr ls = lightSettings.getScene(aArrayIndex);
    ls->setSceCon(aValue);
    lightSettings.updateScene(ls);
  }
  else
    return inherited::setBehaviourParam(aParamName, aArrayIndex, aValue); // none of my params, let parent handle it
  // set a local param, mark dirty
  lightSettings.markDirty();
  return ErrorPtr();
}



// this is usually called from the device container when device is added (detected)
ErrorPtr LightBehaviour::load()
{
  // load light settings (and scenes along with it)
  return lightSettings.loadFromStore(deviceP->dsid.getString().c_str());
}


// this is usually called from the device container in regular intervals
ErrorPtr LightBehaviour::save()
{
  // save light settings (and scenes along with it)
  return lightSettings.saveToStore(deviceP->dsid.getString().c_str());
}


ErrorPtr LightBehaviour::forget()
{
  // delete light settings (and scenes along with it)
  return lightSettings.deleteFromStore();
}



#pragma mark - ButtonBehaviour description/shortDesc


string LightBehaviour::shortDesc()
{
  return string("Light");
}


string LightBehaviour::description()
{
  string s = string_format("dS behaviour %s\n", shortDesc().c_str());
  string_format_append(s, "- hardware: %s\n", hasDimmer ? "dimmer" : "switch");
  string_format_append(s, "- logical brightness = %d, logical on = %d, localPriority = %d\n", logicalBrightness, isLocigallyOn, localPriority);
  string_format_append(s, "- dimmable: %d, mindim=%d, maxdim=%d, onThreshold=%d\n", lightSettings.isDimmable, lightSettings.minDim, lightSettings.maxDim, lightSettings.onThreshold);
  s.append(inherited::description());
  return s;
}







