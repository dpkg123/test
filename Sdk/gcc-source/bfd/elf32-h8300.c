/* Generic support for 32-bit ELF
   Copyright 1993, 1995, 1998, 1999, 2001, 2002
   Free Software Foundation, Inc.

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
#include "elf-bfd.h"
#include "elf/h8.h"

static reloc_howto_type *elf32_h8_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void elf32_h8_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static void elf32_h8_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rel *));
static unsigned long elf32_h8_mach
  PARAMS ((flagword));
static void elf32_h8_final_write_processing
  PARAMS ((bfd *, boolean));
static boolean elf32_h8_object_p
  PARAMS ((bfd *));
static boolean elf32_h8_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static boolean elf32_h8_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, boolean *));
static boolean elf32_h8_relax_delete_bytes
  PARAMS ((bfd *, asection *, bfd_vma, int));
static boolean elf32_h8_symbol_address_p
  PARAMS ((bfd *, asection *, bfd_vma));
static bfd_byte *elf32_h8_get_relocated_section_contents
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *,
	   bfd_byte *, boolean, asymbol **));
static bfd_reloc_status_type elf32_h8_final_link_relocate
  PARAMS ((unsigned long, bfd *, bfd *, asection *,
	   bfd_byte *, bfd_vma, bfd_vma, bfd_vma,
	   struct bfd_link_info *, asection *, int));
static boolean elf32_h8_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *,
	   bfd_byte *, Elf_Internal_Rela *,
	   Elf_Internal_Sym *, asection **));
static bfd_reloc_status_type special
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

/* This does not include any relocation information, but should be
   good enough for GDB or objdump to read the file.  */

