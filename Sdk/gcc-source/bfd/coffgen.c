/* Support for the generic parts of COFF, for BFD.
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

/* Most of this hacked by  Steve Chamberlain, sac@cygnus.com.
   Split out of coffcode.h by Ian Taylor, ian@cygnus.com.  */

/* This file contains COFF code that is not dependent on any
   particular COFF target.  There is only one version of this file in
   libbfd.a, so no target specific code may be put in here.  Or, to
   put it another way,

   ********** DO NOT PUT TARGET SPECIFIC CODE IN THIS FILE **********

   If you need to add some target specific behaviour, add a new hook
   function to bfd_coff_backend_data.

   Some of these functions are also called by the ECOFF routines.
   Those functions may not use any COFF specific information, such as
   coff_data (abfd).  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "libcoff.h"

static void coff_fix_symbol_name
  PARAMS ((bfd *, asymbol *, combined_entry_type *, bfd_size_type *,
	   asection **, bfd_size_type *));
static boolean coff_write_symbol
  PARAMS ((bfd *, asymbol *, combined_entry_type *, bfd_vma *,
	   bfd_size_type *, asection **, bfd_size_type *));
static boolean coff_write_alien_symbol
  PARAMS ((bfd *, asymbol *, bfd_vma *, bfd_size_type *,
	   asection **, bfd_size_type *));
static boolean coff_write_native_symbol
  PARAMS ((bfd *, coff_symbol_type *, bfd_vma *, bfd_size_type *,
	   asection **, bfd_size_type *));
static void coff_pointerize_aux
  PARAMS ((bfd *, combined_entry_type *, combined_entry_type *,
	   unsigned int, combined_entry_type *));
static asection *make_a_section_from_file
  PARAMS ((bfd *, struct internal_scnhdr *, unsigned int));
const bfd_target *coff_real_object_p
  PARAMS ((bfd *, unsigned, struct internal_filehdr *,
	   struct internal_aouthdr *));
static void fixup_symbol_value
  PARAMS ((bfd *, coff_symbol_type *, struct internal_syment *));
static char *build_debug_section
  PARAMS ((bfd *));
static char *copy_name
  PARAMS ((bfd *, char *, size_t));

#define STRING_SIZE_SIZE (4)

/* Take a section header read from a coff file (in HOST byte order),
   and make a BFD "section" out of it.  This is used by ECOFF.  */
static asection *
make_a_section_from_file (abfd, hdr, target_index)
     bfd *abfd;
     struct internal_scnhdr *hdr;
     unsigned int target_index;
{
  asection *return_section;
  char *name;
  boolean result = true;

  name = NULL;

  /* Handle long section names as in PE.  */
  if (bfd_coff_long_section_names (abfd)
      && hdr->s_name[0] == '/')
    {
      char buf[SCNNMLEN];
      long strindex;
      char *p;
      const char *strings;

      memcpy (buf, hdr->s_name + 1, SCNNMLEN - 1);
      buf[SCNNMLEN - 1] = '\0';
      strindex = strtol (buf, &p, 10);
      if (*p == '\0' && strindex >= 0)
	{
	  strings = _bfd_coff_read_string_table (abfd);
	  if (strings == NULL)
	    return false;
	  /* FIXME: For extra safety, we should make sure that
             strindex does not run us past the end, but right now we
             don't know the length of the string table.  */
	  strings += strindex;
	  name = bfd_alloc (abfd, (bfd_size_type) strlen (strings) + 1);
	  if (name == NULL)
	    return false;
	  strcpy (name, strings);
	}
    }

  if (name == NULL)
    {
      /* Assorted wastage to null-terminate the name, thanks AT&T! */
      name = bfd_alloc (abfd, (bfd_size_type) sizeof (hdr->s_name) + 1);
      if (name == NULL)
	return NULL;
      strncpy (name, (char *) &hdr->s_name[0], sizeof (hdr->s_name));
      name[sizeof (hdr->s_name)] = 0;
    }

  return_section = bfd_make_section_anyway (abfd, name);
  if (return_section == NULL)
    return NULL;

  return_section->vma = hdr->s_vaddr;
  return_section->lma = hdr->s_paddr;
  return_section->_raw_size = hdr->s_size;
  return_section->filepos = hdr->s_scnptr;
  return_section->rel_filepos = hdr->s_relptr;
  return_section->reloc_count = hdr->s_nreloc;

  bfd_coff_set_alignment_hook (abfd, return_section, hdr);

  return_section->line_filepos = hdr->s_lnnoptr;

  return_section->lineno_count = hdr->s_nlnno;
  return_section->userdata = NULL;
  return_section->next = (asection *) NULL;
  return_section->target_index = target_index;

  if (! bfd_coff_styp_to_sec_flags_hook (abfd, hdr, name, return_section,
					 &return_section->flags))
    result = false;

  /* At least on i386-coff, the line number count for a shared library
     section must be ignored.  */
  if ((return_section->flags & SEC_COFF_SHARED_LIBRARY) != 0)
    return_section->lineno_count = 0;

  if (hdr->s_nreloc != 0)
    return_section->flags |= SEC_RELOC;
  /* FIXME: should this check 'hdr->s_size > 0' */
  if (hdr->s_scnptr != 0)
    return_section->flags |= SEC_HAS_CONTENTS;

  if (!result)
    return NULL;
  else
    return return_section;
}

/* Read in a COFF object and make it into a BFD.  This is used by
   ECOFF as well.  */

const bfd_target *
coff_real_object_p (abfd, nscns, internal_f, internal_a)
     bfd *abfd;
     unsigned nscns;
     struct internal_filehdr *internal_f;
     struct internal_aouthdr *internal_a;
{
  flagword oflags = abfd->flags;
  bfd_vma ostart = bfd_get_start_address (abfd);
  PTR tdata;
  PTR tdata_save;
  bfd_size_type readsize;	/* length of file_info */
  unsigned int scnhsz;
  char *external_sections;
#ifdef DYNAMIC_LINKING
  boolean dynamic;
#endif

  if (!(internal_f->f_flags & F_RELFLG))
    abfd->flags |= HAS_RELOC;
  if ((internal_f->f_flags & F_EXEC))
    abfd->flags |= EXEC_P;
  if (!(internal_f->f_flags & F_LNNO))
    abfd->flags |= HAS_LINENO;
  if (!(internal_f->f_flags & F_LSYMS))
    abfd->flags |= HAS_LOCALS;

  /* FIXME: How can we set D_PAGED correctly?  */
  if ((internal_f->f_flags & F_EXEC) != 0)
    abfd->flags |= D_PAGED;

  bfd_get_symcount (abfd) = internal_f->f_nsyms;
  if (internal_f->f_nsyms)
    abfd->flags |= HAS_SYMS;

  if (internal_a != (struct internal_aouthdr *) NULL)
    bfd_get_start_address (abfd) = internal_a->entry;
  else
    bfd_get_start_address (abfd) = 0;

  /* Set up the tdata area.  ECOFF uses its own routine, and overrides
     abfd->flags.  */
  tdata_save = abfd->tdata.any;
  tdata = bfd_coff_mkobject_hook (abfd, (PTR) internal_f, (PTR) internal_a);
  if (tdata == NULL)
    goto fail2;

#ifdef DYNAMIC_LINKING
  dynamic = (abfd->flags & DYNAMIC) != 0;

  if (dynamic)
    dyn_data(abfd) =
      (struct dynamic_info *) bfd_zalloc(abfd, sizeof (struct dynamic_info));
#endif

  scnhsz = bfd_coff_scnhsz (abfd);
  readsize = (bfd_size_type) nscns * scnhsz;
  external_sections = (char *) bfd_alloc (abfd, readsize);
  if (!external_sections)
    goto fail;

  if (bfd_bread ((PTR) external_sections, readsize, abfd) != readsize)
    goto fail;

  /* Set the arch/mach *before* swapping in sections; section header swapping
     may depend on arch/mach info.  */
  if (! bfd_coff_set_arch_mach_hook (abfd, (PTR) internal_f))
    goto fail;

  /* Now copy data as required; construct all asections etc */
  if (nscns != 0)
    {
      unsigned int i;
#ifdef DYNAMIC_LINKING
      asection *sec;
      const char *name;
      coff_coffsections(abfd) = 
	  (asection **)bfd_alloc(abfd, sizeof(asection *) * nscns);
#endif
      for (i = 0; i < nscns; i++)
	{
	  struct internal_scnhdr tmp;
	  bfd_coff_swap_scnhdr_in (abfd,
				   (PTR) (external_sections + i * scnhsz),
				   (PTR) & tmp);
#ifdef DYNAMIC_LINKING /* [ */
	  sec = make_a_section_from_file (abfd, &tmp, i + 1);

	  if (sec == NULL)
	    goto fail;

	  coff_coffsections(abfd)[i] = sec;

	  if (dynamic)
	    {
	      name=bfd_get_section_name(abfd, sec);

	      /* we need quick access to certain sections, so we do the name
		 lookup once, and some other housekeeping along the way. */

	      if (strcmp(name, ".dynamic") == 0)
		{
		  coff_dynamic(abfd) = sec;
		  sec->info_r = sec->reloc_count;
		  sec->info_l = sec->lineno_count;
		  sec->lineno_count = 0;
		  sec->reloc_count = 0;
		}
	      else if (strcmp(name, ".dynsym") == 0)
		{
		  coff_dynsymtab(abfd) = sec;
		  sec->info_r = sec->reloc_count;
		  sec->info_l = sec->lineno_count;
		  sec->lineno_count = 0;
		  sec->reloc_count = 0;
		}
	      else if (strcmp(name, ".dynstr") == 0)
		{
		  coff_dynstrtab(abfd) = sec;
		  sec->info_r = sec->reloc_count;
		  sec->info_l = sec->lineno_count;
		  sec->lineno_count = 0;
		  sec->reloc_count = 0;
		}
	      else if (strcmp(name, ".gnu.version_d") == 0)
		{
		  coff_dynverdef(abfd) = sec;
		  sec->info_r = sec->reloc_count;
		  sec->info_l = sec->lineno_count;
		  sec->lineno_count = 0;
		  sec->reloc_count = 0;
		}
	      else if (strcmp(name, ".gnu.version_r") == 0)
		{
		  coff_dynverref(abfd) = sec;
		  sec->info_r = sec->reloc_count;
		  sec->info_l = sec->lineno_count;
		  sec->lineno_count = 0;
		  sec->reloc_count = 0;
		}
	      else if (strcmp(name, ".gnu.version") == 0)
		{
		  coff_dynversym(abfd) = sec;
		  sec->info_r = sec->reloc_count;
		  sec->info_l = sec->lineno_count;
		  sec->lineno_count = 0;
		  sec->reloc_count = 0;
		}
	    }

#else /* ][ */
	  if (make_a_section_from_file (abfd, &tmp, i + 1) == NULL)
	    goto fail;
#endif /* ] */
	}
    }

#ifdef DYNAMIC_LINKING
  if (dynamic)
    {
      if (coff_dynamic(abfd) != NULL && coff_dynstrtab(abfd) != NULL)
	  coff_dynamic(abfd)->link_index = coff_dynstrtab(abfd)->index;

      if (coff_dynsymtab(abfd) != NULL && coff_dynstrtab(abfd) != NULL)
	  coff_dynsymtab(abfd)->link_index = coff_dynstrtab(abfd)->index;

      if (coff_dynverdef(abfd) != NULL && coff_dynversym(abfd) != NULL)
	  coff_dynverdef(abfd)->link_index = coff_dynversym(abfd)->index;
    }
#endif
  /*  make_abs_section (abfd); */

  return abfd->xvec;

 fail:
  bfd_release (abfd, tdata);
 fail2:
  abfd->tdata.any = tdata_save;
  abfd->flags = oflags;
  bfd_get_start_address (abfd) = ostart;
  return (const bfd_target *) NULL;
}

/* Turn a COFF file into a BFD, but fail with bfd_error_wrong_format if it is
   not a COFF file.  This is also used by ECOFF.  */

