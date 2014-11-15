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

#ifndef __vdcd__movinglightbehaviour__
#define __vdcd__movinglightbehaviour__

#include "device.hpp"
#include "dsscene.hpp"
#include "colorlightbehaviour.hpp"

using namespace std;

namespace p44 {


  class VPosChannel : public ChannelBehaviour
  {
    typedef ChannelBehaviour inherited;

  public:
    VPosChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 0.01; /* arbitrary */ };

    virtual DsChannelType getChannelType() { return channeltype_position_v; }; ///< the dS channel type
    virtual const char *getName() { return "vertical position"; };
    virtual double getMin() { return 0; }; // position goes from 0 to 100%
    virtual double getMax() { return 100; };
    virtual double getDimPerMS() { return 100/FULL_SCALE_DIM_TIME_MS; }; // dimming through full scale should be FULL_SCALE_DIM_TIME_MS
  };


  class HPosChannel : public ChannelBehaviour
  {
    typedef ChannelBehaviour inherited;

  public:
    HPosChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 0.01; /* arbitrary */ };

    virtual DsChannelType getChannelType() { return channeltype_position_h; }; ///< the dS channel type
    virtual const char *getName() { return "horizontal position"; };
    virtual double getMin() { return 0; }; // position goes from 0 to 100%
    virtual double getMax() { return 100; };
    virtual double getDimPerMS() { return 100/FULL_SCALE_DIM_TIME_MS; }; // dimming through full scale should be FULL_SCALE_DIM_TIME_MS
  };



  class MovingLightScene : public ColorLightScene
  {
    typedef ColorLightScene inherited;
    
  public:
    MovingLightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo); ///< constructor, sets values according to dS specs' default values

    /// @name moving light scene specific values
    /// @{

    double hPos; ///< horizontal position
    double vPos; ///< vertical position

    /// @}

    /// Set default scene values for a specified scene number
    /// @param aSceneNo the scene number to set default values
    virtual void setDefaultSceneValues(SceneNo aSceneNo);

    // scene values implementation
    virtual double sceneValue(size_t aChannelIndex);
    virtual void setSceneValue(size_t aChannelIndex, double aValue);

  protected:

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

  };
  typedef boost::intrusive_ptr<MovingLightScene> MovingLightScenePtr;



  /// the persistent parameters of a light scene device (including scene table)
  class MovingLightDeviceSettings : public ColorLightDeviceSettings
  {
    typedef ColorLightDeviceSettings inherited;

  public:
    MovingLightDeviceSettings(Device &aDevice);

    /// factory method to create the correct subclass type of DsScene
    /// @param aSceneNo the scene number to create a scene object for.
    /// @note setDefaultSceneValues() must be called to set default scene values
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);

  };


  class MovingLightBehaviour : public RGBColorLightBehaviour
  {
    typedef RGBColorLightBehaviour inherited;

  public:

    /// @name channels
    /// @{
    ChannelBehaviourPtr horizontalPosition;
    ChannelBehaviourPtr verticalPosition;
    /// @}


    MovingLightBehaviour(Device &aDevice);

    /// device type identifier
    /// @return constant identifier for this type of behaviour
    virtual const char *behaviourTypeIdentifier() { return "movingcolorlight"; };

    /// check for presence of model feature (flag in dSS visibility matrix)
    /// @param aFeatureIndex the feature to check for
    /// @return true if this output behaviour has the feature (which means dSS Configurator must provide UI for it)
    virtual bool hasModelFeature(DsModelFeatures aFeatureIndex);

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

  typedef boost::intrusive_ptr<MovingLightBehaviour> MovingLightBehaviourPtr;


} // namespace p44

#endif /* defined(__vdcd__movinglightbehaviour__) */
