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

#include "colorlightbehaviour.hpp"

#include "colorutils.hpp"

using namespace p44;


#pragma mark - ColorChannel

double ColorChannel::getChannelValueCalculated()
{
  // check with behaviour first
  ColorLightBehaviour *cl = dynamic_cast<ColorLightBehaviour *>(&output);
  if (cl) {
    if (cl->colorMode!=colorMode()) {
      // asking for a color channel that is not native -> have it calculated
      cl->deriveMissingColorChannels();
    }
  }
  // now return it
  return getChannelValue();
}


#pragma mark - ColorLightScene


ColorLightScene::ColorLightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inherited(aSceneDeviceSettings, aSceneNo)
{
}


#pragma mark - color scene values/channels


double ColorLightScene::sceneValue(size_t aChannelIndex)
{
  ChannelBehaviourPtr cb = getDevice().getChannelByIndex(aChannelIndex);
  switch (cb->getChannelType()) {
    case channeltype_hue: return colorMode==colorLightModeHueSaturation ? XOrHueOrCt : 0;
    case channeltype_saturation: return colorMode==colorLightModeHueSaturation ? YOrSat : 0;
    case channeltype_colortemp: return colorMode==colorLightModeCt ? XOrHueOrCt : 0;
    case channeltype_cie_x: return colorMode==colorLightModeXY ? XOrHueOrCt : 0;
    case channeltype_cie_y: return colorMode==colorLightModeXY ? YOrSat : 0;
    default: return inherited::sceneValue(aChannelIndex);
  }
  return 0;
}


void ColorLightScene::setSceneValue(size_t aChannelIndex, double aValue)
{
  ChannelBehaviourPtr cb = getDevice().getChannelByIndex(aChannelIndex);
  switch (cb->getChannelType()) {
    case channeltype_hue: XOrHueOrCt = aValue; colorMode = colorLightModeHueSaturation; break;
    case channeltype_saturation: YOrSat = aValue; colorMode = colorLightModeHueSaturation; break;
    case channeltype_colortemp: XOrHueOrCt = aValue; colorMode = colorLightModeCt; break;
    case channeltype_cie_x: XOrHueOrCt = aValue; colorMode = colorLightModeXY; break;
    case channeltype_cie_y: YOrSat = aValue; colorMode = colorLightModeXY; break;
    default: inherited::setSceneValue(aChannelIndex, aValue); break;
  }
}


#pragma mark - Color Light Scene persistence

const char *ColorLightScene::tableName()
{
  return "ColorLightScenes";
}

// data field definitions

static const size_t numColorSceneFields = 3;

size_t ColorLightScene::numFieldDefs()
{
  return inherited::numFieldDefs()+numColorSceneFields;
}


const FieldDefinition *ColorLightScene::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numColorSceneFields] = {
    { "colorMode", SQLITE_INTEGER },
    { "XOrHueOrCt", SQLITE_FLOAT },
    { "YOrSat", SQLITE_FLOAT }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numColorSceneFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void ColorLightScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the fields
  colorMode = (ColorLightMode)aRow->get<int>(aIndex++);
  XOrHueOrCt = aRow->get<double>(aIndex++);
  YOrSat = aRow->get<double>(aIndex++);
}


/// bind values to passed statement
void ColorLightScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, (int)colorMode);
  aStatement.bind(aIndex++, XOrHueOrCt);
  aStatement.bind(aIndex++, YOrSat);
}



#pragma mark - default color scene

void ColorLightScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  // set the common light scene defaults
  inherited::setDefaultSceneValues(aSceneNo);
  // Add special color lamp behaviour
  switch (aSceneNo) {
    case ALARM1:
    case FIRE:
      // Alert - use cold white
      colorMode = colorLightModeCt;
      XOrHueOrCt = 153; // = 1E6/153 = 6535K = cold white
      YOrSat = 0;
      break;
    default:
      // default color is warm white
      colorMode = colorLightModeCt;
      XOrHueOrCt = 370; // = 1E6/370 = 2700k = warm white
      YOrSat = 0;
  }
}