const bfd_target *
coff_object_p (abfd)
     bfd *abfd;
{
  bfd_size_type filhsz;
  bfd_size_type aoutsz;
  unsigned int nscns;
  PTR filehdr;
  struct internal_filehdr internal_f;
  struct internal_aouthdr internal_a;

  /* figure out how much to read */
  filhsz = bfd_coff_filhsz (abfd);
  aoutsz = bfd_coff_aoutsz (abfd);

  filehdr = bfd_alloc (abfd, filhsz);
  if (filehdr == NULL)
    return NULL;
  if (bfd_bread (filehdr, filhsz, abfd) != filhsz)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      bfd_release (abfd, filehdr);
      return NULL;
    }
  bfd_coff_swap_filehdr_in (abfd, filehdr, &internal_f);
  bfd_release (abfd, filehdr);

  /* The XCOFF format has two sizes for the f_opthdr.  SMALL_AOUTSZ
     (less than aoutsz) used in object files and AOUTSZ (equal to
     aoutsz) in executables.  The bfd_coff_swap_aouthdr_in function
     expects this header to be aoutsz bytes in length, so we use that
     value in the call to bfd_alloc below.  But we must be careful to
     only read in f_opthdr bytes in the call to bfd_bread.  We should
     also attempt to catch corrupt or non-COFF binaries with a strange
     value for f_opthdr.  */
  if (! bfd_coff_bad_format_hook (abfd, &internal_f)
      || internal_f.f_opthdr > aoutsz)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }
  nscns = internal_f.f_nscns;

  if (internal_f.f_opthdr)
    {
      PTR opthdr;

      opthdr = bfd_alloc (abfd, aoutsz);
      if (opthdr == NULL)
	return NULL;
      if (bfd_bread (opthdr, (bfd_size_type) internal_f.f_opthdr, abfd)
	  != internal_f.f_opthdr)
	{
	  bfd_release (abfd, opthdr);
	  return NULL;
	}
      bfd_coff_swap_aouthdr_in (abfd, opthdr, (PTR) &internal_a);
      bfd_release (abfd, opthdr);
    }

  return coff_real_object_p (abfd, nscns, &internal_f,
			     (internal_f.f_opthdr != 0
			      ? &internal_a
			      : (struct internal_aouthdr *) NULL));
}

/* Get the BFD section from a COFF symbol section number.  */

asection *
coff_section_from_bfd_index (abfd, index)
     bfd *abfd;
     int index;
{
#ifndef DYNAMIC_LINKING
  struct sec *answer = abfd->sections;
#endif

  if (index == N_ABS)
    return bfd_abs_section_ptr;
  if (index == N_UNDEF)
    return bfd_und_section_ptr;
  if (index == N_DEBUG)
    return bfd_abs_section_ptr;

#ifdef DYNAMIC_LINKING
  /* range check; special negatives were eliminated above */
  if (index < 0 || index > (int)abfd->section_count)
    return bfd_und_section_ptr;

  return coff_coffsections(abfd)[index-1];
#else
  while (answer)
    {
      if (answer->target_index == index)
	return answer;
      answer = answer->next;
    }

  /* We should not reach this point, but the SCO 3.2v4 /lib/libc_s.a
     has a bad symbol table in biglitpow.o.  */
  return bfd_und_section_ptr;
#endif
}

/* Get the upper bound of a COFF symbol table.  */

long
coff_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  if (!bfd_coff_slurp_symbol_table (abfd))
    return -1;

  return (bfd_get_symcount (abfd) + 1) * (sizeof (coff_symbol_type *));
}

/* Canonicalize a COFF symbol table.  */

long
coff_get_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  unsigned int counter;
  coff_symbol_type *symbase;
  coff_symbol_type **location = (coff_symbol_type **) alocation;

  if (!bfd_coff_slurp_symbol_table (abfd))
    return -1;

  symbase = obj_symbols (abfd);
  counter = bfd_get_symcount (abfd);
  while (counter-- > 0)
    *location++ = symbase++;

  *location = NULL;

  return bfd_get_symcount (abfd);
}

/* Get the name of a symbol.  The caller must pass in a buffer of size
   >= SYMNMLEN + 1.  */

const char *
_bfd_coff_internal_syment_name (abfd, sym, buf)
     bfd *abfd;
     const struct internal_syment *sym;
     char *buf;
{
  /* FIXME: It's not clear this will work correctly if sizeof
     (_n_zeroes) != 4.  */
  if (sym->_n._n_n._n_zeroes != 0
      || sym->_n._n_n._n_offset == 0)
    {
      memcpy (buf, sym->_n._n_name, SYMNMLEN);
      buf[SYMNMLEN] = '\0';
      return buf;
    }
  else
    {
      const char *strings;

      BFD_ASSERT (sym->_n._n_n._n_offset >= STRING_SIZE_SIZE);
      strings = obj_coff_strings (abfd);
      if (strings == NULL)
	{
	  strings = _bfd_coff_read_string_table (abfd);
	  if (strings == NULL)
	    return NULL;
	}
      return strings + sym->_n._n_n._n_offset;
    }
}

/* Read in and swap the relocs.  This returns a buffer holding the
   relocs for section SEC in file ABFD.  If CACHE is true and
   INTERNAL_RELOCS is NULL, the relocs read in will be saved in case
   the function is called again.  If EXTERNAL_RELOCS is not NULL, it
   is a buffer large enough to hold the unswapped relocs.  If
   INTERNAL_RELOCS is not NULL, it is a buffer large enough to hold
   the swapped relocs.  If REQUIRE_INTERNAL is true, then the return
   value must be INTERNAL_RELOCS.  The function returns NULL on error.  */

struct internal_reloc *
_bfd_coff_read_internal_relocs (abfd, sec, cache, external_relocs,
				require_internal, internal_relocs)
     bfd *abfd;
     asection *sec;
     boolean cache;
     bfd_byte *external_relocs;
     boolean require_internal;
     struct internal_reloc *internal_relocs;
{
  bfd_size_type relsz;
  bfd_byte *free_external = NULL;
  struct internal_reloc *free_internal = NULL;
  bfd_byte *erel;
  bfd_byte *erel_end;
  struct internal_reloc *irel;
  bfd_size_type amt;

  if (coff_section_data (abfd, sec) != NULL
      && coff_section_data (abfd, sec)->relocs != NULL)
    {
      if (! require_internal)
	return coff_section_data (abfd, sec)->relocs;
      memcpy (internal_relocs, coff_section_data (abfd, sec)->relocs,
	      sec->reloc_count * sizeof (struct internal_reloc));
      return internal_relocs;
    }

  relsz = bfd_coff_relsz (abfd);

  amt = sec->reloc_count * relsz;
  if (external_relocs == NULL)
    {
      free_external = (bfd_byte *) bfd_malloc (amt);
      if (free_external == NULL && sec->reloc_count > 0)
	goto error_return;
      external_relocs = free_external;
    }

  if (bfd_seek (abfd, sec->rel_filepos, SEEK_SET) != 0
      || bfd_bread (external_relocs, amt, abfd) != amt)
    goto error_return;

  if (internal_relocs == NULL)
    {
      amt = sec->reloc_count;
      amt *= sizeof (struct internal_reloc);
      free_internal = (struct internal_reloc *) bfd_malloc (amt);
      if (free_internal == NULL && sec->reloc_count > 0)
	goto error_return;
      internal_relocs = free_internal;
    }

  /* Swap in the relocs.  */
  erel = external_relocs;
  erel_end = erel + relsz * sec->reloc_count;
  irel = internal_relocs;
  for (; erel < erel_end; erel += relsz, irel++)
    bfd_coff_swap_reloc_in (abfd, (PTR) erel, (PTR) irel);

  if (free_external != NULL)
    {
      free (free_external);
      free_external = NULL;
    }

  if (cache && free_internal != NULL)
    {
      if (coff_section_data (abfd, sec) == NULL)
	{
	  amt = sizeof (struct coff_section_tdata);
	  sec->used_by_bfd = (PTR) bfd_zalloc (abfd, amt);
	  if (sec->used_by_bfd == NULL)
	    goto error_return;
	  coff_section_data (abfd, sec)->contents = NULL;
	}
      coff_section_data (abfd, sec)->relocs = free_internal;
    }

  return internal_relocs;

 error_return:
  if (free_external != NULL)
    free (free_external);
  if (free_internal != NULL)
    free (free_internal);
  return NULL;
}

/* Set lineno_count for the output sections of a COFF file.  */

int
coff_count_linenumbers (abfd)
     bfd *abfd;
{
  unsigned int limit = bfd_get_symcount (abfd);
  unsigned int i;
  int total = 0;
  asymbol **p;
  asection *s;

  if (limit == 0)
    {
      /* This may be from the backend linker, in which case the
         lineno_count in the sections is correct.  */
      for (s = abfd->sections; s != NULL; s = s->next)
	total += s->lineno_count;
      return total;
    }

  for (s = abfd->sections; s != NULL; s = s->next)
    BFD_ASSERT (s->lineno_count == 0);

  for (p = abfd->outsymbols, i = 0; i < limit; i++, p++)
    {
      asymbol *q_maybe = *p;

      if (bfd_family_coff (bfd_asymbol_bfd (q_maybe)))
	{
	  coff_symbol_type *q = coffsymbol (q_maybe);

	  /* The AIX 4.1 compiler can sometimes generate line numbers
             attached to debugging symbols.  We try to simply ignore
             those here.  */
	  if (q->lineno != NULL
	      && q->symbol.section->owner != NULL)
	    {
	      /* This symbol has line numbers.  Increment the owning
	         section's linenumber count.  */
	      alent *l = q->lineno;

	      do
		{
		  asection * sec = q->symbol.section->output_section;
		  
		  /* Do not try to update fields in read-only sections.  */
		  if (! bfd_is_const_section (sec))
		    sec->lineno_count ++;

		  ++total;
		  ++l;
		}
	      while (l->line_number != 0);
	    }
	}
    }

  return total;
}

/* Takes a bfd and a symbol, returns a pointer to the coff specific
   area of the symbol if there is one.  */

coff_symbol_type *
coff_symbol_from (ignore_abfd, symbol)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     asymbol *symbol;
{
  if (!bfd_family_coff (bfd_asymbol_bfd (symbol)))
    return (coff_symbol_type *) NULL;

  if (bfd_asymbol_bfd (symbol)->tdata.coff_obj_data == (coff_data_type *) NULL)
    return (coff_symbol_type *) NULL;

  return (coff_symbol_type *) symbol;
}

static void
fixup_symbol_value (abfd, coff_symbol_ptr, syment)
     bfd *abfd ATTRIBUTE_UNUSED;
     coff_symbol_type *coff_symbol_ptr;
     struct internal_syment *syment;
{

  /* Normalize the symbol flags */
  if (bfd_is_com_section (coff_symbol_ptr->symbol.section))
    {
      /* a common symbol is undefined with a value */
      syment->n_scnum = N_UNDEF;
      syment->n_value = coff_symbol_ptr->symbol.value;
    }
  else if ((coff_symbol_ptr->symbol.flags & BSF_DEBUGGING) != 0
	   && (coff_symbol_ptr->symbol.flags & BSF_DEBUGGING_RELOC) == 0)
    {
      syment->n_value = coff_symbol_ptr->symbol.value;
    }
  else if (bfd_is_und_section (coff_symbol_ptr->symbol.section))
    {
      syment->n_scnum = N_UNDEF;
      syment->n_value = 0;
    }
  /* FIXME: Do we need to handle the absolute section here?  */
  else
    {
      if (coff_symbol_ptr->symbol.section)
	{
	  syment->n_scnum =
	    coff_symbol_ptr->symbol.section->output_section->target_index;

	  syment->n_value = (coff_symbol_ptr->symbol.value
			     + coff_symbol_ptr->symbol.section->output_offset);

	  syment->n_value += (syment->n_sclass == C_STATLAB)
	    ? coff_symbol_ptr->symbol.section->output_section->lma
	    : coff_symbol_ptr->symbol.section->output_section->vma;
	}
      else
	{
	  BFD_ASSERT (0);
	  /* This can happen, but I don't know why yet (steve@cygnus.com) */
	  syment->n_scnum = N_ABS;
	  syment->n_value = coff_symbol_ptr->symbol.value;
	}
    }
}

/* Run through all the symbols in the symbol table and work out what
   their indexes into the symbol table will be when output.

   Coff requires that each C_FILE symbol points to the next one in the
   chain, and that the last one points to the first external symbol. We
   do that here too.  */

