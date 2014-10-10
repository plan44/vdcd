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

#ifndef __vdcd__climatecontrolbehaviour__
#define __vdcd__climatecontrolbehaviour__

#include "device.hpp"
#include "outputbehaviour.hpp"

using namespace std;

namespace p44 {

  class HeatingLevelChannel : public ChannelBehaviour
  {
    typedef ChannelBehaviour inherited;

  public:
    HeatingLevelChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 1; /* light defaults to historic dS resolution */ };

    virtual DsChannelType getChannelType() { return channeltype_default; }; ///< TODO: needs proper channel type
    virtual const char *getName() { return "heatingLevel"; };
    virtual double getMin() { return 0; }; // heating is 0..100 (cooling would be -100..0)
    virtual double getMax() { return 100; };
    virtual double getDimPerMS() { return 100/FULL_SCALE_DIM_TIME_MS; }; // 7 seconds full scale
    
  };



  /// Implements the behaviour of climate control outputs, in particular evaluating
  /// control values with processControlValue()
  class ClimateControlBehaviour : public OutputBehaviour
  {
    typedef OutputBehaviour inherited;

    /// @name hardware derived parameters (constant during operation)
    /// @{
    /// @}


    /// @name persistent settings
    /// @{
    /// @}


    /// @name internal volatile state
    /// @{
    /// @}

  public:
    ClimateControlBehaviour(Device &aDevice);

    /// @name interface towards actual device hardware (or simulation)
    /// @{


    /// @}


    /// @name interaction with digitalSTROM system
    /// @{

    /// check for presence of model feature (flag in dSS visibility matrix)
    /// @param aFeatureIndex the feature to check for
    /// @return true if this output behaviour has the feature (which means dSS Configurator must provide UI for it)
    virtual bool hasModelFeature(DsModelFeatures aFeatureIndex);

    /// Process a named control value. The type, color and settings of the output determine if at all,
    /// and if, how the value affects the output
    /// @param aName the name of the control value, which describes the purpose
    /// @param aValue the control value to process
    virtual void processControlValue(const string &aName, double aValue);

    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  };

  typedef boost::intrusive_ptr<ClimateControlBehaviour> ClimateControlBehaviourPtr;

} // namespace p44

#endif /* defined(__vdcd__climatecontrolbehaviour__) */
