/* Convenience header for conditional use of GNU <libintl.h>.
   Copyright (C) 1995-1998, 2000-2002, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#ifndef _LIBGETTEXT_H
#define _LIBGETTEXT_H 1

/* NLS can be disabled through the configure --disable-nls option.  */
#if ENABLE_NLS

/* ADR: Need this so gcc -g without -O works. */
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif /* HAVE_LOCALE_H */

/* Get declarations of GNU message catalog functions.  */
# include <libintl.h>

#else

/* Solaris /usr/include/locale.h includes /usr/include/libintl.h, which
   chokes if dcgettext is defined as a macro.  So include it now, to make
   later inclusions of <locale.h> a NOP.  We don't include <libintl.h>
   as well because people using "gettext.h" will not include <libintl.h>,
   and also including <libintl.h> would fail on SunOS 4, whereas <locale.h>
   is OK.  */
/* ADR: Include <locale.h> even if not ENABLE_NLS so can pay attention
 * to locale number formats, etc.
 */
#if defined(__sun) || defined(HAVE_LOCALE_H)
# include <locale.h>
#endif

/* Disabled NLS.
   The casts to 'const char *' serve the purpose of producing warnings
   for invalid uses of the value returned from these functions.
   On pre-ANSI systems without 'const', the config.h file is supposed to
   contain "#define const".  */
/* ADR: BOGUS. Remove const. 19 Feb 2002 */
# define gettext(Msgid) ((char *) (Msgid))
# define dgettext(Domainname, Msgid) ((char *) (Msgid))
# define dcgettext(Domainname, Msgid, Category) ((char *) (Msgid))
# define ngettext(Msgid1, Msgid2, N) \
    ((N) == 1 ? (char *) (Msgid1) : (char *) (Msgid2))
# define dngettext(Domainname, Msgid1, Msgid2, N) \
    ((N) == 1 ? (char *) (Msgid1) : (char *) (Msgid2))
# define dcngettext(Domainname, Msgid1, Msgid2, N, Category) \
    ((N) == 1 ? (char *) (Msgid1) : (char *) (Msgid2))
# define textdomain(Domainname) ((char *) (Domainname))
# define bindtextdomain(Domainname, Dirname) ((char *) (Dirname))
# define bind_textdomain_codeset(Domainname, Codeset) ((char *) (Codeset))

#endif

/* A pseudo function call that serves as a marker for the automated
   extraction of messages, but does not call gettext().  The run-time
   translation is done at a different place in the code.
   The argument, String, should be a literal string.  Concatenated strings
   and other string expressions won't work.
   The macro's expansion is not parenthesized, so that it is suitable as
   initializer for static 'char[]' or 'const char[]' variables.  */
#define gettext_noop(String) String

#endif /* _LIBGETTEXT_H */
