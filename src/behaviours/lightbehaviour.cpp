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

#include "lightbehaviour.hpp"

using namespace p44;



#pragma mark - LightScene


LightScene::LightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inherited(aSceneDeviceSettings, aSceneNo)
{
  sceneBrightness = 0;
  effect = scene_effect_smooth;
}


#pragma mark - scene values/channels


double LightScene::sceneValue(size_t aOutputIndex)
{
  return sceneBrightness;
}


void LightScene::setSceneValue(size_t aOutputIndex, double aValue)
{
  if (aOutputIndex==0) {
    sceneBrightness = aValue;
  }
}


#pragma mark - Light Scene persistence

const char *LightScene::tableName()
{
  return "LightScenes";
}

// data field definitions

static const size_t numSceneFields = 2;

size_t LightScene::numFieldDefs()
{
  return inherited::numFieldDefs()+numSceneFields;
}


const FieldDefinition *LightScene::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numSceneFields] = {
    { "brightness", SQLITE_INTEGER },
    { "effect", SQLITE_INTEGER }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numSceneFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void LightScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  sceneBrightness = aRow->get<int>(aIndex++);
  effect = (DsSceneEffect)aRow->get<int>(aIndex++);
}


/// bind values to passed statement
void LightScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // bind the fields
  aStatement.bind(aIndex++, sceneBrightness);
  aStatement.bind(aIndex++, effect);
}


#pragma mark - Light scene property access


static char lightscene_key;

enum {
  effect_key,
  numLightSceneProperties
};


int LightScene::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  return inherited::numProps(aDomain, aParentDescriptor)+numLightSceneProperties;
}


PropertyDescriptorPtr LightScene::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numLightSceneProperties] = {
    { "effect", apivalue_int64, effect_key, OKEY(lightscene_key) },
  };
  int n = inherited::numProps(aDomain, aParentDescriptor);
  if (aPropIndex<n)
    return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


bool LightScene::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(lightscene_key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        case effect_key:
          aPropValue->setUint8Value(effect);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        case effect_key:
          effect = (DsSceneEffect)aPropValue->uint8Value();
          markDirty();
          return true;
      }
    }
  }
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


#pragma mark - default scene values

