/* BFD back-end for Intel 386 PE IMAGE COFF files.
   Copyright 1995 Free Software Foundation, Inc.

This file is part of BFD, the Binary File Descriptor library.

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

#include "bfd.h"
#include "sysdep.h"

#define TARGET_SYM nt_alphapei_vec
#define TARGET_NAME "pei-alpha"
#define IMAGE_BASE NT_IMAGE_BASE
#define COFF_IMAGE_WITH_PE
#define COFF_WITH_PE
#define PCRELOFFSET true
/* #define TARGET_UNDERSCORE '_' ... no _ for alpha */
#define COFF_LONG_SECTION_NAMES
#define COFF_LONG_FILENAMES
#define INPUT_FORMAT nt_alphape_vec

#define ALIGN_HEADER_TO 0x1000

#include "coff-nt_alpha.c"



