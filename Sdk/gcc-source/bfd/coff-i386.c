/* BFD back-end for Intel 386 COFF files.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002
   Free Software Foundation, Inc.
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

#include "coff/i386.h"

#include "coff/internal.h"

#ifdef COFF_WITH_PE
#include "coff/pe.h"
#endif

#ifdef COFF_GO32_EXE
#include "coff/go32exe.h"
#endif

#include "libcoff.h"

static bfd_reloc_status_type coff_i386_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *coff_i386_rtype_to_howto
  PARAMS ((bfd *, asection *, struct internal_reloc *,
	   struct coff_link_hash_entry *, struct internal_syment *,
	   bfd_vma *));
static reloc_howto_type *coff_i386_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));

/* This limits the alignment of common to this power; 8 byte alignment is
   what's "good" on Pentiums, etc. */
#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (3)
/* The page size is a guess based on ELF.  */

#define COFF_PAGE_SIZE 0x1000

/* For some reason when using i386 COFF the value stored in the .text
   section for a reference to a common symbol is the value itself plus
   any desired offset.  Ian Taylor, Cygnus Support.  */

/* If we are producing relocateable output, we need to do some
   adjustments to the object file that are not done by the
   bfd_perform_relocation function.  This function is called by every
   reloc type to make any required adjustments.  */

static bfd_reloc_status_type
coff_i386_reloc (abfd, reloc_entry, symbol, data, input_section, output_bfd,
		 error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  symvalue diff;

#ifndef COFF_WITH_PE
  if (output_bfd == (bfd *) NULL)
    return bfd_reloc_continue;
#endif

  if (bfd_is_com_section (symbol->section))
    {
#ifndef COFF_WITH_PE
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
#else
      /* In PE mode, we do not offset the common symbol.  */
      diff = reloc_entry->addend;
#endif
    }
  else
    {
      /* For some reason bfd_perform_relocation always effectively
	 ignores the addend for a COFF target when producing
	 relocateable output.  This seems to be always wrong for 386
	 COFF, so we handle the addend here instead.  */
#ifdef COFF_WITH_PE
      if (output_bfd == (bfd *) NULL)
	{
	  reloc_howto_type *howto = reloc_entry->howto;

	  /* Although PC relative relocations are very similar between
	     PE and non-PE formats, but they are off by 1 << howto->size
	     bytes. For the external relocation, PE is very different
	     from others. See md_apply_fix3 () in gas/config/tc-i386.c.
	     When we link PE and non-PE object files together to
	     generate a non-PE executable, we have to compensate it
	     here.  */
	  if (howto->pc_relative && howto->pcrel_offset)
	    diff = -(1 << howto->size);
	  else
	    diff = -reloc_entry->addend;
	}
      else
#endif
	diff = reloc_entry->addend;
    }

#ifdef COFF_WITH_PE
  /* FIXME: How should this case be handled?  */
  if (reloc_entry->howto->type == R_IMAGEBASE
      && output_bfd != NULL
      && bfd_get_flavour(output_bfd) == bfd_target_coff_flavour)
    diff -= pe_data (output_bfd)->pe_opthdr.ImageBase;
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
	      bfd_put_16 (abfd, (bfd_vma) x, addr);
	    }
	    break;

	  case 2:
	    {
	      long x = bfd_get_32 (abfd, addr);
	      DOIT (x);
	      bfd_put_32 (abfd, (bfd_vma) x, addr);
	    }
	    break;

	  default:
	    abort ();
	  }
      }

  /* Now let bfd_perform_relocation finish everything up.  */
  return bfd_reloc_continue;
}

#ifdef COFF_WITH_PE
/* Return true if this relocation should appear in the output .reloc
   section.  */

static boolean in_reloc_p PARAMS ((bfd *, reloc_howto_type *));

static boolean in_reloc_p (abfd, howto)
     bfd * abfd ATTRIBUTE_UNUSED;
     reloc_howto_type *howto;
{
  return ! howto->pc_relative && howto->type != R_IMAGEBASE;
}
#endif /* COFF_WITH_PE */

#ifndef PCRELOFFSET
#define PCRELOFFSET false
#endif

