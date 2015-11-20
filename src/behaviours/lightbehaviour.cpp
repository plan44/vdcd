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

#include "lightbehaviour.hpp"

#include <math.h>

using namespace p44;



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

#define STANDARD_DIM_CURVE_EXPONENT 4 // standard exponent, usually ok for PWM for LEDs

LightBehaviour::LightBehaviour(Device &aDevice) :
  inherited(aDevice),
  // hardware derived parameters
  // persistent settings
  onThreshold(50.0),
  dimCurveExp(STANDARD_DIM_CURVE_EXPONENT),
  // volatile state
  hardwareHasSetMinDim(false),
  fadeDownTicket(0),
  blinkTicket(0)
{
  // make it member of the light group
  setGroupMembership(group_yellow_light, true);
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
  brightness = BrightnessChannelPtr(new BrightnessChannel(*this));
  addChannel(brightness);
}


Tristate LightBehaviour::hasModelFeature(DsModelFeatures aFeatureIndex)
{
  // now check for light behaviour level features
  switch (aFeatureIndex) {
    case modelFeature_outmode:
      // Lights that support dimming (not only switched) should have this
      return getOutputFunction()!=outputFunction_switch ? yes : no;
    case modelFeature_outmodeswitch:
      // Lights with switch-only output (not dimmable) should have this
      return getOutputFunction()==outputFunction_switch ? yes : no;
    case modelFeature_outmodegeneric:
      // suppress generic output mode, we have switched/dimmer selection
      return no;
    case modelFeature_transt:
      // Assumption: all light output devices have transition times
      return yes;
    default:
      // not available at this level, ask base class
      return inherited::hasModelFeature(aFeatureIndex);
  }
}



void LightBehaviour::initMinBrightness(Brightness aMin)
{
  // save min
  brightness->setDimMin(aMin);
  hardwareHasSetMinDim = true;
}


Brightness LightBehaviour::brightnessForHardware()
{
  if (!isEnabled()) {
    // disabled lights are off
    return 0;
  }
  else if (isDimmable()) {
    // dim output (applying output mode transformations)
    return outputValueAccordingToMode(brightness->getTransitionalValue());
  }
  else {
    // switch output
    return brightness->getChannelValue() >= onThreshold ? brightness->getMax() : brightness->getMin();
  }
}


void LightBehaviour::syncBrightnessFromHardware(Brightness aBrightness, bool aAlwaysSync)
{
  if (
    isDimmable() || // for dimmable lights: always update value
    ((aBrightness>=onThreshold) != (brightness->getChannelValue()>=onThreshold)) // for switched outputs: keep value if onThreshold condition is already met
  ) {
    brightness->syncChannelValue(aBrightness, aAlwaysSync);
  }
}




#pragma mark - behaviour interaction with digitalSTROM system


#define AUTO_OFF_FADE_TIME (60*Second)
#define AUTO_OFF_FADE_STEPSIZE 5

// apply scene
bool LightBehaviour::applyScene(DsScenePtr aScene)
{
  // check special cases for light scenes
  LightScenePtr lightScene = boost::dynamic_pointer_cast<LightScene>(aScene);
  if (lightScene) {
    // any scene call cancels actions (and fade down)
    stopSceneActions();
    SceneCmd sceneCmd = lightScene->sceneCmd;
    // now check for special hard-wired scenes
    if (sceneCmd==scene_cmd_slow_off) {
      // slow fade down
      Brightness b = brightness->getChannelValue();
      if (b>0) {
        Brightness mb = b - brightness->getMinDim();
        MLMicroSeconds fadeStepTime;
        if (mb>AUTO_OFF_FADE_STEPSIZE)
          fadeStepTime = AUTO_OFF_FADE_TIME / mb * AUTO_OFF_FADE_STEPSIZE; // more than one step
        else
          fadeStepTime = AUTO_OFF_FADE_TIME; // single step, to be executed after fade time
        fadeDownTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&LightBehaviour::fadeDownHandler, this, fadeStepTime), fadeStepTime);
        LOG(LOG_NOTICE, "- ApplyScene(AUTO_OFF): starting slow fade down from %d to %d (and then OFF) in steps of %d, stepTime = %dmS", (int)b, (int)brightness->getMinDim(), AUTO_OFF_FADE_STEPSIZE, (int)(fadeStepTime/MilliSecond));
        return false; // fade down process will take care of output updates
      }
    }
  } // if lightScene
  // other type of scene, let base class handle it
  return inherited::applyScene(aScene);
}


