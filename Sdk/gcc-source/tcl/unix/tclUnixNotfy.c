/*
 * tclUnixNotify.c --
 *
 *	This file contains the implementation of the select-based
 *	Unix-specific notifier, which is the lowest-level part of the
 *	Tcl event loop.  This file works together with
 *	../generic/tclNotify.c.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclUnixNotfy.c,v 1.6.8.2 2000/09/15 16:58:25 spolk Exp $
 */

#include "tclInt.h"
#include "tclPort.h"
#include <signal.h> 

extern TclStubs tclStubs;

/*
 * This structure is used to keep track of the notifier info for a 
 * a registered file.
 */

typedef struct FileHandler {
    int fd;
    int mask;			/* Mask of desired events: TCL_READABLE,
				 * etc. */
    int readyMask;		/* Mask of events that have been seen since the
				 * last time file handlers were invoked for
				 * this file. */
    Tcl_FileProc *proc;		/* Procedure to call, in the style of
				 * Tcl_CreateFileHandler. */
    ClientData clientData;	/* Argument to pass to proc. */
    struct FileHandler *nextPtr;/* Next in list of all files we care about. */
} FileHandler;

/*
 * The following structure is what is added to the Tcl event queue when
 * file handlers are ready to fire.
 */

typedef struct FileHandlerEvent {
    Tcl_Event header;		/* Information that is standard for
				 * all events. */
    int fd;			/* File descriptor that is ready.  Used
				 * to find the FileHandler structure for
				 * the file (can't point directly to the
				 * FileHandler structure because it could
				 * go away while the event is queued). */
} FileHandlerEvent;

/*
 * The following static structure contains the state information for the
 * select based implementation of the Tcl notifier.  One of these structures
 * is created for each thread that is using the notifier.  
 */

