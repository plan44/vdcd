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

#ifndef __vdcd__auxchannelbehaviour__
#define __vdcd__auxchannelbehaviour__

#include "device.hpp"
#include "outputbehaviour.hpp"

using namespace std;

namespace p44 {


  /// Auxiliary output channel. This behaviour is usually associated with a primary OutputBehaviour
  /// (for example a ColorLightBehaviour) and serves to represent the auxiliary channels of a device.
  /// The actual compound behaviour is usually implemented in the primary output behaviour because
  /// often channel values must be combined/transformed to be applied to hardware.
  class AuxiliaryChannelBehaviour : public OutputBehaviour
  {
    typedef OutputBehaviour inherited;

    /// the primary output for which this channel is a auxiliary channel
    OutputBehaviour &primaryOutput;

  public:

    AuxiliaryChannelBehaviour(Device &aDevice, OutputBehaviour &PrimaryOutput);

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

    /// @name interaction with digitalSTROM system
    /// @{

    /// get currently set output value from device hardware
    virtual int32_t getOutputValue();

    /// set new output value on device
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness (if possible in hardware)
    virtual void setOutputValue(int32_t aNewValue, MLMicroSeconds aTransitionTime=0);

    /// switch on at minimum brightness if not already on (needed for callSceneMin), only relevant for lights
    virtual void onAtMinBrightness();

    /// Process a named control value. The type, color and settings of the output determine if at all,
    /// and if, how the value affects the output
    /// @param aName the name of the control value, which describes the purpose
    /// @param aValue the control value to process
    virtual void processControlValue(const string &aName, double aValue);

    /// @}


  };
  
  typedef boost::intrusive_ptr<AuxiliaryChannelBehaviour> AuxiliaryChannelBehaviourPtr;

} // namespace p44

#endif /* defined(__vdcd__auxchannelbehaviour__) */
