//
//  devicesettings.cpp
//  vdcd
//
//  Created by Lukas Zeller on 22.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "devicesettings.hpp"

#include "device.hpp"

using namespace p44;


DeviceSettings::DeviceSettings(Device &aDevice) :
  inherited(aDevice.getDeviceContainer().getDsParamStore()),
  device(aDevice),
  deviceFlags(0),
  zoneID(0)
{
}


// SQLIte3 table name to store these parameters to
const char *DeviceSettings::tableName()
{
  return "DeviceSettings";
}


// data field definitions

static const size_t numFields = 3;

size_t DeviceSettings::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *DeviceSettings::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "deviceFlags", SQLITE_INTEGER },
    { "deviceName", SQLITE_TEXT },
    { "zoneID", SQLITE_INTEGER }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void DeviceSettings::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the field value
  deviceFlags = aRow->get<int>(aIndex++);
  device.setName(nonNullCStr(aRow->get<const char *>(aIndex++)));
  zoneID = aRow->get<int>(aIndex++);
}


// bind values to passed statement
void DeviceSettings::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // bind the fields
  aStatement.bind(aIndex++, deviceFlags);
  aStatement.bind(aIndex++, device.getName().c_str());
  aStatement.bind(aIndex++, zoneID);
}
