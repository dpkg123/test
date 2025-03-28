/* bfd back-end for HP PA-RISC SOM objects.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by the Center for Software Science at the
   University of Utah.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "alloca-conf.h"
#include "bfd.h"
#include "sysdep.h"

#if defined (HOST_HPPAHPUX) || defined (HOST_HPPABSD) || defined (HOST_HPPAOSF) || defined(HOST_HPPAMPEIX)

#include "libbfd.h"
#include "som.h"
#include "safe-ctype.h"

#include <sys/param.h>
#include <signal.h>
#include <machine/reg.h>
#include <sys/file.h>

/* Magic not defined in standard HP-UX header files until 8.0 */

#ifndef CPU_PA_RISC1_0
#define CPU_PA_RISC1_0 0x20B
#endif /* CPU_PA_RISC1_0 */

#ifndef CPU_PA_RISC1_1
#define CPU_PA_RISC1_1 0x210
#endif /* CPU_PA_RISC1_1 */

#ifndef CPU_PA_RISC2_0
#define CPU_PA_RISC2_0 0x214
#endif /* CPU_PA_RISC2_0 */

#ifndef _PA_RISC1_0_ID
#define _PA_RISC1_0_ID CPU_PA_RISC1_0
#endif /* _PA_RISC1_0_ID */

#ifndef _PA_RISC1_1_ID
#define _PA_RISC1_1_ID CPU_PA_RISC1_1
#endif /* _PA_RISC1_1_ID */

#ifndef _PA_RISC2_0_ID
#define _PA_RISC2_0_ID CPU_PA_RISC2_0
#endif /* _PA_RISC2_0_ID */

#ifndef _PA_RISC_MAXID
#define _PA_RISC_MAXID	0x2FF
#endif /* _PA_RISC_MAXID */

#ifndef _PA_RISC_ID
#define _PA_RISC_ID(__m_num)		\
    (((__m_num) == _PA_RISC1_0_ID) ||	\
     ((__m_num) >= _PA_RISC1_1_ID && (__m_num) <= _PA_RISC_MAXID))
#endif /* _PA_RISC_ID */

/* HIUX in it's infinite stupidity changed the names for several "well
   known" constants.  Work around such braindamage.  Try the HPUX version
   first, then the HIUX version, and finally provide a default.  */
#ifdef HPUX_AUX_ID
#define EXEC_AUX_ID HPUX_AUX_ID
#endif

#if !defined (EXEC_AUX_ID) && defined (HIUX_AUX_ID)
#define EXEC_AUX_ID HIUX_AUX_ID
#endif

#ifndef EXEC_AUX_ID
#define EXEC_AUX_ID 0
#endif

/* Size (in chars) of the temporary buffers used during fixup and string
   table writes.   */

#define SOM_TMP_BUFSIZE 8192

/* Size of the hash table in archives.  */
#define SOM_LST_HASH_SIZE 31

/* Max number of SOMs to be found in an archive.  */
#define SOM_LST_MODULE_LIMIT 1024

/* Generic alignment macro.  */
#define SOM_ALIGN(val, alignment) \
  (((val) + (alignment) - 1) &~ ((unsigned long) (alignment) - 1))

/* SOM allows any one of the four previous relocations to be reused
   with a "R_PREV_FIXUP" relocation entry.  Since R_PREV_FIXUP
   relocations are always a single byte, using a R_PREV_FIXUP instead
   of some multi-byte relocation makes object files smaller.

   Note one side effect of using a R_PREV_FIXUP is the relocation that
   is being repeated moves to the front of the queue.  */
struct reloc_queue {
  unsigned char *reloc;
  unsigned int size;
} reloc_queue[4];

/* This fully describes the symbol types which may be attached to
   an EXPORT or IMPORT directive.  Only SOM uses this formation
   (ELF has no need for it).  */
typedef enum {
  SYMBOL_TYPE_UNKNOWN,
  SYMBOL_TYPE_ABSOLUTE,
  SYMBOL_TYPE_CODE,
  SYMBOL_TYPE_DATA,
  SYMBOL_TYPE_ENTRY,
  SYMBOL_TYPE_MILLICODE,
  SYMBOL_TYPE_PLABEL,
  SYMBOL_TYPE_PRI_PROG,
  SYMBOL_TYPE_SEC_PROG,
} pa_symbol_type;

struct section_to_type {
  char *section;
  char type;
};

/* Assorted symbol information that needs to be derived from the BFD symbol
   and/or the BFD backend private symbol data.  */
struct som_misc_symbol_info {
  unsigned int symbol_type;
  unsigned int symbol_scope;
  unsigned int arg_reloc;
  unsigned int symbol_info;
  unsigned int symbol_value;
  unsigned int priv_level;
  unsigned int secondary_def;
};

/* Forward declarations */

static boolean som_mkobject PARAMS ((bfd *));
static const bfd_target * som_object_setup PARAMS ((bfd *,
						    struct header *,
						    struct som_exec_auxhdr *,
						    unsigned long));
static boolean setup_sections PARAMS ((bfd *, struct header *, unsigned long));
static const bfd_target * som_object_p PARAMS ((bfd *));
static boolean som_write_object_contents PARAMS ((bfd *));
static boolean som_slurp_string_table PARAMS ((bfd *));
static unsigned int som_slurp_symbol_table PARAMS ((bfd *));
static long som_get_symtab_upper_bound PARAMS ((bfd *));
static long som_canonicalize_reloc PARAMS ((bfd *, sec_ptr,
					    arelent **, asymbol **));
static long som_get_reloc_upper_bound PARAMS ((bfd *, sec_ptr));
static unsigned int som_set_reloc_info PARAMS ((unsigned char *, unsigned int,
						arelent *, asection *,
						asymbol **, boolean));
static boolean som_slurp_reloc_table PARAMS ((bfd *, asection *,
					      asymbol **, boolean));
static long som_get_symtab PARAMS ((bfd *, asymbol **));
static asymbol * som_make_empty_symbol PARAMS ((bfd *));
static void som_print_symbol PARAMS ((bfd *, PTR,
				      asymbol *, bfd_print_symbol_type, 
				      asection *));
static boolean som_new_section_hook PARAMS ((bfd *, asection *));
static boolean som_bfd_copy_private_symbol_data PARAMS ((bfd *, asymbol *,
							  bfd *, asymbol *));
static boolean som_bfd_copy_private_section_data PARAMS ((bfd *, asection *,
							  bfd *, asection *));
static boolean som_bfd_copy_private_bfd_data PARAMS ((bfd *, bfd *));
#define som_bfd_merge_private_bfd_data _bfd_generic_bfd_merge_private_bfd_data
#define som_bfd_set_private_flags _bfd_generic_bfd_set_private_flags
static boolean som_bfd_is_local_label_name PARAMS ((bfd *, const char *));
static boolean som_set_section_contents PARAMS ((bfd *, sec_ptr, PTR,
						 file_ptr, bfd_size_type));
static boolean som_get_section_contents PARAMS ((bfd *, sec_ptr, PTR,
						 file_ptr, bfd_size_type));
static boolean som_set_arch_mach PARAMS ((bfd *, enum bfd_architecture,
					  unsigned long));
static boolean som_find_nearest_line PARAMS ((bfd *, asection *,
					      asymbol **, bfd_vma,
					      const char **,
					      const char **,
					      unsigned int *));
static void som_get_symbol_info PARAMS ((bfd *, asymbol *, symbol_info *));
static asection * bfd_section_from_som_symbol PARAMS ((bfd *,
					struct symbol_dictionary_record *));
static int log2 PARAMS ((unsigned int));
static bfd_reloc_status_type hppa_som_reloc PARAMS ((bfd *, arelent *,
						     asymbol *, PTR,
						     asection *, bfd *,
						     char **));
static void som_initialize_reloc_queue PARAMS ((struct reloc_queue *));
static void som_reloc_queue_insert PARAMS ((unsigned char *, unsigned int,
					    struct reloc_queue *));
static void som_reloc_queue_fix PARAMS ((struct reloc_queue *, unsigned int));
static int som_reloc_queue_find PARAMS ((unsigned char *, unsigned int,
					 struct reloc_queue *));
static unsigned char * try_prev_fixup PARAMS ((bfd *, int *, unsigned char *,
					       unsigned int,
					       struct reloc_queue *));

static unsigned char * som_reloc_skip PARAMS ((bfd *, unsigned int,
					       unsigned char *, unsigned int *,
					       struct reloc_queue *));
static unsigned char * som_reloc_addend PARAMS ((bfd *, bfd_vma,
						 unsigned char *,
					         unsigned int *,
						 struct reloc_queue *));
static unsigned char * som_reloc_call PARAMS ((bfd *, unsigned char *,
					       unsigned int *,
					       arelent *, int,
					       struct reloc_queue *));
static unsigned long som_count_spaces PARAMS ((bfd *));
static unsigned long som_count_subspaces PARAMS ((bfd *));
static int compare_syms PARAMS ((const void *, const void *));
static int compare_subspaces PARAMS ((const void *, const void *));
static unsigned long som_compute_checksum PARAMS ((bfd *));
static boolean som_prep_headers PARAMS ((bfd *));
static int som_sizeof_headers PARAMS ((bfd *, boolean));
static boolean som_finish_writing PARAMS ((bfd *));
static boolean som_build_and_write_symbol_table PARAMS ((bfd *));
static void som_prep_for_fixups PARAMS ((bfd *, asymbol **, unsigned long));
static boolean som_write_fixups PARAMS ((bfd *, unsigned long, unsigned int *));
static boolean som_write_space_strings PARAMS ((bfd *, unsigned long,
						unsigned int *));
static boolean som_write_symbol_strings PARAMS ((bfd *, unsigned long,
						 asymbol **, unsigned int,
						 unsigned *,
						 COMPUNIT *));
static boolean som_begin_writing PARAMS ((bfd *));
static reloc_howto_type * som_bfd_reloc_type_lookup
	PARAMS ((bfd *, bfd_reloc_code_real_type));
static char som_section_type PARAMS ((const char *));
static int som_decode_symclass PARAMS ((asymbol *));
static boolean som_bfd_count_ar_symbols PARAMS ((bfd *, struct lst_header *,
						 symindex *));

static boolean som_bfd_fill_in_ar_symbols PARAMS ((bfd *, struct lst_header *,
						   carsym **syms));
static boolean som_slurp_armap PARAMS ((bfd *));
static boolean som_write_armap PARAMS ((bfd *, unsigned int, struct orl *,
					unsigned int, int));
static void som_bfd_derive_misc_symbol_info PARAMS ((bfd *, asymbol *,
					     struct som_misc_symbol_info *));
static boolean som_bfd_prep_for_ar_write PARAMS ((bfd *, unsigned int *,
						  unsigned int *));
static unsigned int som_bfd_ar_symbol_hash PARAMS ((asymbol *));
static boolean som_bfd_ar_write_symbol_stuff PARAMS ((bfd *, unsigned int,
						      unsigned int,
						      struct lst_header,
						      unsigned int));
static boolean som_is_space PARAMS ((asection *));
static boolean som_is_subspace PARAMS ((asection *));
static boolean som_is_container PARAMS ((asection *, asection *));
static boolean som_bfd_free_cached_info PARAMS ((bfd *));
static boolean som_bfd_link_split_section PARAMS ((bfd *, asection *));

/* Map SOM section names to POSIX/BSD single-character symbol types.

   This table includes all the standard subspaces as defined in the
   current "PRO ABI for PA-RISC Systems", $UNWIND$ which for
   some reason was left out, and sections specific to embedded stabs.  */

static const struct section_to_type stt[] = {
  {"$TEXT$", 't'},
  {"$SHLIB_INFO$", 't'},
  {"$MILLICODE$", 't'},
  {"$LIT$", 't'},
  {"$CODE$", 't'},
  {"$UNWIND_START$", 't'},
  {"$UNWIND$", 't'},
  {"$PRIVATE$", 'd'},
  {"$PLT$", 'd'},
  {"$SHLIB_DATA$", 'd'},
  {"$DATA$", 'd'},
  {"$SHORTDATA$", 'g'},
  {"$DLT$", 'd'},
  {"$GLOBAL$", 'g'},
  {"$SHORTBSS$", 's'},
  {"$BSS$", 'b'},
  {"$GDB_STRINGS$", 'N'},
  {"$GDB_SYMBOLS$", 'N'},
  {0, 0}
};

/* About the relocation formatting table...

   There are 256 entries in the table, one for each possible
   relocation opcode available in SOM.  We index the table by
   the relocation opcode.  The names and operations are those
   defined by a.out_800 (4).

   Right now this table is only used to count and perform minimal
   processing on relocation streams so that they can be internalized
   into BFD and symbolically printed by utilities.  To make actual use
   of them would be much more difficult, BFD's concept of relocations
   is far too simple to handle SOM relocations.  The basic assumption
   that a relocation can be completely processed independent of other
   relocations before an object file is written is invalid for SOM.

   The SOM relocations are meant to be processed as a stream, they
   specify copying of data from the input section to the output section
   while possibly modifying the data in some manner.  They also can
   specify that a variable number of zeros or uninitialized data be
   inserted on in the output segment at the current offset.  Some
   relocations specify that some previous relocation be re-applied at
   the current location in the input/output sections.  And finally a number
   of relocations have effects on other sections (R_ENTRY, R_EXIT,
   R_UNWIND_AUX and a variety of others).  There isn't even enough room
   in the BFD relocation data structure to store enough information to
   perform all the relocations.

   Each entry in the table has three fields.

   The first entry is an index into this "class" of relocations.  This
   index can then be used as a variable within the relocation itself.

   The second field is a format string which actually controls processing
   of the relocation.  It uses a simple postfix machine to do calculations
   based on variables/constants found in the string and the relocation
   stream.

   The third field specifys whether or not this relocation may use
   a constant (V) from the previous R_DATA_OVERRIDE rather than a constant
   stored in the instruction.

   Variables:

   L = input space byte count
   D = index into class of relocations
   M = output space byte count
   N = statement number (unused?)
   O = stack operation
   R = parameter relocation bits
   S = symbol index
   T = first 32 bits of stack unwind information
   U = second 32 bits of stack unwind information
   V = a literal constant (usually used in the next relocation)
   P = a previous relocation

   Lower case letters (starting with 'b') refer to following
   bytes in the relocation stream.  'b' is the next 1 byte,
   c is the next 2 bytes, d is the next 3 bytes, etc...
   This is the variable part of the relocation entries that
   makes our life a living hell.

   numerical constants are also used in the format string.  Note
   the constants are represented in decimal.

   '+', "*" and "=" represents the obvious postfix operators.
   '<' represents a left shift.

   Stack Operations:

   Parameter Relocation Bits:

   Unwind Entries:

   Previous Relocations:  The index field represents which in the queue
   of 4 previous fixups should be re-applied.

   Literal Constants:  These are generally used to represent addend
   parts of relocations when these constants are not stored in the
   fields of the instructions themselves.  For example the instruction
   addil foo-$global$-0x1234 would use an override for "0x1234" rather
   than storing it into the addil itself.  */

struct fixup_format {
  int D;
  const char *format;
};

static const struct fixup_format som_fixup_formats[256] = {
  /* R_NO_RELOCATION */
  {  0, "LD1+4*=" },		/* 0x00 */
  {  1, "LD1+4*=" },		/* 0x01 */
  {  2, "LD1+4*=" },		/* 0x02 */
  {  3, "LD1+4*=" },		/* 0x03 */
  {  4, "LD1+4*=" },		/* 0x04 */
  {  5, "LD1+4*=" },		/* 0x05 */
  {  6, "LD1+4*=" },		/* 0x06 */
  {  7, "LD1+4*=" },		/* 0x07 */
  {  8, "LD1+4*=" },		/* 0x08 */
  {  9, "LD1+4*=" },		/* 0x09 */
  { 10, "LD1+4*=" },		/* 0x0a */
  { 11, "LD1+4*=" },		/* 0x0b */
  { 12, "LD1+4*=" },		/* 0x0c */
  { 13, "LD1+4*=" },		/* 0x0d */
  { 14, "LD1+4*=" },		/* 0x0e */
  { 15, "LD1+4*=" },		/* 0x0f */
  { 16, "LD1+4*=" },		/* 0x10 */
  { 17, "LD1+4*=" },		/* 0x11 */
  { 18, "LD1+4*=" },		/* 0x12 */
  { 19, "LD1+4*=" },		/* 0x13 */
  { 20, "LD1+4*=" },		/* 0x14 */
  { 21, "LD1+4*=" },		/* 0x15 */
  { 22, "LD1+4*=" },		/* 0x16 */
  { 23, "LD1+4*=" },		/* 0x17 */
  {  0, "LD8<b+1+4*=" },	/* 0x18 */
  {  1, "LD8<b+1+4*=" },	/* 0x19 */
  {  2, "LD8<b+1+4*=" },	/* 0x1a */
  {  3, "LD8<b+1+4*=" },	/* 0x1b */
  {  0, "LD16<c+1+4*=" },	/* 0x1c */
  {  1, "LD16<c+1+4*=" },	/* 0x1d */
  {  2, "LD16<c+1+4*=" },	/* 0x1e */
  {  0, "Ld1+=" },		/* 0x1f */
  /* R_ZEROES */
  {  0, "Lb1+4*=" },		/* 0x20 */
  {  1, "Ld1+=" },		/* 0x21 */
  /* R_UNINIT */
  {  0, "Lb1+4*=" },		/* 0x22 */
  {  1, "Ld1+=" },		/* 0x23 */
  /* R_RELOCATION */
  {  0, "L4=" },		/* 0x24 */
  /* R_DATA_ONE_SYMBOL */
  {  0, "L4=Sb=" },		/* 0x25 */
  {  1, "L4=Sd=" },		/* 0x26 */
  /* R_DATA_PLEBEL */
  {  0, "L4=Sb=" },		/* 0x27 */
  {  1, "L4=Sd=" },		/* 0x28 */
  /* R_SPACE_REF */
  {  0, "L4=" },		/* 0x29 */
  /* R_REPEATED_INIT */
  {  0, "L4=Mb1+4*=" },		/* 0x2a */
  {  1, "Lb4*=Mb1+L*=" },	/* 0x2b */
  {  2, "Lb4*=Md1+4*=" },	/* 0x2c */
  {  3, "Ld1+=Me1+=" },		/* 0x2d */
  {  0, "" },			/* 0x2e */
  {  0, "" },			/* 0x2f */
  /* R_PCREL_CALL */
  {  0, "L4=RD=Sb=" },		/* 0x30 */
  {  1, "L4=RD=Sb=" },		/* 0x31 */
  {  2, "L4=RD=Sb=" },		/* 0x32 */
  {  3, "L4=RD=Sb=" },		/* 0x33 */
  {  4, "L4=RD=Sb=" },		/* 0x34 */
  {  5, "L4=RD=Sb=" },		/* 0x35 */
  {  6, "L4=RD=Sb=" },		/* 0x36 */
  {  7, "L4=RD=Sb=" },		/* 0x37 */
  {  8, "L4=RD=Sb=" },		/* 0x38 */
  {  9, "L4=RD=Sb=" },		/* 0x39 */
  {  0, "L4=RD8<b+=Sb=" },	/* 0x3a */
  {  1, "L4=RD8<b+=Sb=" },	/* 0x3b */
  {  0, "L4=RD8<b+=Sd=" },	/* 0x3c */
  {  1, "L4=RD8<b+=Sd=" },	/* 0x3d */
  /* R_SHORT_PCREL_MODE */
  {  0, "" },			/* 0x3e */
  /* R_LONG_PCREL_MODE */
  {  0, "" },			/* 0x3f */
  /* R_ABS_CALL */
  {  0, "L4=RD=Sb=" },		/* 0x40 */
  {  1, "L4=RD=Sb=" },		/* 0x41 */
  {  2, "L4=RD=Sb=" },		/* 0x42 */
  {  3, "L4=RD=Sb=" },		/* 0x43 */
  {  4, "L4=RD=Sb=" },		/* 0x44 */
  {  5, "L4=RD=Sb=" },		/* 0x45 */
  {  6, "L4=RD=Sb=" },		/* 0x46 */
  {  7, "L4=RD=Sb=" },		/* 0x47 */
  {  8, "L4=RD=Sb=" },		/* 0x48 */
  {  9, "L4=RD=Sb=" },		/* 0x49 */
  {  0, "L4=RD8<b+=Sb=" },	/* 0x4a */
  {  1, "L4=RD8<b+=Sb=" },	/* 0x4b */
  {  0, "L4=RD8<b+=Sd=" },	/* 0x4c */
  {  1, "L4=RD8<b+=Sd=" },	/* 0x4d */
  /* R_RESERVED */
  {  0, "" },			/* 0x4e */
  {  0, "" },			/* 0x4f */
  /* R_DP_RELATIVE */
  {  0, "L4=SD=" },		/* 0x50 */
  {  1, "L4=SD=" },		/* 0x51 */
  {  2, "L4=SD=" },		/* 0x52 */
  {  3, "L4=SD=" },		/* 0x53 */
  {  4, "L4=SD=" },		/* 0x54 */
  {  5, "L4=SD=" },		/* 0x55 */
  {  6, "L4=SD=" },		/* 0x56 */
  {  7, "L4=SD=" },		/* 0x57 */
  {  8, "L4=SD=" },		/* 0x58 */
  {  9, "L4=SD=" },		/* 0x59 */
  { 10, "L4=SD=" },		/* 0x5a */
  { 11, "L4=SD=" },		/* 0x5b */
  { 12, "L4=SD=" },		/* 0x5c */
  { 13, "L4=SD=" },		/* 0x5d */
  { 14, "L4=SD=" },		/* 0x5e */
  { 15, "L4=SD=" },		/* 0x5f */
  { 16, "L4=SD=" },		/* 0x60 */
  { 17, "L4=SD=" },		/* 0x61 */
  { 18, "L4=SD=" },		/* 0x62 */
  { 19, "L4=SD=" },		/* 0x63 */
  { 20, "L4=SD=" },		/* 0x64 */
  { 21, "L4=SD=" },		/* 0x65 */
  { 22, "L4=SD=" },		/* 0x66 */
  { 23, "L4=SD=" },		/* 0x67 */
  { 24, "L4=SD=" },		/* 0x68 */
  { 25, "L4=SD=" },		/* 0x69 */
  { 26, "L4=SD=" },		/* 0x6a */
  { 27, "L4=SD=" },		/* 0x6b */
  { 28, "L4=SD=" },		/* 0x6c */
  { 29, "L4=SD=" },		/* 0x6d */
  { 30, "L4=SD=" },		/* 0x6e */
  { 31, "L4=SD=" },		/* 0x6f */
  { 32, "L4=Sb=" },		/* 0x70 */
  { 33, "L4=Sd=" },		/* 0x71 */
  /* R_RESERVED */
  {  0, "" },			/* 0x72 */
  {  0, "" },			/* 0x73 */
  {  0, "" },			/* 0x74 */
  {  0, "" },			/* 0x75 */
  {  0, "" },			/* 0x76 */
  {  0, "" },			/* 0x77 */
  /* R_DLT_REL */
  {  0, "L4=Sb=" },		/* 0x78 */
  {  1, "L4=Sd=" },		/* 0x79 */
  /* R_RESERVED */
  {  0, "" },			/* 0x7a */
  {  0, "" },			/* 0x7b */
  {  0, "" },			/* 0x7c */
  {  0, "" },			/* 0x7d */
  {  0, "" },			/* 0x7e */
  {  0, "" },			/* 0x7f */
  /* R_CODE_ONE_SYMBOL */
  {  0, "L4=SD=" },		/* 0x80 */
  {  1, "L4=SD=" },		/* 0x81 */
  {  2, "L4=SD=" },		/* 0x82 */
  {  3, "L4=SD=" },		/* 0x83 */
  {  4, "L4=SD=" },		/* 0x84 */
  {  5, "L4=SD=" },		/* 0x85 */
  {  6, "L4=SD=" },		/* 0x86 */
  {  7, "L4=SD=" },		/* 0x87 */
  {  8, "L4=SD=" },		/* 0x88 */
  {  9, "L4=SD=" },		/* 0x89 */
  { 10, "L4=SD=" },		/* 0x8q */
  { 11, "L4=SD=" },		/* 0x8b */
  { 12, "L4=SD=" },		/* 0x8c */
  { 13, "L4=SD=" },		/* 0x8d */
  { 14, "L4=SD=" },		/* 0x8e */
  { 15, "L4=SD=" },		/* 0x8f */
  { 16, "L4=SD=" },		/* 0x90 */
  { 17, "L4=SD=" },		/* 0x91 */
  { 18, "L4=SD=" },		/* 0x92 */
  { 19, "L4=SD=" },		/* 0x93 */
  { 20, "L4=SD=" },		/* 0x94 */
  { 21, "L4=SD=" },		/* 0x95 */
  { 22, "L4=SD=" },		/* 0x96 */
  { 23, "L4=SD=" },		/* 0x97 */
  { 24, "L4=SD=" },		/* 0x98 */
  { 25, "L4=SD=" },		/* 0x99 */
  { 26, "L4=SD=" },		/* 0x9a */
  { 27, "L4=SD=" },		/* 0x9b */
  { 28, "L4=SD=" },		/* 0x9c */
  { 29, "L4=SD=" },		/* 0x9d */
  { 30, "L4=SD=" },		/* 0x9e */
  { 31, "L4=SD=" },		/* 0x9f */
  { 32, "L4=Sb=" },		/* 0xa0 */
  { 33, "L4=Sd=" },		/* 0xa1 */
  /* R_RESERVED */
  {  0, "" },			/* 0xa2 */
  {  0, "" },			/* 0xa3 */
  {  0, "" },			/* 0xa4 */
  {  0, "" },			/* 0xa5 */
  {  0, "" },			/* 0xa6 */
  {  0, "" },			/* 0xa7 */
  {  0, "" },			/* 0xa8 */
  {  0, "" },			/* 0xa9 */
  {  0, "" },			/* 0xaa */
  {  0, "" },			/* 0xab */
  {  0, "" },			/* 0xac */
  {  0, "" },			/* 0xad */
  /* R_MILLI_REL */
  {  0, "L4=Sb=" },		/* 0xae */
  {  1, "L4=Sd=" },		/* 0xaf */
  /* R_CODE_PLABEL */
  {  0, "L4=Sb=" },		/* 0xb0 */
  {  1, "L4=Sd=" },		/* 0xb1 */
  /* R_BREAKPOINT */
  {  0, "L4=" },		/* 0xb2 */
  /* R_ENTRY */
  {  0, "Te=Ue=" },		/* 0xb3 */
  {  1, "Uf=" },		/* 0xb4 */
  /* R_ALT_ENTRY */
  {  0, "" },			/* 0xb5 */
  /* R_EXIT */
  {  0, "" },			/* 0xb6 */
  /* R_BEGIN_TRY */
  {  0, "" },			/* 0xb7 */
  /* R_END_TRY */
  {  0, "R0=" },		/* 0xb8 */
  {  1, "Rb4*=" },		/* 0xb9 */
  {  2, "Rd4*=" },		/* 0xba */
  /* R_BEGIN_BRTAB */
  {  0, "" },			/* 0xbb */
  /* R_END_BRTAB */
  {  0, "" },			/* 0xbc */
  /* R_STATEMENT */
  {  0, "Nb=" },		/* 0xbd */
  {  1, "Nc=" },		/* 0xbe */
  {  2, "Nd=" },		/* 0xbf */
  /* R_DATA_EXPR */
  {  0, "L4=" },		/* 0xc0 */
  /* R_CODE_EXPR */
  {  0, "L4=" },		/* 0xc1 */
  /* R_FSEL */
  {  0, "" },			/* 0xc2 */
  /* R_LSEL */
  {  0, "" },			/* 0xc3 */
  /* R_RSEL */
  {  0, "" },			/* 0xc4 */
  /* R_N_MODE */
  {  0, "" },			/* 0xc5 */
  /* R_S_MODE */
  {  0, "" },			/* 0xc6 */
  /* R_D_MODE */
  {  0, "" },			/* 0xc7 */
  /* R_R_MODE */
  {  0, "" },			/* 0xc8 */
  /* R_DATA_OVERRIDE */
  {  0, "V0=" },		/* 0xc9 */
  {  1, "Vb=" },		/* 0xca */
  {  2, "Vc=" },		/* 0xcb */
  {  3, "Vd=" },		/* 0xcc */
  {  4, "Ve=" },		/* 0xcd */
  /* R_TRANSLATED */
  {  0, "" },			/* 0xce */
  /* R_AUX_UNWIND */
  {  0,"Sd=Vf=Ef=" },	       /* 0xcf */
  /* R_COMP1 */
  {  0, "Ob=" },		/* 0xd0 */
  /* R_COMP2 */
  {  0, "Ob=Sd=" },		/* 0xd1 */
  /* R_COMP3 */
  {  0, "Ob=Ve=" },		/* 0xd2 */
  /* R_PREV_FIXUP */
  {  0, "P" },			/* 0xd3 */
  {  1, "P" },			/* 0xd4 */
  {  2, "P" },			/* 0xd5 */
  {  3, "P" },			/* 0xd6 */
  /* R_SEC_STMT */
  {  0, "" },			/* 0xd7 */
  /* R_N0SEL */
  {  0, "" },			/* 0xd8 */
  /* R_N1SEL */
  {  0, "" },			/* 0xd9 */
  /* R_LINETAB */
  {  0, "Eb=Sd=Ve=" },		/* 0xda */
  /* R_LINETAB_ESC */
  {  0, "Eb=Mb=" },		/* 0xdb */
  /* R_LTP_OVERRIDE */
  {  0, "" },			/* 0xdc */
  /* R_COMMENT */
  {  0, "Ob=Ve=" },		/* 0xdd */
  /* R_RESERVED */
  {  0, "" },			/* 0xde */
  {  0, "" },			/* 0xdf */
  {  0, "" },			/* 0xe0 */
  {  0, "" },			/* 0xe1 */
  {  0, "" },			/* 0xe2 */
  {  0, "" },			/* 0xe3 */
  {  0, "" },			/* 0xe4 */
  {  0, "" },			/* 0xe5 */
  {  0, "" },			/* 0xe6 */
  {  0, "" },			/* 0xe7 */
  {  0, "" },			/* 0xe8 */
  {  0, "" },			/* 0xe9 */
  {  0, "" },			/* 0xea */
  {  0, "" },			/* 0xeb */
  {  0, "" },			/* 0xec */
  {  0, "" },			/* 0xed */
  {  0, "" },			/* 0xee */
  {  0, "" },			/* 0xef */
  {  0, "" },			/* 0xf0 */
  {  0, "" },			/* 0xf1 */
  {  0, "" },			/* 0xf2 */
  {  0, "" },			/* 0xf3 */
  {  0, "" },			/* 0xf4 */
  {  0, "" },			/* 0xf5 */
  {  0, "" },			/* 0xf6 */
  {  0, "" },			/* 0xf7 */
  {  0, "" },			/* 0xf8 */
  {  0, "" },			/* 0xf9 */
  {  0, "" },			/* 0xfa */
  {  0, "" },			/* 0xfb */
  {  0, "" },			/* 0xfc */
  {  0, "" },			/* 0xfd */
  {  0, "" },			/* 0xfe */
  {  0, "" },			/* 0xff */
};

