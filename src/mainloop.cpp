//
//  mainloop.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 01.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "mainloop.hpp"

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
#include <unistd.h>
#include <sys/poll.h>
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



void SyncIOMainLoop::syncIOHandlerForFd(int aFD, SyncIOHandler &h)
{
  SyncIOHandlerMap::iterator pos = syncIOHandlers.find(aFD);
  if (pos!=syncIOHandlers.end())
    h = pos->second;
  else {
    h.monitoredFD = aFD;
    h.readReadyCB = NULL;
    h.writeReadyCB = NULL;
    h.errorCB = NULL;
  }
}


void SyncIOMainLoop::registerReadReadyHandler(int aFD, SyncIOCB aReadReadyCB)
{
  SyncIOHandler h;
  syncIOHandlerForFd(aFD, h);
  h.readReadyCB = aReadReadyCB;
	syncIOHandlers[aFD] = h;
}

void SyncIOMainLoop::registerWriteReadyHandler(int aFD, SyncIOCB aWriteReadyCB)
{
  SyncIOHandler h;
  syncIOHandlerForFd(aFD, h);
  h.writeReadyCB = aWriteReadyCB;
	syncIOHandlers[aFD] = h;
}

void SyncIOMainLoop::registerIOErrorHandler(int aFD, SyncIOCB aIOErrorCB)
{
  SyncIOHandler h;
  syncIOHandlerForFd(aFD, h);
  h.errorCB = aIOErrorCB;
	syncIOHandlers[aFD] = h;
}


void SyncIOMainLoop::registerSyncIOHandlers(int aFD, SyncIOCB aReadCB, SyncIOCB aWriteCB, SyncIOCB aErrorCB)
{
  SyncIOHandler h;
  h.monitoredFD = aFD;
  h.readReadyCB = aReadCB;
  h.writeReadyCB = aWriteCB;
  h.errorCB = aErrorCB;
	syncIOHandlers[aFD] = h;
}


void SyncIOMainLoop::unregisterSyncIOHandlers(int aFD)
{
  syncIOHandlers.erase(aFD);
}



bool SyncIOMainLoop::handleSyncIO(MLMicroSeconds aTimeout)
{
  // create poll structure
  struct pollfd *pollFds = NULL;
  size_t numFDsToTest = syncIOHandlers.size();
  if (numFDsToTest>0) {
    // allocate pollfd array
    pollFds = new struct pollfd[numFDsToTest];
  }
  // fill poll structure
  SyncIOHandlerMap::iterator pos = syncIOHandlers.begin();
  size_t i = 0;
  // collect FDs
  while (pos!=syncIOHandlers.end()) {
    SyncIOHandler h = pos->second;
    struct pollfd *pollfdP = &pollFds[i];
    pollfdP->fd = h.monitoredFD;
    pollfdP->revents = 0; // no event returned so far
    pollfdP->events =
      (h.readReadyCB ? POLLIN : 0) | // interested when ready to read
      (h.writeReadyCB ? POLLOUT : 0); // interested when ready to write
    ++i;
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
  if (numReadyFDs>0) {
    // check the descriptor sets and call handlers when needed
    for (int i = 0; i<numFDsToTest; i++) {
      struct pollfd *pollfdP = &pollFds[i];
      bool readReady = pollfdP->revents & POLLIN;
      bool writeReady = pollfdP->revents & POLLOUT;
      bool errorFound = pollfdP->revents & (POLLERR|POLLHUP|POLLNVAL);
      if (readReady || writeReady || errorFound) {
        SyncIOHandler h = syncIOHandlers[pollfdP->fd];
        if (readReady) h.readReadyCB(this, cycleStartTime, i, pollfdP->revents);
        if (writeReady) h.writeReadyCB(this, cycleStartTime, i, pollfdP->revents);
        if (errorFound) h.errorCB(this, cycleStartTime, i, pollfdP->revents);
      }
    }
  }
  // return true if we actually handled some I/O
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

