//
//  operationqueue.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 02.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__operationqueue__
#define __p44bridged__operationqueue__

#include "p44bridged_common.hpp"


using namespace std;


// Errors
typedef enum {
  OQErrorOK,
  OQErrorAborted,
  OQErrorTimedOut,
} OQErrors;

class OQError : public Error
{
public:
  static const char *domain() { return "OperationQueue"; };
  OQError(OQErrors aError) : Error(ErrorCode(aError)) {};
  OQError(OQErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  virtual const char *getErrorDomain() const { return OQError::domain(); };
};


class Operation;
class OperationQueue;

/// Operation completion callback
typedef boost::function<void (Operation *, OperationQueue *, ErrorPtr)> OperationFinalizeCB;

/// Operation
typedef boost::shared_ptr<Operation> OperationPtr;
class Operation
{
protected:
  OperationFinalizeCB finalizeCallback;
  bool initiated;
  bool aborted;
  MLMicroSeconds timeout; // timeout
  MLMicroSeconds timesOutAt; // absolute time for timeout
  MLMicroSeconds initiationDelay; // how much to delay initiation (after first attempt to initiate)
  MLMicroSeconds initiatesNotBefore; // absolute time for earliest initiation
public:
  /// if this flag is set, no operation queued after this operation will execute
  bool inSequence;
  /// constructor
  Operation();
  /// set callback to execute when operation completes
  void setOperationCB(OperationFinalizeCB aCallBack);
  /// set delay for initiation (after first attempt to initiate)
  void setInitiationDelay(MLMicroSeconds aInitiationDelay);
  /// set earliest time to execute
  void setInitiatesAt(MLMicroSeconds aInitiatesAt);
  /// set timeout (from initiation)
  void setTimeout(MLMicroSeconds aTimeout);
  /// check if can be initiated
  /// @return false if cannot be initiated now and must be retried
  virtual bool canInitiate();
  /// call to initiate operation
  /// @return false if cannot be initiated now and must be retried
  /// @note internally calls canInitiate() first
  virtual bool initiate();
  /// check if already initiated
  bool isInitiated();
  /// call to deliver received bytes
  /// @param aNumBytes number of bytes ready for accepting
  /// @param aBytes pointer to bytes buffer
  /// @return number of bytes operation could accept, 0 if none
  virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);
  /// call to check if operation has timed out
  bool hasTimedOutAt(MLMicroSeconds aRefTime = MainLoop::now());
  /// call to check if operation has completed
  /// @return true if completed
  virtual bool hasCompleted();
  /// call to execute after completion, can chain another operation by returning it
  virtual OperationPtr finalize(OperationQueue *aQueueP);
  /// abort operation
  virtual void abortOperation(ErrorPtr aError);
};



/// Operation queue
class OperationQueue
{
  MainLoop *mainLoopP;
protected:
  typedef list<OperationPtr> OperationList;
  OperationList operationQueue;
public:
  /// create operation queue linked into specified mainloop
  OperationQueue(MainLoop *aMainLoopP);
  /// destructor
  ~OperationQueue();

  /// queue a new operation
  /// @param aOperation the operation to queue
  void queueOperation(OperationPtr aOperation);

  /// process operations now
  /// @return true if operations processed for now, i.e. no need to call again immediately
  ///   false if processOperations() should be called ASAP again (in the same mainloop cycle if possible)
  bool processOperations();

  /// abort all pending operations
  void abortOperations();
};



#endif /* defined(__p44bridged__operationqueue__) */
