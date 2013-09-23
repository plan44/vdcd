//
//  devicesettings.hpp
//  vdcd
//
//  Created by Lukas Zeller on 22.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__devicesettings__
#define __vdcd__devicesettings__

#include "persistentparams.hpp"


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
