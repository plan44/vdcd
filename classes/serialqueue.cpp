//
//  serialqueue.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 12.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "serialqueue.hpp"



#pragma mark - SerialOperation

SerialOperation::SerialOperation() :
  initiated(false),
  inSequence(true) // by default, execute in sequence
{
}

/// set transmitter
void SerialOperation::setTransmitter(SerialOperationTransmitter aTransmitter)
{
  transmitter = aTransmitter;
}

/// set callback to execute when operation completes
void SerialOperation::setSerialOperationCB(SerialOperationFinalizeCB aCallBack)
{
  finalizeCallback = aCallBack;
}

/// call to initiate operation
/// @return false if cannot be initiated now and must be retried
bool SerialOperation::initiate()
{
  initiated=true;
  return initiated;
}

/// check if already initiated
bool SerialOperation::isInitiated()
{
  return initiated;
}

/// call to deliver received bytes
/// @return number of bytes operation could accept, 0 if none
size_t SerialOperation::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
{
  return 0;
}


/// call to check if operation has completed
/// @return true if completed
bool SerialOperation::hasCompleted()
{
  return true;
}


/// call to execute after completion
SerialOperationPtr SerialOperation::finalize(SerialOperationQueue *aQueueP)
{
  if (finalizeCallback) {
    finalizeCallback(this,aQueueP);
  }
  return SerialOperationPtr();
}


#pragma mark - SerialOperationSend


SerialOperationSend::SerialOperationSend(size_t aNumBytes, uint8_t *aBytes)
{
  // copy data
  dataP = NULL;
  dataSize = aNumBytes;
  if (dataSize>0) {
    dataP = (uint8_t *)malloc(dataSize);
    memcpy(dataP, aBytes, dataSize);
  }
}

SerialOperationSend::~SerialOperationSend() {
  if (dataP) {
    free(dataP);
  }
}

bool SerialOperationSend::initiate() {
  size_t res;
  if (dataP && transmitter) {
    // transmit
    res = transmitter(dataSize,dataP);
    // show
    #ifdef DEBUG
    std::string s;
    for (size_t i=0; i<dataSize; i++) {
      string_format_append(s, "%02X ",dataP[i]);
    }
    DBGLOG(LOG_DEBUG,"Transmitted bytes: %s\n", s.c_str());
    #endif
    // early release
    free(dataP);
    dataP = NULL;
  }
  // executed
  return inherited::initiate();
}



#pragma mark - SerialOperationReceive


SerialOperationReceive::SerialOperationReceive(size_t aExpectedBytes)
{
  // allocate buffer
  expectedBytes = aExpectedBytes;
  dataP = (uint8_t *)malloc(expectedBytes);
  dataIndex = 0;
};


size_t SerialOperationReceive::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
{
  // append bytes into buffer
  if (!initiated)
    return 0; // cannot accept bytes when not yet initiated
  if (aNumBytes>expectedBytes)
    aNumBytes = expectedBytes;
  if (aNumBytes>0) {
    memcpy(dataP+dataIndex, aBytes, aNumBytes);
    dataIndex += aNumBytes;
    expectedBytes -= aNumBytes;
  }
  // return number of bytes actually accepted
  return aNumBytes;
}


bool SerialOperationReceive::hasCompleted()
{
  // completed if all expected bytes received
  return expectedBytes<=0;
}


#pragma mark - SerialOperationSendAndReceive


SerialOperationSendAndReceive::SerialOperationSendAndReceive(size_t aNumBytes, uint8_t *aBytes, size_t aExpectedBytes) :
  inherited(aNumBytes, aBytes),
  expectedBytes(aExpectedBytes)
{
};


SerialOperationPtr SerialOperationSendAndReceive::finalize(SerialOperationQueue *aQueueP)
{
  if (aQueueP) {
    // insert receive operation
    SerialOperationPtr op(new SerialOperationReceive(expectedBytes));
    op->setSerialOperationCB(finalizeCallback); // inherit completion callback
    return op;
  }
  return SerialOperationPtr(); // none
}


#pragma mark - SerialOperationQueue



/// set transmitter
void SerialOperationQueue::setTransmitter(SerialOperationTransmitter aTransmitter)
{
  transmitter = aTransmitter;
}


/// queue a new operation
/// @param aOperation the operation to queue
void SerialOperationQueue::queueOperation(SerialOperationPtr aOperation)
{
  aOperation->setTransmitter(transmitter);
  operationQueue.push_back(aOperation);
}


/// deliver bytes to the most recent waiting operation
size_t SerialOperationQueue::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
{
  // first check if some operations still need processing
  processOperations();
  // let operations receive bytes
  size_t acceptedBytes = 0;
  for (operationQueue_t::iterator pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
    size_t consumed = (*pos)->acceptBytes(aNumBytes, aBytes);
    aBytes += consumed; // advance pointer
    aNumBytes -= consumed; // count
    acceptedBytes += consumed;
    if (aNumBytes<=0)
      break; // all bytes consumed
  }
  if (aNumBytes>0) {
    // Still bytes left to accept
    // TODO: possibly create "unexpected receive" operation
  }
  // check if some operations might be complete now
  processOperations();
  // return number of accepted bytes
  return acceptedBytes;
};


/// process operations now
void SerialOperationQueue::processOperations()
{
  bool processed = false;
  while (!processed) {
    operationQueue_t::iterator pos;
    // (re)start with first element in queue
    for (pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
      SerialOperationPtr op = *pos;
      if (!op->isInitiated()) {
        // initiate now
        if (!op->initiate()) {
          // cannot initiate this one now, check if we can continue with others
          if (op->inSequence) {
            // this op needs to be initiated before others can be checked
            processed = true; // something must happen outside this routine to change the state of the op, so done for now
            break;
          }
        }
      }
      if (op->isInitiated()) {
        // initiated, check if already completed
        if (op->hasCompleted()) {
          // operation has completed
          // - remove from list
          operationQueue_t::iterator nextPos = operationQueue.erase(pos);
          // - finalize. This might push new operations in front or back of the queue
          SerialOperationPtr nextOp = op->finalize(this);
          if (nextOp) {
            operationQueue.insert(nextPos, nextOp);
          }
          // restart with start of (modified) queue
          break;
        }
        else {
          // operation has not yet completed
          if (op->inSequence) {
            // this op needs to be complete before others can be checked
            processed = true; // something must happen outside this routine to change the state of the op, so done for now
            break;
          }
        }
      }
    } // for all ops in queue
    if (pos==operationQueue.end()) processed = true; // if seen all, we're done for now as well
  } // while not processed
};









