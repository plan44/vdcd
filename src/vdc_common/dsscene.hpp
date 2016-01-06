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

#ifndef __vdcd__dsscene__
#define __vdcd__dsscene__


#include "persistentparams.hpp"
#include "propertycontainer.hpp"

#include "devicesettings.hpp"

using namespace std;

namespace p44 {

  /// scene number
  typedef uint8_t SceneNo;

  /// area this scene applies to (usually derives from SceneNo, 0 if scene is not an area scene)
  typedef uint8_t SceneArea;

  /// scene commands
  typedef enum {
    scene_cmd_none, ///< no command, reserved scene
    scene_cmd_invoke, ///< standard scene invoke behaviour, i.e. load channel values, apply effects if any
    scene_cmd_off, ///< standard off behaviour (usually equivalent to invoke, but might have slightly different semantics in certain behaviours)
    scene_cmd_min, ///< standard min behaviour (usually equivalent to invoke, but might have slightly different semantics in certain behaviours)
    scene_cmd_max, ///< standard max behaviour (usually equivalent to invoke, but might have slightly different semantics in certain behaviours)
    scene_cmd_increment, ///< increment, dim up
    scene_cmd_decrement, ///< decrement, dim down
    scene_cmd_area_continue, ///< special case: continue last area increment/decrement
    scene_cmd_stop, ///< stop
    scene_cmd_slow_off, ///< slow motion off
    scene_cmd_audio_repeat_off, ///< audio: repeat off
    scene_cmd_audio_repeat_1, ///< audio: repeat 1
    scene_cmd_audio_repeat_all, ///< audio: repeat all
    scene_cmd_audio_previous_title, ///< audio: Previous Title
    scene_cmd_audio_next_title, ///< audio: Next Title
    scene_cmd_audio_previous_channel, ///< audio: Previous Channel
    scene_cmd_audio_next_channel, ///< audio: Next Channel
    scene_cmd_audio_mute, ///< audio: Mute
    scene_cmd_audio_unmute, ///< audio: Unmute
    scene_cmd_audio_play, ///< audio: Play
    scene_cmd_audio_pause, ///< audio: Pause
    scene_cmd_audio_shuffle_off, ///< audio: Shuffle Off
    scene_cmd_audio_shuffle_on, ///< audio: Shuffle On
    scene_cmd_audio_resume_off, ///< audio: Resume Off
    scene_cmd_audio_resume_on, ///< audio: Resume On
    scene_cmd_heating_summer_mode, ///< heating: Switch to summer mode
    scene_cmd_heating_winter_mode, ///< heating: Switch to winter mode
    scene_cmd_heating_valve_prophylaxis, ///< heating: Valve prophylaxis
  } SceneCmd;
  


  /// per scene value flags as represented in sceneValueFlags
  enum {
    valueflags_dontCare = 0x0001, ///< if set, value of this channel/output will not be recalled with scene
  };


  class SceneDeviceSettings;
  class Device;
  class DeviceSettings;

  class OutputBehaviour;
  typedef boost::intrusive_ptr<OutputBehaviour> OutputBehaviourPtr;

  /// Abstract base class for a single entry of a device's scene table. Implements the basic persistence
  /// and property access mechanisms which can be extended in concrete subclasses.
  /// @note concrete subclasses for standard dS behaviours exist as part of the behaviour implementation
  ///   (such as light, colorlight) - so usually device makers don't need to implement subclasses of DsScene.
  /// @note DsScene objects are managed by the SceneDeviceSettings container class in a way that tries
  ///   to minimize the number of actual DsScene objects in memory for efficiency reasons. So
  ///   most DsScene objects are created on the fly via the newDefaultScene() factory method when
  ///   used. Also, only scenes explicitly configured to differ from the standard scene values for
  ///   the behaviour are actually persisted into the database.
  class DsScene : public PropertyContainer, public PersistentParams
  {
    typedef PersistentParams inheritedParams;
    typedef PropertyContainer inheritedProps;

    friend class SceneDeviceSettings;

    SceneDeviceSettings &sceneDeviceSettings;

  protected:

    /// generic DB persisted scene flag word, can be used by subclasses to map flags onto in loadFromRow() and bindToStatement()
    /// @note base class already maps some flags, see commonflags_xxx enum in implementation.
    uint32_t globalSceneFlags;

    /// sets or resets global scene flag indicated by mask, sets dirty if global flags actually changed
    void setGlobalSceneFlag(uint32_t aMask, bool aNewValue);

  public:
    DsScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo); ///< constructor, creates empty scene
    virtual ~DsScene() {}; // important for multiple inheritance!

    /// @name common scene values (available in all scene objects)
    /// @{

    SceneNo sceneNo; ///< scene number
    SceneCmd sceneCmd; ///< scene command
    SceneArea sceneArea; ///< scene area, 0 if none

    /// @}


    /// @name access to scene level flags
    /// @{

    /// check if scene is dontCare (on the scene level, regardless of individual value's dontCare)
    /// @return dontCare status
    bool isDontCare();

    /// set if scene should have dontCare status on scene level
    /// @param aDontCare new flag state
    void setDontCare(bool aDontCare);

    /// check if scene ignores local priority
    /// @return dontCare status
    bool ignoresLocalPriority();

