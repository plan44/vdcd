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

#include "audiobehaviour.hpp"

#include <math.h>

using namespace p44;



#pragma mark - AudioDeviceSettings with default audio scenes factory


AudioDeviceSettings::AudioDeviceSettings(Device &aDevice) :
  inherited(aDevice)
{
};


DsScenePtr AudioDeviceSettings::newDefaultScene(SceneNo aSceneNo)
{
  AudioScenePtr audioScene = AudioScenePtr(new AudioScene(*this, aSceneNo));
  audioScene->setDefaultSceneValues(aSceneNo);
  // return it
  return audioScene;
}



#pragma mark - AudioBehaviour

#define STANDARD_DIM_CURVE_EXPONENT 4 // standard exponent, usually ok for PWM for LEDs

AudioBehaviour::AudioBehaviour(Device &aDevice) :
  inherited(aDevice)
  // hardware derived parameters
  // persistent settings
  // volatile state
{
  // make it member of the audio group
  setGroupMembership(group_cyan_audio, true);
  // primary output controls brightness
  setHardwareName("volume");
  // add the volume channel (every audio device has volume)
  volume = AudioVolumeChannelPtr(new AudioVolumeChannel(*this));
  addChannel(volume);
}


Tristate AudioBehaviour::hasModelFeature(DsModelFeatures aFeatureIndex)
{
  // now check for audio behaviour level features
  switch (aFeatureIndex) {
    case modelFeature_outmodegeneric:
      // wants generic output mode
      return yes;
    default:
      // not available at this level, ask base class
      return inherited::hasModelFeature(aFeatureIndex);
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
    stopActions();
    SceneNo sceneNo = lightScene->sceneNo;
    // now check for special hard-wired scenes
    if (sceneNo==AUTO_OFF) {
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
        LOG(LOG_NOTICE,"- ApplyScene(AUTO_OFF): starting slow fade down from %d to %d (and then OFF) in steps of %d, stepTime = %dmS\n", (int)b, (int)brightness->getMinDim(), AUTO_OFF_FADE_STEPSIZE, (int)(fadeStepTime/MilliSecond));
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
    LOG(LOG_INFO,"- ApplyScene(AUTO_OFF): reached minDim, now turning off lamp\n");
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
    lightScene->setRepVar(lightScene->value, brightness->getChannelValue());
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
      loadChannelsFromScene(lightScene);
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


void LightBehaviour::stopActions()
{
  // stop fading down
  MainLoop::currentMainLoop().cancelExecutionTicket(fadeDownTicket);
  // stop blink
  if (blinkTicket) stopBlink();
  // let inherited stop as well
  inherited::stopActions();
}



void LightBehaviour::identifyToUser()
{
  // simple, non-parametrized blink, 4 seconds, 2 second period, 1 second on
  blink(4*Second, LightScenePtr(), NULL, 2*Second, 50);
}


#pragma mark - description/shortDesc


string AudioBehaviour::shortDesc()
{
  return string("Audio");
}


string AudioBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- volume = %.1f, localPriority = %d\n", volume->getChannelValue(), hasLocalPriority());
  s.append(inherited::description());
  return s;
}