typedef struct {
  Brightness brightness; ///< output value for this scene
  DsSceneEffect effect;
  bool ignoreLocalPriority; ///< if set, local priority is ignored when calling this scene
  bool dontCare; ///< if set, applying this scene does not change the output value
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
  // { brightness, effect, ignoreLocalPriority, dontCare }
  {   0, scene_effect_smooth, false, false }, // 0 : Preset 0 - T0_S0
  {   0, scene_effect_smooth, true , false }, // 1 : Area 1 Off - T1_S0
  {   0, scene_effect_smooth, true , false }, // 2 : Area 2 Off - T2_S0
  {   0, scene_effect_smooth, true , false }, // 3 : Area 3 Off - T3_S0
  {   0, scene_effect_smooth, true , false }, // 4 : Area 4 Off - T4_S0
  { 255, scene_effect_smooth, false, false }, // 5 : Preset 1 - T0_S1
  { 255, scene_effect_smooth, true , false }, // 6 : Area 1 On - T1_S1
  { 255, scene_effect_smooth, true , false }, // 7 : Area 2 On - T1_S1
  { 255, scene_effect_smooth, true , false }, // 8 : Area 3 On - T1_S1
  { 255, scene_effect_smooth, true , false }, // 9 : Area 4 On - T1_S1
  {   0, scene_effect_smooth, true , false }, // 10 : Area Stepping continue - T1234_CONT
  {   0, scene_effect_smooth, false, false }, // 11 : Decrement - DEC_S
  {   0, scene_effect_smooth, false, false }, // 12 : Increment - INC_S
  {   0, scene_effect_smooth, true , false }, // 13 : Minimum - MIN_S
  { 255, scene_effect_smooth, true , false }, // 14 : Maximum - MAX_S
  {   0, scene_effect_smooth, true , false }, // 15 : Stop - STOP_S
  {   0, scene_effect_smooth, false, true  }, // 16 : Reserved
  { 192, scene_effect_smooth, false, false }, // 17 : Preset 2 - T0_S2
  { 128, scene_effect_smooth, false, false }, // 18 : Preset 3 - T0_S3
  {  64, scene_effect_smooth, false, false }, // 19 : Preset 4 - T0_S4
  { 192, scene_effect_smooth, false, false }, // 20 : Preset 12 - T1_S2
  { 128, scene_effect_smooth, false, false }, // 21 : Preset 13 - T1_S3
  {  64, scene_effect_smooth, false, false }, // 22 : Preset 14 - T1_S4
  { 192, scene_effect_smooth, false, false }, // 23 : Preset 22 - T2_S2
  { 168, scene_effect_smooth, false, false }, // 24 : Preset 23 - T2_S3
  {  64, scene_effect_smooth, false, false }, // 25 : Preset 24 - T2_S4
  { 192, scene_effect_smooth, false, false }, // 26 : Preset 32 - T3_S2
  { 168, scene_effect_smooth, false, false }, // 27 : Preset 33 - T3_S3
  {  64, scene_effect_smooth, false, false }, // 28 : Preset 34 - T3_S4
  { 192, scene_effect_smooth, false, false }, // 29 : Preset 42 - T4_S2
  { 168, scene_effect_smooth, false, false }, // 30 : Preset 43 - T4_S3
  {  64, scene_effect_smooth, false, false }, // 31 : Preset 44 - T4_S4
  {   0, scene_effect_smooth, false, false }, // 32 : Preset 10 - T1E_S0
  { 255, scene_effect_smooth, false, false }, // 33 : Preset 11 - T1E_S1
  {   0, scene_effect_smooth, false, false }, // 34 : Preset 20 - T2E_S0
  { 255, scene_effect_smooth, false, false }, // 35 : Preset 21 - T2E_S1
  {   0, scene_effect_smooth, false, false }, // 36 : Preset 30 - T3E_S0
  { 255, scene_effect_smooth, false, false }, // 37 : Preset 31 - T3E_S1
  {   0, scene_effect_smooth, false, false }, // 38 : Preset 40 - T4E_S0
  { 255, scene_effect_smooth, false, false }, // 39 : Preset 41 - T4E_S1
  {   0, scene_effect_smooth, false, false }, // 40 : Fade down to 0 in 1min - AUTO_OFF
  {   0, scene_effect_smooth, false, true  }, // 41 : Reserved
  {   0, scene_effect_smooth, true , false }, // 42 : Area 1 Decrement - T1_DEC
  {   0, scene_effect_smooth, true , false }, // 43 : Area 1 Increment - T1_INC
  {   0, scene_effect_smooth, true , false }, // 44 : Area 2 Decrement - T2_DEC
  {   0, scene_effect_smooth, true , false }, // 45 : Area 2 Increment - T2_INC
  {   0, scene_effect_smooth, true , false }, // 46 : Area 3 Decrement - T3_DEC
  {   0, scene_effect_smooth, true , false }, // 47 : Area 3 Increment - T3_INC
  {   0, scene_effect_smooth, true , false }, // 48 : Area 4 Decrement - T4_DEC
  {   0, scene_effect_smooth, true , false }, // 49 : Area 4 Increment - T4_INC
  {   0, scene_effect_smooth, true , false }, // 50 : Device (Local Button) on : LOCAL_OFF
  { 255, scene_effect_smooth, true , false }, // 51 : Device (Local Button) on : LOCAL_ON
  {   0, scene_effect_smooth, true , false }, // 52 : Area 1 Stop - T1_STOP_S
  {   0, scene_effect_smooth, true , false }, // 53 : Area 2 Stop - T2_STOP_S
  {   0, scene_effect_smooth, true , false }, // 54 : Area 3 Stop - T3_STOP_S
  {   0, scene_effect_smooth, true , false }, // 55 : Area 4 Stop - T4_STOP_S
  {   0, scene_effect_smooth, false, true  }, // 56 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 57 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 58 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 59 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 60 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 61 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 62 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 63 : Reserved
  // global, appartment-wide, group independent scenes
  {   0, scene_effect_slow  , true , false }, // 64 : Auto Standby - AUTO_STANDBY
  { 255, scene_effect_smooth, true , false }, // 65 : Panic - SIG_PANIC
  {   0, scene_effect_smooth, false, true  }, // 66 : Reserved (ENERGY_OL)
  {   0, scene_effect_smooth, true , false }, // 67 : Standby - STANDBY
  {   0, scene_effect_smooth, true , false }, // 68 : Deep Off - DEEP_OFF
  {   0, scene_effect_smooth, true , false }, // 69 : Sleeping - SLEEPING
  { 255, scene_effect_smooth, true , true  }, // 70 : Wakeup - WAKE_UP
  { 255, scene_effect_smooth, true , true  }, // 71 : Present - PRESENT
  {   0, scene_effect_smooth, true , false }, // 72 : Absent - ABSENT
  {   0, scene_effect_smooth, true , true  }, // 73 : Door Bell - SIG_BELL
  {   0, scene_effect_smooth, false, true  }, // 74 : Reserved (SIG_ALARM)
  { 255, scene_effect_smooth, false, true  }, // 75 : Zone Active
  { 255, scene_effect_smooth, false, true  }, // 76 : Reserved
  { 255, scene_effect_smooth, false, true  }, // 77 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 78 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 79 : Reserved
  // all other scenes equal or higher
  {   0, scene_effect_smooth, false, true  }, // 80..n : Reserved
};


void LightScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  // fetch from defaults
  if (aSceneNo>NUMDEFAULTSCENES)
    aSceneNo = NUMDEFAULTSCENES; // last entry in the table is the default for all higher scene numbers
  const DefaultSceneParams &p = defaultScenes[aSceneNo];
  // now set default values
  // - common scene flags
  setIgnoreLocalPriority(p.ignoreLocalPriority);
  setDontCare(p.dontCare);
  // - light scene specifics
  sceneBrightness = p.brightness;
  effect = p.effect;
}


#pragma mark - LightDeviceSettings with default light scenes factory


LightDeviceSettings::LightDeviceSettings(Device &aDevice) :
  inherited(aDevice)
{
};


DsScenePtr LightDeviceSettings::newDefaultScene(SceneNo aSceneNo)
{
  LightScenePtr lightScene = LightScenePtr(new LightScene(*this, aSceneNo));
  lightScene->setDefaultSceneValues(aSceneNo);
  // return it
  return lightScene;
}



#pragma mark - LightBehaviour


LightBehaviour::LightBehaviour(Device &aDevice) :
  inherited(aDevice),
  // hardware derived parameters
  // persistent settings
  onThreshold(128),
  minBrightness(1),
  maxBrightness(255),
  // volatile state
  fadeDownTicket(0),
  blinkCounter(0),
  logicalBrightness(0)
{
  // should always be a member of the light group
  setGroup(group_yellow_light);
  // primary output controls brightness
  setHardwareName("brightness");
  // persistent settings
  dimTimeUp[0] = 0x0F; // 100mS
  dimTimeUp[1] = 0x3F; // 800mS
  dimTimeUp[2] = 0x2F; // 400mS
  dimTimeDown[0] = 0x0F; // 100mS
  dimTimeDown[1] = 0x3F; // 800mS
  dimTimeDown[2] = 0x2F; // 400mS
  // add the brightness channel (every light has brightness)
  brightness = ChannelBehaviourPtr(new ChannelBehaviour(*this));
  brightness->setChannelIdentification(channeltype_brightness, "brightness");
  addChannel(brightness);
}


Brightness LightBehaviour::getLogicalBrightness()
{
  return logicalBrightness;
}