static reloc_howto_type h8_elf_howto_table[] = {
#define R_H8_NONE_X 0
  HOWTO (R_H8_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 special,			/* special_function */
	 "R_H8_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */
#define R_H8_DIR32_X (R_H8_NONE_X + 1)
  HOWTO (R_H8_DIR32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 special,			/* special_function */
	 "R_H8_DIR32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */
#define R_H8_DIR16_X (R_H8_DIR32_X + 1)
  HOWTO (R_H8_DIR16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 special,			/* special_function */
	 "R_H8_DIR16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */
#define R_H8_DIR8_X (R_H8_DIR16_X + 1)
  HOWTO (R_H8_DIR8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 special,			/* special_function */
	 "R_H8_DIR16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x000000ff,		/* dst_mask */
	 false),		/* pcrel_offset */
#define R_H8_DIR16A8_X (R_H8_DIR8_X + 1)
  HOWTO (R_H8_DIR16A8,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 special,			/* special_function */
	 "R_H8_DIR16A8",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */
#define R_H8_DIR16R8_X (R_H8_DIR16A8_X + 1)
  HOWTO (R_H8_DIR16R8,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 special,			/* special_function */
	 "R_H8_DIR16R8",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */
#define R_H8_DIR24A8_X (R_H8_DIR16R8_X + 1)
  HOWTO (R_H8_DIR24A8,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 special,			/* special_function */
	 "R_H8_DIR24A8",	/* name */
	 true,			/* partial_inplace */
	 0xff000000,		/* src_mask */
	 0x00ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */
#define R_H8_DIR24R8_X (R_H8_DIR24A8_X + 1)
  HOWTO (R_H8_DIR24R8,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 special,			/* special_function */
	 "R_H8_DIR24R8",	/* name */
	 true,			/* partial_inplace */
	 0xff000000,		/* src_mask */
	 0x00ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */
#define R_H8_DIR32A16_X (R_H8_DIR24R8_X + 1)
  HOWTO (R_H8_DIR32A16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 special,			/* special_function */
	 "R_H8_DIR32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */
#define R_H8_PCREL16_X (R_H8_DIR32A16_X + 1)
  HOWTO (R_H8_PCREL16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 special,			/* special_function */
	 "R_H8_PCREL16",	/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true),			/* pcrel_offset */
#define R_H8_PCREL8_X (R_H8_PCREL16_X + 1)
  HOWTO (R_H8_PCREL8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 special,			/* special_function */
	 "R_H8_PCREL8",		/* name */
	 false,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 true),			/* pcrel_offset */
};

/* This structure is used to map BFD reloc codes to H8 ELF relocs.  */

struct elf_reloc_map {
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char howto_index;
};

/* An array mapping BFD reloc codes to SH ELF relocs.  */

static const struct elf_reloc_map h8_reloc_map[] = {
  { BFD_RELOC_NONE, R_H8_NONE_X },
  { BFD_RELOC_32, R_H8_DIR32_X },
  { BFD_RELOC_16, R_H8_DIR16_X },
  { BFD_RELOC_8, R_H8_DIR8_X },
  { BFD_RELOC_H8_DIR16A8, R_H8_DIR16A8_X },
  { BFD_RELOC_H8_DIR16R8, R_H8_DIR16R8_X },
  { BFD_RELOC_H8_DIR24A8, R_H8_DIR24A8_X },
  { BFD_RELOC_H8_DIR24R8, R_H8_DIR24R8_X },
  { BFD_RELOC_H8_DIR32A16, R_H8_DIR32A16_X },
  { BFD_RELOC_16_PCREL, R_H8_PCREL16_X },
  { BFD_RELOC_8_PCREL, R_H8_PCREL8_X },
};


static reloc_howto_type *
elf32_h8_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0; i < sizeof (h8_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (h8_reloc_map[i].bfd_reloc_val == code)
	return &h8_elf_howto_table[(int) h8_reloc_map[i].howto_index];
    }
  return NULL;
}

static void
elf32_h8_info_to_howto (abfd, bfd_reloc, elf_reloc)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *bfd_reloc;
     Elf32_Internal_Rela *elf_reloc;
{
  unsigned int r;
  unsigned int i;

  r = ELF32_R_TYPE (elf_reloc->r_info);
  for (i = 0; i < sizeof (h8_elf_howto_table) / sizeof (reloc_howto_type); i++)
    if (h8_elf_howto_table[i].type == r)
      {
	bfd_reloc->howto = &h8_elf_howto_table[i];
	return;
      }
  abort ();
}

static void
elf32_h8_info_to_howto_rel (abfd, bfd_reloc, elf_reloc)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *bfd_reloc;
     Elf32_Internal_Rel *elf_reloc ATTRIBUTE_UNUSED;
{
  unsigned int r;

  abort ();
  r = ELF32_R_TYPE (elf_reloc->r_info);
  bfd_reloc->howto = &h8_elf_howto_table[r];
}

/* Special handling for H8/300 relocs.
   We only come here for pcrel stuff and return normally if not an -r link.
   When doing -r, we can't do any arithmetic for the pcrel stuff, because
   we support relaxing on the H8/300 series chips.  */
static bfd_reloc_status_type
special (abfd, reloc_entry, symbol, data, input_section, output_bfd,
	 error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry ATTRIBUTE_UNUSED;
     asymbol *symbol ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  if (output_bfd == (bfd *) NULL)
    return bfd_reloc_continue;

  /* Adjust the reloc address to that in the output section.  */
  reloc_entry->address += input_section->output_offset;
  return bfd_reloc_ok;
}

/* Perform a relocation as part of a final link.  */
static bfd_reloc_status_type
elf32_h8_final_link_relocate (r_type, input_bfd, output_bfd,
			      input_section, contents, offset, value,
			      addend, info, sym_sec, is_local)
     unsigned long r_type;
     bfd *input_bfd;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd_byte *contents;
     bfd_vma offset;
     bfd_vma value;
     bfd_vma addend;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sym_sec ATTRIBUTE_UNUSED;
     int is_local ATTRIBUTE_UNUSED;
{
  bfd_byte *hit_data = contents + offset;

  switch (r_type)
    {

    case R_H8_NONE:
      return bfd_reloc_ok;

    case R_H8_DIR32:
    case R_H8_DIR32A16:
    case R_H8_DIR24A8:
      value += addend;
      bfd_put_32 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_H8_DIR16:
    case R_H8_DIR16A8:
    case R_H8_DIR16R8:
      value += addend;
      bfd_put_16 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    /* AKA R_RELBYTE */
    case R_H8_DIR8:
      value += addend;

      bfd_put_8 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_H8_DIR24R8:
      value += addend;

      /* HIT_DATA is the address for the first byte for the relocated
	 value.  Subtract 1 so that we can manipulate the data in 32bit
	 hunks.  */
      hit_data--;

      /* Clear out the top byte in value.  */
      value &= 0xffffff;

      /* Retrieve the type byte for value from the section contents.  */
      value |= (bfd_get_32 (input_bfd, hit_data) & 0xff000000);

      /* Now scribble it out in one 32bit hunk.  */
      bfd_put_32 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_H8_PCREL16:
      value -= (input_section->output_section->vma
		+ input_section->output_offset);
      value -= offset;
      value += addend;

      /* The value is relative to the start of the instruction,
	 not the relocation offset.  Subtract 2 to account for
	 this minor issue.  */
      value -= 2;

      bfd_put_16 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_H8_PCREL8:
      value -= (input_section->output_section->vma
		+ input_section->output_offset);
      value -= offset;
      value += addend;

      /* The value is relative to the start of the instruction,
	 not the relocation offset.  Subtract 1 to account for
	 this minor issue.  */
      value -= 1;

      bfd_put_8 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    default:
      return bfd_reloc_notsupported;
    }
}

/* Relocate an H8 ELF section.  */
static boolean
elf32_h8_relocate_section (output_bfd, info, input_bfd, input_section,
			   contents, relocs, local_syms, local_sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *relocs;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel, *relend;

  if (info->relocateable)
    return true;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      unsigned int r_type;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      bfd_vma relocation;
      bfd_reloc_status_type r;

      /* This is a final link.  */
      r_symndx = ELF32_R_SYM (rel->r_info);
      r_type = ELF32_R_TYPE (rel->r_info);
      h = NULL;
      sym = NULL;
      sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, sec, rel);
	}
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      relocation = (h->root.u.def.value
			    + sec->output_section->vma
			    + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset, true)))
		return false;
	      relocation = 0;
	    }
	}

      r = elf32_h8_final_link_relocate (r_type, input_bfd, output_bfd,
					input_section,
					contents, rel->r_offset,
					relocation, rel->r_addend,
					info, sec, h == NULL);

      if (r != bfd_reloc_ok)
	{
	  const char *name;
	  const char *msg = (const char *) 0;
	  arelent bfd_reloc;
	  reloc_howto_type *howto;

	  elf32_h8_info_to_howto (input_bfd, &bfd_reloc, rel);
	  howto = bfd_reloc.howto;

	  if (h != NULL)
	    name = h->root.root.string;
	  else
	    {
	      name = (bfd_elf_string_from_elf_section
		      (input_bfd, symtab_hdr->sh_link, sym->st_name));
	      if (name == NULL || *name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      if (! ((*info->callbacks->reloc_overflow)
		     (info, name, howto->name, (bfd_vma) 0,
		      input_bfd, input_section, rel->r_offset)))
		return false;
	      break;

	    case bfd_reloc_undefined:
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, name, input_bfd, input_section,
		      rel->r_offset, true)))
		return false;
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      goto common_error;

	    case bfd_reloc_notsupported:
	      msg = _("internal error: unsupported relocation error");
	      goto common_error;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous error");
	      goto common_error;

	    default:
	      msg = _("internal error: unknown error");
	      /* fall through */

	    common_error:
	      if (!((*info->callbacks->warning)
		    (info, msg, name, input_bfd, input_section,
		     rel->r_offset)))
		return false;
	      break;
	    }
	}
    }

  return true;
}

