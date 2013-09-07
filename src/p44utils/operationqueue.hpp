//
//  operationqueue.hpp
//  p44utils
//
//  Created by Lukas Zeller on 02.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44utils__operationqueue__
#define __p44utils__operationqueue__

#include "p44_common.hpp"


using namespace std;

namespace p44 {


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
    virtual const char *getErrorDomain() const { return OQError::domain(); };
    OQError(OQErrors aError) : Error(ErrorCode(aError)) {};
    OQError(OQErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };


  class Operation;
  class OperationQueue;

  /// Operation completion callback
  typedef boost::function<void (Operation *, OperationQueue *, ErrorPtr)> OperationFinalizeCB;

  /// Operation
  typedef boost::intrusive_ptr<Operation> OperationPtr;
  class Operation : public P44Obj
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
  class OperationQueue : public P44Obj
  {
    MainLoop &mainLoop;
  protected:
    typedef list<OperationPtr> OperationList;
    OperationList operationQueue;
  public:
    /// create operation queue linked into specified mainloop
    OperationQueue(MainLoop &aMainLoop);
    /// destructor
    virtual ~OperationQueue();

    /// queue a new operation
    /// @param aOperation the operation to queue
    void queueOperation(OperationPtr aOperation);

    /// process immediately pending operations now
    void processOperations();

    /// abort all pending operations
    void abortOperations();
    
  private:
    /// handler which is registered with mainloop
    /// @return true if operations processed for now, i.e. no need to call again immediately
    ///   false if processOperations() should be called ASAP again (in the same mainloop cycle if possible)
    bool idleHandler();
  };

} // namespace p44


#endif /* defined(__p44utils__operationqueue__) */
