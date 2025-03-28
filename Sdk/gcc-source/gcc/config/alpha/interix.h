/* Definitions of target machine for GNU compiler, for DEC Alpha
   running Windows/NT.
   Copyright (C) 1995, 1996 Free Software Foundation, Inc.
   Donn Terry, Softway Systems, Inc.
   From code
       Contributed by Richard Kenner (kenner@vlsi1.ultra.nyu.edu)

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Names to predefine in the preprocessor for this target machine.  */

#define HANDLE_SYSV_PRAGMA
#undef HANDLE_PRAGMA_WEAK  /* until the link format can handle it */

/* For the sake of libgcc2.c, indicate target supports atexit.  */
#define HAVE_ATEXIT

/* cpp handles __STDC__ */
/* The three "Alpha" defines on the first such line are from the CLAXP spec */
#undef CPP_PREDEFINES
#define CPP_PREDEFINES " \
  -D__INTERIX \
  -D__OPENNT \
  -D__Alpha_AXP -D_M_ALPHA -D_ALPHA_  \
  -D__alpha -D__alpha__\
  -D__stdcall= \
  -D__cdecl= \
  -Asystem(unix) -Asystem(interix) -Asystem(interix) -Acpu(alpha) -Amachine(alpha)"

#undef CPP_SPEC
/* Write out the correct language type definition for the header files.  
   Unless we have assembler language, write out the symbols for C.
   cpp_cpu is an Intel specific variant. See i386.h
   mieee is an alpha specific variant.  Cross polination a bad idea.
   */
#define CPP_SPEC "\
%{!.S:	-D__LANGUAGE_C__ -D__LANGUAGE_C %{!ansi:-DLANGUAGE_C}}  \
%{.S:	-D__LANGUAGE_ASSEMBLY__ -D__LANGUAGE_ASSEMBLY %{!ansi:-DLANGUAGE_ASSEMBLY}} \
%{.cc:	-D__LANGUAGE_C_PLUS_PLUS__ -D__LANGUAGE_C_PLUS_PLUS -D__cplusplus} \
%{.cxx:	-D__LANGUAGE_C_PLUS_PLUS__ -D__LANGUAGE_C_PLUS_PLUS -D__cplusplus} \
%{.C:	-D__LANGUAGE_C_PLUS_PLUS__ -D__LANGUAGE_C_PLUS_PLUS -D__cplusplus} \
%{.m:	-D__LANGUAGE_OBJECTIVE_C__ -D__LANGUAGE_OBJECTIVE_C} \
%{mieee:-D_IEEE_FP} \
%{mieee-with-inexact:-D_IEEE_FP -D_IEEE_FP_INEXACT} \
%{posix:-D_POSIX_SOURCE}"

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (alpha Interix)");

#define YES_UNDERSCORES

/* Use gas spec; below is for MS assembler
#undef ASM_SPEC
#undef ASM_FINAL_SPEC
#define ASM_SPEC "-nopp -nologo %{g:-Zi}"
*/

/* Pointer is 32 bits but the hardware has 64-bit addresses, sign extended. */
#undef POINTER_SIZE
#define POINTER_SIZE 32
#define POINTERS_EXTEND_UNSIGNED 0

/* We don't change Pmode to the "obvious" SI mode... the above appears
   to affect the in-memory size; we want the registers to stay DImode
   to match the md file */

/* "long" is 32 bits.  */
#undef LONG_TYPE_SIZE
#define LONG_TYPE_SIZE 32

#undef WCHAR_TYPE
#define WCHAR_TYPE "short unsigned int"
#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 16

#undef DBX_DEBUGGING_INFO
#undef MIPS_DEBUGGING_INFO

#define ASM_LOAD_ADDR(loc, reg)   "     lda " #reg "," #loc "\n"
#include "config/interix.h"

/* alpha.h sets this, but it doesn't apply to us */
#undef OBJECT_FORMAT_ECOFF
#undef OBJECT_FORMAT_COFF

