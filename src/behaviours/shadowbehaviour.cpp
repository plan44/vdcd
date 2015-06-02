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
  // persistent settings (defaults are MixWerk's)
  openTime(55),
  closeTime(55),
  angleOpenTime(1.8),
  angleCloseTime(1.8),
  // volatile state
  startedMoving(Never),
  movingDirection(0),
  movingAngle(false),
  movingTicket(0),
  updatingMovement(false),
  needAnotherShortMove(false)
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
    case modelFeature_outvalue8:
      // Shade outputs are 16bit resolution and be labelled "Position", not "Value"
      return no; // suppress general 8-bit outmode assumption
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
  if (!isEnabled()) {
    // disabled blinds are always non-moving!
    return 0;
  }
  else {
    // trigger movement update
    MainLoop::currentMainLoop().executeOnce(boost::bind(&ShadowBehaviour::updateMovement, this));
    // return current moving direction as demanded by timed blind control mechanism
    return movingDirection;
  }
}


#define MAX_SHORT_MOVE_TIME (400*MilliSecond)
#define MIN_LONG_MOVE_TIME (1200*MilliSecond)

void ShadowBehaviour::updateMovement()
{
  // prevent recursion via applyChannel
  if (updatingMovement) return;
  updatingMovement = true;
  // check if channels need update
  MLMicroSeconds stopIn = 0;
  double dist = 0;
  bool changed = false;
  needAnotherShortMove = false;
  if (position->needsApplying() && !position->inTransition()) {
    changed = true;
    // determine current position (100 = full up/open)
    position->transitionStep(0); // start transition
    movingAngle = false;
    double targetpos = position->getChannelValue();
    startingPosition = getPosition(false);
    // full up or down always schedule full way to synchronize
    if (targetpos>=100) {
      // fully up, always do full cycle to synchronize position
      dist = 120; // 20% extra to fully run into end switch
    }
    else if (targetpos<=0) {
      // fully down, always do full cycle to synchronize position
      dist = -120; // 20% extra to fully run into end switch
    }
    else {
      // somewhere in between, actually estimate distance
      dist = targetpos-startingPosition; // distance to move up
    }
    // calculate moving time
    if (dist>0) {
      // we'll move up
      stopIn = openTime*Second/100.0*dist;
      // when moving up, angle gets fully opened
      angle->syncChannelValue(100, true);
      changed = true;
    }
    else if (dist<0) {
      // we'll move down
      stopIn = closeTime*Second/100.0*-dist;
      // when moving down, angle gets fully closed
      angle->syncChannelValue(0, true);
      changed = true;
    }
    LOG(LOG_INFO,"Blind position=%.1f%% requested, current=%.1f%% -> moving %s for %.3f Seconds\n", targetpos, startingPosition, dist>0 ? "up" : "down", (double)stopIn/Second);
  }
  else if (angle->needsApplying() && !position->inTransition()) {
    // can accept angle changes only when position is no longer in transition
    // determine current angle (100 = fully open)
    if (position->getChannelValue()>=100) {
      // blind is fully up, angle is irrelevant -> consider applied
      angle->channelValueApplied();
    }
    else {
      changed = true;
      angle->transitionStep(0); // start transition
      movingAngle = true;
      double targetang = angle->getChannelValue();
      startingAngle = getAngle(false);
      dist = targetang-startingAngle; // distance to move up
      // calculate new stop time
      if (dist>0) {
        stopIn = angleOpenTime*Second/100.0*dist; // up
      }
      else if (dist<0) {
        stopIn = angleCloseTime*Second/100.0*-dist; // down
      }
      LOG(LOG_INFO,"Blind angle=%.1f%% requested, current=%.1f%% -> moving %s for %.3f Seconds\n", targetang, startingAngle, dist>0 ? "up" : "down", (double)stopIn/Second);
    }
  }
  // now apply
  if (changed) {
    if (stopIn>0) {
      if (stopIn<MIN_LONG_MOVE_TIME && stopIn>MAX_SHORT_MOVE_TIME) {
        // need multiple shorts
        stopIn = MAX_SHORT_MOVE_TIME;
        LOG(LOG_INFO,"- must restrict to %.3f Seconds to prevent starting continuous blind movement\n", (double)stopIn/Second);
        needAnotherShortMove = true;
      }
      // Apply new movement
      if (movingDirection!=0) {
        // already moving
        // - cancel old end of movement timer
        MainLoop::currentMainLoop().cancelExecutionTicket(movingTicket);
      }
      // - start new timer
      movingTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&ShadowBehaviour::endMoving, this), stopIn);
      // - possibly update movement
      int dir = dist>0 ? 1 : (dist<0 ? -1 : 0);
      if (dir!=movingDirection) {
        // change of direction
        movingDirection = dir;
        // starts moving now
        startedMoving = MainLoop::now();
        // apply
        device.requestApplyingChannels(boost::bind(&ShadowBehaviour::beginMoving, this), false); // not dimming
        // remain in updatingMovement state until applied
        return;
      }
    }
    else {
      // position reached
      if (movingDirection!=0) {
        MainLoop::currentMainLoop().cancelExecutionTicket(movingTicket);
        movingDirection = 0;
        // actually stop
        endMoving();
        // remain in updatingMovement state until applied
        return;
      }
    }
  }
  // done
  updatingMovement = false;
}