static const int comp1_opcodes[] = {
  0x00,
  0x40,
  0x41,
  0x42,
  0x43,
  0x44,
  0x45,
  0x46,
  0x47,
  0x48,
  0x49,
  0x4a,
  0x4b,
  0x60,
  0x80,
  0xa0,
  0xc0,
  -1
};

static const int comp2_opcodes[] = {
  0x00,
  0x80,
  0x82,
  0xc0,
  -1
};

static const int comp3_opcodes[] = {
  0x00,
  0x02,
  -1
};

/* These apparently are not in older versions of hpux reloc.h (hpux7).  */
#ifndef R_DLT_REL
#define R_DLT_REL 0x78
#endif

#ifndef R_AUX_UNWIND
#define R_AUX_UNWIND 0xcf
#endif

#ifndef R_SEC_STMT
#define R_SEC_STMT 0xd7
#endif

/* And these first appeared in hpux10.  */
#ifndef R_SHORT_PCREL_MODE
#define NO_PCREL_MODES
#define R_SHORT_PCREL_MODE 0x3e
#endif

#ifndef R_LONG_PCREL_MODE
#define R_LONG_PCREL_MODE 0x3f
#endif

#ifndef R_N0SEL
#define R_N0SEL 0xd8
#endif

#ifndef R_N1SEL
#define R_N1SEL 0xd9
#endif

#ifndef R_LINETAB
#define R_LINETAB 0xda
#endif

#ifndef R_LINETAB_ESC
#define R_LINETAB_ESC 0xdb
#endif

#ifndef R_LTP_OVERRIDE
#define R_LTP_OVERRIDE 0xdc
#endif

#ifndef R_COMMENT
#define R_COMMENT 0xdd
#endif

#define SOM_HOWTO(TYPE, NAME)	\
  HOWTO(TYPE, 0, 0, 32, false, 0, 0, hppa_som_reloc, NAME, false, 0, 0, false)

static reloc_howto_type som_hppa_howto_table[] = {
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_NO_RELOCATION, "R_NO_RELOCATION"),
  SOM_HOWTO (R_ZEROES, "R_ZEROES"),
  SOM_HOWTO (R_ZEROES, "R_ZEROES"),
  SOM_HOWTO (R_UNINIT, "R_UNINIT"),
  SOM_HOWTO (R_UNINIT, "R_UNINIT"),
  SOM_HOWTO (R_RELOCATION, "R_RELOCATION"),
  SOM_HOWTO (R_DATA_ONE_SYMBOL, "R_DATA_ONE_SYMBOL"),
  SOM_HOWTO (R_DATA_ONE_SYMBOL, "R_DATA_ONE_SYMBOL"),
  SOM_HOWTO (R_DATA_PLABEL, "R_DATA_PLABEL"),
  SOM_HOWTO (R_DATA_PLABEL, "R_DATA_PLABEL"),
  SOM_HOWTO (R_SPACE_REF, "R_SPACE_REF"),
  SOM_HOWTO (R_REPEATED_INIT, "REPEATED_INIT"),
  SOM_HOWTO (R_REPEATED_INIT, "REPEATED_INIT"),
  SOM_HOWTO (R_REPEATED_INIT, "REPEATED_INIT"),
  SOM_HOWTO (R_REPEATED_INIT, "REPEATED_INIT"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_PCREL_CALL, "R_PCREL_CALL"),
  SOM_HOWTO (R_SHORT_PCREL_MODE, "R_SHORT_PCREL_MODE"),
  SOM_HOWTO (R_LONG_PCREL_MODE, "R_LONG_PCREL_MODE"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_ABS_CALL, "R_ABS_CALL"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_DP_RELATIVE, "R_DP_RELATIVE"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_DLT_REL, "R_DLT_REL"),
  SOM_HOWTO (R_DLT_REL, "R_DLT_REL"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_CODE_ONE_SYMBOL, "R_CODE_ONE_SYMBOL"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_MILLI_REL, "R_MILLI_REL"),
  SOM_HOWTO (R_MILLI_REL, "R_MILLI_REL"),
  SOM_HOWTO (R_CODE_PLABEL, "R_CODE_PLABEL"),
  SOM_HOWTO (R_CODE_PLABEL, "R_CODE_PLABEL"),
  SOM_HOWTO (R_BREAKPOINT, "R_BREAKPOINT"),
  SOM_HOWTO (R_ENTRY, "R_ENTRY"),
  SOM_HOWTO (R_ENTRY, "R_ENTRY"),
  SOM_HOWTO (R_ALT_ENTRY, "R_ALT_ENTRY"),
  SOM_HOWTO (R_EXIT, "R_EXIT"),
  SOM_HOWTO (R_BEGIN_TRY, "R_BEGIN_TRY"),
  SOM_HOWTO (R_END_TRY, "R_END_TRY"),
  SOM_HOWTO (R_END_TRY, "R_END_TRY"),
  SOM_HOWTO (R_END_TRY, "R_END_TRY"),
  SOM_HOWTO (R_BEGIN_BRTAB, "R_BEGIN_BRTAB"),
  SOM_HOWTO (R_END_BRTAB, "R_END_BRTAB"),
  SOM_HOWTO (R_STATEMENT, "R_STATEMENT"),
  SOM_HOWTO (R_STATEMENT, "R_STATEMENT"),
  SOM_HOWTO (R_STATEMENT, "R_STATEMENT"),
  SOM_HOWTO (R_DATA_EXPR, "R_DATA_EXPR"),
  SOM_HOWTO (R_CODE_EXPR, "R_CODE_EXPR"),
  SOM_HOWTO (R_FSEL, "R_FSEL"),
  SOM_HOWTO (R_LSEL, "R_LSEL"),
  SOM_HOWTO (R_RSEL, "R_RSEL"),
  SOM_HOWTO (R_N_MODE, "R_N_MODE"),
  SOM_HOWTO (R_S_MODE, "R_S_MODE"),
  SOM_HOWTO (R_D_MODE, "R_D_MODE"),
  SOM_HOWTO (R_R_MODE, "R_R_MODE"),
  SOM_HOWTO (R_DATA_OVERRIDE, "R_DATA_OVERRIDE"),
  SOM_HOWTO (R_DATA_OVERRIDE, "R_DATA_OVERRIDE"),
  SOM_HOWTO (R_DATA_OVERRIDE, "R_DATA_OVERRIDE"),
  SOM_HOWTO (R_DATA_OVERRIDE, "R_DATA_OVERRIDE"),
  SOM_HOWTO (R_DATA_OVERRIDE, "R_DATA_OVERRIDE"),
  SOM_HOWTO (R_TRANSLATED, "R_TRANSLATED"),
  SOM_HOWTO (R_AUX_UNWIND, "R_AUX_UNWIND"),
  SOM_HOWTO (R_COMP1, "R_COMP1"),
  SOM_HOWTO (R_COMP2, "R_COMP2"),
  SOM_HOWTO (R_COMP3, "R_COMP3"),
  SOM_HOWTO (R_PREV_FIXUP, "R_PREV_FIXUP"),
  SOM_HOWTO (R_PREV_FIXUP, "R_PREV_FIXUP"),
  SOM_HOWTO (R_PREV_FIXUP, "R_PREV_FIXUP"),
  SOM_HOWTO (R_PREV_FIXUP, "R_PREV_FIXUP"),
  SOM_HOWTO (R_SEC_STMT, "R_SEC_STMT"),
  SOM_HOWTO (R_N0SEL, "R_N0SEL"),
  SOM_HOWTO (R_N1SEL, "R_N1SEL"),
  SOM_HOWTO (R_LINETAB, "R_LINETAB"),
  SOM_HOWTO (R_LINETAB_ESC, "R_LINETAB_ESC"),
  SOM_HOWTO (R_LTP_OVERRIDE, "R_LTP_OVERRIDE"),
  SOM_HOWTO (R_COMMENT, "R_COMMENT"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED"),
  SOM_HOWTO (R_RESERVED, "R_RESERVED")
};

/* Initialize the SOM relocation queue.  By definition the queue holds
   the last four multibyte fixups.  */

static void
som_initialize_reloc_queue (queue)
     struct reloc_queue *queue;
{
  queue[0].reloc = NULL;
  queue[0].size = 0;
  queue[1].reloc = NULL;
  queue[1].size = 0;
  queue[2].reloc = NULL;
  queue[2].size = 0;
  queue[3].reloc = NULL;
  queue[3].size = 0;
}

/* Insert a new relocation into the relocation queue.  */

static void
som_reloc_queue_insert (p, size, queue)
     unsigned char *p;
     unsigned int size;
     struct reloc_queue *queue;
{
  queue[3].reloc = queue[2].reloc;
  queue[3].size = queue[2].size;
  queue[2].reloc = queue[1].reloc;
  queue[2].size = queue[1].size;
  queue[1].reloc = queue[0].reloc;
  queue[1].size = queue[0].size;
  queue[0].reloc = p;
  queue[0].size = size;
}

/* When an entry in the relocation queue is reused, the entry moves
   to the front of the queue.  */

static void
som_reloc_queue_fix (queue, index)
     struct reloc_queue *queue;
     unsigned int index;
{
  if (index == 0)
    return;

  if (index == 1)
    {
      unsigned char *tmp1 = queue[0].reloc;
      unsigned int tmp2 = queue[0].size;
      queue[0].reloc = queue[1].reloc;
      queue[0].size = queue[1].size;
      queue[1].reloc = tmp1;
      queue[1].size = tmp2;
      return;
    }

  if (index == 2)
    {
      unsigned char *tmp1 = queue[0].reloc;
      unsigned int tmp2 = queue[0].size;
      queue[0].reloc = queue[2].reloc;
      queue[0].size = queue[2].size;
      queue[2].reloc = queue[1].reloc;
      queue[2].size = queue[1].size;
      queue[1].reloc = tmp1;
      queue[1].size = tmp2;
      return;
    }

  if (index == 3)
    {
      unsigned char *tmp1 = queue[0].reloc;
      unsigned int tmp2 = queue[0].size;
      queue[0].reloc = queue[3].reloc;
      queue[0].size = queue[3].size;
      queue[3].reloc = queue[2].reloc;
      queue[3].size = queue[2].size;
      queue[2].reloc = queue[1].reloc;
      queue[2].size = queue[1].size;
      queue[1].reloc = tmp1;
      queue[1].size = tmp2;
      return;
    }
  abort ();
}

/* Search for a particular relocation in the relocation queue.  */

static int
som_reloc_queue_find (p, size, queue)
     unsigned char *p;
     unsigned int size;
     struct reloc_queue *queue;
{
  if (queue[0].reloc && !memcmp (p, queue[0].reloc, size)
      && size == queue[0].size)
    return 0;
  if (queue[1].reloc && !memcmp (p, queue[1].reloc, size)
      && size == queue[1].size)
    return 1;
  if (queue[2].reloc && !memcmp (p, queue[2].reloc, size)
      && size == queue[2].size)
    return 2;
  if (queue[3].reloc && !memcmp (p, queue[3].reloc, size)
      && size == queue[3].size)
    return 3;
  return -1;
}

static unsigned char *
try_prev_fixup (abfd, subspace_reloc_sizep, p, size, queue)
     bfd *abfd ATTRIBUTE_UNUSED;
     int *subspace_reloc_sizep;
     unsigned char *p;
     unsigned int size;
     struct reloc_queue *queue;
{
  int queue_index = som_reloc_queue_find (p, size, queue);

  if (queue_index != -1)
    {
      /* Found this in a previous fixup.  Undo the fixup we
	 just built and use R_PREV_FIXUP instead.  We saved
	 a total of size - 1 bytes in the fixup stream.  */
      bfd_put_8 (abfd, R_PREV_FIXUP + queue_index, p);
      p += 1;
      *subspace_reloc_sizep += 1;
      som_reloc_queue_fix (queue, queue_index);
    }
  else
    {
      som_reloc_queue_insert (p, size, queue);
      *subspace_reloc_sizep += size;
      p += size;
    }
  return p;
}

/* Emit the proper R_NO_RELOCATION fixups to map the next SKIP
   bytes without any relocation.  Update the size of the subspace
   relocation stream via SUBSPACE_RELOC_SIZE_P; also return the
   current pointer into the relocation stream.  */

static unsigned char *
som_reloc_skip (abfd, skip, p, subspace_reloc_sizep, queue)
     bfd *abfd;
     unsigned int skip;
     unsigned char *p;
     unsigned int *subspace_reloc_sizep;
     struct reloc_queue *queue;
{
  /* Use a 4 byte R_NO_RELOCATION entry with a maximal value
     then R_PREV_FIXUPs to get the difference down to a
     reasonable size.  */
  if (skip >= 0x1000000)
    {
      skip -= 0x1000000;
      bfd_put_8 (abfd, R_NO_RELOCATION + 31, p);
      bfd_put_8 (abfd, 0xff, p + 1);
      bfd_put_16 (abfd, (bfd_vma) 0xffff, p + 2);
      p = try_prev_fixup (abfd, subspace_reloc_sizep, p, 4, queue);
      while (skip >= 0x1000000)
	{
	  skip -= 0x1000000;
	  bfd_put_8 (abfd, R_PREV_FIXUP, p);
	  p++;
	  *subspace_reloc_sizep += 1;
	  /* No need to adjust queue here since we are repeating the
	     most recent fixup.  */
	}
    }

  /* The difference must be less than 0x1000000.  Use one
     more R_NO_RELOCATION entry to get to the right difference.  */
  if ((skip & 3) == 0 && skip <= 0xc0000 && skip > 0)
    {
      /* Difference can be handled in a simple single-byte
	 R_NO_RELOCATION entry.  */
      if (skip <= 0x60)
	{
	  bfd_put_8 (abfd, R_NO_RELOCATION + (skip >> 2) - 1, p);
	  *subspace_reloc_sizep += 1;
	  p++;
	}
      /* Handle it with a two byte R_NO_RELOCATION entry.  */
      else if (skip <= 0x1000)
	{
	  bfd_put_8 (abfd, R_NO_RELOCATION + 24 + (((skip >> 2) - 1) >> 8), p);
	  bfd_put_8 (abfd, (skip >> 2) - 1, p + 1);
	  p = try_prev_fixup (abfd, subspace_reloc_sizep, p, 2, queue);
	}
      /* Handle it with a three byte R_NO_RELOCATION entry.  */
      else
	{
	  bfd_put_8 (abfd, R_NO_RELOCATION + 28 + (((skip >> 2) - 1) >> 16), p);
	  bfd_put_16 (abfd, (bfd_vma) (skip >> 2) - 1, p + 1);
	  p = try_prev_fixup (abfd, subspace_reloc_sizep, p, 3, queue);
	}
    }
  /* Ugh.  Punt and use a 4 byte entry.  */
  else if (skip > 0)
    {
      bfd_put_8 (abfd, R_NO_RELOCATION + 31, p);
      bfd_put_8 (abfd, (skip - 1) >> 16, p + 1);
      bfd_put_16 (abfd, (bfd_vma) skip - 1, p + 2);
      p = try_prev_fixup (abfd, subspace_reloc_sizep, p, 4, queue);
    }
  return p;
}

/* Emit the proper R_DATA_OVERRIDE fixups to handle a nonzero addend
   from a BFD relocation.  Update the size of the subspace relocation
   stream via SUBSPACE_RELOC_SIZE_P; also return the current pointer
   into the relocation stream.  */

static unsigned char *
som_reloc_addend (abfd, addend, p, subspace_reloc_sizep, queue)
     bfd *abfd;
     bfd_vma addend;
     unsigned char *p;
     unsigned int *subspace_reloc_sizep;
     struct reloc_queue *queue;
{
  if (addend + 0x80 < 0x100)
    {
      bfd_put_8 (abfd, R_DATA_OVERRIDE + 1, p);
      bfd_put_8 (abfd, addend, p + 1);
      p = try_prev_fixup (abfd, subspace_reloc_sizep, p, 2, queue);
    }
  else if (addend + 0x8000 < 0x10000)
    {
      bfd_put_8 (abfd, R_DATA_OVERRIDE + 2, p);
      bfd_put_16 (abfd, addend, p + 1);
      p = try_prev_fixup (abfd, subspace_reloc_sizep, p, 3, queue);
    }
  else if (addend + 0x800000 < 0x1000000)
    {
      bfd_put_8 (abfd, R_DATA_OVERRIDE + 3, p);
      bfd_put_8 (abfd, addend >> 16, p + 1);
      bfd_put_16 (abfd, addend, p + 2);
      p = try_prev_fixup (abfd, subspace_reloc_sizep, p, 4, queue);
    }
  else
    {
      bfd_put_8 (abfd, R_DATA_OVERRIDE + 4, p);
      bfd_put_32 (abfd, addend, p + 1);
      p = try_prev_fixup (abfd, subspace_reloc_sizep, p, 5, queue);
    }
  return p;
}

/* Handle a single function call relocation.  */

static unsigned char *
som_reloc_call (abfd, p, subspace_reloc_sizep, bfd_reloc, sym_num, queue)
     bfd *abfd;
     unsigned char *p;
     unsigned int *subspace_reloc_sizep;
     arelent *bfd_reloc;
     int sym_num;
     struct reloc_queue *queue;
{
  int arg_bits = HPPA_R_ARG_RELOC (bfd_reloc->addend);
  int rtn_bits = arg_bits & 0x3;
  int type, done = 0;

  /* You'll never believe all this is necessary to handle relocations
     for function calls.  Having to compute and pack the argument
     relocation bits is the real nightmare.

     If you're interested in how this works, just forget it.  You really
     do not want to know about this braindamage.  */

  /* First see if this can be done with a "simple" relocation.  Simple
     relocations have a symbol number < 0x100 and have simple encodings
     of argument relocations.  */

  if (sym_num < 0x100)
    {
      switch (arg_bits)
	{
	case 0:
	case 1:
	  type = 0;
	  break;
	case 1 << 8:
	case 1 << 8 | 1:
	  type = 1;
	  break;
	case 1 << 8 | 1 << 6:
	case 1 << 8 | 1 << 6 | 1:
	  type = 2;
	  break;
	case 1 << 8 | 1 << 6 | 1 << 4:
	case 1 << 8 | 1 << 6 | 1 << 4 | 1:
	  type = 3;
	  break;
	case 1 << 8 | 1 << 6 | 1 << 4 | 1 << 2:
	case 1 << 8 | 1 << 6 | 1 << 4 | 1 << 2 | 1:
	  type = 4;
	  break;
	default:
	  /* Not one of the easy encodings.  This will have to be
	     handled by the more complex code below.  */
	  type = -1;
	  break;
	}
      if (type != -1)
	{
	  /* Account for the return value too.  */
	  if (rtn_bits)
	    type += 5;

	  /* Emit a 2 byte relocation.  Then see if it can be handled
	     with a relocation which is already in the relocation queue.  */
	  bfd_put_8 (abfd, bfd_reloc->howto->type + type, p);
	  bfd_put_8 (abfd, sym_num, p + 1);
	  p = try_prev_fixup (abfd, subspace_reloc_sizep, p, 2, queue);
	  done = 1;
	}
    }

  /* If this could not be handled with a simple relocation, then do a hard
     one.  Hard relocations occur if the symbol number was too high or if
     the encoding of argument relocation bits is too complex.  */
  if (! done)
    {
      /* Don't ask about these magic sequences.  I took them straight
	 from gas-1.36 which took them from the a.out man page.  */
      type = rtn_bits;
      if ((arg_bits >> 6 & 0xf) == 0xe)
	type += 9 * 40;
      else
	type += (3 * (arg_bits >> 8 & 3) + (arg_bits >> 6 & 3)) * 40;
      if ((arg_bits >> 2 & 0xf) == 0xe)
	type += 9 * 4;
      else
	type += (3 * (arg_bits >> 4 & 3) + (arg_bits >> 2 & 3)) * 4;

      /* Output the first two bytes of the relocation.  These describe
	 the length of the relocation and encoding style.  */
      bfd_put_8 (abfd, bfd_reloc->howto->type + 10
		 + 2 * (sym_num >= 0x100) + (type >= 0x100),
		 p);
      bfd_put_8 (abfd, type, p + 1);

      /* Now output the symbol index and see if this bizarre relocation
	 just happened to be in the relocation queue.  */
      if (sym_num < 0x100)
	{
	  bfd_put_8 (abfd, sym_num, p + 2);
	  p = try_prev_fixup (abfd, subspace_reloc_sizep, p, 3, queue);
	}
      else
	{
	  bfd_put_8 (abfd, sym_num >> 16, p + 2);
	  bfd_put_16 (abfd, (bfd_vma) sym_num, p + 3);
	  p = try_prev_fixup (abfd, subspace_reloc_sizep, p, 5, queue);
	}
    }
  return p;
}

/* Return the logarithm of X, base 2, considering X unsigned.
   Abort -1 if X is not a power or two or is zero.  */

static int
log2 (x)
     unsigned int x;
{
  int log = 0;

  /* Test for 0 or a power of 2.  */
  if (x == 0 || x != (x & -x))
    return -1;

  while ((x >>= 1) != 0)
    log++;
  return log;
}

static bfd_reloc_status_type
hppa_som_reloc (abfd, reloc_entry, symbol_in, data,
		input_section, output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol_in ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  if (output_bfd)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }
  return bfd_reloc_ok;
}

/* Given a generic HPPA relocation type, the instruction format,
   and a field selector, return one or more appropriate SOM relocations.  */

