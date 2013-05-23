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
    typedef DSBehaviour inherited;
  public:
    // constructor
    ButtonBehaviour(Device *aDeviceP);

    /// @name functional identification for digitalSTROM system
    /// @{

    virtual uint16_t functionId();
    virtual uint16_t productId();
    virtual uint16_t groupMemberShip();
    virtual uint8_t ltMode();
    virtual uint8_t outputMode();
    virtual uint8_t buttonIdGroup();

    /// @}

    /// @name interface towards actual device hardware (or simulation)

    /// button action occurred
    /// @param aPressed true if action is button pressed, false if action is button released
    void buttonAction(bool aPressed);
    
    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  };
  
}


#endif /* defined(__p44bridged__buttonbehaviour__) */