boolean
coff_renumber_symbols (bfd_ptr, first_undef)
     bfd *bfd_ptr;
     int *first_undef;
{
  unsigned int symbol_count = bfd_get_symcount (bfd_ptr);
  asymbol **symbol_ptr_ptr = bfd_ptr->outsymbols;
  unsigned int native_index = 0;
  struct internal_syment *last_file = (struct internal_syment *) NULL;
  unsigned int symbol_index;

  /* COFF demands that undefined symbols come after all other symbols.
     Since we don't need to impose this extra knowledge on all our
     client programs, deal with that here.  Sort the symbol table;
     just move the undefined symbols to the end, leaving the rest
     alone.  The O'Reilly book says that defined global symbols come
     at the end before the undefined symbols, so we do that here as
     well.  */
  /* @@ Do we have some condition we could test for, so we don't always
     have to do this?  I don't think relocatability is quite right, but
     I'm not certain.  [raeburn:19920508.1711EST]  */
  {
    asymbol **newsyms;
    unsigned int i;
    bfd_size_type amt;

    amt = sizeof (asymbol *) * ((bfd_size_type) symbol_count + 1);
    newsyms = (asymbol **) bfd_alloc (bfd_ptr, amt);
    if (!newsyms)
      return false;
    bfd_ptr->outsymbols = newsyms;
    for (i = 0; i < symbol_count; i++)
      if ((symbol_ptr_ptr[i]->flags & BSF_NOT_AT_END) != 0
	  || (!bfd_is_und_section (symbol_ptr_ptr[i]->section)
	      && !bfd_is_com_section (symbol_ptr_ptr[i]->section)
	      && ((symbol_ptr_ptr[i]->flags & BSF_FUNCTION) != 0
		  || ((symbol_ptr_ptr[i]->flags & (BSF_GLOBAL | BSF_WEAK))
		      == 0))))
	*newsyms++ = symbol_ptr_ptr[i];

    for (i = 0; i < symbol_count; i++)
      if ((symbol_ptr_ptr[i]->flags & BSF_NOT_AT_END) == 0
	  && !bfd_is_und_section (symbol_ptr_ptr[i]->section)
	  && (bfd_is_com_section (symbol_ptr_ptr[i]->section)
	      || ((symbol_ptr_ptr[i]->flags & BSF_FUNCTION) == 0
		  && ((symbol_ptr_ptr[i]->flags & (BSF_GLOBAL | BSF_WEAK))
		      != 0))))
	*newsyms++ = symbol_ptr_ptr[i];

    *first_undef = newsyms - bfd_ptr->outsymbols;

    for (i = 0; i < symbol_count; i++)
      if ((symbol_ptr_ptr[i]->flags & BSF_NOT_AT_END) == 0
	  && bfd_is_und_section (symbol_ptr_ptr[i]->section))
	*newsyms++ = symbol_ptr_ptr[i];
    *newsyms = (asymbol *) NULL;
    symbol_ptr_ptr = bfd_ptr->outsymbols;
  }

  for (symbol_index = 0; symbol_index < symbol_count; symbol_index++)
    {
      coff_symbol_type *coff_symbol_ptr = coff_symbol_from (bfd_ptr, symbol_ptr_ptr[symbol_index]);
      symbol_ptr_ptr[symbol_index]->udata.i = symbol_index;
      if (coff_symbol_ptr && coff_symbol_ptr->native)
	{
	  combined_entry_type *s = coff_symbol_ptr->native;
	  int i;

	  if (s->u.syment.n_sclass == C_FILE)
	    {
	      if (last_file != (struct internal_syment *) NULL)
		last_file->n_value = native_index;
	      last_file = &(s->u.syment);
	    }
	  else
	    {

	      /* Modify the symbol values according to their section and
	         type */

	      fixup_symbol_value (bfd_ptr, coff_symbol_ptr, &(s->u.syment));
	    }
	  for (i = 0; i < s->u.syment.n_numaux + 1; i++)
	    s[i].offset = native_index++;
	}
      else
	{
	  native_index++;
	}
    }
  obj_conv_table_size (bfd_ptr) = native_index;

  return true;
}

/* Run thorough the symbol table again, and fix it so that all
   pointers to entries are changed to the entries' index in the output
   symbol table.  */

void
coff_mangle_symbols (bfd_ptr)
     bfd *bfd_ptr;
{
  unsigned int symbol_count = bfd_get_symcount (bfd_ptr);
  asymbol **symbol_ptr_ptr = bfd_ptr->outsymbols;
  unsigned int symbol_index;

  for (symbol_index = 0; symbol_index < symbol_count; symbol_index++)
    {
      coff_symbol_type *coff_symbol_ptr =
      coff_symbol_from (bfd_ptr, symbol_ptr_ptr[symbol_index]);

      if (coff_symbol_ptr && coff_symbol_ptr->native)
	{
	  int i;
	  combined_entry_type *s = coff_symbol_ptr->native;

	  if (s->fix_value)
	    {
	      /* FIXME: We should use a union here.  */
	      s->u.syment.n_value =
		(bfd_vma)((combined_entry_type *)
			  ((unsigned long) s->u.syment.n_value))->offset;
	      s->fix_value = 0;
	    }
	  if (s->fix_line)
	    {
	      /* The value is the offset into the line number entries
                 for the symbol's section.  On output, the symbol's
                 section should be N_DEBUG.  */
	      s->u.syment.n_value =
		(coff_symbol_ptr->symbol.section->output_section->line_filepos
		 + s->u.syment.n_value * bfd_coff_linesz (bfd_ptr));
	      coff_symbol_ptr->symbol.section =
		coff_section_from_bfd_index (bfd_ptr, N_DEBUG);
	      BFD_ASSERT (coff_symbol_ptr->symbol.flags & BSF_DEBUGGING);
	    }
	  for (i = 0; i < s->u.syment.n_numaux; i++)
	    {
	      combined_entry_type *a = s + i + 1;
	      if (a->fix_tag)
		{
		  a->u.auxent.x_sym.x_tagndx.l =
		    a->u.auxent.x_sym.x_tagndx.p->offset;
		  a->fix_tag = 0;
		}
	      if (a->fix_end)
		{
		  a->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.l =
		    a->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.p->offset;
		  a->fix_end = 0;
		}
	      if (a->fix_scnlen)
		{
		  a->u.auxent.x_csect.x_scnlen.l =
		    a->u.auxent.x_csect.x_scnlen.p->offset;
		  a->fix_scnlen = 0;
		}
	    }
	}
    }
}

static void
coff_fix_symbol_name (abfd, symbol, native, string_size_p,
		      debug_string_section_p, debug_string_size_p)
     bfd *abfd;
     asymbol *symbol;
     combined_entry_type *native;
     bfd_size_type *string_size_p;
     asection **debug_string_section_p;
     bfd_size_type *debug_string_size_p;
{
  unsigned int name_length;
  union internal_auxent *auxent;
  char *name = (char *) (symbol->name);

  if (name == (char *) NULL)
    {
      /* coff symbols always have names, so we'll make one up */
      symbol->name = "strange";
      name = (char *) symbol->name;
    }
  name_length = strlen (name);

  if (native->u.syment.n_sclass == C_FILE
      && native->u.syment.n_numaux > 0)
    {
      unsigned int filnmlen;

      if (bfd_coff_force_symnames_in_strings (abfd))
	{
          native->u.syment._n._n_n._n_offset =
	      (*string_size_p + STRING_SIZE_SIZE);
	  native->u.syment._n._n_n._n_zeroes = 0;
	  *string_size_p += 6;  /* strlen(".file") + 1 */
	}
      else
  	strncpy (native->u.syment._n._n_name, ".file", SYMNMLEN);

      auxent = &(native + 1)->u.auxent;

      filnmlen = bfd_coff_filnmlen (abfd);

      if (bfd_coff_long_filenames (abfd))
	{
	  if (name_length <= filnmlen)
	    {
	      strncpy (auxent->x_file.x_fname, name, filnmlen);
	    }
	  else
	    {
	      auxent->x_file.x_n.x_offset = *string_size_p + STRING_SIZE_SIZE;
	      auxent->x_file.x_n.x_zeroes = 0;
	      *string_size_p += name_length + 1;
	    }
	}
      else
	{
	  strncpy (auxent->x_file.x_fname, name, filnmlen);
	  if (name_length > filnmlen)
	    name[filnmlen] = '\0';
	}
    }
  else
    {
      if (name_length <= SYMNMLEN && !bfd_coff_force_symnames_in_strings (abfd))
	{
	  /* This name will fit into the symbol neatly */
	  strncpy (native->u.syment._n._n_name, symbol->name, SYMNMLEN);
	}
      else if (!bfd_coff_symname_in_debug (abfd, &native->u.syment))
	{
	  native->u.syment._n._n_n._n_offset = (*string_size_p
						+ STRING_SIZE_SIZE);
	  native->u.syment._n._n_n._n_zeroes = 0;
	  *string_size_p += name_length + 1;
	}
      else
	{
	  file_ptr filepos;
	  bfd_byte buf[4];
	  int prefix_len = bfd_coff_debug_string_prefix_length (abfd);

	  /* This name should be written into the .debug section.  For
	     some reason each name is preceded by a two byte length
	     and also followed by a null byte.  FIXME: We assume that
	     the .debug section has already been created, and that it
	     is large enough.  */
	  if (*debug_string_section_p == (asection *) NULL)
	    *debug_string_section_p = bfd_get_section_by_name (abfd, ".debug");
	  filepos = bfd_tell (abfd);
	  if (prefix_len == 4)
	    bfd_put_32 (abfd, (bfd_vma) (name_length + 1), buf);
	  else
	    bfd_put_16 (abfd, (bfd_vma) (name_length + 1), buf);

	  if (!bfd_set_section_contents (abfd,
					 *debug_string_section_p,
					 (PTR) buf,
					 (file_ptr) *debug_string_size_p,
					 (bfd_size_type) prefix_len)
	      || !bfd_set_section_contents (abfd,
					    *debug_string_section_p,
					    (PTR) symbol->name,
					    (file_ptr) (*debug_string_size_p
							+ prefix_len),
					    (bfd_size_type) name_length + 1))
	    abort ();
	  if (bfd_seek (abfd, filepos, SEEK_SET) != 0)
	    abort ();
	  native->u.syment._n._n_n._n_offset =
	      *debug_string_size_p + prefix_len;
	  native->u.syment._n._n_n._n_zeroes = 0;
	  *debug_string_size_p += name_length + 1 + prefix_len;
	}
    }
}

/* We need to keep track of the symbol index so that when we write out
   the relocs we can get the index for a symbol.  This method is a
   hack.  FIXME.  */

#define set_index(symbol, idx)	((symbol)->udata.i = (idx))

/* Write a symbol out to a COFF file.  */

static boolean
coff_write_symbol (abfd, symbol, native, written, string_size_p,
		   debug_string_section_p, debug_string_size_p)
     bfd *abfd;
     asymbol *symbol;
     combined_entry_type *native;
     bfd_vma *written;
     bfd_size_type *string_size_p;
     asection **debug_string_section_p;
     bfd_size_type *debug_string_size_p;
{
  unsigned int numaux = native->u.syment.n_numaux;
  int type = native->u.syment.n_type;
  int class = native->u.syment.n_sclass;
  PTR buf;
  bfd_size_type symesz;

  if (native->u.syment.n_sclass == C_FILE)
    symbol->flags |= BSF_DEBUGGING;

  if (symbol->flags & BSF_DEBUGGING
      && bfd_is_abs_section (symbol->section))
    {
      native->u.syment.n_scnum = N_DEBUG;
    }
  else if (bfd_is_abs_section (symbol->section))
    {
      native->u.syment.n_scnum = N_ABS;
    }
  else if (bfd_is_und_section (symbol->section))
    {
      native->u.syment.n_scnum = N_UNDEF;
    }
  else
    {
      native->u.syment.n_scnum =
	symbol->section->output_section->target_index;
    }

  coff_fix_symbol_name (abfd, symbol, native, string_size_p,
			debug_string_section_p, debug_string_size_p);

  symesz = bfd_coff_symesz (abfd);
  buf = bfd_alloc (abfd, symesz);
  if (!buf)
    return false;
  bfd_coff_swap_sym_out (abfd, &native->u.syment, buf);
  if (bfd_bwrite (buf, symesz, abfd) != symesz)
    return false;
  bfd_release (abfd, buf);

  if (native->u.syment.n_numaux > 0)
    {
      bfd_size_type auxesz;
      unsigned int j;

      auxesz = bfd_coff_auxesz (abfd);
      buf = bfd_alloc (abfd, auxesz);
      if (!buf)
	return false;
      for (j = 0; j < native->u.syment.n_numaux; j++)
	{
	  bfd_coff_swap_aux_out (abfd,
				 &((native + j + 1)->u.auxent),
				 type,
				 class,
				 (int) j,
				 native->u.syment.n_numaux,
				 buf);
	  if (bfd_bwrite (buf, auxesz, abfd) != auxesz)
	    return false;
	}
      bfd_release (abfd, buf);
    }

  /* Store the index for use when we write out the relocs.  */
  set_index (symbol, *written);

  *written += numaux + 1;
  return true;
}

