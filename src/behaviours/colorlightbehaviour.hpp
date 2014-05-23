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

#ifndef __vdcd__colorlightbehaviour__
#define __vdcd__colorlightbehaviour__

#include "device.hpp"
#include "dsscene.hpp"
#include "lightbehaviour.hpp"
#include "auxchannelbehaviour.hpp"

using namespace std;

namespace p44 {


  typedef enum {
    ColorLightModeNone, ///< no color information stored, only brightness
    ColorLightModeHueSaturation, ///< "hs" - hue & saturation
    ColorLightModeXY, ///< "xy" - CIE color space coordinates
    ColorLightModeCt, ///< "ct" - Mired color temperature: 153 (6500K) to 500 (2000K)
  } ColorLightMode;

  class ColorLightScene : public LightScene
  {
    typedef LightScene inherited;
  public:
    ColorLightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo); ///< constructor, sets values according to dS specs' default values

    /// @name color light scene specific values
    /// @{

    ColorLightMode colorMode; ///< color mode (hue+Saturation or CIE xy or color temperature)
    double XOrHueOrCt; ///< X or hue or ct, depending on colorMode
    double YOrSat; ///< Y or saturation, depending on colorMode

    /// @}

    /// Set default scene values for a specified scene number
    /// @param aSceneNo the scene number to set default values
    virtual void setDefaultSceneValues(SceneNo aSceneNo);

    // scene values implementation
    virtual double sceneValue(size_t aOutputIndex);
    virtual void setSceneValue(size_t aOutputIndex, double aValue);

  protected:

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);

  };
  typedef boost::intrusive_ptr<ColorLightScene> ColorLightScenePtr;



  /// the persistent parameters of a light scene device (including scene table)
  class ColorLightDeviceSettings : public LightDeviceSettings
  {
    typedef LightDeviceSettings inherited;

  public:
    ColorLightDeviceSettings(Device &aDevice);

  protected:

    /// factory method to create the correct subclass type of DsScene
    /// @param aSceneNo the scene number to create a scene object for.
    /// @note setDefaultSceneValues() must be called to set default scene values
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);

  };


  class ColorLightBehaviour : public LightBehaviour
  {
    typedef LightBehaviour inherited;


    /// @name hardware derived parameters (constant during operation)
    /// @{
    /// @}


    /// @name persistent settings
    /// @{
    /// @}


    /// @name internal volatile state
    /// @{
    /// @}


    /// @name auxiliary behaviours
    /// @{
    AuxiliaryChannelBehaviourPtr hue;
    AuxiliaryChannelBehaviourPtr saturation;
    AuxiliaryChannelBehaviourPtr ct;
    AuxiliaryChannelBehaviourPtr cieX;
    AuxiliaryChannelBehaviourPtr cieY;
    /// @}



  public:
    ColorLightBehaviour(Device &aDevice);


    /// create and add auxiliary channels to the device
    /// @note this is called after adding an output channel to a device
    ///   and is intended for autocreating needed auxiliary channels like hsb/rgb/ct for color lights
    virtual void createAuxChannels();


    /// @name interface towards actual device hardware (or simulation)
    /// @{

//    /// Get the current logical brightness
//    /// @return 0..255, linear brightness as perceived by humans (half value = half brightness)
//    Brightness getLogicalBrightness();
//
//    /// @return true if device is dimmable
//    bool isDimmable() { return outputFunction==outputFunction_dimmer && outputMode==outputmode_gradual; };
//
//    /// set new brightness
//    /// @param aBrightness 0..255, linear brightness as perceived by humans (half value = half brightness)
//    /// @param aTransitionTimeUp time in microseconds to be spent on transition from current to higher new logical brightness
//    /// @param aTransitionTimeDown time in microseconds to be spent on transition from current to lower new logical brightness
//    ///   if not specified or <0, aTransitionTimeUp is used for both directions
//    void setLogicalBrightness(Brightness aBrightness, MLMicroSeconds aTransitionTimeUp, MLMicroSeconds aTransitionTimeDown=-1);
//
//    /// update logical brightness from actual output state
//    void updateLogicalBrightnessFromOutput();
//
//    /// initialize behaviour with actual device's brightness parameters
//    /// @param aMin minimal brightness that can be set
//    /// @param aMax maximal brightness that can be set
//    /// @note brightness: 0..255, linear brightness as perceived by humans (half value = half brightness)
//    void initBrightnessParams(Brightness aMin, Brightness aMax);

    /// @}


    /// @name interaction with digitalSTROM system
    /// @{

    /// perform special scene actions (like flashing) which are independent of dontCare flag.
    /// @param aScene the scene that was called (if not dontCare, applyScene() has already been called)
    virtual void performSceneActions(DsScenePtr aScene);

    /// capture current state into passed scene object
    /// @param aScene the scene object to update
    /// @param aDoneCB will be called when capture is complete
    /// @note call markDirty on aScene in case it is changed (otherwise captured values will not be saved)
    virtual void captureScene(DsScenePtr aScene, DoneCB aDoneCB);

    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  protected:

    /// called by applyScene to actually recall a scene from the scene table
    /// This allows lights with more parameters than just brightness (e.g. color lights) to recall
    /// additional values that were saved as captureScene()
    virtual void recallScene(LightScenePtr aLightScene);

  private:

  };

  typedef boost::intrusive_ptr<ColorLightBehaviour> ColorLightBehaviourPtr;

} // namespace p44

#endif /* defined(__vdcd__colorlightbehaviour__) */