/* Object files encode the specific H8 model they were compiled
   for in the ELF flags field.

   Examine that field and return the proper BFD machine type for
   the object file.  */
static unsigned long
elf32_h8_mach (flags)
     flagword flags;
{
  switch (flags & EF_H8_MACH)
    {
    case E_H8_MACH_H8300:
    default:
      return bfd_mach_h8300;

    case E_H8_MACH_H8300H:
      return bfd_mach_h8300h;

    case E_H8_MACH_H8300S:
      return bfd_mach_h8300s;
    }
}

/* The final processing done just before writing out a H8 ELF object
   file.  We use this opportunity to encode the BFD machine type
   into the flags field in the object file.  */

static void
elf32_h8_final_write_processing (abfd, linker)
     bfd *abfd;
     boolean linker ATTRIBUTE_UNUSED;
{
  unsigned long val;

  switch (bfd_get_mach (abfd))
    {
    default:
    case bfd_mach_h8300:
      val = E_H8_MACH_H8300;
      break;

    case bfd_mach_h8300h:
      val = E_H8_MACH_H8300H;
      break;

    case bfd_mach_h8300s:
      val = E_H8_MACH_H8300S;
      break;
    }

  elf_elfheader (abfd)->e_flags &= ~ (EF_H8_MACH);
  elf_elfheader (abfd)->e_flags |= val;
}

/* Return nonzero if ABFD represents a valid H8 ELF object file; also
   record the encoded machine type found in the ELF flags.  */

static boolean
elf32_h8_object_p (abfd)
     bfd *abfd;
{
  bfd_default_set_arch_mach (abfd, bfd_arch_h8300,
			     elf32_h8_mach (elf_elfheader (abfd)->e_flags));
  return true;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  The only data we need to copy at this
   time is the architecture/machine information.  */

static boolean
elf32_h8_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  if (bfd_get_arch (obfd) == bfd_get_arch (ibfd)
      && bfd_get_mach (obfd) < bfd_get_mach (ibfd))
    {
      if (! bfd_set_arch_mach (obfd, bfd_get_arch (ibfd),
                               bfd_get_mach (ibfd)))
        return false;
    }

  return true;
}

