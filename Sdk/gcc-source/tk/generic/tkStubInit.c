/* 
 * tkStubInit.c --
 *
 *	This file contains the initializers for the Tk stub vectors.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tkStubInit.c,v 1.3.6.1 2000/09/26 16:08:26 spolk Exp $
 */

#include "tkInt.h"
#include "tkPort.h"

#ifdef __WIN32__
#include "tkWinInt.h"
#endif
#ifdef MAC_TCL
#include "tkMacInt.h"
#endif

#include "tkDecls.h"
#include "tkPlatDecls.h"
#include "tkIntDecls.h"
#include "tkIntPlatDecls.h"
#include "tkIntXlibDecls.h"

/*
 * Remove macros that will interfere with the definitions below.
 */

#define Tk_CreateCanvasVisitor ((void (*) _ANSI_ARGS_((Tcl_Interp * interp, \
			VOID * typePtr))) NULL)
#define Tk_GetCanvasVisitor ((VOID * (*) _ANSI_ARGS_((Tcl_Interp * interp, \
			CONST char * name))) NULL)

/*
 * WARNING: The contents of this file is automatically generated by the
 * tools/genStubs.tcl script. Any modifications to the function declarations
 * below should be made in the generic/tk.decls script.
 */

/* !BEGIN!: Do not edit below this line. */

