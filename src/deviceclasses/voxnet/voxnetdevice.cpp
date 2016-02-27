//
//  Copyright (c) 2015-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
#define FOCUSLOGLEVEL 0

#include "voxnetdevice.hpp"

#if ENABLE_VOXNET

using namespace p44;


#pragma mark - VoxnetDevice

// max volume value for voxnet
#define MAX_VOXNET_VOLUME 40
// maximum number of content sources (titles in an album/playlist, radio stations)
#define MAX_VOXNET_CONTENTSOURCES 50

VoxnetDevice::VoxnetDevice(VoxnetDeviceContainer *aClassContainerP, const string aVoxnetRoomID) :
  inherited(aClassContainerP),
  voxnetRoomID(aVoxnetRoomID),
  knownMuted(false),
  playIsOnlyUnmute(false),
  prePauseVolume(0),
  prePausePower(true),
  prePauseContentSource(0),
  pauseToPoweroffTicket(0),
  messageTimerTicket(0)
{
  // audio device
  primaryGroup = group_cyan_audio;
  // just color light settings, which include a color scene table
  installSettings(DeviceSettingsPtr(new VoxnetDeviceSettings(*this)));
  // - add audio device behaviour
  AudioBehaviourPtr a = AudioBehaviourPtr(new AudioBehaviour(*this));
  a->setHardwareOutputConfig(outputFunction_dimmer, outputmode_gradual, usage_room, true, -1);
  // - adjust resolution for volume
  a->volume->setResolution(a->volume->getMax()/MAX_VOXNET_VOLUME);
  a->contentSource->setNumIndices(MAX_VOXNET_CONTENTSOURCES);
  addBehaviour(a);
  // - create dSUID
  deriveDsUid();
}



VoxnetDeviceContainer &VoxnetDevice::getVoxnetDeviceContainer()
{
  return *(static_cast<VoxnetDeviceContainer *>(classContainerP));
}


void VoxnetDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  // disconnection is immediate, so we can call inherited right now
  inherited::disconnect(aForgetParams, aDisconnectResultHandler);
}

#define SUPPORT_OLD_AUDIO_BEHAVIOUR 1

bool VoxnetDevice::prepareSceneCall(DsScenePtr aScene)
{
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  AudioScenePtr as = boost::dynamic_pointer_cast<AudioScene>(aScene);
  bool continueApply = true;
  if (as) {
    SceneCmd scmd = as->sceneCmd;
    #if SUPPORT_OLD_AUDIO_BEHAVIOUR
    if (as->sceneNo==ROOM_OFF) {
      // main off = pause
      scmd = scene_cmd_audio_pause;
    }
    else if (as->sceneNo==ROOM_ON) {
      // main on = play
      scmd = scene_cmd_audio_play;
    }
    #endif
    // check for pausing current audio (for message or real pause)
    switch (scmd) {
      case scene_cmd_audio_next_channel:
      case scene_cmd_audio_next_title:
        sendVoxnetText(string_format("%s:room:next", voxnetRoomID.c_str()));
        // Note: we rely on status parsing to actually update content source channel change
        continueApply = false; // that's all what we need to do
        break;
      case scene_cmd_audio_previous_channel:
      case scene_cmd_audio_previous_title:
        // Note: we rely on status parsing to actually update content source channel change
        sendVoxnetText(string_format("%s:room:previous", voxnetRoomID.c_str()));
        continueApply = false; // that's all what we need to do
        break;
      case scene_cmd_audio_play:
        // TODO: play does not have a function when power is off or nothing is selected
        if (ab->powerState->getIndex()==dsAudioPower_on && playIsOnlyUnmute) {
          // already on - only unmute in case it is muted
          if (knownMuted) {
            sendVoxnetText(string_format("%s:room:mute:off", voxnetRoomID.c_str()));
            knownMuted = false;
          }
          continueApply = false; // that's all what we need to do
        }
        else {
          // powered off before, or otherwise needed to restore try to restore pre-pause state
          if (ab->knownPaused) {
            restorePrePauseState();
          }
        }
        // definitely no longer paused
        playIsOnlyUnmute = false;
        ab->knownPaused = false;
        break;
      case scene_cmd_audio_pause:
        // is NOP when power is already off
        if (ab->powerState->getIndex()==dsAudioPower_on) {
          ab->knownPaused = true; // Note: this flag is reset based on status parsing
          capturePrePauseState();
          // TODO: voxnet text does not have pause yet
          // for now, just mute
          knownMuted = true;
          sendVoxnetText(string_format("%s:room:mute:on", voxnetRoomID.c_str()));
          playIsOnlyUnmute = true;
          // TODO: %%% schedule a poweroff timeout
        }
        continueApply = false; // that's all what we need to do
        break;
      case scene_cmd_stop:
      case scene_cmd_off:
        // TODO: better command than room power off
        sendVoxnetText(string_format("%s:room:off", voxnetRoomID.c_str()));
        continueApply = false; // that's all what we need to do
        break;
        // Unimplemented ones:
      case scene_cmd_audio_repeat_off:
      case scene_cmd_audio_repeat_1:
      case scene_cmd_audio_repeat_all:
      case scene_cmd_audio_shuffle_off:
      case scene_cmd_audio_shuffle_on:
      case scene_cmd_audio_resume_off:
      case scene_cmd_audio_resume_on:
        continueApply = false; // that's all what we need to do
      default:
        break;
    }
  }
  // check for messages
  if (as->isMessage() && !messageTimerTicket && !ab->knownPaused) {
    // message, and no message already playing or audio already paused
    capturePrePauseState();
  }
  // prepared ok
  return continueApply;
}