/* Write out a symbol to a COFF file that does not come from a COFF
   file originally.  This symbol may have been created by the linker,
   or we may be linking a non COFF file to a COFF file.  */

static boolean
coff_write_alien_symbol (abfd, symbol, written, string_size_p,
			 debug_string_section_p, debug_string_size_p)
     bfd *abfd;
     asymbol *symbol;
     bfd_vma *written;
     bfd_size_type *string_size_p;
     asection **debug_string_section_p;
     bfd_size_type *debug_string_size_p;
{
  combined_entry_type *native;
  combined_entry_type dummy;

  native = &dummy;
  native->u.syment.n_type = T_NULL;
  native->u.syment.n_flags = 0;
  if (bfd_is_und_section (symbol->section))
    {
      native->u.syment.n_scnum = N_UNDEF;
      native->u.syment.n_value = symbol->value;
    }
  else if (bfd_is_com_section (symbol->section))
    {
      native->u.syment.n_scnum = N_UNDEF;
      native->u.syment.n_value = symbol->value;
    }
  else if (symbol->flags & BSF_DEBUGGING)
    {
      /* There isn't much point to writing out a debugging symbol
         unless we are prepared to convert it into COFF debugging
         format.  So, we just ignore them.  We must clobber the symbol
         name to keep it from being put in the string table.  */
      symbol->name = "";
      return true;
    }
  else
    {
      native->u.syment.n_scnum =
	symbol->section->output_section->target_index;
      native->u.syment.n_value = (symbol->value
	                          + symbol->section->output_section->vma
				  + symbol->section->output_offset);

      /* Copy the any flags from the file header into the symbol.
         FIXME: Why?  */
      {
	coff_symbol_type *c = coff_symbol_from (abfd, symbol);
	if (c != (coff_symbol_type *) NULL)
	  native->u.syment.n_flags = bfd_asymbol_bfd (&c->symbol)->flags;
      }
    }

  native->u.syment.n_type = 0;
  if (symbol->flags & BSF_LOCAL)
    native->u.syment.n_sclass = C_STAT;
  else if (symbol->flags & BSF_WEAK)
    native->u.syment.n_sclass = C_WEAKEXT;
  else
    native->u.syment.n_sclass = C_EXT;
  native->u.syment.n_numaux = 0;

  return coff_write_symbol (abfd, symbol, native, written, string_size_p,
			    debug_string_section_p, debug_string_size_p);
}

/* Write a native symbol to a COFF file.  */

static boolean
coff_write_native_symbol (abfd, symbol, written, string_size_p,
			  debug_string_section_p, debug_string_size_p)
     bfd *abfd;
     coff_symbol_type *symbol;
     bfd_vma *written;
     bfd_size_type *string_size_p;
     asection **debug_string_section_p;
     bfd_size_type *debug_string_size_p;
{
  combined_entry_type *native = symbol->native;
  alent *lineno = symbol->lineno;

  /* If this symbol has an associated line number, we must store the
     symbol index in the line number field.  We also tag the auxent to
     point to the right place in the lineno table.  */
  if (lineno && !symbol->done_lineno && symbol->symbol.section->owner != NULL)
    {
      unsigned int count = 0;
      lineno[count].u.offset = *written;
      if (native->u.syment.n_numaux)
	{
	  union internal_auxent *a = &((native + 1)->u.auxent);

	  a->x_sym.x_fcnary.x_fcn.x_lnnoptr =
	    symbol->symbol.section->output_section->moving_line_filepos;
	}

      /* Count and relocate all other linenumbers.  */
      count++;
      while (lineno[count].line_number != 0)
	{
#if 0
	  /* 13 april 92. sac
	     I've been told this, but still need proof:
	     > The second bug is also in `bfd/coffcode.h'.  This bug
	     > causes the linker to screw up the pc-relocations for
	     > all the line numbers in COFF code.  This bug isn't only
	     > specific to A29K implementations, but affects all
	     > systems using COFF format binaries.  Note that in COFF
	     > object files, the line number core offsets output by
	     > the assembler are relative to the start of each
	     > procedure, not to the start of the .text section.  This
	     > patch relocates the line numbers relative to the
	     > `native->u.syment.n_value' instead of the section
	     > virtual address.
	     > modular!olson@cs.arizona.edu (Jon Olson)
	   */
	  lineno[count].u.offset += native->u.syment.n_value;
#else
	  lineno[count].u.offset +=
	    (symbol->symbol.section->output_section->vma
	     + symbol->symbol.section->output_offset);
#endif
	  count++;
	}
      symbol->done_lineno = true;

      if (! bfd_is_const_section (symbol->symbol.section->output_section))
	symbol->symbol.section->output_section->moving_line_filepos +=
	  count * bfd_coff_linesz (abfd);
    }

  return coff_write_symbol (abfd, &(symbol->symbol), native, written,
			    string_size_p, debug_string_section_p,
			    debug_string_size_p);
}

/* Write out the COFF symbols.  */

boolean
coff_write_symbols (abfd)
     bfd *abfd;
{
  bfd_size_type string_size;
  asection *debug_string_section;
  bfd_size_type debug_string_size;
  unsigned int i;
  unsigned int limit = bfd_get_symcount (abfd);
  bfd_signed_vma written = 0;
  asymbol **p;

  string_size = 0;
  debug_string_section = NULL;
  debug_string_size = 0;

  /* If this target supports long section names, they must be put into
     the string table.  This is supported by PE.  This code must
     handle section names just as they are handled in
     coff_write_object_contents.  */
  if (bfd_coff_long_section_names (abfd))
    {
      asection *o;

      for (o = abfd->sections; o != NULL; o = o->next)
	{
	  size_t len;

	  len = strlen (o->name);
	  if (len > SCNNMLEN)
	    string_size += len + 1;
	}
    }

  /* Seek to the right place */
  if (bfd_seek (abfd, obj_sym_filepos (abfd), SEEK_SET) != 0)
    return false;

  /* Output all the symbols we have */

  written = 0;
  for (p = abfd->outsymbols, i = 0; i < limit; i++, p++)
    {
      asymbol *symbol = *p;
      coff_symbol_type *c_symbol = coff_symbol_from (abfd, symbol);

      if (c_symbol == (coff_symbol_type *) NULL
	  || c_symbol->native == (combined_entry_type *) NULL)
	{
	  if (!coff_write_alien_symbol (abfd, symbol, &written, &string_size,
					&debug_string_section,
					&debug_string_size))
	    return false;
	}
      else
	{
	  if (!coff_write_native_symbol (abfd, c_symbol, &written,
					 &string_size, &debug_string_section,
					 &debug_string_size))
	    return false;
	}
    }

  obj_raw_syment_count (abfd) = written;

  /* Now write out strings */

  if (string_size != 0)
    {
      unsigned int size = string_size + STRING_SIZE_SIZE;
      bfd_byte buffer[STRING_SIZE_SIZE];

#if STRING_SIZE_SIZE == 4
      H_PUT_32 (abfd, size, buffer);
#else
 #error Change H_PUT_32
#endif
      if (bfd_bwrite ((PTR) buffer, (bfd_size_type) sizeof (buffer), abfd)
	  != sizeof (buffer))
	return false;

      /* Handle long section names.  This code must handle section
	 names just as they are handled in coff_write_object_contents.  */
      if (bfd_coff_long_section_names (abfd))
	{
	  asection *o;

	  for (o = abfd->sections; o != NULL; o = o->next)
	    {
	      size_t len;

	      len = strlen (o->name);
	      if (len > SCNNMLEN)
		{
		  if (bfd_bwrite (o->name, (bfd_size_type) (len + 1), abfd)
		      != len + 1)
		    return false;
		}
	    }
	}

      for (p = abfd->outsymbols, i = 0;
	   i < limit;
	   i++, p++)
	{
	  asymbol *q = *p;
	  size_t name_length = strlen (q->name);
	  coff_symbol_type *c_symbol = coff_symbol_from (abfd, q);
	  size_t maxlen;

	  /* Figure out whether the symbol name should go in the string
	     table.  Symbol names that are short enough are stored
	     directly in the syment structure.  File names permit a
	     different, longer, length in the syment structure.  On
	     XCOFF, some symbol names are stored in the .debug section
	     rather than in the string table.  */

	  if (c_symbol == NULL
	      || c_symbol->native == NULL)
	    {
	      /* This is not a COFF symbol, so it certainly is not a
	         file name, nor does it go in the .debug section.  */
	      maxlen = bfd_coff_force_symnames_in_strings (abfd) ? 0 : SYMNMLEN;
	    }
	  else if (bfd_coff_symname_in_debug (abfd,
					      &c_symbol->native->u.syment))
	    {
	      /* This symbol name is in the XCOFF .debug section.
	         Don't write it into the string table.  */
	      maxlen = name_length;
	    }
	  else if (c_symbol->native->u.syment.n_sclass == C_FILE
		   && c_symbol->native->u.syment.n_numaux > 0)
	    {
	      if (bfd_coff_force_symnames_in_strings (abfd))
		{
		  if (bfd_bwrite (".file", (bfd_size_type) 6, abfd) != 6)
		    return false;
		}
	      maxlen = bfd_coff_filnmlen (abfd);
	    }
	  else
	    maxlen = bfd_coff_force_symnames_in_strings (abfd) ? 0 : SYMNMLEN;

	  if (name_length > maxlen)
	    {
	      if (bfd_bwrite ((PTR) (q->name), (bfd_size_type) name_length + 1,
			     abfd) != name_length + 1)
		return false;
	    }
	}
    }
  else
    {
      /* We would normally not write anything here, but we'll write
         out 4 so that any stupid coff reader which tries to read the
         string table even when there isn't one won't croak.  */
      unsigned int size = STRING_SIZE_SIZE;
      bfd_byte buffer[STRING_SIZE_SIZE];

#if STRING_SIZE_SIZE == 4
      H_PUT_32 (abfd, size, buffer);
#else
 #error Change H_PUT_32
#endif
      if (bfd_bwrite ((PTR) buffer, (bfd_size_type) STRING_SIZE_SIZE, abfd)
	  != STRING_SIZE_SIZE)
	return false;
    }

  /* Make sure the .debug section was created to be the correct size.
     We should create it ourselves on the fly, but we don't because
     BFD won't let us write to any section until we know how large all
     the sections are.  We could still do it by making another pass
     over the symbols.  FIXME.  */
  BFD_ASSERT (debug_string_size == 0
	      || (debug_string_section != (asection *) NULL
		  && (BFD_ALIGN (debug_string_size,
				 1 << debug_string_section->alignment_power)
		      == bfd_section_size (abfd, debug_string_section))));

  return true;
}