void LightBehaviour::setLogicalBrightness(Brightness aBrightness, MLMicroSeconds aTransitionTimeUp, MLMicroSeconds aTransitionTimeDown)
{
  if (aBrightness>255) aBrightness = 255;
  MLMicroSeconds tt = aTransitionTimeDown<0 || aBrightness>logicalBrightness ? aTransitionTimeUp : aTransitionTimeDown;
  logicalBrightness = aBrightness;
  if (isDimmable()) {
    // dimmable, 0=off, 1..255=brightness
    getChannelByType(channeltype_brightness)->setChannelValue(logicalBrightness, tt);
  }
  else {
    // not dimmable, on if logical brightness is above threshold
    getChannelByType(channeltype_brightness)->setChannelValue(logicalBrightness>=onThreshold ? 255 : 0, tt);
  }
}


void LightBehaviour::updateLogicalBrightnessFromOutput()
{
  Brightness o = getChannelByType(channeltype_brightness)->getChannelValue();
  if (isDimmable()) {
    logicalBrightness = o;
  }
  else {
    logicalBrightness = o>onThreshold ? 255 : 0;
  }
}


void LightBehaviour::initBrightnessParams(Brightness aMin, Brightness aMax)
{
  // save max and min
  if (aMin!=minBrightness || aMax!=maxBrightness) {
    maxBrightness = aMax;
    minBrightness = aMin>0 ? aMin : 1; // never below 1
    markDirty();
  }
}



#pragma mark - behaviour interaction with digitalSTROM system


#define AUTO_OFF_FADE_TIME (60*Second)

// apply scene
bool LightBehaviour::applyScene(DsScenePtr aScene)
{
  // we can only handle light scenes
  LightScenePtr lightScene = boost::dynamic_pointer_cast<LightScene>(aScene);
  if (lightScene) {
    // any scene call cancels fade down
    MainLoop::currentMainLoop().cancelExecutionTicket(fadeDownTicket);
    SceneNo sceneNo = lightScene->sceneNo;
    // now check new scene
/*
    // Note: Area dimming scene calls are converted to INC_S/DEC_S/STOP_S at the Device class level
    //  so we only need to handle INC_S/DEC_S and STOP_S here.
    if (sceneNo==DEC_S || sceneNo==INC_S) {
      // dimming up/down special scenes
      //  Rule 4: All devices which are turned on and not in local priority state take part in the dimming process.
      //  Note: local priority check is done at the device level
      Brightness b = getLogicalBrightness();
      if (b>0) {
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
          setLogicalBrightness(nb, 300*MilliSecond); // up commands arrive approx every 250mS, give it some extra to avoid stutter
          LOG(LOG_DEBUG,"- ApplyScene(DIM): Dimming in progress, %d -> %d\n", b, nb);
        }
      }
    }
    else if (sceneNo==STOP_S) {
      // stop dimming
      // TODO: when fine tuning dimming, we'll need to actually stop ongoing dimming. For now, it's just a NOP
      if (LOGENABLED(LOG_NOTICE)) {
        Brightness b = getLogicalBrightness();
        LOG(LOG_NOTICE,"- ApplyScene(DIM): Stopped dimming, final value is %d\n", b);
      }
    }
    else
*/
    if (sceneNo==MIN_S) {
      Brightness b = minBrightness;
      setLogicalBrightness(b, transitionTimeFromSceneEffect(lightScene->effect, false));
      LOG(LOG_NOTICE,"- ApplyScene(MIN_S): setting brightness to minDim %d\n", b);
    }
    else if (sceneNo==AUTO_OFF) {
      // slow fade down
      Brightness b = getLogicalBrightness();
      if (b>0) {
        MLMicroSeconds fadeStepTime = AUTO_OFF_FADE_TIME / b;
        fadeDownTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&LightBehaviour::fadeDownHandler, this, fadeStepTime, b-1), fadeStepTime);
        LOG(LOG_NOTICE,"- ApplyScene(AUTO_OFF): starting slow fade down to zero\n", b);
        return false; // fade down process will take care of output updates
      }
    }
    else {
      // apply stored scene value(s) to output(s)
      recallScene(lightScene);
      LOG(LOG_NOTICE,"- ApplyScene(%d): Applied output value(s) from scene\n", sceneNo);
    }
    // ready for applying values to hardware
    return true;
  } // if lightScene
  else {
    // other type of scene, let base class handle it
    return inherited::applyScene(aScene);
  }
}


