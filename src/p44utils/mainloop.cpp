//
//  Copyright (c) 2013 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

#include "mainloop.hpp"

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
#include <unistd.h>
#include <sys/param.h>
#include <sys/wait.h>

#include "fdcomm.hpp"

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
  return ((uint64_t)(tsp.tv_sec))*1000000ll + tsp.tv_nsec/1000; // uS
#endif
}


// the current thread's main looop
#if BOOST_DISABLE_THREADS
static MainLoop *currentMainLoopP = NULL;
#else
static __thread MainLoop *currentMainLoopP = NULL;
#endif

// get the per-thread singleton mainloop
MainLoop &MainLoop::currentMainLoop()
{
	if (currentMainLoopP==NULL) {
		// need to create it
		currentMainLoopP = new MainLoop();
	}
	return *currentMainLoopP;
}


ErrorPtr ExecError::exitStatus(int aExitStatus, const char *aContextMessage)
{
  if (aExitStatus==0)
    return ErrorPtr(); // empty, no error
  return ErrorPtr(new ExecError(aExitStatus, nonNullCStr(aContextMessage)));
}



MainLoop::MainLoop() :
	terminated(false),
  loopCycleTime(MAINLOOP_DEFAULT_CYCLE_TIME_uS),
  cycleStartTime(Never),
  exitCode(EXIT_SUCCESS),
  idleHandlersChanged(false),
  oneTimeHandlersChanged(false)
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
      idleHandlersChanged = true;
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
      oneTimeHandlersChanged = true;
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
      oneTimeHandlersChanged = true;
		}
		else {
			// skip
		  ++pos;
		}
	}
}


void MainLoop::cancelExecutionTicket(long &aTicketNo)
{
  if (aTicketNo==0) return; // no ticket, NOP
  for (OnetimeHandlerList::iterator pos = onetimeHandlers.begin(); pos!=onetimeHandlers.end(); ++pos) {
		if (pos->ticketNo==aTicketNo) {
			pos = onetimeHandlers.erase(pos);
      oneTimeHandlersChanged = true;
      break;
		}
	}
  // reset the ticket
  aTicketNo = 0;
}


void MainLoop::waitForPid(WaitCB aCallback, pid_t aPid)
{
  LOG(LOG_DEBUG,"waitForPid: requested wait for pid=%d\n", aPid);
  if (aCallback) {
    // install new callback
    WaitHandler h;
    h.callback = aCallback;
    h.pid = aPid;
    waitHandlers[aPid] = h;
  }
  else {
    WaitHandlerMap::iterator pos = waitHandlers.find(aPid);
    if (pos!=waitHandlers.end()) {
      // remove it from list
      waitHandlers.erase(pos);
    }
  }
}


extern char **environ;


