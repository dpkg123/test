/* Utility to pick a temporary filename prefix.
   Copyright (C) 1996, 1997, 1998, 2001 Free Software Foundation, Inc.


This is brought down to this version of gcc from a newer one so that
we can use the randomized temp file name mechanism.  This is not
a particularly minimal port, and the history intentionally shows.
If you're working on a newer gcc, you shouldn't be using this.


This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include <string.h>

#include <sys/file.h>   /* May get R_OK, etc. on some systems.  */
#include <stdio.h>	/* May get P_tmpdir.  */
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define PARAMS(x) x

/* Subroutine of choose_tmpdir.
   If BASE is non-NULL, return it.
   Otherwise it checks if DIR is a usable directory.
   If success, DIR is returned.
   Otherwise NULL is returned.  */

static const char *try PARAMS ((const char *, const char *));

static const char *
try (dir, base)
     const char *dir, *base;
{
  if (base != 0)
    return base;
  if (dir != 0
      && access (dir, R_OK | W_OK | X_OK) == 0)
    return dir;
  return 0;
}

extern int mkstemps PARAMS ((char *, int));

extern char *xmalloc PARAMS((size_t));

/* '/' works just fine on MS-DOS based systems.  */
#ifndef DIR_SEPARATOR
#define DIR_SEPARATOR '/'
#endif

/* Name of temporary file.
   mktemp requires 6 trailing X's.  */
#define TEMP_FILE "ccXXXXXX"
#define TEMP_FILE_LEN (sizeof(TEMP_FILE) - 1)


static const char tmp[] = { DIR_SEPARATOR, 't', 'm', 'p', 0 };
static const char usrtmp[] =
{ DIR_SEPARATOR, 'u', 's', 'r', DIR_SEPARATOR, 't', 'm', 'p', 0 };
static const char vartmp[] =
{ DIR_SEPARATOR, 'v', 'a', 'r', DIR_SEPARATOR, 't', 'm', 'p', 0 };

static char *memoized_tmpdir;

/*

@deftypefn Replacement char* choose_tmpdir ()

Returns a pointer to a directory path suitable for creating temporary
files in.

@end deftypefn

*/

char *
choose_tmpdir ()
{
  const char *base = 0;
  char *tmpdir;
  unsigned int len;

  if (memoized_tmpdir)
    return memoized_tmpdir;

  base = try (getenv ("TMPDIR"), base);
  base = try (getenv ("TMP"), base);
  base = try (getenv ("TEMP"), base);

#ifdef P_tmpdir
  base = try (P_tmpdir, base);
#endif

  /* Try /var/tmp, /usr/tmp, then /tmp.  */
  base = try (vartmp, base);
  base = try (usrtmp, base);
  base = try (tmp, base);
 
  /* If all else fails, use the current directory!  */
  if (base == 0)
    base = ".";

  /* Append DIR_SEPARATOR to the directory we've chosen
     and return it.  */
  len = strlen (base);
  tmpdir = xmalloc (len + 2);
  strcpy (tmpdir, base);
  tmpdir[len] = DIR_SEPARATOR;
  tmpdir[len+1] = '\0';

  memoized_tmpdir = tmpdir;
  return tmpdir;
}

/*

@deftypefn Replacement char* make_temp_file (const char *@var{suffix})

Return a temporary file name (as a string) or @code{NULL} if unable to
create one.  @var{suffix} is a suffix to append to the file name.  The
string is @code{malloc}ed, and the temporary file has been created.

@end deftypefn

*/

char *
make_temp_file (suffix)
     const char *suffix;
{
  const char *base = choose_tmpdir ();
  char *temp_filename;
  int base_len, suffix_len;
  int fd;

  if (suffix == 0)
    suffix = "";

  base_len = strlen (base);
  suffix_len = strlen (suffix);

  temp_filename = xmalloc (base_len
			   + TEMP_FILE_LEN
			   + suffix_len + 1);
  strcpy (temp_filename, base);
  strcpy (temp_filename + base_len, TEMP_FILE);
  strcpy (temp_filename + base_len + TEMP_FILE_LEN, suffix);

  fd = mkstemps (temp_filename, suffix_len);
  /* If mkstemps failed, then something bad is happening.  Maybe we should
     issue a message about a possible security attack in progress?  */
  if (fd == -1)
    abort ();
  /* Similarly if we can not close the file.  */
  if (close (fd))
    abort ();
  return temp_filename;
}

/* Copyright (C) 1991, 1992, 1996, 1998 Free Software Foundation, Inc.
   This file is derived from mkstemp.c from the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/time.h>

/* We need to provide a type for gcc_uint64_t.  */
#ifdef __GNUC__
__extension__ typedef unsigned long long gcc_uint64_t;
#else
typedef unsigned long gcc_uint64_t;
#endif

#ifndef TMP_MAX
#define TMP_MAX 16384
#endif

/*

@deftypefn Replacement int mkstemps (char *@var{template}, int @var{suffix_len})

Generate a unique temporary file name from @var{template}.
@var{template} has the form:

@example
   @var{path}/ccXXXXXX@var{suffix}
@end example

@var{suffix_len} tells us how long @var{suffix} is (it can be zero
length).  The last six characters of @var{template} before @var{suffix}
must be @samp{XXXXXX}; they are replaced with a string that makes the
filename unique.  Returns a file descriptor open on the file for
reading and writing.

@end deftypefn

*/

int
mkstemps (template, suffix_len)
     char *template;
     int suffix_len;
{
  static const char letters[]
    = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static gcc_uint64_t value;
  struct timeval tv;
  char *XXXXXX;
  size_t len;
  int count;

  len = strlen (template);

  if ((int) len < 6 + suffix_len
      || strncmp (&template[len - 6 - suffix_len], "XXXXXX", 6))
    {
      return -1;
    }

  XXXXXX = &template[len - 6 - suffix_len];

  /* Get some more or less random data.  */
  gettimeofday (&tv, NULL);
  value += ((gcc_uint64_t) tv.tv_usec << 16) ^ tv.tv_sec ^ getpid ();

  for (count = 0; count < TMP_MAX; ++count)
    {
      gcc_uint64_t v = value;
      int fd;

      /* Fill in the random bits.  */
      XXXXXX[0] = letters[v % 62];
      v /= 62;
      XXXXXX[1] = letters[v % 62];
      v /= 62;
      XXXXXX[2] = letters[v % 62];
      v /= 62;
      XXXXXX[3] = letters[v % 62];
      v /= 62;
      XXXXXX[4] = letters[v % 62];
      v /= 62;
      XXXXXX[5] = letters[v % 62];

#ifdef VMS
      fd = open (template, O_RDWR|O_CREAT|O_EXCL, 0600, "fop=tmd");
#else
      fd = open (template, O_RDWR|O_CREAT|O_EXCL, 0600);
#endif
      if (fd >= 0)
	/* The file does not exist.  */
	return fd;

      /* This is a random value.  It is only necessary that the next
	 TMP_MAX values generated by adding 7777 to VALUE are different
	 with (module 2^32).  */
      value += 7777;
    }

  /* We return the null string if we can't find a unique file name.  */
  template[0] = '\0';
  return -1;
}
