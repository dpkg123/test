/* tc-mn10300.h -- Header file for tc-mn10300.c.
   Copyright 1996, 1997, 2000, 2001, 2002 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#define TC_MN10300

#define TARGET_BYTES_BIG_ENDIAN 0

#ifndef BFD_ASSEMBLER
 #error MN10300 support requires BFD_ASSEMBLER
#endif

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_mn10300

#define TARGET_FORMAT "elf32-mn10300"

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

/* For fixup and relocation handling.  */
#define TC_FORCE_RELOCATION(fixp) mn10300_force_relocation (fixp)
extern int mn10300_force_relocation PARAMS ((struct fix *));

/* Do not adjust relocations involving symbols in code sections,
   because it breaks linker relaxations.  This could be fixed in the
   linker, but this fix is simpler, and it pretty much only affects
   object size a little bit.  */
#define TC_FORCE_RELOCATION_SUB_SAME(FIX, SEG)	\
  (! SEG_NORMAL (SEG)				\
   || ((SEG)->flags & SEC_CODE) != 0)

/* Fixup debug sections since we will never relax them.  */
#define TC_LINKRELAX_FIXUP(seg) (seg->flags & SEC_ALLOC)

#define md_operand(x)

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

#define md_number_to_chars number_to_chars_littleendian

/* Don't bother to adjust relocs.  */
#define tc_fix_adjustable(FIX) 0
/* #define tc_fix_adjustable(FIX) mn10300_fix_adjustable (FIX) */
extern boolean mn10300_fix_adjustable PARAMS ((struct fix *));

/* We do relaxing in the assembler as well as the linker.  */
extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

#define DWARF2_LINE_MIN_INSN_LENGTH 1
