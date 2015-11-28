//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
#include "outputbehaviour.hpp"
#include "simplescene.hpp"
#include "jsonvdcapi.hpp"

using namespace p44;

static char dsscene_key;

#pragma mark - private scene channel access class

static char dsscene_channels_key;
static char scenevalue_key;

// local property container for channels/outputs
class SceneChannels : public PropertyContainer
{
  typedef PropertyContainer inherited;

  enum {
    value_key,
    dontCare_key,
    numValueProperties
  };

  DsScene &scene;

public:
  SceneChannels(DsScene &aScene) : scene(aScene) {};

protected:

  int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
  {
    if (!aParentDescriptor->hasObjectKey(scenevalue_key)) {
      // channels/outputs container
      return scene.numSceneValues();
    }
    // actual fields of channel/output
    // Note: SceneChannels is private an can't be derived, so no subclass adding properties must be considered
    return numValueProperties;
  }


  PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
  {
    // scene value level properties
    static const PropertyDescription valueproperties[numValueProperties] = {
      { "value", apivalue_double, value_key, OKEY(scenevalue_key) },
      { "dontCare", apivalue_bool, dontCare_key, OKEY(scenevalue_key) },
    };
    // Note: SceneChannels is private an can't be derived, so no subclass adding properties must be considered
    return PropertyDescriptorPtr(new StaticPropertyDescriptor(&valueproperties[aPropIndex], aParentDescriptor));
  }


  PropertyDescriptorPtr getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
  {
    if (aParentDescriptor->hasObjectKey(dsscene_channels_key)) {
      // array-like container of channels
      PropertyDescriptorPtr propDesc;
      bool numericName = getNextPropIndex(aPropMatch, aStartIndex);
      if (numericName) {
        // specific channel addressed by ID, look up index for it
        DsChannelType ct = (DsChannelType)aStartIndex;
        aStartIndex = PROPINDEX_NONE; // default to not found
        ChannelBehaviourPtr cb = scene.getDevice().getChannelByType(ct);
        if (cb) {
          aStartIndex = (int)cb->getChannelIndex(); // found, return index
        }
      }
      int n = numProps(aDomain, aParentDescriptor);
      if (aStartIndex!=PROPINDEX_NONE && aStartIndex<n) {
        // within range, create descriptor
        DynamicPropertyDescriptor *descP = new DynamicPropertyDescriptor(aParentDescriptor);
        if (numericName) {
          // query specified a channel number -> return same number in result (to return "0" when default channel "0" was explicitly queried)
          descP->propertyName = aPropMatch; // query = name of object
        }
        else {
          // wildcard, name by channel type
          descP->propertyName = string_format("%d", scene.getDevice().getChannelByIndex(aStartIndex)->getChannelType());
        }
        descP->propertyType = aParentDescriptor->type();
        descP->propertyFieldKey = aStartIndex;
        descP->propertyObjectKey = OKEY(scenevalue_key);
        propDesc = PropertyDescriptorPtr(descP);
        // advance index
        aStartIndex++;
      }
      if (aStartIndex>=n || numericName) {
        // no more descriptors OR specific descriptor accessed -> no "next" descriptor
        aStartIndex = PROPINDEX_NONE;
      }
      return propDesc;
    }
    // actual fields of a single channel
    return inherited::getDescriptorByName(aPropMatch, aStartIndex, aDomain, aParentDescriptor);
  }


