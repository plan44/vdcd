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

#ifndef __vdcd__devicesettings__
#define __vdcd__devicesettings__

#include "persistentparams.hpp"

#include "dsdefs.h"

using namespace std;

namespace p44 {

  class Device;
  class DeviceSettings;


  /// common settings for devices, can be used when device does not need a scene table, but has some global settings
  class DeviceSettings : public PersistentParams, public P44Obj
  {
    typedef PersistentParams inherited;
    friend class Device;

    Device &device;

  protected:

    /// generic device flag word, can be used by subclasses to map flags onto at loadFromRow() and bindToStatement()
    int deviceFlags;
    /// global dS zone ID
    int zoneID;
    /// global extra groups
    DsGroupMask extraGroups;

  public:
    DeviceSettings(Device &aDevice);
    virtual ~DeviceSettings() {}; // important for multiple inheritance!

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);

    /// @}

  };
  typedef boost::intrusive_ptr<DeviceSettings> DeviceSettingsPtr;

  
} // namespace p44


#endif /* defined(__vdcd__devicesettings__) */
