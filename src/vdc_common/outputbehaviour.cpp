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

#include "outputbehaviour.hpp"

using namespace p44;


OutputBehaviour::OutputBehaviour(Device &aDevice) :
  inherited(aDevice),
  // persistent settings
  outputMode(outputmode_disabled), // none by default, hardware should set a default matching the actual HW capabilities
  outputGroups(1<<group_variable), // all devices are in group 0 by default
  pushChanges(false) // do not push changes
{
  // set default hardware default configuration
  setHardwareOutputConfig(outputFunction_switch, usage_undefined, false, -1);
}


void OutputBehaviour::setHardwareOutputConfig(DsOutputFunction aOutputFunction, DsUsageHint aUsage, bool aVariableRamp, double aMaxPower)
{
  outputFunction = aOutputFunction;
  outputUsage = aUsage;
  variableRamp = aVariableRamp;
  maxPower = aMaxPower;
  // determine default output mode
  switch (outputFunction) {
    case outputFunction_switch:
      outputMode = outputmode_binary;
      break;
    case outputFunction_dimmer:
    case outputFunction_positional:
      outputMode = outputmode_gradual;
      break;
    default:
      outputMode = outputmode_disabled;
      break;
  }
}


void OutputBehaviour::addChannel(ChannelBehaviourPtr aChannel)
{
  aChannel->channelIndex = channels.size();
  channels.push_back(aChannel);
}


size_t OutputBehaviour::numChannels()
{
  return channels.size();
}



ChannelBehaviourPtr OutputBehaviour::getChannelByIndex(size_t aChannelIndex, bool aPendingApplyOnly)
{
  if (aChannelIndex<channels.size()) {
    ChannelBehaviourPtr ch = channels[aChannelIndex];
    if (!aPendingApplyOnly || ch->needsApplying())
      return ch;
    // found but has no apply pending -> return no channel
  }
  return ChannelBehaviourPtr();
}


ChannelBehaviourPtr OutputBehaviour::getChannelByType(DsChannelType aChannelType, bool aPendingApplyOnly)
{
  if (aChannelType==channeltype_default)
    return getChannelByIndex(0, aPendingApplyOnly); // first channel is primary/default channel by internal convention
  // look for channel with matching type
  for (ChannelBehaviourVector::iterator pos = channels.begin(); pos!=channels.end(); ++pos) {
    if ((*pos)->getChannelType()==aChannelType) {
      if (!aPendingApplyOnly || (*pos)->needsApplying())
        return *pos; // found
      break; // found but has no apply pending -> return no channel
    }
  }
  return ChannelBehaviourPtr();
}



bool OutputBehaviour::isMember(DsGroup aGroup)
{
  return
    (aGroup==device.getPrimaryGroup()) || // is always member of primary group
    ((outputGroups & 0x1ll<<aGroup)!=0); // explicit extra membership flag set
}


void OutputBehaviour::setGroupMembership(DsGroup aGroup, bool aIsMember)
{
  DsGroupMask newGroups = outputGroups;
  if (aIsMember) {
    // make explicitly member of a group
    newGroups |= (0x1ll<<aGroup);
  }
  else {
    // not explicitly member
    newGroups &= ~(0x1ll<<aGroup);
  }
  if (newGroups!=outputGroups) {
    outputGroups = newGroups;
    markDirty();
  }
}




#pragma mark - persistence implementation


// SQLIte3 table name to store these parameters to
const char *OutputBehaviour::tableName()
{
  return "OutputSettings";
}


// data field definitions

static const size_t numFields = 3;

size_t OutputBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *OutputBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "outputMode", SQLITE_INTEGER },
    { "outputFlags", SQLITE_INTEGER },
    { "outputGroups", SQLITE_INTEGER }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


enum {
  outputflag_pushChanges = 0x0001,
};

/// load values from passed row
void OutputBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  outputMode = (DsOutputMode)aRow->get<int>(aIndex++);
  int flags = aRow->get<int>(aIndex++);
  // decode the flags
  pushChanges = flags & outputflag_pushChanges;
}


// bind values to passed statement
void OutputBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // encode the flags
  int flags = 0;
  if (pushChanges) flags |= outputflag_pushChanges;
  // bind the fields
  aStatement.bind(aIndex++, outputMode);
  aStatement.bind(aIndex++, flags);
}



#pragma mark - output property access

static char output_key;
static char output_groups_key;


// next level (groups)

int OutputBehaviour::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (aParentDescriptor->hasObjectKey(output_groups_key)) {
    return 64; // group mask has 64 bits for now
  }
  return inherited::numProps(aDomain, aParentDescriptor);
}


PropertyContainerPtr OutputBehaviour::getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain)
{
  if (aPropertyDescriptor->isArrayContainer() && aPropertyDescriptor->hasObjectKey(output_groups_key)) {
    return PropertyContainerPtr(this); // handle groups array myself
  }
  // unknown here
  return inherited::getContainer(aPropertyDescriptor, aDomain);
}


