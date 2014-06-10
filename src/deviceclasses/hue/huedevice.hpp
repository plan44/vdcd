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

#ifndef __vdcd__huedevice__
#define __vdcd__huedevice__

#include "device.hpp"

#include "lightbehaviour.hpp"

#include "jsonobject.hpp"

using namespace std;

namespace p44 {

  class HueDeviceContainer;
  class HueDevice;
  class HueComm;

  typedef enum {
    hueColorModeNone, ///< no color information stored, only brightness
    hueColorModeHueSaturation, ///< "hs" - hue & saturation
    hueColorModeXY, ///< "xy" - CIE color space coordinates
    hueColorModeCt, ///< "ct" - Mired color temperature: 153 (6500K) to 500 (2000K) for hue Lights
  } HueColorMode;



  class HueLightScene : public LightScene
  {
    typedef LightScene inherited;
  public:
    HueLightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo); ///< constructor, sets values according to dS specs' default values

    /// @name hue light scene specific values
    /// @{

    HueColorMode colorMode; ///< color mode (hue+Saturation or CIE xy or color temperature)
    double XOrHueOrCt; ///< X or hue or ct, depending on colorMode
    double YOrSat; ///< Y or saturation, depending on colorMode

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
  typedef boost::intrusive_ptr<HueLightScene> HueLightScenePtr;


  class HueLightBehaviour : public LightBehaviour
  {
    typedef LightBehaviour inherited;

  public:

    HueLightBehaviour(Device &aDevice);

    /// capture current state into passed scene object
    /// @param aScene the scene object to update
    /// @param aCompletedCB will be called when capture is complete
    /// @note call markDirty on aScene in case it is changed (otherwise captured values will not be saved)
    virtual void captureScene(DsScenePtr aScene, DoneCB aDoneCB);

    /// perform special scene actions (like flashing) which are independent of dontCare flag.
    /// @param aScene the scene that was called (if not dontCare, applyScene() has already been called)
    virtual void performSceneActions(DsScenePtr aScene);


  protected:

    /// called by applyScene to actually recall a scene from the scene table
    /// This allows lights with more parameters than just brightness (e.g. color lights) to recall
    /// additional values that were saved as captureScene()
    virtual void recallScene(LightScenePtr aLightScene);

  private:

    void sceneColorsReceived(HueLightScenePtr aHueScene, DoneCB aDoneCB, JsonObjectPtr aDeviceInfo, ErrorPtr aError);


  };
  typedef boost::intrusive_ptr<HueLightBehaviour> HueLightBehaviourPtr;



  /// the persistent parameters of a light scene device (including scene table)
  class HueDeviceSettings : public LightDeviceSettings
  {
    typedef LightDeviceSettings inherited;

  public:
    HueDeviceSettings(Device &aDevice);

  protected:

    /// factory method to create the correct subclass type of DsScene with default values
    /// @param aSceneNo the scene number to create a scene object with proper default values for.
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);
    
  };




  typedef boost::intrusive_ptr<HueDevice> HueDevicePtr;
  class HueDevice : public Device
  {
    typedef Device inherited;
    friend class HueLightBehaviour;

    string lightID; ///< the ID as used in the hue bridge

    // information from the device itself
    string hueModel;

    // scene to update colors from when updating output
    HueLightScenePtr pendingColorScene;

  public:
    HueDevice(HueDeviceContainer *aClassContainerP, const string &aLightID);

    HueDeviceContainer &hueDeviceContainer();
    HueComm &hueComm();

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

    /// set user assignable name
    /// @param new name of the hue device
    /// @note will propagate the name to the hue bridge to name the light itself
    virtual void setName(const string &aName);


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

    /// identify the device to the user
    /// @note for lights, this is usually implemented as a blink operation, but depending on the device type,
    ///   this can be anything.
    virtual void identifyToUser();

    /// disconnect device. For hue, we'll check if the device is still reachable via the bridge, and only if not
    /// we allow disconnection
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDisconnectResultHandler will be called to report true if device could be disconnected,
    ///   false in case it is certain that the device is still connected to this and only this vDC
    virtual void disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler);

    /// set new channel value on device
    /// @param aChannelBehaviour the channel behaviour which has a new output value to be sent to the hardware output
    /// @note depending on how the actual device communication works, the implementation might need to consult all
    ///   channel behaviours to collect data for an outgoing message.
    virtual void updateChannelValue(ChannelBehaviour &aChannelBehaviour);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return hueModel; };

    /// @}


  protected:

    void deriveDsUid();

  private:

    void deviceStateReceived(CompletedCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aDeviceInfo, ErrorPtr aError);
    void presenceStateReceived(PresenceCB aPresenceResultHandler, JsonObjectPtr aDeviceInfo, ErrorPtr aError);
    void disconnectableHandler(bool aForgetParams, DisconnectCB aDisconnectResultHandler, bool aPresent);
    void alertHandler(int aLeftCycles);
    void outputChangeSent(ChannelBehaviour &aChannelBehaviour, ErrorPtr aError);

  };
  
} // namespace p44


#endif /* defined(__vdcd__huedevice__) */
