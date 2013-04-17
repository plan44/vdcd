//
//  serialqueue.h
//  p44bridged
//
//  Created by Lukas Zeller on 12.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__serialqueue__
#define __p44bridged__serialqueue__

#include "p44bridged_common.hpp"

#include <time.h>

#include <list>

using namespace std;


// Errors
typedef enum {
  SQErrorOK,
  SQErrorAborted,
  SQErrorTransmit,
  SQErrorTimedOut,
} SQErrors;

class SQError : public Error
{
public:
  SQError(SQErrors aError) : Error(ErrorCode(aError)) {};
  SQError(SQErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  virtual const char *getErrorDomain() { return "SerialQueue"; }
};


typedef long long SQMilliSeconds;

class SerialOperation;
class SerialOperationQueue;

/// SerialOperation completion callback
typedef boost::function<void (SerialOperation *, SerialOperationQueue *, ErrorPtr)> SerialOperationFinalizeCB;

/// SerialOperation transmitter
typedef boost::function<size_t (size_t aNumBytes, uint8_t *aBytes)> SerialOperationTransmitter;


/// Serial operation
typedef boost::shared_ptr<SerialOperation> SerialOperationPtr;
class SerialOperation
{
protected:
  SerialOperationFinalizeCB finalizeCallback;
  SerialOperationTransmitter transmitter;
  bool initiated;
  bool aborted;
  SQMilliSeconds timeout; // timeout
  SQMilliSeconds timesOutAt; // absolute time for timeout
  SQMilliSeconds initiatesNotBefore;
public:
  /// current time in SQMilliseconds
  static SQMilliSeconds now();
  /// if this flag is set, no operation queued after this operation will execute
  bool inSequence;
  /// constructor
  SerialOperation();
  /// set transmitter
  void setTransmitter(SerialOperationTransmitter aTransmitter);
  /// set callback to execute when operation completes
  void setSerialOperationCB(SerialOperationFinalizeCB aCallBack);
  /// set earliest time to execute
  void setInitiatesAt(SQMilliSeconds aInitiatesAt);
  /// set timeout (from initiation)
  void setTimeout(SQMilliSeconds aTimeout);
  /// call to initiate operation
  /// @return false if cannot be initiated now and must be retried
  virtual bool initiate();
  /// check if already initiated
  bool isInitiated();
  /// call to deliver received bytes
  /// @param aNumBytes number of bytes ready for accepting
  /// @param aBytes pointer to bytes buffer
  /// @return number of bytes operation could accept, 0 if none
  virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);
  /// call to check if operation has timed out
  bool hasTimedOutAt(SQMilliSeconds aRefTime = SerialOperation::now());
  /// call to check if operation has completed
  /// @return true if completed
  virtual bool hasCompleted();
  /// call to execute after completion, can chain another operation by returning it
  virtual SerialOperationPtr finalize(SerialOperationQueue *aQueueP);
  /// abort operation
  virtual void abortOperation(ErrorPtr aError);
};


/// Send operation
class SerialOperationSend : public SerialOperation
{
  typedef SerialOperation inherited;

  size_t dataSize;
  uint8_t *dataP;
public:

  SerialOperationSend(size_t aNumBytes, uint8_t *aBytes);
  virtual ~SerialOperationSend();
  virtual bool initiate();
};
typedef boost::shared_ptr<SerialOperationSend> SerialOperationSendPtr;


/// receive operation
class SerialOperationReceive : public SerialOperation
{
  typedef SerialOperation inherited;

  size_t expectedBytes;
  uint8_t *dataP;
  size_t dataIndex;

public:

  SerialOperationReceive(size_t aExpectedBytes);
  uint8_t *getDataP() { return dataP; };
  size_t getDataSize() { return dataIndex; };

  virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);
  virtual bool hasCompleted();
  virtual void abortOperation(ErrorPtr aError);
};
typedef boost::shared_ptr<SerialOperationReceive> SerialOperationReceivePtr;


/// send operation which automatically inserts a receive operation after completion
class SerialOperationSendAndReceive : public SerialOperationSend
{
  typedef SerialOperationSend inherited;

  size_t expectedBytes;

public:

  SerialOperationSendAndReceive(size_t aNumBytes, uint8_t *aBytes, size_t aExpectedBytes);

  virtual SerialOperationPtr finalize(SerialOperationQueue *aQueueP = NULL);
};



/// Serial operation queue
class SerialOperationQueue
{
  typedef list<SerialOperationPtr> operationQueue_t;
  operationQueue_t operationQueue;
  SerialOperationTransmitter transmitter;

public:

  /// set transmitter
  void setTransmitter(SerialOperationTransmitter aTransmitter);

  /// queue a new operation
  /// @param aOperation the operation to queue
  void queueOperation(SerialOperationPtr aOperation);

  /// deliver bytes to the most recent waiting operation
  size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);

  /// process operations now
  void processOperations();

  /// abort all pending operations
  void abortOperations();
};




#endif /* defined(__p44bridged__serialqueue__) */