int **
hppa_som_gen_reloc_type (abfd, base_type, format, field, sym_diff, sym)
     bfd *abfd;
     int base_type;
     int format;
     enum hppa_reloc_field_selector_type_alt field;
     int sym_diff;
     asymbol *sym;
{
  int *final_type, **final_types;

  final_types = (int **) bfd_alloc (abfd, (bfd_size_type) sizeof (int *) * 6);
  final_type = (int *) bfd_alloc (abfd, (bfd_size_type) sizeof (int));
  if (!final_types || !final_type)
    return NULL;

  /* The field selector may require additional relocations to be
     generated.  It's impossible to know at this moment if additional
     relocations will be needed, so we make them.  The code to actually
     write the relocation/fixup stream is responsible for removing
     any redundant relocations.  */
  switch (field)
    {
    case e_fsel:
    case e_psel:
    case e_lpsel:
    case e_rpsel:
      final_types[0] = final_type;
      final_types[1] = NULL;
      final_types[2] = NULL;
      *final_type = base_type;
      break;

    case e_tsel:
    case e_ltsel:
    case e_rtsel:
      final_types[0] = (int *) bfd_alloc (abfd, (bfd_size_type) sizeof (int));
      if (!final_types[0])
	return NULL;
      if (field == e_tsel)
	*final_types[0] = R_FSEL;
      else if (field == e_ltsel)
	*final_types[0] = R_LSEL;
      else
	*final_types[0] = R_RSEL;
      final_types[1] = final_type;
      final_types[2] = NULL;
      *final_type = base_type;
      break;

    case e_lssel:
    case e_rssel:
      final_types[0] = (int *) bfd_alloc (abfd, (bfd_size_type) sizeof (int));
      if (!final_types[0])
	return NULL;
      *final_types[0] = R_S_MODE;
      final_types[1] = final_type;
      final_types[2] = NULL;
      *final_type = base_type;
      break;

    case e_lsel:
    case e_rsel:
      final_types[0] = (int *) bfd_alloc (abfd, (bfd_size_type) sizeof (int));
      if (!final_types[0])
	return NULL;
      *final_types[0] = R_N_MODE;
      final_types[1] = final_type;
      final_types[2] = NULL;
      *final_type = base_type;
      break;

    case e_ldsel:
    case e_rdsel:
      final_types[0] = (int *) bfd_alloc (abfd, (bfd_size_type) sizeof (int));
      if (!final_types[0])
	return NULL;
      *final_types[0] = R_D_MODE;
      final_types[1] = final_type;
      final_types[2] = NULL;
      *final_type = base_type;
      break;

    case e_lrsel:
    case e_rrsel:
      final_types[0] = (int *) bfd_alloc (abfd, (bfd_size_type) sizeof (int));
      if (!final_types[0])
	return NULL;
      *final_types[0] = R_R_MODE;
      final_types[1] = final_type;
      final_types[2] = NULL;
      *final_type = base_type;
      break;

    case e_nsel:
      final_types[0] = (int *) bfd_alloc (abfd, (bfd_size_type) sizeof (int));
      if (!final_types[0])
	return NULL;
      *final_types[0] = R_N1SEL;
      final_types[1] = final_type;
      final_types[2] = NULL;
      *final_type = base_type;
      break;

    case e_nlsel:
    case e_nlrsel:
      final_types[0] = (int *) bfd_alloc (abfd, (bfd_size_type) sizeof (int));
      if (!final_types[0])
	return NULL;
      *final_types[0] = R_N0SEL;
      final_types[1] = (int *) bfd_alloc (abfd, (bfd_size_type) sizeof (int));
      if (!final_types[1])
	return NULL;
      if (field == e_nlsel)
	*final_types[1] = R_N_MODE;
      else
	*final_types[1] = R_R_MODE;
      final_types[2] = final_type;
      final_types[3] = NULL;
      *final_type = base_type;
      break;
    }

  switch (base_type)
    {
    case R_HPPA:
      /* The difference of two symbols needs *very* special handling.  */
      if (sym_diff)
	{
	  bfd_size_type amt = sizeof (int);
	  final_types[0] = (int *) bfd_alloc (abfd, amt);
	  final_types[1] = (int *) bfd_alloc (abfd, amt);
	  final_types[2] = (int *) bfd_alloc (abfd, amt);
	  final_types[3] = (int *) bfd_alloc (abfd, amt);
	  if (!final_types[0] || !final_types[1] || !final_types[2])
	    return NULL;
	  if (field == e_fsel)
	    *final_types[0] = R_FSEL;
	  else if (field == e_rsel)
	    *final_types[0] = R_RSEL;
	  else if (field == e_lsel)
	    *final_types[0] = R_LSEL;
	  *final_types[1] = R_COMP2;
	  *final_types[2] = R_COMP2;
	  *final_types[3] = R_COMP1;
	  final_types[4] = final_type;
	  if (format == 32)
	    *final_types[4] = R_DATA_EXPR;
	  else
	    *final_types[4] = R_CODE_EXPR;
	  final_types[5] = NULL;
	  break;
	}
      /* PLABELs get their own relocation type.  */
      else if (field == e_psel
	       || field == e_lpsel
	       || field == e_rpsel)
	{
	  /* A PLABEL relocation that has a size of 32 bits must
	     be a R_DATA_PLABEL.  All others are R_CODE_PLABELs.  */
	  if (format == 32)
	    *final_type = R_DATA_PLABEL;
	  else
	    *final_type = R_CODE_PLABEL;
	}
      /* PIC stuff.  */
      else if (field == e_tsel
	       || field == e_ltsel
	       || field == e_rtsel)
	*final_type = R_DLT_REL;
      /* A relocation in the data space is always a full 32bits.  */
      else if (format == 32)
	{
	  *final_type = R_DATA_ONE_SYMBOL;

	  /* If there's no SOM symbol type associated with this BFD
	     symbol, then set the symbol type to ST_DATA.

	     Only do this if the type is going to default later when
	     we write the object file.

	     This is done so that the linker never encounters an
	     R_DATA_ONE_SYMBOL reloc involving an ST_CODE symbol.

	     This allows the compiler to generate exception handling
	     tables.

	     Note that one day we may need to also emit BEGIN_BRTAB and
	     END_BRTAB to prevent the linker from optimizing away insns
	     in exception handling regions.  */
	  if (som_symbol_data (sym)->som_type == SYMBOL_TYPE_UNKNOWN
	      && (sym->flags & BSF_SECTION_SYM) == 0
	      && (sym->flags & BSF_FUNCTION) == 0
	      && ! bfd_is_com_section (sym->section))
	    som_symbol_data (sym)->som_type = SYMBOL_TYPE_DATA;
	}
      break;

    case R_HPPA_GOTOFF:
      /* More PLABEL special cases.  */
      if (field == e_psel
	  || field == e_lpsel
	  || field == e_rpsel)
	*final_type = R_DATA_PLABEL;
      break;

    case R_HPPA_COMPLEX:
      /* The difference of two symbols needs *very* special handling.  */
      if (sym_diff)
	{
	  bfd_size_type amt = sizeof (int);
	  final_types[0] = (int *) bfd_alloc (abfd, amt);
	  final_types[1] = (int *) bfd_alloc (abfd, amt);
	  final_types[2] = (int *) bfd_alloc (abfd, amt);
	  final_types[3] = (int *) bfd_alloc (abfd, amt);
	  if (!final_types[0] || !final_types[1] || !final_types[2])
	    return NULL;
	  if (field == e_fsel)
	    *final_types[0] = R_FSEL;
	  else if (field == e_rsel)
	    *final_types[0] = R_RSEL;
	  else if (field == e_lsel)
	    *final_types[0] = R_LSEL;
	  *final_types[1] = R_COMP2;
	  *final_types[2] = R_COMP2;
	  *final_types[3] = R_COMP1;
	  final_types[4] = final_type;
	  if (format == 32)
	    *final_types[4] = R_DATA_EXPR;
	  else
	    *final_types[4] = R_CODE_EXPR;
	  final_types[5] = NULL;
	  break;
	}
      else
	break;

    case R_HPPA_NONE:
    case R_HPPA_ABS_CALL:
      /* Right now we can default all these.  */
      break;

    case R_HPPA_PCREL_CALL:
      {
#ifndef NO_PCREL_MODES
	/* If we have short and long pcrel modes, then generate the proper
	   mode selector, then the pcrel relocation.  Redundant selectors
	   will be eliminted as the relocs are sized and emitted.  */
	bfd_size_type amt = sizeof (int);
	final_types[0] = (int *) bfd_alloc (abfd, amt);
	if (!final_types[0])
	  return NULL;
	if (format == 17)
	  *final_types[0] = R_SHORT_PCREL_MODE;
	else
	  *final_types[0] = R_LONG_PCREL_MODE;
	final_types[1] = final_type;
	final_types[2] = NULL;
	*final_type = base_type;
#endif
	break;
      }
    }
  return final_types;
}

/* Return the address of the correct entry in the PA SOM relocation
   howto table.  */

static reloc_howto_type *
som_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  if ((int) code < (int) R_NO_RELOCATION + 255)
    {
      BFD_ASSERT ((int) som_hppa_howto_table[(int) code].type == (int) code);
      return &som_hppa_howto_table[(int) code];
    }

  return (reloc_howto_type *) 0;
}

/* Perform some initialization for an object.  Save results of this
   initialization in the BFD.  */

static const bfd_target *
som_object_setup (abfd, file_hdrp, aux_hdrp, current_offset)
     bfd *abfd;
     struct header *file_hdrp;
     struct som_exec_auxhdr *aux_hdrp;
     unsigned long current_offset;
{
  asection *section;
  int found;

  /* som_mkobject will set bfd_error if som_mkobject fails.  */
  if (! som_mkobject (abfd))
    return 0;

  /* Set BFD flags based on what information is available in the SOM.  */
  abfd->flags = BFD_NO_FLAGS;
  if (file_hdrp->symbol_total)
    abfd->flags |= HAS_LINENO | HAS_DEBUG | HAS_SYMS | HAS_LOCALS;

  switch (file_hdrp->a_magic)
    {
    case DEMAND_MAGIC:
      abfd->flags |= (D_PAGED | WP_TEXT | EXEC_P);
      break;
    case SHARE_MAGIC:
      abfd->flags |= (WP_TEXT | EXEC_P);
      break;
    case EXEC_MAGIC:
      abfd->flags |= (EXEC_P);
      break;
    case RELOC_MAGIC:
      abfd->flags |= HAS_RELOC;
      break;
#ifdef SHL_MAGIC
    case SHL_MAGIC:
#endif
#ifdef DL_MAGIC
    case DL_MAGIC:
#endif
      abfd->flags |= DYNAMIC;
      break;

    default:
      break;
    }

  /* Allocate space to hold the saved exec header information.  */
  obj_som_exec_data (abfd) = (struct som_exec_data *)
    bfd_zalloc (abfd, (bfd_size_type) sizeof (struct som_exec_data));
  if (obj_som_exec_data (abfd) == NULL)
    return NULL;

  /* The braindamaged OSF1 linker switched exec_flags and exec_entry!

     We used to identify OSF1 binaries based on NEW_VERSION_ID, but
     apparently the latest HPUX linker is using NEW_VERSION_ID now.

     It's about time, OSF has used the new id since at least 1992;
     HPUX didn't start till nearly 1995!.

     The new approach examines the entry field.  If it's zero or not 4
     byte aligned then it's not a proper code address and we guess it's
     really the executable flags.  */
  found = 0;
  for (section = abfd->sections; section; section = section->next)
    {
      if ((section->flags & SEC_CODE) == 0)
	continue;
      if (aux_hdrp->exec_entry >= section->vma
	  && aux_hdrp->exec_entry < section->vma + section->_cooked_size)
	found = 1;
    }
  if (aux_hdrp->exec_entry == 0
      || (aux_hdrp->exec_entry & 0x3) != 0
      || ! found)
    {
      bfd_get_start_address (abfd) = aux_hdrp->exec_flags;
      obj_som_exec_data (abfd)->exec_flags = aux_hdrp->exec_entry;
    }
  else
    {
      bfd_get_start_address (abfd) = aux_hdrp->exec_entry + current_offset;
      obj_som_exec_data (abfd)->exec_flags = aux_hdrp->exec_flags;
    }

  obj_som_exec_data (abfd)->version_id = file_hdrp->version_id;

  bfd_default_set_arch_mach (abfd, bfd_arch_hppa, pa10);
  bfd_get_symcount (abfd) = file_hdrp->symbol_total;

  /* Initialize the saved symbol table and string table to NULL.
     Save important offsets and sizes from the SOM header into
     the BFD.  */
  obj_som_stringtab (abfd) = (char *) NULL;
  obj_som_symtab (abfd) = (som_symbol_type *) NULL;
  obj_som_sorted_syms (abfd) = NULL;
  obj_som_stringtab_size (abfd) = file_hdrp->symbol_strings_size;
  obj_som_sym_filepos (abfd) = file_hdrp->symbol_location + current_offset;
  obj_som_str_filepos (abfd) = (file_hdrp->symbol_strings_location
				+ current_offset);
  obj_som_reloc_filepos (abfd) = (file_hdrp->fixup_request_location
				  + current_offset);
  obj_som_exec_data (abfd)->system_id = file_hdrp->system_id;

  return abfd->xvec;
}

/* Convert all of the space and subspace info into BFD sections.  Each space
   contains a number of subspaces, which in turn describe the mapping between
   regions of the exec file, and the address space that the program runs in.
   BFD sections which correspond to spaces will overlap the sections for the
   associated subspaces.  */

static boolean
setup_sections (abfd, file_hdr, current_offset)
     bfd *abfd;
     struct header *file_hdr;
     unsigned long current_offset;
{
  char *space_strings;
  unsigned int space_index, i;
  unsigned int total_subspaces = 0;
  asection **subspace_sections = NULL;
  asection *section;
  bfd_size_type amt;

  /* First, read in space names.  */

  amt = file_hdr->space_strings_size;
  space_strings = bfd_malloc (amt);
  if (!space_strings && amt != 0)
    goto error_return;

  if (bfd_seek (abfd, current_offset + file_hdr->space_strings_location,
		SEEK_SET) != 0)
    goto error_return;
  if (bfd_bread (space_strings, amt, abfd) != amt)
    goto error_return;

  /* Loop over all of the space dictionaries, building up sections.  */
  for (space_index = 0; space_index < file_hdr->space_total; space_index++)
    {
      struct space_dictionary_record space;
      struct subspace_dictionary_record subspace, save_subspace;
      int subspace_index;
      asection *space_asect;
      char *newname;

      /* Read the space dictionary element.  */
      if (bfd_seek (abfd,
		    (current_offset + file_hdr->space_location
		     + space_index * sizeof space),
		    SEEK_SET) != 0)
	goto error_return;
      amt = sizeof space;
      if (bfd_bread (&space, amt, abfd) != amt)
	goto error_return;

      /* Setup the space name string.  */
      space.name.n_name = space.name.n_strx + space_strings;

      /* Make a section out of it.  */
      amt = strlen (space.name.n_name) + 1;
      newname = bfd_alloc (abfd, amt);
      if (!newname)
	goto error_return;
      strcpy (newname, space.name.n_name);

      space_asect = bfd_make_section_anyway (abfd, newname);
      if (!space_asect)
	goto error_return;

      if (space.is_loadable == 0)
	space_asect->flags |= SEC_DEBUGGING;

      /* Set up all the attributes for the space.  */
      if (! bfd_som_set_section_attributes (space_asect, space.is_defined,
					    space.is_private, space.sort_key,
					    space.space_number))
	goto error_return;

      /* If the space has no subspaces, then we're done.  */
      if (space.subspace_quantity == 0)
	continue;

      /* Now, read in the first subspace for this space.  */
      if (bfd_seek (abfd,
		    (current_offset + file_hdr->subspace_location
		     + space.subspace_index * sizeof subspace),
		    SEEK_SET) != 0)
	goto error_return;
      amt = sizeof subspace;
      if (bfd_bread (&subspace, amt, abfd) != amt)
	goto error_return;
      /* Seek back to the start of the subspaces for loop below.  */
      if (bfd_seek (abfd,
		    (current_offset + file_hdr->subspace_location
		     + space.subspace_index * sizeof subspace),
		    SEEK_SET) != 0)
	goto error_return;

      /* Setup the start address and file loc from the first subspace
         record.  */
      space_asect->vma = subspace.subspace_start;
      space_asect->filepos = subspace.file_loc_init_value + current_offset;
      space_asect->alignment_power = log2 (subspace.alignment);
      if (space_asect->alignment_power == (unsigned) -1)
	goto error_return;

      /* Initialize save_subspace so we can reliably determine if this
	 loop placed any useful values into it.  */
      memset (&save_subspace, 0, sizeof (struct subspace_dictionary_record));

      /* Loop over the rest of the subspaces, building up more sections.  */
      for (subspace_index = 0; subspace_index < space.subspace_quantity;
	   subspace_index++)
	{
	  asection *subspace_asect;

	  /* Read in the next subspace.  */
	  amt = sizeof subspace;
	  if (bfd_bread (&subspace, amt, abfd) != amt)
	    goto error_return;

	  /* Setup the subspace name string.  */
	  subspace.name.n_name = subspace.name.n_strx + space_strings;

	  amt = strlen (subspace.name.n_name) + 1;
	  newname = bfd_alloc (abfd, amt);
	  if (!newname)
	    goto error_return;
	  strcpy (newname, subspace.name.n_name);

	  /* Make a section out of this subspace.  */
	  subspace_asect = bfd_make_section_anyway (abfd, newname);
	  if (!subspace_asect)
	    goto error_return;

	  /* Store private information about the section.  */
	  if (! bfd_som_set_subsection_attributes (subspace_asect, space_asect,
						   subspace.access_control_bits,
						   subspace.sort_key,
						   subspace.quadrant))
	    goto error_return;

	  /* Keep an easy mapping between subspaces and sections.
	     Note we do not necessarily read the subspaces in the
	     same order in which they appear in the object file.

	     So to make the target index come out correctly, we
	     store the location of the subspace header in target
	     index, then sort using the location of the subspace
	     header as the key.  Then we can assign correct
	     subspace indices.  */
	  total_subspaces++;
	  subspace_asect->target_index = bfd_tell (abfd) - sizeof (subspace);

	  /* Set SEC_READONLY and SEC_CODE/SEC_DATA as specified
	     by the access_control_bits in the subspace header.  */
	  switch (subspace.access_control_bits >> 4)
	    {
	    /* Readonly data.  */
	    case 0x0:
	      subspace_asect->flags |= SEC_DATA | SEC_READONLY;
	      break;

	    /* Normal data.  */
	    case 0x1:
	      subspace_asect->flags |= SEC_DATA;
	      break;

	    /* Readonly code and the gateways.
	       Gateways have other attributes which do not map
	       into anything BFD knows about.  */
	    case 0x2:
	    case 0x4:
	    case 0x5:
	    case 0x6:
	    case 0x7:
	      subspace_asect->flags |= SEC_CODE | SEC_READONLY;
	      break;

	    /* dynamic (writable) code.  */
	    case 0x3:
	      subspace_asect->flags |= SEC_CODE;
	      break;
	    }

	  if (subspace.dup_common || subspace.is_common)
	    subspace_asect->flags |= SEC_IS_COMMON;
	  else if (subspace.subspace_length > 0)
	    subspace_asect->flags |= SEC_HAS_CONTENTS;

	  if (subspace.is_loadable)
	    subspace_asect->flags |= SEC_ALLOC | SEC_LOAD;
	  else
	    subspace_asect->flags |= SEC_DEBUGGING;

	  if (subspace.code_only)
	    subspace_asect->flags |= SEC_CODE;

	  /* Both file_loc_init_value and initialization_length will
	     be zero for a BSS like subspace.  */
	  if (subspace.file_loc_init_value == 0
	      && subspace.initialization_length == 0)
	    subspace_asect->flags &= ~(SEC_DATA | SEC_LOAD | SEC_HAS_CONTENTS);

	  /* This subspace has relocations.
	     The fixup_request_quantity is a byte count for the number of
	     entries in the relocation stream; it is not the actual number
	     of relocations in the subspace.  */
	  if (subspace.fixup_request_quantity != 0)
	    {
	      subspace_asect->flags |= SEC_RELOC;
	      subspace_asect->rel_filepos = subspace.fixup_request_index;
	      som_section_data (subspace_asect)->reloc_size
		= subspace.fixup_request_quantity;
	      /* We can not determine this yet.  When we read in the
		 relocation table the correct value will be filled in.  */
	      subspace_asect->reloc_count = (unsigned) -1;
	    }

	  /* Update save_subspace if appropriate.  */
	  if (subspace.file_loc_init_value > save_subspace.file_loc_init_value)
	    save_subspace = subspace;

	  subspace_asect->vma = subspace.subspace_start;
	  subspace_asect->_cooked_size = subspace.subspace_length;
	  subspace_asect->_raw_size = subspace.subspace_length;
	  subspace_asect->filepos = (subspace.file_loc_init_value
				     + current_offset);
	  subspace_asect->alignment_power = log2 (subspace.alignment);
	  if (subspace_asect->alignment_power == (unsigned) -1)
	    goto error_return;
	}

      /* This can happen for a .o which defines symbols in otherwise
         empty subspaces.  */
      if (!save_subspace.file_loc_init_value)
	{
	  space_asect->_cooked_size = 0;
	  space_asect->_raw_size = 0;
	}
      else
	{
	  /* Setup the sizes for the space section based upon the info in the
	     last subspace of the space.  */
	  space_asect->_cooked_size = (save_subspace.subspace_start
				       - space_asect->vma
				       + save_subspace.subspace_length);
	  space_asect->_raw_size = (save_subspace.file_loc_init_value
				    - space_asect->filepos
				    + save_subspace.initialization_length);
	}
    }
  /* Now that we've read in all the subspace records, we need to assign
     a target index to each subspace.  */
  amt = total_subspaces;
  amt *= sizeof (asection *);
  subspace_sections = (asection **) bfd_malloc (amt);
  if (subspace_sections == NULL)
    goto error_return;

  for (i = 0, section = abfd->sections; section; section = section->next)
    {
      if (!som_is_subspace (section))
	continue;

      subspace_sections[i] = section;
      i++;
    }
  qsort (subspace_sections, total_subspaces,
	 sizeof (asection *), compare_subspaces);

  /* subspace_sections is now sorted in the order in which the subspaces
     appear in the object file.  Assign an index to each one now.  */
  for (i = 0; i < total_subspaces; i++)
    subspace_sections[i]->target_index = i;

  if (space_strings != NULL)
    free (space_strings);

  if (subspace_sections != NULL)
    free (subspace_sections);

  return true;

 error_return:
  if (space_strings != NULL)
    free (space_strings);

  if (subspace_sections != NULL)
    free (subspace_sections);
  return false;
}

/* Read in a SOM object and make it into a BFD.  */

static const bfd_target *
som_object_p (abfd)
     bfd *abfd;
{
  struct header file_hdr;
  struct som_exec_auxhdr aux_hdr;
  unsigned long current_offset = 0;
  struct lst_header lst_header;
  struct som_entry som_entry;
  bfd_size_type amt;
#define ENTRY_SIZE sizeof (struct som_entry)

  amt = FILE_HDR_SIZE;
  if (bfd_bread ((PTR) &file_hdr, amt, abfd) != amt)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return 0;
    }

  if (!_PA_RISC_ID (file_hdr.system_id))
    {
      bfd_set_error (bfd_error_wrong_format);
      return 0;
    }

  switch (file_hdr.a_magic)
    {
    case RELOC_MAGIC:
    case EXEC_MAGIC:
    case SHARE_MAGIC:
    case DEMAND_MAGIC:
#ifdef DL_MAGIC
    case DL_MAGIC:
#endif
#ifdef SHL_MAGIC
    case SHL_MAGIC:
#endif
#ifdef SHARED_MAGIC_CNX
    case SHARED_MAGIC_CNX:
#endif
      break;

#ifdef EXECLIBMAGIC
    case EXECLIBMAGIC:
      /* Read the lst header and determine where the SOM directory begins.  */

      if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_wrong_format);
	  return 0;
	}

      amt = SLSTHDR;
      if (bfd_bread ((PTR) &lst_header, amt, abfd) != amt)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_wrong_format);
	  return 0;
	}

      /* Position to and read the first directory entry.  */

      if (bfd_seek (abfd, lst_header.dir_loc, SEEK_SET) != 0)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_wrong_format);
	  return 0;
	}

      amt = ENTRY_SIZE;
      if (bfd_bread ((PTR) &som_entry, amt, abfd) != amt)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_wrong_format);
	  return 0;
	}

      /* Now position to the first SOM.  */

      if (bfd_seek (abfd, som_entry.location, SEEK_SET) != 0)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_wrong_format);
	  return 0;
	}

      current_offset = som_entry.location;

      /* And finally, re-read the som header.  */
      amt = FILE_HDR_SIZE;
      if (bfd_bread ((PTR) &file_hdr, amt, abfd) != amt)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_wrong_format);
	  return 0;
	}

      break;
#endif

    default:
      bfd_set_error (bfd_error_wrong_format);
      return 0;
    }

  if (file_hdr.version_id != VERSION_ID
      && file_hdr.version_id != NEW_VERSION_ID)
    {
      bfd_set_error (bfd_error_wrong_format);
      return 0;
    }

  /* If the aux_header_size field in the file header is zero, then this
     object is an incomplete executable (a .o file).  Do not try to read
     a non-existant auxiliary header.  */
  memset (&aux_hdr, 0, sizeof (struct som_exec_auxhdr));
  if (file_hdr.aux_header_size != 0)
    {
      amt = AUX_HDR_SIZE;
      if (bfd_bread ((PTR) &aux_hdr, amt, abfd) != amt)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_wrong_format);
	  return 0;
	}
    }

  if (!setup_sections (abfd, &file_hdr, current_offset))
    {
      /* setup_sections does not bubble up a bfd error code.  */
      bfd_set_error (bfd_error_bad_value);
      return 0;
    }

  /* This appears to be a valid SOM object.  Do some initialization.  */
  return som_object_setup (abfd, &file_hdr, &aux_hdr, current_offset);
}

/* Create a SOM object.  */

static boolean
som_mkobject (abfd)
     bfd *abfd;
{
  /* Allocate memory to hold backend information.  */
  abfd->tdata.som_data = (struct som_data_struct *)
    bfd_zalloc (abfd, (bfd_size_type) sizeof (struct som_data_struct));
  if (abfd->tdata.som_data == NULL)
    return false;
  return true;
}

/* Initialize some information in the file header.  This routine makes
   not attempt at doing the right thing for a full executable; it
   is only meant to handle relocatable objects.  */

