/* Definitions for hosting on WIN32, built with Microsoft Visual C/C++, for GDB.
   Copyright 1996 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "i386/xm-cygwin32.h"

#undef PRINTF_HAS_LONG_LONG
#undef HAVE_UNISTD_H
#undef HAVE_TERMIO_H
#undef HAVE_TERMIOS_H
#undef HAVE_SGTTY_H
#undef HAVE_SBRK
#define CANT_FORK

#define MALLOC_INCOMPATIBLE
#define NO_MMALLOC
#define NO_MMCHECK

#include <malloc.h>

#define SIGQUIT 3
#define SIGTRAP 5