/* This function handles relaxing for the H8..

   There's a few relaxing opportunites available on the H8:

     jmp/jsr:24    ->    bra/bsr:8		2 bytes
     The jmp may be completely eliminated if the previous insn is a
     conditional branch to the insn after the jump.  In that case
     we invert the branch and delete the jump and save 4 bytes.

     bCC:16          ->    bCC:8                  2 bytes
     bsr:16          ->    bsr:8                  2 bytes

     mov.b:16	     ->    mov.b:8                2 bytes
     mov.b:24/32     ->    mov.b:8                4 bytes

     mov.[bwl]:24/32 ->    mov.[bwl]:16           2 bytes */

static boolean
elf32_h8_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     boolean *again;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *contents = NULL;
  Elf_Internal_Sym *isymbuf = NULL;
  static asection *last_input_section = NULL;
  static Elf_Internal_Rela *last_reloc = NULL;

  /* Assume nothing changes.  */
  *again = false;

  /* We don't have to do anything for a relocateable link, if
     this section does not have relocs, or if this is not a
     code section.  */
  if (link_info->relocateable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0)
    return true;

  /* If this is the first time we have been called for this section,
     initialize the cooked size.  */
  if (sec->_cooked_size == 0)
    sec->_cooked_size = sec->_raw_size;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  /* Get a copy of the native relocations.  */
  internal_relocs = (_bfd_elf32_link_read_relocs
		     (abfd, sec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;

  if (sec != last_input_section)
    last_reloc = NULL;

  last_input_section = sec;

  /* Walk through the relocs looking for relaxing opportunities.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;

      /* Keep track of the previous reloc so that we can delete
	 some long jumps created by the compiler.  */
      if (irel != internal_relocs)
	last_reloc = irel - 1;

      if (ELF32_R_TYPE (irel->r_info) != R_H8_DIR24R8
	  && ELF32_R_TYPE (irel->r_info) != R_H8_PCREL16
	  && ELF32_R_TYPE (irel->r_info) != R_H8_DIR16A8
	  && ELF32_R_TYPE (irel->r_info) != R_H8_DIR24A8
	  && ELF32_R_TYPE (irel->r_info) != R_H8_DIR32A16)
	continue;

      /* Get the section contents if we haven't done so already.  */
      if (contents == NULL)
	{
	  /* Get cached copy if it exists.  */
	  if (elf_section_data (sec)->this_hdr.contents != NULL)
	    contents = elf_section_data (sec)->this_hdr.contents;
	  else
	    {
	      /* Go get them off disk.  */
	      contents = (bfd_byte *) bfd_malloc (sec->_raw_size);
	      if (contents == NULL)
		goto error_return;

	      if (! bfd_get_section_contents (abfd, sec, contents,
					      (file_ptr) 0, sec->_raw_size))
		goto error_return;
	    }
	}

      /* Read this BFD's local symbols if we haven't done so already.  */
      if (isymbuf == NULL && symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (abfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    goto error_return;
	}

      /* Get the value of the symbol referred to by the reloc.  */
      if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  /* A local symbol.  */
	  Elf_Internal_Sym *isym;
	  asection *sym_sec;

	  isym = isymbuf + ELF32_R_SYM (irel->r_info);
	  sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
	  symval = (isym->st_value
		    + sym_sec->output_section->vma
		    + sym_sec->output_offset);
	}
      else
	{
	  unsigned long indx;
	  struct elf_link_hash_entry *h;

	  /* An external symbol.  */
	  indx = ELF32_R_SYM (irel->r_info) - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  BFD_ASSERT (h != NULL);
	  if (h->root.type != bfd_link_hash_defined
	      && h->root.type != bfd_link_hash_defweak)
	    {
	      /* This appears to be a reference to an undefined
                 symbol.  Just ignore it--it will be caught by the
                 regular reloc processing.  */
	      continue;
	    }

	  symval = (h->root.u.def.value
		    + h->root.u.def.section->output_section->vma
		    + h->root.u.def.section->output_offset);
	}

      /* For simplicity of coding, we are going to modify the section
	 contents, the section relocs, and the BFD symbol table.  We
	 must tell the rest of the code not to free up this
	 information.  It would be possible to instead create a table
	 of changes which have to be made, as is done in coff-mips.c;
	 that would be more work, but would require less memory when
	 the linker is run.  */
      switch (ELF32_R_TYPE (irel->r_info))
	{
        /* Try to turn a 24 bit absolute branch/call into an 8 bit
	   pc-relative branch/call.  */
	case R_H8_DIR24R8:
	  {
	    bfd_vma value = symval + irel->r_addend;
	    bfd_vma dot, gap;

	    /* Get the address of this instruction.  */
	    dot = (sec->output_section->vma
		   + sec->output_offset + irel->r_offset - 1);

	    /* Compute the distance from this insn to the branch target.  */
	    gap = value - dot;

	    /* If the distance is within -126..+130 inclusive, then we can
	       relax this jump.  +130 is valid since the target will move
	       two bytes closer if we do relax this branch.  */
	    if ((int) gap >= -126 && (int) gap <= 130)
	      {
		unsigned char code;

		/* Note that we've changed the relocs, section contents,
		   etc.  */
		elf_section_data (sec)->relocs = internal_relocs;
		elf_section_data (sec)->this_hdr.contents = contents;
		symtab_hdr->contents = (unsigned char *) isymbuf;

		/* If the previous instruction conditionally jumped around
		   this instruction, we may be able to reverse the condition
		   and redirect the previous instruction to the target of
		   this instruction.

		   Such sequences are used by the compiler to deal with
		   long conditional branches.  */
		if ((int) gap <= 130
		    && (int) gap >= -128
		    && last_reloc
		    && ELF32_R_TYPE (last_reloc->r_info) == R_H8_PCREL8
		    && ELF32_R_SYM (last_reloc->r_info) < symtab_hdr->sh_info)
		  {
		    bfd_vma last_value;
		    asection *last_sym_sec;
		    Elf_Internal_Sym *last_sym;

		    /* We will need to examine the symbol used by the
		       previous relocation.  */

		    last_sym = isymbuf + ELF32_R_SYM (last_reloc->r_info);
		    last_sym_sec
		      = bfd_section_from_elf_index (abfd, last_sym->st_shndx);
		    last_value = (last_sym->st_value
				  + last_sym_sec->output_section->vma
				  + last_sym_sec->output_offset);

		    /* Verify that the previous relocation was for a
		       branch around this instruction and that no symbol
		       exists at the current location.  */
		    if (last_value == dot + 4
			&& last_reloc->r_offset + 2 == irel->r_offset
			&& ! elf32_h8_symbol_address_p (abfd, sec, dot))
		      {
			/* We can eliminate this jump.  Twiddle the
			   previous relocation as necessary.  */
			irel->r_info
			  = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					  ELF32_R_TYPE (R_H8_NONE));

			last_reloc->r_info
			  = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					  ELF32_R_TYPE (R_H8_PCREL8));
			last_reloc->r_addend = irel->r_addend;

			code = bfd_get_8 (abfd,
					  contents + last_reloc->r_offset - 1);
			code ^= 1;
			bfd_put_8 (abfd,
				   code,
			contents + last_reloc->r_offset - 1);

			/* Delete four bytes of data.  */
			if (!elf32_h8_relax_delete_bytes (abfd, sec,
							  irel->r_offset - 1,
							  4))
			  goto error_return;

			*again = true;
			break;
		      }
		  }

		/* We could not eliminate this jump, so just shorten it.  */
		code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

		if (code == 0x5e)
		  bfd_put_8 (abfd, 0x55, contents + irel->r_offset - 1);
		else if (code == 0x5a)
		  bfd_put_8 (abfd, 0x40, contents + irel->r_offset - 1);
		else
		  abort ();

		/* Fix the relocation's type.  */
		irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					     R_H8_PCREL8);

		/* Delete two bytes of data.  */
		if (!elf32_h8_relax_delete_bytes (abfd, sec,
						  irel->r_offset + 1, 2))
		  goto error_return;

		/* That will change things, so, we should relax again.
		   Note that this is not required, and it may be slow.  */
		*again = true;
	      }
	    break;
	  }

	/* Try to turn a 16bit pc-relative branch into a 8bit pc-relative
	   branch.  */
	case R_H8_PCREL16:
	  {
	    bfd_vma value = symval + irel->r_addend;
	    bfd_vma dot;
	    bfd_vma gap;

	    /* Get the address of this instruction.  */
	    dot = (sec->output_section->vma
		   + sec->output_offset
		   + irel->r_offset - 2);

	    gap = value - dot;

	    /* If the distance is within -126..+130 inclusive, then we can
	       relax this jump.  +130 is valid since the target will move
	       two bytes closer if we do relax this branch.  */
	    if ((int) gap >= -126 && (int) gap <= 130)
	      {
		unsigned char code;

		/* Note that we've changed the relocs, section contents,
		   etc.  */
		elf_section_data (sec)->relocs = internal_relocs;
		elf_section_data (sec)->this_hdr.contents = contents;
		symtab_hdr->contents = (unsigned char *) isymbuf;

		/* Get the opcode.  */
		code = bfd_get_8 (abfd, contents + irel->r_offset - 2);

		if (code == 0x58)
		  {
		    /* bCC:16 -> bCC:8 */
		    /* Get the condition code from the original insn.  */
		    code = bfd_get_8 (abfd, contents + irel->r_offset - 1);
		    code &= 0xf0;
		    code >>= 4;
		    code |= 0x40;
		    bfd_put_8 (abfd, code, contents + irel->r_offset - 2);
		  }
		else if (code == 0x5c)
		  bfd_put_8 (abfd, 0x55, contents + irel->r_offset - 2);
		else
		  abort ();

		/* Fix the relocation's type.  */
		irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					     R_H8_PCREL8);
		irel->r_offset--;

		/* Delete two bytes of data.  */
		if (!elf32_h8_relax_delete_bytes (abfd, sec,
						  irel->r_offset + 1, 2))
		  goto error_return;

		/* That will change things, so, we should relax again.
		   Note that this is not required, and it may be slow.  */
		*again = true;
	      }
	    break;
	  }

	/* This is a 16 bit absolute address in a "mov.b" insn, which may
	   become an 8 bit absolute address if its in the right range.  */
	case R_H8_DIR16A8:
	  {
	    bfd_vma value = symval + irel->r_addend;

	    if ((bfd_get_mach (abfd) == bfd_mach_h8300
		 && value >= 0xff00
		 && value <= 0xffff)
		|| ((bfd_get_mach (abfd) == bfd_mach_h8300h
		     || bfd_get_mach (abfd) == bfd_mach_h8300s)
		    && value >= 0xffff00
		    && value <= 0xffffff))
	      {
		unsigned char code;

		/* Note that we've changed the relocs, section contents,
		   etc.  */
		elf_section_data (sec)->relocs = internal_relocs;
		elf_section_data (sec)->this_hdr.contents = contents;
		symtab_hdr->contents = (unsigned char *) isymbuf;

		/* Get the opcode.  */
		code = bfd_get_8 (abfd, contents + irel->r_offset - 2);

		/* Sanity check.  */
		if (code != 0x6a)
		  abort ();

		code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

		if ((code & 0xf0) == 0x00)
		  bfd_put_8 (abfd,
			     (code & 0xf) | 0x20,
			     contents + irel->r_offset - 2);
		else if ((code & 0xf0) == 0x80)
		  bfd_put_8 (abfd,
			     (code & 0xf) | 0x30,
			     contents + irel->r_offset - 2);
		else
		  abort ();

		/* Fix the relocation's type.  */
		irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					     R_H8_DIR8);

		/* Delete two bytes of data.  */
		if (!elf32_h8_relax_delete_bytes (abfd, sec,
						  irel->r_offset + 1, 2))
		  goto error_return;

		/* That will change things, so, we should relax again.
		   Note that this is not required, and it may be slow.  */
		*again = true;
	      }
	    break;
	  }

	/* This is a 24 bit absolute address in a "mov.b" insn, which may
	   become an 8 bit absolute address if its in the right range.  */
	case R_H8_DIR24A8:
	  {
	    bfd_vma value = symval + irel->r_addend;

	    if ((bfd_get_mach (abfd) == bfd_mach_h8300
		 && value >= 0xff00
		 && value <= 0xffff)
		|| ((bfd_get_mach (abfd) == bfd_mach_h8300h
		     || bfd_get_mach (abfd) == bfd_mach_h8300s)
		    && value >= 0xffff00
		    && value <= 0xffffff))
	      {
		unsigned char code;

		/* Note that we've changed the relocs, section contents,
		   etc.  */
		elf_section_data (sec)->relocs = internal_relocs;
		elf_section_data (sec)->this_hdr.contents = contents;
		symtab_hdr->contents = (unsigned char *) isymbuf;

		/* Get the opcode.  */
		code = bfd_get_8 (abfd, contents + irel->r_offset - 2);

		/* Sanity check.  */
		if (code != 0x6a)
		  abort ();

		code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

		if ((code & 0xf0) == 0x00)
		  bfd_put_8 (abfd,
			     (code & 0xf) | 0x20,
			     contents + irel->r_offset - 2);
		else if ((code & 0xf0) == 0x80)
		  bfd_put_8 (abfd,
			     (code & 0xf) | 0x30,
			     contents + irel->r_offset - 2);
		else
		  abort ();

		/* Fix the relocation's type.  */
		irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					     R_H8_DIR8);

		/* Delete two bytes of data.  */
		if (!elf32_h8_relax_delete_bytes (abfd, sec, irel->r_offset, 2))
		  goto error_return;

		/* That will change things, so, we should relax again.
		   Note that this is not required, and it may be slow.  */
		*again = true;
	      }
	  }

	/* FALLTHRU */

	/* This is a 24/32bit absolute address in a "mov" insn, which may
	   become a 16bit absoulte address if it is in the right range.  */
	case R_H8_DIR32A16:
	  {
	    bfd_vma value = symval + irel->r_addend;

	    if (value <= 0x7fff || value >= 0xff8000)
	      {
		unsigned char code;

		/* Note that we've changed the relocs, section contents,
		   etc.  */
		elf_section_data (sec)->relocs = internal_relocs;
		elf_section_data (sec)->this_hdr.contents = contents;
		symtab_hdr->contents = (unsigned char *) isymbuf;

		/* Get the opcode.  */
		code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

		/* We just need to turn off bit 0x20.  */
		code &= ~0x20;

		bfd_put_8 (abfd, code, contents + irel->r_offset - 1);

		/* Fix the relocation's type.  */
		irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					     R_H8_DIR16A8);

		/* Delete two bytes of data.  */
		if (!elf32_h8_relax_delete_bytes (abfd, sec,
						  irel->r_offset + 1, 2))
		  goto error_return;

		/* That will change things, so, we should relax again.
		   Note that this is not required, and it may be slow.  */
		*again = true;
	      }
	    break;
	  }

	default:
	  break;
	}
    }

  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    {
      if (! link_info->keep_memory)
	free (isymbuf);
      else
	symtab_hdr->contents = (unsigned char *) isymbuf;
    }

  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    {
      if (! link_info->keep_memory)
	free (contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (sec)->this_hdr.contents = contents;
	}
    }

  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return true;

 error_return:
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);
  return false;
}

