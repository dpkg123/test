/* 
 * tclHistory.c --
 *
 *	This module and the Tcl library file history.tcl together implement
 *	Tcl command history. Tcl_RecordAndEval(Obj) can be called to record
 *	commands ("events") before they are executed. Commands defined in
 *	history.tcl may be used to perform history substitutions.
 *
 * Copyright (c) 1990-1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclHistory.c,v 1.6.8.1 2000/04/06 22:38:28 spolk Exp $
 */

#include "tclInt.h"
#include "tclPort.h"


/*
 *----------------------------------------------------------------------
 *
 * Tcl_RecordAndEval --
 *
 *	This procedure adds its command argument to the current list of
 *	recorded events and then executes the command by calling
 *	Tcl_Eval.
 *
 * Results:
 *	The return value is a standard Tcl return value, the result of
 *	executing cmd.
 *
 * Side effects:
 *	The command is recorded and executed.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_RecordAndEval(interp, cmd, flags)
    Tcl_Interp *interp;		/* Token for interpreter in which command
				 * will be executed. */
    char *cmd;			/* Command to record. */
    int flags;			/* Additional flags.  TCL_NO_EVAL means
				 * only record: don't execute command.
				 * TCL_EVAL_GLOBAL means use Tcl_GlobalEval
				 * instead of Tcl_Eval. */
{
    register Tcl_Obj *cmdPtr;
    int length = strlen(cmd);
    int result;

    if (length > 0) {
	/*
	 * Call Tcl_RecordAndEvalObj to do the actual work.
	 */

	cmdPtr = Tcl_NewStringObj(cmd, length);
	Tcl_IncrRefCount(cmdPtr);
	result = Tcl_RecordAndEvalObj(interp, cmdPtr, flags);

	/*
	 * Move the interpreter's object result to the string result, 
	 * then reset the object result.
	 */

	Tcl_SetResult(interp, TclGetString(Tcl_GetObjResult(interp)),
	        TCL_VOLATILE);

	/*
	 * Discard the Tcl object created to hold the command.
	 */
	
	Tcl_DecrRefCount(cmdPtr);	
    } else {
	/*
	 * An empty string. Just reset the interpreter's result.
	 */

	Tcl_ResetResult(interp);
	result = TCL_OK;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RecordAndEvalObj --
 *
 *	This procedure adds the command held in its argument object to the
 *	current list of recorded events and then executes the command by
 *	calling Tcl_EvalObj.
 *
 * Results:
 *	The return value is a standard Tcl return value, the result of
 *	executing the command.
 *
 * Side effects:
 *	The command is recorded and executed.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_RecordAndEvalObj(interp, cmdPtr, flags)
    Tcl_Interp *interp;		/* Token for interpreter in which command
				 * will be executed. */
    Tcl_Obj *cmdPtr;		/* Points to object holding the command to
				 * record and execute. */
    int flags;			/* Additional flags. TCL_NO_EVAL means
				 * record only: don't execute the command.
				 * TCL_EVAL_GLOBAL means evaluate the
				 * script in global variable context instead
				 * of the current procedure. */
{
    int result;
    Tcl_Obj *list[3];
    register Tcl_Obj *objPtr;

    /*
     * Do recording by eval'ing a tcl history command: history add $cmd.
     */

    list[0] = Tcl_NewStringObj("history", -1);
    list[1] = Tcl_NewStringObj("add", -1);
    list[2] = cmdPtr;
    
    objPtr = Tcl_NewListObj(3, list);
    Tcl_IncrRefCount(objPtr);
    (void) Tcl_EvalObjEx(interp, objPtr, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(objPtr);

    /*
     * Execute the command.
     */

    result = TCL_OK;
    if (!(flags & TCL_NO_EVAL)) {
	result = Tcl_EvalObjEx(interp, cmdPtr, flags & TCL_EVAL_GLOBAL);
    }
    return result;
}