#pragma mark - ColorLightDeviceSettings with default light scenes factory


ColorLightDeviceSettings::ColorLightDeviceSettings(Device &aDevice) :
  inherited(aDevice)
{
};


DsScenePtr ColorLightDeviceSettings::newDefaultScene(SceneNo aSceneNo)
{
  ColorLightScenePtr colorLightScene = ColorLightScenePtr(new ColorLightScene(*this, aSceneNo));
  colorLightScene->setDefaultSceneValues(aSceneNo);
  // return it
  return colorLightScene;
}



#pragma mark - ColorLightBehaviour


ColorLightBehaviour::ColorLightBehaviour(Device &aDevice) :
  inherited(aDevice),
  colorMode(colorLightModeNone),
  derivedValuesComplete(false)
{
  // primary channel of a color light is always a dimmer controlling the brightness
  setHardwareOutputConfig(outputFunction_colordimmer, usage_undefined, true, -1);
  // Create and add auxiliary channels to the device for Hue, Saturation, Color Temperature and CIE x,y
  // - hue
  hue = ChannelBehaviourPtr(new HueChannel(*this));
  addChannel(hue);
  // - saturation
  saturation = ChannelBehaviourPtr(new SaturationChannel(*this));
  addChannel(saturation);
  // - color temperature
  ct = ChannelBehaviourPtr(new ColorTempChannel(*this));
  addChannel(ct);
  // - CIE x and y
  cieX = ChannelBehaviourPtr(new CieXChannel(*this));
  addChannel(cieX);
  cieY = ChannelBehaviourPtr(new CieYChannel(*this));
  addChannel(cieY);
}


bool ColorLightBehaviour::hasModelFeature(DsModelFeatures aFeatureIndex)
{
  // now check for light behaviour level features
  switch (aFeatureIndex) {
    case modelFeature_outputchannels:
      // Assumption: all color light output devices need the multi-channel color lamp UI
      return true;
    default:
      // not available at this level, ask base class
      return inherited::hasModelFeature(aFeatureIndex);
  }
}



void ColorLightBehaviour::loadChannelsFromScene(DsScenePtr aScene)
{
  // load basic light scene info
  inherited::loadChannelsFromScene(aScene);
  // now load color specific scene information
  ColorLightScenePtr colorLightScene = boost::dynamic_pointer_cast<ColorLightScene>(aScene);
  if (colorLightScene) {
    MLMicroSeconds ttUp = transitionTimeFromSceneEffect(colorLightScene->effect, true);
    MLMicroSeconds ttDown = transitionTimeFromSceneEffect(colorLightScene->effect, false);
    // prepare next color values in channels
    colorMode = colorLightScene->colorMode;
    switch (colorMode) {
      case colorLightModeHueSaturation: {
        hue->setChannelValueIfNotDontCare(colorLightScene, colorLightScene->XOrHueOrCt, ttUp, ttDown, true);
        saturation->setChannelValueIfNotDontCare(colorLightScene, colorLightScene->YOrSat, ttUp, ttDown, true);
        break;
      }
      case colorLightModeXY: {
        cieX->setChannelValueIfNotDontCare(colorLightScene, colorLightScene->XOrHueOrCt, ttUp, ttDown, true);
        cieY->setChannelValueIfNotDontCare(colorLightScene, colorLightScene->YOrSat, ttUp, ttDown, true);
        break;
      }
      case colorLightModeCt: {
        ct->setChannelValueIfNotDontCare(colorLightScene, colorLightScene->XOrHueOrCt, ttUp, ttDown, true);
        break;
      }
      default:
        colorMode = colorLightModeNone;
    }
  }
  // need recalculation of values
  derivedValuesComplete = false;
}


