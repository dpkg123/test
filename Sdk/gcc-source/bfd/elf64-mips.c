/* MIPS-specific support for 64-bit ELF
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.
   Ian Lance Taylor, Cygnus Support
   Linker support added by Mark Mitchell, CodeSourcery, LLC.
   <mark@codesourcery.com>

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

/* This file supports the 64-bit MIPS ELF ABI.

   The MIPS 64-bit ELF ABI uses an unusual reloc format.  This file
   overrides the usual ELF reloc handling, and handles reading and
   writing the relocations here.  */

/* TODO: Many things are unsupported, even if there is some code for it
 .       (which was mostly stolen from elf32-mips.c and slightly adapted).
 .
 .   - Relocation handling for REL relocs is wrong in many cases and
 .     generally untested.
 .   - Relocation handling for RELA relocs related to GOT support are
 .     also likely to be wrong.
 .   - Support for MIPS16 is untested.
 .   - Combined relocs with RSS_* entries are unsupported.
 .   - The whole GOT handling for NewABI is missing, some parts of
 .     the OldABI version is still lying around and should be removed.
 */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "aout/ar.h"
#include "bfdlink.h"
#include "genlink.h"
#include "elf-bfd.h"
#include "elfxx-mips.h"
#include "elf/mips.h"

/* Get the ECOFF swapping routines.  The 64-bit ABI is not supposed to
   use ECOFF.  However, we support it anyhow for an easier changeover.  */
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/internal.h"
#include "coff/ecoff.h"
/* The 64 bit versions of the mdebug data structures are in alpha.h.  */
#include "coff/alpha.h"
#define ECOFF_SIGNED_64
#include "ecoffswap.h"

static void mips_elf64_swap_reloc_in
  PARAMS ((bfd *, const Elf64_Mips_External_Rel *,
	   Elf64_Mips_Internal_Rel *));
static void mips_elf64_swap_reloca_in
  PARAMS ((bfd *, const Elf64_Mips_External_Rela *,
	   Elf64_Mips_Internal_Rela *));
static void mips_elf64_swap_reloc_out
  PARAMS ((bfd *, const Elf64_Mips_Internal_Rel *,
	   Elf64_Mips_External_Rel *));
static void mips_elf64_swap_reloca_out
  PARAMS ((bfd *, const Elf64_Mips_Internal_Rela *,
	   Elf64_Mips_External_Rela *));
static void mips_elf64_be_swap_reloc_in
  PARAMS ((bfd *, const bfd_byte *, Elf_Internal_Rel *));
static void mips_elf64_be_swap_reloc_out
  PARAMS ((bfd *, const Elf_Internal_Rel *, bfd_byte *));
static void mips_elf64_be_swap_reloca_in
  PARAMS ((bfd *, const bfd_byte *, Elf_Internal_Rela *));
static void mips_elf64_be_swap_reloca_out
  PARAMS ((bfd *, const Elf_Internal_Rela *, bfd_byte *));
static reloc_howto_type *bfd_elf64_bfd_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static reloc_howto_type *mips_elf64_rtype_to_howto
  PARAMS ((unsigned int, boolean));
static void mips_elf64_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf64_Internal_Rel *));
static void mips_elf64_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf64_Internal_Rela *));
static long mips_elf64_get_reloc_upper_bound PARAMS ((bfd *, asection *));
static boolean mips_elf64_slurp_one_reloc_table
  PARAMS ((bfd *, asection *, asymbol **, const Elf_Internal_Shdr *));
static boolean mips_elf64_slurp_reloc_table
  PARAMS ((bfd *, asection *, asymbol **, boolean));
static void mips_elf64_write_relocs PARAMS ((bfd *, asection *, PTR));
static void mips_elf64_write_rel
  PARAMS((bfd *, asection *, Elf_Internal_Shdr *, int *, PTR));
static void mips_elf64_write_rela
  PARAMS((bfd *, asection *, Elf_Internal_Shdr *, int *, PTR));
