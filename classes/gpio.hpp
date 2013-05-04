//
//  gpio.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 03.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__gpio__
#define __p44bridged__gpio__

#include <string>

#ifdef __APPLE__
#define GPIO_SIMULATION 1
#endif


using namespace std;

namespace p44 {

  class Gpio
  {
    int gpioFD;
    bool pinState;
    bool output;
    bool inverted;
    string name;
    #if GPIO_SIMULATION
    int inputBitNo;
    #endif
  public:
    Gpio(const char* aGpioName, bool aOutput, bool aInverted = false, bool aInitialState = false);
    ~Gpio();
    void setState(bool aState);
    bool getState();
  };


  class ButtonGpio : public Gpio
  {
    
  };




} // namespace p44

#endif /* defined(__p44bridged__gpio__) */
