//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

#include "p44obj.hpp"


using namespace p44;

namespace p44 {

  void intrusive_ptr_add_ref(P44Obj* o)
  {
    ++(o->refCount);
  }

  void intrusive_ptr_release(P44Obj* o)
  {
    if(--(o->refCount) == 0)
      delete o;
  }

//  template<typename T>
//  void intrusive_ptr_add_ref(T* o){
//    ++(o->refCount);
//  }
//
//  template<typename T>
//  void intrusive_ptr_release(T* o){
//    if(--(o->refCount) == 0)
//      delete o;
//  }



}