static boolean
som_prep_headers (abfd)
     bfd *abfd;
{
  struct header *file_hdr;
  asection *section;
  bfd_size_type amt = sizeof (struct header);

  /* Make and attach a file header to the BFD.  */
  file_hdr = (struct header *) bfd_zalloc (abfd, amt);
  if (file_hdr == NULL)
    return false;
  obj_som_file_hdr (abfd) = file_hdr;

  if (abfd->flags & (EXEC_P | DYNAMIC))
    {

      /* Make and attach an exec header to the BFD.  */
      amt = sizeof (struct som_exec_auxhdr);
      obj_som_exec_hdr (abfd) =
	(struct som_exec_auxhdr *) bfd_zalloc (abfd, amt);
      if (obj_som_exec_hdr (abfd) == NULL)
	return false;

      if (abfd->flags & D_PAGED)
	file_hdr->a_magic = DEMAND_MAGIC;
      else if (abfd->flags & WP_TEXT)
	file_hdr->a_magic = SHARE_MAGIC;
#ifdef SHL_MAGIC
      else if (abfd->flags & DYNAMIC)
	file_hdr->a_magic = SHL_MAGIC;
#endif
      else
	file_hdr->a_magic = EXEC_MAGIC;
    }
  else
    file_hdr->a_magic = RELOC_MAGIC;

  /* These fields are optional, and embedding timestamps is not always
     a wise thing to do, it makes comparing objects during a multi-stage
     bootstrap difficult.  */
  file_hdr->file_time.secs = 0;
  file_hdr->file_time.nanosecs = 0;

  file_hdr->entry_space = 0;
  file_hdr->entry_subspace = 0;
  file_hdr->entry_offset = 0;
  file_hdr->presumed_dp = 0;

  /* Now iterate over the sections translating information from
     BFD sections to SOM spaces/subspaces.  */

  for (section = abfd->sections; section != NULL; section = section->next)
    {
      /* Ignore anything which has not been marked as a space or
	 subspace.  */
      if (!som_is_space (section) && !som_is_subspace (section))
	continue;

      if (som_is_space (section))
	{
	  /* Allocate space for the space dictionary.  */
	  amt = sizeof (struct space_dictionary_record);
	  som_section_data (section)->space_dict =
	    (struct space_dictionary_record *) bfd_zalloc (abfd, amt);
	  if (som_section_data (section)->space_dict == NULL)
	    return false;
	  /* Set space attributes.  Note most attributes of SOM spaces
	     are set based on the subspaces it contains.  */
	  som_section_data (section)->space_dict->loader_fix_index = -1;
	  som_section_data (section)->space_dict->init_pointer_index = -1;

	  /* Set more attributes that were stuffed away in private data.  */
	  som_section_data (section)->space_dict->sort_key =
	    som_section_data (section)->copy_data->sort_key;
	  som_section_data (section)->space_dict->is_defined =
	    som_section_data (section)->copy_data->is_defined;
	  som_section_data (section)->space_dict->is_private =
	    som_section_data (section)->copy_data->is_private;
	  som_section_data (section)->space_dict->space_number =
	    som_section_data (section)->copy_data->space_number;
	}
      else
	{
	  /* Allocate space for the subspace dictionary.  */
	  amt = sizeof (struct subspace_dictionary_record);
	  som_section_data (section)->subspace_dict =
	    (struct subspace_dictionary_record *) bfd_zalloc (abfd, amt);
	  if (som_section_data (section)->subspace_dict == NULL)
	    return false;

	  /* Set subspace attributes.  Basic stuff is done here, additional
	     attributes are filled in later as more information becomes
	     available.  */
	  if (section->flags & SEC_IS_COMMON)
	    {
	      som_section_data (section)->subspace_dict->dup_common = 1;
	      som_section_data (section)->subspace_dict->is_common = 1;
	    }

	  if (section->flags & SEC_ALLOC)
	    som_section_data (section)->subspace_dict->is_loadable = 1;

	  if (section->flags & SEC_CODE)
	    som_section_data (section)->subspace_dict->code_only = 1;

	  som_section_data (section)->subspace_dict->subspace_start =
	    section->vma;
	  som_section_data (section)->subspace_dict->subspace_length =
	    bfd_section_size (abfd, section);
	  som_section_data (section)->subspace_dict->initialization_length =
	    bfd_section_size (abfd, section);
	  som_section_data (section)->subspace_dict->alignment =
	    1 << section->alignment_power;

	  /* Set more attributes that were stuffed away in private data.  */
	  som_section_data (section)->subspace_dict->sort_key =
	    som_section_data (section)->copy_data->sort_key;
	  som_section_data (section)->subspace_dict->access_control_bits =
	    som_section_data (section)->copy_data->access_control_bits;
	  som_section_data (section)->subspace_dict->quadrant =
	    som_section_data (section)->copy_data->quadrant;
	}
    }
  return true;
}

/* Return true if the given section is a SOM space, false otherwise.  */

static boolean
som_is_space (section)
     asection *section;
{
  /* If no copy data is available, then it's neither a space nor a
     subspace.  */
  if (som_section_data (section)->copy_data == NULL)
    return false;

  /* If the containing space isn't the same as the given section,
     then this isn't a space.  */
  if (som_section_data (section)->copy_data->container != section
      && (som_section_data (section)->copy_data->container->output_section
	  != section))
    return false;

  /* OK.  Must be a space.  */
  return true;
}

/* Return true if the given section is a SOM subspace, false otherwise.  */

static boolean
som_is_subspace (section)
     asection *section;
{
  /* If no copy data is available, then it's neither a space nor a
     subspace.  */
  if (som_section_data (section)->copy_data == NULL)
    return false;

  /* If the containing space is the same as the given section,
     then this isn't a subspace.  */
  if (som_section_data (section)->copy_data->container == section
      || (som_section_data (section)->copy_data->container->output_section
	  == section))
    return false;

  /* OK.  Must be a subspace.  */
  return true;
}

/* Return true if the given space containins the given subspace.  It
   is safe to assume space really is a space, and subspace really
   is a subspace.  */

static boolean
som_is_container (space, subspace)
     asection *space, *subspace;
{
  return (som_section_data (subspace)->copy_data->container == space
	  || (som_section_data (subspace)->copy_data->container->output_section
	      == space));
}

/* Count and return the number of spaces attached to the given BFD.  */

static unsigned long
som_count_spaces (abfd)
     bfd *abfd;
{
  int count = 0;
  asection *section;

  for (section = abfd->sections; section != NULL; section = section->next)
    count += som_is_space (section);

  return count;
}

/* Count the number of subspaces attached to the given BFD.  */

static unsigned long
som_count_subspaces (abfd)
     bfd *abfd;
{
  int count = 0;
  asection *section;

  for (section = abfd->sections; section != NULL; section = section->next)
    count += som_is_subspace (section);

  return count;
}

/* Return -1, 0, 1 indicating the relative ordering of sym1 and sym2.

   We desire symbols to be ordered starting with the symbol with the
   highest relocation count down to the symbol with the lowest relocation
   count.  Doing so compacts the relocation stream.  */

static int
compare_syms (arg1, arg2)
     const PTR arg1;
     const PTR arg2;

{
  asymbol **sym1 = (asymbol **) arg1;
  asymbol **sym2 = (asymbol **) arg2;
  unsigned int count1, count2;

  /* Get relocation count for each symbol.  Note that the count
     is stored in the udata pointer for section symbols!  */
  if ((*sym1)->flags & BSF_SECTION_SYM)
    count1 = (*sym1)->udata.i;
  else
    count1 = som_symbol_data (*sym1)->reloc_count;

  if ((*sym2)->flags & BSF_SECTION_SYM)
    count2 = (*sym2)->udata.i;
  else
    count2 = som_symbol_data (*sym2)->reloc_count;

  /* Return the appropriate value.  */
  if (count1 < count2)
    return 1;
  else if (count1 > count2)
    return -1;
  return 0;
}

/* Return -1, 0, 1 indicating the relative ordering of subspace1
   and subspace.  */

static int
compare_subspaces (arg1, arg2)
     const PTR arg1;
     const PTR arg2;

{
  asection **subspace1 = (asection **) arg1;
  asection **subspace2 = (asection **) arg2;

  if ((*subspace1)->target_index < (*subspace2)->target_index)
    return -1;
  else if ((*subspace2)->target_index < (*subspace1)->target_index)
    return 1;
  else
    return 0;
}

/* Perform various work in preparation for emitting the fixup stream.  */

static void
som_prep_for_fixups (abfd, syms, num_syms)
     bfd *abfd;
     asymbol **syms;
     unsigned long num_syms;
{
  unsigned long i;
  asection *section;
  asymbol **sorted_syms;
  bfd_size_type amt;

  /* Most SOM relocations involving a symbol have a length which is
     dependent on the index of the symbol.  So symbols which are
     used often in relocations should have a small index.  */

  /* First initialize the counters for each symbol.  */
  for (i = 0; i < num_syms; i++)
    {
      /* Handle a section symbol; these have no pointers back to the
	 SOM symbol info.  So we just use the udata field to hold the
	 relocation count.  */
      if (som_symbol_data (syms[i]) == NULL
	  || syms[i]->flags & BSF_SECTION_SYM)
	{
	  syms[i]->flags |= BSF_SECTION_SYM;
	  syms[i]->udata.i = 0;
	}
      else
	som_symbol_data (syms[i])->reloc_count = 0;
    }

  /* Now that the counters are initialized, make a weighted count
     of how often a given symbol is used in a relocation.  */
  for (section = abfd->sections; section != NULL; section = section->next)
    {
      int j;

      /* Does this section have any relocations?  */
      if ((int) section->reloc_count <= 0)
	continue;

      /* Walk through each relocation for this section.  */
      for (j = 1; j < (int) section->reloc_count; j++)
	{
	  arelent *reloc = section->orelocation[j];
	  int scale;

	  /* A relocation against a symbol in the *ABS* section really
	     does not have a symbol.  Likewise if the symbol isn't associated
	     with any section.  */
	  if (reloc->sym_ptr_ptr == NULL
	      || bfd_is_abs_section ((*reloc->sym_ptr_ptr)->section))
	    continue;

	  /* Scaling to encourage symbols involved in R_DP_RELATIVE
	     and R_CODE_ONE_SYMBOL relocations to come first.  These
	     two relocations have single byte versions if the symbol
	     index is very small.  */
	  if (reloc->howto->type == R_DP_RELATIVE
	      || reloc->howto->type == R_CODE_ONE_SYMBOL)
	    scale = 2;
	  else
	    scale = 1;

	  /* Handle section symbols by storing the count in the udata
	     field.  It will not be used and the count is very important
	     for these symbols.  */
	  if ((*reloc->sym_ptr_ptr)->flags & BSF_SECTION_SYM)
	    {
	      (*reloc->sym_ptr_ptr)->udata.i =
		(*reloc->sym_ptr_ptr)->udata.i + scale;
	      continue;
	    }

	  /* A normal symbol.  Increment the count.  */
	  som_symbol_data (*reloc->sym_ptr_ptr)->reloc_count += scale;
	}
    }

  /* Sort a copy of the symbol table, rather than the canonical
     output symbol table.  */
  amt = num_syms;
  amt *= sizeof (asymbol *);
  sorted_syms = (asymbol **) bfd_zalloc (abfd, amt);
  memcpy (sorted_syms, syms, num_syms * sizeof (asymbol *));
  qsort (sorted_syms, num_syms, sizeof (asymbol *), compare_syms);
  obj_som_sorted_syms (abfd) = sorted_syms;

  /* Compute the symbol indexes, they will be needed by the relocation
     code.  */
  for (i = 0; i < num_syms; i++)
    {
      /* A section symbol.  Again, there is no pointer to backend symbol
	 information, so we reuse the udata field again.  */
      if (sorted_syms[i]->flags & BSF_SECTION_SYM)
	sorted_syms[i]->udata.i = i;
      else
	som_symbol_data (sorted_syms[i])->index = i;
    }
}

static boolean
som_write_fixups (abfd, current_offset, total_reloc_sizep)
     bfd *abfd;
     unsigned long current_offset;
     unsigned int *total_reloc_sizep;
{
  unsigned int i, j;
  /* Chunk of memory that we can use as buffer space, then throw
     away.  */
  unsigned char tmp_space[SOM_TMP_BUFSIZE];
  unsigned char *p;
  unsigned int total_reloc_size = 0;
  unsigned int subspace_reloc_size = 0;
  unsigned int num_spaces = obj_som_file_hdr (abfd)->space_total;
  asection *section = abfd->sections;
  bfd_size_type amt;

  memset (tmp_space, 0, SOM_TMP_BUFSIZE);
  p = tmp_space;

  /* All the fixups for a particular subspace are emitted in a single
     stream.  All the subspaces for a particular space are emitted
     as a single stream.

     So, to get all the locations correct one must iterate through all the
     spaces, for each space iterate through its subspaces and output a
     fixups stream.  */
  for (i = 0; i < num_spaces; i++)
    {
      asection *subsection;

      /* Find a space.  */
      while (!som_is_space (section))
	section = section->next;

      /* Now iterate through each of its subspaces.  */
      for (subsection = abfd->sections;
	   subsection != NULL;
	   subsection = subsection->next)
	{
	  int reloc_offset;
	  unsigned int current_rounding_mode;
#ifndef NO_PCREL_MODES
	  int current_call_mode;
#endif

	  /* Find a subspace of this space.  */
	  if (!som_is_subspace (subsection)
	      || !som_is_container (section, subsection))
	    continue;

	  /* If this subspace does not have real data, then we are
	     finised with it.  */
	  if ((subsection->flags & SEC_HAS_CONTENTS) == 0)
	    {
	      som_section_data (subsection)->subspace_dict->fixup_request_index
		= -1;
	      continue;
	    }

	  /* This subspace has some relocations.  Put the relocation stream
	     index into the subspace record.  */
	  som_section_data (subsection)->subspace_dict->fixup_request_index
	    = total_reloc_size;

	  /* To make life easier start over with a clean slate for
	     each subspace.  Seek to the start of the relocation stream
	     for this subspace in preparation for writing out its fixup
	     stream.  */
	  if (bfd_seek (abfd, current_offset + total_reloc_size, SEEK_SET) != 0)
	    return false;

	  /* Buffer space has already been allocated.  Just perform some
	     initialization here.  */
	  p = tmp_space;
	  subspace_reloc_size = 0;
	  reloc_offset = 0;
	  som_initialize_reloc_queue (reloc_queue);
	  current_rounding_mode = R_N_MODE;
#ifndef NO_PCREL_MODES
	  current_call_mode = R_SHORT_PCREL_MODE;
#endif

	  /* Translate each BFD relocation into one or more SOM
	     relocations.  */
	  for (j = 0; j < subsection->reloc_count; j++)
	    {
	      arelent *bfd_reloc = subsection->orelocation[j];
	      unsigned int skip;
	      int sym_num;

	      /* Get the symbol number.  Remember it's stored in a
		 special place for section symbols.  */
	      if ((*bfd_reloc->sym_ptr_ptr)->flags & BSF_SECTION_SYM)
		sym_num = (*bfd_reloc->sym_ptr_ptr)->udata.i;
	      else
		sym_num = som_symbol_data (*bfd_reloc->sym_ptr_ptr)->index;

	      /* If there is not enough room for the next couple relocations,
		 then dump the current buffer contents now.  Also reinitialize
		 the relocation queue.

		 No single BFD relocation could ever translate into more
		 than 100 bytes of SOM relocations (20bytes is probably the
		 upper limit, but leave lots of space for growth).  */
	      if (p - tmp_space + 100 > SOM_TMP_BUFSIZE)
		{
		  amt = p - tmp_space;
		  if (bfd_bwrite ((PTR) tmp_space, amt, abfd) != amt)
		    return false;

		  p = tmp_space;
		  som_initialize_reloc_queue (reloc_queue);
		}

	      /* Emit R_NO_RELOCATION fixups to map any bytes which were
		 skipped.  */
	      skip = bfd_reloc->address - reloc_offset;
	      p = som_reloc_skip (abfd, skip, p,
				  &subspace_reloc_size, reloc_queue);

	      /* Update reloc_offset for the next iteration.

		 Many relocations do not consume input bytes.  They
		 are markers, or set state necessary to perform some
		 later relocation.  */
	      switch (bfd_reloc->howto->type)
		{
		case R_ENTRY:
		case R_ALT_ENTRY:
		case R_EXIT:
		case R_N_MODE:
		case R_S_MODE:
		case R_D_MODE:
		case R_R_MODE:
		case R_FSEL:
		case R_LSEL:
		case R_RSEL:
		case R_COMP1:
		case R_COMP2:
		case R_BEGIN_BRTAB:
		case R_END_BRTAB:
		case R_BEGIN_TRY:
		case R_END_TRY:
		case R_N0SEL:
		case R_N1SEL:
#ifndef NO_PCREL_MODES
		case R_SHORT_PCREL_MODE:
		case R_LONG_PCREL_MODE:
#endif
		  reloc_offset = bfd_reloc->address;
		  break;

		default:
		  reloc_offset = bfd_reloc->address + 4;
		  break;
		}

	      /* Now the actual relocation we care about.  */
	      switch (bfd_reloc->howto->type)
		{
		case R_PCREL_CALL:
		case R_ABS_CALL:
		  p = som_reloc_call (abfd, p, &subspace_reloc_size,
				      bfd_reloc, sym_num, reloc_queue);
		  break;

		case R_CODE_ONE_SYMBOL:
		case R_DP_RELATIVE:
		  /* Account for any addend.  */
		  if (bfd_reloc->addend)
		    p = som_reloc_addend (abfd, bfd_reloc->addend, p,
					  &subspace_reloc_size, reloc_queue);

		  if (sym_num < 0x20)
		    {
		      bfd_put_8 (abfd, bfd_reloc->howto->type + sym_num, p);
		      subspace_reloc_size += 1;
		      p += 1;
		    }
		  else if (sym_num < 0x100)
		    {
		      bfd_put_8 (abfd, bfd_reloc->howto->type + 32, p);
		      bfd_put_8 (abfd, sym_num, p + 1);
		      p = try_prev_fixup (abfd, &subspace_reloc_size, p,
					  2, reloc_queue);
		    }
		  else if (sym_num < 0x10000000)
		    {
		      bfd_put_8 (abfd, bfd_reloc->howto->type + 33, p);
		      bfd_put_8 (abfd, sym_num >> 16, p + 1);
		      bfd_put_16 (abfd, (bfd_vma) sym_num, p + 2);
		      p = try_prev_fixup (abfd, &subspace_reloc_size,
					  p, 4, reloc_queue);
		    }
		  else
		    abort ();
		  break;

		case R_DATA_ONE_SYMBOL:
		case R_DATA_PLABEL:
		case R_CODE_PLABEL:
		case R_DLT_REL:
		  /* Account for any addend using R_DATA_OVERRIDE.  */
		  if (bfd_reloc->howto->type != R_DATA_ONE_SYMBOL
		      && bfd_reloc->addend)
		    p = som_reloc_addend (abfd, bfd_reloc->addend, p,
					  &subspace_reloc_size, reloc_queue);

		  if (sym_num < 0x100)
		    {
		      bfd_put_8 (abfd, bfd_reloc->howto->type, p);
		      bfd_put_8 (abfd, sym_num, p + 1);
		      p = try_prev_fixup (abfd, &subspace_reloc_size, p,
					  2, reloc_queue);
		    }
		  else if (sym_num < 0x10000000)
		    {
		      bfd_put_8 (abfd, bfd_reloc->howto->type + 1, p);
		      bfd_put_8 (abfd, sym_num >> 16, p + 1);
		      bfd_put_16 (abfd, (bfd_vma) sym_num, p + 2);
		      p = try_prev_fixup (abfd, &subspace_reloc_size,
					  p, 4, reloc_queue);
		    }
		  else
		    abort ();
		  break;

		case R_ENTRY:
		  {
		    unsigned int tmp;
		    arelent *tmp_reloc = NULL;
		    bfd_put_8 (abfd, R_ENTRY, p);

		    /* R_ENTRY relocations have 64 bits of associated
		       data.  Unfortunately the addend field of a bfd
		       relocation is only 32 bits.  So, we split up
		       the 64bit unwind information and store part in
		       the R_ENTRY relocation, and the rest in the R_EXIT
		       relocation.  */
		    bfd_put_32 (abfd, bfd_reloc->addend, p + 1);

		    /* Find the next R_EXIT relocation.  */
		    for (tmp = j; tmp < subsection->reloc_count; tmp++)
		      {
			tmp_reloc = subsection->orelocation[tmp];
			if (tmp_reloc->howto->type == R_EXIT)
			  break;
		      }

		    if (tmp == subsection->reloc_count)
		      abort ();

		    bfd_put_32 (abfd, tmp_reloc->addend, p + 5);
		    p = try_prev_fixup (abfd, &subspace_reloc_size,
					p, 9, reloc_queue);
		    break;
		  }

		case R_N_MODE:
		case R_S_MODE:
		case R_D_MODE:
		case R_R_MODE:
		  /* If this relocation requests the current rounding
		     mode, then it is redundant.  */
		  if (bfd_reloc->howto->type != current_rounding_mode)
		    {
		      bfd_put_8 (abfd, bfd_reloc->howto->type, p);
		      subspace_reloc_size += 1;
		      p += 1;
		      current_rounding_mode = bfd_reloc->howto->type;
		    }
		  break;

#ifndef NO_PCREL_MODES
		case R_LONG_PCREL_MODE:
		case R_SHORT_PCREL_MODE:
		  if (bfd_reloc->howto->type != current_call_mode)
		    {
		      bfd_put_8 (abfd, bfd_reloc->howto->type, p);
		      subspace_reloc_size += 1;
		      p += 1;
		      current_call_mode = bfd_reloc->howto->type;
		    }
		  break;
#endif

		case R_EXIT:
		case R_ALT_ENTRY:
		case R_FSEL:
		case R_LSEL:
		case R_RSEL:
		case R_BEGIN_BRTAB:
		case R_END_BRTAB:
		case R_BEGIN_TRY:
		case R_N0SEL:
		case R_N1SEL:
		  bfd_put_8 (abfd, bfd_reloc->howto->type, p);
		  subspace_reloc_size += 1;
		  p += 1;
		  break;

		case R_END_TRY:
		  /* The end of an exception handling region.  The reloc's
		     addend contains the offset of the exception handling
		     code.  */
		  if (bfd_reloc->addend == 0)
		    bfd_put_8 (abfd, bfd_reloc->howto->type, p);
		  else if (bfd_reloc->addend < 1024)
		    {
		      bfd_put_8 (abfd, bfd_reloc->howto->type + 1, p);
		      bfd_put_8 (abfd, bfd_reloc->addend / 4, p + 1);
		      p = try_prev_fixup (abfd, &subspace_reloc_size,
					  p, 2, reloc_queue);
		    }
		  else
		    {
		      bfd_put_8 (abfd, bfd_reloc->howto->type + 2, p);
		      bfd_put_8 (abfd, (bfd_reloc->addend / 4) >> 16, p + 1);
		      bfd_put_16 (abfd, bfd_reloc->addend / 4, p + 2);
		      p = try_prev_fixup (abfd, &subspace_reloc_size,
					  p, 4, reloc_queue);
		    }
		  break;

		case R_COMP1:
		  /* The only time we generate R_COMP1, R_COMP2 and
		     R_CODE_EXPR relocs is for the difference of two
		     symbols.  Hence we can cheat here.  */
		  bfd_put_8 (abfd, bfd_reloc->howto->type, p);
		  bfd_put_8 (abfd, 0x44, p + 1);
		  p = try_prev_fixup (abfd, &subspace_reloc_size,
				      p, 2, reloc_queue);
		  break;

		case R_COMP2:
		  /* The only time we generate R_COMP1, R_COMP2 and
		     R_CODE_EXPR relocs is for the difference of two
		     symbols.  Hence we can cheat here.  */
		  bfd_put_8 (abfd, bfd_reloc->howto->type, p);
		  bfd_put_8 (abfd, 0x80, p + 1);
		  bfd_put_8 (abfd, sym_num >> 16, p + 2);
		  bfd_put_16 (abfd, (bfd_vma) sym_num, p + 3);
		  p = try_prev_fixup (abfd, &subspace_reloc_size,
				      p, 5, reloc_queue);
		  break;

		case R_CODE_EXPR:
		case R_DATA_EXPR:
		  /* The only time we generate R_COMP1, R_COMP2 and
		     R_CODE_EXPR relocs is for the difference of two
		     symbols.  Hence we can cheat here.  */
		  bfd_put_8 (abfd, bfd_reloc->howto->type, p);
		  subspace_reloc_size += 1;
		  p += 1;
		  break;

		/* Put a "R_RESERVED" relocation in the stream if
		   we hit something we do not understand.  The linker
		   will complain loudly if this ever happens.  */
		default:
		  bfd_put_8 (abfd, 0xff, p);
		  subspace_reloc_size += 1;
		  p += 1;
		  break;
		}
	    }

	  /* Last BFD relocation for a subspace has been processed.
	     Map the rest of the subspace with R_NO_RELOCATION fixups.  */
	  p = som_reloc_skip (abfd, bfd_section_size (abfd, subsection)
			              - reloc_offset,
			      p, &subspace_reloc_size, reloc_queue);

	  /* Scribble out the relocations.  */
	  amt = p - tmp_space;
	  if (bfd_bwrite ((PTR) tmp_space, amt, abfd) != amt)
	    return false;
	  p = tmp_space;

	  total_reloc_size += subspace_reloc_size;
	  som_section_data (subsection)->subspace_dict->fixup_request_quantity
	    = subspace_reloc_size;
	}
      section = section->next;
    }
  *total_reloc_sizep = total_reloc_size;
  return true;
}

/* Write out the space/subspace string table.  */

static boolean
som_write_space_strings (abfd, current_offset, string_sizep)
     bfd *abfd;
     unsigned long current_offset;
     unsigned int *string_sizep;
{
  /* Chunk of memory that we can use as buffer space, then throw
     away.  */
  size_t tmp_space_size = SOM_TMP_BUFSIZE;
  unsigned char *tmp_space = alloca (tmp_space_size);
  unsigned char *p = tmp_space;
  unsigned int strings_size = 0;
  asection *section;
  bfd_size_type amt;

  /* Seek to the start of the space strings in preparation for writing
     them out.  */
  if (bfd_seek (abfd, (file_ptr) current_offset, SEEK_SET) != 0)
    return false;

  /* Walk through all the spaces and subspaces (order is not important)
     building up and writing string table entries for their names.  */
  for (section = abfd->sections; section != NULL; section = section->next)
    {
      size_t length;

      /* Only work with space/subspaces; avoid any other sections
	 which might have been made (.text for example).  */
      if (!som_is_space (section) && !som_is_subspace (section))
	continue;

      /* Get the length of the space/subspace name.  */
      length = strlen (section->name);

      /* If there is not enough room for the next entry, then dump the
         current buffer contents now and maybe allocate a larger
         buffer.  Each entry will take 4 bytes to hold the string
         length + the string itself + null terminator.  */
      if (p - tmp_space + 5 + length > tmp_space_size)
	{
	  /* Flush buffer before refilling or reallocating.  */
	  amt = p - tmp_space;
	  if (bfd_bwrite ((PTR) &tmp_space[0], amt, abfd) != amt)
	    return false;

	  /* Reallocate if now empty buffer still too small.  */
	  if (5 + length > tmp_space_size)
	    {
	      /* Ensure a minimum growth factor to avoid O(n**2) space
                 consumption for n strings.  The optimal minimum
                 factor seems to be 2, as no other value can guarantee
                 wasting less than 50% space.  (Note that we cannot
                 deallocate space allocated by `alloca' without
                 returning from this function.)  The same technique is
                 used a few more times below when a buffer is
                 reallocated.  */
	      tmp_space_size = MAX (2 * tmp_space_size, 5 + length);
	      tmp_space = alloca (tmp_space_size);
	    }

	  /* Reset to beginning of the (possibly new) buffer space.  */
	  p = tmp_space;
	}

      /* First element in a string table entry is the length of the
	 string.  Alignment issues are already handled.  */
      bfd_put_32 (abfd, (bfd_vma) length, p);
      p += 4;
      strings_size += 4;

      /* Record the index in the space/subspace records.  */
      if (som_is_space (section))
	som_section_data (section)->space_dict->name.n_strx = strings_size;
      else
	som_section_data (section)->subspace_dict->name.n_strx = strings_size;

      /* Next comes the string itself + a null terminator.  */
      strcpy (p, section->name);
      p += length + 1;
      strings_size += length + 1;

      /* Always align up to the next word boundary.  */
      while (strings_size % 4)
	{
	  bfd_put_8 (abfd, 0, p);
	  p++;
	  strings_size++;
	}
    }

  /* Done with the space/subspace strings.  Write out any information
     contained in a partial block.  */
  amt = p - tmp_space;
  if (bfd_bwrite ((PTR) &tmp_space[0], amt, abfd) != amt)
    return false;
  *string_sizep = strings_size;
  return true;
}

