//
//  p44obj.cpp
//  vdcd
//
//  Created by Lukas Zeller on 23.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
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