// TODO: for later: consider if fadeDown is not an action like blink...
void LightBehaviour::fadeDownHandler(MLMicroSeconds aFadeStepTime)
{
  Brightness b = brightness->dimChannelValue(-AUTO_OFF_FADE_STEPSIZE, aFadeStepTime);
  bool isAtMin = b<=brightness->getMinDim();
  if (isAtMin) {
    LOG(LOG_INFO, "- ApplyScene(AUTO_OFF): reached minDim, now turning off lamp");
    brightness->setChannelValue(0); // off
  }
  // Note: device.requestApplyingChannels paces requests to hardware, so we can just call it here without special precautions
  device.requestApplyingChannels(NULL, true); // dimming mode
  if (!isAtMin) {
    // continue
    fadeDownTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&LightBehaviour::fadeDownHandler, this, aFadeStepTime), aFadeStepTime);
  }
}


void LightBehaviour::loadChannelsFromScene(DsScenePtr aScene)
{
  LightScenePtr lightScene = boost::dynamic_pointer_cast<LightScene>(aScene);
  if (lightScene) {
    // load brightness channel from scene
    Brightness b = lightScene->value;
    DsSceneEffect e = lightScene->effect;
    brightness->setChannelValueIfNotDontCare(lightScene, b, transitionTimeFromSceneEffect(e, true), transitionTimeFromSceneEffect(e, false), true);
  }
  else {
    // only if not light scene, use default loader
    inherited::loadChannelsFromScene(aScene);
  }
}


