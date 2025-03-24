/* BFD back-end for Alpha COFF files.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995 Free Software Foundation, Inc.
   Written by Cygnus Support.

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
#include "libbfd.h"
#include "obstack.h"

#include "coff/nt_alpha.h"

#include "coff/internal.h"

#ifdef COFF_WITH_PE
#include "coff/pe.h"
#endif

#include "libcoff.h"

/* A type to hold an instruction */
typedef unsigned long insn_word;

static reloc_howto_type *coff_nt_alpha_rtype_to_howto
  PARAMS ((bfd *, asection *, struct internal_reloc *,
	   struct coff_link_hash_entry *, struct internal_syment *,
	   bfd_vma *));

static boolean alpha_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	     struct internal_reloc *, struct internal_syment *, asection **));

static void alpha_find_stubs 
  PARAMS((bfd *, struct bfd_link_info *, asection *));

static boolean alpha_emit_stubs 
  PARAMS((bfd *, struct bfd_link_info *, asection *, struct bfd_link_order *));

/* How to process the various reloc types.  */

static reloc_howto_type *alpha_bfd_reloc_type_lookup
 PARAMS ((bfd *, bfd_reloc_code_real_type));

static bfd_reloc_status_type
reloc_nil PARAMS ((bfd *, arelent *, asymbol *, PTR,
		   asection *, bfd *, char **));

static bfd_reloc_status_type 
   alpha_inline_reloc PARAMS ((bfd *abfd, arelent *reloc,
			       asymbol *symbol, PTR data,
			       asection *section, bfd *output_bfd,
			       char **error));

static bfd_reloc_status_type 
   alpha_refhi_reloc PARAMS ((bfd *abfd, arelent *reloc,
			       asymbol *symbol, PTR data,
			       asection *section, bfd *output_bfd,
			       char **error));

static bfd_reloc_status_type 
   alpha_reflo_reloc PARAMS ((bfd *abfd, arelent *reloc,
			       asymbol *symbol, PTR data,
			       asection *section, bfd *output_bfd,
			       char **error));

static bfd_reloc_status_type
   alpha_pair_reloc PARAMS ((bfd *abfd, arelent *reloc,
			       asymbol *symbol, PTR data,
			       asection *section, bfd *output_bfd,
			       char **error));


static bfd_reloc_status_type 
   alpha_match_reloc PARAMS ((bfd *abfd, arelent *reloc,
			       asymbol *symbol, PTR data,
			       asection *section, bfd *output_bfd,
			       char **error));
static bfd_reloc_status_type 
   alpha_section_reloc PARAMS ((bfd *abfd, arelent *reloc,
			       asymbol *symbol, PTR data,
			       asection *section, bfd *output_bfd,
			       char **error));
static bfd_reloc_status_type 
   alpha_secrel_reloc PARAMS ((bfd *abfd, arelent *reloc,
			       asymbol *symbol, PTR data,
			       asection *section, bfd *output_bfd,
			       char **error));
static bfd_reloc_status_type 
   alpha_longnb_reloc PARAMS ((bfd *abfd, arelent *reloc,
			       asymbol *symbol, PTR data,
			       asection *section, bfd *output_bfd,
			       char **error));

static bfd_reloc_status_type
reloc_nil (abfd, reloc, sym, data, sec, output_bfd, error_message)
     bfd *abfd;
     arelent *reloc;
     asymbol *sym;
     PTR data;
     asection *sec;
     bfd *output_bfd;
     char **error_message;
{
  return bfd_reloc_ok;
}

/* This limits the alignment of common to this power; 8 byte alignment is
   what's "good" on Alpha (would 4 be better, or does it try to cache
   align commons at all.) */
#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (3)

/* This is a somewhat educated guess; for the linker it wants to 
   be 0x2000 aligned before it will work, but the MS tools seem
   happy with 0x1000... why???? */
#define COFF_PAGE_SIZE 0x2000

#if 0 /* unused, but for reference */
/* For some reason when using nt_alpha COFF the value stored in the .text
   section for a reference to a common symbol is the value itself plus
   any desired offset.  Ian Taylor, Cygnus Support.  */

/* If we are producing relocateable output, we need to do some
   adjustments to the object file that are not done by the
   bfd_perform_relocation function.  This function is called by every
   reloc type to make any required adjustments.  */

static bfd_reloc_status_type coff_nt_alpha_reloc 
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

static bfd_reloc_status_type
coff_nt_alpha_reloc (abfd, reloc_entry, symbol, data, input_section, output_bfd,
		 error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  symvalue diff;

  if (output_bfd == (bfd *) NULL)
    return bfd_reloc_continue;


  if (bfd_is_com_section (symbol->section))
    {
      /* We are relocating a common symbol.  The current value in the
	 object file is ORIG + OFFSET, where ORIG is the value of the
	 common symbol as seen by the object file when it was compiled
	 (this may be zero if the symbol was undefined) and OFFSET is
	 the offset into the common symbol (normally zero, but may be
	 non-zero when referring to a field in a common structure).
	 ORIG is the negative of reloc_entry->addend, which is set by
	 the CALC_ADDEND macro below.  We want to replace the value in
	 the object file with NEW + OFFSET, where NEW is the value of
	 the common symbol which we are going to put in the final
	 object file.  NEW is symbol->value.  */
      diff = symbol->value + reloc_entry->addend;
    }
  else
    {
      /* For some reason bfd_perform_relocation always effectively
	 ignores the addend for a COFF target when producing
	 relocateable output.  This seems to be always wrong for 386
	 COFF, so we handle the addend here instead.  */
      diff = reloc_entry->addend;
    }


#ifdef COFF_WITH_PE
  if (reloc_entry->howto->type == IMAGE_REL_ALPHA_REFLONGNB)
    {
/*      diff -= coff_data(output_bfd)->link_info->pe_info.image_base.value;*/
      abort();
    }
#endif

#define DOIT(x) \
  x = ((x & ~howto->dst_mask) | (((x & howto->src_mask) + diff) & howto->dst_mask))

    if (diff != 0)
      {
	reloc_howto_type *howto = reloc_entry->howto;
	unsigned char *addr = (unsigned char *) data + reloc_entry->address;

	switch (howto->size)
	  {
	  case 0:
	    {
	      char x = bfd_get_8 (abfd, addr);
	      DOIT (x);
	      bfd_put_8 (abfd, x, addr);
	    }
	    break;

	  case 1:
	    {
	      short x = bfd_get_16 (abfd, addr);
	      DOIT (x);
	      bfd_put_16 (abfd, x, addr);
	    }
	    break;

	  case 2:
	    {
	      long x = bfd_get_32 (abfd, addr);
	      DOIT (x);
	      bfd_put_32 (abfd, x, addr);
	    }
	    break;

	  default:
	    abort ();
	  }
      }

  /* Now let bfd_perform_relocation finish everything up.  */
  return bfd_reloc_continue;
}
#endif

#ifdef COFF_WITH_PE
/* Return true if this relocation should
   appear in the output .reloc section. */

static boolean in_reloc_p PARAMS((bfd *, reloc_howto_type *));
static boolean in_reloc_p(abfd, howto)
     bfd * abfd;
     reloc_howto_type *howto;
{
  /* ??? This is highly questionable */
  return ! howto->pc_relative && howto->type != IMAGE_REL_ALPHA_REFLONGNB;
}     
#endif

#ifndef PCRELOFFSET
#define PCRELOFFSET false
#endif

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

#undef HOWTO
#define HOWTO(C, R,S,B, P, BI, O, SF, NAME, INPLACE,MASKSRC,MASKDST,PC,SIV,HO) \
  {(unsigned)C,R,S,B, P, BI, O,SF,NAME,INPLACE,MASKSRC,MASKDST,PC,SIV,HO}