void MainLoop::fork_and_execve(ExecCB aCallback, const char *aPath, char *const aArgv[], char *const aEnvp[], bool aPipeBackStdOut)
{
  LOG(LOG_DEBUG,"fork_and_execve: preparing to fork for executing '%s' now\n", aPath);
  pid_t child_pid;
  int answerPipe[2]; /* Child to parent pipe */

  // prepare environment
  if (aEnvp==NULL) {
    aEnvp = environ;
  }
  // prepare pipe in case we want answer collected
  if (aPipeBackStdOut) {
    if(pipe(answerPipe)<0) {
      // pipe could not be created
      aCallback(*this, cycleStartTime, SysError::errNo(),"");
      return;
    }
  }
  // fork child process
  child_pid = fork();
  if (child_pid>=0) {
    // fork successful
    if (child_pid==0) {
      // this is the child process (fork() returns 0 for the child process)
      LOG(LOG_DEBUG,"forked child process: preparing for execve\n", aPath);
      if (aPipeBackStdOut) {
        dup2(answerPipe[1],STDOUT_FILENO); // replace STDOUT by writing end of pipe
        close(answerPipe[1]); // release the original descriptor (does NOT really close the file)
        close(answerPipe[0]); // close child's reading end of pipe (parent uses it!)
      }
      // close all non-std file descriptors
      int fd = getdtablesize();
      while (fd>STDERR_FILENO) close(fd--);
      // change to the requested child process
      execve(aPath, aArgv, aEnvp); // replace process with new binary/script
      // execv returns only in case of error
      exit(127);
    }
    else {
      // this is the parent process, wait for the child to terminate
      LOG(LOG_DEBUG,"fork_and_execve: child pid=%d, parent will now set up pipe string collector\n", child_pid);
      FdStringCollectorPtr ans;
      if (aPipeBackStdOut) {
        LOG(LOG_DEBUG,"fork_and_execve: parent will now set up pipe string collector\n");
        close(answerPipe[1]); // close parent's writing end (child uses it!)
        // set up collector for data returned from child process
        ans = FdStringCollectorPtr(new FdStringCollector(SyncIOMainLoop::currentMainLoop()));
        ans->setFd(answerPipe[0]);
      }
      LOG(LOG_DEBUG,"fork_and_execve: now calling waitForPid(%d)\n", child_pid);
      waitForPid(boost::bind(&MainLoop::execChildTerminated, this, aCallback, ans, _3, _4), child_pid);
    }
  }
  else {
    if (aCallback) {
      // fork failed, call back with error
      aCallback(*this, cycleStartTime, SysError::errNo(),"");
    }
  }
  return;
}


void MainLoop::fork_and_system(ExecCB aCallback, const char *aCommandLine, bool aPipeBackStdOut)
{
  char * args[4];
  args[0] = (char *)"sh";
  args[1] = (char *)"-c";
  args[2] = (char *)aCommandLine;
  args[3] = NULL;
  fork_and_execve(aCallback, "/bin/sh", args, NULL, aPipeBackStdOut);
}


void MainLoop::execChildTerminated(ExecCB aCallback, FdStringCollectorPtr aAnswerCollector, pid_t aPid, int aStatus)
{
  LOG(LOG_DEBUG,"execChildTerminated: pid=%d, aStatus=%d\n", aPid, aStatus);
  if (aCallback) {
    LOG(LOG_DEBUG,"- callback set, execute it\n");
    ErrorPtr err = ExecError::exitStatus(WEXITSTATUS(aStatus));
    if (aAnswerCollector) {
      LOG(LOG_DEBUG,"- aAnswerCollector: starting collectToEnd\n");
      aAnswerCollector->collectToEnd(boost::bind(&MainLoop::childAnswerCollected, this, aCallback, aAnswerCollector, err));
    }
    else {
      // call back directly
      LOG(LOG_DEBUG,"- no aAnswerCollector: callback immediately\n");
      aCallback(*this, cycleStartTime, err, "");
    }
  }
}


void MainLoop::childAnswerCollected(ExecCB aCallback, FdStringCollectorPtr aAnswerCollector, ErrorPtr aError)
{
  LOG(LOG_DEBUG,"childAnswerCollected: error = %s\n", Error::isOK(aError) ? "none" : aError->description().c_str());
  // close my end of the pipe
  aAnswerCollector->stopMonitoringAndClose();
  // now get answer
  string answer = aAnswerCollector->collectedData;
  LOG(LOG_DEBUG,"- Answer = %s\n", answer.c_str());
  // call back directly
  aCallback(*this, cycleStartTime, aError, answer);
}




void MainLoop::terminate(int aExitCode)
{
  exitCode = aExitCode;
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
      allCompleted = allCompleted && checkWait();
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
    } // not terminated
  } // not terminated
	return exitCode;
}


void MainLoop::runOnetimeHandlers()
{
  int rep = 5; // max 5 re-evaluations of list due to changes
  do {
    OnetimeHandlerList::iterator pos = onetimeHandlers.begin();
    oneTimeHandlersChanged = false; // detect changes happening from callbacks
    while (pos!=onetimeHandlers.end() && pos->executionTime<=cycleStartTime) {
      if (terminated) return; // terminated means everything is considered complete
      OneTimeCB cb = pos->callback; // get handler
      pos = onetimeHandlers.erase(pos); // remove from queue
      cb(*this, cycleStartTime); // call handler
      if (oneTimeHandlersChanged) {
        // callback has caused change of onetime handlers list, pos gets invalid
        break; // but done for now
      }
      ++pos;
    }
  } while(oneTimeHandlersChanged && rep-->0); // limit repetitions due to changed one time handlers to prevent endless loop
}


