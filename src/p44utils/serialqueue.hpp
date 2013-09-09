//
//  serialqueue.h
//  p44utils
//
//  Created by Lukas Zeller on 12.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44utils__serialqueue__
#define __p44utils__serialqueue__

#include "p44_common.hpp"

#include <time.h>

#include <list>

#include "operationqueue.hpp"

using namespace std;

namespace p44 {


  // Errors
  typedef enum {
    SQErrorTransmit,
  } SQErrors;

  class SQError : public Error
  {
  public:
    static const char *domain() { return "SerialQueue"; };
    virtual const char *getErrorDomain() const { return SQError::domain(); };
    SQError(SQErrors aError) : Error(ErrorCode(aError)) {};
    SQError(SQErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };


  class SerialOperation;
  class SerialOperationQueue;

  /// SerialOperation completion callback
  typedef boost::function<void (SerialOperation &aSerialOperation, SerialOperationQueue *aQueueP, ErrorPtr aError)> SerialOperationFinalizeCB;

  /// SerialOperation transmitter
  typedef boost::function<size_t (size_t aNumBytes, const uint8_t *aBytes)> SerialOperationTransmitter;


  /// Serial operation
  typedef boost::intrusive_ptr<SerialOperation> SerialOperationPtr;
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
    size_t appendIndex;
    uint8_t *dataP;
  public:

    SerialOperationSend(size_t aNumBytes, uint8_t *aBytes);
    virtual ~SerialOperationSend();

    void setDataSize(size_t aDataSize);
    void appendData(size_t aNumBytes, uint8_t *aBytes);
    void clearData();


    virtual bool initiate();
  };
  typedef boost::intrusive_ptr<SerialOperationSend> SerialOperationSendPtr;


  /// receive operation
  class SerialOperationReceive : public SerialOperation
  {
    typedef SerialOperation inherited;

    size_t expectedBytes;
    uint8_t *dataP;
    size_t dataIndex;

  public:

    SerialOperationReceive(size_t aExpectedBytes);
    virtual ~SerialOperationReceive();
    uint8_t *getDataP() { return dataP; };
    size_t getDataSize() { return dataIndex; };
    void clearData();

    virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);
    virtual bool hasCompleted();
    virtual void abortOperation(ErrorPtr aError);
  };
  typedef boost::intrusive_ptr<SerialOperationReceive> SerialOperationReceivePtr;


  /// send operation which automatically inserts a receive operation after completion
  class SerialOperationSendAndReceive : public SerialOperationSend
  {
    typedef SerialOperationSend inherited;

    size_t expectedBytes;

  public:

    SerialOperationSendAndReceive(size_t aNumBytes, uint8_t *aBytes, size_t aExpectedBytes);

    virtual OperationPtr finalize(OperationQueue *aQueueP = NULL);
  };


  /// SerialOperation receiver
  typedef boost::function<size_t (size_t aMaxBytes, uint8_t *aBytes)> SerialOperationReceiver;

  /// Serial operation queue
  class SerialOperationQueue : public OperationQueue
  {
    typedef OperationQueue inherited;

    int fdToMonitor;
    SerialOperationTransmitter transmitter;
    SerialOperationReceiver receiver;
  
	public:
    /// create operation queue linked into specified Synchronous IO mainloop
    SerialOperationQueue(SyncIOMainLoop &aMainLoop);
    /// destructor
    virtual ~SerialOperationQueue();

    /// set transmitter to be used for all operations
    void setTransmitter(SerialOperationTransmitter aTransmitter);
    /// set receiver
    void setReceiver(SerialOperationReceiver aReceiver);

    /// set filedescriptor to be monitored by SyncIO mainloop
    /// @param aFileDescriptor open file descriptor for file/socket, <0 to remove monitoring
    void setFDtoMonitor(int aFileDescriptor = -1);

    /// queue a new operation
    /// @param aOperation the serial IO operation to queue
    void queueSerialOperation(SerialOperationPtr aOperation);

  private:
    /// base class implementation: deliver bytes to the most recent waiting operation
    virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);

    /// SyncIOMainloop handlers
    bool pollHandler(SyncIOMainLoop &aMainLoop, MLMicroSeconds aCycleStartTime, int aFD, int aPollFlags);


  };


} // namespace p44


#endif /* defined(__p44utils__serialqueue__) */
