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

#include "channelbehaviour.hpp"
#include "outputbehaviour.hpp"
#include "math.h"

using namespace p44;

#pragma mark - channel behaviour

ChannelBehaviour::ChannelBehaviour(OutputBehaviour &aOutput) :
  output(aOutput),
  channelUpdatePending(false), // no output update pending
  nextTransitionTime(0), // none
  channelLastSent(Never), // we don't known nor have we sent the output state
  cachedChannelValue(0), // channel output value cache
  resolution(1) // dummy default resolution (derived classes must provide sensible defaults)
{
}


void ChannelBehaviour::setResolution(double aResolution)
{
  resolution = aResolution;
}


bool ChannelBehaviour::isPrimary()
{
  // internal convention: first channel is the default channel
  return channelIndex==0;
}


string ChannelBehaviour::description()
{
  return string_format(
    "Channel '%s' (channelType=%d): min: %0.1f, max: %0.1f, resolution: %0.3f",
    getName(), (int)getChannelType(),
    getMin(), getMax(), getResolution()
  );
}



#pragma mark - channel value handling


// used at startup and before saving scenes to get the current value FROM the hardware
// NOT to be used to change the hardware channel value!
void ChannelBehaviour::syncChannelValue(double aActualChannelValue)
{
  LOG(LOG_INFO,
    "Channel '%s' in device %s: cached value synchronized to %0.2f\n",
    getName(), output.device.shortDesc().c_str(), aActualChannelValue
  );
  cachedChannelValue = aActualChannelValue;
  channelValueApplied(); // now we know that we are in sync
}



void ChannelBehaviour::setChannelValue(double aNewValue, MLMicroSeconds aTransitionTimeUp, MLMicroSeconds aTransitionTimeDown)
{
  setChannelValue(aNewValue, aNewValue>getChannelValue() ? aTransitionTimeUp : aTransitionTimeDown);
}


void ChannelBehaviour::setChannelValueIfNotDontCare(DsScenePtr aScene, double aNewValue, MLMicroSeconds aTransitionTimeUp, MLMicroSeconds aTransitionTimeDown)
{
  if (!(aScene->isSceneValueFlagSet(getChannelIndex(), valueflags_dontCare))) {
    setChannelValue(aNewValue, aNewValue>getChannelValue() ? aTransitionTimeUp : aTransitionTimeDown);
  }
}




void ChannelBehaviour::setChannelValue(double aNewValue, MLMicroSeconds aTransitionTime)
{
  // make sure new value is within bounds
  if (aNewValue>getMax())
    aNewValue = getMax();
  else if (aNewValue<getMin())
    aNewValue = getMin();
  // prevent propagating changes smaller than device resolution
  if (fabs(aNewValue-cachedChannelValue)>=getResolution()) {
    LOG(LOG_INFO,
      "Channel '%s' in device %s: is requested to apply new value %0.2f (transition time=%lld uS), last known value is %0.2f\n",
      getName(), output.device.shortDesc().c_str(), aNewValue, aTransitionTime, cachedChannelValue
    );
    // apply
    cachedChannelValue = aNewValue;
    nextTransitionTime = aTransitionTime;
    channelUpdatePending = true; // pending to be sent to the device
    channelLastSent = Never; // cachedChannelValue is no longer applied (does not correspond with actual hardware)
  }
}


void ChannelBehaviour::dimChannelValue(double aIncrement, MLMicroSeconds aTransitionTime)
{
  double newValue = cachedChannelValue+aIncrement;
  if (newValue<getMinDim())
    newValue = getMinDim();
  else if (newValue>getMax())
    newValue = getMax();
  // apply (silently)
  if (newValue!=cachedChannelValue) {
    cachedChannelValue = newValue;
    nextTransitionTime = aTransitionTime;
    channelUpdatePending = true; // pending to be sent to the device
    channelLastSent = Never; // cachedChannelValue is no longer applied (does not correspond with actual hardware)
  }
}



