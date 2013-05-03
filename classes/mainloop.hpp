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

class MainLoop;

// Mainloop timing unit
typedef long long MLMicroSeconds;

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
		MLMicroSeconds executionTime;
		OneTimeCB callback;
	} OnetimeHandler;
	typedef std::list<OnetimeHandler> OnetimeHandlerList;
	OnetimeHandlerList onetimeHandlers;
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
	void executeOnceAt(OneTimeCB aCallback, MLMicroSeconds aExecutionTime, void *aSubmitterP = NULL);
	/// have handler called from the mainloop once with an optional delay from now
	/// @param aCallback the functor to be called
	/// @param aDelay delay from now when to execute (approximately)
	void executeOnce(OneTimeCB aCallback, MLMicroSeconds aDelay = 0, void *aSubmitterP = NULL);
  /// cancel pending execution requests from submitter (NULL = cancel all)
  void cancelExecutionsFrom(void *aSubmitterP);
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
typedef boost::function<bool (SyncIOMainLoop *aMainLoop, MLMicroSeconds aCycleStartTime, int aFD)> SyncIOCB;

typedef boost::shared_ptr<SyncIOMainLoop> SyncIOMainLoopPtr;
/// A main loop with a Synchronous I/O multiplexer (select() call)
class SyncIOMainLoop : public MainLoop
{
  typedef MainLoop inherited;
	typedef struct {
		int monitoredFD;
		SyncIOCB readReadyCB;
		SyncIOCB writeReadyCB;
		SyncIOCB errorCB;
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
	/// register routine with mainloop for being called at least once per loop cycle
	/// @param aSubscriberP usually "this" of the caller, or another unique memory address which allows unregistering later
	/// @param aFD the file descriptor
	/// @param aReadCB the functor to be called when the file descriptor is ready for reading
	/// @param aWriteCB the functor to be called when the file descriptor is ready for writing
	/// @param aErrorCB the functor to be called when the file descriptor has an error
	void registerSyncIOHandlers(int aFD, SyncIOCB aReadCB, SyncIOCB aWriteCB, SyncIOCB aErrorCB);
	/// unregister all handlers registered by a given subscriber
	/// @param aSubscriberP a value identifying the subscriber
	/// @param aFD the file descriptor
	void unregisterSyncIOHandlers(int aFD);
	/// run the mainloop
	/// @return returns a exit code
	virtual int run();
protected:
	/// handle IO
  /// @return true if I/O handling occurred
	bool handleSyncIO(MLMicroSeconds aTimeout);
};



#endif /* defined(__p44bridged__mainloop__) */
