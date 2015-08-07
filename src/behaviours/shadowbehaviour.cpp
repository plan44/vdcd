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

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 6

#include "shadowbehaviour.hpp"


using namespace p44;


#pragma mark - ShadowScene


ShadowScene::ShadowScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inherited(aSceneDeviceSettings, aSceneNo),
  angle(0)
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



#pragma mark - default shadow scene

void ShadowScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  // set the common simple scene defaults
  inherited::setDefaultSceneValues(aSceneNo);
  // Add special shadow behaviour
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
  // by default, angle is 0 and don'tCare
  angle = 0;
  ShadowBehaviourPtr shadowBehaviour = boost::dynamic_pointer_cast<ShadowBehaviour>(getOutputBehaviour());
  if (shadowBehaviour) {
    setSceneValueFlags(shadowBehaviour->angle->getChannelIndex(), valueflags_dontCare, true);
  }
  markClean(); // default values are always clean (but setSceneValueFlags sets dirty)
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

#define MIN_INTERRUPTABLE_MOVE_TIME (5*Second)
#define POSITION_TO_ANGLE_DELAY (1*Second)
#define INTER_SHORT_MOVE_DELAY (1*Second)


ShadowBehaviour::ShadowBehaviour(Device &aDevice) :
  inherited(aDevice),
  // hardware derived parameters
  shadowDeviceKind(shadowdevice_jalousie),
  minMoveTime(200*MilliSecond),
  maxShortMoveTime(0),
  minLongMoveTime(0),
  // persistent settings (defaults are MixWerk's)
  openTime(54),
  closeTime(51),
  angleOpenTime(1),
  angleCloseTime(1),
  // volatile state
  referenceTime(Never),
  movingTicket(0),
  blindState(blind_idle),
  movingUp(false),
  referencePosition(100), // assume fully open, at top
  referenceAngle(100) // at top means that angle is open as well
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