void LightBehaviour::saveChannelsToScene(DsScenePtr aScene)
{
  LightScenePtr lightScene = boost::dynamic_pointer_cast<LightScene>(aScene);
  if (lightScene) {
    // save brightness channel from scene
    lightScene->setPVar(lightScene->value, brightness->getChannelValue());
    lightScene->setSceneValueFlags(brightness->getChannelIndex(), valueflags_dontCare, false);
  }
  else {
    // only if not light scene, use default save
    inherited::saveChannelsToScene(aScene);
  }
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


// dS Dimming rule for Light:
//  Rule 4 All devices which are turned on and not in local priority state take part in the dimming process.

bool LightBehaviour::canDim(DsChannelType aChannelType)
{
  // to dim anything (not only brightness), brightness value must be >0
  return brightness->getChannelValue()>0;
}


void LightBehaviour::onAtMinBrightness(DsScenePtr aScene)
{
  if (brightness->getChannelValue()<=0) {
    // device is off and must be set to minimal logical brightness
    // but only if the brightness stored in the scene is not zero
    LightScenePtr lightScene = boost::dynamic_pointer_cast<LightScene>(aScene);
    if (lightScene && lightScene->sceneValue(brightness->getChannelIndex())>0) {
      // - load scene values for channels
      loadChannelsFromScene(lightScene); // Note: causes log message because channel is set to new value
      // - override brightness with minDim
      brightness->setChannelValue(brightness->getMinDim(), transitionTimeFromSceneEffect(lightScene->effect, true));
    }
  }
}


void LightBehaviour::performSceneActions(DsScenePtr aScene, SimpleCB aDoneCB)
{
  // we can only handle light scenes
  LightScenePtr lightScene = boost::dynamic_pointer_cast<LightScene>(aScene);
  if (lightScene && lightScene->effect==scene_effect_alert) {
    // run blink effect
    blink(4*Second, lightScene, aDoneCB, 2*Second, 50);
    return;
  }
  // none of my effects, let inherited check
  inherited::performSceneActions(aScene, aDoneCB);
}


void LightBehaviour::stopSceneActions()
{
  // stop fading down
  MainLoop::currentMainLoop().cancelExecutionTicket(fadeDownTicket);
  // stop blink
  if (blinkTicket) stopBlink();
  // let inherited stop as well
  inherited::stopSceneActions();
}



void LightBehaviour::identifyToUser()
{
  // simple, non-parametrized blink, 4 seconds, 2 second period, 1 second on
  blink(4*Second, LightScenePtr(), NULL, 2*Second, 50);
}


#pragma mark - PWM dim curve

// TODO: implement multi point adjustable curves, logarithmic curve with adjustable exponent for now

// PWM    = PWM value
// maxPWM = max PWM value
// B      = brightness
// maxB   = max brightness value
// S      = dim Curve Exponent (1=linear, 2=quadratic, ...)
//
//                   (B*S/maxB)
//                 e            - 1
// PWM =  maxPWM * ----------------
//                      S
//                    e   - 1
//
//                           S
//        maxB        P * (e   - 1)
// B   =  ---- * ln ( ------------- + 1)
//          S             maxP
//

double LightBehaviour::brightnessToPWM(Brightness aBrightness, double aMaxPWM)
{
  return aMaxPWM*((exp(aBrightness*dimCurveExp/100)-1)/(exp(dimCurveExp)-1));
}


Brightness LightBehaviour::PWMToBrightness(double aPWM, double aMaxPWM)
{
  return 100/dimCurveExp*log(aPWM*(exp(dimCurveExp)-1)/aMaxPWM + 1);
}




#pragma mark - blinking


void LightBehaviour::blink(MLMicroSeconds aDuration, LightScenePtr aParamScene, SimpleCB aDoneCB, MLMicroSeconds aBlinkPeriod, int aOnRatioPercent)
{
  // save current state in temp scene
  blinkDoneHandler = aDoneCB;
  SceneDeviceSettingsPtr scenes = device.getScenes();
  if (scenes) {
    // device has scenes, get a default scene to capture current state
    blinkRestoreScene = boost::dynamic_pointer_cast<LightScene>(device.getScenes()->newDefaultScene(T0_S0)); // main off
    captureScene(blinkRestoreScene, false, boost::bind(&LightBehaviour::beforeBlinkStateSavedHandler, this, aDuration, aParamScene, aBlinkPeriod, aOnRatioPercent));
  }
  else {
    // device has no scenes (some switch outputs don't have scenes)
    beforeBlinkStateSavedHandler(aDuration, aParamScene, aBlinkPeriod, aOnRatioPercent);
  }
}


void LightBehaviour::stopBlink()
{
  // immediately terminate (also kills ticket)
  blinkHandler(0 /* done */, false, 0, 0); // dummy params, only to immediately stop it
}



void LightBehaviour::beforeBlinkStateSavedHandler(MLMicroSeconds aDuration, LightScenePtr aParamScene, MLMicroSeconds aBlinkPeriod, int aOnRatioPercent)
{
  // apply the parameter scene if any
  if (aParamScene) loadChannelsFromScene(aParamScene);
  // start flashing
  MLMicroSeconds blinkOnTime = (aBlinkPeriod*aOnRatioPercent*10)/1000;
  aBlinkPeriod -= blinkOnTime; // blink off time
  // start off, so first action will be on
  blinkHandler(MainLoop::now()+aDuration, false, blinkOnTime, aBlinkPeriod);
}


void LightBehaviour::blinkHandler(MLMicroSeconds aEndTime, bool aState, MLMicroSeconds aOnTime, MLMicroSeconds aOffTime)
{
  if (MainLoop::now()>=aEndTime) {
    // kill scheduled execution, if any
    MainLoop::currentMainLoop().cancelExecutionTicket(blinkTicket);
    // restore previous values if any
    if (blinkRestoreScene) {
      loadChannelsFromScene(blinkRestoreScene);
      blinkRestoreScene.reset();
      device.requestApplyingChannels(NULL, false); // apply to hardware, not dimming
    }
    // done, call end handler if any
    if (blinkDoneHandler) {
      SimpleCB h = blinkDoneHandler;
      blinkDoneHandler = NULL;
      h();
    }
    return;
  }
  else if (!aState) {
    // turn on
    brightness->setChannelValue(brightness->getMax(), 0);
  }
  else {
    // turn off
    brightness->setChannelValue(brightness->getMinDim(), 0);
  }
  // apply to hardware
  device.requestApplyingChannels(NULL, false); // not dimming
  aState = !aState; // toggle
  // schedule next event
  blinkTicket = MainLoop::currentMainLoop().executeOnce(
    boost::bind(&LightBehaviour::blinkHandler, this, aEndTime, aState, aOnTime, aOffTime),
    aState ? aOnTime : aOffTime
  );
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
    { "switchThreshold", SQLITE_FLOAT }, // formerly onThreshold, renamed because type changed
    { "minBrightness", SQLITE_FLOAT }, // formerly minBrightness, renamed because type changed
    { "dimUpTimes", SQLITE_INTEGER },
    { "dimDownTimes", SQLITE_INTEGER },
    { "dimCurveExp", SQLITE_FLOAT },
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void LightBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // read onThreshold only if not NULL
  if (aRow->column_type(aIndex)!=SQLITE_NULL)
    onThreshold = aRow->get<double>(aIndex);
  aIndex++;
  // get the other fields
  Brightness md = aRow->get<double>(aIndex++);
  if (!hardwareHasSetMinDim) brightness->setDimMin(md); // only apply if not set by hardware
  uint32_t du = aRow->get<int>(aIndex++);
  uint32_t dd = aRow->get<int>(aIndex++);
  // read dim curve exponent only if not NULL
  if (aRow->column_type(aIndex)!=SQLITE_NULL)
    dimCurveExp = aRow->get<double>(aIndex);
  aIndex++;
  // dissect dimming times
  dimTimeUp[0] = du & 0xFF;
  dimTimeUp[1] = (du>>8) & 0xFF;
  dimTimeUp[2] = (du>>16) & 0xFF;
  dimTimeDown[0] = dd & 0xFF;
  dimTimeDown[1] = (dd>>8) & 0xFF;
  dimTimeDown[2] = (dd>>16) & 0xFF;
}


// bind values to passed statement
void LightBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
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
  aStatement.bind(aIndex++, brightness->getMinDim());
  aStatement.bind(aIndex++, (int)du);
  aStatement.bind(aIndex++, (int)dd);
  aStatement.bind(aIndex++, dimCurveExp);
}



