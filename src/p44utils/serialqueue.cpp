//
//  serialqueue.cpp
//
//  Created by Lukas Zeller on 12.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "serialqueue.hpp"

using namespace p44;


#define DEFAULT_RECEIVE_TIMEOUT 3000000 // [uS] = 3 seconds


#pragma mark - SerialOperation


SerialOperation::SerialOperation()
{
}

// set transmitter
void SerialOperation::setTransmitter(SerialOperationTransmitter aTransmitter)
{
  transmitter = aTransmitter;
}


// call to deliver received bytes
size_t SerialOperation::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
{
  return 0;
}



#pragma mark - SerialOperationSend


SerialOperationSend::SerialOperationSend(size_t aNumBytes, uint8_t *aBytes) :
  dataP(NULL)
{
  // copy data
  setDataSize(aNumBytes);
  appendData(aNumBytes, aBytes);
}


SerialOperationSend::~SerialOperationSend()
{
  clearData();
}




void SerialOperationSend::clearData()
{
  if (dataP) {
    delete [] dataP;
    dataP = NULL;
  }
  dataSize = 0;
  appendIndex = 0;
}


void SerialOperationSend::setDataSize(size_t aDataSize)
{
  clearData();
  if (aDataSize>0) {
    dataSize = aDataSize;
    dataP = new uint8_t[dataSize];
  }
}


void SerialOperationSend::appendData(size_t aNumBytes, uint8_t *aBytes)
{
  if (appendIndex+aNumBytes>dataSize)
    aNumBytes = dataSize-appendIndex;
  if (aNumBytes>0) {
    memcpy(dataP, aBytes, aNumBytes);
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
    clearData();
  }
  // executed
  return inherited::initiate();
}



#pragma mark - SerialOperationReceive


SerialOperationReceive::SerialOperationReceive(size_t aExpectedBytes)
{
  // allocate buffer
  expectedBytes = aExpectedBytes;
  dataP = new uint8_t[expectedBytes];
  dataIndex = 0;
  setTimeout(DEFAULT_RECEIVE_TIMEOUT);
};


SerialOperationReceive::~SerialOperationReceive()
{
  clearData();
}



void SerialOperationReceive::clearData()
{
  if (dataP) {
    delete [] dataP;
    dataP = NULL;
  }
  expectedBytes = 0;
  dataIndex = 0;
}


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
  clearData(); // don't expect any more, early release
  inherited::abortOperation(aError);
}


#pragma mark - SerialOperationSendAndReceive


SerialOperationSendAndReceive::SerialOperationSendAndReceive(size_t aNumBytes, uint8_t *aBytes, size_t aExpectedBytes) :
  inherited(aNumBytes, aBytes),
  expectedBytes(aExpectedBytes)
{
};


OperationPtr SerialOperationSendAndReceive::finalize(OperationQueue *aQueueP)
{
  if (aQueueP) {
    // insert receive operation
    SerialOperationPtr op(new SerialOperationReceive(expectedBytes));
    op->setOperationCB(finalizeCallback); // inherit completion callback
    finalizeCallback = NULL; // prevent it to be called from this object!
    return op;
  }
  return SerialOperationPtr(); // none
}


#pragma mark - SerialOperationQueue


// Link into mainloop
SerialOperationQueue::SerialOperationQueue(SyncIOMainLoop *aMainLoopP) :
  inherited(aMainLoopP),
  fdToMonitor(-1)
{
}


SerialOperationQueue::~SerialOperationQueue()
{
	setFDtoMonitor(-1); // causes unregistering from mainloop
}


// set transmitter
void SerialOperationQueue::setTransmitter(SerialOperationTransmitter aTransmitter)
{
  transmitter = aTransmitter;
}


// set receiver
void SerialOperationQueue::setReceiver(SerialOperationReceiver aReceiver)
{
  receiver = aReceiver;
}


// set filedescriptor to be monitored by SyncIO mainloop
void SerialOperationQueue::setFDtoMonitor(int aFileDescriptor)
{
  if (aFileDescriptor!=fdToMonitor) {
    // unregister previous one, if any
    if (fdToMonitor>=0) {
      SyncIOMainLoop::currentMainLoop()->unregisterPollHandler(fdToMonitor);
    }
    // unregister new one, if any
    if (aFileDescriptor>=0) {
      // register
      SyncIOMainLoop::currentMainLoop()->registerPollHandler(
        aFileDescriptor,
        POLLIN,
        boost::bind(&SerialOperationQueue::pollHandler, this, _1, _2, _3, _4)
      );
    }
    // save new FD
    fdToMonitor = aFileDescriptor;
  }
}


#define RECBUFFER_SIZE 100

bool SerialOperationQueue::pollHandler(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD, int aPollFlags)
{
  if (receiver) {
    uint8_t buffer[RECBUFFER_SIZE];
    size_t numBytes = receiver(RECBUFFER_SIZE, buffer);
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
		SerialOperationPtr sop = boost::dynamic_pointer_cast<SerialOperation>(*pos);
    size_t consumed = 0;
		if (sop)
			consumed = sop->acceptBytes(aNumBytes, aBytes);
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



