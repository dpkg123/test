/* 
 * tkAtom.c --
 *
 *	This file manages a cache of X Atoms in order to avoid
 *	interactions with the X server.  It's much like the Xmu
 *	routines, except it has a cleaner interface (caller
 *	doesn't have to provide permanent storage for atom names,
 *	for example).
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tkAtom.c,v 1.7.6.1 2000/05/04 21:26:22 spolk Exp $
 */

#include "tkPort.h"
#include "tkInt.h"

/*
 * The following are a list of the predefined atom strings.
 * They should match those found in xatom.h
 */

static char * atomNameArray[] = {
    "PRIMARY",		"SECONDARY",		"ARC",
    "ATOM",		"BITMAP",		"CARDINAL",
    "COLORMAP",		"CURSOR",		"CUT_BUFFER0",
    "CUT_BUFFER1",	"CUT_BUFFER2",		"CUT_BUFFER3",
    "CUT_BUFFER4",	"CUT_BUFFER5",		"CUT_BUFFER6",
    "CUT_BUFFER7",	"DRAWABLE",		"FONT",
    "INTEGER",		"PIXMAP",		"POINT",
    "RECTANGLE",	"RESOURCE_MANAGER",	"RGB_COLOR_MAP",
    "RGB_BEST_MAP",	"RGB_BLUE_MAP",		"RGB_DEFAULT_MAP",
    "RGB_GRAY_MAP",	"RGB_GREEN_MAP",	"RGB_RED_MAP",
    "STRING",		"VISUALID",		"WINDOW",
    "WM_COMMAND",	"WM_HINTS",		"WM_CLIENT_MACHINE",
    "WM_ICON_NAME",	"WM_ICON_SIZE",		"WM_NAME",
    "WM_NORMAL_HINTS",	"WM_SIZE_HINTS",	"WM_ZOOM_HINTS",
    "MIN_SPACE",	"NORM_SPACE",		"MAX_SPACE",
    "END_SPACE",	"SUPERSCRIPT_X",	"SUPERSCRIPT_Y",
    "SUBSCRIPT_X",	"SUBSCRIPT_Y",		"UNDERLINE_POSITION",
    "UNDERLINE_THICKNESS", "STRIKEOUT_ASCENT",	"STRIKEOUT_DESCENT",
    "ITALIC_ANGLE",	"X_HEIGHT",		"QUAD_WIDTH",
    "WEIGHT",		"POINT_SIZE",		"RESOLUTION",
    "COPYRIGHT",	"NOTICE",		"FONT_NAME",
    "FAMILY_NAME",	"FULL_NAME",		"CAP_HEIGHT",
    "WM_CLASS",		"WM_TRANSIENT_FOR",
    (char *) NULL
};

/*
 * Forward references to procedures defined in this file:
 */

static void	AtomInit _ANSI_ARGS_((TkDisplay *dispPtr));

/*
 *--------------------------------------------------------------
 *
 * Tk_InternAtom --
 *
 *	Given a string, produce the equivalent X atom.  This
 *	procedure is equivalent to XInternAtom, except that it
 *	keeps a local cache of atoms.  Once a name is known,
 *	the server need not be contacted again for that name.
 *
 * Results:
 *	The return value is the Atom corresponding to name.
 *
 * Side effects:
 *	A new entry may be added to the local atom cache.
 *
 *--------------------------------------------------------------
 */

