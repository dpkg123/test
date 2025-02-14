/*=============================================================================
	SPUUtils.h: Utility functions for SPU management from the PU side
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include <sys/ppu_thread.h>
#include <sys/spu_thread.h>
#include <sys/event.h>

#include "SPUUtils.h"

#ifndef _PS3_THREADING_H
#define _PS3_THREADING_H

#define SPU_THREADS_USE_DEBUGF 0

typedef __CSTD uint8_t sys_spu_event_port_t;


enum EEventType
{
	ET_PPUUserEvent,
	ET_SPUUserEvent,
	ET_SPUDMAEvent,
};

/**
 * Maps an EThreadPriority to a PU thread priority number.
 *
 * @param Prio The Unreal thread priority
 *
 * @return The PU thread priority where 0 is highest, 4095 is lowest
 */
DWORD GetPUThreadPriority(EThreadPriority Prio);

/**
 * This is a class that encapsulates an event queue. It is not required
 * to run in a thread - see FPPURunnable for the thread-based event queue.
 *
 * Local event queues (within the same process) does not have a key.
 * Global event queues have a pre-determined global key and can be used
 *   for IPC (Inter-Process-Communication).
 *
 * NOTE: SPUs can connect to local event queues on the PPU. I think. --Smedis
 */
class FPPUEventQueue
{
public:

	/** 
	 * Constructor - creates an event queue with the given key
	 * 
	 * @param InEventQueueKey	An optional unqiue key for this event queue.
	 * @param InQueueDepth		The depth of the event queue. Default is one deep (to process the terminate message)
	 */
	FPPUEventQueue(sys_ipc_key_t InEventQueueKey = SYS_EVENT_QUEUE_LOCAL, INT InQueueDepth = 1);

	/**
	 * Destructor - clean up the queue and termination ports
	 * @todo: what if SPUThread ports are still connected to the queue?
	 */
	virtual ~FPPUEventQueue();

	/**
	 * Create an event queue for this thread to listen on 
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	UBOOL CreateQueue(void);

	/**
	 * Destroy the event queue for this thread to listen on (can be called externally as well as from the destructor)
	 */
	void DestroyQueue(void);

	/**
	 * Look for events in the queue, and call OnEvent for any events
	 *
	 * @param bAsynchronous If TRUE, this will process any and all events in the queue and then return, even if there are no events. If FALSE, it will block waiting for a single event, process it, then return
	 */
	void ProcessEventQueue(UBOOL bAsynchronous);

	/** Returns TRUE if the event queue is local to the process. */
	UBOOL IsLocal( );

	/** Returns TRUE if the event queue is global and can be used for IPC (Inter-Process-Communication). */
	UBOOL IsGlobal( );

	/**
	 * Called before the OnEvent message is called. This is needed for the FPPURunnable to break out
	 * of its loop. The return value specifies whether or not to call OnEvent
	 *
	 * @param EventType			The type of event (PPU, SPU, etc)
	 * @param SourceEventPort	First message param
	 * @param Message1			First message param
	 * @param Message2			Second message param
	 * @param Message3			Third message param
	 *
	 * @return TRUE if OnEvent should be called
	 */
	virtual UBOOL PeekEvent(EEventType EventType, QWORD SourceEventPort, QWORD Message1, QWORD Message2, QWORD Message3) { return TRUE; }

	/**
	 * Called when a non-terminate message is received from the queue
	 *
	 * @param EventType			The type of event (PPU, SPU, etc)
	 * @param SourceEventPort	First message param
	 * @param Message1			First message param
	 * @param Message2			Second message param
	 * @param Message3			Third message param
	 */
	virtual void OnEvent(EEventType EventType, QWORD SourceEventPort, QWORD Message1, QWORD Message2, QWORD Message3) = 0;

protected:

	/** The key to assign to the event queue */
	sys_ipc_key_t EventQueueKey;

	/** The max depth of the queue */
	DWORD QueueDepth;

	/** Is the queue created? */
	UBOOL bIsQueueCreated;

	/** The event queue object */
	sys_event_queue_t EventQueue;
};

/**
 * This is a base class for PPU runnables that contain an event queue.
 * All runnables on the PPU probably will want to subclass from this 
 * class. This will allow for:
 *	A) Automated event queue management (simply implement the OnEvent
 *     function)
 *  B) Support for attaching an SPU thread to the event queue
 *  C) Support being stopped by the main thread.
 */