#pragma mark - property access


static char light_key;

// settings properties

enum {
  onThreshold_key,
  minBrightness_key,
  dimTimeUp_key, // upAlt1/2 must immediately follow (array index calculation in accessField below!)
  dimTimeUpAlt1_key,
  dimTimeUpAlt2_key,
  dimTimeDown_key, // downAlt1/2 must immediately follow (array index calculation in accessField below!)
  dimTimeDownAlt1_key,
  dimTimeDownAlt2_key,
  dimCurveExp_key,
  numSettingsProperties
};


int LightBehaviour::numSettingsProps() { return inherited::numSettingsProps()+numSettingsProperties; }
const PropertyDescriptorPtr LightBehaviour::getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numSettingsProperties] = {
    { "onThreshold", apivalue_double, onThreshold_key+settings_key_offset, OKEY(light_key) },
    { "minBrightness", apivalue_double, minBrightness_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeUp", apivalue_uint64, dimTimeUp_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeUpAlt1", apivalue_uint64, dimTimeUpAlt1_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeUpAlt2", apivalue_uint64, dimTimeUpAlt2_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeDown", apivalue_uint64, dimTimeDown_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeDownAlt1", apivalue_uint64, dimTimeDownAlt1_key+settings_key_offset, OKEY(light_key) },
    { "dimTimeDownAlt2", apivalue_uint64, dimTimeDownAlt2_key+settings_key_offset, OKEY(light_key) },
    { "x-p44-dimCurveExp", apivalue_double, dimCurveExp_key+settings_key_offset, OKEY(light_key) },
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
          aPropValue->setDoubleValue(onThreshold);
          return true;
        case minBrightness_key+settings_key_offset:
          aPropValue->setDoubleValue(brightness->getMinDim());
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
        case dimCurveExp_key+settings_key_offset:
          aPropValue->setDoubleValue(dimCurveExp);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Settings properties
        case onThreshold_key+settings_key_offset:
          setPVar(onThreshold, aPropValue->doubleValue());
          return true;
        case minBrightness_key+settings_key_offset:
          brightness->setDimMin(aPropValue->doubleValue());
          if (!hardwareHasSetMinDim) markDirty();
          return true;
        case dimTimeUp_key+settings_key_offset:
        case dimTimeUpAlt1_key+settings_key_offset:
        case dimTimeUpAlt2_key+settings_key_offset:
          setPVar(dimTimeUp[aPropertyDescriptor->fieldKey()-(dimTimeUp_key+settings_key_offset)], (DimmingTime)aPropValue->int32Value());
          return true;
        case dimTimeDown_key+settings_key_offset:
        case dimTimeDownAlt1_key+settings_key_offset:
        case dimTimeDownAlt2_key+settings_key_offset:
          setPVar(dimTimeDown[aPropertyDescriptor->fieldKey()-(dimTimeDown_key+settings_key_offset)], (DimmingTime)aPropValue->int32Value());
          return true;
        case dimCurveExp_key+settings_key_offset:
          setPVar(dimCurveExp, aPropValue->doubleValue());
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
  string s = string_format("%s behaviour", shortDesc().c_str());
  string_format_append(s, "\n- brightness = %.1f, localPriority = %d", brightness->getChannelValue(), hasLocalPriority());
  string_format_append(s, "\n- dimmable: %d, minBrightness=%.1f, onThreshold=%.1f", isDimmable(), brightness->getMinDim(), onThreshold);
  s.append(inherited::description());
  return s;
}







