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

using namespace p44;



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
    case channeltype_hue: return colorMode==ColorLightModeHueSaturation ? XOrHueOrCt : 0;
    case channeltype_saturation: return colorMode==ColorLightModeHueSaturation ? YOrSat : 0;
    case channeltype_colortemp: return colorMode==ColorLightModeCt ? XOrHueOrCt : 0;
    case channeltype_cie_x: return colorMode==ColorLightModeXY ? XOrHueOrCt : 0;
    case channeltype_cie_y: return colorMode==ColorLightModeXY ? YOrSat : 0;
    default: return inherited::sceneValue(aChannelIndex);
  }
  return 0;
}


void ColorLightScene::setSceneValue(size_t aChannelIndex, double aValue)
{
  ChannelBehaviourPtr cb = getDevice().getChannelByIndex(aChannelIndex);
  switch (cb->getChannelType()) {
    case channeltype_hue: XOrHueOrCt = aValue; colorMode=ColorLightModeHueSaturation; break;
    case channeltype_saturation: YOrSat = aValue; colorMode=ColorLightModeHueSaturation; break;
    case channeltype_colortemp: XOrHueOrCt = aValue; colorMode=ColorLightModeCt; break;
    case channeltype_cie_x: XOrHueOrCt = aValue; colorMode=ColorLightModeXY; break;
    case channeltype_cie_y: YOrSat = aValue; colorMode=ColorLightModeXY; break;
    default: inherited::setSceneValue(aChannelIndex, aValue);
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
void ColorLightScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  colorMode = (ColorLightMode)aRow->get<int>(aIndex++);
  XOrHueOrCt = aRow->get<double>(aIndex++);
  YOrSat = aRow->get<double>(aIndex++);
}


/// bind values to passed statement
void ColorLightScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
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
  // TODO: implement according to dS Specs for color lights
  // %%% for now, just set to light bulb white color temperature
  colorMode = ColorLightModeCt;
  XOrHueOrCt = 270; // 270mired = 2700K = warm white
  YOrSat = 0;
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
  inherited(aDevice)
{
  // primary channel of a color light is always a dimmer controlling the brightness
  setHardwareOutputConfig(outputFunction_dimmer, usage_undefined, true, -1);
  // Create and add auxiliary channels to the device for Hue, Saturation, Color Temperature and CIE x,y
  // - hue
  hue = ChannelBehaviourPtr(new ChannelBehaviour(*this));
  hue->setChannelIdentification(channeltype_hue, "hue");
  addChannel(hue);
  // - saturation
  saturation = ChannelBehaviourPtr(new ChannelBehaviour(*this));
  saturation->setChannelIdentification(channeltype_saturation, "saturation");
  addChannel(saturation);
  // - color temperature
  ct = ChannelBehaviourPtr(new ChannelBehaviour(*this));
  ct->setChannelIdentification(channeltype_colortemp, "color temperature");
  addChannel(ct);
  // - CIE x and y
  cieX = ChannelBehaviourPtr(new ChannelBehaviour(*this));
  cieX->setChannelIdentification(channeltype_cie_x, "CIE X");
  addChannel(cieX);
  cieY = ChannelBehaviourPtr(new ChannelBehaviour(*this));
  cieY->setChannelIdentification(channeltype_cie_y, "CIE Y");
  addChannel(cieY);
}





#pragma mark - behaviour interaction with digitalSTROM system


#define AUTO_OFF_FADE_TIME (60*Second)

void ColorLightBehaviour::recallScene(LightScenePtr aLightScene)
{
  ColorLightScenePtr colorLightScene = boost::dynamic_pointer_cast<ColorLightScene>(aLightScene);
  if (colorLightScene) {
    // prepare next color values in device
//    HueDevice *devP = dynamic_cast<HueDevice *>(&device);
//    if (devP) {
//      devP->pendingColorScene = colorLightScene;
//      outputUpdatePending = true; // we need an output update, even if main output value (brightness) has not changed in new scene
//    }
  }
  // let base class update logical brightness, which will in turn update the primary output, which will then
  // catch the colors from pendingColorScene
  inherited::recallScene(aLightScene);
}


void ColorLightBehaviour::performSceneActions(DsScenePtr aScene)
{
  // for now, just let base class check these
  inherited::performSceneActions(aScene);
}



// capture scene
void ColorLightBehaviour::captureScene(DsScenePtr aScene, DoneCB aDoneCB)
{
  // we can only handle light scenes
  ColorLightScenePtr colorLightScene = boost::dynamic_pointer_cast<ColorLightScene>(aScene);
  if (colorLightScene) {
    // make sure logical brightness is updated from output
    updateLogicalBrightnessFromOutput();
//    // just capture the output value
//    if (lightScene->sceneBrightness != getLogicalBrightness()) {
//      lightScene->sceneBrightness = getLogicalBrightness();
//      lightScene->markDirty();
//    }
  }
  inherited::captureScene(aScene, aDoneCB);
}


#pragma mark - description/shortDesc


string ColorLightBehaviour::shortDesc()
{
  return string("ColorLight");
}


string ColorLightBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  // TODO: add color specific info here
  s.append(inherited::description());
  return s;
}