void ColorLightBehaviour::saveChannelsToScene(DsScenePtr aScene)
{
  // save basic light scene info
  inherited::saveChannelsToScene(aScene);
  // now save color specific scene information
  ColorLightScenePtr colorLightScene = boost::dynamic_pointer_cast<ColorLightScene>(aScene);
  if (colorLightScene) {
    colorLightScene->colorMode = colorMode;
    // save the values and adjust don't cares according to color mode
    switch (colorMode) {
      case colorLightModeHueSaturation: {
        // don't care unused ones
        colorLightScene->setSceneValueFlags(cieX->getChannelIndex(), valueflags_dontCare, true);
        colorLightScene->setSceneValueFlags(cieY->getChannelIndex(), valueflags_dontCare, true);
        colorLightScene->setSceneValueFlags(ct->getChannelIndex(), valueflags_dontCare, true);
        // assign the used values
        colorLightScene->setRepVar(colorLightScene->XOrHueOrCt, hue->getChannelValue());
        colorLightScene->setSceneValueFlags(hue->getChannelIndex(), valueflags_dontCare, false);
        colorLightScene->setRepVar(colorLightScene->YOrSat, saturation->getChannelValue());
        colorLightScene->setSceneValueFlags(saturation->getChannelIndex(), valueflags_dontCare, false);
        break;
      }
      case colorLightModeXY: {
        // don't care unused ones
        colorLightScene->setSceneValueFlags(hue->getChannelIndex(), valueflags_dontCare, true);
        colorLightScene->setSceneValueFlags(saturation->getChannelIndex(), valueflags_dontCare, true);
        colorLightScene->setSceneValueFlags(ct->getChannelIndex(), valueflags_dontCare, true);
        // assign the used values
        colorLightScene->setRepVar(colorLightScene->XOrHueOrCt, cieX->getChannelValue());
        colorLightScene->setSceneValueFlags(cieX->getChannelIndex(), valueflags_dontCare, false);
        colorLightScene->setRepVar(colorLightScene->YOrSat, cieY->getChannelValue());
        colorLightScene->setSceneValueFlags(cieY->getChannelIndex(), valueflags_dontCare, false);
        break;
      }
      case colorLightModeCt: {
        // don't care unused ones
        colorLightScene->setSceneValueFlags(cieX->getChannelIndex(), valueflags_dontCare, true);
        colorLightScene->setSceneValueFlags(cieY->getChannelIndex(), valueflags_dontCare, true);
        colorLightScene->setSceneValueFlags(hue->getChannelIndex(), valueflags_dontCare, true);
        colorLightScene->setSceneValueFlags(saturation->getChannelIndex(), valueflags_dontCare, true);
        // assign the used values
        colorLightScene->setRepVar(colorLightScene->XOrHueOrCt, ct->getChannelValue());
        colorLightScene->setSceneValueFlags(ct->getChannelIndex(), valueflags_dontCare, false);
        break;
      }
      default: {
        // all color related information is dontCare
        colorLightScene->setSceneValueFlags(hue->getChannelIndex(), valueflags_dontCare, true);
        colorLightScene->setSceneValueFlags(saturation->getChannelIndex(), valueflags_dontCare, true);
        colorLightScene->setSceneValueFlags(cieX->getChannelIndex(), valueflags_dontCare, true);
        colorLightScene->setSceneValueFlags(cieY->getChannelIndex(), valueflags_dontCare, true);
        colorLightScene->setSceneValueFlags(ct->getChannelIndex(), valueflags_dontCare, true);
        break;
      }
    }
  }
}


#pragma mark - color services for implementing color lights


bool ColorLightBehaviour::deriveColorMode()
{
  // the need to derive the color modes only arises when
  // colors have changed, so this invalidates the derived channel values
  derivedValuesComplete = false;
  // check changed channels
  if (hue->needsApplying() || saturation->needsApplying()) {
    colorMode = colorLightModeHueSaturation;
    return true;
  }
  else if (cieX->needsApplying() || cieY->needsApplying()) {
    colorMode = colorLightModeXY;
    return true;
  }
  else if (ct->needsApplying()) {
    colorMode = colorLightModeCt;
    return true;
  }
  // could not determine new color mode (assuming old is still ok)
  return false;
}