static reloc_howto_type howto_table[] =
{
  EMPTY_HOWTO (0),
  EMPTY_HOWTO (1),
  EMPTY_HOWTO (2),
  EMPTY_HOWTO (3),
  EMPTY_HOWTO (4),
  EMPTY_HOWTO (5),
  HOWTO (R_DIR32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */
	 "dir32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 true),			/* pcrel_offset */
  /* PE IMAGE_REL_I386_DIR32NB relocation (7).	*/
  HOWTO (R_IMAGEBASE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */
	 "rva32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */
  EMPTY_HOWTO (010),
  EMPTY_HOWTO (011),
  EMPTY_HOWTO (012),
  EMPTY_HOWTO (013),
  EMPTY_HOWTO (014),
  EMPTY_HOWTO (015),
  EMPTY_HOWTO (016),
  /* Byte relocation (017).  */
  HOWTO (R_RELBYTE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */
	 "8",			/* name */
	 true,			/* partial_inplace */
	 0x000000ff,		/* src_mask */
	 0x000000ff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */
  /* 16-bit word relocation (020).  */
  HOWTO (R_RELWORD,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */
	 "16",			/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */
  /* 32-bit longword relocation (021).	*/
  HOWTO (R_RELLONG,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */
	 "32",			/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */
  /* Byte PC relative relocation (022).	 */
  HOWTO (R_PCRBYTE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */
	 "DISP8",		/* name */
	 true,			/* partial_inplace */
	 0x000000ff,		/* src_mask */
	 0x000000ff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */
  /* 16-bit word PC relative relocation (023).	*/
  HOWTO (R_PCRWORD,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */
	 "DISP16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */
  /* 32-bit longword PC relative relocation (024).  */
  HOWTO (R_PCRLONG,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */
	 "DISP32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */

#ifdef DYNAMIC_LINKING /* [ */
  {025,0,0,0,0,0,0,0,"",0,0,0,0},
  {026,0,0,0,0,0,0,0,"",0,0,0,0},
  {027,0,0,0,0,0,0,0,"",0,0,0,0},
  HOWTO (R_GNU_GOT32,		/* type */                                 
	 0,			/* rightshift */                           
	 2,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 32,			/* bitsize */                   
	 false,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "GOT32",		/* name */                                 
	 true,			/* partial_inplace */                      
	 0xffffffff,		/* src_mask */                             
	 0xffffffff,		/* dst_mask */                             
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_GNU_PLT32,		/* type */                                 
	 0,			/* rightshift */                           
	 2,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 32,			/* bitsize */                   
	 true,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "PLT32",		/* name */                                 
	 true,			/* partial_inplace */                      
	 0xffffffff,		/* src_mask */                             
	 0xffffffff,		/* dst_mask */                             
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_GNU_COPY,		/* type 032 */                                  
	 0,			/* rightshift */                           
	 2,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 32,			/* bitsize */                   
	 false,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "COPY",		/* name */                                 
	 true,			/* partial_inplace */                      
	 0xffffffff,		/* src_mask */                             
	 0xffffffff,		/* dst_mask */                             
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_GNU_GLOB_DAT,	/* type 033 */                                 
	 0,			/* rightshift */                           
	 2,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 32,			/* bitsize */                   
	 false,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "GLOB_DAT",		/* name */                                 
	 true,			/* partial_inplace */                      
	 0xffffffff,		/* src_mask */                             
	 0xffffffff,		/* dst_mask */                             
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_GNU_JUMP_SLOT,	/* type 034 */                                 
	 0,			/* rightshift */                           
	 2,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 32,			/* bitsize */                   
	 true,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "JUMP_SLOT",		/* name */                                 
	 true,			/* partial_inplace */                      
	 0xffffffff,		/* src_mask */                             
	 0xffffffff,		/* dst_mask */                             
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_GNU_RELATIVE,	/* type 035 */                                 
	 0,			/* rightshift */                           
	 2,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 32,			/* bitsize */                   
	 true,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "RELATIVE",		/* name */                                 
	 true,			/* partial_inplace */                      
	 0xffffffff,		/* src_mask */                             
	 0xffffffff,		/* dst_mask */                             
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_GNU_GOTOFF,		/* type */                                 
	 0,			/* rightshift */                           
	 2,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 32,			/* bitsize */                   
	 false,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "GOTOFF",		/* name */                                 
	 true,			/* partial_inplace */                      
	 0xffffffff,		/* src_mask */                             
	 0xffffffff,		/* dst_mask */                             
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_GNU_GOTPC,		/* type */                                 
	 0,			/* rightshift */                           
	 2,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 32,			/* bitsize */                   
	 true,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "GOTPC",		/* name */                                 
	 true,			/* partial_inplace */                      
	 0xffffffff,		/* src_mask */                             
	 0xffffffff,		/* dst_mask */                             
	 PCRELOFFSET),		/* pcrel_offset */
#endif /* ] */
};

/* Turn a howto into a reloc  nunmber */

#define SELECT_RELOC(x,howto) { x.r_type = howto->type; }
#define BADMAG(x) I386BADMAG(x)
#define I386 1			/* Customize coffcode.h */

#define RTYPE2HOWTO(cache_ptr, dst)					\
  ((cache_ptr)->howto =							\
   ((dst)->r_type < sizeof (howto_table) / sizeof (howto_table[0])	\
    ? howto_table + (dst)->r_type					\
    : NULL))

/* For 386 COFF a STYP_NOLOAD | STYP_BSS section is part of a shared
   library.  On some other COFF targets STYP_BSS is normally
   STYP_NOLOAD.  */
#define BSS_NOLOAD_IS_SHARED_LIBRARY

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
    if (ptr && howto_table[reloc.r_type].pc_relative)		\
      cache_ptr->addend += asect->vma;				\
  }

/* We use the special COFF backend linker.  */
#define coff_relocate_section _bfd_coff_generic_relocate_section

/* Convert an rtype to howto for the COFF backend linker.  */

static reloc_howto_type *
coff_i386_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     struct internal_reloc *rel;
     struct coff_link_hash_entry *h;
     struct internal_syment *sym;
     bfd_vma *addendp;
{
  reloc_howto_type *howto;

  if (rel->r_type > sizeof (howto_table) / sizeof (howto_table[0]))
    {
      bfd_set_error (bfd_error_bad_value);
      return NULL;
    }

  howto = howto_table + rel->r_type;

#ifdef COFF_WITH_PE
  /* Cancel out code in _bfd_coff_generic_relocate_section.  */
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
      /* I think we *do* want to bypass this.  If we don't, I have
	 seen some data parameters get the wrong relocation address.
	 If I link two versions with and without this section bypassed
	 and then do a binary comparison, the addresses which are
	 different can be looked up in the map.  The case in which
	 this section has been bypassed has addresses which correspond
	 to values I can find in the map.  */
      *addendp -= sym->n_value;
#endif
    }

#ifndef COFF_WITH_PE
  /* If the output symbol is common (in which case this must be a
     relocateable link), we need to add in the final size of the
     common symbol.  */
  if (h != NULL && h->root.type == bfd_link_hash_common)
    *addendp += h->root.u.c.size;
#endif

#ifdef COFF_WITH_PE
  if (howto->pc_relative)
    {
      *addendp -= 4;

      /* If the symbol is defined, then the generic code is going to
         add back the symbol value in order to cancel out an
         adjustment it made to the addend.  However, we set the addend
         to 0 at the start of this function.  We need to adjust here,
         to avoid the adjustment the generic code will make.  FIXME:
         This is getting a bit hackish.  */
      if (sym != NULL && sym->n_scnum != 0)
	*addendp -= sym->n_value;
    }

  if (rel->r_type == R_IMAGEBASE
      && (bfd_get_flavour(sec->output_section->owner)
	  == bfd_target_coff_flavour))
    {
      *addendp -= pe_data(sec->output_section->owner)->pe_opthdr.ImageBase;
    }
#endif

  return howto;
}

#define coff_bfd_reloc_type_lookup coff_i386_reloc_type_lookup

static reloc_howto_type *
coff_i386_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    case BFD_RELOC_RVA:
      return howto_table + R_IMAGEBASE;
    case BFD_RELOC_32:
      return howto_table + R_DIR32;
    case BFD_RELOC_32_PCREL:
      return howto_table + R_PCRLONG;
    case BFD_RELOC_16:
      return howto_table + R_RELWORD;
    case BFD_RELOC_16_PCREL:
      return howto_table + R_PCRWORD;
    case BFD_RELOC_8:
      return howto_table + R_RELBYTE;
    case BFD_RELOC_8_PCREL:
      return howto_table + R_PCRBYTE;
#ifdef DYNAMIC_LINKING
    case BFD_RELOC_386_GOT32:
    case BFD_RELOC_386_PLT32:
    case BFD_RELOC_386_GOTOFF:
    case BFD_RELOC_386_GOTPC:
      return howto_table + (R_GNU_GOT32 + (code - BFD_RELOC_386_GOT32));
#endif
    default:
      BFD_FAIL ();
      return 0;
    }
}

#define coff_rtype_to_howto coff_i386_rtype_to_howto

#ifdef DYNAMIC_LINKING /* [ */

static boolean coff_i386_link_create_dynamic_sections 
      PARAMS (( bfd *, struct bfd_link_info *));
static boolean coff_i386_check_relocs 
      PARAMS (( bfd *, struct bfd_link_info *, asection *, 
		const struct internal_reloc *relocs));
static boolean coff_i386_adjust_dynamic_symbol 
      PARAMS (( bfd*, struct bfd_link_info *, struct coff_link_hash_entry *,
		boolean));
static boolean coff_i386_finish_dynamic_symbol 
      PARAMS (( bfd *, struct bfd_link_info *, struct coff_link_hash_entry *,
                struct internal_syment *));
static boolean coff_i386_finish_dynamic_sections 
      PARAMS (( bfd *, struct bfd_link_info *));

#define coff_backend_link_create_dynamic_sections \
             coff_i386_link_create_dynamic_sections
#define coff_backend_check_relocs coff_i386_check_relocs
#define coff_backend_adjust_dynamic_symbol coff_i386_adjust_dynamic_symbol
#define coff_backend_size_dynamic_sections pei_generic_size_dynamic_sections
#define coff_backend_finish_dynamic_symbol coff_i386_finish_dynamic_symbol
#define coff_backend_finish_dynamic_sections coff_i386_finish_dynamic_sections

#endif /* ] */

#ifdef TARGET_UNDERSCORE

/* If i386 gcc uses underscores for symbol names, then it does not use
   a leading dot for local labels, so if TARGET_UNDERSCORE is defined
   we treat all symbols starting with L as local.  */

static boolean coff_i386_is_local_label_name PARAMS ((bfd *, const char *));

static boolean
coff_i386_is_local_label_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  if (name[0] == 'L')
    return true;

  return _bfd_coff_is_local_label_name (abfd, name);
}

#define coff_bfd_is_local_label_name coff_i386_is_local_label_name

#endif /* TARGET_UNDERSCORE */

#include "coffcode.h"


#ifdef DYNAMIC_LINKING /* [ */

/* The size in bytes of an entry in the procedure linkage table.  */

#define PLT_ENTRY_SIZE 16

/* The first entry in a (PIC) procedure linkage table look like this.  */

static const bfd_byte coff_i386_pic_plt0_entry[PLT_ENTRY_SIZE] =
{
  0xff, 0xb3, 4, 0, 0, 0,	/* pushl 4(%ebx) */
  0xff, 0xa3, 8, 0, 0, 0,	/* jmp *8(%ebx) */
  0, 0, 0, 0			/* pad out to 16 bytes.  */
};

/* Subsequent entries in a (PIC) procedure linkage table look like this.  */

static const bfd_byte coff_i386_pic_plt_entry[PLT_ENTRY_SIZE] =
{
  0xff, 0xa3,	/* jmp *offset(%ebx) */
  0, 0, 0, 0,	/* replaced with offset of this symbol in .got.  */
  0x68,		/* pushl immediate */
  0, 0, 0, 0,	/* replaced with offset into relocation table.  */
  0xe9,		/* jmp relative */
  0, 0, 0, 0	/* replaced with offset to start of .plt.  */
};

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
coff_i386_link_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;
  struct coff_link_hash_entry *h = NULL;

  /* We need to create .plt, .rel.plt, .got, .got.plt, .dynbss, and
     .rel.bss sections.  */

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  s = bfd_make_section (abfd, ".plt");
  coff_hash_table (info)->splt = s;
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s,
				  (flags | SEC_CODE | SEC_READONLY))
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

  if (! _bfd_coff_create_got_section (abfd, info, GOT_SYM, true))
    return false;

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

  /* The .rel.bss section holds copy relocs.  This section is not
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
coff_i386_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const struct internal_reloc *relocs;
{
  bfd *dynobj;
  bfd_vma *local_got_offsets;
  const struct internal_reloc *rel;
  const struct internal_reloc *rel_end;
  asection *sgot;
  asection *srelgot;
  asection *sreloc;

  if (info->relocateable)
    return true;

  /* we don't do this for .stabs (or .stabstr). */
  if (strncmp(sec->name, ".stab",5) == 0)
    return true;

  dynobj = coff_hash_table (info)->dynobj;
  local_got_offsets = coff_local_got_offsets (abfd);

  sgot = NULL;
  srelgot = NULL;
  sreloc = NULL;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      long r_symndx;
      struct coff_link_hash_entry *h;

      r_symndx = rel->r_symndx;

      if (r_symndx == -1)
	h = NULL;
      else
	h = obj_coff_sym_hashes(abfd)[r_symndx];

      switch (rel->r_type)
	{
	case R_GNU_GOTOFF:
	case R_GNU_GOTPC:
	case R_GNU_GOT32:
	  /* This symbol/reloc requires at least the existence of a GOT */
	  if (dynobj == NULL)
	      coff_hash_table (info)->dynobj = dynobj = abfd;

	  if (sgot == NULL)
	      sgot = coff_hash_table(info)->sgot;

	  if (sgot == NULL)
	    {
	      if (! _bfd_coff_create_got_section (dynobj, info, GOT_SYM, true))
		return false;
	      sgot = coff_hash_table(info)->sgot;
	    }


	  if (rel->r_type != R_GNU_GOT32)
	      break;

	  /* This symbol requires a real global offset table entry (and
	     a relocation for it).  */
	  if (srelgot == NULL
	      && (h != NULL || info->shared))
	    {
	      srelgot = coff_hash_table(info)->srelgot;
	      if (srelgot == NULL)
		{
		  srelgot = bfd_make_section (dynobj, ".rel.got");
                  coff_hash_table(info)->srelgot = srelgot;
		  if (srelgot == NULL
		      || ! bfd_set_section_flags (dynobj, srelgot,
						  (SEC_ALLOC
						   | SEC_LOAD
						   | SEC_HAS_CONTENTS
						   | SEC_IN_MEMORY
						   | SEC_LINKER_CREATED
						   | SEC_READONLY))
		      || ! bfd_set_section_alignment (dynobj, srelgot, 1))
		    return false;
		}
	    }

	  /* If it has a GOT relocation, it needs a GOT offset.  If it
	     has a symbol table entry, it goes there, otherwise we build
	     a special table (for locals). */
	  if (h != NULL)
	    {
	      if (h->got_offset != (bfd_vma) -1)
		{
		  /* We have already allocated space in the .got.  */
		  break;
		}
	      h->got_offset = sgot->_raw_size;

	      /* Make sure this symbol is output as a dynamic symbol.  */
	      if (h->dynindx == -1)
		{
		  if (! _bfd_coff_link_record_dynamic_symbol (info, h))
		    return false;
		}

// fprintf(stderr, "relgot adds reloc #%d in slot %d, %s\n", srelgot->_raw_size/10, h->got_offset/4, h->root.root.string); //
	      srelgot->_raw_size += bfd_coff_relsz (abfd);
	    }
	  else
	    {
     	      /* This is a global offset table entry for a local
                 symbol.  */
	      if (local_got_offsets == NULL)
		{
		  size_t size;
		  register unsigned int i;

		  size = obj_raw_syment_count(abfd) * sizeof (bfd_vma);
		  local_got_offsets = (bfd_vma *) bfd_alloc (abfd, size);
		  if (local_got_offsets == NULL)
		    return false;
		  coff_local_got_offsets (abfd) = local_got_offsets;
		  for (i = 0; i <obj_raw_syment_count(abfd); i++)
		    local_got_offsets[i] = (bfd_vma) -1;
		}
	      if (local_got_offsets[r_symndx] != (bfd_vma) -1)
		{
		  /* We have already allocated space in the .got.  */
		  break;
		}
	      local_got_offsets[r_symndx] = sgot->_raw_size;

	      if (info->shared)
		{
		  /* If we are generating a shared object, we need to
                     output a R_GNU_RELATIVE reloc so that the dynamic
                     linker can adjust this GOT entry.  */
		  srelgot->_raw_size += bfd_coff_relsz (abfd);
// fprintf(stderr, "relgot adds anonymous\n"); //
		}
	    }

	  sgot->_raw_size += 4;

	  break;

	case R_GNU_PLT32:
	  /* This symbol requires a procedure linkage table entry.  We
             actually build the entry in adjust_dynamic_symbol,
             because this might be a case of linking PIC code which is
             never referenced by a dynamic object, in which case we
             don't need to generate a procedure linkage table entry
             after all.  */

	  /* If this is a local symbol, we resolve it directly without
             creating a procedure linkage table entry.  */
	  if (h == NULL)
	    continue;

	  if (dynobj == NULL)
	      coff_hash_table (info)->dynobj = dynobj = abfd;

	  h->coff_link_hash_flags |= COFF_LINK_HASH_NEEDS_PLT;

	  break;

	case R_PCRLONG:
	  /* local symbols don't get dynamic PCRLONG relocs */
	  if (h == NULL)
	      break;

	  /* Symbolicly linked shared libs may not propigate all PCRLONG 
	     relocations.  For defined symbols they end up just being 
	     normal branches.  (When it's not symbolic, they are subject 
	     to dynamic symbol resolution, so each needs a relocation entry.)  

	     Undefined symbols still need an entry, in case someone else 
	     defines them.  Since we don't yet know for sure whether it's
	     undefined or not, we count them separately. */

	  /* drop thru */
        case R_DIR32:

	  if (dynobj == NULL)
	      coff_hash_table (info)->dynobj = dynobj = abfd;

	  /* DIR32 relocations always need some sort of relocation,
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
      		  if (rel->r_type == R_PCRLONG)
		      h->num_relative_relocs_needed++;
		  else
		      h->num_long_relocs_needed++;
		}
#ifdef USE_COPY_RELOC
	      else
#else
	      else if (info->shared)
#endif
		{
	          sreloc->_raw_size += bfd_coff_relsz (abfd);
		}
	    }

	  break;

	default:
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
coff_i386_adjust_dynamic_symbol (dynobj, info, h, skip)
     struct bfd_link_info *info;
     struct coff_link_hash_entry *h;
     bfd *dynobj;
     boolean skip;
{
  asection *s;
#ifdef USE_COPY_RELOC
  unsigned int power_of_two;
#endif
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
     know the symbol type.)  (N.B.: we can occasionally have dynamic symbol
     that we reference (and thus need to count) that isn't defined, if
     it's coming via an indirect shared lib.)

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
             || (h->dynindx != -1 && h->root.type == bfd_link_hash_undefined)
             || info->shared)
        )
    {
#undef DEBUG_COUNTING
#ifdef DEBUG_COUNTING //!!
int did_print=0;
if (h->num_long_relocs_needed || h->num_relative_relocs_needed) 
{
did_print = 1;
fprintf(stderr, "%s (%x) adds %d+%d(?)", h->root.root.string, h->coff_link_hash_flags, h->num_long_relocs_needed, h->num_relative_relocs_needed); //!!
}
#endif
      /* We'll need a dynamic reloc for 'long relocs' passed above (whether
	 it'll be symbolic or relative is determined by the relocation code)
	 and whether it's a suppressed symbol or not (suppressed either
	 explicitly, or because it's symbolic mode).  For suppressed
	 symbols, if they're locally defined and absolute, no relocation.
	 (Read || as "or else if" below, it'll make more sense.) */
      if (! info->shared 
	  || !(info->symbolic || h->dynindx == -1)
	  || !((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0)
	  || !bfd_is_abs_section(h->root.u.def.section)
	  )
	{
          sreloc->_raw_size += 
	      (h->num_long_relocs_needed * bfd_coff_relsz (dynobj));
          h->num_long_relocs_needed = -h->num_long_relocs_needed;
	}
      /* relative relocs are needed in shared but not for suppressed symbols,
	 but undefined suppressed symbols do get a reloc. We couldn't
	 tell about undefined when we first counted them, but now we can.
	 Note... these tests really are different; things fail if not. */
      if (
#ifdef USE_DLLS
	  (h->coff_link_hash_flags & COFF_LINK_HASH_DLL_DEFINED) == 0
#endif
	  && (! info->shared 
	     || !(info->symbolic || h->dynindx == -1)
	     || !((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0)
	     || h->root.type == bfd_link_hash_undefined)
	  )
	{
          sreloc->_raw_size += 
	      (h->num_relative_relocs_needed * bfd_coff_relsz (dynobj));
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
     will fill in the contents of the procedure linkage table later,
     when we know the address of the .got section.  */
  if (ISFCN(h->type)
      || (h->coff_link_hash_flags & COFF_LINK_HASH_NEEDS_PLT) != 0)
    {
      if (! info->shared
	  && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) == 0
	  && (h->coff_link_hash_flags & COFF_LINK_HASH_REF_DYNAMIC) == 0)
	{
	  /* This case can occur if we saw a PLT32 reloc in an input
             file, but the symbol was never mentioned by a dynamic
             object.  In such a case, we don't actually need to build
             a procedure linkage table, and we can just do a PC32
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
	s->_raw_size += PLT_ENTRY_SIZE;

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

      /* We also need to make an entry in the .got.plt section, which
	 will be placed in the .got section by the linker script.  */

      s = coff_hash_table(info)->sgotplt;
      BFD_ASSERT (s != NULL);
      s->_raw_size += 4;

      /* We also need to make an entry in the .rel.plt section.  */

      s = coff_hash_table(info)->srelplt;
      BFD_ASSERT (s != NULL);
      s->_raw_size += bfd_coff_relsz (dynobj);
//fprintf(stderr, "relPLT adds # %d, %s\n",( s->_raw_size/bfd_coff_relsz (dynobj))-1, h->root.root.string);

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
      srel->_raw_size += bfd_coff_relsz (abfd);
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
coff_i386_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct coff_link_hash_entry *h;
     struct internal_syment *sym;
{
  bfd *dynobj;
  bfd_vma imagebase=0;
  if (pe_data(output_bfd) != NULL)
      imagebase = pe_data(output_bfd)->pe_opthdr.ImageBase;

  /* It is possible to get here with h->dynindx == -1; a symbol that is
     forced local may still need a GOT entry. */

  dynobj = coff_hash_table (info)->dynobj;

  if (h->plt_offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *sgotplt;
      asection *srelplt;
      bfd_vma plt_index;
      bfd_vma got_offset;
      struct internal_reloc rel;

      /* This symbol has an entry in the procedure linkage table.  Set
	 it up.  */

    if ((h->coff_link_hash_flags & COFF_LINK_WEAK_PLT) == 0) {
      BFD_ASSERT (h->dynindx != -1);

      splt = coff_hash_table(info)->splt;
      sgotplt = coff_hash_table(info)->sgotplt;
      srelplt = coff_hash_table(info)->srelplt;
      BFD_ASSERT (splt != NULL && sgotplt != NULL && srelplt != NULL);

      /* Get the index in the procedure linkage table which
	 corresponds to this symbol.  This is the index of this symbol
	 in all the symbols for which we are making plt entries.  The
	 first entry in the procedure linkage table is reserved.  */
      plt_index = h->plt_offset / PLT_ENTRY_SIZE - 1;

      /* Get the offset into the .got table of the entry that
	 corresponds to this function.  Each .got entry is 4 bytes.
	 The first three are reserved.  */
      got_offset = (plt_index + 3) * 4;

      /* Fill in the entry in the procedure linkage table.

	 Non-PIC code in a shared library won't go thru the PLT/GOT
	 (it gets relocated inline), so using PIC in any 
	 shared lib works.  Since the caller of the stub must be in
	 this shared lib (and thus share a single GOT value) %ebx will
	 be right when the PIC PLT entry is called. 

	 Main won't go thru the PLT/GOT unless there's PIC in it; whether
	 the PLT is PIC or not really doesn't matter, and it saves a
	 few relocation entries if it is PIC.  */

      /* No need for a base_file entry here (code is PIC) */
      memcpy (splt->contents + h->plt_offset, coff_i386_pic_plt_entry,
	      PLT_ENTRY_SIZE);
      bfd_put_32 (output_bfd, got_offset,
		  splt->contents + h->plt_offset + 2);

      /* Fill in rest of plt entry: the offset to the relocation and
	 the start of the PLT.  Tricky code, here:

	 - This becomes the branch stub/thunk for this symbol.
	 - Instruction 1 is a jump indirect to the symbol's GOT slot
	   * Initially, the GOT slot contains the address of the next
	     instruction below.
	   * After dynamic link, this is the address of the real function.
	 - Instruction 2 pushes the offset of the relocation entry.
	 - Instruction 3 jumps indirect to the start of PLT, which
	   contains:
	      pushl .got+4   # startup crams in &info struct for this lib
	      jmp   *.got+8  # startup crams in &runtime linker.
	 - .got+8 contains the address of the runtime dynamic linker.
	 - The dynamic linker is effectively called with 2 args,
	      - the reloc entry
	      - the info struct.  (This struct is private to the runtime
		  side of things.  We don't know anything about it here.)

	 Thus, until the dynamic linker has done it's thing (the first
	 call) we jump to the dynamic linker after having pushed the relocation
	 information it requires.  After that (once the GOT is modified)
	 we jump to the real routine.  */

      bfd_put_32 (output_bfd, plt_index * bfd_coff_relsz (output_bfd),
		  splt->contents + h->plt_offset + 7);
      bfd_put_32 (output_bfd, - (h->plt_offset + PLT_ENTRY_SIZE),
		  splt->contents + h->plt_offset + 12);

      /* if (info->base_file ...  do something */
      /* Fill in the entry in the global offset table.  */
      bfd_put_32 (output_bfd,
		  (splt->output_section->vma
		   + splt->output_offset
		   + h->plt_offset
		   + 6 + imagebase),
		  sgotplt->contents + got_offset);

      /* Fill in the entry in the .rel.plt section.  */
      rel.r_symndx = h->dynindx;
      rel.r_vaddr = (sgotplt->output_section->vma
		      + sgotplt->output_offset
		      + got_offset);

//fprintf(stderr, "relPLT emits # %d, %s\n", plt_index, h->root.root.string);
      rel.r_type = R_GNU_JUMP_SLOT;
      bfd_coff_swap_reloc_out (output_bfd, &rel,
			srelplt->contents + plt_index * bfd_coff_relsz (output_bfd));
      srelplt->reloc_count++;  /* used for subsequent assert */
    }

      if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  */
	  sym->n_scnum = N_UNDEF;
	  sym->n_sclass = C_EXT;
	}
    }

  if (h->got_offset != (bfd_vma) -1)
    {
      asection *sgot;
      asection *srelgot;
      struct internal_reloc rel;

      /* This symbol has an entry in the global offset table.  Set it
	 up.  */

      sgot = coff_hash_table(info)->sgot;
      srelgot = coff_hash_table(info)->srelgot;
      BFD_ASSERT (sgot != NULL && srelgot != NULL);

      rel.r_vaddr = (sgot->output_section->vma
		      + sgot->output_offset
		      + (h->got_offset &~ 1));

      /* If this is a -Bsymbolic link, and the symbol is defined
	 locally, we just want to emit a RELATIVE reloc.  The entry in
	 the global offset table will already have been initialized in
	 the relocate_section function.  */
      if ((info->shared
	     && info->symbolic
	     && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR))
	  || (h->coff_link_hash_flags & COFF_LINK_FORCED_LOCAL) != 0) 
        {
	  rel.r_type = R_GNU_RELATIVE;
	  rel.r_symndx = 0;
	}
      else
	{
          BFD_ASSERT (h->dynindx != -1);
	  bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + h->got_offset);
	  rel.r_type = R_GNU_GLOB_DAT;
	  rel.r_symndx = h->dynindx;
	}

      bfd_coff_swap_reloc_out (output_bfd, &rel,
	      (srelgot->contents + srelgot->reloc_count*bfd_coff_relsz (output_bfd)));
// fprintf(stderr, "finish relgot emits %d, reloc #%d, for slot %d, %s\n", rel.r_type, srelgot->reloc_count, h->got_offset/4, h->root.root.string); //
      ++srelgot->reloc_count;
    }

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
      bfd_coff_swap_reloc_out (output_bfd, &rel,
		      (s->contents + s->reloc_count*bfd_coff_relsz (abfd)));
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
coff_i386_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sgotplt;
  asection *sdyn;
  bfd_vma imagebase=0;

  if (pe_data(output_bfd) != NULL)
      imagebase = pe_data(output_bfd)->pe_opthdr.ImageBase;

  dynobj = coff_hash_table (info)->dynobj;

  sgotplt = coff_hash_table(info)->sgotplt;
  BFD_ASSERT (sgotplt != NULL);
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
	  asection *s;

	  bfd_coff_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;

	    case DT_PLTGOT:
	      s = coff_hash_table(info)->sgotplt;
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
	   memcpy (splt->contents, coff_i386_pic_plt0_entry, PLT_ENTRY_SIZE);
	}
    }

  /* Fill in the first three entries in the global offset table.  */
  if (sgotplt->_raw_size > 0)
    {
      /* if (info->base_file ...) */
      if (sdyn == NULL)
	bfd_put_32 (output_bfd, (bfd_vma) 0, sgotplt->contents);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset + imagebase,
		    sgotplt->contents);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgotplt->contents + 4);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgotplt->contents + 8);
    }

  return true;
}

#endif /* ] DYNAMIC_LINKING */

#ifdef INPUT_FORMAT
extern const bfd_target INPUT_FORMAT;
#define PINPUT_FORMAT &INPUT_FORMAT
#else
#define PINPUT_FORMAT NULL
#endif

const bfd_target
#ifdef TARGET_SYM
  TARGET_SYM =
#else
  i386coff_vec =
#endif
{
#ifdef TARGET_NAME
  TARGET_NAME,
#else
  "coff-i386",			/* name */
#endif
  bfd_target_coff_flavour,
  PINPUT_FORMAT,		/* format of acceptable input files for link */
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
#ifdef DYNAMIC_LINKING
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED | DYNAMIC),
#else
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
#endif

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC /* section flags */
#ifdef COFF_WITH_PE
   | SEC_LINK_ONCE | SEC_LINK_DUPLICATES | SEC_READONLY
#endif
   | SEC_CODE | SEC_DATA),

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

/* Note that we allow an object file to be treated as a core file as well.  */
    {_bfd_dummy_target, coff_object_p, /* bfd_check_format */
       bfd_generic_archive_p, coff_object_p},
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

  NULL,

  COFF_SWAP_TABLE
};
