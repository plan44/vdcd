//
//  mainloop.cpp
//  p44utils
//
//  Created by Lukas Zeller on 01.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "mainloop.hpp"

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
#include <unistd.h>
#include <sys/param.h>

#pragma mark - MainLoop


#define MAINLOOP_DEFAULT_CYCLE_TIME_uS 100000 // 100mS


using namespace p44;

// time reference in milliseconds
MLMicroSeconds MainLoop::now()
{
#ifdef __APPLE__
  static bool timeInfoKnown = false;
  static mach_timebase_info_data_t tb;
  if (!timeInfoKnown) {
    mach_timebase_info(&tb);
  }
  double t = mach_absolute_time();
  return t * (double)tb.numer / (double)tb.denom / 1e3; // uS
#else
  struct timespec tsp;
  clock_gettime(CLOCK_MONOTONIC, &tsp);
  // return milliseconds
  return tsp.tv_sec*1000000 + tsp.tv_nsec/1000; // uS
#endif
}


// the current thread's main looop
#if BOOST_DISABLE_THREADS
static MainLoop *currentMainLoopP = NULL;
#else
static __thread MainLoop *currentMainLoopP = NULL;
#endif

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
  loopCycleTime(MAINLOOP_DEFAULT_CYCLE_TIME_uS),
  cycleStartTime(Never)
{
}


void MainLoop::setLoopCycleTime(MLMicroSeconds aCycleTime)
{
	loopCycleTime = aCycleTime;
}


MLMicroSeconds MainLoop::remainingCycleTime()
{
  return cycleStartTime+loopCycleTime-now();
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
		}
		else {
			// skip
		  ++pos;
		}
	}
}


long MainLoop::executeOnce(OneTimeCB aCallback, MLMicroSeconds aDelay, void *aSubmitterP)
{
	MLMicroSeconds executionTime = now()+aDelay;
	return executeOnceAt(aCallback, executionTime, aSubmitterP);
}


long MainLoop::executeOnceAt(OneTimeCB aCallback, MLMicroSeconds aExecutionTime, void *aSubmitterP)
{
	OnetimeHandler h;
  h.ticketNo = ++ticketNo;
  h.submitterP = aSubmitterP;
  h.executionTime = aExecutionTime;
	h.callback = aCallback;
	// insert in queue before first item that has a higher execution time
	OnetimeHandlerList::iterator pos = onetimeHandlers.begin();
  while (pos!=onetimeHandlers.end()) {
    if (pos->executionTime>aExecutionTime) {
      onetimeHandlers.insert(pos, h);
      return ticketNo;
    }
    ++pos;
  }
  // none executes later than this one, just append
  onetimeHandlers.push_back(h);
  return ticketNo;
}


void MainLoop::cancelExecutionsFrom(void *aSubmitterP)
{
	OnetimeHandlerList::iterator pos = onetimeHandlers.begin();
	while(aSubmitterP==NULL || pos!=onetimeHandlers.end()) {
		if (pos->submitterP==aSubmitterP) {
			pos = onetimeHandlers.erase(pos);
		}
		else {
			// skip
		  ++pos;
		}
	}
}


void MainLoop::cancelExecutionTicket(long aTicketNo)
{
  if (aTicketNo==0) return; // no ticket, NOP
  for (OnetimeHandlerList::iterator pos = onetimeHandlers.begin(); pos!=onetimeHandlers.end(); ++pos) {
		if (pos->ticketNo==aTicketNo) {
			pos = onetimeHandlers.erase(pos);
      break;
		}
	}
}



void MainLoop::terminate()
{
  terminated = true;
}


int MainLoop::run()
{
  while (!terminated) {
    cycleStartTime = now();
    // start of a new cycle
    while (!terminated) {
      runOnetimeHandlers();
      if (terminated) break;
			bool allCompleted = runIdleHandlers();
      if (terminated) break;
      MLMicroSeconds timeLeft = remainingCycleTime();
      if (timeLeft>0) {
        if (allCompleted) {
          // nothing to do any more, sleep rest of cycle
          usleep((useconds_t)timeLeft);
          break; // end of cycle
        }
        // not all completed, use time for running handlers again
      }
      else {
        // no time left, end of cycle
        break;
      }
    }
  }
	return EXIT_SUCCESS;
}


void MainLoop::runOnetimeHandlers()
{
	OnetimeHandlerList::iterator pos = onetimeHandlers.begin();
  while (pos!=onetimeHandlers.end() && pos->executionTime<=cycleStartTime) {
    if (terminated) return; // terminated means everything is considered complete
    OneTimeCB cb = pos->callback; // get handler
    pos = onetimeHandlers.erase(pos); // remove from queue
    cb(this, cycleStartTime); // call handler
    ++pos;
  }
}


bool MainLoop::runIdleHandlers()
{
	IdleHandlerList::iterator pos = idleHandlers.begin();
  bool allCompleted = true;
  while (pos!=idleHandlers.end()) {
    if (terminated) return true; // terminated means everything is considered complete
    IdleCB cb = pos->callback; // get handler
    allCompleted = allCompleted && cb(this, cycleStartTime); // call handler
		++pos;
  }
  return allCompleted;
}


