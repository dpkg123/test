/* MIPS-specific support for 32-bit ELF
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Most of the information added by Ian Lance Taylor, Cygnus Support,
   <ian@cygnus.com>.
   N32/64 ABI support added by Mark Mitchell, CodeSourcery, LLC.
   <mark@codesourcery.com>
   Traditional MIPS targets support added by Koundinya.K, Dansk Data
   Elektronik & Operations Research Group. <kk@ddeorg.soft.net>

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

/* This file handles MIPS ELF targets.  SGI Irix 5 uses a slightly
   different MIPS ELF from other targets.  This matters when linking.
   This file supports both, switching at runtime.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "genlink.h"
#include "elf-bfd.h"
#include "elfxx-mips.h"
#include "elf/mips.h"

/* Get the ECOFF swapping routines.  */
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/internal.h"
#include "coff/ecoff.h"
#include "coff/mips.h"
#define ECOFF_SIGNED_32
#include "ecoffswap.h"

static bfd_reloc_status_type mips_elf_generic_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf_hi16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf_lo16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf_got16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static boolean mips_elf_assign_gp PARAMS ((bfd *, bfd_vma *));
static bfd_reloc_status_type mips_elf_final_gp
  PARAMS ((bfd *, asymbol *, boolean, char **, bfd_vma *));
static bfd_reloc_status_type mips_elf_gprel16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf_literal_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf_gprel32_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type gprel32_with_gp
  PARAMS ((bfd *, asymbol *, arelent *, asection *, boolean, PTR, bfd_vma));
static bfd_reloc_status_type mips_elf_shift6_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips16_jump_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips16_gprel_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static reloc_howto_type *mips_elf_n32_rtype_to_howto
  PARAMS ((unsigned int, boolean));
static void mips_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rel *));
static void mips_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rela *));
static boolean mips_elf_sym_is_global PARAMS ((bfd *, asymbol *));
static boolean mips_elf_n32_object_p PARAMS ((bfd *));
static boolean elf32_mips_grok_prstatus
  PARAMS ((bfd *, Elf_Internal_Note *));
static boolean elf32_mips_grok_psinfo
  PARAMS ((bfd *, Elf_Internal_Note *));
static irix_compat_t elf_n32_mips_irix_compat
  PARAMS ((bfd *));

extern const bfd_target bfd_elf32_nbigmips_vec;
extern const bfd_target bfd_elf32_nlittlemips_vec;

static bfd_vma prev_reloc_address = -1;
static bfd_vma prev_reloc_addend = 0;

/* Nonzero if ABFD is using the N32 ABI.  */
#define ABI_N32_P(abfd) \
  ((elf_elfheader (abfd)->e_flags & EF_MIPS_ABI2) != 0)

/* Whether we are trying to be compatible with IRIX at all.  */
#define SGI_COMPAT(abfd) \
  (elf_n32_mips_irix_compat (abfd) != ict_none)

/* The number of local .got entries we reserve.  */
#define MIPS_RESERVED_GOTNO (2)

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

/* The relocation table used for SHT_REL sections.  */

