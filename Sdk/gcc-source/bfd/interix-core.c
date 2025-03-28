/* Interix (PEI) core file support for BFD.
   Copyright (C) 1995-97, 1999 Free Software Foundation, Inc.

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

/* Derived from other various bfd core file packages.
   -- Donn Terry, 5/99 */

#define COFF_WITH_PE
#define COFF_IMAGE_WITH_PE

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "coff/i386.h"
#include "coff/pe.h"
#include "libcoff.h"
#include "libpei.h"

#include <signal.h>
#include <sys/procfs.h>

/* We assume we're self-hosting only, so we can get away with this. */
#ifdef _M_ALPHA
#define TARGET_SYM nt_alphapei_vec
#endif
#ifdef _M_IX86
#define TARGET_SYM i386pei_vec
#endif

extern bfd_target TARGET_SYM;

/* Forward declarations. */
static char *interix_core_file_failing_command PARAMS ((bfd *));
static int interix_core_file_failing_signal PARAMS ((bfd *));
static boolean interix_core_file_matches_executable_p 
  PARAMS ((bfd *, bfd *));

static bfd_coff_backend_data interix_core_swap_table;

char *
interix_core_file_failing_command (abfd)
     bfd *abfd;
{
  asection *section;
  psinfo_t *p;

  section = bfd_get_section_by_name(abfd,".psinfo");

  if (section == NULL)
      return NULL;

  p = (psinfo_t *)section->contents;

  if (p == NULL)
    {
      p = (psinfo_t *)bfd_alloc(abfd, section->_raw_size);
      bfd_get_section_contents(abfd, section, p, 0, section->_raw_size);
      section->contents = (unsigned char *)p;
      section->flags |= SEC_IN_MEMORY;
    }

  return p->pr_psargs;
}

/* Return the number of the signal that caused the core dump.  Presumably,
   since we have a core file, we got a signal of some kind, so don't bother
   checking the other process status fields, just return the signal number.
   */

int
interix_core_file_failing_signal (abfd)
     bfd *abfd;
{
  asection *section;
  pstatus_t *p;

  section = bfd_get_section_by_name(abfd,".pstatus");
  if (section == NULL)
      return -1;

  p = (pstatus_t *)section->contents;

  if (p == NULL)
    {
      p = (pstatus_t *)bfd_alloc(abfd, section->_raw_size);
      bfd_get_section_contents(abfd, section, p, 0, section->_raw_size);
      section->contents = (unsigned char *)p;
      section->flags |= SEC_IN_MEMORY;
    }

  return p->pr_lwp.pr_cursig;
}

/* Check to see if the core file could reasonably be expected to have
   come for the current executable file.  Note that by default we return
   true unless we find something that indicates that there might be a
   problem.
   */

boolean
interix_core_file_matches_executable_p (core_bfd, exec_bfd)
     bfd *core_bfd;
     bfd *exec_bfd;
{
  char *corename;
  char *execname;
  asection *section;
  psinfo_t *p;

  /* First, xvecs must match since both are PEI files for the same target. */

  if (exec_bfd->xvec != &TARGET_SYM)
    {
      bfd_set_error (bfd_error_system_call);
      return false;
    }

  section = bfd_get_section_by_name(core_bfd,".psinfo");

  /* Probably not a core file */
  if (section == NULL)
      return false;

  p = (psinfo_t *)section->contents;

  if (p == NULL)
    {
      p = (psinfo_t *)bfd_alloc(core_bfd, section->_raw_size);
      bfd_get_section_contents(core_bfd, section, p, 0, section->_raw_size);
      section->contents = (unsigned char *)p;
      section->flags |= SEC_IN_MEMORY;
    }

  corename = p->pr_fname;

  /* Find the last component of the executable pathname. */

  if ((execname = strrchr (exec_bfd->filename, '/')) != NULL)
    {
      execname++;
    }
  else
    {
      execname = (char *) exec_bfd->filename;
    }

  /* See if they match */

  return strcmp (execname, corename) ? false : true;

}

static boolean interix_core_bad_format_hook 
    PARAMS (( bfd *abfd, PTR internal_filehdr));

static boolean
interix_core_bad_format_hook (abfd, filehdr)
     bfd * abfd ATTRIBUTE_UNUSED;
     PTR filehdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;

  if (internal_f->f_magic != I386MAGIC+0x1000)
    return false;

  return true;
}


/* When our swap_filehdr_in is called the first time, it updates the rest
   of the data structure to match what we need.  This is a bit strange
   because the entry points we need are defined in coffcode.h as statics.
   We can't change that because there are at least two copies (one for
   pe and one for pei, which are slightly different) and possibly more
   if we allow multiple architectures.  (Yeah: we could rewrite coffcode.h
   to generate unique names, but Not Right Now.)  */

#define backend_info ((bfd_coff_backend_data *) (TARGET_SYM.backend_data))

static void interix_core_swap_filehdr_in PARAMS((bfd *, PTR, PTR));

static void
interix_core_swap_filehdr_in (abfd, src, dst)
     bfd            *abfd;
     PTR	     src;
     PTR	     dst;
{
    interix_core_swap_table._bfd_coff_mkobject_hook = 
	backend_info->_bfd_coff_mkobject_hook;
    interix_core_swap_table._bfd_coff_swap_scnhdr_in = 
	backend_info->_bfd_coff_swap_scnhdr_in;
    interix_core_swap_table._bfd_set_alignment_hook = 
	backend_info->_bfd_set_alignment_hook;
    interix_core_swap_table._bfd_styp_to_sec_flags_hook = 
	backend_info->_bfd_styp_to_sec_flags_hook;
    interix_core_swap_table._bfd_coff_set_arch_mach_hook = 
	backend_info->_bfd_coff_set_arch_mach_hook;

    /* And never come back here again. */
    interix_core_swap_table._bfd_coff_swap_filehdr_in = 
	backend_info->_bfd_coff_swap_filehdr_in;

    /* Do our real work. */
    (backend_info->_bfd_coff_swap_filehdr_in) (abfd, src, dst);

}