boolean
coff_write_linenumbers (abfd)
     bfd *abfd;
{
  asection *s;
  bfd_size_type linesz;
  PTR buff;

  linesz = bfd_coff_linesz (abfd);
  buff = bfd_alloc (abfd, linesz);
  if (!buff)
    return false;
  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    {
      if (s->lineno_count)
	{
	  asymbol **q = abfd->outsymbols;
	  if (bfd_seek (abfd, s->line_filepos, SEEK_SET) != 0)
	    return false;
	  /* Find all the linenumbers in this section */
	  while (*q)
	    {
	      asymbol *p = *q;
	      if (p->section->output_section == s)
		{
		  alent *l =
		  BFD_SEND (bfd_asymbol_bfd (p), _get_lineno,
			    (bfd_asymbol_bfd (p), p));
		  if (l)
		    {
		      /* Found a linenumber entry, output */
		      struct internal_lineno out;
		      memset ((PTR) & out, 0, sizeof (out));
		      out.l_lnno = 0;
		      out.l_addr.l_symndx = l->u.offset;
		      bfd_coff_swap_lineno_out (abfd, &out, buff);
		      if (bfd_bwrite (buff, (bfd_size_type) linesz, abfd)
			  != linesz)
			return false;
		      l++;
		      while (l->line_number)
			{
			  out.l_lnno = l->line_number;
			  out.l_addr.l_symndx = l->u.offset;
			  bfd_coff_swap_lineno_out (abfd, &out, buff);
			  if (bfd_bwrite (buff, (bfd_size_type) linesz, abfd)
			      != linesz)
			    return false;
			  l++;
			}
		    }
		}
	      q++;
	    }
	}
    }
  bfd_release (abfd, buff);
  return true;
}

alent *
coff_get_lineno (ignore_abfd, symbol)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     asymbol *symbol;
{
  return coffsymbol (symbol)->lineno;
}

#if 0

/* This is only called from coff_add_missing_symbols, which has been
   disabled.  */

asymbol *
coff_section_symbol (abfd, name)
     bfd *abfd;
     char *name;
{
  asection *sec = bfd_make_section_old_way (abfd, name);
  asymbol *sym;
  combined_entry_type *csym;

  sym = sec->symbol;
  csym = coff_symbol_from (abfd, sym)->native;
  /* Make sure back-end COFF stuff is there.  */
  if (csym == 0)
    {
      struct foo
	{
	  coff_symbol_type sym;
	  /* @@FIXME This shouldn't use a fixed size!!  */
	  combined_entry_type e[10];
	};
      struct foo *f;

      f = (struct foo *) bfd_zalloc (abfd, (bfd_size_type) sizeof (*f));
      if (!f)
	{
	  bfd_set_error (bfd_error_no_error);
	  return NULL;
	}
      coff_symbol_from (abfd, sym)->native = csym = f->e;
    }
  csym[0].u.syment.n_sclass = C_STAT;
  csym[0].u.syment.n_numaux = 1;
/*  SF_SET_STATICS (sym);       @@ ??? */
  csym[1].u.auxent.x_scn.x_scnlen = sec->_raw_size;
  csym[1].u.auxent.x_scn.x_nreloc = sec->reloc_count;
  csym[1].u.auxent.x_scn.x_nlinno = sec->lineno_count;

  if (sec->output_section == NULL)
    {
      sec->output_section = sec;
      sec->output_offset = 0;
    }

  return sym;
}

#endif /* 0 */

/* This function transforms the offsets into the symbol table into
   pointers to syments.  */

static void
coff_pointerize_aux (abfd, table_base, symbol, indaux, auxent)
     bfd *abfd;
     combined_entry_type *table_base;
     combined_entry_type *symbol;
     unsigned int indaux;
     combined_entry_type *auxent;
{
  unsigned int type = symbol->u.syment.n_type;
  unsigned int class = symbol->u.syment.n_sclass;

  if (coff_backend_info (abfd)->_bfd_coff_pointerize_aux_hook)
    {
      if ((*coff_backend_info (abfd)->_bfd_coff_pointerize_aux_hook)
	  (abfd, table_base, symbol, indaux, auxent))
	return;
    }

  /* Don't bother if this is a file or a section */
  if (class == C_STAT && type == T_NULL)
    return;
  if (class == C_FILE)
    return;

  /* Otherwise patch up */
#define N_TMASK coff_data (abfd)->local_n_tmask
#define N_BTSHFT coff_data (abfd)->local_n_btshft
  if ((ISFCN (type) || ISTAG (class) || class == C_BLOCK || class == C_FCN)
      && auxent->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.l > 0)
    {
      auxent->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.p =
	table_base + auxent->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.l;
      auxent->fix_end = 1;
    }
  /* A negative tagndx is meaningless, but the SCO 3.2v4 cc can
     generate one, so we must be careful to ignore it.  */
  if (auxent->u.auxent.x_sym.x_tagndx.l > 0)
    {
      auxent->u.auxent.x_sym.x_tagndx.p =
	table_base + auxent->u.auxent.x_sym.x_tagndx.l;
      auxent->fix_tag = 1;
    }
}

/* Allocate space for the ".debug" section, and read it.
   We did not read the debug section until now, because
   we didn't want to go to the trouble until someone needed it.  */

static char *
build_debug_section (abfd)
     bfd *abfd;
{
  char *debug_section;
  file_ptr position;
  bfd_size_type sec_size;

  asection *sect = bfd_get_section_by_name (abfd, ".debug");

  if (!sect)
    {
      bfd_set_error (bfd_error_no_debug_section);
      return NULL;
    }

  sec_size = bfd_get_section_size_before_reloc (sect);
  debug_section = (PTR) bfd_alloc (abfd, sec_size);
  if (debug_section == NULL)
    return NULL;

  /* Seek to the beginning of the `.debug' section and read it.
     Save the current position first; it is needed by our caller.
     Then read debug section and reset the file pointer.  */

  position = bfd_tell (abfd);
  if (bfd_seek (abfd, sect->filepos, SEEK_SET) != 0
      || bfd_bread (debug_section, sec_size, abfd) != sec_size
      || bfd_seek (abfd, position, SEEK_SET) != 0)
    return NULL;
  return debug_section;
}

/* Return a pointer to a malloc'd copy of 'name'.  'name' may not be
   \0-terminated, but will not exceed 'maxlen' characters.  The copy *will*
   be \0-terminated.  */
static char *
copy_name (abfd, name, maxlen)
     bfd *abfd;
     char *name;
     size_t maxlen;
{
  size_t len;
  char *newname;

  for (len = 0; len < maxlen; ++len)
    {
      if (name[len] == '\0')
	{
	  break;
	}
    }

  if ((newname = (PTR) bfd_alloc (abfd, (bfd_size_type) len + 1)) == NULL)
    return (NULL);
  strncpy (newname, name, len);
  newname[len] = '\0';
  return newname;
}

/* Read in the external symbols.  */

boolean
_bfd_coff_get_external_symbols (abfd)
     bfd *abfd;
{
  bfd_size_type symesz;
  bfd_size_type size;
  PTR syms;

  if (obj_coff_external_syms (abfd) != NULL)
    return true;

  symesz = bfd_coff_symesz (abfd);

  size = obj_raw_syment_count (abfd) * symesz;

  syms = (PTR) bfd_malloc (size);
  if (syms == NULL && size != 0)
    return false;

  if (bfd_seek (abfd, obj_sym_filepos (abfd), SEEK_SET) != 0
      || bfd_bread (syms, size, abfd) != size)
    {
      if (syms != NULL)
	free (syms);
      return false;
    }

  obj_coff_external_syms (abfd) = syms;

  return true;
}

/* Read in the external strings.  The strings are not loaded until
   they are needed.  This is because we have no simple way of
   detecting a missing string table in an archive.  */

const char *
_bfd_coff_read_string_table (abfd)
     bfd *abfd;
{
  char extstrsize[STRING_SIZE_SIZE];
  bfd_size_type strsize;
  char *strings;
  file_ptr pos;

  if (obj_coff_strings (abfd) != NULL)
    return obj_coff_strings (abfd);

  if (obj_sym_filepos (abfd) == 0)
    {
      bfd_set_error (bfd_error_no_symbols);
      return NULL;
    }

  pos = obj_sym_filepos (abfd);
  pos += obj_raw_syment_count (abfd) * bfd_coff_symesz (abfd);
  if (bfd_seek (abfd, pos, SEEK_SET) != 0)
    return NULL;

  if (bfd_bread (extstrsize, (bfd_size_type) sizeof extstrsize, abfd)
      != sizeof extstrsize)
    {
      if (bfd_get_error () != bfd_error_file_truncated)
	return NULL;

      /* There is no string table.  */
      strsize = STRING_SIZE_SIZE;
    }
  else
    {
#if STRING_SIZE_SIZE == 4
      strsize = H_GET_32 (abfd, extstrsize);
#else
 #error Change H_GET_32
#endif
    }

  if (strsize < STRING_SIZE_SIZE)
    {
      (*_bfd_error_handler)
	(_("%s: bad string table size %lu"), bfd_archive_filename (abfd),
	 (unsigned long) strsize);
      bfd_set_error (bfd_error_bad_value);
      return NULL;
    }

  strings = (char *) bfd_malloc (strsize);
  if (strings == NULL)
    return NULL;

  if (bfd_bread (strings + STRING_SIZE_SIZE, strsize - STRING_SIZE_SIZE, abfd)
      != strsize - STRING_SIZE_SIZE)
    {
      free (strings);
      return NULL;
    }

  obj_coff_strings (abfd) = strings;

  return strings;
}

/* Free up the external symbols and strings read from a COFF file.  */

boolean
_bfd_coff_free_symbols (abfd)
     bfd *abfd;
{
  if (obj_coff_external_syms (abfd) != NULL
      && ! obj_coff_keep_syms (abfd))
    {
      free (obj_coff_external_syms (abfd));
      obj_coff_external_syms (abfd) = NULL;
    }
  if (obj_coff_strings (abfd) != NULL
      && ! obj_coff_keep_strings (abfd))
    {
      free (obj_coff_strings (abfd));
      obj_coff_strings (abfd) = NULL;
    }
  return true;
}

/* Read a symbol table into freshly bfd_allocated memory, swap it, and
   knit the symbol names into a normalized form.  By normalized here I
   mean that all symbols have an n_offset pointer that points to a null-
   terminated string.  */

