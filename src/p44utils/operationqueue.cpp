//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "operationqueue.hpp"

using namespace p44;


Operation::Operation() :
  initiated(false),
  aborted(false),
  timeout(0), // no timeout
  timesOutAt(0), // no timeout time set
  initiationDelay(0), // no initiation delay
  initiatesNotBefore(0), // no initiation time
  inSequence(true) // by default, execute in sequence
{
}

// set timeout
void Operation::setTimeout(MLMicroSeconds aTimeout)
{
  timeout = aTimeout;
}


// set delay for initiation (after first attempt to initiate)
void Operation::setInitiationDelay(MLMicroSeconds aInitiationDelay)
{
  initiationDelay = aInitiationDelay;
  initiatesNotBefore = 0;
}

// set earliest time to execute
void Operation::setInitiatesAt(MLMicroSeconds aInitiatesAt)
{
  initiatesNotBefore = aInitiatesAt;
}


// check if can be initiated
bool Operation::canInitiate()
{
  MLMicroSeconds now = MainLoop::now();
  if (initiationDelay>0) {
    //DBGLOG(LOG_DEBUG,"requesting initiation delay of %lld uS\n", initiationDelay);
    if (initiatesNotBefore==0) {
      // first time queried, start delay now
      initiatesNotBefore = now+initiationDelay;
      //DBGLOG(LOG_DEBUG,"- now is %lld, will initiate at %lld uS\n", now, initiatesNotBefore);
      initiationDelay = 0; // consumed
    }
  }
  // can be initiated when delay is over
  return initiatesNotBefore==0 || initiatesNotBefore<now;
}



// call to initiate operation
bool Operation::initiate()
{
  if (!canInitiate()) return false;
  initiated = true;
  if (timeout!=0)
    timesOutAt = MainLoop::now()+timeout;
  else
    timesOutAt = 0;
  return initiated;
}


// check if already initiated
bool Operation::isInitiated()
{
  return initiated;
}


// call to check if operation has completed
bool Operation::hasCompleted()
{
  return true;
}


bool Operation::hasTimedOutAt(MLMicroSeconds aRefTime)
{
  if (timesOutAt==0) return false;
  return aRefTime>=timesOutAt;
}



#pragma mark - OperationQueue


// create operation queue into specified mainloop
OperationQueue::OperationQueue(MainLoop &aMainLoop) :
  mainLoop(aMainLoop)
{
  // register with mainloop
  mainLoop.registerIdleHandler(this, boost::bind(&OperationQueue::idleHandler, this));
}


// destructor
OperationQueue::~OperationQueue()
{
  // unregister from mainloop
  mainLoop.unregisterIdleHandlers(this);
}


// queue a new operation
void OperationQueue::queueOperation(OperationPtr aOperation)
{
  operationQueue.push_back(aOperation);
}


// process all pending operations now
void OperationQueue::processOperations()
{
	bool completed = true;
	do {
		completed = idleHandler();
	} while (!completed);
}



bool OperationQueue::idleHandler()
{
  bool pleaseCallAgainSoon = false; // assume nothing to do
  if (!operationQueue.empty()) {
    MLMicroSeconds now = MainLoop::now();
    OperationList::iterator pos;
    // (re)start with first element in queue
    for (pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
      OperationPtr op = *pos;
      if (op->hasTimedOutAt(now)) {
        // remove from list
        operationQueue.erase(pos);
        // abort with timeout
        op->abortOperation(ErrorPtr(new OQError(OQErrorTimedOut)));
        // restart with start of (modified) queue
        pleaseCallAgainSoon = true;
        break;
      }
      if (!op->isInitiated()) {
        // initiate now
        if (!op->initiate()) {
          // cannot initiate this one now, check if we can continue with others
          if (op->inSequence) {
            // this op needs to be initiated before others can be checked
            pleaseCallAgainSoon = false; // as we can't initate right now, let mainloop cycle pass
            break;
          }
        }
      }
      if (op->isInitiated()) {
        // initiated, check if already completed
        if (op->hasCompleted()) {
          // operation has completed
          // - remove from list
          OperationList::iterator nextPos = operationQueue.erase(pos);
          // - finalize. This might push new operations in front or back of the queue
          OperationPtr nextOp = op->finalize(this);
          if (nextOp) {
            operationQueue.insert(nextPos, nextOp);
          }
          // restart with start of (modified) queue
          pleaseCallAgainSoon = true;
          break;
        }
        else {
          // operation has not yet completed
          if (op->inSequence) {
            // this op needs to be complete before others can be checked
            pleaseCallAgainSoon = false; // as we can't initate right now, let mainloop cycle pass
            break;
          }
        }
      }
    } // for all ops in queue
  } // queue not empty
  // if not everything is processed we'd like to process, return false, causing main loop to call us ASAP again
  return !pleaseCallAgainSoon;
};



// abort all pending operations
void OperationQueue::abortOperations()
{
  for (OperationList::iterator pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
    (*pos)->abortOperation(ErrorPtr(new OQError(OQErrorAborted)));
  }
  // empty queue
  operationQueue.clear();
}