Atom
Tk_InternAtom(tkwin, name)
    Tk_Window tkwin;		/* Window token;  map name to atom
				 * for this window's display. */
    char *name;			/* Name to turn into atom. */
{
    register TkDisplay *dispPtr;
    register Tcl_HashEntry *hPtr;
    int new;

    dispPtr = ((TkWindow *) tkwin)->dispPtr;
    if (!dispPtr->atomInit) {
	AtomInit(dispPtr);
    }

    hPtr = Tcl_CreateHashEntry(&dispPtr->nameTable, name, &new);
    if (new) {
	Tcl_HashEntry *hPtr2;
	Atom atom;

	atom = XInternAtom(dispPtr->display, name, False);
	Tcl_SetHashValue(hPtr, atom);
	hPtr2 = Tcl_CreateHashEntry(&dispPtr->atomTable, (char *) atom,
		&new);
	Tcl_SetHashValue(hPtr2, Tcl_GetHashKey(&dispPtr->nameTable, hPtr));
    }
    return (Atom) Tcl_GetHashValue(hPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GetAtomName --
 *
 *	This procedure is equivalent to XGetAtomName except that
 *	it uses the local atom cache to avoid contacting the
 *	server.
 *
 * Results:
 *	The return value is a character string corresponding to
 *	the atom given by "atom".  This string's storage space
 *	is static:  it need not be freed by the caller, and should
 *	not be modified by the caller.  If "atom" doesn't exist
 *	on tkwin's display, then the string "?bad atom?" is returned.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Tk_GetAtomName(tkwin, atom)
    Tk_Window tkwin;		/* Window token;  map atom to name
				 * relative to this window's
				 * display. */
    Atom atom;			/* Atom whose name is wanted. */
{
    register TkDisplay *dispPtr;
    register Tcl_HashEntry *hPtr;

    dispPtr = ((TkWindow *) tkwin)->dispPtr;
    if (!dispPtr->atomInit) {
	AtomInit(dispPtr);
    }

    hPtr = Tcl_FindHashEntry(&dispPtr->atomTable, (char *) atom);
    if (hPtr == NULL) {
	char *name;
	Tk_ErrorHandler handler;
	int new, mustFree;

	handler= Tk_CreateErrorHandler(dispPtr->display, BadAtom,
		-1, -1, (Tk_ErrorProc *) NULL, (ClientData) NULL);
	name = XGetAtomName(dispPtr->display, atom);
	mustFree = 1;
	if (name == NULL) {
	    name = "?bad atom?";
	    mustFree = 0;
	}
	Tk_DeleteErrorHandler(handler);
	hPtr = Tcl_CreateHashEntry(&dispPtr->nameTable, (char *) name,
		&new);
	Tcl_SetHashValue(hPtr, atom);
	if (mustFree) {
	    XFree(name);
	}
	name = Tcl_GetHashKey(&dispPtr->nameTable, hPtr);
	hPtr = Tcl_CreateHashEntry(&dispPtr->atomTable, (char *) atom,
		&new);
	Tcl_SetHashValue(hPtr, name);
    }
    return (char *) Tcl_GetHashValue(hPtr);
}

/*
 *--------------------------------------------------------------
 *
 * AtomInit --
 *
 *	Initialize atom-related information for a display.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tables get initialized, etc. etc..
 *
 *--------------------------------------------------------------
 */

static void
AtomInit(dispPtr)
    register TkDisplay *dispPtr;	/* Display to initialize. */
{
    Tcl_HashEntry *hPtr;
    Atom atom;

    dispPtr->atomInit = 1;
    Tcl_InitHashTable(&dispPtr->nameTable, TCL_STRING_KEYS);
    Tcl_InitHashTable(&dispPtr->atomTable, TCL_ONE_WORD_KEYS);

    for (atom = 1; atom <= XA_LAST_PREDEFINED; atom++) {
	hPtr = Tcl_FindHashEntry(&dispPtr->atomTable, (char *) atom);
	if (hPtr == NULL) {
	    char *name;
	    int new;

	    name = atomNameArray[atom - 1];
	    hPtr = Tcl_CreateHashEntry(&dispPtr->nameTable, (char *) name,
		&new);
	    Tcl_SetHashValue(hPtr, atom);
	    name = Tcl_GetHashKey(&dispPtr->nameTable, hPtr);
	    hPtr = Tcl_CreateHashEntry(&dispPtr->atomTable, (char *) atom,
		&new);
	    Tcl_SetHashValue(hPtr, name);
	}
    }
}