bool MainLoop::runIdleHandlers()
{
	IdleHandlerList::iterator pos = idleHandlers.begin();
  bool allCompleted = true;
  idleHandlersChanged = false; // detect changes happening from callbacks
  while (pos!=idleHandlers.end()) {
    if (terminated) return true; // terminated means everything is considered complete
    IdleCB cb = pos->callback; // get handler
    allCompleted = allCompleted && cb(*this, cycleStartTime); // call handler
    if (idleHandlersChanged) {
      // callback has caused change of idlehandlers list, pos gets invalid
      return false; // not really completed, cause calling again soon
    }
		++pos;
  }
  return allCompleted;
}


bool MainLoop::checkWait()
{
  if (waitHandlers.size()>0) {
    // check for process signal
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid>0) {
      LOG(LOG_DEBUG,"checkWait: child pid=%d reports exit status %d\n", pid, status);
      // process has status
      WaitHandlerMap::iterator pos = waitHandlers.find(pid);
      if (pos!=waitHandlers.end()) {
        // we have a callback
        WaitCB cb = pos->second.callback; // get it
        // remove it from list
        waitHandlers.erase(pos);
        // call back
        LOG(LOG_DEBUG,"- calling wait handler for pid=%d now\n", pid);
        cb(*this, cycleStartTime, pid, status);
        return false; // more process status could be ready, call soon again
      }
    }
    else if (pid<0) {
      // error when calling waitpid
      int e = errno;
      if (e==ECHILD) {
        // no more children
        LOG(LOG_DEBUG,"checkWait: no children any more -> ending all waits\n");
        // - inform all still waiting handlers
        WaitHandlerMap oldHandlers = waitHandlers; // copy
        waitHandlers.clear(); // remove all handlers from real list, as new handlers might be added in handlers we'll call now
        for (WaitHandlerMap::iterator pos = oldHandlers.begin(); pos!=oldHandlers.end(); pos++) {
          WaitCB cb = pos->second.callback; // get callback
          cb(*this, cycleStartTime, pos->second.pid, 0); // fake status
        }
      }
      else {
        LOG(LOG_DEBUG,"checkWait: waitpid returns error %s\n", strerror(e));
      }
    }
  }
  return true; // all checked
}




#pragma mark - SyncIOMainLoop



// get the per-thread singleton Synchronous IO mainloop
SyncIOMainLoop &SyncIOMainLoop::currentMainLoop()
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
  assert(mlP); // must exist now
	return *mlP;
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
          if (pos->second.pollHandler(*this, cycleStartTime, pollfdP->fd, pollfdP->revents))
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
      allCompleted = allCompleted && checkWait();
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
    } // not terminated
  } // not terminated
	return exitCode;
}


#pragma mark - execution in subthreads


ChildThreadWrapperPtr SyncIOMainLoop::executeInThread(ThreadRoutine aThreadRoutine, ThreadSignalHandler aThreadSignalHandler)
{
  return ChildThreadWrapperPtr(new ChildThreadWrapper(*this, aThreadRoutine, aThreadSignalHandler));
}


#pragma mark - ChildThreadWrapper


static void *thread_start_function(void *arg)
{
  // pass into method of wrapper
  return static_cast<ChildThreadWrapper *>(arg)->startFunction();
}


void *ChildThreadWrapper::startFunction()
{
  // run the routine
  threadRoutine(*this);
  // signal termination
  terminated();
  return NULL;
}



