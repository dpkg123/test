/*=============================================================================
	SPUUtils.cpp: Utility functions for PU control of SPU
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Core.h"
#include "PS3Threading.h"
#include "PU_SPU.h"
#include "FFileManagerPS3.h"

//#include <proc.h>
//#include <spu/spu_syscall_num.h>
#include <sys/event.h>
#include <spu/include/sys/spu_event.h>
#include <spu_printf.h>
#include <sys/spu_utility.h>
#include <sys/spu_thread_group.h>
#include <sys/fs_external.h>

#include <sys/spu_image.h>

/** The global synchonization object factory.	*/
FSynchronizeFactory*	GSynchronizeFactory = NULL;
/** The global thread factory.					*/
FThreadFactory*			GThreadFactory		= NULL;
/** Global thread pool for shared async operations */
FQueuedThreadPool*		GThreadPool			= NULL;


// @todo: get these from a header file, but i can't include mfc_io.h because of spu_intrinsics.h
#define MFC_PUT_CMD				0x20
#define MFC_GET_CMD				0x40

#define	DEFAULT_STACK_SIZE		(64 * 1024)

/** 
 * Constructor - creates an event queue with the given key
 * 
 * @param InEventQueueKey	An optional unqiue key for this event queue.
 * @param InQueueDepth		The depth of the event queue. Default is one deep (to process the terminate message)
 */
FPPUEventQueue::FPPUEventQueue(sys_ipc_key_t InEventQueueKey, INT InQueueDepth)
:	EventQueueKey(InEventQueueKey)
,	QueueDepth(InQueueDepth)
,   bIsQueueCreated(FALSE)
{
}

/**
 * Destructor - clean up the queue and termination ports
 */
FPPUEventQueue::~FPPUEventQueue()
{
	// make sure we're cleaned up
	DestroyQueue();
}


/**
 * Create an event queue for this thread to listen on 
 *
 * @return True if initialization was successful, false otherwise
 */
UBOOL FPPUEventQueue::CreateQueue(void)
{
	printf("Creating event queue %d, with depth %d...\n", EventQueueKey, QueueDepth);

	sys_event_queue_attribute_t EventQueueAttr;
    sys_event_queue_attribute_initialize(EventQueueAttr);

	// Create the event queue for this runnable 
	INT Error = sys_event_queue_create(&EventQueue, &EventQueueAttr, EventQueueKey, QueueDepth);
	if (Error)
	{
		printf("FPPUEventQueue::CreateQueue: sys_event_queue_create failed (%d)\n", Error);
		return FALSE;
	}

	return TRUE;
}


/**
 * Destroy the event queue for this thread to listen on (can be called externally as well as from the destructor)
 */
void FPPUEventQueue::DestroyQueue(void)
{
	// return if our queue is invalid
	if (!bIsQueueCreated)
	{
		return;
	}

	// @todo: what to do about any still-connected ports?

	// tear down the Event Queue
	sys_event_queue_destroy(EventQueue, 0);
}

/**
 * Look for events in the queue, and call OnEvent for any events
 *
 * @param bAsynchronous If TRUE, this will process any and all events in the queue and then return, even if there are no events. If FALSE, it will block waiting for a single event, process it, then return
 */