static asymbol *interix_core_make_empty_symbol PARAMS ((bfd *));

/* Needs to be there when making a new section */
static asymbol *
interix_core_make_empty_symbol (abfd)
     bfd *abfd;
{
  asymbol *new = (asymbol *) bfd_zalloc (abfd, sizeof (asymbol));
  if (new)
    new->the_bfd = abfd;
  return new;
}

static const bfd_target *
pe_bfd_core_p (bfd * abfd)
{
  /* pei_bfd_object_p will recognize an Interix core file and an executable.
     If we're not looking for core, bail out. */

  if (abfd->format != bfd_core) 
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  return pei_bfd_object_p (abfd);
}

#define interix_core_get_symtab_upper_bound _bfd_nosymbols_get_symtab_upper_bound
#define interix_core_get_symtab _bfd_nosymbols_get_symtab
#define interix_core_print_symbol _bfd_nosymbols_print_symbol
#define interix_core_get_symbol_info _bfd_nosymbols_get_symbol_info
#define interix_core_bfd_is_local_label_name \
  _bfd_nosymbols_bfd_is_local_label_name
#define interix_core_get_lineno _bfd_nosymbols_get_lineno
#define interix_core_find_nearest_line _bfd_nosymbols_find_nearest_line
#define interix_core_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define interix_core_read_minisymbols _bfd_nosymbols_read_minisymbols
#define interix_core_minisymbol_to_symbol _bfd_nosymbols_minisymbol_to_symbol

static  bfd_coff_backend_data interix_core_swap_table =
{
  NULL, NULL, NULL,
  NULL, NULL,
  NULL, NULL,
  NULL, NULL,
  NULL,
  FILHSZ, AOUTSZ, SCNHSZ, SYMESZ, AUXESZ, RELSZ, LINESZ,
  0,
#ifdef COFF_LONG_FILENAMES
  true,
#else
  false,
#endif
#ifdef COFF_LONG_SECTION_NAMES
  true,
#else
  false,
#endif
  0,
  false,
  0,
  interix_core_swap_filehdr_in, 
  _bfd_pei_swap_aouthdr_in, 
  NULL, /* scnhdr_in */
  NULL, /* reloc_in */
  interix_core_bad_format_hook, 
  NULL, /* arch_mach */
  NULL, /* mkobject */
  NULL, /* styp_to_sec */
  NULL, /* set_align */
  NULL, /* slurp_sym */
  NULL, /* symname_in */
  NULL, /* pointerize_aux */
  NULL, /* print_aux */
  NULL, /* reloc16_extra */
  NULL, /* reloc_16_estimate */
  NULL, /* sym_class_ */
  NULL, /* section_file_pos */
  NULL, /* start_final */
  NULL, /* reloc_sec */
  NULL, /* rtype_to_howto */
  NULL, /* adjust_symndx */
  NULL, /* add_one_symbol */
  NULL, /* output_has_begun */
  NULL, /* final_link_postscript */
  NULL, /* canon_one_symbol */
#ifdef DYNAMIC_LINKING
  NULL, /* swap_dyn_in */
  NULL, /* swap_dyn_out */
  NULL, /* swap_verdef_in */
  NULL, /* swap_verdef_out */
  NULL, /* swap_verdaux_in */
  NULL, /* swap_verdaux_out */
  NULL, /* swap_verneed_in */
  NULL, /* swap_verneed_out */
  NULL, /* swap_vernaux_in */
  NULL, /* swap_vernaux_out */
  NULL, /* swap_versym_in */
  NULL, /* swap_versym_out */
  NULL, /* create_dynamic_sec */
  NULL, /* check_relocs */
  NULL, /* adjust_dynamic_sym */
  NULL, /* size_dynamic_sec */
  NULL, /* finish_dynamic_sym */
  NULL, /* finish_dynamic_secs */
#endif
};

const bfd_target interix_core_vec =
  {
    "interix-core",
    bfd_target_coff_flavour,
    NULL,			/* no alternative type */
    BFD_ENDIAN_LITTLE,		/* target byte order */
    BFD_ENDIAN_LITTLE,		/* target headers byte order */
    (HAS_RELOC | EXEC_P |	/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    0,				/* leading underscore */
    ' ',						   /* ar_pad_char */
    15,							   /* ar_max_namelen */

    bfd_getl64, bfd_getl_signed_64, bfd_putl64,
       bfd_getl32, bfd_getl_signed_32, bfd_putl32,
       bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
    bfd_getl64, bfd_getl_signed_64, bfd_putl64,
       bfd_getl32, bfd_getl_signed_32, bfd_putl32,
       bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

    {_bfd_dummy_target, pe_bfd_core_p, /* bfd_check_format */
     _bfd_dummy_target, pe_bfd_core_p},

    {bfd_false, bfd_false, /* bfd_set_format */
     bfd_false, bfd_false},

    {bfd_false, bfd_false, /* bfd_write_contents */
     bfd_false, bfd_false},

    BFD_JUMP_TABLE_GENERIC (_bfd_generic),
    BFD_JUMP_TABLE_COPY (_bfd_generic),
    BFD_JUMP_TABLE_CORE (interix),
    BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
    BFD_JUMP_TABLE_SYMBOLS (interix_core),
    BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
    BFD_JUMP_TABLE_WRITE (_bfd_generic),
    BFD_JUMP_TABLE_LINK (_bfd_nolink),
    BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    NULL,
    &interix_core_swap_table			/* backend_data */
};
