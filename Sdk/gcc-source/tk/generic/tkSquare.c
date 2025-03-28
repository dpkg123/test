/* 
 * tkSquare.c --
 *
 *	This module implements "square" widgets that are object
 *	based.  A "square" is a widget that displays a single square that can 
 *	be moved around and resized.  This file is intended as an example
 *	of how to build a widget;  it isn't included in the
 *	normal wish, but it is included in "tktest".
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tkSquare.c,v 1.7.6.1 2000/05/04 21:26:24 spolk Exp $
 */

#include "tkPort.h"
#define __NO_OLD_CONFIG
#include "tk.h"
#include "tkInt.h"

/*
 * A data structure of the following type is kept for each square
 * widget managed by this file:
 */

typedef struct {
    Tk_Window tkwin;		/* Window that embodies the square.  NULL
				 * means window has been deleted but
				 * widget record hasn't been cleaned up yet. */
    Display *display;		/* X's token for the window's display. */
    Tcl_Interp *interp;		/* Interpreter associated with widget. */
    Tcl_Command widgetCmd;	/* Token for square's widget command. */
    Tk_OptionTable optionTable;	/* Token representing the configuration
				 * specifications. */
    Tcl_Obj *xPtr, *yPtr;	/* Position of square's upper-left corner
				 * within widget. */
    int x, y;
    Tcl_Obj *sizeObjPtr;	/* Width and height of square. */

    /*
     * Information used when displaying widget:
     */

    Tcl_Obj *borderWidthPtr;	/* Width of 3-D border around whole widget. */
    Tcl_Obj *bgBorderPtr;
    Tcl_Obj *fgBorderPtr;
    Tcl_Obj *reliefPtr;
    GC gc;			/* Graphics context for copying from
				 * off-screen pixmap onto screen. */
    Tcl_Obj *doubleBufferPtr;	/* Non-zero means double-buffer redisplay
				 * with pixmap;  zero means draw straight
				 * onto the display. */
    int updatePending;		/* Non-zero means a call to SquareDisplay
				 * has already been scheduled. */
} Square;

/*
 * Information used for argv parsing.
 */

static Tk_OptionSpec configSpecs[] = {
    {TK_OPTION_BORDER, "-background", "background", "Background",
	    "#d9d9d9", Tk_Offset(Square, bgBorderPtr), -1, 0,
	    (ClientData) "white"},
    {TK_OPTION_SYNONYM, "-bd", (char *) NULL, (char *) NULL,
	    (char *) NULL, 0, -1, 0, (ClientData) "-borderwidth"},
    {TK_OPTION_SYNONYM, "-bg", (char *) NULL, (char *) NULL,
	    (char *) NULL, 0, -1, 0, (ClientData) "-background"},
    {TK_OPTION_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	    "2", Tk_Offset(Square, borderWidthPtr), -1},
    {TK_OPTION_BOOLEAN, "-dbl", "doubleBuffer", "DoubleBuffer",
	    "1", Tk_Offset(Square, doubleBufferPtr), -1},
    {TK_OPTION_SYNONYM, "-fg", (char *) NULL, (char *) NULL,
	    (char *) NULL, 0, -1, 0, (ClientData) "-foreground"},
    {TK_OPTION_BORDER, "-foreground", "foreground", "Foreground",
	    "#b03060", Tk_Offset(Square, fgBorderPtr), -1, 0,
	    (ClientData) "black"},
    {TK_OPTION_PIXELS, "-posx", "posx", "PosX", "0",
	    Tk_Offset(Square, xPtr), -1},
    {TK_OPTION_PIXELS, "-posy", "posy", "PosY", "0",
	    Tk_Offset(Square, yPtr), -1},
    {TK_OPTION_RELIEF, "-relief", "relief", "Relief",
	    "raised", Tk_Offset(Square, reliefPtr), -1},
    {TK_OPTION_PIXELS, "-size", "size", "Size", "20",
	    Tk_Offset(Square, sizeObjPtr), -1},
    {TK_OPTION_END}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

int			SquareObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc, 
			    Tcl_Obj * CONST objv[]));
static void		SquareDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static int		SquareConfigure _ANSI_ARGS_((Tcl_Interp *interp,
			    Square *squarePtr));
static void		SquareDestroy _ANSI_ARGS_((char *memPtr));
static void		SquareDisplay _ANSI_ARGS_((ClientData clientData));
static void		KeepInWindow _ANSI_ARGS_((Square *squarePtr));
static void		SquareObjEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static int		SquareWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *, int objc, Tcl_Obj * CONST objv[]));

