//
//  Copyright (c) 2013 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
