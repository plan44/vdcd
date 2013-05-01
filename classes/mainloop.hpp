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
typedef long long MLMilliSeconds;

/// Mainloop callback
typedef boost::function<bool (MainLoop *aMainLoop, MLMilliSeconds aTimeRef)> IdleCB;

typedef boost::shared_ptr<MainLoop> MainLoopPtr;
/// A main loop for a thread
class MainLoop
{
	typedef struct {
		void *subscriberP;
		IdleCB callback;
	} IdleHandler;
	std::list<IdleHandler> idleHandlers;
	typedef struct {
		MLMilliSeconds executionTime;
		IdleCB callback;
	} OnetimeHandler;
	std::list<OnetimeHandler> onetimeHandlers;
protected:
	MLMilliSeconds loopCycleTime;
	bool terminated;
	MLMilliSeconds cycleStartTime;
public:
	/// returns the current thread's mainloop
	static MainLoop *currentMainLoop();
	/// set the cycle time
	void setLoopCycleTime(MLMilliSeconds aCycleTime);
	/// register routine with mainloop for being called at least once per loop cycle
	/// @param aSubscriberP usually "this" of the caller, or another unique memory address which allows unregistering later
	/// @param aCallback the functor to be called
	void registerIdleHandler(void *aSubscriberP, IdleCB aCallback);
	/// unregister all handlers registered by a given subscriber
	/// @param aSubscriberP a value identifying the subscriber
	void unregisterIdleHandlers(void *aSubscriberP);
	/// have handler called from the mainloop once with an optional delay from now
	/// @param aCallback the functor to be called
	void execute(IdleCB aCallback, MLMilliSeconds aDelay = 0);
	/// run the mainloop
	void run();
	/// terminate the mainloop
	void terminate();
protected:
	/// run one cycle of the mainloop
	virtual void runOneCylce();
};


class SyncIOMainLoop;

/// I/O callback
typedef boost::function<bool (SyncIOMainLoop *aMainLoop, MLMilliSeconds aTimeRef, int aFD)> SyncIOCB;

typedef boost::shared_ptr<SyncIOMainLoop> SyncIOMainLoopPtr;
/// A main loop with a Synchronous I/O multiplexer (select() call)
class SyncIOMainLoop : public MainLoop
{
	typedef struct {
		void *subscriberP;
		int monitoredFD;
		SyncIOCB readReadyCB;
		SyncIOCB writeReadyCB;
		SyncIOCB errorCB;
	} SyncIOClient;
	std::list<SyncIOClient> syncIOHandlers;
public:
	/// register routine with mainloop for being called at least once per loop cycle
	/// @param aSubscriberP usually "this" of the caller, or another unique memory address which allows unregistering later
	/// @param aFD the file descriptor
	/// @param aReadCB the functor to be called when the file descriptor is ready for reading
	/// @param aWriteCB the functor to be called when the file descriptor is ready for writing
	/// @param aErrorCB the functor to be called when the file descriptor has an error
	void registerSyncIOHandlers(void *aSubscriberP, int aFD, SyncIOCB aReadCB, SyncIOCB aWriteCB, SyncIOCB aErrorCB);
	/// unregister all handlers registered by a given subscriber
	/// @param aSubscriberP a value identifying the subscriber
	/// @param aFD the file descriptor
	void unregisterSyncIOHandlers(void *aSubscriberP, int aFD);
protected:
	/// run one cycle of the mainloop
	virtual void runOneCylce();
};



#endif /* defined(__p44bridged__mainloop__) */