void FPPUEventQueue::ProcessEventQueue(UBOOL bAsynchronous)
{
	if (bAsynchronous)
	{
		// @todo this!
		appErrorf(TEXT("Async events not supported yet!"));
	}
	else
	{
//debugf(TEXT("Waiting for event on queue %d"), EventQueueKey);
		// wait for any event
		sys_event_t Event;
		// @todo: Wrap this?
		// @todo: Added no timelimit (0) for SDK 050.003 --Smedis
		sys_event_queue_receive(EventQueue, &Event, 0);

		// by default, this is a PPU user event
		EEventType EventType = ET_PPUUserEvent;
		QWORD SourceKey = Event.source;
		QWORD Data1 = Event.data1;
		QWORD Data2 = Event.data2;
		QWORD Data3 = Event.data3;

		// check for SPU events
		if (Event.source == SYS_SPU_THREAD_EVENT_USER_KEY || Event.source == SYS_SPU_THREAD_EVENT_DMA_KEY)
		{
			EventType = ET_SPUUserEvent;
			if (Event.source == SYS_SPU_THREAD_EVENT_DMA_KEY)
			{
				EventType = ET_SPUDMAEvent;
			}
			// for SPU events, the source port is in the upper 32 bits of data2,
			// and data1 will have the SPU thread id, the OnEvent function should handle that
			SourceKey = (Event.data2 >> 32);
			Data2 = Event.data2 & 0xFFFFFFFF;
		}

		// let a subclass peek at the event to tell us wheter or not to call OnEvent
		if (PeekEvent(EventType, SourceKey, Data1, Data2, Data3))
		{
			// call the event handler for PPU events
			OnEvent(EventType, SourceKey, Data1, Data2, Data3);
		}
	}
}

/**
* Returns TRUE if the event queue is local to the process.
*/
UBOOL FPPUEventQueue::IsLocal( )
{
	return (EventQueueKey == SYS_EVENT_QUEUE_LOCAL);
}

/**
* Returns TRUE if the event queue is global and can be used for IPC (Inter-Process-Communication).
*/
UBOOL FPPUEventQueue::IsGlobal( )
{
	return (EventQueueKey != SYS_EVENT_QUEUE_LOCAL);
}


/**
 * Zeros any members
 */
FQueuedThreadPPU::FQueuedThreadPPU(void) :
	DoWorkEvent(NULL),
	Thread(0),
	TimeToDie(FALSE),
	QueuedWork(NULL),
	OwningThreadPool(NULL)
{
}

/**
 * Deletes any allocated resources. Kills the thread if it is running.
 */
FQueuedThreadPPU::~FQueuedThreadPPU(void)
{
	// If there is a background thread running, kill it
	if (Thread)
	{
		// Kill() will clean up the event
		Kill(TRUE);
	}
}

/**
 * The thread entry point. Simply forwards the call on to the right
 * thread main function
 */
void FQueuedThreadPPU::_ThreadProc(QWORD This)
{
	check(This);

	// Store the thread-id as a TLS variable.
	appTryInitializeTls();

	PTRINT Pointer = (PTRINT) This;
	((FQueuedThreadPPU*)Pointer)->Run();
}

/**
 * The real thread entry point. It waits for work events to be queued. Once
 * an event is queued, it executes it and goes back to waiting.
 */
void FQueuedThreadPPU::Run(void)
{
	while (!TimeToDie)
	{
		STAT(StatsUpdate.ConditionalUpdate());   // maybe advance the stats frame
		// Wait for some work to do
		DoWorkEvent->Wait();
		FQueuedWork* LocalQueuedWork = QueuedWork;
		QueuedWork = NULL;
		check(LocalQueuedWork || TimeToDie); // well you woke me up, where is the job or termination request?
		while (LocalQueuedWork)
		{
			STAT(StatsUpdate.ConditionalUpdate());   // maybe advance the stats frame
			// Tell the object to do the work
			LocalQueuedWork->DoThreadedWork();
			// Let the object cleanup before we remove our ref to it
			LocalQueuedWork = OwningThreadPool->ReturnToPoolOrGetNextJob(this);
		} 
	}
}

/**
 * Creates the thread with the specified stack size and creates the various
 * events to be able to communicate with it.
 *
 * @param InPool The thread pool interface used to place this thread
 * back into the pool of available threads when its work is done
 * @param ProcessorMask Specifies which processors should be used by the pool
 * @param InStackSize The size of the stack to create. 0 means use the
 * current thread's stack size
 * @param ThreadPriority priority of new thread
 *
 * @return True if the thread and all of its initialization was successful, false otherwise
 */
