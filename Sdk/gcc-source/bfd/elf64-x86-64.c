/* X86-64 specific support for 64-bit ELF
   Copyright 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Jan Hubicka <jh@suse.cz>.

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
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"

#include "elf/x86-64.h"

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value.  */
#define MINUS_ONE (~ (bfd_vma) 0)

/* The relocation "howto" table.  Order of fields:
   type, size, bitsize, pc_relative, complain_on_overflow,
   special_function, name, partial_inplace, src_mask, dst_pack, pcrel_offset.  */
static reloc_howto_type x86_64_elf_howto_table[] =
{
  HOWTO(R_X86_64_NONE, 0, 0, 0, false, 0, complain_overflow_dont,
	bfd_elf_generic_reloc, "R_X86_64_NONE",	false, 0x00000000, 0x00000000,
	false),
  HOWTO(R_X86_64_64, 0, 4, 64, false, 0, complain_overflow_bitfield,
	bfd_elf_generic_reloc, "R_X86_64_64", false, MINUS_ONE, MINUS_ONE,
	false),
  HOWTO(R_X86_64_PC32, 0, 2, 32, true, 0, complain_overflow_signed,
	bfd_elf_generic_reloc, "R_X86_64_PC32", false, 0xffffffff, 0xffffffff,
	true),
  HOWTO(R_X86_64_GOT32, 0, 2, 32, false, 0, complain_overflow_signed,
	bfd_elf_generic_reloc, "R_X86_64_GOT32", false, 0xffffffff, 0xffffffff,
	false),
  HOWTO(R_X86_64_PLT32, 0, 2, 32, true, 0, complain_overflow_signed,
	bfd_elf_generic_reloc, "R_X86_64_PLT32", false, 0xffffffff, 0xffffffff,
	true),
  HOWTO(R_X86_64_COPY, 0, 2, 32, false, 0, complain_overflow_bitfield,
	bfd_elf_generic_reloc, "R_X86_64_COPY", false, 0xffffffff, 0xffffffff,
	false),
  HOWTO(R_X86_64_GLOB_DAT, 0, 4, 64, false, 0, complain_overflow_bitfield,
	bfd_elf_generic_reloc, "R_X86_64_GLOB_DAT", false, MINUS_ONE,
	MINUS_ONE, false),
  HOWTO(R_X86_64_JUMP_SLOT, 0, 4, 64, false, 0, complain_overflow_bitfield,
	bfd_elf_generic_reloc, "R_X86_64_JUMP_SLOT", false, MINUS_ONE,
	MINUS_ONE, false),
  HOWTO(R_X86_64_RELATIVE, 0, 4, 64, false, 0, complain_overflow_bitfield,
	bfd_elf_generic_reloc, "R_X86_64_RELATIVE", false, MINUS_ONE,
	MINUS_ONE, false),
  HOWTO(R_X86_64_GOTPCREL, 0, 2, 32, true, 0, complain_overflow_signed,
	bfd_elf_generic_reloc, "R_X86_64_GOTPCREL", false, 0xffffffff,
	0xffffffff, true),
  HOWTO(R_X86_64_32, 0, 2, 32, false, 0, complain_overflow_unsigned,
	bfd_elf_generic_reloc, "R_X86_64_32", false, 0xffffffff, 0xffffffff,
	false),
  HOWTO(R_X86_64_32S, 0, 2, 32, false, 0, complain_overflow_signed,
	bfd_elf_generic_reloc, "R_X86_64_32S", false, 0xffffffff, 0xffffffff,
	false),
  HOWTO(R_X86_64_16, 0, 1, 16, false, 0, complain_overflow_bitfield,
	bfd_elf_generic_reloc, "R_X86_64_16", false, 0xffff, 0xffff, false),
  HOWTO(R_X86_64_PC16,0, 1, 16, true, 0, complain_overflow_bitfield,
	bfd_elf_generic_reloc, "R_X86_64_PC16", false, 0xffff, 0xffff, true),
  HOWTO(R_X86_64_8, 0, 0, 8, false, 0, complain_overflow_signed,
	bfd_elf_generic_reloc, "R_X86_64_8", false, 0xff, 0xff, false),
  HOWTO(R_X86_64_PC8, 0, 0, 8, true, 0, complain_overflow_signed,
	bfd_elf_generic_reloc, "R_X86_64_PC8", false, 0xff, 0xff, true),
  HOWTO(R_X86_64_DTPMOD64, 0, 4, 64, false, 0, complain_overflow_bitfield,
	bfd_elf_generic_reloc, "R_X86_64_DTPMOD64", false, MINUS_ONE,
	MINUS_ONE, false),
  HOWTO(R_X86_64_DTPOFF64, 0, 4, 64, false, 0, complain_overflow_bitfield,
	bfd_elf_generic_reloc, "R_X86_64_DTPOFF64", false, MINUS_ONE,
	MINUS_ONE, false),
  HOWTO(R_X86_64_TPOFF64, 0, 4, 64, false, 0, complain_overflow_bitfield,
	bfd_elf_generic_reloc, "R_X86_64_TPOFF64", false, MINUS_ONE,
	MINUS_ONE, false),
  HOWTO(R_X86_64_TLSGD, 0, 2, 32, true, 0, complain_overflow_signed,
	bfd_elf_generic_reloc, "R_X86_64_TLSGD", false, 0xffffffff,
	0xffffffff, true),
  HOWTO(R_X86_64_TLSLD, 0, 2, 32, true, 0, complain_overflow_signed,
	bfd_elf_generic_reloc, "R_X86_64_TLSLD", false, 0xffffffff,
	0xffffffff, true),
  HOWTO(R_X86_64_DTPOFF32, 0, 2, 32, false, 0, complain_overflow_bitfield,
	bfd_elf_generic_reloc, "R_X86_64_DTPOFF32", false, 0xffffffff,
	0xffffffff, false),
  HOWTO(R_X86_64_GOTTPOFF, 0, 2, 32, true, 0, complain_overflow_signed,
	bfd_elf_generic_reloc, "R_X86_64_GOTTPOFF", false, 0xffffffff,
	0xffffffff, true),
  HOWTO(R_X86_64_TPOFF32, 0, 2, 32, false, 0, complain_overflow_signed,
	bfd_elf_generic_reloc, "R_X86_64_TPOFF32", false, 0xffffffff,
	0xffffffff, false),

/* GNU extension to record C++ vtable hierarchy.  */
  HOWTO (R_X86_64_GNU_VTINHERIT, 0, 4, 0, false, 0, complain_overflow_dont,
	 NULL, "R_X86_64_GNU_VTINHERIT", false, 0, 0, false),

/* GNU extension to record C++ vtable member usage.  */
  HOWTO (R_X86_64_GNU_VTENTRY, 0, 4, 0, false, 0, complain_overflow_dont,
	 _bfd_elf_rel_vtable_reloc_fn, "R_X86_64_GNU_VTENTRY", false, 0, 0,
	 false)
};

/* Map BFD relocs to the x86_64 elf relocs.  */
struct elf_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct elf_reloc_map x86_64_reloc_map[] =
{
  { BFD_RELOC_NONE,		R_X86_64_NONE, },
  { BFD_RELOC_64,		R_X86_64_64,   },
  { BFD_RELOC_32_PCREL,		R_X86_64_PC32, },
  { BFD_RELOC_X86_64_GOT32,	R_X86_64_GOT32,},
  { BFD_RELOC_X86_64_PLT32,	R_X86_64_PLT32,},
  { BFD_RELOC_X86_64_COPY,	R_X86_64_COPY, },
  { BFD_RELOC_X86_64_GLOB_DAT,	R_X86_64_GLOB_DAT, },
  { BFD_RELOC_X86_64_JUMP_SLOT, R_X86_64_JUMP_SLOT, },
  { BFD_RELOC_X86_64_RELATIVE,	R_X86_64_RELATIVE, },
  { BFD_RELOC_X86_64_GOTPCREL,	R_X86_64_GOTPCREL, },
  { BFD_RELOC_32,		R_X86_64_32, },
  { BFD_RELOC_X86_64_32S,	R_X86_64_32S, },
  { BFD_RELOC_16,		R_X86_64_16, },
  { BFD_RELOC_16_PCREL,		R_X86_64_PC16, },
  { BFD_RELOC_8,		R_X86_64_8, },
  { BFD_RELOC_8_PCREL,		R_X86_64_PC8, },
  { BFD_RELOC_X86_64_DTPMOD64,	R_X86_64_DTPMOD64, },
  { BFD_RELOC_X86_64_DTPOFF64,	R_X86_64_DTPOFF64, },
  { BFD_RELOC_X86_64_TPOFF64,	R_X86_64_TPOFF64, },
  { BFD_RELOC_X86_64_TLSGD,	R_X86_64_TLSGD, },
  { BFD_RELOC_X86_64_TLSLD,	R_X86_64_TLSLD, },
  { BFD_RELOC_X86_64_DTPOFF32,	R_X86_64_DTPOFF32, },
  { BFD_RELOC_X86_64_GOTTPOFF,	R_X86_64_GOTTPOFF, },
  { BFD_RELOC_X86_64_TPOFF32,	R_X86_64_TPOFF32, },
  { BFD_RELOC_VTABLE_INHERIT,	R_X86_64_GNU_VTINHERIT, },
  { BFD_RELOC_VTABLE_ENTRY,	R_X86_64_GNU_VTENTRY, },
};

static reloc_howto_type *elf64_x86_64_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void elf64_x86_64_info_to_howto
  PARAMS ((bfd *, arelent *, Elf64_Internal_Rela *));
static boolean elf64_x86_64_grok_prstatus
  PARAMS ((bfd *, Elf_Internal_Note *));
static boolean elf64_x86_64_grok_psinfo
  PARAMS ((bfd *, Elf_Internal_Note *));
static struct bfd_link_hash_table *elf64_x86_64_link_hash_table_create
  PARAMS ((bfd *));
static int elf64_x86_64_tls_transition
  PARAMS ((struct bfd_link_info *, int, int));
static boolean elf64_x86_64_mkobject
  PARAMS((bfd *));
static boolean elf64_x86_64_elf_object_p PARAMS ((bfd *abfd));
static boolean create_got_section
  PARAMS((bfd *, struct bfd_link_info *));
static boolean elf64_x86_64_create_dynamic_sections
  PARAMS((bfd *, struct bfd_link_info *));
static void elf64_x86_64_copy_indirect_symbol
  PARAMS ((struct elf_backend_data *, struct elf_link_hash_entry *,
	   struct elf_link_hash_entry *));
static boolean elf64_x86_64_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *sec,
	   const Elf_Internal_Rela *));
static asection *elf64_x86_64_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));

static boolean elf64_x86_64_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));

static struct bfd_hash_entry *link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static boolean elf64_x86_64_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));

static boolean allocate_dynrelocs
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean readonly_dynrelocs
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf64_x86_64_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_vma dtpoff_base
  PARAMS ((struct bfd_link_info *));
static bfd_vma tpoff
  PARAMS ((struct bfd_link_info *, bfd_vma));
static boolean elf64_x86_64_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	 Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static boolean elf64_x86_64_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *sym));
static boolean elf64_x86_64_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static enum elf_reloc_type_class elf64_x86_64_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));

/* Given a BFD reloc type, return a HOWTO structure.  */
static reloc_howto_type *
elf64_x86_64_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;
  for (i = 0; i < sizeof (x86_64_reloc_map) / sizeof (struct elf_reloc_map);
       i++)
    {
      if (x86_64_reloc_map[i].bfd_reloc_val == code)
	return &x86_64_elf_howto_table[i];
    }
  return 0;
}

/* Given an x86_64 ELF reloc type, fill in an arelent structure.  */