  PropertyContainerPtr getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain)
  {
    // the only subcontainer are the fields, handled by myself
    return PropertyContainerPtr(this);
  }



  bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
  {
    if (aPropertyDescriptor->hasObjectKey(scenevalue_key)) {
      // Scene value level
      // - get the output index
      size_t outputIndex = aPropertyDescriptor->parentDescriptor->fieldKey();
      if (aMode==access_read) {
        // read properties
        switch (aPropertyDescriptor->fieldKey()) {
          case value_key:
            aPropValue->setDoubleValue(scene.sceneValue(outputIndex));
            return true;
          case dontCare_key:
            aPropValue->setBoolValue(scene.isSceneValueFlagSet(outputIndex, valueflags_dontCare));
            return true;
        }
      }
      else {
        // write properties
        switch (aPropertyDescriptor->fieldKey()) {
          case value_key:
            scene.setSceneValue(outputIndex, aPropValue->doubleValue());
            return true;
          case dontCare_key:
            scene.setSceneValueFlags(outputIndex, valueflags_dontCare, aPropValue->boolValue());
            return true;
        }
      }
    }
    return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
  }
};
typedef boost::intrusive_ptr<SceneChannels> SceneChannelsPtr;



#pragma mark - scene base class


DsScene::DsScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inheritedParams(aSceneDeviceSettings.paramStore),
  sceneDeviceSettings(aSceneDeviceSettings),
  sceneNo(aSceneNo),
  sceneArea(0), // not area scene by default
  sceneCmd(scene_cmd_invoke), // simple invoke command by default
  globalSceneFlags(0)
{
  sceneChannels = SceneChannelsPtr(new SceneChannels(*this));
}


Device &DsScene::getDevice()
{
  return sceneDeviceSettings.device;
}


OutputBehaviourPtr DsScene::getOutputBehaviour()
{
  return sceneDeviceSettings.device.output;
}


void DsScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  sceneNo = aSceneNo; // usually already set, but still make sure
  sceneCmd = scene_cmd_invoke; // assume invoke type
  sceneArea = 0; // no area scene by default
  markClean(); // default values are always clean
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


// flags in globalSceneFlags
enum {
  // scene global
  globalflags_sceneDontCare = 0x0001, ///< scene level dontcare
  globalflags_ignoreLocalPriority = 0x0002,
  globalflags_sceneLevelMask = 0x0003,

  // per value dontCare flags, 16 channels max
  globalflags_valueDontCare0 = 0x100,
  globalflags_valueDontCareMask = 0xFFFF00,
};


void DsScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inheritedParams::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the scene number
  sceneNo = aRow->get<int>(aIndex++);
  // as the scene is loaded into a object which did not yet have the correct scene number
  // default values must be set again now that the sceneNo is known
  // Note: this is important to make sure those field which are not stored have the correct scene related value (sceneCmd, sceneArea)
  setDefaultSceneValues(sceneNo);
  // then proceed with loading other fields
  globalSceneFlags = aRow->get<int>(aIndex++);
}


void DsScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inheritedParams::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, (int)sceneNo);
  aStatement.bind(aIndex++, (int)globalSceneFlags);
}


#pragma mark - scene flags


void DsScene::setGlobalSceneFlag(uint32_t aMask, bool aNewValue)
{
  uint32_t newFlags = (globalSceneFlags & ~aMask) | (aNewValue ? aMask : 0);
  setPVar(globalSceneFlags, newFlags);
}



bool DsScene::isDontCare()
{
  return globalSceneFlags & globalflags_sceneDontCare;
}

void DsScene::setDontCare(bool aDontCare)
{
  setGlobalSceneFlag(globalflags_sceneDontCare, aDontCare);
}



bool DsScene::ignoresLocalPriority()
{
  return globalSceneFlags & globalflags_ignoreLocalPriority;
}

void DsScene::setIgnoreLocalPriority(bool aIgnoreLocalPriority)
{
  uint32_t newFlags = (globalSceneFlags & ~globalflags_ignoreLocalPriority) | (aIgnoreLocalPriority ? globalflags_ignoreLocalPriority : 0);
  setPVar(globalSceneFlags, newFlags);
}


#pragma mark - scene values/channels


int DsScene::numSceneValues()
{
  return getDevice().numChannels();
}


uint32_t DsScene::sceneValueFlags(size_t aOutputIndex)
{
  uint32_t flags = 0;
  // up to 16 channel's dontCare flags are mapped into globalSceneFlags
  if (aOutputIndex<numSceneValues()) {
    if (globalSceneFlags & (globalflags_valueDontCare0<<aOutputIndex)) {
      flags |= valueflags_dontCare; // this value's dontCare is set
    }
  }
  return flags;
}


