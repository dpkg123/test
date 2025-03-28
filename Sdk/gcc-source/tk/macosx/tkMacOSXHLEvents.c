/* 
 * tkMacOSXHLEvents.c --
 *
 *        Implements high level event support for the Macintosh.  Currently, 
 *        the only event that really does anything is the Quit event.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tkMacOSXHLEvents.c,v 1.1.1.1 2002/09/24 20:38:44 kseitz Exp $
 */

#include "tkMacOSXUtil.h"
#include "tkMacOSXInt.h"

#include <Carbon/Carbon.h>

/*
 * This is a Tcl_Event structure that the Quit AppleEvent handler
 * uses to schedule the tkReallyKillMe function.
 */
 
typedef struct KillEvent {
    Tcl_Event header;                /* Information that is standard for
                                 * all events. */
    Tcl_Interp *interp;                /* Interp that was passed to the
                                 * Quit AppleEvent */
} KillEvent;

/*
 * Static functions used only in this file.
 */

static OSErr QuitHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon);
static OSErr OappHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon);
static OSErr OdocHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon);
static OSErr PrintHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon);
static OSErr ScriptHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon);
static OSErr PrefsHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon);

static int MissedAnyParameters _ANSI_ARGS_((const AppleEvent *theEvent));
static int ReallyKillMe _ANSI_ARGS_((Tcl_Event *eventPtr, int flags));

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInitAppleEvents --
 *
 *        Initilize the Apple Events on the Macintosh.  This registers the
 *        core event handlers.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

void 
TkMacOSXInitAppleEvents(
    Tcl_Interp *interp)                /* Interp to handle basic events. */
{
    OSErr err;
    AEEventHandlerUPP        OappHandlerUPP, OdocHandlerUPP,
        PrintHandlerUPP, QuitHandlerUPP, ScriptHandlerUPP,
        PrefsHandlerUPP;
        
    /*
     * Install event handlers for the core apple events.
     */
    QuitHandlerUPP = NewAEEventHandlerUPP(QuitHandler);
    err = AEInstallEventHandler(kCoreEventClass, kAEQuitApplication,
            QuitHandlerUPP, (long) interp, false);

    OappHandlerUPP = NewAEEventHandlerUPP(OappHandler);
    err = AEInstallEventHandler(kCoreEventClass, kAEOpenApplication,
            OappHandlerUPP, (long) interp, false);

    OdocHandlerUPP = NewAEEventHandlerUPP(OdocHandler);
    err = AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
            OdocHandlerUPP, (long) interp, false);

    PrintHandlerUPP = NewAEEventHandlerUPP(PrintHandler);
    err = AEInstallEventHandler(kCoreEventClass, kAEPrintDocuments,
            PrintHandlerUPP, (long) interp, false);

    PrefsHandlerUPP = NewAEEventHandlerUPP(PrefsHandler);
    err = AEInstallEventHandler(kCoreEventClass, kAEShowPreferences,
            PrefsHandlerUPP, (long) interp, false);

    if (interp != NULL) {
        ScriptHandlerUPP = NewAEEventHandlerUPP(ScriptHandler);
        err = AEInstallEventHandler('misc', 'dosc',
            ScriptHandlerUPP, (long) interp, false);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXDoHLEvent --
 *
 *        Dispatch incomming highlevel events.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Depends on the incoming event.
 *
 *----------------------------------------------------------------------
 */

int
TkMacOSXDoHLEvent(EventRecord *theEvent)
{
    return AEProcessAppleEvent(theEvent);
}

/*
 *----------------------------------------------------------------------
 *
 * QuitHandler, OappHandler, etc. --
 *
 *        These are the core Apple event handlers.  Only the Quit event does
 *        anything interesting.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */
OSErr QuitHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon)
{
    Tcl_Interp *interp = (Tcl_Interp *) handlerRefcon;
    KillEvent *eventPtr;
    
    /*
     * Call the exit command from the event loop, since you are not supposed
     * to call ExitToShell in an Apple Event Handler.  We put this at the head
     * of Tcl's event queue because this message usually comes when the Mac is
     * shutting down, and we want to kill the shell as quickly as possible.
     */
    
    eventPtr = (KillEvent *) ckalloc(sizeof(KillEvent));
    eventPtr->header.proc = ReallyKillMe;
    eventPtr->interp = interp;
     
    Tcl_QueueEvent((Tcl_Event *) eventPtr, TCL_QUEUE_HEAD);

    return noErr;
}