static void
elf64_x86_64_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf64_Internal_Rela *dst;
{
  unsigned r_type, i;

  r_type = ELF64_R_TYPE (dst->r_info);
  if (r_type < (unsigned int) R_X86_64_GNU_VTINHERIT)
    {
      BFD_ASSERT (r_type <= (unsigned int) R_X86_64_TPOFF32);
      i = r_type;
    }
  else
    {
      BFD_ASSERT (r_type < (unsigned int) R_X86_64_max);
      i = r_type - ((unsigned int) R_X86_64_GNU_VTINHERIT - R_X86_64_TPOFF32 - 1);
    }
  cache_ptr->howto = &x86_64_elf_howto_table[i];
  BFD_ASSERT (r_type == cache_ptr->howto->type);
}

/* Support for core dump NOTE sections.  */
static boolean
elf64_x86_64_grok_prstatus (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  int offset;
  size_t raw_size;

  switch (note->descsz)
    {
      default:
	return false;

      case 336:		/* sizeof(istruct elf_prstatus) on Linux/x86_64 */
	/* pr_cursig */
	elf_tdata (abfd)->core_signal
	  = bfd_get_16 (abfd, note->descdata + 12);

	/* pr_pid */
	elf_tdata (abfd)->core_pid
	  = bfd_get_32 (abfd, note->descdata + 32);

	/* pr_reg */
	offset = 112;
	raw_size = 216;

	break;
    }

  /* Make a ".reg/999" section.  */
  return _bfd_elfcore_make_pseudosection (abfd, ".reg",
					  raw_size, note->descpos + offset);
}

static boolean
elf64_x86_64_grok_psinfo (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  switch (note->descsz)
    {
      default:
	return false;

      case 136:		/* sizeof(struct elf_prpsinfo) on Linux/x86_64 */
	elf_tdata (abfd)->core_program
	 = _bfd_elfcore_strndup (abfd, note->descdata + 40, 16);
	elf_tdata (abfd)->core_command
	 = _bfd_elfcore_strndup (abfd, note->descdata + 56, 80);
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

/* Functions for the x86-64 ELF linker.	 */

/* The name of the dynamic interpreter.	 This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/lib/ld64.so.1"

/* The size in bytes of an entry in the global offset table.  */

#define GOT_ENTRY_SIZE 8

/* The size in bytes of an entry in the procedure linkage table.  */

#define PLT_ENTRY_SIZE 16

/* The first entry in a procedure linkage table looks like this.  See the
   SVR4 ABI i386 supplement and the x86-64 ABI to see how this works.  */

static const bfd_byte elf64_x86_64_plt0_entry[PLT_ENTRY_SIZE] =
{
  0xff, 0x35, 8, 0, 0, 0,	/* pushq GOT+8(%rip)  */
  0xff, 0x25, 16, 0, 0, 0,	/* jmpq *GOT+16(%rip) */
  0x90, 0x90, 0x90, 0x90	/* pad out to 16 bytes with nops.  */
};

/* Subsequent entries in a procedure linkage table look like this.  */

static const bfd_byte elf64_x86_64_plt_entry[PLT_ENTRY_SIZE] =
{
  0xff, 0x25,	/* jmpq *name@GOTPC(%rip) */
  0, 0, 0, 0,	/* replaced with offset to this symbol in .got.	 */
  0x68,		/* pushq immediate */
  0, 0, 0, 0,	/* replaced with index into relocation table.  */
  0xe9,		/* jmp relative */
  0, 0, 0, 0	/* replaced with offset to start of .plt0.  */
};

/* The x86-64 linker needs to keep track of the number of relocs that
   it decides to copy as dynamic relocs in check_relocs for each symbol.
   This is so that it can later discard them if they are found to be
   unnecessary.  We store the information in a field extending the
   regular ELF linker hash table.  */

struct elf64_x86_64_dyn_relocs
{
  /* Next section.  */
  struct elf64_x86_64_dyn_relocs *next;

  /* The input section of the reloc.  */
  asection *sec;

  /* Total number of relocs copied for the input section.  */
  bfd_size_type count;

  /* Number of pc-relative relocs copied for the input section.  */
  bfd_size_type pc_count;
};

/* x86-64 ELF linker hash entry.  */

struct elf64_x86_64_link_hash_entry
{
  struct elf_link_hash_entry elf;

  /* Track dynamic relocs copied for this symbol.  */
  struct elf64_x86_64_dyn_relocs *dyn_relocs;

#define GOT_UNKNOWN	0
#define GOT_NORMAL	1
#define GOT_TLS_GD	2
#define GOT_TLS_IE	3
  unsigned char tls_type;
};

#define elf64_x86_64_hash_entry(ent) \
  ((struct elf64_x86_64_link_hash_entry *)(ent))

struct elf64_x86_64_obj_tdata
{
  struct elf_obj_tdata root;

  /* tls_type for each local got entry.  */
  char *local_got_tls_type;
};

#define elf64_x86_64_tdata(abfd) \
  ((struct elf64_x86_64_obj_tdata *) (abfd)->tdata.any)

#define elf64_x86_64_local_got_tls_type(abfd) \
  (elf64_x86_64_tdata (abfd)->local_got_tls_type)


/* x86-64 ELF linker hash table.  */

struct elf64_x86_64_link_hash_table
{
  struct elf_link_hash_table elf;

  /* Short-cuts to get to dynamic linker sections.  */
  asection *sgot;
  asection *sgotplt;
  asection *srelgot;
  asection *splt;
  asection *srelplt;
  asection *sdynbss;
  asection *srelbss;

  union {
    bfd_signed_vma refcount;
    bfd_vma offset;
  } tls_ld_got;

  /* Small local sym to section mapping cache.  */
  struct sym_sec_cache sym_sec;
};

/* Get the x86-64 ELF linker hash table from a link_info structure.  */

#define elf64_x86_64_hash_table(p) \
  ((struct elf64_x86_64_link_hash_table *) ((p)->hash))

/* Create an entry in an x86-64 ELF linker hash table.	*/

static struct bfd_hash_entry *
link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (entry == NULL)
    {
      entry = bfd_hash_allocate (table,
				 sizeof (struct elf64_x86_64_link_hash_entry));
      if (entry == NULL)
	return entry;
    }

  /* Call the allocation method of the superclass.  */
  entry = _bfd_elf_link_hash_newfunc (entry, table, string);
  if (entry != NULL)
    {
      struct elf64_x86_64_link_hash_entry *eh;

      eh = (struct elf64_x86_64_link_hash_entry *) entry;
      eh->dyn_relocs = NULL;
      eh->tls_type = GOT_UNKNOWN;
    }

  return entry;
}

/* Create an X86-64 ELF linker hash table.  */

static struct bfd_link_hash_table *
elf64_x86_64_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf64_x86_64_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct elf64_x86_64_link_hash_table);

  ret = (struct elf64_x86_64_link_hash_table *) bfd_malloc (amt);
  if (ret == NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->elf, abfd, link_hash_newfunc))
    {
      free (ret);
      return NULL;
    }

  ret->sgot = NULL;
  ret->sgotplt = NULL;
  ret->srelgot = NULL;
  ret->splt = NULL;
  ret->srelplt = NULL;
  ret->sdynbss = NULL;
  ret->srelbss = NULL;
  ret->sym_sec.abfd = NULL;
  ret->tls_ld_got.refcount = 0;

  return &ret->elf.root;
}

/* Create .got, .gotplt, and .rela.got sections in DYNOBJ, and set up
   shortcuts to them in our hash table.  */

static boolean
create_got_section (dynobj, info)
     bfd *dynobj;
     struct bfd_link_info *info;
{
  struct elf64_x86_64_link_hash_table *htab;

  if (! _bfd_elf_create_got_section (dynobj, info))
    return false;

  htab = elf64_x86_64_hash_table (info);
  htab->sgot = bfd_get_section_by_name (dynobj, ".got");
  htab->sgotplt = bfd_get_section_by_name (dynobj, ".got.plt");
  if (!htab->sgot || !htab->sgotplt)
    abort ();

  htab->srelgot = bfd_make_section (dynobj, ".rela.got");
  if (htab->srelgot == NULL
      || ! bfd_set_section_flags (dynobj, htab->srelgot,
				  (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
				   | SEC_IN_MEMORY | SEC_LINKER_CREATED
				   | SEC_READONLY))
      || ! bfd_set_section_alignment (dynobj, htab->srelgot, 3))
    return false;
  return true;
}

/* Create .plt, .rela.plt, .got, .got.plt, .rela.got, .dynbss, and
   .rela.bss sections in DYNOBJ, and set up shortcuts to them in our
   hash table.  */

static boolean
elf64_x86_64_create_dynamic_sections (dynobj, info)
     bfd *dynobj;
     struct bfd_link_info *info;
{
  struct elf64_x86_64_link_hash_table *htab;

  htab = elf64_x86_64_hash_table (info);
  if (!htab->sgot && !create_got_section (dynobj, info))
    return false;

  if (!_bfd_elf_create_dynamic_sections (dynobj, info))
    return false;

  htab->splt = bfd_get_section_by_name (dynobj, ".plt");
  htab->srelplt = bfd_get_section_by_name (dynobj, ".rela.plt");
  htab->sdynbss = bfd_get_section_by_name (dynobj, ".dynbss");
  if (!info->shared)
    htab->srelbss = bfd_get_section_by_name (dynobj, ".rela.bss");

  if (!htab->splt || !htab->srelplt || !htab->sdynbss
      || (!info->shared && !htab->srelbss))
    abort ();

  return true;
}

/* Copy the extra info we tack onto an elf_link_hash_entry.  */

static void
elf64_x86_64_copy_indirect_symbol (bed, dir, ind)
     struct elf_backend_data *bed;
     struct elf_link_hash_entry *dir, *ind;
{
  struct elf64_x86_64_link_hash_entry *edir, *eind;

  edir = (struct elf64_x86_64_link_hash_entry *) dir;
  eind = (struct elf64_x86_64_link_hash_entry *) ind;

  if (eind->dyn_relocs != NULL)
    {
      if (edir->dyn_relocs != NULL)
	{
	  struct elf64_x86_64_dyn_relocs **pp;
	  struct elf64_x86_64_dyn_relocs *p;

	  if (ind->root.type == bfd_link_hash_indirect)
	    abort ();

	  /* Add reloc counts against the weak sym to the strong sym
	     list.  Merge any entries against the same section.  */
	  for (pp = &eind->dyn_relocs; (p = *pp) != NULL; )
	    {
	      struct elf64_x86_64_dyn_relocs *q;

	      for (q = edir->dyn_relocs; q != NULL; q = q->next)
		if (q->sec == p->sec)
		  {
		    q->pc_count += p->pc_count;
		    q->count += p->count;
		    *pp = p->next;
		    break;
		  }
	      if (q == NULL)
		pp = &p->next;
	    }
	  *pp = edir->dyn_relocs;
	}

      edir->dyn_relocs = eind->dyn_relocs;
      eind->dyn_relocs = NULL;
    }

  if (ind->root.type == bfd_link_hash_indirect
      && dir->got.refcount <= 0)
    {
      edir->tls_type = eind->tls_type;
      eind->tls_type = GOT_UNKNOWN;
    }

  _bfd_elf_link_hash_copy_indirect (bed, dir, ind);
}

static boolean
elf64_x86_64_mkobject (abfd)
     bfd *abfd;
{
  bfd_size_type amt = sizeof (struct elf64_x86_64_obj_tdata);
  abfd->tdata.any = bfd_zalloc (abfd, amt);
  if (abfd->tdata.any == NULL)
    return false;
  return true;
}

