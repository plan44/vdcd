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

#include "valuesource.hpp"

using namespace p44;

ValueSource::ValueSource()
{
}


ValueSource::~ValueSource()
{
  // inform all of the listeners that the value is gone
  notifyListeners(valueevent_removed);
  listeners.clear();
}


void ValueSource::addSourceListener(ValueListenerCB aCallback, void *aListener)
{
  listeners.insert(make_pair(aListener, aCallback));
}


void ValueSource::removeSourceListener(void *aListener)
{
  listeners.erase(aListener);
}



void ValueSource::notifyListeners(ValueListenerEvent aEvent)
{
  for (ListenerMap::iterator pos=listeners.begin(); pos!=listeners.end(); ++pos) {
    ValueListenerCB cb = pos->second;
    cb(*this, aEvent);
  }
}