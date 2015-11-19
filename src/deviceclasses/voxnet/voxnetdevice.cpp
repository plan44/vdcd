//
//  Copyright (c) 2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
  unmuteVolume(0),
  knownMuted(false),
  messageTimerTicket(0)
{
  // TODO: make these configurable
  messageSourceID = "#S00113220A2A41";
  messageStream = "radio";
  messageShellCommand = "echo bratenExplodiert >/tmp/nextmsg; killall -SIGUSR1 ices";
  //messageShellCommand = "echo message_@contentindex >/tmp/nextmsg; killall -SIGUSR1 ices";
  messageTitleNo = 1;
  messageLength = 10; // Seconds

  // audio device
  primaryGroup = group_cyan_audio;
  // just color light settings, which include a color scene table
  installSettings(DeviceSettingsPtr(new AudioDeviceSettings(*this)));
  // - add audio device behaviour
  AudioBehaviourPtr a = AudioBehaviourPtr(new AudioBehaviour(*this));
  a->setHardwareOutputConfig(outputFunction_dimmer, outputmode_gradual_positive, usage_room, true, -1);
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


void VoxnetDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  // check for scene context
  AudioScenePtr as = boost::dynamic_pointer_cast<AudioScene>(ab->sceneContextForApply());
  if (as) {
    switch (as->sceneCmd) {
      case scene_cmd_audio_mute:
        ab->volume->setChannelValue(0); // mute
        break;
      case scene_cmd_audio_unmute:
        ab->volume->setChannelValue(unmuteVolume>0 ? unmuteVolume : 1); // restore value known before last mute, but at least non-zero
        break;
      case scene_cmd_audio_next_channel:
      case scene_cmd_audio_next_title:
        getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:room:next", voxnetRoomID.c_str()));
        break;
      case scene_cmd_audio_previous_channel:
      case scene_cmd_audio_previous_title:
        getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:room:previous", voxnetRoomID.c_str()));
        break;
      case scene_cmd_audio_pause:
        // TODO: voxnet text does not have pause yet
        // for now, just do same as for stop
      case scene_cmd_stop:
        // TODO: better command than room power off
        getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:room:off", voxnetRoomID.c_str()));
        break;
      case scene_cmd_audio_play:
        // TODO: better command
        getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:play", voxnetRoomID.c_str()));
        break;
      case scene_cmd_audio_message_priority: // invoke priority message,
      case scene_cmd_audio_message_interruptible: // invoke message which is interruptible
      case scene_cmd_audio_message: // invoke message, similar to standard scene invoke behaviour, but in message mode
        // play message
        playMessage(as);
        break;
      // Unimplemented ones:
      case scene_cmd_audio_repeat_off:
      case scene_cmd_audio_repeat_1:
      case scene_cmd_audio_repeat_all:
      case scene_cmd_audio_shuffle_off:
      case scene_cmd_audio_shuffle_on:
      case scene_cmd_audio_resume_off:
      case scene_cmd_audio_resume_on:
      default:
        break;
    }
    // execute custom scene commands
    if (!as->command.empty()) {
      string subcmd, params;
      if (keyAndValue(as->command, subcmd, params, ':')) {
        if (subcmd=="voxnet") {
          // Syntax: voxnet:<voxnet text command or macro>
          // direct execution of voxnet commands, replacing extra "magic" identifiers as follows:
          // - @dsroom with this device's room ID
          size_t i;
          while ((i = params.find("@dsroom"))!=string::npos) {
            params.replace(i, 7, voxnetRoomID);
          }
          getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(params);
        }
        else {
          ALOG(LOG_ERR, "Unknown scene command: %s", as->command.c_str());
        }
      }
    }
  }
  // finally apply the values
  // - Volume
  if (ab->volume->needsApplying()) {
    int voxvol = ab->volume->getTransitionalValue()*MAX_VOXNET_VOLUME/100;
    // transmit a volume or mute command
    string cmd;
    if (voxvol==0) {
      // volume 0 is interpreted as muted
      knownMuted = true;
      cmd = string_format("%s:room:mute:on", voxnetRoomID.c_str());
    }
    else {
      // send volume change
      cmd = string_format("%s:room:volume:set:%d", voxnetRoomID.c_str(), voxvol);
    }
    getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(cmd);
    // unmute if previously muted
    if (knownMuted && voxvol>0) {
      // need to unmute
      getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:room:mute:off", voxnetRoomID.c_str()));
    }
    ab->volume->channelValueApplied(); // confirm having applied the value
  }
  // - Power
  if (ab->powerState->needsApplying()) {
    bool powerOn = ab->powerState->getTransitionalValue();
    if (!powerOn) {
      // transmit a room off command
      getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:room:off", voxnetRoomID.c_str()));
    }
    ab->powerState->channelValueApplied(); // confirm having applied the value
  }
  // - content source
  if (ab->contentSource->needsApplying()) {
    // transmit a play command for the source
    getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:play:%d", voxnetRoomID.c_str(), ab->contentSource->getIndex()));
    ab->contentSource->channelValueApplied(); // confirm having applied the value
  }
  // let inherited complete the apply
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


void VoxnetDevice::playMessage(AudioScenePtr aAudioScene)
{
  if (messageTimerTicket) {
    // message is already playing, source selection already made
    MainLoop::currentMainLoop().cancelExecutionTicket(messageTimerTicket);
  }
  else {
    // prepare for message playing
    getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format(
      "%s:room:select:%s;%s:stream:%s",
      voxnetRoomID.c_str(),
      messageSourceID.c_str(),
      voxnetRoomID.c_str(),
      messageStream.c_str()
    ));
    if (messageTitleNo>0) {
      // start playing the title (radio station)
      getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:play:%d", voxnetRoomID.c_str(), messageTitleNo));
    }
  }
  // shell command to trigger actual message play
  if (!messageShellCommand.empty()) {
    // first start it
    size_t i;
    string sc = messageShellCommand;
    while ((i = sc.find("@contentindex"))!=string::npos) {
      sc.replace(i, 13, string_format("%d", aAudioScene->contentSource));
    }
    MainLoop::currentMainLoop().fork_and_system(boost::bind(&VoxnetDevice::playingStarted, this, _3), sc.c_str());
  }
  else {
    // no shell command, consider started already
    playingStarted("");
  }
}


