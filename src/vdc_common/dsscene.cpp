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

#include "dsscene.hpp"

#include "device.hpp"

using namespace p44;


#pragma mark - scene base class


DsScene::DsScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inheritedParams(aSceneDeviceSettings.paramStore),
  sceneDeviceSettings(aSceneDeviceSettings),
  sceneNo(aSceneNo)
{
}

#pragma mark - scene persistence


// primary key field definitions

static const size_t numKeys = 1;

size_t DsScene::numKeyDefs()
{
  return inheritedParams::numKeyDefs()+numKeys;
}

const FieldDefinition *DsScene::getKeyDef(size_t aIndex)
{
  static const FieldDefinition keyDefs[numKeys] = {
    { "sceneNo", SQLITE_INTEGER }, // parent's key plus this one identifies the scene among all saved scenes of all devices
  };
  if (aIndex<inheritedParams::numKeyDefs())
    return inheritedParams::getKeyDef(aIndex);
  aIndex -= inheritedParams::numKeyDefs();
  if (aIndex<numKeys)
    return &keyDefs[aIndex];
  return NULL;
}


// data field definitions

static const size_t numFields = 1;

size_t DsScene::numFieldDefs()
{
  return inheritedParams::numFieldDefs()+numFields;
}


const FieldDefinition *DsScene::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "commonFlags", SQLITE_INTEGER }
  };
  if (aIndex<inheritedParams::numFieldDefs())
    return inheritedParams::getFieldDef(aIndex);
  aIndex -= inheritedParams::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


enum {
  commonflags_dontCare = 0x0001,
  commonflags_ignoreLocalPriority = 0x0002,
  commonflags_mask = 0x0003
};


/// load values from passed row
void DsScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inheritedParams::loadFromRow(aRow, aIndex);
  // get the fields
  sceneNo = aRow->get<int>(aIndex++);
  sceneFlags = aRow->get<int>(aIndex++);
  // decode the flags
  dontCare = sceneFlags & commonflags_dontCare;
  ignoreLocalPriority = sceneFlags & commonflags_ignoreLocalPriority;
}


// bind values to passed statement
void DsScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inheritedParams::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // encode the flags
  sceneFlags &= ~commonflags_mask; // clear my flags
  if (dontCare) sceneFlags |= commonflags_dontCare;
  if (ignoreLocalPriority) sceneFlags |= commonflags_ignoreLocalPriority;
  // bind the fields
  aStatement.bind(aIndex++, (int)sceneNo);
  aStatement.bind(aIndex++, sceneFlags);
}


#pragma mark - scene property access

static char dsscene_key;

enum {
  dontCare_key,
  ignoreLocalPriority_key,
  numSceneProperties
};




int DsScene::numProps(int aDomain)
{
  return inheritedProps::numProps(aDomain)+numSceneProperties;
}


const PropertyDescriptor *DsScene::getPropertyDescriptor(int aPropIndex, int aDomain)
{
  static const PropertyDescriptor properties[numSceneProperties] = {
    #warning "move value# here as well, restructure for MOC"
    { "dontCare", apivalue_bool, true, dontCare_key, &dsscene_key },
    { "ignoreLocalPriority", apivalue_bool, false, ignoreLocalPriority_key, &dsscene_key },
  };
  int n = inheritedProps::numProps(aDomain);
  if (aPropIndex<n)
    return inheritedProps::getPropertyDescriptor(aPropIndex, aDomain); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return &properties[aPropIndex];
}


bool DsScene::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&dsscene_key) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        case dontCare_key:
          // TODO: implement MOC
          if (aIndex==PROP_ARRAY_SIZE)
            aPropValue->setInt32Value(1); // %%% single element for now
          else
            aPropValue->setBoolValue(dontCare);
          return true;
        case ignoreLocalPriority_key:
          aPropValue->setBoolValue(ignoreLocalPriority);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor.accessKey) {
        case dontCare_key:
          dontCare = aPropValue->boolValue();
          markDirty();
          return true;
        case ignoreLocalPriority_key:
          ignoreLocalPriority = aPropValue->boolValue();
          markDirty();
          return true;
      }
    }
  }
  return inheritedProps::accessField(aMode, aPropValue, aPropertyDescriptor, aIndex);
}




#pragma mark - scene device settings base class


SceneDeviceSettings::SceneDeviceSettings(Device &aDevice) :
  inherited(aDevice)
{
}



DsScenePtr SceneDeviceSettings::getScene(SceneNo aSceneNo)
{
  // see if we have a stored version different from the default
  DsSceneMap::iterator pos = scenes.find(aSceneNo);
  if (pos!=scenes.end()) {
    // found scene params in map
    return pos->second;
  }
  else {
    // just return default values for this scene
    return newDefaultScene(aSceneNo);
  }
}



void SceneDeviceSettings::updateScene(DsScenePtr aScene)
{
  if (aScene->rowid==0) {
    // unstored so far, add to map of non-default scenes
    scenes[aScene->sceneNo] = aScene;
  }
  // anyway, mark scene dirty
  aScene->markDirty();
  // as we need the ROWID of the lightsettings as parentID, make sure we get saved if we don't have one
  if (rowid==0) markDirty();
}


#pragma mark - scene table persistence


// load child parameters (scenes)
ErrorPtr SceneDeviceSettings::loadChildren()
{
  ErrorPtr err;
  // my own ROWID is the parent key for the children
  string parentID = string_format("%d",rowid);
  // create a template
  DsScenePtr scene = newDefaultScene(0);
  // get the query
  sqlite3pp::query *queryP = scene->newLoadAllQuery(parentID.c_str());
  if (queryP==NULL) {
    // real error preparing query
    err = paramStore.error();
  }
  else {
    for (sqlite3pp::query::iterator row = queryP->begin(); row!=queryP->end(); ++row) {
      // got record
      // - load record fields into scene object
      int index = 0;
      scene->loadFromRow(row, index);
      // - put scene into map of non-default scenes
      scenes[scene->sceneNo] = scene;
      // - fresh object for next row
      scene = newDefaultScene(0);
    }
    delete queryP; queryP = NULL;
  }
  return err;
}

// save child parameters (scenes)
ErrorPtr SceneDeviceSettings::saveChildren()
{
  ErrorPtr err;
  // Cannot save children before I have my own rowID
  if (rowid!=0) {
    // my own ROWID is the parent key for the children
    string parentID = string_format("%d",rowid);
    // save all elements of the map (only dirty ones will be actually stored to DB
    for (DsSceneMap::iterator pos = scenes.begin(); pos!=scenes.end(); ++pos) {
      err = pos->second->saveToStore(parentID.c_str());
    }
  }
  return err;
}


// save child parameters (scenes)
ErrorPtr SceneDeviceSettings::deleteChildren()
{
  ErrorPtr err;
  for (DsSceneMap::iterator pos = scenes.begin(); pos!=scenes.end(); ++pos) {
    err = pos->second->deleteFromStore();
  }
  return err;
}


