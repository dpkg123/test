/* 
 * pkge.c --
 *
 *	This file contains a simple Tcl package "pkge" that is intended
 *	for testing the Tcl dynamic loading facilities.  Its Init
 *	procedure returns an error in order to test how this is handled.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: pkge.c,v 1.6.8.2 2000/09/15 16:58:34 spolk Exp $
 */

#include "tcl.h"


/*
 *----------------------------------------------------------------------
 *
 * Pkge_Init --
 *
 *	This is a package initialization procedure, which is called
 *	by Tcl when this package is to be added to an interpreter.
 *
 * Results:
 *	Returns TCL_ERROR and leaves an error message in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Pkge_Init(interp)
    Tcl_Interp *interp;		/* Interpreter in which the package is
				 * to be made available. */
{
    static char script[] = "if 44 {open non_existent}";
    if (Tcl_InitStubs(interp, TCL_VERSION, 1) == NULL) {
	return TCL_ERROR;
    }
    return Tcl_Eval(interp, script);
}