void ColorLightBehaviour::deriveMissingColorChannels()
{
  if (!derivedValuesComplete) {
    Row3 HSV;
    Row3 xyV;
    double mired;
    switch (colorMode) {
      case colorLightModeHueSaturation:
        // missing CIE and ct
        HSV[0] = hue->getChannelValue(); // 0..360
        HSV[1] = saturation->getChannelValue()/100; // 0..1
        HSV[2] = brightness->getChannelValue()/100; // 0..1
        HSVtoxyV(HSV, xyV);
        cieX->syncChannelValue(xyV[0]);
        cieY->syncChannelValue(xyV[1]);
        xyVtoCT(xyV, mired);
        ct->syncChannelValue(mired);
        break;
      case colorLightModeXY:
        // missing HSV and ct
        xyV[0] = cieX->getChannelValue();
        xyV[1] = cieY->getChannelValue();
        xyV[2] = brightness->getChannelValue()/100; // 0..1
        xyVtoCT(xyV, mired);
        ct->syncChannelValue(mired);
      xyVtoHSV:
        xyVtoHSV(xyV, HSV);
        hue->syncChannelValue(HSV[0]);
        saturation->syncChannelValue(HSV[1]*100); // 0..100%
        break;
      case colorLightModeCt:
        // missing HSV and xy
        // - xy
        CTtoxyV(ct->getChannelValue(), xyV);
        cieX->syncChannelValue(xyV[0]);
        cieY->syncChannelValue(xyV[1]);
        // - also create HSV
        goto xyVtoHSV;
      default:
        break;
    }
    derivedValuesComplete = true;
    if (DBGLOGENABLED(LOG_DEBUG)) {
      // show all values, plus RGB
      DBGLOG(LOG_DEBUG, "Color mode = %s, actual and derived channel settings:\n", colorMode==colorLightModeHueSaturation ? "HSB" : (colorMode==colorLightModeXY ? "CIExy" : (colorMode==colorLightModeCt ? "CT" : "none")));
      DBGLOG(LOG_DEBUG, "- HSV : %6.1f, %6.1f, %6.1f [%%, %%, %%]\n", hue->getChannelValue(), saturation->getChannelValue(), brightness->getChannelValue());
      DBGLOG(LOG_DEBUG, "- xyV : %6.4f, %6.4f, %6.4f [0..1, 0..1, %%]\n", cieX->getChannelValue(), cieY->getChannelValue(), brightness->getChannelValue());
      DBGLOG(LOG_DEBUG, "- CT  : %6.0f, %6.0f [mired, K]\n", ct->getChannelValue(), 1E6/ct->getChannelValue());
    }
  }
}



#pragma mark - description/shortDesc


string ColorLightBehaviour::shortDesc()
{
  return string("ColorLight");
}


string ColorLightBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- color mode = %s", colorMode==colorLightModeHueSaturation ? "HSB" : (colorMode==colorLightModeXY ? "CIExy" : (colorMode==colorLightModeCt ? "CT" : "none")));
  // TODO: add color specific info here
  s.append(inherited::description());
  return s;
}



#pragma mark - RGBColorLightBehaviour


RGBColorLightBehaviour::RGBColorLightBehaviour(Device &aDevice) :
  inherited(aDevice)
{
  // default to sRGB with D65 white point
  matrix3x3_copy(sRGB_d65_calibration, calibration);
  // default white assumed to contribute equally to R,G,B with 50% each
  whiteRGB[0] = 0.5; whiteRGB[1] = 0.5; whiteRGB[2] = 0.5;
  // default amber assumed to be AMBER web color #FFBE00 = 100%, 75%, 0% contributing 50% intensity
  amberRGB[0] = 0.5; amberRGB[1] = 0.375; amberRGB[2] = 0;
}