void VoxnetDevice::capturePrePauseState()
{
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  // remember current state to be able to restore later
  prePauseVolume = ab->volume->getChannelValue();
  prePausePower = ab->powerState->getIndex()==dsAudioPower_on;
  prePauseSource = currentSource;
  prePauseStream = currentStream;
  prePauseContentSource = ab->contentSource->getIndex();
  ALOG(LOG_NOTICE,
    "Captured pre-pause state; source=%s, stream=%s, contentsource=%d, power=%s, volume=%d",
    prePauseSource.c_str(),
    prePauseStream.c_str(),
    prePauseContentSource,
    prePausePower ? "ON" : "OFF",
    (int)prePauseVolume
  );
}


void VoxnetDevice::restorePrePauseState()
{
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  // revert source and stream to previous
  ALOG(LOG_NOTICE,
    "Restoring pre-pause state; source=%s, stream=%s, contentsource=%d, power=%s, volume=%d",
    prePauseSource.c_str(),
    prePauseStream.c_str(),
    prePauseContentSource,
    prePausePower ? "ON" : "OFF",
    (int)prePauseVolume
  );
  if (prePausePower) {
    // was on before, revert
    sendVoxnetText(string_format(
      "%s:room:select:%s;%s:stream:%s",
      voxnetRoomID.c_str(),
      prePauseSource.c_str(),
      voxnetRoomID.c_str(),
      prePauseStream.c_str()
    ));
    // revert volume to previous
    ab->volume->setChannelValue(prePauseVolume, 0, true); // restore value known before message started playing, will also unmute if needed
    // if waking from poweroff, re-apply content source as well to make sure playing re-starts
    if (ab->powerState->getIndex()!=dsAudioPower_on) {
      ab->contentSource->setChannelValue(prePauseContentSource, 0, true);
    }
  }
  else {
    // was off before, turn off again
    ab->powerState->setChannelValue(dsAudioPower_power_save, 0, true);
  }
  requestApplyingChannels(NULL, false);
  // not paused any more
  ab->knownPaused = false;
}