void LightBehaviour::fadeDownHandler(MLMicroSeconds aFadeStepTime, Brightness aBrightness)
{
  setLogicalBrightness(aBrightness, aFadeStepTime);
  if (!hwUpdateInProgress || aBrightness==0) {
    // prevent additional apply calls until either 0 reached or previous step done
    hwUpdateInProgress = true;
    device.applyChannelValues(boost::bind(&LightBehaviour::fadeDownStepDone, this));
  }
  if (aBrightness>0) {
    fadeDownTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&LightBehaviour::fadeDownHandler, this, aFadeStepTime, aBrightness-1), aFadeStepTime);
  }
}


void LightBehaviour::fadeDownStepDone()
{
  hwUpdateInProgress = false;
}


void LightBehaviour::recallScene(LightScenePtr aLightScene)
{
  // now apply scene's brightness
  Brightness b = aLightScene->sceneBrightness;
  DsSceneEffect e = aLightScene->effect;
  setLogicalBrightness(b, transitionTimeFromSceneEffect(e, true), transitionTimeFromSceneEffect(e, false));
}



// capture scene
void LightBehaviour::captureScene(DsScenePtr aScene, DoneCB aDoneCB)
{
  // we can only handle light scenes
  LightScenePtr lightScene = boost::dynamic_pointer_cast<LightScene>(aScene);
  if (lightScene) {
    // make sure logical brightness is updated from output
    updateLogicalBrightnessFromOutput();
    // just capture the output value
    if (lightScene->sceneBrightness != getLogicalBrightness()) {
      lightScene->sceneBrightness = getLogicalBrightness();
      lightScene->markDirty();
    }
  }
  inherited::captureScene(aScene, aDoneCB);
}





/// @param aDimTime : dimming time specification in dS format (Bit 7..4 = exponent, Bit 3..0 = 1/150 seconds, i.e. 0x0F = 100mS)
static MLMicroSeconds transitionTimeFromDimTime(uint8_t aDimTime)
{
  return ((MLMicroSeconds)(aDimTime & 0xF)*100*MilliSecond/15)<<((aDimTime>>4) & 0xF);
}


MLMicroSeconds LightBehaviour::transitionTimeFromSceneEffect(DsSceneEffect aEffect, bool aDimUp)
{
  uint8_t dimTimeIndex;
  switch (aEffect) {
    case scene_effect_smooth : dimTimeIndex = 0; break;
    case scene_effect_slow : dimTimeIndex = 1; break;
    case scene_effect_veryslow : dimTimeIndex = 2; break;
    default: return 0; // no known effect -> just return 0 for transition time
  }
  // dimTimeIndex found, look up actual time
  return transitionTimeFromDimTime(aDimUp ? dimTimeUp[dimTimeIndex] : dimTimeDown[dimTimeIndex]);
}


void LightBehaviour::blink(MLMicroSeconds aDuration, MLMicroSeconds aBlinkPeriod, int aOnRatioPercent)
{
  MLMicroSeconds blinkOnTime = (aBlinkPeriod*aOnRatioPercent*10)/1000;
  aBlinkPeriod -= blinkOnTime; // blink off time
  // start off, so first action will be on
  blinkHandler(MainLoop::now()+aDuration, false, blinkOnTime, aBlinkPeriod, getLogicalBrightness());
}


void LightBehaviour::blinkHandler(MLMicroSeconds aEndTime, bool aState, MLMicroSeconds aOnTime, MLMicroSeconds aOffTime, Brightness aOrigBrightness)
{
  if (MainLoop::now()>=aEndTime) {
    // done, restore original brightness
    setLogicalBrightness(aOrigBrightness, 0);
    return;
  }
  else if (!aState) {
    // turn on
    setLogicalBrightness(255, 0);
  }
  else {
    // turn off
    setLogicalBrightness(minBrightness, 0);
  }
  aState = !aState; // toggle
  // schedule next event
  MainLoop::currentMainLoop().executeOnce(
    boost::bind(&LightBehaviour::blinkHandler, this, aEndTime, aState, aOnTime, aOffTime, aOrigBrightness),
    aState ? aOnTime : aOffTime
  );
}