void ShadowBehaviour::setDeviceParams(ShadowDeviceKind aShadowDeviceKind, MLMicroSeconds aMinMoveTime, MLMicroSeconds aMaxShortMoveTime, MLMicroSeconds aMinLongMoveTime)
{
  shadowDeviceKind = aShadowDeviceKind;
  minMoveTime = aMinMoveTime;
  maxShortMoveTime = aMaxShortMoveTime;
  minLongMoveTime = aMinLongMoveTime;
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



#pragma mark - Blind Movement Sequencer


double ShadowBehaviour::getPosition()
{
  double pos = referencePosition;
  if (referenceTime!=Never) {
    // moving, interpolate current position
    MLMicroSeconds mt = MainLoop::now()-referenceTime; // moving time
    if (movingUp)
      pos += position->getMax()*mt/Second/openTime; // moving up (open)
    else
      pos -= position->getMax()*mt/Second/closeTime; // moving down (close)
  }
  // limit to range
  if (pos>position->getMax()) pos = position->getMax();
  else if (pos<0) pos=0;
  return pos;
}


double ShadowBehaviour::getAngle()
{
  double ang = referenceAngle;
  if (referenceTime!=Never) {
    // moving, interpolate current position
    MLMicroSeconds mt = MainLoop::now()-referenceTime; // moving time
    if (movingUp)
      ang += angle->getMax()*mt/Second/angleOpenTime; // moving up (open)
    else
      ang -= angle->getMax()*mt/Second/angleCloseTime; // moving down (close)
  }
  // limit to range
  if (ang>angle->getMax()) ang = angle->getMax();
  else if (ang<0) ang=0;
  return ang;
}


void ShadowBehaviour::moveTimerStart()
{
  referenceTime = MainLoop::now();
}


void ShadowBehaviour::moveTimerStop()
{
  referencePosition = getPosition();
  referenceAngle = getAngle();
  referenceTime = Never;
}


void ShadowBehaviour::syncBlindState()
{
  position->syncChannelValue(getPosition());
  angle->syncChannelValue(getAngle());
}


void ShadowBehaviour::applyBlindChannels(MovementChangeCB aMovementCB, SimpleCB aApplyDoneCB, bool aForDimming)
{
  FOCUSLOG("Initiating blind moving sequence\n");
  movementCB = aMovementCB;
  if (blindState!=blind_idle) {
    // not idle
    if (aForDimming && blindState==blind_positioning) {
      // dimming requested while in progress of positioning
      // -> don't stop, just re-calculate position and timing
      blindState = blind_dimming;
      stopped(aApplyDoneCB);
      return;
    }
    // normal operation: stop first
    if (blindState==blind_stopping) {
      // already stopping, just make sure we'll apply afterwards
      blindState = blind_stopping_before_apply;
    }
    else {
      // something in progress, stop now
      blindState = blind_stopping_before_apply;
      stop(aApplyDoneCB);
    }
  }
  else {
    // can start right away
    applyPosition(aApplyDoneCB);
  }
}


void ShadowBehaviour::dimBlind(MovementChangeCB aMovementCB, DsDimMode aDimMode)
{
  FOCUSLOG("dimBlind called for %s\n", aDimMode==dimmode_up ? "UP" : (aDimMode==dimmode_down ? "DOWN" : "STOP"));
  if (aDimMode==dimmode_stop) {
    // simply stop
    movementCB = aMovementCB; // install new
    stop(NULL);
  }
  else {
    if (movementCB) {
      // already running - just consider stopped to sample current positions
      blindState = blind_idle;
      stopped(NULL);
    }
    // install new callback (likely same as before, if any)
    movementCB = aMovementCB;
    // prepare moving
    MLMicroSeconds stopIn;
    if (aDimMode==dimmode_up) {
      movingUp = true;
      stopIn = openTime*Second*1.2; // max movement = fully up
    }
    else {
      movingUp = false;
      stopIn = closeTime*Second*1.2; // max movement = fully down
    }
    // start moving
    blindState = blind_dimming;
    startMoving(stopIn, NULL);
  }
}




void ShadowBehaviour::stop(SimpleCB aApplyDoneCB)
{
  if (movementCB) {
    if (blindState==blind_positioning) {
      // if stopping after positioning, we might need to apply the angle afterwards
      blindState = blind_stopping_before_turning;
    }
    else if (blindState!=blind_stopping_before_apply) {
      // normal stop, unless this is a stop caused by a request to apply new values afterwards
      blindState = blind_stopping;
    }
    LOG(LOG_INFO,"Stopping all movement%s\n", blindState==blind_stopping_before_apply ? " before applying" : "");
    MainLoop::currentMainLoop().cancelExecutionTicket(movingTicket);
    movementCB(boost::bind(&ShadowBehaviour::stopped, this, aApplyDoneCB), 0);
  }
  else {
    // no movement sequence in progress
    blindState = blind_idle; // just to make sure
    if (aApplyDoneCB) aApplyDoneCB();
  }
}



void ShadowBehaviour::stopped(SimpleCB aApplyDoneCB)
{
  moveTimerStop();
  FOCUSLOG("- calculated current blind position=%.1f%%, angle=%.1f\n", referencePosition, referenceAngle);
  // next step depends on state
  switch (blindState) {
    case blind_stopping_before_apply:
      // now idle
      blindState = blind_idle;
      // continue with positioning
    case blind_dimming:
      // just apply new position (dimming case, move still running)
      applyPosition(aApplyDoneCB);
      break;
    case blind_stopping_before_turning:
      // after blind movement, always re-apply angle
      MainLoop::currentMainLoop().executeOnce(boost::bind(&ShadowBehaviour::applyAngle, this, aApplyDoneCB), POSITION_TO_ANGLE_DELAY);
      break;
    default:
      // end of sequence
      // - confirm apply and update actual values
      position->channelValueApplied();
      angle->channelValueApplied();
      position->syncChannelValue(getPosition());
      angle->syncChannelValue(getAngle());
      // - done
      allDone(aApplyDoneCB);
      break;
  }
}


void ShadowBehaviour::allDone(SimpleCB aApplyDoneCB)
{
  moveTimerStop();
  movementCB = NULL;
  blindState = blind_idle;
  LOG(LOG_INFO,"End of movement sequence, reached position=%.1f%%, angle=%.1f\n", referencePosition, referenceAngle);
  if (aApplyDoneCB) aApplyDoneCB();
}



void ShadowBehaviour::applyPosition(SimpleCB aApplyDoneCB)
{
  // decide what to do next
  if (position->needsApplying()) {
    FOCUSLOG("- starting position apply: %.1f -> %.1f\n", referencePosition, position->getChannelValue());
    // set new position
    targetPosition = position->getChannelValue();
    // as position changes angle, make sure we have a valid target angle (even in case it is not marked needsApplying() right now)
    targetAngle = angle->getChannelValue();
    // new position requested, calculate next move
    double dist = 0;
    MLMicroSeconds stopIn = 0;
    // full up or down always schedule full way to synchronize
    if (targetPosition>=100) {
      // fully up, always do full cycle to synchronize position
      dist = 120; // 20% extra to fully run into end switch
    }
    else if (targetPosition<=0) {
      // fully down, always do full cycle to synchronize position
      dist = -120; // 20% extra to fully run into end switch
    }
    else {
      // somewhere in between, actually estimate distance
      dist = targetPosition-referencePosition; // distance to move up
    }
    // calculate moving time
    if (dist>0) {
      // we'll move up
      movingUp = true;
      stopIn = openTime*Second/100.0*dist;
      // we only want moves which result in a defined angle -> stretch when needed
      if (stopIn<angleOpenTime)
        stopIn = angleOpenTime;
    }
    else if (dist<0) {
      // we'll move down
      movingUp = false;
      stopIn = closeTime*Second/100.0*-dist;
      // we only want moves which result in a defined angle -> stretch when needed
      if (stopIn<angleCloseTime)
        stopIn = angleCloseTime;
    }
    LOG(LOG_INFO,
      "Blind position=%.1f%% requested, current=%.1f%% -> moving %s for %.3f Seconds\n",
      targetPosition, referencePosition, dist>0 ? "up" : "down", (double)stopIn/Second
    );
    // start moving position if not already moving (dimming case)
    if (blindState!=blind_positioning) {
      blindState = blind_positioning;
      startMoving(stopIn, aApplyDoneCB);
    }
  }
  else {
    // position already ok: only if angle has to change, we'll have to do anything at all
    if (angle->needsApplying()) {
      // apply angle separately
      targetAngle = angle->getChannelValue();
      applyAngle(aApplyDoneCB);
    }
    else {
      // nothing to do at all, confirm done
      allDone(aApplyDoneCB);
    }
  }
}


void ShadowBehaviour::applyAngle(SimpleCB aApplyDoneCB)
{
  // determine current angle (100 = fully open)
  if (shadowDeviceKind!=shadowdevice_jalousie) {
    // ignore angle, just consider done
    allDone(aApplyDoneCB);
  }
  else if (getPosition()>=100) {
    // blind is fully up, angle is irrelevant -> consider applied
    referenceAngle = targetAngle;
    angle->channelValueApplied();
    allDone(aApplyDoneCB);
  }
  else {
    FOCUSLOG("- starting angle apply: %.1f -> %.1f\n", referenceAngle, targetAngle);
    double dist = targetAngle-referenceAngle; // distance to move up
    MLMicroSeconds stopIn = 0;
    // calculate new stop time
    if (dist>0) {
      movingUp = true;
      stopIn = angleOpenTime*Second/100.0*dist; // up
    }
    else if (dist<0) {
      movingUp = false;
      stopIn = angleCloseTime*Second/100.0*-dist; // down
    }
    // For full opened or closed, add 20% to make sure we're in sync
    if (targetAngle>=100 || targetAngle<=0) stopIn *= 1.2;
    LOG(LOG_INFO,
      "Blind angle=%.1f%% requested, current=%.1f%% -> moving %s for %.3f Seconds\n",
      targetAngle, referenceAngle, dist>0 ? "up" : "down", (double)stopIn/Second
    );
    // start moving angle
    blindState = blind_turning;
    startMoving(stopIn, aApplyDoneCB);
  }
}



void ShadowBehaviour::startMoving(MLMicroSeconds aStopIn, SimpleCB aApplyDoneCB)
{
  // determine direction
  int dir = movingUp ? 1 : -1;
  // check if we can do the move in one part
  if (aStopIn<minMoveTime) {
    // end of this move
    if (blindState==blind_positioning)
      blindState = blind_stopping_before_turning;
    stopped(aApplyDoneCB);
    return;
  }
  // actually start moving
  FOCUSLOG("- start moving into direction = %d\n", dir);
  movementCB(boost::bind(&ShadowBehaviour::moveStarted, this, aStopIn, aApplyDoneCB), dir);
}


void ShadowBehaviour::moveStarted(MLMicroSeconds aStopIn, SimpleCB aApplyDoneCB)
{
  // started
  moveTimerStart();
  MLMicroSeconds remaining = aStopIn;
  if (maxShortMoveTime>0 && aStopIn<minLongMoveTime && aStopIn>maxShortMoveTime) {
    // need multiple shorter segments
    if (remaining<2*minLongMoveTime && remaining>2*minMoveTime) {
      // evenly divide
      remaining /= 2;
      aStopIn = remaining;
    }
    else {
      // reduce by max short time move and carry over rest
      aStopIn = maxShortMoveTime;
      remaining-=aStopIn;
    }
    FOCUSLOG("- must restrict to %.3f Seconds now (%.3f later) to prevent starting continuous blind movement\n", (double)aStopIn/Second, (double)remaining/Second);
  }
  else {
    remaining = 0;
  }
  if (aStopIn>MIN_INTERRUPTABLE_MOVE_TIME) {
    // this is a long move, allow interrupting it
    // - which means that we confirm applied now
    FOCUSLOG("- is long move, should be interruptable -> confirming applied now\n");
    if (aApplyDoneCB) aApplyDoneCB();
    // - and prevent calling back again later
    aApplyDoneCB = NULL;
  }
  FOCUSLOG("- move started, scheduling stop in %.3f Seconds\n", (double)aStopIn/Second);
  movingTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&ShadowBehaviour::endMove, this, remaining, aApplyDoneCB), aStopIn);
}


