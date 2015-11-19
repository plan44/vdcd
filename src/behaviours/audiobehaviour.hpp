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

#ifndef __vdcd__audiobehaviour__
#define __vdcd__audiobehaviour__

#include "device.hpp"
#include "simplescene.hpp"
#include "outputbehaviour.hpp"

using namespace std;

namespace p44 {

  /// Audio volume channel, 0..100%
  class AudioVolumeChannel : public ChannelBehaviour
  {
    typedef ChannelBehaviour inherited;

  public:
    AudioVolumeChannel(OutputBehaviour &aOutput) : inherited(aOutput)
    {
      resolution = 0.1; // arbitrary, 1:1000 seems ok
    };

    virtual DsChannelType getChannelType() { return channeltype_p44_audio_volume; }; ///< the dS channel type
    virtual const char *getName() { return "volume"; };
    virtual double getMin() { return 0; }; // dS volume goes from 0 to 100%
    virtual double getMax() { return 100; };

  };
  typedef boost::intrusive_ptr<AudioVolumeChannel> AudioVolumeChannelPtr;


  /// Audio power state channel
  class AudioPowerStateChannel : public IndexChannel
  {
    typedef IndexChannel inherited;

  public:
    AudioPowerStateChannel(OutputBehaviour &aOutput) : inherited(aOutput) { setNumIndices(numDsAudioPowerStates); }; ///< see DsAudioPowerState enum

    virtual DsChannelType getChannelType() { return channeltype_p44_audio_power_state; }; ///< the dS channel type
    virtual const char *getName() { return "powerstate"; };

  };
  typedef boost::intrusive_ptr<AudioPowerStateChannel> AudioPowerStateChannelPtr;


  /// Audio content source channel
  class AudioContentSourceChannel : public IndexChannel
  {
    typedef IndexChannel inherited;

  public:
    AudioContentSourceChannel(OutputBehaviour &aOutput) : inherited(aOutput) {};

    virtual DsChannelType getChannelType() { return channeltype_p44_audio_content_source; }; ///< the dS channel type
    virtual const char *getName() { return "contentsource"; };

  };
  typedef boost::intrusive_ptr<AudioContentSourceChannel> AudioContentSourceChannelPtr;



  /// A concrete class implementing the Scene object for a audio device, having a volume channel plus a index value (for specific song/sound effects)
  /// @note subclasses can implement more parameters
  class AudioScene : public SimpleCmdScene
  {
    typedef SimpleCmdScene inherited;

  public:
    AudioScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo);

    /// @name audio scene specific values
    /// @{

    uint32_t contentSource; ///< the index of a content source, e.g. a song/sound effect from a list
    DsAudioPowerState powerState; ///< the power state of the audio device

    /// @}

    /// Set default scene values for a specified scene number
    /// @param aSceneNo the scene number to set default values
    virtual void setDefaultSceneValues(SceneNo aSceneNo);

    // scene values implementation
    virtual double sceneValue(size_t aChannelIndex);
    virtual void setSceneValue(size_t aChannelIndex, double aValue);

    // query flags
    bool hasFixVol();
    bool isMessage();
    bool isPriorityMessage();
    bool isInterruptible();
    bool hasPausedRestore();

  protected:

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

  };
  typedef boost::intrusive_ptr<AudioScene> AudioScenePtr;



  /// the persistent parameters of a audio scene device (including scene table)
  /// @note subclasses can implement more parameters
  class AudioDeviceSettings : public CmdSceneDeviceSettings
  {
    typedef CmdSceneDeviceSettings inherited;

  public:
    AudioDeviceSettings(Device &aDevice);

    /// factory method to create the correct subclass type of DsScene
    /// @param aSceneNo the scene number to create a scene object for.
    /// @note setDefaultSceneValues() must be called to set default scene values
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);

  };


  /// Implements the behaviour of a digitalSTROM Audio device
  class AudioBehaviour : public OutputBehaviour
  {
    typedef OutputBehaviour inherited;


    /// @name hardware derived parameters (constant during operation)
    /// @{
    /// @}


    /// @name persistent settings
    /// @{
    /// @}


    /// @name internal volatile state
    /// @{
    /// @}


  public:

    AudioBehaviour(Device &aDevice);

    /// device type identifier
    /// @return constant identifier for this type of behaviour
    virtual const char *behaviourTypeIdentifier() { return "audio"; };

    /// the volume channel
    AudioVolumeChannelPtr volume;
    /// the power state channel
    AudioPowerStateChannelPtr powerState;
    /// the content source channel
    AudioContentSourceChannelPtr contentSource;

    /// @name interaction with digitalSTROM system
    /// @{

    /// check for presence of model feature (flag in dSS visibility matrix)
    /// @param aFeatureIndex the feature to check for
    /// @return yes if this output behaviour has the feature, no if (explicitly) not, undefined if asked entity does not know
    virtual Tristate hasModelFeature(DsModelFeatures aFeatureIndex);

    /// apply scene to output channels
    /// @param aScene the scene to apply to output channels
    /// @return true if apply is complete, i.e. everything ready to apply to hardware outputs.
    ///   false if scene cannot yet be applied to hardware, and will be performed later
    /// @note this derived class' applyScene only implements special hard-wired behaviour specific scenes
    ///   basic scene apply functionality is provided by base class' implementation already.
    virtual bool applyScene(DsScenePtr aScene);

    /// perform special scene actions (like flashing) which are independent of dontCare flag.
    /// @param aScene the scene that was called (if not dontCare, applyScene() has already been called)
    /// @param aDoneCB will be called when scene actions have completed (but not necessarily when stopped by stopActions())
    virtual void performSceneActions(DsScenePtr aScene, SimpleCB aDoneCB);

    /// will be called to stop all ongoing actions before next callScene etc. is issued.
    /// @note this must stop all ongoing actions such that applying another scene or action right afterwards
    ///   cannot mess up things.
    virtual void stopActions();

    /// check if this channel of this device is allowed to dim now (for lights, this will prevent dimming lights that are off)
    /// @param aChannelType the channel to check
    virtual bool canDim(DsChannelType aChannelType);

    /// identify the device to the user in a behaviour-specific way
    /// @note implemented as blinking for LightBehaviour
    virtual void identifyToUser();

    /// @}


    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  protected:

    /// called by applyScene to load channel values from a scene.
    /// @param aScene the scene to load channel values from
    /// @note Scenes don't have 1:1 representation of all channel values for footprint and logic reasons, so this method
    ///   is implemented in the specific behaviours according to the scene layout for that behaviour.
    virtual void loadChannelsFromScene(DsScenePtr aScene);

    /// called by captureScene to save channel values to a scene.
    /// @param aScene the scene to save channel values to
    /// @note Scenes don't have 1:1 representation of all channel values for footprint and logic reasons, so this method
    ///   is implemented in the specific behaviours according to the scene layout for that behaviour.
    /// @note call markDirty on aScene in case it is changed (otherwise captured values will not be saved)
    virtual void saveChannelsToScene(DsScenePtr aScene);

  };

  typedef boost::intrusive_ptr<AudioBehaviour> AudioBehaviourPtr;

} // namespace p44

#endif /* defined(__vdcd__audiobehaviour__) */