static boolean
elf64_x86_64_elf_object_p (abfd)
  bfd *abfd;
{
  /* Allocate our special target data.  */
  struct elf64_x86_64_obj_tdata *new_tdata;
  bfd_size_type amt = sizeof (struct elf64_x86_64_obj_tdata);
  new_tdata = bfd_zalloc (abfd, amt);
  if (new_tdata == NULL)
    return false;
  new_tdata->root = *abfd->tdata.elf_obj_data;
  abfd->tdata.any = new_tdata;
  /* Set the right machine number for an x86-64 elf64 file.  */
  bfd_default_set_arch_mach (abfd, bfd_arch_i386, bfd_mach_x86_64);
  return true;
}

static int
elf64_x86_64_tls_transition (info, r_type, is_local)
     struct bfd_link_info *info;
     int r_type;
     int is_local;
{
  if (info->shared)
    return r_type;

  switch (r_type)
    {
    case R_X86_64_TLSGD:
    case R_X86_64_GOTTPOFF:
      if (is_local)
	return R_X86_64_TPOFF32;
      return R_X86_64_GOTTPOFF;
    case R_X86_64_TLSLD:
      return R_X86_64_TPOFF32;
    }

   return r_type;
}

/* Look through the relocs for a section during the first phase, and
   calculate needed space in the global offset table, procedure
   linkage table, and dynamic reloc sections.  */