/* Delete some bytes from a section while relaxing.  */

static boolean
elf32_h8_relax_delete_bytes (abfd, sec, addr, count)
     bfd *abfd;
     asection *sec;
     bfd_vma addr;
     int count;
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend;
  Elf_Internal_Rela *irelalign;
  Elf_Internal_Sym *isym;
  Elf_Internal_Sym *isymend;
  bfd_vma toaddr;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;

  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  contents = elf_section_data (sec)->this_hdr.contents;

  /* The deletion must stop at the next ALIGN reloc for an aligment
     power larger than the number of bytes we are deleting.  */

  irelalign = NULL;
  toaddr = sec->_cooked_size;

  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;

  /* Actually delete the bytes.  */
  memmove (contents + addr, contents + addr + count,
	   (size_t) (toaddr - addr - count));
  sec->_cooked_size -= count;

  /* Adjust all the relocs.  */
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      /* Get the new reloc address.  */
      if ((irel->r_offset > addr
	   && irel->r_offset < toaddr))
	irel->r_offset -= count;
    }

  /* Adjust the local symbols defined in this section.  */
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  isym = (Elf_Internal_Sym *) symtab_hdr->contents;
  isymend = isym + symtab_hdr->sh_info;
  for (; isym < isymend; isym++)
    {
      if (isym->st_shndx == sec_shndx
	  && isym->st_value > addr
	  && isym->st_value < toaddr)
	isym->st_value -= count;
    }

  /* Now adjust the global symbols defined in this section.  */
  symcount = (symtab_hdr->sh_size / sizeof (Elf32_External_Sym)
	      - symtab_hdr->sh_info);
  sym_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;
  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;
      if ((sym_hash->root.type == bfd_link_hash_defined
	   || sym_hash->root.type == bfd_link_hash_defweak)
	  && sym_hash->root.u.def.section == sec
	  && sym_hash->root.u.def.value > addr
	  && sym_hash->root.u.def.value < toaddr)
	{
	  sym_hash->root.u.def.value -= count;
	}
    }

  return true;
}

