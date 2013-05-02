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

#include "operationqueue.hpp"

using namespace std;


// Errors
typedef enum {
  SQErrorTransmit,
} SQErrors;

class SQError : public Error
{
public:
  static const char *domain() { return "SerialQueue"; };
  SQError(SQErrors aError) : Error(ErrorCode(aError)) {};
  SQError(SQErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  virtual const char *getErrorDomain() const { return SQError::domain(); };
};


class SerialOperation;
class SerialOperationQueue;

/// SerialOperation completion callback
typedef boost::function<void (SerialOperation *, SerialOperationQueue *, ErrorPtr)> SerialOperationFinalizeCB;

/// SerialOperation transmitter
typedef boost::function<size_t (size_t aNumBytes, uint8_t *aBytes)> SerialOperationTransmitter;


/// Serial operation
typedef boost::shared_ptr<SerialOperation> SerialOperationPtr;
class SerialOperation : public Operation
{
protected:
  SerialOperationTransmitter transmitter;
public:
  /// constructor
  SerialOperation();
  /// set transmitter
  void setTransmitter(SerialOperationTransmitter aTransmitter);
  /// call to deliver received bytes
  /// @param aNumBytes number of bytes ready for accepting
  /// @param aBytes pointer to bytes buffer
  /// @return number of bytes operation could accept, 0 if none
  virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);
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


/// SerialOperation reader
typedef boost::function<size_t (size_t aMaxBytes, uint8_t *aBytes)> SerialOperationReader;

/// Serial operation queue
class SerialOperationQueue : public OperationQueue
{
  typedef OperationQueue inherited;

  SerialOperationTransmitter transmitter;
  SerialOperationReader reader;
  int fdToMonitor;
public:
  /// create operation queue linked into specified Synchronous IO mainloop
  SerialOperationQueue(SyncIOMainLoop *aMainLoopP);
  /// destructor
  ~SerialOperationQueue();

  /// set transmitter to be used for all operations
  void setTransmitter(SerialOperationTransmitter aTransmitter);
  /// set reader
  void setReader(SerialOperationReader aReader);

  /// set filedescriptor to be monitored by SyncIO mainloop
  /// @param aFileDescriptor open file descriptor for file/socket, <0 to remove monitoring
  void setFDtoMonitor(int aFileDescriptor = -1);

  /// queue a new operation
  /// @param aOperation the serial IO operation to queue
  void queueSerialOperation(SerialOperationPtr aOperation);

private:
  /// deliver bytes to the most recent waiting operation
  size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);

  /// SyncIOMainloop handlers
  bool readyForRead(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD);
//  bool readyForWrite(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD);
//  bool errorOccurred(SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD);


};




#endif /* defined(__p44bridged__serialqueue__) */