TkIntStubs tkIntStubs = {
    TCL_STUB_MAGIC,
    NULL,
    TkAllocWindow, /* 0 */
    TkBezierPoints, /* 1 */
    TkBezierScreenPoints, /* 2 */
    TkBindDeadWindow, /* 3 */
    TkBindEventProc, /* 4 */
    TkBindFree, /* 5 */
    TkBindInit, /* 6 */
    TkChangeEventWindow, /* 7 */
    TkClipInit, /* 8 */
    TkComputeAnchor, /* 9 */
    TkCopyAndGlobalEval, /* 10 */
    TkCreateBindingProcedure, /* 11 */
    TkCreateCursorFromData, /* 12 */
    TkCreateFrame, /* 13 */
    TkCreateMainWindow, /* 14 */
    TkCurrentTime, /* 15 */
    TkDeleteAllImages, /* 16 */
    TkDoConfigureNotify, /* 17 */
    TkDrawInsetFocusHighlight, /* 18 */
    TkEventDeadWindow, /* 19 */
    TkFillPolygon, /* 20 */
    TkFindStateNum, /* 21 */
    TkFindStateString, /* 22 */
    TkFocusDeadWindow, /* 23 */
    TkFocusFilterEvent, /* 24 */
    TkFocusKeyEvent, /* 25 */
    TkFontPkgInit, /* 26 */
    TkFontPkgFree, /* 27 */
    TkFreeBindingTags, /* 28 */
    TkpFreeCursor, /* 29 */
    TkGetBitmapData, /* 30 */
    TkGetButtPoints, /* 31 */
    TkGetCursorByName, /* 32 */
    TkGetDefaultScreenName, /* 33 */
    TkGetDisplay, /* 34 */
    TkGetDisplayOf, /* 35 */
    TkGetFocusWin, /* 36 */
    TkGetInterpNames, /* 37 */
    TkGetMiterPoints, /* 38 */
    TkGetPointerCoords, /* 39 */
    TkGetServerInfo, /* 40 */
    TkGrabDeadWindow, /* 41 */
    TkGrabState, /* 42 */
    TkIncludePoint, /* 43 */
    TkInOutEvents, /* 44 */
    TkInstallFrameMenu, /* 45 */
    TkKeysymToString, /* 46 */
    TkLineToArea, /* 47 */
    TkLineToPoint, /* 48 */
    TkMakeBezierCurve, /* 49 */
    TkMakeBezierPostscript, /* 50 */
    TkOptionClassChanged, /* 51 */
    TkOptionDeadWindow, /* 52 */
    TkOvalToArea, /* 53 */
    TkOvalToPoint, /* 54 */
    TkpChangeFocus, /* 55 */
    TkpCloseDisplay, /* 56 */
    TkpClaimFocus, /* 57 */
    TkpDisplayWarning, /* 58 */
    TkpGetAppName, /* 59 */
    TkpGetOtherWindow, /* 60 */
    TkpGetWrapperWindow, /* 61 */
    TkpInit, /* 62 */
    TkpInitializeMenuBindings, /* 63 */
    TkpMakeContainer, /* 64 */
    TkpMakeMenuWindow, /* 65 */
    TkpMakeWindow, /* 66 */
    TkpMenuNotifyToplevelCreate, /* 67 */
    TkpOpenDisplay, /* 68 */
    TkPointerEvent, /* 69 */
    TkPolygonToArea, /* 70 */
    TkPolygonToPoint, /* 71 */
    TkPositionInTree, /* 72 */
    TkpRedirectKeyEvent, /* 73 */
    TkpSetMainMenubar, /* 74 */
    TkpUseWindow, /* 75 */
    TkpWindowWasRecentlyDeleted, /* 76 */
    TkQueueEventForAllChildren, /* 77 */
    TkReadBitmapFile, /* 78 */
    TkScrollWindow, /* 79 */
    TkSelDeadWindow, /* 80 */
    TkSelEventProc, /* 81 */
    TkSelInit, /* 82 */
    TkSelPropProc, /* 83 */
    TkSetClassProcs, /* 84 */
    TkSetWindowMenuBar, /* 85 */
    TkStringToKeysym, /* 86 */
    TkThickPolyLineToArea, /* 87 */
    TkWmAddToColormapWindows, /* 88 */
    TkWmDeadWindow, /* 89 */
    TkWmFocusToplevel, /* 90 */
    TkWmMapWindow, /* 91 */
    TkWmNewWindow, /* 92 */
    TkWmProtocolEventProc, /* 93 */
    TkWmRemoveFromColormapWindows, /* 94 */
    TkWmRestackToplevel, /* 95 */
    TkWmSetClass, /* 96 */
    TkWmUnmapWindow, /* 97 */
    TkDebugBitmap, /* 98 */
    TkDebugBorder, /* 99 */
    TkDebugCursor, /* 100 */
    TkDebugColor, /* 101 */
    TkDebugConfig, /* 102 */
    TkDebugFont, /* 103 */
    TkFindStateNumObj, /* 104 */
    TkGetBitmapPredefTable, /* 105 */
    TkGetDisplayList, /* 106 */
    TkGetMainInfoList, /* 107 */
    TkGetWindowFromObj, /* 108 */
    TkpGetString, /* 109 */
    TkpGetSubFonts, /* 110 */
    TkpGetSystemDefault, /* 111 */
    TkpMenuThreadInit, /* 112 */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 113 */
#endif /* UNIX */
#ifdef __WIN32__
    TkClipBox, /* 113 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkClipBox, /* 113 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 114 */
#endif /* UNIX */
#ifdef __WIN32__
    TkCreateRegion, /* 114 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkCreateRegion, /* 114 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 115 */
#endif /* UNIX */
#ifdef __WIN32__
    TkDestroyRegion, /* 115 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkDestroyRegion, /* 115 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 116 */
#endif /* UNIX */
#ifdef __WIN32__
    TkIntersectRegion, /* 116 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkIntersectRegion, /* 116 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 117 */
#endif /* UNIX */
#ifdef __WIN32__
    TkRectInRegion, /* 117 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkRectInRegion, /* 117 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 118 */
#endif /* UNIX */
#ifdef __WIN32__
    TkSetRegion, /* 118 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkSetRegion, /* 118 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 119 */
#endif /* UNIX */
#ifdef __WIN32__
    TkUnionRectWithRegion, /* 119 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkUnionRectWithRegion, /* 119 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 120 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 120 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkGenerateActivateEvents, /* 120 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 121 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 121 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkpCreateNativeBitmap, /* 121 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 122 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 122 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkpDefineNativeBitmaps, /* 122 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 123 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 123 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkpGetMS, /* 123 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 124 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 124 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkpGetNativeAppBitmap, /* 124 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 125 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 125 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkPointerDeadWindow, /* 125 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 126 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 126 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkpSetCapture, /* 126 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 127 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 127 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkpSetCursor, /* 127 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 128 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 128 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkpWmSetState, /* 128 */
#endif /* MAC_TCL */
    NULL, /* 129 */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 130 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 130 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkGetTransientMaster, /* 130 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 131 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 131 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkGenerateButtonEvent, /* 131 */
#endif /* MAC_TCL */
    NULL, /* 132 */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 133 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 133 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkGenWMDestroyEvent, /* 133 */
#endif /* MAC_TCL */
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    NULL, /* 134 */
#endif /* UNIX */
#ifdef __WIN32__
    NULL, /* 134 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkGenWMConfigureEvent, /* 134 */
#endif /* MAC_TCL */
    TkpDrawHighlightBorder, /* 135 */
    TkSetFocusWin, /* 136 */
    TkpSetKeycodeAndState, /* 137 */
    TkpGetKeySym, /* 138 */
    TkpInitKeymapInfo, /* 139 */
};

