//
//  climatecontrolbehaviour.cpp
//  vdcd
//
//  Created by Lukas Zeller on 27.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "climatecontrolbehaviour.hpp"

using namespace p44;


ClimateControlBehaviour::ClimateControlBehaviour(Device &aDevice) :
  inherited(aDevice)
{
}



void ClimateControlBehaviour::processControlValue(const string &aName, double aValue)
{
  if (aName=="heatingLevel") {
    if (group==group_roomtemperature_control && outputMode!=outputmode_disabled) {
      // apply positive values to valve output, clip to 100 max
      setOutputValue(aValue<0 ? 0 : (aValue>100 ? 100 : aValue));
    }
  }
}





string ClimateControlBehaviour::shortDesc()
{
  return string("ClimateControl");
}


string ClimateControlBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  s.append(inherited::description());
  return s;
}