static boolean
elf64_x86_64_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  struct elf64_x86_64_link_hash_table *htab;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sreloc;

  if (info->relocateable)
    return true;

  htab = elf64_x86_64_hash_table (info);
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);

  sreloc = NULL;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned int r_type;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      r_symndx = ELF64_R_SYM (rel->r_info);
      r_type = ELF64_R_TYPE (rel->r_info);

      if (r_symndx >= NUM_SHDR_ENTRIES (symtab_hdr))
	{
	  (*_bfd_error_handler) (_("%s: bad symbol index: %d"),
				 bfd_archive_filename (abfd),
				 r_symndx);
	  return false;
	}

      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      r_type = elf64_x86_64_tls_transition (info, r_type, h == NULL);
      switch (r_type)
	{
	case R_X86_64_TLSLD:
	  htab->tls_ld_got.refcount += 1;
	  goto create_got;

	case R_X86_64_TPOFF32:
	  if (info->shared)
	    {
	      (*_bfd_error_handler)
		(_("%s: relocation %s can not be used when making a shared object; recompile with -fPIC"),
		 bfd_archive_filename (abfd),
		 x86_64_elf_howto_table[r_type].name);
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	  break;

	case R_X86_64_GOTTPOFF:
	  if (info->shared)
	    info->flags |= DF_STATIC_TLS;
	  /* Fall through */

	case R_X86_64_GOT32:
	case R_X86_64_GOTPCREL:
	case R_X86_64_TLSGD:
	  /* This symbol requires a global offset table entry.	*/
	  {
	    int tls_type, old_tls_type;

	    switch (r_type)
	      {
	      default: tls_type = GOT_NORMAL; break;
	      case R_X86_64_TLSGD: tls_type = GOT_TLS_GD; break;
	      case R_X86_64_GOTTPOFF: tls_type = GOT_TLS_IE; break;
	      }

	    if (h != NULL)
	      {
		h->got.refcount += 1;
		old_tls_type = elf64_x86_64_hash_entry (h)->tls_type;
	      }
	    else
	      {
		bfd_signed_vma *local_got_refcounts;

		/* This is a global offset table entry for a local symbol.  */
		local_got_refcounts = elf_local_got_refcounts (abfd);
		if (local_got_refcounts == NULL)
		  {
		    bfd_size_type size;

		    size = symtab_hdr->sh_info;
		    size *= sizeof (bfd_signed_vma) + sizeof (char);
		    local_got_refcounts = ((bfd_signed_vma *)
					   bfd_zalloc (abfd, size));
		    if (local_got_refcounts == NULL)
		      return false;
		    elf_local_got_refcounts (abfd) = local_got_refcounts;
		    elf64_x86_64_local_got_tls_type (abfd)
		      = (char *) (local_got_refcounts + symtab_hdr->sh_info);
		  }
		local_got_refcounts[r_symndx] += 1;
		old_tls_type
		  = elf64_x86_64_local_got_tls_type (abfd) [r_symndx];
	      }

	    /* If a TLS symbol is accessed using IE at least once,
	       there is no point to use dynamic model for it.  */
	    if (old_tls_type != tls_type && old_tls_type != GOT_UNKNOWN
		&& (old_tls_type != GOT_TLS_GD || tls_type != GOT_TLS_IE))
	      {
		if (old_tls_type == GOT_TLS_IE && tls_type == GOT_TLS_GD)
		  tls_type = old_tls_type;
		else
		  {
		    (*_bfd_error_handler)
		      (_("%s: %s' accessed both as normal and thread local symbol"),
		       bfd_archive_filename (abfd),
		       h ? h->root.root.string : "<local>");
		    return false;
		  }
	      }

	    if (old_tls_type != tls_type)
	      {
		if (h != NULL)
		  elf64_x86_64_hash_entry (h)->tls_type = tls_type;
		else
		  elf64_x86_64_local_got_tls_type (abfd) [r_symndx] = tls_type;
	      }
	  }
	  /* Fall through */

	  //case R_X86_64_GOTPCREL:
	create_got:
	  if (htab->sgot == NULL)
	    {
	      if (htab->elf.dynobj == NULL)
		htab->elf.dynobj = abfd;
	      if (!create_got_section (htab->elf.dynobj, info))
		return false;
	    }
	  break;

	case R_X86_64_PLT32:
	  /* This symbol requires a procedure linkage table entry.  We
	     actually build the entry in adjust_dynamic_symbol,
	     because this might be a case of linking PIC code which is
	     never referenced by a dynamic object, in which case we
	     don't need to generate a procedure linkage table entry
	     after all.	 */

	  /* If this is a local symbol, we resolve it directly without
	     creating a procedure linkage table entry.	*/
	  if (h == NULL)
	    continue;

	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	  h->plt.refcount += 1;
	  break;

	case R_X86_64_8:
	case R_X86_64_16:
	case R_X86_64_32:
	case R_X86_64_32S:
	  /* Let's help debug shared library creation.  These relocs
	     cannot be used in shared libs.  Don't error out for
	     sections we don't care about, such as debug sections or
	     non-constant sections.  */
	  if (info->shared
	      && (sec->flags & SEC_ALLOC) != 0
	      && (sec->flags & SEC_READONLY) != 0)
	    {
	      (*_bfd_error_handler)
		(_("%s: relocation %s can not be used when making a shared object; recompile with -fPIC"),
		 bfd_archive_filename (abfd),
		 x86_64_elf_howto_table[r_type].name);
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	  /* Fall through.  */

	case R_X86_64_PC8:
	case R_X86_64_PC16:
	case R_X86_64_PC32:
	case R_X86_64_64:
	  if (h != NULL && !info->shared)
	    {
	      /* If this reloc is in a read-only section, we might
		 need a copy reloc.  We can't check reliably at this
		 stage whether the section is read-only, as input
		 sections have not yet been mapped to output sections.
		 Tentatively set the flag for now, and correct in
		 adjust_dynamic_symbol.  */
	      h->elf_link_hash_flags |= ELF_LINK_NON_GOT_REF;

	      /* We may need a .plt entry if the function this reloc
		 refers to is in a shared lib.  */
	      h->plt.refcount += 1;
	    }

	  /* If we are creating a shared library, and this is a reloc
	     against a global symbol, or a non PC relative reloc
	     against a local symbol, then we need to copy the reloc
	     into the shared library.  However, if we are linking with
	     -Bsymbolic, we do not need to copy a reloc against a
	     global symbol which is defined in an object we are
	     including in the link (i.e., DEF_REGULAR is set).	At
	     this point we have not seen all the input files, so it is
	     possible that DEF_REGULAR is not set now but will be set
	     later (it is never cleared).  In case of a weak definition,
	     DEF_REGULAR may be cleared later by a strong definition in
	     a shared library.  We account for that possibility below by
	     storing information in the relocs_copied field of the hash
	     table entry.  A similar situation occurs when creating
	     shared libraries and symbol visibility changes render the
	     symbol local.

	     If on the other hand, we are creating an executable, we
	     may need to keep relocations for symbols satisfied by a
	     dynamic library if we manage to avoid copy relocs for the
	     symbol.  */
	  if ((info->shared
	       && (sec->flags & SEC_ALLOC) != 0
	       && (((r_type != R_X86_64_PC8)
		    && (r_type != R_X86_64_PC16)
		    && (r_type != R_X86_64_PC32))
		   || (h != NULL
		       && (! info->symbolic
			   || h->root.type == bfd_link_hash_defweak
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))))
	      || (!info->shared
		  && (sec->flags & SEC_ALLOC) != 0
		  && h != NULL
		  && (h->root.type == bfd_link_hash_defweak
		      || (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0)))
	    {
	      struct elf64_x86_64_dyn_relocs *p;
	      struct elf64_x86_64_dyn_relocs **head;

	      /* We must copy these reloc types into the output file.
		 Create a reloc section in dynobj and make room for
		 this reloc.  */
	      if (sreloc == NULL)
		{
		  const char *name;
		  bfd *dynobj;

		  name = (bfd_elf_string_from_elf_section
			  (abfd,
			   elf_elfheader (abfd)->e_shstrndx,
			   elf_section_data (sec)->rel_hdr.sh_name));
		  if (name == NULL)
		    return false;

		  if (strncmp (name, ".rela", 5) != 0
		      || strcmp (bfd_get_section_name (abfd, sec),
				 name + 5) != 0)
		    {
		      (*_bfd_error_handler)
			(_("%s: bad relocation section name `%s\'"),
			 bfd_archive_filename (abfd), name);
		    }

		  if (htab->elf.dynobj == NULL)
		    htab->elf.dynobj = abfd;

		  dynobj = htab->elf.dynobj;

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  if (sreloc == NULL)
		    {
		      flagword flags;

		      sreloc = bfd_make_section (dynobj, name);
		      flags = (SEC_HAS_CONTENTS | SEC_READONLY
			       | SEC_IN_MEMORY | SEC_LINKER_CREATED);
		      if ((sec->flags & SEC_ALLOC) != 0)
			flags |= SEC_ALLOC | SEC_LOAD;
		      if (sreloc == NULL
			  || ! bfd_set_section_flags (dynobj, sreloc, flags)
			  || ! bfd_set_section_alignment (dynobj, sreloc, 3))
			return false;
		    }
		  elf_section_data (sec)->sreloc = sreloc;
		}

	      /* If this is a global symbol, we count the number of
		 relocations we need for this symbol.  */
	      if (h != NULL)
		{
		  head = &((struct elf64_x86_64_link_hash_entry *) h)->dyn_relocs;
		}
	      else
		{
		  /* Track dynamic relocs needed for local syms too.
		     We really need local syms available to do this
		     easily.  Oh well.  */

		  asection *s;
		  s = bfd_section_from_r_symndx (abfd, &htab->sym_sec,
						 sec, r_symndx);
		  if (s == NULL)
		    return false;

		  head = ((struct elf64_x86_64_dyn_relocs **)
			  &elf_section_data (s)->local_dynrel);
		}

	      p = *head;
	      if (p == NULL || p->sec != sec)
		{
		  bfd_size_type amt = sizeof *p;
		  p = ((struct elf64_x86_64_dyn_relocs *)
		       bfd_alloc (htab->elf.dynobj, amt));
		  if (p == NULL)
		    return false;
		  p->next = *head;
		  *head = p;
		  p->sec = sec;
		  p->count = 0;
		  p->pc_count = 0;
		}

	      p->count += 1;
	      if (r_type == R_X86_64_PC8
		  || r_type == R_X86_64_PC16
		  || r_type == R_X86_64_PC32)
		p->pc_count += 1;
	    }
	  break;

	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	case R_X86_64_GNU_VTINHERIT:
	  if (!_bfd_elf64_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return false;
	  break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	case R_X86_64_GNU_VTENTRY:
	  if (!_bfd_elf64_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return false;
	  break;

	default:
	  break;
	}
    }

  return true;
}

/* Return the section that should be marked against GC for a given
   relocation.	*/

static asection *
elf64_x86_64_gc_mark_hook (sec, info, rel, h, sym)
     asection *sec;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *rel;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  if (h != NULL)
    {
      switch (ELF64_R_TYPE (rel->r_info))
	{
	case R_X86_64_GNU_VTINHERIT:
	case R_X86_64_GNU_VTENTRY:
	  break;

	default:
	  switch (h->root.type)
	    {
	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      return h->root.u.def.section;

	    case bfd_link_hash_common:
	      return h->root.u.c.p->section;

	    default:
	      break;
	    }
	}
    }
  else
    return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  return NULL;
}

/* Update the got entry reference counts for the section being removed.	 */

static boolean
elf64_x86_64_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;
  unsigned long r_symndx;
  int r_type;
  struct elf_link_hash_entry *h;

  elf_section_data (sec)->local_dynrel = NULL;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    switch ((r_type = elf64_x86_64_tls_transition (info,
						   ELF64_R_TYPE (rel->r_info),
						   ELF64_R_SYM (rel->r_info)
						   >= symtab_hdr->sh_info)))
      {
      case R_X86_64_TLSLD:
	if (elf64_x86_64_hash_table (info)->tls_ld_got.refcount > 0)
	  elf64_x86_64_hash_table (info)->tls_ld_got.refcount -= 1;
	break;

      case R_X86_64_TLSGD:
      case R_X86_64_GOTTPOFF:
      case R_X86_64_GOT32:
      case R_X86_64_GOTPCREL:
	r_symndx = ELF64_R_SYM (rel->r_info);
	if (r_symndx >= symtab_hdr->sh_info)
	  {
	    h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	    if (h->got.refcount > 0)
	      h->got.refcount -= 1;
	  }
	else if (local_got_refcounts != NULL)
	  {
	    if (local_got_refcounts[r_symndx] > 0)
	      local_got_refcounts[r_symndx] -= 1;
	  }
	break;

      case R_X86_64_8:
      case R_X86_64_16:
      case R_X86_64_32:
      case R_X86_64_64:
      case R_X86_64_32S:
      case R_X86_64_PC8:
      case R_X86_64_PC16:
      case R_X86_64_PC32:
	r_symndx = ELF64_R_SYM (rel->r_info);
	if (r_symndx >= symtab_hdr->sh_info)
	  {
	    struct elf64_x86_64_link_hash_entry *eh;
	    struct elf64_x86_64_dyn_relocs **pp;
	    struct elf64_x86_64_dyn_relocs *p;

	    h = sym_hashes[r_symndx - symtab_hdr->sh_info];

	    if (!info->shared && h->plt.refcount > 0)
	      h->plt.refcount -= 1;

	    eh = (struct elf64_x86_64_link_hash_entry *) h;

	    for (pp = &eh->dyn_relocs; (p = *pp) != NULL; pp = &p->next)
	      if (p->sec == sec)
		{
		  if (ELF64_R_TYPE (rel->r_info) == R_X86_64_PC8
		      || ELF64_R_TYPE (rel->r_info) == R_X86_64_PC16
		      || ELF64_R_TYPE (rel->r_info) == R_X86_64_PC32)
		    p->pc_count -= 1;
		  p->count -= 1;
		  if (p->count == 0)
		    *pp = p->next;
		  break;
		}
	  }
	break;


      case R_X86_64_PLT32:
	r_symndx = ELF64_R_SYM (rel->r_info);
	if (r_symndx >= symtab_hdr->sh_info)
	  {
	    h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	    if (h->plt.refcount > 0)
	      h->plt.refcount -= 1;
	  }
	break;

      default:
	break;
      }

  return true;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.	*/

static boolean
elf64_x86_64_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  struct elf64_x86_64_link_hash_table *htab;
  struct elf64_x86_64_link_hash_entry * eh;
  struct elf64_x86_64_dyn_relocs *p;
  asection *s;
  unsigned int power_of_two;

  /* If this is a function, put it in the procedure linkage table.  We
     will fill in the contents of the procedure linkage table later,
     when we know the address of the .got section.  */
  if (h->type == STT_FUNC
      || (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      if (h->plt.refcount <= 0
	  || (! info->shared
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) == 0
	      && h->root.type != bfd_link_hash_undefweak
	      && h->root.type != bfd_link_hash_undefined))
	{
	  /* This case can occur if we saw a PLT32 reloc in an input
	     file, but the symbol was never referred to by a dynamic
	     object, or if all references were garbage collected.  In
	     such a case, we don't actually need to build a procedure
	     linkage table, and we can just do a PC32 reloc instead.  */
	  h->plt.offset = (bfd_vma) -1;
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	}

      return true;
    }
  else
    /* It's possible that we incorrectly decided a .plt reloc was
       needed for an R_X86_64_PC32 reloc to a non-function sym in
       check_relocs.  We can't decide accurately between function and
       non-function syms in check-relocs;  Objects loaded later in
       the link may change h->type.  So fix it now.  */
    h->plt.offset = (bfd_vma) -1;

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.	 */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined
		  || h->weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
      return true;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.	 */

  /* If we are creating a shared library, we must presume that the
     only references to the symbol are via the global offset table.
     For such cases we need not do anything here; the relocations will
     be handled correctly by relocate_section.	*/
  if (info->shared)
    return true;

  /* If there are no references to this symbol that do not use the
     GOT, we don't need to generate a copy reloc.  */
  if ((h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0)
    return true;

  /* If -z nocopyreloc was given, we won't generate them either.  */
  if (info->nocopyreloc)
    {
      h->elf_link_hash_flags &= ~ELF_LINK_NON_GOT_REF;
      return true;
    }

  eh = (struct elf64_x86_64_link_hash_entry *) h;
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      s = p->sec->output_section;
      if (s != NULL && (s->flags & SEC_READONLY) != 0)
	break;
    }

  /* If we didn't find any dynamic relocs in read-only sections, then
     we'll be keeping the dynamic relocs and avoiding the copy reloc.  */
  if (p == NULL)
    {
      h->elf_link_hash_flags &= ~ELF_LINK_NON_GOT_REF;
      return true;
    }

  /* We must allocate the symbol in our .dynbss section, which will
     become part of the .bss section of the executable.	 There will be
     an entry for this symbol in the .dynsym section.  The dynamic
     object will contain position independent code, so all references
     from the dynamic object to this symbol will go through the global
     offset table.  The dynamic linker will use the .dynsym entry to
     determine the address it must put in the global offset table, so
     both the dynamic object and the regular object will refer to the
     same memory location for the variable.  */

  htab = elf64_x86_64_hash_table (info);

  /* We must generate a R_X86_64_COPY reloc to tell the dynamic linker
     to copy the initial value out of the dynamic object and into the
     runtime process image.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      htab->srelbss->_raw_size += sizeof (Elf64_External_Rela);
      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_COPY;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.	16-bytes is the size
     of the largest type that requires hard alignment -- long double.  */
  /* FIXME: This is VERY ugly. Should be fixed for all architectures using
     this construct.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 4)
    power_of_two = 4;

  /* Apply the required alignment.  */
  s = htab->sdynbss;
  s->_raw_size = BFD_ALIGN (s->_raw_size, (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (htab->elf.dynobj, s))
    {
      if (! bfd_set_section_alignment (htab->elf.dynobj, s, power_of_two))
	return false;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;

  return true;
}

/* This is the condition under which elf64_x86_64_finish_dynamic_symbol
   will be called from elflink.h.  If elflink.h doesn't call our
   finish_dynamic_symbol routine, we'll need to do something about
   initializing any .plt and .got entries in elf64_x86_64_relocate_section.  */
#define WILL_CALL_FINISH_DYNAMIC_SYMBOL(DYN, INFO, H) \
  ((DYN)								\
   && ((INFO)->shared							\
       || ((H)->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)	\
   && ((H)->dynindx != -1						\
       || ((H)->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0))

/* Allocate space in .plt, .got and associated reloc sections for
   dynamic relocs.  */

static boolean
allocate_dynrelocs (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct bfd_link_info *info;
  struct elf64_x86_64_link_hash_table *htab;
  struct elf64_x86_64_link_hash_entry *eh;
  struct elf64_x86_64_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_indirect)
    return true;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  info = (struct bfd_link_info *) inf;
  htab = elf64_x86_64_hash_table (info);

  if (htab->elf.dynamic_sections_created
      && h->plt.refcount > 0)
    {
      /* Make sure this symbol is output as a dynamic symbol.
	 Undefined weak syms won't yet be marked as dynamic.  */
      if (h->dynindx == -1
	  && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	{
	  if (! bfd_elf64_link_record_dynamic_symbol (info, h))
	    return false;
	}

      if (WILL_CALL_FINISH_DYNAMIC_SYMBOL (1, info, h))
	{
	  asection *s = htab->splt;

	  /* If this is the first .plt entry, make room for the special
	     first entry.  */
	  if (s->_raw_size == 0)
	    s->_raw_size += PLT_ENTRY_SIZE;

	  h->plt.offset = s->_raw_size;

	  /* If this symbol is not defined in a regular file, and we are
	     not generating a shared library, then set the symbol to this
	     location in the .plt.  This is required to make function
	     pointers compare as equal between the normal executable and
	     the shared library.  */
	  if (! info->shared
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	    {
	      h->root.u.def.section = s;
	      h->root.u.def.value = h->plt.offset;
	    }

	  /* Make room for this entry.  */
	  s->_raw_size += PLT_ENTRY_SIZE;

	  /* We also need to make an entry in the .got.plt section, which
	     will be placed in the .got section by the linker script.  */
	  htab->sgotplt->_raw_size += GOT_ENTRY_SIZE;

	  /* We also need to make an entry in the .rela.plt section.  */
	  htab->srelplt->_raw_size += sizeof (Elf64_External_Rela);
	}
      else
	{
	  h->plt.offset = (bfd_vma) -1;
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	}
    }
  else
    {
      h->plt.offset = (bfd_vma) -1;
      h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
    }

  /* If R_X86_64_GOTTPOFF symbol is now local to the binary,
     make it a R_X86_64_TPOFF32 requiring no GOT entry.  */
  if (h->got.refcount > 0
      && !info->shared
      && h->dynindx == -1
      && elf64_x86_64_hash_entry (h)->tls_type == GOT_TLS_IE)
    h->got.offset = (bfd_vma) -1;
  else if (h->got.refcount > 0)
    {
      asection *s;
      boolean dyn;
      int tls_type = elf64_x86_64_hash_entry (h)->tls_type;

      /* Make sure this symbol is output as a dynamic symbol.
	 Undefined weak syms won't yet be marked as dynamic.  */
      if (h->dynindx == -1
	  && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	{
	  if (! bfd_elf64_link_record_dynamic_symbol (info, h))
	    return false;
	}

      s = htab->sgot;
      h->got.offset = s->_raw_size;
      s->_raw_size += GOT_ENTRY_SIZE;
      /* R_X86_64_TLSGD needs 2 consecutive GOT slots.  */
      if (tls_type == GOT_TLS_GD)
	s->_raw_size += GOT_ENTRY_SIZE;
      dyn = htab->elf.dynamic_sections_created;
      /* R_X86_64_TLSGD needs one dynamic relocation if local symbol
	 and two if global.
	 R_X86_64_GOTTPOFF needs one dynamic relocation.  */
      if ((tls_type == GOT_TLS_GD && h->dynindx == -1)
	  || tls_type == GOT_TLS_IE)
	htab->srelgot->_raw_size += sizeof (Elf64_External_Rela);
      else if (tls_type == GOT_TLS_GD)
	htab->srelgot->_raw_size += 2 * sizeof (Elf64_External_Rela);
      else if (WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info, h))
	htab->srelgot->_raw_size += sizeof (Elf64_External_Rela);
    }
  else
    h->got.offset = (bfd_vma) -1;

  eh = (struct elf64_x86_64_link_hash_entry *) h;
  if (eh->dyn_relocs == NULL)
    return true;

  /* In the shared -Bsymbolic case, discard space allocated for
     dynamic pc-relative relocs against symbols which turn out to be
     defined in regular objects.  For the normal shared case, discard
     space for pc-relative relocs that have become local due to symbol
     visibility changes.  */

  if (info->shared)
    {
      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
	  && ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0
	      || info->symbolic))
	{
	  struct elf64_x86_64_dyn_relocs **pp;

	  for (pp = &eh->dyn_relocs; (p = *pp) != NULL; )
	    {
	      p->count -= p->pc_count;
	      p->pc_count = 0;
	      if (p->count == 0)
		*pp = p->next;
	      else
		pp = &p->next;
	    }
	}
    }
  else
    {
      /* For the non-shared case, discard space for relocs against
	 symbols which turn out to need copy relocs or are not
	 dynamic.  */

      if ((h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0
	  && (((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	       && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	      || (htab->elf.dynamic_sections_created
		  && (h->root.type == bfd_link_hash_undefweak
		      || h->root.type == bfd_link_hash_undefined))))
	{
	  /* Make sure this symbol is output as a dynamic symbol.
	     Undefined weak syms won't yet be marked as dynamic.  */
	  if (h->dynindx == -1
	      && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	    {
	      if (! bfd_elf64_link_record_dynamic_symbol (info, h))
		return false;
	    }

	  /* If that succeeded, we know we'll be keeping all the
	     relocs.  */
	  if (h->dynindx != -1)
	    goto keep;
	}

      eh->dyn_relocs = NULL;

    keep: ;
    }

  /* Finally, allocate space.  */
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      asection *sreloc = elf_section_data (p->sec)->sreloc;
      sreloc->_raw_size += p->count * sizeof (Elf64_External_Rela);
    }

  return true;
}

/* Find any dynamic relocs that apply to read-only sections.  */

static boolean
readonly_dynrelocs (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct elf64_x86_64_link_hash_entry *eh;
  struct elf64_x86_64_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  eh = (struct elf64_x86_64_link_hash_entry *) h;
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      asection *s = p->sec->output_section;

      if (s != NULL && (s->flags & SEC_READONLY) != 0)
	{
	  struct bfd_link_info *info = (struct bfd_link_info *) inf;

	  info->flags |= DF_TEXTREL;

	  /* Not an error, just cut short the traversal.  */
	  return false;
	}
    }
  return true;
}

/* Set the sizes of the dynamic sections.  */

static boolean
elf64_x86_64_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  struct elf64_x86_64_link_hash_table *htab;
  bfd *dynobj;
  asection *s;
  boolean relocs;
  bfd *ibfd;

  htab = elf64_x86_64_hash_table (info);
  dynobj = htab->elf.dynobj;
  if (dynobj == NULL)
    abort ();

  if (htab->elf.dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (! info->shared)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  if (s == NULL)
	    abort ();
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}
    }

  /* Set up .got offsets for local syms, and space for local dynamic
     relocs.  */
  for (ibfd = info->input_bfds; ibfd != NULL; ibfd = ibfd->link_next)
    {
      bfd_signed_vma *local_got;
      bfd_signed_vma *end_local_got;
      char *local_tls_type;
      bfd_size_type locsymcount;
      Elf_Internal_Shdr *symtab_hdr;
      asection *srel;

      if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour)
	continue;

      for (s = ibfd->sections; s != NULL; s = s->next)
	{
	  struct elf64_x86_64_dyn_relocs *p;

	  for (p = *((struct elf64_x86_64_dyn_relocs **)
		     &elf_section_data (s)->local_dynrel);
	       p != NULL;
	       p = p->next)
	    {
	      if (!bfd_is_abs_section (p->sec)
		  && bfd_is_abs_section (p->sec->output_section))
		{
		  /* Input section has been discarded, either because
		     it is a copy of a linkonce section or due to
		     linker script /DISCARD/, so we'll be discarding
		     the relocs too.  */
		}
	      else if (p->count != 0)
		{
		  srel = elf_section_data (p->sec)->sreloc;
		  srel->_raw_size += p->count * sizeof (Elf64_External_Rela);
		  if ((p->sec->output_section->flags & SEC_READONLY) != 0)
		    info->flags |= DF_TEXTREL;

		}
	    }
	}

      local_got = elf_local_got_refcounts (ibfd);
      if (!local_got)
	continue;

      symtab_hdr = &elf_tdata (ibfd)->symtab_hdr;
      locsymcount = symtab_hdr->sh_info;
      end_local_got = local_got + locsymcount;
      local_tls_type = elf64_x86_64_local_got_tls_type (ibfd);
      s = htab->sgot;
      srel = htab->srelgot;
      for (; local_got < end_local_got; ++local_got, ++local_tls_type)
	{
	  if (*local_got > 0)
	    {
	      *local_got = s->_raw_size;
	      s->_raw_size += GOT_ENTRY_SIZE;
	      if (*local_tls_type == GOT_TLS_GD)
		s->_raw_size += GOT_ENTRY_SIZE;
	      if (info->shared
		  || *local_tls_type == GOT_TLS_GD
		  || *local_tls_type == GOT_TLS_IE)
		srel->_raw_size += sizeof (Elf64_External_Rela);
	    }
	  else
	    *local_got = (bfd_vma) -1;
	}
    }

  if (htab->tls_ld_got.refcount > 0)
    {
      /* Allocate 2 got entries and 1 dynamic reloc for R_X86_64_TLSLD
	 relocs.  */
      htab->tls_ld_got.offset = htab->sgot->_raw_size;
      htab->sgot->_raw_size += 2 * GOT_ENTRY_SIZE;
      htab->srelgot->_raw_size += sizeof (Elf64_External_Rela);
    }
  else
    htab->tls_ld_got.offset = -1;

  /* Allocate global sym .plt and .got entries, and space for global
     sym dynamic relocs.  */
  elf_link_hash_traverse (&htab->elf, allocate_dynrelocs, (PTR) info);

  /* We now have determined the sizes of the various dynamic sections.
     Allocate memory for them.  */
  relocs = false;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      if (s == htab->splt
	  || s == htab->sgot
	  || s == htab->sgotplt)
	{
	  /* Strip this section if we don't need it; see the
	     comment below.  */
	}
      else if (strncmp (bfd_get_section_name (dynobj, s), ".rela", 5) == 0)
	{
	  if (s->_raw_size != 0 && s != htab->srelplt)
	    relocs = true;

	  /* We use the reloc_count field as a counter if we need
	     to copy relocs into the output file.  */
	  s->reloc_count = 0;
	}
      else
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (s->_raw_size == 0)
	{
	  /* If we don't need this section, strip it from the
	     output file.  This is mostly to handle .rela.bss and
	     .rela.plt.  We must create both sections in
	     create_dynamic_sections, because they must be created
	     before the linker maps input sections to output
	     sections.  The linker does that before
	     adjust_dynamic_symbol is called, and it is that
	     function which decides whether anything needs to go
	     into these sections.  */

	  _bfd_strip_section_from_output (info, s);
	  continue;
	}

      /* Allocate memory for the section contents.  We use bfd_zalloc
	 here in case unused entries are not reclaimed before the
	 section's contents are written out.  This should not happen,
	 but this way if it does, we get a R_X86_64_NONE reloc instead
	 of garbage.  */
      s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->_raw_size);
      if (s->contents == NULL)
	return false;
    }

  if (htab->elf.dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf64_x86_64_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.	The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  bfd_elf64_add_dynamic_entry (info, (bfd_vma) (TAG), (bfd_vma) (VAL))

      if (! info->shared)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0))
	    return false;
	}

      if (htab->splt->_raw_size != 0)
	{
	  if (!add_dynamic_entry (DT_PLTGOT, 0)
	      || !add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0))
	    return false;
	}

      if (relocs)
	{
	  if (!add_dynamic_entry (DT_RELA, 0)
	      || !add_dynamic_entry (DT_RELASZ, 0)
	      || !add_dynamic_entry (DT_RELAENT, sizeof (Elf64_External_Rela)))
	    return false;

	  /* If any dynamic relocs apply to a read-only section,
	     then we need a DT_TEXTREL entry.  */
	  if ((info->flags & DF_TEXTREL) == 0)
	    elf_link_hash_traverse (&htab->elf, readonly_dynrelocs,
				    (PTR) info);

	  if ((info->flags & DF_TEXTREL) != 0)
	    {
	      if (!add_dynamic_entry (DT_TEXTREL, 0))
		return false;
	    }
	}
    }
