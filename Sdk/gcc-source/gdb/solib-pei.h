/* Handle shared libraries for GDB, the GNU Debugger.
   Copyright 2000
   Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "../libexec/libdl/include/link.h" // in libexec for now, whence?

#define ERROR_ADDR  0xffffffff          /* a non-zero address that's an error */

struct lm_info {
    CORE_ADDR lmoffset;			/* Offset between Image base and real */
};