PropertyDescriptorPtr OutputBehaviour::getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (aParentDescriptor && aParentDescriptor->hasObjectKey(output_groups_key)) {
    // array-like container
    PropertyDescriptorPtr propDesc;
    bool numericName = getNextPropIndex(aPropMatch, aStartIndex);
    int n = numProps(aDomain, aParentDescriptor);
    if (aStartIndex!=PROPINDEX_NONE && aStartIndex<n) {
      // within range, create descriptor
      DynamicPropertyDescriptor *descP = new DynamicPropertyDescriptor(aParentDescriptor);
      descP->propertyName = string_format("%d", aStartIndex);
      descP->propertyType = aParentDescriptor->type();
      descP->propertyFieldKey = aStartIndex;
      descP->propertyObjectKey = aParentDescriptor->objectKey();
      propDesc = PropertyDescriptorPtr(descP);
      // advance index
      aStartIndex++;
    }
    if (aStartIndex>=n || numericName) {
      // no more descriptors OR specific descriptor accessed -> no "next" descriptor
      aStartIndex = PROPINDEX_NONE;
    }
    return propDesc;
  }
  // None of the containers within Device - let base class handle Device-Level properties
  return inherited::getDescriptorByName(aPropMatch, aStartIndex, aDomain, aParentDescriptor);
}




// description properties

enum {
  outputFunction_key,
  outputUsage_key,
  variableRamp_key,
  maxPower_key,
  numDescProperties
};


int OutputBehaviour::numDescProps() { return numDescProperties; }
const PropertyDescriptorPtr OutputBehaviour::getDescDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numDescProperties] = {
    { "function", apivalue_uint64, outputFunction_key+descriptions_key_offset, OKEY(output_key) },
    { "outputUsage", apivalue_uint64, outputUsage_key+descriptions_key_offset, OKEY(output_key) },
    { "variableRamp", apivalue_bool, variableRamp_key+descriptions_key_offset, OKEY(output_key) },
    { "maxPower", apivalue_double, maxPower_key+descriptions_key_offset, OKEY(output_key) },
  };
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


// settings properties

enum {
  mode_key,
  pushChanges_key,
  groups_key,
  numSettingsProperties
};


int OutputBehaviour::numSettingsProps() { return numSettingsProperties; }
const PropertyDescriptorPtr OutputBehaviour::getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numSettingsProperties] = {
    { "mode", apivalue_uint64, mode_key+settings_key_offset, OKEY(output_key) },
    { "pushChanges", apivalue_bool, pushChanges_key+settings_key_offset, OKEY(output_key) },
    { "groups", apivalue_bool+propflag_container, groups_key+settings_key_offset, OKEY(output_groups_key) }
  };
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


// state properties

enum {
  localPriority_key,
  numStateProperties
};


int OutputBehaviour::numStateProps() { return numStateProperties; }
const PropertyDescriptorPtr OutputBehaviour::getStateDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numStateProperties] = {
    { "localPriority", apivalue_bool, localPriority_key+states_key_offset, OKEY(output_key) }
  };
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}



// access to all fields

bool OutputBehaviour::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(output_groups_key)) {
    if (aMode==access_read) {
      // read group membership
      if (isMember((DsGroup)aPropertyDescriptor->fieldKey())) {
        aPropValue->setBoolValue(true);
        return true;
      }
      return false;
    }
    else {
      // write group
      setGroupMembership((DsGroup)aPropertyDescriptor->fieldKey(), aPropValue->boolValue());
      return true;
    }
  }
  else if (aPropertyDescriptor->hasObjectKey(output_key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Description properties
        case outputFunction_key+descriptions_key_offset:
          aPropValue->setUint8Value(outputFunction);
          return true;
        case outputUsage_key+descriptions_key_offset:
          aPropValue->setUint16Value(outputUsage);
          return true;
        case variableRamp_key+descriptions_key_offset:
          aPropValue->setBoolValue(variableRamp);
          return true;
        case maxPower_key+descriptions_key_offset:
          aPropValue->setDoubleValue(maxPower);
          return true;
        // Settings properties
        case mode_key+settings_key_offset:
          aPropValue->setUint8Value(outputMode);
          return true;
        case pushChanges_key+settings_key_offset:
          aPropValue->setBoolValue(pushChanges);
          return true;
        // State properties
        case localPriority_key+states_key_offset:
          aPropValue->setBoolValue(localPriority);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Settings properties
        case mode_key+settings_key_offset:
          outputMode = (DsOutputMode)aPropValue->int32Value();
          markDirty();
          return true;
        case pushChanges_key+settings_key_offset:
          pushChanges = aPropValue->boolValue();
          markDirty();
          return true;
        // State properties
        case localPriority_key+states_key_offset:
          localPriority = aPropValue->boolValue();
          markDirty();
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}



#pragma mark - description/shortDesc


string OutputBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- hardware output function: %d (%s)\n", outputFunction, outputFunction==outputFunction_dimmer ? "dimmer" : (outputFunction==outputFunction_switch ? "switch" : "other"));
  s.append(inherited::description());
  return s;
}

