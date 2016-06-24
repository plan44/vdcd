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

#include "channelbehaviour.hpp"
#include "outputbehaviour.hpp"
#include "math.h"

using namespace p44;

#pragma mark - channel behaviour

ChannelBehaviour::ChannelBehaviour(OutputBehaviour &aOutput) :
  output(aOutput),
  channelUpdatePending(false), // no output update pending
  nextTransitionTime(0), // none
  channelLastSync(Never), // we don't known nor have we sent the output state
  cachedChannelValue(0), // channel output value cache
  previousChannelValue(0), // previous output value
  transitionProgress(1), // no transition in progress
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


bool ChannelBehaviour::transitionStep(double aStepSize)
{
  if (aStepSize<=0) {
    transitionProgress = 0; // start
    return true; // in transition
  }
  if (inTransition()) {
    setTransitionProgress(transitionProgress+aStepSize);
    return true; // still in transition (at least until this step is actually applied)
  }
  // no longer in transition
  return false;
}


void ChannelBehaviour::setTransitionProgress(double aProgress)
{
  if (aProgress<0) aProgress = 0;
  // set
  transitionProgress = aProgress;
  if (transitionProgress>=1) {
    // transition complete
    transitionProgress=1;
    previousChannelValue = cachedChannelValue; // end of transition reached, old previous value is no longer needed
  }
}



void ChannelBehaviour::setTransitionValue(double aCurrentValue, bool aIsInitial)
{
  if (aIsInitial) {
    // initial value of transition (rather than previously known cached one)
    previousChannelValue = aCurrentValue;
    transitionProgress = 0; // start of transition
  }
  else {
    // intermediate value within transition
    double d = cachedChannelValue-previousChannelValue;
    setTransitionProgress(d==0 ? 1 : aCurrentValue/d-previousChannelValue);
  }
}




bool ChannelBehaviour::inTransition()
{
  return transitionProgress<1;
}


double ChannelBehaviour::getChannelValue()
{
  // current value is cached value
  return cachedChannelValue;
}


bool ChannelBehaviour::getChannelValueBool()
{
  return getChannelValue() >= (getMax()-getMin())/2;
}



double ChannelBehaviour::getTransitionalValue()
{
  if (inTransition()) {
    // calculate transitional value
    return previousChannelValue+transitionProgress*(cachedChannelValue-previousChannelValue);
  }
  else {
    // current value is cached value
    return cachedChannelValue;
  }
}


// used at startup and before saving scenes to get the current value FROM the hardware
// NOT to be used to change the hardware channel value!
void ChannelBehaviour::syncChannelValue(double aActualChannelValue, bool aAlwaysSync)
{
  if (!channelUpdatePending || aAlwaysSync) {
    if (cachedChannelValue!=aActualChannelValue || LOGENABLED(LOG_DEBUG)) {
      // show only changes except if debugging
      SALOG(output.device,LOG_INFO,
        "Channel '%s': cached value synchronized from %0.2f -> %0.2f",
        getName(), cachedChannelValue, aActualChannelValue
      );
    }
    // make sure new value is within bounds
    if (aActualChannelValue>getMax())
      cachedChannelValue = getMax();
    else if (aActualChannelValue<getMin())
      cachedChannelValue = getMin();
    else
      cachedChannelValue = aActualChannelValue;
    // reset transitions and pending updates
    previousChannelValue = cachedChannelValue;
    transitionProgress = 1; // not in transition
    channelUpdatePending = false; // we are in sync
    channelLastSync = MainLoop::now(); // value is current
  }
}


void ChannelBehaviour::syncChannelValueBool(bool aValue, bool aAlwaysSync)
{
  if (aValue!=getChannelValueBool()) {
    syncChannelValue(aValue ? getMax() : getMin(), aAlwaysSync);
  }
}




void ChannelBehaviour::setChannelValue(double aNewValue, MLMicroSeconds aTransitionTimeUp, MLMicroSeconds aTransitionTimeDown, bool aAlwaysApply)
{
  setChannelValue(aNewValue, aNewValue>getTransitionalValue() ? aTransitionTimeUp : aTransitionTimeDown, aAlwaysApply);
}