void ShadowBehaviour::endMove(MLMicroSeconds aRemainingMoveTime, SimpleCB aApplyDoneCB)
{
  if (aRemainingMoveTime<=0) {
    // move is complete, regular stop
    stop(aApplyDoneCB);
  }
  else {
    // move is segmented, needs pause now and restart later
    // - stop (=start pause)
    FOCUSLOG("- end of move segment, pause now\n");
    movementCB(boost::bind(&ShadowBehaviour::movePaused, this, aRemainingMoveTime, aApplyDoneCB), 0);
  }
}


void ShadowBehaviour::movePaused(MLMicroSeconds aRemainingMoveTime, SimpleCB aApplyDoneCB)
{
  // paused, restart afterwards
  FOCUSLOG("- move paused, waiting to start next segment\n");
  // must update reference values between segments as well, otherwise estimate will include pause
  moveTimerStop();
  // schedule next segment
  MainLoop::currentMainLoop().executeOnce(boost::bind(&ShadowBehaviour::startMoving, this, aRemainingMoveTime, aApplyDoneCB), INTER_SHORT_MOVE_DELAY);
}


#pragma mark - behaviour interaction with digitalSTROM system


// apply scene
bool ShadowBehaviour::applyScene(DsScenePtr aScene)
{
  // check special cases for shadow scenes
  ShadowScenePtr shadowScene = boost::dynamic_pointer_cast<ShadowScene>(aScene);
  if (shadowScene) {
    // any scene call cancels actions (and fade down)
    stopActions();
  } // if shadowScene
  // otherwise, let base class handle it
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
  stop(NULL);
  // let inherited stop as well
  inherited::stopActions();
}


#define IDENTITY_MOVE_TIME (Second*1.5)

void ShadowBehaviour::identifyToUser()
{
  DsDimMode dimMode = position->getChannelValue()>50 ? dimmode_down : dimmode_up;
  // move a little
  device.dimChannelForArea(channeltype_default, dimMode, -1, IDENTITY_MOVE_TIME);
  MainLoop::currentMainLoop().executeOnce(boost::bind(&ShadowBehaviour::reverseIdentify, this, dimMode==dimmode_up ? dimmode_down : dimmode_up), IDENTITY_MOVE_TIME*2);
}


void ShadowBehaviour::reverseIdentify(DsDimMode aDimMode)
{
  device.dimChannelForArea(channeltype_default, aDimMode, -1, IDENTITY_MOVE_TIME);
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