/* Write out the symbol string table.  */

static boolean
som_write_symbol_strings (abfd, current_offset, syms, num_syms, string_sizep,
			  compilation_unit)
     bfd *abfd;
     unsigned long current_offset;
     asymbol **syms;
     unsigned int num_syms;
     unsigned int *string_sizep;
     COMPUNIT *compilation_unit;
{
  unsigned int i;

  /* Chunk of memory that we can use as buffer space, then throw
     away.  */
  size_t tmp_space_size = SOM_TMP_BUFSIZE;
  unsigned char *tmp_space = alloca (tmp_space_size);
  unsigned char *p = tmp_space;

  unsigned int strings_size = 0;
  unsigned char *comp[4];
  bfd_size_type amt;

  /* This gets a bit gruesome because of the compilation unit.  The
     strings within the compilation unit are part of the symbol
     strings, but don't have symbol_dictionary entries.  So, manually
     write them and update the compliation unit header.  On input, the
     compilation unit header contains local copies of the strings.
     Move them aside.  */
  if (compilation_unit)
    {
      comp[0] = compilation_unit->name.n_name;
      comp[1] = compilation_unit->language_name.n_name;
      comp[2] = compilation_unit->product_id.n_name;
      comp[3] = compilation_unit->version_id.n_name;
    }

  /* Seek to the start of the space strings in preparation for writing
     them out.  */
  if (bfd_seek (abfd, (file_ptr) current_offset, SEEK_SET) != 0)
    return false;

  if (compilation_unit)
    {
      for (i = 0; i < 4; i++)
	{
	  size_t length = strlen (comp[i]);

	  /* If there is not enough room for the next entry, then dump
	     the current buffer contents now and maybe allocate a
	     larger buffer.  */
	  if (p - tmp_space + 5 + length > tmp_space_size)
	    {
	      /* Flush buffer before refilling or reallocating.  */
	      amt = p - tmp_space;
	      if (bfd_bwrite ((PTR) &tmp_space[0], amt, abfd) != amt)
		return false;

	      /* Reallocate if now empty buffer still too small.  */
	      if (5 + length > tmp_space_size)
		{
		  /* See alloca above for discussion of new size.  */
		  tmp_space_size = MAX (2 * tmp_space_size, 5 + length);
		  tmp_space = alloca (tmp_space_size);
		}

	      /* Reset to beginning of the (possibly new) buffer
                 space.  */
	      p = tmp_space;
	    }

	  /* First element in a string table entry is the length of
	     the string.  This must always be 4 byte aligned.  This is
	     also an appropriate time to fill in the string index
	     field in the symbol table entry.  */
	  bfd_put_32 (abfd, (bfd_vma) length, p);
	  strings_size += 4;
	  p += 4;

	  /* Next comes the string itself + a null terminator.  */
	  strcpy (p, comp[i]);

	  switch (i)
	    {
	    case 0:
	      obj_som_compilation_unit (abfd)->name.n_strx = strings_size;
	      break;
	    case 1:
	      obj_som_compilation_unit (abfd)->language_name.n_strx =
		strings_size;
	      break;
	    case 2:
	      obj_som_compilation_unit (abfd)->product_id.n_strx =
		strings_size;
	      break;
	    case 3:
	      obj_som_compilation_unit (abfd)->version_id.n_strx =
		strings_size;
	      break;
	    }

	  p += length + 1;
	  strings_size += length + 1;

	  /* Always align up to the next word boundary.  */
	  while (strings_size % 4)
	    {
	      bfd_put_8 (abfd, 0, p);
	      strings_size++;
	      p++;
	    }
	}
    }

  for (i = 0; i < num_syms; i++)
    {
      size_t length = strlen (syms[i]->name);

      /* If there is not enough room for the next entry, then dump the
	 current buffer contents now and maybe allocate a larger buffer.  */
     if (p - tmp_space + 5 + length > tmp_space_size)
	{
	  /* Flush buffer before refilling or reallocating.  */
	  amt = p - tmp_space;
	  if (bfd_bwrite ((PTR) &tmp_space[0], amt, abfd) != amt)
	    return false;

	  /* Reallocate if now empty buffer still too small.  */
	  if (5 + length > tmp_space_size)
	    {
	      /* See alloca above for discussion of new size.  */
	      tmp_space_size = MAX (2 * tmp_space_size, 5 + length);
	      tmp_space = alloca (tmp_space_size);
	    }

	  /* Reset to beginning of the (possibly new) buffer space.  */
	  p = tmp_space;
	}

      /* First element in a string table entry is the length of the
	 string.  This must always be 4 byte aligned.  This is also
	 an appropriate time to fill in the string index field in the
	 symbol table entry.  */
      bfd_put_32 (abfd, (bfd_vma) length, p);
      strings_size += 4;
      p += 4;

      /* Next comes the string itself + a null terminator.  */
      strcpy (p, syms[i]->name);

      som_symbol_data (syms[i])->stringtab_offset = strings_size;
      p += length + 1;
      strings_size += length + 1;

      /* Always align up to the next word boundary.  */
      while (strings_size % 4)
	{
	  bfd_put_8 (abfd, 0, p);
	  strings_size++;
	  p++;
	}
    }

  /* Scribble out any partial block.  */
  amt = p - tmp_space;
  if (bfd_bwrite ((PTR) &tmp_space[0], amt, abfd) != amt)
    return false;

  *string_sizep = strings_size;
  return true;
}

/* Compute variable information to be placed in the SOM headers,
   space/subspace dictionaries, relocation streams, etc.  Begin
   writing parts of the object file.  */

static boolean
som_begin_writing (abfd)
     bfd *abfd;
{
  unsigned long current_offset = 0;
  int strings_size = 0;
  unsigned long num_spaces, num_subspaces, i;
  asection *section;
  unsigned int total_subspaces = 0;
  struct som_exec_auxhdr *exec_header = NULL;

  /* The file header will always be first in an object file,
     everything else can be in random locations.  To keep things
     "simple" BFD will lay out the object file in the manner suggested
     by the PRO ABI for PA-RISC Systems.  */

  /* Before any output can really begin offsets for all the major
     portions of the object file must be computed.  So, starting
     with the initial file header compute (and sometimes write)
     each portion of the object file.  */

  /* Make room for the file header, it's contents are not complete
     yet, so it can not be written at this time.  */
  current_offset += sizeof (struct header);

  /* Any auxiliary headers will follow the file header.  Right now
     we support only the copyright and version headers.  */
  obj_som_file_hdr (abfd)->aux_header_location = current_offset;
  obj_som_file_hdr (abfd)->aux_header_size = 0;
  if (abfd->flags & (EXEC_P | DYNAMIC))
    {
      /* Parts of the exec header will be filled in later, so
	 delay writing the header itself.  Fill in the defaults,
	 and write it later.  */
      current_offset += sizeof (struct som_exec_auxhdr);
      obj_som_file_hdr (abfd)->aux_header_size
	+= sizeof (struct som_exec_auxhdr);
      exec_header = obj_som_exec_hdr (abfd);
      exec_header->som_auxhdr.type = EXEC_AUX_ID;
      exec_header->som_auxhdr.length = 40;
    }
  if (obj_som_version_hdr (abfd) != NULL)
    {
      bfd_size_type len;

      if (bfd_seek (abfd, (file_ptr) current_offset, SEEK_SET) != 0)
	return false;

      /* Write the aux_id structure and the string length.  */
      len = sizeof (struct aux_id) + sizeof (unsigned int);
      obj_som_file_hdr (abfd)->aux_header_size += len;
      current_offset += len;
      if (bfd_bwrite ((PTR) obj_som_version_hdr (abfd), len, abfd) != len)
	return false;

      /* Write the version string.  */
      len = obj_som_version_hdr (abfd)->header_id.length - sizeof (int);
      obj_som_file_hdr (abfd)->aux_header_size += len;
      current_offset += len;
      if (bfd_bwrite ((PTR) obj_som_version_hdr (abfd)->user_string, len, abfd)
	  != len)
	return false;
    }

  if (obj_som_copyright_hdr (abfd) != NULL)
    {
      bfd_size_type len;

      if (bfd_seek (abfd, (file_ptr) current_offset, SEEK_SET) != 0)
	return false;

      /* Write the aux_id structure and the string length.  */
      len = sizeof (struct aux_id) + sizeof (unsigned int);
      obj_som_file_hdr (abfd)->aux_header_size += len;
      current_offset += len;
      if (bfd_bwrite ((PTR) obj_som_copyright_hdr (abfd), len, abfd) != len)
	return false;

      /* Write the copyright string.  */
      len = obj_som_copyright_hdr (abfd)->header_id.length - sizeof (int);
      obj_som_file_hdr (abfd)->aux_header_size += len;
      current_offset += len;
      if (bfd_bwrite ((PTR) obj_som_copyright_hdr (abfd)->copyright, len, abfd)
	  != len)
	return false;
    }

  /* Next comes the initialization pointers; we have no initialization
     pointers, so current offset does not change.  */
  obj_som_file_hdr (abfd)->init_array_location = current_offset;
  obj_som_file_hdr (abfd)->init_array_total = 0;

  /* Next are the space records.  These are fixed length records.

     Count the number of spaces to determine how much room is needed
     in the object file for the space records.

     The names of the spaces are stored in a separate string table,
     and the index for each space into the string table is computed
     below.  Therefore, it is not possible to write the space headers
     at this time.  */
  num_spaces = som_count_spaces (abfd);
  obj_som_file_hdr (abfd)->space_location = current_offset;
  obj_som_file_hdr (abfd)->space_total = num_spaces;
  current_offset += num_spaces * sizeof (struct space_dictionary_record);

  /* Next are the subspace records.  These are fixed length records.

     Count the number of subspaes to determine how much room is needed
     in the object file for the subspace records.

     A variety if fields in the subspace record are still unknown at
     this time (index into string table, fixup stream location/size, etc).  */
  num_subspaces = som_count_subspaces (abfd);
  obj_som_file_hdr (abfd)->subspace_location = current_offset;
  obj_som_file_hdr (abfd)->subspace_total = num_subspaces;
  current_offset += num_subspaces * sizeof (struct subspace_dictionary_record);

  /* Next is the string table for the space/subspace names.  We will
     build and write the string table on the fly.  At the same time
     we will fill in the space/subspace name index fields.  */

  /* The string table needs to be aligned on a word boundary.  */
  if (current_offset % 4)
    current_offset += (4 - (current_offset % 4));

  /* Mark the offset of the space/subspace string table in the
     file header.  */
  obj_som_file_hdr (abfd)->space_strings_location = current_offset;

  /* Scribble out the space strings.  */
  if (! som_write_space_strings (abfd, current_offset, &strings_size))
    return false;

  /* Record total string table size in the header and update the
     current offset.  */
  obj_som_file_hdr (abfd)->space_strings_size = strings_size;
  current_offset += strings_size;

  /* Next is the compilation unit.  */
  obj_som_file_hdr (abfd)->compiler_location = current_offset;
  obj_som_file_hdr (abfd)->compiler_total = 0;
  if (obj_som_compilation_unit (abfd))
    {
      obj_som_file_hdr (abfd)->compiler_total = 1;
      current_offset += COMPUNITSZ;
    }

  /* Now compute the file positions for the loadable subspaces, taking
     care to make sure everything stays properly aligned.  */

  section = abfd->sections;
  for (i = 0; i < num_spaces; i++)
    {
      asection *subsection;
      int first_subspace;
      unsigned int subspace_offset = 0;

      /* Find a space.  */
      while (!som_is_space (section))
	section = section->next;

      first_subspace = 1;
      /* Now look for all its subspaces.  */
      for (subsection = abfd->sections;
	   subsection != NULL;
	   subsection = subsection->next)
	{

	  if (!som_is_subspace (subsection)
	      || !som_is_container (section, subsection)
	      || (subsection->flags & SEC_ALLOC) == 0)
	    continue;

	  /* If this is the first subspace in the space, and we are
	     building an executable, then take care to make sure all
	     the alignments are correct and update the exec header.  */
	  if (first_subspace
	      && (abfd->flags & (EXEC_P | DYNAMIC)))
	    {
	      /* Demand paged executables have each space aligned to a
		 page boundary.  Sharable executables (write-protected
		 text) have just the private (aka data & bss) space aligned
		 to a page boundary.  Ugh.  Not true for HPUX.

		 The HPUX kernel requires the text to always be page aligned
		 within the file regardless of the executable's type.  */
	      if (abfd->flags & (D_PAGED | DYNAMIC)
		  || (subsection->flags & SEC_CODE)
		  || ((abfd->flags & WP_TEXT)
		      && (subsection->flags & SEC_DATA)))
		current_offset = SOM_ALIGN (current_offset, PA_PAGESIZE);

	      /* Update the exec header.  */
	      if (subsection->flags & SEC_CODE && exec_header->exec_tfile == 0)
		{
		  exec_header->exec_tmem = section->vma;
		  exec_header->exec_tfile = current_offset;
		}
	      if (subsection->flags & SEC_DATA && exec_header->exec_dfile == 0)
		{
		  exec_header->exec_dmem = section->vma;
		  exec_header->exec_dfile = current_offset;
		}

	      /* Keep track of exactly where we are within a particular
		 space.  This is necessary as the braindamaged HPUX
		 loader will create holes between subspaces *and*
		 subspace alignments are *NOT* preserved.  What a crock.  */
	      subspace_offset = subsection->vma;

	      /* Only do this for the first subspace within each space.  */
	      first_subspace = 0;
	    }
	  else if (abfd->flags & (EXEC_P | DYNAMIC))
	    {
	      /* The braindamaged HPUX loader may have created a hole
		 between two subspaces.  It is *not* sufficient to use
		 the alignment specifications within the subspaces to
		 account for these holes -- I've run into at least one
		 case where the loader left one code subspace unaligned
		 in a final executable.

		 To combat this we keep a current offset within each space,
		 and use the subspace vma fields to detect and preserve
		 holes.  What a crock!

		 ps.  This is not necessary for unloadable space/subspaces.  */
	      current_offset += subsection->vma - subspace_offset;
	      if (subsection->flags & SEC_CODE)
		exec_header->exec_tsize += subsection->vma - subspace_offset;
	      else
		exec_header->exec_dsize += subsection->vma - subspace_offset;
	      subspace_offset += subsection->vma - subspace_offset;
	    }

	  subsection->target_index = total_subspaces++;
	  /* This is real data to be loaded from the file.  */
	  if (subsection->flags & SEC_LOAD)
	    {
	      /* Update the size of the code & data.  */
	      if (abfd->flags & (EXEC_P | DYNAMIC)
		  && subsection->flags & SEC_CODE)
		exec_header->exec_tsize += subsection->_cooked_size;
	      else if (abfd->flags & (EXEC_P | DYNAMIC)
		       && subsection->flags & SEC_DATA)
		exec_header->exec_dsize += subsection->_cooked_size;
	      som_section_data (subsection)->subspace_dict->file_loc_init_value
		= current_offset;
	      subsection->filepos = current_offset;
	      current_offset += bfd_section_size (abfd, subsection);
	      subspace_offset += bfd_section_size (abfd, subsection);
	    }
	  /* Looks like uninitialized data.  */
	  else
	    {
	      /* Update the size of the bss section.  */
	      if (abfd->flags & (EXEC_P | DYNAMIC))
		exec_header->exec_bsize += subsection->_cooked_size;

	      som_section_data (subsection)->subspace_dict->file_loc_init_value
		= 0;
	      som_section_data (subsection)->subspace_dict->
		initialization_length = 0;
	    }
	}
      /* Goto the next section.  */
      section = section->next;
    }

  /* Finally compute the file positions for unloadable subspaces.
     If building an executable, start the unloadable stuff on its
     own page.  */

  if (abfd->flags & (EXEC_P | DYNAMIC))
    current_offset = SOM_ALIGN (current_offset, PA_PAGESIZE);

  obj_som_file_hdr (abfd)->unloadable_sp_location = current_offset;
  section = abfd->sections;
  for (i = 0; i < num_spaces; i++)
    {
      asection *subsection;

      /* Find a space.  */
      while (!som_is_space (section))
	section = section->next;

      if (abfd->flags & (EXEC_P | DYNAMIC))
	current_offset = SOM_ALIGN (current_offset, PA_PAGESIZE);

      /* Now look for all its subspaces.  */
      for (subsection = abfd->sections;
	   subsection != NULL;
	   subsection = subsection->next)
	{

	  if (!som_is_subspace (subsection)
	      || !som_is_container (section, subsection)
	      || (subsection->flags & SEC_ALLOC) != 0)
	    continue;

	  subsection->target_index = total_subspaces++;
	  /* This is real data to be loaded from the file.  */
	  if ((subsection->flags & SEC_LOAD) == 0)
	    {
	      som_section_data (subsection)->subspace_dict->file_loc_init_value
		= current_offset;
	      subsection->filepos = current_offset;
	      current_offset += bfd_section_size (abfd, subsection);
	    }
	  /* Looks like uninitialized data.  */
	  else
	    {
	      som_section_data (subsection)->subspace_dict->file_loc_init_value
		= 0;
	      som_section_data (subsection)->subspace_dict->
		initialization_length = bfd_section_size (abfd, subsection);
	    }
	}
      /* Goto the next section.  */
      section = section->next;
    }

  /* If building an executable, then make sure to seek to and write
     one byte at the end of the file to make sure any necessary
     zeros are filled in.  Ugh.  */
  if (abfd->flags & (EXEC_P | DYNAMIC))
    current_offset = SOM_ALIGN (current_offset, PA_PAGESIZE);
  if (bfd_seek (abfd, (file_ptr) current_offset - 1, SEEK_SET) != 0)
    return false;
  if (bfd_bwrite ((PTR) "", (bfd_size_type) 1, abfd) != 1)
    return false;

  obj_som_file_hdr (abfd)->unloadable_sp_size
    = current_offset - obj_som_file_hdr (abfd)->unloadable_sp_location;

  /* Loader fixups are not supported in any way shape or form.  */
  obj_som_file_hdr (abfd)->loader_fixup_location = 0;
  obj_som_file_hdr (abfd)->loader_fixup_total = 0;

  /* Done.  Store the total size of the SOM so far.  */
  obj_som_file_hdr (abfd)->som_length = current_offset;

  return true;
}

/* Finally, scribble out the various headers to the disk.  */

static boolean
som_finish_writing (abfd)
     bfd *abfd;
{
  int num_spaces = som_count_spaces (abfd);
  asymbol **syms = bfd_get_outsymbols (abfd);
  int i, num_syms, strings_size;
  int subspace_index = 0;
  file_ptr location;
  asection *section;
  unsigned long current_offset;
  unsigned int total_reloc_size;
  bfd_size_type amt;

  /* We must set up the version identifier here as objcopy/strip copy
     private BFD data too late for us to handle this in som_begin_writing.  */
  if (obj_som_exec_data (abfd)
      && obj_som_exec_data (abfd)->version_id)
    obj_som_file_hdr (abfd)->version_id = obj_som_exec_data (abfd)->version_id;
  else
    obj_som_file_hdr (abfd)->version_id = NEW_VERSION_ID;

  /* Next is the symbol table.  These are fixed length records.

     Count the number of symbols to determine how much room is needed
     in the object file for the symbol table.

     The names of the symbols are stored in a separate string table,
     and the index for each symbol name into the string table is computed
     below.  Therefore, it is not possible to write the symbol table
     at this time.

     These used to be output before the subspace contents, but they
     were moved here to work around a stupid bug in the hpux linker
     (fixed in hpux10).  */
  current_offset = obj_som_file_hdr (abfd)->som_length;

  /* Make sure we're on a word boundary.  */
  if (current_offset % 4)
    current_offset += (4 - (current_offset % 4));

  num_syms = bfd_get_symcount (abfd);
  obj_som_file_hdr (abfd)->symbol_location = current_offset;
  obj_som_file_hdr (abfd)->symbol_total = num_syms;
  current_offset += num_syms * sizeof (struct symbol_dictionary_record);

  /* Next are the symbol strings.
     Align them to a word boundary.  */
  if (current_offset % 4)
    current_offset += (4 - (current_offset % 4));
  obj_som_file_hdr (abfd)->symbol_strings_location = current_offset;

  /* Scribble out the symbol strings.  */
  if (! som_write_symbol_strings (abfd, current_offset, syms,
				  num_syms, &strings_size,
				  obj_som_compilation_unit (abfd)))
    return false;

  /* Record total string table size in header and update the
     current offset.  */
  obj_som_file_hdr (abfd)->symbol_strings_size = strings_size;
  current_offset += strings_size;

  /* Do prep work before handling fixups.  */
  som_prep_for_fixups (abfd,
		       bfd_get_outsymbols (abfd),
		       bfd_get_symcount (abfd));

  /* At the end of the file is the fixup stream which starts on a
     word boundary.  */
  if (current_offset % 4)
    current_offset += (4 - (current_offset % 4));
  obj_som_file_hdr (abfd)->fixup_request_location = current_offset;

  /* Write the fixups and update fields in subspace headers which
     relate to the fixup stream.  */
  if (! som_write_fixups (abfd, current_offset, &total_reloc_size))
    return false;

  /* Record the total size of the fixup stream in the file header.  */
  obj_som_file_hdr (abfd)->fixup_request_total = total_reloc_size;

  /* Done.  Store the total size of the SOM.  */
  obj_som_file_hdr (abfd)->som_length = current_offset + total_reloc_size;

  /* Now that the symbol table information is complete, build and
     write the symbol table.  */
  if (! som_build_and_write_symbol_table (abfd))
    return false;

  /* Subspaces are written first so that we can set up information
     about them in their containing spaces as the subspace is written.  */

  /* Seek to the start of the subspace dictionary records.  */
  location = obj_som_file_hdr (abfd)->subspace_location;
  if (bfd_seek (abfd, location, SEEK_SET) != 0)
    return false;

  section = abfd->sections;
  /* Now for each loadable space write out records for its subspaces.  */
  for (i = 0; i < num_spaces; i++)
    {
      asection *subsection;

      /* Find a space.  */
      while (!som_is_space (section))
	section = section->next;

      /* Now look for all its subspaces.  */
      for (subsection = abfd->sections;
	   subsection != NULL;
	   subsection = subsection->next)
	{

	  /* Skip any section which does not correspond to a space
	     or subspace.  Or does not have SEC_ALLOC set (and therefore
	     has no real bits on the disk).  */
	  if (!som_is_subspace (subsection)
	      || !som_is_container (section, subsection)
	      || (subsection->flags & SEC_ALLOC) == 0)
	    continue;

	  /* If this is the first subspace for this space, then save
	     the index of the subspace in its containing space.  Also
	     set "is_loadable" in the containing space.  */

	  if (som_section_data (section)->space_dict->subspace_quantity == 0)
	    {
	      som_section_data (section)->space_dict->is_loadable = 1;
	      som_section_data (section)->space_dict->subspace_index
		= subspace_index;
	    }

	  /* Increment the number of subspaces seen and the number of
	     subspaces contained within the current space.  */
	  subspace_index++;
	  som_section_data (section)->space_dict->subspace_quantity++;

	  /* Mark the index of the current space within the subspace's
	     dictionary record.  */
	  som_section_data (subsection)->subspace_dict->space_index = i;

	  /* Dump the current subspace header.  */
	  amt = sizeof (struct subspace_dictionary_record);
	  if (bfd_bwrite ((PTR) som_section_data (subsection)->subspace_dict,
			 amt, abfd) != amt)
	    return false;
	}
      /* Goto the next section.  */
      section = section->next;
    }

  /* Now repeat the process for unloadable subspaces.  */
  section = abfd->sections;
  /* Now for each space write out records for its subspaces.  */
  for (i = 0; i < num_spaces; i++)
    {
      asection *subsection;

      /* Find a space.  */
      while (!som_is_space (section))
	section = section->next;

      /* Now look for all its subspaces.  */
      for (subsection = abfd->sections;
	   subsection != NULL;
	   subsection = subsection->next)
	{

	  /* Skip any section which does not correspond to a space or
	     subspace, or which SEC_ALLOC set (and therefore handled
	     in the loadable spaces/subspaces code above).  */

	  if (!som_is_subspace (subsection)
	      || !som_is_container (section, subsection)
	      || (subsection->flags & SEC_ALLOC) != 0)
	    continue;

	  /* If this is the first subspace for this space, then save
	     the index of the subspace in its containing space.  Clear
	     "is_loadable".  */

	  if (som_section_data (section)->space_dict->subspace_quantity == 0)
	    {
	      som_section_data (section)->space_dict->is_loadable = 0;
	      som_section_data (section)->space_dict->subspace_index
		= subspace_index;
	    }

	  /* Increment the number of subspaces seen and the number of
	     subspaces contained within the current space.  */
	  som_section_data (section)->space_dict->subspace_quantity++;
	  subspace_index++;

	  /* Mark the index of the current space within the subspace's
	     dictionary record.  */
	  som_section_data (subsection)->subspace_dict->space_index = i;

	  /* Dump this subspace header.  */
	  amt = sizeof (struct subspace_dictionary_record);
	  if (bfd_bwrite ((PTR) som_section_data (subsection)->subspace_dict,
			 amt, abfd) != amt)
	    return false;
	}
      /* Goto the next section.  */
      section = section->next;
    }

  /* All the subspace dictiondary records are written, and all the
     fields are set up in the space dictionary records.

     Seek to the right location and start writing the space
     dictionary records.  */
  location = obj_som_file_hdr (abfd)->space_location;
  if (bfd_seek (abfd, location, SEEK_SET) != 0)
    return false;

  section = abfd->sections;
  for (i = 0; i < num_spaces; i++)
    {
      /* Find a space.  */
      while (!som_is_space (section))
	section = section->next;

      /* Dump its header.  */
      amt = sizeof (struct space_dictionary_record);
      if (bfd_bwrite ((PTR) som_section_data (section)->space_dict,
		     amt, abfd) != amt)
	return false;

      /* Goto the next section.  */
      section = section->next;
    }

  /* Write the compilation unit record if there is one.  */
  if (obj_som_compilation_unit (abfd))
    {
      location = obj_som_file_hdr (abfd)->compiler_location;
      if (bfd_seek (abfd, location, SEEK_SET) != 0)
	return false;

      amt = COMPUNITSZ;
      if (bfd_bwrite ((PTR) obj_som_compilation_unit (abfd), amt, abfd) != amt)
	return false;
    }

  /* Setting of the system_id has to happen very late now that copying of
     BFD private data happens *after* section contents are set.  */
  if (abfd->flags & (EXEC_P | DYNAMIC))
    obj_som_file_hdr (abfd)->system_id = obj_som_exec_data (abfd)->system_id;
  else if (bfd_get_mach (abfd) == pa20)
    obj_som_file_hdr (abfd)->system_id = CPU_PA_RISC2_0;
  else if (bfd_get_mach (abfd) == pa11)
    obj_som_file_hdr (abfd)->system_id = CPU_PA_RISC1_1;
  else
    obj_som_file_hdr (abfd)->system_id = CPU_PA_RISC1_0;

  /* Compute the checksum for the file header just before writing
     the header to disk.  */
  obj_som_file_hdr (abfd)->checksum = som_compute_checksum (abfd);

  /* Only thing left to do is write out the file header.  It is always
     at location zero.  Seek there and write it.  */
  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    return false;
  amt = sizeof (struct header);
  if (bfd_bwrite ((PTR) obj_som_file_hdr (abfd), amt, abfd) != amt)
    return false;

  /* Now write the exec header.  */
  if (abfd->flags & (EXEC_P | DYNAMIC))
    {
      long tmp, som_length;
      struct som_exec_auxhdr *exec_header;

      exec_header = obj_som_exec_hdr (abfd);
      exec_header->exec_entry = bfd_get_start_address (abfd);
      exec_header->exec_flags = obj_som_exec_data (abfd)->exec_flags;

      /* Oh joys.  Ram some of the BSS data into the DATA section
	 to be compatable with how the hp linker makes objects
	 (saves memory space).  */
      tmp = exec_header->exec_dsize;
      tmp = SOM_ALIGN (tmp, PA_PAGESIZE);
      exec_header->exec_bsize -= (tmp - exec_header->exec_dsize);
      if (exec_header->exec_bsize < 0)
	exec_header->exec_bsize = 0;
      exec_header->exec_dsize = tmp;

      /* Now perform some sanity checks.  The idea is to catch bogons now and
	 inform the user, instead of silently generating a bogus file.  */
      som_length = obj_som_file_hdr (abfd)->som_length;
      if (exec_header->exec_tfile + exec_header->exec_tsize > som_length
	  || exec_header->exec_dfile + exec_header->exec_dsize > som_length)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      if (bfd_seek (abfd, obj_som_file_hdr (abfd)->aux_header_location,
		    SEEK_SET) != 0)
	return false;

      amt = AUX_HDR_SIZE;
      if (bfd_bwrite ((PTR) exec_header, amt, abfd) != amt)
	return false;
    }
  return true;
}