#undef add_dynamic_entry

  return true;
}

/* Return the base VMA address which should be subtracted from real addresses
   when resolving @dtpoff relocation.
   This is PT_TLS segment p_vaddr.  */

static bfd_vma
dtpoff_base (info)
     struct bfd_link_info *info;
{
  /* If tls_segment is NULL, we should have signalled an error already.  */
  if (elf_hash_table (info)->tls_segment == NULL)
    return 0;
  return elf_hash_table (info)->tls_segment->start;
}

/* Return the relocation value for @tpoff relocation
   if STT_TLS virtual address is ADDRESS.  */

static bfd_vma
tpoff (info, address)
     struct bfd_link_info *info;
     bfd_vma address;
{
  struct elf_link_tls_segment *tls_segment
    = elf_hash_table (info)->tls_segment;

  /* If tls_segment is NULL, we should have signalled an error already.  */
  if (tls_segment == NULL)
    return 0;
  return address - align_power (tls_segment->size, tls_segment->align)
	 - tls_segment->start;
}

/* Relocate an x86_64 ELF section.  */

static boolean
elf64_x86_64_relocate_section (output_bfd, info, input_bfd, input_section,
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
  struct elf64_x86_64_link_hash_table *htab;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_vma *local_got_offsets;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  if (info->relocateable)
    return true;

  htab = elf64_x86_64_hash_table (info);
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  local_got_offsets = elf_local_got_offsets (input_bfd);

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      unsigned int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sec;
      bfd_vma off;
      bfd_vma relocation;
      boolean unresolved_reloc;
      bfd_reloc_status_type r;
      int tls_type;

      r_type = ELF64_R_TYPE (rel->r_info);
      if (r_type == (int) R_X86_64_GNU_VTINHERIT
	  || r_type == (int) R_X86_64_GNU_VTENTRY)
	continue;

      if (r_type >= R_X86_64_max)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      howto = x86_64_elf_howto_table + r_type;
      r_symndx = ELF64_R_SYM (rel->r_info);
      h = NULL;
      sym = NULL;
      sec = NULL;
      unresolved_reloc = false;
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
	      if (sec->output_section == NULL)
		{
		  /* Set a flag that will be cleared later if we find a
		     relocation value for this symbol.  output_section
		     is typically NULL for symbols satisfied by a shared
		     library.  */
		  unresolved_reloc = true;
		  relocation = 0;
		}
	      else
		relocation = (h->root.u.def.value
			      + sec->output_section->vma
			      + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else if (info->shared
		   && (!info->symbolic || info->allow_shlib_undefined)
		   && !info->no_undefined
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    relocation = 0;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset,
		      (!info->shared || info->no_undefined
		       || ELF_ST_VISIBILITY (h->other)))))
		return false;
	      relocation = 0;
	    }
	}
      /* When generating a shared object, the relocations handled here are
	 copied into the output file to be resolved at run time.  */
      switch (r_type)
	{
	case R_X86_64_GOT32:
	  /* Relocation is to the entry for this symbol in the global
	     offset table.  */
	case R_X86_64_GOTPCREL:
	  /* Use global offset table as symbol value.  */
	  if (htab->sgot == NULL)
	    abort ();

	  if (h != NULL)
	    {
	      boolean dyn;

	      off = h->got.offset;
	      dyn = htab->elf.dynamic_sections_created;

	      if (! WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info, h)
		  || (info->shared
		      && (info->symbolic
			  || h->dynindx == -1
			  || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL))
		      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
		{
		  /* This is actually a static link, or it is a -Bsymbolic
		     link and the symbol is defined locally, or the symbol
		     was forced to be local because of a version file.	We
		     must initialize this entry in the global offset table.
		     Since the offset must always be a multiple of 8, we
		     use the least significant bit to record whether we
		     have initialized it already.

		     When doing a dynamic link, we create a .rela.got
		     relocation entry to initialize the value.	This is
		     done in the finish_dynamic_symbol routine.	 */
		  if ((off & 1) != 0)
		    off &= ~1;
		  else
		    {
		      bfd_put_64 (output_bfd, relocation,
				  htab->sgot->contents + off);
		      h->got.offset |= 1;
		    }
		}
	      else
		unresolved_reloc = false;
	    }
	  else
	    {
	      if (local_got_offsets == NULL)
		abort ();

	      off = local_got_offsets[r_symndx];

	      /* The offset must always be a multiple of 8.  We use
		 the least significant bit to record whether we have
		 already generated the necessary reloc.	 */
	      if ((off & 1) != 0)
		off &= ~1;
	      else
		{
		  bfd_put_64 (output_bfd, relocation,
			      htab->sgot->contents + off);

		  if (info->shared)
		    {
		      asection *srelgot;
		      Elf_Internal_Rela outrel;
		      Elf64_External_Rela *loc;

		      /* We need to generate a R_X86_64_RELATIVE reloc
			 for the dynamic linker.  */
		      srelgot = htab->srelgot;
		      if (srelgot == NULL)
			abort ();

		      outrel.r_offset = (htab->sgot->output_section->vma
					 + htab->sgot->output_offset
					 + off);
		      outrel.r_info = ELF64_R_INFO (0, R_X86_64_RELATIVE);
		      outrel.r_addend = relocation;
		      loc = (Elf64_External_Rela *) srelgot->contents;
		      loc += srelgot->reloc_count++;
		      bfd_elf64_swap_reloca_out (output_bfd, &outrel, loc);
		    }

		  local_got_offsets[r_symndx] |= 1;
		}
	    }

	  if (off >= (bfd_vma) -2)
	    abort ();

	  relocation = htab->sgot->output_offset + off;
	  if (r_type == R_X86_64_GOTPCREL)
	    relocation += htab->sgot->output_section->vma;

	  break;

	case R_X86_64_PLT32:
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table.  */

	  /* Resolve a PLT32 reloc against a local symbol directly,
	     without using the procedure linkage table.	 */
	  if (h == NULL)
	    break;

	  if (h->plt.offset == (bfd_vma) -1
	      || htab->splt == NULL)
	    {
	      /* We didn't make a PLT entry for this symbol.  This
		 happens when statically linking PIC code, or when
		 using -Bsymbolic.  */
	      break;
	    }

	  relocation = (htab->splt->output_section->vma
			+ htab->splt->output_offset
			+ h->plt.offset);
	  unresolved_reloc = false;
	  break;

	case R_X86_64_PC8:
	case R_X86_64_PC16:
	case R_X86_64_PC32:
	case R_X86_64_8:
	case R_X86_64_16:
	case R_X86_64_32:
	case R_X86_64_64:
	  /* FIXME: The ABI says the linker should make sure the value is
	     the same when it's zeroextended to 64 bit.	 */

	  /* r_symndx will be zero only for relocs against symbols
	     from removed linkonce sections, or sections discarded by
	     a linker script.  */
	  if (r_symndx == 0
	      || (input_section->flags & SEC_ALLOC) == 0)
	    break;

	  if ((info->shared
	       && ((r_type != R_X86_64_PC8
		    && r_type != R_X86_64_PC16
		    && r_type != R_X86_64_PC32)
		   || (h != NULL
		       && h->dynindx != -1
		       && (! info->symbolic
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))))
	      || (!info->shared
		  && h != NULL
		  && h->dynindx != -1
		  && (h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0
		  && (((h->elf_link_hash_flags
			& ELF_LINK_HASH_DEF_DYNAMIC) != 0
		       && (h->elf_link_hash_flags
			   & ELF_LINK_HASH_DEF_REGULAR) == 0)
		      || h->root.type == bfd_link_hash_undefweak
		      || h->root.type == bfd_link_hash_undefined)))
	    {
	      Elf_Internal_Rela outrel;
	      boolean skip, relocate;
	      asection *sreloc;
	      Elf64_External_Rela *loc;

	      /* When generating a shared object, these relocations
		 are copied into the output file to be resolved at run
		 time.	*/

	      skip = false;
	      relocate = false;

	      outrel.r_offset =
		_bfd_elf_section_offset (output_bfd, info, input_section,
					 rel->r_offset);
	      if (outrel.r_offset == (bfd_vma) -1)
		skip = true;
	      else if (outrel.r_offset == (bfd_vma) -2)
		skip = true, relocate = true;

	      outrel.r_offset += (input_section->output_section->vma
				  + input_section->output_offset);

	      if (skip)
		memset (&outrel, 0, sizeof outrel);

	      /* h->dynindx may be -1 if this symbol was marked to
		 become local.  */
	      else if (h != NULL
		       && h->dynindx != -1
		       && (r_type == R_X86_64_PC8
			   || r_type == R_X86_64_PC16
			   || r_type == R_X86_64_PC32
			   || !info->shared
			   || !info->symbolic
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))
		{
		  outrel.r_info = ELF64_R_INFO (h->dynindx, r_type);
		  outrel.r_addend = rel->r_addend;
		}
	      else
		{
		  /* This symbol is local, or marked to become local.  */
		  if (r_type == R_X86_64_64)
		    {
		      relocate = true;
		      outrel.r_info = ELF64_R_INFO (0, R_X86_64_RELATIVE);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		  else
		    {
		      long sindx;

		      if (h == NULL)
			sec = local_sections[r_symndx];
		      else
			{
			  BFD_ASSERT (h->root.type == bfd_link_hash_defined
				      || (h->root.type
					  == bfd_link_hash_defweak));
			  sec = h->root.u.def.section;
			}
		      if (sec != NULL && bfd_is_abs_section (sec))
			sindx = 0;
		      else if (sec == NULL || sec->owner == NULL)
			{
			  bfd_set_error (bfd_error_bad_value);
			  return false;
			}
		      else
			{
			  asection *osec;

			  osec = sec->output_section;
			  sindx = elf_section_data (osec)->dynindx;
			  BFD_ASSERT (sindx > 0);
			}

		      outrel.r_info = ELF64_R_INFO (sindx, r_type);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		}

	      sreloc = elf_section_data (input_section)->sreloc;
	      if (sreloc == NULL)
		abort ();

	      loc = (Elf64_External_Rela *) sreloc->contents;
	      loc += sreloc->reloc_count++;
	      bfd_elf64_swap_reloca_out (output_bfd, &outrel, loc);

	      /* If this reloc is against an external symbol, we do
		 not want to fiddle with the addend.  Otherwise, we
		 need to include the symbol value so that it becomes
		 an addend for the dynamic reloc.  */
	      if (! relocate)
		continue;
	    }

	  break;

	case R_X86_64_TLSGD:
	case R_X86_64_GOTTPOFF:
	  r_type = elf64_x86_64_tls_transition (info, r_type, h == NULL);
	  tls_type = GOT_UNKNOWN;
	  if (h == NULL && local_got_offsets)
	    tls_type = elf64_x86_64_local_got_tls_type (input_bfd) [r_symndx];
	  else if (h != NULL)
	    {
	      tls_type = elf64_x86_64_hash_entry (h)->tls_type;
	      if (!info->shared && h->dynindx == -1 && tls_type == GOT_TLS_IE)
		r_type = R_X86_64_TPOFF32;
	    }
	  if (r_type == R_X86_64_TLSGD)
	    {
	      if (tls_type == GOT_TLS_IE)
		r_type = R_X86_64_GOTTPOFF;
	    }

	  if (r_type == R_X86_64_TPOFF32)
	    {
	      BFD_ASSERT (! unresolved_reloc);
	      if (ELF64_R_TYPE (rel->r_info) == R_X86_64_TLSGD)
		{
		  unsigned int i;
		  static unsigned char tlsgd[8]
		    = { 0x66, 0x48, 0x8d, 0x3d, 0x66, 0x66, 0x48, 0xe8 };

		  /* GD->LE transition.
		     .byte 0x66; leaq foo@tlsgd(%rip), %rdi
		     .word 0x6666; rex64; call __tls_get_addr@plt
		     Change it into:
		     movq %fs:0, %rax
		     leaq foo@tpoff(%rax), %rax */
		  BFD_ASSERT (rel->r_offset >= 4);
		  for (i = 0; i < 4; i++)
		    BFD_ASSERT (bfd_get_8 (input_bfd,
					   contents + rel->r_offset - 4 + i)
				== tlsgd[i]);
		  BFD_ASSERT (rel->r_offset + 12 <= input_section->_raw_size);
		  for (i = 0; i < 4; i++)
		    BFD_ASSERT (bfd_get_8 (input_bfd,
					   contents + rel->r_offset + 4 + i)
				== tlsgd[i+4]);
		  BFD_ASSERT (rel + 1 < relend);
		  BFD_ASSERT (ELF64_R_TYPE (rel[1].r_info) == R_X86_64_PLT32);
		  memcpy (contents + rel->r_offset - 4,
			  "\x64\x48\x8b\x04\x25\0\0\0\0\x48\x8d\x80\0\0\0",
			  16);
		  bfd_put_32 (output_bfd, tpoff (info, relocation),
			      contents + rel->r_offset + 8);
		  /* Skip R_X86_64_PLT32.  */
		  rel++;
		  continue;
		}
	      else
		{
		  unsigned int val, type, reg;

		  /* IE->LE transition:
		     Originally it can be one of:
		     movq foo@gottpoff(%rip), %reg
		     addq foo@gottpoff(%rip), %reg
		     We change it into:
		     movq $foo, %reg
		     leaq foo(%reg), %reg
		     addq $foo, %reg.  */
		  BFD_ASSERT (rel->r_offset >= 3);
		  val = bfd_get_8 (input_bfd, contents + rel->r_offset - 3);
		  BFD_ASSERT (val == 0x48 || val == 0x4c);
		  type = bfd_get_8 (input_bfd, contents + rel->r_offset - 2);
		  BFD_ASSERT (type == 0x8b || type == 0x03);
		  reg = bfd_get_8 (input_bfd, contents + rel->r_offset - 1);
		  BFD_ASSERT ((reg & 0xc7) == 5);
		  reg >>= 3;
		  BFD_ASSERT (rel->r_offset + 4 <= input_section->_raw_size);
		  if (type == 0x8b)
		    {
		      /* movq */
		      if (val == 0x4c)
			bfd_put_8 (output_bfd, 0x49,
				   contents + rel->r_offset - 3);
		      bfd_put_8 (output_bfd, 0xc7,
				 contents + rel->r_offset - 2);
		      bfd_put_8 (output_bfd, 0xc0 | reg,
				 contents + rel->r_offset - 1);
		    }
		  else if (reg == 4)
		    {
		      /* addq -> addq - addressing with %rsp/%r12 is
			 special  */
		      if (val == 0x4c)
			bfd_put_8 (output_bfd, 0x49,
				   contents + rel->r_offset - 3);
		      bfd_put_8 (output_bfd, 0x81,
				 contents + rel->r_offset - 2);
		      bfd_put_8 (output_bfd, 0xc0 | reg,
				 contents + rel->r_offset - 1);
		    }
		  else
		    {
		      /* addq -> leaq */
		      if (val == 0x4c)
			bfd_put_8 (output_bfd, 0x4d,
				   contents + rel->r_offset - 3);
		      bfd_put_8 (output_bfd, 0x8d,
				 contents + rel->r_offset - 2);
		      bfd_put_8 (output_bfd, 0x80 | reg | (reg << 3),
				 contents + rel->r_offset - 1);
		    }
		  bfd_put_32 (output_bfd, tpoff (info, relocation),
			      contents + rel->r_offset);
		  continue;
		}
	    }

	  if (htab->sgot == NULL)
	    abort ();

	  if (h != NULL)
	    off = h->got.offset;
	  else
	    {
	      if (local_got_offsets == NULL)
		abort ();

	      off = local_got_offsets[r_symndx];
	    }

	  if ((off & 1) != 0)
	    off &= ~1;
          else
	    {
	      Elf_Internal_Rela outrel;
	      Elf64_External_Rela *loc;
	      int dr_type, indx;

	      if (htab->srelgot == NULL)
		abort ();

	      outrel.r_offset = (htab->sgot->output_section->vma
				 + htab->sgot->output_offset + off);

	      indx = h && h->dynindx != -1 ? h->dynindx : 0;
	      if (r_type == R_X86_64_TLSGD)
		dr_type = R_X86_64_DTPMOD64;
	      else
		dr_type = R_X86_64_TPOFF64;

	      bfd_put_64 (output_bfd, 0, htab->sgot->contents + off);
	      outrel.r_addend = 0;
	      if (dr_type == R_X86_64_TPOFF64 && indx == 0)
		outrel.r_addend = relocation - dtpoff_base (info);
	      outrel.r_info = ELF64_R_INFO (indx, dr_type);

	      loc = (Elf64_External_Rela *) htab->srelgot->contents;
	      loc += htab->srelgot->reloc_count++;
	      bfd_elf64_swap_reloca_out (output_bfd, &outrel, loc);

	      if (r_type == R_X86_64_TLSGD)
		{
		  if (indx == 0)
		    {
	    	      BFD_ASSERT (! unresolved_reloc);
		      bfd_put_64 (output_bfd,
				  relocation - dtpoff_base (info),
				  htab->sgot->contents + off + GOT_ENTRY_SIZE);
		    }
		  else
		    {
		      bfd_put_64 (output_bfd, 0,
				  htab->sgot->contents + off + GOT_ENTRY_SIZE);
		      outrel.r_info = ELF64_R_INFO (indx,
						    R_X86_64_DTPOFF64);
		      outrel.r_offset += GOT_ENTRY_SIZE;
		      htab->srelgot->reloc_count++;
		      loc++;
		      bfd_elf64_swap_reloca_out (output_bfd, &outrel,
						 loc);
		    }
		}

	      if (h != NULL)
		h->got.offset |= 1;
	      else
		local_got_offsets[r_symndx] |= 1;
	    }

	  if (off >= (bfd_vma) -2)
	    abort ();
	  if (r_type == ELF64_R_TYPE (rel->r_info))
	    {
	      relocation = htab->sgot->output_section->vma
			   + htab->sgot->output_offset + off;
	      unresolved_reloc = false;
	    }
	  else
	    {
	      unsigned int i;
	      static unsigned char tlsgd[8]
		= { 0x66, 0x48, 0x8d, 0x3d, 0x66, 0x66, 0x48, 0xe8 };

	      /* GD->IE transition.
		 .byte 0x66; leaq foo@tlsgd(%rip), %rdi
		 .word 0x6666; rex64; call __tls_get_addr@plt
		 Change it into:
		 movq %fs:0, %rax
		 addq foo@gottpoff(%rip), %rax */
	      BFD_ASSERT (rel->r_offset >= 4);
	      for (i = 0; i < 4; i++)
	        BFD_ASSERT (bfd_get_8 (input_bfd,
				       contents + rel->r_offset - 4 + i)
			    == tlsgd[i]);
	      BFD_ASSERT (rel->r_offset + 12 <= input_section->_raw_size);
	      for (i = 0; i < 4; i++)
	        BFD_ASSERT (bfd_get_8 (input_bfd,
				       contents + rel->r_offset + 4 + i)
			    == tlsgd[i+4]);
	      BFD_ASSERT (rel + 1 < relend);
	      BFD_ASSERT (ELF64_R_TYPE (rel[1].r_info) == R_X86_64_PLT32);
	      memcpy (contents + rel->r_offset - 4,
		      "\x64\x48\x8b\x04\x25\0\0\0\0\x48\x03\x05\0\0\0",
		      16);

	      relocation = (htab->sgot->output_section->vma
			    + htab->sgot->output_offset + off
			    - rel->r_offset
			    - input_section->output_section->vma
			    - input_section->output_offset
			    - 12);
	      bfd_put_32 (output_bfd, relocation,
			  contents + rel->r_offset + 8);
	      /* Skip R_X86_64_PLT32.  */
	      rel++;
	      continue;
	    }
	  break;

	case R_X86_64_TLSLD:
	  if (! info->shared)
	    {
	      /* LD->LE transition:
		 Ensure it is:
		 leaq foo@tlsld(%rip), %rdi; call __tls_get_addr@plt.
		 We change it into:
		 .word 0x6666; .byte 0x66; movl %fs:0, %rax.  */
	      BFD_ASSERT (rel->r_offset >= 3);
	      BFD_ASSERT (bfd_get_8 (input_bfd, contents + rel->r_offset - 3)
			  == 0x48);
	      BFD_ASSERT (bfd_get_8 (input_bfd, contents + rel->r_offset - 2)
			  == 0x8d);
	      BFD_ASSERT (bfd_get_8 (input_bfd, contents + rel->r_offset - 1)
			  == 0x3d);
	      BFD_ASSERT (rel->r_offset + 9 <= input_section->_raw_size);
	      BFD_ASSERT (bfd_get_8 (input_bfd, contents + rel->r_offset + 4)
			  == 0xe8);
	      BFD_ASSERT (rel + 1 < relend);
	      BFD_ASSERT (ELF64_R_TYPE (rel[1].r_info) == R_X86_64_PLT32);
	      memcpy (contents + rel->r_offset - 3,
		      "\x66\x66\x66\x64\x48\x8b\x04\x25\0\0\0", 12);
	      /* Skip R_X86_64_PLT32.  */
	      rel++;
	      continue;
	    }

	  if (htab->sgot == NULL)
	    abort ();

	  off = htab->tls_ld_got.offset;
	  if (off & 1)
	    off &= ~1;
	  else
	    {
	      Elf_Internal_Rela outrel;
	      Elf64_External_Rela *loc;

	      if (htab->srelgot == NULL)
		abort ();

	      outrel.r_offset = (htab->sgot->output_section->vma
				 + htab->sgot->output_offset + off);

	      bfd_put_64 (output_bfd, 0,
			  htab->sgot->contents + off);
	      bfd_put_64 (output_bfd, 0,
			  htab->sgot->contents + off + GOT_ENTRY_SIZE);
	      outrel.r_info = ELF64_R_INFO (0, R_X86_64_DTPMOD64);
	      outrel.r_addend = 0;
	      loc = (Elf64_External_Rela *) htab->srelgot->contents;
	      loc += htab->srelgot->reloc_count++;
	      bfd_elf64_swap_reloca_out (output_bfd, &outrel, loc);
	      htab->tls_ld_got.offset |= 1;
	    }
	  relocation = htab->sgot->output_section->vma
		       + htab->sgot->output_offset + off;
	  unresolved_reloc = false;
	  break;

	case R_X86_64_DTPOFF32:
	  if (info->shared || (input_section->flags & SEC_CODE) == 0)
	    relocation -= dtpoff_base (info);
	  else
	    relocation = tpoff (info, relocation);
	  break;

	case R_X86_64_TPOFF32:
	  BFD_ASSERT (! info->shared);
	  relocation = tpoff (info, relocation);
	  break;

	default:
	  break;
	}

      /* Dynamic relocs are not propagated for SEC_DEBUGGING sections
	 because such sections are not SEC_ALLOC and thus ld.so will
	 not process them.  */
      if (unresolved_reloc
	  && !((input_section->flags & SEC_DEBUGGING) != 0
	       && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0))
	(*_bfd_error_handler)
	  (_("%s(%s+0x%lx): unresolvable relocation against symbol `%s'"),
	   bfd_archive_filename (input_bfd),
	   bfd_get_section_name (input_bfd, input_section),
	   (long) rel->r_offset,
	   h->root.root.string);

      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				    contents, rel->r_offset,
				    relocation, rel->r_addend);

      if (r != bfd_reloc_ok)
	{
	  const char *name;

	  if (h != NULL)
	    name = h->root.root.string;
	  else
	    {
	      name = bfd_elf_string_from_elf_section (input_bfd,
						      symtab_hdr->sh_link,
						      sym->st_name);
	      if (name == NULL)
		return false;
	      if (*name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }

	  if (r == bfd_reloc_overflow)
	    {

	      if (! ((*info->callbacks->reloc_overflow)
		     (info, name, howto->name, (bfd_vma) 0,
		      input_bfd, input_section, rel->r_offset)))
		return false;
	    }
	  else
	    {
	      (*_bfd_error_handler)
		(_("%s(%s+0x%lx): reloc against `%s': error %d"),
		 bfd_archive_filename (input_bfd),
		 bfd_get_section_name (input_bfd, input_section),
		 (long) rel->r_offset, name, (int) r);
	      return false;
	    }
	}
    }

  return true;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static boolean
elf64_x86_64_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  struct elf64_x86_64_link_hash_table *htab;

  htab = elf64_x86_64_hash_table (info);

  if (h->plt.offset != (bfd_vma) -1)
    {
      bfd_vma plt_index;
      bfd_vma got_offset;
      Elf_Internal_Rela rela;
      Elf64_External_Rela *loc;

      /* This symbol has an entry in the procedure linkage table.  Set
	 it up.	 */

      if (h->dynindx == -1
	  || htab->splt == NULL
	  || htab->sgotplt == NULL
	  || htab->srelplt == NULL)
	abort ();

      /* Get the index in the procedure linkage table which
	 corresponds to this symbol.  This is the index of this symbol
	 in all the symbols for which we are making plt entries.  The
	 first entry in the procedure linkage table is reserved.  */
      plt_index = h->plt.offset / PLT_ENTRY_SIZE - 1;

      /* Get the offset into the .got table of the entry that
	 corresponds to this function.	Each .got entry is GOT_ENTRY_SIZE
	 bytes. The first three are reserved for the dynamic linker.  */
      got_offset = (plt_index + 3) * GOT_ENTRY_SIZE;

      /* Fill in the entry in the procedure linkage table.  */
      memcpy (htab->splt->contents + h->plt.offset, elf64_x86_64_plt_entry,
	      PLT_ENTRY_SIZE);

      /* Insert the relocation positions of the plt section.  The magic
	 numbers at the end of the statements are the positions of the
	 relocations in the plt section.  */
      /* Put offset for jmp *name@GOTPCREL(%rip), since the
	 instruction uses 6 bytes, subtract this value.  */
      bfd_put_32 (output_bfd,
		      (htab->sgotplt->output_section->vma
		       + htab->sgotplt->output_offset
		       + got_offset
		       - htab->splt->output_section->vma
		       - htab->splt->output_offset
		       - h->plt.offset
		       - 6),
		  htab->splt->contents + h->plt.offset + 2);
      /* Put relocation index.  */
      bfd_put_32 (output_bfd, plt_index,
		  htab->splt->contents + h->plt.offset + 7);
      /* Put offset for jmp .PLT0.  */
      bfd_put_32 (output_bfd, - (h->plt.offset + PLT_ENTRY_SIZE),
		  htab->splt->contents + h->plt.offset + 12);

      /* Fill in the entry in the global offset table, initially this
	 points to the pushq instruction in the PLT which is at offset 6.  */
      bfd_put_64 (output_bfd, (htab->splt->output_section->vma
			       + htab->splt->output_offset
			       + h->plt.offset + 6),
		  htab->sgotplt->contents + got_offset);

      /* Fill in the entry in the .rela.plt section.  */
      rela.r_offset = (htab->sgotplt->output_section->vma
		       + htab->sgotplt->output_offset
		       + got_offset);
      rela.r_info = ELF64_R_INFO (h->dynindx, R_X86_64_JUMP_SLOT);
      rela.r_addend = 0;
      loc = (Elf64_External_Rela *) htab->srelplt->contents + plt_index;
      bfd_elf64_swap_reloca_out (output_bfd, &rela, loc);

      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  This is a clue
	     for the dynamic linker, to make function pointer
	     comparisons work between an application and shared
	     library.  */
	  sym->st_shndx = SHN_UNDEF;
	}
    }

  if (h->got.offset != (bfd_vma) -1
      && elf64_x86_64_hash_entry (h)->tls_type != GOT_TLS_GD
      && elf64_x86_64_hash_entry (h)->tls_type != GOT_TLS_IE)
    {
      Elf_Internal_Rela rela;
      Elf64_External_Rela *loc;

      /* This symbol has an entry in the global offset table.  Set it
	 up.  */

      if (htab->sgot == NULL || htab->srelgot == NULL)
	abort ();

      rela.r_offset = (htab->sgot->output_section->vma
		       + htab->sgot->output_offset
		       + (h->got.offset &~ (bfd_vma) 1));

      /* If this is a static link, or it is a -Bsymbolic link and the
	 symbol is defined locally or was forced to be local because
	 of a version file, we just want to emit a RELATIVE reloc.
	 The entry in the global offset table will already have been
	 initialized in the relocate_section function.  */
      if (info->shared
	  && (info->symbolic
	      || h->dynindx == -1
	      || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL))
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	{
	  BFD_ASSERT((h->got.offset & 1) != 0);
	  rela.r_info = ELF64_R_INFO (0, R_X86_64_RELATIVE);
	  rela.r_addend = (h->root.u.def.value
			   + h->root.u.def.section->output_section->vma
			   + h->root.u.def.section->output_offset);
	}
      else
	{
	  BFD_ASSERT((h->got.offset & 1) == 0);
	  bfd_put_64 (output_bfd, (bfd_vma) 0,
		      htab->sgot->contents + h->got.offset);
	  rela.r_info = ELF64_R_INFO (h->dynindx, R_X86_64_GLOB_DAT);
	  rela.r_addend = 0;
	}

      loc = (Elf64_External_Rela *) htab->srelgot->contents;
      loc += htab->srelgot->reloc_count++;
      bfd_elf64_swap_reloca_out (output_bfd, &rela, loc);
    }

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_COPY) != 0)
    {
      Elf_Internal_Rela rela;
      Elf64_External_Rela *loc;

      /* This symbol needs a copy reloc.  Set it up.  */

      if (h->dynindx == -1
	  || (h->root.type != bfd_link_hash_defined
	      && h->root.type != bfd_link_hash_defweak)
	  || htab->srelbss == NULL)
	abort ();

      rela.r_offset = (h->root.u.def.value
		       + h->root.u.def.section->output_section->vma
		       + h->root.u.def.section->output_offset);
      rela.r_info = ELF64_R_INFO (h->dynindx, R_X86_64_COPY);
      rela.r_addend = 0;
      loc = (Elf64_External_Rela *) htab->srelbss->contents;
      loc += htab->srelbss->reloc_count++;
      bfd_elf64_swap_reloca_out (output_bfd, &rela, loc);
    }

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return true;
}