/* Return true if a symbol exists at the given address, else return
   false.  */
static boolean
elf32_h8_symbol_address_p (abfd, sec, addr)
     bfd *abfd;
     asection *sec;
     bfd_vma addr;
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  Elf_Internal_Sym *isym;
  Elf_Internal_Sym *isymend;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;

  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  /* Examine all the symbols.  */
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  isym = (Elf_Internal_Sym *) symtab_hdr->contents;
  isymend = isym + symtab_hdr->sh_info;
  for (; isym < isymend; isym++)
    {
      if (isym->st_shndx == sec_shndx
	  && isym->st_value == addr)
	return true;
    }

  symcount = (symtab_hdr->sh_size / sizeof (Elf32_External_Sym)
	      - symtab_hdr->sh_info);
  sym_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;
  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;
      if ((sym_hash->root.type == bfd_link_hash_defined
	   || sym_hash->root.type == bfd_link_hash_defweak)
	  && sym_hash->root.u.def.section == sec
	  && sym_hash->root.u.def.value == addr)
	return true;
    }

  return false;
}

/* This is a version of bfd_generic_get_relocated_section_contents
   which uses elf32_h8_relocate_section.  */

static bfd_byte *
elf32_h8_get_relocated_section_contents (output_bfd, link_info, link_order,
					 data, relocateable, symbols)
     bfd *output_bfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  Elf_Internal_Shdr *symtab_hdr;
  asection *input_section = link_order->u.indirect.section;
  bfd *input_bfd = input_section->owner;
  asection **sections = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Sym *isymbuf = NULL;

  /* We only need to handle the case of relaxing, or of having a
     particular set of section contents, specially.  */
  if (relocateable
      || elf_section_data (input_section)->this_hdr.contents == NULL)
    return bfd_generic_get_relocated_section_contents (output_bfd, link_info,
						       link_order, data,
						       relocateable,
						       symbols);

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  memcpy (data, elf_section_data (input_section)->this_hdr.contents,
	  (size_t) input_section->_raw_size);

  if ((input_section->flags & SEC_RELOC) != 0
      && input_section->reloc_count > 0)
    {
      asection **secpp;
      Elf_Internal_Sym *isym, *isymend;
      bfd_size_type amt;

      internal_relocs = (_bfd_elf32_link_read_relocs
			 (input_bfd, input_section, (PTR) NULL,
			  (Elf_Internal_Rela *) NULL, false));
      if (internal_relocs == NULL)
	goto error_return;

      if (symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (input_bfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    goto error_return;
	}

      amt = symtab_hdr->sh_info;
      amt *= sizeof (asection *);
      sections = (asection **) bfd_malloc (amt);
      if (sections == NULL && amt != 0)
	goto error_return;

      isymend = isymbuf + symtab_hdr->sh_info;
      for (isym = isymbuf, secpp = sections; isym < isymend; ++isym, ++secpp)
	{
	  asection *isec;

	  if (isym->st_shndx == SHN_UNDEF)
	    isec = bfd_und_section_ptr;
	  else if (isym->st_shndx == SHN_ABS)
	    isec = bfd_abs_section_ptr;
	  else if (isym->st_shndx == SHN_COMMON)
	    isec = bfd_com_section_ptr;
	  else
	    isec = bfd_section_from_elf_index (input_bfd, isym->st_shndx);

	  *secpp = isec;
	}

      if (! elf32_h8_relocate_section (output_bfd, link_info, input_bfd,
				       input_section, data, internal_relocs,
				       isymbuf, sections))
	goto error_return;

      if (sections != NULL)
	free (sections);
      if (isymbuf != NULL
	  && symtab_hdr->contents != (unsigned char *) isymbuf)
	free (isymbuf);
      if (elf_section_data (input_section)->relocs != internal_relocs)
	free (internal_relocs);
    }

  return data;

 error_return:
  if (sections != NULL)
    free (sections);
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (internal_relocs != NULL
      && elf_section_data (input_section)->relocs != internal_relocs)
    free (internal_relocs);
  return NULL;
}


