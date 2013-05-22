//
//  buttonbehaviour.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__buttonbehaviour__
#define __p44bridged__buttonbehaviour__

#include "device.hpp"

using namespace std;

namespace p44 {

  class ButtonBehaviour : public DSBehaviour
  {
  public:

    /// @name functional identification for digitalSTROM system
    /// @{

    virtual uint16_t functionId() = 0;
    virtual uint16_t productId() = 0;
    virtual uint16_t groupMemberShip() = 0;
    virtual uint8_t ltMode() = 0;
    virtual uint8_t outputMode() = 0;
    virtual uint8_t buttonIdGroup() = 0;

    /// @}


  };
  
}


#endif /* defined(__p44bridged__buttonbehaviour__) */