UBOOL FQueuedThreadPPU::Create(FQueuedThreadPool* InPool,DWORD ProcessorMask,
	DWORD InStackSize,EThreadPriority ThreadPriority)
{
	check(OwningThreadPool == NULL && Thread == 0);
	// Copy the parameters for use in the thread
	OwningThreadPool = InPool;
	// Create the work event used to notify this thread of work
	DoWorkEvent = GSynchronizeFactory->CreateSynchEvent();

	// make sure the stack size is at least a minimal size when not in FinalRelease
	// because appOutputDebugStringf needs 4k, which won't fit in 8k stacks
	DWORD StackSize = InStackSize ? InStackSize : DEFAULT_STACK_SIZE;
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	StackSize = Max<DWORD>(StackSize, 16 * 1024);
#endif

	if (DoWorkEvent != NULL)
	{
		sys_ppu_thread_create(
			&Thread,							// return value
			_ThreadProc,						// entry point
			PUAddress(this),					// parameter
			GetPUThreadPriority(ThreadPriority),// priority as a number [0..4095]
			StackSize,							// stack size for thread
			SYS_PPU_THREAD_CREATE_JOINABLE,		// flag as joinable, non-interrupt thread // @todo - make a way to specify the flags, in the Runnable maybe?
			"PoolThread");						// a standard thread name
	}

	// If it fails, clear all the vars
	if (Thread == 0)
	{
		OwningThreadPool = NULL;
		// Use the factory to clean up this event
		if (DoWorkEvent != NULL)
		{
			GSynchronizeFactory->Destroy(DoWorkEvent);
		}
		DoWorkEvent = NULL;
	}
	return Thread != 0;
}

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
 * @return True if the thread exited gracefully, false otherwise
 */
UBOOL FQueuedThreadPPU::Kill(UBOOL bShouldWait, DWORD MaxWaitTime, UBOOL bShouldDeleteSelf)
{
//	appErrorf(TEXT("Killing a queued thread is unsupported on PS3!"));
#if !FINAL_RELEASE
	printf("Killing a queued thread is unsupported on PS3!\n");
#endif
	return FALSE;
}

/**
 * Tells the thread there is work to be done. Upon completion, the thread
 * is responsible for adding itself back into the available pool.
 *
 * @param InQueuedWork The queued work to perform
 */
void FQueuedThreadPPU::DoWork(FQueuedWork* InQueuedWork)
{
	check(QueuedWork == NULL && "Can't do more than one task at a time");
	// Tell the thread the work to be done
	QueuedWork = InQueuedWork;
	// Tell the thread to wake up and do its job
	DoWorkEvent->Trigger();
}

/**
 * Cleans up any threads that were allocated in the pool
 */
FQueuedThreadPoolPS3::~FQueuedThreadPoolPS3(void)
{
	if (QueuedThreads.Num() > 0)
	{
		Destroy();
	}
}

/**
 * Creates the thread pool with the specified number of threads
 *
 * @param InNumQueuedThreads Specifies the number of threads to use in the pool
 * @param ProcessorMask Specifies which processors should be used by the pool
 * @param StackSize The size of stack the threads in the pool need (32K default)
 *
 * @return Whether the pool creation was successful or not
 */
UBOOL FQueuedThreadPoolPS3::Create(DWORD InNumQueuedThreads, DWORD ProcessorMask, DWORD StackSize,EThreadPriority ThreadPriority)
{
	checkf(InNumQueuedThreads == 1, TEXT("PS3 Trophy code assumes only 1 Queued Thread - update Trophy async work to delay concurrent tasks"));

	// Make sure we have synch objects
	UBOOL bWasSuccessful = CreateSynchObjects();
	if (bWasSuccessful == TRUE)
	{
		FScopeLock Lock(SynchQueue);
		// Presize the array so there is no extra memory allocated
		QueuedThreads.Empty(InNumQueuedThreads);

		// @todo: manage processormask or something for putting some on SPUs?

		// Now create each thread and add it to the array
		for (DWORD Count = 0; Count < InNumQueuedThreads && bWasSuccessful == TRUE; Count++)
		{
			// Create a new queued thread
			FQueuedThread* NewThread = new FQueuedThreadPPU();

			// Now create the thread and add it if ok
			if (NewThread->Create(this, ProcessorMask, StackSize, ThreadPriority) == TRUE)
			{
				QueuedThreads.AddItem(NewThread);
			}
			else
			{
				// Failed to fully create so clean up
				bWasSuccessful = FALSE;
				delete NewThread;
			}
		}
	}

	// Destroy any created threads if the full set was not succesful
	if (bWasSuccessful == FALSE)
	{
		Destroy();
	}

	return bWasSuccessful;
}








