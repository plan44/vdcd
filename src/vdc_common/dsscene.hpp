//
//  Copyright (c) 2013 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

  class SceneDeviceSettings;
  class Device;
  class DeviceSettings;

  class DsScene : public PropertyContainer, public PersistentParams
  {
    typedef PersistentParams inheritedParams;
    typedef PropertyContainer inheritedProps;

    friend class SceneDeviceSettings;

    SceneDeviceSettings &sceneDeviceSettings;

  protected:

    /// generic DB persisted scene flag word, can be used by subclasses to map flags onto in loadFromRow() and bindToStatement()
    /// @note base class already maps some flags, see commonflags_xxx enum in implementation.
    int sceneFlags;

  public:
    DsScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo); ///< constructor, creates empty scene
    virtual ~DsScene() {}; // important for multiple inheritance!

    /// @name common scene values (available in all scene objects)
    /// @{

    SceneNo sceneNo; ///< scene number

    // flags mapped into sceneFlags for storage
    bool dontCare; ///< if set, applying this scene does not change the output value(s). This is used for configuration of areas
    bool ignoreLocalPriority; ///< if set, local priority is ignored when calling this scene

    /// @}

  protected:

    // property access implementation
    virtual int numProps(int aDomain);
    virtual const PropertyDescriptor *getPropertyDescriptor(int aPropIndex, int aDomain);
    virtual bool accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);

    // persistence implementation
    virtual const char *tableName() = 0;
    virtual size_t numKeyDefs();
    virtual const FieldDefinition *getKeyDef(size_t aIndex);
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);

    /// @}
  };
  typedef boost::intrusive_ptr<DsScene> DsScenePtr;
  typedef map<SceneNo, DsScenePtr> DsSceneMap;



  /// the persistent parameters of a device with a scene table
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