void DsScene::setSceneValueFlags(size_t aOutputIndex, uint32_t aFlagMask, bool aSet)
{
  // up to 16 channel's dontCare flags are mapped into globalSceneFlags
  if (aOutputIndex<numSceneValues()) {
    uint32_t flagmask = globalflags_valueDontCare0<<aOutputIndex;
    uint32_t newFlags;
    if (aSet)
      newFlags = globalSceneFlags | ((aFlagMask & valueflags_dontCare) ? flagmask : 0);
    else
      newFlags = globalSceneFlags & ~((aFlagMask & valueflags_dontCare) ? flagmask : 0);
    if (newFlags!=globalSceneFlags) {
      // actually changed
      globalSceneFlags = newFlags;
      markDirty();
    }
  }
}


// utility function to check scene value flag
bool DsScene::isSceneValueFlagSet(size_t aOutputIndex, uint32_t aFlagMask)
{
  // only dontCare flag exists per value in base class
  return sceneValueFlags(aOutputIndex) & aFlagMask;
}



#pragma mark - scene property access


enum {
  channels_key,
  ignoreLocalPriority_key,
  dontCare_key,
  numSceneProperties
};




int DsScene::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  return inheritedProps::numProps(aDomain, aParentDescriptor)+numSceneProperties;
}



PropertyDescriptorPtr DsScene::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  // scene level properties
  static const PropertyDescription sceneproperties[numSceneProperties] = {
    { "channels", apivalue_object+propflag_container, channels_key, OKEY(dsscene_channels_key) },
    { "ignoreLocalPriority", apivalue_bool, ignoreLocalPriority_key, OKEY(dsscene_key) },
    { "dontCare", apivalue_bool, dontCare_key, OKEY(dsscene_key) },
  };
  int n = inheritedProps::numProps(aDomain, aParentDescriptor);
  if (aPropIndex<n)
    return inheritedProps::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&sceneproperties[aPropIndex], aParentDescriptor));
}


PropertyContainerPtr DsScene::getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain)
{
  // the only container is sceneChannels
  return sceneChannels;
}



bool DsScene::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(dsscene_key)) {
    // global scene level
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        case ignoreLocalPriority_key:
          aPropValue->setBoolValue(ignoresLocalPriority());
          return true;
        case dontCare_key:
          aPropValue->setBoolValue(isDontCare());
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        case ignoreLocalPriority_key:
          setIgnoreLocalPriority(aPropValue->boolValue());
          return true;
        case dontCare_key:
          setDontCare(aPropValue->boolValue());
          return true;
      }
    }
  }
  return inheritedProps::accessField(aMode, aPropValue, aPropertyDescriptor);
}



#pragma mark - scene device settings base class


SceneDeviceSettings::SceneDeviceSettings(Device &aDevice) :
  inherited(aDevice)
{
}


DsScenePtr SceneDeviceSettings::newDefaultScene(SceneNo aSceneNo)
{
  SimpleScenePtr simpleScene = SimpleScenePtr(new SimpleScene(*this, aSceneNo));
  simpleScene->setDefaultSceneValues(aSceneNo);
  // return it
  return simpleScene;
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
  // as we need the ROWID of the settings as parentID, make sure we get saved if we don't have one
  if (rowid==0) markDirty();
}





#pragma mark - scene table persistence