#undef ASM_FILE_START
#define ASM_FILE_START(FILE)					\
{								\
  alpha_write_verstamp (FILE);					\
  fprintf (FILE, "\t.set noreorder\n");				\
  fprintf (FILE, "\t.set volatile\n");                          \
  fprintf (FILE, "\t.set noat\n");				\
  fprintf (FILE, "\t.globl\t_fltused\n");			\
  ASM_OUTPUT_SOURCE_FILENAME (FILE, main_input_filename);	\
}

/* LINK_SPEC */

/* MD_STARTFILE_PREFIX */

/* ASM_OUTPUT_LOOP_ALIGN; ASM_OUTPUT_ALIGN_CODE */

/* WinNT (and thus Interix) use unsigned int */
#define SIZE_TYPE "unsigned int"

/* Codegen macro overrides for NT internal conventions */

/* the below are ecoff specific... we don't need them, so
   undef them (they'll get a default later) */

#undef PUT_SDB_BLOCK_START
#undef PUT_SDB_BLOCK_END

/* the following are OSF linker (not gld) specific... we don't want them */
#undef HAS_INIT_SECTION
#undef LD_INIT_SWITCH
#undef LD_FINI_SWITCH


/* The following are needed for C++, but also needed for profiling */

/* Support const sections and the ctors and dtors sections for g++.
   Note that there appears to be two different ways to support const
   sections at the moment.  You can either #define the symbol
   READONLY_DATA_SECTION (giving it some code which switches to the
   readonly data section) or else you can #define the symbols
   EXTRA_SECTIONS, EXTRA_SECTION_FUNCTIONS, SELECT_SECTION, and
   SELECT_RTX_SECTION.  We do both here just to be on the safe side.  */

#define USE_CONST_SECTION	1

#define CONST_SECTION_ASM_OP	".rdata"

/* Define the pseudo-ops used to switch to the .ctors and .dtors sections.

   Note that we want to give these sections the SHF_WRITE attribute
   because these sections will actually contain data (i.e. tables of
   addresses of functions in the current root executable or shared library
   file) and, in the case of a shared library, the relocatable addresses
   will have to be properly resolved/relocated (and then written into) by
   the dynamic linker when it actually attaches the given shared library
   to the executing process.  (Note that on SVR4, you may wish to use the
   `-z text' option to the ELF linker, when building a shared library, as
   an additional check that you are doing everything right.  But if you do
   use the `-z text' option when building a shared library, you will get
   errors unless the .ctors and .dtors sections are marked as writable
   via the SHF_WRITE attribute.)  */

#define CTORS_SECTION_ASM_OP	".ctors"
#define DTORS_SECTION_ASM_OP	".dtors"

/* A default list of other sections which we might be "in" at any given
   time.  For targets that use additional sections (e.g. .tdesc) you
   should override this definition in the target-specific file which
   includes this file.  */

#undef EXTRA_SECTIONS
#define EXTRA_SECTIONS in_const, in_ctors, in_dtors

/* A default list of extra section function definitions.  For targets
   that use additional sections (e.g. .tdesc) you should override this
   definition in the target-specific file which includes this file.  */

#undef EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS						\
  CONST_SECTION_FUNCTION						\
  CTORS_SECTION_FUNCTION						\
  DTORS_SECTION_FUNCTION

#undef READONLY_DATA_SECTION
#define READONLY_DATA_SECTION() const_section ()

extern void text_section ();

#define CONST_SECTION_FUNCTION						\
void									\
const_section ()							\
{									\
  if (!USE_CONST_SECTION)						\
    text_section();							\
  else if (in_section != in_const)					\
    {									\
      fprintf (asm_out_file, "%s\n", CONST_SECTION_ASM_OP);		\
      in_section = in_const;						\
    }									\
}

#define CTORS_SECTION_FUNCTION						\
void									\
ctors_section ()							\
{									\
  if (in_section != in_ctors)						\
    {									\
      fprintf (asm_out_file, "%s\n", CTORS_SECTION_ASM_OP);		\
      in_section = in_ctors;						\
    }									\
}