/* Compute and return the checksum for a SOM file header.  */

static unsigned long
som_compute_checksum (abfd)
     bfd *abfd;
{
  unsigned long checksum, count, i;
  unsigned long *buffer = (unsigned long *) obj_som_file_hdr (abfd);

  checksum = 0;
  count = sizeof (struct header) / sizeof (unsigned long);
  for (i = 0; i < count; i++)
    checksum ^= *(buffer + i);

  return checksum;
}

static void
som_bfd_derive_misc_symbol_info (abfd, sym, info)
     bfd *abfd ATTRIBUTE_UNUSED;
     asymbol *sym;
     struct som_misc_symbol_info *info;
{
  /* Initialize.  */
  memset (info, 0, sizeof (struct som_misc_symbol_info));

  /* The HP SOM linker requires detailed type information about
     all symbols (including undefined symbols!).  Unfortunately,
     the type specified in an import/export statement does not
     always match what the linker wants.  Severe braindamage.  */

  /* Section symbols will not have a SOM symbol type assigned to
     them yet.  Assign all section symbols type ST_DATA.  */
  if (sym->flags & BSF_SECTION_SYM)
    info->symbol_type = ST_DATA;
  else
    {
      /* Common symbols must have scope SS_UNSAT and type
	 ST_STORAGE or the linker will choke.  */
      if (bfd_is_com_section (sym->section))
	{
	  info->symbol_scope = SS_UNSAT;
	  info->symbol_type = ST_STORAGE;
	}

      /* It is possible to have a symbol without an associated
	 type.  This happens if the user imported the symbol
	 without a type and the symbol was never defined
	 locally.  If BSF_FUNCTION is set for this symbol, then
	 assign it type ST_CODE (the HP linker requires undefined
	 external functions to have type ST_CODE rather than ST_ENTRY).  */
      else if ((som_symbol_data (sym)->som_type == SYMBOL_TYPE_UNKNOWN
		|| som_symbol_data (sym)->som_type == SYMBOL_TYPE_CODE)
	       && bfd_is_und_section (sym->section)
	       && sym->flags & BSF_FUNCTION)
	info->symbol_type = ST_CODE;

      /* Handle function symbols which were defined in this file.
	 They should have type ST_ENTRY.  Also retrieve the argument
	 relocation bits from the SOM backend information.  */
      else if (som_symbol_data (sym)->som_type == SYMBOL_TYPE_ENTRY
	       || (som_symbol_data (sym)->som_type == SYMBOL_TYPE_CODE
		   && (sym->flags & BSF_FUNCTION))
	       || (som_symbol_data (sym)->som_type == SYMBOL_TYPE_UNKNOWN
		   && (sym->flags & BSF_FUNCTION)))
	{
	  info->symbol_type = ST_ENTRY;
	  info->arg_reloc = som_symbol_data (sym)->tc_data.ap.hppa_arg_reloc;
	  info->priv_level= som_symbol_data (sym)->tc_data.ap.hppa_priv_level;
	}

      /* For unknown symbols set the symbol's type based on the symbol's
	 section (ST_DATA for DATA sections, ST_CODE for CODE sections).  */
      else if (som_symbol_data (sym)->som_type == SYMBOL_TYPE_UNKNOWN)
	{
	  if (sym->section->flags & SEC_CODE)
	    info->symbol_type = ST_CODE;
	  else
	    info->symbol_type = ST_DATA;
	}

      else if (som_symbol_data (sym)->som_type == SYMBOL_TYPE_UNKNOWN)
	info->symbol_type = ST_DATA;

      /* From now on it's a very simple mapping.  */
      else if (som_symbol_data (sym)->som_type == SYMBOL_TYPE_ABSOLUTE)
	info->symbol_type = ST_ABSOLUTE;
      else if (som_symbol_data (sym)->som_type == SYMBOL_TYPE_CODE)
	info->symbol_type = ST_CODE;
      else if (som_symbol_data (sym)->som_type == SYMBOL_TYPE_DATA)
	info->symbol_type = ST_DATA;
      else if (som_symbol_data (sym)->som_type == SYMBOL_TYPE_MILLICODE)
	info->symbol_type = ST_MILLICODE;
      else if (som_symbol_data (sym)->som_type == SYMBOL_TYPE_PLABEL)
	info->symbol_type = ST_PLABEL;
      else if (som_symbol_data (sym)->som_type == SYMBOL_TYPE_PRI_PROG)
	info->symbol_type = ST_PRI_PROG;
      else if (som_symbol_data (sym)->som_type == SYMBOL_TYPE_SEC_PROG)
	info->symbol_type = ST_SEC_PROG;
    }

  /* Now handle the symbol's scope.  Exported data which is not
     in the common section has scope SS_UNIVERSAL.  Note scope
     of common symbols was handled earlier!  */
  if (bfd_is_und_section (sym->section))
    info->symbol_scope = SS_UNSAT;
  else if (sym->flags & (BSF_EXPORT | BSF_WEAK)
	   && ! bfd_is_com_section (sym->section))
    info->symbol_scope = SS_UNIVERSAL;
  /* Anything else which is not in the common section has scope
     SS_LOCAL.  */
  else if (! bfd_is_com_section (sym->section))
    info->symbol_scope = SS_LOCAL;

  /* Now set the symbol_info field.  It has no real meaning
     for undefined or common symbols, but the HP linker will
     choke if it's not set to some "reasonable" value.  We
     use zero as a reasonable value.  */
  if (bfd_is_com_section (sym->section)
      || bfd_is_und_section (sym->section)
      || bfd_is_abs_section (sym->section))
    info->symbol_info = 0;
  /* For all other symbols, the symbol_info field contains the
     subspace index of the space this symbol is contained in.  */
  else
    info->symbol_info = sym->section->target_index;

  /* Set the symbol's value.  */
  info->symbol_value = sym->value + sym->section->vma;

  /* The secondary_def field is for weak symbols.  */
  if (sym->flags & BSF_WEAK)
    info->secondary_def = true;
  else
    info->secondary_def = false;

}

/* Build and write, in one big chunk, the entire symbol table for
   this BFD.  */

static boolean
som_build_and_write_symbol_table (abfd)
     bfd *abfd;
{
  unsigned int num_syms = bfd_get_symcount (abfd);
  file_ptr symtab_location = obj_som_file_hdr (abfd)->symbol_location;
  asymbol **bfd_syms = obj_som_sorted_syms (abfd);
  struct symbol_dictionary_record *som_symtab = NULL;
  unsigned int i;
  bfd_size_type symtab_size;

  /* Compute total symbol table size and allocate a chunk of memory
     to hold the symbol table as we build it.  */
  symtab_size = num_syms;
  symtab_size *= sizeof (struct symbol_dictionary_record);
  som_symtab = (struct symbol_dictionary_record *) bfd_zmalloc (symtab_size);
  if (som_symtab == NULL && symtab_size != 0)
    goto error_return;

  /* Walk over each symbol.  */
  for (i = 0; i < num_syms; i++)
    {
      struct som_misc_symbol_info info;

      /* This is really an index into the symbol strings table.
	 By the time we get here, the index has already been
	 computed and stored into the name field in the BFD symbol.  */
      som_symtab[i].name.n_strx = som_symbol_data(bfd_syms[i])->stringtab_offset;

      /* Derive SOM information from the BFD symbol.  */
      som_bfd_derive_misc_symbol_info (abfd, bfd_syms[i], &info);

      /* Now use it.  */
      som_symtab[i].symbol_type = info.symbol_type;
      som_symtab[i].symbol_scope = info.symbol_scope;
      som_symtab[i].arg_reloc = info.arg_reloc;
      som_symtab[i].symbol_info = info.symbol_info;
      som_symtab[i].xleast = 3;
      som_symtab[i].symbol_value = info.symbol_value | info.priv_level;
      som_symtab[i].secondary_def = info.secondary_def;
    }

  /* Everything is ready, seek to the right location and
     scribble out the symbol table.  */
  if (bfd_seek (abfd, symtab_location, SEEK_SET) != 0)
    return false;

  if (bfd_bwrite ((PTR) som_symtab, symtab_size, abfd) != symtab_size)
    goto error_return;

  if (som_symtab != NULL)
    free (som_symtab);
  return true;
 error_return:
  if (som_symtab != NULL)
    free (som_symtab);
  return false;
}

/* Write an object in SOM format.  */

static boolean
som_write_object_contents (abfd)
     bfd *abfd;
{
  if (! abfd->output_has_begun)
    {
      /* Set up fixed parts of the file, space, and subspace headers.
	 Notify the world that output has begun.  */
      som_prep_headers (abfd);
      abfd->output_has_begun = true;
      /* Start writing the object file.  This include all the string
	 tables, fixup streams, and other portions of the object file.  */
      som_begin_writing (abfd);
    }

  return (som_finish_writing (abfd));
}

/* Read and save the string table associated with the given BFD.  */

static boolean
som_slurp_string_table (abfd)
     bfd *abfd;
{
  char *stringtab;
  bfd_size_type amt;

  /* Use the saved version if its available.  */
  if (obj_som_stringtab (abfd) != NULL)
    return true;

  /* I don't think this can currently happen, and I'm not sure it should
     really be an error, but it's better than getting unpredictable results
     from the host's malloc when passed a size of zero.  */
  if (obj_som_stringtab_size (abfd) == 0)
    {
      bfd_set_error (bfd_error_no_symbols);
      return false;
    }

  /* Allocate and read in the string table.  */
  amt = obj_som_stringtab_size (abfd);
  stringtab = bfd_zmalloc (amt);
  if (stringtab == NULL)
    return false;

  if (bfd_seek (abfd, obj_som_str_filepos (abfd), SEEK_SET) != 0)
    return false;

  if (bfd_bread (stringtab, amt, abfd) != amt)
    return false;

  /* Save our results and return success.  */
  obj_som_stringtab (abfd) = stringtab;
  return true;
}

/* Return the amount of data (in bytes) required to hold the symbol
   table for this object.  */

static long
som_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  if (!som_slurp_symbol_table (abfd))
    return -1;

  return (bfd_get_symcount (abfd) + 1) * (sizeof (asymbol *));
}

/* Convert from a SOM subspace index to a BFD section.  */

static asection *
bfd_section_from_som_symbol (abfd, symbol)
     bfd *abfd;
     struct symbol_dictionary_record *symbol;
{
  asection *section;

  /* The meaning of the symbol_info field changes for functions
     within executables.  So only use the quick symbol_info mapping for
     incomplete objects and non-function symbols in executables.  */
  if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0
      || (symbol->symbol_type != ST_ENTRY
	  && symbol->symbol_type != ST_PRI_PROG
	  && symbol->symbol_type != ST_SEC_PROG
	  && symbol->symbol_type != ST_MILLICODE))
    {
      unsigned int index = symbol->symbol_info;
      for (section = abfd->sections; section != NULL; section = section->next)
	if (section->target_index == index && som_is_subspace (section))
	  return section;

      /* Could be a symbol from an external library (such as an OMOS
	 shared library).  Don't abort.  */
      return bfd_abs_section_ptr;

    }
  else
    {
      unsigned int value = symbol->symbol_value;

      /* For executables we will have to use the symbol's address and
	 find out what section would contain that address.   Yuk.  */
      for (section = abfd->sections; section; section = section->next)
	{
	  if (value >= section->vma
	      && value <= section->vma + section->_cooked_size
	      && som_is_subspace (section))
	    return section;
	}

      /* Could be a symbol from an external library (such as an OMOS
	 shared library).  Don't abort.  */
      return bfd_abs_section_ptr;

    }
}

/* Read and save the symbol table associated with the given BFD.  */

static unsigned int
som_slurp_symbol_table (abfd)
     bfd *abfd;
{
  int symbol_count = bfd_get_symcount (abfd);
  int symsize = sizeof (struct symbol_dictionary_record);
  char *stringtab;
  struct symbol_dictionary_record *buf = NULL, *bufp, *endbufp;
  som_symbol_type *sym, *symbase;
  bfd_size_type amt;

  /* Return saved value if it exists.  */
  if (obj_som_symtab (abfd) != NULL)
    goto successful_return;

  /* Special case.  This is *not* an error.  */
  if (symbol_count == 0)
    goto successful_return;

  if (!som_slurp_string_table (abfd))
    goto error_return;

  stringtab = obj_som_stringtab (abfd);

  amt = symbol_count;
  amt *= sizeof (som_symbol_type);
  symbase = (som_symbol_type *) bfd_zmalloc (amt);
  if (symbase == NULL)
    goto error_return;

  /* Read in the external SOM representation.  */
  amt = symbol_count;
  amt *= symsize;
  buf = bfd_malloc (amt);
  if (buf == NULL && amt != 0)
    goto error_return;
  if (bfd_seek (abfd, obj_som_sym_filepos (abfd), SEEK_SET) != 0)
    goto error_return;
  if (bfd_bread (buf, amt, abfd) != amt)
    goto error_return;

  /* Iterate over all the symbols and internalize them.  */
  endbufp = buf + symbol_count;
  for (bufp = buf, sym = symbase; bufp < endbufp; ++bufp)
    {

      /* I don't think we care about these.  */
      if (bufp->symbol_type == ST_SYM_EXT
	  || bufp->symbol_type == ST_ARG_EXT)
	continue;

      /* Set some private data we care about.  */
      if (bufp->symbol_type == ST_NULL)
	som_symbol_data (sym)->som_type = SYMBOL_TYPE_UNKNOWN;
      else if (bufp->symbol_type == ST_ABSOLUTE)
	som_symbol_data (sym)->som_type = SYMBOL_TYPE_ABSOLUTE;
      else if (bufp->symbol_type == ST_DATA)
	som_symbol_data (sym)->som_type = SYMBOL_TYPE_DATA;
      else if (bufp->symbol_type == ST_CODE)
	som_symbol_data (sym)->som_type = SYMBOL_TYPE_CODE;
      else if (bufp->symbol_type == ST_PRI_PROG)
	som_symbol_data (sym)->som_type = SYMBOL_TYPE_PRI_PROG;
      else if (bufp->symbol_type == ST_SEC_PROG)
	som_symbol_data (sym)->som_type = SYMBOL_TYPE_SEC_PROG;
      else if (bufp->symbol_type == ST_ENTRY)
	som_symbol_data (sym)->som_type = SYMBOL_TYPE_ENTRY;
      else if (bufp->symbol_type == ST_MILLICODE)
	som_symbol_data (sym)->som_type = SYMBOL_TYPE_MILLICODE;
      else if (bufp->symbol_type == ST_PLABEL)
	som_symbol_data (sym)->som_type = SYMBOL_TYPE_PLABEL;
      else
	som_symbol_data (sym)->som_type = SYMBOL_TYPE_UNKNOWN;
      som_symbol_data (sym)->tc_data.ap.hppa_arg_reloc = bufp->arg_reloc;

      /* Some reasonable defaults.  */
      sym->symbol.the_bfd = abfd;
      sym->symbol.name = bufp->name.n_strx + stringtab;
      sym->symbol.value = bufp->symbol_value;
      sym->symbol.section = 0;
      sym->symbol.flags = 0;

      switch (bufp->symbol_type)
	{
	case ST_ENTRY:
	case ST_MILLICODE:
	  sym->symbol.flags |= BSF_FUNCTION;
	  som_symbol_data (sym)->tc_data.ap.hppa_priv_level =
	    sym->symbol.value & 0x3;
	  sym->symbol.value &= ~0x3;
	  break;

	case ST_STUB:
	case ST_CODE:
	case ST_PRI_PROG:
	case ST_SEC_PROG:
	  som_symbol_data (sym)->tc_data.ap.hppa_priv_level =
	    sym->symbol.value & 0x3;
	  sym->symbol.value &= ~0x3;
	  /* If the symbol's scope is SS_UNSAT, then these are
	     undefined function symbols.  */
	  if (bufp->symbol_scope == SS_UNSAT)
	    sym->symbol.flags |= BSF_FUNCTION;

	default:
	  break;
	}

      /* Handle scoping and section information.  */
      switch (bufp->symbol_scope)
	{
	/* symbol_info field is undefined for SS_EXTERNAL and SS_UNSAT symbols,
	   so the section associated with this symbol can't be known.  */
	case SS_EXTERNAL:
	  if (bufp->symbol_type != ST_STORAGE)
	    sym->symbol.section = bfd_und_section_ptr;
	  else
	    sym->symbol.section = bfd_com_section_ptr;
	  sym->symbol.flags |= (BSF_EXPORT | BSF_GLOBAL);
	  break;

	case SS_UNSAT:
	  if (bufp->symbol_type != ST_STORAGE)
	    sym->symbol.section = bfd_und_section_ptr;
	  else
	    sym->symbol.section = bfd_com_section_ptr;
	  break;

	case SS_UNIVERSAL:
	  sym->symbol.flags |= (BSF_EXPORT | BSF_GLOBAL);
	  sym->symbol.section = bfd_section_from_som_symbol (abfd, bufp);
	  sym->symbol.value -= sym->symbol.section->vma;
	  break;

#if 0
	/* SS_GLOBAL and SS_LOCAL are two names for the same thing.
	   Sound dumb?  It is.  */
	case SS_GLOBAL:
#endif
	case SS_LOCAL:
	  sym->symbol.flags |= BSF_LOCAL;
	  sym->symbol.section = bfd_section_from_som_symbol (abfd, bufp);
	  sym->symbol.value -= sym->symbol.section->vma;
	  break;
	}

      /* Check for a weak symbol.  */
      if (bufp->secondary_def)
	sym->symbol.flags |= BSF_WEAK;

      /* Mark section symbols and symbols used by the debugger.
	 Note $START$ is a magic code symbol, NOT a section symbol.  */
      if (sym->symbol.name[0] == '$'
	  && sym->symbol.name[strlen (sym->symbol.name) - 1] == '$'
	  && !strcmp (sym->symbol.name, sym->symbol.section->name))
	sym->symbol.flags |= BSF_SECTION_SYM;
      else if (!strncmp (sym->symbol.name, "L$0\002", 4))
	{
	  sym->symbol.flags |= BSF_SECTION_SYM;
	  sym->symbol.name = sym->symbol.section->name;
	}
      else if (!strncmp (sym->symbol.name, "L$0\001", 4))
	sym->symbol.flags |= BSF_DEBUGGING;

      /* Note increment at bottom of loop, since we skip some symbols
         we can not include it as part of the for statement.  */
      sym++;
    }

  /* We modify the symbol count to record the number of BFD symbols we
     created.  */
  bfd_get_symcount (abfd) = sym - symbase;

  /* Save our results and return success.  */
  obj_som_symtab (abfd) = symbase;
 successful_return:
  if (buf != NULL)
    free (buf);
  return (true);

 error_return:
  if (buf != NULL)
    free (buf);
  return false;
}

/* Canonicalize a SOM symbol table.  Return the number of entries
   in the symbol table.  */

static long
som_get_symtab (abfd, location)
     bfd *abfd;
     asymbol **location;
{
  int i;
  som_symbol_type *symbase;

  if (!som_slurp_symbol_table (abfd))
    return -1;

  i = bfd_get_symcount (abfd);
  symbase = obj_som_symtab (abfd);

  for (; i > 0; i--, location++, symbase++)
    *location = &symbase->symbol;

  /* Final null pointer.  */
  *location = 0;
  return (bfd_get_symcount (abfd));
}

/* Make a SOM symbol.  There is nothing special to do here.  */

static asymbol *
som_make_empty_symbol (abfd)
     bfd *abfd;
{
  bfd_size_type amt = sizeof (som_symbol_type);
  som_symbol_type *new = (som_symbol_type *) bfd_zalloc (abfd, amt);
  if (new == NULL)
    return 0;
  new->symbol.the_bfd = abfd;

  return &new->symbol;
}

/* Print symbol information.  */

static void
som_print_symbol (ignore_abfd, afile, symbol, how, ignored)
     bfd *abfd;
     PTR afile;
     asymbol *symbol;
     bfd_print_symbol_type how;
     asymbol *ignored ATTRIBUTE_UNUSED;
{
  FILE *file = (FILE *) afile;
  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;
    case bfd_print_symbol_more:
      fprintf (file, "som ");
      fprintf_vma (file, symbol->value);
      fprintf (file, " %lx", (long) symbol->flags);
      break;
    case bfd_print_symbol_all:
      {
	const char *section_name;
	section_name = symbol->section ? symbol->section->name : "(*none*)";
	bfd_print_symbol_vandf (abfd, (PTR) file, symbol);
	fprintf (file, " %s\t%s", section_name, symbol->name);
	break;
      }
    }
}

