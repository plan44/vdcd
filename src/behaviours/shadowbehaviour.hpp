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

#ifndef __vdcd__shadowbehaviour__
#define __vdcd__shadowbehaviour__

#include "device.hpp"
#include "simplescene.hpp"
#include "outputbehaviour.hpp"

using namespace std;

namespace p44 {

  class ShadowPositionChannel : public ChannelBehaviour
  {
    typedef ChannelBehaviour inherited;

    MLMicroSeconds fullRangeTime;

  public:
    ShadowPositionChannel(OutputBehaviour &aOutput) : inherited(aOutput)
    {
      resolution = 100/65536; // position defaults to historic dS 1/65536 of full scale resolution
      fullRangeTime = 50*Second; // just an average blind full range time
    };

    /// Set time it takes to run trough a full range (0..100%), approximately
    void setFullRangeTime(MLMicroSeconds aFullRangeTime) { fullRangeTime = aFullRangeTime; };

    virtual DsChannelType getChannelType() { return channeltype_position_v; }; ///< the dS channel type
    virtual const char *getName() { return "position"; };
    virtual double getMin() { return 0; }; // dS position goes from 0 to 100%
    virtual double getMax() { return 100; };
    virtual double getDimPerMS() { return (getMax()-getMin())*1000.0/fullRangeTime; }; // dimming is such that it goes from min..max in fullRangeTime

  };
  typedef boost::intrusive_ptr<ShadowPositionChannel> ShadowPositionChannelPtr;


  class ShadowAngleChannel : public ChannelBehaviour
  {
    typedef ChannelBehaviour inherited;

    MLMicroSeconds fullRangeTime;

  public:
    ShadowAngleChannel(OutputBehaviour &aOutput) : inherited(aOutput)
    {
      resolution = 100/65536; // position defaults to historic dS 1/65536 of full scale resolution
      fullRangeTime = 1.5*Second; // just an average blind angle turn time
    };

    /// Set time it takes to run trough a full range (0..100%), approximately
    void setFullRangeTime(MLMicroSeconds aFullRangeTime) { fullRangeTime = aFullRangeTime; };

    virtual DsChannelType getChannelType() { return channeltype_position_angle; }; ///< the dS channel type
    virtual const char *getName() { return "angle"; };
    virtual double getMin() { return 0; }; // dS position goes from 0 to 100%
    virtual double getMax() { return 100; };
    virtual double getDimPerMS() { return (getMax()-getMin())*1000.0/fullRangeTime; }; // dimming is such that it goes from min..max in fullRangeTime

  };
  typedef boost::intrusive_ptr<ShadowAngleChannel> ShadowAngleChannelPtr;



  class ShadowScene : public SimpleScene
  {
    typedef SimpleScene inherited;

  public:
    ShadowScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo); ///< constructor, sets values according to dS specs' default values

    /// @name shadow scene specific values
    /// @{

    double angle; ///< shadow device angle

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
  typedef boost::intrusive_ptr<ShadowScene> ShadowScenePtr;



  /// the persistent parameters of a light scene device (including scene table)
  /// @note subclasses can implement more parameters, like for example ColorLightDeviceSettings for color lights.
  class ShadowDeviceSettings : public SceneDeviceSettings
  {
    typedef SceneDeviceSettings inherited;

  public:
    ShadowDeviceSettings(Device &aDevice);

    /// factory method to create the correct subclass type of DsScene
    /// @param aSceneNo the scene number to create a scene object for.
    /// @note setDefaultSceneValues() must be called to set default scene values
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);

  };


  typedef enum {
    shadowdevice_rollerblind,
    shadowdevice_jalousie,
    shadowdevice_sunblind
  } ShadowDeviceKind;


  /// Implements the behaviour of a digitalSTROM Light device, such as maintaining the logical brightness,
  /// dimming and alert (blinking) functions.
  class ShadowBehaviour : public OutputBehaviour
  {
    typedef OutputBehaviour inherited;


    /// @name hardware derived parameters (constant during operation)
    /// @{
    ShadowDeviceKind shadowDeviceKind;
    /// @}


    /// @name persistent settings
    /// @{
    MLMicroSeconds openTime;
    MLMicroSeconds closeTime;
    MLMicroSeconds angleOpenTime;
    MLMicroSeconds angleCloseTime;
    /// @}


    /// @name internal volatile state
    /// @{
    /// @}


  public:
    ShadowBehaviour(Device &aDevice);

    /// device type identifier
    /// @return constant identifier for this type of behaviour
    virtual const char *behaviourTypeIdentifier() { return "shadow"; };

    /// the channels
    ShadowPositionChannelPtr position;
    ShadowAngleChannelPtr angle;

    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// set kind (roller, jalousie, etc.) of shadow device
    /// @param aShadowDeviceKind kind of device
    void setDeviceKind(ShadowDeviceKind aShadowDeviceKind) { shadowDeviceKind = aShadowDeviceKind; };

    /// returns the direction the blind should move right now
    /// @return 0=stopped, -1=moving down, +1=moving up
    int currentMovingDirection();

    /// @}


    /// @name interaction with digitalSTROM system
    /// @{

    /// check for presence of model feature (flag in dSS visibility matrix)
    /// @param aFeatureIndex the feature to check for
    /// @return true if this output behaviour has the feature (which means dSS Configurator must provide UI for it)
    virtual bool hasModelFeature(DsModelFeatures aFeatureIndex);

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


    // property access implementation for descriptor/settings/states
    //virtual int numDescProps();
    //virtual const PropertyDescriptorPtr getDescDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    virtual int numSettingsProps();
    virtual const PropertyDescriptorPtr getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    //virtual int numStateProps();
    //virtual const PropertyDescriptorPtr getStateDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    // combined field access for all types of properties
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

  private:

  };

  typedef boost::intrusive_ptr<ShadowBehaviour> ShadowBehaviourPtr;

} // namespace p44

#endif /* defined(__vdcd__shadowbehaviour__) */