// load child parameters (scenes)
ErrorPtr SceneDeviceSettings::loadChildren()
{
  ErrorPtr err;
  // my own ROWID is the parent key for the children
  string parentID = string_format("%llu",rowid);
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
      uint64_t flags;
      scene->loadFromRow(row, index, &flags);
      // - put scene into map of non-default scenes
      scenes[scene->sceneNo] = scene;
      // - fresh object for next row
      scene = newDefaultScene(0);
    }
    delete queryP; queryP = NULL;
    // Now check for default settings from files
    loadScenesFromFiles();
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
    string parentID = string_format("%llu",rowid);
    // save all elements of the map (only dirty ones will be actually stored to DB
    for (DsSceneMap::iterator pos = scenes.begin(); pos!=scenes.end(); ++pos) {
      err = pos->second->saveToStore(parentID.c_str(), true); // multiple children of same parent allowed
      if (!Error::isOK(err)) LOG(LOG_ERR,"vdSD %s: Error saving scene %d: %s", device.shortDesc().c_str(), pos->second->sceneNo, err->description().c_str());
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



#pragma mark - additional scene defaults from files


void SceneDeviceSettings::loadScenesFromFiles()
{
  string dir = device.getDeviceContainer().getPersistentDataDir();
  const int numLevels = 4;
  string levelids[numLevels];
  // Level strategy: most specialized will be active, unless lower levels specify explicit override
  // - Baselines are hardcoded defaults plus settings (already) loaded from persistent store
  // - Level 0 are settings related to the device instance (dSUID)
  // - Level 1 are settings related to the device type (deviceTypeIdentifier())
  // - Level 2 are settings related to the behaviour (behaviourTypeIdentifier())
  // - Level 3 are settings related to the device class (deviceClassIdentifier())
  levelids[0] = "vdsd_" + device.getDsUid().getString();
  levelids[1] = string(device.deviceTypeIdentifier()) + "_device";
  levelids[2] = string(device.output->behaviourTypeIdentifier()) + "_behaviour";
  levelids[3] = device.classContainerP->deviceClassIdentifier();
  for(int i=0; i<numLevels; ++i) {
    // try to open config file
    string fn = dir+"scenes_"+levelids[i]+".csv";
    string line;
    int lineNo = 0;
    FILE *file = fopen(fn.c_str(), "r");
    if (!file) {
      int syserr = errno;
      if (syserr!=ENOENT) {
        // file not existing is ok, all other errors must be reported
        LOG(LOG_ERR, "failed opening file %s - %s", fn.c_str(), strerror(syserr));
      }
      // don't process, try next
    }
    else {
      // file opened
      while (string_fgetline(file, line)) {
        lineNo++;
        // skip empty lines and those starting with #, allowing to format and comment CSV
        if (line.empty() || line[0]=='#') {
          // skip this line
          continue;
        }
        string f;
        const char *p = line.c_str();
        // first field is scene number
        bool overridden = false;
        if (nextCSVField(p, f)) {
          const char *fp = f.c_str();
          if (!*fp) continue; // empty scene number field -> invalid line
          // check override prefix
          if (*fp=='!') {
            ++fp;
            overridden = true;
          }
          // read scene number
          int sceneNo;
          if (sscanf(fp, "%d", &sceneNo)!=1) {
            continue; // no valid scene number -> invalid line
            LOG(LOG_ERR, "%s:%d - no or invalid scene number", fn.c_str(), lineNo);
          }
          // check if this scene is already in the list (i.e. already has non-hardwired settings)
          DsSceneMap::iterator pos = scenes.find(sceneNo);
          DsScenePtr scene;
          if (pos!=scenes.end()) {
            // this scene already has settings, only apply if this is an overridden
            if (!overridden) continue; // scene already configured by more specialized level -> dont apply
            scene = pos->second;
          }
          else {
            // no settings yet, create the scene object
            scene = newDefaultScene(sceneNo);
          }
          // process rest of CSV line as property name/value pairs
          scene->readPropsFromCSV(VDC_API_DOMAIN, false, p, fn.c_str(), lineNo);
          // these changes are NOT to be made persistent in DB!
          scene->markClean();
          // put scene into table
          scenes[sceneNo] = scene;
          SALOG(device, LOG_INFO, "Customized scene %d %sfrom config file %s", sceneNo, overridden ? "(with override) " : "", fn.c_str());
        }
      }
      fclose(file);
    }
  }
}
