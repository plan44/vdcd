//
//  climatecontrolbehaviour.hpp
//  vdcd
//
//  Created by Lukas Zeller on 27.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__climatecontrolbehaviour__
#define __vdcd__climatecontrolbehaviour__

#include "device.hpp"
#include "outputbehaviour.hpp"

using namespace std;

namespace p44 {


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
