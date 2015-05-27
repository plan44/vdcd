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

#include "shadowbehaviour.hpp"


using namespace p44;


#pragma mark - ShadowScene


ShadowScene::ShadowScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inherited(aSceneDeviceSettings, aSceneNo)
{
}


#pragma mark - shadow scene values/channels


double ShadowScene::sceneValue(size_t aChannelIndex)
{
  ChannelBehaviourPtr cb = getDevice().getChannelByIndex(aChannelIndex);
  if (cb->getChannelType()==channeltype_position_angle) {
    return angle;
  }
  return inherited::sceneValue(aChannelIndex);
}


void ShadowScene::setSceneValue(size_t aChannelIndex, double aValue)
{
  ChannelBehaviourPtr cb = getDevice().getChannelByIndex(aChannelIndex);
  if (cb->getChannelType()==channeltype_position_angle) {
    angle = aValue;
    return;
  }
  inherited::setSceneValue(aChannelIndex, aValue);
}


#pragma mark - shadow scene persistence

const char *ShadowScene::tableName()
{
  return "ShadowScenes";
}

// data field definitions

static const size_t numShadowSceneFields = 1;

size_t ShadowScene::numFieldDefs()
{
  return inherited::numFieldDefs()+numShadowSceneFields;
}


const FieldDefinition *ShadowScene::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numShadowSceneFields] = {
    { "angle", SQLITE_FLOAT }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numShadowSceneFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void ShadowScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the fields
  angle = aRow->get<double>(aIndex++);
}


/// bind values to passed statement
void ShadowScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, angle);
}



#pragma mark - default color scene

void ShadowScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  // set the common simple scene defaults
  inherited::setDefaultSceneValues(aSceneNo);
  // Add special shadow behaviour
  // Note: the specs docs state that preset 2 has
  switch (aSceneNo) {
    case SIG_PANIC:
    case SMOKE:
    case HAIL:
      // Panic, Smoke, Hail: open
      value = 100;
      break;
    case T0_S2:
    case T1_S2:
    case T2_S2:
    case T3_S2:
    case T4_S2:
      // For some reason, Preset 2 is not 75%, but also 100% for shade devices.
      value = 100;
      break;
  }
}


#pragma mark - ShadowDeviceSettings with default shadow scenes factory


ShadowDeviceSettings::ShadowDeviceSettings(Device &aDevice) :
  inherited(aDevice)
{
};


DsScenePtr ShadowDeviceSettings::newDefaultScene(SceneNo aSceneNo)
{
  ShadowScenePtr shadowScene = ShadowScenePtr(new ShadowScene(*this, aSceneNo));
  shadowScene->setDefaultSceneValues(aSceneNo);
  // return it
  return shadowScene;
}



#pragma mark - ShadowBehaviour

ShadowBehaviour::ShadowBehaviour(Device &aDevice) :
  inherited(aDevice),
  // hardware derived parameters
  shadowDeviceKind(shadowdevice_jalousie),
  // persistent settings
  openTime(60),
  closeTime(60),
  angleOpenTime(2),
  angleCloseTime(2)
{
  // make it member of the light group
  setGroupMembership(group_grey_shadow, true);
  // primary output controls brightness
  setHardwareName("position");
  // add the channels (every shadow device has an angle so far)
  position = ShadowPositionChannelPtr(new ShadowPositionChannel(*this));
  addChannel(position);
  angle = ShadowAngleChannelPtr(new ShadowAngleChannel(*this));
  addChannel(angle);
}


Tristate ShadowBehaviour::hasModelFeature(DsModelFeatures aFeatureIndex)
{
  // now check for light behaviour level features
  switch (aFeatureIndex) {
    case modelFeature_transt:
      // Assumption: all shadow output devices don't transition times
      return no;
    case modelFeature_shadeposition:
      // Assumption: Shade outputs should be 16bit resolution and be labelled "Position", not "Value"
      return yes;
    default:
      // not available at this level, ask base class
      return inherited::hasModelFeature(aFeatureIndex);
  }
}