combined_entry_type *
coff_get_normalized_symtab (abfd)
     bfd *abfd;
{
  combined_entry_type *internal;
  combined_entry_type *internal_ptr;
  combined_entry_type *symbol_ptr;
  combined_entry_type *internal_end;
  size_t symesz;
  char *raw_src;
  char *raw_end;
  const char *string_table = NULL;
  char *debug_section = NULL;
  bfd_size_type size;

  if (obj_raw_syments (abfd) != NULL)
    return obj_raw_syments (abfd);

  size = obj_raw_syment_count (abfd) * sizeof (combined_entry_type);
  internal = (combined_entry_type *) bfd_zalloc (abfd, size);
  if (internal == NULL && size != 0)
    return NULL;
  internal_end = internal + obj_raw_syment_count (abfd);

  if (! _bfd_coff_get_external_symbols (abfd))
    return NULL;

  raw_src = (char *) obj_coff_external_syms (abfd);

  /* mark the end of the symbols */
  symesz = bfd_coff_symesz (abfd);
  raw_end = (char *) raw_src + obj_raw_syment_count (abfd) * symesz;

  /* FIXME SOMEDAY.  A string table size of zero is very weird, but
     probably possible.  If one shows up, it will probably kill us.  */

  /* Swap all the raw entries */
  for (internal_ptr = internal;
       raw_src < raw_end;
       raw_src += symesz, internal_ptr++)
    {

      unsigned int i;
      bfd_coff_swap_sym_in (abfd, (PTR) raw_src,
			    (PTR) & internal_ptr->u.syment);
      symbol_ptr = internal_ptr;

      for (i = 0;
	   i < symbol_ptr->u.syment.n_numaux;
	   i++)
	{
	  internal_ptr++;
	  raw_src += symesz;
	  bfd_coff_swap_aux_in (abfd, (PTR) raw_src,
				symbol_ptr->u.syment.n_type,
				symbol_ptr->u.syment.n_sclass,
				(int) i, symbol_ptr->u.syment.n_numaux,
				&(internal_ptr->u.auxent));
	  coff_pointerize_aux (abfd, internal, symbol_ptr, i,
			       internal_ptr);
	}
    }

  /* Free the raw symbols, but not the strings (if we have them).  */
  obj_coff_keep_strings (abfd) = true;
  if (! _bfd_coff_free_symbols (abfd))
    return NULL;

  for (internal_ptr = internal; internal_ptr < internal_end;
       internal_ptr++)
    {
      if (internal_ptr->u.syment.n_sclass == C_FILE
	  && internal_ptr->u.syment.n_numaux > 0)
	{
	  /* make a file symbol point to the name in the auxent, since
	     the text ".file" is redundant */
	  if ((internal_ptr + 1)->u.auxent.x_file.x_n.x_zeroes == 0)
	    {
	      /* the filename is a long one, point into the string table */
	      if (string_table == NULL)
		{
		  string_table = _bfd_coff_read_string_table (abfd);
		  if (string_table == NULL)
		    return NULL;
		}

	      internal_ptr->u.syment._n._n_n._n_offset =
		((long)
		 (string_table
		  + (internal_ptr + 1)->u.auxent.x_file.x_n.x_offset));
	    }
	  else
	    {
	      /* Ordinary short filename, put into memory anyway.  The
                 Microsoft PE tools sometimes store a filename in
                 multiple AUX entries.  */
	      if (internal_ptr->u.syment.n_numaux > 1
		  && coff_data (abfd)->pe)
		{
		  internal_ptr->u.syment._n._n_n._n_offset =
		    ((long)
		     copy_name (abfd,
				(internal_ptr + 1)->u.auxent.x_file.x_fname,
				internal_ptr->u.syment.n_numaux * symesz));
		}
	      else
		{
		  internal_ptr->u.syment._n._n_n._n_offset =
		    ((long)
		     copy_name (abfd,
				(internal_ptr + 1)->u.auxent.x_file.x_fname,
				(size_t) bfd_coff_filnmlen (abfd)));
		}
	    }
	}
      else
	{
	  if (internal_ptr->u.syment._n._n_n._n_zeroes != 0)
	    {
	      /* This is a "short" name.  Make it long.  */
	      size_t i;
	      char *newstring;

	      /* find the length of this string without walking into memory
	         that isn't ours.  */
	      for (i = 0; i < 8; ++i)
		if (internal_ptr->u.syment._n._n_name[i] == '\0')
		  break;

	      newstring = (PTR) bfd_zalloc (abfd, (bfd_size_type) (i + 1));
	      if (newstring == NULL)
		return (NULL);
	      strncpy (newstring, internal_ptr->u.syment._n._n_name, i);
	      internal_ptr->u.syment._n._n_n._n_offset = (long int) newstring;
	      internal_ptr->u.syment._n._n_n._n_zeroes = 0;
	    }
	  else if (internal_ptr->u.syment._n._n_n._n_offset == 0)
	    internal_ptr->u.syment._n._n_n._n_offset = (long int) "";
	  else if (!bfd_coff_symname_in_debug (abfd, &internal_ptr->u.syment))
	    {
	      /* Long name already.  Point symbol at the string in the
                 table.  */
	      if (string_table == NULL)
		{
		  string_table = _bfd_coff_read_string_table (abfd);
		  if (string_table == NULL)
		    return NULL;
		}
	      internal_ptr->u.syment._n._n_n._n_offset =
		((long int)
		 (string_table
		  + internal_ptr->u.syment._n._n_n._n_offset));
	    }
	  else
	    {
	      /* Long name in debug section.  Very similar.  */
	      if (debug_section == NULL)
		debug_section = build_debug_section (abfd);
	      internal_ptr->u.syment._n._n_n._n_offset = (long int)
		(debug_section + internal_ptr->u.syment._n._n_n._n_offset);
	    }
	}
      internal_ptr += internal_ptr->u.syment.n_numaux;
    }

  obj_raw_syments (abfd) = internal;
  BFD_ASSERT (obj_raw_syment_count (abfd)
	      == (unsigned int) (internal_ptr - internal));

  return (internal);
}				/* coff_get_normalized_symtab() */

long
coff_get_reloc_upper_bound (abfd, asect)
     bfd *abfd;
     sec_ptr asect;
{
  if (bfd_get_format (abfd) != bfd_object)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }
  return (asect->reloc_count + 1) * sizeof (arelent *);
}

asymbol *
coff_make_empty_symbol (abfd)
     bfd *abfd;
{
  bfd_size_type amt = sizeof (coff_symbol_type);
  coff_symbol_type *new = (coff_symbol_type *) bfd_zalloc (abfd, amt);
  if (new == NULL)
    return (NULL);
  new->symbol.section = 0;
  new->native = 0;
  new->lineno = (alent *) NULL;
  new->done_lineno = false;
  new->symbol.the_bfd = abfd;
  return &new->symbol;
}

/* Make a debugging symbol.  */

asymbol *
coff_bfd_make_debug_symbol (abfd, ptr, sz)
     bfd *abfd;
     PTR ptr ATTRIBUTE_UNUSED;
     unsigned long sz ATTRIBUTE_UNUSED;
{
  bfd_size_type amt = sizeof (coff_symbol_type);
  coff_symbol_type *new = (coff_symbol_type *) bfd_alloc (abfd, amt);
  if (new == NULL)
    return (NULL);
  /* @@ The 10 is a guess at a plausible maximum number of aux entries
     (but shouldn't be a constant).  */
  amt = sizeof (combined_entry_type) * 10;
  new->native = (combined_entry_type *) bfd_zalloc (abfd, amt);
  if (!new->native)
    return (NULL);
  new->symbol.section = bfd_abs_section_ptr;
  new->symbol.flags = BSF_DEBUGGING;
  new->lineno = (alent *) NULL;
  new->done_lineno = false;
  new->symbol.the_bfd = abfd;
  return &new->symbol;
}

void
coff_get_symbol_info (abfd, symbol, ret)
     bfd *abfd;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
  if (coffsymbol (symbol)->native != NULL
      && coffsymbol (symbol)->native->fix_value)
    {
      ret->value = coffsymbol (symbol)->native->u.syment.n_value -
	(unsigned long) obj_raw_syments (abfd);
    }
}

/* Return the COFF syment for a symbol.  */

boolean
bfd_coff_get_syment (abfd, symbol, psyment)
     bfd *abfd;
     asymbol *symbol;
     struct internal_syment *psyment;
{
  coff_symbol_type *csym;

  csym = coff_symbol_from (abfd, symbol);
  if (csym == NULL || csym->native == NULL)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  *psyment = csym->native->u.syment;

  if (csym->native->fix_value)
    psyment->n_value = psyment->n_value -
      (unsigned long) obj_raw_syments (abfd);

  /* FIXME: We should handle fix_line here.  */

  return true;
}

/* Return the COFF auxent for a symbol.  */

boolean
bfd_coff_get_auxent (abfd, symbol, indx, pauxent)
     bfd *abfd;
     asymbol *symbol;
     int indx;
     union internal_auxent *pauxent;
{
  coff_symbol_type *csym;
  combined_entry_type *ent;

  csym = coff_symbol_from (abfd, symbol);

  if (csym == NULL
      || csym->native == NULL
      || indx >= csym->native->u.syment.n_numaux)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  ent = csym->native + indx + 1;

  *pauxent = ent->u.auxent;

  if (ent->fix_tag)
    pauxent->x_sym.x_tagndx.l =
      ((combined_entry_type *) pauxent->x_sym.x_tagndx.p
       - obj_raw_syments (abfd));

  if (ent->fix_end)
    pauxent->x_sym.x_fcnary.x_fcn.x_endndx.l =
      ((combined_entry_type *) pauxent->x_sym.x_fcnary.x_fcn.x_endndx.p
       - obj_raw_syments (abfd));

  if (ent->fix_scnlen)
    pauxent->x_csect.x_scnlen.l =
      ((combined_entry_type *) pauxent->x_csect.x_scnlen.p
       - obj_raw_syments (abfd));

  return true;
}

/* Print out information about COFF symbol.  */

void
coff_print_symbol (abfd, filep, symbol, how, base)
     bfd *abfd;
     PTR filep;
     asymbol *symbol;
     bfd_print_symbol_type how;
     asymbol *base;
{
  FILE *file = (FILE *) filep;

  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;

    case bfd_print_symbol_more:
      fprintf (file, "coff %s %s",
	       coffsymbol (symbol)->native ? "n" : "g",
	       coffsymbol (symbol)->lineno ? "l" : " ");
      break;

    case bfd_print_symbol_all:
      if (coffsymbol (symbol)->native)
	{
	  bfd_vma val;
	  unsigned int aux;
	  combined_entry_type *combined = coffsymbol (symbol)->native;
	  struct lineno_cache_entry *l = coffsymbol (symbol)->lineno;
  	  combined_entry_type *root = coffsymbol (base)->native;

	  fprintf (file, "[%3ld]", (long) (combined - root));

	  if (! combined->fix_value)
	    val = (bfd_vma) combined->u.syment.n_value;
	  else
	    val = combined->u.syment.n_value - (unsigned long) root;

#ifndef XCOFF64
	  fprintf (file,
		   "(sec %2d)(fl 0x%02x)(ty %3x)(scl %3d) (nx %d) 0x%08lx %s",
		   combined->u.syment.n_scnum,
		   combined->u.syment.n_flags,
		   combined->u.syment.n_type,
		   combined->u.syment.n_sclass,
		   combined->u.syment.n_numaux,
		   (unsigned long) val,
		   symbol->name);
#else
	  /* Print out the wide, 64 bit, symbol value */
	  fprintf (file,
		   "(sec %2d)(fl 0x%02x)(ty %3x)(scl %3d) (nx %d) 0x%016llx %s",
		   combined->u.syment.n_scnum,
		   combined->u.syment.n_flags,
		   combined->u.syment.n_type,
		   combined->u.syment.n_sclass,
		   combined->u.syment.n_numaux,
		   val,
		   symbol->name);
#endif

	  for (aux = 0; aux < combined->u.syment.n_numaux; aux++)
	    {
	      combined_entry_type *auxp = combined + aux + 1;
	      long tagndx;

	      if (auxp->fix_tag)
		tagndx = auxp->u.auxent.x_sym.x_tagndx.p - root;
	      else
		tagndx = auxp->u.auxent.x_sym.x_tagndx.l;

	      fprintf (file, "\n");

	      if (bfd_coff_print_aux (abfd, file, root, combined, auxp, aux))
		continue;

	      switch (combined->u.syment.n_sclass)
		{
		case C_FILE:
		  fprintf (file, "File ");
		  break;

		case C_STAT:
		  if (combined->u.syment.n_type == T_NULL)
		    /* probably a section symbol? */
		    {
		      fprintf (file, "AUX scnlen 0x%lx nreloc %d nlnno %d",
			       (long) auxp->u.auxent.x_scn.x_scnlen,
			       auxp->u.auxent.x_scn.x_nreloc,
			       auxp->u.auxent.x_scn.x_nlinno);
		      if (auxp->u.auxent.x_scn.x_checksum != 0
			  || auxp->u.auxent.x_scn.x_associated != 0
			  || auxp->u.auxent.x_scn.x_comdat != 0)
			fprintf (file, " checksum 0x%lx assoc %d comdat %d",
				 auxp->u.auxent.x_scn.x_checksum,
				 auxp->u.auxent.x_scn.x_associated,
				 auxp->u.auxent.x_scn.x_comdat);
		      break;
		    }
		    /* else fall through */
		case C_EXT:
		  if (ISFCN (combined->u.syment.n_type))
		    {
		      long next, llnos;

		      if (auxp->fix_end)
			next = (auxp->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.p
			       - root);
		      else
			next = auxp->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.l;
		      llnos = auxp->u.auxent.x_sym.x_fcnary.x_fcn.x_lnnoptr;
		      fprintf (file,
			       "AUX tagndx %ld ttlsiz 0x%lx lnnos %ld next %ld",
			       tagndx, auxp->u.auxent.x_sym.x_misc.x_fsize,
			       llnos, next);
		      break;
		    }
		  /* else fall through */
		default:
		  fprintf (file, "AUX lnno %d size 0x%x tagndx %ld",
			   auxp->u.auxent.x_sym.x_misc.x_lnsz.x_lnno,
			   auxp->u.auxent.x_sym.x_misc.x_lnsz.x_size,
			   tagndx);
		  if (auxp->fix_end)
		    fprintf (file, " endndx %ld",
			     ((long)
			      (auxp->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.p
			       - root)));
		  break;
		}
	    }

	  if (l)
	    {
	      fprintf (file, "\n%s :", l->u.sym->name);
	      l++;
	      while (l->line_number)
		{
		  fprintf (file, "\n%4d : 0x%lx",
			   l->line_number,
			   ((unsigned long)
			    (l->u.offset + symbol->section->vma)));
		  l++;
		}
	    }
	}
      else
	{
	  bfd_print_symbol_vandf (abfd, (PTR) file, symbol);
	  fprintf (file, " %-5s %s %s %s",
		   symbol->section->name,
		   coffsymbol (symbol)->native ? "n" : "g",
		   coffsymbol (symbol)->lineno ? "l" : " ",
		   symbol->name);
	}
    }
}

