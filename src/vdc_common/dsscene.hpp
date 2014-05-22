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

#ifndef __vdcd__dsscene__
#define __vdcd__dsscene__


#include "persistentparams.hpp"
#include "propertycontainer.hpp"

#include "devicesettings.hpp"

using namespace std;

namespace p44 {

  #define MAX_SCENE_NO 79 // TODO: this is an arbitrary number so far, to limit the array property. Get a real number for it

  typedef uint8_t SceneNo;

  // per scene value flags as represented in sceneValueFlags
  enum {
    valueflags_dontCare = 0x0001, ///< if set, value of this channel/output will not be recalled with scene
  };


  class SceneDeviceSettings;
  class Device;
  class DeviceSettings;

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

  public:
    DsScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo); ///< constructor, creates empty scene
    virtual ~DsScene() {}; // important for multiple inheritance!

    /// @name common scene values (available in all scene objects)
    /// @{

    SceneNo sceneNo; ///< scene number

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
    /// @param aOutputIndex the output index
    /// @return the flag word
    virtual uint32_t sceneValueFlags(size_t aOutputIndex);

    /// modify per-value scene flags
    /// @param aOutputIndex the output index
    /// @param aFlagMask the flags to set or clear
    /// @param aSet if true, flags set in aFlagMask will be set, otherwise cleared
    virtual void setSceneValueFlags(size_t aOutputIndex, uint32_t aFlagMask, bool aSet);

    /// get scene value
    /// @param aOutputIndex the output index
    /// @return the scene value
    virtual double sceneValue(size_t aOutputIndex) = 0;

    /// modify per-value scene flags
    /// @param aOutputIndex the output index
    /// @param aValue the new scene value
    virtual void setSceneValue(size_t aOutputIndex, double aValue) = 0;

    /// get output index by channelID
    /// @param aChannelID a channe ID
    /// @return 0 for unknown channel, 1..n for channel
    virtual int getChannelId(size_t aOutputIndex);

    /// utility: get output index by channelID
    /// @param aChannelID a channe ID
    /// @return -1 if channel does not exist, output index of that channel otherwise
    size_t getOutputIndexByChannel(int aChannelID);

    /// utility: check a scene value flag
    /// @param aOutputIndex the output index
    /// @param aFlagMask the flag to check
    bool isSceneValueFlagSet(size_t aOutputIndex, uint32_t aFlagMask);

    /// @}

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
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);

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
    void updateScene(DsScenePtr aScene);

    /// reset scene to default values
    /// @param aSceneNo the scene to revert to default values
    /// @note database records will be deleted if the scene had non-default values before.
    void resetScene(SceneNo aSceneNo);

    /// @}

  protected:

    /// factory method to create the correct subclass type of DsScene with default values
    /// @param aSceneNo the scene number to create a scene object with proper default values for.
    /// @note this method must be derived in concrete subclasses to return the appropriate scene object
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo) = 0;

    // persistence implementation
    virtual ErrorPtr loadChildren();
    virtual ErrorPtr saveChildren();
    virtual ErrorPtr deleteChildren();
    
    /// @}
  };
  typedef boost::intrusive_ptr<SceneDeviceSettings> SceneDeviceSettingsPtr;


} // namespace p44


#endif /* defined(__vdcd__dsscene__) */
