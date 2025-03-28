/*
 * dirent.h --
 *
 *	Declarations of a library of directory-reading procedures
 *	in the POSIX style ("struct dirent").
 *
 * Copyright (c) 1991 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: dirent2.h,v 1.6.8.1 2000/04/06 22:38:26 spolk Exp $
 */

#ifndef _DIRENT
#define _DIRENT

#ifndef _TCL
#include <tcl.h>
#endif

/*
 * Dirent structure, which holds information about a single
 * directory entry.
 */

#define MAXNAMLEN 255
#define DIRBLKSIZ 512

struct dirent {
    long d_ino;			/* Inode number of entry */
    short d_reclen;		/* Length of this record */
    short d_namlen;		/* Length of string in d_name */
    char d_name[MAXNAMLEN + 1];	/* Name must be no longer than this */
};

/*
 * State that keeps track of the reading of a directory (clients
 * should never look inside this structure;  the fields should
 * only be accessed by the library procedures).
 */

typedef struct _dirdesc {
    int dd_fd;
    long dd_loc;
    long dd_size;
    char dd_buf[DIRBLKSIZ];
} DIR;

/*
 * Procedures defined for reading directories:
 */

extern void		closedir _ANSI_ARGS_((DIR *dirp));
extern DIR *		opendir _ANSI_ARGS_((char *name));
extern struct dirent *	readdir _ANSI_ARGS_((DIR *dirp));

#endif /* _DIRENT */