/* Return whether a symbol name implies a local symbol.  In COFF,
   local symbols generally start with ``.L''.  Most targets use this
   function for the is_local_label_name entry point, but some may
   override it.  */

boolean
_bfd_coff_is_local_label_name (abfd, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *name;
{
  return (boolean) (name[0] == '.' && name[1] == 'L');
}

/* Provided a BFD, a section and an offset (in bytes, not octets) into the
   section, calculate and return the name of the source file and the line
   nearest to the wanted location.  */

boolean
coff_find_nearest_line (abfd, section, symbols, offset, filename_ptr,
			functionname_ptr, line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     const char **filename_ptr;
     const char **functionname_ptr;
     unsigned int *line_ptr;
{
  boolean found;
  unsigned int i;
  unsigned int line_base;
  coff_data_type *cof = coff_data (abfd);
  /* Run through the raw syments if available */
  combined_entry_type *p;
  combined_entry_type *pend;
  alent *l;
  struct coff_section_tdata *sec_data;
  bfd_size_type amt;

  /* Before looking through the symbol table, try to use a .stab
     section to find the information.  */
  if (! _bfd_stab_section_find_nearest_line (abfd, symbols, section, offset,
					     &found, filename_ptr,
					     functionname_ptr, line_ptr,
					     &coff_data(abfd)->line_info))
    return false;

  if (found)
    return true;

  /* Also try examining DWARF2 debugging information.  */
  if (_bfd_dwarf2_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr,
				     line_ptr, 0,
				     &coff_data(abfd)->dwarf2_find_line_info))
    return true;

  *filename_ptr = 0;
  *functionname_ptr = 0;
  *line_ptr = 0;

  /* Don't try and find line numbers in a non coff file */
  if (!bfd_family_coff (abfd))
    return false;

  if (cof == NULL)
    return false;

  /* Find the first C_FILE symbol.  */
  p = cof->raw_syments;
  if (!p)
    return false;

  pend = p + cof->raw_syment_count;
  while (p < pend)
    {
      if (p->u.syment.n_sclass == C_FILE)
	break;
      p += 1 + p->u.syment.n_numaux;
    }

  if (p < pend)
    {
      bfd_vma sec_vma;
      bfd_vma maxdiff;

      /* Look through the C_FILE symbols to find the best one.  */
      sec_vma = bfd_get_section_vma (abfd, section);
      *filename_ptr = (char *) p->u.syment._n._n_n._n_offset;
      maxdiff = (bfd_vma) 0 - (bfd_vma) 1;
      while (1)
	{
	  combined_entry_type *p2;

	  for (p2 = p + 1 + p->u.syment.n_numaux;
	       p2 < pend;
	       p2 += 1 + p2->u.syment.n_numaux)
	    {
	      if (p2->u.syment.n_scnum > 0
		  && (section
		      == coff_section_from_bfd_index (abfd,
						      p2->u.syment.n_scnum)))
		break;
	      if (p2->u.syment.n_sclass == C_FILE)
		{
		  p2 = pend;
		  break;
		}
	    }

	  /* We use <= MAXDIFF here so that if we get a zero length
             file, we actually use the next file entry.  */
	  if (p2 < pend
	      && offset + sec_vma >= (bfd_vma) p2->u.syment.n_value
	      && offset + sec_vma - (bfd_vma) p2->u.syment.n_value <= maxdiff)
	    {
	      *filename_ptr = (char *) p->u.syment._n._n_n._n_offset;
	      maxdiff = offset + sec_vma - p2->u.syment.n_value;
	    }

	  /* Avoid endless loops on erroneous files by ensuring that
	     we always move forward in the file.  */
	  if (p >= cof->raw_syments + p->u.syment.n_value)
	    break;

	  p = cof->raw_syments + p->u.syment.n_value;
	  if (p > pend || p->u.syment.n_sclass != C_FILE)
	    break;
	}
    }

  /* Now wander though the raw linenumbers of the section */
  /* If we have been called on this section before, and the offset we
     want is further down then we can prime the lookup loop.  */
  sec_data = coff_section_data (abfd, section);
  if (sec_data != NULL
      && sec_data->i > 0
      && offset >= sec_data->offset)
    {
      i = sec_data->i;
      *functionname_ptr = sec_data->function;
      line_base = sec_data->line_base;
    }
  else
    {
      i = 0;
      line_base = 0;
    }

  if (section->lineno != NULL)
    {
      bfd_vma last_value = 0;

      l = &section->lineno[i];

      for (; i < section->lineno_count; i++)
	{
	  if (l->line_number == 0)
	    {
	      /* Get the symbol this line number points at */
	      coff_symbol_type *coff = (coff_symbol_type *) (l->u.sym);
	      if (coff->symbol.value > offset)
		break;
	      *functionname_ptr = coff->symbol.name;
	      last_value = coff->symbol.value;
	      if (coff->native)
		{
		  combined_entry_type *s = coff->native;
		  s = s + 1 + s->u.syment.n_numaux;

		  /* In XCOFF a debugging symbol can follow the
		     function symbol.  */
		  if (s->u.syment.n_scnum == N_DEBUG)
		    s = s + 1 + s->u.syment.n_numaux;

		  /* S should now point to the .bf of the function.  */
		  if (s->u.syment.n_numaux)
		    {
		      /* The linenumber is stored in the auxent.  */
		      union internal_auxent *a = &((s + 1)->u.auxent);
		      line_base = a->x_sym.x_misc.x_lnsz.x_lnno;
		      *line_ptr = line_base;
		    }
		}
	    }
	  else
	    {
	      if (l->u.offset > offset)
		break;
	      *line_ptr = l->line_number + line_base - 1;
	    }
	  l++;
	}

      /* If we fell off the end of the loop, then assume that this
	 symbol has no line number info.  Otherwise, symbols with no
	 line number info get reported with the line number of the
	 last line of the last symbol which does have line number
	 info.  We use 0x100 as a slop to account for cases where the
	 last line has executable code.  */
      if (i >= section->lineno_count
	  && last_value != 0
	  && offset - last_value > 0x100)
	{
	  *functionname_ptr = NULL;
	  *line_ptr = 0;
	}
    }

  /* Cache the results for the next call.  */
  if (sec_data == NULL && section->owner == abfd)
    {
      amt = sizeof (struct coff_section_tdata);
      section->used_by_bfd = (PTR) bfd_zalloc (abfd, amt);
      sec_data = (struct coff_section_tdata *) section->used_by_bfd;
    }
  if (sec_data != NULL)
    {
      sec_data->offset = offset;
      sec_data->i = i;
      sec_data->function = *functionname_ptr;
      sec_data->line_base = line_base;
    }

  return true;
}

int
coff_sizeof_headers (abfd, reloc)
     bfd *abfd;
     boolean reloc;
{
  size_t size;

  if (! reloc)
    {
      size = bfd_coff_filhsz (abfd) + bfd_coff_aoutsz (abfd);
    }
  else
    {
      size = bfd_coff_filhsz (abfd);
    }

  size += abfd->section_count * bfd_coff_scnhsz (abfd);
  return size;
}

/* Change the class of a coff symbol held by BFD.  */
boolean
bfd_coff_set_symbol_class (abfd, symbol, class)
     bfd *         abfd;
     asymbol *     symbol;
     unsigned int  class;
{
  coff_symbol_type * csym;

  csym = coff_symbol_from (abfd, symbol);
  if (csym == NULL)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }
  else if (csym->native == NULL)
    {
      /* This is an alien symbol which no native coff backend data.
	 We cheat here by creating a fake native entry for it and
	 then filling in the class.  This code is based on that in
	 coff_write_alien_symbol().  */

      combined_entry_type * native;
      bfd_size_type amt = sizeof (* native);

      native = (combined_entry_type *) bfd_zalloc (abfd, amt);
      if (native == NULL)
	return false;

      native->u.syment.n_type   = T_NULL;
      native->u.syment.n_sclass = class;

      if (bfd_is_und_section (symbol->section))
	{
	  native->u.syment.n_scnum = N_UNDEF;
	  native->u.syment.n_value = symbol->value;
	}
      else if (bfd_is_com_section (symbol->section))
	{
	  native->u.syment.n_scnum = N_UNDEF;
	  native->u.syment.n_value = symbol->value;
	}
      else
	{
	  native->u.syment.n_scnum =
	    symbol->section->output_section->target_index;
	  native->u.syment.n_value = (symbol->value
				      + symbol->section->output_section->vma
				      + symbol->section->output_offset);

	  /* Copy the any flags from the file header into the symbol.
	     FIXME: Why?  */
	  native->u.syment.n_flags = bfd_asymbol_bfd (& csym->symbol)->flags;
	}

      csym->native = native;
    }
  else
    {
      csym->native->u.syment.n_sclass = class;
    }

  return true;
}


#ifdef DYNAMIC_LINKING /* [ */
/* Create some sections which will be filled in with dynamic linking
   information.  ABFD is an input file which requires dynamic sections
   to be created.  The dynamic sections take up virtual memory space
   when the final executable is run, so we need to create them before
   addresses are assigned to the output sections.  We work out the
   actual contents and size of these sections later.  */

long
coff_get_dynamic_symtab_upper_bound (abfd)
     bfd *abfd;
{
  long symcount;
  long symtab_size;
  asection *hdr;

  hdr = coff_dynsymtab(abfd);

  if (hdr == NULL)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  symcount = bfd_section_size(abfd, hdr) / bfd_coff_symesz (abfd);
  symtab_size = (symcount - 1 + 1) * (sizeof (asymbol *));

  return symtab_size;
}