bool VoxnetDevice::prepareSceneApply(DsScenePtr aScene)
{
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  AudioScenePtr as = boost::dynamic_pointer_cast<AudioScene>(aScene);
  // execute custom scene commands
  string playcmd;
  if (ab->stateRestoreCmdValid) {
    string subcmd, params;
    if (keyAndValue(ab->stateRestoreCmd, subcmd, params, ':')) {
      if (subcmd=="voxnet") {
        // Syntax: voxnet:<voxnet text command or macro>
        // direct execution of voxnet commands, replacing extra "magic" identifiers as follows:
        // - @dsroom with this device's room ID
        size_t i;
        while ((i = params.find("@{dsroom}"))!=string::npos) {
          params.replace(i, 9, voxnetRoomID);
        }
        // also allow @{sceneno} and @{channel...} placeholders
        as->substitutePlaceholders(params);
        ALOG(LOG_INFO, "sending voxnet scene command: %s", params.c_str());
        sendVoxnetText(params);
        // applying a scene with a voxnet command means that play cannot just unmute, but must restore state
        playIsOnlyUnmute = false;
        // make sure contentSource channel is also re-applied
        ab->contentSource->setNeedsApplying();
      }
      else if (subcmd=="msgcmd") {
        // Syntax: msgcmd:shell command
        // direct execution of a shell command supposed to play a message, replacing extra "magic" identifiers as follows:
        // - @{sceneno} with the scene's number
        // - @{channel...} channel values
        as->substitutePlaceholders(params);
        playcmd=params;
      }
      else {
        ALOG(LOG_ERR, "Unknown scene command: %s", as->command.c_str());
      }
    }
  }
  // check for messages
  if (as->isMessage()) {
    playMessage(as, playcmd);
  }
  // still apply channel values
  return true;
}


void VoxnetDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  // Apply the values
  // - Volume
  if (ab->volume->needsApplying()) {
    int voxvol = ab->volume->getTransitionalValue()*MAX_VOXNET_VOLUME/100;
    // transmit a volume or mute command
    string cmd;
    if (voxvol==0) {
      // volume 0 is interpreted as muted
      knownMuted = true;
      sendVoxnetText(string_format("%s:room:mute:on", voxnetRoomID.c_str()));
    }
    else {
      // we need to send a volume change. Note: setting volume automatically unmutes
      cmd = string_format("%s:room:volume:set:%d", voxnetRoomID.c_str(), voxvol);
      if (ab->knownPaused && voxnetSettings()->playToUnmuteDelayMS>0) {
        // this is applying non-zero volume after a pause - delay it according to playToUnmuteDelayMS (to cover stream switching delays)
        MainLoop::currentMainLoop().executeOnce(boost::bind(&VoxnetDevice::sendVoxnetText, this, cmd), voxnetSettings()->playToUnmuteDelayMS*MilliSecond);
      }
      else {
        // apply immediately
        sendVoxnetText(cmd);
      }
    }
    ab->volume->channelValueApplied(); // confirm having applied the value
  }
  // - Power
  if (ab->powerState->needsApplying()) {
    bool powerOn = ab->powerState->getIndex()==dsAudioPower_on;
    if (!powerOn) {
      // transmit a room off command
      sendVoxnetText(string_format("%s:room:off", voxnetRoomID.c_str()));
    }
    ab->powerState->channelValueApplied(); // confirm having applied the value
  }
  // - content source
  if (ab->contentSource->needsApplying()) {
    // transmit a play command for the source
    if (ab->contentSource->getIndex()>0) {
      sendVoxnetText(string_format("%s:play:%d", voxnetRoomID.c_str(), ab->contentSource->getIndex()));
      ab->knownPaused = false; // not paused any more
      ab->contentSource->channelValueApplied(); // confirm having applied the value
    }
  }
  // let inherited complete the apply
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void VoxnetDevice::sendVoxnetText(const string aVoxnetText)
{
  getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(aVoxnetText);
}