/**
 * Name for termination ports.
 * This name is arbitrarily chosen.
 */
#define EVENT_PORT_TERMINATE		(1000000-1)


/** 
 * Constructor - creates an event queue with the given key
 * 
 * @param InEventQueueKey	An optional unqiue key for this event queue.
 * @param InQueueDepth		The depth of the event queue. Default is one deep (to process the terminate message)
 */
FPPURunnable::FPPURunnable(sys_ipc_key_t InEventQueueKey, INT InQueueDepth)
:	FPPUEventQueue(InEventQueueKey, InQueueDepth)
,	bWasStopRequested(FALSE)
{
}

/**
 * Create an event queue for this thread to listen on 
 *
 * @return True if initialization was successful, false otherwise
 */
UBOOL FPPURunnable::Init(void)
{
	if (CreateQueue() == FALSE)
	{
		return FALSE;
	}

	// Create a local event port that the Stop function will use to terminate the thread gracefully
	INT Error = sys_event_port_create(&TerminatePort, SYS_EVENT_PORT_LOCAL, EVENT_PORT_TERMINATE);
	if (Error)
	{
		debugf(TEXT("FPPURunnable::Init: sys_event_port_create failed (%d)"), Error);
		return FALSE;
	}

	// Connect the port to the local/global event queue
	Error = sys_event_port_connect_local(TerminatePort, EventQueue);
	if (Error) 
	{
		debugf(TEXT("FPPURunnable::Init: sys_event_connect failed (%d)"), Error);
		return FALSE;
	}

	return TRUE;
}

/**
 * Wait for events, and call OnEvent when we receive one
 *
 * @return The exit code of the runnable object
 */
DWORD FPPURunnable::Run(void)
{
	// loop until we receive an terminate message
	while (!bWasStopRequested)
	{
		ProcessEventQueue(FALSE);
	}

	// return a nothing code
	return 0;
}

/**
 * This is called if a thread is requested to terminate early
 */
void FPPURunnable::Stop(void)
{
	sys_event_port_send(TerminatePort, MESSAGE_TERMINATE, 0, 0);
}

/**
 * Nothing to do, destructor cleans up, because it has to happen after thread terminates (i think)
 */
void FPPURunnable::Exit(void)
{
	// disconnect and destroy the termination port
	sys_event_port_disconnect(TerminatePort);
	sys_event_port_destroy(TerminatePort);

	// tear down our inherited queue
	DestroyQueue();
}

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
UBOOL FPPURunnable::PeekEvent(EEventType EventType, QWORD SourceEventPort, QWORD Message1, QWORD Message2, QWORD Message3) 
{
	// if we got a message on the termination port, then we set our stop flag
	// as well as return FALSE so that OnEvent won't be called
	if (SourceEventPort == TerminatePort)
	{
		check(Message1 == MESSAGE_TERMINATE);
		bWasStopRequested = TRUE;
		return FALSE;
	}

	// if not the terminate message, do the normal behavior
	return FPPUEventQueue::PeekEvent(EventType, SourceEventPort, Message1, Message2, Message3);
}















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
FRunnableThread* FThreadFactoryPU::CreateThread(FRunnable* InRunnable, const TCHAR* ThreadName, UBOOL bAutoDeleteSelf, UBOOL bAutoDeleteRunnable, DWORD InStackSize, EThreadPriority InThreadPri)
{
	// allocate a thread
	FRunnableThreadPU* NewThread = new FRunnableThreadPU;

	// let the thread create itself
	if (NewThread->Create(InRunnable, ThreadName, bAutoDeleteSelf, bAutoDeleteRunnable, InStackSize, InThreadPri) == false)
	{
		// if we returned false, the thread failed to create
		delete NewThread;

		// @todo: Should we delete the Runnable if bAutoDeleteRunnable is set?
		return NULL;
	}

	return NewThread;
}

