/* 
 * tclTimer.c --
 *
 *	This file provides timer event management facilities for Tcl,
 *	including the "after" command.
 *
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclTimer.c,v 1.6.8.1 2000/04/06 22:38:29 spolk Exp $
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * For each timer callback that's pending there is one record of the following
 * type.  The normal handlers (created by Tcl_CreateTimerHandler) are chained
 * together in a list sorted by time (earliest event first).
 */

typedef struct TimerHandler {
    Tcl_Time time;			/* When timer is to fire. */
    Tcl_TimerProc *proc;		/* Procedure to call. */
    ClientData clientData;		/* Argument to pass to proc. */
    Tcl_TimerToken token;		/* Identifies handler so it can be
					 * deleted. */
    struct TimerHandler *nextPtr;	/* Next event in queue, or NULL for
					 * end of queue. */
} TimerHandler;

/*
 * The data structure below is used by the "after" command to remember
 * the command to be executed later.  All of the pending "after" commands
 * for an interpreter are linked together in a list.
 */

typedef struct AfterInfo {
    struct AfterAssocData *assocPtr;
				/* Pointer to the "tclAfter" assocData for
				 * the interp in which command will be
				 * executed. */
    Tcl_Obj *commandPtr;	/* Command to execute. */
    int id;			/* Integer identifier for command;  used to
				 * cancel it. */
    Tcl_TimerToken token;	/* Used to cancel the "after" command.  NULL
				 * means that the command is run as an
				 * idle handler rather than as a timer
				 * handler.  NULL means this is an "after
				 * idle" handler rather than a
                                 * timer handler. */
    struct AfterInfo *nextPtr;	/* Next in list of all "after" commands for
				 * this interpreter. */
} AfterInfo;

/*
 * One of the following structures is associated with each interpreter
 * for which an "after" command has ever been invoked.  A pointer to
 * this structure is stored in the AssocData for the "tclAfter" key.
 */

typedef struct AfterAssocData {
    Tcl_Interp *interp;		/* The interpreter for which this data is
				 * registered. */
    AfterInfo *firstAfterPtr;	/* First in list of all "after" commands
				 * still pending for this interpreter, or
				 * NULL if none. */
} AfterAssocData;

/*
 * There is one of the following structures for each of the
 * handlers declared in a call to Tcl_DoWhenIdle.  All of the
 * currently-active handlers are linked together into a list.
 */

typedef struct IdleHandler {
    Tcl_IdleProc (*proc);	/* Procedure to call. */
    ClientData clientData;	/* Value to pass to proc. */
    int generation;		/* Used to distinguish older handlers from
				 * recently-created ones. */
    struct IdleHandler *nextPtr;/* Next in list of active handlers. */
} IdleHandler;

/*
 * The timer and idle queues are per-thread because they are associated
 * with the notifier, which is also per-thread.
 *
 * All static variables used in this file are collected into a single
 * instance of the following structure.  For multi-threaded implementations,
 * there is one instance of this structure for each thread.
 *
 * Notice that different structures with the same name appear in other
 * files.  The structure defined below is used in this file only.
 */

typedef struct ThreadSpecificData {
    TimerHandler *firstTimerHandlerPtr;	/* First event in queue. */
    int lastTimerId;		/* Timer identifier of most recently
				 * created timer. */
    int timerPending;		/* 1 if a timer event is in the queue. */
    IdleHandler *idleList;	/* First in list of all idle handlers. */
    IdleHandler *lastIdlePtr;	/* Last in list (or NULL for empty list). */
    int idleGeneration;		/* Used to fill in the "generation" fields
				 * of IdleHandler structures.  Increments
				 * each time Tcl_DoOneEvent starts calling
				 * idle handlers, so that all old handlers
				 * can be called without calling any of the
				 * new ones created by old ones. */
    int afterId;		/* For unique identifiers of after events. */
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

/*
 * Prototypes for procedures referenced only in this file:
 */

static void		AfterCleanupProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp));
static void		AfterProc _ANSI_ARGS_((ClientData clientData));
static void		FreeAfterPtr _ANSI_ARGS_((AfterInfo *afterPtr));
static AfterInfo *	GetAfterEvent _ANSI_ARGS_((AfterAssocData *assocPtr,
			    Tcl_Obj *commandPtr));
