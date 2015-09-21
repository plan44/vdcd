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

#ifndef __vdcd__devicesettings__
#define __vdcd__devicesettings__

#include "persistentparams.hpp"

#include "dsdefs.h"

using namespace std;

namespace p44 {

  class Device;
  class DeviceSettings;
  class DsScene;


  /// Base class for persistent settings common to all devices.
  /// @note This class can be used as-is for devices without a scene table (such as pure inputs and sensors),
  ///   but it also is the base class for SceneDeviceSettings which implements a scene table on top of it.
  class DeviceSettings : public PersistentParams, public P44Obj
  {
    typedef PersistentParams inherited;
    friend class Device;
    friend class DsScene;

  protected:

    Device &device;

    /// generic device flag word, can be used by subclasses to map flags onto at loadFromRow() and bindToStatement()
    int deviceFlags;
    /// global dS zone ID, zero if no zone assigned
    int zoneID;

  public:
    DeviceSettings(Device &aDevice);
    virtual ~DeviceSettings() {}; // important for multiple inheritance!

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

    /// @}

  };
  typedef boost::intrusive_ptr<DeviceSettings> DeviceSettingsPtr;

  
} // namespace p44


#endif /* defined(__vdcd__devicesettings__) */