void ChannelBehaviour::setChannelValueIfNotDontCare(DsScenePtr aScene, double aNewValue, MLMicroSeconds aTransitionTimeUp, MLMicroSeconds aTransitionTimeDown, bool aAlwaysApply)
{
  if (!(aScene->isSceneValueFlagSet(getChannelIndex(), valueflags_dontCare))) {
    setChannelValue(aNewValue, aNewValue>getTransitionalValue() ? aTransitionTimeUp : aTransitionTimeDown, aAlwaysApply);
  }
}




void ChannelBehaviour::setChannelValue(double aNewValue, MLMicroSeconds aTransitionTime, bool aAlwaysApply)
{
  // make sure new value is within bounds
  if (aNewValue>getMax())
    aNewValue = getMax();
  else if (aNewValue<getMin())
    aNewValue = getMin();
  // prevent propagating changes smaller than device resolution, but always apply when transition is in progress
  if (aAlwaysApply || inTransition() || fabs(aNewValue-cachedChannelValue)>=getResolution()) {
    SALOG(output.device, LOG_INFO,
      "Channel '%s' is requested to change from %0.2f ->  %0.2f (transition time=%d mS)",
      getName(), cachedChannelValue, aNewValue, (int)(aTransitionTime/MilliSecond)
    );
    // setting new value captures current (possibly transitional) value as previous and completes transition
    previousChannelValue = channelLastSync!=Never ? getTransitionalValue() : aNewValue; // If there is no valid previous value, set current as previous.
    transitionProgress = 1; // consider done
    // save target parameters for next transition
    cachedChannelValue = aNewValue;
    nextTransitionTime = aTransitionTime;
    channelUpdatePending = true; // pending to be sent to the device
    channelLastSync = Never; // cachedChannelValue is no longer applied (does not correspond with actual hardware)
  }
}


double ChannelBehaviour::dimChannelValue(double aIncrement, MLMicroSeconds aTransitionTime)
{
  double newValue = cachedChannelValue+aIncrement;
  if (newValue<getMinDim()) {
    if (wrapsAround())
      newValue += getMax()-getMin(); // wrap backwards
    else
      newValue = getMinDim(); // just stay at min
  }
  else if (newValue>getMax()) {
    if (wrapsAround())
      newValue -= getMax()-getMin(); // wrap backwards
    else
      newValue = getMax(); // just stay at max
  }
  // apply (silently), only if value has actually changed (but even if change is below resolution)
  if (newValue!=cachedChannelValue) {
    // setting new value captures current (possibly transitional) value as previous and completes transition
    previousChannelValue = channelLastSync!=Never ? getTransitionalValue() : newValue; // If there is no valid previous value, set current as previous.
    transitionProgress = 1; // consider done
    // save target parameters for next transition
    cachedChannelValue = newValue;
    nextTransitionTime = aTransitionTime;
    channelUpdatePending = true; // pending to be sent to the device
    channelLastSync = Never; // cachedChannelValue is no longer applied (does not correspond with actual hardware)
  }
  return newValue;
}



void ChannelBehaviour::channelValueApplied(bool aAnyWay)
{
  if (channelUpdatePending || aAnyWay) {
    channelUpdatePending = false; // applied (might still be in transition, though)
    channelLastSync = MainLoop::now(); // now we know that we are in sync
    if (!aAnyWay) {
      // only log when actually of importance (to prevent messages for devices that apply mostly immediately)
      SALOG(output.device, LOG_INFO,
        "Channel '%s' has applied new value %0.2f to hardware%s",
        getName(), cachedChannelValue, inTransition() ? " (still in transition)" : " (complete)"
      );
    }
  }
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
          // get value of channel, possibly calculating it if needed (color conversions)
          aPropValue->setDoubleValue(getChannelValueCalculated());
          return true;
        case age_key+states_key_offset:
          if (channelLastSync==Never)
            aPropValue->setNull(); // no value known
          else
            aPropValue->setDoubleValue((double)(MainLoop::now()-channelLastSync)/Second);
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
          setChannelValue(aPropValue->doubleValue(), 0, true); // always apply, no transition time
          return true;
      }
    }
  }
  // single class level properties only, don't call inherited
  return false;
}

