//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __p44utils__serialqueue__
#define __p44utils__serialqueue__

#include "p44_common.hpp"

#include <time.h>

#include <list>

#include "operationqueue.hpp"

#include "serialcomm.hpp"

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
  class SerialOperationSendAndReceive;

  /// SerialOperation completion callback
  typedef boost::function<void (SerialOperation &aSerialOperation, OperationQueue *aQueueP, ErrorPtr aError)> SerialOperationFinalizeCB;

  /// SerialOperation transmitter
  typedef boost::function<size_t (size_t aNumBytes, const uint8_t *aBytes)> SerialOperationTransmitter;


  /// Serial operation
  typedef boost::intrusive_ptr<SerialOperation> SerialOperationPtr;
  class SerialOperation : public Operation
  {
    friend class SerialOperationSendAndReceive;
  protected:
    SerialOperationTransmitter transmitter;
    SerialOperationFinalizeCB callback;
  public:
    /// constructor
    SerialOperation(SerialOperationFinalizeCB aCallback);

    /// set transmitter
    void setTransmitter(SerialOperationTransmitter aTransmitter);

    /// call to deliver received bytes
    /// @param aNumBytes number of bytes ready for accepting
    /// @param aBytes pointer to bytes buffer
    /// @return number of bytes operation could accept, 0 if none
    virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);

    virtual OperationPtr finalize(OperationQueue *aQueueP = NULL);
    virtual void abortOperation(ErrorPtr aError);
  };


  /// Send operation
  class SerialOperationSend : public SerialOperation
  {
    typedef SerialOperation inherited;

    size_t dataSize;
    size_t appendIndex;
    uint8_t *dataP;
  public:

    SerialOperationSend(size_t aNumBytes, uint8_t *aBytes, SerialOperationFinalizeCB aCallback);
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

    SerialOperationReceive(size_t aExpectedBytes, SerialOperationFinalizeCB aCallback);
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

    SerialOperationSendAndReceive(size_t aNumBytes, uint8_t *aBytes, size_t aExpectedBytes, SerialOperationFinalizeCB aCallback);

    virtual OperationPtr finalize(OperationQueue *aQueueP = NULL);
  };


  /// SerialOperation receiver
  typedef boost::function<size_t (size_t aMaxBytes, uint8_t *aBytes)> SerialOperationReceiver;

  /// Serial operation queue
  class SerialOperationQueue : public OperationQueue
  {
    typedef OperationQueue inherited;

    SerialOperationTransmitter transmitter;
    SerialOperationReceiver receiver;

	public:

    /// the serial communication channel
    SerialCommPtr serialComm;

    /// create operation queue linked into specified Synchronous IO mainloop
    SerialOperationQueue(SyncIOMainLoop &aMainLoop);

    /// destructor
    virtual ~SerialOperationQueue();

    /// set transmitter to be used for all operations
    void setTransmitter(SerialOperationTransmitter aTransmitter);

    /// set receiver
    void setReceiver(SerialOperationReceiver aReceiver);

    /// queue a new operation
    /// @param aOperation the serial IO operation to queue
    void queueSerialOperation(SerialOperationPtr aOperation);

  private:
    /// base class implementation: deliver bytes to the most recent waiting operation
    virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);

    /// FdComm handler
    void receiveHandler(FdComm *aFdCommP, ErrorPtr aError);

    /// standard transmitter
    size_t standardTransmitter(size_t aNumBytes, const uint8_t *aBytes);

    /// standard receiver
    size_t standardReceiver(size_t aMaxBytes, uint8_t *aBytes);

  };


} // namespace p44


#endif /* defined(__p44utils__serialqueue__) */