class FPPURunnable : public FRunnable, public FPPUEventQueue
{
public:
	/** 
	 * Constructor - creates a global event queue with the given key, or a local event queue with SYS_EVENT_QUEUE_LOCAL.
	 * 
	 * @param InEventQueueKey	An optional unqiue key for this event queue.
	 * @param InQueueDepth		The depth of the event queue. Default is one deep (to process the terminate message)
	 */
	FPPURunnable(sys_ipc_key_t InEventQueueKey = SYS_EVENT_QUEUE_LOCAL, INT InQueueDepth = 1);

	/**
	 * Create an event queue for this thread to listen on 
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	virtual UBOOL Init(void);

	/**
	 * Wait for events, and call OnEvent when we receive one
	 *
	 * @return The exit code of the runnable object
	 */
	virtual DWORD Run(void);

	/**
	 * This is called if a thread is requested to terminate early
	 */
	virtual void Stop(void);

	/**
	 * Nothing to do, destructor cleans up, because it has to happen after thread terminates (i think)
	 */
	virtual void Exit(void);


	/**
	 * Called before the OnEvent message is called. This is needed for the FPPURunnable to break out
	 * of its loop. The return value specifies whether or not to call OnEvent
	 *
	 * @param EventType			The type of event (PPU, SPU, etc)
	 * @param SourceEventPort	First message param
	 * @param Message1			First message param
	 * @param Message2			Second message param
	 * @param Message3			Third message param
	 *
	 * @return TRUE if OnEvent should be called
	 */
	virtual UBOOL PeekEvent(EEventType EventType, QWORD SourceEventPort, QWORD Message1, QWORD Message2, QWORD Message3);

protected:
	/** An event port to send the terminate message over */
	sys_event_port_t TerminatePort;

	/** TRUE if the PeekEvent function received the terminate message */
	UBOOL bWasStopRequested;
};


/**
 * This is the PS3 class used for all poolable threads
 */
class FQueuedThreadPPU : public FQueuedThread
{
	/** The event that tells the thread there is work to do */
	FEvent* DoWorkEvent;

	/** The thread object */
	sys_ppu_thread_t	Thread;

	/** If true, the thread should exit */
	volatile UBOOL TimeToDie;

	/** The work this thread is doing */
	FQueuedWork* volatile QueuedWork;

	/** The pool this thread belongs to */
	FQueuedThreadPool* OwningThreadPool;

	/**
	 * Helper to manage stat updates
	 */
	STAT(FCheckForStatsUpdate StatsUpdate);

	/**
	 * The thread entry point. Simply forwards the call on to the right 
	 * thread main function 
	 */
	static void _ThreadProc(QWORD Arg);

	/**
	 * The real thread entry point. It waits for work events to be queued. Once
	 * an event is queued, it executes it and goes back to waiting.
	 */
	void Run(void);

public:
	/**
	 * Zeros any members
	 */
	FQueuedThreadPPU(void);

	/**
	 * Deletes any allocated resources. Kills the thread if it is running.
	 */
	virtual ~FQueuedThreadPPU(void);

	/**
	 * Creates the thread with the specified stack size and creates the various
	 * events to be able to communicate with it.
	 *
	 * @param InPool The thread pool interface used to place this thread
	 *		  back into the pool of available threads when its work is done
	 * @param ProcessorMask The processor set to run the thread on
	 * @param InStackSize The size of the stack to create. 0 means use the
	 *		  current thread's stack size
	 * @param ThreadPriority priority of new thread
	 *
	 * @return True if the thread and all of its initialization was successful, false otherwise
	 */
	virtual UBOOL Create(class FQueuedThreadPool* InPool,DWORD ProcessorMask,
		DWORD InStackSize = 0,EThreadPriority ThreadPriority=TPri_Normal);
	
	/**
	 * Tells the thread to exit. If the caller needs to know when the thread
	 * has exited, it should use the bShouldWait value and tell it how long
	 * to wait before deciding that it is deadlocked and needs to be destroyed.
	 * NOTE: having a thread forcibly destroyed can cause leaks in TLS, etc.
	 *
	 * @param bShouldWait If true, the call will wait for the thread to exit
	 * @param MaxWaitTime The amount of time to wait before killing it. It
	 * defaults to inifinite.
	 * @param bShouldDeleteSelf Whether to delete ourselves upon completion
	 *
	 * @return True if the thread exited gracefull, false otherwise
	 */
	virtual UBOOL Kill(UBOOL bShouldWait = FALSE,DWORD MaxWaitTime = INFINITE,
		UBOOL bShouldDeleteSelf = FALSE);

	/**
	 * Tells the thread there is work to be done. Upon completion, the thread
	 * is responsible for adding itself back into the available pool.
	 *
	 * @param InQueuedWork The queued work to perform
	 */
	virtual void DoWork(FQueuedWork* InQueuedWork);
};