void ShadowBehaviour::beginMoving()
{
  updatingMovement = false;
}


void ShadowBehaviour::endMoving()
{
  updatingMovement = true; // prevent further updates/applies until actually stopped
  movingTicket = 0;
  movingDirection = 0;
  LOG(LOG_INFO,"Blind movement for %s complete -> stopping now\n", movingAngle ? "angle" : "position");
  device.requestApplyingChannels(boost::bind(&ShadowBehaviour::endedMoving, this), false); // not dimming
}

#define POSITION_TO_ANGLE_DELAY (1*Second)
#define INTER_SHORT_MOVE_DELAY (200*MilliSecond)

void ShadowBehaviour::endedMoving()
{
  updatingMovement = false;
  // check for segmented short moves
  if (needAnotherShortMove) {
    // commit what we've done so far, but needs more
    LOG(LOG_INFO,"Blind %s movement not yet complete -> need another step\n", movingAngle ? "angle" : "position");
    if (movingAngle)
      angle->syncChannelValue(getAngle(true), true);
    else
      position->syncChannelValue(getPosition(true), true);
    // not complete yet, recalculate remaining movement
    MainLoop::currentMainLoop().executeOnce(boost::bind(&ShadowBehaviour::updateMovement, this), INTER_SHORT_MOVE_DELAY);
  }
  else if (!movingAngle) {
    // position transition done
    position->transitionStep(1);
    position->channelValueApplied();
    // angle always needs to be re-applied when position has changed
    LOG(LOG_INFO,"Blind position movement stopped -> adjusting angle now\n");
    angle->setNeedsApplying();
    MainLoop::currentMainLoop().executeOnce(boost::bind(&ShadowBehaviour::updateMovement, this), POSITION_TO_ANGLE_DELAY);
  }
  else {
    // angle transition done
    angle->transitionStep(1);
    angle->channelValueApplied();
    LOG(LOG_INFO,"Blind angle movement stopped -> done\n");
  }
}


void ShadowBehaviour::stopMovement()
{
  // stop any movement, NOW
  MainLoop::currentMainLoop().cancelExecutionTicket(movingTicket);
  // get current positions
  position->syncChannelValue(getPosition(false), true);
  angle->syncChannelValue(getAngle(false), true);
  movingDirection = 0;
  // make hardware stop moving
  device.requestApplyingChannels(NULL, false); // not dimming
}