/**
 * Cleans up the specified thread object using the correct heap
 *
 * @param InThread The thread object to destroy
 */
void FThreadFactoryPU::Destroy(FRunnableThread* InThread)
{
	delete InThread;
}




/**
 * Zeroes members
 */
FRunnableThreadPU::FRunnableThreadPU(void)
{
	Runnable				= NULL;
	bAutoDeleteSelf			= false;
	bAutoDeleteRunnable		= false;
	bThreadRunning			= false;
	bThreadSuspended		= false;
}

/**
 * Cleans up any resources
 */
FRunnableThreadPU::~FRunnableThreadPU(void)
{
	// if the thread is still running, it should be shut down
	if (bThreadRunning)
	{
		Kill(true);
	}
}

/**
 * Creates and initializes a thread with the specified stack size and thread priority.
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
 * @return True if the thread and all of its initialization was successful, false otherwise
 */
UBOOL FRunnableThreadPU::Create(FRunnable* InRunnable, const TCHAR* ThreadName, UBOOL bInAutoDeleteSelf, UBOOL bInAutoDeleteRunnable, DWORD InStackSize, EThreadPriority InThreadPri)
{
	// remember our inputs
	Runnable = InRunnable;
	bAutoDeleteSelf = bInAutoDeleteSelf;
	bAutoDeleteRunnable = bAutoDeleteRunnable;

	// Create a Semaphore object that we will wait on
	sys_semaphore_attribute_t SemaphoreAttr;
	sys_semaphore_attribute_initialize( SemaphoreAttr );
	sys_semaphore_create( &Semaphore, &SemaphoreAttr, 0, 1 );

	
	// make sure the stack size is at least a minimal size when not in FinalRelease
	// because appOutputDebugStringf needs 4k, which won't fit in 8k stacks
	DWORD StackSize = InStackSize ? InStackSize : DEFAULT_STACK_SIZE;
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	StackSize = Max<DWORD>(StackSize, 16 * 1024);
#endif

	// create a PU thread
	INT Result = sys_ppu_thread_create(
		&Thread,							// return value
		_ThreadProc,						// entry point
		PUAddress(this),					// parameter
		GetPUThreadPriority(InThreadPri),	// priority as a number [0..4095]
		StackSize,							// stack size for thread
		SYS_PPU_THREAD_CREATE_JOINABLE,		// flag as joinable, non-interrupt thread // @todo - make a way to specify the flags, in the Runnable maybe?
		TCHAR_TO_ANSI(ThreadName));			// a standard thread name

	debugf(TEXT("Waiting for runnable to be initialized..."));

	// wait until the thread has initialized (timing out after 2 seconds)
	sys_semaphore_wait( Semaphore, 2*1000*1000 );

	//clean up the syncpoint
	sys_semaphore_destroy( Semaphore );

	return Result == SUCCEEDED;
}


/**
 * Changes the thread priority of the currently running thread
 *
 * @param NewPriority The thread priority to change to
 */
void FRunnableThreadPU::SetThreadPriority(EThreadPriority NewPriority)
{
	// set the priority for this thread by number
	sys_ppu_thread_set_priority(Thread, GetPUThreadPriority(NewPriority));	
}

/**
 * Tells the thread to either pause execution or resume depending on the
 * passed in value.
 *
 * @param bShouldPause Whether to pause the thread (true) or resume (false)
 */