static double colorCompScaled(double aColorComp, double aMax)
{
  if (aColorComp<0) aColorComp = 0; // limit to >=0
  aColorComp *= aMax;
  if (aColorComp>aMax) aColorComp = aMax;
  return aColorComp;
}

void RGBColorLightBehaviour::getRGB(double &aRed, double &aGreen, double &aBlue, double aMax)
{
  Row3 RGB;
  Row3 xyV;
  Row3 XYZ;
  Row3 HSV;
  double scale = 1;
  switch (colorMode) {
    case colorLightModeHueSaturation: {
      HSV[0] = hue->getChannelValue(); // 0..360
      HSV[1] = saturation->getChannelValue()/100; // 0..1
      HSV[2] = brightness->getChannelValue()/100; // 0..1
      HSVtoRGB(HSV, RGB);
      break;
    }
    case colorLightModeCt: {
      // Note: for some reason, passing brightness to V gives bad results,
      // so for now we always assume 1 and scale resulting RGB
      CTtoxyV(ct->getChannelValue(), xyV);
      xyVtoXYZ(xyV, XYZ);
      XYZtoRGB(calibration, XYZ, RGB);
      // for color temperature, color is more important than brightness, so scale down if a color component exceeds 1
      double m = 1;
      if (RGB[0]>m) m = RGB[0];
      if (RGB[1]>m) m = RGB[1];
      if (RGB[2]>m) m = RGB[2];
      // include actual brightness into scale calculation
      scale = brightness->getChannelValue()/100/m;
      break;
    }
    case colorLightModeXY: {
      // Note: for some reason, passing brightness to V gives bad results,
      // so for now we always assume 1 and scale resulting RGB
      xyV[0] = cieX->getChannelValue();
      xyV[1] = cieY->getChannelValue();
      xyVtoXYZ(xyV, XYZ);
      // convert using calibration for this lamp
      XYZtoRGB(calibration, XYZ, RGB);
      scale = brightness->getChannelValue()/100; // 0..1
      break;
    }
    default: {
      // no color, just set R=G=B=brightness
      RGB[0] = brightness->getChannelValue()/100;
      RGB[1] = RGB[0];
      RGB[2] = RGB[0];
      break;
    }
  }
  aRed = colorCompScaled(RGB[0], aMax*scale);
  aGreen = colorCompScaled(RGB[1], aMax*scale);
  aBlue = colorCompScaled(RGB[2], aMax*scale);
}


void RGBColorLightBehaviour::setRGB(double aRed, double aGreen, double aBlue, double aMax)
{
  Row3 RGB;
  RGB[0] = aRed/aMax;
  RGB[1] = aGreen/aMax;
  RGB[2] = aBlue/aMax;
  // always convert to HSV, as this can actually represent the values seen on the light
  Row3 HSV;
  RGBtoHSV(RGB, HSV);
  // set the channels
  hue->syncChannelValue(HSV[0]);
  saturation->syncChannelValue(HSV[1]*100);
  brightness->syncChannelValue(HSV[2]*100);
  // change the mode if needed
  if (colorMode!=colorLightModeHueSaturation) {
    colorMode = colorLightModeHueSaturation;
    // force recalculation of derived color value
    derivedValuesComplete = false;
  }
}


