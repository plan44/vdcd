//
//  p44obj.hpp
//  p44utils
//
//  Created by Lukas Zeller on 23.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__p44obj__
#define __vdcd__p44obj__

#include <boost/intrusive_ptr.hpp>

namespace p44 {

  class P44Obj;

  void intrusive_ptr_add_ref(P44Obj* o);
  void intrusive_ptr_release(P44Obj* o);

  class P44Obj {
    friend void intrusive_ptr_add_ref(P44Obj* o);
    friend void intrusive_ptr_release(P44Obj* o);

    int refCount;

  protected:
    P44Obj() : refCount(0) {};
    virtual ~P44Obj() {}; // important for multiple inheritance
  };



} // namespace p44


#endif /* defined(__vdcd__p44obj__) */