TkIntPlatStubs tkIntPlatStubs = {
    TCL_STUB_MAGIC,
    NULL,
#if !defined(__WIN32__) && !defined(MAC_TCL) /* UNIX */
    TkCreateXEventSource, /* 0 */
    TkFreeWindowId, /* 1 */
    TkInitXId, /* 2 */
    TkpCmapStressed, /* 3 */
    TkpSync, /* 4 */
    TkUnixContainerId, /* 5 */
    TkUnixDoOneXEvent, /* 6 */
    TkUnixSetMenubar, /* 7 */
#endif /* UNIX */
#ifdef __WIN32__
    TkAlignImageData, /* 0 */
    NULL, /* 1 */
    TkGenerateActivateEvents, /* 2 */
    TkpGetMS, /* 3 */
    TkPointerDeadWindow, /* 4 */
    TkpPrintWindowId, /* 5 */
    TkpScanWindowId, /* 6 */
    TkpSetCapture, /* 7 */
    TkpSetCursor, /* 8 */
    TkpWmSetState, /* 9 */
    TkSetPixmapColormap, /* 10 */
    TkWinCancelMouseTimer, /* 11 */
    TkWinClipboardRender, /* 12 */
    TkWinEmbeddedEventProc, /* 13 */
    TkWinFillRect, /* 14 */
    TkWinGetBorderPixels, /* 15 */
    TkWinGetDrawableDC, /* 16 */
    TkWinGetModifierState, /* 17 */
    TkWinGetSystemPalette, /* 18 */
    TkWinGetWrapperWindow, /* 19 */
    TkWinHandleMenuEvent, /* 20 */
    TkWinIndexOfColor, /* 21 */
    TkWinReleaseDrawableDC, /* 22 */
    TkWinResendEvent, /* 23 */
    TkWinSelectPalette, /* 24 */
    TkWinSetMenu, /* 25 */
    TkWinSetWindowPos, /* 26 */
    TkWinWmCleanup, /* 27 */
    TkWinXCleanup, /* 28 */
    TkWinXInit, /* 29 */
    TkWinSetForegroundWindow, /* 30 */
    TkWinDialogDebug, /* 31 */
    TkWinGetMenuSystemDefault, /* 32 */
    TkWinGetPlatformId, /* 33 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    TkGenerateActivateEvents, /* 0 */
    TkpCreateNativeBitmap, /* 1 */
    TkpDefineNativeBitmaps, /* 2 */
    TkpGetMS, /* 3 */
    NULL, /* 4 */
    TkPointerDeadWindow, /* 5 */
    TkpSetCapture, /* 6 */
    TkpSetCursor, /* 7 */
    TkpWmSetState, /* 8 */
    NULL, /* 9 */
    TkAboutDlg, /* 10 */
    NULL, /* 11 */
    NULL, /* 12 */
    TkGetTransientMaster, /* 13 */
    TkGenerateButtonEvent, /* 14 */
    NULL, /* 15 */
    TkGenWMDestroyEvent, /* 16 */
    TkGenWMConfigureEvent, /* 17 */
    TkMacButtonKeyState, /* 18 */
    TkMacClearMenubarActive, /* 19 */
    TkMacConvertEvent, /* 20 */
    TkMacDispatchMenuEvent, /* 21 */
    TkMacInstallCursor, /* 22 */
    TkMacConvertTkEvent, /* 23 */
    TkMacHandleTearoffMenu, /* 24 */
    NULL, /* 25 */
    TkMacInvalClipRgns, /* 26 */
    TkMacDoHLEvent, /* 27 */
    NULL, /* 28 */
    TkMacGenerateTime, /* 29 */
    TkMacGetDrawablePort, /* 30 */
    TkMacGetScrollbarGrowWindow, /* 31 */
    TkMacGetXWindow, /* 32 */
    TkMacGrowToplevel, /* 33 */
    TkMacHandleMenuSelect, /* 34 */
    TkMacHaveAppearance, /* 35 */
    TkMacInitAppleEvents, /* 36 */
    TkMacInitMenus, /* 37 */
    TkMacInvalidateWindow, /* 38 */
    TkMacIsCharacterMissing, /* 39 */
    TkMacMakeRealWindowExist, /* 40 */
    TkMacMakeStippleMap, /* 41 */
    TkMacMenuClick, /* 42 */
    TkMacRegisterOffScreenWindow, /* 43 */
    TkMacResizable, /* 44 */
    NULL, /* 45 */
    TkMacSetHelpMenuItemCount, /* 46 */
    TkMacSetScrollbarGrow, /* 47 */
    TkMacSetUpClippingRgn, /* 48 */
    TkMacSetUpGraphicsPort, /* 49 */
    TkMacUpdateClipRgn, /* 50 */
    TkMacUnregisterMacWindow, /* 51 */
    TkMacUseMenuID, /* 52 */
    TkMacVisableClipRgn, /* 53 */
    TkMacWinBounds, /* 54 */
    TkMacWindowOffset, /* 55 */
    NULL, /* 56 */
    TkSetMacColor, /* 57 */
    TkSetWMName, /* 58 */
    TkSuspendClipboard, /* 59 */
    NULL, /* 60 */
    TkMacZoomToplevel, /* 61 */
    Tk_TopCoordsToWindow, /* 62 */
    TkMacContainerId, /* 63 */
    TkMacGetHostToplevel, /* 64 */
    TkMacPreprocessMenu, /* 65 */
#endif /* MAC_TCL */
};