long
coff_canonicalize_dynamic_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  unsigned long size;
  unsigned long strtab_size;
  asection *dynsym;
  asection *dynstr;
  char *raw_src;
  char *raw_end;
  int sym_count;
  bfd_size_type symesz;
  char *string_table = NULL;
  coff_symbol_type *cached_area;
  int syt_index = 0;
  combined_entry_type *isyms;
  combined_entry_type *internal;
  combined_entry_type *symbol_ptr;
  unsigned int n_tmask;
  unsigned int n_btshft;
  unsigned int n_btmask;
  int i;


  n_tmask = coff_data (abfd)->local_n_tmask;
  n_btshft = coff_data (abfd)->local_n_btshft;
  n_btmask = coff_data (abfd)->local_n_btmask;

  symesz = bfd_coff_symesz (abfd);

  /* First get the dynamic symbol table */
  dynsym = coff_dynsymtab(abfd);

  if (dynsym == NULL) 
     return 0;

  sym_count = dynsym->info_r;

  if (sym_count == 0)
     return 0;

  size = sym_count * symesz;

  raw_src = (PTR) bfd_malloc (size);
  if (raw_src == NULL)
     return -1;
  
  if (! bfd_get_section_contents (abfd, dynsym, raw_src, (file_ptr) 0, size))
     return -1;

  /* mark the end of the symbols */
  raw_end = (char *) raw_src + sym_count * symesz;

  dynstr = coff_dynstrtab(abfd);

  string_table = NULL;
  strtab_size = 0;

  if (dynstr != NULL)
    {
      strtab_size = bfd_section_size(abfd, dynstr); 
      if (strtab_size == 0) abort();
      string_table = (PTR) bfd_malloc (strtab_size);
      if (string_table == NULL)
          return -1;
  
      if (! bfd_get_section_contents (abfd, dynstr, string_table, 
	      (file_ptr) 0, strtab_size))
	return -1;
    }

  /* Allocate enough room for all the symbols in cached form */
  cached_area = (coff_symbol_type *)
	 bfd_alloc (abfd, (sym_count * sizeof (coff_symbol_type)));

  if (cached_area == NULL)
    return -1;

  internal = isyms = (combined_entry_type *)
	 bfd_alloc (abfd, (sym_count * sizeof (combined_entry_type)));

  if (isyms == NULL)
    return -1;

  /* Swap all the raw entries */
  for (; raw_src < raw_end; raw_src += symesz)
    {
      coff_symbol_type *dst = &cached_area[syt_index];

      bfd_coff_swap_sym_in (abfd, (PTR) raw_src, (PTR)&isyms->u.syment);

      /* canonicalize the name; can't use other code because it's from
	 the dynamic symbol table here. */
      if (isyms->u.syment._n._n_n._n_zeroes != 0)
	{
	  /* This is a "short" name.  Make it long.  */
	  unsigned long i = 0;
	  char *newstring = NULL;

	  /* find the length of this string without walking into memory
	     that isn't ours.  */
	  for (i = 0; i < 8; ++i)
	    {
	      if (isyms->u.syment._n._n_name[i] == '\0')
		{
		  break;
		}		/* if end of string */
	    }		/* possible lengths of this string. */

	  if ((newstring = (PTR) bfd_alloc (abfd, ++i)) == NULL)
	    return -1;
	  memset (newstring, 0, i);
	  strncpy (newstring, isyms->u.syment._n._n_name, i - 1);
	  isyms->u.syment._n._n_n._n_offset = (long int) newstring;
	  isyms->u.syment._n._n_n._n_zeroes = 0;
	}
      else if (isyms->u.syment._n._n_n._n_offset == 0)
	isyms->u.syment._n._n_n._n_offset = (long int) "";
      else
	{
	  /* Long name already.  Point symbol at the string in the
	     table.  */
	  if (string_table == NULL)
	    {
		return -1;
	    }
	  if ((unsigned long)isyms->u.syment._n._n_n._n_offset > strtab_size
	     || isyms->u.syment._n._n_n._n_offset < 0)
	    {
	        isyms->u.syment._n._n_n._n_offset = (int)"BAD SYMBOL";
	    }
	  isyms->u.syment._n._n_n._n_offset =
	    ((long int)
	     (string_table
	      + isyms->u.syment._n._n_n._n_offset));
	}

      bfd_coff_canonicalize_one_symbol(abfd, isyms, dst);

      alocation[syt_index] = (asymbol *)dst;

      /* Because of C_NT_WEAK n_numaux isn't always zero. */
      symbol_ptr = isyms;
      for (i = 0;
	   i < isyms->u.syment.n_numaux;
	   i++)
	{
	  raw_src += symesz;
	  isyms++;
	  bfd_coff_swap_aux_in (abfd, (PTR) raw_src,
				symbol_ptr->u.syment.n_type,
				symbol_ptr->u.syment.n_sclass,
				i, symbol_ptr->u.syment.n_numaux,
				&(isyms->u.auxent));
	  coff_pointerize_aux (abfd, internal, symbol_ptr, i,
			       isyms);
         syt_index += 1;
	  alocation[syt_index] = NULL;
	}
      syt_index += 1;
      isyms++;
    }				/* bfdize the native symtab */

  return syt_index; 
} 			/* coff_canonicalize_dynamic_symtab() */


/* Return the size required for the dynamic reloc entries.  Any
   section that was actually installed in the BFD, and has type
   SHT_REL or SHT_RELA, and uses the dynamic symbol table, is
   considered to be a dynamic reloc section.  */

long
coff_get_dynamic_reloc_upper_bound (abfd)
     bfd *abfd;
{
  long ret;
  asection *s;

  if (dyn_data (abfd) == 0)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  ret = sizeof (arelent *);
  for (s = abfd->sections; s != NULL; s = s->next)
   if (strncmp(s->name, ".rel.", 5) == 0)
      {
          ret += ((pei_section_data(abfd,s)->virt_size 
		   / bfd_coff_relsz(abfd)) * sizeof (arelent *));
      }

  return ret;
}

/* A special case that assures that the first slot is used, so "real"
   entries can assume the offset is not zero; only some stringtabs
   (.dynstr and others derived from ELF) rely on this assumption, and
   it could be removed, but it'd take more hunting than it's worth. */
struct bfd_strtab_hash *
_bfd_coff_stringtab_init ()
{
  struct bfd_strtab_hash *ret;

  ret = _bfd_stringtab_init ();
  if (ret != NULL)
    {
      bfd_size_type loc;

      loc = _bfd_stringtab_add (ret, "", true, false);
      BFD_ASSERT (loc == 0 || loc == (bfd_size_type) -1);
      if (loc == (bfd_size_type) -1)
        {
          _bfd_stringtab_free (ret);
          ret = NULL;
        }
    }
  return ret;
}

/* get a string-table section */
char *
bfd_coff_get_str_section (abfd, shindex)
     bfd * abfd;
     unsigned int shindex;
{
  asection **i_shdrp;
  char *shstrtab = NULL;
  unsigned int shstrtabsize;
  asection *sec;

  i_shdrp = coff_coffsections (abfd);
  if (i_shdrp == 0 || i_shdrp[shindex] == 0)
    return 0;

  sec = i_shdrp[shindex];
  shstrtab = (char *) sec->contents;

  if (shstrtab == NULL)
    {
      /* No cached one, attempt to read, and cache what we read. */

      shstrtabsize = bfd_section_size(abfd, sec);
      shstrtab = (char *) bfd_malloc (shstrtabsize);
      if (shstrtab == NULL)
	 return NULL;
  
      if (! bfd_get_section_contents (abfd, sec, shstrtab, 
	      (file_ptr) 0, shstrtabsize))
	 return NULL;

      sec->contents = (PTR) shstrtab;
    }
  return shstrtab;
}

/* This is how we pick strings out of a string table section;
   not used for the main coff string table. */
char *
bfd_coff_string_from_coff_section (abfd, shindex, strindex)
     bfd * abfd;
     unsigned int shindex;
     unsigned int strindex;
{
  asection *hdr;

  if (strindex == 0)
    return "";

  hdr = coff_coffsections (abfd)[shindex];

  if (hdr->contents == NULL
      && bfd_coff_get_str_section (abfd, shindex) == NULL)
    return NULL;

  return ((char *) hdr->contents) + strindex;
}


/* Read in the version information.  */

boolean
_bfd_coff_slurp_version_tables (abfd)
     bfd *abfd;
{
  bfd_byte *contents = NULL;

  if (coff_dynverdef (abfd) != 0)
    {
      asection *hdr;
      coff_external_verdef *everdef;
      coff_internal_verdef *iverdef;
      unsigned int i;

      hdr = coff_dynverdef(abfd);

      dyn_data (abfd)->verdef =
	((coff_internal_verdef *)
	 bfd_zalloc (abfd, hdr->info_r * sizeof (coff_internal_verdef)));
      if (dyn_data (abfd)->verdef == NULL)
	goto error_return;

      dyn_data (abfd)->cverdefs = hdr->info_r;

      contents = (bfd_byte *) bfd_malloc (bfd_section_size(abfd, hdr));
      if (contents == NULL)
	goto error_return;

      if (!bfd_get_section_contents (abfd, hdr, contents,
	  (file_ptr) 0, bfd_section_size(abfd, hdr)))
        goto error_return;

      everdef = (coff_external_verdef *) contents;
      iverdef = dyn_data (abfd)->verdef;
      for (i = 0; i < hdr->info_r; i++, iverdef++)
	{
	  coff_external_verdaux *everdaux;
	  coff_internal_verdaux *iverdaux;
	  unsigned int j;

	  bfd_coff_swap_verdef_in (abfd, everdef, iverdef);

	  iverdef->vd_bfd = abfd;

	  iverdef->vd_auxptr = ((coff_internal_verdaux *)
				bfd_alloc (abfd,
					   (iverdef->vd_cnt
					    * sizeof (coff_internal_verdaux))));
	  if (iverdef->vd_auxptr == NULL)
	    goto error_return;

	  everdaux = ((coff_external_verdaux *)
		      ((bfd_byte *) everdef + iverdef->vd_aux));
	  iverdaux = iverdef->vd_auxptr;
	  for (j = 0; j < iverdef->vd_cnt; j++, iverdaux++)
	    {
	      bfd_coff_swap_verdaux_in (abfd, everdaux, iverdaux);

	      iverdaux->vda_nodename =
		bfd_coff_string_from_coff_section (abfd, hdr->link_index,
						 iverdaux->vda_name);
	      if (iverdaux->vda_nodename == NULL)
		goto error_return;

	      if (j + 1 < iverdef->vd_cnt)
		iverdaux->vda_nextptr = iverdaux + 1;
	      else
		iverdaux->vda_nextptr = NULL;

	      everdaux = ((coff_external_verdaux *)
			  ((bfd_byte *) everdaux + iverdaux->vda_next));
	    }

	  iverdef->vd_nodename = iverdef->vd_auxptr->vda_nodename;

	  if (i + 1 < hdr->info_r)
	    iverdef->vd_nextdef = iverdef + 1;
	  else
	    iverdef->vd_nextdef = NULL;

	  everdef = ((coff_external_verdef *)
		     ((bfd_byte *) everdef + iverdef->vd_next));
	}

      free (contents);
      contents = NULL;
    }

  if (coff_dynverref (abfd) != 0)
    {
      asection *hdr;
      coff_external_verneed *everneed;
      coff_internal_verneed *iverneed;
      unsigned int i;

      hdr = coff_dynverref(abfd);

      dyn_data (abfd)->verref =
	((coff_internal_verneed *)
	 bfd_zalloc (abfd, hdr->info_r * sizeof (coff_internal_verneed)));
      if (dyn_data (abfd)->verref == NULL)
	goto error_return;

      dyn_data (abfd)->cverrefs = hdr->info_r;

      contents = (bfd_byte *) bfd_malloc (bfd_section_size(abfd, hdr));
      if (contents == NULL)
	goto error_return;

      if (!bfd_get_section_contents (abfd, hdr, contents,
	  (file_ptr) 0, bfd_section_size(abfd, hdr)))
        goto error_return;

      everneed = (coff_external_verneed *) contents;
      iverneed = dyn_data (abfd)->verref;
      for (i = 0; i < hdr->info_r; i++, iverneed++)
	{
	  coff_external_vernaux *evernaux;
	  coff_internal_vernaux *ivernaux;
	  unsigned int j;

	  bfd_coff_swap_verneed_in (abfd, everneed, iverneed);

	  iverneed->vn_bfd = abfd;

	  iverneed->vn_filename =
	    bfd_coff_string_from_coff_section (abfd, hdr->link_index,
					     iverneed->vn_file);
	  if (iverneed->vn_filename == NULL)
	    goto error_return;

	  iverneed->vn_auxptr =
	    ((coff_internal_vernaux *)
	     bfd_alloc (abfd,
			iverneed->vn_cnt * sizeof (coff_internal_vernaux)));

	  evernaux = ((coff_external_vernaux *)
		      ((bfd_byte *) everneed + iverneed->vn_aux));
	  ivernaux = iverneed->vn_auxptr;
	  for (j = 0; j < iverneed->vn_cnt; j++, ivernaux++)
	    {
	      bfd_coff_swap_vernaux_in (abfd, evernaux, ivernaux);

	      ivernaux->vna_nodename =
		bfd_coff_string_from_coff_section (abfd, hdr->link_index,
						 ivernaux->vna_name);
	      if (ivernaux->vna_nodename == NULL)
		goto error_return;

	      if (j + 1 < iverneed->vn_cnt)
		ivernaux->vna_nextptr = ivernaux + 1;
	      else
		ivernaux->vna_nextptr = NULL;

	      evernaux = ((coff_external_vernaux *)
			  ((bfd_byte *) evernaux + ivernaux->vna_next));
	    }

	  if (i + 1 < hdr->info_r)
	    iverneed->vn_nextref = iverneed + 1;
	  else
	    iverneed->vn_nextref = NULL;

	  everneed = ((coff_external_verneed *)
		      ((bfd_byte *) everneed + iverneed->vn_next));
	}

      free (contents);
      contents = NULL;
    }

  return true;

 error_return:
  if (contents == NULL)
    free (contents);
  return false;
}
#endif /* ] */