static double transferToColor(Row3 &aCol, double &aRed, double &aGreen, double &aBlue)
{
  bool hasRed = aCol[0]>0;
  bool hasGreen = aCol[1]>0;
  bool hasBlue = aCol[2]>0;
  double fr = hasRed ? aRed/aCol[0] : 0;
  double fg = hasGreen ? aGreen/aCol[1] : 0;
  double fb = hasBlue ? aBlue/aCol[2] : 0;
  // - find non-zero fraction to use of external color
  double f = fg>fb && hasBlue ? fb : fg;
  f = fr>f && (hasBlue || hasGreen) ? f : fr;
  if (f>1) f=1; // limit to 1
  // - now subtract from RGB values what we've transferred to separate color
  if (hasRed) aRed = aRed - f*aCol[0];
  if (hasGreen) aGreen = aGreen - f*aCol[1];
  if (hasBlue) aBlue = aBlue - f*aCol[2];
  // - find fraction RGB HAS to contribute without loosing color information
  double u = 1-aCol[0]; // how much of red RGB needs to contribute
  if (1-aCol[1]>u) u = 1-aCol[1]; // how much of green
  if (1-aCol[2]>u) u = 1-aCol[2]; // how much of blue
  //   now scale RGB up to minimal fraction it HAS to contribute
  if (u>0) {
    u = 1/u;
    aRed *= u;
    aBlue *= u;
    aGreen *= u;
  }
  return f;
}


static void transferFromColor(Row3 &aCol, double aAmount, double &aRed, double &aGreen, double &aBlue)
{
  // first scale RGB down to non-transferable amount
  if (aCol[0]<1) aRed *= 1-aCol[0];
  if (aCol[1]<1) aGreen *= 1-aCol[1];
  if (aCol[2]<1) aBlue *= 1-aCol[2];
  // then add amount from separate color
  aRed += aAmount*aCol[0];
  aGreen += aAmount*aCol[1];
  aBlue += aAmount*aCol[2];
}




void RGBColorLightBehaviour::getRGBW(double &aRed, double &aGreen, double &aBlue, double &aWhite, double aMax)
{
  // first get 0..1 RGB
  double r,g,b;
  getRGB(r, g, b, 1);
  // transfer as much as possible to the white channel
  double w = transferToColor(whiteRGB, r, g, b);
  // Finally scale as requested
  aWhite = colorCompScaled(w, aMax);
  aRed = colorCompScaled(r, aMax);
  aGreen = colorCompScaled(g, aMax);
  aBlue = colorCompScaled(b, aMax);
}


void RGBColorLightBehaviour::getRGBWA(double &aRed, double &aGreen, double &aBlue, double &aWhite, double &aAmber, double aMax)
{
  // first get RGBW
  double r,g,b;
  getRGB(r, g, b, 1);
  // transfer as much as possible to the white channel
  double w = transferToColor(whiteRGB, r, g, b);
  // then transfer as much as possible to the amber channel
  double a = transferToColor(amberRGB, r, g, b);
  // Finally scale as requested
  aAmber = colorCompScaled(a, aMax);
  aWhite = colorCompScaled(w, aMax);
  aRed = colorCompScaled(r, aMax);
  aGreen = colorCompScaled(g, aMax);
  aBlue = colorCompScaled(b, aMax);
}


void RGBColorLightBehaviour::setRGBW(double aRed, double aGreen, double aBlue, double aWhite, double aMax)
{
  Row3 RGB;
  RGB[0] = aRed/aMax;
  RGB[1] = aGreen/aMax;
  RGB[2] = aBlue/aMax;
  transferFromColor(whiteRGB, aWhite/aMax, RGB[0], RGB[1], RGB[2]);
  // always convert to HSV, as this can actually represent the values seen on the light
  Row3 HSV;
  RGBtoHSV(RGB, HSV);
  // set the channels
  hue->syncChannelValue(HSV[0]);
  saturation->syncChannelValue(HSV[1]*100);
  brightness->syncChannelValue(HSV[2]*100);
  // change the mode if needed
  if (colorMode!=colorLightModeHueSaturation) {
    colorMode = colorLightModeHueSaturation;
    // force recalculation of derived color value
    derivedValuesComplete = false;
  }
}


void RGBColorLightBehaviour::appliedRGB()
{
  brightness->channelValueApplied(true);
  switch (colorMode) {
    case colorLightModeHueSaturation:
      hue->channelValueApplied(true);
      saturation->channelValueApplied(true);
      break;
    case colorLightModeCt:
      ct->channelValueApplied(true);
      break;
    case colorLightModeXY:
      cieX->channelValueApplied(true);
      cieY->channelValueApplied(true);
      break;
    default:
      // no color
      break;
  }
}