/**
 * This class fills in the platform specific features that the parent
 * class doesn't implement. The parent class handles all common, non-
 * platform specific code, while this class provides all of the Windows
 * specific methods. It handles the creation of the threads used in the
 * thread pool.
 */
class FQueuedThreadPoolPS3 : public FQueuedThreadPoolBase
{
public:
	/**
	 * Cleans up any threads that were allocated in the pool
	 */
	virtual ~FQueuedThreadPoolPS3(void);

	/**
	 * Creates the thread pool with the specified number of threads
	 *
	 * @param InNumQueuedThreads Specifies the number of threads to use in the pool
	 * @param ProcessorMask Specifies which processors should be used by the pool
	 * @param StackSize The size of stack the threads in the pool need (32K default)
	 * @param ThreadPriority priority of new pool thread
	 *
	 * @return Whether the pool creation was successful or not
	 */
	virtual UBOOL Create(DWORD InNumQueuedThreads,DWORD ProcessorMask = 0, DWORD StackSize = (32 * 1024),EThreadPriority ThreadPriority=TPri_Normal);

};

/** 
 * This class will generate a PU thread.
 */
class FThreadFactoryPU : public FThreadFactory
{
	/**
	 * Creates the thread with the specified stack size and thread priority.
	 *
	 * @param InRunnable The runnable object to execute
	 * @param ThreadName Name of the thread
	 * @param bAutoDeleteSelf Whether to delete this object on exit
	 * @param bAutoDeleteRunnable Whether to delete the runnable object on exit
	 * @param InStackSize The size of the stack to create. 0 means use the
	 * current thread's stack size
	 * @param InThreadPri Tells the thread whether it needs to adjust its
	 * priority or not. Defaults to normal priority
	 *
	 * @return The newly created thread or NULL if it failed
	 */
	virtual FRunnableThread* CreateThread(FRunnable* InRunnable, const TCHAR* ThreadName,
		UBOOL bAutoDeleteSelf = 0,UBOOL bAutoDeleteRunnable = 0,
		DWORD InStackSize = 0,EThreadPriority InThreadPri = TPri_Normal);

	/**
	 * Cleans up the specified thread object using the correct heap
	 *
	 * @param InThread The thread object to destroy
	 */
	virtual void Destroy(FRunnableThread* InThread);
};

/**
 * This is the PU thread implementation for PU threads (NOT SPU threads!)
 */
class FRunnableThreadPU : public FRunnableThread
{
public:
	/**
	 * Zeroes members
	 */
	FRunnableThreadPU(void);

	/**
	 * Cleans up any resources
	 */
	virtual ~FRunnableThreadPU(void);

	/**
	 * Creates the thread with the specified stack size and thread priority.
	 *
	 * @param InRunnable The runnable object to execute
	 * @param ThreadName Name of the thread
	 * @param bAutoDeleteSelf Whether to delete this object on exit
	 * @param bAutoDeleteRunnable Whether to delete the runnable object on exit
	 * @param InStackSize The size of the stack to create. 0 means use the
	 * current thread's stack size
	 * @param InThreadPri Tells the thread whether it needs to adjust its
	 * priority or not. Defaults to normal priority
	 *
	 * @return The newly created thread or NULL if it failed
	 */
	UBOOL Create(FRunnable* InRunnable, const TCHAR* ThreadName,
		UBOOL bAutoDeleteSelf = 0,UBOOL bAutoDeleteRunnable = 0,
		DWORD InStackSize = 0,EThreadPriority InThreadPri = TPri_Normal);

	/**
	 * Changes the thread priority of the currently running thread
	 *
	 * @param NewPriority The thread priority to change to
	 */
	virtual void SetThreadPriority(EThreadPriority NewPriority);

	/**
	 * Not supported on the PU - only one processor!
	 *
	 * @param ProcessorNum The preferred processor for executing the thread on
	 */
	virtual void SetProcessorAffinity(DWORD ProcessorNum) {}

	/**
	 * Tells the thread to either pause execution or resume depending on the
	 * passed in value.
	 *
	 * @param bShouldPause Whether to pause the thread (true) or resume (false)
	 */
	virtual void Suspend(UBOOL bShouldPause = 1);

	/**
	 * Tells the thread to exit. If the caller needs to know when the thread
	 * has exited, it should use the bShouldWait value and tell it how long
	 * to wait before deciding that it is deadlocked and needs to be destroyed.
	 * The implementation is responsible for calling Stop() on the runnable.
	 * NOTE: having a thread forcibly destroyed can cause leaks in TLS, etc.
	 *
	 * @param bShouldWait If true, the call will wait for the thread to exit
	 * @param MaxWaitTime The amount of time to wait before killing it.
	 * Defaults to inifinite.
	 *
	 * @return True if the thread exited gracefull, false otherwise
	 */
	virtual UBOOL Kill(UBOOL bShouldWait = 0, DWORD MaxWaitTime = INFINITE);

