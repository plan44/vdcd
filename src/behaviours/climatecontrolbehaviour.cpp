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

#include "climatecontrolbehaviour.hpp"

using namespace p44;


#pragma mark - ClimateControlScene


ClimateControlScene::ClimateControlScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inherited(aSceneDeviceSettings, aSceneNo)
{
}


void ClimateControlScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  // set the common simple scene defaults
  inherited::setDefaultSceneValues(aSceneNo);
  // Add special climate behaviour scene commands
  switch (aSceneNo) {
    case CLIMATE_WINTER:
      sceneCmd = scene_cmd_heating_winter_mode;
      break;
    case CLIMATE_SUMMER:
      sceneCmd = scene_cmd_heating_summer_mode;
      break;
    case CLIMATE_VALVE_PROPHYLAXIS:
      sceneCmd = scene_cmd_heating_valve_prophylaxis;
      break;
    default:
      break;
  }
}


#pragma mark - ShadowDeviceSettings with default shadow scenes factory


ClimateDeviceSettings::ClimateDeviceSettings(Device &aDevice) :
  inherited(aDevice)
{
}


DsScenePtr ClimateDeviceSettings::newDefaultScene(SceneNo aSceneNo)
{
  ClimateControlScenePtr climateControlScene = ClimateControlScenePtr(new ClimateControlScene(*this, aSceneNo));
  climateControlScene->setDefaultSceneValues(aSceneNo);
  // return it
  return climateControlScene;
}



#pragma mark - ClimateControlBehaviour


ClimateControlBehaviour::ClimateControlBehaviour(Device &aDevice) :
  inherited(aDevice),
  summerMode(false), // assume valve active
  runProphylaxis(false) // no run scheduled
{
  // make it member of the room temperature control group by default
  setGroupMembership(group_roomtemperature_control, true);
  // add the output channel
  // TODO: do we have a proper channel type for this?
  ChannelBehaviourPtr ch = ChannelBehaviourPtr(new HeatingLevelChannel(*this));
  addChannel(ch);
}



void ClimateControlBehaviour::processControlValue(const string &aName, double aValue)
{
  if (aName=="heatingLevel") {
    if (isMember(group_roomtemperature_control) && outputMode!=outputmode_disabled) {
      // apply positive values to (default) valve output, clip to 100 max
      ChannelBehaviourPtr cb = getChannelByType(channeltype_default);
      if (cb) {
        cb->setChannelValue(aValue<0 ? 0 : (aValue>100 ? 100 : aValue), 0, true); // always apply
      }
    }
  }
}


Tristate ClimateControlBehaviour::hasModelFeature(DsModelFeatures aFeatureIndex)
{
  // now check for climate control behaviour level features
  switch (aFeatureIndex) {
    case modelFeature_blink:
      // heating outputs can't blink
      return no;
    case modelFeature_heatinggroup:
      // Assumption: virtual heating control devices (valves) do have group and mode setting...
      return yes;
    case modelFeature_heatingoutmode:
      // ...but not the more specific PWM and heating props
      return no;
    case modelFeature_valvetype:
      // for now, all climate control devices are heating valves
      return yes;
    default:
      // not available at this level, ask base class
      return inherited::hasModelFeature(aFeatureIndex);
  }
}


// apply scene
// - execute special climate commands
bool ClimateControlBehaviour::applyScene(DsScenePtr aScene)
{
  // check the special hardwired scenes
  if (isMember(group_roomtemperature_control)) {
    SceneCmd sceneCmd = aScene->sceneCmd;
    switch (sceneCmd) {
      case scene_cmd_heating_winter_mode:
        // switch to winter mode
        summerMode = false;
        return true;
      case scene_cmd_heating_summer_mode:
        // switch to summer mode
        summerMode = true;
        return true;
      case scene_cmd_heating_valve_prophylaxis:
        // valve prophylaxis
        runProphylaxis = true;
        return true;
      default:
        break;
    }
  }
  // other type of scene, let base class handle it
  return inherited::applyScene(aScene);
}


#pragma mark - persistence

/// load values from passed row
void ClimateControlBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  // get the data
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // decode the flags
  if (aCommonFlagsP) summerMode = *aCommonFlagsP & outputflag_summerMode;
}


// bind values to passed statement
void ClimateControlBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  // encode the flags
  if (summerMode) aCommonFlags |= outputflag_summerMode;
  // bind
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
}



string ClimateControlBehaviour::shortDesc()
{
  return string("ClimateControl");
}


string ClimateControlBehaviour::description()
{
  string s = string_format("%s behaviour (in %smode)\n", shortDesc().c_str(), isSummerMode() ? "summer" : "winter");
  s.append(inherited::description());
  return s;
}