TkIntXlibStubs tkIntXlibStubs = {
    TCL_STUB_MAGIC,
    NULL,
#ifdef __WIN32__
    XSetDashes, /* 0 */
    XGetModifierMapping, /* 1 */
    XCreateImage, /* 2 */
    XGetImage, /* 3 */
    XGetAtomName, /* 4 */
    XKeysymToString, /* 5 */
    XCreateColormap, /* 6 */
    XCreatePixmapCursor, /* 7 */
    XCreateGlyphCursor, /* 8 */
    XGContextFromGC, /* 9 */
    XListHosts, /* 10 */
    XKeycodeToKeysym, /* 11 */
    XStringToKeysym, /* 12 */
    XRootWindow, /* 13 */
    XSetErrorHandler, /* 14 */
    XIconifyWindow, /* 15 */
    XWithdrawWindow, /* 16 */
    XGetWMColormapWindows, /* 17 */
    XAllocColor, /* 18 */
    XBell, /* 19 */
    XChangeProperty, /* 20 */
    XChangeWindowAttributes, /* 21 */
    XClearWindow, /* 22 */
    XConfigureWindow, /* 23 */
    XCopyArea, /* 24 */
    XCopyPlane, /* 25 */
    XCreateBitmapFromData, /* 26 */
    XDefineCursor, /* 27 */
    XDeleteProperty, /* 28 */
    XDestroyWindow, /* 29 */
    XDrawArc, /* 30 */
    XDrawLines, /* 31 */
    XDrawRectangle, /* 32 */
    XFillArc, /* 33 */
    XFillPolygon, /* 34 */
    XFillRectangles, /* 35 */
    XForceScreenSaver, /* 36 */
    XFreeColormap, /* 37 */
    XFreeColors, /* 38 */
    XFreeCursor, /* 39 */
    XFreeModifiermap, /* 40 */
    XGetGeometry, /* 41 */
    XGetInputFocus, /* 42 */
    XGetWindowProperty, /* 43 */
    XGetWindowAttributes, /* 44 */
    XGrabKeyboard, /* 45 */
    XGrabPointer, /* 46 */
    XKeysymToKeycode, /* 47 */
    XLookupColor, /* 48 */
    XMapWindow, /* 49 */
    XMoveResizeWindow, /* 50 */
    XMoveWindow, /* 51 */
    XNextEvent, /* 52 */
    XPutBackEvent, /* 53 */
    XQueryColors, /* 54 */
    XQueryPointer, /* 55 */
    XQueryTree, /* 56 */
    XRaiseWindow, /* 57 */
    XRefreshKeyboardMapping, /* 58 */
    XResizeWindow, /* 59 */
    XSelectInput, /* 60 */
    XSendEvent, /* 61 */
    XSetCommand, /* 62 */
    XSetIconName, /* 63 */
    XSetInputFocus, /* 64 */
    XSetSelectionOwner, /* 65 */
    XSetWindowBackground, /* 66 */
    XSetWindowBackgroundPixmap, /* 67 */
    XSetWindowBorder, /* 68 */
    XSetWindowBorderPixmap, /* 69 */
    XSetWindowBorderWidth, /* 70 */
    XSetWindowColormap, /* 71 */
    XTranslateCoordinates, /* 72 */
    XUngrabKeyboard, /* 73 */
    XUngrabPointer, /* 74 */
    XUnmapWindow, /* 75 */
    XWindowEvent, /* 76 */
    XDestroyIC, /* 77 */
    XFilterEvent, /* 78 */
    XmbLookupString, /* 79 */
    TkPutImage, /* 80 */
    NULL, /* 81 */
    XParseColor, /* 82 */
    XCreateGC, /* 83 */
    XFreeGC, /* 84 */
    XInternAtom, /* 85 */
    XSetBackground, /* 86 */
    XSetForeground, /* 87 */
    XSetClipMask, /* 88 */
    XSetClipOrigin, /* 89 */
    XSetTSOrigin, /* 90 */
    XChangeGC, /* 91 */
    XSetFont, /* 92 */
    XSetArcMode, /* 93 */
    XSetStipple, /* 94 */
    XSetFillRule, /* 95 */
    XSetFillStyle, /* 96 */
    XSetFunction, /* 97 */
    XSetLineAttributes, /* 98 */
    _XInitImageFuncPtrs, /* 99 */
    XCreateIC, /* 100 */
    XGetVisualInfo, /* 101 */
    XSetWMClientMachine, /* 102 */
    XStringListToTextProperty, /* 103 */
    XDrawLine, /* 104 */
    XWarpPointer, /* 105 */
    XFillRectangle, /* 106 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    XSetDashes, /* 0 */
    XGetModifierMapping, /* 1 */
    XCreateImage, /* 2 */
    XGetImage, /* 3 */
    XGetAtomName, /* 4 */
    XKeysymToString, /* 5 */
    XCreateColormap, /* 6 */
    XGContextFromGC, /* 7 */
    XKeycodeToKeysym, /* 8 */
    XStringToKeysym, /* 9 */
    XRootWindow, /* 10 */
    XSetErrorHandler, /* 11 */
    XAllocColor, /* 12 */
    XBell, /* 13 */
    XChangeProperty, /* 14 */
    XChangeWindowAttributes, /* 15 */
    XConfigureWindow, /* 16 */
    XCopyArea, /* 17 */
    XCopyPlane, /* 18 */
    XCreateBitmapFromData, /* 19 */
    XDefineCursor, /* 20 */
    XDestroyWindow, /* 21 */
    XDrawArc, /* 22 */
    XDrawLines, /* 23 */
    XDrawRectangle, /* 24 */
    XFillArc, /* 25 */
    XFillPolygon, /* 26 */
    XFillRectangles, /* 27 */
    XFreeColormap, /* 28 */
    XFreeColors, /* 29 */
    XFreeModifiermap, /* 30 */
    XGetGeometry, /* 31 */
    XGetWindowProperty, /* 32 */
    XGrabKeyboard, /* 33 */
    XGrabPointer, /* 34 */
    XKeysymToKeycode, /* 35 */
    XMapWindow, /* 36 */
    XMoveResizeWindow, /* 37 */
    XMoveWindow, /* 38 */
    XQueryPointer, /* 39 */
    XRaiseWindow, /* 40 */
    XRefreshKeyboardMapping, /* 41 */
    XResizeWindow, /* 42 */
    XSelectInput, /* 43 */
    XSendEvent, /* 44 */
    XSetIconName, /* 45 */
    XSetInputFocus, /* 46 */
    XSetSelectionOwner, /* 47 */
    XSetWindowBackground, /* 48 */
    XSetWindowBackgroundPixmap, /* 49 */
    XSetWindowBorder, /* 50 */
    XSetWindowBorderPixmap, /* 51 */
    XSetWindowBorderWidth, /* 52 */
    XSetWindowColormap, /* 53 */
    XUngrabKeyboard, /* 54 */
    XUngrabPointer, /* 55 */
    XUnmapWindow, /* 56 */
    TkPutImage, /* 57 */
    XParseColor, /* 58 */
    XCreateGC, /* 59 */
    XFreeGC, /* 60 */
    XInternAtom, /* 61 */
    XSetBackground, /* 62 */
    XSetForeground, /* 63 */
    XSetClipMask, /* 64 */
    XSetClipOrigin, /* 65 */
    XSetTSOrigin, /* 66 */
    XChangeGC, /* 67 */
    XSetFont, /* 68 */
    XSetArcMode, /* 69 */
    XSetStipple, /* 70 */
    XSetFillRule, /* 71 */
    XSetFillStyle, /* 72 */
    XSetFunction, /* 73 */
    XSetLineAttributes, /* 74 */
    _XInitImageFuncPtrs, /* 75 */
    XCreateIC, /* 76 */
    XGetVisualInfo, /* 77 */
    XSetWMClientMachine, /* 78 */
    XStringListToTextProperty, /* 79 */
    XDrawSegments, /* 80 */
    XForceScreenSaver, /* 81 */
    XDrawLine, /* 82 */
    XFillRectangle, /* 83 */
    XClearWindow, /* 84 */
    XDrawPoint, /* 85 */
    XDrawPoints, /* 86 */
    XWarpPointer, /* 87 */
    XQueryColor, /* 88 */
    XQueryColors, /* 89 */
#endif /* MAC_TCL */
};

