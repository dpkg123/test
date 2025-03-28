/* tc-hppa.h -- Header file for the PA
   Copyright 1989, 1993, 1994, 1995, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* HP PA-RISC support was contributed by the Center for Software Science
   at the University of Utah.  */

/* Please refrain from exposing the world to the internals of tc-hppa.c
   when this file is included.  This means only declaring exported functions,
   (please PARAMize them!) not exporting structures and data items which
   are used solely within tc-hppa.c, etc.

   Also refrain from adding any more object file dependent code, there is
   already far too much object file format dependent code in this file.
   In theory this file should contain only exported functions, structures
   and data declarations common to all PA assemblers.  */

#ifndef _TC_HPPA_H
#define _TC_HPPA_H

#ifndef TC_HPPA
#define TC_HPPA	1
#endif

#define TARGET_BYTES_BIG_ENDIAN 1

#define TARGET_ARCH bfd_arch_hppa

#define WORKING_DOT_WORD

#ifdef OBJ_ELF
#if TARGET_ARCH_SIZE == 64
#include "bfd/elf64-hppa.h"
#ifdef TE_LINUX
#define TARGET_FORMAT "elf64-hppa-linux"
#else
#define TARGET_FORMAT "elf64-hppa"
#endif
#else /* TARGET_ARCH_SIZE == 32 */
#include "bfd/elf32-hppa.h"
#ifdef TE_LINUX
#define TARGET_FORMAT "elf32-hppa-linux"
#else
#define TARGET_FORMAT "elf32-hppa"
#endif
#endif
#endif

#ifdef OBJ_SOM
#include "bfd/som.h"
#define TARGET_FORMAT "som"
#endif

#if defined(TE_LINUX) || defined(TE_NetBSD)
/* Define to compile in an extra assembler option, -c, which enables a
   warning (once per file) when a comment is encountered.
   The hppa comment char is a `;' which tends to occur in random C asm
   statements.  A semicolon is a line separator for most assemblers.
   It's hard to find these lurking semicolons.  Thus...  */
#define WARN_COMMENTS 1
#endif

#ifdef TE_NetBSD
/* XXX the original OpenBSD code has labels without colons,
   so this is required, for now -- fredette@netbsd.org */
/* Labels are not required to have a colon for a suffix.  */
#define LABELS_WITHOUT_COLONS 1
#endif

/* FIXME.  Why oh why aren't these defined somewhere globally?  */
#ifndef FALSE
#define FALSE   (0)
#define TRUE    (!FALSE)
#endif

#define ASEC_NULL (asection *)0

/* pa_define_label gets used outside of tc-hppa.c via tc_frob_label.  */
extern void pa_define_label PARAMS ((symbolS *));

extern void parse_cons_expression_hppa PARAMS ((expressionS *));
extern void cons_fix_new_hppa PARAMS ((fragS *, int, int, expressionS *));
extern int hppa_force_relocation PARAMS ((struct fix *));

/* This gets called before writing the object file to make sure
   things like entry/exit and proc/procend pairs match.  */
extern void pa_check_eof PARAMS ((void));
#define tc_frob_file pa_check_eof

#define tc_frob_label(sym) pa_define_label (sym)

/* The PA does not need support for either of these.  */
#define tc_crawl_symbol_chain(headers) {;}
#define tc_headers_hook(headers) {;}

#define RELOC_EXPANSION_POSSIBLE
#define MAX_RELOC_EXPANSION 6

/* The PA needs to parse field selectors in .byte, etc.  */

#define TC_PARSE_CONS_EXPRESSION(EXP, NBYTES) \
  parse_cons_expression_hppa (EXP)
#define TC_CONS_FIX_NEW cons_fix_new_hppa

/* On the PA, an equal sign often appears as a condition or nullification
   completer in an instruction.  This can be detected by checking the
   previous character, if the character is a comma, then the equal is
   being used as part of an instruction.  */
#define TC_EQUAL_IN_INSN(C, PTR)	((C) == ',')

/* Similarly for an exclamation point.  It is used in FP comparison
   instructions and as an end of line marker.  When used in an instruction
   it will always follow a comma.  */
#define TC_EOL_IN_INSN(PTR)	(*(PTR) == '!' && (PTR)[-1] == ',')

int hppa_fix_adjustable PARAMS((struct fix *));
#define tc_fix_adjustable hppa_fix_adjustable

#define EXTERN_FORCE_RELOC 1

/* Because of the strange PA calling conventions, it is sometimes
   necessary to emit a relocation for a call even though it would
   normally appear safe to handle it completely within GAS.  */
#define TC_FORCE_RELOCATION(FIX) hppa_force_relocation (FIX)

#ifdef OBJ_SOM
/* If a symbol is imported, but never used, then the symbol should
   *not* end up in the symbol table.  Likewise for absolute symbols
   with local scope.  */
#define tc_frob_symbol(sym,punt) \
    if ((S_GET_SEGMENT (sym) == &bfd_und_section && ! symbol_used_p (sym)) \
	|| (S_GET_SEGMENT (sym) == &bfd_abs_section \
	    && ! S_IS_EXTERNAL (sym))) \
      punt = 1

/* We need to be able to make relocations involving the difference of
   two symbols.  This includes the difference of two symbols when
   one of them is undefined (this comes up in PIC code generation).

   We don't define DIFF_EXPR_OK because it does the wrong thing if
   the add symbol is undefined and the sub symbol is a symbol in
   the same section as the relocation.  We also need some way to
   specialize some code in adjust_reloc_syms.  */
#define UNDEFINED_DIFFERENCE_OK
#endif

/* Values passed to md_apply_fix3 don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#ifdef OBJ_ELF
/* Handle .type psuedo.  Given a type string of `millicode', set the
   internal elf symbol type to STT_PARISC_MILLI, and return
   BSF_FUNCTION for the BFD symbol type.  */
#define md_elf_symbol_type(name, sym, elf)				\
  ((strcmp ((name), "millicode") == 0					\
    || strcmp ((name), "STT_PARISC_MILLI") == 0)			\
   ? (((elf)->internal_elf_sym.st_info = ELF_ST_INFO			\
       (ELF_ST_BIND ((elf)->internal_elf_sym.st_info), STT_PARISC_MILLI)\
       ), BSF_FUNCTION)							\
   : -1)

#define tc_frob_symbol(sym,punt) \
  { \
    if ((S_GET_SEGMENT (sym) == &bfd_und_section && ! symbol_used_p (sym)) \
	|| (S_GET_SEGMENT (sym) == &bfd_abs_section \
	    && ! S_IS_EXTERNAL (sym)) \
	|| strcmp (S_GET_NAME (sym), "$global$") == 0 \
	|| strcmp (S_GET_NAME (sym), "$PIC_pcrel$0") == 0) \
      punt = 1; \
  }

#define elf_tc_final_processing	elf_hppa_final_processing
void elf_hppa_final_processing PARAMS ((void));

#define DWARF2_LINE_MIN_INSN_LENGTH 4
#endif /* OBJ_ELF */

#define md_operand(x)

/* Allow register expressions to be treated as absolute expressions.
   A silly fudge required for backwards compatibility.  */
#define md_optimize_expr hppa_force_reg_syms_absolute

int hppa_force_reg_syms_absolute
  PARAMS ((expressionS *, operatorT, expressionS *));

#define TC_FIX_TYPE PTR
#define TC_INIT_FIX_DATA(FIX) ((FIX)->tc_fix_data = NULL)

#endif /* _TC_HPPA_H */