void FRunnableThreadPU::Suspend(UBOOL bShouldPause)
{
	// there is no way to stop a PU thread externally, so we set a flag, and
	// hope the runnable will query for it (since it gets a pointer this this)
	bThreadSuspended = bShouldPause;
}

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
UBOOL FRunnableThreadPU::Kill(UBOOL bShouldWait, DWORD MaxWaitTime)
{
	// @todo kill the thread!
	if (!bThreadRunning)
	{
		return true;
	}

	// mark that the thread is no longer valid
	bThreadRunning = false;

	// allow the thread to clean up in case it gets forcibly terminated
	Runnable->Stop();

	// wait for completion if we desire
	// @todo: There is a very high chance of getting deadlocked waiting for the thread to end
	if (bShouldWait)
	{
		WaitForCompletion();
	}

	// Should we delete the runnable?
	if (bAutoDeleteRunnable == TRUE)
	{
		delete Runnable;
		Runnable = NULL;
	}

	// Delete ourselves if requested
	if (bAutoDeleteSelf == TRUE)
	{
		GThreadFactory->Destroy(this);
	}

	return true;
}

/**
 * Halts the caller until this thread is has completed its work.
 */
void FRunnableThreadPU::WaitForCompletion(void)
{
	QWORD ReturnCode;
	// wait for the thread to complete
	sys_ppu_thread_join(Thread, &ReturnCode);
}

/**
 * The thread entry function that just calls a function in the actual 
 * PUThread class.
 */
void FRunnableThreadPU::_ThreadProc(QWORD Arg)
{
	PTRINT Pointer = (PTRINT) Arg;

	// If we have access to TLS, then store the thread-id as a TLS variable.
	appTryInitializeTls();

	// call the non-static thread function
	((FRunnableThreadPU*)Pointer)->Run();
}

/**
 * The thread entry point. It calls the Init/Run/Exit methods on
 * the runnable object
 */
void FRunnableThreadPU::Run()
{
	// Assume we'll fail init
	DWORD ExitCode = 1;
	check(Runnable);

	// Initialize the runnable object
	UBOOL InitReturn = Runnable->Init();

	// let the caller know we have finished initializing
	sys_semaphore_post( Semaphore, 1 );

	if (InitReturn == TRUE)
	{
		// Now run the task that needs to be done
		ExitCode = Runnable->Run();
		// Allow any allocated resources to be cleaned up
		Runnable->Exit();
	}

	// Clean ourselves up without waiting
	if (bAutoDeleteSelf == TRUE)
	{
		// Passing TRUE here will deadlock the thread
		Kill(FALSE);
	}

	// return from the thread with the exit code
	sys_ppu_thread_exit(ExitCode);
}

/**
* Thread ID for this thread 
*
* @return ID that was set by CreateThread
*/
DWORD FRunnableThreadPU::GetThreadID(void)
{
	return Thread;
}

/**
 * Constructor that zeroes the handle
 */
FEventPU::FEventPU(void)
{
	EventQueueID = 0;
	EventPortID = 0;
}

/**
 * Cleans up the event handle if valid
 */
FEventPU::~FEventPU(void)
{
	if (EventQueueID != 0)
	{
		sys_event_queue_destroy(EventQueueID, 0);
	}

	if (EventPortID != 0)
	{
		sys_event_port_destroy(EventPortID);
	}
}

/**
 * Waits for the event to be signaled before returning
 */
void FEventPU::Lock(void)
{
	// @todo: Joe, do we need these functions?
	check(0);
	//// wait forever to be sent an event
	//sys_event_t Event;
	//sys_event_queue_receive(EventQueueID, &Event, 0);
}

/**
 * Triggers the event so any waiting threads are allowed access
 */