void VoxnetDevice::playMessage(AudioScenePtr aAudioScene, const string aPlayCmd)
{
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  ALOG(LOG_INFO, "playing message");
  if (messageTimerTicket) {
    // message is already playing, source selection already made
    MainLoop::currentMainLoop().cancelExecutionTicket(messageTimerTicket);
  }
  else {
    // prepare for message playing
    sendVoxnetText(string_format(
      "%s:room:select:%s;%s:stream:%s",
      voxnetRoomID.c_str(),
      voxnetSettings()->messageSourceID.c_str(),
      voxnetRoomID.c_str(),
      voxnetSettings()->messageStream.c_str()
    ));
    if (voxnetSettings()->messageTitleNo>0) {
      // start playing the title (radio station)
      sendVoxnetText(string_format("%s:play:%d", voxnetRoomID.c_str(), voxnetSettings()->messageTitleNo));
    }
  }
  // shell command to trigger actual message play
  string sc = aPlayCmd;
  if (sc.empty()) {
    sc = voxnetSettings()->messageShellCommand;
  }
  if (!sc.empty()) {
    // first start it
    // - allow @{sceneno} and @{channel...} placeholders
    aAudioScene->substitutePlaceholders(sc);
    ALOG(LOG_INFO, "- executing play shell command: %s", sc.c_str());
    MainLoop::currentMainLoop().fork_and_system(boost::bind(&VoxnetDevice::playingStarted, this, _3), sc.c_str(), true);
  }
  else {
    // no shell command, consider started already
    playingStarted("");
  }
}


void VoxnetDevice::playingStarted(const string &aPlayCommandOutput)
{
  ALOG(LOG_INFO,"- play shell command returns: %s", aPlayCommandOutput.c_str());
  int duration = voxnetSettings()->messageDuration;
  if (duration==0) {
    // shell command is supposed to return it
    sscanf(aPlayCommandOutput.c_str(), "%d", &duration);
  }
  if (duration>0) {
    // set up end-of-message timer
    ALOG(LOG_INFO,"- play time of %d seconds starts now", duration);
    messageTimerTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&VoxnetDevice::endOfMessage, this), duration*Second);
  }
  else {
    ALOG(LOG_INFO,"- play time is unlimited");
  }
}



void VoxnetDevice::endOfMessage()
{
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  MainLoop::currentMainLoop().cancelExecutionTicket(messageTimerTicket);
  if (ab->knownPaused) {
    ALOG(LOG_INFO,"Message played to end, but audio was paused before -> NOP now, but make sure state will be restored at play");
    playIsOnlyUnmute = false;
  }
  else {
    ALOG(LOG_INFO,"Message played to end -> reverting source and restoring volume");
    restorePrePauseState();
  }
}