#define TARGET_BIG_SYM			bfd_elf32_h8300_vec
#define TARGET_BIG_NAME			"elf32-h8300"
#define ELF_ARCH			bfd_arch_h8300
#define ELF_MACHINE_CODE		EM_H8_300
#define ELF_MAXPAGESIZE			0x1
#define bfd_elf32_bfd_reloc_type_lookup elf32_h8_reloc_type_lookup
#define elf_info_to_howto		elf32_h8_info_to_howto
#define elf_info_to_howto_rel		elf32_h8_info_to_howto_rel

/* So we can set/examine bits in e_flags to get the specific
   H8 architecture in use.  */
#define elf_backend_final_write_processing \
  elf32_h8_final_write_processing
#define elf_backend_object_p \
  elf32_h8_object_p
#define bfd_elf32_bfd_merge_private_bfd_data \
  elf32_h8_merge_private_bfd_data

/* ??? when elf_backend_relocate_section is not defined, elf32-target.h
   defaults to using _bfd_generic_link_hash_table_create, but
   elflink.h:bfd_elf32_size_dynamic_sections uses
   dynobj = elf_hash_table (info)->dynobj;
   and thus requires an elf hash table.  */
#define bfd_elf32_bfd_link_hash_table_create _bfd_elf_link_hash_table_create

/* Use an H8 specific linker, not the ELF generic linker.  */
#define elf_backend_relocate_section elf32_h8_relocate_section
#define elf_backend_rela_normal		1

/* And relaxing stuff.  */
#define bfd_elf32_bfd_relax_section     elf32_h8_relax_section
#define bfd_elf32_bfd_get_relocated_section_contents \
                                elf32_h8_get_relocated_section_contents


#include "elf32-target.h"
