//
//  Copyright (c) 2013-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
#include <math.h>

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
    case CLIMATE_ENABLE:
      sceneCmd = scene_cmd_climatecontrol_enable;
      sceneArea = 0; // not an area scene any more
      break;
    case CLIMATE_DISABLE:
      sceneCmd = scene_cmd_climatecontrol_disable;
      sceneArea = 0; // not an area scene any more
      break;
    case CLIMATE_VALVE_PROPHYLAXIS:
      sceneCmd = scene_cmd_climatecontrol_valve_prophylaxis;
      sceneArea = 0; // not an area scene any more
      break;
    default:
      break;
  }
  markClean(); // default values are always clean
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
  heatingSystemCapability(hscapability_heatingAndCooling), // assume valve can handle both negative and positive values (even if only by applying absolute value to valve)
  climateControlIdle(false), // assume valve active
  runProphylaxis(false) // no run scheduled
{
  // make it member of the room temperature control group by default
  setGroupMembership(group_roomtemperature_control, true);
  // add the output channel
  // TODO: do we have a proper channel type for this?
  ChannelBehaviourPtr ch = ChannelBehaviourPtr(new HeatingLevelChannel(*this));
  addChannel(ch);
}



bool ClimateControlBehaviour::processControlValue(const string &aName, double aValue)
{
  if (aName=="heatingLevel") {
    if (isMember(group_roomtemperature_control) && isEnabled()) {
      ChannelBehaviourPtr cb = getChannelByType(channeltype_default);
      if (cb) {
        // clip to -100..0..100 range
        if (aValue<-100) aValue = -100;
        else if (aValue>100) aValue = 100;
        // limit according to heatingSystemCapability setting
        switch (heatingSystemCapability) {
          case hscapability_heatingOnly:
            // 0..100
            if (aValue<0) aValue = 0; // ignore negatives
            break;
          case hscapability_coolingOnly:
            // -100..0
            if (aValue>0) aValue = 0; // ignore positives
            break;
          default:
          case hscapability_heatingAndCooling:
            // pass all values
            break;
        }
        // adapt to hardware capabilities
        if (outputFunction!=outputFunction_bipolar_positional) {
          // non-bipolar valves can only handle positive values, even for cooling
          aValue = fabs(aValue);
        }
        // apply now
        cb->setChannelValue(aValue, 0, true); // always apply
        return true; // needs apply
      }
    }
  }
  return inherited::processControlValue(aName, aValue);
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
      case scene_cmd_climatecontrol_enable:
        // switch to winter mode
        climateControlIdle = false;
        return true;
      case scene_cmd_climatecontrol_disable:
        // switch to summer mode
        climateControlIdle = true;
        return true;
      case scene_cmd_climatecontrol_valve_prophylaxis:
        // valve prophylaxis
        runProphylaxis = true;
        return true;
      default:
        // all other scene calls are suppressed in group_roomtemperature_control
        return false;
    }
  }
  // other type of scene, let base class handle it
  return inherited::applyScene(aScene);
}


#pragma mark - persistence


const char *ClimateControlBehaviour::tableName()
{
  return "ClimateOutputSettings";
}


// data field definitions

static const size_t numFields = 1;

size_t ClimateControlBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *ClimateControlBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "heatingSystemCapability", SQLITE_INTEGER },
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void ClimateControlBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  // get the data
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // decode the common flags
  if (aCommonFlagsP) climateControlIdle = *aCommonFlagsP & outputflag_climateControlIdle;
  // get the fields
  heatingSystemCapability = (DsHeatingSystemCapability)aRow->get<int>(aIndex++);
}


// bind values to passed statement
void ClimateControlBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  // encode the flags
  if (climateControlIdle) aCommonFlags |= outputflag_climateControlIdle;
  // bind
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, heatingSystemCapability);
}


#pragma mark - property access


static char climatecontrol_key;

// settings properties

enum {
  heatingSystemCapability_key,
  numSettingsProperties
};


int ClimateControlBehaviour::numSettingsProps() { return inherited::numSettingsProps()+numSettingsProperties; }
const PropertyDescriptorPtr ClimateControlBehaviour::getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numSettingsProperties] = {
    { "heatingSystemCapability", apivalue_uint64, heatingSystemCapability_key+settings_key_offset, OKEY(climatecontrol_key) },
  };
  int n = inherited::numSettingsProps();
  if (aPropIndex<n)
    return inherited::getSettingsDescriptorByIndex(aPropIndex, aParentDescriptor);
  aPropIndex -= n;
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


// access to all fields

bool ClimateControlBehaviour::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(climatecontrol_key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Settings properties
        case heatingSystemCapability_key+settings_key_offset: aPropValue->setUint8Value(heatingSystemCapability); return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Settings properties
        case heatingSystemCapability_key+settings_key_offset: setPVar(heatingSystemCapability, (DsHeatingSystemCapability)aPropValue->uint8Value()); return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}




#pragma mark - description


string ClimateControlBehaviour::shortDesc()
{
  return string("ClimateControl");
}


string ClimateControlBehaviour::description()
{
  string s = string_format("%s behaviour (in %s mode)", shortDesc().c_str(), isClimateControlIdle() ? "idle" : "active");
  s.append(inherited::description());
  return s;
}