#pragma mark - SyncIOMainLoop



// get the per-thread singleton Synchronous IO mainloop
SyncIOMainLoop *SyncIOMainLoop::currentMainLoop()
{
  SyncIOMainLoop *mlP = NULL;
	if (currentMainLoopP==NULL) {
		// need to create it
		mlP = new SyncIOMainLoop();
    currentMainLoopP = mlP;
	}
  else {
    mlP = dynamic_cast<SyncIOMainLoop *>(currentMainLoopP);
  }
	return mlP;
}


SyncIOMainLoop::SyncIOMainLoop()
{
}



void SyncIOMainLoop::registerPollHandler(int aFD, int aPollFlags, SyncIOCB aPollEventHandler)
{
  if (aPollEventHandler.empty())
    unregisterPollHandler(aFD); // no handler means unregistering handler
  // register new handler
  SyncIOHandler h;
  h.monitoredFD = aFD;
  h.pollFlags = aPollFlags;
  h.pollHandler = aPollEventHandler;
	syncIOHandlers[aFD] = h;
}


void SyncIOMainLoop::changePollFlags(int aFD, int aSetPollFlags, int aClearPollFlags)
{
  SyncIOHandlerMap::iterator pos = syncIOHandlers.find(aFD);
  if (pos!=syncIOHandlers.end()) {
    // found fd to set flags for
    if (aClearPollFlags>=0) {
      // read modify write
      // - clear specified flags
      pos->second.pollFlags &= ~aClearPollFlags;
      pos->second.pollFlags |= aSetPollFlags;
    }
    else {
      // just set
      pos->second.pollFlags = aSetPollFlags;
    }
  }
}



void SyncIOMainLoop::unregisterPollHandler(int aFD)
{
  syncIOHandlers.erase(aFD);
}



bool SyncIOMainLoop::handleSyncIO(MLMicroSeconds aTimeout)
{
  // create poll structure
  struct pollfd *pollFds = NULL;
  size_t maxFDsToTest = syncIOHandlers.size();
  if (maxFDsToTest>0) {
    // allocate pollfd array (max, in case some are disabled, we'll need less)
    pollFds = new struct pollfd[maxFDsToTest];
  }
  // fill poll structure
  SyncIOHandlerMap::iterator pos = syncIOHandlers.begin();
  size_t numFDsToTest = 0;
  // collect FDs
  while (pos!=syncIOHandlers.end()) {
    SyncIOHandler h = pos->second;
    if (h.pollFlags) {
      // don't include handlers that are currently disabled (no flags set)
      struct pollfd *pollfdP = &pollFds[numFDsToTest];
      pollfdP->fd = h.monitoredFD;
      pollfdP->events = h.pollFlags;
      pollfdP->revents = 0; // no event returned so far
      ++numFDsToTest;
    }
    ++pos;
  }
  // block until input becomes available or timeout
  int numReadyFDs = 0;
  if (numFDsToTest>0) {
    // actual FDs to test
    numReadyFDs = poll(pollFds, (int)numFDsToTest, (int)(aTimeout/MilliSecond));
  }
  else {
    // nothing to test, just await timeout
    if (aTimeout>0) {
      usleep((useconds_t)aTimeout);
    }
  }
  // call handlers
  bool didHandle = false;
  if (numReadyFDs>0) {
    // at least one of the flagged events has occurred in at least one FD
    // - find the FDs that are affected and call their handlers when needed
    for (int i = 0; i<numFDsToTest; i++) {
      struct pollfd *pollfdP = &pollFds[i];
      if (pollfdP->revents) {
        // an event has occurred for this FD
        // - get handler, note that it might have been deleted in the meantime
        SyncIOHandlerMap::iterator pos = syncIOHandlers.find(pollfdP->fd);
        if (pos!=syncIOHandlers.end()) {
          // - there is a handler
          if (pos->second.pollHandler(this, cycleStartTime, pollfdP->fd, pollfdP->revents))
            didHandle = true; // really handled (not just checked flags and decided it's nothing to handle)
        }
      }
    }
  }
  // return the poll array
  delete[] pollFds;
  // return true if poll actually reported something (not just timed out)
  return numReadyFDs>0;
}



int SyncIOMainLoop::run()
{
  while (!terminated) {
    cycleStartTime = now();
    // start of a new cycle
    while (!terminated) {
      runOnetimeHandlers();
      if (terminated) break;
			bool allCompleted = runIdleHandlers();
      if (terminated) break;
      MLMicroSeconds timeLeft = remainingCycleTime();
      // if other handlers have not completed yet, don't wait for I/O, just quickly check
      bool iohandled = false;
      if (!allCompleted || timeLeft<=0) {
        // no time to wait for I/O, just check
        iohandled = handleSyncIO(0);
      }
      else {
        // nothing to do except waiting for I/O
        iohandled = handleSyncIO(timeLeft);
        if (!iohandled) {
          // timed out, end of cycle
          break;
        }
        // not timed out, means we might still have some time left
      }
      // if no time left, end the cycle, otherwise re-run handlers
      if (terminated || remainingCycleTime()<=0)
        break; // no more time, end the cycle here
    }
  }
	return EXIT_SUCCESS;
}