static ThreadSpecificData *InitTimer _ANSI_ARGS_((void));
static void		TimerExitProc _ANSI_ARGS_((ClientData clientData));
static int		TimerHandlerEventProc _ANSI_ARGS_((Tcl_Event *evPtr,
			    int flags));
static void		TimerCheckProc _ANSI_ARGS_((ClientData clientData,
			    int flags));
static void		TimerSetupProc _ANSI_ARGS_((ClientData clientData,
			    int flags));

/*
 *----------------------------------------------------------------------
 *
 * InitTimer --
 *
 *	This function initializes the timer module.
 *
 * Results:
 *	A pointer to the thread specific data.
 *
 * Side effects:
 *	Registers the idle and timer event sources.
 *
 *----------------------------------------------------------------------
 */

static ThreadSpecificData *
InitTimer()
{
    ThreadSpecificData *tsdPtr = 
	(ThreadSpecificData *) TclThreadDataKeyGet(&dataKey);

    if (tsdPtr == NULL) {
	tsdPtr = TCL_TSD_INIT(&dataKey);
	Tcl_CreateEventSource(TimerSetupProc, TimerCheckProc, NULL);
	Tcl_CreateThreadExitHandler(TimerExitProc, NULL);
    }
    return tsdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TimerExitProc --
 *
 *	This function is call at exit or unload time to remove the
 *	timer and idle event sources.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes the timer and idle event sources.
 *
 *----------------------------------------------------------------------
 */

static void
TimerExitProc(clientData)
    ClientData clientData;	/* Not used. */
{
    Tcl_DeleteEventSource(TimerSetupProc, TimerCheckProc, NULL);
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_CreateTimerHandler --
 *
 *	Arrange for a given procedure to be invoked at a particular
 *	time in the future.
 *
 * Results:
 *	The return value is a token for the timer event, which
 *	may be used to delete the event before it fires.
 *
 * Side effects:
 *	When milliseconds have elapsed, proc will be invoked
 *	exactly once.
 *
 *--------------------------------------------------------------
 */

Tcl_TimerToken
Tcl_CreateTimerHandler(milliseconds, proc, clientData)
    int milliseconds;		/* How many milliseconds to wait
				 * before invoking proc. */
    Tcl_TimerProc *proc;	/* Procedure to invoke. */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    register TimerHandler *timerHandlerPtr, *tPtr2, *prevPtr;
    Tcl_Time time;
    ThreadSpecificData *tsdPtr;

    tsdPtr = InitTimer();

    timerHandlerPtr = (TimerHandler *) ckalloc(sizeof(TimerHandler));

    /*
     * Compute when the event should fire.
     */

    TclpGetTime(&time);
    timerHandlerPtr->time.sec = time.sec + milliseconds/1000;
    timerHandlerPtr->time.usec = time.usec + (milliseconds%1000)*1000;
    if (timerHandlerPtr->time.usec >= 1000000) {
	timerHandlerPtr->time.usec -= 1000000;
	timerHandlerPtr->time.sec += 1;
    }

    /*
     * Fill in other fields for the event.
     */

    timerHandlerPtr->proc = proc;
    timerHandlerPtr->clientData = clientData;
    tsdPtr->lastTimerId++;
    timerHandlerPtr->token = (Tcl_TimerToken) tsdPtr->lastTimerId;

    /*
     * Add the event to the queue in the correct position
     * (ordered by event firing time).
     */

    for (tPtr2 = tsdPtr->firstTimerHandlerPtr, prevPtr = NULL; tPtr2 != NULL;
	    prevPtr = tPtr2, tPtr2 = tPtr2->nextPtr) {
	if ((tPtr2->time.sec > timerHandlerPtr->time.sec)
		|| ((tPtr2->time.sec == timerHandlerPtr->time.sec)
		&& (tPtr2->time.usec > timerHandlerPtr->time.usec))) {
	    break;
	}
    }
    timerHandlerPtr->nextPtr = tPtr2;
    if (prevPtr == NULL) {
	tsdPtr->firstTimerHandlerPtr = timerHandlerPtr;
    } else {
	prevPtr->nextPtr = timerHandlerPtr;
    }

    TimerSetupProc(NULL, TCL_ALL_EVENTS);

    return timerHandlerPtr->token;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_DeleteTimerHandler --
 *
 *	Delete a previously-registered timer handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroy the timer callback identified by TimerToken,
 *	so that its associated procedure will not be called.
 *	If the callback has already fired, or if the given
 *	token doesn't exist, then nothing happens.
 *
 *--------------------------------------------------------------
 */

void
Tcl_DeleteTimerHandler(token)
    Tcl_TimerToken token;	/* Result previously returned by
				 * Tcl_DeleteTimerHandler. */
{
    register TimerHandler *timerHandlerPtr, *prevPtr;
    ThreadSpecificData *tsdPtr;

    tsdPtr = InitTimer();
    for (timerHandlerPtr = tsdPtr->firstTimerHandlerPtr, prevPtr = NULL;
	    timerHandlerPtr != NULL; prevPtr = timerHandlerPtr,
	    timerHandlerPtr = timerHandlerPtr->nextPtr) {
	if (timerHandlerPtr->token != token) {
	    continue;
	}
	if (prevPtr == NULL) {
	    tsdPtr->firstTimerHandlerPtr = timerHandlerPtr->nextPtr;
	} else {
	    prevPtr->nextPtr = timerHandlerPtr->nextPtr;
	}
	ckfree((char *) timerHandlerPtr);
	return;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TimerSetupProc --
 *
 *	This function is called by Tcl_DoOneEvent to setup the timer
 *	event source for before blocking.  This routine checks both the
 *	idle and after timer lists.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May update the maximum notifier block time.
 *
 *----------------------------------------------------------------------
 */

static void
TimerSetupProc(data, flags)
    ClientData data;		/* Not used. */
    int flags;			/* Event flags as passed to Tcl_DoOneEvent. */
{
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = InitTimer();

    if (((flags & TCL_IDLE_EVENTS) && tsdPtr->idleList)
	    || ((flags & TCL_TIMER_EVENTS) && tsdPtr->timerPending)) {
	/*
	 * There is an idle handler or a pending timer event, so just poll.
	 */

	blockTime.sec = 0;
	blockTime.usec = 0;

    } else if ((flags & TCL_TIMER_EVENTS) && tsdPtr->firstTimerHandlerPtr) {
	/*
	 * Compute the timeout for the next timer on the list.
	 */

	TclpGetTime(&blockTime);
	blockTime.sec = tsdPtr->firstTimerHandlerPtr->time.sec - blockTime.sec;
	blockTime.usec = tsdPtr->firstTimerHandlerPtr->time.usec -
		blockTime.usec;
	if (blockTime.usec < 0) {
	    blockTime.sec -= 1;
	    blockTime.usec += 1000000;
	}
	if (blockTime.sec < 0) {
	    blockTime.sec = 0;
	    blockTime.usec = 0;
	}
    } else {
	return;
    }
	
    Tcl_SetMaxBlockTime(&blockTime);
}

/*
 *----------------------------------------------------------------------
 *
 * TimerCheckProc --
 *
 *	This function is called by Tcl_DoOneEvent to check the timer
 *	event source for events.  This routine checks both the
 *	idle and after timer lists.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May queue an event and update the maximum notifier block time.
 *
 *----------------------------------------------------------------------
 */

static void
TimerCheckProc(data, flags)
    ClientData data;		/* Not used. */
    int flags;			/* Event flags as passed to Tcl_DoOneEvent. */
{
    Tcl_Event *timerEvPtr;
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = InitTimer();

    if ((flags & TCL_TIMER_EVENTS) && tsdPtr->firstTimerHandlerPtr) {
	/*
	 * Compute the timeout for the next timer on the list.
	 */

	TclpGetTime(&blockTime);
	blockTime.sec = tsdPtr->firstTimerHandlerPtr->time.sec - blockTime.sec;
	blockTime.usec = tsdPtr->firstTimerHandlerPtr->time.usec -
		blockTime.usec;
	if (blockTime.usec < 0) {
	    blockTime.sec -= 1;
	    blockTime.usec += 1000000;
	}
	if (blockTime.sec < 0) {
	    blockTime.sec = 0;
	    blockTime.usec = 0;
	}

	/*
	 * If the first timer has expired, stick an event on the queue.
	 */

	if (blockTime.sec == 0 && blockTime.usec == 0 &&
		!tsdPtr->timerPending) {
	    tsdPtr->timerPending = 1;
	    timerEvPtr = (Tcl_Event *) ckalloc(sizeof(Tcl_Event));
	    timerEvPtr->proc = TimerHandlerEventProc;
	    Tcl_QueueEvent(timerEvPtr, TCL_QUEUE_TAIL);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TimerHandlerEventProc --
 *
 *	This procedure is called by Tcl_ServiceEvent when a timer event
 *	reaches the front of the event queue.  This procedure handles
 *	the event by invoking the callbacks for all timers that are
 *	ready.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed
 *	from the queue.  Returns 0 if the event was not handled, meaning
 *	it should stay on the queue.  The only time the event isn't
 *	handled is if the TCL_TIMER_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the timer handler callback procedures do.
 *
 *----------------------------------------------------------------------
 */

static int
TimerHandlerEventProc(evPtr, flags)
    Tcl_Event *evPtr;		/* Event to service. */
    int flags;			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    TimerHandler *timerHandlerPtr, **nextPtrPtr;
    Tcl_Time time;
    int currentTimerId;
    ThreadSpecificData *tsdPtr = InitTimer();

    /*
     * Do nothing if timers aren't enabled.  This leaves the event on the
     * queue, so we will get to it as soon as ServiceEvents() is called
     * with timers enabled.
     */

    if (!(flags & TCL_TIMER_EVENTS)) {
	return 0;
    }

    /*
     * The code below is trickier than it may look, for the following
     * reasons:
     *
     * 1. New handlers can get added to the list while the current
     *    one is being processed.  If new ones get added, we don't
     *    want to process them during this pass through the list to avoid
     *	  starving other event sources.  This is implemented using the
     *	  token number in the handler:  new handlers will have a
     *    newer token than any of the ones currently on the list.
     * 2. The handler can call Tcl_DoOneEvent, so we have to remove
     *    the handler from the list before calling it. Otherwise an
     *    infinite loop could result.
     * 3. Tcl_DeleteTimerHandler can be called to remove an element from
     *    the list while a handler is executing, so the list could
     *    change structure during the call.
     * 4. Because we only fetch the current time before entering the loop,
     *    the only way a new timer will even be considered runnable is if
     *	  its expiration time is within the same millisecond as the
     *	  current time.  This is fairly likely on Windows, since it has
     *	  a course granularity clock.  Since timers are placed
     *	  on the queue in time order with the most recently created
     *    handler appearing after earlier ones with the same expiration
     *	  time, we don't have to worry about newer generation timers
     *	  appearing before later ones.
     */

    tsdPtr->timerPending = 0;
    currentTimerId = tsdPtr->lastTimerId;
    TclpGetTime(&time);
    while (1) {
	nextPtrPtr = &tsdPtr->firstTimerHandlerPtr;
	timerHandlerPtr = tsdPtr->firstTimerHandlerPtr;
	if (timerHandlerPtr == NULL) {
	    break;
	}
	    
	if ((timerHandlerPtr->time.sec > time.sec)
		|| ((timerHandlerPtr->time.sec == time.sec)
			&& (timerHandlerPtr->time.usec > time.usec))) {
	    break;
	}

	/*
	 * Bail out if the next timer is of a newer generation.
	 */

	if ((currentTimerId - (int)timerHandlerPtr->token) < 0) {
	    break;
	}

	/*
	 * Remove the handler from the queue before invoking it,
	 * to avoid potential reentrancy problems.
	 */

	(*nextPtrPtr) = timerHandlerPtr->nextPtr;
	(*timerHandlerPtr->proc)(timerHandlerPtr->clientData);
	ckfree((char *) timerHandlerPtr);
    }
    TimerSetupProc(NULL, TCL_TIMER_EVENTS);
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_DoWhenIdle --
 *
 *	Arrange for proc to be invoked the next time the system is
 *	idle (i.e., just before the next time that Tcl_DoOneEvent
 *	would have to wait for something to happen).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Proc will eventually be called, with clientData as argument.
 *	See the manual entry for details.
 *
 *--------------------------------------------------------------
 */

void
Tcl_DoWhenIdle(proc, clientData)
    Tcl_IdleProc *proc;		/* Procedure to invoke. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
{
    register IdleHandler *idlePtr;
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = InitTimer();

    idlePtr = (IdleHandler *) ckalloc(sizeof(IdleHandler));
    idlePtr->proc = proc;
    idlePtr->clientData = clientData;
    idlePtr->generation = tsdPtr->idleGeneration;
    idlePtr->nextPtr = NULL;
    if (tsdPtr->lastIdlePtr == NULL) {
	tsdPtr->idleList = idlePtr;
    } else {
	tsdPtr->lastIdlePtr->nextPtr = idlePtr;
    }
    tsdPtr->lastIdlePtr = idlePtr;

    blockTime.sec = 0;
    blockTime.usec = 0;
    Tcl_SetMaxBlockTime(&blockTime);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CancelIdleCall --
 *
 *	If there are any when-idle calls requested to a given procedure
 *	with given clientData, cancel all of them.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the proc/clientData combination were on the when-idle list,
 *	they are removed so that they will never be called.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CancelIdleCall(proc, clientData)
    Tcl_IdleProc *proc;		/* Procedure that was previously registered. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
{
    register IdleHandler *idlePtr, *prevPtr;
    IdleHandler *nextPtr;
    ThreadSpecificData *tsdPtr = InitTimer();

    for (prevPtr = NULL, idlePtr = tsdPtr->idleList; idlePtr != NULL;
	    prevPtr = idlePtr, idlePtr = idlePtr->nextPtr) {
	while ((idlePtr->proc == proc)
		&& (idlePtr->clientData == clientData)) {
	    nextPtr = idlePtr->nextPtr;
	    ckfree((char *) idlePtr);
	    idlePtr = nextPtr;
	    if (prevPtr == NULL) {
		tsdPtr->idleList = idlePtr;
	    } else {
		prevPtr->nextPtr = idlePtr;
	    }
	    if (idlePtr == NULL) {
		tsdPtr->lastIdlePtr = prevPtr;
		return;
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclServiceIdle --
 *
 *	This procedure is invoked by the notifier when it becomes
 *	idle.  It will invoke all idle handlers that are present at
 *	the time the call is invoked, but not those added during idle
 *	processing.
 *
 * Results:
 *	The return value is 1 if TclServiceIdle found something to
 *	do, otherwise return value is 0.
 *
 * Side effects:
 *	Invokes all pending idle handlers.
 *
 *----------------------------------------------------------------------
 */

int
TclServiceIdle()
{
    IdleHandler *idlePtr;
    int oldGeneration;
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = InitTimer();

    if (tsdPtr->idleList == NULL) {
	return 0;
    }

    oldGeneration = tsdPtr->idleGeneration;
    tsdPtr->idleGeneration++;

    /*
     * The code below is trickier than it may look, for the following
     * reasons:
     *
     * 1. New handlers can get added to the list while the current
     *    one is being processed.  If new ones get added, we don't
     *    want to process them during this pass through the list (want
     *    to check for other work to do first).  This is implemented
     *    using the generation number in the handler:  new handlers
     *    will have a different generation than any of the ones currently
     *    on the list.
     * 2. The handler can call Tcl_DoOneEvent, so we have to remove
     *    the handler from the list before calling it. Otherwise an
     *    infinite loop could result.
     * 3. Tcl_CancelIdleCall can be called to remove an element from
     *    the list while a handler is executing, so the list could
     *    change structure during the call.
     */

    for (idlePtr = tsdPtr->idleList;
	    ((idlePtr != NULL)
		    && ((oldGeneration - idlePtr->generation) >= 0));
	    idlePtr = tsdPtr->idleList) {
	tsdPtr->idleList = idlePtr->nextPtr;
	if (tsdPtr->idleList == NULL) {
	    tsdPtr->lastIdlePtr = NULL;
	}
	(*idlePtr->proc)(idlePtr->clientData);
	ckfree((char *) idlePtr);
    }
    if (tsdPtr->idleList) {
	blockTime.sec = 0;
	blockTime.usec = 0;
	Tcl_SetMaxBlockTime(&blockTime);
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AfterObjCmd --
 *
 *	This procedure is invoked to process the "after" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_AfterObjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Points to the "tclAfter" assocData for
				 * this interpreter, or NULL if the assocData
				 * hasn't been created yet.*/
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int ms;
    AfterInfo *afterPtr;
    AfterAssocData *assocPtr = (AfterAssocData *) clientData;
    Tcl_CmdInfo cmdInfo;
    int length;
    char *argString;
    int index;
    char buf[16 + TCL_INTEGER_SPACE];
    static char *afterSubCmds[] = {"cancel", "idle", "info", (char *) NULL};
    enum afterSubCmds {AFTER_CANCEL, AFTER_IDLE, AFTER_INFO};
    ThreadSpecificData *tsdPtr = InitTimer();

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    /*
     * Create the "after" information associated for this interpreter,
     * if it doesn't already exist.  Associate it with the command too,
     * so that it will be passed in as the ClientData argument in the
     * future.
     */

    if (assocPtr == NULL) {
	assocPtr = (AfterAssocData *) ckalloc(sizeof(AfterAssocData));
	assocPtr->interp = interp;
	assocPtr->firstAfterPtr = NULL;
	Tcl_SetAssocData(interp, "tclAfter", AfterCleanupProc,
		(ClientData) assocPtr);
	cmdInfo.proc = NULL;
	cmdInfo.clientData = (ClientData) NULL;
	cmdInfo.objProc = Tcl_AfterObjCmd;
	cmdInfo.objClientData = (ClientData) assocPtr;
	cmdInfo.deleteProc = NULL;
	cmdInfo.deleteData = (ClientData) assocPtr;
	Tcl_SetCommandInfo(interp, Tcl_GetStringFromObj(objv[0], &length),
		&cmdInfo);
    }

    /*
     * First lets see if the command was passed a number as the first argument.
     */

    if (objv[1]->typePtr == &tclIntType) {
	ms = (int) objv[1]->internalRep.longValue;
	goto processInteger;
    }
    argString = Tcl_GetStringFromObj(objv[1], &length);
    if (isdigit(UCHAR(argString[0]))) {	/* INTL: digit */
	if (Tcl_GetIntFromObj(interp, objv[1], &ms) != TCL_OK) {
	    return TCL_ERROR;
	}
processInteger:
	if (ms < 0) {
	    ms = 0;
	}
	if (objc == 2) {
	    Tcl_Sleep(ms);
	    return TCL_OK;
	}
	afterPtr = (AfterInfo *) ckalloc((unsigned) (sizeof(AfterInfo)));
	afterPtr->assocPtr = assocPtr;
	if (objc == 3) {
	    afterPtr->commandPtr = objv[2];
	} else {
 	    afterPtr->commandPtr = Tcl_ConcatObj(objc-2, objv+2);
	}
	Tcl_IncrRefCount(afterPtr->commandPtr);
	/*
	 * The variable below is used to generate unique identifiers for
	 * after commands.  This id can wrap around, which can potentially
	 * cause problems.  However, there are not likely to be problems
	 * in practice, because after commands can only be requested to
	 * about a month in the future, and wrap-around is unlikely to
	 * occur in less than about 1-10 years.  Thus it's unlikely that
	 * any old ids will still be around when wrap-around occurs.
	 */
	afterPtr->id = tsdPtr->afterId;
	tsdPtr->afterId += 1;
	afterPtr->token = Tcl_CreateTimerHandler(ms, AfterProc,
		(ClientData) afterPtr);
	afterPtr->nextPtr = assocPtr->firstAfterPtr;
	assocPtr->firstAfterPtr = afterPtr;
	sprintf(buf, "after#%d", afterPtr->id);
	Tcl_AppendResult(interp, buf, (char *) NULL);
	return TCL_OK;
    }

    /*
     * If it's not a number it must be a subcommand.
     */

    if (Tcl_GetIndexFromObj(NULL, objv[1], afterSubCmds, "argument",
            0, &index) != TCL_OK) {
	Tcl_AppendResult(interp, "bad argument \"", argString,
		"\": must be cancel, idle, info, or a number",
		(char *) NULL);
	return TCL_ERROR;
    }
    switch ((enum afterSubCmds) index) {
        case AFTER_CANCEL: {
	    Tcl_Obj *commandPtr;
	    char *command, *tempCommand;
	    int tempLength;

	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "id|command");
		return TCL_ERROR;
	    }
	    if (objc == 3) {
		commandPtr = objv[2];
	    } else {
		commandPtr = Tcl_ConcatObj(objc-2, objv+2);;
	    }
	    command = Tcl_GetStringFromObj(commandPtr, &length);
	    for (afterPtr = assocPtr->firstAfterPtr;  afterPtr != NULL;
		    afterPtr = afterPtr->nextPtr) {
		tempCommand = Tcl_GetStringFromObj(afterPtr->commandPtr,
			&tempLength);
		if ((length == tempLength)
		        && (memcmp((void*) command, (void*) tempCommand,
			        (unsigned) length) == 0)) {
		    break;
		}
	    }
	    if (afterPtr == NULL) {
		afterPtr = GetAfterEvent(assocPtr, commandPtr);
	    }
	    if (objc != 3) {
		Tcl_DecrRefCount(commandPtr);
	    }
	    if (afterPtr != NULL) {
		if (afterPtr->token != NULL) {
		    Tcl_DeleteTimerHandler(afterPtr->token);
		} else {
		    Tcl_CancelIdleCall(AfterProc, (ClientData) afterPtr);
		}
		FreeAfterPtr(afterPtr);
	    }
	    break;
	}
	case AFTER_IDLE:
	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "script script ...");
		return TCL_ERROR;
	    }
	    afterPtr = (AfterInfo *) ckalloc((unsigned) (sizeof(AfterInfo)));
	    afterPtr->assocPtr = assocPtr;
	    if (objc == 3) {
 		afterPtr->commandPtr = objv[2];
	    } else {
		afterPtr->commandPtr = Tcl_ConcatObj(objc-2, objv+2);
	    }
	    Tcl_IncrRefCount(afterPtr->commandPtr);
	    afterPtr->id = tsdPtr->afterId;
	    tsdPtr->afterId += 1;
	    afterPtr->token = NULL;
	    afterPtr->nextPtr = assocPtr->firstAfterPtr;
	    assocPtr->firstAfterPtr = afterPtr;
	    Tcl_DoWhenIdle(AfterProc, (ClientData) afterPtr);
	    sprintf(buf, "after#%d", afterPtr->id);
	    Tcl_AppendResult(interp, buf, (char *) NULL);
	    break;
	case AFTER_INFO: {
	    Tcl_Obj *resultListPtr;

	    if (objc == 2) {
		for (afterPtr = assocPtr->firstAfterPtr; afterPtr != NULL;
		     afterPtr = afterPtr->nextPtr) {
		    if (assocPtr->interp == interp) {
			sprintf(buf, "after#%d", afterPtr->id);
			Tcl_AppendElement(interp, buf);
		    }
		}
		return TCL_OK;
	    }
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "?id?");
		return TCL_ERROR;
	    }
	    afterPtr = GetAfterEvent(assocPtr, objv[2]);
	    if (afterPtr == NULL) {
		Tcl_AppendResult(interp, "event \"", Tcl_GetString(objv[2]),
			"\" doesn't exist", (char *) NULL);
		return TCL_ERROR;
	    }
	    resultListPtr = Tcl_GetObjResult(interp);
 	    Tcl_ListObjAppendElement(interp, resultListPtr, afterPtr->commandPtr);
 	    Tcl_ListObjAppendElement(interp, resultListPtr, Tcl_NewStringObj(
 		(afterPtr->token == NULL) ? "idle" : "timer", -1));
	    Tcl_SetObjResult(interp, resultListPtr);
	    break;
	}
	default: {
	    panic("Tcl_AfterObjCmd: bad subcommand index to afterSubCmds");
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetAfterEvent --
 *
 *	This procedure parses an "after" id such as "after#4" and
 *	returns a pointer to the AfterInfo structure.
 *
 * Results:
 *	The return value is either a pointer to an AfterInfo structure,
 *	if one is found that corresponds to "cmdString" and is for interp,
 *	or NULL if no corresponding after event can be found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static AfterInfo *
GetAfterEvent(assocPtr, commandPtr)
    AfterAssocData *assocPtr;	/* Points to "after"-related information for
				 * this interpreter. */
    Tcl_Obj *commandPtr;
{
    char *cmdString;		/* Textual identifier for after event, such
				 * as "after#6". */
    AfterInfo *afterPtr;
    int id;
    char *end;

    cmdString = Tcl_GetString(commandPtr);
    if (strncmp(cmdString, "after#", 6) != 0) {
	return NULL;
    }
    cmdString += 6;
    id = strtoul(cmdString, &end, 10);
    if ((end == cmdString) || (*end != 0)) {
	return NULL;
    }
    for (afterPtr = assocPtr->firstAfterPtr; afterPtr != NULL;
	    afterPtr = afterPtr->nextPtr) {
	if (afterPtr->id == id) {
	    return afterPtr;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * AfterProc --
 *
 *	Timer callback to execute commands registered with the
 *	"after" command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Executes whatever command was specified.  If the command
 *	returns an error, then the command "bgerror" is invoked
 *	to process the error;  if bgerror fails then information
 *	about the error is output on stderr.
 *
 *----------------------------------------------------------------------
 */

static void
AfterProc(clientData)
    ClientData clientData;	/* Describes command to execute. */
{
    AfterInfo *afterPtr = (AfterInfo *) clientData;
    AfterAssocData *assocPtr = afterPtr->assocPtr;
    AfterInfo *prevPtr;
    int result;
    Tcl_Interp *interp;
    char *script;
    int numBytes;

    /*
     * First remove the callback from our list of callbacks;  otherwise
     * someone could delete the callback while it's being executed, which
     * could cause a core dump.
     */

    if (assocPtr->firstAfterPtr == afterPtr) {
	assocPtr->firstAfterPtr = afterPtr->nextPtr;
    } else {
	for (prevPtr = assocPtr->firstAfterPtr; prevPtr->nextPtr != afterPtr;
		prevPtr = prevPtr->nextPtr) {
	    /* Empty loop body. */
	}
	prevPtr->nextPtr = afterPtr->nextPtr;
    }

    /*
     * Execute the callback.
     */

    interp = assocPtr->interp;
    Tcl_Preserve((ClientData) interp);
    script = Tcl_GetStringFromObj(afterPtr->commandPtr, &numBytes);
    result = Tcl_EvalEx(interp, script, numBytes, TCL_EVAL_GLOBAL);
    if (result != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (\"after\" script)");
	Tcl_BackgroundError(interp);
    }
    Tcl_Release((ClientData) interp);
    
    /*
     * Free the memory for the callback.
     */

    Tcl_DecrRefCount(afterPtr->commandPtr);
    ckfree((char *) afterPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FreeAfterPtr --
 *
 *	This procedure removes an "after" command from the list of
 *	those that are pending and frees its resources.  This procedure
 *	does *not* cancel the timer handler;  if that's needed, the
 *	caller must do it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The memory associated with afterPtr is released.
 *
 *----------------------------------------------------------------------
 */

static void
FreeAfterPtr(afterPtr)
    AfterInfo *afterPtr;		/* Command to be deleted. */
{
    AfterInfo *prevPtr;
    AfterAssocData *assocPtr = afterPtr->assocPtr;

    if (assocPtr->firstAfterPtr == afterPtr) {
	assocPtr->firstAfterPtr = afterPtr->nextPtr;
    } else {
	for (prevPtr = assocPtr->firstAfterPtr; prevPtr->nextPtr != afterPtr;
		prevPtr = prevPtr->nextPtr) {
	    /* Empty loop body. */
	}
	prevPtr->nextPtr = afterPtr->nextPtr;
    }
    Tcl_DecrRefCount(afterPtr->commandPtr);
    ckfree((char *) afterPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * AfterCleanupProc --
 *
 *	This procedure is invoked whenever an interpreter is deleted
 *	to cleanup the AssocData for "tclAfter".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	After commands are removed.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
AfterCleanupProc(clientData, interp)
    ClientData clientData;	/* Points to AfterAssocData for the
				 * interpreter. */
    Tcl_Interp *interp;		/* Interpreter that is being deleted. */
{
    AfterAssocData *assocPtr = (AfterAssocData *) clientData;
    AfterInfo *afterPtr;

    while (assocPtr->firstAfterPtr != NULL) {
	afterPtr = assocPtr->firstAfterPtr;
	assocPtr->firstAfterPtr = afterPtr->nextPtr;
	if (afterPtr->token != NULL) {
	    Tcl_DeleteTimerHandler(afterPtr->token);
	} else {
	    Tcl_CancelIdleCall(AfterProc, (ClientData) afterPtr);
	}
	Tcl_DecrRefCount(afterPtr->commandPtr);
	ckfree((char *) afterPtr);
    }
    ckfree((char *) assocPtr);
}