static boolean
som_bfd_is_local_label_name (abfd, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *name;
{
  return (name[0] == 'L' && name[1] == '$');
}

/* Count or process variable-length SOM fixup records.

   To avoid code duplication we use this code both to compute the number
   of relocations requested by a stream, and to internalize the stream.

   When computing the number of relocations requested by a stream the
   variables rptr, section, and symbols have no meaning.

   Return the number of relocations requested by the fixup stream.  When
   not just counting

   This needs at least two or three more passes to get it cleaned up.  */

static unsigned int
som_set_reloc_info (fixup, end, internal_relocs, section, symbols, just_count)
     unsigned char *fixup;
     unsigned int end;
     arelent *internal_relocs;
     asection *section;
     asymbol **symbols;
     boolean just_count;
{
  unsigned int op, varname, deallocate_contents = 0;
  unsigned char *end_fixups = &fixup[end];
  const struct fixup_format *fp;
  const char *cp;
  unsigned char *save_fixup;
  int variables[26], stack[20], c, v, count, prev_fixup, *sp, saved_unwind_bits;
  const int *subop;
  arelent *rptr = internal_relocs;
  unsigned int offset = 0;

#define	var(c)		variables[(c) - 'A']
#define	push(v)		(*sp++ = (v))
#define	pop()		(*--sp)
#define	emptystack()	(sp == stack)

  som_initialize_reloc_queue (reloc_queue);
  memset (variables, 0, sizeof (variables));
  memset (stack, 0, sizeof (stack));
  count = 0;
  prev_fixup = 0;
  saved_unwind_bits = 0;
  sp = stack;

  while (fixup < end_fixups)
    {

      /* Save pointer to the start of this fixup.  We'll use
	 it later to determine if it is necessary to put this fixup
	 on the queue.  */
      save_fixup = fixup;

      /* Get the fixup code and its associated format.  */
      op = *fixup++;
      fp = &som_fixup_formats[op];

      /* Handle a request for a previous fixup.  */
      if (*fp->format == 'P')
	{
	  /* Get pointer to the beginning of the prev fixup, move
	     the repeated fixup to the head of the queue.  */
	  fixup = reloc_queue[fp->D].reloc;
	  som_reloc_queue_fix (reloc_queue, fp->D);
	  prev_fixup = 1;

	  /* Get the fixup code and its associated format.  */
	  op = *fixup++;
	  fp = &som_fixup_formats[op];
	}

      /* If this fixup will be passed to BFD, set some reasonable defaults.  */
      if (! just_count
	  && som_hppa_howto_table[op].type != R_NO_RELOCATION
	  && som_hppa_howto_table[op].type != R_DATA_OVERRIDE)
	{
	  rptr->address = offset;
	  rptr->howto = &som_hppa_howto_table[op];
	  rptr->addend = 0;
	  rptr->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
	}

      /* Set default input length to 0.  Get the opcode class index
	 into D.  */
      var ('L') = 0;
      var ('D') = fp->D;
      var ('U') = saved_unwind_bits;

      /* Get the opcode format.  */
      cp = fp->format;

      /* Process the format string.  Parsing happens in two phases,
	 parse RHS, then assign to LHS.  Repeat until no more
	 characters in the format string.  */
      while (*cp)
	{
	  /* The variable this pass is going to compute a value for.  */
	  varname = *cp++;

	  /* Start processing RHS.  Continue until a NULL or '=' is found.  */
	  do
	    {
	      c = *cp++;

	      /* If this is a variable, push it on the stack.  */
	      if (ISUPPER (c))
		push (var (c));

	      /* If this is a lower case letter, then it represents
		 additional data from the fixup stream to be pushed onto
		 the stack.  */
	      else if (ISLOWER (c))
		{
		  int bits = (c - 'a') * 8;
		  for (v = 0; c > 'a'; --c)
		    v = (v << 8) | *fixup++;
		  if (varname == 'V')
		    v = sign_extend (v, bits);
		  push (v);
		}

	      /* A decimal constant.  Push it on the stack.  */
	      else if (ISDIGIT (c))
		{
		  v = c - '0';
		  while (ISDIGIT (*cp))
		    v = (v * 10) + (*cp++ - '0');
		  push (v);
		}
	      else
		/* An operator.  Pop two two values from the stack and
		   use them as operands to the given operation.  Push
		   the result of the operation back on the stack.  */
		switch (c)
		  {
		  case '+':
		    v = pop ();
		    v += pop ();
		    push (v);
		    break;
		  case '*':
		    v = pop ();
		    v *= pop ();
		    push (v);
		    break;
		  case '<':
		    v = pop ();
		    v = pop () << v;
		    push (v);
		    break;
		  default:
		    abort ();
		  }
	    }
	  while (*cp && *cp != '=');

	  /* Move over the equal operator.  */
	  cp++;

	  /* Pop the RHS off the stack.  */
	  c = pop ();

	  /* Perform the assignment.  */
	  var (varname) = c;

	  /* Handle side effects. and special 'O' stack cases.  */
	  switch (varname)
	    {
	    /* Consume some bytes from the input space.  */
	    case 'L':
	      offset += c;
	      break;
	    /* A symbol to use in the relocation.  Make a note
	       of this if we are not just counting.  */
	    case 'S':
	      if (! just_count)
		rptr->sym_ptr_ptr = &symbols[c];
	      break;
	    /* Argument relocation bits for a function call.  */
	    case 'R':
	      if (! just_count)
		{
		  unsigned int tmp = var ('R');
		  rptr->addend = 0;

		  if ((som_hppa_howto_table[op].type == R_PCREL_CALL
		       && R_PCREL_CALL + 10 > op)
		      || (som_hppa_howto_table[op].type == R_ABS_CALL
			  && R_ABS_CALL + 10 > op))
		    {
		      /* Simple encoding.  */
		      if (tmp > 4)
			{
			  tmp -= 5;
			  rptr->addend |= 1;
			}
		      if (tmp == 4)
			rptr->addend |= 1 << 8 | 1 << 6 | 1 << 4 | 1 << 2;
		      else if (tmp == 3)
			rptr->addend |= 1 << 8 | 1 << 6 | 1 << 4;
		      else if (tmp == 2)
			rptr->addend |= 1 << 8 | 1 << 6;
		      else if (tmp == 1)
			rptr->addend |= 1 << 8;
		    }
		  else
		    {
		      unsigned int tmp1, tmp2;

		      /* First part is easy -- low order two bits are
			 directly copied, then shifted away.  */
		      rptr->addend = tmp & 0x3;
		      tmp >>= 2;

		      /* Diving the result by 10 gives us the second
			 part.  If it is 9, then the first two words
			 are a double precision paramater, else it is
			 3 * the first arg bits + the 2nd arg bits.  */
		      tmp1 = tmp / 10;
		      tmp -= tmp1 * 10;
		      if (tmp1 == 9)
			rptr->addend += (0xe << 6);
		      else
			{
			  /* Get the two pieces.  */
			  tmp2 = tmp1 / 3;
			  tmp1 -= tmp2 * 3;
			  /* Put them in the addend.  */
			  rptr->addend += (tmp2 << 8) + (tmp1 << 6);
			}

		      /* What's left is the third part.  It's unpacked
			 just like the second.  */
		      if (tmp == 9)
			rptr->addend += (0xe << 2);
		      else
			{
			  tmp2 = tmp / 3;
			  tmp -= tmp2 * 3;
			  rptr->addend += (tmp2 << 4) + (tmp << 2);
			}
		    }
		  rptr->addend = HPPA_R_ADDEND (rptr->addend, 0);
		}
	      break;
	    /* Handle the linker expression stack.  */
	    case 'O':
	      switch (op)
		{
		case R_COMP1:
		  subop = comp1_opcodes;
		  break;
		case R_COMP2:
		  subop = comp2_opcodes;
		  break;
		case R_COMP3:
		  subop = comp3_opcodes;
		  break;
		default:
		  abort ();
		}
	      while (*subop <= (unsigned char) c)
		++subop;
	      --subop;
	      break;
	    /* The lower 32unwind bits must be persistent.  */
	    case 'U':
	      saved_unwind_bits = var ('U');
	      break;

	    default:
	      break;
	    }
	}

      /* If we used a previous fixup, clean up after it.  */
      if (prev_fixup)
	{
	  fixup = save_fixup + 1;
	  prev_fixup = 0;
	}
      /* Queue it.  */
      else if (fixup > save_fixup + 1)
	som_reloc_queue_insert (save_fixup, fixup - save_fixup, reloc_queue);

      /* We do not pass R_DATA_OVERRIDE or R_NO_RELOCATION
	 fixups to BFD.  */
      if (som_hppa_howto_table[op].type != R_DATA_OVERRIDE
	  && som_hppa_howto_table[op].type != R_NO_RELOCATION)
	{
	  /* Done with a single reloction. Loop back to the top.  */
	  if (! just_count)
	    {
	      if (som_hppa_howto_table[op].type == R_ENTRY)
		rptr->addend = var ('T');
	      else if (som_hppa_howto_table[op].type == R_EXIT)
		rptr->addend = var ('U');
	      else if (som_hppa_howto_table[op].type == R_PCREL_CALL
		       || som_hppa_howto_table[op].type == R_ABS_CALL)
		;
	      else if (som_hppa_howto_table[op].type == R_DATA_ONE_SYMBOL)
		{
		  /* Try what was specified in R_DATA_OVERRIDE first
		     (if anything).  Then the hard way using the
		     section contents.  */
		  rptr->addend = var ('V');

		  if (rptr->addend == 0 && !section->contents)
		    {
		      /* Got to read the damn contents first.  We don't
		         bother saving the contents (yet).  Add it one
			 day if the need arises.  */
		      section->contents = bfd_malloc (section->_raw_size);
		      if (section->contents == NULL)
			return (unsigned) -1;

		      deallocate_contents = 1;
		      bfd_get_section_contents (section->owner,
						section,
						section->contents,
						(bfd_vma) 0,
						section->_raw_size);
		    }
		  else if (rptr->addend == 0)
		    rptr->addend = bfd_get_32 (section->owner,
					       (section->contents
						+ offset - var ('L')));

		}
	      else
		rptr->addend = var ('V');
	      rptr++;
	    }
	  count++;
	  /* Now that we've handled a "full" relocation, reset
	     some state.  */
	  memset (variables, 0, sizeof (variables));
	  memset (stack, 0, sizeof (stack));
	}
    }
  if (deallocate_contents)
    free (section->contents);

  return count;

#undef var
#undef push
#undef pop
#undef emptystack
}

/* Read in the relocs (aka fixups in SOM terms) for a section.

   som_get_reloc_upper_bound calls this routine with JUST_COUNT
   set to true to indicate it only needs a count of the number
   of actual relocations.  */

static boolean
som_slurp_reloc_table (abfd, section, symbols, just_count)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     boolean just_count;
{
  char *external_relocs;
  unsigned int fixup_stream_size;
  arelent *internal_relocs;
  unsigned int num_relocs;
  bfd_size_type amt;

  fixup_stream_size = som_section_data (section)->reloc_size;
  /* If there were no relocations, then there is nothing to do.  */
  if (section->reloc_count == 0)
    return true;

  /* If reloc_count is -1, then the relocation stream has not been
     parsed.  We must do so now to know how many relocations exist.  */
  if (section->reloc_count == (unsigned) -1)
    {
      amt = fixup_stream_size;
      external_relocs = (char *) bfd_malloc (amt);
      if (external_relocs == (char *) NULL)
	return false;
      /* Read in the external forms.  */
      if (bfd_seek (abfd,
		    obj_som_reloc_filepos (abfd) + section->rel_filepos,
		    SEEK_SET)
	  != 0)
	return false;
      if (bfd_bread (external_relocs, amt, abfd) != amt)
	return false;

      /* Let callers know how many relocations found.
	 also save the relocation stream as we will
	 need it again.  */
      section->reloc_count = som_set_reloc_info (external_relocs,
						 fixup_stream_size,
						 NULL, NULL, NULL, true);

      som_section_data (section)->reloc_stream = external_relocs;
    }

  /* If the caller only wanted a count, then return now.  */
  if (just_count)
    return true;

  num_relocs = section->reloc_count;
  external_relocs = som_section_data (section)->reloc_stream;
  /* Return saved information about the relocations if it is available.  */
  if (section->relocation != (arelent *) NULL)
    return true;

  amt = num_relocs;
  amt *= sizeof (arelent);
  internal_relocs = (arelent *) bfd_zalloc (abfd, (amt));
  if (internal_relocs == (arelent *) NULL)
    return false;

  /* Process and internalize the relocations.  */
  som_set_reloc_info (external_relocs, fixup_stream_size,
		      internal_relocs, section, symbols, false);

  /* We're done with the external relocations.  Free them.  */
  free (external_relocs);
  som_section_data (section)->reloc_stream = NULL;

  /* Save our results and return success.  */
  section->relocation = internal_relocs;
  return (true);
}

/* Return the number of bytes required to store the relocation
   information associated with the given section.  */

static long
som_get_reloc_upper_bound (abfd, asect)
     bfd *abfd;
     sec_ptr asect;
{
  /* If section has relocations, then read in the relocation stream
     and parse it to determine how many relocations exist.  */
  if (asect->flags & SEC_RELOC)
    {
      if (! som_slurp_reloc_table (abfd, asect, NULL, true))
	return -1;
      return (asect->reloc_count + 1) * sizeof (arelent *);
    }
  /* There are no relocations.  */
  return 0;
}

/* Convert relocations from SOM (external) form into BFD internal
   form.  Return the number of relocations.  */

static long
som_canonicalize_reloc (abfd, section, relptr, symbols)
     bfd *abfd;
     sec_ptr section;
     arelent **relptr;
     asymbol **symbols;
{
  arelent *tblptr;
  int count;

  if (! som_slurp_reloc_table (abfd, section, symbols, false))
    return -1;

  count = section->reloc_count;
  tblptr = section->relocation;

  while (count--)
    *relptr++ = tblptr++;

  *relptr = (arelent *) NULL;
  return section->reloc_count;
}

extern const bfd_target som_vec;

/* A hook to set up object file dependent section information.  */

static boolean
som_new_section_hook (abfd, newsect)
     bfd *abfd;
     asection *newsect;
{
  bfd_size_type amt = sizeof (struct som_section_data_struct);
  newsect->used_by_bfd = (PTR) bfd_zalloc (abfd, amt);
  if (!newsect->used_by_bfd)
    return false;
  newsect->alignment_power = 3;

  /* We allow more than three sections internally.  */
  return true;
}

/* Copy any private info we understand from the input symbol
   to the output symbol.  */

static boolean
som_bfd_copy_private_symbol_data (ibfd, isymbol, obfd, osymbol)
     bfd *ibfd;
     asymbol *isymbol;
     bfd *obfd;
     asymbol *osymbol;
{
  struct som_symbol *input_symbol = (struct som_symbol *) isymbol;
  struct som_symbol *output_symbol = (struct som_symbol *) osymbol;

  /* One day we may try to grok other private data.  */
  if (ibfd->xvec->flavour != bfd_target_som_flavour
      || obfd->xvec->flavour != bfd_target_som_flavour)
    return false;

  /* The only private information we need to copy is the argument relocation
     bits.  */
  output_symbol->tc_data.ap.hppa_arg_reloc =
    input_symbol->tc_data.ap.hppa_arg_reloc;

  return true;
}

/* Copy any private info we understand from the input section
   to the output section.  */

static boolean
som_bfd_copy_private_section_data (ibfd, isection, obfd, osection)
     bfd *ibfd;
     asection *isection;
     bfd *obfd;
     asection *osection;
{
  bfd_size_type amt;

  /* One day we may try to grok other private data.  */
  if (ibfd->xvec->flavour != bfd_target_som_flavour
      || obfd->xvec->flavour != bfd_target_som_flavour
      || (!som_is_space (isection) && !som_is_subspace (isection)))
    return true;

  amt = sizeof (struct som_copyable_section_data_struct);
  som_section_data (osection)->copy_data =
    (struct som_copyable_section_data_struct *) bfd_zalloc (obfd, amt);
  if (som_section_data (osection)->copy_data == NULL)
    return false;

  memcpy (som_section_data (osection)->copy_data,
	  som_section_data (isection)->copy_data,
	  sizeof (struct som_copyable_section_data_struct));

  /* Reparent if necessary.  */
  if (som_section_data (osection)->copy_data->container)
    som_section_data (osection)->copy_data->container =
      som_section_data (osection)->copy_data->container->output_section;

  return true;
}

/* Copy any private info we understand from the input bfd
   to the output bfd.  */

static boolean
som_bfd_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd, *obfd;
{
  /* One day we may try to grok other private data.  */
  if (ibfd->xvec->flavour != bfd_target_som_flavour
      || obfd->xvec->flavour != bfd_target_som_flavour)
    return true;

  /* Allocate some memory to hold the data we need.  */
  obj_som_exec_data (obfd) = (struct som_exec_data *)
    bfd_zalloc (obfd, (bfd_size_type) sizeof (struct som_exec_data));
  if (obj_som_exec_data (obfd) == NULL)
    return false;

  /* Now copy the data.  */
  memcpy (obj_som_exec_data (obfd), obj_som_exec_data (ibfd),
	  sizeof (struct som_exec_data));

  return true;
}

/* Set backend info for sections which can not be described
   in the BFD data structures.  */

boolean
bfd_som_set_section_attributes (section, defined, private, sort_key, spnum)
     asection *section;
     int defined;
     int private;
     unsigned int sort_key;
     int spnum;
{
  /* Allocate memory to hold the magic information.  */
  if (som_section_data (section)->copy_data == NULL)
    {
      bfd_size_type amt = sizeof (struct som_copyable_section_data_struct);
      som_section_data (section)->copy_data =
	(struct som_copyable_section_data_struct *) bfd_zalloc (section->owner,
								amt);
      if (som_section_data (section)->copy_data == NULL)
	return false;
    }
  som_section_data (section)->copy_data->sort_key = sort_key;
  som_section_data (section)->copy_data->is_defined = defined;
  som_section_data (section)->copy_data->is_private = private;
  som_section_data (section)->copy_data->container = section;
  som_section_data (section)->copy_data->space_number = spnum;
  return true;
}

/* Set backend info for subsections which can not be described
   in the BFD data structures.  */

boolean
bfd_som_set_subsection_attributes (section, container, access,
				   sort_key, quadrant)
     asection *section;
     asection *container;
     int access;
     unsigned int sort_key;
     int quadrant;
{
  /* Allocate memory to hold the magic information.  */
  if (som_section_data (section)->copy_data == NULL)
    {
      bfd_size_type amt = sizeof (struct som_copyable_section_data_struct);
      som_section_data (section)->copy_data =
	(struct som_copyable_section_data_struct *) bfd_zalloc (section->owner,
								amt);
      if (som_section_data (section)->copy_data == NULL)
	return false;
    }
  som_section_data (section)->copy_data->sort_key = sort_key;
  som_section_data (section)->copy_data->access_control_bits = access;
  som_section_data (section)->copy_data->quadrant = quadrant;
  som_section_data (section)->copy_data->container = container;
  return true;
}

/* Set the full SOM symbol type.  SOM needs far more symbol information
   than any other object file format I'm aware of.  It is mandatory
   to be able to know if a symbol is an entry point, millicode, data,
   code, absolute, storage request, or procedure label.  If you get
   the symbol type wrong your program will not link.  */

void
bfd_som_set_symbol_type (symbol, type)
     asymbol *symbol;
     unsigned int type;
{
  som_symbol_data (symbol)->som_type = type;
}

/* Attach an auxiliary header to the BFD backend so that it may be
   written into the object file.  */

boolean
bfd_som_attach_aux_hdr (abfd, type, string)
     bfd *abfd;
     int type;
     char *string;
{
  bfd_size_type amt;

  if (type == VERSION_AUX_ID)
    {
      size_t len = strlen (string);
      int pad = 0;

      if (len % 4)
	pad = (4 - (len % 4));
      amt = sizeof (struct aux_id) + sizeof (unsigned int) + len + pad;
      obj_som_version_hdr (abfd) =
	(struct user_string_aux_hdr *) bfd_zalloc (abfd, amt);
      if (!obj_som_version_hdr (abfd))
	return false;
      obj_som_version_hdr (abfd)->header_id.type = VERSION_AUX_ID;
      obj_som_version_hdr (abfd)->header_id.length = len + pad;
      obj_som_version_hdr (abfd)->header_id.length += sizeof (int);
      obj_som_version_hdr (abfd)->string_length = len;
      strncpy (obj_som_version_hdr (abfd)->user_string, string, len);
    }
  else if (type == COPYRIGHT_AUX_ID)
    {
      int len = strlen (string);
      int pad = 0;

      if (len % 4)
	pad = (4 - (len % 4));
      amt = sizeof (struct aux_id) + sizeof (unsigned int) + len + pad;
      obj_som_copyright_hdr (abfd) =
	(struct copyright_aux_hdr *) bfd_zalloc (abfd, amt);
      if (!obj_som_copyright_hdr (abfd))
	return false;
      obj_som_copyright_hdr (abfd)->header_id.type = COPYRIGHT_AUX_ID;
      obj_som_copyright_hdr (abfd)->header_id.length = len + pad;
      obj_som_copyright_hdr (abfd)->header_id.length += sizeof (int);
      obj_som_copyright_hdr (abfd)->string_length = len;
      strcpy (obj_som_copyright_hdr (abfd)->copyright, string);
    }
  return true;
}

/* Attach an compilation unit header to the BFD backend so that it may be
   written into the object file.  */

boolean
bfd_som_attach_compilation_unit (abfd, name, language_name, product_id,
				 version_id)
     bfd *abfd;
     const char *name;
     const char *language_name;
     const char *product_id;
     const char *version_id;
{
  COMPUNIT *n = (COMPUNIT *) bfd_zalloc (abfd, (bfd_size_type) COMPUNITSZ);
  if (n == NULL)
    return false;

#define STRDUP(f) \
  if (f != NULL) \
    { \
      n->f.n_name = bfd_alloc (abfd, (bfd_size_type) strlen (f) + 1); \
      if (n->f.n_name == NULL) \
	return false; \
      strcpy (n->f.n_name, f); \
    }

  STRDUP (name);
  STRDUP (language_name);
  STRDUP (product_id);
  STRDUP (version_id);

#undef STRDUP

  obj_som_compilation_unit (abfd) = n;

  return true;
}

static boolean
som_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (count == 0 || ((section->flags & SEC_HAS_CONTENTS) == 0))
    return true;
  if ((bfd_size_type) (offset+count) > section->_raw_size
      || bfd_seek (abfd, (file_ptr) (section->filepos + offset), SEEK_SET) != 0
      || bfd_bread (location, count, abfd) != count)
    return (false); /* on error */
  return (true);
}

static boolean
som_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (! abfd->output_has_begun)
    {
      /* Set up fixed parts of the file, space, and subspace headers.
	 Notify the world that output has begun.  */
      som_prep_headers (abfd);
      abfd->output_has_begun = true;
      /* Start writing the object file.  This include all the string
	 tables, fixup streams, and other portions of the object file.  */
      som_begin_writing (abfd);
    }

  /* Only write subspaces which have "real" contents (eg. the contents
     are not generated at run time by the OS).  */
  if (!som_is_subspace (section)
      || ((section->flags & SEC_HAS_CONTENTS) == 0))
    return true;

  /* Seek to the proper offset within the object file and write the
     data.  */
  offset += som_section_data (section)->subspace_dict->file_loc_init_value;
  if (bfd_seek (abfd, offset, SEEK_SET) != 0)
    return false;

  if (bfd_bwrite ((PTR) location, count, abfd) != count)
    return false;
  return true;
}

static boolean
som_set_arch_mach (abfd, arch, machine)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  /* Allow any architecture to be supported by the SOM backend.  */
  return bfd_default_set_arch_mach (abfd, arch, machine);
}

static boolean
som_find_nearest_line (abfd, section, symbols, offset, filename_ptr,
		       functionname_ptr, line_ptr)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
     asymbol **symbols ATTRIBUTE_UNUSED;
     bfd_vma offset ATTRIBUTE_UNUSED;
     const char **filename_ptr ATTRIBUTE_UNUSED;
     const char **functionname_ptr ATTRIBUTE_UNUSED;
     unsigned int *line_ptr ATTRIBUTE_UNUSED;
{
  return (false);
}

static int
som_sizeof_headers (abfd, reloc)
     bfd *abfd ATTRIBUTE_UNUSED;
     boolean reloc ATTRIBUTE_UNUSED;
{
  (*_bfd_error_handler) (_("som_sizeof_headers unimplemented"));
  fflush (stderr);
  abort ();
  return (0);
}

/* Return the single-character symbol type corresponding to
   SOM section S, or '?' for an unknown SOM section.  */

static char
som_section_type (s)
     const char *s;
{
  const struct section_to_type *t;

  for (t = &stt[0]; t->section; t++)
    if (!strcmp (s, t->section))
      return t->type;
  return '?';
}

static int
som_decode_symclass (symbol)
     asymbol *symbol;
{
  char c;

  if (bfd_is_com_section (symbol->section))
    return 'C';
  if (bfd_is_und_section (symbol->section))
    return 'U';
  if (bfd_is_ind_section (symbol->section))
    return 'I';
  if (symbol->flags & BSF_WEAK)
    return 'W';
  if (!(symbol->flags & (BSF_GLOBAL | BSF_LOCAL)))
    return '?';

  if (bfd_is_abs_section (symbol->section)
      || (som_symbol_data (symbol) != NULL
	  && som_symbol_data (symbol)->som_type == SYMBOL_TYPE_ABSOLUTE))
    c = 'a';
  else if (symbol->section)
    c = som_section_type (symbol->section->name);
  else
    return '?';
  if (symbol->flags & BSF_GLOBAL)
    c = TOUPPER (c);
  return c;
}

/* Return information about SOM symbol SYMBOL in RET.  */

static void
som_get_symbol_info (ignore_abfd, symbol, ret)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     asymbol *symbol;
     symbol_info *ret;
{
  ret->type = som_decode_symclass (symbol);
  if (ret->type != 'U')
    ret->value = symbol->value + symbol->section->vma;
  else
    ret->value = 0;
  ret->name = symbol->name;
}

/* Count the number of symbols in the archive symbol table.  Necessary
   so that we can allocate space for all the carsyms at once.  */

static boolean
som_bfd_count_ar_symbols (abfd, lst_header, count)
     bfd *abfd;
     struct lst_header *lst_header;
     symindex *count;
{
  unsigned int i;
  unsigned int *hash_table = NULL;
  bfd_size_type amt;
  file_ptr lst_filepos = bfd_tell (abfd) - sizeof (struct lst_header);

  amt = lst_header->hash_size;
  amt *= sizeof (unsigned int);
  hash_table = (unsigned int *) bfd_malloc (amt);
  if (hash_table == NULL && lst_header->hash_size != 0)
    goto error_return;

  /* Don't forget to initialize the counter!  */
  *count = 0;

  /* Read in the hash table.  The has table is an array of 32bit file offsets
     which point to the hash chains.  */
  if (bfd_bread ((PTR) hash_table, amt, abfd) != amt)
    goto error_return;

  /* Walk each chain counting the number of symbols found on that particular
     chain.  */
  for (i = 0; i < lst_header->hash_size; i++)
    {
      struct lst_symbol_record lst_symbol;

      /* An empty chain has zero as it's file offset.  */
      if (hash_table[i] == 0)
	continue;

      /* Seek to the first symbol in this hash chain.  */
      if (bfd_seek (abfd, lst_filepos + hash_table[i], SEEK_SET) != 0)
	goto error_return;

      /* Read in this symbol and update the counter.  */
      amt = sizeof (lst_symbol);
      if (bfd_bread ((PTR) &lst_symbol, amt, abfd) != amt)
	goto error_return;

      (*count)++;

      /* Now iterate through the rest of the symbols on this chain.  */
      while (lst_symbol.next_entry)
	{

	  /* Seek to the next symbol.  */
	  if (bfd_seek (abfd, lst_filepos + lst_symbol.next_entry, SEEK_SET)
	      != 0)
	    goto error_return;

	  /* Read the symbol in and update the counter.  */
	  amt = sizeof (lst_symbol);
	  if (bfd_bread ((PTR) &lst_symbol, amt, abfd) != amt)
	    goto error_return;

	  (*count)++;
	}
    }
  if (hash_table != NULL)
    free (hash_table);
  return true;

 error_return:
  if (hash_table != NULL)
    free (hash_table);
  return false;
}

/* Fill in the canonical archive symbols (SYMS) from the archive described
   by ABFD and LST_HEADER.  */