ChildThreadWrapper::ChildThreadWrapper(SyncIOMainLoop &aParentThreadMainLoop, ThreadRoutine aThreadRoutine, ThreadSignalHandler aThreadSignalHandler) :
  parentThreadMainLoop(aParentThreadMainLoop),
  threadRoutine(aThreadRoutine),
  parentSignalHandler(aThreadSignalHandler),
  threadRunning(false)
{
  // create a signal pipe
  int pipeFdPair[2];
  if (pipe(pipeFdPair)==0) {
    // pipe could be created
    // - save FDs
    parentSignalFd = pipeFdPair[0]; // 0 is the reading end
    childSignalFd = pipeFdPair[1]; // 1 is the writing end
    // - install poll handler in the parent mainloop
    parentThreadMainLoop.registerPollHandler(parentSignalFd, POLLIN, boost::bind(&ChildThreadWrapper::signalPipeHandler, this, _4));
    // create a pthread (with default attrs for now
    threadRunning = true; // before creating it, to make sure it is set when child starts to run
    if (pthread_create(&pthread, NULL, thread_start_function, this)!=0) {
      // error, could not create thread, fake a signal callback immediately
      threadRunning = false;
      if (parentSignalHandler)
        parentSignalHandler(aParentThreadMainLoop, *this, threadSignalFailedToStart);
    }
    else {
      // thread created ok, keep wrapper object alive
      selfRef = ChildThreadWrapperPtr(this);
    }
  }
  else {
    // pipe could not be created
    if (parentSignalHandler)
      parentSignalHandler(aParentThreadMainLoop, *this, threadSignalFailedToStart);
  }
}


ChildThreadWrapper::~ChildThreadWrapper()
{
  // cancel thread
  cancel();
}



// called from child thread when terminated
void ChildThreadWrapper::terminated()
{
  signalParentThread(threadSignalCompleted);
}




// called from child thread to send signal
void ChildThreadWrapper::signalParentThread(ThreadSignals aSignalCode)
{
  uint8_t sigByte = aSignalCode;
  write(childSignalFd, &sigByte, 1);
}


// cleanup, called from parent thread
void ChildThreadWrapper::finalizeThreadExecution()
{
  // synchronize with actual end of thread execution
  pthread_join(pthread, NULL);
  threadRunning = false;
  // unregister the handler
  SyncIOMainLoop::currentMainLoop().unregisterPollHandler(parentSignalFd);
  // close the pipes
  close(childSignalFd);
  close(parentSignalFd);
}



// can be called from parent thread
void ChildThreadWrapper::cancel()
{
  if (threadRunning) {
    // cancel it
    pthread_cancel(pthread);
    // wait for cancellation to complete
    finalizeThreadExecution();
    // cancelled
    if (parentSignalHandler)
      parentSignalHandler(parentThreadMainLoop, *this, threadSignalCancelled);
  }
}



// called on parent thread from SyncIOMainloop
bool ChildThreadWrapper::signalPipeHandler(int aPollFlags)
{
  ThreadSignals sig = threadSignalNone;
  //DBGLOG(LOG_DEBUG, "\nMAINTHREAD: signalPipeHandler with pollFlags=0x%X\n", aPollFlags);
  if (aPollFlags & POLLIN) {
    uint8_t sigByte;
    ssize_t res = read(parentSignalFd, &sigByte, 1); // read signal byte
    if (res==1) {
      sig = (ThreadSignals)sigByte;
    }
  }
  else if (aPollFlags & POLLHUP) {
    // HUP means thread has terminated and closed the other end of the pipe already
    // - treat like receiving a threadSignalCompleted
    sig = threadSignalCompleted;
  }
  if (sig!=threadSignalNone) {
    // check for thread terminated
    if (sig==threadSignalCompleted) {
      // finalize thread execution first
      finalizeThreadExecution();
    }
    // got signal byte, call handler
    if (parentSignalHandler) {
      parentSignalHandler(parentThreadMainLoop, *this, sig);
    }
    // in case nobody keeps this object any more, it might be deleted now
    selfRef.reset();
    // handled some i/O
    return true;
  }
  return false; // did not handle any I/O
}


