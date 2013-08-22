//
//  application.hpp
//  p44utils
//
//  Created by Lukas Zeller on 02.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44utils__application__
#define __p44utils__application__

#include "p44_common.hpp"

using namespace std;

namespace p44 {

  class MainLoop;

  class Application : public P44Obj
  {
    MainLoop *mainLoopP;
  public:
    /// constructor
    Application(MainLoop *aMainLoopP);
    /// default constructor
    Application();
    /// destructor
    virtual ~Application();
    /// main routine
    virtual int main(int argc, char **argv);
    /// get shared instance (singleton)
    Application *sharedApplication();
  protected:
    /// daemonize
    void daemonize();
    /// start running the app's main loop
    int run();
    /// scheduled to run when mainloop has started
    virtual void initialize();
  };

} // namespace p44


#endif /* defined(__p44utils__application__) */