/* Used to decide how to sort relocs in an optimal manner for the
   dynamic linker, before writing them out.  */

static enum elf_reloc_type_class
elf64_x86_64_reloc_type_class (rela)
     const Elf_Internal_Rela *rela;
{
  switch ((int) ELF64_R_TYPE (rela->r_info))
    {
    case R_X86_64_RELATIVE:
      return reloc_class_relative;
    case R_X86_64_JUMP_SLOT:
      return reloc_class_plt;
    case R_X86_64_COPY:
      return reloc_class_copy;
    default:
      return reloc_class_normal;
    }
}

/* Finish up the dynamic sections.  */

static boolean
elf64_x86_64_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  struct elf64_x86_64_link_hash_table *htab;
  bfd *dynobj;
  asection *sdyn;

  htab = elf64_x86_64_hash_table (info);
  dynobj = htab->elf.dynobj;
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (htab->elf.dynamic_sections_created)
    {
      Elf64_External_Dyn *dyncon, *dynconend;

      if (sdyn == NULL || htab->sgot == NULL)
	abort ();

      dyncon = (Elf64_External_Dyn *) sdyn->contents;
      dynconend = (Elf64_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  asection *s;

	  bfd_elf64_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      continue;

	    case DT_PLTGOT:
	      dyn.d_un.d_ptr = htab->sgot->output_section->vma;
	      break;

	    case DT_JMPREL:
	      dyn.d_un.d_ptr = htab->srelplt->output_section->vma;
	      break;

	    case DT_PLTRELSZ:
	      s = htab->srelplt->output_section;
	      if (s->_cooked_size != 0)
		dyn.d_un.d_val = s->_cooked_size;
	      else
		dyn.d_un.d_val = s->_raw_size;
	      break;

	    case DT_RELASZ:
	      /* The procedure linkage table relocs (DT_JMPREL) should
		 not be included in the overall relocs (DT_RELA).
		 Therefore, we override the DT_RELASZ entry here to
		 make it not include the JMPREL relocs.  Since the
		 linker script arranges for .rela.plt to follow all
		 other relocation sections, we don't have to worry
		 about changing the DT_RELA entry.  */
	      if (htab->srelplt != NULL)
		{
		  s = htab->srelplt->output_section;
		  if (s->_cooked_size != 0)
		    dyn.d_un.d_val -= s->_cooked_size;
		  else
		    dyn.d_un.d_val -= s->_raw_size;
		}
	      break;
	    }

	  bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	}

      /* Fill in the special first entry in the procedure linkage table.  */
      if (htab->splt && htab->splt->_raw_size > 0)
	{
	  /* Fill in the first entry in the procedure linkage table.  */
	  memcpy (htab->splt->contents, elf64_x86_64_plt0_entry,
		  PLT_ENTRY_SIZE);
	  /* Add offset for pushq GOT+8(%rip), since the instruction
	     uses 6 bytes subtract this value.  */
	  bfd_put_32 (output_bfd,
		      (htab->sgotplt->output_section->vma
		       + htab->sgotplt->output_offset
		       + 8
		       - htab->splt->output_section->vma
		       - htab->splt->output_offset
		       - 6),
		      htab->splt->contents + 2);
	  /* Add offset for jmp *GOT+16(%rip). The 12 is the offset to
	     the end of the instruction.  */
	  bfd_put_32 (output_bfd,
		      (htab->sgotplt->output_section->vma
		       + htab->sgotplt->output_offset
		       + 16
		       - htab->splt->output_section->vma
		       - htab->splt->output_offset
		       - 12),
		      htab->splt->contents + 8);

	  elf_section_data (htab->splt->output_section)->this_hdr.sh_entsize =
	    PLT_ENTRY_SIZE;
	}
    }

  if (htab->sgotplt)
    {
      /* Fill in the first three entries in the global offset table.  */
      if (htab->sgotplt->_raw_size > 0)
	{
	  /* Set the first entry in the global offset table to the address of
	     the dynamic section.  */
	  if (sdyn == NULL)
	    bfd_put_64 (output_bfd, (bfd_vma) 0, htab->sgotplt->contents);
	  else
	    bfd_put_64 (output_bfd,
			sdyn->output_section->vma + sdyn->output_offset,
			htab->sgotplt->contents);
	  /* Write GOT[1] and GOT[2], needed for the dynamic linker.  */
	  bfd_put_64 (output_bfd, (bfd_vma) 0, htab->sgotplt->contents + GOT_ENTRY_SIZE);
	  bfd_put_64 (output_bfd, (bfd_vma) 0, htab->sgotplt->contents + GOT_ENTRY_SIZE*2);
	}

      elf_section_data (htab->sgotplt->output_section)->this_hdr.sh_entsize =
	GOT_ENTRY_SIZE;
    }

  return true;
}


