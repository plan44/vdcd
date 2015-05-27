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

#include "movinglightbehaviour.hpp"


using namespace p44;


#pragma mark - MovingLightScene


MovingLightScene::MovingLightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inherited(aSceneDeviceSettings, aSceneNo)
{
}


#pragma mark - color scene values/channels


double MovingLightScene::sceneValue(size_t aChannelIndex)
{
  ChannelBehaviourPtr cb = getDevice().getChannelByIndex(aChannelIndex);
  switch (cb->getChannelType()) {
    case channeltype_position_h : return hPos;
    case channeltype_position_v : return vPos;
    default: return inherited::sceneValue(aChannelIndex);
  }
  return 0;
}


void MovingLightScene::setSceneValue(size_t aChannelIndex, double aValue)
{
  ChannelBehaviourPtr cb = getDevice().getChannelByIndex(aChannelIndex);
  switch (cb->getChannelType()) {
    case channeltype_position_h : hPos = aValue;
    case channeltype_position_v : vPos = aValue;
    default: inherited::setSceneValue(aChannelIndex, aValue); break;
  }
}


#pragma mark - Color Light Scene persistence

const char *MovingLightScene::tableName()
{
  return "MovingLightScenes";
}

// data field definitions

static const size_t numMovingLightSceneFields = 2;

size_t MovingLightScene::numFieldDefs()
{
  return inherited::numFieldDefs()+numMovingLightSceneFields;
}


const FieldDefinition *MovingLightScene::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numMovingLightSceneFields] = {
    { "hPos", SQLITE_FLOAT },
    { "vPos", SQLITE_FLOAT }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numMovingLightSceneFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void MovingLightScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the fields
  hPos = aRow->get<double>(aIndex++);
  vPos = aRow->get<double>(aIndex++);
}


/// bind values to passed statement
void MovingLightScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, hPos);
  aStatement.bind(aIndex++, vPos);
}



#pragma mark - default moving light scene

void MovingLightScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  // set the common moving light scene defaults
  inherited::setDefaultSceneValues(aSceneNo);
  // No default value for position
  hPos = 0;
  vPos = 0;
  // set dontcare flags for position by default
  MovingLightBehaviourPtr movingLightBehaviour = boost::dynamic_pointer_cast<MovingLightBehaviour>(getOutputBehaviour());
  if (movingLightBehaviour) {
    setSceneValueFlags(movingLightBehaviour->horizontalPosition->getChannelIndex(), valueflags_dontCare, true);
    setSceneValueFlags(movingLightBehaviour->verticalPosition->getChannelIndex(), valueflags_dontCare, true);
  }
}


#pragma mark - MovingLightDeviceSettings with default light scenes factory


MovingLightDeviceSettings::MovingLightDeviceSettings(Device &aDevice) :
  inherited(aDevice)
{
};


DsScenePtr MovingLightDeviceSettings::newDefaultScene(SceneNo aSceneNo)
{
  MovingLightScenePtr movingLightScene = MovingLightScenePtr(new MovingLightScene(*this, aSceneNo));
  movingLightScene->setDefaultSceneValues(aSceneNo);
  // return it
  return movingLightScene;
}



#pragma mark - MovingLightBehaviour


MovingLightBehaviour::MovingLightBehaviour(Device &aDevice) :
  inherited(aDevice)
{
  // Create and add auxiliary channels to the device for horizontal and vertical position
  // - horizontal position
  horizontalPosition = ChannelBehaviourPtr(new HPosChannel(*this));
  addChannel(horizontalPosition);
  // - vertical position
  verticalPosition = ChannelBehaviourPtr(new VPosChannel(*this));
  addChannel(verticalPosition);
}


Tristate MovingLightBehaviour::hasModelFeature(DsModelFeatures aFeatureIndex)
{
  // now check for light behaviour level features
  switch (aFeatureIndex) {
//    case modelFeature_positionControls: //%%% does not exist yet...
//      // Assumption: all moving light output devices need a UI for position
//      return yes;
    default:
      // not available at this level, ask base class
      return inherited::hasModelFeature(aFeatureIndex);
  }
}



void MovingLightBehaviour::loadChannelsFromScene(DsScenePtr aScene)
{
  // load color light scene info
  inherited::loadChannelsFromScene(aScene);
  // now load moving light specific scene information
  MovingLightScenePtr movingLightScene = boost::dynamic_pointer_cast<MovingLightScene>(aScene);
  if (movingLightScene) {
    MLMicroSeconds ttUp = transitionTimeFromSceneEffect(movingLightScene->effect, true);
    MLMicroSeconds ttDown = transitionTimeFromSceneEffect(movingLightScene->effect, false);
    // prepare next position values in channels
    horizontalPosition->setChannelValueIfNotDontCare(movingLightScene, movingLightScene->hPos, ttUp, ttDown, true);
    verticalPosition->setChannelValueIfNotDontCare(movingLightScene, movingLightScene->vPos, ttUp, ttDown, true);
  }
}


void MovingLightBehaviour::saveChannelsToScene(DsScenePtr aScene)
{
  // save basic light scene info
  inherited::saveChannelsToScene(aScene);
  // now save moving light specific scene information
  MovingLightScenePtr movingLightScene = boost::dynamic_pointer_cast<MovingLightScene>(aScene);
  if (movingLightScene) {
    // when saved, position values are no longer dontcare
    movingLightScene->setSceneValueFlags(horizontalPosition->getChannelIndex(), valueflags_dontCare, false);
    movingLightScene->setRepVar(movingLightScene->hPos, horizontalPosition->getChannelValue());
    movingLightScene->setSceneValueFlags(verticalPosition->getChannelIndex(), valueflags_dontCare, false);
    movingLightScene->setRepVar(movingLightScene->vPos, verticalPosition->getChannelValue());
  }
}



void MovingLightBehaviour::positionTransitionStep(double aStepSize)
{
  horizontalPosition->transitionStep(aStepSize);
  verticalPosition->transitionStep(aStepSize);
}



void MovingLightBehaviour::appliedPosition()
{
  horizontalPosition->channelValueApplied();
  verticalPosition->channelValueApplied();
}



#pragma mark - description/shortDesc


string MovingLightBehaviour::shortDesc()
{
  return string("MovingColorLight");
}


