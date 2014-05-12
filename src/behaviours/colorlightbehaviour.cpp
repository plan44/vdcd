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


double ColorLightScene::sceneValue(size_t aOutputIndex)
{
  return sceneBrightness;
}


void ColorLightScene::setSceneValue(size_t aOutputIndex, double aValue)
{
  if (aOutputIndex==0) {
    sceneBrightness = aValue;
  }
}


#pragma mark - Color Light Scene persistence

const char *ColorLightScene::tableName()
{
  return "ColorLightScenes";
}

// data field definitions

static const size_t numSceneFields = 3;

size_t ColorLightScene::numFieldDefs()
{
  return inherited::numFieldDefs()+numSceneFields;
}


const FieldDefinition *ColorLightScene::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numSceneFields] = {
    { "brightness", SQLITE_INTEGER },
    { "lightFlags", SQLITE_INTEGER },
    { "dimTimeSelector", SQLITE_INTEGER }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numSceneFields)
    return &dataDefs[aIndex];
  return NULL;
}


enum {
  lightflag_specialBehaviour = 0x0001,
  lightflag_flashing = 0x0002
};


/// load values from passed row
void ColorLightScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  sceneBrightness = aRow->get<int>(aIndex++);
  int lightflags = aRow->get<int>(aIndex++);
  dimTimeSelector = aRow->get<int>(aIndex++);
  // decode the flags
  specialBehaviour = lightflags & lightflag_specialBehaviour;
  flashing = lightflags & lightflag_flashing;
}


/// bind values to passed statement
void ColorLightScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // encode the flags
  int lightflags = 0;
  if (specialBehaviour) lightflags |= lightflag_specialBehaviour;
  if (flashing) lightflags |= lightflag_flashing;
  // bind the fields
  aStatement.bind(aIndex++, sceneBrightness);
  aStatement.bind(aIndex++, lightflags);
  aStatement.bind(aIndex++, dimTimeSelector);
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
}



void ColorLightBehaviour::createAuxChannels()
{
  // TODO: create and add auxiliary channels to the device for Hue, Saturation, Color Temperature and CIE x,y
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
//      devP->pendingColorScene = hueScene;
//      outputUpdatePending = true; // we need an output update, even if main output value (brightness) has not changed in new scene
//    }
  }
  // let base class update logical brightness, which will in turn update the primary output, which will then
  // catch the colors from pendingColorScene
  inherited::recallScene(aLightScene);
}


void ColorLightBehaviour::performSceneActions(DsScenePtr aScene)
{
  // we can only handle light scenes
  ColorLightScenePtr colorLightScene = boost::dynamic_pointer_cast<ColorLightScene>(aScene);
  if (colorLightScene) {
    // flash
    // TODO: set the color from the scene, and if dontCare is set, revert it back to previous color afterwards
    blink(2*Second, 400*MilliSecond, 80);
  }
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