static bfd_reloc_status_type mips_elf64_hi16_reloc
  PARAMS ((bfd *, arelent *, asymbol *,	PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_gprel16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_literal_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_gprel32_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_shift6_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_got16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips16_jump_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips16_gprel_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static boolean mips_elf64_assign_gp PARAMS ((bfd *, bfd_vma *));
static bfd_reloc_status_type mips_elf64_final_gp
  PARAMS ((bfd *, asymbol *, boolean, char **, bfd_vma *));
static boolean mips_elf64_object_p PARAMS ((bfd *));
static irix_compat_t elf64_mips_irix_compat PARAMS ((bfd *));

extern const bfd_target bfd_elf64_bigmips_vec;
extern const bfd_target bfd_elf64_littlemips_vec;

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

/* The number of local .got entries we reserve.  */
#define MIPS_RESERVED_GOTNO (2)

/* The relocation table used for SHT_REL sections.  */

static reloc_howto_type mips_elf64_howto_table_rel[] =
{
  /* No relocation.  */
  HOWTO (R_MIPS_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit relocation.  */
  HOWTO (R_MIPS_16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit relocation.  */
  HOWTO (R_MIPS_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit symbol relative relocation.  */
  HOWTO (R_MIPS_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL32",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 26 bit jump address.  */
  HOWTO (R_MIPS_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
				/* This needs complex overflow
				   detection, because the upper 36
				   bits must match the PC + 4.  */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_26",		/* name */
	 true,			/* partial_inplace */
	 0x03ffffff,		/* src_mask */
	 0x03ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* R_MIPS_HI16 and R_MIPS_LO16 are unsupported for NewABI REL.
     However, the native IRIX6 tools use them, so we try our best. */

  /* High 16 bits of symbol value.  */
  HOWTO (R_MIPS_HI16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf64_hi16_reloc,	/* special_function */
	 "R_MIPS_HI16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_MIPS_LO16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_LO16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* GP relative reference.  */
  HOWTO (R_MIPS_GPREL16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf64_gprel16_reloc, /* special_function */
	 "R_MIPS_GPREL16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to literal section.  */
  HOWTO (R_MIPS_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf64_literal_reloc, /* special_function */
	 "R_MIPS_LITERAL",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to global offset table.  */
  HOWTO (R_MIPS_GOT16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf64_got16_reloc, /* special_function */
	 "R_MIPS_GOT16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit PC relative reference.  */
  HOWTO (R_MIPS_PC16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PC16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16 bit call through global offset table.  */
  HOWTO (R_MIPS_CALL16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit GP relative reference.  */
  HOWTO (R_MIPS_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf64_gprel32_reloc, /* special_function */
	 "R_MIPS_GPREL32",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  EMPTY_HOWTO (13),
  EMPTY_HOWTO (14),
  EMPTY_HOWTO (15),

  /* A 5 bit shift field.  */
  HOWTO (R_MIPS_SHIFT5,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SHIFT5",	/* name */
	 true,			/* partial_inplace */
	 0x000007c0,		/* src_mask */
	 0x000007c0,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 6 bit shift field.  */
  HOWTO (R_MIPS_SHIFT6,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mips_elf64_shift6_reloc, /* special_function */
	 "R_MIPS_SHIFT6",	/* name */
	 true,			/* partial_inplace */
	 0x000007c4,		/* src_mask */
	 0x000007c4,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit relocation.  */
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_64",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement in the global offset table.  */
  HOWTO (R_MIPS_GOT_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_DISP",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement to page pointer in the global offset table.  */
  HOWTO (R_MIPS_GOT_PAGE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_PAGE",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Offset from page pointer in the global offset table.  */
  HOWTO (R_MIPS_GOT_OFST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_OFST",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_GOT_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_HI16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_GOT_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit substraction.  */
  HOWTO (R_MIPS_SUB,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SUB",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_A,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_A",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction, and change all relocations
     to refer to the old instruction at the address.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_B,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_B",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Delete a 32 bit instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_DELETE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_DELETE",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The MIPS ELF64 ABI Draft wants us to support these for REL relocations.
     We don't, because
       a) It means building the addend from a R_MIPS_HIGHEST/R_MIPS_HIGHER/
	  R_MIPS_HI16/R_MIPS_LO16 sequence with varying ordering, using
	  fallable heuristics.
       b) No other NewABI toolchain actually emits such relocations.  */
  EMPTY_HOWTO (R_MIPS_HIGHER),
  EMPTY_HOWTO (R_MIPS_HIGHEST),

  /* High 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_CALL_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_HI16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_CALL_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Section displacement, used by an associated event location section.  */
  HOWTO (R_MIPS_SCN_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SCN_DISP",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_REL16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL16",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* These two are obsolete.  */
  EMPTY_HOWTO (R_MIPS_ADD_IMMEDIATE),
  EMPTY_HOWTO (R_MIPS_PJUMP),

  /* Similiar to R_MIPS_REL32, but used for relocations in a GOT section.
     It must be used for multigot GOT's (and only there).  */
  HOWTO (R_MIPS_RELGOT,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_RELGOT",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Protected jump conversion.  This is an optimization hint.  No
     relocation is required for correctness.  */
  HOWTO (R_MIPS_JALR,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_JALR",	        /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x00000000,		/* dst_mask */
	 false),		/* pcrel_offset */
};

/* The relocation table used for SHT_RELA sections.  */

static reloc_howto_type mips_elf64_howto_table_rela[] =
{
  /* No relocation.  */
  HOWTO (R_MIPS_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit relocation.  */
  HOWTO (R_MIPS_16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit relocation.  */
  HOWTO (R_MIPS_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit symbol relative relocation.  */
  HOWTO (R_MIPS_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 26 bit jump address.  */
  HOWTO (R_MIPS_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
				/* This needs complex overflow
				   detection, because the upper 36
				   bits must match the PC + 4.  */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_26",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x03ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of symbol value.  */
  HOWTO (R_MIPS_HI16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_HI16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_MIPS_LO16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_LO16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* GP relative reference.  */
  HOWTO (R_MIPS_GPREL16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf64_gprel16_reloc, /* special_function */
	 "R_MIPS_GPREL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to literal section.  */
  HOWTO (R_MIPS_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf64_literal_reloc, /* special_function */
	 "R_MIPS_LITERAL",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to global offset table.  */
  HOWTO (R_MIPS_GOT16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf64_got16_reloc, /* special_function */
	 "R_MIPS_GOT16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit PC relative reference.  */
  HOWTO (R_MIPS_PC16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PC16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16 bit call through global offset table.  */
  HOWTO (R_MIPS_CALL16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit GP relative reference.  */
  HOWTO (R_MIPS_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf64_gprel32_reloc, /* special_function */
	 "R_MIPS_GPREL32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  EMPTY_HOWTO (13),
  EMPTY_HOWTO (14),
  EMPTY_HOWTO (15),

  /* A 5 bit shift field.  */
  HOWTO (R_MIPS_SHIFT5,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SHIFT5",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x000007c0,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 6 bit shift field.  */
  HOWTO (R_MIPS_SHIFT6,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mips_elf64_shift6_reloc, /* special_function */
	 "R_MIPS_SHIFT6",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x000007c4,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit relocation.  */
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_64",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement in the global offset table.  */
  HOWTO (R_MIPS_GOT_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_DISP",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement to page pointer in the global offset table.  */
  HOWTO (R_MIPS_GOT_PAGE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_PAGE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Offset from page pointer in the global offset table.  */
  HOWTO (R_MIPS_GOT_OFST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_OFST",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_GOT_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_HI16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_GOT_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_LO16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit substraction.  */
  HOWTO (R_MIPS_SUB,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SUB",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_A,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_A",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction, and change all relocations
     to refer to the old instruction at the address.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_B,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_B",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Delete a 32 bit instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_DELETE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_DELETE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the higher value of a 64 bit addend.  */
  HOWTO (R_MIPS_HIGHER,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_MIPS_HIGHER",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the highest value of a 64 bit addend.  */
  HOWTO (R_MIPS_HIGHEST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_MIPS_HIGHEST",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_CALL_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_HI16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_CALL_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_LO16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Section displacement, used by an associated event location section.  */
  HOWTO (R_MIPS_SCN_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SCN_DISP",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_REL16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* These two are obsolete.  */
  EMPTY_HOWTO (R_MIPS_ADD_IMMEDIATE),
  EMPTY_HOWTO (R_MIPS_PJUMP),

  /* Similiar to R_MIPS_REL32, but used for relocations in a GOT section.
     It must be used for multigot GOT's (and only there).  */
  HOWTO (R_MIPS_RELGOT,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_RELGOT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Protected jump conversion.  This is an optimization hint.  No
     relocation is required for correctness.  */
  HOWTO (R_MIPS_JALR,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_JALR",	        /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x00000000,		/* dst_mask */
	 false),		/* pcrel_offset */
};

/* The reloc used for the mips16 jump instruction.  */
static reloc_howto_type elf_mips16_jump_howto =
  HOWTO (R_MIPS16_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 			/* This needs complex overflow
				   detection, because the upper four
				   bits must match the PC.  */
	 mips16_jump_reloc,	/* special_function */
	 "R_MIPS16_26",		/* name */
	 true,			/* partial_inplace */
	 0x3ffffff,		/* src_mask */
	 0x3ffffff,		/* dst_mask */
	 false);		/* pcrel_offset */

/* The reloc used for the mips16 gprel instruction.  */
static reloc_howto_type elf_mips16_gprel_howto =
  HOWTO (R_MIPS16_GPREL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips16_gprel_reloc,	/* special_function */
	 "R_MIPS16_GPREL",	/* name */
	 true,			/* partial_inplace */
	 0x07ff001f,		/* src_mask */
	 0x07ff001f,	        /* dst_mask */
	 false);		/* pcrel_offset */

/* GNU extension to record C++ vtable hierarchy */
static reloc_howto_type elf_mips_gnu_vtinherit_howto =
  HOWTO (R_MIPS_GNU_VTINHERIT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_MIPS_GNU_VTINHERIT", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false);		/* pcrel_offset */

/* GNU extension to record C++ vtable member usage */
static reloc_howto_type elf_mips_gnu_vtentry_howto =
  HOWTO (R_MIPS_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_elf_rel_vtable_reloc_fn, /* special_function */
	 "R_MIPS_GNU_VTENTRY",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false);		/* pcrel_offset */

/* Swap in a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_swap_reloc_in (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_External_Rel *src;
     Elf64_Mips_Internal_Rel *dst;
{
  dst->r_offset = H_GET_64 (abfd, src->r_offset);
  dst->r_sym = H_GET_32 (abfd, src->r_sym);
  dst->r_ssym = H_GET_8 (abfd, src->r_ssym);
  dst->r_type3 = H_GET_8 (abfd, src->r_type3);
  dst->r_type2 = H_GET_8 (abfd, src->r_type2);
  dst->r_type = H_GET_8 (abfd, src->r_type);
}

/* Swap in a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_swap_reloca_in (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_External_Rela *src;
     Elf64_Mips_Internal_Rela *dst;
{
  dst->r_offset = H_GET_64 (abfd, src->r_offset);
  dst->r_sym = H_GET_32 (abfd, src->r_sym);
  dst->r_ssym = H_GET_8 (abfd, src->r_ssym);
  dst->r_type3 = H_GET_8 (abfd, src->r_type3);
  dst->r_type2 = H_GET_8 (abfd, src->r_type2);
  dst->r_type = H_GET_8 (abfd, src->r_type);
  dst->r_addend = H_GET_S64 (abfd, src->r_addend);
}

/* Swap out a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_swap_reloc_out (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_Internal_Rel *src;
     Elf64_Mips_External_Rel *dst;
{
  H_PUT_64 (abfd, src->r_offset, dst->r_offset);
  H_PUT_32 (abfd, src->r_sym, dst->r_sym);
  H_PUT_8 (abfd, src->r_ssym, dst->r_ssym);
  H_PUT_8 (abfd, src->r_type3, dst->r_type3);
  H_PUT_8 (abfd, src->r_type2, dst->r_type2);
  H_PUT_8 (abfd, src->r_type, dst->r_type);
}

/* Swap out a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_swap_reloca_out (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_Internal_Rela *src;
     Elf64_Mips_External_Rela *dst;
{
  H_PUT_64 (abfd, src->r_offset, dst->r_offset);
  H_PUT_32 (abfd, src->r_sym, dst->r_sym);
  H_PUT_8 (abfd, src->r_ssym, dst->r_ssym);
  H_PUT_8 (abfd, src->r_type3, dst->r_type3);
  H_PUT_8 (abfd, src->r_type2, dst->r_type2);
  H_PUT_8 (abfd, src->r_type, dst->r_type);
  H_PUT_S64 (abfd, src->r_addend, dst->r_addend);
}

/* Swap in a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_be_swap_reloc_in (abfd, src, dst)
     bfd *abfd;
     const bfd_byte *src;
     Elf_Internal_Rel *dst;
{
  Elf64_Mips_Internal_Rel mirel;

  mips_elf64_swap_reloc_in (abfd,
			    (const Elf64_Mips_External_Rel *) src,
			    &mirel);

  dst[0].r_offset = mirel.r_offset;
  dst[0].r_info = ELF64_R_INFO (mirel.r_sym, mirel.r_type);
  dst[1].r_offset = mirel.r_offset;
  dst[1].r_info = ELF64_R_INFO (mirel.r_ssym, mirel.r_type2);
  dst[2].r_offset = mirel.r_offset;
  dst[2].r_info = ELF64_R_INFO (STN_UNDEF, mirel.r_type3);
}

/* Swap in a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_be_swap_reloca_in (abfd, src, dst)
     bfd *abfd;
     const bfd_byte *src;
     Elf_Internal_Rela *dst;
{
  Elf64_Mips_Internal_Rela mirela;

  mips_elf64_swap_reloca_in (abfd,
			     (const Elf64_Mips_External_Rela *) src,
			     &mirela);

  dst[0].r_offset = mirela.r_offset;
  dst[0].r_info = ELF64_R_INFO (mirela.r_sym, mirela.r_type);
  dst[0].r_addend = mirela.r_addend;
  dst[1].r_offset = mirela.r_offset;
  dst[1].r_info = ELF64_R_INFO (mirela.r_ssym, mirela.r_type2);
  dst[1].r_addend = 0;
  dst[2].r_offset = mirela.r_offset;
  dst[2].r_info = ELF64_R_INFO (STN_UNDEF, mirela.r_type3);
  dst[2].r_addend = 0;
}

/* Swap out a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_be_swap_reloc_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Rel *src;
     bfd_byte *dst;
{
  Elf64_Mips_Internal_Rel mirel;

  mirel.r_offset = src[0].r_offset;
  BFD_ASSERT(src[0].r_offset == src[1].r_offset);
#if 0  
  BFD_ASSERT(src[0].r_offset == src[2].r_offset);
#endif

  mirel.r_type = ELF64_MIPS_R_TYPE (src[0].r_info);
  mirel.r_sym = ELF64_R_SYM (src[0].r_info);
  mirel.r_type2 = ELF64_MIPS_R_TYPE (src[1].r_info);
  mirel.r_ssym = ELF64_MIPS_R_SSYM (src[1].r_info);
  mirel.r_type3 = ELF64_MIPS_R_TYPE (src[2].r_info);

  mips_elf64_swap_reloc_out (abfd, &mirel,
			     (Elf64_Mips_External_Rel *) dst);
}

/* Swap out a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_be_swap_reloca_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Rela *src;
     bfd_byte *dst;
{
  Elf64_Mips_Internal_Rela mirela;

  mirela.r_offset = src[0].r_offset;
  BFD_ASSERT(src[0].r_offset == src[1].r_offset);
  BFD_ASSERT(src[0].r_offset == src[2].r_offset);

  mirela.r_type = ELF64_MIPS_R_TYPE (src[0].r_info);
  mirela.r_sym = ELF64_R_SYM (src[0].r_info);
  mirela.r_addend = src[0].r_addend;
  BFD_ASSERT(src[1].r_addend == 0);
  BFD_ASSERT(src[2].r_addend == 0);

  mirela.r_type2 = ELF64_MIPS_R_TYPE (src[1].r_info);
  mirela.r_ssym = ELF64_MIPS_R_SSYM (src[1].r_info);
  mirela.r_type3 = ELF64_MIPS_R_TYPE (src[2].r_info);

  mips_elf64_swap_reloca_out (abfd, &mirela,
			      (Elf64_Mips_External_Rela *) dst);
}

/* Do a R_MIPS_HI16 relocation.  */

static bfd_reloc_status_type
mips_elf64_hi16_reloc (abfd, reloc_entry, symbol, data, input_section,
		       output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* If we're relocating, and this is an external symbol, we don't
     want to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (((reloc_entry->addend & 0xffff) + 0x8000) & ~0xffff)
    reloc_entry->addend += 0x8000;

  return bfd_reloc_continue;
}

/* Do a R_MIPS_GOT16 reloc.  This is a reloc against the global offset
   table used for PIC code.  If the symbol is an external symbol, the
   instruction is modified to contain the offset of the appropriate
   entry in the global offset table.  If the symbol is a section
   symbol, the next reloc is a R_MIPS_LO16 reloc.  The two 16 bit
   addends are combined to form the real addend against the section
   symbol; the GOT16 is modified to contain the offset of an entry in
   the global offset table, and the LO16 is modified to offset it
   appropriately.  Thus an offset larger than 16 bits requires a
   modified value in the global offset table.

   This implementation suffices for the assembler, but the linker does
   not yet know how to create global offset tables.  */

static bfd_reloc_status_type
mips_elf64_got16_reloc (abfd, reloc_entry, symbol, data, input_section,
			output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  /* If we're relocating, and this is a local symbol, we can handle it
     just like an R_MIPS_HI16.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) != 0)
    return mips_elf64_hi16_reloc (abfd, reloc_entry, symbol, data,
				  input_section, output_bfd, error_message);


  /* Otherwise we try to handle it as R_MIPS_GOT_DISP.  */
  return bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
				input_section, output_bfd, error_message);
}

/* Set the GP value for OUTPUT_BFD.  Returns false if this is a
   dangerous relocation.  */

static boolean
mips_elf64_assign_gp (output_bfd, pgp)
     bfd *output_bfd;
     bfd_vma *pgp;
{
  unsigned int count;
  asymbol **sym;
  unsigned int i;

  /* If we've already figured out what GP will be, just return it.  */
  *pgp = _bfd_get_gp_value (output_bfd);
  if (*pgp)
    return true;

  count = bfd_get_symcount (output_bfd);
  sym = bfd_get_outsymbols (output_bfd);

  /* The linker script will have created a symbol named `_gp' with the
     appropriate value.  */
  if (sym == (asymbol **) NULL)
    i = count;
  else
    {
      for (i = 0; i < count; i++, sym++)
	{
	  register const char *name;

	  name = bfd_asymbol_name (*sym);
	  if (*name == '_' && strcmp (name, "_gp") == 0)
	    {
	      *pgp = bfd_asymbol_value (*sym);
	      _bfd_set_gp_value (output_bfd, *pgp);
	      break;
	    }
	}
    }

  if (i >= count)
    {
      /* Only get the error once.  */
      *pgp = 4;
      _bfd_set_gp_value (output_bfd, *pgp);
      return false;
    }

  return true;
}

/* We have to figure out the gp value, so that we can adjust the
   symbol value correctly.  We look up the symbol _gp in the output
   BFD.  If we can't find it, we're stuck.  We cache it in the ELF
   target data.  We don't need to adjust the symbol value for an
   external symbol if we are producing relocateable output.  */

static bfd_reloc_status_type
mips_elf64_final_gp (output_bfd, symbol, relocateable, error_message, pgp)
     bfd *output_bfd;
     asymbol *symbol;
     boolean relocateable;
     char **error_message;
     bfd_vma *pgp;
{
  if (bfd_is_und_section (symbol->section)
      && ! relocateable)
    {
      *pgp = 0;
      return bfd_reloc_undefined;
    }

  *pgp = _bfd_get_gp_value (output_bfd);
  if (*pgp == 0
      && (! relocateable
	  || (symbol->flags & BSF_SECTION_SYM) != 0))
    {
      if (relocateable)
	{
	  /* Make up a value.  */
	  *pgp = symbol->section->output_section->vma /*+ 0x4000*/;
	  _bfd_set_gp_value (output_bfd, *pgp);
	}
      else if (!mips_elf64_assign_gp (output_bfd, pgp))
	{
	  *error_message =
	    (char *) _("GP relative relocation when _gp not defined");
	  return bfd_reloc_dangerous;
	}
    }

  return bfd_reloc_ok;
}

/* Do a R_MIPS_GPREL16 relocation.  This is a 16 bit value which must
   become the offset from the gp register.  */

static bfd_reloc_status_type
mips_elf64_gprel16_reloc (abfd, reloc_entry, symbol, data, input_section,
			  output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != (bfd *) NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  ret = mips_elf64_final_gp (output_bfd, symbol, relocateable, error_message,
			     &gp);
  if (ret != bfd_reloc_ok)
    return ret;

  return _bfd_mips_elf_gprel16_with_gp (abfd, symbol, reloc_entry,
					input_section, relocateable,
					data, gp);
}

/* Do a R_MIPS_LITERAL relocation.  */

static bfd_reloc_status_type
mips_elf64_literal_reloc (abfd, reloc_entry, symbol, data, input_section,
			  output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;

  /* If we're relocating, and this is an external symbol, we don't
     want to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* FIXME: The entries in the .lit8 and .lit4 sections should be merged.  */
  if (output_bfd != (bfd *) NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  ret = mips_elf64_final_gp (output_bfd, symbol, relocateable, error_message,
			     &gp);
  if (ret != bfd_reloc_ok)
    return ret;

  return _bfd_mips_elf_gprel16_with_gp (abfd, symbol, reloc_entry,
					input_section, relocateable,
					data, gp);
}

/* Do a R_MIPS_GPREL32 relocation.  This is a 32 bit value which must
   become the offset from the gp register.  */

static bfd_reloc_status_type
mips_elf64_gprel32_reloc (abfd, reloc_entry, symbol, data, input_section,
			  output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;
  bfd_vma relocation;
  unsigned long val;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      *error_message = (char *)
	_("32bits gp relative relocation occurs for an external symbol");
      return bfd_reloc_outofrange;
    }

  if (output_bfd != (bfd *) NULL)
    {
      relocateable = true;
      gp = _bfd_get_gp_value (output_bfd);
    }
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;

      ret = mips_elf64_final_gp (output_bfd, symbol, relocateable,
				 error_message, &gp);
      if (ret != bfd_reloc_ok)
	return ret;
    }

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  if (reloc_entry->howto->src_mask == 0)
    {
      /* This case arises with the 64-bit MIPS ELF ABI.  */
      val = 0;
    }
  else
    val = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

  /* Set val to the offset into the section or symbol.  */
  val += reloc_entry->addend;

  /* Adjust val for the final section location and GP value.  If we
     are producing relocateable output, we don't want to do this for
     an external symbol.  */
  if (! relocateable
      || (symbol->flags & BSF_SECTION_SYM) != 0)
    val += relocation - gp;

  bfd_put_32 (abfd, val, (bfd_byte *) data + reloc_entry->address);

  if (relocateable)
    reloc_entry->address += input_section->output_offset;

  return bfd_reloc_ok;
}

/* Do a R_MIPS_SHIFT6 relocation. The MSB of the shift is stored at bit 2,
   the rest is at bits 6-10. The bitpos already got right by the howto.  */

static bfd_reloc_status_type
mips_elf64_shift6_reloc (abfd, reloc_entry, symbol, data, input_section,
			 output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* If we're relocating, and this is an external symbol, we don't
     want to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  reloc_entry->addend = (reloc_entry->addend & 0x00007c0)
			| (reloc_entry->addend & 0x00000800) >> 9;

  return bfd_reloc_continue;
}

/* Handle a mips16 jump.  */

static bfd_reloc_status_type
mips16_jump_reloc (abfd, reloc_entry, symbol, data, input_section,
		   output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* FIXME.  */
  {
    static boolean warned;

    if (! warned)
      (*_bfd_error_handler)
	(_("Linking mips16 objects into %s format is not supported"),
	 bfd_get_target (input_section->output_section->owner));
    warned = true;
  }

  return bfd_reloc_undefined;
}

/* Handle a mips16 GP relative reloc.  */

static bfd_reloc_status_type
mips16_gprel_reloc (abfd, reloc_entry, symbol, data, input_section,
		    output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;
  unsigned short extend, insn;
  unsigned long final;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  ret = mips_elf64_final_gp (output_bfd, symbol, relocateable, error_message,
			     &gp);
  if (ret != bfd_reloc_ok)
    return ret;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* Pick up the mips16 extend instruction and the real instruction.  */
  extend = bfd_get_16 (abfd, (bfd_byte *) data + reloc_entry->address);
  insn = bfd_get_16 (abfd, (bfd_byte *) data + reloc_entry->address + 2);

  /* Stuff the current addend back as a 32 bit value, do the usual
     relocation, and then clean up.  */
  bfd_put_32 (abfd,
	      (bfd_vma) (((extend & 0x1f) << 11)
			 | (extend & 0x7e0)
			 | (insn & 0x1f)),
	      (bfd_byte *) data + reloc_entry->address);

  ret = _bfd_mips_elf_gprel16_with_gp (abfd, symbol, reloc_entry,
				       input_section, relocateable, data, gp);

  final = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);
  bfd_put_16 (abfd,
	      (bfd_vma) ((extend & 0xf800)
			 | ((final >> 11) & 0x1f)
			 | (final & 0x7e0)),
	      (bfd_byte *) data + reloc_entry->address);
  bfd_put_16 (abfd,
	      (bfd_vma) ((insn & 0xffe0)
			 | (final & 0x1f)),
	      (bfd_byte *) data + reloc_entry->address + 2);

  return ret;
}

/* A mapping from BFD reloc types to MIPS ELF reloc types.  */

struct elf_reloc_map {
  bfd_reloc_code_real_type bfd_val;
  enum elf_mips_reloc_type elf_val;
};

static const struct elf_reloc_map mips_reloc_map[] =
{
  { BFD_RELOC_NONE, R_MIPS_NONE },
  { BFD_RELOC_16, R_MIPS_16 },
  { BFD_RELOC_32, R_MIPS_32 },
  /* There is no BFD reloc for R_MIPS_REL32.  */
  { BFD_RELOC_64, R_MIPS_64 },
  { BFD_RELOC_CTOR, R_MIPS_64 },
  { BFD_RELOC_16_PCREL, R_MIPS_PC16 },
  { BFD_RELOC_HI16_S, R_MIPS_HI16 },
  { BFD_RELOC_LO16, R_MIPS_LO16 },
  { BFD_RELOC_GPREL16, R_MIPS_GPREL16 },
  { BFD_RELOC_GPREL32, R_MIPS_GPREL32 },
  { BFD_RELOC_MIPS_JMP, R_MIPS_26 },
  { BFD_RELOC_MIPS_LITERAL, R_MIPS_LITERAL },
  { BFD_RELOC_MIPS_GOT16, R_MIPS_GOT16 },
  { BFD_RELOC_MIPS_CALL16, R_MIPS_CALL16 },
  { BFD_RELOC_MIPS_SHIFT5, R_MIPS_SHIFT5 },
  { BFD_RELOC_MIPS_SHIFT6, R_MIPS_SHIFT6 },
  { BFD_RELOC_MIPS_GOT_DISP, R_MIPS_GOT_DISP },
  { BFD_RELOC_MIPS_GOT_PAGE, R_MIPS_GOT_PAGE },
  { BFD_RELOC_MIPS_GOT_OFST, R_MIPS_GOT_OFST },
  { BFD_RELOC_MIPS_GOT_HI16, R_MIPS_GOT_HI16 },
  { BFD_RELOC_MIPS_GOT_LO16, R_MIPS_GOT_LO16 },
  { BFD_RELOC_MIPS_SUB, R_MIPS_SUB },
  { BFD_RELOC_MIPS_INSERT_A, R_MIPS_INSERT_A },
  { BFD_RELOC_MIPS_INSERT_B, R_MIPS_INSERT_B },
  { BFD_RELOC_MIPS_DELETE, R_MIPS_DELETE },
  { BFD_RELOC_MIPS_HIGHEST, R_MIPS_HIGHEST },
  { BFD_RELOC_MIPS_HIGHER, R_MIPS_HIGHER },
  { BFD_RELOC_MIPS_CALL_HI16, R_MIPS_CALL_HI16 },
  { BFD_RELOC_MIPS_CALL_LO16, R_MIPS_CALL_LO16 },
  { BFD_RELOC_MIPS_SCN_DISP, R_MIPS_SCN_DISP },
  { BFD_RELOC_MIPS_REL16, R_MIPS_REL16 },
  /* Use of R_MIPS_ADD_IMMEDIATE and R_MIPS_PJUMP is deprecated.  */
  { BFD_RELOC_MIPS_RELGOT, R_MIPS_RELGOT },
  { BFD_RELOC_MIPS_JALR, R_MIPS_JALR }
};

/* Given a BFD reloc type, return a howto structure.  */

static reloc_howto_type *
bfd_elf64_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;
  /* FIXME: We default to RELA here instead of choosing the right
     relocation variant.  */
  reloc_howto_type *howto_table = mips_elf64_howto_table_rela;

  for (i = 0; i < sizeof (mips_reloc_map) / sizeof (struct elf_reloc_map);
       i++)
    {
      if (mips_reloc_map[i].bfd_val == code)
	return &howto_table[(int) mips_reloc_map[i].elf_val];
    }

  switch (code)
    {
    case BFD_RELOC_MIPS16_JMP:
      return &elf_mips16_jump_howto;
    case BFD_RELOC_MIPS16_GPREL:
      return &elf_mips16_gprel_howto;
    case BFD_RELOC_VTABLE_INHERIT:
      return &elf_mips_gnu_vtinherit_howto;
    case BFD_RELOC_VTABLE_ENTRY:
      return &elf_mips_gnu_vtentry_howto;
    default:
      bfd_set_error (bfd_error_bad_value);
      return NULL;
    }
}

/* Given a MIPS Elf64_Internal_Rel, fill in an arelent structure.  */

static reloc_howto_type *
mips_elf64_rtype_to_howto (r_type, rela_p)
     unsigned int r_type;
     boolean rela_p;
{
  switch (r_type)
    {
    case R_MIPS16_26:
      return &elf_mips16_jump_howto;
    case R_MIPS16_GPREL:
      return &elf_mips16_gprel_howto;
    case R_MIPS_GNU_VTINHERIT:
      return &elf_mips_gnu_vtinherit_howto;
    case R_MIPS_GNU_VTENTRY:
      return &elf_mips_gnu_vtentry_howto;
    default:
      BFD_ASSERT (r_type < (unsigned int) R_MIPS_max);
      if (rela_p)
	return &mips_elf64_howto_table_rela[r_type];
      else
	return &mips_elf64_howto_table_rel[r_type];
      break;
    }
}

/* Prevent relocation handling by bfd for MIPS ELF64.  */

static void
mips_elf64_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr ATTRIBUTE_UNUSED;
     Elf64_Internal_Rel *dst ATTRIBUTE_UNUSED;
{
  BFD_ASSERT (0);
}

static void
mips_elf64_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr ATTRIBUTE_UNUSED;
     Elf64_Internal_Rela *dst ATTRIBUTE_UNUSED;
{
  BFD_ASSERT (0);
}

/* Since each entry in an SHT_REL or SHT_RELA section can represent up
   to three relocs, we must tell the user to allocate more space.  */

static long
mips_elf64_get_reloc_upper_bound (abfd, sec)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
{
  return (sec->reloc_count * 3 + 1) * sizeof (arelent *);
}

/* Read the relocations from one reloc section.  */

static boolean
mips_elf64_slurp_one_reloc_table (abfd, asect, symbols, rel_hdr)
     bfd *abfd;
     asection *asect;
     asymbol **symbols;
     const Elf_Internal_Shdr *rel_hdr;
{
  PTR allocated = NULL;
  bfd_byte *native_relocs;
  arelent *relents;
  arelent *relent;
  bfd_vma count;
  bfd_vma i;
  int entsize;
  reloc_howto_type *howto_table;

  allocated = (PTR) bfd_malloc (rel_hdr->sh_size);
  if (allocated == NULL)
    return false;

  if (bfd_seek (abfd, rel_hdr->sh_offset, SEEK_SET) != 0
      || (bfd_bread (allocated, rel_hdr->sh_size, abfd) != rel_hdr->sh_size))
    goto error_return;

  native_relocs = (bfd_byte *) allocated;

  relents = asect->relocation + asect->reloc_count;

  entsize = rel_hdr->sh_entsize;
  BFD_ASSERT (entsize == sizeof (Elf64_Mips_External_Rel)
	      || entsize == sizeof (Elf64_Mips_External_Rela));

  count = rel_hdr->sh_size / entsize;

  if (entsize == sizeof (Elf64_Mips_External_Rel))
    howto_table = mips_elf64_howto_table_rel;
  else
    howto_table = mips_elf64_howto_table_rela;

  relent = relents;
  for (i = 0; i < count; i++, native_relocs += entsize)
    {
      Elf64_Mips_Internal_Rela rela;
      boolean used_sym, used_ssym;
      int ir;

      if (entsize == sizeof (Elf64_Mips_External_Rela))
	mips_elf64_swap_reloca_in (abfd,
				   (Elf64_Mips_External_Rela *) native_relocs,
				   &rela);
      else
	{
	  Elf64_Mips_Internal_Rel rel;

	  mips_elf64_swap_reloc_in (abfd,
				    (Elf64_Mips_External_Rel *) native_relocs,
				    &rel);
	  rela.r_offset = rel.r_offset;
	  rela.r_sym = rel.r_sym;
	  rela.r_ssym = rel.r_ssym;
	  rela.r_type3 = rel.r_type3;
	  rela.r_type2 = rel.r_type2;
	  rela.r_type = rel.r_type;
	  rela.r_addend = 0;
	}

      /* Each entry represents exactly three actual relocations.  */

      used_sym = false;
      used_ssym = false;
      for (ir = 0; ir < 3; ir++)
	{
	  enum elf_mips_reloc_type type;

	  switch (ir)
	    {
	    default:
	      abort ();
	    case 0:
	      type = (enum elf_mips_reloc_type) rela.r_type;
	      break;
	    case 1:
	      type = (enum elf_mips_reloc_type) rela.r_type2;
	      break;
	    case 2:
	      type = (enum elf_mips_reloc_type) rela.r_type3;
	      break;
	    }

	  /* Some types require symbols, whereas some do not.  */
	  switch (type)
	    {
	    case R_MIPS_NONE:
	    case R_MIPS_LITERAL:
	    case R_MIPS_INSERT_A:
	    case R_MIPS_INSERT_B:
	    case R_MIPS_DELETE:
	      relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
	      break;

	    default:
	      if (! used_sym)
		{
		  if (rela.r_sym == 0)
		    relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
		  else
		    {
		      asymbol **ps, *s;

		      ps = symbols + rela.r_sym - 1;
		      s = *ps;
		      if ((s->flags & BSF_SECTION_SYM) == 0)
			relent->sym_ptr_ptr = ps;
		      else
			relent->sym_ptr_ptr = s->section->symbol_ptr_ptr;
		    }

		  used_sym = true;
		}
	      else if (! used_ssym)
		{
		  switch (rela.r_ssym)
		    {
		    case RSS_UNDEF:
		      relent->sym_ptr_ptr =
			bfd_abs_section_ptr->symbol_ptr_ptr;
		      break;

		    case RSS_GP:
		    case RSS_GP0:
		    case RSS_LOC:
		      /* FIXME: I think these need to be handled using
                         special howto structures.  */
		      BFD_ASSERT (0);
		      break;

		    default:
		      BFD_ASSERT (0);
		      break;
		    }

		  used_ssym = true;
		}
	      else
		relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;

	      break;
	    }

	  /* The address of an ELF reloc is section relative for an
	     object file, and absolute for an executable file or
	     shared library.  The address of a BFD reloc is always
	     section relative.  */
	  if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
	    relent->address = rela.r_offset;
	  else
	    relent->address = rela.r_offset - asect->vma;

	  relent->addend = rela.r_addend;

	  relent->howto = &howto_table[(int) type];

	  ++relent;
	}
    }

  asect->reloc_count += (relent - relents) / 3;

  if (allocated != NULL)
    free (allocated);

  return true;

 error_return:
  if (allocated != NULL)
    free (allocated);
  return false;
}

/* Read the relocations.  On Irix 6, there can be two reloc sections
   associated with a single data section.  */

static boolean
mips_elf64_slurp_reloc_table (abfd, asect, symbols, dynamic)
     bfd *abfd;
     asection *asect;
     asymbol **symbols;
     boolean dynamic;
{
  bfd_size_type amt;
  struct bfd_elf_section_data * const d = elf_section_data (asect);

  if (dynamic)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  if (asect->relocation != NULL
      || (asect->flags & SEC_RELOC) == 0
      || asect->reloc_count == 0)
    return true;

  /* Allocate space for 3 arelent structures for each Rel structure.  */
  amt = asect->reloc_count;
  amt *= 3 * sizeof (arelent);
  asect->relocation = (arelent *) bfd_alloc (abfd, amt);
  if (asect->relocation == NULL)
    return false;

  /* The slurp_one_reloc_table routine increments reloc_count.  */
  asect->reloc_count = 0;

  if (! mips_elf64_slurp_one_reloc_table (abfd, asect, symbols, &d->rel_hdr))
    return false;
  if (d->rel_hdr2 != NULL)
    {
      if (! mips_elf64_slurp_one_reloc_table (abfd, asect, symbols,
					      d->rel_hdr2))
	return false;
    }

  return true;
}

/* Write out the relocations.  */

static void
mips_elf64_write_relocs (abfd, sec, data)
     bfd *abfd;
     asection *sec;
     PTR data;
{
  boolean *failedp = (boolean *) data;
  int count;
  Elf_Internal_Shdr *rel_hdr;
  unsigned int idx;

  /* If we have already failed, don't do anything.  */
  if (*failedp)
    return;

  if ((sec->flags & SEC_RELOC) == 0)
    return;

  /* The linker backend writes the relocs out itself, and sets the
     reloc_count field to zero to inhibit writing them here.  Also,
     sometimes the SEC_RELOC flag gets set even when there aren't any
     relocs.  */
  if (sec->reloc_count == 0)
    return;

  /* We can combine up to three relocs that refer to the same address
     if the latter relocs have no associated symbol.  */
  count = 0;
  for (idx = 0; idx < sec->reloc_count; idx++)
    {
      bfd_vma addr;
      unsigned int i;

      ++count;

      addr = sec->orelocation[idx]->address;
      for (i = 0; i < 2; i++)
	{
	  arelent *r;

	  if (idx + 1 >= sec->reloc_count)
	    break;
	  r = sec->orelocation[idx + 1];
	  if (r->address != addr
	      || ! bfd_is_abs_section ((*r->sym_ptr_ptr)->section)
	      || (*r->sym_ptr_ptr)->value != 0)
	    break;

	  /* We can merge the reloc at IDX + 1 with the reloc at IDX.  */

	  ++idx;
	}
    }

  rel_hdr = &elf_section_data (sec)->rel_hdr;

  /* Do the actual relocation.  */

  if (rel_hdr->sh_entsize == sizeof(Elf64_Mips_External_Rel))
    mips_elf64_write_rel (abfd, sec, rel_hdr, &count, data);
  else if (rel_hdr->sh_entsize == sizeof(Elf64_Mips_External_Rela))
    mips_elf64_write_rela (abfd, sec, rel_hdr, &count, data);
  else
    BFD_ASSERT (0);
}

static void
mips_elf64_write_rel (abfd, sec, rel_hdr, count, data)
     bfd *abfd;
     asection *sec;
     Elf_Internal_Shdr *rel_hdr;
     int *count;
     PTR data;
{
  boolean *failedp = (boolean *) data;
  Elf64_Mips_External_Rel *ext_rel;
  unsigned int idx;
  asymbol *last_sym = 0;
  int last_sym_idx = 0;

  rel_hdr->sh_size = (bfd_vma)(rel_hdr->sh_entsize * *count);
  rel_hdr->contents = (PTR) bfd_alloc (abfd, rel_hdr->sh_size);
  if (rel_hdr->contents == NULL)
    {
      *failedp = true;
      return;
    }

  ext_rel = (Elf64_Mips_External_Rel *) rel_hdr->contents;
  for (idx = 0; idx < sec->reloc_count; idx++, ext_rel++)
    {
      arelent *ptr;
      Elf64_Mips_Internal_Rel int_rel;
      asymbol *sym;
      int n;
      unsigned int i;

      ptr = sec->orelocation[idx];

      /* The address of an ELF reloc is section relative for an object
	 file, and absolute for an executable file or shared library.
	 The address of a BFD reloc is always section relative.  */
      if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
	int_rel.r_offset = ptr->address;
      else
	int_rel.r_offset = ptr->address + sec->vma;

      sym = *ptr->sym_ptr_ptr;
      if (sym == last_sym)
	n = last_sym_idx;
      else
	{
	  last_sym = sym;
	  n = _bfd_elf_symbol_from_bfd_symbol (abfd, &sym);
	  if (n < 0)
	    {
	      *failedp = true;
	      return;
	    }
	  last_sym_idx = n;
	}

      int_rel.r_sym = n;
      int_rel.r_ssym = RSS_UNDEF;

      if ((*ptr->sym_ptr_ptr)->the_bfd->xvec != abfd->xvec
	  && ! _bfd_elf_validate_reloc (abfd, ptr))
	{
	  *failedp = true;
	  return;
	}

      int_rel.r_type = ptr->howto->type;
      int_rel.r_type2 = (int) R_MIPS_NONE;
      int_rel.r_type3 = (int) R_MIPS_NONE;

      for (i = 0; i < 2; i++)
	{
	  arelent *r;

	  if (idx + 1 >= sec->reloc_count)
	    break;
	  r = sec->orelocation[idx + 1];
	  if (r->address != ptr->address
	      || ! bfd_is_abs_section ((*r->sym_ptr_ptr)->section)
	      || (*r->sym_ptr_ptr)->value != 0)
	    break;

	  /* We can merge the reloc at IDX + 1 with the reloc at IDX.  */

	  if (i == 0)
	    int_rel.r_type2 = r->howto->type;
	  else
	    int_rel.r_type3 = r->howto->type;

	  ++idx;
	}

      mips_elf64_swap_reloc_out (abfd, &int_rel, ext_rel);
    }

  BFD_ASSERT (ext_rel - (Elf64_Mips_External_Rel *) rel_hdr->contents
	      == *count);
}

static void
mips_elf64_write_rela (abfd, sec, rela_hdr, count, data)
     bfd *abfd;
     asection *sec;
     Elf_Internal_Shdr *rela_hdr;
     int *count;
     PTR data;
{
  boolean *failedp = (boolean *) data;
  Elf64_Mips_External_Rela *ext_rela;
  unsigned int idx;
  asymbol *last_sym = 0;
  int last_sym_idx = 0;

  rela_hdr->sh_size = (bfd_vma)(rela_hdr->sh_entsize * *count);
  rela_hdr->contents = (PTR) bfd_alloc (abfd, rela_hdr->sh_size);
  if (rela_hdr->contents == NULL)
    {
      *failedp = true;
      return;
    }

  ext_rela = (Elf64_Mips_External_Rela *) rela_hdr->contents;
  for (idx = 0; idx < sec->reloc_count; idx++, ext_rela++)
    {
      arelent *ptr;
      Elf64_Mips_Internal_Rela int_rela;
      asymbol *sym;
      int n;
      unsigned int i;

      ptr = sec->orelocation[idx];

      /* The address of an ELF reloc is section relative for an object
	 file, and absolute for an executable file or shared library.
	 The address of a BFD reloc is always section relative.  */
      if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
	int_rela.r_offset = ptr->address;
      else
	int_rela.r_offset = ptr->address + sec->vma;

      sym = *ptr->sym_ptr_ptr;
      if (sym == last_sym)
	n = last_sym_idx;
      else
	{
	  last_sym = sym;
	  n = _bfd_elf_symbol_from_bfd_symbol (abfd, &sym);
	  if (n < 0)
	    {
	      *failedp = true;
	      return;
	    }
	  last_sym_idx = n;
	}

      int_rela.r_sym = n;
      int_rela.r_addend = ptr->addend;
      int_rela.r_ssym = RSS_UNDEF;

      if ((*ptr->sym_ptr_ptr)->the_bfd->xvec != abfd->xvec
	  && ! _bfd_elf_validate_reloc (abfd, ptr))
	{
	  *failedp = true;
	  return;
	}

      int_rela.r_type = ptr->howto->type;
      int_rela.r_type2 = (int) R_MIPS_NONE;
      int_rela.r_type3 = (int) R_MIPS_NONE;

      for (i = 0; i < 2; i++)
	{
	  arelent *r;

	  if (idx + 1 >= sec->reloc_count)
	    break;
	  r = sec->orelocation[idx + 1];
	  if (r->address != ptr->address
	      || ! bfd_is_abs_section ((*r->sym_ptr_ptr)->section)
	      || (*r->sym_ptr_ptr)->value != 0)
	    break;

	  /* We can merge the reloc at IDX + 1 with the reloc at IDX.  */

	  if (i == 0)
	    int_rela.r_type2 = r->howto->type;
	  else
	    int_rela.r_type3 = r->howto->type;

	  ++idx;
	}

      mips_elf64_swap_reloca_out (abfd, &int_rela, ext_rela);
    }

  BFD_ASSERT (ext_rela - (Elf64_Mips_External_Rela *) rela_hdr->contents
	      == *count);
}

/* Set the right machine number for a MIPS ELF file.  */

static boolean
mips_elf64_object_p (abfd)
     bfd *abfd;
{
  unsigned long mach;

  /* Irix 6 is broken.  Object file symbol tables are not always
     sorted correctly such that local symbols precede global symbols,
     and the sh_info field in the symbol table is not always right.  */
  if (elf64_mips_irix_compat (abfd) != ict_none)
    elf_bad_symtab (abfd) = true;

  mach = _bfd_elf_mips_mach (elf_elfheader (abfd)->e_flags);
  bfd_default_set_arch_mach (abfd, bfd_arch_mips, mach);
  return true;
}

/* Depending on the target vector we generate some version of Irix
   executables or "normal" MIPS ELF ABI executables.  */
static irix_compat_t
elf64_mips_irix_compat (abfd)
     bfd *abfd;
{
  if ((abfd->xvec == &bfd_elf64_bigmips_vec)
      || (abfd->xvec == &bfd_elf64_littlemips_vec))
    return ict_irix6;
  else
    return ict_none;
}

/* ECOFF swapping routines.  These are used when dealing with the
   .mdebug section, which is in the ECOFF debugging format.  */
static const struct ecoff_debug_swap mips_elf64_ecoff_debug_swap =
{
  /* Symbol table magic number.  */
  magicSym2,
  /* Alignment of debugging information.  E.g., 4.  */
  8,
  /* Sizes of external symbolic information.  */
  sizeof (struct hdr_ext),
  sizeof (struct dnr_ext),
  sizeof (struct pdr_ext),
  sizeof (struct sym_ext),
  sizeof (struct opt_ext),
  sizeof (struct fdr_ext),
  sizeof (struct rfd_ext),
  sizeof (struct ext_ext),
  /* Functions to swap in external symbolic data.  */
  ecoff_swap_hdr_in,
  ecoff_swap_dnr_in,
  ecoff_swap_pdr_in,
  ecoff_swap_sym_in,
  ecoff_swap_opt_in,
  ecoff_swap_fdr_in,
  ecoff_swap_rfd_in,
  ecoff_swap_ext_in,
  _bfd_ecoff_swap_tir_in,
  _bfd_ecoff_swap_rndx_in,
  /* Functions to swap out external symbolic data.  */
  ecoff_swap_hdr_out,
  ecoff_swap_dnr_out,
  ecoff_swap_pdr_out,
  ecoff_swap_sym_out,
  ecoff_swap_opt_out,
  ecoff_swap_fdr_out,
  ecoff_swap_rfd_out,
  ecoff_swap_ext_out,
  _bfd_ecoff_swap_tir_out,
  _bfd_ecoff_swap_rndx_out,
  /* Function to read in symbolic data.  */
  _bfd_mips_elf_read_ecoff_info
};

/* Relocations in the 64 bit MIPS ELF ABI are more complex than in
   standard ELF.  This structure is used to redirect the relocation
   handling routines.  */

const struct elf_size_info mips_elf64_size_info =
{
  sizeof (Elf64_External_Ehdr),
  sizeof (Elf64_External_Phdr),
  sizeof (Elf64_External_Shdr),
  sizeof (Elf64_Mips_External_Rel),
  sizeof (Elf64_Mips_External_Rela),
  sizeof (Elf64_External_Sym),
  sizeof (Elf64_External_Dyn),
  sizeof (Elf_External_Note),
  4,            /* hash-table entry size */
  3,            /* internal relocations per external relocations */
  64,		/* arch_size */
  8,		/* file_align */
  ELFCLASS64,
  EV_CURRENT,
  bfd_elf64_write_out_phdrs,
  bfd_elf64_write_shdrs_and_ehdr,
  mips_elf64_write_relocs,
  bfd_elf64_swap_symbol_in,
  bfd_elf64_swap_symbol_out,
  mips_elf64_slurp_reloc_table,
  bfd_elf64_slurp_symbol_table,
  bfd_elf64_swap_dyn_in,
  bfd_elf64_swap_dyn_out,
  mips_elf64_be_swap_reloc_in,
  mips_elf64_be_swap_reloc_out,
  mips_elf64_be_swap_reloca_in,
  mips_elf64_be_swap_reloca_out
};

#define ELF_ARCH			bfd_arch_mips
#define ELF_MACHINE_CODE		EM_MIPS

/* The SVR4 MIPS ABI says that this should be 0x10000, but Irix 5 uses
   a value of 0x1000, and we are compatible.
   FIXME: How does this affect NewABI?  */
#define ELF_MAXPAGESIZE			0x1000

#define elf_backend_collect		true
#define elf_backend_type_change_ok	true
#define elf_backend_can_gc_sections	true
#define elf_info_to_howto		mips_elf64_info_to_howto_rela
#define elf_info_to_howto_rel		mips_elf64_info_to_howto_rel
#define elf_backend_object_p		mips_elf64_object_p
#define elf_backend_symbol_processing	_bfd_mips_elf_symbol_processing
#define elf_backend_section_processing	_bfd_mips_elf_section_processing
#define elf_backend_section_from_shdr	_bfd_mips_elf_section_from_shdr
#define elf_backend_fake_sections	_bfd_mips_elf_fake_sections
#define elf_backend_section_from_bfd_section \
				_bfd_mips_elf_section_from_bfd_section
#define elf_backend_add_symbol_hook	_bfd_mips_elf_add_symbol_hook
#define elf_backend_link_output_symbol_hook \
				_bfd_mips_elf_link_output_symbol_hook
#define elf_backend_create_dynamic_sections \
				_bfd_mips_elf_create_dynamic_sections
#define elf_backend_check_relocs	_bfd_mips_elf_check_relocs
#define elf_backend_adjust_dynamic_symbol \
				_bfd_mips_elf_adjust_dynamic_symbol
#define elf_backend_always_size_sections \
				_bfd_mips_elf_always_size_sections
#define elf_backend_size_dynamic_sections \
				_bfd_mips_elf_size_dynamic_sections
#define elf_backend_relocate_section    _bfd_mips_elf_relocate_section
#define elf_backend_finish_dynamic_symbol \
				_bfd_mips_elf_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
				_bfd_mips_elf_finish_dynamic_sections
#define elf_backend_final_write_processing \
				_bfd_mips_elf_final_write_processing
#define elf_backend_additional_program_headers \
				_bfd_mips_elf_additional_program_headers
#define elf_backend_modify_segment_map	_bfd_mips_elf_modify_segment_map
#define elf_backend_gc_mark_hook	_bfd_mips_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook	_bfd_mips_elf_gc_sweep_hook
#define elf_backend_hide_symbol		_bfd_mips_elf_hide_symbol
#define elf_backend_ignore_discarded_relocs \
					_bfd_mips_elf_ignore_discarded_relocs
#define elf_backend_mips_irix_compat	elf64_mips_irix_compat
#define elf_backend_mips_rtype_to_howto	mips_elf64_rtype_to_howto
#define elf_backend_ecoff_debug_swap	&mips_elf64_ecoff_debug_swap
#define elf_backend_size_info		mips_elf64_size_info

#define elf_backend_got_header_size	(4 * MIPS_RESERVED_GOTNO)
#define elf_backend_plt_header_size	0

/* MIPS ELF64 can use a mixture of REL and RELA, but some Relocations
   work better/work only in RELA, so we default to this.  */
#define elf_backend_may_use_rel_p	1
#define elf_backend_may_use_rela_p	1
#define elf_backend_default_use_rela_p	1

#define elf_backend_write_section	_bfd_mips_elf_write_section

/* We don't set bfd_elf64_bfd_is_local_label_name because the 32-bit
   MIPS-specific function only applies to IRIX5, which had no 64-bit
   ABI.  */
#define bfd_elf64_find_nearest_line	_bfd_mips_elf_find_nearest_line
#define bfd_elf64_set_section_contents	_bfd_mips_elf_set_section_contents
#define bfd_elf64_bfd_get_relocated_section_contents \
				_bfd_elf_mips_get_relocated_section_contents
#define bfd_elf64_bfd_link_hash_table_create \
				_bfd_mips_elf_link_hash_table_create
#define bfd_elf64_bfd_final_link	_bfd_mips_elf_final_link
#define bfd_elf64_bfd_merge_private_bfd_data \
				_bfd_mips_elf_merge_private_bfd_data
#define bfd_elf64_bfd_set_private_flags	_bfd_mips_elf_set_private_flags
#define bfd_elf64_bfd_print_private_bfd_data \
				_bfd_mips_elf_print_private_bfd_data

#define bfd_elf64_get_reloc_upper_bound mips_elf64_get_reloc_upper_bound

/* MIPS ELF64 archive functions.  */
#define bfd_elf64_archive_functions
extern boolean bfd_elf64_archive_slurp_armap
  PARAMS((bfd *));
extern boolean bfd_elf64_archive_write_armap
  PARAMS((bfd *, unsigned int, struct orl *, unsigned int, int));
#define bfd_elf64_archive_slurp_extended_name_table \
			_bfd_archive_coff_slurp_extended_name_table
#define bfd_elf64_archive_construct_extended_name_table \
			_bfd_archive_coff_construct_extended_name_table
#define bfd_elf64_archive_truncate_arname \
			_bfd_archive_coff_truncate_arname
#define bfd_elf64_archive_read_ar_hdr	_bfd_archive_coff_read_ar_hdr
#define bfd_elf64_archive_openr_next_archived_file \
			_bfd_archive_coff_openr_next_archived_file
#define bfd_elf64_archive_get_elt_at_index \
			_bfd_archive_coff_get_elt_at_index
#define bfd_elf64_archive_generic_stat_arch_elt \
			_bfd_archive_coff_generic_stat_arch_elt
#define bfd_elf64_archive_update_armap_timestamp \
			_bfd_archive_coff_update_armap_timestamp

/* The SGI style (n)64 NewABI.  */
#define TARGET_LITTLE_SYM		bfd_elf64_littlemips_vec
#define TARGET_LITTLE_NAME		"elf64-littlemips"
#define TARGET_BIG_SYM			bfd_elf64_bigmips_vec
#define TARGET_BIG_NAME			"elf64-bigmips"

#include "elf64-target.h"

#define INCLUDED_TARGET_FILE            /* More a type of flag.  */

/* The SYSV-style 'traditional' (n)64 NewABI.  */
#undef TARGET_LITTLE_SYM
#undef TARGET_LITTLE_NAME
#undef TARGET_BIG_SYM
#undef TARGET_BIG_NAME

#define TARGET_LITTLE_SYM               bfd_elf64_tradlittlemips_vec
#define TARGET_LITTLE_NAME              "elf64-tradlittlemips"
#define TARGET_BIG_SYM                  bfd_elf64_tradbigmips_vec
#define TARGET_BIG_NAME                 "elf64-tradbigmips"

/* Include the target file again for this target.  */
#include "elf64-target.h"