bool VoxnetDevice::processVoxnetStatus(const string aVoxnetID, const string aVoxnetStatus)
{
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  string kv;
  string k, v;
  size_t i;
  size_t e;
  bool needFullStatus = false;
  if (aVoxnetStatus[0]=='[') {
    if (aVoxnetID==currentSource) {
      AFOCUSLOG("Source command confirmation: %s", aVoxnetStatus.c_str());
      // $r.zone1:$A0011324822231:[room\:off]:ok
      if (aVoxnetStatus.substr(0,6)=="[next]") {
        ab->knownPaused = false;
        ab->contentSource->syncChannelValue(ab->contentSource->getIndex()+1);
      }
      else if (aVoxnetStatus.substr(0,10)=="[previous]") {
        ab->knownPaused = false;
        ab->contentSource->syncChannelValue(ab->contentSource->getIndex()-1);
      }
      else if (aVoxnetStatus.substr(0,5)=="[play") {
        ab->knownPaused = false;
        int cs = 1; // play without number is considered to be the first title in the play list
        if (aVoxnetStatus.substr(5,2)=="\\:") {
          // maybe there's a play number
          sscanf(aVoxnetStatus.c_str()+7, "%d", &cs); // try to get play number
        }
        ab->contentSource->syncChannelValue(cs);
      }
    }
    else if (aVoxnetID==voxnetRoomID) {
      AFOCUSLOG("Room command confirmation: %s", aVoxnetStatus.c_str());
      // $r.zone1:$A0011324822231:[room\:off]:ok
      if (aVoxnetStatus.substr(0,11)=="[room\\:off]") {
        // room known off, update power channel
        ab->powerState->syncChannelValue(dsAudioPower_power_save);
      }
    }
  }
  else if (aVoxnetID==voxnetRoomID) {
    AFOCUSLOG("Room Status: %s", aVoxnetStatus.c_str());
    // streaming=$U00113220A2A40:volume=10:balance=1:treble=1:bass=2:mute=off
    i = 0;
    double vol = 0;
    do {
      e = aVoxnetStatus.find_first_of(":", i);
      kv.assign(aVoxnetStatus, i, e==string::npos ? e : e-i);
      if (keyAndValue(kv, k, v, '=')) {
        // extract state
        if (k=="mute") {
          // mute state
          knownMuted = v=="on";
        }
        else if (k=="volume") {
          // current volume
          int voxvol;
          sscanf(v.c_str(), "%d", &voxvol);
          vol = (double)voxvol/MAX_VOXNET_VOLUME*100;
        }
        else if (k=="streaming") {
          // streaming
          // TODO: For now we consider any streaming as indication of device being on
          //   (and Voxnet 219 does not have a deep off)
          ab->powerState->syncChannelValue(v=="$unknown" ? dsAudioPower_power_save : dsAudioPower_on);
          // try to resolve source
          getVoxnetDeviceContainer().voxnetComm->resolveVoxnetRef(v);
          // - check if streaming a user (i.e. we'd need further resolving)
          if (v.size()>1 && v[1]=='U') {
            // this is a user reference
            if (v!=currentUser) {
              currentUser = v;
              ALOG(LOG_NOTICE,"User changed: %s", v.c_str());
              needFullStatus = true;
            }
          }
          else {
            currentUser.clear(); // remove user assignment
            // save current source ID
            if (v!=currentSource) {
              currentSource = v;
              ALOG(LOG_NOTICE,"Source changed: %s", v.c_str());
              currentStream.clear(); // source change always invalidates stream
              needFullStatus = true;
            }
          }
        }
      }
      i = e+1;
    } while (e!=string::npos);
    // update channel state (only if not currently dimming, as feedback would ruin smooth dimming)
    if (!isDimming) {
      // - save the volume, we might need it for unmute
      if (knownMuted) vol = 0; // when muted, channel value is 0
      ab->volume->syncChannelValue(vol);
    }
  }
  else if (aVoxnetID==currentSource) {
    AFOCUSLOG("Source Status: %s", aVoxnetStatus.c_str());
    // streaming=radio:info_1=SRF Virus
    i = 0;
    do {
      e = aVoxnetStatus.find_first_of(":", i);
      kv.assign(aVoxnetStatus, i, e==string::npos ? e : e-i);
      if (keyAndValue(kv, k, v, '=')) {
        // extract state
        if (
          k=="streaming" || // Voxnet 215 server says "streaming"
          k=="stream" // voxnet 219 room amplifier says "stream"
        ) {
          // Substream of the source
          if (v!=currentStream) {
            currentStream = v;
            ALOG(LOG_NOTICE, "Stream of source %s changed to %s", currentSource.c_str(), v.c_str());
            needFullStatus = true;
          }
        }
      }
      i = e+1;
    } while (e!=string::npos);
  }
  else if (aVoxnetID==currentUser) {
    AFOCUSLOG("User Status: %s", aVoxnetStatus.c_str());
    // selected=$MyMusic2
    // streaming=radio:info_1=SRF Virus
    i = 0;
    do {
      e = aVoxnetStatus.find_first_of(":", i);
      kv.assign(aVoxnetStatus, i, e==string::npos ? e : e-i);
      if (keyAndValue(kv, k, v, '=')) {
        // extract state
        if (k=="selected") {
          // source of current user changed
          getVoxnetDeviceContainer().voxnetComm->resolveVoxnetRef(v);
          if (v!=currentSource) {
            currentSource = v;
            ALOG(LOG_NOTICE,"Source changed: %s (via user %s)", v.c_str(), currentUser.c_str());
            currentStream.clear(); // source change always invalidates stream
            needFullStatus = true;
          }
        }
      }
      i = e+1;
    } while (e!=string::npos);
  }
  ALOG(LOG_DEBUG,
    "Overall Status: currentUser=%s, currentSource=%s, currentStream=%s",
    currentUser.c_str(), currentSource.c_str(), currentStream.c_str()
  );
  // update state restore command when we have collected everything
  if (!needFullStatus) {
    // data should be complete now, create new state restore command
    // $r.zone2:room:select:$MyMusic1;$r.zone2:stream:music;$r.zone2:play
    if (!currentSource.empty()) {
      // only save state if we have a current source
      string srcmd = string_format(
        "voxnet:@{dsroom}:room:select:%s;@{dsroom}:stream:%s",
        currentSource.c_str(), // always source, we don't persist users
        currentStream.c_str()
      );
      if (srcmd!=ab->stateRestoreCmd) {
        ab->stateRestoreCmd = srcmd;
        ab->stateRestoreCmdValid = true;
        ALOG(LOG_INFO,"State restore command updated: %s", srcmd.c_str());
      }
    }
  }
  return needFullStatus;
}



