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
#include "colorutils.hpp"

using namespace std;

namespace p44 {


  typedef enum {
    colorLightModeNone, ///< no color information stored, only brightness
    colorLightModeHueSaturation, ///< "hs" - hue & saturation
    colorLightModeXY, ///< "xy" - CIE color space coordinates
    colorLightModeCt, ///< "ct" - Mired color temperature: 153 (6500K) to 500 (2000K)
  } ColorLightMode;



  class ColorChannel : public ChannelBehaviour
  {
    typedef ChannelBehaviour inherited;

  public:

    ColorChannel(OutputBehaviour &aOutput) : inherited(aOutput) {};

    virtual ColorLightMode colorMode() = 0;

    /// get current value of this channel - and calculate it if it is not set in the device, but must be calculated from other channels
    virtual double getChannelValueCalculated();

  };


  class HueChannel : public ColorChannel
  {
    typedef ColorChannel inherited;

  public:
    HueChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 0.1; /* 0.1 degree */ };

    virtual DsChannelType getChannelType() { return channeltype_hue; }; ///< the dS channel type
    virtual ColorLightMode colorMode() { return colorLightModeHueSaturation; };
    virtual const char *getName() { return "hue"; };
    virtual double getMin() { return 0; }; // hue goes from 0 to (almost) 360 degrees
    virtual double getMax() { return 358.6; };
    virtual bool wrapsAround() { return true; }; ///< hue wraps around
    virtual double getDimPerMS() { return 360.0/FULL_SCALE_DIM_TIME_MS; }; // dimming through full scale should be FULL_SCALE_DIM_TIME_MS
  };


  class SaturationChannel : public ColorChannel
  {
    typedef ColorChannel inherited;

  public:
    SaturationChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 0.1; /* 0.1 percent */ };

    virtual DsChannelType getChannelType() { return channeltype_saturation; }; ///< the dS channel type
    virtual ColorLightMode colorMode() { return colorLightModeHueSaturation; };
    virtual const char *getName() { return "saturation"; };
    virtual double getMin() { return 0; }; // saturation goes from 0 to 100 percent
    virtual double getMax() { return 100; };
    virtual double getDimPerMS() { return 100.0/FULL_SCALE_DIM_TIME_MS; }; // dimming through full scale should be FULL_SCALE_DIM_TIME_MS
  };


  class ColorTempChannel : public ColorChannel
  {
    typedef ColorChannel inherited;

  public:
    ColorTempChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 1; /* 1 mired */ };

    virtual DsChannelType getChannelType() { return channeltype_colortemp; }; ///< the dS channel type
    virtual ColorLightMode colorMode() { return colorLightModeCt; };
    virtual const char *getName() { return "color temperature"; };
    virtual double getMin() { return 100; }; // CT goes from 100 to 1000 mired (10000K to 1000K)
    virtual double getMax() { return 1000; };
    virtual double getDimPerMS() { return 900.0/FULL_SCALE_DIM_TIME_MS; }; // dimming through full scale should be
  };


  class CieXChannel : public ColorChannel
  {
    typedef ColorChannel inherited;

  public:
    CieXChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 0.01; /* 1% of full scale */ };

    virtual DsChannelType getChannelType() { return channeltype_cie_x; }; ///< the dS channel type
    virtual ColorLightMode colorMode() { return colorLightModeXY; };
    virtual const char *getName() { return "CIE X"; };
    virtual double getMin() { return 0; }; // CIE x and y have 0..1 range
    virtual double getMax() { return 1; };
    virtual double getDimPerMS() { return 1.0/FULL_SCALE_DIM_TIME_MS; }; // dimming through full scale should be
  };


  class CieYChannel : public ColorChannel
  {
    typedef ColorChannel inherited;

  public:
    CieYChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 0.01; /* 1% of full scale */ };

    virtual DsChannelType getChannelType() { return channeltype_cie_y; }; ///< the dS channel type
    virtual ColorLightMode colorMode() { return colorLightModeXY; };
    virtual const char *getName() { return "CIE Y"; };
    virtual double getMin() { return 0; }; // CIE x and y have 0..1 range
    virtual double getMax() { return 1; };
    virtual double getDimPerMS() { return 1.0/FULL_SCALE_DIM_TIME_MS; }; // dimming through full scale should be
  };




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
    virtual double sceneValue(size_t aChannelIndex);
    virtual void setSceneValue(size_t aChannelIndex, double aValue);

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

    /// factory method to create the correct subclass type of DsScene
    /// @param aSceneNo the scene number to create a scene object for.
    /// @note setDefaultSceneValues() must be called to set default scene values
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);

  };


  class ColorLightBehaviour : public LightBehaviour
  {
    typedef LightBehaviour inherited;

  public:

    /// @name internal volatile state
    /// @{
    ColorLightMode colorMode;
    bool derivedValuesComplete;
    /// @}


    /// @name channels
    /// @{
    ChannelBehaviourPtr hue;
    ChannelBehaviourPtr saturation;
    ChannelBehaviourPtr ct;
    ChannelBehaviourPtr cieX;
    ChannelBehaviourPtr cieY;
    /// @}


    ColorLightBehaviour(Device &aDevice);

    /// @name color services for implementing color lights
    /// @{

    /// derives the color mode from channel values that need to be applied to hardware
    /// @return true if mode could be found
    bool deriveColorMode();

    /// derives the values for the not-current color representations' channels
    /// by converting between representations
    void deriveMissingColorChannels();

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

  typedef boost::intrusive_ptr<ColorLightBehaviour> ColorLightBehaviourPtr;



  class RGBColorLightBehaviour : public ColorLightBehaviour
  {
    typedef ColorLightBehaviour inherited;

  public:

    /// @name settings (color calibration)
    /// @{
    Matrix3x3 calibration; ///< calibration matrix: [[Xr,Xg,Xb],[Yr,Yg,Yb],[Zr,Zg,Zb]]
    /// @}

    RGBColorLightBehaviour(Device &aDevice);

    /// @name color services for implementing color lights
    /// @{

    /// get RGB colors for applying to lamp
    void getRGB(double &aRed, double &aGreen, double &aBlue, double aMax);

    /// get RGB colors for applying to lamp
    void setRGB(double aRed, double aGreen, double aBlue, double aMax);

    /// mark RGB values applied (flags channels applied depending on colormode)
    void appliedRGB();

    /// @}

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  protected:

    // property access implementation for descriptor/settings/states
    virtual int numSettingsProps();
    virtual const PropertyDescriptorPtr getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    // combined field access for all types of properties
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);

  };

  typedef boost::intrusive_ptr<RGBColorLightBehaviour> RGBColorLightBehaviourPtr;

} // namespace p44

#endif /* defined(__vdcd__colorlightbehaviour__) */
