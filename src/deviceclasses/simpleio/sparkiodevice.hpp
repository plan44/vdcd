//
//  Copyright (c) 2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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


#ifndef __vdcd__sparkiodevice__
#define __vdcd__sparkiodevice__

#include "device.hpp"

#include "jsonwebclient.hpp"
#include "lightbehaviour.hpp"

using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class SparkIoDevice;

  class SparkLightScene : public LightScene
  {
    typedef LightScene inherited;
  public:
    SparkLightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo); ///< constructor, sets values according to dS specs' default values

    /// @name spark core based light scene specific values
    /// @{

    uint32_t extendedState; // extended state (beyond brightness) of the spark core light

    /// @}

    /// Set default scene values for a specified scene number
    /// @param aSceneNo the scene number to set default values
    virtual void setDefaultSceneValues(SceneNo aSceneNo);

  protected:

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);

    // property access implementation
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

  };
  typedef boost::intrusive_ptr<SparkLightScene> SparkLightScenePtr;


  class SparkLightBehaviour : public LightBehaviour
  {
    typedef LightBehaviour inherited;

  public:

    SparkLightBehaviour(Device &aDevice);

    /// capture current state into passed scene object
    /// @param aScene the scene object to update
    /// @param aCompletedCB will be called when capture is complete
    /// @note call markDirty on aScene in case it is changed (otherwise captured values will not be saved)
    virtual void captureScene(DsScenePtr aScene, DoneCB aDoneCB);

  protected:

    /// called by applyScene to actually recall a scene from the scene table
    /// This allows lights with more parameters than just brightness (e.g. color lights) to recall
    /// additional values that were saved at captureScene()
    virtual void recallScene(LightScenePtr aLightScene);

  private:

    void sceneStateReceived(SparkLightScenePtr aSparkScene, DoneCB aDoneCB, JsonObjectPtr aJsonResponse, ErrorPtr aError);

  };
  typedef boost::intrusive_ptr<SparkLightBehaviour> SparkLightBehaviourPtr;



  /// the persistent parameters of a light scene device (including scene table)
  class SparkDeviceSettings : public LightDeviceSettings
  {
    typedef LightDeviceSettings inherited;

  public:
    SparkDeviceSettings(Device &aDevice);

  protected:

    /// factory method to create the correct subclass type of DsScene with default values
    /// @param aSceneNo the scene number to create a scene object with proper default values for.
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);
    
  };
  
  

  typedef boost::intrusive_ptr<SparkIoDevice> SparkIoDevicePtr;
  class SparkIoDevice : public Device
  {
    typedef Device inherited;
    friend class SparkLightBehaviour;

    int32_t outputValue;
    string sparkCoreID;
    string sparkCoreToken;
    JsonWebClient sparkCloudComm;
    int apiVersion;
    bool outputChangePending;
    SparkLightScenePtr pendingSparkScene;

  public:
    SparkIoDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// initializes the physical device for being used
    /// @param aFactoryReset if set, the device will be inititalized as thoroughly as possible (factory reset, default settings etc.)
    /// @note this is called before interaction with dS system starts (usually just after collecting devices)
    /// @note implementation should call inherited when complete, so superclasses could chain further activity
    virtual void initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset);

    /// check presence of this addressable
    /// @param aPresenceResultHandler will be called to report presence status
    virtual void checkPresence(PresenceCB aPresenceResultHandler);

    /// set new output value on device
    /// @param aOutputBehaviour the output behaviour which has a new output value to be sent to the hardware output
    /// @note depending on how the actual device communication works, the implementation might need to consult all
    ///   output behaviours to collect data for an outgoing message.
    virtual void updateOutputValue(OutputBehaviour &aOutputBehaviour);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "spark core based device"; }

    /// @}

  protected:

    void deriveDsUid();

  private:

    bool sparkApiCall(JsonWebClientCB aResponseCB, string aArgs);

    void apiVersionReceived(CompletedCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aJsonResponse, ErrorPtr aError);
    void presenceStateReceived(PresenceCB aPresenceResultHandler, JsonObjectPtr aDeviceInfo, ErrorPtr aError);

    void postOutputValue(OutputBehaviour &aOutputBehaviour);
    void outputChanged(OutputBehaviour &aOutputBehaviour, JsonObjectPtr aJsonResponse, ErrorPtr aError);

  };
  
} // namespace p44

#endif /* defined(__vdcd__sparkiodevice__) */
