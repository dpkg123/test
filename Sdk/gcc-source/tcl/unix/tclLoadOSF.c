/* 
 * tclLoadOSF.c --
 *
 *	This procedure provides a version of the TclLoadFile that works
 *	under OSF/1 1.0/1.1/1.2 and related systems, utilizing the old OSF/1
 *	/sbin/loader and /usr/include/loader.h.  OSF/1 versions from 1.3 and
 *	on use ELF, rtld, and dlopen()[/usr/include/ldfcn.h].
 *
 *	This is useful for:
 *		OSF/1 1.0, 1.1, 1.2 (from OSF)
 *			includes: MK4 and AD1 (from OSF RI)
 *		OSF/1 1.3 (from OSF) using ROSE
 *		HP OSF/1 1.0 ("Acorn") using COFF
 *
 *	This is likely to be useful for:
 *		Paragon OSF/1 (from Intel) 
 *		HI-OSF/1 (from Hitachi) 
 *
 *	This is NOT to be used on:
 *		Digitial Alpha OSF/1 systems
 *		OSF/1 1.3 or later (from OSF) using ELF
 *			includes: MK6, MK7, AD2, AD3 (from OSF RI)
 *
 *	This approach to things was utter @&^#; thankfully,
 * 	OSF/1 eventually supported dlopen().
 *
 *	John Robert LoVerso <loverso@freebsd.osf.org>
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclLoadOSF.c,v 1.6.8.1 2000/04/06 22:38:39 spolk Exp $
 */

#include "tclInt.h"
#include <sys/types.h>
#include <loader.h>

/*
 *----------------------------------------------------------------------
 *
 * TclpLoadFile --
 *
 *	Dynamically loads a binary code file into memory and returns
 *	the addresses of two procedures within that file, if they
 *	are defined.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs, an error
 *	message is left in the interp's result.  *proc1Ptr and *proc2Ptr
 *	are filled in with the addresses of the symbols given by
 *	*sym1 and *sym2, or NULL if those symbols can't be found.
 *
 * Side effects:
 *	New code suddenly appears in memory.
 *
 *----------------------------------------------------------------------
 */

int
TclpLoadFile(interp, fileName, sym1, sym2, proc1Ptr, proc2Ptr, clientDataPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *fileName;		/* Name of the file containing the desired
				 * code. */
    char *sym1, *sym2;		/* Names of two procedures to look up in
				 * the file's symbol table. */
    Tcl_PackageInitProc **proc1Ptr, **proc2Ptr;
				/* Where to return the addresses corresponding
				 * to sym1 and sym2. */
    ClientData *clientDataPtr;	/* Filled with token for dynamically loaded
				 * file which will be passed back to 
				 * TclpUnloadFile() to unload the file. */
{
    ldr_module_t lm;
    char *pkg;

    lm = (Tcl_PackageInitProc *) load(fileName, LDR_NOFLAGS);
    if (lm == LDR_NULL_MODULE) {
	Tcl_AppendResult(interp, "couldn't load file \"", fileName,
	    "\": ", Tcl_PosixError (interp), (char *) NULL);
	return TCL_ERROR;
    }

    *clientDataPtr = NULL;
    
    /*
     * My convention is to use a [OSF loader] package name the same as shlib,
     * since the idiots never implemented ldr_lookup() and it is otherwise
     * impossible to get a package name given a module.
     *
     * I build loadable modules with a makefile rule like 
     *		ld ... -export $@: -o $@ $(OBJS)
     */
    if ((pkg = strrchr(fileName, '/')) == NULL)
	pkg = fileName;
    else
	pkg++;
    *proc1Ptr = ldr_lookup_package(pkg, sym1);
    *proc2Ptr = ldr_lookup_package(pkg, sym2);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpUnloadFile --
 *
 *	Unloads a dynamically loaded binary code file from memory.
 *	Code pointers in the formerly loaded file are no longer valid
 *	after calling this function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Does nothing.  Can anything be done?
 *
 *----------------------------------------------------------------------
 */

void
TclpUnloadFile(clientData)
    ClientData clientData;	/* ClientData returned by a previous call
				 * to TclpLoadFile().  The clientData is 
				 * a token that represents the loaded 
				 * file. */
{
}

/*
 *----------------------------------------------------------------------
 *
 * TclGuessPackageName --
 *
 *	If the "load" command is invoked without providing a package
 *	name, this procedure is invoked to try to figure it out.
 *
 * Results:
 *	Always returns 0 to indicate that we couldn't figure out a
 *	package name;  generic code will then try to guess the package
 *	from the file name.  A return value of 1 would have meant that
 *	we figured out the package name and put it in bufPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGuessPackageName(fileName, bufPtr)
    char *fileName;		/* Name of file containing package (already
				 * translated to local form if needed). */
    Tcl_DString *bufPtr;	/* Initialized empty dstring.  Append
				 * package name to this if possible. */
{
    return 0;
}