static reloc_howto_type elf_mips_howto_table_rel[] =
{
  /* No relocation.  */
  HOWTO (R_MIPS_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
				   detection, because the upper four
				   bits must match the PC + 4.  */
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_hi16_reloc,	/* special_function */
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
	 mips_elf_lo16_reloc,	/* special_function */
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
	 mips_elf_gprel16_reloc, /* special_function */
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
	 mips_elf_literal_reloc, /* special_function */
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
	 mips_elf_got16_reloc,	/* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_gprel32_reloc, /* special_function */
	 "R_MIPS_GPREL32",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The remaining relocs are defined on Irix 5, although they are
     not defined by the ABI.  */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_shift6_reloc,	/* special_function */
	 "R_MIPS_SHIFT6",	/* name */
	 true,			/* partial_inplace */
	 0x000007c4,		/* src_mask */
	 0x000007c4,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 64 bit relocation.  */
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
	 "R_MIPS_GOT_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit subtraction.  */
  HOWTO (R_MIPS_SUB,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
       b) No other NEwABI toolchain actually emits such relocations.  */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
	 "R_MIPS_CALL_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Section displacement.  */
  HOWTO (R_MIPS_SCN_DISP,       /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf_generic_reloc, /* special_function */
	 "R_MIPS_SCN_DISP",     /* name */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
	 "R_MIPS_JALR",	        /* name */
	 false,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x00000000,		/* dst_mask */
	 false),		/* pcrel_offset */
};

/* The relocation table used for SHT_RELA sections.  */

static reloc_howto_type elf_mips_howto_table_rela[] =
{
  /* No relocation.  */
  HOWTO (R_MIPS_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
	 "R_MIPS_16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit relocation.  */
  HOWTO (R_MIPS_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_gprel16_reloc, /* special_function */
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
	 mips_elf_literal_reloc, /* special_function */
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
	 mips_elf_got16_reloc,	/* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_gprel32_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_shift6_reloc,	/* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
	 "R_MIPS_SCN_DISP",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit relocation.  */
  HOWTO (R_MIPS_REL16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
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
	 mips_elf_generic_reloc, /* special_function */
	 "R_MIPS_JALR",	        /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
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

/* This is derived from bfd_elf_generic_reloc.  NewABI allows us to have
   several relocations against the same address.  The addend is derived
   from the addends of preceding relocations.  If we don't need to
   do something special,  we simply keep track of the addend.  */

#define GET_RELOC_ADDEND(obfd, sym, entry, sec)				\
{									\
  /* If we're relocating, and this is an external symbol, we don't	\
     want to change anything.  */					\
    if (obfd != (bfd *) NULL						\
	&& (sym->flags & BSF_SECTION_SYM) == 0				\
	&& (! entry->howto->partial_inplace				\
	    || entry->addend == 0))					\
      {									\
        entry->address += sec->output_offset;				\
        return bfd_reloc_ok;						\
      }									\
									\
    /* The addend of combined relocs is remembered and left for		\
       subsequent relocs.  */						\
    if (prev_reloc_address != reloc_entry->address)			\
      {									\
        prev_reloc_address = reloc_entry->address;			\
        prev_reloc_addend = reloc_entry->addend;			\
      }									\
    else								\
      reloc_entry->addend = prev_reloc_addend;				\
}

#define SET_RELOC_ADDEND(entry)						\
{									\
  prev_reloc_addend = entry->addend;					\
}

static bfd_reloc_status_type
mips_elf_generic_reloc (abfd, reloc_entry, symbol, data, input_section,
			output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  GET_RELOC_ADDEND (output_bfd, symbol, reloc_entry, input_section)

  return bfd_reloc_continue;
}

/* Do a R_MIPS_HI16 relocation.  This has to be done in combination
   with a R_MIPS_LO16 reloc, because there is a carry from the LO16 to
   the HI16.  Here we just save the information we need; we do the
   actual relocation when we see the LO16.

   MIPS ELF requires that the LO16 immediately follow the HI16.  As a
   GNU extension, for non-pc-relative relocations, we permit an
   arbitrary number of HI16 relocs to be associated with a single LO16
   reloc.  This extension permits gcc to output the HI and LO relocs
   itself.

   This cannot be done for PC-relative relocations because both the HI16
   and LO16 parts of the relocations must be done relative to the LO16
   part, and there can be carry to or borrow from the HI16 part.  */

struct mips_hi16
{
  struct mips_hi16 *next;
  bfd_byte *addr;
  bfd_vma addend;
};

/* FIXME: This should not be a static variable.  */

static struct mips_hi16 *mips_hi16_list;

static bfd_reloc_status_type
mips_elf_hi16_reloc (abfd, reloc_entry, symbol, data, input_section,
		     output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_reloc_status_type ret;
  bfd_vma relocation;
  struct mips_hi16 *n;

  GET_RELOC_ADDEND (output_bfd, symbol, reloc_entry, input_section)

  ret = bfd_reloc_ok;

  if (bfd_is_und_section (symbol->section) && output_bfd == (bfd *) NULL)
    ret = bfd_reloc_undefined;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* Save the information, and let LO16 do the actual relocation.  */
  n = (struct mips_hi16 *) bfd_malloc ((bfd_size_type) sizeof *n);
  if (n == NULL)
    return bfd_reloc_outofrange;
  n->addr = (bfd_byte *) data + reloc_entry->address;
  n->addend = relocation;
  n->next = mips_hi16_list;
  mips_hi16_list = n;

  if (output_bfd != (bfd *) NULL)
    reloc_entry->address += input_section->output_offset;

  return ret;
}

/* Do a R_MIPS_LO16 relocation.  This is a straightforward 16 bit
   inplace relocation; this function exists in order to do the
   R_MIPS_HI16 relocation described above.  */

static bfd_reloc_status_type
mips_elf_lo16_reloc (abfd, reloc_entry, symbol, data, input_section,
		     output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  if (mips_hi16_list != NULL)
    {
      struct mips_hi16 *l;

      l = mips_hi16_list;
      while (l != NULL)
	{
	  unsigned long insn;
	  unsigned long val;
	  unsigned long vallo;
	  struct mips_hi16 *next;

	  /* Do the HI16 relocation.  Note that we actually don't need
	     to know anything about the LO16 itself, except where to
	     find the low 16 bits of the addend needed by the LO16.  */
	  insn = bfd_get_32 (abfd, l->addr);
	  vallo = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

	  /* The low order 16 bits are always treated as a signed
	     value.  */
	  vallo = ((vallo & 0xffff) ^ 0x8000) - 0x8000;
	  val = ((insn & 0xffff) << 16) + vallo;
	  val += l->addend;

	  /* If PC-relative, we need to subtract out the address of the LO
	     half of the HI/LO.  (The actual relocation is relative
	     to that instruction.)  */
	  if (reloc_entry->howto->pc_relative)
	    val -= reloc_entry->address;

	  /* At this point, "val" has the value of the combined HI/LO
	     pair.  If the low order 16 bits (which will be used for
	     the LO16 insn) are negative, then we will need an
	     adjustment for the high order 16 bits.  */
	  val += 0x8000;
	  val = (val >> 16) & 0xffff;

	  insn &= ~ (bfd_vma) 0xffff;
	  insn |= val;
	  bfd_put_32 (abfd, (bfd_vma) insn, l->addr);

	  next = l->next;
	  free (l);
	  l = next;
	}

      mips_hi16_list = NULL;
    }

  /* Now do the LO16 reloc in the usual way.  */
  return mips_elf_generic_reloc (abfd, reloc_entry, symbol, data,
				 input_section, output_bfd, error_message);
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
mips_elf_got16_reloc (abfd, reloc_entry, symbol, data, input_section,
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
     just like HI16.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) != 0)
    return mips_elf_hi16_reloc (abfd, reloc_entry, symbol, data,
				input_section, output_bfd, error_message);

  /* Otherwise we try to handle it as R_MIPS_GOT_DISP.  */
  return mips_elf_generic_reloc (abfd, reloc_entry, symbol, data,
				 input_section, output_bfd, error_message);
}

/* Set the GP value for OUTPUT_BFD.  Returns false if this is a
   dangerous relocation.  */

static boolean
mips_elf_assign_gp (output_bfd, pgp)
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
mips_elf_final_gp (output_bfd, symbol, relocateable, error_message, pgp)
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
      else if (!mips_elf_assign_gp (output_bfd, pgp))
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
mips_elf_gprel16_reloc (abfd, reloc_entry, symbol, data, input_section,
			output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;

  GET_RELOC_ADDEND (output_bfd, symbol, reloc_entry, input_section)

  if (output_bfd != (bfd *) NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  ret = mips_elf_final_gp (output_bfd, symbol, relocateable, error_message,
			   &gp);
  if (ret != bfd_reloc_ok)
    return ret;

  return _bfd_mips_elf_gprel16_with_gp (abfd, symbol, reloc_entry,
					input_section, relocateable,
					data, gp);
}

/* Do a R_MIPS_LITERAL relocation.  */

static bfd_reloc_status_type
mips_elf_literal_reloc (abfd, reloc_entry, symbol, data, input_section,
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

  GET_RELOC_ADDEND (output_bfd, symbol, reloc_entry, input_section)

  /* FIXME: The entries in the .lit8 and .lit4 sections should be merged.  */
  if (output_bfd != (bfd *) NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  ret = mips_elf_final_gp (output_bfd, symbol, relocateable, error_message,
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
mips_elf_gprel32_reloc (abfd, reloc_entry, symbol, data, input_section,
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

  GET_RELOC_ADDEND (output_bfd, symbol, reloc_entry, input_section)

  /* R_MIPS_GPREL32 relocations are defined for local symbols only.
     We will only have an addend if this is a newly created reloc,
     not read from an ELF file.  */
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

      ret = mips_elf_final_gp (output_bfd, symbol, relocateable,
			       error_message, &gp);
      if (ret != bfd_reloc_ok)
	return ret;
    }

  return gprel32_with_gp (abfd, symbol, reloc_entry, input_section,
			  relocateable, data, gp);
}

static bfd_reloc_status_type
gprel32_with_gp (abfd, symbol, reloc_entry, input_section, relocateable, data,
		 gp)
     bfd *abfd;
     asymbol *symbol;
     arelent *reloc_entry;
     asection *input_section;
     boolean relocateable;
     PTR data;
     bfd_vma gp;
{
  bfd_vma relocation;
  unsigned long val;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  if (reloc_entry->howto->src_mask == 0)
    val = 0;
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

  bfd_put_32 (abfd, (bfd_vma) val, (bfd_byte *) data + reloc_entry->address);

  if (relocateable)
    reloc_entry->address += input_section->output_offset;

  return bfd_reloc_ok;
}

/* Do a R_MIPS_SHIFT6 relocation. The MSB of the shift is stored at bit 2,
   the rest is at bits 6-10. The bitpos already got right by the howto.  */

static bfd_reloc_status_type
mips_elf_shift6_reloc (abfd, reloc_entry, symbol, data, input_section,
		       output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  GET_RELOC_ADDEND (output_bfd, symbol, reloc_entry, input_section)

  reloc_entry->addend = (reloc_entry->addend & 0x00007c0)
			| (reloc_entry->addend & 0x00000800) >> 9;

  SET_RELOC_ADDEND (reloc_entry)

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
  static boolean warned = false;

  GET_RELOC_ADDEND (output_bfd, symbol, reloc_entry, input_section)

  /* FIXME.  */
  if (! warned)
    (*_bfd_error_handler)
      (_("Linking mips16 objects into %s format is not supported"),
       bfd_get_target (input_section->output_section->owner));
  warned = true;

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

  GET_RELOC_ADDEND (output_bfd, symbol, reloc_entry, input_section)

  if (output_bfd != NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  ret = mips_elf_final_gp (output_bfd, symbol, relocateable, error_message,
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

#undef GET_RELOC_ADDEND
#undef SET_RELOC_ADDEND

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
  { BFD_RELOC_CTOR, R_MIPS_32 },
  { BFD_RELOC_64, R_MIPS_64 },
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
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;
  /* FIXME: We default to RELA here instead of choosing the right
     relocation variant.  */
  reloc_howto_type *howto_table = elf_mips_howto_table_rela;

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

/* Given a MIPS Elf32_Internal_Rel, fill in an arelent structure.  */

static reloc_howto_type *
mips_elf_n32_rtype_to_howto (r_type, rela_p)
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
	return &elf_mips_howto_table_rela[r_type];
      else
	return &elf_mips_howto_table_rel[r_type];
      break;
    }
}

/* Given a MIPS Elf32_Internal_Rel, fill in an arelent structure.  */

static void
mips_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf32_Internal_Rel *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  cache_ptr->howto = mips_elf_n32_rtype_to_howto (r_type, false);

  /* The addend for a GPREL16 or LITERAL relocation comes from the GP
     value for the object file.  We get the addend now, rather than
     when we do the relocation, because the symbol manipulations done
     by the linker may cause us to lose track of the input BFD.  */
  if (((*cache_ptr->sym_ptr_ptr)->flags & BSF_SECTION_SYM) != 0
      && (r_type == (unsigned int) R_MIPS_GPREL16
	  || r_type == (unsigned int) R_MIPS_LITERAL))
    cache_ptr->addend = elf_gp (abfd);
}

/* Given a MIPS Elf32_Internal_Rela, fill in an arelent structure.  */

static void
mips_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf32_Internal_Rela *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  cache_ptr->howto = mips_elf_n32_rtype_to_howto (r_type, true);
  cache_ptr->addend = dst->r_addend;
}

/* Determine whether a symbol is global for the purposes of splitting
   the symbol table into global symbols and local symbols.  At least
   on Irix 5, this split must be between section symbols and all other
   symbols.  On most ELF targets the split is between static symbols
   and externally visible symbols.  */

static boolean
mips_elf_sym_is_global (abfd, sym)
     bfd *abfd ATTRIBUTE_UNUSED;
     asymbol *sym;
{
  if (SGI_COMPAT (abfd))
    return (sym->flags & BSF_SECTION_SYM) == 0;
  else
    return ((sym->flags & (BSF_GLOBAL | BSF_WEAK)) != 0
	    || bfd_is_und_section (bfd_get_section (sym))
	    || bfd_is_com_section (bfd_get_section (sym)));
}

/* Set the right machine number for a MIPS ELF file.  */

static boolean
mips_elf_n32_object_p (abfd)
     bfd *abfd;
{
  unsigned long mach;

  /* Irix 5 and 6 are broken.  Object file symbol tables are not always
     sorted correctly such that local symbols precede global symbols,
     and the sh_info field in the symbol table is not always right.  */
  if (SGI_COMPAT (abfd))
    elf_bad_symtab (abfd) = true;

  mach = _bfd_elf_mips_mach (elf_elfheader (abfd)->e_flags);
  bfd_default_set_arch_mach (abfd, bfd_arch_mips, mach);

  if (! ABI_N32_P(abfd))
    return false;

  return true;
}

/* Support for core dump NOTE sections.  */
static boolean
elf32_mips_grok_prstatus (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  int offset;
  unsigned int raw_size;

  switch (note->descsz)
    {
      default:
	return false;

      case 256:		/* Linux/MIPS */
	/* pr_cursig */
	elf_tdata (abfd)->core_signal = bfd_get_16 (abfd, note->descdata + 12);

	/* pr_pid */
	elf_tdata (abfd)->core_pid = bfd_get_32 (abfd, note->descdata + 24);

	/* pr_reg */
	offset = 72;
	raw_size = 180;

	break;
    }

  /* Make a ".reg/999" section.  */
  return _bfd_elfcore_make_pseudosection (abfd, ".reg", raw_size,
					  note->descpos + offset);
}

static boolean
elf32_mips_grok_psinfo (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  switch (note->descsz)
    {
      default:
	return false;

      case 128:		/* Linux/MIPS elf_prpsinfo */
	elf_tdata (abfd)->core_program
	 = _bfd_elfcore_strndup (abfd, note->descdata + 32, 16);
	elf_tdata (abfd)->core_command
	 = _bfd_elfcore_strndup (abfd, note->descdata + 48, 80);
    }

  /* Note that for some reason, a spurious space is tacked
     onto the end of the args in some (at least one anyway)
     implementations, so strip it off if it exists.  */

  {
    char *command = elf_tdata (abfd)->core_command;
    int n = strlen (command);

    if (0 < n && command[n - 1] == ' ')
      command[n - 1] = '\0';
  }

  return true;
}

/* Depending on the target vector we generate some version of Irix
   executables or "normal" MIPS ELF ABI executables.  */
static irix_compat_t
elf_n32_mips_irix_compat (abfd)
     bfd *abfd;
{
  if ((abfd->xvec == &bfd_elf32_nbigmips_vec)
      || (abfd->xvec == &bfd_elf32_nlittlemips_vec))
    return ict_irix6;
  else
    return ict_none;
}

/* ECOFF swapping routines.  These are used when dealing with the
   .mdebug section, which is in the ECOFF debugging format.  */
static const struct ecoff_debug_swap mips_elf32_ecoff_debug_swap = {
  /* Symbol table magic number.  */
  magicSym,
  /* Alignment of debugging information.  E.g., 4.  */
  4,
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

#define ELF_ARCH			bfd_arch_mips
#define ELF_MACHINE_CODE		EM_MIPS

/* The SVR4 MIPS ABI says that this should be 0x10000, but Irix 5 uses
   a value of 0x1000, and we are compatible.
   FIXME: How does this affect NewABI?  */
#define ELF_MAXPAGESIZE			0x1000

#define elf_backend_collect		true
#define elf_backend_type_change_ok	true
#define elf_backend_can_gc_sections	true
#define elf_info_to_howto		mips_info_to_howto_rela
#define elf_info_to_howto_rel		mips_info_to_howto_rel
#define elf_backend_sym_is_global	mips_elf_sym_is_global
#define elf_backend_object_p		mips_elf_n32_object_p
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
#define elf_backend_relocate_section	_bfd_mips_elf_relocate_section
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
#define elf_backend_copy_indirect_symbol \
					_bfd_mips_elf_copy_indirect_symbol
#define elf_backend_hide_symbol		_bfd_mips_elf_hide_symbol
#define elf_backend_grok_prstatus	elf32_mips_grok_prstatus
#define elf_backend_grok_psinfo		elf32_mips_grok_psinfo
#define elf_backend_ecoff_debug_swap	&mips_elf32_ecoff_debug_swap

#define elf_backend_got_header_size	(4 * MIPS_RESERVED_GOTNO)
#define elf_backend_plt_header_size	0

/* MIPS n32 ELF can use a mixture of REL and RELA, but some Relocations
   work better/work only in RELA, so we default to this.  */
#define elf_backend_may_use_rel_p	1
#define elf_backend_may_use_rela_p	1
#define elf_backend_default_use_rela_p	1
#define elf_backend_sign_extend_vma	true

#define elf_backend_discard_info	_bfd_mips_elf_discard_info
#define elf_backend_ignore_discarded_relocs \
					_bfd_mips_elf_ignore_discarded_relocs
#define elf_backend_write_section	_bfd_mips_elf_write_section
#define elf_backend_mips_irix_compat	elf_n32_mips_irix_compat
#define elf_backend_mips_rtype_to_howto	mips_elf_n32_rtype_to_howto
#define bfd_elf32_find_nearest_line	_bfd_mips_elf_find_nearest_line
#define bfd_elf32_set_section_contents	_bfd_mips_elf_set_section_contents
#define bfd_elf32_bfd_get_relocated_section_contents \
				_bfd_elf_mips_get_relocated_section_contents
#define bfd_elf32_bfd_link_hash_table_create \
					_bfd_mips_elf_link_hash_table_create
#define bfd_elf32_bfd_final_link	_bfd_mips_elf_final_link
#define bfd_elf32_bfd_merge_private_bfd_data \
					_bfd_mips_elf_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags	_bfd_mips_elf_set_private_flags
#define bfd_elf32_bfd_print_private_bfd_data \
					_bfd_mips_elf_print_private_bfd_data

/* Support for SGI-ish mips targets using n32 ABI.  */

#define TARGET_LITTLE_SYM               bfd_elf32_nlittlemips_vec
#define TARGET_LITTLE_NAME              "elf32-nlittlemips"
#define TARGET_BIG_SYM                  bfd_elf32_nbigmips_vec
#define TARGET_BIG_NAME                 "elf32-nbigmips"

#include "elf32-target.h"

/* Support for traditional mips targets using n32 ABI.  */
#define INCLUDED_TARGET_FILE            /* More a type of flag.  */

#undef TARGET_LITTLE_SYM
#undef TARGET_LITTLE_NAME
#undef TARGET_BIG_SYM
#undef TARGET_BIG_NAME

#define TARGET_LITTLE_SYM               bfd_elf32_ntradlittlemips_vec
#define TARGET_LITTLE_NAME              "elf32-ntradlittlemips"
#define TARGET_BIG_SYM                  bfd_elf32_ntradbigmips_vec
#define TARGET_BIG_NAME                 "elf32-ntradbigmips"

/* Include the target file again for this target.  */
#include "elf32-target.h"
