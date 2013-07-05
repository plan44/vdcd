//
//  mainloop.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 01.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__mainloop__
#define __p44bridged__mainloop__

#include "p44bridged_common.hpp"

#include <sys/poll.h>

using namespace std;

namespace p44 {

  class MainLoop;

  // Mainloop timing unit
  typedef long long MLMicroSeconds;
  const MLMicroSeconds Never = 0;
  const MLMicroSeconds Infinite = -1;
  const MLMicroSeconds MicroSecond = 1;
  const MLMicroSeconds MilliSecond = 1000;
  const MLMicroSeconds Second = 1000*MilliSecond;
  const MLMicroSeconds Minute = 60*Second;

  /// Mainloop callback
  typedef boost::function<bool (MainLoop *aMainLoop, MLMicroSeconds aCycleStartTime)> IdleCB;
  typedef boost::function<void (MainLoop *aMainLoop, MLMicroSeconds aCycleStartTime)> OneTimeCB;


  /// A main loop for a thread
  class MainLoop
  {
    typedef struct {
      void *subscriberP;
      IdleCB callback;
    } IdleHandler;
    typedef std::list<IdleHandler> IdleHandlerList;
    IdleHandlerList idleHandlers;
    typedef struct {
      void *submitterP;
      long ticketNo;
      MLMicroSeconds executionTime;
      OneTimeCB callback;
    } OnetimeHandler;
    typedef std::list<OnetimeHandler> OnetimeHandlerList;
    OnetimeHandlerList onetimeHandlers;
    long ticketNo;
  protected:
    bool terminated;
    MLMicroSeconds loopCycleTime;
    MLMicroSeconds cycleStartTime;
    // protected constructor
    MainLoop();
  public:

    /// returns the current thread's mainloop
    static MainLoop *currentMainLoop();

    /// returns the current microsecond
    static MLMicroSeconds now();

    /// set the cycle time
    void setLoopCycleTime(MLMicroSeconds aCycleTime);

    /// get time left for current cycle
    MLMicroSeconds remainingCycleTime();

    /// register routine with mainloop for being called at least once per loop cycle
    /// @param aSubscriberP usually "this" of the caller, or another unique memory address which allows unregistering later
    /// @param aCallback the functor to be called
    void registerIdleHandler(void *aSubscriberP, IdleCB aCallback);

    /// unregister all handlers registered by a given subscriber
    /// @param aSubscriberP a value identifying the subscriber
    void unregisterIdleHandlers(void *aSubscriberP);

    /// have handler called from the mainloop once with an optional delay from now
    /// @param aCallback the functor to be called
    /// @param aExecutionTime when to execute (approximately), in now() timescale
    /// @param aSubmitterP optionally, an identifying value which allows to cancel the pending execution requests
    /// @return ticket number which can be used to cancel this specific execution request
    long executeOnceAt(OneTimeCB aCallback, MLMicroSeconds aExecutionTime, void *aSubmitterP = NULL);

    /// have handler called from the mainloop once with an optional delay from now
    /// @param aCallback the functor to be called
    /// @param aDelay delay from now when to execute (approximately)
    /// @return ticket number which can be used to cancel this specific execution request
    long executeOnce(OneTimeCB aCallback, MLMicroSeconds aDelay = 0, void *aSubmitterP = NULL);

    /// cancel pending execution requests from submitter (NULL = cancel all)
    void cancelExecutionsFrom(void *aSubmitterP);

    /// cancel pending execution by ticket number
    void cancelExecutionTicket(long aTicketNo);

    /// terminate the mainloop
    void terminate();
    /// run the mainloop
    /// @return returns a exit code
    virtual int run();
  protected:
    // run all handlers
    void runOnetimeHandlers();
    bool runIdleHandlers();
  };


  class SyncIOMainLoop;

  /// I/O callback
  typedef boost::function<bool (SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD, int aPollFlags)> SyncIOCB;

  typedef boost::shared_ptr<SyncIOMainLoop> SyncIOMainLoopPtr;
  /// A main loop with a Synchronous I/O multiplexer (select() call)
  class SyncIOMainLoop : public MainLoop
  {
    typedef MainLoop inherited;
    typedef struct {
      int monitoredFD;
      int pollFlags;
      SyncIOCB pollHandler;
    } SyncIOHandler;
    typedef std::map<int, SyncIOHandler> SyncIOHandlerMap;
    SyncIOHandlerMap syncIOHandlers;
    // private constructor
    SyncIOMainLoop();
  public:

    /// returns the current thread's SyncIOMainLoop
    /// @return the current SyncIOMainLoop. Returns NULL if current mainloop for this thread is not a SyncIOMainLoop
    /// @note creates a SyncIOMainLoop for the thread if no other type of mainloop already exists
    static SyncIOMainLoop *currentMainLoop();

    /// register handler to be called for activity on specified file descriptor
    /// @param aFD the file descriptor to poll
    /// @param aPollFlags POLLxxx flags to specify events we want a callback for
    /// @param aFdEventCB the functor to be called when poll() reports an event for one of the flags set in aPollFlags
    void registerPollHandler(int aFD, int aPollFlags, SyncIOCB aPollEventHandler);

    /// change the poll flags for an already registered handler
    /// @param aFD the file descriptor
    /// @param aPollFlags POLLxxx flags to specify events we want a callback for
    void changePollFlags(int aFD, int aSetPollFlags, int aClearPollFlags=-1);


    /// unregister all handlers registered by a given subscriber
    /// @param aSubscriberP a value identifying the subscriber
    /// @param aFD the file descriptor
    void unregisterPollHandler(int aFD);

    /// run the mainloop
    /// @return returns a exit code
    virtual int run();
  protected:
    /// handle IO
    /// @return true if I/O handling occurred
    bool handleSyncIO(MLMicroSeconds aTimeout);

  private:

    void syncIOHandlerForFd(int aFD, SyncIOHandler &h);

  };

} // namespace p44

#endif /* defined(__p44bridged__mainloop__) */