#pragma mark - persistence implementation


const char *RGBColorLightBehaviour::tableName()
{
  return "RGBLightSettings";
}


// data field definitions

static const size_t numFields = 9;

size_t RGBColorLightBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *RGBColorLightBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "Xr", SQLITE_FLOAT },
    { "Yr", SQLITE_FLOAT },
    { "Zr", SQLITE_FLOAT },
    { "Xg", SQLITE_FLOAT },
    { "Yg", SQLITE_FLOAT },
    { "Zg", SQLITE_FLOAT },
    { "Xb", SQLITE_FLOAT },
    { "Yb", SQLITE_FLOAT },
    { "Zb", SQLITE_FLOAT },
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void RGBColorLightBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inherited::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the fields
  //  [[Xr,Xg,Xb],[Yr,Yg,Yb],[Zr,Zg,Zb]]
  for (int i=0; i<3; i++) {
    for (int j=0; j<3; j++) {
      calibration[j][i] = aRow->get<double>(aIndex++);
    }
  }
}


// bind values to passed statement
void RGBColorLightBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  //  [[Xr,Xg,Xb],[Yr,Yg,Yb],[Zr,Zg,Zb]]
  for (int i=0; i<3; i++) {
    for (int j=0; j<3; j++) {
      aStatement.bind(aIndex++, calibration[j][i]);
    }
  }
}



#pragma mark - property access


static char rgblight_key;

// settings properties

enum {
  Xr_key,
  Yr_key,
  Zr_key,
  Xg_key,
  Yg_key,
  Zg_key,
  Xb_key,
  Yb_key,
  Zb_key,
  numSettingsProperties
};


int RGBColorLightBehaviour::numSettingsProps() { return inherited::numSettingsProps()+numSettingsProperties; }
const PropertyDescriptorPtr RGBColorLightBehaviour::getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numSettingsProperties] = {
    { "Xr", apivalue_double, Xr_key+settings_key_offset, OKEY(rgblight_key) },
    { "Yr", apivalue_double, Yr_key+settings_key_offset, OKEY(rgblight_key) },
    { "Zr", apivalue_double, Zr_key+settings_key_offset, OKEY(rgblight_key) },
    { "Xg", apivalue_double, Xg_key+settings_key_offset, OKEY(rgblight_key) },
    { "Yg", apivalue_double, Yg_key+settings_key_offset, OKEY(rgblight_key) },
    { "Zg", apivalue_double, Zg_key+settings_key_offset, OKEY(rgblight_key) },
    { "Xb", apivalue_double, Xb_key+settings_key_offset, OKEY(rgblight_key) },
    { "Yb", apivalue_double, Yb_key+settings_key_offset, OKEY(rgblight_key) },
    { "Zb", apivalue_double, Zb_key+settings_key_offset, OKEY(rgblight_key) },
  };
  int n = inherited::numSettingsProps();
  if (aPropIndex<n)
    return inherited::getSettingsDescriptorByIndex(aPropIndex, aParentDescriptor);
  aPropIndex -= n;
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


// access to all fields

bool RGBColorLightBehaviour::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(rgblight_key)) {
    int ix = (int)aPropertyDescriptor->fieldKey()-settings_key_offset;
    if (ix>=Xr_key && ix<=Zb_key) {
      if (aMode==access_read) {
        // read properties
        aPropValue->setDoubleValue(calibration[ix/3][ix%3]);
      }
      else {
        // write properties
        calibration[ix/3][ix%3] = aPropValue->doubleValue();
      }
      return true;
    }
    else {
      // check other props
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


#pragma mark - description/shortDesc


string RGBColorLightBehaviour::shortDesc()
{
  return string("RGBLight");
}