static boolean
som_bfd_fill_in_ar_symbols (abfd, lst_header, syms)
     bfd *abfd;
     struct lst_header *lst_header;
     carsym **syms;
{
  unsigned int i, len;
  carsym *set = syms[0];
  unsigned int *hash_table = NULL;
  struct som_entry *som_dict = NULL;
  bfd_size_type amt;
  file_ptr lst_filepos = bfd_tell (abfd) - sizeof (struct lst_header);

  amt = lst_header->hash_size;
  amt *= sizeof (unsigned int);
  hash_table = (unsigned int *) bfd_malloc (amt);
  if (hash_table == NULL && lst_header->hash_size != 0)
    goto error_return;

  /* Read in the hash table.  The has table is an array of 32bit file offsets
     which point to the hash chains.  */
  if (bfd_bread ((PTR) hash_table, amt, abfd) != amt)
    goto error_return;

  /* Seek to and read in the SOM dictionary.  We will need this to fill
     in the carsym's filepos field.  */
  if (bfd_seek (abfd, lst_filepos + lst_header->dir_loc, SEEK_SET) != 0)
    goto error_return;

  amt = lst_header->module_count;
  amt *= sizeof (struct som_entry);
  som_dict = (struct som_entry *) bfd_malloc (amt);
  if (som_dict == NULL && lst_header->module_count != 0)
    goto error_return;

  if (bfd_bread ((PTR) som_dict, amt, abfd) != amt)
    goto error_return;

  /* Walk each chain filling in the carsyms as we go along.  */
  for (i = 0; i < lst_header->hash_size; i++)
    {
      struct lst_symbol_record lst_symbol;

      /* An empty chain has zero as it's file offset.  */
      if (hash_table[i] == 0)
	continue;

      /* Seek to and read the first symbol on the chain.  */
      if (bfd_seek (abfd, lst_filepos + hash_table[i], SEEK_SET) != 0)
	goto error_return;

      amt = sizeof (lst_symbol);
      if (bfd_bread ((PTR) &lst_symbol, amt, abfd) != amt)
	goto error_return;

      /* Get the name of the symbol, first get the length which is stored
	 as a 32bit integer just before the symbol.

	 One might ask why we don't just read in the entire string table
	 and index into it.  Well, according to the SOM ABI the string
	 index can point *anywhere* in the archive to save space, so just
	 using the string table would not be safe.  */
      if (bfd_seek (abfd, lst_filepos + lst_header->string_loc
			    + lst_symbol.name.n_strx - 4, SEEK_SET) != 0)
	goto error_return;

      if (bfd_bread (&len, (bfd_size_type) 4, abfd) != 4)
	goto error_return;

      /* Allocate space for the name and null terminate it too.  */
      set->name = bfd_zalloc (abfd, (bfd_size_type) len + 1);
      if (!set->name)
	goto error_return;
      if (bfd_bread (set->name, (bfd_size_type) len, abfd) != len)
	goto error_return;

      set->name[len] = 0;

      /* Fill in the file offset.  Note that the "location" field points
	 to the SOM itself, not the ar_hdr in front of it.  */
      set->file_offset = som_dict[lst_symbol.som_index].location
			  - sizeof (struct ar_hdr);

      /* Go to the next symbol.  */
      set++;

      /* Iterate through the rest of the chain.  */
      while (lst_symbol.next_entry)
	{
	  /* Seek to the next symbol and read it in.  */
	  if (bfd_seek (abfd, lst_filepos + lst_symbol.next_entry, SEEK_SET)
	      != 0)
	    goto error_return;

	  amt = sizeof (lst_symbol);
	  if (bfd_bread ((PTR) &lst_symbol, amt, abfd) != amt)
	    goto error_return;

	  /* Seek to the name length & string and read them in.  */
	  if (bfd_seek (abfd, lst_filepos + lst_header->string_loc
				+ lst_symbol.name.n_strx - 4, SEEK_SET) != 0)
	    goto error_return;

	  if (bfd_bread (&len, (bfd_size_type) 4, abfd) != 4)
	    goto error_return;

	  /* Allocate space for the name and null terminate it too.  */
	  set->name = bfd_zalloc (abfd, (bfd_size_type) len + 1);
	  if (!set->name)
	    goto error_return;

	  if (bfd_bread (set->name, (bfd_size_type) len, abfd) != len)
	    goto error_return;
	  set->name[len] = 0;

	  /* Fill in the file offset.  Note that the "location" field points
	     to the SOM itself, not the ar_hdr in front of it.  */
	  set->file_offset = som_dict[lst_symbol.som_index].location
			       - sizeof (struct ar_hdr);

	  /* Go on to the next symbol.  */
	  set++;
	}
    }
  /* If we haven't died by now, then we successfully read the entire
     archive symbol table.  */
  if (hash_table != NULL)
    free (hash_table);
  if (som_dict != NULL)
    free (som_dict);
  return true;

 error_return:
  if (hash_table != NULL)
    free (hash_table);
  if (som_dict != NULL)
    free (som_dict);
  return false;
}

/* Read in the LST from the archive.  */

static boolean
som_slurp_armap (abfd)
     bfd *abfd;
{
  struct lst_header lst_header;
  struct ar_hdr ar_header;
  unsigned int parsed_size;
  struct artdata *ardata = bfd_ardata (abfd);
  char nextname[17];
  bfd_size_type amt = 16;
  int i = bfd_bread ((PTR) nextname, amt, abfd);

  /* Special cases.  */
  if (i == 0)
    return true;
  if (i != 16)
    return false;

  if (bfd_seek (abfd, (file_ptr) -16, SEEK_CUR) != 0)
    return false;

  /* For archives without .o files there is no symbol table.  */
  if (strncmp (nextname, "/               ", 16))
    {
      bfd_has_map (abfd) = false;
      return true;
    }

  /* Read in and sanity check the archive header.  */
  amt = sizeof (struct ar_hdr);
  if (bfd_bread ((PTR) &ar_header, amt, abfd) != amt)
    return false;

  if (strncmp (ar_header.ar_fmag, ARFMAG, 2))
    {
      bfd_set_error (bfd_error_malformed_archive);
      return false;
    }

  /* How big is the archive symbol table entry?  */
  errno = 0;
  parsed_size = strtol (ar_header.ar_size, NULL, 10);
  if (errno != 0)
    {
      bfd_set_error (bfd_error_malformed_archive);
      return false;
    }

  /* Save off the file offset of the first real user data.  */
  ardata->first_file_filepos = bfd_tell (abfd) + parsed_size;

  /* Read in the library symbol table.  We'll make heavy use of this
     in just a minute.  */
  amt = sizeof (struct lst_header);
  if (bfd_bread ((PTR) &lst_header, amt, abfd) != amt)
    return false;

  /* Sanity check.  */
  if (lst_header.a_magic != LIBMAGIC)
    {
      bfd_set_error (bfd_error_malformed_archive);
      return false;
    }

  /* Count the number of symbols in the library symbol table.  */
  if (! som_bfd_count_ar_symbols (abfd, &lst_header, &ardata->symdef_count))
    return false;

  /* Get back to the start of the library symbol table.  */
  if (bfd_seek (abfd, (ardata->first_file_filepos - parsed_size
		       + sizeof (struct lst_header)), SEEK_SET) != 0)
    return false;

  /* Initializae the cache and allocate space for the library symbols.  */
  ardata->cache = 0;
  amt = ardata->symdef_count;
  amt *= sizeof (carsym);
  ardata->symdefs = (carsym *) bfd_alloc (abfd, amt);
  if (!ardata->symdefs)
    return false;

  /* Now fill in the canonical archive symbols.  */
  if (! som_bfd_fill_in_ar_symbols (abfd, &lst_header, &ardata->symdefs))
    return false;

  /* Seek back to the "first" file in the archive.  Note the "first"
     file may be the extended name table.  */
  if (bfd_seek (abfd, ardata->first_file_filepos, SEEK_SET) != 0)
    return false;

  /* Notify the generic archive code that we have a symbol map.  */
  bfd_has_map (abfd) = true;
  return true;
}

/* Begin preparing to write a SOM library symbol table.

   As part of the prep work we need to determine the number of symbols
   and the size of the associated string section.  */

static boolean
som_bfd_prep_for_ar_write (abfd, num_syms, stringsize)
     bfd *abfd;
     unsigned int *num_syms, *stringsize;
{
  bfd *curr_bfd = abfd->archive_head;

  /* Some initialization.  */
  *num_syms = 0;
  *stringsize = 0;

  /* Iterate over each BFD within this archive.  */
  while (curr_bfd != NULL)
    {
      unsigned int curr_count, i;
      som_symbol_type *sym;

      /* Don't bother for non-SOM objects.  */
      if (curr_bfd->format != bfd_object
	  || curr_bfd->xvec->flavour != bfd_target_som_flavour)
	{
	  curr_bfd = curr_bfd->next;
	  continue;
	}

      /* Make sure the symbol table has been read, then snag a pointer
	 to it.  It's a little slimey to grab the symbols via obj_som_symtab,
	 but doing so avoids allocating lots of extra memory.  */
      if (! som_slurp_symbol_table (curr_bfd))
	return false;

      sym = obj_som_symtab (curr_bfd);
      curr_count = bfd_get_symcount (curr_bfd);

      /* Examine each symbol to determine if it belongs in the
	 library symbol table.  */
      for (i = 0; i < curr_count; i++, sym++)
	{
	  struct som_misc_symbol_info info;

	  /* Derive SOM information from the BFD symbol.  */
	  som_bfd_derive_misc_symbol_info (curr_bfd, &sym->symbol, &info);

	  /* Should we include this symbol?  */
	  if (info.symbol_type == ST_NULL
	      || info.symbol_type == ST_SYM_EXT
	      || info.symbol_type == ST_ARG_EXT)
	    continue;

	  /* Only global symbols and unsatisfied commons.  */
	  if (info.symbol_scope != SS_UNIVERSAL
	      && info.symbol_type != ST_STORAGE)
	    continue;

	  /* Do no include undefined symbols.  */
	  if (bfd_is_und_section (sym->symbol.section))
	    continue;

	  /* Bump the various counters, being careful to honor
	     alignment considerations in the string table.  */
	  (*num_syms)++;
	  *stringsize = *stringsize + strlen (sym->symbol.name) + 5;
	  while (*stringsize % 4)
	    (*stringsize)++;
	}

      curr_bfd = curr_bfd->next;
    }
  return true;
}

/* Hash a symbol name based on the hashing algorithm presented in the
   SOM ABI.  */

static unsigned int
som_bfd_ar_symbol_hash (symbol)
     asymbol *symbol;
{
  unsigned int len = strlen (symbol->name);

  /* Names with length 1 are special.  */
  if (len == 1)
    return 0x1000100 | (symbol->name[0] << 16) | symbol->name[0];

  return ((len & 0x7f) << 24) | (symbol->name[1] << 16)
	  | (symbol->name[len - 2] << 8) | symbol->name[len - 1];
}

/* Do the bulk of the work required to write the SOM library
   symbol table.  */

static boolean
som_bfd_ar_write_symbol_stuff (abfd, nsyms, string_size, lst, elength)
     bfd *abfd;
     unsigned int nsyms, string_size;
     struct lst_header lst;
     unsigned elength;
{
  file_ptr lst_filepos;
  char *strings = NULL, *p;
  struct lst_symbol_record *lst_syms = NULL, *curr_lst_sym;
  bfd *curr_bfd;
  unsigned int *hash_table = NULL;
  struct som_entry *som_dict = NULL;
  struct lst_symbol_record **last_hash_entry = NULL;
  unsigned int curr_som_offset, som_index = 0;
  bfd_size_type amt;

  amt = lst.hash_size;
  amt *= sizeof (unsigned int);
  hash_table = (unsigned int *) bfd_zmalloc (amt);
  if (hash_table == NULL && lst.hash_size != 0)
    goto error_return;

  amt = lst.module_count;
  amt *= sizeof (struct som_entry);
  som_dict = (struct som_entry *) bfd_zmalloc (amt);
  if (som_dict == NULL && lst.module_count != 0)
    goto error_return;

  amt = lst.hash_size;
  amt *= sizeof (struct lst_symbol_record *);
  last_hash_entry = ((struct lst_symbol_record **) bfd_zmalloc (amt));
  if (last_hash_entry == NULL && lst.hash_size != 0)
    goto error_return;

  /* Lots of fields are file positions relative to the start
     of the lst record.  So save its location.  */
  lst_filepos = bfd_tell (abfd) - sizeof (struct lst_header);

  /* Symbols have som_index fields, so we have to keep track of the
     index of each SOM in the archive.

     The SOM dictionary has (among other things) the absolute file
     position for the SOM which a particular dictionary entry
     describes.  We have to compute that information as we iterate
     through the SOMs/symbols.  */
  som_index = 0;

  /* We add in the size of the archive header twice as the location
     in the SOM dictionary is the actual offset of the SOM, not the
     archive header before the SOM.  */
  curr_som_offset = 8 + 2 * sizeof (struct ar_hdr) + lst.file_end;

  /* Make room for the archive header and the contents of the
     extended string table.  Note that elength includes the size
     of the archive header for the extended name table!  */
  if (elength)
    curr_som_offset += elength;

  /* Make sure we're properly aligned.  */
  curr_som_offset = (curr_som_offset + 0x1) & ~0x1;

  /* FIXME should be done with buffers just like everything else...  */
  amt = nsyms;
  amt *= sizeof (struct lst_symbol_record);
  lst_syms = bfd_malloc (amt);
  if (lst_syms == NULL && nsyms != 0)
    goto error_return;
  strings = bfd_malloc ((bfd_size_type) string_size);
  if (strings == NULL && string_size != 0)
    goto error_return;

  p = strings;
  curr_lst_sym = lst_syms;

  curr_bfd = abfd->archive_head;
  while (curr_bfd != NULL)
    {
      unsigned int curr_count, i;
      som_symbol_type *sym;

      /* Don't bother for non-SOM objects.  */
      if (curr_bfd->format != bfd_object
	  || curr_bfd->xvec->flavour != bfd_target_som_flavour)
	{
	  curr_bfd = curr_bfd->next;
	  continue;
	}

      /* Make sure the symbol table has been read, then snag a pointer
	 to it.  It's a little slimey to grab the symbols via obj_som_symtab,
	 but doing so avoids allocating lots of extra memory.  */
      if (! som_slurp_symbol_table (curr_bfd))
	goto error_return;

      sym = obj_som_symtab (curr_bfd);
      curr_count = bfd_get_symcount (curr_bfd);

      for (i = 0; i < curr_count; i++, sym++)
	{
	  struct som_misc_symbol_info info;

	  /* Derive SOM information from the BFD symbol.  */
	  som_bfd_derive_misc_symbol_info (curr_bfd, &sym->symbol, &info);

	  /* Should we include this symbol?  */
	  if (info.symbol_type == ST_NULL
	      || info.symbol_type == ST_SYM_EXT
	      || info.symbol_type == ST_ARG_EXT)
	    continue;

	  /* Only global symbols and unsatisfied commons.  */
	  if (info.symbol_scope != SS_UNIVERSAL
	      && info.symbol_type != ST_STORAGE)
	    continue;

	  /* Do no include undefined symbols.  */
	  if (bfd_is_und_section (sym->symbol.section))
	    continue;

	  /* If this is the first symbol from this SOM, then update
	     the SOM dictionary too.  */
	  if (som_dict[som_index].location == 0)
	    {
	      som_dict[som_index].location = curr_som_offset;
	      som_dict[som_index].length = arelt_size (curr_bfd);
	    }

	  /* Fill in the lst symbol record.  */
	  curr_lst_sym->hidden = 0;
	  curr_lst_sym->secondary_def = info.secondary_def;
	  curr_lst_sym->symbol_type = info.symbol_type;
	  curr_lst_sym->symbol_scope = info.symbol_scope;
	  curr_lst_sym->check_level = 0;
	  curr_lst_sym->must_qualify = 0;
	  curr_lst_sym->initially_frozen = 0;
	  curr_lst_sym->memory_resident = 0;
	  curr_lst_sym->is_common = bfd_is_com_section (sym->symbol.section);
	  curr_lst_sym->dup_common = 0;
	  curr_lst_sym->xleast = 3;
	  curr_lst_sym->arg_reloc = info.arg_reloc;
	  curr_lst_sym->name.n_strx = p - strings + 4;
	  curr_lst_sym->qualifier_name.n_strx = 0;
	  curr_lst_sym->symbol_info = info.symbol_info;
	  curr_lst_sym->symbol_value = info.symbol_value | info.priv_level;
	  curr_lst_sym->symbol_descriptor = 0;
	  curr_lst_sym->reserved = 0;
	  curr_lst_sym->som_index = som_index;
	  curr_lst_sym->symbol_key = som_bfd_ar_symbol_hash (&sym->symbol);
	  curr_lst_sym->next_entry = 0;

	  /* Insert into the hash table.  */
	  if (hash_table[curr_lst_sym->symbol_key % lst.hash_size])
	    {
	      struct lst_symbol_record *tmp;

	      /* There is already something at the head of this hash chain,
		 so tack this symbol onto the end of the chain.  */
	      tmp = last_hash_entry[curr_lst_sym->symbol_key % lst.hash_size];
	      tmp->next_entry
		= (curr_lst_sym - lst_syms) * sizeof (struct lst_symbol_record)
		  + lst.hash_size * 4
		  + lst.module_count * sizeof (struct som_entry)
		  + sizeof (struct lst_header);
	    }
	  else
	    {
	      /* First entry in this hash chain.  */
	      hash_table[curr_lst_sym->symbol_key % lst.hash_size]
		= (curr_lst_sym - lst_syms) * sizeof (struct lst_symbol_record)
		  + lst.hash_size * 4
		  + lst.module_count * sizeof (struct som_entry)
		  + sizeof (struct lst_header);
	    }

	  /* Keep track of the last symbol we added to this chain so we can
	     easily update its next_entry pointer.  */
	  last_hash_entry[curr_lst_sym->symbol_key % lst.hash_size]
	    = curr_lst_sym;

	  /* Update the string table.  */
	  bfd_put_32 (abfd, strlen (sym->symbol.name), p);
	  p += 4;
	  strcpy (p, sym->symbol.name);
	  p += strlen (sym->symbol.name) + 1;
	  while ((int) p % 4)
	    {
	      bfd_put_8 (abfd, 0, p);
	      p++;
	    }

	  /* Head to the next symbol.  */
	  curr_lst_sym++;
	}

      /* Keep track of where each SOM will finally reside; then look
	 at the next BFD.  */
      curr_som_offset += arelt_size (curr_bfd) + sizeof (struct ar_hdr);

      /* A particular object in the archive may have an odd length; the
	 linker requires objects begin on an even boundary.  So round
	 up the current offset as necessary.  */
      curr_som_offset = (curr_som_offset + 0x1) &~ (unsigned) 1;
      curr_bfd = curr_bfd->next;
      som_index++;
    }

  /* Now scribble out the hash table.  */
  amt = lst.hash_size * 4;
  if (bfd_bwrite ((PTR) hash_table, amt, abfd) != amt)
    goto error_return;

  /* Then the SOM dictionary.  */
  amt = lst.module_count * sizeof (struct som_entry);
  if (bfd_bwrite ((PTR) som_dict, amt, abfd) != amt)
    goto error_return;

  /* The library symbols.  */
  amt = nsyms * sizeof (struct lst_symbol_record);
  if (bfd_bwrite ((PTR) lst_syms, amt, abfd) != amt)
    goto error_return;

  /* And finally the strings.  */
  amt = string_size;
  if (bfd_bwrite ((PTR) strings, amt, abfd) != amt)
    goto error_return;

  if (hash_table != NULL)
    free (hash_table);
  if (som_dict != NULL)
    free (som_dict);
  if (last_hash_entry != NULL)
    free (last_hash_entry);
  if (lst_syms != NULL)
    free (lst_syms);
  if (strings != NULL)
    free (strings);
  return true;

 error_return:
  if (hash_table != NULL)
    free (hash_table);
  if (som_dict != NULL)
    free (som_dict);
  if (last_hash_entry != NULL)
    free (last_hash_entry);
  if (lst_syms != NULL)
    free (lst_syms);
  if (strings != NULL)
    free (strings);

  return false;
}

/* Write out the LST for the archive.

   You'll never believe this is really how armaps are handled in SOM...  */

static boolean
som_write_armap (abfd, elength, map, orl_count, stridx)
     bfd *abfd;
     unsigned int elength;
     struct orl *map ATTRIBUTE_UNUSED;
     unsigned int orl_count ATTRIBUTE_UNUSED;
     int stridx ATTRIBUTE_UNUSED;
{
  bfd *curr_bfd;
  struct stat statbuf;
  unsigned int i, lst_size, nsyms, stringsize;
  struct ar_hdr hdr;
  struct lst_header lst;
  int *p;
  bfd_size_type amt;

  /* We'll use this for the archive's date and mode later.  */
  if (stat (abfd->filename, &statbuf) != 0)
    {
      bfd_set_error (bfd_error_system_call);
      return false;
    }
  /* Fudge factor.  */
  bfd_ardata (abfd)->armap_timestamp = statbuf.st_mtime + 60;

  /* Account for the lst header first.  */
  lst_size = sizeof (struct lst_header);

  /* Start building the LST header.  */
  /* FIXME:  Do we need to examine each element to determine the
     largest id number?  */
  lst.system_id = CPU_PA_RISC1_0;
  lst.a_magic = LIBMAGIC;
  lst.version_id = VERSION_ID;
  lst.file_time.secs = 0;
  lst.file_time.nanosecs = 0;

  lst.hash_loc = lst_size;
  lst.hash_size = SOM_LST_HASH_SIZE;

  /* Hash table is a SOM_LST_HASH_SIZE 32bit offsets.  */
  lst_size += 4 * SOM_LST_HASH_SIZE;

  /* We need to count the number of SOMs in this archive.  */
  curr_bfd = abfd->archive_head;
  lst.module_count = 0;
  while (curr_bfd != NULL)
    {
      /* Only true SOM objects count.  */
      if (curr_bfd->format == bfd_object
	  && curr_bfd->xvec->flavour == bfd_target_som_flavour)
	lst.module_count++;
      curr_bfd = curr_bfd->next;
    }
  lst.module_limit = lst.module_count;
  lst.dir_loc = lst_size;
  lst_size += sizeof (struct som_entry) * lst.module_count;

  /* We don't support import/export tables, auxiliary headers,
     or free lists yet.  Make the linker work a little harder
     to make our life easier.  */

  lst.export_loc = 0;
  lst.export_count = 0;
  lst.import_loc = 0;
  lst.aux_loc = 0;
  lst.aux_size = 0;

  /* Count how many symbols we will have on the hash chains and the
     size of the associated string table.  */
  if (! som_bfd_prep_for_ar_write (abfd, &nsyms, &stringsize))
    return false;

  lst_size += sizeof (struct lst_symbol_record) * nsyms;

  /* For the string table.  One day we might actually use this info
     to avoid small seeks/reads when reading archives.  */
  lst.string_loc = lst_size;
  lst.string_size = stringsize;
  lst_size += stringsize;

  /* SOM ABI says this must be zero.  */
  lst.free_list = 0;
  lst.file_end = lst_size;

  /* Compute the checksum.  Must happen after the entire lst header
     has filled in.  */
  p = (int *) &lst;
  lst.checksum = 0;
  for (i = 0; i < sizeof (struct lst_header) / sizeof (int) - 1; i++)
    lst.checksum ^= *p++;

  sprintf (hdr.ar_name, "/               ");
  sprintf (hdr.ar_date, "%ld", bfd_ardata (abfd)->armap_timestamp);
  sprintf (hdr.ar_uid, "%ld", (long) getuid ());
  sprintf (hdr.ar_gid, "%ld", (long) getgid ());
  sprintf (hdr.ar_mode, "%-8o", (unsigned int) statbuf.st_mode);
  sprintf (hdr.ar_size, "%-10d", (int) lst_size);
  hdr.ar_fmag[0] = '`';
  hdr.ar_fmag[1] = '\012';

  /* Turn any nulls into spaces.  */
  for (i = 0; i < sizeof (struct ar_hdr); i++)
    if (((char *) (&hdr))[i] == '\0')
      (((char *) (&hdr))[i]) = ' ';

  /* Scribble out the ar header.  */
  amt = sizeof (struct ar_hdr);
  if (bfd_bwrite ((PTR) &hdr, amt, abfd) != amt)
    return false;

  /* Now scribble out the lst header.  */
  amt = sizeof (struct lst_header);
  if (bfd_bwrite ((PTR) &lst, amt, abfd) != amt)
    return false;

  /* Build and write the armap.  */
  if (!som_bfd_ar_write_symbol_stuff (abfd, nsyms, stringsize, lst, elength))
    return false;

  /* Done.  */
  return true;
}

/* Free all information we have cached for this BFD.  We can always
   read it again later if we need it.  */

static boolean
som_bfd_free_cached_info (abfd)
     bfd *abfd;
{
  asection *o;

  if (bfd_get_format (abfd) != bfd_object)
    return true;

#define FREE(x) if (x != NULL) { free (x); x = NULL; }
  /* Free the native string and symbol tables.  */
  FREE (obj_som_symtab (abfd));
  FREE (obj_som_stringtab (abfd));
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
      /* Free the native relocations.  */
      o->reloc_count = (unsigned) -1;
      FREE (som_section_data (o)->reloc_stream);
      /* Free the generic relocations.  */
      FREE (o->relocation);
    }
#undef FREE

  return true;
}

/* End of miscellaneous support functions.  */

/* Linker support functions.  */

static boolean
som_bfd_link_split_section (abfd, sec)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
{
  return (som_is_subspace (sec) && sec->_raw_size > 240000);
}

#define	som_close_and_cleanup		som_bfd_free_cached_info

#define som_read_ar_hdr			_bfd_generic_read_ar_hdr
#define som_openr_next_archived_file	bfd_generic_openr_next_archived_file
#define som_get_elt_at_index		_bfd_generic_get_elt_at_index
#define som_generic_stat_arch_elt	bfd_generic_stat_arch_elt
#define som_truncate_arname		bfd_bsd_truncate_arname
#define som_slurp_extended_name_table	_bfd_slurp_extended_name_table
#define som_construct_extended_name_table \
  _bfd_archive_coff_construct_extended_name_table
#define som_update_armap_timestamp	bfd_true
#define som_bfd_print_private_bfd_data  _bfd_generic_bfd_print_private_bfd_data

#define som_get_lineno                  _bfd_nosymbols_get_lineno
#define som_bfd_make_debug_symbol	_bfd_nosymbols_bfd_make_debug_symbol
#define som_read_minisymbols		_bfd_generic_read_minisymbols
#define som_minisymbol_to_symbol	_bfd_generic_minisymbol_to_symbol
#define som_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

#define som_bfd_get_relocated_section_contents \
 bfd_generic_get_relocated_section_contents
#define som_bfd_relax_section bfd_generic_relax_section
#define som_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define som_bfd_link_hash_table_free _bfd_generic_link_hash_table_free
#define som_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define som_bfd_link_just_syms _bfd_generic_link_just_syms
#define som_bfd_final_link _bfd_generic_final_link

#define som_bfd_gc_sections		bfd_generic_gc_sections
#define som_bfd_merge_sections		bfd_generic_merge_sections
#define som_bfd_discard_group		bfd_generic_discard_group

const bfd_target som_vec = {
  "som",			/* name */
  bfd_target_som_flavour,
  BFD_ENDIAN_BIG,		/* target byte order */
  BFD_ENDIAN_BIG,		/* target headers byte order */
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED | DYNAMIC),
  (SEC_CODE | SEC_DATA | SEC_ROM | SEC_HAS_CONTENTS
   | SEC_ALLOC | SEC_LOAD | SEC_RELOC),		/* section flags */

/* leading_symbol_char: is the first char of a user symbol
   predictable, and if so what is it */
  0,
  '/',				/* ar_pad_char */
  14,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */
  {_bfd_dummy_target,
   som_object_p,		/* bfd_check_format */
   bfd_generic_archive_p,
   _bfd_dummy_target
  },
  {
    bfd_false,
    som_mkobject,
    _bfd_generic_mkarchive,
    bfd_false
  },
  {
    bfd_false,
    som_write_object_contents,
    _bfd_write_archive_contents,
    bfd_false,
  },
#undef som

  BFD_JUMP_TABLE_GENERIC (som),
  BFD_JUMP_TABLE_COPY (som),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (som),
  BFD_JUMP_TABLE_SYMBOLS (som),
  BFD_JUMP_TABLE_RELOCS (som),
  BFD_JUMP_TABLE_WRITE (som),
  BFD_JUMP_TABLE_LINK (som),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,

  (PTR) 0
};

#endif /* HOST_HPPAHPUX || HOST_HPPABSD || HOST_HPPAOSF */