typedef struct ThreadSpecificData {
    FileHandler *firstFileHandlerPtr;
				/* Pointer to head of file handler list. */
    fd_set checkMasks[3];
				/* This array is used to build up the masks
				 * to be used in the next call to select.
				 * Bits are set in response to calls to
				 * Tcl_CreateFileHandler. */
    fd_set readyMasks[3];
				/* This array reflects the readable/writable
				 * conditions that were found to exist by the
				 * last call to select. */
    int numFdBits;		/* Number of valid bits in checkMasks
				 * (one more than highest fd for which
				 * Tcl_WatchFile has been called). */
#ifdef TCL_THREADS
    int onList;			/* True if it is in this list */
    unsigned int pollState;	/* pollState is used to implement a polling 
				 * handshake between each thread and the
				 * notifier thread. Bits defined below. */
    struct ThreadSpecificData *nextPtr, *prevPtr;
                                /* All threads that are currently waiting on 
                                 * an event have their ThreadSpecificData
                                 * structure on a doubly-linked listed formed
                                 * from these pointers.  You must hold the
                                 * notifierMutex lock before accessing these
                                 * fields. */
    Tcl_Condition waitCV;     /* Any other thread alerts a notifier
				 * that an event is ready to be processed
				 * by signaling this condition variable. */
    int eventReady;           /* True if an event is ready to be processed.
                               * Used as condition flag together with
                               * waitCV above. */
#endif
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

#ifdef TCL_THREADS
/*
 * The following static indicates the number of threads that have
 * initialized notifiers.
 *
 * You must hold the notifierMutex lock before accessing this variable.
 */

static int notifierCount = 0;

/*
 * The following variable points to the head of a doubly-linked list of 
 * of ThreadSpecificData structures for all threads that are currently
 * waiting on an event.
 *
 * You must hold the notifierMutex lock before accessing this list.
 */

static ThreadSpecificData *waitingListPtr = NULL;

/*
 * The notifier thread spends all its time in select() waiting for a
 * file descriptor associated with one of the threads on the waitingListPtr
 * list to do something interesting.  But if the contents of the
 * waitingListPtr list ever changes, we need to wake up and restart
 * the select() system call.  You can wake up the notifier thread by
 * writing a single byte to the file descriptor defined below.  This
 * file descriptor is the input-end of a pipe and the notifier thread is
 * listening for data on the output-end of the same pipe.  Hence writing
 * to this file descriptor will cause the select() system call to return
 * and wake up the notifier thread.
 *
 * You must hold the notifierMutex lock before accessing this list.
 */

static int triggerPipe = -1;

/*
 * The notifierMutex locks access to all of the global notifier state. 
 */

TCL_DECLARE_MUTEX(notifierMutex)

/*
 * The notifier thread signals the notifierCV when it has finished
 * initializing the triggerPipe and right before the notifier
 * thread terminates.
 */

static Tcl_Condition notifierCV;

/*
 * The pollState bits
 *	POLL_WANT is set by each thread before it waits on its condition
 *		variable.  It is checked by the notifier before it does
 *		select.
 *	POLL_DONE is set by the notifier if it goes into select after
 *		seeing POLL_WANT.  The idea is to ensure it tries a select
 *		with the same bits the initial thread had set.
 */
#define POLL_WANT	0x1
#define POLL_DONE	0x2

/*
 * This is the thread ID of the notifier thread that does select.
 */
static Tcl_ThreadId notifierThread;

#endif

/*
 * Static routines defined in this file.
 */

#ifdef TCL_THREADS
static void	NotifierThreadProc _ANSI_ARGS_((ClientData clientData));
#endif
static int	FileHandlerEventProc _ANSI_ARGS_((Tcl_Event *evPtr,
		    int flags));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InitNotifier --
 *
 *	Initializes the platform specific notifier state.
 *
 * Results:
 *	Returns a handle to the notifier state for this thread..
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_InitNotifier()
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

#ifdef TCL_THREADS
    tsdPtr->eventReady = 0;

    /*
     * Start the Notifier thread if necessary.
     */

    Tcl_MutexLock(&notifierMutex);
    if (notifierCount == 0) {
	if (Tcl_CreateThread(&notifierThread, NotifierThreadProc, NULL,
		     TCL_THREAD_STACK_DEFAULT, TCL_THREAD_NOFLAGS) != TCL_OK) {
	    panic("Tcl_InitNotifier: unable to start notifier thread");
	}
    }
    notifierCount++;

    /*
     * Wait for the notifier pipe to be created.
     */

    while (triggerPipe < 0) {
	Tcl_ConditionWait(&notifierCV, &notifierMutex, NULL);
    }

    Tcl_MutexUnlock(&notifierMutex);
#endif
    return (ClientData) tsdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FinalizeNotifier --
 *
 *	This function is called to cleanup the notifier state before
 *	a thread is terminated.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May terminate the background notifier thread if this is the
 *	last notifier instance.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FinalizeNotifier(clientData)
    ClientData clientData;		/* Not used. */
{
#ifdef TCL_THREADS
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    Tcl_MutexLock(&notifierMutex);
    notifierCount--;

    /*
     * If this is the last thread to use the notifier, close the notifier
     * pipe and wait for the background thread to terminate.
     */

    if (notifierCount == 0) {
	if (triggerPipe < 0) {
	    panic("Tcl_FinalizeNotifier: notifier pipe not initialized");
	}

        /*
	 * Send "q" message to the notifier thread so that it will
	 * terminate.  The notifier will return from its call to select()
	 * and notice that a "q" message has arrived, it will then close
	 * its side of the pipe and terminate its thread.  Note the we can
	 * not just close the pipe and check for EOF in the notifier
	 * thread because if a background child process was created with
	 * exec, select() would not register the EOF on the pipe until the
	 * child processes had terminated. [Bug: 4139]
	 */
	write(triggerPipe, "q", 1);
	close(triggerPipe);

	Tcl_ConditionWait(&notifierCV, &notifierMutex, NULL);
    }

    /*
     * Clean up any synchronization objects in the thread local storage.
     */

    Tcl_ConditionFinalize(&(tsdPtr->waitCV));

    Tcl_MutexUnlock(&notifierMutex);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AlertNotifier --
 *
 *	Wake up the specified notifier from any thread. This routine
 *	is called by the platform independent notifier code whenever
 *	the Tcl_ThreadAlert routine is called.  This routine is
 *	guaranteed not to be called on a given notifier after
 *	Tcl_FinalizeNotifier is called for that notifier.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Signals the notifier condition variable for the specified
 *	notifier.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AlertNotifier(clientData)
    ClientData clientData;
{
#ifdef TCL_THREADS
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *) clientData;
    Tcl_MutexLock(&notifierMutex);
    tsdPtr->eventReady = 1;
    Tcl_ConditionNotify(&tsdPtr->waitCV);
    Tcl_MutexUnlock(&notifierMutex);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetTimer --
 *
 *	This procedure sets the current notifier timer value.  This
 *	interface is not implemented in this notifier because we are
 *	always running inside of Tcl_DoOneEvent.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetTimer(timePtr)
    Tcl_Time *timePtr;		/* Timeout value, may be NULL. */
{
    /*
     * The interval timer doesn't do anything in this implementation,
     * because the only event loop is via Tcl_DoOneEvent, which passes
     * timeout values to Tcl_WaitForEvent.
     */

    if (tclStubs.tcl_SetTimer != Tcl_SetTimer) {
	tclStubs.tcl_SetTimer(timePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ServiceModeHook --
 *
 *	This function is invoked whenever the service mode changes.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ServiceModeHook(mode)
    int mode;			/* Either TCL_SERVICE_ALL, or
				 * TCL_SERVICE_NONE. */
{
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateFileHandler --
 *
 *	This procedure registers a file handler with the select notifier.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates a new file handler structure.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CreateFileHandler(fd, mask, proc, clientData)
    int fd;			/* Handle of stream to watch. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions under which
				 * proc should be called. */
    Tcl_FileProc *proc;		/* Procedure to call for each
				 * selected event. */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    FileHandler *filePtr;

    if (tclStubs.tcl_CreateFileHandler != Tcl_CreateFileHandler) {
	tclStubs.tcl_CreateFileHandler(fd, mask, proc, clientData);
	return;
    }

    for (filePtr = tsdPtr->firstFileHandlerPtr; filePtr != NULL;
	 filePtr = filePtr->nextPtr) {
	if (filePtr->fd == fd) {
	    break;
	}
    }
    if (filePtr == NULL) {
	filePtr = (FileHandler*) ckalloc(sizeof(FileHandler));
	filePtr->fd = fd;
	filePtr->readyMask = 0;
	filePtr->nextPtr = tsdPtr->firstFileHandlerPtr;
	tsdPtr->firstFileHandlerPtr = filePtr;
    }
    filePtr->proc = proc;
    filePtr->clientData = clientData;
    filePtr->mask = mask;

    /*
     * Update the check masks for this file.
     */

    if (mask & TCL_READABLE) {
	FD_SET(fd, &tsdPtr->checkMasks[0]);
    } else {
	FD_CLR(fd, &tsdPtr->checkMasks[0]);
    } 
    if (mask & TCL_WRITABLE) {
	FD_SET(fd, &tsdPtr->checkMasks[1]);
    } else {
	FD_CLR(fd, &tsdPtr->checkMasks[1]);
    }
    if (mask & TCL_EXCEPTION) {
	FD_SET(fd, &tsdPtr->checkMasks[2]);
    } else {
	FD_CLR(fd, &tsdPtr->checkMasks[2]);
    }
    if (tsdPtr->numFdBits <= fd) {
	tsdPtr->numFdBits = fd+1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DeleteFileHandler --
 *
 *	Cancel a previously-arranged callback arrangement for
 *	a file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a callback was previously registered on file, remove it.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DeleteFileHandler(fd)
    int fd;		/* Stream id for which to remove callback procedure. */
{
    FileHandler *filePtr, *prevPtr;
    int i;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (tclStubs.tcl_DeleteFileHandler != Tcl_DeleteFileHandler) {
	tclStubs.tcl_DeleteFileHandler(fd);
	return;
    }

    /*
     * Find the entry for the given file (and return if there isn't one).
     */

    for (prevPtr = NULL, filePtr = tsdPtr->firstFileHandlerPtr; ;
	 prevPtr = filePtr, filePtr = filePtr->nextPtr) {
	if (filePtr == NULL) {
	    return;
	}
	if (filePtr->fd == fd) {
	    break;
	}
    }

    /*
     * Update the check masks for this file.
     */

    if (filePtr->mask & TCL_READABLE) {
	FD_CLR(fd, &tsdPtr->checkMasks[0]);
    }
    if (filePtr->mask & TCL_WRITABLE) {
	FD_CLR(fd, &tsdPtr->checkMasks[1]);
    }
    if (filePtr->mask & TCL_EXCEPTION) {
	FD_CLR(fd, &tsdPtr->checkMasks[2]);
    }

    /*
     * Find current max fd.
     */

    if (fd+1 == tsdPtr->numFdBits) {
        for (i=fd; i >= 0; i--) {
	    if (FD_ISSET(i, &tsdPtr->checkMasks[0])
	       || FD_ISSET(i, &tsdPtr->checkMasks[1])
	       || FD_ISSET(i, &tsdPtr->checkMasks[2])) {
		break;
	    }
	}
	tsdPtr->numFdBits = i+1;  /* i might be -1; that's right. */
    }

    /*
     * Clean up information in the callback record.
     */

    if (prevPtr == NULL) {
	tsdPtr->firstFileHandlerPtr = filePtr->nextPtr;
    } else {
	prevPtr->nextPtr = filePtr->nextPtr;
    }
    ckfree((char *) filePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FileHandlerEventProc --
 *
 *	This procedure is called by Tcl_ServiceEvent when a file event
 *	reaches the front of the event queue.  This procedure is
 *	responsible for actually handling the event by invoking the
 *	callback for the file handler.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed
 *	from the queue.  Returns 0 if the event was not handled, meaning
 *	it should stay on the queue.  The only time the event isn't
 *	handled is if the TCL_FILE_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the file handler's callback procedure does.
 *
 *----------------------------------------------------------------------
 */

static int
FileHandlerEventProc(evPtr, flags)
    Tcl_Event *evPtr;		/* Event to service. */
    int flags;			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    int mask;
    FileHandler *filePtr;
    FileHandlerEvent *fileEvPtr = (FileHandlerEvent *) evPtr;
    ThreadSpecificData *tsdPtr;

    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }

    /*
     * Search through the file handlers to find the one whose handle matches
     * the event.  We do this rather than keeping a pointer to the file
     * handler directly in the event, so that the handler can be deleted
     * while the event is queued without leaving a dangling pointer.
     */

    tsdPtr = TCL_TSD_INIT(&dataKey);
    for (filePtr = tsdPtr->firstFileHandlerPtr; filePtr != NULL;
	 filePtr = filePtr->nextPtr) {
	if (filePtr->fd != fileEvPtr->fd) {
	    continue;
	}

	/*
	 * The code is tricky for two reasons:
	 * 1. The file handler's desired events could have changed
	 *    since the time when the event was queued, so AND the
	 *    ready mask with the desired mask.
	 * 2. The file could have been closed and re-opened since
	 *    the time when the event was queued.  This is why the
	 *    ready mask is stored in the file handler rather than
	 *    the queued event:  it will be zeroed when a new
	 *    file handler is created for the newly opened file.
	 */

	mask = filePtr->readyMask & filePtr->mask;
	filePtr->readyMask = 0;
	if (mask != 0) {
	    (*filePtr->proc)(filePtr->clientData, mask);
	}
	break;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitForEvent --
 *
 *	This function is called by Tcl_DoOneEvent to wait for new
 *	events on the message queue.  If the block time is 0, then
 *	Tcl_WaitForEvent just polls without blocking.
 *
 * Results:
 *	Returns -1 if the select would block forever, otherwise
 *	returns 0.
 *
 * Side effects:
 *	Queues file events that are detected by the select.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WaitForEvent(timePtr)
    Tcl_Time *timePtr;		/* Maximum block time, or NULL. */
{
    FileHandler *filePtr;
    FileHandlerEvent *fileEvPtr;
    struct timeval timeout, *timeoutPtr;
    int mask;
#ifdef TCL_THREADS
    int waitForFiles;
#else
    int numFound;
#endif
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (tclStubs.tcl_WaitForEvent != Tcl_WaitForEvent) {
	return tclStubs.tcl_WaitForEvent(timePtr);
    }

    /*
     * Set up the timeout structure.  Note that if there are no events to
     * check for, we return with a negative result rather than blocking
     * forever.
     */

    if (timePtr) {
	timeout.tv_sec = timePtr->sec;
	timeout.tv_usec = timePtr->usec;
	timeoutPtr = &timeout;
#ifndef TCL_THREADS
    } else if (tsdPtr->numFdBits == 0) {
	/*
	 * If there are no threads, no timeout, and no fds registered,
	 * then there are no events possible and we must avoid deadlock.
	 * Note that this is not entirely correct because there might
	 * be a signal that could interrupt the select call, but we
	 * don't handle that case if we aren't using threads.
	 */

	return -1;
#endif
    } else {
	timeoutPtr = NULL;
    }

#ifdef TCL_THREADS
    /*
     * Place this thread on the list of interested threads, signal the
     * notifier thread, and wait for a response or a timeout.
     */

    Tcl_MutexLock(&notifierMutex);

    waitForFiles = (tsdPtr->numFdBits > 0);
    if (timePtr != NULL && timePtr->sec == 0 && timePtr->usec == 0) {
	/*
	 * Cannot emulate a polling select with a polling condition variable.
	 * Instead, pretend to wait for files and tell the notifier
	 * thread what we are doing.  The notifier thread makes sure
	 * it goes through select with its select mask in the same state
	 * as ours currently is.  We block until that happens.
	 */

	waitForFiles = 1;
	tsdPtr->pollState = POLL_WANT;
	timePtr = NULL;
    } else {
	tsdPtr->pollState = 0;
    }

    if (waitForFiles) {
        /*
         * Add the ThreadSpecificData structure of this thread to the list
         * of ThreadSpecificData structures of all threads that are waiting
         * on file events.
         */


        tsdPtr->nextPtr = waitingListPtr;
        if (waitingListPtr) {
            waitingListPtr->prevPtr = tsdPtr;
        }
        tsdPtr->prevPtr = 0;
        waitingListPtr = tsdPtr;
	tsdPtr->onList = 1;
	
	write(triggerPipe, "", 1);
    }

    memset((VOID *) tsdPtr->readyMasks, 0, sizeof(tsdPtr->readyMasks));

    if (!tsdPtr->eventReady) {
        Tcl_ConditionWait(&tsdPtr->waitCV, &notifierMutex, timePtr);
    }
    tsdPtr->eventReady = 0;

    if (waitForFiles && tsdPtr->onList) {
	/*
	 * Remove the ThreadSpecificData structure of this thread from the
	 * waiting list.  Alert the notifier thread to recompute its select
	 * masks - skipping this caused a hang when trying to close a pipe
	 * which the notifier thread was still doing a select on.
	 */

        if (tsdPtr->prevPtr) {
            tsdPtr->prevPtr->nextPtr = tsdPtr->nextPtr;
        } else {
            waitingListPtr = tsdPtr->nextPtr;
        }
        if (tsdPtr->nextPtr) {
            tsdPtr->nextPtr->prevPtr = tsdPtr->prevPtr;
        }
        tsdPtr->nextPtr = tsdPtr->prevPtr = NULL;
	tsdPtr->onList = 0;
	write(triggerPipe, "", 1);
    }

    
#else
    memcpy((VOID *) tsdPtr->readyMasks, (VOID *) tsdPtr->checkMasks,
	    sizeof(tsdPtr->readyMasks));
    numFound = select(tsdPtr->numFdBits,
	    &tsdPtr->readyMasks[0],
	    &tsdPtr->readyMasks[1],
	    &tsdPtr->readyMasks[2], timeoutPtr);
    /*
     * Some systems don't clear the masks after an error, so
     * we have to do it here.
     */

    if (numFound == -1) {
	memset((VOID *) tsdPtr->readyMasks, 0, sizeof(tsdPtr->readyMasks));
    }
#endif

    /*
     * Queue all detected file events before returning.
     */

    for (filePtr = tsdPtr->firstFileHandlerPtr; (filePtr != NULL);
	 filePtr = filePtr->nextPtr) {
	mask = 0;

	if (FD_ISSET(filePtr->fd,&tsdPtr->readyMasks[0])) {
	    mask |= TCL_READABLE;
	}
	if (FD_ISSET(filePtr->fd,&tsdPtr->readyMasks[1])) {
	    mask |= TCL_WRITABLE;
	}
	if (FD_ISSET(filePtr->fd,&tsdPtr->readyMasks[2])) {
	    mask |= TCL_EXCEPTION;
	}

	if (!mask) {
	    continue;
	}

	/*
	 * Don't bother to queue an event if the mask was previously
	 * non-zero since an event must still be on the queue.
	 */

	if (filePtr->readyMask == 0) {
	    fileEvPtr = (FileHandlerEvent *) ckalloc(
		sizeof(FileHandlerEvent));
	    fileEvPtr->header.proc = FileHandlerEventProc;
	    fileEvPtr->fd = filePtr->fd;
	    Tcl_QueueEvent((Tcl_Event *) fileEvPtr, TCL_QUEUE_TAIL);
	}
	filePtr->readyMask = mask;
    }
#ifdef TCL_THREADS
    Tcl_MutexUnlock(&notifierMutex);
#endif
    return 0;
}

#ifdef TCL_THREADS
/*
 *----------------------------------------------------------------------
 *
 * NotifierThreadProc --
 *
 *	This routine is the initial (and only) function executed by the
 *	special notifier thread.  Its job is to wait for file descriptors
 *	to become readable or writable or to have an exception condition
 *	and then to notify other threads who are interested in this
 *	information by signalling a condition variable.  Other threads
 *	can signal this notifier thread of a change in their interests
 *	by writing a single byte to a special pipe that the notifier
 *	thread is monitoring.
 *
 * Result:
 *	None.  Once started, this routine never exits.  It dies with
 *	the overall process.
 *
 * Side effects:
 *	The trigger pipe used to signal the notifier thread is created
 *	when the notifier thread first starts.
 *
 *----------------------------------------------------------------------
 */

static void
NotifierThreadProc(clientData)
    ClientData clientData;	/* Not used. */
{
    ThreadSpecificData *tsdPtr;
    fd_set masks[3];
    int fds[2];
    int i, j, status, numFdBits, found, receivePipe;
    struct timeval poll = {0., 0.}, *timePtr;
    char buf[2];

    if (pipe(fds) != 0) {
	panic("NotifierThreadProc: could not create trigger pipe.");
    }

    receivePipe = fds[0];

#ifndef USE_FIONBIO
    status = fcntl(receivePipe, F_GETFL);
    status |= O_NONBLOCK;
    if (fcntl(receivePipe, F_SETFL, status) < 0) {
	panic("NotifierThreadProc: could not make receive pipe non blocking.");
    }
    status = fcntl(fds[1], F_GETFL);
    status |= O_NONBLOCK;
    if (fcntl(fds[1], F_SETFL, status) < 0) {
	panic("NotifierThreadProc: could not make trigger pipe non blocking.");
    }
#else
    if (ioctl(receivePipe, (int) FIONBIO, &status) < 0) {
	panic("NotifierThreadProc: could not make receive pipe non blocking.");
    }
    if (ioctl(fds[1], (int) FIONBIO, &status) < 0) {
	panic("NotifierThreadProc: could not make trigger pipe non blocking.");
    }
#endif

    /*
     * Install the write end of the pipe into the global variable.
     */

    Tcl_MutexLock(&notifierMutex);
    triggerPipe = fds[1];

    /*
     * Signal any threads that are waiting.
     */

    Tcl_ConditionNotify(&notifierCV);
    Tcl_MutexUnlock(&notifierMutex);

    /*
     * Look for file events and report them to interested threads.
     */

    while (1) {
	/*
	 * Set up the select mask to include the receive pipe.
	 */

	memset((VOID *)masks, 0, sizeof(masks));
        numFdBits = receivePipe + 1;
	FD_SET(receivePipe, &masks[0])

	/*
	 * Add in the check masks from all of the waiting notifiers.
	 */
	
	Tcl_MutexLock(&notifierMutex);
	timePtr = NULL;
        for (tsdPtr = waitingListPtr; tsdPtr; tsdPtr = tsdPtr->nextPtr) {
	    /* This is not quite strictly portable, but should work
	       many places. */
	    for (j=0; j<3; j++) {
		for (i = 0;
		     i < (int)(sizeof(masks[j].fds_bits)
			 / sizeof(masks[j].fds_bits[0]));
		     i++) {
		    masks[j].fds_bits[i] 
			|= tsdPtr->checkMasks[j].fds_bits[i];
		}
	    }
            if (tsdPtr->numFdBits > numFdBits) {
                numFdBits = tsdPtr->numFdBits;
            }
	    if (tsdPtr->pollState & POLL_WANT) {
		/*
		 * Here we make sure we go through select() with the same
		 * mask bits that were present when the thread tried to poll.
		 */

		tsdPtr->pollState |= POLL_DONE;
		timePtr = &poll;
	    }
        }
	Tcl_MutexUnlock(&notifierMutex);

	found = select(numFdBits, (SELECT_MASK *) &masks[0],
				  (SELECT_MASK *) &masks[1],
				  (SELECT_MASK *) &masks[2], timePtr);
	if (found == -1) {
	    /*
	     * Try again immediately on an error.
	     */

	    continue;
        }

	/*
	 * Alert any threads that are waiting on a ready file descriptor.
	 */

	Tcl_MutexLock(&notifierMutex);
        for (tsdPtr = waitingListPtr; tsdPtr; tsdPtr = tsdPtr->nextPtr) {
	    found = 0;

	    /* This is not quite strictly portable, but should work
	       many places. */
	    for (j=0; j<3; j++) {
		for (i = 0;
		     i < (int)(sizeof(checkMasks[j].fds_bits)
			 / sizeof(checkMasks[j].fds_bits[0]));
		     i++) {
		    readyMasks[j].fds_bits[i] 
			= tsdPtr->masks[j].fds_bits[i]
			  & tsdPtr->checkMasks[j].fds_bits[i];
		    found |= readyMasks[j].fds_bits[i];
		}
	    }
            if (found || (tsdPtr->pollState & POLL_DONE)) {
                tsdPtr->eventReady = 1;
		Tcl_ConditionNotify(&tsdPtr->waitCV);
		if (tsdPtr->onList) {
		    /*
		     * Remove the ThreadSpecificData structure of this
		     * thread from the waiting list. This prevents us from
		     * continuously spining on select until the other
		     * threads runs and services the file event.
		     */
	    
		    if (tsdPtr->prevPtr) {
			tsdPtr->prevPtr->nextPtr = tsdPtr->nextPtr;
		    } else {
			waitingListPtr = tsdPtr->nextPtr;
		    }
		    if (tsdPtr->nextPtr) {
			tsdPtr->nextPtr->prevPtr = tsdPtr->prevPtr;
		    }
		    tsdPtr->nextPtr = tsdPtr->prevPtr = NULL;
		    tsdPtr->onList = 0;
		    tsdPtr->pollState = 0;
		}
            }
        }
	Tcl_MutexUnlock(&notifierMutex);
	
	/*
	 * Consume the next byte from the notifier pipe if the pipe was
	 * readable.  Note that there may be multiple bytes pending, but
	 * to avoid a race condition we only read one at a time.
	 */

	if (FD_ISSET(receivePipe, &masks[0])) {
	    i = read(receivePipe, buf, 1);

	    if ((i == 0) || ((i == 1) && (buf[0] == 'q'))) {
		/*
		 * Someone closed the write end of the pipe or sent us a
		 * Quit message [Bug: 4139] and then closed the write end
		 * of the pipe so we need to shut down the notifier thread.
		 */

		break;
	    }
	}
    }

    /*
     * Clean up the read end of the pipe and signal any threads waiting on
     * termination of the notifier thread.
     */

    close(receivePipe);
    Tcl_MutexLock(&notifierMutex);
    triggerPipe = -1;
    Tcl_ConditionNotify(&notifierCV);
    Tcl_MutexUnlock(&notifierMutex);
}
#endif


