/* Host-dependent definitions for i386 running Interix.
   Copyright 2002 Free Software Foundation, Inc.

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

#ifndef XM_I386INTERIX_H
#define XM_I386INTERIX_H

/* FIXME: This file is only temporary, and the final intent is to get
   rid of each macro defined here. This will be done later.  */

/*  Used in coffread.c to adjust the symbol offsets.  */
#define ADJUST_OBJFILE_OFFSETS(objfile, type) \
    pei_adjust_objfile_offsets(objfile, type)

extern CORE_ADDR bfd_getImageBase(bfd *abfd);
#define NONZERO_LINK_BASE(abfd) bfd_getImageBase(abfd)

#endif /* XM_I386INTERIX_H */