    /// set if scene should ignore local priority
    /// @param aIgnoreLocalPriority new flag state
    void setIgnoreLocalPriority(bool aIgnoreLocalPriority);

    /// @}


    /// @name access to scene values (1 or more for MOC)
    /// @{

    /// number of scene values (=usually number of outputs/channels of device )
    /// @return number of scene values
    virtual int numSceneValues();

    /// get per-value scene flags
    /// @param aChannelIndex the channel index (0=primary channel, 1..n other channels)
    /// @return the flag word
    virtual uint32_t sceneValueFlags(size_t aChannelIndex);

    /// modify per-value scene flags
    /// @param aChannelIndex the channel index (0=primary channel, 1..n other channels)
    /// @param aFlagMask the flags to set or clear
    /// @param aSet if true, flags set in aFlagMask will be set, otherwise cleared
    /// @note marks scene dirty if flags are actually changed
    virtual void setSceneValueFlags(size_t aChannelIndex, uint32_t aFlagMask, bool aSet);

    /// get scene value
    /// @param aChannelIndex the channel index (0=primary channel, 1..n other channels)
    /// @return the scene value
    virtual double sceneValue(size_t aChannelIndex) = 0;

    /// modify scene value
    /// @param aChannelIndex the channel index (0=primary channel, 1..n other channels)
    /// @param aValue the new scene value
    /// @note marks scene dirty if value is actually changed
    virtual void setSceneValue(size_t aChannelIndex, double aValue) = 0;

    /// utility: check a scene value flag
    /// @param aChannelIndex the channel index (0=primary channel, 1..n other channels)
    /// @param aFlagMask the flag to check
    bool isSceneValueFlagSet(size_t aChannelIndex, uint32_t aFlagMask);

    /// @}

    /// get device
    /// @return the device this scene belongs to
    Device &getDevice();

    /// get device
    /// @return the output behaviour controlled by this scene
    OutputBehaviourPtr getOutputBehaviour();

    /// Set default scene values for a specified scene number
    /// @param aSceneNo the scene number to set default values
    virtual void setDefaultSceneValues(SceneNo aSceneNo);

  protected:

    // property access implementation
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyContainerPtr getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain);
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

    // persistence implementation
    virtual const char *tableName() = 0;
    virtual size_t numKeyDefs();
    virtual const FieldDefinition *getKeyDef(size_t aIndex);
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

  private:

    PropertyContainerPtr sceneChannels; // private container for implementing scene channels/outputs

  };
  typedef boost::intrusive_ptr<DsScene> DsScenePtr;
  typedef map<SceneNo, DsScenePtr> DsSceneMap;



  /// Abstract base class for the persistent parameters of a device with a scene table
  /// @note concrete subclasses for standard dS behaviours exist as part of the behaviour implementation
  ///   (such as light, colorlight) - so usually device makers don't need to implement subclasses of SceneDeviceSettings.
  /// @note The SceneDeviceSettings object manages the scene table in a way that tries
  ///   to minimize the number of actual DsScene objects in memory for efficiency reasons. So
  ///   most DsScene objects are created on the fly via the newDefaultScene() factory method only when
  ///   needed e.g. for calling a scene. Only scenes that were explicitly configured to differ from the
  ///   standard scene values for the behaviour are actually persisted into the database.
  class SceneDeviceSettings : public DeviceSettings
  {
    typedef DeviceSettings inherited;

    friend class DsScene;
    friend class Device;
    friend class SceneChannels;

    DsSceneMap scenes; ///< the user defined scenes (default scenes will be created on the fly)

  public:
    SceneDeviceSettings(Device &aDevice);


    /// @name Access scenes
    /// @{

    /// get the parameters for the scene
    /// @param aSceneNo the scene to get current settings for.
    /// @note the object returned may not be attached to a container (if it is a default scene
    ///   created on the fly). Scene modifications must be posted using updateScene()
    DsScenePtr getScene(SceneNo aSceneNo);

    /// update scene (mark dirty, add to list of non-default scene objects)
    /// @param aSceneNo the scene to save modified settings for.
    /// @note call updateScene only if scene values are changed from defaults, because
    ///   updating a scene creates DB records and needs more run-time memory.
    /// @note always updates the scene and causes write to DB even if scene was not marked dirty already
    void updateScene(DsScenePtr aScene);

    /// reset scene to default values
    /// @param aSceneNo the scene to revert to default values
    /// @note database records will be deleted if the scene had non-default values before.
    void resetScene(SceneNo aSceneNo);

    /// factory method to create the correct subclass type of DsScene with default values
    /// @param aSceneNo the scene number to create a scene object with proper default values for.
    /// @note this method can be derived in concrete subclasses to return the appropriate scene object.
    ///   Base class returns a SimpleScene.
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);

    /// @}

  protected:

    // persistence implementation
    virtual ErrorPtr loadChildren();
    virtual ErrorPtr saveChildren();
    virtual ErrorPtr deleteChildren();
    
    /// @}
  };
  typedef boost::intrusive_ptr<SceneDeviceSettings> SceneDeviceSettingsPtr;


} // namespace p44


#endif /* defined(__vdcd__dsscene__) */