static reloc_howto_type alpha_howto_table[] =
{
  /* Reloc type 0 is ignored by itself.  However, it appears after a
     GPDISP reloc to identify the location where the low order 16 bits
     of the gp register are loaded.  */
  HOWTO (ALPHA_R_IGNORE,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "IGNORE",		/* name */
	 true,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* A 32 bit reference to a symbol.  #1 */
  HOWTO (ALPHA_R_REFLONG,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "REFLONG",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* A 64 bit reference to a symbol.  #2 */
  HOWTO (ALPHA_R_REFQUAD,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "REFQUAD",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* A 32 bit GP relative offset.  This is just like REFLONG except
     that when the value is used the value of the gp register will be
     added in.  */
  HOWTO (ALPHA_R_GPREL32,	/* type #3 */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "GPREL32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* Used for an instruction that refers to memory off the GP
     register.  The offset is 16 bits of the 32 bit instruction.  This
     reloc always seems to be against the .lita section.  */
  HOWTO (ALPHA_R_LITERAL,	/* type #4 */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "LITERAL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* This reloc only appears immediately following a LITERAL reloc.
     It identifies a use of the literal.  It seems that the linker can
     use this to eliminate a portion of the .lita section.  The symbol
     index is special: 1 means the literal address is in the base
     register of a memory format instruction; 2 means the literal
     address is in the byte offset register of a byte-manipulation
     instruction; 3 means the literal address is in the target
     register of a jsr instruction.  This does not actually do any
     relocation.  */
  HOWTO (ALPHA_R_LITUSE,	/* type #5 */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "LITUSE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* Load the gp register.  This is always used for a ldah instruction
     which loads the upper 16 bits of the gp register.  The next reloc
     will be an IGNORE reloc which identifies the location of the lda
     instruction which loads the lower 16 bits.  The symbol index of
     the GPDISP instruction appears to actually be the number of bytes
     between the ldah and lda instructions.  This gives two different
     ways to determine where the lda instruction is; I don't know why
     both are used.  The value to use for the relocation is the
     difference between the GP value and the current location; the
     load will always be done against a register holding the current
     address.  */
  HOWTO (ALPHA_R_GPDISP,	/* type #6 */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 reloc_nil,		/* special_function */
	 "GPDISP",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* A 21 bit branch.  The native assembler generates these for
     branches within the text segment, and also fills in the PC
     relative offset in the instruction.  */
  HOWTO (ALPHA_R_BRADDR,	/* type #7 */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 21,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "BRADDR",		/* name */
	 true,			/* partial_inplace */
	 0x1fffff,		/* src_mask */
	 0x1fffff,		/* dst_mask */
	 true,			/* pcrel_offset; different on NT */
	 false, false),         /* symndx is value, has offset */

  /* A hint for a jump to a register.  */
  HOWTO (ALPHA_R_HINT,		/* type #8 */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 14,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "HINT",		/* name */
	 true,			/* partial_inplace */
	 0x3fff,		/* src_mask */
	 0x3fff,		/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

#ifndef COFF_WITH_PE  /* [ */
  /* 16 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL16,	/* type #9 */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* 32 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL32,	/* type #10 */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* A 64 bit PC relative offset.  #11 */
  HOWTO (ALPHA_R_SREL64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL64",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false,			/* pcrel_offset */
	 false),                /* symndx is value */

  /* Push a value on the reloc evaluation stack.  */
  HOWTO (ALPHA_R_OP_PUSH,	/* type #12 */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "OP_PUSH",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* Store the value from the stack at the given address.  Store it in
     a bitfield of size r_size starting at bit position r_offset.  */
  HOWTO (ALPHA_R_OP_STORE,	/* type #13 */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "OP_STORE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* Subtract the reloc address from the value on the top of the
     relocation stack.  */
  HOWTO (ALPHA_R_OP_PSUB,	/* type #14 */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "OP_PSUB",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* Shift the value on the top of the relocation stack right by the
     given value.  */
  HOWTO (ALPHA_R_OP_PRSHIFT,	/* type #15 */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "OP_PRSHIFT",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false,			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* Adjust the GP value for a new range in the object file.  */
  HOWTO (ALPHA_R_GPVALUE,	/* type #16 */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "GPVALUE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false			/* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

#else /* ][ */  /* COFF_WITH_PE (==NT) */

  /* 32-bit address spread over 2 instructions; see absolute or match */
  HOWTO (IMAGE_REL_ALPHA_INLINE_REFLONG,   /* type #9 */             
	 0,	                /* rightshift */                           
	 1,	                /* size (0 = byte, 1 = short, 2 = long) */ 
	 16,	                /* bitsize */                   
	 false,	                /* pc_relative */                          
	 0,	                /* bitpos */                               
	 complain_overflow_signed, /* complain_on_overflow */
	 alpha_inline_reloc,	/* special_function */                     
	 "INLINE",              /* name */
	 true,	                /* partial_inplace */                      
	 0xffffffff,	        /* src_mask */                             
	 0xffffffff,        	/* dst_mask */                             
	 false,                 /* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* High portion of a 32 bit address */
  HOWTO (IMAGE_REL_ALPHA_REFHI,   /* type #10 */             
	 0,	                /* rightshift */                           
	 1,	                /* size (0 = byte, 1 = short, 2 = long) */ 
	 16,	                /* bitsize */                   
	 false,	                /* pc_relative */                          
	 0,	                /* bitpos */                               
	 complain_overflow_signed, /* complain_on_overflow */
	 alpha_refhi_reloc,	/* special_function */                     
	 "REFHI",               /* name */
	 true,	                /* partial_inplace */                      
	 0xffffffff,	        /* src_mask */                             
	 0xffffffff,        	/* dst_mask */                             
	 false,                 /* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* Low portion of a 32 bit address; the high part comes in with REFHI */
  HOWTO (IMAGE_REL_ALPHA_REFLO, /* type #11 */             
	 0,	                /* rightshift */                           
	 1,	                /* size (0 = byte, 1 = short, 2 = long) */ 
	 16,	                /* bitsize */                   
	 false,	                /* pc_relative */                          
	 0,	                /* bitpos */                               
	 complain_overflow_dont, /* complain_on_overflow */
	 alpha_reflo_reloc,	/* special_function */                     
	 "REFLO",               /* name */
	 true,	                /* partial_inplace */                      
	 0xffff,	        /* src_mask */                             
	 0xffff,        	/* dst_mask */                             
	 false,                 /* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* Connects REFHI and REFLO; REFHI is mostly a placeholder in this
     table; this entry does the work */
  HOWTO (IMAGE_REL_ALPHA_PAIR,  /* type #12 */             
	 16,	                /* rightshift */                           
	 1,	                /* size (0 = byte, 1 = short, 2 = long) */ 
	 16,	                /* bitsize */                   
	 false,	                /* pc_relative */                          
	 0,	                /* bitpos */                               
	 complain_overflow_unsigned, /* complain_on_overflow */
	 alpha_pair_reloc,        /* special_function */                     
	 "PAIR",                /* name */
	 true,	                /* partial_inplace */                      
	 0xffff,	        /* src_mask */                             
	 0xffff,        	/* dst_mask */                             
	 false,                 /* pcrel_offset */
	 true, false),         /* symndx is value, has offset */

  /* Connects with INLINE_REFLOG */
  HOWTO (IMAGE_REL_ALPHA_MATCH, /* type #13 */             
	 0,	                /* rightshift */                           
	 1,	                /* size (0 = byte, 1 = short, 2 = long) */ 
	 16,	                /* bitsize */                   
	 false,	                /* pc_relative */                          
	 0,	                /* bitpos */                               
	 complain_overflow_signed, /* complain_on_overflow */
	 alpha_match_reloc,     /* special_function */                     
	 "MATCH",               /* name */
	 true,	                /* partial_inplace */                      
	 0xffffffff,	        /* src_mask */                             
	 0xffffffff,        	/* dst_mask */                             
	 false,                 /* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* section header number */
  HOWTO (IMAGE_REL_ALPHA_SECTION,/* type #14 */             
	 0,	                /* rightshift */                           
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */ 
	 32,	                /* bitsize */                   
	 false,	                /* pc_relative */                          
	 0,	                /* bitpos */                               
	 complain_overflow_signed, /* complain_on_overflow */
	 alpha_section_reloc,     /* special_function */                     
	 "SECTION",             /* name */
	 true,	                /* partial_inplace */                      
	 0xffffffff,	        /* src_mask */                             
	 0xffffffff,        	/* dst_mask */                             
	 false,                 /* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

  /* va of containing section (as in an image sectionhdr) */
  HOWTO (IMAGE_REL_ALPHA_SECREL,/* type #15 */             
	 0,	                /* rightshift */                           
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */ 
	 32,	                /* bitsize */                   
	 false,	                /* pc_relative */                          
	 0,	                /* bitpos */                               
	 complain_overflow_signed, /* complain_on_overflow */
	 alpha_secrel_reloc,    /* special_function */                     
	 "SECREL",              /* name */
	 true,	                /* partial_inplace */                      
	 0xffffffff,	        /* src_mask */                             
	 0xffffffff,        	/* dst_mask */                             
	 false,                 /* pcrel_offset */
	 false, false),         /* symndx is value, has offset */


  /* 32-bit addr w/ image base */
  HOWTO (IMAGE_REL_ALPHA_REFLONGNB,/* type 16*/             
	 0,	                /* rightshift */                           
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */ 
	 32,	                /* bitsize */                   
	 false,	                /* pc_relative */                          
	 0,	                /* bitpos */                               
	 complain_overflow_signed, /* complain_on_overflow */
	 alpha_longnb_reloc,    /* special_function */
	 "ADDR32NB",            /* name */
	 true,	                /* partial_inplace */                      
	 0xffffffff,	        /* src_mask */                             
	 0xffffffff,        	/* dst_mask */                             
	 false,                 /* pcrel_offset */
	 false, false),         /* symndx is value, has offset */

#endif /* ] */
};

#ifdef DYNAMIC_LINKING /* [ */
  /* The following are used only for dynamic linking; there don't seem
     to be corresponding relocations for the NT usage model; the only
     meaningful fields are name, number and sym-is-value. */

#define DYN_HOWTO(C, NAME,SV,HO) \
  {(unsigned)C,0,0,0,0,0,0,0,NAME,0,0,0,0,SV,HO}

static reloc_howto_type alpha_dyn_howto_table[] =
{
  /* 32/64 bit relative relocation */
  DYN_HOWTO(ALPHA_DYN_R_REL_32, "REL_32",false,false),        /* 101 */
  DYN_HOWTO(ALPHA_DYN_R_REL_64, "REL_64",false,false),        /* 102 */

  DYN_HOWTO(ALPHA_DYN_R_JMP_SLOT, "JMP_SLOT",false,false),    /* 103 */

  DYN_HOWTO(ALPHA_DYN_R_REL_REFHI, "REL_REFHI",false,false),  /* 104 */
  DYN_HOWTO(ALPHA_DYN_R_REL_REFLO, "REL_REFLO",false,false),  /* 105 */
  DYN_HOWTO(ALPHA_DYN_R_REL_PAIR , "REL_PAIR",true,false),    /* 106 */

  DYN_HOWTO(ALPHA_DYN_R_COMB     , "COMB",false,true),        /* 107 */
  DYN_HOWTO(ALPHA_DYN_R_REL_HIONLY, "HIONLY",false,false),    /* 108 */

  /* REFLONG, REFQUAD come straight thru.  HI/LO/PAIR come thru with
     "symbolic" implied. */
};
#endif /* ] */
     


/* Turn a howto into a reloc  nunmber */

#define SELECT_RELOC(x,howto) { x.r_type = howto->type; }
#define BADMAG(x) NT_ALPHABADMAG(x)
#define NT_ALPHA 1			/* Customize coffcode.h */

#define RTYPE2HOWTO(cache_ptr, dst) \
  if ((dst)->r_type <                                                 \
       sizeof(alpha_howto_table)/sizeof(reloc_howto_type))            \
    (cache_ptr)->howto = alpha_howto_table + (dst)->r_type;           \
  else if ((dst)->r_type-ALPHA_DYN_R_REL_32 <                         \
       sizeof(alpha_dyn_howto_table)/sizeof(reloc_howto_type))        \
    (cache_ptr)->howto = alpha_dyn_howto_table + ((dst)->r_type       \
			 -ALPHA_DYN_R_REL_32);                        \
  else (cache_ptr)->howto = NULL                                     


/* Compute the addend of a reloc.  If the reloc is to a common symbol,
   the object file contains the value of the common symbol.  By the
   time this is called, the linker may be using a different symbol
   from a different object file with a different value.  Therefore, we
   hack wildly to locate the original symbol from this file so that we
   can make the correct adjustment.  This macro sets coffsym to the
   symbol from the original file, and uses it to set the addend value
   correctly.  If this is not a common symbol, the usual addend
   calculation is done, except that an additional tweak is needed for
   PC relative relocs.
   FIXME: This macro refers to symbols and asect; these are from the
   calling function, not the macro arguments.  */

#define CALC_ADDEND(abfd, ptr, reloc, cache_ptr)		\
  {								\
    coff_symbol_type *coffsym = (coff_symbol_type *) NULL;	\
    if (ptr && bfd_asymbol_bfd (ptr) != abfd)			\
      coffsym = (obj_symbols (abfd)				\
	         + (cache_ptr->sym_ptr_ptr - symbols));		\
    else if (ptr)						\
      coffsym = coff_symbol_from (abfd, ptr);			\
    if (coffsym != (coff_symbol_type *) NULL			\
	&& coffsym->native->u.syment.n_scnum == 0)		\
      cache_ptr->addend = - coffsym->native->u.syment.n_value;	\
    else if (ptr && bfd_asymbol_bfd (ptr) == abfd		\
	     && ptr->section != (asection *) NULL)		\
      cache_ptr->addend = - (ptr->section->vma + ptr->value);	\
    else							\
      cache_ptr->addend = 0;					\
    if (ptr && alpha_howto_table[reloc.r_type].pc_relative)	\
      cache_ptr->addend += asect->vma;				\
  }

/* we use our own relocate section */
#define coff_relocate_section alpha_relocate_section

/* Our own branch stubs analysis and emitting routine, too */
#define coff_bfd_find_stubs alpha_find_stubs
#define coff_bfd_emit_stubs alpha_emit_stubs

static reloc_howto_type *
coff_nt_alpha_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd *abfd;
     asection *sec;
     struct internal_reloc *rel;
     struct coff_link_hash_entry *h;
     struct internal_syment *sym;
     bfd_vma *addendp;
{

  reloc_howto_type *howto;

  if ( rel->r_type > sizeof(alpha_howto_table)/sizeof(reloc_howto_type))
      abort();
  howto = alpha_howto_table + rel->r_type;

#ifdef COFF_WITH_PE
  *addendp = 0;
#endif

  if (howto->pc_relative)
    *addendp += sec->vma;

  if (sym != NULL && sym->n_scnum == 0 && sym->n_value != 0)
    {
      /* This is a common symbol.  The section contents include the
	 size (sym->n_value) as an addend.  The relocate_section
	 function will be adding in the final value of the symbol.  We
	 need to subtract out the current size in order to get the
	 correct result.  */
 
      BFD_ASSERT (h != NULL);


#ifndef COFF_WITH_PE
      /* I think we *do* want to bypass this.  If we don't, I have seen some data
	 parameters get the wrong relcation address.  If I link two versions
	 with and without this section bypassed and then do a binary comparison,
	 the addresses which are different can be looked up in the map.  The 
	 case in which this section has been bypassed has addresses which correspond
	 to values I can find in the map */
      *addendp -= sym->n_value;
#endif
    }

  /* If the output symbol is common (in which case this must be a
     relocateable link), we need to add in the final size of the
     common symbol.  */
  if (h != NULL && h->root.type == bfd_link_hash_common) 
    *addendp += h->root.u.c.size;


#ifdef COFF_WITH_PE
  if (howto->pc_relative)
    *addendp -= sizeof(insn_word);

  if (rel->r_type == IMAGE_REL_ALPHA_REFLONGNB)
    {
      *addendp -= pe_data(sec->output_section->owner)->pe_opthdr.ImageBase;
    }
#endif

  return howto;
}


#define coff_bfd_reloc_type_lookup alpha_bfd_reloc_type_lookup

static reloc_howto_type *alpha_bfd_reloc_type_lookup PARAMS((bfd*, bfd_reloc_code_real_type));

static reloc_howto_type *
alpha_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  int alpha_type;

  switch (code)
    {
    case BFD_RELOC_32:
    case BFD_RELOC_CTOR:   /* refers to constructor table size */
      alpha_type = ALPHA_R_REFLONG;
      break;
    case BFD_RELOC_64:
      alpha_type = ALPHA_R_REFQUAD;
      break;
    case BFD_RELOC_GPREL32:
      alpha_type = ALPHA_R_GPREL32;
      break;
    case BFD_RELOC_ALPHA_LITERAL:
      alpha_type = ALPHA_R_LITERAL;
      break;
    case BFD_RELOC_ALPHA_LITUSE:
      alpha_type = ALPHA_R_LITUSE;
      break;
    case BFD_RELOC_ALPHA_GPDISP_HI16:
      alpha_type = ALPHA_R_GPDISP;
      break;
    case BFD_RELOC_ALPHA_GPDISP_LO16:
      alpha_type = ALPHA_R_IGNORE;
      break;
    case BFD_RELOC_23_PCREL_S2:
      alpha_type = ALPHA_R_BRADDR;
      break;
    case BFD_RELOC_ALPHA_HINT:
      alpha_type = ALPHA_R_HINT;
      break;
#ifdef COFF_WITH_PE
    case BFD_RELOC_ALPHA_PAIR:
      alpha_type = IMAGE_REL_ALPHA_PAIR;
      break;
    case BFD_RELOC_ALPHA_REFHI:
      alpha_type = IMAGE_REL_ALPHA_REFHI;
      break;
    case BFD_RELOC_ALPHA_REFLO:
      alpha_type = IMAGE_REL_ALPHA_REFLO;
      break;
    /* There are others, but no indication yet that they are used */
#else
    case BFD_RELOC_16_PCREL:
      alpha_type = ALPHA_R_SREL16;
      break;
    case BFD_RELOC_32_PCREL:
      alpha_type = ALPHA_R_SREL32;
      break;
    case BFD_RELOC_64_PCREL:
      alpha_type = ALPHA_R_SREL64;
      break;
#if 0
    case ???:
      alpha_type = ALPHA_R_OP_PUSH;
      break;
    case ???:
      alpha_type = ALPHA_R_OP_STORE;
      break;
    case ???:
      alpha_type = ALPHA_R_OP_PSUB;
      break;
    case ???:
      alpha_type = ALPHA_R_OP_PRSHIFT;
      break;
    case ???:
      alpha_type = ALPHA_R_GPVALUE;
      break;
#endif
#endif
    default:
      return (reloc_howto_type *) NULL;
    }

  return &alpha_howto_table[alpha_type];
}

#define coff_rtype_to_howto coff_nt_alpha_rtype_to_howto

#ifdef DYNAMIC_LINKING /* [ */
static boolean coff_alpha_link_create_dynamic_sections 
      PARAMS (( bfd *, struct bfd_link_info *));
static boolean coff_alpha_check_relocs 
      PARAMS (( bfd *, struct bfd_link_info *, asection *, 
		const struct internal_reloc *relocs));
static boolean coff_alpha_adjust_dynamic_symbol 
      PARAMS (( bfd*, struct bfd_link_info *, struct coff_link_hash_entry *,
		boolean));
static boolean coff_alpha_finish_dynamic_symbol 
      PARAMS (( bfd *, struct bfd_link_info *, struct coff_link_hash_entry *,
                struct internal_syment *));
static boolean coff_alpha_finish_dynamic_sections 
      PARAMS (( bfd *, struct bfd_link_info *));

#define coff_backend_link_create_dynamic_sections \
             coff_alpha_link_create_dynamic_sections
#define coff_backend_check_relocs coff_alpha_check_relocs
#define coff_backend_adjust_dynamic_symbol coff_alpha_adjust_dynamic_symbol
#define coff_backend_size_dynamic_sections pei_generic_size_dynamic_sections
#define coff_backend_finish_dynamic_symbol coff_alpha_finish_dynamic_symbol
#define coff_backend_finish_dynamic_sections coff_alpha_finish_dynamic_sections
#endif /* ] */

#include "coffcode.h"

static const bfd_target * nt_alpha_coff_object_p PARAMS((bfd *));

static const bfd_target *
nt_alpha_coff_object_p(a)
     bfd *a;
{
  return coff_object_p(a);
}

#ifdef DYNAMIC_LINKING /* [ */

/* PLT/GOT Stuff */
#define PLT_HEADER_SIZE 32
#define PLT_HEADER_WORD1        0xc3600000      /* br   $27,.+4     */
#define PLT_HEADER_WORD2        0xa77b000c      /* ldq  $27,12($27) */
#define PLT_HEADER_WORD3        0x47ff041f      /* nop              */
#define PLT_HEADER_WORD4        0x6b7b0000      /* jmp  $27,($27)   */

#define PLT_ENTRY_SIZE 12
#define PLT_ENTRY_WORD1         0x279f0000      /* ldah $28, 0($31) */
#define PLT_ENTRY_WORD2         0x239c0000      /* lda  $28, 0($28) */
#define PLT_ENTRY_WORD3         0xc3e00000      /* br   $31, plt0   */

#define MAX_GOT_ENTRIES         (64*1024 / 8)

  /* Since the spelling of _DYNAMIC et. al. changes, this becomes architecture
     dependent */

#ifdef TARGET_UNDERSCORE
#define DYNAMIC_SYM "__DYNAMIC"
#define GOT_SYM     "__GLOBAL_OFFSET_TABLE_"
#define PLT_SYM     "__PROCEDURE_LINKAGE_TABLE_"
#else
#define DYNAMIC_SYM "_DYNAMIC"
#define GOT_SYM     "_GLOBAL_OFFSET_TABLE_"
#define PLT_SYM     "_PROCEDURE_LINKAGE_TABLE_"
#endif

static boolean
coff_alpha_link_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;
  struct coff_link_hash_entry *h = NULL;

  /* We need to create .plt, .rel.plt, .got, .got.plt, .dynbss, and
     .rel.bss sections.  */

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED | SEC_READ);

  s = bfd_make_section (abfd, ".plt");
  coff_hash_table (info)->splt = s;
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s,
				  (flags | SEC_CODE | SEC_WRITE | SEC_EXEC))
				  /* Yup... writeable code on Alpha */
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;

  /* Define the symbol __PROCEDURE_LINKAGE_TABLE_ at the start of the
     .plt section.  */
  if (! (bfd_coff_link_add_one_symbol
	 (info, abfd, PLT_SYM, BSF_GLOBAL, s,
	  (bfd_vma) 0, (const char *) NULL, 
	  false, false,
	  (struct bfd_link_hash_entry **) &h)))
    return false;
  h->coff_link_hash_flags |= COFF_LINK_HASH_DEF_REGULAR;
  h->type = 0;

  if (info->shared
      && ! _bfd_coff_link_record_dynamic_symbol (info, h))
    return false;

  s = bfd_make_section (abfd, ".rel.plt");
  coff_hash_table (info)->srelplt = s;
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY | SEC_DATA)
      || ! bfd_set_section_alignment (abfd, s, 1))
    return false;

  /* This is dead code until we generate PIC (and thus need .got);
     the only thing it currently does is create a useless (and misleading)
     __GLOBAL_OFFSET_TABLE symbol
  if (! _bfd_coff_create_got_section (abfd, info, GOT_SYM, false))
    return false;
  */

#ifdef USE_COPY_RELOC /* [ */
  /* The .dynbss section is a place to put symbols which are defined
     by dynamic objects, are referenced by regular objects, and are
     not functions.  We must allocate space for them in the process
     image and use a R_*_COPY reloc to tell the dynamic linker to
     initialize them at run time.  The linker script puts the .dynbss
     section into the .bss section of the final image.  */
  s = bfd_make_section (abfd, ".dynbss");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, SEC_ALLOC))
    return false;

  /* The .rel[a].bss section holds copy relocs.  This section is not
     normally needed.  We need to create it here, though, so that the
     linker will map it to an output section.  We can't just create it
     only if we need it, because we will not know whether we need it
     until we have seen all the input files, and the first time the
     main linker code calls BFD after examining all the input files
     (size_dynamic_sections) the input sections have already been
     mapped to the output sections.  If the section turns out not to
     be needed, we can discard it later.  We will never need this
     section when generating a shared object, since they do not use
     copy relocs.  */
  if (! info->shared)
    {
      s = bfd_make_section (abfd, ".rel.bss");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY
						     | SEC_DYNAMIC)
	  || ! bfd_set_section_alignment (abfd, s, 1))
	return false;
    }
#endif /* ] */


  /* The special symbol _DYNAMIC is always set to the start of the
     .dynamic section.  This call occurs before we have processed the
     symbols for any dynamic object, so we don't have to worry about
     overriding a dynamic definition.  We could set _DYNAMIC in a
     linker script, but we only want to define it if we are, in fact,
     creating a .dynamic section.  We don't want to define it if there
     is no .dynamic section, since on some ELF platforms the start up
     code examines it to decide how to initialize the process.  */
  s = coff_hash_table (info)->dynamic;
  h = NULL;
  if (! (bfd_coff_link_add_one_symbol
	 (info, abfd, DYNAMIC_SYM, BSF_GLOBAL, s, (bfd_vma) 0,
	  (const char *) NULL, 
	  false, false,
	  (struct bfd_link_hash_entry **) &h)))
    return false;
  h->coff_link_hash_flags |= COFF_LINK_HASH_DEF_REGULAR;

  return true;
}

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static boolean
coff_alpha_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const struct internal_reloc *relocs;
{
  bfd *dynobj;
  bfd_vma *local_got_offsets;
  const struct internal_reloc *rel;
  const struct internal_reloc *rel_end;
  asection *srelgot;
  asection *sreloc;

  if (info->relocateable)
    return true;

  /* we don't do this for .stabs (or .stabstr). */
  if (strncmp(sec->name, ".stab",5) == 0)
    return true;

  dynobj = coff_hash_table (info)->dynobj;
  local_got_offsets = coff_local_got_offsets (abfd);

  srelgot = NULL;
  sreloc = NULL;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned long r_symndx;
      struct coff_link_hash_entry *h;

      if (rel->r_type == IMAGE_REL_ALPHA_PAIR)
	  continue;

      r_symndx = rel->r_symndx;

      if (r_symndx == -1)
	h = NULL;
      else
	h = obj_coff_sym_hashes(abfd)[r_symndx];

      switch (rel->r_type)
	{
	case ALPHA_R_BRADDR:
	  /* This symbol requires a procedure linkage table entry.  We
             actually build the entry in adjust_dynamic_symbol,
             because this might be a case of linking PIC code which is
             never referenced by a dynamic object, in which case we
             don't need to generate a procedure linkage table entry
             after all.  */

	  if (dynobj == NULL)
	      coff_hash_table (info)->dynobj = dynobj = abfd;

	  /* If this is a local symbol, we resolve it directly without
             creating a procedure linkage table entry.  */
	  if (h == NULL)
	    continue;

	  h->coff_link_hash_flags |= COFF_LINK_HASH_NEEDS_PLT;

	  break;

        case IMAGE_REL_ALPHA_REFLO:
        case IMAGE_REL_ALPHA_REFHI:
	  /* We count REFHI and REFLO separately; if the symbol ends up being
	     relative, the number of REFHIs is what we want.
	     If it ends up being symbolic, it's the number of REFLOs.
	     The two can differ because a single REFHI can serve for
	     several REFLOs if someone is smart enough.  (MSVC occasionally
	     does this. */

	case ALPHA_R_REFLONG:
	  /* Reflongs get the same treatment, and we count them in BOTH
	     buckets, choosing to preserve only one. */

	  if (dynobj == NULL)
	      coff_hash_table (info)->dynobj = dynobj = abfd;

	  /* REFLONG relocations always need some sort of relocation,
	     either RELATIVE or symbolic; either way, count them. */
#ifdef USE_COPY_RELOC
	  if (info->shared)
#endif
	    {
	      /* When creating a shared object, or not using COPY relocations,
		 we must copy some of these reloc types into the output file.
		 We create a reloc section in dynobj and make room for this
		 reloc.  */
	      if (sreloc == NULL)
		{
	          sreloc = coff_hash_table (info)->sreloc;
		  if (sreloc == NULL)
		    {
		      flagword flags;

		      sreloc = bfd_make_section (dynobj, ".rel.internal");
	              coff_hash_table (info)->sreloc = sreloc;
		      flags = (SEC_HAS_CONTENTS | SEC_READONLY
			       | SEC_IN_MEMORY | SEC_LINKER_CREATED);
		      if ((sec->flags & SEC_ALLOC) != 0)
			flags |= SEC_ALLOC | SEC_LOAD;
		      if (sreloc == NULL
			  || ! bfd_set_section_flags (dynobj, sreloc, flags)
			  || ! bfd_set_section_alignment (dynobj, sreloc, 1))
			return false;
		    }
		}

	      /* For the generic reloc section, we want to count the
		 number of relocations needed on a per-symbol basis,
		 and then increase the section size after we determine
		 which symbols will actually need runtime reloc entries.
		 Local symbols (h==NULL) will get RELATIVE relocs (if any),
		 so just count 'em now.   We figure out section symbols
		 later, as it *might* look like an ordinary ref now. */

	      if (h)
		{
      		  switch (rel->r_type)
		    {
		    case IMAGE_REL_ALPHA_REFHI:
		      /* REFHI should be counted for relative relocs */
		      h->num_relative_relocs_needed++;
		      break;
		    case IMAGE_REL_ALPHA_REFLO:
		      /* REFLO should be counted for symbolic relocs */
		      h->num_long_relocs_needed++;
		      break;
		    case ALPHA_R_REFLONG:
		      h->num_relative_relocs_needed++;
		      h->num_long_relocs_needed++;
		      break;
		    }
		}
#ifdef USE_COPY_RELOC
	      else
#else
	      else if (info->shared)
#endif
		{
		  /* Only high (and full) relocations count here */
		  if (rel->r_type != IMAGE_REL_ALPHA_REFLO)
		     sreloc->_raw_size += RELSZ;
		}
	    }
	  break;

	case IMAGE_REL_ALPHA_REFLONGNB:
	  /* Used to form VMAs in .idata$4 and .idata$5; VMAs so don't
	     need runtime relocations. */

        case IMAGE_REL_ALPHA_SECREL:
        case IMAGE_REL_ALPHA_SECTION:
	  /* Used in MS debug sections, which we ignore */

	case ALPHA_R_HINT:
	  /* No runtime relocation for hints; link time is as good as
	     we'll do. */
	  break;

	default:
          BFD_ASSERT(!"Unexpected relocation; ignored");
	  break;
	}
    }
  return true;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static boolean
coff_alpha_adjust_dynamic_symbol (dynobj, info, h, skip)
     struct bfd_link_info *info;
     struct coff_link_hash_entry *h;
     bfd *dynobj;
     boolean skip;
{
  asection *s;
  unsigned int power_of_two;
  asection *sreloc = coff_hash_table (info)->sreloc;

  /* The first part is done for every symbol; the rest only for 
     ones selected by the generic adjust_dynamic_symbol */

  /* Earlier, we counted the number of relocations each symbol might
     need.  Now that we know the symbol type, increase the size of 
     .rel.internal for those symbols that will get dynamic relocations.

     If it's a dynamic symbol (unconditionally) or we're doing a
     shared library, we want to emit dynamic relocations 
     for those relocations we counted up earlier.  (The code there knows
     which types of relocations we care about, but doesn't for sure
     know the symbol type.)

     If it is a symbolic shared library, we don't want to count locally
     defined absolute symbols, because they won't get a relocation.

     DLL symbols are ALWAYS static w.r.t. .so's; their dynamic nature
     is handled by the DLL mechanism.

     Section symbols are (implicitly) counted in shared libraries 
     because they get RELATIVE relocs that weren't counted (again because 
     we couldn't be sure of symbol type at the time.)

     Note: the fact that the count is non-zero is in the formal sense
     equivalent to REF_REGULAR being true, so we don't need to check
     REF_REGULAR, just add the count.

     Note: if it's a very simple case, sreloc may be null (and
     consequently all counts zero), and we don't need to bother.  */

  if (sreloc != NULL 
	&& (((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) != 0)
             || info->shared)
        )
    {
#undef DEBUG_COUNTING // delete later; as an anchor for now
#ifdef DEBUG_COUNTING //!!
int did_print=0;
if (h->num_long_relocs_needed || h->num_relative_relocs_needed) 
{
did_print = 1;
fprintf(stderr, "%s (%x) adds %d+%d(?)\n", h->root.root.string, h->coff_link_hash_flags, h->num_long_relocs_needed, h->num_relative_relocs_needed); //!!
}
#endif
      /* Determine which relocation type (or if any) will be emitted,
	 based upon whether the symbol (now that we know for sure its type)
	 is local or not.  Undefined are always exported, section and DLL
	 symbols always local.  Shared libs (when not symbolic or have
	 other symbols suppressed) or not defined locally get symbols. */
      if (bfd_is_abs_section(h->root.u.def.section))
	{
	  /* Nothing; no runtime relocation generated for abs. */
	}
      else if (
	  (
	     (! info->shared 
	     || !(info->symbolic || h->dynindx == -1)
	     || !((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0)
	     )

	  && h->root.type != bfd_link_hash_section
#ifdef USE_DLLS
	  && (h->coff_link_hash_flags & COFF_LINK_HASH_DLL_DEFINED) == 0
#endif
	  )
	  || h->root.type == bfd_link_hash_undefined
	  )
	{
	  /* Symbol is external to shared lib (has dynamic SYT entry); use
	     symbolic reloc at runtime, which implies COMB (and PAIR, rarely),
	     so count the incoming REFLO (and long) relocs. */
#ifdef DEBUG_COUNTING //!!
if (did_print) fprintf(stderr, "choose long: %s %d\n", h->root.root.string, h->num_long_relocs_needed);
#endif
	  sreloc->_raw_size += (h->num_long_relocs_needed * RELSZ);
	  h->num_long_relocs_needed = -h->num_long_relocs_needed;
	}
      else
	{
	  /* Symbol is local to shared lib (no external SYT entry); use
	     relative reloc at runtime which implies HIONLY, so count
	     the incoming REFHI (and long) relocs. */
#ifdef DEBUG_COUNTING //!!
if (did_print) fprintf(stderr, "choose relative: %s %d\n", h->root.root.string, h->num_relative_relocs_needed);
#endif
	  sreloc->_raw_size += (h->num_relative_relocs_needed * RELSZ);
	  h->num_relative_relocs_needed = -h->num_relative_relocs_needed;
	}
#ifdef DEBUG_COUNTING //!!
if (did_print) fprintf(stderr, "relocs = %d\n", sreloc->_raw_size); //!!
#endif
    }

  if (skip)
    return true;

  /* Make sure we know what is going on here.  */
#ifdef USE_WEAK
  BFD_ASSERT (dynobj != NULL
	      && ((h->coff_link_hash_flags & COFF_LINK_HASH_NEEDS_PLT)
		  || h->weakdef != NULL
		  || ((h->coff_link_hash_flags
		       & COFF_LINK_HASH_DEF_DYNAMIC) != 0
		      && (h->coff_link_hash_flags
			  & COFF_LINK_HASH_REF_REGULAR) != 0
		      && (h->coff_link_hash_flags
			  & COFF_LINK_HASH_DEF_REGULAR) == 0)));
#else
  BFD_ASSERT (dynobj != NULL
	      && ((h->coff_link_hash_flags & COFF_LINK_HASH_NEEDS_PLT)
		  || ((h->coff_link_hash_flags
		       & COFF_LINK_HASH_DEF_DYNAMIC) != 0
		      && (h->coff_link_hash_flags
			  & COFF_LINK_HASH_REF_REGULAR) != 0
		      && (h->coff_link_hash_flags
			  & COFF_LINK_HASH_DEF_REGULAR) == 0)));
#endif

  /* If this is a function, put it in the procedure linkage table.  We
     will fill in the contents of the procedure linkage table later. */
  if (ISFCN(h->type)
      || (h->coff_link_hash_flags & COFF_LINK_HASH_NEEDS_PLT) != 0)
    {
      if (! info->shared
	  && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) == 0
	  && (h->coff_link_hash_flags & COFF_LINK_HASH_REF_DYNAMIC) == 0)
	{
	  /* This case can occur if we saw a BRADDR reloc in an input
             file, but the symbol was never mentioned by a dynamic
             object.  In such a case, we don't actually need to build
             a procedure linkage table, and we can just do a local BRADDR
             reloc instead.  */
	  BFD_ASSERT ((h->coff_link_hash_flags & COFF_LINK_HASH_NEEDS_PLT) != 0);
	  return true;
	}

      /* Make sure this symbol is output as a dynamic symbol.  */
      if (h->dynindx == -1)
	{
	  if (! _bfd_coff_link_record_dynamic_symbol (info, h))
	    return false;
	}

      s = coff_hash_table(info)->splt;
      BFD_ASSERT (s != NULL);

      /* If this is the first .plt entry, make room for the special
	 first entry.  */
      if (s->_raw_size == 0)
	s->_raw_size += PLT_HEADER_SIZE;

      /* If this symbol is not defined in a regular file, and we are
	 not generating a shared library, then set the symbol to this
	 location in the .plt.  This is required to make function
	 pointers compare as equal between the normal executable and
	 the shared library.  */
      if (! info->shared
	  && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  h->root.u.def.section = s;
	  h->root.u.def.value = s->_raw_size;
	}

      h->plt_offset = s->_raw_size;

      /* Make room for this entry.  */
      s->_raw_size += PLT_ENTRY_SIZE;

      /* We also need to make an entry in the .rel.plt section.  */

      s = coff_hash_table(info)->srelplt;
      BFD_ASSERT (s != NULL);
      s->_raw_size += RELSZ;
//fprintf(stderr, "relPLT adds # %d, %s\n",( s->_raw_size/RELSZ)-1, h->root.root.string);

      return true;
    }

#ifdef USE_WEAK
  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined
		  || h->weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
      return true;
    }
#endif

#ifdef USE_COPY_RELOC /* [ */
  /* If we are not doing COPY relocations at all, we assume the same
     case as for shared libraries, and pay the cost of doing relocations
     for such symbols at runtime if the symbol is accessed by non-PIC
     code.  (By being smart, it's not necessarily that bad.) */

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  */

  /* If we are creating a shared library, we must presume that the
     only references to the symbol are via the global offset table.
     For such cases we need not do anything here; the relocations will
     be handled correctly by relocate_section.  */
  if (info->shared)
    return true;

  /* We must allocate the symbol in our .dynbss section, which will
     become part of the .bss section of the executable.  There will be
     an entry for this symbol in the .dynsym section.  The dynamic
     object will contain position independent code, so all references
     from the dynamic object to this symbol will go through the global
     offset table.  The dynamic linker will use the .dynsym entry to
     determine the address it must put in the global offset table, so
     both the dynamic object and the regular object will refer to the
     same memory location for the variable.  */

  s = bfd_get_section_by_name (dynobj, ".dynbss");
  BFD_ASSERT (s != NULL);

  /* We must generate a R_GNU_COPY reloc to tell the dynamic linker to
     copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rel.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      asection *srel;

      srel = bfd_get_section_by_name (dynobj, ".rel.bss");
      BFD_ASSERT (srel != NULL);
      srel->_raw_size += RELSZ;
      h->coff_link_hash_flags |= COFF_LINK_HASH_NEEDS_COPY;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 3)
    power_of_two = 3;

  /* Apply the required alignment.  */
  s->_raw_size = BFD_ALIGN (s->_raw_size,
			    (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (dynobj, s))
    {
      if (! bfd_set_section_alignment (dynobj, s, power_of_two))
	return false;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;
#endif /* ] */

  return true;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static boolean
coff_alpha_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct coff_link_hash_entry *h;
     struct internal_syment *sym;
{
  bfd *dynobj;
  bfd_vma ImageBase=bfd_getImageBase(output_bfd);

  /* It is possible to get here with h->dynindx == -1; a symbol that is
     forced local may still need a GOT entry. */

  dynobj = coff_hash_table (info)->dynobj;

  if (h->plt_offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *srelplt;
      bfd_vma plt_index;
      bfd_vma got_offset;
      struct internal_reloc rel;
      unsigned insn1, insn2, insn3;
      long hi, lo;


      /* This symbol has an entry in the procedure linkage table.  Set
	 it up.  */

      BFD_ASSERT (h->dynindx != -1);

      splt = coff_hash_table(info)->splt;
      srelplt = coff_hash_table(info)->srelplt;
      BFD_ASSERT (splt != NULL && srelplt != NULL);

      /* Get the index in the procedure linkage table which
	 corresponds to this symbol.  This is the index of this symbol
	 in all the symbols for which we are making plt entries.  The
	 first entry in the procedure linkage table is reserved.  */
      plt_index = (h->plt_offset - PLT_HEADER_SIZE) / PLT_ENTRY_SIZE;

      /* Fill in rest of plt entry: the offset to the relocation and
	 the start of the PLT.  Tricky code, here:

	   - Put the PLT slot number in R28 (just 'cause it's handy).
	   - Jump (relative backwards) to the beginning of the PLT.
	   - The plt does a BR to figure out where it is, and loads
	     up R27 with the address of the trampoline, which the
	     dynamic linker has stuffed into the first word after the
	     jump.
	   - Jump to the trampoline.
	   - The trampoline moves R28 to a0, and the 2d quadword of
	     data (which it has filled in with an info structure about
	     which we know nothing here) to a1, and calls it's runtime
	     fixup routine.
	   - The runtime fixup routine replaces the three instructions
	     we put in with an appropriate (it varies depending on span)
	     sequence to jump to the real entry point.
	     It also actually does the first transfer of control.

	 Thus, until the dynamic linker has done it's thing (the first
	 call) we jump to the dynamic linker after having pushed the relocation
	 information it requires.  After that (once the PLT is modified)
	 we jump to the real routine.  */

      /* Fill in the entry in the procedure linkage table.  
      /* decompose the reloc offset for the plt for ldah+lda */
      hi = plt_index * RELSZ;
      lo = ((hi & 0xffff) ^ 0x8000) - 0x8000;
      hi = (hi - lo) >> 16;

      /* Insn1 & insn2 load up the slot number.
	 insn3 is a jump backwards relative to the beginning of the plt */
      insn1 = PLT_ENTRY_WORD1 | (hi & 0xffff);
      insn2 = PLT_ENTRY_WORD2 | (lo & 0xffff);
      insn3 = PLT_ENTRY_WORD3 | ((-(h->plt_offset + 12) >> 2) & 0x1fffff);

      bfd_put_32 (output_bfd, insn1, splt->contents + h->plt_offset);
      bfd_put_32 (output_bfd, insn2, splt->contents + h->plt_offset + 4);
      bfd_put_32 (output_bfd, insn3, splt->contents + h->plt_offset + 8);
      /* if (info->base_file ...  do something? */

      /* Fill in the entry in the .rel.plt section.  */
      rel.r_symndx = h->dynindx;
      rel.r_vaddr = plt_index;

//fprintf(stderr, "relPLT emits # %d, %s\n", plt_index, h->root.root.string);
      rel.r_type = ALPHA_DYN_R_JMP_SLOT;
      rel.r_offset = 0;
      bfd_coff_swap_reloc_out (output_bfd, &rel,
				srelplt->contents + plt_index * RELSZ);
      srelplt->reloc_count++;  /* used for subsequent assert */

      if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  */
	  sym->n_scnum = N_UNDEF;
	  sym->n_sclass = C_EXT;
	}
    }

  /* The GOT never gets set, so...  If it ever does, there may be
     a need for multiple GOTs if the 65K size limit becomes an issue. */
  BFD_ASSERT(h->got_offset == -1);

#ifdef USE_COPY_RELOC /* [ */
  if ((h->coff_link_hash_flags & COFF_LINK_HASH_NEEDS_COPY) != 0)
    {
      asection *s;
      struct internal_reloc rel;

      /* This symbol needs a copy reloc.  Set it up.  */

      BFD_ASSERT (h->dynindx != -1
		  && (h->root.type == bfd_link_hash_defined
		      || h->root.type == bfd_link_hash_defweak));

      s = bfd_get_section_by_name (h->root.u.def.section->owner,
				   ".rel.bss");
      BFD_ASSERT (s != NULL);

      rel.r_vaddr = (h->root.u.def.value
		      + h->root.u.def.section->output_section->vma
		      + h->root.u.def.section->output_offset);
      rel.r_type = R_GNU_COPY;
      rel.r_symndx = h->dynindx;
      rel.r_offset = 0;
      bfd_coff_swap_reloc_out (output_bfd, &rel,
			      (s->contents + s->reloc_count*RELSZ));
      ++s->reloc_count;
    }
#endif /* ] */

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  if (strcmp (h->root.root.string, DYNAMIC_SYM) == 0
      || strcmp (h->root.root.string, GOT_SYM) == 0)
    sym->n_scnum = N_ABS;

  return true;
}

/* Finish up the dynamic sections.  */

static boolean
coff_alpha_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sdyn;
  bfd_vma ImageBase=bfd_getImageBase(output_bfd);


  dynobj = coff_hash_table (info)->dynobj;

  sdyn = coff_hash_table (info)->dynamic;

  if (coff_hash_table (info)->dynamic_sections_created)
    {
      asection *splt;
      coff_external_dyn *dyncon, *dynconend;

      splt = coff_hash_table(info)->splt;
      BFD_ASSERT (splt != NULL && sdyn != NULL);

      dyncon = (coff_external_dyn *) sdyn->contents;
      dynconend = (coff_external_dyn *) (sdyn->contents + sdyn->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  coff_internal_dyn dyn;
	  const char *name;
	  asection *s;

	  bfd_coff_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;

	    case DT_PLTGOT:
	      /* Caution; this varies with architecture */
	      s = coff_hash_table(info)->splt;
	      goto get_vma;
	    case DT_JMPREL:
	      s = coff_hash_table(info)->srelplt;
	    get_vma:
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_ptr = s->output_section->vma 
	                       + s->vma 
			       + s->output_offset;
	      bfd_coff_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;

	    case DT_PLTRELSZ:
	      /* Get size from .rel.plt section. */
	      s = coff_hash_table(info)->srelplt;
	      BFD_ASSERT (s != NULL);
	      if (s->_cooked_size != 0)
		dyn.d_un.d_val = s->_cooked_size;
	      else
		dyn.d_un.d_val = s->_raw_size;
	      bfd_coff_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;

	    case DT_RELSZ:
	      /* Accumulate sizes from .rel.internal and .rel.got sections */
	      s = coff_hash_table(info)->sreloc;
	      dyn.d_un.d_val = 0;
	      if (s != NULL)
		{
		  if (s->_cooked_size != 0)
		    dyn.d_un.d_val = s->_cooked_size;
		  else
		    dyn.d_un.d_val = s->_raw_size;
		}

	      s = coff_hash_table(info)->srelgot;
	      if (s != NULL)
		{
		  if (s->_cooked_size != 0)
		    dyn.d_un.d_val += s->_cooked_size;
		  else
		    dyn.d_un.d_val += s->_raw_size;
		}

	      bfd_coff_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;
	    }
	}

      /* Fill in the first entry in the procedure linkage table.  */
      if (splt->_raw_size > 0)
	{
	  /* if (info->base_file ...) */

          bfd_put_32 (output_bfd, PLT_HEADER_WORD1, splt->contents);
          bfd_put_32 (output_bfd, PLT_HEADER_WORD2, splt->contents + 4);
          bfd_put_32 (output_bfd, PLT_HEADER_WORD3, splt->contents + 8);
          bfd_put_32 (output_bfd, PLT_HEADER_WORD4, splt->contents + 12);

          /* The next two 64-bit words will be filled in by ld.so */
          bfd_put_32 (output_bfd, 0, splt->contents + 16);
          bfd_put_32 (output_bfd, 0, splt->contents + 20);
          bfd_put_32 (output_bfd, 0, splt->contents + 24);
          bfd_put_32 (output_bfd, 0, splt->contents + 28);
	}
    }

  return true;
}

#endif /* ] */

#ifdef INPUT_FORMAT
extern const bfd_target INPUT_FORMAT;
#define PINPUT_FORMAT &INPUT_FORMAT
#else
#define PINPUT_FORMAT NULL
#endif

/* The length, in words, of a stub branch */
#define STUB_WORDS 4

const bfd_target
#ifdef TARGET_SYM
  TARGET_SYM =
#else
  nt_alpha_vec =
#endif
{
#ifdef TARGET_NAME
  TARGET_NAME,
#else
  "coff-alpha",			/* name */
#endif
  bfd_target_coff_flavour,
  PINPUT_FORMAT,		/* format of acceptable input files for link */
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */
  0x3ffffb,			/* Jump span; since the span is w.r.t.						   the updated PC, we back off one instruction
				   so backwards branches are always in range.
				   (Not worth the trouble to fix it inline,
				   and makes the code less general.) */
  STUB_WORDS*sizeof(insn_word),	/* size of stub branch */
				/* 12 bytes generated, but to preserve
				   cache alignment, call it 16 */
  4,				/* alignment of first stub (power of 2) */
  COFF_PAGE_SIZE,		/* page size */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

  (SEC_HAS_CONTENTS | SEC_ALLOC /* section flags */
     | SEC_LOAD | SEC_RELOC | SEC_READ
     | SEC_READONLY | SEC_CODE | SEC_DATA | SEC_EXEC | SEC_WRITE),
  /* COFF_WITH_PE added
   * | SEC_LINK_ONCE | SEC_LINK_DUPLICATES),
   * but there doesn't seem to be any reason for that; removed because
   * it was causing problems, and it certainly doesn't apear to be
   * needed */

#ifdef TARGET_UNDERSCORE
  TARGET_UNDERSCORE,		/* leading underscore */
#else
  0,				/* leading underscore */
#endif
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */

  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

/* Note that we allow an object file to be treated as a core file as well. */
    {_bfd_dummy_target, nt_alpha_coff_object_p, /* bfd_check_format */
       bfd_generic_archive_p, nt_alpha_coff_object_p},
    {bfd_false, coff_mkobject, _bfd_generic_mkarchive, /* bfd_set_format */
       bfd_false},
    {bfd_false, coff_write_object_contents, /* bfd_write_contents */
       _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (coff),
     BFD_JUMP_TABLE_COPY (coff),
     BFD_JUMP_TABLE_CORE (_bfd_nocore),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
     BFD_JUMP_TABLE_SYMBOLS (coff),
     BFD_JUMP_TABLE_RELOCS (coff),
     BFD_JUMP_TABLE_WRITE (coff),
     BFD_JUMP_TABLE_LINK (coff),
#ifdef DYNAMIC_LINKING
     BFD_JUMP_TABLE_DYNAMIC (coff),
#else
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),
#endif

  COFF_SWAP_TABLE,
};


/* We assume that REFHI is always immediately followed by a PAIR, and
   PAIR only follows a REFHI */
static bfd_vma lastrefhi_relocation;
static bfd_byte *lastrefhi_addr = 0;

static bfd_reloc_status_type
alpha_refhi_reloc (abfd,
		  reloc_entry,
		  symbol,
		  data,
		  input_section,
		  output_bfd,
		  error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  bfd_reloc_status_type ret;
  bfd_vma relocation;

  if (reloc_entry->address > input_section->_cooked_size)
    {
      lastrefhi_addr = (bfd_byte *) -1;
      return bfd_reloc_outofrange;
    }

  ret = bfd_reloc_ok;
  if (bfd_is_und_section (symbol->section)
      && output_bfd == (bfd *) NULL)
    ret = bfd_reloc_undefined;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  /* we ignore addend here... both REFHI and PAIR will get whatever addend
     modifications apply; if we used THIS addend, we'd apply the addend
     modifications twice.  (Since relocations coming from a .o file have
     the symbol encoded in the REFHI and the offset in the PAIR, 
     it's the REFHI addend we should ignore.) */
  /* relocation += reloc_entry->addend; */
  /* Save the information, and let REFLO do the actual relocation.  */
  lastrefhi_addr = (bfd_byte *)data + reloc_entry->address;
  lastrefhi_relocation = relocation;

  reloc_entry->addend = 0;  /* just for sanity's sake */

  return ret;
}

static bfd_reloc_status_type
alpha_pair_reloc (abfd,
		  reloc_entry,
		  symbol,
		  data,
		  input_section,
		  output_bfd,
		  error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{

  insn_word insn;
  bfd_vma val;

  if (lastrefhi_addr == (bfd_byte *)-1) 
    {
      /* we skipped the last REFHI, so skip this too */
      lastrefhi_addr = NULL;
      return bfd_reloc_ok;
    }

  if (lastrefhi_addr != (bfd_byte *)data + reloc_entry->address) 
    {
      abort();
    }

  /* Do the REFHI relocation.  Note that we actually don't
     need to know anything about the REFLO itself, except
     where to find the low 16 bits of the addend needed by the
     REFHI.  */

  insn = bfd_get_32 (abfd, lastrefhi_addr);
  val = ((insn & 0xffff) << 16) + reloc_entry->addend + lastrefhi_relocation;

  /* If the calculation above yeilds a number that when decaputated
     to 16 bits would be negative, we need to bump the high order
     portion by 1 so that when the instructions are executed,
     the result is right because second instruction will do signed
     arithmetic on the value loaded by the first one.  The
     REFLO relocation will perform the same calculation as above
     (but inside _bfd_final_link_relocate) except that the high
     order portion from this instruction won't be there.  However,
     the result will also be trimmed to 16 bits, and the resulting
     low 16 bits should be the same as those computed here */

  if ((val & 0x8000) != 0)
    val = val + 0x10000;

  insn = (insn &~ 0xffff) | ((val >> 16) & 0xffff);
  bfd_put_32 (abfd, insn, lastrefhi_addr);

  /* We took care of the REFHI here, so the caller needn't do anything
     to it.  As far as the PAIR relocation, we need to adjust the 
     relocation itself to contain the new offset */

  reloc_entry->addend = val&0xffff;;

  return bfd_reloc_ok;
}

static bfd_reloc_status_type
alpha_reflo_reloc (abfd,
		  reloc_entry,
		  symbol,
		  data,
		  input_section,
		  output_bfd,
		  error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{

  insn_word insn;
  bfd_vma val;
  unsigned long relocation;

  /* REFLO and PAIR are VERY similar; the major difference is that
     the whole idea of a "high part" is just ignored, and the result
     goes back into the instruction */

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;

  insn = bfd_get_32 (abfd, (bfd_byte *)data + reloc_entry->address);
  val = (insn & 0xffff) + relocation;

  insn = (insn &~ 0xffff) | (val & 0xffff);
  bfd_put_32 (abfd, insn, (bfd_byte *)data + reloc_entry->address);

  reloc_entry->addend = 0;

  return bfd_reloc_ok;
}

static bfd_reloc_status_type
alpha_section_reloc (abfd,
		  reloc_entry,
		  symbol,
		  data,
		  input_section,
		  output_bfd,
		  error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
    /* ??? */
    abort();
    return 0;  /* shut up the compiler */
}

static bfd_reloc_status_type
alpha_secrel_reloc (abfd,
		  reloc_entry,
		  symbol,
		  data,
		  input_section,
		  output_bfd,
		  error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
    abort();
    return 0;  /* shut up the compiler */
}

static bfd_reloc_status_type
alpha_longnb_reloc (abfd,
		  reloc_entry,
		  symbol,
		  data,
		  input_section,
		  output_bfd,
		  error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
    /* it may be that in this case, no function is the right answer, but... */
    /* ??? */
    abort();
    return 0;  /* shut up the compiler */
}

static bfd_reloc_status_type
alpha_inline_reloc (abfd,
		  reloc_entry,
		  symbol,
		  data,
		  input_section,
		  output_bfd,
		  error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
    /* ??? */
    abort();
    return 0;  /* shut up the compiler */
}

static bfd_reloc_status_type
alpha_match_reloc (abfd,
		  reloc_entry,
		  symbol,
		  data,
		  input_section,
		  output_bfd,
		  error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
    /* ??? */
    abort();
    return 0;  /* shut up the compiler */
}

/* Check if values are in range.  Conceptually: abs(a-b)<d, but works
   for unsigned */
#define INRANGE(a,b,d) (((a)<(b)?((b)-(a)):((a)-(b))) < (d))

/* Relocate a section; we can't use bfd_generic_relocate_section because
   of REFHI/REFLO/PAIR and long branches */

static boolean
alpha_relocate_section (output_bfd, info, input_bfd,
				    input_section, contents, relocs, syms,
				    sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     struct internal_reloc *relocs;
     struct internal_syment *syms;
     asection **sections;
{
  struct internal_reloc *rel;
  struct internal_reloc *relend;
  long refhi_symndx = -1;
  bfd_vma ImageBase = pe_data(output_bfd)->pe_opthdr.ImageBase;

#ifdef DYNAMIC_LINKING
  asection *splt;
  asection *sreloc;
  asection *srelgot;
  boolean dynamic;
  bfd *dynobj;
  bfd_vma *local_got_offsets;
  bfd_size_type symrsz = bfd_coff_relsz(output_bfd);
  boolean is_stab_section, is_pdata_section;
  int local_rtype;
  bfd_vma refhi_offset = 0xffffffff;

  dynamic = coff_hash_table (info)->dynamic_sections_created;
  if (dynamic)
    {
      dynobj = coff_hash_table (info)->dynobj;
      local_got_offsets = coff_local_got_offsets (input_bfd);
      is_stab_section = 
	  strncmp(bfd_get_section_name(input_bfd,input_section),".stab",5) == 0;
      is_pdata_section = 
	  strncmp(bfd_get_section_name(input_bfd,input_section),".pdata",5) == 0;

      sreloc = coff_hash_table (info)->sreloc;
      splt = coff_hash_table(info)->splt;
      srelgot = coff_hash_table(info)->srelgot;
      /* BFD_ASSERT (sreloc != NULL); -- it gets made on demand */
      BFD_ASSERT (splt != NULL);
      /* BFD_ASSERT (srelgot != NULL); -- it gets made on demand */
    }
#endif

  rel = relocs;
  relend = rel + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      long symndx;
      struct coff_link_hash_entry *h;
      struct internal_syment *sym;
      bfd_vma addend;
      bfd_vma val;
      reloc_howto_type *howto;
      bfd_reloc_status_type rstat;
      int symclass;
      asection *sec;
      boolean has_ext;
      boolean needImageBase;  /* Not all relocs get ImageBase */
      boolean needStaticReloc, needDynamicReloc;  /* whether to omit them */
      boolean valIsValid;  /* whether the computed val is to be trusted */
      struct internal_reloc outrel;

      switch (rel->r_type)
	{
        case IMAGE_REL_ALPHA_PAIR:
	  BFD_ASSERT(refhi_symndx != -1);
	  symndx = refhi_symndx;
	  /* just to add to the excitement....  _bfd_coff_link_input_bfd
	     needs to know that the relocation's symndx value isn't really
	     a symndx if it's going to write the relocations out.  We
	     tell it here (and update the synndx at the same time).  See
	     cofflink.c for more on this.  Also, since this could occur
	     in a relocateable link, and we might simply skip some
	     relocations, we need to do it here */
	  rel->r_size = 1;
	  break;
        case IMAGE_REL_ALPHA_REFHI:
	  symndx = refhi_symndx = rel->r_symndx;
	  /* We still have to emit dynamic relocs, so we can't quit yet */
	  break;

	default:
	  symndx = rel->r_symndx;
	  break;
	}

      if (symndx == -1)
	{
	  h = NULL;
	  sym = NULL;
	}
      else
	{    
          symclass = bfd_coff_sym_is_global(input_bfd, &syms[symndx]);
	  if (symclass == SYM_SECTION_DEFINITION) 
	    {
	      /* a ref to a section definition wants to just use the section
		 information.  All other references want to proceed by
		 getting the hash.  Here we have the ref to def'n;
		 this issue becomes critical after section $-name sorts,
		 because we really want the section beginning, not some
		 random chunk of the section. */
	      h = NULL;
	    }
	  else
	    {
	      /* If this is a relocateable link, and we're dealing with
		 a relocation against a symbol (rather than a section),
		 leave it alone */
	      if (info->relocateable)
		continue;
	      /* not a ref to a section definition */
	      h = obj_coff_sym_hashes (input_bfd)[symndx];
	    }
	    sym = syms + symndx;
	}

      /* COFF treats common symbols in one of two ways.  Either the
         size of the symbol is included in the section contents, or it
         is not.  We assume that the size is not included, and force
         the rtype_to_howto function to adjust the addend as needed.  */

      if (sym != NULL && sym->n_scnum != 0)
	addend = - sym->n_value;
      else
	addend = 0;

      howto = bfd_coff_rtype_to_howto (input_bfd, input_section, rel, h,
				       sym, &addend);
      if (howto == NULL)
	return false;

      /* If we are doing a relocateable link, then we can just ignore
         a PC relative reloc that is pcrel_offset.  It will already
         have the correct value.  If this is not a relocateable link,
         then we should ignore the symbol value.  */
      if (howto->pc_relative && howto->pcrel_offset)
	{
	  if (info->relocateable)
	    continue;
#if 0 /* see comment above... rtype_to_howto should do this as needed */
/* ???  Adding/subtracting/adding/subtracting... sym->n_value seems
        rather pointless; someone at FSF or Cygnus who understands most
	of the relocatable formats should try to find a better way
	because not only is this wasteful, its VERY fragile.  */
	  if (sym != NULL && sym->n_scnum != 0)
	    addend += sym->n_value;
#endif
	}

      has_ext = false;

      val = 0;

      valIsValid = true;
      needStaticReloc = true;
      needDynamicReloc = false;

      if (h == NULL)
	{
	  if (symndx == -1)
	    {
	      sec = bfd_abs_section_ptr;
	      val = 0;
	    }
	  else
	    {
	      sec = sections[symndx];
              val = (sec->output_section->vma
		     + sec->output_offset
		     + sym->n_value
		     - sec->vma);
	    }
	}
      else
	{
	   switch(h->root.type)
	    {
	    case bfd_link_hash_defined_ext:
	    case bfd_link_hash_defweak_ext:
	      sec = h->root.u.defext.perm->section;
#ifdef DYNAMIC_LINKING
	      /* In some cases, we don't need the relocation
		 value.  We check specially because in some
		 obscure cases sec->output_section will be NULL.
		 We'll sort that out in the switch on relocation type
		 below, and then complain as needed.  */
	      if (sec == NULL || sec->output_section == NULL)
		  valIsValid = false;
	      else
#endif
		{
		  val = (h->root.u.defext.perm->value
		     + sec->output_section->vma
		     + sec->output_offset);
		  has_ext = true;
		}
	      break;

	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	    case bfd_link_hash_section:
	      sec = h->root.u.def.section;
#ifdef DYNAMIC_LINKING
	      /* In some cases, we don't need the relocation
		 value.  We check specially because in some
		 obscure cases sec->output_section will be NULL.
		 We'll sort that out in the switch on relocation type
		 below, and then complain as needed.  */
	      if (sec == NULL || sec->output_section == NULL)
		  valIsValid = false;
	      else
#endif
		  val = (h->root.u.def.value
		     + sec->output_section->vma
		     + sec->output_offset);
	      break;

	    case bfd_link_hash_undefweak:
#ifdef USE_WEAK
	      sec = NULL;
	      val = 0;
	      break;
#endif
	    case bfd_link_hash_undefined:
	    case bfd_link_hash_new:
	    case bfd_link_hash_common:
	    case bfd_link_hash_indirect:
	    case bfd_link_hash_warning:

	    /* The symbol is undefined (in some way)... some of the above
	       probably should never be able to happen. */

#ifdef DYNAMIC_LINKING /* [ */
	    /* If it has a dynamic index, let it thru (because presumably
	       it'll get fixed at runtime), except in the special
	       case of a -Bsymbolic shared lib link. */
	      if (h->dynindx != -1 && (info->shared? !info->symbolic : true))
		{
	          val = 0;
		  valIsValid = true;
		}
	      else
#endif /* ] */
	        {
	          valIsValid = false;
		}
	      sec = NULL;
	      break;

	    default:
	      abort;
	    }
	}

      /* Branches have limited spans, and if the target is out of range
	 we need to jump to an in-range stub.  Eariler passes should
	 have created in-range stubs for every branch.  If the real
	 value is out of range, we search for a stub, and substitute its
	 address into VAL.  (It's an internal error for a branch to be
	 out of range.)   This only makes sense for final links,
	 and when has_ext is set (if not relocatable, has_ext can't
	 be set, so we only need one test).  If has_ext didn't get
	 set when it should have been, we have an internal error that will
	 show up as an unreachable branch.  We put has_ext first, because
	 for most (statistical) cases it will be false. */
      if (has_ext && rel->r_type == ALPHA_R_BRADDR)
	{
	   bfd_vma currPC;
	   insn_word insn;

	   /* There's a possiblity that somehow, someway, someone generated
	      a BRADDR with an offset... we can't handle that, but failing
	      silently would be really nasty. */

	   insn = bfd_get_32 (input_bfd, 
	            contents + rel->r_vaddr - input_section->vma);

	   if ((insn & 0x1fffff) != 0 || addend != -4) /* -4 for adjusted PC */
	     {
		  (*_bfd_error_handler)
		    ("%s: branch at 0x%lx in section `%s' has offset",
		     bfd_get_filename (input_bfd),
		     (unsigned long) rel->r_vaddr,
		     bfd_get_section_name (input_bfd, input_section));
	     }

	   currPC = rel->r_vaddr + input_section->output_section->vma
	                         + input_section->output_offset;
	   /* A branch address... it could be out of range and we have
	      to hunt up a stub. ...careful, unsigned distances... */
	   if (!INRANGE(currPC, val, info->jump_span))
	     {
		struct extra_addresses *s;

		/* out of range; we have to find an in-range stub */
		s = h->root.u.defext.perm->extras;
		while (s != NULL)
		  {
		    if (INRANGE(currPC, s->addr, info->jump_span))
		      {
			 val = s->addr;
			 goto match; /* that is, continue outer loop */;
		      }
		    s=s->next;
		  }
		/* there's always supposed to be one!  (Prior stages are
		   buggy if not.) */
		abort();
	     }
	     match:;
	}

      needImageBase = !howto->pc_relative
                       && sec != NULL 
		       && !bfd_is_abs_section(sec->output_section);

#ifdef DYNAMIC_LINKING /* [ */
      /* If no dynamic stuff at all, skip this switch */
      if (dynamic)
      switch (rel->r_type)
	{
	case ALPHA_R_BRADDR:
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table, if appropriate  */

	  /* Resolve a BRADDR reloc again a local symbol directly,
             without using the procedure linkage table.  */
	  if (h == NULL) {
	      /* Gas puts the symbol's value in the instruction in this
		 case, so back it out. */
	      val -= sym->n_value;
	      break;
	    }

	  if (h->plt_offset == (bfd_vma) -1)
	    {
	      /* We didn't make a PLT entry for this symbol.  This
                 happens when statically linking PIC code, or when
                 using -Bsymbolic.  */
	      break;
	    }

	  val = (splt->output_section->vma
			+ splt->output_offset
			+ h->plt_offset);
	  needImageBase = false;
	  valIsValid = true;

	  break;

	case IMAGE_REL_ALPHA_REFHI:
	  local_rtype = 0;
	  goto filter_relocs;

	case IMAGE_REL_ALPHA_REFLO:
	  local_rtype = 1;
	  goto filter_relocs;

	case IMAGE_REL_ALPHA_PAIR:
	  local_rtype = 2;
	  goto filter_relocs;

	case ALPHA_R_REFLONG:
	  local_rtype = 3;

	filter_relocs:
	  /* Full length (not branch) addresses; decide more finely
	     if we care */

#ifdef USE_COPY_RELOC
	  /* Don't bother if not a shared lib */
	  if (!info->shared)
	      break;
#endif
	  /* stabs don't need this */
	  if (is_stab_section)
	      break;
	
	  /* When generating a shared object (or when not using
	     COPY relocations), these relocations
	     are copied into the output file to be resolved at run
	     time.  Ditto if a non-shared object and COPY relocations
	     are not being used. */

#ifndef USE_COPY_RELOC
	  /* if we don't use copy relocs, certain symbol types will have
	     leaked thru above that for a main (==not shared) we want to
	     skip */
	  if (!info->shared && h != NULL)
	    {
	      if (h->dynindx == -1)
		  break;
	      /* An ordinary symbol doesn't need a dynamic reloc in main;
		 we get here when a shared library calls back into main, 
		 and there are also (ordinary) calls within main. */
	      if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR)!=0)
		  break;
	    }
#endif

	  outrel.r_vaddr = rel->r_vaddr
			   + input_section->output_section->vma
			   + input_section->output_offset;

	  outrel.r_offset = 0;

	  /* The basic idea here is that we may or may not want to
	     emit either class of relocation; for static relocations,
	     if this reloc is against an external symbol, we do
	     not want to fiddle with the addend.  Otherwise, we
	     need to include the symbol value so that it becomes
	     an addend for the dynamic reloc.  */

	  /* pdata is always relative (because it applies to THIS
	     instance of the procedure, not the dynamic one);
	     we assume the only reloc in .pdata is REFLONG. */
	  if (h == NULL || is_pdata_section)
	    {
	      /* symbol is local to the .o */
	      needStaticReloc = true;
	      needDynamicReloc = info->shared &&
		    !bfd_is_abs_section(sec->output_section);
	      outrel.r_type = ALPHA_DYN_R_REL_32;
	      outrel.r_symndx = 0;
	    }
	  else if ((info->symbolic || h->dynindx == -1)
		  && (h->coff_link_hash_flags
		      & COFF_LINK_HASH_DEF_REGULAR) != 0)
	    {
	      /* symbol is local to the module because it's symbolic;
		 h->dynindx may be -1 if this symbol was marked to
		 become local. */
	      needStaticReloc = true;
#ifdef USE_COPY_RELOC
	      needDynamicReloc = true;
#else
	      needDynamicReloc = info->shared;
#endif
	      /* no relocs for module local, absolute symbols */
	      needDynamicReloc &= 
		    !bfd_is_abs_section(sec->output_section);
	      outrel.r_type = ALPHA_DYN_R_REL_32;
	      outrel.r_symndx = 0;
	    }
	  else if (((h->coff_link_hash_flags 
		    & COFF_LINK_HASH_DEF_REGULAR) == 0)
		   || (info->shared && !info->symbolic))
	    {
	      /* Not defined in this module, or this is a shared 
		 library (not symbolic), so we need a dynamic reloc;
		 if it is defined in a main, it always wins even
		 if there's also a dynamic version. */
	      BFD_ASSERT (h->dynindx != -1);
	      needStaticReloc = false;
	      needDynamicReloc = true;
	      valIsValid = true;
	      outrel.r_type = ALPHA_R_REFLONG;
	      outrel.r_symndx = h->dynindx;
	    }

	  /* now that we have all the rules sorted out, we need to 
	     sort out the types into real relocation entries. */

	  if (!needDynamicReloc)
	     break;

	  /* Until proven otherwise, we assume that REFHI/PAIR/REFLO comes
	     in exactly that order, so we can coalesce them into a single
	     relocation.

	     This relocation entry format squeezes a 4th field into the
	     10 byte entry.
	     The usual format
	       Bytes
	         2  Relocation type   (1 is prob OK, but a pain to deal with)
		 4  Offset
		 4  Syt index (if needed).

	     The new one
	       Bytes
	         2  Relocation type
		 4  Offset
		 3  Syt index (if needed).
		 1  word(!) offset from offset to REFLO instruction.

	     This reduces the number of symbols available to 16M, which
	     seems enough for now.

	     In the internal format, we just use the r_offset field for
	     this.  It is otherwise unused by COFF.

	     At the same time, we only omit one relocation entry for
	     relative relocs: only the high portion ever changes anyway,
	     because NT limits us to 65K alignment of a shared library
	     (or other "executable").  (It's not worth the trouble, but
	     possible, to do the same thing to the relative relocs.)

	     There are two reasons to do this compression: space savings
	     and so we can reduce the number of symbol table index lookups
	     at runtime.  (By 2x just by doing this, and more if we also
	     sort by symbol table index.)
	  */

	  {
	     int reltypes[] = {
		 /* relative relocs */
		 ALPHA_DYN_R_REL_REFHI, 
		      ALPHA_DYN_R_REL_REFLO, 
		      ALPHA_DYN_R_REL_PAIR,
		      ALPHA_DYN_R_REL_32,
		 /* symbolic relocs */
		 IMAGE_REL_ALPHA_REFHI, 
		      IMAGE_REL_ALPHA_REFLO, 
		      IMAGE_REL_ALPHA_PAIR,
		      ALPHA_R_REFLONG
		 };
	     outrel.r_type = reltypes
		  [(outrel.r_type==ALPHA_DYN_R_REL_32?0:1)*4+local_rtype];
	  }

	  switch (outrel.r_type)
	    {
	    case IMAGE_REL_ALPHA_REFHI:
	       BFD_ASSERT(refhi_offset == 0xffffffff);
	       refhi_offset = outrel.r_vaddr;
	       needDynamicReloc = false;
	       break;
	    case IMAGE_REL_ALPHA_PAIR:
	       /* ignore it; the value will come thru from the REFLO */
	       BFD_ASSERT(refhi_offset != 0xffffffff);
	       needDynamicReloc = false;
	       break;
	    case IMAGE_REL_ALPHA_REFLO:
	       {
	       long offset;

	       BFD_ASSERT(++h->num_long_relocs_needed <= 0);
	       /* If some other COMB used up this REFHI, we just emit
		  an ordinary REFLO (the same REFHI may support more
		  than one REFLO on the same symbol). We'll just emit
		  the bare REFLO */
	       if (refhi_offset == 0xffffffff)
		   break;
	       /* Theoretically, REFHIs and the corresponding REFLOs could
		  be intermixed.  If so, we should detect and gripe (or
		  do it right). */
	       BFD_ASSERT(refhi_symndx == rel->r_symndx);
	       offset = ((long)(outrel.r_vaddr - refhi_offset))>>2;
	       outrel.r_vaddr = refhi_offset;
	       if (offset > 127 || offset < -128)
		 {
		   /* If we blow out of our +- 128 instruction range,
		      we'll report it for now; if it ever really happens,
		      we'll have to find more range or continue to export
		      a few triplets. */
	           (*_bfd_error_handler)
		   ("bad COMB relocation attempted in %s at %p;\n"
		    "Lower opt level on this .o.  "
		    "Please report to gcc@interix.com",
		    input_bfd->filename, rel->r_vaddr);
		   return false;
	         }
	       outrel.r_offset=offset&0xff;
	       outrel.r_type = ALPHA_DYN_R_COMB;
	       refhi_offset = 0xffffffff;
	       }
	       break;

	    case ALPHA_DYN_R_REL_REFHI:
	       /* Since we can assume 64Kb alignment on NT... */
	       outrel.r_type = ALPHA_DYN_R_REL_HIONLY;
	       if (h != NULL)
	           BFD_ASSERT(++h->num_relative_relocs_needed <= 0);
	       /* Emit as is */
	       break;

	    case ALPHA_DYN_R_REL_REFLO:
	    case ALPHA_DYN_R_REL_PAIR:
	       /* Discard */
	       needDynamicReloc = false;
	       break;

	    case ALPHA_DYN_R_REL_32:
	       if (h == NULL)
		   break;
	       /* for .pdata we don't (at this point) know which counter
		  was used, so decrement the negative one.  This weakens
		  the check here, but at least the count stays right if
		  all is well. */
	       if (h->num_relative_relocs_needed < 0)
		 {
		   BFD_ASSERT(++h->num_relative_relocs_needed <= 0);
		 }
	       else if (h->num_long_relocs_needed < 0)
		 {
		   BFD_ASSERT(++h->num_long_relocs_needed <= 0);
		 }
	       else
		 {
		   BFD_ASSERT(!"Counters messed up");
		 }
	       break;

	    case ALPHA_R_REFLONG:
	       BFD_ASSERT(++h->num_long_relocs_needed <= 0);
	       break;

	    }
	  break;

	default:
	  break;
	}
#endif /* ] */

      if (!valIsValid)
	{
	  if (h->root.type == bfd_link_hash_undefined)
	    {
	      if (!info->relocateable)
		{
		    if (!((*info->callbacks->undefined_symbol)
			 (info, h->root.root.string, input_bfd, input_section,
			  rel->r_vaddr - input_section->vma)))
		    return false;
		}
	    }
	  else
	    {
	      (*_bfd_error_handler)
		("%s: warning: unresolvable relocation against symbol `%s' from %s section",
		 bfd_get_filename (input_bfd), h->root.root.string,
		 bfd_get_section_name (input_bfd, input_section));
	    }

	  val = 0;
	}

#ifdef DYNAMIC_LINKING

      if (needDynamicReloc)
	{
	  bfd_coff_swap_reloc_out (output_bfd, &outrel,
				    (struct external_reloc *)
				      (sreloc->contents
				     + sreloc->reloc_count * symrsz));
	  ++sreloc->reloc_count;
#ifdef DEBUG_COUNTING  // !!
{
struct coff_link_hash_entry *hh;
char buf[SYMNMLEN + 1];
fprintf(stderr, "added reloc # %d (%d) for %s / %s ", sreloc->reloc_count, outrel.r_type, symndx==-1?"local-symbol":h?h->root.root.string:"suppressed-symbol",_bfd_coff_internal_syment_name (input_bfd, sym, buf));

if (symndx!=-1){
hh = obj_coff_sym_hashes (input_bfd)[symndx];
if (hh) fprintf(stderr, "%d %d %d\n", (hh->coff_link_hash_flags & COFF_LINK_HASH_REF_REGULAR) != 0, (hh->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) != 0, hh->root.type);
else fprintf(stderr, "nohash\n");
}
}
#endif
	}

      if (!needStaticReloc)
	continue;
#endif

      if (needImageBase)
          addend += ImageBase;

      if (info->base_file)
	{
	  /* Emit a reloc if the backend thinks it needs it. */
	  if (sym && pe_data(output_bfd)->in_reloc_p(output_bfd, howto))
	    {
	      /* relocation to a symbol in a section which
		 isn't absolute - we output the address here 
		 to a file; however, to do this right we need a type */
	      bfd_vma addr = rel->r_vaddr 
		- input_section->vma 
		+ input_section->output_offset 
		+ input_section->output_section->vma;
	      fwrite (&addr, 1, sizeof(bfd_vma), (FILE *) info->base_file);
	    }
	}

      switch(rel->r_type)
	{
	case IMAGE_REL_ALPHA_REFHI:
	  continue;  /* It's a no-op at this point */

	case IMAGE_REL_ALPHA_PAIR:
	  /* the symndx (and conseuently val) being used came from the 
	     prior REFHI; we contribute the low-order bits of the addend
	     via the content of the symbol table index field */
          {
	  insn_word insn;

	  insn = bfd_get_32 (input_bfd, 
	            contents + rel->r_vaddr - input_section->vma);

	  val = val + ((insn & 0xffff) << 16) + rel->r_symndx + addend;

	  /* If the calculation above yields a number that when decaputated
	     to 16 bits would be negative, we need to bump the high order
	     portion by 1 so that when the instructions are executed,
	     the result is right because second instruction will do signed
	     arithmetic on the value loaded by the first one.  The
	     REFLO relocation will perform the same calculation as above
	     (but inside _bfd_final_link_relocate) except that the high
	     order portion from this instruction won't be there.  However,
	     the result will also be trimmed to 16 bits, and the resulting
	     low 16 bits should be the same as those computed here */

	  if ((val & 0x8000) != 0)
	    val += 0x10000;

	  insn = (insn &~ 0xffff) | ((val >> 16) & 0xffff);
	  bfd_put_32 (input_bfd, (bfd_vma) insn,
		      contents + rel->r_vaddr - input_section->vma);

	  rel->r_symndx = ((val & 0xffff)<<16)>>16;  /* the new low order bits */

	  rstat = bfd_reloc_ok;
	  }
	  break;

        default:
          rstat = _bfd_final_link_relocate (howto, input_bfd, input_section,
					contents,
					rel->r_vaddr - input_section->vma,
					val, addend);
	  break;
	}

      switch (rstat)
	{
	default:
	  abort ();
	case bfd_reloc_ok:
	  break;
	case bfd_reloc_outofrange:
	  (*_bfd_error_handler)
	    ("%s: bad reloc address 0x%lx in section `%s'",
	     bfd_get_filename (input_bfd),
	     (unsigned long) rel->r_vaddr,
	     bfd_get_section_name (input_bfd, input_section));
	  return false;
	case bfd_reloc_overflow:
	  {
	    const char *name;
	    char buf[SYMNMLEN + 1];

	    if (symndx == -1)
	      name = "*ABS*";
	    else if (h != NULL)
	      name = h->root.root.string;
	    else
	      {
		name = _bfd_coff_internal_syment_name (input_bfd, sym, buf);
		if (name == NULL)
		  return false;
	      }

	    if (! ((*info->callbacks->reloc_overflow)
		   (info, name, howto->name, (bfd_vma) 0, input_bfd,
		    input_section, rel->r_vaddr - input_section->vma)))
	      return false;
	  }
	}
    }
  return true;
}

/*  Parse relocations looking for out of range branches */
static void
alpha_find_stubs (output_bfd, info, input_section)
     bfd *output_bfd;
     struct bfd_link_info *info;
     asection *input_section;
{ 
  struct internal_reloc *rel;
  struct internal_reloc *relend;
  bfd *input_bfd = input_section->owner;
  bfd_vma lastseen = 0;
  static struct extended_def_vol *stub_list_tail;

  if ((input_section->flags & SEC_RELOC) == 0
      || input_section->reloc_count == 0)
     /* nothing to do */
     return;

  rel = _bfd_coff_read_internal_relocs 
           (input_bfd, input_section, true, NULL, false, NULL);

  relend = rel + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      long symndx;
      struct coff_link_hash_entry *h;
      bfd_vma val;
      asection *sec;
      bfd_vma currPC;
      boolean has_ext;

      /* Ignore uninteresting stuff */
      if (rel->r_type != ALPHA_R_BRADDR)
	  continue;

      BFD_ASSERT(lastseen <= rel->r_vaddr);
      lastseen=rel->r_vaddr;

      symndx = rel->r_symndx;

      /* If an absolute section or a section symbol, we'll ignore it.
	 A branch to these seems rather unlikely. */
      if (symndx == -1)
	 continue;
  
      h = obj_coff_sym_hashes (input_bfd)[symndx];
      if (h == NULL)
	 continue;

      /* There's some space-saving magic going on with .vol and .perm;
	 see bfdlink.h for more details. */
      if (h->root.type == bfd_link_hash_defined_ext
	  || h->root.type == bfd_link_hash_defweak_ext)
	{
	  sec = h->root.u.defext.perm->section;
	  val = (h->root.u.defext.perm->value
	     + sec->output_section->vma
	     + sec->output_offset);
	  has_ext = true;
	}
      else if (h->root.type == bfd_link_hash_defined
	  || h->root.type == bfd_link_hash_defweak)
	  /* we'll never branch to a section symbol */
	{
	  sec = h->root.u.def.section;
	  val = (h->root.u.def.value
	     + sec->output_section->vma
	     + sec->output_offset);
          has_ext = false;
	}
      else 
	/* undefined; let someone else handle complaining. */
	continue;

      currPC = input_section->output_offset
	       + input_section->output_section->vma 
	       + rel->r_vaddr;

      /* Already in the chain, we don't need to worry about it for now,
	 in that there's already a branch scheduled that we're sure to
	 be able to reach */
      if (has_ext && h->root.u.defext.vol != NULL)
          continue;

      /* At this point, val is the target address of the branch.
	 If it's lower than the end of this section, it's an accurate
	 address.  If it's higher (forward), it's inaccurate.
	 We'll assume forwards are out-of-range until proven otherwise

	 Dynamic symbols (that is, long branches to other shared libs)
	 are assumed to be always out of range.  */

      if (val 
	  < input_section->output_offset 
	    + input_section->output_section->vma
	    + input_section->_cooked_size)
	{
	  /* Backwards branch; is it in range? (Addresses in the same
	     input section are presumed to be in range (this is where
	     currPC might be below val).  If not, we can't save it anyway!) */
	  struct extra_addresses *s;
	  if (INRANGE(val, currPC, info->jump_span))
	       /* in range; nothing to do */
	      continue;

	  /* out of range (or dynamic); is there already an in-range one
	     added to the symbol table? */

	  if (has_ext)
	    {
	      s = h->root.u.defext.perm->extras;
	      while (s != NULL)
		{
		  if (INRANGE(s->addr, currPC, info->jump_span))
		    {
		       goto no_action /* that is, continue outer loop */;
		    }
		  s=s->next;
		}
	    }
	}

      /* There's an ugly little special case of adjacent calls forcing
	 us out of available headroom.  That's handled elsewhere */

      /* No branch in range; schedule one by threading thru the symbol table;
	 we assume the relocs are in r_vaddr order */
      if (!has_ext)
	{
	   /* This symbol has not been involved in the branch stub stuff
	      before; convert to a ..._ext type symbol, and create
	      the permanent part. */
	   struct extended_def_perm *p = 
	      (struct extended_def_perm *)
	       bfd_malloc(sizeof(struct extended_def_perm));

	   p->value = h->root.u.def.value;
	   p->section = h->root.u.def.section;
	   p->extras = NULL;
	   if (h->root.type == bfd_link_hash_defined)
	       h->root.type = bfd_link_hash_defined_ext;
	   if (h->root.type == bfd_link_hash_defweak)
	       h->root.type = bfd_link_hash_defweak_ext;
	   h->root.u.defext.perm = p;
	}

      /* Now the volatile part; we assured that when has_ext is true
	 (that is, it had been involved in stubs before) that the 
	 volatile part was null, above.  First, get it from the
	 cache or make a new one.  */
      if (info->stub_vol_cache != NULL) 
	{
	   h->root.u.defext.vol = info->stub_vol_cache;
	   info->stub_vol_cache=h->root.u.defext.vol->ref_chain;
	}
      else
	{
	   h->root.u.defext.vol = (struct extended_def_vol *)
		bfd_malloc(sizeof(struct extended_def_vol));
	}

      /* Fill it in */
      h->root.u.defext.vol->referencing_address = currPC;
      h->root.u.defext.vol->referencing_section = input_section;
      h->root.u.defext.vol->hash_entry = &h->root;
      h->root.u.defext.vol->ref_chain = NULL;

      if (info->stub_list_head != NULL) 
	{
          stub_list_tail->ref_chain = h->root.u.defext.vol;
          stub_list_tail = h->root.u.defext.vol;
	}
      else
	{
          stub_list_tail = info->stub_list_head = h->root.u.defext.vol;
	}

    no_action:;
  }
}

/* Given the appropriate link order, generate branch stub instructions */
/*ARGSUSED*/
static boolean
alpha_emit_stubs (output_bfd, info, output_section, link_order)
     bfd *output_bfd;
     struct bfd_link_info *info;
     asection *output_section;
     struct bfd_link_order *link_order;
{
  insn_word *space;
  int i;
  boolean result;
  int count;
  struct coff_link_hash_entry *h;
  asection *sec;
  bfd_vma val;
  bfd_vma ImageBase = pe_data(output_bfd)->pe_opthdr.ImageBase;

  BFD_ASSERT ((output_section->flags & SEC_HAS_CONTENTS) != 0);

  space = (long *) bfd_malloc (link_order->size);

  if (space == NULL)
    return false;

  count = link_order->u.stubs.s->entries_count;

  /* Usually... Emit 3 instruction sequence (in 4 word chunk)
     ldah  $27,<high part>
     lda   $27,($27)<low part>
     jmp   ($27)

     This is fine for NT on the alpha (32 bit addresses), but won't cut it 
     for bigger configurations.

     If the branch proves to be in range (from here) of a short branch,
     use one.  It doesn't end up saving any space, but it will be faster.
  */
  for (i = 0; i < count; i++)
    {
       bfd_vma currPC;

       h = (struct coff_link_hash_entry *)link_order->u.stubs.s->stublist[i];
       if (h->root.type == bfd_link_hash_defined_ext
	  || h->root.type == bfd_link_hash_defweak_ext)
	 {
	   sec = h->root.u.defext.perm->section;
	   val = (h->root.u.defext.perm->value
	      + sec->output_section->vma
	      + sec->output_offset);
	 }
       else
	   abort();

       currPC = link_order->offset
		+ output_section->vma
	        + STUB_WORDS*i*sizeof(insn_word);

       /* Never short for dynamic symbol */
       if (INRANGE(val, currPC, info->jump_span))
	 {
	    long disp;

	    disp = (long)((val >> 2) & 0x3fffffff) 
		 - (long)((currPC >> 2) & 0x3fffffff)
		 - 1;
	    space[STUB_WORDS*i] = 0xc3e00000 | (disp & 0x1fffff);
	    space[STUB_WORDS*i+1] = 0;
	    space[STUB_WORDS*i+2] = 0;
	    space[STUB_WORDS*i+3] = 0;
	 }
       else
	 {
           val += ImageBase;
	   /* Tweak the value to deal with sign extension (see REFHI above) */
	   if ((val & 0x8000) != 0)
	      val += 0x10000;
	   space[STUB_WORDS*i]   = 0x277f0000 + ((val >> 16) & 0xffff);
	   space[STUB_WORDS*i+1] = 0x237b0000 + (val&0xffff);
	   space[STUB_WORDS*i+2] = 0x6bfb0000;
	   space[STUB_WORDS*i+3] = 0;

	   if (info->base_file)
	     {
	       /* Emit a reloc if the backend thinks it needs it.
		  In this case, it always does because we know it's a branch */
	       if (sec)
		 {
		   /* relocation to a symbol in a section which
		      isn't absolute - we output the address here 
		      to a file; however, to do this right we need a type */
		   bfd_vma addr = sizeof(insn_word)*(STUB_WORDS*i)
		     - sec->vma 
		     + sec->output_offset 
		     + sec->output_section->vma;
		   fwrite (&addr, 1, sizeof(bfd_vma), (FILE *) info->base_file);

		   addr = sizeof(insn_word)*(STUB_WORDS*i+1)
		     - sec->vma 
		     + sec->output_offset 
		     + sec->output_section->vma;
		   fwrite (&addr, 1, sizeof(bfd_vma), (FILE *) info->base_file);
		 }
	     }
	 }
     }

  result = bfd_set_section_contents (output_bfd, output_section, space,
				     (file_ptr) link_order->offset,
				     link_order->size);
  free (space);
  return result;
}