	/**
	 * Halts the caller until this thread is has completed its work.
	 */
	virtual void WaitForCompletion(void);

	/**
	* Thread ID for this thread 
	*
	* @return ID that was set by CreateThread
	*/
	virtual DWORD GetThreadID(void);

	/**
	 * Queries if the runnable should pause itself (yes this is unfortunate, no there is
	 * no way to pause a thread externally!)
	 *
	 * @return True if the runnable should be paused
	 */
	UBOOL IsSuspended(void)
	{
		return bThreadSuspended;
	}

protected:

	/** The thread structurefor the thread */
	sys_ppu_thread_t	Thread;
	/** The runnable object to execute on this thread */
	FRunnable*			Runnable;
	/** Whether we should delete ourselves on thread exit */
	UBOOL				bAutoDeleteSelf;
	/** Whether we should delete the runnable on thread exit */
	UBOOL				bAutoDeleteRunnable;
	/** Whether the thread is currently running */
	UBOOL				bThreadRunning;
	/** Whether the runnable should be paused */
	UBOOL				bThreadSuspended;

	/** A SpinLock variable so that we can do a simple wait for thread initialization */
	sys_semaphore_t		Semaphore;


	/**
	 * The thread entry function that just calls a function in the actual 
	 * PUThread class.
	 */
	static void _ThreadProc(QWORD Arg);

	/**
	 * The real thread entry point. It calls the Init/Run/Exit methods on
	 * the runnable object
	 */
	void Run(void);
};

/**
 * This is the PU version of an event
 */
class FEventPU : public FEvent
{
	/** Event queue and port to send on */
	sys_event_queue_t	EventQueueID;
	sys_event_port_t	EventPortID;

	/** Manual reset events mean that they don't untrigger if one thread gets the event */
	UBOOL bIsManualReset;

public:
	/**
	 * Constructor that zeroes the handle
	 */
	FEventPU(void);

	/**
	 * Cleans up the event handle if valid
	 */
	virtual ~FEventPU(void);

	/**
	 * Waits for the event to be signaled before returning
	 */
	virtual void Lock(void);

	/**
	 * Triggers the event so any waiting threads are allowed access
	 */
	virtual void Unlock(void);

	/**
	 * Creates the event. Manually reset events stay triggered until reset.
	 * Named events share the same underlying event.
	 *
	 * @param bIsManualReset Whether the event requires manual reseting or not
	 * @param InName Whether to use a commonly shared event or not. If so this
	 * is the name of the event to share.
	 *
	 * @return Returns TRUE if the event was created, FALSE otherwise
	 */
	virtual UBOOL Create(UBOOL bIsManualReset = FALSE,const TCHAR* InName = NULL);

	/**
	 * Triggers the event so any waiting threads are activated
	 */
	virtual void Trigger(void);

	/**
	 * Resets the event to an untriggered (waitable) state
	 */
	virtual void Reset(void);

	/**
	 * Triggers the event and resets the triggered state NOTE: This behaves
	 * differently for auto-reset versus manual reset events. All threads
	 * are released for manual reset events and only one is for auto reset
	 */
	virtual void Pulse(void);

	/**
	 * Waits for the event to be triggered
	 *
	 * @param WaitTime Time in milliseconds to wait before abandoning the event
	 * (DWORD)-1 is treated as wait infinite
	 *
	 * @return TRUE if the event was signaled, FALSE if the wait timed out
	 */
	virtual UBOOL Wait(DWORD WaitTime = (DWORD)-1);
};


/**
 * This is the PU interface for creating synchronization objects
 */
class FSynchronizeFactoryPU : public FSynchronizeFactory
{
public:
	/**
	 * Creates a new critical section
	 *
	 * @return The new critical section object or NULL otherwise
	 */
	virtual FCriticalSection* CreateCriticalSection(void);

	/**
	 * Creates a new event
	 *
	 * @param bIsManualReset Whether the event requires manual reseting or not
	 * @param InName Whether to use a commonly shared event or not. If so this
	 * is the name of the event to share.
	 *
	 * @return Returns the new event object if successful, NULL otherwise
	 */
	virtual FEvent* CreateSynchEvent(UBOOL bIsManualReset = FALSE,const TCHAR* InName = NULL);

	/**
	 * Cleans up the specified synchronization object using the correct heap
	 *
	 * @param InSynchObj The synchronization object to destroy
	 */
	virtual void Destroy(FSynchronize* InSynchObj);
};
#endif