int ShadowBehaviour::currentMovingDirection()
{
  // TODO: %%% implement
  if (!isEnabled()) {
    // disabled blinds are always non-moving!
    return 0;
  }
//  else if (isDimmable()) {
//    // dim output
//    return brightness->getTransitionalValue();
//  }
//  else {
//    // switch output
//    return brightness->getChannelValue() >= onThreshold ? brightness->getMax() : brightness->getMin();
//  }
  return 0; // TODO: %%% implement
}




#pragma mark - behaviour interaction with digitalSTROM system


#define AUTO_OFF_FADE_TIME (60*Second)
#define AUTO_OFF_FADE_STEPSIZE 5

// apply scene
bool ShadowBehaviour::applyScene(DsScenePtr aScene)
{
/* TODO: implement */ #warning %%% implement
  // check special cases for light scenes
//  ShadowScenePtr shadowScene = boost::dynamic_pointer_cast<ShadowScene>(aScene);
//  if (shadowScene) {
//    // any scene call cancels actions (and fade down)
//    stopActions();
//    SceneNo sceneNo = shadowScene->sceneNo;
//    // now check for special hard-wired scenes
//    if (sceneNo==AUTO_OFF) {
//      // slow fade down
//      Brightness b = brightness->getChannelValue();
//      if (b>0) {
//        Brightness mb = b - brightness->getMinDim();
//        MLMicroSeconds fadeStepTime;
//        if (mb>AUTO_OFF_FADE_STEPSIZE)
//          fadeStepTime = AUTO_OFF_FADE_TIME / mb * AUTO_OFF_FADE_STEPSIZE; // more than one step
//        else
//          fadeStepTime = AUTO_OFF_FADE_TIME; // single step, to be executed after fade time
//        fadeDownTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&LightBehaviour::fadeDownHandler, this, fadeStepTime), fadeStepTime);
//        LOG(LOG_NOTICE,"- ApplyScene(AUTO_OFF): starting slow fade down from %d to %d (and then OFF) in steps of %d, stepTime = %dmS\n", (int)b, (int)brightness->getMinDim(), AUTO_OFF_FADE_STEPSIZE, (int)(fadeStepTime/MilliSecond));
//        return false; // fade down process will take care of output updates
//      }
//    }
//  } // if shadowScene
  // other type of scene, let base class handle it
  return inherited::applyScene(aScene);
}



void ShadowBehaviour::loadChannelsFromScene(DsScenePtr aScene)
{
  ShadowScenePtr shadowScene = boost::dynamic_pointer_cast<ShadowScene>(aScene);
  /* TODO: implement */ #warning %%% implement
//  if (shadowScene) {
//    // load brightness channel from scene
//    Brightness b = shadowScene->value;
//    DsSceneEffect e = shadowScene->effect;
//    brightness->setChannelValueIfNotDontCare(shadowScene, b, transitionTimeFromSceneEffect(e, true), transitionTimeFromSceneEffect(e, false), true);
//  }
//  else {
//    // only if not light scene, use default loader
//    inherited::loadChannelsFromScene(aScene);
//  }
}


void ShadowBehaviour::saveChannelsToScene(DsScenePtr aScene)
{
  ShadowScenePtr shadowScene = boost::dynamic_pointer_cast<ShadowScene>(aScene);
  /* TODO: implement */ #warning %%% implement
//  if (shadowScene) {
//    // save brightness channel from scene
//    shadowScene->setRepVar(shadowScene->value, brightness->getChannelValue());
//    shadowScene->setSceneValueFlags(brightness->getChannelIndex(), valueflags_dontCare, false);
//  }
//  else {
//    // only if not light scene, use default save
//    inherited::saveChannelsToScene(aScene);
//  }
}




void ShadowBehaviour::performSceneActions(DsScenePtr aScene, SimpleCB aDoneCB)
{
  // we can only handle light scenes
  /* TODO: implement */ #warning %%% implement
//  ShadowScenePtr shadowScene = boost::dynamic_pointer_cast<ShadowScene>(aScene);
//  if (shadowScene && shadowScene->effect==scene_effect_alert) {
//    // run blink effect
//    blink(4*Second, shadowScene, aDoneCB, 2*Second, 50);
//    return;
//  }
//  // none of my effects, let inherited check
//  inherited::performSceneActions(aScene, aDoneCB);
}