TkPlatStubs tkPlatStubs = {
    TCL_STUB_MAGIC,
    NULL,
#ifdef __WIN32__
    Tk_AttachHWND, /* 0 */
    Tk_GetHINSTANCE, /* 1 */
    Tk_GetHWND, /* 2 */
    Tk_HWNDToWindow, /* 3 */
    Tk_PointerEvent, /* 4 */
    Tk_TranslateWinEvent, /* 5 */
#endif /* __WIN32__ */
#ifdef MAC_TCL
    Tk_MacSetEmbedHandler, /* 0 */
    Tk_MacTurnOffMenus, /* 1 */
    Tk_MacTkOwnsCursor, /* 2 */
    TkMacInitMenus, /* 3 */
    TkMacInitAppleEvents, /* 4 */
    TkMacConvertEvent, /* 5 */
    TkMacConvertTkEvent, /* 6 */
    TkGenWMConfigureEvent, /* 7 */
    TkMacInvalClipRgns, /* 8 */
    TkMacHaveAppearance, /* 9 */
    TkMacGetDrawablePort, /* 10 */
#endif /* MAC_TCL */
};

static TkStubHooks tkStubHooks = {
    &tkPlatStubs,
    &tkIntStubs,
    &tkIntPlatStubs,
    &tkIntXlibStubs
};