void ChannelBehaviour::channelValueApplied()
{
  channelUpdatePending = false; // applied
  channelLastSent = MainLoop::now(); // now we know that we are in sync
  LOG(LOG_INFO,
    "Channel '%s' in device %s: has applied new value %0.2f to hardware\n",
    getName(), output.device.shortDesc().c_str(), cachedChannelValue
  );
}



#pragma mark - channel property access

// Note: this is a simplified single class property access mechanims. ChannelBehaviour is not meant to be derived.

enum {
  name_key,
  channelIndex_key,
  min_key,
  max_key,
  resolution_key,
  numChannelDescProperties
};

enum {
  numChannelSettingsProperties
};

enum {
  value_key,
  age_key,
  numChannelStateProperties
};

static char channel_Key;


int ChannelBehaviour::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  switch (aParentDescriptor->parentDescriptor->fieldKey()) {
    case descriptions_key_offset: return numChannelDescProperties;
    case settings_key_offset: return numChannelSettingsProperties;
    case states_key_offset: return numChannelStateProperties;
    default: return 0;
  }
}


PropertyDescriptorPtr ChannelBehaviour::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription channelDescProperties[numChannelDescProperties] = {
    { "name", apivalue_string, name_key+descriptions_key_offset, OKEY(channel_Key) },
    { "channelIndex", apivalue_uint64, channelIndex_key+descriptions_key_offset, OKEY(channel_Key) },
    { "min", apivalue_double, min_key+descriptions_key_offset, OKEY(channel_Key) },
    { "max", apivalue_double, max_key+descriptions_key_offset, OKEY(channel_Key) },
    { "resolution", apivalue_double, resolution_key+descriptions_key_offset, OKEY(channel_Key) },
  };
  //static const PropertyDescription channelSettingsProperties[numChannelSettingsProperties] = {
  //};
  static const PropertyDescription channelStateProperties[numChannelStateProperties] = {
    { "value", apivalue_double, value_key+states_key_offset, OKEY(channel_Key) }, // note: so far, pbuf API requires uint here
    { "age", apivalue_double, age_key+states_key_offset, OKEY(channel_Key) },
  };
  if (aPropIndex>=numProps(aDomain, aParentDescriptor))
    return NULL;
  switch (aParentDescriptor->parentDescriptor->fieldKey()) {
    case descriptions_key_offset:
      return PropertyDescriptorPtr(new StaticPropertyDescriptor(&channelDescProperties[aPropIndex], aParentDescriptor));
      //case settings_key_offset:
      //  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&channelSettingsProperties[aPropIndex], aParentDescriptor));
    case states_key_offset:
      return PropertyDescriptorPtr(new StaticPropertyDescriptor(&channelStateProperties[aPropIndex], aParentDescriptor));
    default:
      return NULL;
  }
}


// access to all fields
bool ChannelBehaviour::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(channel_Key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Description properties
        case name_key+descriptions_key_offset:
          aPropValue->setStringValue(getName());
          return true;
        case channelIndex_key+descriptions_key_offset:
          aPropValue->setUint8Value(channelIndex);
          return true;
        case min_key+descriptions_key_offset:
          aPropValue->setDoubleValue(getMin());
          return true;
        case max_key+descriptions_key_offset:
          aPropValue->setDoubleValue(getMax());
          return true;
        case resolution_key+descriptions_key_offset:
          aPropValue->setDoubleValue(getResolution());
          return true;
        // Settings properties
        // - none for now
        // States properties
        case value_key+states_key_offset:
          aPropValue->setDoubleValue(getChannelValue());
          return true;
        case age_key+states_key_offset:
          if (channelLastSent==Never)
            aPropValue->setNull(); // no value known
          else
            aPropValue->setDoubleValue(((double)MainLoop::now()-channelLastSent)/Second);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Settings properties
        // - none for now
        // States properties
        case value_key+states_key_offset:
          setChannelValue(aPropValue->doubleValue());
          return true;
      }
    }
  }
  // single class level properties only, don't call inherited
  return false;
}