/*
 *--------------------------------------------------------------
 *
 * SquareCmd --
 *
 *	This procedure is invoked to process the "square" Tcl
 *	command.  It creates a new "square" widget.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A new widget is created and configured.
 *
 *--------------------------------------------------------------
 */

int
SquareObjCmd(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj * CONST objv[];	/* Argument objects. */
{
    Square *squarePtr;
    Tk_Window tkwin;
    Tk_OptionTable optionTable = (Tk_OptionTable) clientData;
    Tcl_CmdInfo info;
    char *commandName;

    if (optionTable == NULL) {
	/*
	 * The first time this procedure is invoked, optionTable will
	 * be NULL.  We then create the option table from the template
	 * and store the table pointer as the command's clinical so
	 * we'll have easy access to it in the future.
	 */

	optionTable = Tk_CreateOptionTable(interp, configSpecs);
	commandName = Tcl_GetStringFromObj(objv[0], (int *) NULL);
	Tcl_GetCommandInfo(interp, commandName, &info);
	info.clientData = (ClientData) optionTable;
	Tcl_SetCommandInfo(interp, commandName, &info);
    }

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "pathName ?options?");
	return TCL_ERROR;
    }

    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp), 
	    Tcl_GetStringFromObj(objv[1], NULL), (char *) NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    Tk_SetClass(tkwin, "Square");

    /*
     * Allocate and initialize the widget record.
     */

    squarePtr = (Square *) ckalloc(sizeof(Square));
    squarePtr->tkwin = tkwin;
    squarePtr->display = Tk_Display(tkwin);
    squarePtr->interp = interp;
    squarePtr->widgetCmd = Tcl_CreateObjCommand(interp,
	    Tk_PathName(squarePtr->tkwin), SquareWidgetCmd,
	    (ClientData) squarePtr, SquareDeletedProc);
    squarePtr->xPtr = NULL;
    squarePtr->yPtr = NULL;
    squarePtr->x = 0;
    squarePtr->y = 0;
    squarePtr->sizeObjPtr = NULL;
    squarePtr->borderWidthPtr = NULL;
    squarePtr->bgBorderPtr = NULL;
    squarePtr->fgBorderPtr = NULL;
    squarePtr->reliefPtr = NULL;
    squarePtr->gc = None;
    squarePtr->doubleBufferPtr = NULL;
    squarePtr->updatePending = 0;
    squarePtr->optionTable = optionTable;

    if (Tk_InitOptions(interp, (char *) squarePtr, optionTable, tkwin)
	    != TCL_OK) {
	Tk_DestroyWindow(squarePtr->tkwin);
	ckfree((char *) squarePtr);
	return TCL_ERROR;
    }

    Tk_CreateEventHandler(squarePtr->tkwin, ExposureMask|StructureNotifyMask,
	    SquareObjEventProc, (ClientData) squarePtr);
    if (Tk_SetOptions(interp, (char *) squarePtr, optionTable, objc - 2,
	    objv + 2, tkwin, NULL, (int *) NULL) != TCL_OK) {
	goto error;
    }
    if (SquareConfigure(interp, squarePtr) != TCL_OK) {
	goto error;
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(Tk_PathName(squarePtr->tkwin),
	    -1));
    return TCL_OK;

error:
    Tk_DestroyWindow(squarePtr->tkwin);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * SquareWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
