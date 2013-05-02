//
//  mainloop.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 01.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "mainloop.hpp"

#define MAINLOOP_DEFAULT_CYCLE_TIME_MS 100


#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

// time reference in milliseconds
MLMilliSeconds MainLoop::now()
{
#ifdef __APPLE__
  static bool timeInfoKnown = false;
  static mach_timebase_info_data_t tb;
  if (!timeInfoKnown) {
    mach_timebase_info(&tb);
  }
  double t = mach_absolute_time();
  return t * (double)tb.numer / (double)tb.denom / 1e6; // ms
#else
  struct timespec tsp;
  clock_gettime(CLOCK_MONOTONIC, &tsp);
  // return milliseconds
  return tsp.tv_sec*1000 + tsp.tv_nsec/1000000;
#endif
}


// the current theread's main looop
static __thread MainLoop *currentMainLoopP = NULL;

// get the per-thread singleton mainloop
MainLoop *MainLoop::currentMainLoop()
{
	if (currentMainLoopP==NULL) {
		// need to create it
		currentMainLoopP = new MainLoop();
	}
	return currentMainLoopP;
}


MainLoop::MainLoop() :
	terminated(false),
  loopCycleTime(MAINLOOP_DEFAULT_CYCLE_TIME_MS)
{
	
}


void MainLoop::setLoopCycleTime(MLMilliSeconds aCycleTime)
{
	loopCycleTime = aCycleTime;
}


void MainLoop::registerIdleHandler(void *aSubscriberP, IdleCB aCallback)
{
	IdleHandler h;
	h.subscriberP = aSubscriberP;
	h.callback = aCallback;
	idleHandlers.push_back(h);
}


void MainLoop::unregisterIdleHandlers(void *aSubscriberP)
{
	IdleHandlerList::iterator pos = idleHandlers.begin();
	while(pos!=idleHandlers.end()) {
		if (pos->subscriberP==aSubscriberP) {
			pos = idleHandlers.erase(pos);
			#error is pos returning next element?
		}
		else {
			// skip
		  ++pos;
		}
	}
}


void MainLoop::executeOnce(IdleCB aCallback, MLMilliSeconds aDelay)
{
	MLMilliSeconds executionTime = now()+aDelay;
	executeOnceAt(aCallback, executionTime);
}


void MainLoop::executeOnceAt(IdleCB aCallback, MLMilliSeconds aExecutionTime)
{
	// place in queue at right time
}





