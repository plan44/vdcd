//
//  lightbehaviour.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 19.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__lightbehaviour__
#define __p44bridged__lightbehaviour__

#include "device.hpp"

using namespace std;

namespace p44 {

  class LightBehaviour : public DSBehaviour
  {
  public:
    /// @return current brightness value, 0..255, linear brightness as perceived by humans (half value = half brightness)
    uint8_t getBrightness();

  };

}

#endif /* defined(__p44bridged__lightbehaviour__) */
