/* xstrdup.c -- copy a string with out of memory checking
   Copyright (C) 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef lint
static const char SSIRCSid[]="$Header: /E/interix/src/cpio/xstrdup.c,v 1.2 1997/10/09 15:30:47 SSI_DEV+bruce Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#include <string.h>
#else
#include <strings.h>
#endif
char *xmalloc ();

/* Return a newly allocated copy of STRING.  */

char *
xstrdup (string)
     char *string;
{
  return strcpy (xmalloc (strlen (string) + 1), string);
}