void LightBehaviour::onAtMinBrightness()
{
  if (getLogicalBrightness()==0) {
    // device is off and must be set to minimal logical brightness
    setLogicalBrightness(minBrightness, transitionTimeFromDimTime(dimTimeUp[0]));
  }
}


void LightBehaviour::performSceneActions(DsScenePtr aScene)
{
  // we can only handle light scenes
  LightScenePtr lightScene = boost::dynamic_pointer_cast<LightScene>(aScene);
  if (lightScene && lightScene->effect==scene_effect_alert) {
    // flash
    blink(2*Second, 400*MilliSecond, 80);
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


int LightBehaviour::numSettingsProps() { return inherited::numSettingsProps()+numSettingsProperties; }
const PropertyDescriptorPtr LightBehaviour::getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numSettingsProperties] = {
    { "onThreshold", apivalue_uint64, onThreshold_key+settings_key_offset, OKEY(light_key) },
    { "minBrightness", apivalue_uint64, minBrightness_key+settings_key_offset, OKEY(light_key) },
    { "maxBrightness", apivalue_uint64, maxBrightness_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeUp", apivalue_uint64, dimTimeUp_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeUpAlt1", apivalue_uint64, dimTimeUpAlt1_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeUpAlt2", apivalue_uint64, dimTimeUpAlt2_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeDown", apivalue_uint64, dimTimeDown_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeDownAlt1", apivalue_uint64, dimTimeDownAlt1_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeDownAlt2", apivalue_uint64, dimTimeDownAlt2_key+settings_key_offset, OKEY(light_key) },
  };
  int n = inherited::numSettingsProps();
  if (aPropIndex<n)
    return inherited::getSettingsDescriptorByIndex(aPropIndex, aParentDescriptor);
  aPropIndex -= n;
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


// access to all fields

bool LightBehaviour::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(light_key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Settings properties
        case onThreshold_key+settings_key_offset:
          aPropValue->setUint8Value(onThreshold);
          return true;
        case minBrightness_key+settings_key_offset:
          aPropValue->setUint8Value(minBrightness);
          return true;
        case maxBrightness_key+settings_key_offset:
          aPropValue->setUint8Value(maxBrightness);
          return true;
        case dimTimeUp_key+settings_key_offset:
        case dimTimeUpAlt1_key+settings_key_offset:
        case dimTimeUpAlt2_key+settings_key_offset:
          aPropValue->setUint8Value(dimTimeUp[aPropertyDescriptor->fieldKey()-(dimTimeUp_key+settings_key_offset)]);
          return true;
        case dimTimeDown_key+settings_key_offset:
        case dimTimeDownAlt1_key+settings_key_offset:
        case dimTimeDownAlt2_key+settings_key_offset:
          aPropValue->setUint8Value(dimTimeDown[aPropertyDescriptor->fieldKey()-(dimTimeDown_key+settings_key_offset)]);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
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
          dimTimeUp[aPropertyDescriptor->fieldKey()-(dimTimeUp_key+settings_key_offset)] = (DimmingTime)aPropValue->int32Value();
          return true;
        case dimTimeDown_key+settings_key_offset:
        case dimTimeDownAlt1_key+settings_key_offset:
        case dimTimeDownAlt2_key+settings_key_offset:
          dimTimeDown[aPropertyDescriptor->fieldKey()-(dimTimeDown_key+settings_key_offset)] = (DimmingTime)aPropValue->int32Value();
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


#pragma mark - description/shortDesc


string LightBehaviour::shortDesc()
{
  return string("Light");
}


string LightBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- logical brightness = %d, localPriority = %d\n", logicalBrightness, hasLocalPriority());
  string_format_append(s, "- dimmable: %d, mindim=%d, maxdim=%d, onThreshold=%d\n", isDimmable(), minBrightness, maxBrightness, onThreshold);
  s.append(inherited::description());
  return s;
}