static OSErr 
OappHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon)
{
    return noErr;
}

/* Called when the user selects 'Preferences...' in MacOS X */
static OSErr 
PrefsHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon)
{
    Tcl_CmdInfo dummy;
    Tcl_Interp *interp = (Tcl_Interp *) handlerRefcon;
    /*
     * Don't bother if we don't have an interp or
     * the show preferences procedure doesn't exist.
     */

    if ((interp == NULL) || 
            (Tcl_GetCommandInfo(interp, "::tk::mac::ShowPreferences", &dummy)) == 0) {
            return noErr;
    }
    Tcl_GlobalEval(interp, "::tk::mac::ShowPreferences");
    return noErr;
}

static OSErr 
OdocHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon)
{
    Tcl_Interp *interp = (Tcl_Interp *) handlerRefcon;
    AEDescList fileSpecList;
    FSSpec file;
    OSErr err;
    DescType type;
    Size actual;
    long count;
    AEKeyword keyword;
    long index;
    Tcl_DString command;
    Tcl_DString pathName;
    Tcl_CmdInfo dummy;

    /*
     * Don't bother if we don't have an interp or
     * the open document procedure doesn't exist.
     */

    if ((interp == NULL) || 
            (Tcl_GetCommandInfo(interp, "::tk::mac::OpenDocument", &dummy)) == 0) {
            return noErr;
    }
    
    /*
     * If we get any errors wil retrieving our parameters
     * we just return with no error.
     */

    err = AEGetParamDesc(event, keyDirectObject,
            typeAEList, &fileSpecList);
    if (err != noErr) {
        return noErr;
    }

    err = MissedAnyParameters(event);
    if (err != noErr) {
        return noErr;
    }

    err = AECountItems(&fileSpecList, &count);
    if (err != noErr) {
        return noErr;
    }

    Tcl_DStringInit(&command);
    Tcl_DStringAppend(&command, "::tk::mac::OpenDocument", -1);
    for (index = 1; index <= count; index++) {
        int length;
        Handle fullPath;
        
        err = AEGetNthPtr(&fileSpecList, index, typeFSS,
                &keyword, &type, (Ptr) &file, sizeof(FSSpec), &actual);
        if ( err != noErr ) {
            continue;
        }

        err = FSpPathFromLocation(&file, &length, &fullPath);
        HLock(fullPath);
        Tcl_ExternalToUtfDString(NULL, *fullPath, length, &pathName);
        HUnlock(fullPath);
        DisposeHandle(fullPath);

        Tcl_DStringAppendElement(&command, Tcl_DStringValue(&pathName));
        Tcl_DStringFree(&pathName);
    }
    
    Tcl_GlobalEval(interp, Tcl_DStringValue(&command));

    Tcl_DStringFree(&command);
    return noErr;
}

static OSErr 
PrintHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon)
{
    return noErr;
}