double ShadowBehaviour::getPosition(bool aAlwaysCalculated)
{
  double pos;
  if (!aAlwaysCalculated && (movingDirection==0 || movingAngle)) {
    // position already reached, just return channel value (angle might still be moving)
    pos = position->getTransitionalValue();
  }
  else {
    // position in progress -> calculate
    MLMicroSeconds mt = MainLoop::now()-startedMoving; // moving time
    if (movingDirection>0) {
      pos = startingPosition + position->getMax()*mt/Second/openTime; // moving up (open)
    }
    else {
      pos = startingPosition - position->getMax()*mt/Second/closeTime; // moving down (close)
    }
  }
  // limit to range
  if (pos>position->getMax()) pos = position->getMax();
  else if (pos<0) pos=0;
  return pos;
}


double ShadowBehaviour::getAngle(bool aAlwaysCalculated)
{
  double ang;
  if (!aAlwaysCalculated && movingDirection==0) {
    // position and angle already reached, just return channel value
    ang = angle->getTransitionalValue();
  }
  else if (!movingAngle) {
    // not yet moving angle, current angle is max or min depending on current direction
    ang = movingDirection>0 ? angle->getMax() : 0; // fully open angle when moving up, fully closed otherwise
  }
  else {
    // moving angle in progress
    MLMicroSeconds mt = MainLoop::now()-startedMoving; // moving time
    if (movingDirection>0) {
      ang = startingPosition + angle->getMax()*mt/Second/angleOpenTime; // moving up (open)
    }
    else {
      ang = startingPosition - angle->getMax()*mt/Second/angleCloseTime; // moving down (close)
    }
  }
  // limit to range
  if (ang>angle->getMax()) ang = angle->getMax();
  else if (ang<0) ang=0;
  return ang;
}



#pragma mark - behaviour interaction with digitalSTROM system


#define AUTO_OFF_FADE_TIME (60*Second)
#define AUTO_OFF_FADE_STEPSIZE 5

// apply scene
bool ShadowBehaviour::applyScene(DsScenePtr aScene)
{
  // check special cases for shadow scenes
  ShadowScenePtr shadowScene = boost::dynamic_pointer_cast<ShadowScene>(aScene);
  if (shadowScene) {
    // any scene call cancels actions (and fade down)
    stopActions();
  } // if shadowScene
  // other type of scene, let base class handle it
  return inherited::applyScene(aScene);
}



void ShadowBehaviour::loadChannelsFromScene(DsScenePtr aScene)
{
  ShadowScenePtr shadowScene = boost::dynamic_pointer_cast<ShadowScene>(aScene);
  if (shadowScene) {
    // load position and angle from scene
    position->setChannelValueIfNotDontCare(shadowScene, shadowScene->value, 0, 0, true);
    angle->setChannelValueIfNotDontCare(shadowScene, shadowScene->angle, 0, 0, true);
  }
  else {
    // only if not shadow scene, use default loader
    inherited::loadChannelsFromScene(aScene);
  }
}


void ShadowBehaviour::saveChannelsToScene(DsScenePtr aScene)
{
  ShadowScenePtr shadowScene = boost::dynamic_pointer_cast<ShadowScene>(aScene);
  if (shadowScene) {
    // save position and angle to scene
    shadowScene->setRepVar(shadowScene->value, position->getChannelValue());
    shadowScene->setSceneValueFlags(position->getChannelIndex(), valueflags_dontCare, false);
    shadowScene->setRepVar(shadowScene->angle, angle->getChannelValue());
    shadowScene->setSceneValueFlags(angle->getChannelIndex(), valueflags_dontCare, false);
  }
  else {
    // only if not shadow scene, use default save
    inherited::saveChannelsToScene(aScene);
  }
}




void ShadowBehaviour::performSceneActions(DsScenePtr aScene, SimpleCB aDoneCB)
{
  // none of my effects, let inherited check
  inherited::performSceneActions(aScene, aDoneCB);
}


void ShadowBehaviour::stopActions()
{
  // stop
  stopMovement();
  // let inherited stop as well
  inherited::stopActions();
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