#pragma mark - description


void VoxnetDevice::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = voxnetRoomID (which is MAC-derived and should be globally unique)
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  dSUID.setNameInSpace(voxnetRoomID, vdcNamespace);
}


string VoxnetDevice::hardwareGUID()
{
  return string_format("voxnetdeviceid:%s", voxnetRoomID.c_str());
}


string VoxnetDevice::hardwareModelGUID()
{
  return "voxnetdevicemodel:voxnet219";
}


string VoxnetDevice::vendorName()
{
  return "Revox";
}



string VoxnetDevice::modelName()
{
  return "Voxnet device";
}



bool VoxnetDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  const char *iconName = "voxnet";
  if (iconName && getIcon(iconName, aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


string VoxnetDevice::getExtraInfo()
{
  string s;
  s = string_format(
    "Room %s | User %s | Source %s | Stream %s",
    voxnetRoomID.c_str(),
    currentUser.c_str(),
    currentSource.c_str(),
    currentStream.c_str()
  );
  return s;
}



string VoxnetDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "\n- Voxnet device %s", voxnetRoomID.c_str());
  return s;
}


#pragma mark - property access

enum {
  messageSource_key,
  messageStream_key,
  messageTitleNo_key,
  messageDuration_key,
  messageShellCmd_key,
  playToUnmuteDelay_key,
  numProperties
};

static char voxnetDevice_key;


int VoxnetDevice::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  // Note: only add my own count when accessing root level properties!!
  if (!aParentDescriptor) {
    // Accessing properties at the Device (root) level, add mine
    return inherited::numProps(aDomain, aParentDescriptor)+numProperties;
  }
  // just return base class' count
  return inherited::numProps(aDomain, aParentDescriptor);
}


PropertyDescriptorPtr VoxnetDevice::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numProperties] = {
    { "x-p44-messageSource", apivalue_string, messageSource_key, OKEY(voxnetDevice_key) },
    { "x-p44-messageStream", apivalue_string, messageStream_key, OKEY(voxnetDevice_key) },
    { "x-p44-messageTitleNo", apivalue_int64, messageTitleNo_key, OKEY(voxnetDevice_key) },
    { "x-p44-messageDuration", apivalue_int64, messageDuration_key, OKEY(voxnetDevice_key) },
    { "x-p44-messageShellCmd", apivalue_string, messageShellCmd_key, OKEY(voxnetDevice_key) },
    { "x-p44-playToUnmuteDelay", apivalue_double, playToUnmuteDelay_key, OKEY(voxnetDevice_key) }
  };
  if (!aParentDescriptor) {
    // root level - accessing properties on the Device level
    int n = inherited::numProps(aDomain, aParentDescriptor);
    if (aPropIndex<n)
      return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
    aPropIndex -= n; // rebase to 0 for my own first property
    return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
  }
  else {
    // other level
    return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
  }
}