#define TARGET_LITTLE_SYM		    bfd_elf64_x86_64_vec
#define TARGET_LITTLE_NAME		    "elf64-x86-64"
#define ELF_ARCH			    bfd_arch_i386
#define ELF_MACHINE_CODE		    EM_X86_64
#define ELF_MAXPAGESIZE			    0x100000

#define elf_backend_can_gc_sections	    1
#define elf_backend_can_refcount	    1
#define elf_backend_want_got_plt	    1
#define elf_backend_plt_readonly	    1
#define elf_backend_want_plt_sym	    0
#define elf_backend_got_header_size	    (GOT_ENTRY_SIZE*3)
#define elf_backend_plt_header_size	    PLT_ENTRY_SIZE
#define elf_backend_rela_normal		    1

#define elf_info_to_howto		    elf64_x86_64_info_to_howto

#define bfd_elf64_bfd_link_hash_table_create \
  elf64_x86_64_link_hash_table_create
#define bfd_elf64_bfd_reloc_type_lookup	    elf64_x86_64_reloc_type_lookup

#define elf_backend_adjust_dynamic_symbol   elf64_x86_64_adjust_dynamic_symbol
#define elf_backend_check_relocs	    elf64_x86_64_check_relocs
#define elf_backend_copy_indirect_symbol    elf64_x86_64_copy_indirect_symbol
#define elf_backend_create_dynamic_sections elf64_x86_64_create_dynamic_sections
#define elf_backend_finish_dynamic_sections elf64_x86_64_finish_dynamic_sections
#define elf_backend_finish_dynamic_symbol   elf64_x86_64_finish_dynamic_symbol
#define elf_backend_gc_mark_hook	    elf64_x86_64_gc_mark_hook
#define elf_backend_gc_sweep_hook	    elf64_x86_64_gc_sweep_hook
#define elf_backend_grok_prstatus	    elf64_x86_64_grok_prstatus
#define elf_backend_grok_psinfo		    elf64_x86_64_grok_psinfo
#define elf_backend_reloc_type_class	    elf64_x86_64_reloc_type_class
#define elf_backend_relocate_section	    elf64_x86_64_relocate_section
#define elf_backend_size_dynamic_sections   elf64_x86_64_size_dynamic_sections
#define elf_backend_object_p		    elf64_x86_64_elf_object_p
#define bfd_elf64_mkobject		    elf64_x86_64_mkobject

#include "elf64-target.h"