void FEventPU::Unlock(void)
{
	check(0);
	//sys_event_queue_cancel(EventQueueID);
}

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
UBOOL FEventPU::Create(UBOOL bInIsManualReset,const TCHAR* InName)
{
	bIsManualReset = bInIsManualReset;

	// initialize the event attributes
	sys_event_queue_attribute_t QueueAttr;
	sys_event_queue_attribute_initialize(QueueAttr);

	// Create the event queue
	if (sys_event_queue_create(&EventQueueID, &QueueAttr, SYS_EVENT_QUEUE_LOCAL, 1) != CELL_OK)
	{
		return FALSE;
	}

	// Create the event port
	if (sys_event_port_create(&EventPortID, SYS_EVENT_PORT_LOCAL, SYS_EVENT_PORT_NO_NAME) != CELL_OK)
	{
		return FALSE;
	}

	// connect the port to the queue for triggering
	if (sys_event_port_connect_local(EventPortID, EventQueueID) != CELL_OK)
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * Triggers the event so any waiting threads are activated
 */
void FEventPU::Trigger(void)
{
	sys_event_port_send(EventPortID, 0, 0, 0);
}

/**
 * Resets the event to an untriggered (waitable) state
 */
void FEventPU::Reset(void)
{
	sys_event_queue_drain(EventQueueID);
}

/**
 * Triggers the event and resets the triggered state NOTE: This behaves
 * differently for auto-reset versus manual reset events. All threads
 * are released for manual reset events and only one is for auto reset
 */
void FEventPU::Pulse(void)
{
	// send an event, then reset everything
	Trigger();
	Reset();
}

/**
 * Waits for the event to be triggered
 *
 * @param WaitTime Time in milliseconds to wait before abandoning the event
 * (DWORD)-1 is treated as wait infinite
 *
 * @return TRUE if the event was signaled, FALSE if the wait timed out
 */
UBOOL FEventPU::Wait(DWORD WaitTime)
{
	// only wait for a certain time (convert to microseconds)
	sys_event_t Event;
	INT ReturnValue = sys_event_queue_receive(EventQueueID, &Event, (WaitTime == INFINITE) ? 0 : (WaitTime == 0) ? 1 : WaitTime * 1000);
	// if we actually got an event
	if (ReturnValue != ETIMEDOUT)
	{
		// if we actually received an event, and we are manual reset, we need to re-trigger the event
		// to keep us in the triggered state until someone resets us
		// if Reset was called, ReturnValue will be ECANCELED, in which case we don't want to re-trigger
		if (ReturnValue == CELL_OK && bIsManualReset)
		{
			Trigger();
		}
		return TRUE;
	}
	return FALSE;
}








/**
 * Creates a new critical section
 *
 * @return The new critical section object or NULL otherwise
 */
FCriticalSection* FSynchronizeFactoryPU::CreateCriticalSection()
{
	return new FCriticalSection();
}

/**
 * Creates a new event
 *
 * @param bIsManualReset Whether the event requires manual reseting or not
 * @param InName Whether to use a commonly shared event or not. If so this
 * is the name of the event to share.
 *
 * @return Returns the new event object if successful, NULL otherwise
 */
FEvent* FSynchronizeFactoryPU::CreateSynchEvent(UBOOL bIsManualReset, const TCHAR* InName)
{
	// Allocate the new object
	FEvent* Event = new FEventPU();
	// If the internal create fails, delete the instance and return NULL
	if (Event->Create(bIsManualReset, InName) == FALSE)
	{
		delete Event;
		Event = NULL;
	}
	return Event;
}


/**
 * Cleans up the specified synchronization object using the correct heap
 *
 * @param InSynchObj The synchronization object to destroy
 */
void FSynchronizeFactoryPU::Destroy(FSynchronize* InSynchObj)
{
	delete InSynchObj;
}

/**
 * Maps an EThreadPriority to a PU thread priority number.
 *
 * @param Prio The Unreal thread priority
 *
 * @return The PU thread priority where 0 is highest, 4095 is lowest
 */
DWORD GetPUThreadPriority(EThreadPriority Prio)
{
	// return a number for each item in the EThreadPriority enum
	switch (Prio)
	{
		case TPri_AboveNormal:	return 1000;
		case TPri_Normal:		return 2000;
		case TPri_BelowNormal:	return 3000;
		default:				return 2048;
	}
}
