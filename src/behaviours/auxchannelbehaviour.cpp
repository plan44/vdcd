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

#include "auxchannelbehaviour.hpp"

using namespace p44;

AuxiliaryChannelBehaviour::AuxiliaryChannelBehaviour(Device &aDevice) :
  inherited(aDevice)
{
  // set default hardware default configuration
  setHardwareOutputConfig(outputFunction_switch, channeltype_undefined, usage_undefined, false, -1);
  // default to joker
  setGroup(group_black_joker);
}




int32_t AuxiliaryChannelBehaviour::getOutputValue()
{
  // TODO: fetch from primary output
  return cachedOutputValue;
}


void AuxiliaryChannelBehaviour::setOutputValue(int32_t aNewValue, MLMicroSeconds aTransitionTime)
{
  // TODO: communicate with primary output instead of triggering a update here?
  //%%% for now, let inherited handle it
  inherited::setOutputValue(aNewValue, aTransitionTime);
}


void AuxiliaryChannelBehaviour::onAtMinBrightness()
{
  // TODO: communicate with primary output instead of triggering a update here?
  //%%% for now, let inherited handle it
  inherited::onAtMinBrightness();
}


void AuxiliaryChannelBehaviour::processControlValue(const string &aName, double aValue)
{
  // TODO: communicate with primary output instead of triggering a update here?
  //%%% for now, let inherited handle it
  inherited::processControlValue(aName, aValue);

}


#pragma mark - description/shortDesc


string AuxiliaryChannelBehaviour::shortDesc()
{
  return string("AuxChannel");
}

