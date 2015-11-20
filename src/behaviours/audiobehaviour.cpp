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


#pragma mark - audio scene values/channels

AudioScene::AudioScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inherited(aSceneDeviceSettings, aSceneNo),
  contentSource(0),
  powerState(dsAudioPower_deep_off)
{
}


double AudioScene::sceneValue(size_t aChannelIndex)
{
  ChannelBehaviourPtr cb = getDevice().getChannelByIndex(aChannelIndex);
  switch (cb->getChannelType()) {
    case channeltype_p44_audio_content_source: return contentSource;
    case channeltype_p44_audio_power_state: return powerState;
    default: return inherited::sceneValue(aChannelIndex);
  }
  return 0;
}


void AudioScene::setSceneValue(size_t aChannelIndex, double aValue)
{
  ChannelBehaviourPtr cb = getDevice().getChannelByIndex(aChannelIndex);
  switch (cb->getChannelType()) {
    case channeltype_p44_audio_content_source: setPVar(contentSource, (uint32_t)aValue); break;
    case channeltype_p44_audio_power_state: setPVar(powerState, (DsAudioPowerState)aValue); break;
    default: inherited::setSceneValue(aChannelIndex, aValue); break;
  }
}


#pragma mark - Audio Scene persistence

const char *AudioScene::tableName()
{
  return "AudioScenes";
}

// data field definitions

static const size_t numAudioSceneFields = 2;

size_t AudioScene::numFieldDefs()
{
  return inherited::numFieldDefs()+numAudioSceneFields;
}


const FieldDefinition *AudioScene::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numAudioSceneFields] = {
    { "contentSource", SQLITE_INTEGER },
    { "powerState", SQLITE_INTEGER },
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numAudioSceneFields)
    return &dataDefs[aIndex];
  return NULL;
}


// flags in globalSceneFlags
enum {
  // parent uses bit 0 and 1 (globalflags_sceneLevelMask) and bits 8..23
  // audio scene global
  audioflags_fixvol = 0x0004, ///< fixed (always recalled) volume
  audioflags_message = 0x0008, ///< is a message
  audioflags_priority = 0x0010, ///< is a priority message
  audioflags_interruptible = 0x0020, ///< is an interruptible message
  audioflags_paused_restore = 0x0040, ///< paused restore after message
};



/// load values from passed row
void AudioScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the fields
  contentSource = aRow->get<int>(aIndex++);
  powerState = (DsAudioPowerState)aRow->get<int>(aIndex++);
}


/// bind values to passed statement
void AudioScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, (int)contentSource);
  aStatement.bind(aIndex++, (int)powerState);
}



#pragma mark - default audio scene

void AudioScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  // set the common light scene defaults
  inherited::setDefaultSceneValues(aSceneNo);
  // Add special audio scene behaviour
  bool psi = false; // default: dont ignore power state
  bool sci = false; // default: dont ignore content source
  switch (aSceneNo) {
    // group related scenes
    case AUDIO_REPEAT_OFF: sceneCmd = scene_cmd_audio_repeat_off; break;
    case AUDIO_REPEAT_1: sceneCmd = scene_cmd_audio_repeat_1; break;
    case AUDIO_REPEAT_ALL: sceneCmd = scene_cmd_audio_repeat_all; break;
    case AUDIO_PREV_TITLE: sceneCmd = scene_cmd_audio_previous_title; break;
    case AUDIO_NEXT_TITLE: sceneCmd = scene_cmd_audio_next_title; break;
    case AUDIO_PREV_CHANNEL: sceneCmd = scene_cmd_audio_previous_channel; break;
    case AUDIO_NEXT_CHANNEL: sceneCmd = scene_cmd_audio_next_channel; break;
    case AUDIO_MUTE: sceneCmd = scene_cmd_audio_mute; break;
    case AUDIO_UNMUTE: sceneCmd = scene_cmd_audio_unmute; break;
    case AUDIO_PLAY: sceneCmd = scene_cmd_audio_play; break;
    case AUDIO_PAUSE: sceneCmd = scene_cmd_audio_pause; break;
    case AUDIO_SHUFFLE_OFF: sceneCmd = scene_cmd_audio_shuffle_off; break;
    case AUDIO_SHUFFLE_ON: sceneCmd = scene_cmd_audio_shuffle_on; break;
    case AUDIO_RESUME_OFF: sceneCmd = scene_cmd_audio_resume_off; break;
    case AUDIO_RESUME_ON: sceneCmd = scene_cmd_audio_resume_on; break;
    // group independent scenes
    case BELL1:
    case BELL2:
    case BELL3:
    case BELL4:
      // Non-Standard: simple messages
      globalSceneFlags |= audioflags_fixvol|audioflags_message;
      value = 30;
      break;
    case SIG_PANIC:
      value = 0; // silent on panic
      globalSceneFlags |= audioflags_fixvol;
      sci = true;
      psi = true;
      break;
    case STANDBY:
      powerState = dsAudioPower_power_save;
      sci = true;
      break;
    case DEEP_OFF:
      powerState = dsAudioPower_deep_off;
      sci = true;
      break;
    case SLEEPING:
    case ABSENT:
      powerState = dsAudioPower_power_save;
      sci = true;
      break;
    case GAS:
      psi = true;
      // fall through
    case FIRE:
    case SMOKE:
    case WATER:
      globalSceneFlags |= audioflags_paused_restore;
      // fall through
    case ALARM1:
    case ALARM2:
    case ALARM3:
    case ALARM4:
      globalSceneFlags |= audioflags_priority;
      // fall through
    case HAIL:
      value = 30;
      globalSceneFlags |= audioflags_fixvol|audioflags_message;
      break;
  }
  // adjust volume default setting
  if (value>0) {
    value=30; // all non-zero volume presets are 30%
  }
  if (
    (aSceneNo>=T0_S2 && aSceneNo<=T4E_S1) || // standard invoke scenes
    (aSceneNo==T0_S0) || // main off
    (aSceneNo==T0_S1) // main on
  ) {
    // powerstate follows volume
    powerState = value>0 ? dsAudioPower_on : dsAudioPower_deep_off;
    // fixvol for mute scenes
    if (value==0) {
      globalSceneFlags |= audioflags_fixvol;
    }
  }
  // adjust per-channel dontcare
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(getOutputBehaviour());
  if (ab) {
    if (psi) setSceneValueFlags(ab->powerState->getChannelIndex(), valueflags_dontCare, true);
    if (sci) setSceneValueFlags(ab->contentSource->getChannelIndex(), valueflags_dontCare, true);
  }
  markClean(); // default values are always clean
}

#pragma mark - FixVol


bool AudioScene::hasFixVol()
{
  return (globalSceneFlags & audioflags_fixvol)!=0;
}


bool AudioScene::isMessage()
{
  return (globalSceneFlags & audioflags_message)!=0;
}

bool AudioScene::isPriorityMessage()
{
  return (globalSceneFlags & audioflags_priority)!=0;
}

bool AudioScene::isInterruptible()
{
  return (globalSceneFlags & audioflags_interruptible)!=0;
}