SquareWidgetCmd(clientData, interp, objc, objv)
    ClientData clientData;		/* Information about square widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    Tcl_Obj * CONST objv[];		/* Argument objects. */
{
    Square *squarePtr = (Square *) clientData;
    int result = TCL_OK;
    static char *squareOptions[] = {"cget", "configure", (char *) NULL};
    enum {
	SQUARE_CGET, SQUARE_CONFIGURE
    };
    Tcl_Obj *resultObjPtr;
    int index;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], squareOptions, "command",
	    0, &index) != TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_Preserve((ClientData) squarePtr);
    
    switch (index) {
	case SQUARE_CGET: {
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "option");
		goto error;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) squarePtr,
		    squarePtr->optionTable, objv[2], squarePtr->tkwin);
	    if (resultObjPtr == NULL) {
		result = TCL_ERROR;
	    } else {
		Tcl_SetObjResult(interp, resultObjPtr);
	    }
	    break;
	}
	case SQUARE_CONFIGURE: {
	    resultObjPtr = NULL;
	    if (objc == 2) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) squarePtr,
			squarePtr->optionTable, (Tcl_Obj *) NULL,
			squarePtr->tkwin);
		if (resultObjPtr == NULL) {
		    result = TCL_ERROR;
		}
	    } else if (objc == 3) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) squarePtr,
			squarePtr->optionTable, objv[2], squarePtr->tkwin);
		if (resultObjPtr == NULL) {
		    result = TCL_ERROR;
		}
	    } else {
		result = Tk_SetOptions(interp, (char *) squarePtr,
			squarePtr->optionTable, objc - 2, objv + 2,
			squarePtr->tkwin, NULL, (int *) NULL);
		if (result == TCL_OK) {
		    result = SquareConfigure(interp, squarePtr);
		}
		if (!squarePtr->updatePending) {
		    Tcl_DoWhenIdle(SquareDisplay, (ClientData) squarePtr);
		    squarePtr->updatePending = 1;
		}
	    }
	    if (resultObjPtr != NULL) {
		Tcl_SetObjResult(interp, resultObjPtr);
	    }
	}
    }
    Tcl_Release((ClientData) squarePtr);
    return result;

    error:
    Tcl_Release((ClientData) squarePtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * SquareConfigure --
 *
 *	This procedure is called to process an argv/argc list in
 *	conjunction with the Tk option database to configure (or
 *	reconfigure) a square widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then the interp's result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for squarePtr;  old resources get freed,
 *	if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
SquareConfigure(interp, squarePtr)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Square *squarePtr;			/* Information about widget. */
{
    int borderWidth;
    Tk_3DBorder bgBorder;
    int doubleBuffer;

    /*
     * Set the background for the window and create a graphics context
     * for use during redisplay.
     */

    bgBorder = Tk_Get3DBorderFromObj(squarePtr->tkwin, 
	    squarePtr->bgBorderPtr);
    Tk_SetWindowBackground(squarePtr->tkwin,
	    Tk_3DBorderColor(bgBorder)->pixel);
    Tcl_GetBooleanFromObj(NULL, squarePtr->doubleBufferPtr, &doubleBuffer);
    if ((squarePtr->gc == None) && (doubleBuffer)) {
	XGCValues gcValues;
	gcValues.function = GXcopy;
	gcValues.graphics_exposures = False;
	squarePtr->gc = Tk_GetGC(squarePtr->tkwin,
		GCFunction|GCGraphicsExposures, &gcValues);
    }

    /*
     * Register the desired geometry for the window.  Then arrange for
     * the window to be redisplayed.
     */

    Tk_GeometryRequest(squarePtr->tkwin, 200, 150);
    Tk_GetPixelsFromObj(NULL, squarePtr->tkwin, squarePtr->borderWidthPtr,
	    &borderWidth);
    Tk_SetInternalBorder(squarePtr->tkwin, borderWidth);
    if (!squarePtr->updatePending) {
	Tcl_DoWhenIdle(SquareDisplay, (ClientData) squarePtr);
	squarePtr->updatePending = 1;
    }
    KeepInWindow(squarePtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * SquareObjEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on squares.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
SquareObjEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Square *squarePtr = (Square *) clientData;

    if (eventPtr->type == Expose) {
	if (!squarePtr->updatePending) {
	    Tcl_DoWhenIdle(SquareDisplay, (ClientData) squarePtr);
	    squarePtr->updatePending = 1;
	}
    } else if (eventPtr->type == ConfigureNotify) {
	KeepInWindow(squarePtr);
	if (!squarePtr->updatePending) {
	    Tcl_DoWhenIdle(SquareDisplay, (ClientData) squarePtr);
	    squarePtr->updatePending = 1;
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (squarePtr->tkwin != NULL) {
	    Tk_FreeConfigOptions((char *) squarePtr, squarePtr->optionTable,
		    squarePtr->tkwin);
	    if (squarePtr->gc != None) {
		Tk_FreeGC(squarePtr->display, squarePtr->gc);
	    }
	    squarePtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(squarePtr->interp,
		    squarePtr->widgetCmd);
	}
	if (squarePtr->updatePending) {
	    Tcl_CancelIdleCall(SquareDisplay, (ClientData) squarePtr);
	}
	Tcl_EventuallyFree((ClientData) squarePtr, SquareDestroy);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SquareDeletedProc --
 *
 *	This procedure is invoked when a widget command is deleted.  If
 *	the widget isn't already in the process of being destroyed,
 *	this command destroys it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
SquareDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Square *squarePtr = (Square *) clientData;
    Tk_Window tkwin = squarePtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *--------------------------------------------------------------
 *
 * SquareDisplay --
 *
 *	This procedure redraws the contents of a square window.
 *	It is invoked as a do-when-idle handler, so it only runs
 *	when there's nothing else for the application to do.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */

static void
SquareDisplay(clientData)
    ClientData clientData;	/* Information about window. */
{
    Square *squarePtr = (Square *) clientData;
    Tk_Window tkwin = squarePtr->tkwin;
    Pixmap pm = None;
    Drawable d;
    int borderWidth, size, relief;
    Tk_3DBorder bgBorder, fgBorder;
    int doubleBuffer;

    squarePtr->updatePending = 0;
    if (!Tk_IsMapped(tkwin)) {
	return;
    }

    /*
     * Create a pixmap for double-buffering, if necessary.
     */

    Tcl_GetBooleanFromObj(NULL, squarePtr->doubleBufferPtr, &doubleBuffer);
    if (doubleBuffer) {
	pm = Tk_GetPixmap(Tk_Display(tkwin), Tk_WindowId(tkwin),
		Tk_Width(tkwin), Tk_Height(tkwin),
		DefaultDepthOfScreen(Tk_Screen(tkwin)));
	d = pm;
    } else {
	d = Tk_WindowId(tkwin);
    }

    /*
     * Redraw the widget's background and border.
     */

    Tk_GetPixelsFromObj(NULL, squarePtr->tkwin, squarePtr->borderWidthPtr,
	    &borderWidth);
    bgBorder = Tk_Get3DBorderFromObj(squarePtr->tkwin, 
	    squarePtr->bgBorderPtr);
    Tk_GetReliefFromObj(NULL, squarePtr->reliefPtr, &relief);
    Tk_Fill3DRectangle(tkwin, d, bgBorder, 0, 0, Tk_Width(tkwin),
	    Tk_Height(tkwin), borderWidth, relief);

    /*
     * Display the square.
     */

    Tk_GetPixelsFromObj(NULL, squarePtr->tkwin, squarePtr->sizeObjPtr, &size);
    fgBorder = Tk_Get3DBorderFromObj(squarePtr->tkwin, 
	    squarePtr->fgBorderPtr);
    Tk_Fill3DRectangle(tkwin, d, fgBorder, squarePtr->x, squarePtr->y, size, 
	    size, borderWidth, TK_RELIEF_RAISED);

    /*
     * If double-buffered, copy to the screen and release the pixmap.
     */

    if (doubleBuffer) {
	XCopyArea(Tk_Display(tkwin), pm, Tk_WindowId(tkwin), squarePtr->gc,
		0, 0, (unsigned) Tk_Width(tkwin), (unsigned) Tk_Height(tkwin),
		0, 0);
	Tk_FreePixmap(Tk_Display(tkwin), pm);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SquareDestroy --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a square at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the square is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
SquareDestroy(memPtr)
    char *memPtr;		/* Info about square widget. */
{
    Square *squarePtr = (Square *) memPtr;
    
    ckfree((char *) squarePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * KeepInWindow --
 *
 *	Adjust the position of the square if necessary to keep it in
 *	the widget's window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The x and y position of the square are adjusted if necessary
 *	to keep the square in the window.
 *
 *----------------------------------------------------------------------
 */

static void
KeepInWindow(squarePtr)
    register Square *squarePtr;		/* Pointer to widget record. */
{
    int i, bd, relief;
    int borderWidth, size;

    Tk_GetPixelsFromObj(NULL, squarePtr->tkwin, squarePtr->borderWidthPtr,
	    &borderWidth);
    Tk_GetPixelsFromObj(NULL, squarePtr->tkwin, squarePtr->xPtr, 
	    &squarePtr->x);
    Tk_GetPixelsFromObj(NULL, squarePtr->tkwin, squarePtr->yPtr, 
	    &squarePtr->y);
    Tk_GetPixelsFromObj(NULL, squarePtr->tkwin, squarePtr->sizeObjPtr, &size);
    Tk_GetReliefFromObj(NULL, squarePtr->reliefPtr, &relief);
    bd = 0;
    if (relief != TK_RELIEF_FLAT) {
	bd = borderWidth;
    }
    i = (Tk_Width(squarePtr->tkwin) - bd) - (squarePtr->x + size);
    if (i < 0) {
	squarePtr->x += i;
    }
    i = (Tk_Height(squarePtr->tkwin) - bd) - (squarePtr->y + size);
    if (i < 0) {
	squarePtr->y += i;
    }
    if (squarePtr->x < bd) {
	squarePtr->x = bd;
    }
    if (squarePtr->y < bd) {
	squarePtr->y = bd;
    }
}