void ShadowBehaviour::stopActions()
{
  /* TODO: implement */ #warning %%% implement
//  // stop fading down
//  MainLoop::currentMainLoop().cancelExecutionTicket(fadeDownTicket);
//  // stop blink
//  if (blinkTicket) stopBlink();
//  // let inherited stop as well
//  inherited::stopActions();
}



void ShadowBehaviour::identifyToUser()
{
  /* TODO: implement */ #warning %%% implement
//  // simple, non-parametrized blink, 4 seconds, 2 second period, 1 second on
//  blink(4*Second, LightScenePtr(), NULL, 2*Second, 50);
}




#pragma mark - persistence implementation


const char *ShadowBehaviour::tableName()
{
  return "ShadowOutputSettings";
}


// data field definitions

static const size_t numFields = 4;

size_t ShadowBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *ShadowBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "openTime", SQLITE_FLOAT },
    { "closeTime", SQLITE_FLOAT },
    { "angleOpenTime", SQLITE_FLOAT },
    { "angleCloseTime", SQLITE_FLOAT },
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void ShadowBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the fields
  openTime = aRow->get<double>(aIndex++);
  closeTime = aRow->get<double>(aIndex++);
  angleOpenTime = aRow->get<double>(aIndex++);
  angleCloseTime = aRow->get<double>(aIndex++);
}


// bind values to passed statement
void ShadowBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, openTime);
  aStatement.bind(aIndex++, closeTime);
  aStatement.bind(aIndex++, angleOpenTime);
  aStatement.bind(aIndex++, angleCloseTime);
}



#pragma mark - property access


static char shadow_key;

// settings properties

enum {
  openTime_key,
  closeTime_key,
  angleOpenTime_key,
  angleCloseTime_key,
  numSettingsProperties
};


int ShadowBehaviour::numSettingsProps() { return inherited::numSettingsProps()+numSettingsProperties; }
const PropertyDescriptorPtr ShadowBehaviour::getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numSettingsProperties] = {
    { "openTime", apivalue_double, openTime_key+settings_key_offset, OKEY(shadow_key) },
    { "closeTime", apivalue_double, closeTime_key+settings_key_offset, OKEY(shadow_key) },
    { "angleOpenTime", apivalue_double, angleOpenTime_key+settings_key_offset, OKEY(shadow_key) },
    { "angleCloseTime", apivalue_double, angleCloseTime_key+settings_key_offset, OKEY(shadow_key) },
  };
  int n = inherited::numSettingsProps();
  if (aPropIndex<n)
    return inherited::getSettingsDescriptorByIndex(aPropIndex, aParentDescriptor);
  aPropIndex -= n;
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


// access to all fields

bool ShadowBehaviour::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(shadow_key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Settings properties
        case openTime_key+settings_key_offset: aPropValue->setDoubleValue(openTime); return true;
        case closeTime_key+settings_key_offset: aPropValue->setDoubleValue(closeTime); return true;
        case angleOpenTime_key+settings_key_offset: aPropValue->setDoubleValue(angleOpenTime); return true;
        case angleCloseTime_key+settings_key_offset: aPropValue->setDoubleValue(angleCloseTime); return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Settings properties
        case openTime_key+settings_key_offset: openTime = aPropValue->doubleValue(); markDirty(); return true;
        case closeTime_key+settings_key_offset: closeTime = aPropValue->doubleValue(); markDirty(); return true;
        case angleOpenTime_key+settings_key_offset: angleOpenTime = aPropValue->doubleValue(); markDirty(); return true;
        case angleCloseTime_key+settings_key_offset: angleCloseTime = aPropValue->doubleValue(); markDirty(); return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


#pragma mark - description/shortDesc


string ShadowBehaviour::shortDesc()
{
  return string("Shadow");
}


string ShadowBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- position = %.1f, angle = %.1f, localPriority = %d\n", position->getChannelValue(), angle->getChannelValue(), hasLocalPriority());
  s.append(inherited::description());
  return s;
}







