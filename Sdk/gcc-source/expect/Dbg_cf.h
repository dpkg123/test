/* Dbg_cf.h.  Generated automatically by configure.  */
/* This file is only to be included by the Debugger itself. */
/* Applications should only include Dbg.h. */

/*
 * Check for headers
 */
#ifndef __NIST_DBG_CF_H__
#define __NIST_DBG_CF_H__

/* #undef NO_STDLIB_H */		/* Tcl requires this name */

/*
 * Check for functions
 */
#define HAVE_STRCHR 1

#ifndef HAVE_STRCHR
#define strchr(s,c) index(s,c)
#endif /* HAVE_STRCHR */

#endif	/* __NIST_DBG_CF_H__ */
