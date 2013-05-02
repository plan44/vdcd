//
//  serialqueue.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 12.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "serialqueue.hpp"


#define DEFAULT_RECEIVE_TIMEOUT 3000 // 3 seconds


#pragma mark - SerialOperation


SerialOperation::SerialOperation()
{
}

// set transmitter
void SerialOperation::setTransmitter(SerialOperationTransmitter aTransmitter)
{
  transmitter = aTransmitter;
}

// set callback to execute when operation completes
void SerialOperation::setSerialOperationCB(SerialOperationFinalizeCB aCallBack)
{
  finalizeCallback = aCallBack;
}


// call to deliver received bytes
size_t SerialOperation::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
{
  return 0;
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


SerialOperationSend::~SerialOperationSend()
{
  if (dataP) {
    free(dataP);
  }
}


bool SerialOperationSend::initiate()
{
  if (!canInitiate()) return false;
  size_t res;
  if (dataP && transmitter) {
    // transmit
    res = transmitter(dataSize,dataP);
    if (res!=dataSize) {
      // error
      abortOperation(ErrorPtr(new SQError(SQErrorTransmit)));
    }
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
  setTimeout(DEFAULT_RECEIVE_TIMEOUT);
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


void SerialOperationReceive::abortOperation(ErrorPtr aError)
{
  expectedBytes = 0; // don't expect any more
  inherited::abortOperation(aError);
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
    finalizeCallback = NULL; // prevent it to be called from this object!
    return op;
  }
  return SerialOperationPtr(); // none
}


#pragma mark - SerialOperationQueue


// Link into mainloop
SerialOperationQueue::SerialOperationQueue(SyncIOMainLoop *aMainLoopP) :
  inherited(aMainLoopP),
  fileDescriptor(-1)
{
}


// set transmitter
void SerialOperationQueue::setTransmitter(SerialOperationTransmitter aTransmitter)
{
  transmitter = aTransmitter;
}


// set reader
void SerialOperationQueue::setReader(SerialOperationReader aReader)
{
  reader = aReader;
}


// set filedescriptor to be monitored by SyncIO mainloop
void SerialOperationQueue::setFDtoMonitor(int aFileDescriptor)
{
  if (aFileDescriptor!=fdToMonitor) {
    // unregister previous one, if any
    if (fdToMonitor>=0) {
      SyncIOMainLoop::currentMainLoop()->unregisterSyncIOHandlers(fdToMonitor);
    }
    // unregister new one, if any
    if (aFileDescriptor>=0) {
      // register
      SyncIOMainLoop::currentMainLoop()->registerSyncIOHandlers(
        aFileDescriptor,
        boost::bind(&SerialOperationQueue::readyForRead, this, _1, _2, _3),
        NULL, // TODO: implement // boost::bind(&SerialOperationQueue::readyForWrite, this, _1, _2, _3),
        NULL // TODO: implement // boost::bind(&SerialOperationQueue::errorOccurred, this, _1, _2, _3)
      );
    }
    // save new FD
    fdToMonitor = aFileDescriptor;
  }
}


#define RECBUFFER_SIZE 100

bool SerialOperationQueue::readyForRead(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD)
{
  if (reader) {
    uint8_t buffer[RECBUFFER_SIZE];
    size_t numBytes = reader(RECBUFFER_SIZE, buffer);
    if (numBytes>0) {
      acceptBytes(numBytes, buffer);
    }
  }
  return true;
}


//bool SerialOperationQueue::readyForWrite(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD)
//{
//
//  return true;
//}
//
//
//bool SerialOperationQueue::errorOccurred(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD)
//{
//
//  return true;
//}




// queue a new serial I/O operation
void SerialOperationQueue::queueSerialOperation(SerialOperationPtr aOperation)
{
  aOperation->setTransmitter(transmitter);
  inherited::queueOperation(aOperation);
}


// deliver bytes to the most recent waiting operation
size_t SerialOperationQueue::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
{
  // first check if some operations still need processing
  processOperations();
  // let operations receive bytes
  size_t acceptedBytes = 0;
  for (OperationList::iterator pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
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



