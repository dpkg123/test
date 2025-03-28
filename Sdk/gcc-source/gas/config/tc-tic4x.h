/* tc-tic4x.h -- Assemble for the Texas TMS320C[34]X.
   Copyright (C) 1997, 2002 Free Software Foundation.
   
   Contributed by Michael P. Hayes (m.hayes@elec.canterbury.ac.nz)

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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define TC_TIC4X
#define C4X

#ifndef BFD_ASSEMBLER
#error TMS320C4x requires BFD_ASSEMBLER
#endif

#define TARGET_ARCH bfd_arch_tic4x

#define WORKING_DOT_WORD

/* There are a number of different formats used for local labels.  gas
   expects local labels either of the form `10$:' or `n:', where n is
   a single digit.  When LOCAL_LABEL_DOLLARS is defined labels of the
   form `10$:' are expected.  When LOCAL_LABEL_FB is defined labels of
   the form `n:' are expected.  The latter are expected to be referred
   to using `nf' for a forward reference of `nb' for a backward
   reference.

   The local labels expected by the TI tools are of the form `$n:',
   where the colon is optional.  Now the $ character is considered to
   be valid symbol name character, so gas doesn't recognise our local
   symbols by default.  Defining LEX_DOLLAR to be 1 means that gas
   won't allow labels starting with $ and thus the hook
   tc_unrecognized_line() will be called from read.c.  We can thus
   parse lines starting with $n as having local labels.

   The other problem is the forward reference of local labels.  If a
   symbol is undefined, symbol_make() calls the md_undefined_symbol()
   hook where we create a local label if recognised.  */

/* Don't stick labels starting with 'L' into symbol table of COFF file. */
#define LOCAL_LABEL(name) ((name)[0] == '$' || (name)[0] == 'L')

#define TARGET_BYTES_BIG_ENDIAN	0
#define OCTETS_PER_BYTE_POWER 	2

#define TARGET_ARCH		bfd_arch_tic4x
#define BFD_ARCH                TARGET_ARCH

#define TC_COUNT_RELOC(x) 	(x->fx_addsy)
#define TC_CONS_RELOC 		RELOC_32
#define TC_COFF_FIX2RTYPE(fixP) tc_coff_fix2rtype (fixP)
#define TC_COFF_SIZEMACHDEP(frag) tc_coff_sizemachdep (frag)
#define NEED_FX_R_TYPE

#define reloc_type 		int

#define NO_RELOC 		0

/* Labels are not required to have a colon for a suffix.  */
#define LABELS_WITHOUT_COLONS 	1

/* Use $ as the section program counter (SPC). */
#define DOLLAR_DOT

/* Accept numbers with a suffix, e.g. 0ffffh, 1010b.  */
#define NUMBERS_WITH_SUFFIX 	1

extern int c4x_unrecognized_line PARAMS ((int));
#define tc_unrecognized_line(c) c4x_unrecognized_line (c)

#define md_number_to_chars number_to_chars_littleendian

extern int c4x_do_align PARAMS ((int, const char *, int, int));
#define md_do_align(n,fill,len,max,l) if (c4x_do_align (n,fill,len,max)) goto l

/* Start of line hook to remove parallel instruction operator || */
extern void c4x_start_line PARAMS ((void));
#define md_start_line_hook() c4x_start_line()

extern void c4x_cleanup PARAMS ((void));
#define md_cleanup() c4x_cleanup()

extern void c4x_end PARAMS ((void));
#define md_end() c4x_end()