bool AudioScene::hasPausedRestore()
{
  return (globalSceneFlags & audioflags_paused_restore)!=0;
}



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
  inherited(aDevice),
  // hardware derived parameters
  // persistent settings
  // volatile state
  unmuteVolume(0)
{
  // make it member of the audio group
  setGroupMembership(group_cyan_audio, true);
  // primary output controls volume
  setHardwareName("volume");
  // add the audio device channels
  // - volume (default channel, comes first)
  volume = AudioVolumeChannelPtr(new AudioVolumeChannel(*this));
  addChannel(volume);
  // - power state
  powerState = AudioPowerStateChannelPtr(new AudioPowerStateChannel(*this));
  addChannel(powerState);
  // - content source
  contentSource = AudioContentSourceChannelPtr(new AudioContentSourceChannel(*this));
  addChannel(contentSource);
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
bool AudioBehaviour::applyScene(DsScenePtr aScene)
{
  // check special actions (commands) for audio scenes
  AudioScenePtr audioScene = boost::dynamic_pointer_cast<AudioScene>(aScene);
  if (audioScene) {
    // any scene call cancels actions (such as fade down)
    stopSceneActions();
    // Note: some of the audio special commands are handled at the applyChannelValues() level
    //   in the device, using sceneContextForApply().
    // Now check for the commands that can be handled at the behaviour level
    SceneCmd sceneCmd = audioScene->sceneCmd;
    switch (sceneCmd) {
      case scene_cmd_audio_mute:
        unmuteVolume = volume->getChannelValue(); ///< save current volume
        volume->setChannelValue(0); // mute
        return true; // don't let inherited load channels, just request apply
      case scene_cmd_audio_unmute:
        volume->setChannelValue(unmuteVolume>0 ? unmuteVolume : 1); // restore value known before last mute, but at least non-zero
        return true; // don't let inherited load channels, just request apply
      case scene_cmd_slow_off:
        // TODO: implement it
        #warning "%%% tbd"
        break;
      default:
        break;
    }
  } // if audio scene
  // perform standard apply (loading channels)
  return inherited::applyScene(aScene);
}


void AudioBehaviour::loadChannelsFromScene(DsScenePtr aScene)
{
  AudioScenePtr audioScene = boost::dynamic_pointer_cast<AudioScene>(aScene);
  if (audioScene) {
    // load channels from scene
    // - volume: ds-audio says: "If the flag is not set, the volume setting of the previously set scene
    //   will be taken over unchanged unless the device was off before the scene call."
    if ((powerState->getChannelValue()!=dsAudioPower_on) || audioScene->hasFixVol()) {
      // device was off before or fixvol is set
      volume->setChannelValueIfNotDontCare(aScene, audioScene->value, 0, 0, false);
    }
    // - powerstate
    powerState->setChannelValueIfNotDontCare(aScene, audioScene->powerState, 0, 0, false);
    // - content source
    contentSource->setChannelValueIfNotDontCare(aScene, audioScene->contentSource, 0, 0, !audioScene->command.empty()); // always apply if there is a command
  }
  else {
    // only if not audio scene, use default loader
    inherited::loadChannelsFromScene(aScene);
  }
}


void AudioBehaviour::saveChannelsToScene(DsScenePtr aScene)
{
  AudioScenePtr audioScene = boost::dynamic_pointer_cast<AudioScene>(aScene);
  if (audioScene) {
    // save channels from scene
    audioScene->setPVar(audioScene->value, volume->getChannelValue());
    audioScene->setSceneValueFlags(volume->getChannelIndex(), valueflags_dontCare, false);
    audioScene->setPVar(audioScene->powerState, (DsAudioPowerState)powerState->getChannelValue());
    audioScene->setSceneValueFlags(powerState->getChannelIndex(), valueflags_dontCare, false);
    audioScene->setPVar(audioScene->contentSource, (uint32_t)contentSource->getChannelValue());
    audioScene->setSceneValueFlags(contentSource->getChannelIndex(), valueflags_dontCare, false);
  }
  else {
    // only if not light scene, use default save
    inherited::saveChannelsToScene(aScene);
  }
}


// dS Dimming rule for Audio:
//  "All selected devices which are turned on and in play state take part in the dimming process."

bool AudioBehaviour::canDim(DsChannelType aChannelType)
{
  // only devices that are on can be dimmed (volume changed)
  if (aChannelType==channeltype_p44_audio_volume) {
    return powerState->getChannelValue()==dsAudioPower_on; // dimmable if on
  }
  else {
    // other audio channels cannot be dimmed anyway
    return false;
  }
}


void AudioBehaviour::performSceneActions(DsScenePtr aScene, SimpleCB aDoneCB)
{
  // we can only handle audio scenes
  AudioScenePtr audioScene = boost::dynamic_pointer_cast<AudioScene>(aScene);
  if (audioScene) {
    // TODO: check for blink effect?
  }
  // none of my effects, let inherited check
  inherited::performSceneActions(aScene, aDoneCB);
}


void AudioBehaviour::stopSceneActions()
{
  // let inherited stop as well
  inherited::stopSceneActions();
}



void AudioBehaviour::identifyToUser()
{
  // blink effect?
  #warning "%%% tbd"
}


#pragma mark - description/shortDesc


string AudioBehaviour::shortDesc()
{
  return string("Audio");
}


string AudioBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s,
    "\n- volume = %.1f, powerstate = %d, contentsource = %u",
    volume->getChannelValue(),
    (int)powerState->getChannelValue(),
    (unsigned int)contentSource->getChannelValue()
  );
  s.append(inherited::description());
  return s;
}