/*
 *----------------------------------------------------------------------
 *
 * ScriptHandler --
 *
 *        This handler process the script event.  
 *
 * Results:
 *        Schedules the given event to be processed.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

static OSErr 
ScriptHandler (const AppleEvent * event, AppleEvent * reply, long handlerRefcon)
{
    OSErr theErr;
    AEDescList theDesc;
    int tclErr = -1;
    Tcl_Interp *interp;
    char errString[128];

    interp = (Tcl_Interp *) handlerRefcon;

    /*
     * The do script event receives one parameter that should be data or a file.
     */
    theErr = AEGetParamDesc(event, keyDirectObject, typeWildCard,
            &theDesc);
    if (theErr != noErr) {
        sprintf(errString, "AEDoScriptHandler: GetParamDesc error %d", theErr);
        theErr = AEPutParamPtr(reply, keyErrorString, typeChar, errString,
                strlen(errString));
    } else if (MissedAnyParameters(event)) {
        sprintf(errString, "AEDoScriptHandler: extra parameters");
        AEPutParamPtr(reply, keyErrorString, typeChar, errString,
                strlen(errString));
        theErr = -1771;
    } else {
        if (theDesc.descriptorType == (DescType)'TEXT') {
            Tcl_DString encodedText;
            short i;
            Size  size;
            char  * data;
            
            size = AEGetDescDataSize(&theDesc);
            
            data = (char *)ckalloc(size + 1);
            if ( !data ) {
                theErr = -1771;
            }
            else {
                   AEGetDescData(&theDesc,data,size);
                   data [ size ] = 0;
                   for (i=0; i<size; i++)
                    if (data[i] == '\r')
                     data[i] = '\n';
                   AEReplaceDescData(theDesc.descriptorType,data,size+1,&theDesc);
            }
            Tcl_ExternalToUtfDString(NULL, data, size,
                    &encodedText);
            tclErr = Tcl_GlobalEval(interp, Tcl_DStringValue(&encodedText));
            Tcl_DStringFree(&encodedText);
        } else if (theDesc.descriptorType == (DescType)'alis') {
            Boolean dummy;
            FSSpec theFSS;
            Handle fullPath;
            int length;
            AliasHandle aliasHandle;

            AEGetDescData (&theDesc,&aliasHandle,sizeof(aliasHandle ) );
            
            theErr = ResolveAlias(NULL, aliasHandle,
                    &theFSS, &dummy);
            if (theErr == noErr) {
                FSpPathFromLocation(&theFSS, &length, &fullPath);
                HLock(fullPath);
                Tcl_EvalFile(interp, *fullPath);
                HUnlock(fullPath);
                DisposeHandle(fullPath);
            } else {
                sprintf(errString, "AEDoScriptHandler: file not found");
                AEPutParamPtr(reply, keyErrorString, typeChar,
                        errString, strlen(errString));
            }
        } else {
            sprintf(errString,
                    "AEDoScriptHandler: invalid script type '%-4.4s', must be 'alis' or 'TEXT'",
                    (char *)(&theDesc.descriptorType));
            AEPutParamPtr(reply, keyErrorString, typeChar,
                    errString, strlen(errString));
            theErr = -1770;
        }
    }

    /*
     * If we actually go to run Tcl code - put the result in the reply.
     */
    if (tclErr >= 0) {
        if (tclErr == TCL_OK)  {
            AEPutParamPtr(reply, keyDirectObject, typeChar,
                Tcl_GetStringResult(interp),
                strlen(Tcl_GetStringResult(interp)));
        } else {
            AEPutParamPtr(reply, keyErrorString, typeChar,
                Tcl_GetStringResult(interp),
                strlen(Tcl_GetStringResult(interp)));
            AEPutParamPtr(reply, keyErrorNumber, typeInteger,
                (Ptr) &tclErr, sizeof(int));
        }
    }
        
    AEDisposeDesc(&theDesc);

    return theErr;
}

/*
 *----------------------------------------------------------------------
 *
 * ReallyKillMe --
 *
 *        This proc tries to kill the shell by running exit, and if that 
 *      has not succeeded (e.g. because someone has renamed the exit 
 *      command), calls Tcl_Exit to really kill the shell.  Called from 
 *      an event scheduled by the "Quit" AppleEvent handler.
 *
 * Results:
 *        Kills the shell.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

static int 
ReallyKillMe(Tcl_Event *eventPtr, int flags) 
{
    Tcl_Interp *interp = ((KillEvent *) eventPtr)->interp;
    if (interp != NULL) {
        Tcl_GlobalEval(interp, "exit");
    }
    Tcl_Exit(0);
    
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * MissedAnyParameters --
 *
 *        Checks to see if parameters are still left in the event.  
 *
 * Results:
 *        True or false.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */
 
static int 
MissedAnyParameters(
    const AppleEvent *theEvent)
{
   DescType returnedType;
   Size actualSize;
   OSErr err;

   err = AEGetAttributePtr(theEvent, keyMissedKeywordAttr, typeWildCard, 
                   &returnedType, NULL, 0, &actualSize);
   
   return (err != errAEDescNotFound);
}