#define DTORS_SECTION_FUNCTION						\
void									\
dtors_section ()							\
{									\
  if (in_section != in_dtors)						\
    {									\
      fprintf (asm_out_file, "%s\n", DTORS_SECTION_ASM_OP);		\
      in_section = in_dtors;						\
    }									\
}

/* The definition of this macro implies that there are cases where
   a scalar value cannot be returned in registers.

   On NT (according to the spec) anything except strings/array that fits
   in 64 bits is returned in the registers (this appears to differ from
   the rest of the Alpha family). */

#undef RETURN_IN_MEMORY
#define RETURN_IN_MEMORY(TYPE) \
  (TREE_CODE (TYPE) == ARRAY_TYPE || int_size_in_bytes(TYPE) > 8)


/* This is for PE/PEI 1-based function-relative lines */
#undef ASM_OUTPUT_SOURCE_LINE
#define ASM_OUTPUT_SOURCE_LINE(STREAM, LINE)				\
  alpha_output_lineno (STREAM, 						\
     ((sdb_begin_function_line <= -1) ? 1 :				\
	 ((LINE) - sdb_begin_function_line + 1 > 0 ?			\
	  (LINE) - sdb_begin_function_line + 1 : 1)), 0);

/* ... but it doesn't work for stabs */
#undef DBX_OUTPUT_SOURCE_LINE
#define DBX_OUTPUT_SOURCE_LINE(FILE, LINE)				\
{ if (write_symbols == SDB_DEBUG) {					\
  alpha_output_lineno (FILE, 						\
     ((sdb_begin_function_line <= -1) ? 1 :				\
	 ((LINE) - sdb_begin_function_line + 1 > 0 ?			\
	  (LINE) - sdb_begin_function_line + 1 : 1)), 0);		\
  } else if (write_symbols == DBX_DEBUG) {				\
    static int sym_lineno = 1;						\
    fprintf (FILE, ".stabn 68,0,%d,$LM%d-",				\
	     LINE, sym_lineno);						\
    assemble_name (FILE,						\
		   XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0)); \
    fprintf (FILE, "\n$LM%d:\n", sym_lineno);				\
    sym_lineno += 1;							\
  } }

#define INT_ASM_OP		".long"

/* A C statement (sans semicolon) to output an element in the table of
   global constructors.  */
#define ASM_OUTPUT_CONSTRUCTOR(FILE,NAME)				\
  do {									\
    ctors_section ();							\
    fprintf (FILE, "\t%s\t ", INT_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    fprintf (FILE, "\n");						\
  } while (0)

/* A C statement (sans semicolon) to output an element in the table of
   global destructors.  */
#define ASM_OUTPUT_DESTRUCTOR(FILE,NAME)       				\
  do {									\
    dtors_section ();                   				\
    fprintf (FILE, "\t%s\t ", INT_ASM_OP);				\
    assemble_name (FILE, NAME);              				\
    fprintf (FILE, "\n");						\
  } while (0)

/* The linker will take care of this, and having them causes problems with
   ld -r (specifically -rU). */
#define CTOR_LISTS_DEFINED_EXTERNALLY

#define SET_ASM_OP	".set"
/* Output a definition (implements alias) */
#define ASM_OUTPUT_DEF(FILE,LABEL1,LABEL2)				\
do									\
{									\
    fprintf ((FILE), "\t%s\t", SET_ASM_OP);				\
    assemble_name (FILE, LABEL1);					\
    fprintf (FILE, ",");						\
    assemble_name (FILE, LABEL2);					\
    fprintf (FILE, "\n");						\
    }									\
while (0)

/* We use the defaults, so undef the null definitions */
#undef PUT_SDB_FUNCTION_START
#undef PUT_SDB_FUNCTION_END
#undef PUT_SDB_EPILOGUE_END

#define HOST_PTR_PRINTF "%p"
#define HOST_PTR_AS_INT unsigned long

#undef PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG

#define PCC_BITFIELD_TYPE_MATTERS 1
#define PCC_BITFIELD_TYPE_TEST TYPE_NATIVE(rec)
#define GROUP_BITFIELDS_BY_ALIGN TYPE_NATIVE(rec)
