/*
 * dirent.h --
 *
 *	This file is a replacement for <dirent.h> in systems that
 *	support the old BSD-style <sys/dir.h> with a "struct direct".
 *
 * Copyright (c) 1991 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: dirent.h,v 1.6.8.1 2000/04/06 22:38:26 spolk Exp $
 */

#ifndef _DIRENT
#define _DIRENT

#include <sys/dir.h>

#define dirent direct

#endif /* _DIRENT */
