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
  preMessageVolume(0),
  knownMuted(false),
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


bool VoxnetDevice::prepareSceneCall(DsScenePtr aScene)
{
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  AudioScenePtr as = boost::dynamic_pointer_cast<AudioScene>(aScene);
  bool continueApply = true;
  if (as) {
    switch (as->sceneCmd) {
      case scene_cmd_audio_next_channel:
      case scene_cmd_audio_next_title:
        getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:room:next", voxnetRoomID.c_str()));
        continueApply = false; // that's all what we need to do
        break;
      case scene_cmd_audio_previous_channel:
      case scene_cmd_audio_previous_title:
        getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:room:previous", voxnetRoomID.c_str()));
        continueApply = false; // that's all what we need to do
        break;
      case scene_cmd_audio_pause:
        // TODO: voxnet text does not have pause yet
        // for now, just do same as for stop
      case scene_cmd_stop:
        // TODO: better command than room power off
        getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:room:off", voxnetRoomID.c_str()));
        continueApply = false; // that's all what we need to do
        break;
      case scene_cmd_audio_play:
        // TODO: better command
        getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:play", voxnetRoomID.c_str()));
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
    // execute custom scene commands
    string playcmd;
    if (!as->command.empty()) {
      string subcmd, params;
      if (keyAndValue(as->command, subcmd, params, ':')) {
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
          getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(params);
        }
        else if (subcmd=="msgcmd") {
          // Syntax: msgcmd:shell command
          // direct execution of a shell command supposed to play a message, replacing extra "magic" identifiers as follows:
          // - @{sceneno} with the scene's number
          // - @{channel...} channel values
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
  }
  // prepared ok
  return continueApply;
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
    bool powerOn = ab->powerState->getIndex()==dsAudioPower_on;
    if (!powerOn) {
      // transmit a room off command
      getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:room:off", voxnetRoomID.c_str()));
    }
    ab->powerState->channelValueApplied(); // confirm having applied the value
  }
  // - content source
  if (ab->contentSource->needsApplying()) {
    // transmit a play command for the source
    if (ab->contentSource->getIndex()>0) {
      getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:play:%d", voxnetRoomID.c_str(), ab->contentSource->getIndex()));
      ab->contentSource->channelValueApplied(); // confirm having applied the value
    }
  }
  // let inherited complete the apply
  inherited::applyChannelValues(aDoneCB, aForDimming);
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
    // remember pre-Message volume
    preMessageVolume = ab->volume->getChannelValue();
    preMessageSource = currentSource;
    preMessageStream = currentStream;
    preMessagePower = ab->powerState->getIndex()==dsAudioPower_on;
    // prepare for message playing
    getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format(
      "%s:room:select:%s;%s:stream:%s",
      voxnetRoomID.c_str(),
      voxnetSettings()->messageSourceID.c_str(),
      voxnetRoomID.c_str(),
      voxnetSettings()->messageStream.c_str()
    ));
    if (voxnetSettings()->messageTitleNo>0) {
      // start playing the title (radio station)
      getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format("%s:play:%d", voxnetRoomID.c_str(), voxnetSettings()->messageTitleNo));
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
    MainLoop::currentMainLoop().fork_and_system(boost::bind(&VoxnetDevice::playingStarted, this, _3), sc.c_str());
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
  // set up end-of-message timer
  ALOG(LOG_INFO,"- play time of %d seconds starts now", duration);
  messageTimerTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&VoxnetDevice::endOfMessage, this), duration*Second);
}



void VoxnetDevice::endOfMessage()
{
  AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
  ALOG(LOG_INFO,"Message played to end -> reverting source and volume (to %d) now", (int)preMessageVolume);
  MainLoop::currentMainLoop().cancelExecutionTicket(messageTimerTicket);
  // revert source and stream to previous
  if (preMessagePower) {
    // was on before, revert
    getVoxnetDeviceContainer().voxnetComm->sendVoxnetText(string_format(
      "%s:room:select:%s;%s:stream:%s",
      voxnetRoomID.c_str(),
      preMessageSource.c_str(),
      voxnetRoomID.c_str(),
      preMessageStream.c_str()
    ));
    // revert volume to previous
    ab->volume->setChannelValue(preMessageVolume); // restore value known before message started playing
  }
  else {
    // was off before, turn off again
    ab->powerState->setChannelValue(dsAudioPower_power_save);
  }
  requestApplyingChannels(NULL, false);
}




bool VoxnetDevice::processVoxnetStatus(const string aVoxnetID, const string aVoxnetStatus)
{
  string kv;
  string k, v;
  size_t i;
  size_t e;
  bool needFullStatus = false;
  if (aVoxnetID==voxnetRoomID) {
    ALOG(LOG_DEBUG, "Room Status: %s", aVoxnetStatus.c_str());
    AudioBehaviourPtr ab = boost::dynamic_pointer_cast<AudioBehaviour>(output);
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
    // update channel state
    // - save the volume, we might need it for unmute
    if (knownMuted) vol = 0; // when muted, channel value is 0
    ab->volume->syncChannelValue(vol);
  }
  else if (aVoxnetID==currentSource) {
    ALOG(LOG_DEBUG, "Source Status: %s", aVoxnetStatus.c_str());
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
    ALOG(LOG_DEBUG, "User Status: %s", aVoxnetStatus.c_str());
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
    { "x-p44-messageShellCmd", apivalue_string, messageShellCmd_key, OKEY(voxnetDevice_key) }
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
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}




#pragma mark - settings


VoxnetDeviceSettings::VoxnetDeviceSettings(Device &aDevice) :
  inherited(aDevice)
{

//  messageSourceID = "$MyMusic2";
//  messageStream = "radio";
//  messageTitleNo = 1;

  messageSourceID = "$s.zone1";
  messageStream = "analog";
  messageTitleNo = 0;

  messageDuration = 20; // Seconds
  messageShellCommand = "/etc/ices/playmessage @{sceneno}";
}



const char *VoxnetDeviceSettings::tableName()
{
  return "VoxnetDeviceSettings";
}


// data field definitions

static const size_t numFields = 5;

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
  messageTitleNo = aRow->get<int>(aIndex++);
  messageDuration = aRow->get<int>(aIndex++);
  messageShellCommand.assign(nonNullCStr(aRow->get<const char *>(aIndex++)));
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
}





#endif // ENABLE_VOXNET