// access to all fields
bool VoxnetDevice::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(voxnetDevice_key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        case messageSource_key: aPropValue->setStringValue(voxnetSettings()->messageSourceID); return true;
        case messageStream_key: aPropValue->setStringValue(voxnetSettings()->messageStream); return true;
        case messageTitleNo_key: aPropValue->setInt32Value(voxnetSettings()->messageTitleNo); return true;
        case messageDuration_key: aPropValue->setInt32Value(voxnetSettings()->messageDuration); return true;
        case messageShellCmd_key: aPropValue->setStringValue(voxnetSettings()->messageShellCommand); return true;
        case playToUnmuteDelay_key: aPropValue->setDoubleValue((double)voxnetSettings()->playToUnmuteDelayMS/1000); return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        case messageSource_key: voxnetSettings()->setPVar(voxnetSettings()->messageSourceID, aPropValue->stringValue()); return true;
        case messageStream_key: voxnetSettings()->setPVar(voxnetSettings()->messageStream, aPropValue->stringValue()); return true;
        case messageTitleNo_key: voxnetSettings()->setPVar(voxnetSettings()->messageTitleNo, (int)aPropValue->int32Value()); return true;
        case messageDuration_key: voxnetSettings()->setPVar(voxnetSettings()->messageDuration, (int)aPropValue->int32Value()); return true;
        case messageShellCmd_key: voxnetSettings()->setPVar(voxnetSettings()->messageShellCommand, aPropValue->stringValue()); return true;
        case playToUnmuteDelay_key: voxnetSettings()->setPVar(voxnetSettings()->playToUnmuteDelayMS, (int)(aPropValue->doubleValue()*1000)); return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}




#pragma mark - settings


VoxnetDeviceSettings::VoxnetDeviceSettings(Device &aDevice) :
  inherited(aDevice),
  messageTitleNo(0),
  messageDuration(10),
  playToUnmuteDelayMS(1000) // one second by default
{
}



const char *VoxnetDeviceSettings::tableName()
{
  return "VoxnetDeviceSettings";
}


// data field definitions

static const size_t numFields = 6;

size_t VoxnetDeviceSettings::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *VoxnetDeviceSettings::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "messageSourceID", SQLITE_TEXT },
    { "messageStream", SQLITE_TEXT },
    { "messageTitleNo", SQLITE_INTEGER },
    { "messageDuration", SQLITE_INTEGER },
    { "messageShellCommand", SQLITE_TEXT },
    { "playToUnmuteDelayMS", SQLITE_INTEGER },
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void VoxnetDeviceSettings::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the field values
  messageSourceID.assign(nonNullCStr(aRow->get<const char *>(aIndex++)));
  messageStream.assign(nonNullCStr(aRow->get<const char *>(aIndex++)));
  aRow->getIfNotNull<int>(aIndex++, messageTitleNo);
  aRow->getIfNotNull<int>(aIndex++, messageDuration);
  messageShellCommand.assign(nonNullCStr(aRow->get<const char *>(aIndex++)));
  aRow->getIfNotNull<int>(aIndex++, playToUnmuteDelayMS);
}


// bind values to passed statement
void VoxnetDeviceSettings::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, messageSourceID.c_str()); // stable string!
  aStatement.bind(aIndex++, messageStream.c_str()); // stable string!
  aStatement.bind(aIndex++, messageTitleNo);
  aStatement.bind(aIndex++, messageDuration);
  aStatement.bind(aIndex++, messageShellCommand.c_str()); // stable string!
  aStatement.bind(aIndex++, playToUnmuteDelayMS);
}



#endif // ENABLE_VOXNET