void VoxnetDevice::playingStarted(const string &aPlayCommandOutput)
{
  int msgLen = messageLength;
  if (msgLen==0) {
    // shell command is supposed to return it
    sscanf(aPlayCommandOutput.c_str(), "%d", &msgLen);
  }
  // set up end-of-message timer
  messageTimerTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&VoxnetDevice::endOfMessage, this), msgLen*Second);
}



void VoxnetDevice::endOfMessage()
{
  MainLoop::currentMainLoop().cancelExecutionTicket(messageTimerTicket);
  // revert source to previous
  getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:room:revert", voxnetRoomID.c_str()));
}




void VoxnetDevice::processVoxnetStatus(const string aVoxnetStatus)
{
  ALOG(LOG_DEBUG, "Status: %s", aVoxnetStatus.c_str());
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  // streaming=$U00113220A2A40:volume=10:balance=1:treble=1:bass=2:mute=off
  string kv;
  size_t i = 0;
  size_t e;
  double vol = 0;
  do {
    e = aVoxnetStatus.find_first_of(":", i);
    kv.assign(aVoxnetStatus, i, e-i);
    string k, v;
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
      }
    }
    i = e+1;
  } while (e!=string::npos);
  // update channel state
  // - save the volume, we might need it for unmute
  unmuteVolume = vol;
  if (knownMuted) vol = 0; // when muted, channel value is 0
  ab->volume->syncChannelValue(vol);
}




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
  s = string_format("Voxnet device %s", voxnetRoomID.c_str());
  return s;
}



string VoxnetDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "\n- Voxnet device %s", voxnetRoomID.c_str());
  return s;
}


#endif // ENABLE_VOXNET



