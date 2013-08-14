//
//  application.cpp
//  p44utils
//
//  Created by Lukas Zeller on 02.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "application.hpp"

#include "mainloop.hpp"

#include <sys/stat.h> // for umask

using namespace p44;


static Application *sharedApplicationP = NULL;


Application *Application::sharedApplication()
{
  return sharedApplicationP;
}


Application::Application(MainLoop *aMainLoopP)
{
	mainLoopP = aMainLoopP;
  sharedApplicationP = this;
}


Application::Application()
{
	mainLoopP = MainLoop::currentMainLoop();
  sharedApplicationP = this;
}

Application::~Application()
{
}


int Application::main(int argc, char **argv)
{
	// NOP application
	exit(EXIT_SUCCESS);
}


void Application::initialize()
{
	// NOP
}


int Application::run()
{
	if (!mainLoopP)
		return EXIT_SUCCESS; // NOP
	// schedule the initialize() method as first mainloop method
	mainLoopP->executeOnce(boost::bind(&Application::initialize, this));
	// run the mainloop
	return mainLoopP->run();
}


void Application::daemonize()
{
  pid_t pid, sid;
	
  /* already a daemon */
  if ( getppid() == 1 ) return;
	
  /* Fork off the parent process */
  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  /* If we got a good PID, then we can exit the parent process. */
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }
	
  /* At this point we are executing as the child process */
	
  /* Change the file mode mask */
  umask(0);
	
  /* Create a new SID for the child process */
  sid = setsid();
  if (sid < 0) {
    exit(EXIT_FAILURE);
  }
	
  /* Change the current working directory.  This prevents the current
	 directory from being locked; hence not being able to remove it. */
  if ((chdir("/")) < 0) {
    exit(EXIT_FAILURE);
  }
	
  /* Redirect standard files to /dev/null */
  freopen( "/dev/null", "r", stdin);
  freopen( "/dev/null", "w", stdout);
  freopen( "/dev/null", "w", stderr);
}