TkStubs tkStubs = {
    TCL_STUB_MAGIC,
    &tkStubHooks,
    Tk_MainLoop, /* 0 */
    Tk_3DBorderColor, /* 1 */
    Tk_3DBorderGC, /* 2 */
    Tk_3DHorizontalBevel, /* 3 */
    Tk_3DVerticalBevel, /* 4 */
    Tk_AddOption, /* 5 */
    Tk_BindEvent, /* 6 */
    Tk_CanvasDrawableCoords, /* 7 */
    Tk_CanvasEventuallyRedraw, /* 8 */
    Tk_CanvasGetCoord, /* 9 */
    Tk_CanvasGetTextInfo, /* 10 */
    Tk_CanvasPsBitmap, /* 11 */
    Tk_CanvasPsColor, /* 12 */
    Tk_CanvasPsFont, /* 13 */
    Tk_CanvasPsPath, /* 14 */
    Tk_CanvasPsStipple, /* 15 */
    Tk_CanvasPsY, /* 16 */
    Tk_CanvasSetStippleOrigin, /* 17 */
    Tk_CanvasTagsParseProc, /* 18 */
    Tk_CanvasTagsPrintProc, /* 19 */
    Tk_CanvasTkwin, /* 20 */
    Tk_CanvasWindowCoords, /* 21 */
    Tk_ChangeWindowAttributes, /* 22 */
    Tk_CharBbox, /* 23 */
    Tk_ClearSelection, /* 24 */
    Tk_ClipboardAppend, /* 25 */
    Tk_ClipboardClear, /* 26 */
    Tk_ConfigureInfo, /* 27 */
    Tk_ConfigureValue, /* 28 */
    Tk_ConfigureWidget, /* 29 */
    Tk_ConfigureWindow, /* 30 */
    Tk_ComputeTextLayout, /* 31 */
    Tk_CoordsToWindow, /* 32 */
    Tk_CreateBinding, /* 33 */
    Tk_CreateBindingTable, /* 34 */
    Tk_CreateErrorHandler, /* 35 */
    Tk_CreateEventHandler, /* 36 */
    Tk_CreateGenericHandler, /* 37 */
    Tk_CreateImageType, /* 38 */
    Tk_CreateItemType, /* 39 */
    Tk_CreatePhotoImageFormat, /* 40 */
    Tk_CreateSelHandler, /* 41 */
    Tk_CreateWindow, /* 42 */
    Tk_CreateWindowFromPath, /* 43 */
    Tk_DefineBitmap, /* 44 */
    Tk_DefineCursor, /* 45 */
    Tk_DeleteAllBindings, /* 46 */
    Tk_DeleteBinding, /* 47 */
    Tk_DeleteBindingTable, /* 48 */
    Tk_DeleteErrorHandler, /* 49 */
    Tk_DeleteEventHandler, /* 50 */
    Tk_DeleteGenericHandler, /* 51 */
    Tk_DeleteImage, /* 52 */
    Tk_DeleteSelHandler, /* 53 */
    Tk_DestroyWindow, /* 54 */
    Tk_DisplayName, /* 55 */
    Tk_DistanceToTextLayout, /* 56 */
    Tk_Draw3DPolygon, /* 57 */
    Tk_Draw3DRectangle, /* 58 */
    Tk_DrawChars, /* 59 */
    Tk_DrawFocusHighlight, /* 60 */
    Tk_DrawTextLayout, /* 61 */
    Tk_Fill3DPolygon, /* 62 */
    Tk_Fill3DRectangle, /* 63 */
    Tk_FindPhoto, /* 64 */
    Tk_FontId, /* 65 */
    Tk_Free3DBorder, /* 66 */
    Tk_FreeBitmap, /* 67 */
    Tk_FreeColor, /* 68 */
    Tk_FreeColormap, /* 69 */
    Tk_FreeCursor, /* 70 */
    Tk_FreeFont, /* 71 */
    Tk_FreeGC, /* 72 */
    Tk_FreeImage, /* 73 */
    Tk_FreeOptions, /* 74 */
    Tk_FreePixmap, /* 75 */
    Tk_FreeTextLayout, /* 76 */
    Tk_FreeXId, /* 77 */
    Tk_GCForColor, /* 78 */
    Tk_GeometryRequest, /* 79 */
    Tk_Get3DBorder, /* 80 */
    Tk_GetAllBindings, /* 81 */
    Tk_GetAnchor, /* 82 */
    Tk_GetAtomName, /* 83 */
    Tk_GetBinding, /* 84 */
    Tk_GetBitmap, /* 85 */
    Tk_GetBitmapFromData, /* 86 */
    Tk_GetCapStyle, /* 87 */
    Tk_GetColor, /* 88 */
    Tk_GetColorByValue, /* 89 */
    Tk_GetColormap, /* 90 */
    Tk_GetCursor, /* 91 */
    Tk_GetCursorFromData, /* 92 */
    Tk_GetFont, /* 93 */
    Tk_GetFontFromObj, /* 94 */
    Tk_GetFontMetrics, /* 95 */
    Tk_GetGC, /* 96 */
    Tk_GetImage, /* 97 */
    Tk_GetImageMasterData, /* 98 */
    Tk_GetItemTypes, /* 99 */
    Tk_GetJoinStyle, /* 100 */
    Tk_GetJustify, /* 101 */
    Tk_GetNumMainWindows, /* 102 */
    Tk_GetOption, /* 103 */
    Tk_GetPixels, /* 104 */
    Tk_GetPixmap, /* 105 */
    Tk_GetRelief, /* 106 */
    Tk_GetRootCoords, /* 107 */
    Tk_GetScrollInfo, /* 108 */
    Tk_GetScreenMM, /* 109 */
    Tk_GetSelection, /* 110 */
    Tk_GetUid, /* 111 */
    Tk_GetVisual, /* 112 */
    Tk_GetVRootGeometry, /* 113 */
    Tk_Grab, /* 114 */
    Tk_HandleEvent, /* 115 */
    Tk_IdToWindow, /* 116 */
    Tk_ImageChanged, /* 117 */
    Tk_Init, /* 118 */
    Tk_InternAtom, /* 119 */
    Tk_IntersectTextLayout, /* 120 */
    Tk_MaintainGeometry, /* 121 */
    Tk_MainWindow, /* 122 */
    Tk_MakeWindowExist, /* 123 */
    Tk_ManageGeometry, /* 124 */
    Tk_MapWindow, /* 125 */
    Tk_MeasureChars, /* 126 */
    Tk_MoveResizeWindow, /* 127 */
    Tk_MoveWindow, /* 128 */
    Tk_MoveToplevelWindow, /* 129 */
    Tk_NameOf3DBorder, /* 130 */
    Tk_NameOfAnchor, /* 131 */
    Tk_NameOfBitmap, /* 132 */
    Tk_NameOfCapStyle, /* 133 */
    Tk_NameOfColor, /* 134 */
    Tk_NameOfCursor, /* 135 */
    Tk_NameOfFont, /* 136 */
    Tk_NameOfImage, /* 137 */
    Tk_NameOfJoinStyle, /* 138 */
    Tk_NameOfJustify, /* 139 */
    Tk_NameOfRelief, /* 140 */
    Tk_NameToWindow, /* 141 */
    Tk_OwnSelection, /* 142 */
    Tk_ParseArgv, /* 143 */
    Tk_PhotoPutBlock, /* 144 */
    Tk_PhotoPutZoomedBlock, /* 145 */
    Tk_PhotoGetImage, /* 146 */
    Tk_PhotoBlank, /* 147 */
    Tk_PhotoExpand, /* 148 */
    Tk_PhotoGetSize, /* 149 */
    Tk_PhotoSetSize, /* 150 */
    Tk_PointToChar, /* 151 */
    Tk_PostscriptFontName, /* 152 */
    Tk_PreserveColormap, /* 153 */
    Tk_QueueWindowEvent, /* 154 */
    Tk_RedrawImage, /* 155 */
    Tk_ResizeWindow, /* 156 */
    Tk_RestackWindow, /* 157 */
    Tk_RestrictEvents, /* 158 */
    Tk_SafeInit, /* 159 */
    Tk_SetAppName, /* 160 */
    Tk_SetBackgroundFromBorder, /* 161 */
    Tk_SetClass, /* 162 */
    Tk_SetGrid, /* 163 */
    Tk_SetInternalBorder, /* 164 */
    Tk_SetWindowBackground, /* 165 */
    Tk_SetWindowBackgroundPixmap, /* 166 */
    Tk_SetWindowBorder, /* 167 */
    Tk_SetWindowBorderWidth, /* 168 */
    Tk_SetWindowBorderPixmap, /* 169 */
    Tk_SetWindowColormap, /* 170 */
    Tk_SetWindowVisual, /* 171 */
    Tk_SizeOfBitmap, /* 172 */
    Tk_SizeOfImage, /* 173 */
    Tk_StrictMotif, /* 174 */
    Tk_TextLayoutToPostscript, /* 175 */
    Tk_TextWidth, /* 176 */
    Tk_UndefineCursor, /* 177 */
    Tk_UnderlineChars, /* 178 */
    Tk_UnderlineTextLayout, /* 179 */
    Tk_Ungrab, /* 180 */
    Tk_UnmaintainGeometry, /* 181 */
    Tk_UnmapWindow, /* 182 */
    Tk_UnsetGrid, /* 183 */
    Tk_UpdatePointer, /* 184 */
    Tk_AllocBitmapFromObj, /* 185 */
    Tk_Alloc3DBorderFromObj, /* 186 */
    Tk_AllocColorFromObj, /* 187 */
    Tk_AllocCursorFromObj, /* 188 */
    Tk_AllocFontFromObj, /* 189 */
    Tk_CreateOptionTable, /* 190 */
    Tk_DeleteOptionTable, /* 191 */
    Tk_Free3DBorderFromObj, /* 192 */
    Tk_FreeBitmapFromObj, /* 193 */
    Tk_FreeColorFromObj, /* 194 */
    Tk_FreeConfigOptions, /* 195 */
    Tk_FreeSavedOptions, /* 196 */
    Tk_FreeCursorFromObj, /* 197 */
    Tk_FreeFontFromObj, /* 198 */
    Tk_Get3DBorderFromObj, /* 199 */
    Tk_GetAnchorFromObj, /* 200 */
    Tk_GetBitmapFromObj, /* 201 */
    Tk_GetColorFromObj, /* 202 */
    Tk_GetCursorFromObj, /* 203 */
    Tk_GetOptionInfo, /* 204 */
    Tk_GetOptionValue, /* 205 */
    Tk_GetJustifyFromObj, /* 206 */
    Tk_GetMMFromObj, /* 207 */
    Tk_GetPixelsFromObj, /* 208 */
    Tk_GetReliefFromObj, /* 209 */
    Tk_GetScrollInfoObj, /* 210 */
    Tk_InitOptions, /* 211 */
    Tk_MainEx, /* 212 */
    Tk_RestoreSavedOptions, /* 213 */
    Tk_SetOptions, /* 214 */
    Tk_InitConsoleChannels, /* 215 */
    Tk_CreateConsoleWindow, /* 216 */
    Tk_CreateSmoothMethod, /* 217 */
    NULL, /* 218 */
    NULL, /* 219 */
    Tk_GetDash, /* 220 */
    Tk_CreateOutline, /* 221 */
    Tk_DeleteOutline, /* 222 */
    Tk_ConfigOutlineGC, /* 223 */
    Tk_ChangeOutlineGC, /* 224 */
    Tk_ResetOutlineGC, /* 225 */
    Tk_CanvasPsOutline, /* 226 */
    Tk_SetTSOrigin, /* 227 */
    Tk_CanvasGetCoordFromObj, /* 228 */
    Tk_CanvasSetOffset, /* 229 */
    Tk_DitherPhoto, /* 230 */
    Tk_PostscriptBitmap, /* 231 */
    Tk_PostscriptColor, /* 232 */
    Tk_PostscriptFont, /* 233 */
    Tk_PostscriptImage, /* 234 */
    Tk_PostscriptPath, /* 235 */
    Tk_PostscriptStipple, /* 236 */
    Tk_PostscriptY, /* 237 */
    Tk_PostscriptPhoto, /* 238 */
};

/* !END!: Do not edit above this line. */
