//
//  Copyright (c) 2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__evaluatorvdc__
#define __vdcd__evaluatorvdc__

#include "vdcd_common.hpp"

#if ENABLE_EVALUATORS

#include "vdc.hpp"
#include "evaluatordevice.hpp"

using namespace std;

namespace p44 {

  class EvaluatorVdc;
  class EvaluatorDevice;

  /// persistence for static device container
  class EvaluatorDevicePersistence : public SQLite3Persistence  {
    typedef SQLite3Persistence inherited;
  protected:
    /// Get DB Schema creation/upgrade SQL statements
    virtual string dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion);
  };


  typedef boost::intrusive_ptr<EvaluatorVdc> EvaluatorVdcPtr;
  class EvaluatorVdc : public Vdc
  {
    typedef Vdc inherited;
    friend class EvaluatorDevice;

    EvaluatorDevicePersistence db;

  public:
    EvaluatorVdc(int aInstanceNumber, VdcHost *aVdcHostP, int aTag);

    void initialize(StatusCB aCompletedCB, bool aFactoryReset);

    virtual const char *vdcClassIdentifier() const;

    virtual void collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings);

    /// some containers (statically defined devices for example) should be invisible for the dS system when they have no
    /// devices.
    /// @return if true, this vDC should not be announced towards the dS system when it has no devices
    virtual bool invisibleWhenEmpty() { return true; }

    /// vdc level methods (p44 specific, JSON only, for configuring evaluator devices)
    virtual ErrorPtr handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams);

    /// @return human readable, language independent suffix to explain vdc functionality.
    ///   Will be appended to product name to create modelName() for vdcs
    virtual string vdcModelSuffix() { return "Sensor Evaluators"; }

    /// Get icon data or name
    /// @param aIcon string to put result into (when method returns true)
    /// - if aWithData is set, binary PNG icon data for given resolution prefix is returned
    /// - if aWithData is not set, only the icon name (without file extension) is returned
    /// @param aWithData if set, PNG data is returned, otherwise only name
    /// @return true if there is an icon, false if not
    virtual bool getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix);

  };

} // namespace p44


#endif // ENABLE_EVALUATORS
#endif // __vdcd__evaluatorvdc__
