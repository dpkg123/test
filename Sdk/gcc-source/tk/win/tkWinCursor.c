/* 
 * tkWinCursor.c --
 *
 *	This file contains Win32 specific cursor related routines.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tkWinCursor.c,v 1.7.6.1 2000/05/04 21:26:31 spolk Exp $
 */

#include "tkWinInt.h"

/*
 * The following data structure contains the system specific data
 * necessary to control Windows cursors.
 */

typedef struct {
    TkCursor info;		/* Generic cursor info used by tkCursor.c */
    HCURSOR winCursor;		/* Win32 cursor handle. */
    int system;			/* 1 if cursor is a system cursor, else 0. */
} TkWinCursor;

/*
 * The table below is used to map from the name of a predefined cursor
 * to its resource identifier.
 */

static struct CursorName {
    char *name;
    LPCTSTR id;
} cursorNames[] = {
    {"starting",		IDC_APPSTARTING},
    {"arrow",			IDC_ARROW},
    {"ibeam",			IDC_IBEAM},
    {"icon",			IDC_ICON},
    {"no",			IDC_NO},
    {"size",			IDC_SIZE},
    {"size_ne_sw",		IDC_SIZENESW},
    {"size_ns",			IDC_SIZENS},
    {"size_nw_se",		IDC_SIZENWSE},
    {"size_we",			IDC_SIZEWE},
    {"uparrow",			IDC_UPARROW},
    {"wait",			IDC_WAIT},
    {"crosshair",		IDC_CROSS},
    {"fleur",			IDC_SIZE},
    {"sb_v_double_arrow",	IDC_SIZENS},
    {"sb_h_double_arrow",	IDC_SIZEWE},
    {"center_ptr",		IDC_UPARROW},
    {"watch",			IDC_WAIT},
    {"xterm",			IDC_IBEAM},
    {NULL,			0}
};

/*
 * The default cursor is used whenever no other cursor has been specified.
 */

#define TK_DEFAULT_CURSOR	IDC_ARROW


/*
 *----------------------------------------------------------------------
 *
 * TkGetCursorByName --
 *
 *	Retrieve a system cursor by name.  
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.  
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkGetCursorByName(interp, tkwin, string)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    Tk_Uid string;		/* Description of cursor.  See manual entry
				 * for details on legal syntax. */
{
    struct CursorName *namePtr;
    TkWinCursor *cursorPtr;

    /*
     * Check for the cursor in the system cursor set.
     */

    for (namePtr = cursorNames; namePtr->name != NULL; namePtr++) {
	if (strcmp(namePtr->name, string) == 0) {
	    break;
	}
    }

    cursorPtr = (TkWinCursor *) ckalloc(sizeof(TkWinCursor));
    cursorPtr->info.cursor = (Tk_Cursor) cursorPtr;
    cursorPtr->winCursor = NULL;
    if (namePtr->name != NULL) {
	cursorPtr->winCursor = LoadCursor(NULL, namePtr->id);
	cursorPtr->system = 1;
    }
    if (cursorPtr->winCursor == NULL) {
	cursorPtr->winCursor = LoadCursor(Tk_GetHINSTANCE(), string);
	cursorPtr->system = 0;
    }
    if (string[0] == '@') {
	int argc;
	char **argv = NULL;
	if (Tcl_SplitList(interp, string, &argc, &argv) != TCL_OK) {
	    return NULL;
	}
	/*
	 * Check for system cursor of type @<filename>, where only
	 * the name is allowed.  This accepts either:
	 *	-cursor @/winnt/cursors/globe.ani
	 *	-cursor @C:/Winnt/cursors/E_arrow.cur
	 *	-cursor {@C:/Program\ Files/Cursors/bart.ani}
	 */
	if ((argc != 1) || (argv[0][0] != '@')) {
	    ckfree((char *) argv);
	    goto badCursorSpec;
	}
	if (Tcl_IsSafe(interp)) {
	    Tcl_AppendResult(interp, "can't get cursor from a file in",
		    " a safe interpreter", (char *) NULL);
	    ckfree((char *) argv);
	    ckfree((char *)cursorPtr);
	    return NULL;
	}
	cursorPtr->winCursor = LoadCursorFromFile(&(argv[0][1]));
	cursorPtr->system = 0;
	ckfree((char *) argv);
    }
    if (cursorPtr->winCursor == NULL) {
	badCursorSpec:
	ckfree((char *)cursorPtr);
	Tcl_AppendResult(interp, "bad cursor spec \"", string, "\"",
		(char *) NULL);
	return NULL;
    } else {
	return (TkCursor *) cursorPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkCreateCursorFromData --
 *
 *	Creates a cursor from the source and mask bits.
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkCreateCursorFromData(tkwin, source, mask, width, height, xHot, yHot,
	fgColor, bgColor)
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    char *source;		/* Bitmap data for cursor shape. */
    char *mask;			/* Bitmap data for cursor mask. */
    int width, height;		/* Dimensions of cursor. */
    int xHot, yHot;		/* Location of hot-spot in cursor. */
    XColor fgColor;		/* Foreground color for cursor. */
    XColor bgColor;		/* Background color for cursor. */
{
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpFreeCursor --
 *
 *	This procedure is called to release a cursor allocated by
 *	TkGetCursorByName.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor data structure is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TkpFreeCursor(cursorPtr)
    TkCursor *cursorPtr;
{
    TkWinCursor *winCursorPtr = (TkWinCursor *) cursorPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpSetCursor --
 *
 *	Set the global cursor.  If the cursor is None, then use the
 *	default Tk cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the mouse cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkpSetCursor(cursor)
    TkpCursor cursor;
{
    HCURSOR hcursor;
    TkWinCursor *winCursor = (TkWinCursor *) cursor;

    if (winCursor == NULL || winCursor->winCursor == NULL) {
	hcursor = LoadCursor(NULL, TK_DEFAULT_CURSOR);
    } else {
	hcursor = winCursor->winCursor;
    }

    if (hcursor != NULL) {
	SetCursor(hcursor);
    }
}

