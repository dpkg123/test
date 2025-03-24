/* COFF specific linker code.
   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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

/* This file contains the COFF backend linker code.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "libcoff.h"

#if defined(DYNAMIC_LINKING) && defined(USE_DLLS)
/* DLL's require some special treatment.  There are two problems: the
   "mechanism" symbols (e.g. __IMPORT_DESCRIPTOR_*) cannot be allowed
   out of the the shared libs, as they will mess up subsequent links
   against the same DLL.  (__imp_ symbols may or may not also fall under
   this, but for different reasons.)

   Secondly, it's not clear whether we want to export the DLL symbols
   at all: do we wish to be able to override them at runtime?  If we don't
   export, they cannot be overridden at runtime.  (This is very like
   a "symbolic" link, except that they're not callable, either.)

   Current decision: don't export any DLL symbols.  If/when we want to
   export them, USE_DLLS should be used as a guide; mostly, that change
   implies turning off the define.  However, a filter to remove the mechanism
   symbols, and possibly the __imp_ symbols, needs to be added.
   (The symbolic flag may still need to be involved?)

   The runtime linker may want the __imp_ symbols to handle double
   thunks that occur when a .so calls a DLL, and the DLL symbols were
   provided in the main program.

   Note: currently, if is_DLL_module doesn't get set, it's equivalent
   to never recognizing that a symbol is a DLL symbol.  */
#endif

static boolean coff_link_add_object_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean coff_link_check_archive_element
  PARAMS ((bfd *, struct bfd_link_info *, boolean *));
static boolean coff_link_check_ar_symbols
  PARAMS ((bfd *, struct bfd_link_info *, boolean *));
static boolean coff_link_add_symbols PARAMS ((bfd *, struct bfd_link_info *));
static char *dores_com PARAMS ((char *, bfd *, int));
static char *get_name PARAMS ((char *, char **));
static int process_embedded_commands
  PARAMS ((bfd *, struct bfd_link_info *, bfd *));
static void mark_relocs PARAMS ((struct coff_final_link_info *, bfd *));
static boolean coff_merge_symbol
  PARAMS ((bfd *, struct bfd_link_info *, const char *, 
	   struct internal_syment *,
	   enum coff_symbol_classification,
	   asection **, bfd_vma *, struct coff_link_hash_entry **,
	   boolean, boolean *, boolean *, boolean *));

/* Return true if SYM is a weak, external symbol. C_NT_WEAK, being really
   an alias, should not be tested for this way because it almost always
   is wrong. */
#define IS_WEAK_EXTERNAL(abfd, sym)			\
  ((sym).n_sclass == C_WEAKEXT)

/* Return true if SYM is an external symbol.  */
#define IS_EXTERNAL(abfd, sym)				\
  ((sym).n_sclass == C_EXT || IS_WEAK_EXTERNAL (abfd, sym))

/* Define macros so that the ISFCN, et. al., macros work correctly.
   These macros are defined in include/coff/internal.h in terms of
   N_TMASK, etc.  These definitions require a user to define local
   variables with the appropriate names, and with values from the
   coff_data (abfd) structure.  */

#define N_TMASK n_tmask
#define N_BTSHFT n_btshft
#define N_BTMASK n_btmask

/* Create an entry in a COFF linker hash table.  */

struct bfd_hash_entry *
_bfd_coff_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct coff_link_hash_entry *ret = (struct coff_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct coff_link_hash_entry *) NULL)
    ret = ((struct coff_link_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct coff_link_hash_entry)));
  if (ret == (struct coff_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct coff_link_hash_entry *)
	 _bfd_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				 table, string));
  if (ret != (struct coff_link_hash_entry *) NULL)
    {
      /* Set local fields.  */
      ret->indx = -1;
      ret->type = T_NULL;
      ret->class = C_NULL;
      ret->numaux = 0;
      ret->auxbfd = NULL;
      ret->aux = NULL;
#ifdef DYNAMIC_LINKING
      ret->dynindx = -1;
      ret->dynstr_index = 0;
#ifdef USE_WEAK
      ret->weakdef = NULL;
#endif
      ret->got_offset = -1;
      ret->plt_offset = -1;
      ret->coff_link_hash_flags = 0;
      ret->num_long_relocs_needed = 0;
      ret->num_relative_relocs_needed = 0;
#ifdef USE_SIZE
      ret->size = 0;
#endif
      ret->verinfo.verdef = NULL;
      /* Assume that we have been called by a non-COFF symbol reader.
         This flag is then reset by the code which reads a COFF input
         file.  This ensures that a symbol created by a non-COFF symbol
         reader will have the flag set correctly.  */
      ret->coff_link_hash_flags = COFF_LINK_NON_COFF;
#endif
    }

  return (struct bfd_hash_entry *) ret;
}

/* Initialize a COFF linker hash table.  */

boolean
_bfd_coff_link_hash_table_init (table, abfd, newfunc)
     struct coff_link_hash_table *table;
     bfd *abfd;
     struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
						struct bfd_hash_table *,
						const char *));
{
  table->stab_info = NULL;
#ifdef DYNAMIC_LINKING
  table->dynamic_sections_created = false;
  table->dynobj = NULL;
  table->dynsymcount = 1; /* Dummy entry for special info. */
  table->dynstr = NULL;
  table->needed = NULL;
  table->hgot = NULL;
  table->sreloc = NULL;
  table->sgot = NULL;
  table->sgotplt = NULL;
  table->srelgot = NULL;
  table->dynamic = NULL;
#endif
  return _bfd_link_hash_table_init (&table->root, abfd, newfunc);
}

/* Create a COFF linker hash table.  */

struct bfd_link_hash_table *
_bfd_coff_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct coff_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct coff_link_hash_table);

  ret = (struct coff_link_hash_table *) bfd_malloc (amt);
  if (ret == NULL)
    return NULL;
  if (! _bfd_coff_link_hash_table_init (ret, abfd,
					_bfd_coff_link_hash_newfunc))
    {
      free (ret);
      return (struct bfd_link_hash_table *) NULL;
    }
  return &ret->root;
}

/* Create an entry in a COFF debug merge hash table.  */

struct bfd_hash_entry *
_bfd_coff_debug_merge_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct coff_debug_merge_hash_entry *ret =
    (struct coff_debug_merge_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct coff_debug_merge_hash_entry *) NULL)
    ret = ((struct coff_debug_merge_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct coff_debug_merge_hash_entry)));
  if (ret == (struct coff_debug_merge_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct coff_debug_merge_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));
  if (ret != (struct coff_debug_merge_hash_entry *) NULL)
    {
      /* Set local fields.  */
      ret->types = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Given a COFF BFD, add symbols to the global hash table as
   appropriate.  */

boolean
_bfd_coff_link_add_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  switch (bfd_get_format (abfd))
    {
    case bfd_object:
      return coff_link_add_object_symbols (abfd, info);
    case bfd_archive:
      return (_bfd_generic_link_add_archive_symbols
	      (abfd, info, coff_link_check_archive_element));
    default:
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }
}

/* This function is called when we want to define a new symbol.  It
   handles the various cases which arise when we find a definition in
   a dynamic object, or when there is already a definition in a
   dynamic object.  The new symbol is described by NAME, SYM, PSEC,
   and PVALUE.  We set SYM_HASH to the hash table entry.  We set
   OVERRIDE if the old symbol is overriding a new definition.  We set
   TYPE_CHANGE_OK if it is OK for the type to change.  We set
   SIZE_CHANGE_OK if it is OK for the size to change.  By OK to
   change, we mean that we shouldn't warn if the type or size does
   change.  */

static boolean
coff_merge_symbol (abfd, info, name, sym, classification, psec, pvalue, 
                  sym_hash, copy, override, type_change_ok, size_change_ok)
     bfd *abfd;
     struct bfd_link_info *info;
     const char *name;
     struct internal_syment *sym;
     enum coff_symbol_classification classification;
     asection **psec;
     bfd_vma *pvalue ATTRIBUTE_UNUSED;
     struct coff_link_hash_entry **sym_hash;
     boolean copy;
     boolean *override;
     boolean *type_change_ok;
     boolean *size_change_ok;
{
  asection *sec;
  struct coff_link_hash_entry *h;
  struct coff_link_hash_entry *h_real;
  boolean new_is_weak;
  boolean old_is_weak_def;
  bfd *oldbfd;
  boolean newdyn, olddyn, olddef, newdef, newdyncommon, olddyncommon;

  unsigned int n_tmask = coff_data (abfd)->local_n_tmask;
  unsigned int n_btshft = coff_data (abfd)->local_n_btshft;

  *override = false;

  sec = *psec;

  if (! bfd_is_und_section (sec))
    h = coff_link_hash_lookup 
	  (coff_hash_table (info), name, true, copy, false);
  else
    h = ((struct coff_link_hash_entry *)
	 bfd_wrapped_link_hash_lookup (abfd, info, name, true, copy, false));

  if (h == NULL)
    return false;

  *sym_hash = h;

  /* This code is for coping with dynamic objects, and is only useful
     if we are doing an COFF link.  */
  if (abfd->xvec != info->hash->creator
     && abfd->xvec != info->hash->creator->input_format)
    return true;

  /* For merging, we only care about real symbols.

     For PE Weaks, we work on the alias name.  (Weakness is a 
     property of the alias.) */
  while ((h->root.type == bfd_link_hash_indirect
	   && !h->root.u.i.info.alias)
	 || h->root.type == bfd_link_hash_warning)
    h = (struct coff_link_hash_entry *) h->root.u.i.link;

  if (h->root.type == bfd_link_hash_indirect)
    {
      /* Implicitly it's an alias! */
      old_is_weak_def = true;
    }
  else
    old_is_weak_def = (h->root.type == bfd_link_hash_defweak);

  new_is_weak = sym->n_sclass == C_WEAKEXT
	  || (obj_pe (abfd) && sym->n_sclass == C_NT_WEAK);


  /* If we just created the symbol, mark it as being a COFF symbol.
     Other than that, there is nothing to do--there is no merge issue
     with a newly defined symbol--so we just return.  */

  if (h->root.type == bfd_link_hash_new)
    {
      h->coff_link_hash_flags &= ~COFF_LINK_NON_COFF;
      return true;
    }

  /* OLDBFD is a BFD associated with the existing symbol.  For 
     indirects and warnings, we'll get it from the real symbol. 
     Don't care if it's an alias or not. */
  h_real = h;
  while (h_real->root.type == bfd_link_hash_indirect
	 || h_real->root.type == bfd_link_hash_warning)
    h_real = (struct coff_link_hash_entry *) h_real->root.u.i.link;

  switch (h_real->root.type)
    {
    default:
      oldbfd = NULL;
      break;

    case bfd_link_hash_undefined:
    case bfd_link_hash_undefweak:
      oldbfd = h_real->root.u.undef.abfd;
      break;

    case bfd_link_hash_defined:
    case bfd_link_hash_defweak:
      oldbfd = h_real->root.u.def.section->owner;
      break;

    case bfd_link_hash_common:
      oldbfd = h_real->root.u.c.p->section->owner;
      break;
    }

  /* In cases involving weak versioned symbols, we may wind up trying
     to merge a symbol with itself.  Catch that here, to avoid the
     confusion that results if we try to override a symbol with
     itself.  The additional tests catch cases like
     _GLOBAL_OFFSET_TABLE_, which are regular symbols defined in a
     dynamic object, which we do want to handle here.  */
  if (abfd == oldbfd
      && ((abfd->flags & DYNAMIC) == 0
	  || (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) == 0))
    return true;

  /* NEWDYN and OLDDYN indicate whether the new or old symbol,
     respectively, is from a dynamic object.  */

  if ((abfd->flags & DYNAMIC) != 0)
    newdyn = true;
  else
    newdyn = false;

  if (oldbfd != NULL)
    olddyn = (oldbfd->flags & DYNAMIC) != 0;
  else
    {
      /* This should be a pretty rare event.  Imposible? */
      olddyn = false;
    }

  /* NEWDEF and OLDDEF indicate whether the new or old symbol,
     respectively, appear to be a definition rather than reference.  */

  newdef = (classification == COFF_SYMBOL_GLOBAL);

  if (h->root.type == bfd_link_hash_undefined
      || h->root.type == bfd_link_hash_undefweak
      || h->root.type == bfd_link_hash_common)
    olddef = false;
  else
    olddef = true;

  /* NEWDYNCOMMON and OLDDYNCOMMON indicate whether the new or old
     symbol, respectively, appears to be a common symbol in a dynamic
     object.  If a symbol appears in an uninitialized section, and is
     not weak, and is not a function, then it may be a common symbol
     which was resolved when the dynamic object was created.  We want
     to treat such symbols specially, because they raise special
     considerations when setting the symbol size: if the symbol
     appears as a common symbol in a regular object, and the size in
     the regular object is larger, we must make sure that we use the
     larger size.  This problematic case can always be avoided in C,
     but it must be handled correctly when using Fortran shared
     libraries.

     Note that if NEWDYNCOMMON is set, NEWDEF will be set, and
     likewise for OLDDYNCOMMON and OLDDEF.

     Note that this test is just a heuristic, and that it is quite
     possible to have an uninitialized symbol in a shared object which
     is really a definition, rather than a common symbol.  This could
     lead to some minor confusion when the symbol really is a common
     symbol in some regular object.  However, I think it will be
     harmless.  */

  if (newdyn
      && newdef
      && (sec->flags & SEC_ALLOC) != 0
      && (sec->flags & SEC_LOAD) == 0
#ifdef USE_SIZE
      && sym->st_size > 0
#endif
      && !new_is_weak
      && !ISFCN(sym->n_type))
    newdyncommon = true;
  else
    newdyncommon = false;

  if (olddyn
      && olddef
      && h->root.type == bfd_link_hash_defined
      && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) != 0
      && (h->root.u.def.section->flags & SEC_ALLOC) != 0
      && (h->root.u.def.section->flags & SEC_LOAD) == 0
#ifdef USE_SIZE
      && h->size > 0
#endif
      && !ISFCN(h->type))
    olddyncommon = true;
  else
    olddyncommon = false;

  /* It's OK to change the type if either the existing symbol or the
     new symbol is weak.  */

  if (old_is_weak_def
      || h->root.type == bfd_link_hash_undefweak
      || new_is_weak)
    *type_change_ok = true;

  /* It's OK to change the size if either the existing symbol or the
     new symbol is weak, or if the old symbol is undefined.  */

  if (*type_change_ok
      || h->root.type == bfd_link_hash_undefined)
    *size_change_ok = true;

#ifdef USE_SIZE
  /* If both the old and the new symbols look like common symbols in a
     dynamic object, set the size of the symbol to the larger of the
     two.  */

  if (olddyncommon
      && newdyncommon
      && sym->st_size != h->size)
    {
      /* Since we think we have two common symbols, issue a multiple
         common warning if desired.  Note that we only warn if the
         size is different.  If the size is the same, we simply let
         the old symbol override the new one as normally happens with
         symbols defined in dynamic objects.  */

      if (! ((*info->callbacks->multiple_common)
	     (info, h->root.root.string, oldbfd, bfd_link_hash_common,
	      h->size, abfd, bfd_link_hash_common, sym->st_size)))
	return false;

      if (sym->st_size > h->size)
	h->size = sym->st_size;

      *size_change_ok = true;
    }
#endif

  /* If we are looking at a dynamic object, and we have found a
     definition, we need to see if the symbol was already defined by
     some other object.  If so, we want to use the existing
     definition, and we do not want to report a multiple symbol
     definition error; we do this by clobbering *PSEC to be
     bfd_und_section_ptr.

     We treat a common symbol as a definition if the symbol in the
     shared library is a function, since common symbols always
     represent variables; this can cause confusion in principle, but
     any such confusion would seem to indicate an erroneous program or
     shared library.  We also permit a common symbol in a regular
     object to override a weak symbol in a shared object.

     We prefer a non-weak definition in a shared library to a weak
     definition in the executable.  */

  if (newdyn
      && newdef
      && (olddef
	  || (h->root.type == bfd_link_hash_common
	      && (new_is_weak
                  || ISFCN(sym->n_type))))
      && (!old_is_weak_def
	  || new_is_weak))
    {
      *override = true;
      newdef = false;
      newdyncommon = false;

      *psec = sec = bfd_und_section_ptr;
      *size_change_ok = true;

      /* If we get here when the old symbol is a common symbol, then
         we are explicitly letting it override a weak symbol or
         function in a dynamic object, and we don't want to warn about
         a type change.  If the old symbol is a defined symbol, a type
         change warning may still be appropriate.  */

      if (h->root.type == bfd_link_hash_common)
	*type_change_ok = true;
    }

  /* Handle the special case of an old common symbol merging with a
     new symbol which looks like a common symbol in a shared object.
     We change *PSEC and *PVALUE to make the new symbol look like a
     common symbol, and let _bfd_generic_link_add_one_symbol will do
     the right thing.  */

  if (newdyncommon
      && h->root.type == bfd_link_hash_common)
    {
      *override = true;
      newdef = false;
      newdyncommon = false;
#ifdef USE_SIZE
      *pvalue = sym->st_size;
#endif
      *psec = sec = bfd_com_section_ptr;
      *size_change_ok = true;
    }

  /* If the old symbol is from a dynamic object, and the new symbol is
     a definition which is not from a dynamic object, then the new
     symbol overrides the old symbol.  Symbols from regular files
     always take precedence over symbols from dynamic objects, even if
     they are defined after the dynamic object in the link.

     As above, we again permit a common symbol in a regular object to
     override a definition in a shared object if the shared object
     symbol is a function or is weak.

     As above, we permit a non-weak definition in a shared object to
     override a weak definition in a regular object. 

     Elf doesn't permit a strong symbol which follows a weak definition
     to override the weak definition if shared libs are not involved.
     We do.  */

  if (! newdyn
      && (newdef
	  || (bfd_is_com_section (sec)
	      && (old_is_weak_def
                  || ISFCN(h->type))))
      && olddyn
      && olddef
      && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) != 0
      && !new_is_weak)
    {
      /* Change the hash table entry to undefined, and let
	 _bfd_generic_link_add_one_symbol do the right thing with the
	 new definition.  */

      h->root.type = bfd_link_hash_undefined;
      /* grub the owner (of the target of the alias, really) out
	 of the alias link.  (Or here, if not an indirect!) */
      if (h_real->root.type == bfd_link_hash_undefined)
	h->root.u.undef.abfd = h_real->root.u.undef.abfd;
      else
	h->root.u.undef.abfd = h_real->root.u.def.section->owner;

      /* To make this work we have to frob the flags so that the rest
         of the code does not think we are using the prior type
	 of definition (regular or dynamic).  Elf doesn't care, but
	 the relocation counting code is very touchy about this. */
      if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0)
	h->coff_link_hash_flags |= COFF_LINK_HASH_REF_REGULAR;
      else if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) != 0)
	h->coff_link_hash_flags |= COFF_LINK_HASH_REF_DYNAMIC;
      h->coff_link_hash_flags &= ~ (COFF_LINK_HASH_DEF_REGULAR
				   | COFF_LINK_HASH_DEF_DYNAMIC);
      *size_change_ok = true;

      olddef = false;
      olddyncommon = false;

      /* We again permit a type change when a common symbol may be
         overriding a function.  */

      if (bfd_is_com_section (sec))
	*type_change_ok = true;

      /* This union may have been set to be non-NULL when this symbol
	 was seen in a dynamic object.  We must force the union to be
	 NULL, so that it is correct for a regular symbol.  */

      h->verinfo.vertree = NULL;

      /* In this special case, if H is the target of an indirection,
         we want the caller to frob with H rather than with the
         indirect symbol.  That will permit the caller to redefine the
         target of the indirection, rather than the indirect symbol
         itself.  FIXME: This will break the -y option if we store a
         symbol with a different name.  */
      *sym_hash = h;
    }

  /* Handle the special case of a new common symbol merging with an
     old symbol that looks like it might be a common symbol defined in
     a shared object.  Note that we have already handled the case in
     which a new common symbol should simply override the definition
     in the shared library.  */

  if (! newdyn
      && bfd_is_com_section (sec)
      && olddyncommon)
    {
      /* It would be best if we could set the hash table entry to a
	 common symbol, but we don't know what to use for the section
	 or the alignment.  */
      if (! ((*info->callbacks->multiple_common)
	     (info, h->root.root.string, oldbfd, bfd_link_hash_common,
	      h->root.u.c.size, abfd, bfd_link_hash_common, sym->n_value)))
	return false;

#ifdef USE_SIZE
      /* If the presumed common symbol in the dynamic object is
         larger, pretend that the new symbol has its size.  */

      if (h->size > *pvalue)
	*pvalue = h->size;
#endif

      /* FIXME: We no longer know the alignment required by the symbol
	 in the dynamic object, so we just wind up using the one from
	 the regular object.  */

      olddef = false;
      olddyncommon = false;

      h->root.type = bfd_link_hash_undefined;
      h->root.u.undef.abfd = h->root.u.def.section->owner;

      *size_change_ok = true;
      *type_change_ok = true;

      h->verinfo.vertree = NULL;
    }

  /* Handle the special case of a weak definition in a regular object
     followed by a non-weak definition in a shared object.  In this
     case, we prefer the definition in the shared object.  */
  if (olddef
      && old_is_weak_def
      && newdef
      && newdyn
      && !new_is_weak)
    {
      /* To make this work we have to frob the flags so that the rest
         of the code does not think we are using the regular
         definition.  */
      if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0)
	h->coff_link_hash_flags |= COFF_LINK_HASH_REF_REGULAR;
      else if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) != 0)
	h->coff_link_hash_flags |= COFF_LINK_HASH_REF_DYNAMIC;
      h->coff_link_hash_flags &= ~ (COFF_LINK_HASH_DEF_REGULAR
				   | COFF_LINK_HASH_DEF_DYNAMIC);

      /* If H is the target of an indirection, we want the caller to
         use H rather than the indirect symbol.  Otherwise if we are
         defining a new indirect symbol we will wind up attaching it
         to the entry we are overriding.  */
      *sym_hash = h;
    }

  /* Handle the special case of a non-weak definition in a shared
     object followed by a weak definition in a regular object.  In
     this case we prefer to definition in the shared object.  To make
     this work we have to tell the caller to not treat the new symbol
     as a definition.  */
  if (olddef
      && olddyn
      && ! old_is_weak_def
      && newdef
      && ! newdyn
      && new_is_weak)
    *override = true;

  return true;
}

/* Add symbols from a COFF object file.  */

static boolean
coff_link_add_object_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  if (! _bfd_coff_get_external_symbols (abfd))
    return false;
  if (! coff_link_add_symbols (abfd, info))
    return false;

  if (! info->keep_memory)
    {
      if (! _bfd_coff_free_symbols (abfd))
	return false;
    }
  return true;
}

/* Check a single archive element to see if we need to include it in
   the link.  *PNEEDED is set according to whether this element is
   needed in the link or not.  This is called via
   _bfd_generic_link_add_archive_symbols.  */

static boolean
coff_link_check_archive_element (abfd, info, pneeded)
     bfd *abfd;
     struct bfd_link_info *info;
     boolean *pneeded;
{
  if (! _bfd_coff_get_external_symbols (abfd))
    return false;

  if (! coff_link_check_ar_symbols (abfd, info, pneeded))
    return false;

  if (*pneeded)
    {
      if (! coff_link_add_symbols (abfd, info))
	return false;
    }

  if (! info->keep_memory || ! *pneeded)
    {
      if (! _bfd_coff_free_symbols (abfd))
	return false;
    }

  return true;
}

/* Look through the symbols to see if this object file should be
   included in the link.  */

static boolean
coff_link_check_ar_symbols (abfd, info, pneeded)
     bfd *abfd;
     struct bfd_link_info *info;
     boolean *pneeded;
{
  bfd_size_type symesz;
  bfd_byte *esym;
  bfd_byte *esym_end;

  *pneeded = false;

  symesz = bfd_coff_symesz (abfd);
  esym = (bfd_byte *) obj_coff_external_syms (abfd);
  esym_end = esym + obj_raw_syment_count (abfd) * symesz;
  while (esym < esym_end)
    {
      struct internal_syment sym;
      enum coff_symbol_classification classification;

      bfd_coff_swap_sym_in (abfd, (PTR) esym, (PTR) &sym);

      classification = bfd_coff_classify_symbol (abfd, &sym);
      if (classification == COFF_SYMBOL_GLOBAL
	  || classification == COFF_SYMBOL_COMMON)
	{
	  const char *name;
	  char buf[SYMNMLEN + 1];
	  struct bfd_link_hash_entry *h;

	  /* This symbol is externally visible, and is defined by this
             object file.  */

	  name = _bfd_coff_internal_syment_name (abfd, &sym, buf);
	  if (name == NULL)
	    return false;
	  h = bfd_link_hash_lookup (info->hash, name, false, false, true);

	  /* auto import */
	  if (!h && info->pei386_auto_import)
	    {
	      if (!strncmp (name,"__imp_", 6))
		{
		  h =
                    bfd_link_hash_lookup (info->hash, name + 6, false, false,
                                          true);
		}
	    }
	  /* We are only interested in symbols that are currently
	     undefined.  If a symbol is currently known to be common,
	     COFF linkers do not bring in an object file which defines
	     it.  */
	  if (h != (struct bfd_link_hash_entry *) NULL
	      && h->type == bfd_link_hash_undefined)
	    {
	      if (! (*info->callbacks->add_archive_element) (info, abfd, name))
		return false;
	      *pneeded = true;
	      return true;
	    }
	}

      esym += (sym.n_numaux + 1) * symesz;
    }

  /* We do not need this object file.  */
  return true;
}

#ifdef DYNAMIC_LINKING
static boolean coff_link_create_dynamic_sections
  PARAMS ((bfd *abfd, struct bfd_link_info *info));
#endif

/* Add all the symbols from an object file to the hash table.  */

static boolean
coff_link_add_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  unsigned int n_tmask = coff_data (abfd)->local_n_tmask;
  unsigned int n_btshft = coff_data (abfd)->local_n_btshft;
  unsigned int n_btmask = coff_data (abfd)->local_n_btmask;
  boolean keep_syms;
  boolean default_copy;
  bfd_size_type symcount;
  struct coff_link_hash_entry **sym_hash;
  bfd_size_type symesz;
  bfd_byte *esym;
  bfd_byte *esym_end;
  bfd_size_type amt;
  long sym_idx;
  boolean size_change_ok, type_change_ok;
#ifdef DYNAMIC_LINKING
  bfd_byte *buf = NULL; /* For housekeeping purposes */
  boolean dynamic;
  boolean using_dynsymtab;
  asection *dynsym = NULL;
  char *dynstrings = NULL;
  coff_external_versym *extversym = NULL;
  coff_external_versym *ever;

  coff_external_dyn *dynbuf = NULL;
#ifdef USE_DLLS
  boolean is_DLL_module;
#endif
#endif

  /* Keep the symbols during this function, in case the linker needs
     to read the generic symbols in order to report an error message.  */
  keep_syms = obj_coff_keep_syms (abfd);
  obj_coff_keep_syms (abfd) = true;

  if (info->keep_memory)
    default_copy = false;
  else
    default_copy = true;

#ifdef DYNAMIC_LINKING /* [ */
#ifdef USE_DLLS

  /* Properly this belongs in the archive reading code, but since this
     is the only place that uses it, I'll let locality dominate. */
  if (bfd_my_archive(abfd) != NULL)
    {
       struct dll_info {
	  boolean is_DLL;
       };
#define ar_dllinfo(abfd) ((struct dll_info *)bfd_my_archive(abfd)->tdata.aout_ar_data->tdata)

       if (ar_dllinfo(abfd) == NULL)
	 {
            (bfd_my_archive(abfd)->tdata.aout_ar_data->tdata)
	       = bfd_alloc(abfd, sizeof(struct dll_info));
	    if (bfd_my_archive(abfd)->tdata.aout_ar_data->tdata == NULL)
	       return false;
            ar_dllinfo(abfd)->is_DLL =
                     bfd_get_section_by_name (abfd, ".idata$6") != NULL
                  || bfd_get_section_by_name (abfd, ".idata$2") != NULL
                  || bfd_get_section_by_name (abfd, ".idata$3") != NULL;
	 }
       is_DLL_module = ar_dllinfo(abfd)->is_DLL;
    }
  else
       is_DLL_module = false;

#endif

  if ((abfd->flags & DYNAMIC) == 0)
    dynamic = false;
  else
    {
      dynamic = true;

      if ((abfd->flags & NO_LINK) != 0)
	{
	  (*_bfd_error_handler)
		(_("%s: Linking with this file is prohibited.\n"), abfd->filename);
	  bfd_set_error (bfd_error_invalid_operation);
	  goto error_return;
	}
      /* You can't use -r against a dynamic object.  Also, there's no
	 hope of using a dynamic object which does not exactly match
	 the format of the output file.  */
      /* A PE shared executable is PEI, so this works for PE. */
      if (info->relocateable || info->hash->creator != abfd->xvec)
	{
	  bfd_set_error (bfd_error_invalid_operation);
	  goto error_return;
	}
    }

  /* As a GNU extension, any input sections which are named
     .gnu.warning.SYMBOL are treated as warning symbols for the given
     symbol.  This differs from .gnu.warning sections, which generate
     warnings when they are included in an output file.  */
  if (! info->shared)
    {
      asection *s;

      for (s = abfd->sections; s != NULL; s = s->next)
	{
	  const char *name;

	  name = bfd_get_section_name (abfd, s);
	  if (strncmp (name, ".gnu.warning.", sizeof ".gnu.warning." - 1) == 0)
	    {
	      char *msg;
	      bfd_size_type sz;

	      name += sizeof ".gnu.warning." - 1;

	      /* If this is a shared object, then look up the symbol
		 in the hash table.  If it is there, and it is already
		 been defined, then we will not be using the entry
		 from this shared object, so we don't need to warn.
		 FIXME: If we see the definition in a regular object
		 later on, we will warn, but we shouldn't.  The only
		 fix is to keep track of what warnings we are supposed
		 to emit, and then handle them all at the end of the
		 link.  */
	      if (dynamic
		  && (abfd->xvec == info->hash->creator
		     || abfd->xvec == info->hash->creator->input_format))
		{
		  struct coff_link_hash_entry *h;

		  h = coff_link_hash_lookup (coff_hash_table (info), name,
					    false, false, true);

		  /* FIXME: What about bfd_link_hash_common?  */
		  if (h != NULL
		      && (h->root.type == bfd_link_hash_defined
			  || h->root.type == bfd_link_hash_defweak))
		    {
		      /* We don't want to issue this warning.  Clobber
                         the section size so that the warning does not
                         get copied into the output file.  */
		      s->_raw_size = 0;
		      continue;
		    }
		}

	      sz = bfd_section_size (abfd, s);
	      msg = (char *) bfd_alloc (abfd, sz);
	      if (msg == NULL)
		goto error_return;

	      if (! bfd_get_section_contents (abfd, s, msg, (file_ptr) 0, sz))
		goto error_return;

	      if (! (bfd_coff_link_add_one_symbol
		     (info, abfd, name, BSF_WARNING, s, (bfd_vma) 0, msg,
		      false, false,
		      (struct bfd_link_hash_entry **) NULL)))
		goto error_return;

	      if (! info->relocateable)
		{
		  /* Clobber the section size so that the warning does
                     not get copied into the output file.  */
		  s->_raw_size = 0;
		}
	    }
	}
    }


  if (dynamic)
    {
      /* Read in any version definitions.  */

      /* Elf now uses a slurp... we could if we needed to. */
      if (coff_dynverdef (abfd) != 0)
	{
	  asection *verdefhdr;
	  bfd_byte *dynver;
	  unsigned int i;
	  const coff_external_verdef *extverdef;
	  coff_internal_verdef *intverdef;

	  /* allocate space for the internals */
	  verdefhdr = coff_dynverdef(abfd);
	  dyn_data (abfd)->verdef =
	    ((coff_internal_verdef *)
	     bfd_zalloc (abfd,
			 verdefhdr->info_r * sizeof (coff_internal_verdef)));
	  if (dyn_data (abfd)->verdef == NULL)
	    goto error_return;

	  /* make temp space for the externals */
	  dynver = (bfd_byte *) bfd_malloc (bfd_section_size(abfd, verdefhdr));
	  if (dynver == NULL)
	    goto error_return;

          if (!bfd_get_section_contents (abfd, verdefhdr, dynver, 
	      (file_ptr) 0, bfd_section_size(abfd, verdefhdr)))
	    goto error_return;

	  extverdef = (const coff_external_verdef *) dynver;
	  intverdef = dyn_data (abfd)->verdef;
	  for (i = 0; i < verdefhdr->info_r; i++, intverdef++)
	    {
	      const coff_external_verdaux *extverdaux;
	      coff_internal_verdaux intverdaux;

	      bfd_coff_swap_verdef_in (abfd, extverdef, intverdef);

	      /* Pick up the name of the version.  */
	      extverdaux = ((const coff_external_verdaux *)
			    ((bfd_byte *) extverdef + intverdef->vd_aux));
	      bfd_coff_swap_verdaux_in (abfd, extverdaux, &intverdaux);

	      intverdef->vd_bfd = abfd;
	      intverdef->vd_nodename =
		bfd_coff_string_from_coff_section (abfd, verdefhdr->link_index,
						 intverdaux.vda_name);

	      extverdef = ((const coff_external_verdef *)
			   ((bfd_byte *) extverdef + intverdef->vd_next));
	    }

	  free (dynver);
	  dynver = NULL;
	}

      /* Read in the symbol versions, but don't bother to convert them
         to internal format.  */
      if (coff_dynversym (abfd) != 0)
	{
	  asection *versymhdr;

	  versymhdr = coff_dynversym(abfd);
	  extversym = (coff_external_versym *) 
	      bfd_malloc (bfd_section_size(abfd, versymhdr));
	  if (extversym == NULL)
	    goto error_return;
          if (!bfd_get_section_contents (abfd, versymhdr, extversym, 
	      (file_ptr) 0, bfd_section_size(abfd, versymhdr)))
	    goto error_return;
	}
    }

  if (dynamic)
      symcount = coff_get_dynamic_symtab_upper_bound (abfd);
    else
#endif /* ] */
      symcount = obj_raw_syment_count (abfd);

  /* We keep a list of the linker hash table entries that correspond
     to particular symbols.  */
  amt = symcount * sizeof (struct coff_link_hash_entry *);
  sym_hash = (struct coff_link_hash_entry **) bfd_zalloc (abfd, amt);
  if (sym_hash == NULL && symcount != 0)
    goto error_return;
  obj_coff_sym_hashes (abfd) = sym_hash;

  symesz = bfd_coff_symesz (abfd);
  BFD_ASSERT (symesz == bfd_coff_auxesz (abfd));

#ifdef DYNAMIC_LINKING /* [ */
  /* If this is a dynamic object, we always link against the .dynsym
     symbol table, not the main symbol table.  The dynamic linker
     will only see the .dynsym symbol table, so there is no reason to
     look at the main one for a dynamic object. 

     This must be done at this point, because if this is the first
     bfd which is dynamic, the code below will "recycle" it to use
     it to hold the output dynamic sections, after having tossed
     the input sections (which, rightly, should not participate
     in the link.)  */

  if (dynamic) 
      dynsym = coff_dynsymtab(abfd);
  if (! dynamic || dynsym == NULL)
    {
      esym = (bfd_byte *) obj_coff_external_syms (abfd);
      esym_end = esym + symcount * symesz;
      using_dynsymtab = false;
      sym_idx=0;
    }
  else
    {
      int size;

      /* We have to get the actual count from the header, rather than dealing
	 with the size, because we don't have an "actual" size, but rather
	 a rounded one. */
      symcount = dynsym->info_r;

      if (symcount == 0)
	 goto error_return;

      size = symcount * symesz;
      buf = esym = (bfd_byte *) bfd_malloc (size);
      if (esym == NULL)
	 goto error_return;
      
      if (! bfd_get_section_contents (abfd, dynsym, 
				      (bfd_byte *)esym, (file_ptr) 0, size))
	 goto error_return;

      esym_end = esym + symcount * symesz;

      /* Skip the first (junk) entry */
      esym = esym + symesz;
      /* But the hash table has to match. */
      sym_hash++;

      dynstrings = bfd_coff_get_str_section (abfd, dynsym->link_index);

      using_dynsymtab = true;
      sym_idx=1;
    }
#else /* ][ */
  esym = (bfd_byte *) obj_coff_external_syms (abfd);
  esym_end = esym + symcount * symesz;
  sym_idx=0;
#endif /* ] */


#ifdef DYNAMIC_LINKING /* [ */
  if (! dynamic)
    {
      /* If we are creating a shared library, create all the dynamic
         sections immediately.  We need to attach them to something,
         so we attach them to this BFD, provided it is the right
         format.  FIXME: If there are no input BFD's of the same
         format as the output, we can't make a shared library.  */
      if (info->shared
	  && ! coff_hash_table (info)->dynamic_sections_created
	  && (abfd->xvec == info->hash->creator
	     || abfd->xvec == info->hash->creator->input_format)
	  )
	{
	  if (! coff_link_create_dynamic_sections (abfd, info))
	    goto error_return;
	}
    }
  else
    {
      asection *s;
      boolean add_needed;
      const char *name;
      bfd_size_type oldsize;
      bfd_size_type strindex;

      /* Find the name to use in a DT_NEEDED entry that refers to this
	 object.  If the object has a DT_SONAME entry, we use it.
	 Otherwise, if the generic linker stuck something in
	 coff_dt_name, we use that.  Otherwise, we just use the file
	 name.  If the generic linker put a null string into
	 coff_dt_name, we don't make a DT_NEEDED entry at all, even if
	 there is a DT_SONAME entry.  */
      add_needed = true;
      name = bfd_get_filename (abfd);
      if (coff_dt_name (abfd) != NULL)
	{
	  name = coff_dt_name (abfd);
	  if (*name == '\0')
	    add_needed = false;
	}

      /* Now get the .dynamic section from this file and merge it into ours. */
      s = coff_dynamic(abfd);
      if (s != NULL)
	{
	  coff_external_dyn *extdyn;
	  coff_external_dyn *extdynend;
	  unsigned long link;

	  dynbuf = (coff_external_dyn *) bfd_malloc ((size_t) s->_raw_size);
	  if (dynbuf == NULL)
	    goto error_return;

	  if (! bfd_get_section_contents (abfd, s, (PTR) dynbuf,
					  (file_ptr) 0, s->_raw_size))
	    goto error_return;

	  link = s->link_index;

	  extdyn = dynbuf;
	  extdynend = extdyn + s->_raw_size / sizeof (coff_external_dyn);
	  for (; extdyn < extdynend; extdyn++)
	    {
	      coff_internal_dyn dyn;

	      bfd_coff_swap_dyn_in (abfd, extdyn, &dyn);
	      if (dyn.d_tag == DT_SONAME)
		{
		  name = bfd_coff_string_from_coff_section (abfd, link,
							  dyn.d_un.d_val);
		  if (name == NULL)
		    goto error_return;
		}
	      if (dyn.d_tag == DT_NEEDED)
		{
		  struct bfd_link_needed_list *n, **pn;
		  char *fnm, *anm;

		  n = ((struct bfd_link_needed_list *)
		       bfd_alloc (abfd, sizeof (struct bfd_link_needed_list)));
		  fnm = bfd_coff_string_from_coff_section (abfd, link,
							 dyn.d_un.d_val);
		  if (n == NULL || fnm == NULL)
		    goto error_return;
		  anm = bfd_alloc (abfd, strlen (fnm) + 1);
		  if (anm == NULL)
		    goto error_return;
		  strcpy (anm, fnm);
		  n->name = anm;
		  n->by = abfd;
		  n->next = NULL;
		  for (pn = &coff_hash_table (info)->needed;
		       *pn != NULL;
		       pn = &(*pn)->next)
		    ;
		  *pn = n;
		}
	    }

	  free (dynbuf);
	  dynbuf = NULL;
	}

      /* If this is the first dynamic object found in the link, create
	 the special sections required for dynamic linking.  */
      if (! coff_hash_table (info)->dynamic_sections_created)
	{
	  if (! coff_link_create_dynamic_sections (abfd, info))
	    goto error_return;
	}

      if (add_needed)
	{
	  /* Add a DT_NEEDED entry for this dynamic object.  */
	  oldsize = _bfd_stringtab_size (coff_hash_table (info)->dynstr);
	  strindex = _bfd_stringtab_add (coff_hash_table (info)->dynstr, name,
					 true, false);
	  if (strindex == (bfd_size_type) -1)
	    goto error_return;

	  if (oldsize == _bfd_stringtab_size (coff_hash_table (info)->dynstr))
	    {
	      asection *sdyn;
	      coff_external_dyn *dyncon, *dynconend;

	      /* The hash table size did not change, which means that
		 the dynamic object name was already entered.  If we
		 have already included this dynamic object in the
		 link, just ignore it.  There is no reason to include
		 a particular dynamic object more than once.  */
	      sdyn = coff_hash_table (info)->dynamic;
	      BFD_ASSERT (sdyn != NULL);

	      dyncon = (coff_external_dyn *) sdyn->contents;
	      dynconend = (coff_external_dyn *) (sdyn->contents +
						sdyn->_raw_size);
	      for (; dyncon < dynconend; dyncon++)
		{
		  coff_internal_dyn dyn;

		  bfd_coff_swap_dyn_in (coff_hash_table (info)->dynobj, dyncon,
				   &dyn);
		  if (dyn.d_tag == DT_NEEDED
		      && dyn.d_un.d_val == strindex)
		    {
		      if (buf != NULL)
			free (buf);
		      if (extversym != NULL)
			free (extversym);

		      /* We do not want to include any of the sections in a
			 dynamic object in the output file.  We hack by simply
			 clobbering the list of sections in the BFD.  This could
			 be handled more cleanly by, say, a new section flag;
			 the existing SEC_NEVER_LOAD flag is not the one we
			 want, because that one still implies that the section
			 takes up space in the output file.  We do this after
			 having extracted the section information from the
			 symbol table.  Because we need the section information
			 until after the symbols are examined, we do this here
			 and after they are examined */
		      abfd->sections = NULL;
		      abfd->section_count = 0;

		      return true;
		    }
		}
	    }

	  if (! coff_add_dynamic_entry (info, DT_NEEDED, strindex))
	    goto error_return;
	}

      /* Save the SONAME, if there is one, because sometimes the
         linker emulation code will need to know it.  */
      if (*name == '\0')
	name = bfd_get_filename (abfd);
      coff_dt_name (abfd) = name;
    }

#ifdef USE_WEAK
    weaks = NULL;
#endif

    ever = extversym;
#endif /* ] */

  while (esym < esym_end)
    {
      struct internal_syment sym;
      enum coff_symbol_classification classification;
      boolean copy = false;
#ifdef DYNAMIC_LINKING
      boolean definition;
      struct coff_link_hash_entry *h = NULL;
      struct coff_link_hash_entry *h_real;

      if (ever != NULL) ever++;
#endif

      bfd_coff_swap_sym_in (abfd, (PTR) esym, (PTR) &sym);

#ifdef DYNAMIC_LINKING
      /* A symbol coming from a .so can be one of three things:
	 A definition (in that .so).
	 An (as yet) unsatisfied symbol 
	 A reference from that .so to some other .so it was linked against.

	 The latter two both look like undefined symbols at this point.

	 The third class is transformed into the first (or second) because the 
	 generic linker looks thru the DT_NEEDED entries of all .so's and 
	 calls this routine for all the libraries thus mentioned.
       */
#endif
      classification = bfd_coff_classify_symbol (abfd, &sym);
      if (classification != COFF_SYMBOL_LOCAL)
	{
	  const char *name;
	  char buf[SYMNMLEN + 4];  /* +1 +3 for possible sequence# */
	  flagword flags;
	  asection *section;
	  bfd_vma value;
	  boolean addit;
	  const char *alias_target_name = NULL;

	  /* This symbol is externally visible.  */

#ifdef DYNAMIC_LINKING
	  if (using_dynsymtab)
	    {
	      if (sym._n._n_n._n_zeroes != 0
		  || sym._n._n_n._n_offset == 0)
		{
		   memcpy (buf, sym._n._n_name, SYMNMLEN);
		   buf[SYMNMLEN] = '\0';
		   name=buf;
		   copy=true;
		}
	      else
		{
		   name = dynstrings + sym._n._n_n._n_offset;
		}
	    }
	  else
#endif
            { // NOTE: more delayed indentation fix
	  name = _bfd_coff_internal_syment_name (abfd, &sym, buf);
	  if (name == NULL)
	    goto error_return;

	  /* We must copy the name into memory if we got it from the
             syment itself, rather than the string table.  */
	  copy = default_copy;
	  if (sym._n._n_n._n_zeroes != 0
	      || sym._n._n_n._n_offset == 0)
	    copy = true;
	    } // END DELAY.

	  value = sym.n_value;
 
	  /* This is horrible, but I can't think of a better way to do it.

	     For PE/PEI format stuff, LIB.EXE-generated .LIB files
	     for DLLs use the symbols .idata$*.  (see ld/emultempl/pe.em).
	     All such .LIB files use the same symbols in the same
	     way.  However, the .idata$4 and .idata$5 symbols behave
	     as if they had a scope of exactly that archive!  (That
	     is, .idata$5 symbols from one .LIB are not related to
	     those from another.)  The other .idata$* symbols have
	     that characteristic to some degree, but in particular,
	     .idata$2 does NOT!  (Specifically, the archive member
	     which defines the import directory table for a given
	     .LIB wants it's reference to .idata$5 (the "Thunk table")
	     to refer to a list of .idata$5 entries ONLY from that one
	     library (similarly, .idata$4.) .   However, .idata$2 itself
	     wants to be collected with the other .idata$2 sections.)

	     To accomplish this in the context of ld, the easiest way seems
	     to be to mangle the names of .idata$[45] symbols to include
	     an archive indicator.  We do that here.

	     Because the section symbols .idata$4 and .idata$5 are also
	     required, we don't mangle the symbols in the very first
	     library.  That will come out first, and all will be well.

	     (Note that the first instance of .idata$2, .idata$4 and
	     .idata$5 in each LIB.EXE built archive is storage class 104
	     (C_SECTION).  Because we don't neccessarily see the archive
	     in order I wasn't able to figure out a way to use that
	     information.  However, it might be a way around this mess.)

	     (Putting this in _bfd_coff_internal_syment_name has nasty
	     side-effects.)  */

	  if (name[0]=='.' && name[1] == 'i' &&
	      strncmp(name, ".idata$",7)==0 && (name[7]=='4'||name[7]=='5'))
	    {
	      static bfd* this_archive = NULL;
	      static int arch_sequence = -1;
	      /* we're assuming we handle all of each archive before going
		 on to the next, and that if we search the same .a file twice,
		 we treat them separately.  (The later (probably) won't work
		 at runtime, but because DLL libs are always all leaf routines,
		 we should never search one twice to begin with.) */
	      if (this_archive != abfd->my_archive)
		{
		 this_archive = abfd->my_archive;
		 arch_sequence++;
		}
	      if (arch_sequence>0)
		{
		 /* in case it's in the string table, put it into the buffer */
		 if (name != &buf[0])
		   {
		     strcpy (buf, name);
		     name = &buf[0];
		     copy = true;
		   }
		   buf[8]=0x7f;
		   /* we'll try to put out a decimal number, but if it
		      overflows in the left digit, there's no loss */
		   buf[9]=arch_sequence/10+'0';
		   buf[10]=arch_sequence%10+'0';
		   buf[11]=0;
		}
	    }
 
	  switch (classification)
	    {
	    default:
	      abort ();

	    case COFF_SYMBOL_GLOBAL:
	      flags = BSF_EXPORT | BSF_GLOBAL;
	      section = coff_section_from_bfd_index (abfd, sym.n_scnum);
	      value -= section->vma;
	      break;

	    case COFF_SYMBOL_UNDEFINED:
	      flags = 0;
	      section = bfd_und_section_ptr;
	      break;

	    case COFF_SYMBOL_COMMON:
	      flags = BSF_GLOBAL;
	      section = bfd_com_section_ptr;
	      break;

	    case COFF_SYMBOL_PE_SECTION:
	      flags = BSF_SECTION_SYM | BSF_GLOBAL;
	      section = coff_section_from_bfd_index (abfd, sym.n_scnum);
	      break;
	    }

#ifdef DYNAMIC_LINKING /* [ */
	  definition = (classification == COFF_SYMBOL_GLOBAL);
	  /* Common is not a definition (in this sense); if defined in a 
	     shared lib it's become an ordinary symbol.  Defined ordinary
	     symbol plus common has the ordinary symbol dominate anyway,
	     which is generally what we want.

	     USE_COPY_RELOC may affect this.  */

#ifdef USE_SIZE
	  size_change_ok = false;
	  /* type_chang_ok is a kluge left over from ELF where certain
	     machines misrecorded types; that seems more likely in COFF,
	     so that is left as a guide. */
	  type_change_ok = get_elf_backend_data (abfd)->type_change_ok;
#else
	  type_change_ok = false;
	  size_change_ok = false;
#endif
#endif /* ] */
	  if (info->hash->creator->flavour == bfd_target_coff_flavour)
	    {
	      boolean override;
#ifdef DYNAMIC_LINKING /* [ */
	      coff_internal_versym iver;
	      unsigned int vernum = -1;

	      if (ever != NULL)
		{
		  bfd_coff_swap_versym_in (abfd, ever, &iver);
		  vernum = iver.vs_vers & VERSYM_VERSION;

		  /* If this is a hidden symbol, or if it is not version
		     1, we append the version name to the symbol name.
		     However, we do not modify a non-hidden absolute
		     symbol, because it might be the version symbol
		     itself.  FIXME: What if it isn't?  */
		  if ((iver.vs_vers & VERSYM_HIDDEN) != 0
		      || (vernum > 1 && ! bfd_is_abs_section (section)))
		    {
		      const char *verstr;
		      int namelen, newlen;
		      char *newname, *p;

		      if (sym.n_scnum != N_UNDEF)
			{
			  if (vernum > dyn_data (abfd)->dynverdef->info_r)
			    {
			      (*_bfd_error_handler)
				(_("%s: %s: invalid version %d (max %d)"),
				 abfd->filename, name, vernum,
				 dyn_data (abfd)->dynverdef->info_r);
			      bfd_set_error (bfd_error_bad_value);
			      goto error_return;
			    }
			  else if (vernum > 1)
			    verstr = 
			       dyn_data (abfd)->verdef[vernum - 1].vd_nodename;
			  else
			    verstr = "";
			}
		      else
			{
			  /* We cannot simply test for the number of
			     entries in the VERNEED section since the
			     numbers for the needed versions do not start
			     at 0.  */
			  coff_internal_verneed *t;

			  verstr = NULL;
			  for (t = dyn_data (abfd)->verref;
			       t != NULL;
			       t = t->vn_nextref)
			    {
			      coff_internal_vernaux *a;

			      for (a = t->vn_auxptr; 
				   a != NULL; 
				   a = a->vna_nextptr)
				{
				  if (a->vna_other == vernum)
				    {
				      verstr = a->vna_nodename;
				      break;
				    }
				}
			      if (a != NULL)
				break;
			    }
			  if (verstr == NULL)
			    {
			      (*_bfd_error_handler)
				(_("%s: %s: invalid needed version %d"),
				 bfd_get_filename (abfd), name, vernum);
			      bfd_set_error (bfd_error_bad_value);
			      goto error_return;
			    }
			}

		      namelen = strlen (name);
		      newlen = namelen + strlen (verstr) + 2;
		      if ((iver.vs_vers & VERSYM_HIDDEN) == 0)
			++newlen;

		      newname = (char *) bfd_alloc (abfd, newlen);
		      if (newname == NULL)
			goto error_return;
		      strcpy (newname, name);
		      p = newname + namelen;
		      *p++ = COFF_VER_CHR;
		      /* If this is a defined non-hidden version symbol,
			 we add another @ to the name.  This indicates the
			 default version of the symbol.  */
		      if ((iver.vs_vers & VERSYM_HIDDEN) == 0
                          && sym.n_scnum != N_UNDEF)
			*p++ = COFF_VER_CHR;
		      strcpy (p, verstr);

		      name = newname;
		    }
		}

#endif /* ] */
	      /* PE weak symbols are aliases... we need to get the aliased
		 real symbol. */
	      if (obj_pe(abfd) && sym.n_sclass == C_NT_WEAK)
		{
		  /* It's a PE weak symbol (type IMAGE_WEAK_EXTERN_SEARCH_ALIAS)
		     That's implemented as an indirect in our parlance. 
		     Find the aux entry, get the referenced symbol, and insert
		     its name.  */

		  int target_idx;
		  union internal_auxent auxent;

		  BFD_ASSERT (sym.n_numaux == 1);

		  /* Read in the aux information */

		  bfd_coff_swap_aux_in (abfd, (PTR) (esym + symesz), sym.n_type,
				        sym.n_sclass, 0, sym.n_numaux,
				        (PTR) &auxent);
		  /* From the aux information, get the backing "strong"
		     local symbol index. */
		  target_idx = auxent.x_sym.x_tagndx.l;

		  /* Check for IMAGE_WEAK_EXTERN_SEARCH_ALIAS; it's all
		     we currently support.  (See the 3/98 or later PE docs;
		     there used to be only two types.) */
		  BFD_ASSERT (auxent.x_sym.x_misc.x_fsize == 3);

		  /* Now that we've caught it... 

		     For the moment (unless/until proven otherwise) we'll
		     assume the real symbol is already declared in this
		     file (possibly as an UNDEF, but it exists). */

		  BFD_ASSERT(target_idx < sym_idx);

		  /* It's an alias (weak symbol).  Treat it that way. */
		  flags = BSF_WEAK | BSF_INDIRECT;

		  /* From the aux entry, get the name as a string. */
		  alias_target_name = 
		      (obj_coff_sym_hashes (abfd))[target_idx]->
				    root.root.string;
		  /* We'll add the symbol later (after symbol merge). */
		}

	      if (! coff_merge_symbol (abfd, info, name, &sym, classification,
				       &section, &value,
				       sym_hash, copy,
				       &override, &type_change_ok,
				       &size_change_ok))
		goto error_return;

	      if (override)
		definition = false;

	      h = *sym_hash;

	      while ((h->root.type == bfd_link_hash_indirect
	  	       && !h->root.u.i.info.alias)
		     || h->root.type == bfd_link_hash_warning)
		h = (struct coff_link_hash_entry *) h->root.u.i.link;

#ifdef DYNAMIC_LINKING /* [ */
	      if (dynamic
		  && dyn_data (abfd)->verdef != NULL
		  && ! override
		  && vernum > 1
		  && definition)
		h->verinfo.verdef = &dyn_data (abfd)->verdef[vernum - 1];
#endif /* ] */

	      /* If it's a section definition, we now know the section;
		 Above, we may have munged the section name in the
		 symbol table, but it's not munged in the section entry
		 itself.  There are two possible places to put it;
		 the section name entry itself, or in the section symbol
		 name.  We use the latter as less likely to cause
		 side effects.  We wait until now so we both have the
		 section, and so we have a permanent copy of the munged
		 name handy (without duplication). */
	      if (classification == COFF_SYMBOL_PE_SECTION)
		{
		   section->symbol->name = h->root.root.string;
		}
	    }
	  addit = true;

	  if (sym.n_sclass == C_WEAKEXT) /* NOT NT_WEAK! */
	    flags = BSF_WEAK;

	  /* In the PE format, section symbols actually refer to the
             start of the output section.  We handle them specially
             here. */
	  if ((flags & BSF_SECTION_SYM) != 0
	      && obj_pe (abfd) 
	      && (*sym_hash)->root.type != bfd_link_hash_new)
	    {

	      if ((*sym_hash)->root.type != bfd_link_hash_undefined
	      && (*sym_hash)->root.type != bfd_link_hash_undefweak
	      && ((*sym_hash)->coff_link_hash_flags
		   & COFF_LINK_HASH_PE_SECTION_SYMBOL) == 0)
		(*_bfd_error_handler)
		  (_("Warning: symbol `%s' is both section and non-section"),
		   name);

	      addit = false;
	    }

	  /* The Microsoft Visual C compiler does string pooling by
	     hashing the constants to an internal symbol name, and
	     relying on the linker comdat support to discard
	     duplicate names.  However, if one string is a literal and
	     one is a data initializer, one will end up in the .data
	     section and one will end up in the .rdata section.  The
	     Microsoft linker will combine them into the .data
	     section, which seems to be wrong since it might cause the
	     literal to change.

	     As long as there are no external references to the
	     symbols, which there shouldn't be, we can treat the .data
	     and .rdata instances as separate symbols.  The comdat
	     code in the linker will do the appropriate merging.  Here
	     we avoid getting a multiple definition error for one of
	     these special symbols.

	     FIXME: I don't think this will work in the case where
	     there are two object files which use the constants as a
	     literal and two object files which use it as a data
	     initializer.  One or the other of the second object files
	     is going to wind up with an inappropriate reference.  */
	  if (obj_pe (abfd)
	      && (classification == COFF_SYMBOL_GLOBAL
		  || classification == COFF_SYMBOL_PE_SECTION)
	      && section->comdat != NULL
	      && strncmp (name, "??_", 3) == 0
	      && strcmp (name, section->comdat->name) == 0)
	    {
	      if (*sym_hash == NULL)
		*sym_hash = coff_link_hash_lookup (coff_hash_table (info),
						   name, false, copy, false);
	      if (*sym_hash != NULL
		  && (*sym_hash)->root.type == bfd_link_hash_defined
		  && (*sym_hash)->root.u.def.section->comdat != NULL
		  && strcmp ((*sym_hash)->root.u.def.section->comdat->name,
			     section->comdat->name) == 0)
		addit = false;
	    }

	  if (addit)
	    {
	      if (! (bfd_coff_link_add_one_symbol
		     (info, abfd, name, flags, section, value,
		      alias_target_name, copy, false,
		      (struct bfd_link_hash_entry **) sym_hash)))
		goto error_return;
	    }

	  if (obj_pe (abfd) && (flags & BSF_SECTION_SYM) != 0)
	    (*sym_hash)->coff_link_hash_flags |=
	      COFF_LINK_HASH_PE_SECTION_SYMBOL;

	  /* If this is a weak alias, propigate the REF flags into
	     the corresponding strong symbol, so it gets properly
	     emitted as well. */
	  h_real = *sym_hash;
	  while ((h_real->root.type == bfd_link_hash_indirect
		   && h_real->root.u.i.info.alias))
	    h_real = (struct coff_link_hash_entry *) h_real->root.u.i.link;
	  h_real->coff_link_hash_flags |=
	    h->coff_link_hash_flags & 
			      (COFF_LINK_HASH_REF_DYNAMIC |
			      COFF_LINK_HASH_REF_REGULAR);
         /* We're going to remember h_real for later use (way
            down below). */
 
	 /* If it's common (and presuming we actually emit it), it should
	    be marked as DEF_REGULAR or DEF_DYNAMIC, as needed.  ld converts 
	    it to bfd_link_hash_defined when it decides to keep it. */
	 if ((*sym_hash)->root.type == bfd_link_hash_common)
	   {
	     (*sym_hash)->coff_link_hash_flags |=
			     dynamic? COFF_LINK_HASH_DEF_DYNAMIC 
				    : COFF_LINK_HASH_DEF_REGULAR;
	     
	   }

          /* Now deal with real indirect symbols. */
	  h = *sym_hash;
	  while ((h->root.type == bfd_link_hash_indirect
		   && !h->root.u.i.info.alias)
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct coff_link_hash_entry *) h->root.u.i.link;
	  *sym_hash = h;

#ifdef DYNAMIC_LINKING /* [ */

#ifdef USE_WEAK
	  new_weakdef = false;
	  if (dynamic
	      && definition
	      && (flags & BSF_WEAK) != 0
	      && ELF_ST_TYPE (sym.st_info) != STT_FUNC
	      && info->hash->creator->flavour == bfd_target_elf_flavour
	      && h->weakdef == NULL)
	    {
	      /* Keep a list of all weak defined non function symbols from
		 a dynamic object, using the weakdef field.  Later in this
		 function we will set the weakdef field to the correct
		 value.  We only put non-function symbols from dynamic
		 objects on this list, because that happens to be the only
		 time we need to know the normal symbol corresponding to a
		 weak symbol, and the information is time consuming to
		 figure out.  If the weakdef field is not already NULL,
		 then this symbol was already defined by some previous
		 dynamic object, and we will be using that previous
		 definition anyhow.  */

	      h->weakdef = weaks;
	      weaks = h;
	      new_weakdef = true;
	    }
#endif
#endif /* ] */

	  /* Limit the alignment of a common symbol to the possible
             alignment of a section.  There is no point to permitting
             a higher alignment for a common symbol: we can not
             guarantee it, and it may cause us to allocate extra space
             in the common section.  */
	  if (section == bfd_com_section_ptr
	      && (*sym_hash)->root.type == bfd_link_hash_common
	      && ((*sym_hash)->root.u.c.p->alignment_power
		  > bfd_coff_default_section_alignment_power (abfd)))
	    (*sym_hash)->root.u.c.p->alignment_power
	      = bfd_coff_default_section_alignment_power (abfd);

	  if (info->hash->creator->flavour == bfd_get_flavour (abfd))
	    {
#ifdef DYNAMIC_LINKING
	      int old_flags;
	      boolean is_dynsym;
	      int new_flag;
#endif
	      /* If we don't have any symbol information currently in
                 the hash table, or if we are looking at a symbol
                 definition, then update the symbol class and type in
                 the hash table.  */
  	      if (((*sym_hash)->class == C_NULL
  		   && (*sym_hash)->type == T_NULL)
  		  || sym.n_scnum != 0
  		  || (sym.n_value != 0
  		      && (*sym_hash)->root.type != bfd_link_hash_defined
  		      && (*sym_hash)->root.type != bfd_link_hash_defweak))
  		{
  		  (*sym_hash)->class = sym.n_sclass;
  		  if (sym.n_type != T_NULL)
  		    {
  		      /* We want to warn if the type changed, but not
  			 if it changed from an unspecified type.
  			 Testing the whole type byte may work, but the
  			 change from (e.g.) a function of unspecified
  			 type to function of known type also wants to
  			 skip the warning.  */
  		      if ((*sym_hash)->type != T_NULL
  			  && (*sym_hash)->type != sym.n_type
  		          && !(DTYPE ((*sym_hash)->type) == DTYPE (sym.n_type)
  		               && (BTYPE ((*sym_hash)->type) == T_NULL
  		                   || BTYPE (sym.n_type) == T_NULL)))
  			(*_bfd_error_handler)
  			  (_("Warning: type of symbol `%s' changed from %d to %d in %s"),
  			   name, (*sym_hash)->type, sym.n_type,
  			   bfd_archive_filename (abfd));

  		      /* We don't want to change from a meaningful
  			 base type to a null one, but if we know
  			 nothing, take what little we might now know.  */
  		      if (BTYPE (sym.n_type) != T_NULL
  			  || (*sym_hash)->type == T_NULL)
			(*sym_hash)->type = sym.n_type;
  		    }
  		  (*sym_hash)->auxbfd = abfd;
		  if (sym.n_numaux != 0)
		    {
		      union internal_auxent *alloc;
		      unsigned int i;
		      bfd_byte *eaux;
		      union internal_auxent *iaux;

		      (*sym_hash)->numaux = sym.n_numaux;
		      alloc = ((union internal_auxent *)
			       bfd_hash_allocate (&info->hash->table,
						  (sym.n_numaux
						   * sizeof (*alloc))));
		      if (alloc == NULL)
			goto error_return;
		      for (i = 0, eaux = esym + symesz, iaux = alloc;
			   i < sym.n_numaux;
			   i++, eaux += symesz, iaux++)
			bfd_coff_swap_aux_in (abfd, (PTR) eaux, sym.n_type,
					      sym.n_sclass, (int) i,
					      sym.n_numaux, (PTR) iaux);
		      (*sym_hash)->aux = alloc;
		    }
		}


#ifdef DYNAMIC_LINKING /* [ */
#ifdef USE_SIZE
	      /* If USE_COPY_RELOC is defined, this code probably needs
		 to be turned on as well, and if it is, the relationship
		 between the type checking here and that already present
		 for native coff */
	      /* Remember the symbol size and type.  */
	      if (sym.st_size != 0
		  && (definition || h->size == 0))
		{
		  if (h->size != 0 && h->size != sym.st_size && ! size_change_ok)
		    (*_bfd_error_handler)
		      (_("Warning: size of symbol `%s' changed from %lu to %lu in %s"),
		       name, (unsigned long) h->size, (unsigned long) sym.st_size,
		       bfd_get_filename (abfd));

		  h->size = sym.st_size;
		}
	      if (ELF_ST_TYPE (sym.st_info) != STT_NOTYPE
		  && (definition || h->type == STT_NOTYPE))
		{
		  if (h->type != STT_NOTYPE
		      && h->type != ELF_ST_TYPE (sym.st_info)
		      && ! type_change_ok)
		    (*_bfd_error_handler)
		      (_("Warning: type of symbol `%s' changed from %d to %d in %s"),
		       name, h->type, ELF_ST_TYPE (sym.st_info),
		       bfd_get_filename (abfd));

		  h->type = ELF_ST_TYPE (sym.st_info);
		}

	      if (sym.st_other != 0
		  && (definition || h->other == 0))
		h->other = sym.st_other;
#endif

	      /* Set a flag in the hash table entry indicating the type of
		 reference or definition we just found.  Keep a count of
		 the number of dynamic symbols we find.  A dynamic symbol
		 is one which is referenced or defined by both a regular
		 object and a shared object.  */
	      old_flags = (*sym_hash)->coff_link_hash_flags;
	      is_dynsym = false;
		  
	      if (! dynamic)
		{
		  if (! definition)
		    new_flag = COFF_LINK_HASH_REF_REGULAR;
		  else
		    new_flag = COFF_LINK_HASH_DEF_REGULAR;
		  if ((info->shared && classification != COFF_SYMBOL_PE_SECTION)
		      || (old_flags & (COFF_LINK_HASH_DEF_DYNAMIC
				       | COFF_LINK_HASH_REF_DYNAMIC)) != 0)
		    {
#ifdef USE_DLLS
			/* If we find a symbol is a DLL symbol, hide it from
			   the shared library stuff completely */
			if (!is_DLL_module)
#endif
		            is_dynsym = true;
		    }
		}
	      else
		{
		  if (! definition)
		    new_flag = COFF_LINK_HASH_REF_DYNAMIC;
		  else
		    new_flag = COFF_LINK_HASH_DEF_DYNAMIC;
		  if ((old_flags & (COFF_LINK_HASH_DEF_REGULAR
				    | COFF_LINK_HASH_REF_REGULAR)) != 0
#ifdef USE_WEAK
		      || ((*sym_hash)->weakdef != NULL
			  && ! new_weakdef
			  && (*sym_hash)->weakdef->dynindx != -1)
#endif
			  )
		    is_dynsym = true;
		}

#ifdef USE_DLLS
	      /* We flag symbols defined in a DLL so we know to not export
		 them; if we find the definition first (__imp_* symbols and
		 the various housekeeping symbols) we skip entering them as
		 dynamic symbols.  However, for forward refs, we can't know
		 until later, thus a flag. We'll not flag sections, because
		 they are neither definition nor reference, really, and it
		 doesn't work if we do.

		 BUT!!!, we treat __imp_ symbols as ordinary, as
		 they will need a dynamic reloc, at least until we
		 generate .reloc sections. */

	      if (definition 
		  && is_DLL_module 
		  && classification != COFF_SYMBOL_PE_SECTION
		  && strncmp(h->root.root.string, "__imp_",6) != 0)
		    new_flag |= COFF_LINK_HASH_DLL_DEFINED;
#endif

	      (*sym_hash)->coff_link_hash_flags |= new_flag;

	      /* If this symbol has a version, and it is the default
		 version, we create an indirect symbol from the default
		 name to the fully decorated name.  This will cause
		 external references which do not specify a version to be
		 bound to this version of the symbol.  */
	      if (definition)
		{
		  char *p;

		  p = strchr (name, COFF_VER_CHR);
		  /* MSVC compiler generated symbols can have @ in them,
		     but they begin with ?, so leave them alone. */
		  if (name[0] != '?' && p != NULL && p[1] == COFF_VER_CHR)
		    {
		      char *shortname;
		      struct coff_link_hash_entry *hi;
		      boolean override;

		      shortname = bfd_hash_allocate (&info->hash->table,
						     p - name + 1);
		      if (shortname == NULL)
			goto error_return;
		      strncpy (shortname, name, p - name);
		      shortname[p - name] = '\0';

		      /* We are going to create a new symbol.  Merge it
			 with any existing symbol with this name.  For the
			 purposes of the merge, act as though we were
			 defining the symbol we just defined, although we
			 actually going to define an indirect symbol.  */
		      type_change_ok = false;
		      size_change_ok = false;
		      if (! coff_merge_symbol (abfd, info, shortname, &sym, 
					      classification,
					      &section, &value, &hi, copy, &override,
					      &type_change_ok, &size_change_ok))
			goto error_return;

		      if (! override)
			{
			  if (! (_bfd_generic_link_add_one_symbol
				 (info, abfd, shortname, BSF_INDIRECT,
				  bfd_ind_section_ptr, (bfd_vma) 0, name, false,
				  false, (struct bfd_link_hash_entry **) &hi)))
			    goto error_return;
			}
		      else
			{
			  /* In this case the symbol named SHORTNAME is
			     overriding the indirect symbol we want to
			     add.  We were planning on making SHORTNAME an
			     indirect symbol referring to NAME.  SHORTNAME
			     is the name without a version.  NAME is the
			     fully versioned name, and it is the default
			     version.

			     Overriding means that we already saw a
			     definition for the symbol SHORTNAME in a
			     regular object, and it is overriding the
			     symbol defined in the dynamic object.

			     When this happens, we actually want to change
			     NAME, the symbol we just added, to refer to
			     SHORTNAME.  This will cause references to
			     NAME in the shared object to become
			     references to SHORTNAME in the regular
			     object.  This is what we expect when we
			     override a function in a shared object: that
			     the references in the shared object will be
			     mapped to the definition in the regular
			     object.  */

			  while ((hi->root.type == bfd_link_hash_indirect
				   && !hi->root.u.i.info.alias)
				 || hi->root.type == bfd_link_hash_warning)
			    hi = 
			      (struct coff_link_hash_entry *) hi->root.u.i.link;

			  h->root.type = bfd_link_hash_indirect;
			  h->root.u.i.link = (struct bfd_link_hash_entry *) hi;
			  h->root.u.i.info.alias = false;
			  if (h->coff_link_hash_flags 
			      & COFF_LINK_HASH_DEF_DYNAMIC)
			    {
			      h->coff_link_hash_flags 
				  &=~ COFF_LINK_HASH_DEF_DYNAMIC;
			      hi->coff_link_hash_flags 
				  |= COFF_LINK_HASH_REF_DYNAMIC;
			      if (hi->coff_link_hash_flags
				  & (COFF_LINK_HASH_REF_REGULAR
				     | COFF_LINK_HASH_DEF_REGULAR))
				{
				  if (! _bfd_coff_link_record_dynamic_symbol 
					(info, hi))
				    goto error_return;
				}
			    }

			  /* Now set HI to H, so that the following code
			     will set the other fields correctly.  */
			  hi = h;
			}

		      /* If there is a duplicate definition somewhere,
			 then HI may not point to an indirect symbol.  We
			 will have reported an error to the user in that
			 case.  */

		      if (hi->root.type == bfd_link_hash_indirect
			   && !hi->root.u.i.info.alias)
			{
			  struct coff_link_hash_entry *ht;

			  /* If the symbol became indirect, then we assume
			     that we have not seen a definition before.  */
			  BFD_ASSERT ((hi->coff_link_hash_flags
				       & (COFF_LINK_HASH_DEF_DYNAMIC
					  | COFF_LINK_HASH_DEF_REGULAR))
				      == 0);

			  ht = 
			      (struct coff_link_hash_entry *) hi->root.u.i.link;

			  /* See if the new flags lead us to realize that
			     the symbol must be dynamic.  */
			  if (! is_dynsym)
			    {
			      if (! dynamic)
				{
				  if (info->shared
				      || ((hi->coff_link_hash_flags
					   & COFF_LINK_HASH_REF_DYNAMIC)
					  != 0))
				    is_dynsym = true;
				}
			      else
				{
				  if ((hi->coff_link_hash_flags
				       & COFF_LINK_HASH_REF_REGULAR) != 0)
				    is_dynsym = true;
				}
			    }
			}

		      /* We also need to define an indirection from the
			 nondefault version of the symbol.  */

		      shortname = bfd_hash_allocate (&info->hash->table,
						     strlen (name));
		      if (shortname == NULL)
			goto error_return;
		      strncpy (shortname, name, p - name);
		      strcpy (shortname + (p - name), p + 1);

		      /* Once again, merge with any existing symbol.  */
		      type_change_ok = false;
		      size_change_ok = false;
		      if (! coff_merge_symbol (abfd, info, shortname, &sym,
					      classification,
					      &section, &value, &hi, copy, &override,
					      &type_change_ok, &size_change_ok))
			goto error_return;

		      if (override)
			{
			  /* Here SHORTNAME is a versioned name, so we
			     don't expect to see the type of override we
			     do in the case above.  */
			  (*_bfd_error_handler)
			    (_("%s: warning: unexpected redefinition of `%s'"),
			     bfd_get_filename (abfd), shortname);
			}
		      else
			{
			  if (! (bfd_coff_link_add_one_symbol
				 (info, abfd, shortname, BSF_INDIRECT,
				  bfd_ind_section_ptr, (bfd_vma) 0, name, 
				  false, false,
				  (struct bfd_link_hash_entry **) &hi)))
			    goto error_return;

			  /* If there is a duplicate definition somewhere,
			     then HI may not point to an indirect symbol.
			     We will have reported an error to the user in
			     that case.  */

			  if (hi->root.type == bfd_link_hash_indirect
				&& !hi->root.u.i.info.alias)
			    {
			      /* If the symbol became indirect, then we
				 assume that we have not seen a definition
				 before.  */
			      BFD_ASSERT ((hi->coff_link_hash_flags
					   & (COFF_LINK_HASH_DEF_DYNAMIC
					      | COFF_LINK_HASH_DEF_REGULAR))
					  == 0);

			      /* See if the new flags lead us to realize
				 that the symbol must be dynamic.  */
			      if (! is_dynsym)
				{
				  if (! dynamic)
				    {
				      if (info->shared
					  || ((hi->coff_link_hash_flags
					       & COFF_LINK_HASH_REF_DYNAMIC)
					      != 0))
					is_dynsym = true;
				    }
				  else
				    {
				      if ((hi->coff_link_hash_flags
					   & COFF_LINK_HASH_REF_REGULAR) != 0)
					is_dynsym = true;
				    }
				}
			    }
			}
		    }
		}

	      if (is_dynsym)
		{
		  if ((*sym_hash)->dynindx == -1)
		    {
		      if (!_bfd_coff_link_record_dynamic_symbol (info, 
			  (*sym_hash)))
			goto error_return;
#ifdef USE_WEAK
		      if ((*sym_hash)->weakdef != NULL
			  && ! new_weakdef
			  && (*sym_hash)->weakdef->dynindx == -1)
			{
			  if (! _bfd_coff_link_record_dynamic_symbol 
					   (info, (*sym_hash)->weakdef))
			    goto error_return;
			}
#endif
		    }
		  /* If this is a weak/indirect symbol, we need to get
		     the real symbol taken care of, too. (*sym_hash and
		     h_real will be the same if this isn't weak, so
		     don't do things twice.) */
		  if (h_real != (*sym_hash)
		      && h_real->dynindx == -1)
		    {
		      if (!_bfd_coff_link_record_dynamic_symbol (info, h_real))
			goto error_return;
#ifdef USE_WEAK
		      if (h_real->weakdef != NULL
			  && ! new_weakdef
			  && h_real->weakdef->dynindx == -1)
			{
			  if (! _bfd_coff_link_record_dynamic_symbol 
					   (info, h_real->weakdef))
			    goto error_return;
			}
#endif
		    }
		}
	    }
#endif /* ] */

 	  if (classification == COFF_SYMBOL_PE_SECTION
	      && (*sym_hash)->numaux != 0)
	    {
	      /* Some PE sections (such as .bss) have a zero size in
                 the section header, but a non-zero size in the AUX
                 record.  Correct that here.

		 FIXME: This is not at all the right place to do this.
		 For example, it won't help objdump.  This needs to be
		 done when we swap in the section header.  */

	      BFD_ASSERT ((*sym_hash)->numaux == 1);
	      if (section->_raw_size == 0)
		section->_raw_size = (*sym_hash)->aux[0].x_scn.x_scnlen;

	      /* FIXME: We could test whether the section sizes
                 matches the size in the aux entry, but apparently
                 that sometimes fails unexpectedly.  */
	    }
	}

      esym += (sym.n_numaux + 1) * symesz;
      sym_hash += sym.n_numaux + 1;
      sym_idx += sym.n_numaux + 1;
    }

#ifdef USE_WEAK
  /* Now set the weakdefs field correctly for all the weak defined
     symbols we found.  The only way to do this is to search all the
     symbols.  Since we only need the information for non functions in
     dynamic objects, that's the only time we actually put anything on
     the list WEAKS.  We need this information so that if a regular
     object refers to a symbol defined weakly in a dynamic object, the
     real symbol in the dynamic object is also put in the dynamic
     symbols; we also must arrange for both symbols to point to the
     same memory location.  We could handle the general case of symbol
     aliasing, but a general symbol alias can only be generated in
     assembler code, handling it correctly would be very time
     consuming, and other ELF linkers don't handle general aliasing
     either.  */
  while (weaks != NULL)
    {
      struct elf_link_hash_entry *hlook;
      asection *slook;
      bfd_vma vlook;
      struct elf_link_hash_entry **hpp;
      struct elf_link_hash_entry **hppend;

      hlook = weaks;
      weaks = hlook->weakdef;
      hlook->weakdef = NULL;

      BFD_ASSERT (hlook->root.type == bfd_link_hash_defined
		  || hlook->root.type == bfd_link_hash_defweak
		  || hlook->root.type == bfd_link_hash_common
		  || hlook->root.type == bfd_link_hash_indirect);
      slook = hlook->root.u.def.section;
      vlook = hlook->root.u.def.value;

      hpp = elf_sym_hashes (abfd);
      hppend = hpp + extsymcount;
      for (; hpp < hppend; hpp++)
	{
	  struct elf_link_hash_entry *h;

	  h = *hpp;
	  if (h != NULL && h != hlook
	      && h->root.type == bfd_link_hash_defined
	      && h->root.u.def.section == slook
	      && h->root.u.def.value == vlook)
	    {
	      hlook->weakdef = h;

	      /* If the weak definition is in the list of dynamic
		 symbols, make sure the real definition is put there
		 as well.  */
	      if (hlook->dynindx != -1
		  && h->dynindx == -1)
		{
		  if (! _bfd_elf_link_record_dynamic_symbol (info, h))
		    goto error_return;
		}

	      /* If the real definition is in the list of dynamic
                 symbols, make sure the weak definition is put there
                 as well.  If we don't do this, then the dynamic
                 loader might not merge the entries for the real
                 definition and the weak definition.  */
	      if (h->dynindx != -1
		  && hlook->dynindx == -1)
		{
		  if (! _bfd_elf_link_record_dynamic_symbol (info, hlook))
		    goto error_return;
		}

	      break;
	    }
	}
    }
#endif

#ifdef DYNAMIC_LINKING /* [ */
    if (dynamic) 
      {
	/* We do not want to include any of the sections in a dynamic
	   object in the output file.  We hack by simply clobbering the
	   list of sections in the BFD.  This could be handled more
	   cleanly by, say, a new section flag; the existing
	   SEC_NEVER_LOAD flag is not the one we want, because that one
	   still implies that the section takes up space in the output
	   file.  We do this after having extracted the section information
	   from the symbol table. */

	abfd->sections = NULL;
	abfd->section_count = 0;
      }

    if (buf != NULL)
      free (buf);

  /* If this object is the same format as the output object, and it is
     not a shared library, then let the backend look through the
     relocs.

     This is required to build global offset table entries and to
     arrange for dynamic relocs.  It is not required for the
     particular common case of linking non PIC code, even when linking
     against shared libraries, but unfortunately there is no way of
     knowing whether an object file has been compiled PIC or not.
     Looking through the relocs is not particularly time consuming.
     The problem is that we must either (1) keep the relocs in memory,
     which causes the linker to require additional runtime memory or
     (2) read the relocs twice from the input file, which wastes time.
     This would be a good case for using mmap.

     I have no idea how to handle linking PIC code into a file of a
     different format.  It probably can't be done.  */

  if (! dynamic
      && (abfd->xvec == info->hash->creator
         || abfd->xvec == info->hash->creator->input_format)
      )
    {
      asection *o;

      for (o = abfd->sections; o != NULL; o = o->next)
	{
	  struct internal_reloc *internal_relocs;
	  boolean ok;

	  if (o->reloc_count == 0)
	     continue;

	  internal_relocs = _bfd_coff_read_internal_relocs
			     (abfd, o, info->keep_memory, NULL,
			      false, NULL);

	  if (internal_relocs == NULL)
	    goto error_return;

	  ok = bfd_coff_backend_check_relocs(abfd, info, o, internal_relocs);

	  if (! info->keep_memory)
	    free (internal_relocs);

	  if (! ok)
	    goto error_return;
	}
    }
#endif /* ] */

  /* If this is a non-traditional, non-relocateable link, try to
     optimize the handling of any .stab/.stabstr sections.  */
  if (! info->relocateable
      && ! info->traditional_format
      && info->hash->creator->flavour == bfd_get_flavour (abfd)
      && (info->strip != strip_all && info->strip != strip_debugger))
    {
      asection *stab, *stabstr;

      stab = bfd_get_section_by_name (abfd, ".stab");
      if (stab != NULL)
	{
	  stabstr = bfd_get_section_by_name (abfd, ".stabstr");

	  if (stabstr != NULL)
	    {
	      struct coff_link_hash_table *table;
	      struct coff_section_tdata *secdata;

	      secdata = coff_section_data (abfd, stab);
	      if (secdata == NULL)
		{
		  amt = sizeof (struct coff_section_tdata);
		  stab->used_by_bfd = (PTR) bfd_zalloc (abfd, amt);
		  if (stab->used_by_bfd == NULL)
		    goto error_return;
		  secdata = coff_section_data (abfd, stab);
		}

	      table = coff_hash_table (info);

	      if (! _bfd_link_section_stabs (abfd, &table->stab_info,
					     stab, stabstr,
					     &secdata->stab_info))
		goto error_return;
	    }
	}
    }

  obj_coff_keep_syms (abfd) = keep_syms;

  return true;

 error_return:
#ifdef DYNAMIC_LINKING
  if (buf != NULL)
    free (buf);
#endif

  obj_coff_keep_syms (abfd) = keep_syms;
  return false;
}

/* Do the final link step.  */

boolean
_bfd_coff_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
#ifdef DYNAMIC_LINKING
  boolean dynamic;
  bfd *dynobj;
#endif
  bfd_size_type symesz;
  struct coff_final_link_info finfo;
  boolean debug_merge_allocated;
  boolean long_section_names;
  asection *o;
  struct bfd_link_order *p;
  bfd_size_type max_sym_count;
  bfd_size_type max_lineno_count;
  bfd_size_type max_reloc_count;
  bfd_size_type max_output_reloc_count;
  bfd_size_type max_contents_size;
  file_ptr rel_filepos;
  unsigned int relsz;
  file_ptr line_filepos;
  unsigned int linesz;
  bfd *sub;
  bfd_byte *external_relocs = NULL;
  char strbuf[STRING_SIZE_SIZE];
  bfd_size_type amt;

#ifdef DYNAMIC_LINKING
  if (info->shared)
    abfd->flags |= DYNAMIC;

  dynamic = coff_hash_table (info)->dynamic_sections_created;
  dynobj = coff_hash_table (info)->dynobj;
#endif

  symesz = bfd_coff_symesz (abfd);

  finfo.info = info;
  finfo.output_bfd = abfd;
  finfo.strtab = NULL;
  finfo.section_info = NULL;
  finfo.last_file_index = -1;
  finfo.last_bf_index = -1;
  finfo.internal_syms = NULL;
  finfo.sec_ptrs = NULL;
  finfo.sym_indices = NULL;
  finfo.outsyms = NULL;
  finfo.linenos = NULL;
  finfo.contents = NULL;
  finfo.external_relocs = NULL;
  finfo.internal_relocs = NULL;
  finfo.global_to_static = false;
  debug_merge_allocated = false;

#ifdef DYNAMIC_LINKING
  if (! dynamic)
    {
      finfo.dynsym_sec = NULL;
      finfo.hash_sec = NULL;
      finfo.symver_sec = NULL;
    }
  else
    {
      finfo.dynsym_sec = bfd_get_section_by_name (dynobj, ".dynsym");
      finfo.hash_sec = bfd_get_section_by_name (dynobj, ".hash");
      BFD_ASSERT (finfo.dynsym_sec != NULL && finfo.hash_sec != NULL);
      finfo.symver_sec = bfd_get_section_by_name (dynobj, ".gnu.version");
      /* Note that it is OK if symver_sec is NULL.  */
    }
#endif

  coff_data (abfd)->link_info = info;

  finfo.strtab = _bfd_stringtab_init ();
  if (finfo.strtab == NULL)
    goto error_return;

  if (! coff_debug_merge_hash_table_init (&finfo.debug_merge))
    goto error_return;
  debug_merge_allocated = true;

  /* Compute the file positions for all the sections.  */
  if (! abfd->output_has_begun)
    {
      if (! bfd_coff_compute_section_file_positions (abfd))
	goto error_return;
    }

  /* Count the line numbers and relocation entries required for the
     output file.  Set the file positions for the relocs.  */
  rel_filepos = obj_relocbase (abfd);
  relsz = bfd_coff_relsz (abfd);
  max_contents_size = 0;
  max_lineno_count = 0;
  max_reloc_count = 0;

  long_section_names = false;
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      o->reloc_count = 0;
      o->lineno_count = 0;
      for (p = o->link_order_head; p != NULL; p = p->next)
	{
	  if (p->type == bfd_indirect_link_order)
	    {
	      asection *sec;

	      sec = p->u.indirect.section;

	      /* Mark all sections which are to be included in the
		 link.  This will normally be every section.  We need
		 to do this so that we can identify any sections which
		 the linker has decided to not include.  */
	      sec->linker_mark = true;

	      if (info->strip == strip_none
		  || info->strip == strip_some)
		o->lineno_count += sec->lineno_count;

	      if (info->relocateable)
		o->reloc_count += sec->reloc_count;

	      if (sec->_raw_size > max_contents_size)
		max_contents_size = sec->_raw_size;
	      if (sec->lineno_count > max_lineno_count)
		max_lineno_count = sec->lineno_count;
	      if (sec->reloc_count > max_reloc_count)
		max_reloc_count = sec->reloc_count;
	    }
	  else if (info->relocateable
		   && (p->type == bfd_section_reloc_link_order
		       || p->type == bfd_symbol_reloc_link_order))
	    ++o->reloc_count;
	}
      if (o->reloc_count == 0)
	o->rel_filepos = 0;
      else
	{
	  o->flags |= SEC_RELOC;
	  o->rel_filepos = rel_filepos;
	  rel_filepos += o->reloc_count * relsz;
	  /* In PE COFF, if there are at least 0xffff relocations an
	     extra relocation will be written out to encode the count.  */
	  if (obj_pe (abfd) && o->reloc_count >= 0xffff)
	    rel_filepos += relsz;
	}

      if (bfd_coff_long_section_names (abfd)
	  && strlen (o->name) > SCNNMLEN)
	{
	  /* This section has a long name which must go in the string
             table.  This must correspond to the code in
             coff_write_object_contents which puts the string index
             into the s_name field of the section header.  That is why
             we pass hash as false.  */
	  if (_bfd_stringtab_add (finfo.strtab, o->name, false, false)
	      == (bfd_size_type) -1)
	    goto error_return;
	  long_section_names = true;
	}
    }

  /* If doing a relocateable link, allocate space for the pointers we
     need to keep.  */
  if (info->relocateable)
    {
      unsigned int i;

      /* We use section_count + 1, rather than section_count, because
         the target_index fields are 1 based.  */
      amt = abfd->section_count + 1;
      amt *= sizeof (struct coff_link_section_info);
      finfo.section_info = (struct coff_link_section_info *) bfd_malloc (amt);
      if (finfo.section_info == NULL)
	goto error_return;
      for (i = 0; i <= abfd->section_count; i++)
	{
	  finfo.section_info[i].relocs = NULL;
	  finfo.section_info[i].rel_hashes = NULL;
	}
    }

  /* We now know the size of the relocs, so we can determine the file
     positions of the line numbers.  */
  line_filepos = rel_filepos;
  linesz = bfd_coff_linesz (abfd);
  max_output_reloc_count = 0;
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if (o->lineno_count == 0)
	o->line_filepos = 0;
      else
	{
	  o->line_filepos = line_filepos;
	  line_filepos += o->lineno_count * linesz;
	}

      if (o->reloc_count != 0)
	{
	  /* We don't know the indices of global symbols until we have
             written out all the local symbols.  For each section in
             the output file, we keep an array of pointers to hash
             table entries.  Each entry in the array corresponds to a
             reloc.  When we find a reloc against a global symbol, we
             set the corresponding entry in this array so that we can
             fix up the symbol index after we have written out all the
             local symbols.

	     Because of this problem, we also keep the relocs in
	     memory until the end of the link.  This wastes memory,
	     but only when doing a relocateable link, which is not the
	     common case.  */
	  BFD_ASSERT (info->relocateable);
	  amt = o->reloc_count;
	  amt *= sizeof (struct internal_reloc);
	  finfo.section_info[o->target_index].relocs =
	    (struct internal_reloc *) bfd_malloc (amt);
	  amt = o->reloc_count;
	  amt *= sizeof (struct coff_link_hash_entry *);
	  finfo.section_info[o->target_index].rel_hashes =
	    (struct coff_link_hash_entry **) bfd_malloc (amt);
	  if (finfo.section_info[o->target_index].relocs == NULL
	      || finfo.section_info[o->target_index].rel_hashes == NULL)
	    goto error_return;

	  if (o->reloc_count > max_output_reloc_count)
	    max_output_reloc_count = o->reloc_count;
	}

      /* Reset the reloc and lineno counts, so that we can use them to
	 count the number of entries we have output so far.  */
      o->reloc_count = 0;
      o->lineno_count = 0;
    }

  obj_sym_filepos (abfd) = line_filepos;

  /* Figure out the largest number of symbols in an input BFD.  Take
     the opportunity to clear the output_has_begun fields of all the
     input BFD's.  */
  max_sym_count = 0;
  for (sub = info->input_bfds; sub != NULL; sub = sub->link_next)
    {
      size_t sz;

      sub->output_has_begun = false;
      sz = obj_raw_syment_count (sub);
      if (sz > max_sym_count)
	max_sym_count = sz;
    }

  /* Allocate some buffers used while linking.  */
  amt = max_sym_count * sizeof (struct internal_syment);
  finfo.internal_syms = (struct internal_syment *) bfd_malloc (amt);
  amt = max_sym_count * sizeof (asection *);
  finfo.sec_ptrs = (asection **) bfd_malloc (amt);
  amt = max_sym_count * sizeof (long);
  finfo.sym_indices = (long *) bfd_malloc (amt);
  finfo.outsyms = (bfd_byte *) bfd_malloc ((max_sym_count + 1) * symesz);
  amt = max_lineno_count * bfd_coff_linesz (abfd);
  finfo.linenos = (bfd_byte *) bfd_malloc (amt);
  finfo.contents = (bfd_byte *) bfd_malloc (max_contents_size);
  amt = max_reloc_count * relsz;
  finfo.external_relocs = (bfd_byte *) bfd_malloc (amt);
  if (! info->relocateable)
    {
      amt = max_reloc_count * sizeof (struct internal_reloc);
      finfo.internal_relocs = (struct internal_reloc *) bfd_malloc (amt);
    }
  if ((finfo.internal_syms == NULL && max_sym_count > 0)
      || (finfo.sec_ptrs == NULL && max_sym_count > 0)
      || (finfo.sym_indices == NULL && max_sym_count > 0)
      || finfo.outsyms == NULL
      || (finfo.linenos == NULL && max_lineno_count > 0)
      || (finfo.contents == NULL && max_contents_size > 0)
      || (finfo.external_relocs == NULL && max_reloc_count > 0)
      || (! info->relocateable
	  && finfo.internal_relocs == NULL
	  && max_reloc_count > 0))
    goto error_return;

  /* We now know the position of everything in the file, except that
     we don't know the size of the symbol table and therefore we don't
     know where the string table starts.  We just build the string
     table in memory as we go along.  We process all the relocations
     for a single input file at once.  */
  obj_raw_syment_count (abfd) = 0;

  if (coff_backend_info (abfd)->_bfd_coff_start_final_link)
    {
      if (! bfd_coff_start_final_link (abfd, info))
	goto error_return;
    }

  for (o = abfd->sections; o != NULL; o = o->next)
    {
      for (p = o->link_order_head; p != NULL; p = p->next)
	{
	  if (p->type == bfd_indirect_link_order
	      && bfd_family_coff (p->u.indirect.section->owner))
	    {
	      sub = p->u.indirect.section->owner;
	      if (! bfd_coff_link_output_has_begun (sub, & finfo))
		{
		  if (! _bfd_coff_link_input_bfd (&finfo, sub))
		    goto error_return;
		  sub->output_has_begun = true;
		}
	    }
	  else if (p->type == bfd_section_reloc_link_order
		   || p->type == bfd_symbol_reloc_link_order)
	    {
	      if (! _bfd_coff_reloc_link_order (abfd, &finfo, o, p))
		goto error_return;
	    }
	  else
	    {
	      if (! _bfd_default_link_order (abfd, info, o, p))
		goto error_return;
	    }
	}
    }

  /* Are there any input bfd's for which output_has_begun didn't
     get set?  If so, the symbols still participate, but the 
     sections don't.  The per-section linker_mark flag should
     keep the individual sections from being linked.
     This also covers the case of section-less .o files. */
  for (sub = info->input_bfds; sub != NULL; sub = sub->link_next)
    {
      if (! bfd_coff_link_output_has_begun (sub, & finfo))
	{
	  if (! _bfd_coff_link_input_bfd (&finfo, sub))
	    goto error_return;
	  sub->output_has_begun = true;
	}
    }

  if (! bfd_coff_final_link_postscript (abfd, & finfo))
    goto error_return;

  /* Free up the buffers used by _bfd_coff_link_input_bfd.  */

  coff_debug_merge_hash_table_free (&finfo.debug_merge);
  debug_merge_allocated = false;

  if (finfo.internal_syms != NULL)
    {
      free (finfo.internal_syms);
      finfo.internal_syms = NULL;
    }
  if (finfo.sec_ptrs != NULL)
    {
      free (finfo.sec_ptrs);
      finfo.sec_ptrs = NULL;
    }
  if (finfo.sym_indices != NULL)
    {
      free (finfo.sym_indices);
      finfo.sym_indices = NULL;
    }
  if (finfo.linenos != NULL)
    {
      free (finfo.linenos);
      finfo.linenos = NULL;
    }
  if (finfo.contents != NULL)
    {
      free (finfo.contents);
      finfo.contents = NULL;
    }
  if (finfo.external_relocs != NULL)
    {
      free (finfo.external_relocs);
      finfo.external_relocs = NULL;
    }
  if (finfo.internal_relocs != NULL)
    {
      free (finfo.internal_relocs);
      finfo.internal_relocs = NULL;
    }

  /* The value of the last C_FILE symbol is supposed to be the symbol
     index of the first external symbol.  Write it out again if
     necessary.  */
  if (finfo.last_file_index != -1
      && (unsigned int) finfo.last_file.n_value != obj_raw_syment_count (abfd))
    {
      file_ptr pos;

      finfo.last_file.n_value = obj_raw_syment_count (abfd);
      bfd_coff_swap_sym_out (abfd, (PTR) &finfo.last_file,
			     (PTR) finfo.outsyms);

      pos = obj_sym_filepos (abfd) + finfo.last_file_index * symesz;
      if (bfd_seek (abfd, pos, SEEK_SET) != 0
	  || bfd_bwrite (finfo.outsyms, symesz, abfd) != symesz)
	return false;
    }

  /* If doing task linking (ld --task-link) then make a pass through the
     global symbols, writing out any that are defined, and making them
     static.  */
  if (info->task_link)
    {
      finfo.failed = false;
      coff_link_hash_traverse (coff_hash_table (info),
			       _bfd_coff_write_task_globals,
			       (PTR) &finfo);
      if (finfo.failed)
	goto error_return;
    }

  /* Write out the global symbols.  */
  finfo.failed = false;
  coff_link_hash_traverse (coff_hash_table (info),
			   _bfd_coff_write_global_sym,
			   (PTR) &finfo);
  if (finfo.failed)
    goto error_return;

  /* The outsyms buffer is used by _bfd_coff_write_global_sym.  */
  if (finfo.outsyms != NULL)
    {
      free (finfo.outsyms);
      finfo.outsyms = NULL;
    }

  if (info->relocateable && max_output_reloc_count > 0)
    {
      /* Now that we have written out all the global symbols, we know
	 the symbol indices to use for relocs against them, and we can
	 finally write out the relocs.  */
      amt = max_output_reloc_count * relsz;
      external_relocs = (bfd_byte *) bfd_malloc (amt);
      if (external_relocs == NULL)
	goto error_return;

      for (o = abfd->sections; o != NULL; o = o->next)
	{
	  struct internal_reloc *irel;
	  struct internal_reloc *irelend;
	  struct coff_link_hash_entry **rel_hash;
	  bfd_byte *erel;

	  if (o->reloc_count == 0)
	    continue;

	  irel = finfo.section_info[o->target_index].relocs;
	  irelend = irel + o->reloc_count;
	  rel_hash = finfo.section_info[o->target_index].rel_hashes;
	  erel = external_relocs;
	  for (; irel < irelend; irel++, rel_hash++, erel += relsz)
	    {
	      if (*rel_hash != NULL)
		{
		  BFD_ASSERT ((*rel_hash)->indx >= 0);
		  irel->r_symndx = (*rel_hash)->indx;
		}
	      bfd_coff_swap_reloc_out (abfd, (PTR) irel, (PTR) erel);
	    }

	  if (bfd_seek (abfd, o->rel_filepos, SEEK_SET) != 0
	      || (bfd_bwrite ((PTR) external_relocs,
			     (bfd_size_type) relsz * o->reloc_count, abfd)
		  != (bfd_size_type) relsz * o->reloc_count))
	    goto error_return;
	}

      free (external_relocs);
      external_relocs = NULL;
    }

#ifdef DYNAMIC_LINKING /* [ */
  /* If we are linking against a dynamic object, or generating a
     shared library, finish up the dynamic linking information.  */
  if (dynamic)
    {
      coff_external_dyn *dyncon, *dynconend;

      /* Fix up .dynamic entries.  */
      o = coff_hash_table (info)->dynamic;
      BFD_ASSERT (o != NULL);

      dyncon = (coff_external_dyn *) o->contents;
      dynconend = (coff_external_dyn *) (o->contents + o->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  coff_internal_dyn dyn;
	  const char *name;

	  bfd_coff_swap_dyn_in (dynobj, dyncon, &dyn);

	  /* In the PE environment, the "addresses" here end up being
	     RVAs (ImageBase not applied) */
	  switch (dyn.d_tag)
	    {
	    default:
	      break;

	      /* SVR4 linkers seem to set DT_INIT and DT_FINI based on
                 magic _init and _fini symbols.  This is pretty ugly,
                 but we are compatible.  */
	    case DT_INIT:
	      name = &"__init"[bfd_get_symbol_leading_char(dynobj)=='_'?0:1];
	      goto get_sym;
	    case DT_FINI:
	      name = &"__fini"[bfd_get_symbol_leading_char(dynobj)=='_'?0:1];
	    get_sym:
	      {
		struct coff_link_hash_entry *h;

		h = coff_link_hash_lookup (coff_hash_table (info), name,
					  false, false, true);
		if (h != NULL
		    && (h->root.type == bfd_link_hash_defined
			|| h->root.type == bfd_link_hash_defweak))
		  {
		    dyn.d_un.d_val = h->root.u.def.value;
		    o = h->root.u.def.section;
		    if (o->output_section != NULL
      			&& (h->coff_link_hash_flags 
			    & COFF_LINK_HASH_DEF_REGULAR) != 0)
		      dyn.d_un.d_val += (o->output_section->vma
					 + o->output_offset);
		    else
		      {
			/* The symbol is imported from another shared
			   library and does not apply to this one.  */
			dyn.d_un.d_val = 0;
		      }

		    bfd_coff_swap_dyn_out (dynobj, &dyn, dyncon);
		  }
	      }
	      break;

	    case DT_HASH:
	      name = ".hash";
	      goto get_vma;
	    case DT_STRTAB:
	      name = ".dynstr";
	      goto get_vma;
	    case DT_SYMTAB:
	      name = ".dynsym";
	      goto get_vma;
	    case DT_VERDEF:
	      name = ".gnu.version_d";
	      goto get_vma;
	    case DT_VERNEED:
	      name = ".gnu.version_r";
	      goto get_vma;
	    case DT_VERSYM:
	      name = ".gnu.version";
	    get_vma:
	      o = bfd_get_section_by_name (dynobj, name);
	      BFD_ASSERT (o != NULL);
	      dyn.d_un.d_ptr = o->output_section->vma 
			       + o->vma + o->output_offset;
	      bfd_coff_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;

	    case DT_RELSZ:
	      {
		 asection *sec;
		/* Get the number of relocations */
		sec = bfd_get_section_by_name(abfd, ".rel.dyn");
		dyn.d_un.d_val = sec->_raw_size;
		bfd_coff_swap_dyn_out (dynobj, &dyn, dyncon);
		break;
	      }

	    case DT_REL:
	      {
		asection *sec;
		/* Get the location of the dynamic relocation section */
		sec = bfd_get_section_by_name(abfd, ".rel.dyn");
		dyn.d_un.d_val = sec->vma + sec->output_offset;
		bfd_coff_swap_dyn_out (dynobj, &dyn, dyncon);
		break;
		}
	    }
	}

      if (! bfd_coff_backend_finish_dynamic_sections (abfd, info))
	goto error_return;
    }

  /* This needs to be done even for simple static loads in case some
     PIC code slipped into our world, but only if they actually did. */
  if (dynobj != NULL)
    {
      for (o = dynobj->sections; o != NULL; o = o->next)
	{
	  if ((o->flags & SEC_HAS_CONTENTS) == 0
	      || o->_raw_size == 0)
	    continue;

	  if ((o->flags & SEC_LINKER_CREATED) == 0)
	    {
	      /* At this point, we are only interested in sections
		 created by coff_link_create_dynamic_sections.  */
	      continue;
	    }

	  if (strcmp (bfd_get_section_name (abfd, o), ".rel.internal") == 0
	      || strcmp (bfd_get_section_name (abfd, o), ".rel.got") == 0
	      || strcmp (bfd_get_section_name (abfd, o), ".rel.plt") == 0)
	    {
    if(o->_raw_size != o->reloc_count*bfd_coff_relsz(abfd)) fprintf(stderr, "%s %ld %d:\n",bfd_get_section_name(abfd,o), o->_raw_size, o->reloc_count * bfd_coff_relsz(abfd)); //
	       /* Dynamic reloc section */
	       BFD_ASSERT(o->_raw_size == o->reloc_count*bfd_coff_relsz(abfd));

	       /* even tho the assert fails, it returns, and not zeroing it
		  out creates havoc later.  If the section was too short,
		  the havoc has already happened. */
	       if (o->_raw_size > o->reloc_count*bfd_coff_relsz(abfd))
		   memset (o->contents+o->reloc_count*bfd_coff_relsz(abfd), 0, 
		     o->_raw_size - o->reloc_count*bfd_coff_relsz(abfd));
	       if (strcmp (bfd_get_section_name(abfd, o), ".rel.internal") == 0)
		 {
		   extern int reloc_compar(const void *, const void *);

		   /* sort the relocations.  It only really works to sort
		      rel.internal: GOT is in a separate section, and 
		      merging them together is a pain.  PLT must be a separate
		      chunk at runtime.  Both PLT and GOT should have only
		      one entry per symbol.  Thus, we just sort .rel.internal.
		      We sort it so that at runtime, the runtime linker
		      can cluster symbol lookups to the same symbol. */

		   qsort(o->contents, o->reloc_count, bfd_coff_relsz(abfd),
		      reloc_compar);
		 }
	    }

	  if (strcmp (bfd_get_section_name (abfd, o), ".dynstr") == 0)
	    {
	      file_ptr off;

	      /* The contents of the .dynstr section are actually in a
		 stringtab.  */
	      off = o->output_section->filepos;
	      if (bfd_seek (abfd, off, SEEK_SET) != 0
		  || ! _bfd_stringtab_emit (abfd,
					    coff_hash_table (info)->dynstr))
		goto error_return;
	    }
	  else
	    {
	      /* all other sections are not string sections */
	      if (! bfd_set_section_contents (abfd, o->output_section,
					      o->contents, o->output_offset,
					      o->_raw_size))
		goto error_return;
	    }
	}
    }
#endif /* ] */

  /* Free up the section information.  */
  if (finfo.section_info != NULL)
    {
      unsigned int i;

      for (i = 0; i < abfd->section_count; i++)
	{
	  if (finfo.section_info[i].relocs != NULL)
	    free (finfo.section_info[i].relocs);
	  if (finfo.section_info[i].rel_hashes != NULL)
	    free (finfo.section_info[i].rel_hashes);
	}
      free (finfo.section_info);
      finfo.section_info = NULL;
    }

  /* If we have optimized stabs strings, output them.  */
  if (coff_hash_table (info)->stab_info != NULL)
    {
      if (! _bfd_write_stab_strings (abfd, &coff_hash_table (info)->stab_info))
	return false;
    }

  /* Write out the string table.  */
  if (obj_raw_syment_count (abfd) != 0 || long_section_names)
    {
      file_ptr pos;

      pos = obj_sym_filepos (abfd) + obj_raw_syment_count (abfd) * symesz;
      if (bfd_seek (abfd, pos, SEEK_SET) != 0)
	return false;

#if STRING_SIZE_SIZE == 4
      H_PUT_32 (abfd,
		_bfd_stringtab_size (finfo.strtab) + STRING_SIZE_SIZE,
		strbuf);
#else
 #error Change H_PUT_32 above
#endif

      if (bfd_bwrite (strbuf, (bfd_size_type) STRING_SIZE_SIZE, abfd)
	  != STRING_SIZE_SIZE)
	return false;

      if (! _bfd_stringtab_emit (abfd, finfo.strtab))
	return false;

      obj_coff_strings_written (abfd) = true;
    }

  _bfd_stringtab_free (finfo.strtab);

  /* Setting bfd_get_symcount to 0 will cause write_object_contents to
     not try to write out the symbols.  */
  bfd_get_symcount (abfd) = 0;

  return true;

 error_return:
  if (debug_merge_allocated)
    coff_debug_merge_hash_table_free (&finfo.debug_merge);
  if (finfo.strtab != NULL)
    _bfd_stringtab_free (finfo.strtab);
  if (finfo.section_info != NULL)
    {
      unsigned int i;

      for (i = 0; i < abfd->section_count; i++)
	{
	  if (finfo.section_info[i].relocs != NULL)
	    free (finfo.section_info[i].relocs);
	  if (finfo.section_info[i].rel_hashes != NULL)
	    free (finfo.section_info[i].rel_hashes);
	}
      free (finfo.section_info);
    }
  if (finfo.internal_syms != NULL)
    free (finfo.internal_syms);
  if (finfo.sec_ptrs != NULL)
    free (finfo.sec_ptrs);
  if (finfo.sym_indices != NULL)
    free (finfo.sym_indices);
  if (finfo.outsyms != NULL)
    free (finfo.outsyms);
  if (finfo.linenos != NULL)
    free (finfo.linenos);
  if (finfo.contents != NULL)
    free (finfo.contents);
  if (finfo.external_relocs != NULL)
    free (finfo.external_relocs);
  if (finfo.internal_relocs != NULL)
    free (finfo.internal_relocs);
  if (external_relocs != NULL)
    free (external_relocs);
  return false;
}

/* parse out a -heap <reserved>,<commit> line */

static char *
dores_com (ptr, output_bfd, heap)
     char *ptr;
     bfd *output_bfd;
     int heap;
{
  if (coff_data(output_bfd)->pe)
    {
      int val = strtoul (ptr, &ptr, 0);
      if (heap)
	pe_data(output_bfd)->pe_opthdr.SizeOfHeapReserve = val;
      else
	pe_data(output_bfd)->pe_opthdr.SizeOfStackReserve = val;

      if (ptr[0] == ',')
	{
	  val = strtoul (ptr+1, &ptr, 0);
	  if (heap)
	    pe_data(output_bfd)->pe_opthdr.SizeOfHeapCommit = val;
	  else
	    pe_data(output_bfd)->pe_opthdr.SizeOfStackCommit = val;
	}
    }
  return ptr;
}

static char *get_name(ptr, dst)
char *ptr;
char **dst;
{
  while (*ptr == ' ')
    ptr++;
  *dst = ptr;
  while (*ptr && *ptr != ' ')
    ptr++;
  *ptr = 0;
  return ptr+1;
}

/* Process any magic embedded commands in a section called .drectve */

static int
process_embedded_commands (output_bfd, info,  abfd)
     bfd *output_bfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     bfd *abfd;
{
  asection *sec = bfd_get_section_by_name (abfd, ".drectve");
  char *s;
  char *e;
  char *copy;
  if (!sec)
    return 1;

  copy = bfd_malloc (sec->_raw_size);
  if (!copy)
    return 0;
  if (! bfd_get_section_contents(abfd, sec, copy, (bfd_vma) 0, sec->_raw_size))
    {
      free (copy);
      return 0;
    }
  e = copy + sec->_raw_size;
  for (s = copy;  s < e ; )
    {
      if (s[0]!= '-') {
	s++;
	continue;
      }
      if (strncmp (s,"-attr", 5) == 0)
	{
	  char *name;
	  char *attribs;
	  asection *asec;

	  int loop = 1;
	  int had_write = 0;
	  int had_read = 0;
	  int had_exec= 0;
	  int had_shared= 0;
	  s += 5;
	  s = get_name(s, &name);
	  s = get_name(s, &attribs);
	  while (loop) {
	    switch (*attribs++)
	      {
	      case 'W':
		had_write = 1;
		break;
	      case 'R':
		had_read = 1;
		break;
	      case 'S':
		had_shared = 1;
		break;
	      case 'X':
		had_exec = 1;
		break;
	      default:
		loop = 0;
	      }
	  }
	  asec = bfd_get_section_by_name (abfd, name);
	  if (asec) {
	    if (had_exec)
	      asec->flags |= SEC_CODE;
	    if (!had_write)
	      asec->flags |= SEC_READONLY;
	  }
	}
      else if (strncmp (s,"-heap", 5) == 0)
	{
	  s = dores_com (s+5, output_bfd, 1);
	}
      else if (strncmp (s,"-stack", 6) == 0)
	{
	  s = dores_com (s+6, output_bfd, 0);
	}
      else
	s++;
    }
  free (copy);
  return 1;
}

/* Place a marker against all symbols which are used by relocations.
   This marker can be picked up by the 'do we skip this symbol ?'
   loop in _bfd_coff_link_input_bfd() and used to prevent skipping
   that symbol.
   */

static void
mark_relocs (finfo, input_bfd)
     struct coff_final_link_info *	finfo;
     bfd * 				input_bfd;
{
  asection * a;

  if ((bfd_get_file_flags (input_bfd) & HAS_SYMS) == 0)
    return;

  for (a = input_bfd->sections; a != (asection *) NULL; a = a->next)
    {
      struct internal_reloc *	internal_relocs;
      struct internal_reloc *	irel;
      struct internal_reloc *	irelend;

      if ((a->flags & SEC_RELOC) == 0 || a->reloc_count  < 1
	|| a->linker_mark == 0)
	continue;
      /* Don't mark relocs in excluded sections.  */
      if (a->output_section == bfd_abs_section_ptr)
	continue;

      /* Read in the relocs.  */
      internal_relocs = _bfd_coff_read_internal_relocs
	(input_bfd, a, false,
	 finfo->external_relocs,
	 finfo->info->relocateable,
	 (finfo->info->relocateable
	  ? (finfo->section_info[ a->output_section->target_index ].relocs + a->output_section->reloc_count)
	  : finfo->internal_relocs)
	);

      if (internal_relocs == NULL)
	continue;

      irel     = internal_relocs;
      irelend  = irel + a->reloc_count;

      /* Place a mark in the sym_indices array (whose entries have
	 been initialised to 0) for all of the symbols that are used
	 in the relocation table.  This will then be picked up in the
	 skip/don't pass */

      for (; irel < irelend; irel++)
	{
	  finfo->sym_indices[ irel->r_symndx ] = -1;
	}
    }
}

#ifdef DYNAMIC_LINKING /* [ */
/* Standard ELF hash function.  Could be changed for COFF, but why bother */
static unsigned long bfd_coff_hash PARAMS((CONST unsigned char *name));
static unsigned long
bfd_coff_hash (name)
     CONST unsigned char *name;
{
  unsigned long h = 0;
  unsigned long g;
  int ch;

  while ((ch = *name++) != '\0')
    {
      h = (h << 4) + ch;
      if ((g = (h & 0xf0000000)) != 0)
        {
          h ^= g >> 24;
          h &= ~g;
        }
    }
  return h;
}

static void _bfd_coff_output_dynamic_symbol PARAMS((struct internal_syment *, 
    struct coff_link_hash_entry *, struct coff_final_link_info *finfo));

/* Actually output the dynamic symbol (and it's hash and stringtable
   supporting information) as needed */
static void 
_bfd_coff_output_dynamic_symbol(isym, h, finfo)
    struct internal_syment *isym;
    struct coff_link_hash_entry *h;
    struct coff_final_link_info *finfo;
{
  char *p, *copy;
  const char *name;
  size_t bucketcount;
  size_t bucket;
  bfd_byte *bucketpos;
  bfd_vma chain;
  bfd_size_type symesz;

  symesz = bfd_coff_symesz (finfo->output_bfd);

  /* If this goes into a string table entry, we need to change the
     index; if not, the inline symbol is just fine */
  if (strlen (h->root.root.string) > SYMNMLEN)
      isym->_n._n_n._n_offset = h->dynstr_index;

  /* In general, we aren't interested in AUX entries.  However, for 
     C_NT_WEAK we need the indirect symbol, which is in the AUX entry,
     which needs to be updated for the dynindx.
     Oh well...  */
  if (h->root.type == bfd_link_hash_indirect
	  && h->root.u.i.info.alias)
    {
      union internal_auxent aux;
      struct coff_link_hash_entry *h_real = h;

      /* Usually, an aux entry is present, but if the stars align just wrong,
	 there isn't one.  Thus, just create one. */
      while (h_real->root.type == bfd_link_hash_indirect
	      && h_real->root.u.i.info.alias)
	h_real = (struct coff_link_hash_entry *) h_real->root.u.i.link;

      memset((PTR)&aux, 0, sizeof(aux));
      aux.x_sym.x_tagndx.l = h_real->dynindx;
      aux.x_sym.x_misc.x_fsize = 3;
      isym->n_numaux = 1;

      bfd_coff_swap_aux_out (finfo->output_bfd, (PTR)&aux, isym->n_type,
			     isym->n_sclass, 0, 1,
			     (PTR) (finfo->dynsym_sec->contents
				  + (h->dynindx+1) * symesz));
    }
  else
    {
      isym->n_numaux = 0;
    }

  /* We're writing into an array here, so it's a bit more convenient
     to write the real symbol after the AUX.  */

  bfd_coff_swap_sym_out (finfo->output_bfd, isym,
		       (PTR) (finfo->dynsym_sec->contents
			      + h->dynindx * symesz));

  /* We didn't include the version string in the dynamic string
     table, so we must not consider it in the hash table.  */
  name = h->root.root.string;
  if (name[0] == '?' || (p = strchr (name, COFF_VER_CHR)) == NULL)
    copy = NULL;
  else
    {
      copy = bfd_alloc (finfo->output_bfd, p - name + 1);
      strncpy (copy, name, p - name);
      copy[p - name] = '\0';
      name = copy;
    }

  bucketcount = coff_hash_table (finfo->info)->bucketcount;
  bucket = bfd_coff_hash ((const unsigned char *) name) % bucketcount;
  bucketpos = ((bfd_byte *) finfo->hash_sec->contents
	       + (bucket + 2) * (ARCH_SIZE / 8));
  chain = bfd_h_get_32 (finfo->output_bfd, bucketpos);
  bfd_h_put_32 (finfo->output_bfd, h->dynindx, bucketpos);
  bfd_h_put_32 (finfo->output_bfd, chain,
	    ((bfd_byte *) finfo->hash_sec->contents
	     + (bucketcount + 2 + h->dynindx) * (ARCH_SIZE / 8)));

  if (copy != NULL)
    bfd_release (finfo->output_bfd, copy);

  if (finfo->symver_sec != NULL && finfo->symver_sec->contents != NULL)
    {
      coff_internal_versym iversym;

      if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  if (h->verinfo.verdef == NULL)
	    iversym.vs_vers = 0;
	  else
	    iversym.vs_vers = h->verinfo.verdef->vd_exp_refno + 1;
	}
      else
	{
	  if (h->verinfo.vertree == NULL)
	    iversym.vs_vers = 1;
	  else
	    iversym.vs_vers = h->verinfo.vertree->vernum + 1;
	}

      if ((h->coff_link_hash_flags & COFF_LINK_HIDDEN) != 0)
	iversym.vs_vers |= VERSYM_HIDDEN;

      bfd_coff_swap_versym_out (finfo->output_bfd, &iversym,
				(((coff_external_versym *)
				  finfo->symver_sec->contents)
				 + h->dynindx));
    }
    return;
}
#endif /* ] */


/* Link an input file into the linker output file.  This function
   handles all the sections and relocations of the input file at once.  */

boolean
_bfd_coff_link_input_bfd (finfo, input_bfd)
     struct coff_final_link_info *finfo;
     bfd *input_bfd;
{
  unsigned int n_tmask = coff_data (input_bfd)->local_n_tmask;
  unsigned int n_btshft = coff_data (input_bfd)->local_n_btshft;
#if 0
  unsigned int n_btmask = coff_data (input_bfd)->local_n_btmask;
#endif
  boolean (*adjust_symndx) PARAMS ((bfd *, struct bfd_link_info *, bfd *,
				    asection *, struct internal_reloc *,
				    boolean *));
  bfd *output_bfd;
  const char *strings;
  bfd_size_type syment_base;
  boolean copy, hash;
  bfd_size_type isymesz;
  bfd_size_type osymesz;
  bfd_size_type linesz;
  bfd_byte *esym;
  bfd_byte *esym_end;
  struct internal_syment *isymp;
  asection **secpp;
  long *indexp;
  unsigned long output_index;
  bfd_byte *outsym;
  struct coff_link_hash_entry **sym_hash;
  asection *o;
  struct coff_link_hash_entry *h;
  long indx;


#ifdef DYNAMIC_LINKING
  /* If this is a dynamic object, we don't want to do anything here:
     we don't want the local symbols, and we don't want the section
     contents.  */
  if ((input_bfd->flags & DYNAMIC) != 0)
     return true;
#endif

  /* Move all the symbols to the output file.  */

  output_bfd = finfo->output_bfd;
  strings = NULL;
  syment_base = obj_raw_syment_count (output_bfd);
  isymesz = bfd_coff_symesz (input_bfd);
  osymesz = bfd_coff_symesz (output_bfd);
  linesz = bfd_coff_linesz (input_bfd);
  BFD_ASSERT (linesz == bfd_coff_linesz (output_bfd));

  copy = false;
  if (! finfo->info->keep_memory)
    copy = true;
  hash = true;
  if ((output_bfd->flags & BFD_TRADITIONAL_FORMAT) != 0)
    hash = false;

  if (! _bfd_coff_get_external_symbols (input_bfd))
    return false;

  esym = (bfd_byte *) obj_coff_external_syms (input_bfd);
  esym_end = esym + obj_raw_syment_count (input_bfd) * isymesz;
  isymp = finfo->internal_syms;
  secpp = finfo->sec_ptrs;
  indexp = finfo->sym_indices;
  output_index = syment_base;
  outsym = finfo->outsyms;

  if (coff_data (output_bfd)->pe)
    {
      if (! process_embedded_commands (output_bfd, finfo->info, input_bfd))
	return false;
    }

  /* If we are going to perform relocations and also strip/discard some symbols
     then we must make sure that we do not strip/discard those symbols that are
     going to be involved in the relocations */
  if ((   finfo->info->strip   != strip_none
       || finfo->info->discard != discard_none)
      && finfo->info->relocateable)
    {
      /* mark the symbol array as 'not-used' */
      memset (indexp, 0, obj_raw_syment_count (input_bfd) * sizeof * indexp);

      mark_relocs (finfo, input_bfd);
    }

  while (esym < esym_end)
    {
      struct internal_syment isym;
      enum coff_symbol_classification classification;
      boolean skip;
      boolean global;
      boolean dont_skip_symbol;
      int add;

      bfd_coff_swap_sym_in (input_bfd, (PTR) esym, (PTR) isymp);

      /* Make a copy of *isymp so that the relocate_section function
	 always sees the original values.  This is more reliable than
	 always recomputing the symbol value even if we are stripping
	 the symbol.  */
      isym = *isymp;

      classification = bfd_coff_classify_symbol (input_bfd, &isym);
      switch (classification)
	{
	default:
	  abort ();
	case COFF_SYMBOL_GLOBAL:
	case COFF_SYMBOL_PE_SECTION:
	case COFF_SYMBOL_LOCAL:
	  *secpp = coff_section_from_bfd_index (input_bfd, isym.n_scnum);
	  break;
	case COFF_SYMBOL_COMMON:
	  *secpp = bfd_com_section_ptr;
	  break;
	case COFF_SYMBOL_UNDEFINED:
	  *secpp = bfd_und_section_ptr;
	  break;
	}

      /* Extract the flag indicating if this symbol is used by a
         relocation.  */
      if ((finfo->info->strip != strip_none
	   || finfo->info->discard != discard_none)
	  && finfo->info->relocateable)
	dont_skip_symbol = *indexp;
      else
	dont_skip_symbol = false;

      *indexp = -1;

      skip = false;
      global = false;
      add = 1 + isym.n_numaux;

      /* If we are stripping all symbols, we want to skip this one.  */
      if (finfo->info->strip == strip_all && ! dont_skip_symbol)
	skip = true;

      /* NT_WEAKs need to come out after the real symbol, so we skip
	 them here; when we write globals we'll discover they're leftovers
	 and write them then. */
      if (obj_pe(input_bfd) && isym.n_sclass == C_NT_WEAK)
	skip = true;

      indx = (esym - (bfd_byte *) obj_coff_external_syms (input_bfd))
	   / isymesz;
      h = obj_coff_sym_hashes (input_bfd)[indx];

      if (! skip)
	{
	  switch (classification)
	    {
	    default:
	      abort ();

	    case COFF_SYMBOL_PE_SECTION:
	      /* For relocatable links, we want to keep the comdat info
		 around. If it's a section symbol that we're going to emit,
		 and it's a section with a comdat symbol, we must
		 emit it now (so it's corresponding COMDAT symbol
		 comes out in the right order).  Any other section
		 symbol (and non-relocateable) gets sorted out later. */
	      if (! finfo->info->relocateable
		  || (h->coff_link_hash_flags 
			  & COFF_LINK_HASH_PE_SECTION_SYMBOL) == 0
		  || h->root.u.def.section->output_section == NULL
		  || h->root.u.def.section->comdat == NULL)
		skip = true;
	      global = true;
	      break;

	    case COFF_SYMBOL_GLOBAL:
	    case COFF_SYMBOL_COMMON:
	      /* This is a global symbol.  Global symbols come at the
		 end of the symbol table, so skip them for now.
		 Locally defined function symbols, however, are an
		 exception, and are not moved to the end.  */
	      global = true;
	      if (! ISFCN (isym.n_type))
		skip = true;
	      break;

	    case COFF_SYMBOL_UNDEFINED:
	      /* Undefined symbols are left for the end.  */
	      global = true;
	      skip = true;
	      break;

	    case COFF_SYMBOL_LOCAL:
	      /* This is a local symbol.  Skip it if we are discarding
                 local symbols.  */
	      if (finfo->info->discard == discard_all && ! dont_skip_symbol)
		skip = true;
	      break;
	    }
	}

#ifndef COFF_WITH_PE
      /* Skip section symbols for sections which are not going to be
	 emitted.  */
      if (!skip
	  && isym.n_sclass == C_STAT
	  && isym.n_type == T_NULL
          && isym.n_numaux > 0)
        {
          if ((*secpp)->output_section == bfd_abs_section_ptr)
            skip = true;
        }
#endif

      /* If we stripping debugging symbols, and this is a debugging
         symbol, then skip it.  FIXME: gas sets the section to N_ABS
         for some types of debugging symbols; I don't know if this is
         a bug or not.  In any case, we handle it here.  */
      if (! skip
	  && finfo->info->strip == strip_debugger
	  && ! dont_skip_symbol
	  && (isym.n_scnum == N_DEBUG
	      || (isym.n_scnum == N_ABS
		  && (isym.n_sclass == C_AUTO
		      || isym.n_sclass == C_REG
		      || isym.n_sclass == C_MOS
		      || isym.n_sclass == C_MOE
		      || isym.n_sclass == C_MOU
		      || isym.n_sclass == C_ARG
		      || isym.n_sclass == C_REGPARM
		      || isym.n_sclass == C_FIELD
		      || isym.n_sclass == C_EOS))))
	skip = true;

      /* If some symbols are stripped based on the name, work out the
	 name and decide whether to skip this symbol.  Symbol table
	 symbols with an index of -2 are "must keep", so don't strip them. */
      if (! skip && (h == NULL  || h->indx != -2)
	  && (finfo->info->strip == strip_some
	      || finfo->info->discard == discard_l))
	{
	  const char *name;
	  char buf[SYMNMLEN + 1];

	  name = _bfd_coff_internal_syment_name (input_bfd, &isym, buf);
	  if (name == NULL)
	    return false;

	  if (! dont_skip_symbol
	      && ((finfo->info->strip == strip_some
		   && (bfd_hash_lookup (finfo->info->keep_hash, name, false,
				    false) == NULL))
		   || (! global
		       && finfo->info->discard == discard_l
		       && bfd_is_local_label_name (input_bfd, name))))
	    skip = true;
	}

      /* If this is an enum, struct, or union tag, see if we have
         already output an identical type.  */
      if (! skip
	  && (finfo->output_bfd->flags & BFD_TRADITIONAL_FORMAT) == 0
	  && (isym.n_sclass == C_ENTAG
	      || isym.n_sclass == C_STRTAG
	      || isym.n_sclass == C_UNTAG)
	  && isym.n_numaux == 1)
	{
	  const char *name;
	  char buf[SYMNMLEN + 1];
	  struct coff_debug_merge_hash_entry *mh;
	  struct coff_debug_merge_type *mt;
	  union internal_auxent aux;
	  struct coff_debug_merge_element **epp;
	  bfd_byte *esl, *eslend;
	  struct internal_syment *islp;
	  bfd_size_type amt;

	  name = _bfd_coff_internal_syment_name (input_bfd, &isym, buf);
	  if (name == NULL)
	    return false;

	  /* Ignore fake names invented by compiler; treat them all as
             the same name.  */
	  if (*name == '~' || *name == '.' || *name == '$'
	      || (*name == bfd_get_symbol_leading_char (input_bfd)
		  && (name[1] == '~' || name[1] == '.' || name[1] == '$')))
	    name = "";

	  mh = coff_debug_merge_hash_lookup (&finfo->debug_merge, name,
					     true, true);
	  if (mh == NULL)
	    return false;

	  /* Allocate memory to hold type information.  If this turns
             out to be a duplicate, we pass this address to
             bfd_release.  */
	  amt = sizeof (struct coff_debug_merge_type);
	  mt = (struct coff_debug_merge_type *) bfd_alloc (input_bfd, amt);
	  if (mt == NULL)
	    return false;
	  mt->class = isym.n_sclass;

	  /* Pick up the aux entry, which points to the end of the tag
             entries.  */
	  bfd_coff_swap_aux_in (input_bfd, (PTR) (esym + isymesz),
				isym.n_type, isym.n_sclass, 0, isym.n_numaux,
				(PTR) &aux);

	  /* Gather the elements.  */
	  epp = &mt->elements;
	  mt->elements = NULL;
	  islp = isymp + 2;
	  esl = esym + 2 * isymesz;
	  eslend = ((bfd_byte *) obj_coff_external_syms (input_bfd)
		    + aux.x_sym.x_fcnary.x_fcn.x_endndx.l * isymesz);
	  while (esl < eslend)
	    {
	      const char *elename;
	      char elebuf[SYMNMLEN + 1];
	      char *name_copy;

	      bfd_coff_swap_sym_in (input_bfd, (PTR) esl, (PTR) islp);

	      amt = sizeof (struct coff_debug_merge_element);
	      *epp = ((struct coff_debug_merge_element *)
		      bfd_alloc (input_bfd, amt));
	      if (*epp == NULL)
		return false;

	      elename = _bfd_coff_internal_syment_name (input_bfd, islp,
							elebuf);
	      if (elename == NULL)
		return false;

	      amt = strlen (elename) + 1;
	      name_copy = (char *) bfd_alloc (input_bfd, amt);
	      if (name_copy == NULL)
		return false;
	      strcpy (name_copy, elename);

	      (*epp)->name = name_copy;
	      (*epp)->type = islp->n_type;
	      (*epp)->tagndx = 0;
	      if (islp->n_numaux >= 1
		  && islp->n_type != T_NULL
		  && islp->n_sclass != C_EOS)
		{
		  union internal_auxent eleaux;
		  long indx;

		  bfd_coff_swap_aux_in (input_bfd, (PTR) (esl + isymesz),
					islp->n_type, islp->n_sclass, 0,
					islp->n_numaux, (PTR) &eleaux);
		  indx = eleaux.x_sym.x_tagndx.l;

		  /* FIXME: If this tagndx entry refers to a symbol
		     defined later in this file, we just ignore it.
		     Handling this correctly would be tedious, and may
		     not be required.  */

		  if (indx > 0
		      && (indx
			  < ((esym -
			      (bfd_byte *) obj_coff_external_syms (input_bfd))
			     / (long) isymesz)))
		    {
		      (*epp)->tagndx = finfo->sym_indices[indx];
		      if ((*epp)->tagndx < 0)
			(*epp)->tagndx = 0;
		    }
		}
	      epp = &(*epp)->next;
	      *epp = NULL;

	      esl += (islp->n_numaux + 1) * isymesz;
	      islp += islp->n_numaux + 1;
	    }

	  /* See if we already have a definition which matches this
             type.  We always output the type if it has no elements,
             for simplicity.  */
	  if (mt->elements == NULL)
	    bfd_release (input_bfd, (PTR) mt);
	  else
	    {
	      struct coff_debug_merge_type *mtl;

	      for (mtl = mh->types; mtl != NULL; mtl = mtl->next)
		{
		  struct coff_debug_merge_element *me, *mel;

		  if (mtl->class != mt->class)
		    continue;

		  for (me = mt->elements, mel = mtl->elements;
		       me != NULL && mel != NULL;
		       me = me->next, mel = mel->next)
		    {
		      if (strcmp (me->name, mel->name) != 0
			  || me->type != mel->type
			  || me->tagndx != mel->tagndx)
			break;
		    }

		  if (me == NULL && mel == NULL)
		    break;
		}

	      if (mtl == NULL || (bfd_size_type) mtl->indx >= syment_base)
		{
		  /* This is the first definition of this type.  */
		  mt->indx = output_index;
		  mt->next = mh->types;
		  mh->types = mt;
		}
	      else
		{
		  /* This is a redefinition which can be merged.  */
		  bfd_release (input_bfd, (PTR) mt);
		  *indexp = mtl->indx;
		  add = (eslend - esym) / isymesz;
		  skip = true;
		}
	    }
	}

      /* We now know whether we are to skip this symbol or not.  */
      if (! skip)
	{
	  /* Adjust the symbol in order to output it.  */

	  if (isym._n._n_n._n_zeroes == 0
	      && isym._n._n_n._n_offset != 0)
	    {
	      const char *name;
	      bfd_size_type indx;

	      /* This symbol has a long name.  Enter it in the string
		 table we are building.  Note that we do not check
		 bfd_coff_symname_in_debug.  That is only true for
		 XCOFF, and XCOFF requires different linking code
		 anyhow.  */
	      name = _bfd_coff_internal_syment_name (input_bfd, &isym,
						     (char *) NULL);
	      if (name == NULL)
		return false;
	      indx = _bfd_stringtab_add (finfo->strtab, name, hash, copy);
	      if (indx == (bfd_size_type) -1)
		return false;
	      isym._n._n_n._n_offset = STRING_SIZE_SIZE + indx;
	    }

	  switch (isym.n_sclass)
	    {
	    case C_AUTO:
	    case C_MOS:
	    case C_EOS:
	    case C_MOE:
	    case C_MOU:
	    case C_UNTAG:
	    case C_STRTAG:
	    case C_ENTAG:
	    case C_TPDEF:
	    case C_ARG:
	    case C_USTATIC:
	    case C_REG:
	    case C_REGPARM:
	    case C_FIELD:
	      /* The symbol value should not be modified.  */
	      break;

	    case C_FCN:
	      if (obj_pe (input_bfd)
		  && strcmp (isym.n_name, ".bf") != 0
		  && isym.n_scnum > 0)
		{
		  /* For PE, .lf and .ef get their value left alone,
		     while .bf gets relocated.  However, they all have
		     "real" section numbers, and need to be moved into
		     the new section.  */
		  isym.n_scnum = (*secpp)->output_section->target_index;
		  break;
		}
	      /* Fall through.  */
	    default:
	    case C_LABEL:  /* Not completely sure about these 2 */
	    case C_EXTDEF:
	    case C_BLOCK:
	    case C_EFCN:
	    case C_NULL:
	    case C_EXT:
	    case C_STAT:
	    case C_SECTION:
	      /* Compute new symbol location.  */
	    if (isym.n_scnum > 0)
	      {
		isym.n_scnum = (*secpp)->output_section->target_index;
		isym.n_value += ((*secpp)->output_offset
		                 + (*secpp)->output_section->vma
		                 - (*secpp)->vma);
	      }
	    break;

	    case C_FILE:
	      /* The value of a C_FILE symbol is the symbol index of
		 the next C_FILE symbol.  The value of the last C_FILE
		 symbol is the symbol index to the first external
		 symbol (actually, coff_renumber_symbols does not get
		 this right--it just sets the value of the last C_FILE
		 symbol to zero--and nobody has ever complained about
		 it).  We try to get this right, below, just before we
		 write the symbols out, but in the general case we may
		 have to write the symbol out twice.  */

	      if (finfo->last_file_index != -1
		  && finfo->last_file.n_value != (bfd_vma) output_index)
		{
		  /* We must correct the value of the last C_FILE
                     entry.  */
		  finfo->last_file.n_value = output_index;
		  if ((bfd_size_type) finfo->last_file_index >= syment_base)
		    {
		      /* The last C_FILE symbol is in this input file.  */
		      bfd_coff_swap_sym_out (output_bfd,
					     (PTR) &finfo->last_file,
					     (PTR) (finfo->outsyms
						    + ((finfo->last_file_index
							- syment_base)
						       * osymesz)));
		    }
		  else
		    {
		      file_ptr pos;

		      /* We have already written out the last C_FILE
			 symbol.  We need to write it out again.  We
			 borrow *outsym temporarily.  */
		      bfd_coff_swap_sym_out (output_bfd,
					     (PTR) &finfo->last_file,
					     (PTR) outsym);
		      pos = obj_sym_filepos (output_bfd);
		      pos += finfo->last_file_index * osymesz;
		      if (bfd_seek (output_bfd, pos, SEEK_SET) != 0
			  || bfd_bwrite (outsym, osymesz, output_bfd) != osymesz)
			return false;
		    }
		}

	      finfo->last_file_index = output_index;
	      finfo->last_file = isym;
	      break;

	    case C_NT_WEAK:
	      BFD_ASSERT(false);

	    }

	  /* If doing task linking, convert normal global function symbols to
	     static functions.  */
	  if (finfo->info->task_link && IS_EXTERNAL (input_bfd, isym))
	    isym.n_sclass = C_STAT;

	  /* Output the symbol.  */

	  bfd_coff_swap_sym_out (output_bfd, (PTR) &isym, (PTR) outsym);

	  *indexp = output_index;

	  if (global)
	    {
	      if (h == NULL)
		{
		  /* This can happen if there were errors earlier in
                     the link.  */
		  bfd_set_error (bfd_error_bad_value);
		  return false;
		}
	      h->indx = output_index;
	    }

	  output_index += add;
	  outsym += add * osymesz;
	}

      esym += add * isymesz;
      isymp += add;
      ++secpp;
      ++indexp;
      for (--add; add > 0; --add)
	{
	  *secpp++ = NULL;
	  *indexp++ = -1;
	}
    }

  /* Fix up the aux entries.  This must be done in a separate pass,
     because we don't know the correct symbol indices until we have
     already decided which symbols we are going to keep.  */

  esym = (bfd_byte *) obj_coff_external_syms (input_bfd);
  esym_end = esym + obj_raw_syment_count (input_bfd) * isymesz;
  isymp = finfo->internal_syms;
  indexp = finfo->sym_indices;
  sym_hash = obj_coff_sym_hashes (input_bfd);
  outsym = finfo->outsyms;
  while (esym < esym_end)
    {
      int add;

      add = 1 + isymp->n_numaux;

      if ((*indexp < 0
	   || (bfd_size_type) *indexp < syment_base)
	  && (*sym_hash == NULL
	      || (*sym_hash)->auxbfd != input_bfd))
	esym += add * isymesz;
      else
	{
	  struct coff_link_hash_entry *h;
	  int i;

	  h = NULL;
	  if (*indexp < 0)
	    {
	      h = *sym_hash;

	      /* The m68k-motorola-sysv assembler will sometimes
                 generate two symbols with the same name, but only one
                 will have aux entries.  */
	      BFD_ASSERT (isymp->n_numaux == 0
			  || h->numaux == isymp->n_numaux);
	    }

	  esym += isymesz;

	  if (h == NULL)
	    outsym += osymesz;

	  /* Handle the aux entries.  This handling is based on
	     coff_pointerize_aux.  I don't know if it always correct.  */
	  for (i = 0; i < isymp->n_numaux && esym < esym_end; i++)
	    {
	      union internal_auxent aux;
	      union internal_auxent *auxp;

	      if (h != NULL)
		auxp = h->aux + i;
	      else
		{
		  bfd_coff_swap_aux_in (input_bfd, (PTR) esym, isymp->n_type,
					isymp->n_sclass, i, isymp->n_numaux,
					(PTR) &aux);
		  auxp = &aux;
		}

	      if (isymp->n_sclass == C_FILE)
		{
		  /* If this is a long filename, we must put it in the
		     string table.  */
		  if (auxp->x_file.x_n.x_zeroes == 0
		      && auxp->x_file.x_n.x_offset != 0)
		    {
		      const char *filename;
		      bfd_size_type indx;

		      BFD_ASSERT (auxp->x_file.x_n.x_offset
				  >= STRING_SIZE_SIZE);
		      if (strings == NULL)
			{
			  strings = _bfd_coff_read_string_table (input_bfd);
			  if (strings == NULL)
			    return false;
			}
		      filename = strings + auxp->x_file.x_n.x_offset;
		      indx = _bfd_stringtab_add (finfo->strtab, filename,
						 hash, copy);
		      if (indx == (bfd_size_type) -1)
			return false;
		      auxp->x_file.x_n.x_offset = STRING_SIZE_SIZE + indx;
		    }
		}
	      else if (isymp->n_sclass == C_NT_WEAK)
		{
		  struct coff_link_hash_entry *h1 = *sym_hash;
		  BFD_ASSERT(h1->root.type == bfd_link_hash_indirect
	      		 && h1->root.u.i.info.alias)
		  /* C_NT_WEAK needs to propigate the change in the AUX
		     entry.  (Path used in .so case, only.) */
		  while (h1->root.type == bfd_link_hash_indirect
	      		 && h1->root.u.i.info.alias)
		    h1 = (struct coff_link_hash_entry *) h1->root.u.i.link;
		  auxp->x_sym.x_tagndx.l = h1->indx;
		}
	      else if (isymp->n_sclass != C_STAT || isymp->n_type != T_NULL)
		{
		  unsigned long indx;

		  if (ISFCN (isymp->n_type)
		      || ISTAG (isymp->n_sclass)
		      || isymp->n_sclass == C_BLOCK
		      || isymp->n_sclass == C_FCN)
		    {
		      indx = auxp->x_sym.x_fcnary.x_fcn.x_endndx.l;
		      if (indx > 0
			  && indx < obj_raw_syment_count (input_bfd))
			{
			  /* We look forward through the symbol for
                             the index of the next symbol we are going
                             to include.  I don't know if this is
                             entirely right.  */
			  while ((finfo->sym_indices[indx] < 0
				  || ((bfd_size_type) finfo->sym_indices[indx]
				      < syment_base))
				 && indx < obj_raw_syment_count (input_bfd))
			    ++indx;
			  if (indx >= obj_raw_syment_count (input_bfd))
			    indx = output_index;
			  else
			    indx = finfo->sym_indices[indx];
			  auxp->x_sym.x_fcnary.x_fcn.x_endndx.l = indx;
			}
		    }

		  indx = auxp->x_sym.x_tagndx.l;
		  if (indx > 0 && indx < obj_raw_syment_count (input_bfd))
		    {
		      long symindx;

		      symindx = finfo->sym_indices[indx];
		      if (symindx < 0)
			auxp->x_sym.x_tagndx.l = 0;
		      else
			auxp->x_sym.x_tagndx.l = symindx;
		    }

		  /* The .bf symbols are supposed to be linked through
		     the endndx field.  We need to carry this list
		     across object files.  */
		  if (i == 0
		      && h == NULL
		      && isymp->n_sclass == C_FCN
		      && (isymp->_n._n_n._n_zeroes != 0
			  || isymp->_n._n_n._n_offset == 0)
		      && isymp->_n._n_name[0] == '.'
		      && isymp->_n._n_name[1] == 'b'
		      && isymp->_n._n_name[2] == 'f'
		      && isymp->_n._n_name[3] == '\0')
		    {
		      if (finfo->last_bf_index != -1)
			{
			  finfo->last_bf.x_sym.x_fcnary.x_fcn.x_endndx.l =
			    *indexp;

			  if ((bfd_size_type) finfo->last_bf_index
			      >= syment_base)
			    {
			      PTR auxout;

			      /* The last .bf symbol is in this input
				 file.  This will only happen if the
				 assembler did not set up the .bf
				 endndx symbols correctly.  */
			      auxout = (PTR) (finfo->outsyms
					      + ((finfo->last_bf_index
						  - syment_base)
						 * osymesz));
			      bfd_coff_swap_aux_out (output_bfd,
						     (PTR) &finfo->last_bf,
						     isymp->n_type,
						     isymp->n_sclass,
						     0, isymp->n_numaux,
						     auxout);
			    }
			  else
			    {
			      file_ptr pos;

			      /* We have already written out the last
                                 .bf aux entry.  We need to write it
                                 out again.  We borrow *outsym
                                 temporarily.  FIXME: This case should
                                 be made faster.  */
			      bfd_coff_swap_aux_out (output_bfd,
						     (PTR) &finfo->last_bf,
						     isymp->n_type,
						     isymp->n_sclass,
						     0, isymp->n_numaux,
						     (PTR) outsym);
			      pos = obj_sym_filepos (output_bfd);
			      pos += finfo->last_bf_index * osymesz;
			      if (bfd_seek (output_bfd, pos, SEEK_SET) != 0
				  || (bfd_bwrite (outsym, osymesz, output_bfd)
				      != osymesz))
				return false;
			    }
			}

		      if (auxp->x_sym.x_fcnary.x_fcn.x_endndx.l != 0)
			finfo->last_bf_index = -1;
		      else
			{
			  /* The endndx field of this aux entry must
                             be updated with the symbol number of the
                             next .bf symbol.  */
			  finfo->last_bf = *auxp;
			  finfo->last_bf_index = (((outsym - finfo->outsyms)
						   / osymesz)
						  + syment_base);
			}
		    }
		}

	      if (h == NULL)
		{
		  bfd_coff_swap_aux_out (output_bfd, (PTR) auxp, isymp->n_type,
					 isymp->n_sclass, i, isymp->n_numaux,
					 (PTR) outsym);
		  outsym += osymesz;
		}

	      esym += isymesz;
	    }
	}

      indexp += add;
      isymp += add;
      sym_hash += add;
    }

  /* Relocate the line numbers, unless we are stripping them.  */
  if (finfo->info->strip == strip_none
      || finfo->info->strip == strip_some)
    {
      for (o = input_bfd->sections; o != NULL; o = o->next)
	{
	  bfd_vma offset;
	  bfd_byte *eline;
	  bfd_byte *elineend;
	  bfd_byte *oeline;
	  boolean skipping;
	  file_ptr pos;
	  bfd_size_type amt;

	  /* FIXME: If SEC_HAS_CONTENTS is not for the section, then
	     build_link_order in ldwrite.c will not have created a
	     link order, which means that we will not have seen this
	     input section in _bfd_coff_final_link, which means that
	     we will not have allocated space for the line numbers of
	     this section.  I don't think line numbers can be
	     meaningful for a section which does not have
	     SEC_HAS_CONTENTS set, but, if they do, this must be
	     changed.  */
	  if (o->lineno_count == 0
	      || (o->output_section->flags & SEC_HAS_CONTENTS) == 0)
	    continue;

	  if (bfd_seek (input_bfd, o->line_filepos, SEEK_SET) != 0
	      || bfd_bread (finfo->linenos, linesz * o->lineno_count,
			   input_bfd) != linesz * o->lineno_count)
	    return false;

	  offset = o->output_section->vma + o->output_offset - o->vma;
	  eline = finfo->linenos;
	  oeline = finfo->linenos;
	  elineend = eline + linesz * o->lineno_count;
	  skipping = false;
	  for (; eline < elineend; eline += linesz)
	    {
	      struct internal_lineno iline;

	      bfd_coff_swap_lineno_in (input_bfd, (PTR) eline, (PTR) &iline);

	      if (iline.l_lnno != 0)
		iline.l_addr.l_paddr += offset;
	      else if (iline.l_addr.l_symndx >= 0
		       && ((unsigned long) iline.l_addr.l_symndx
			   < obj_raw_syment_count (input_bfd)))
		{
		  long indx;

		  indx = finfo->sym_indices[iline.l_addr.l_symndx];

		  if (indx < 0)
		    {
		      /* These line numbers are attached to a symbol
			 which we are stripping.  We must discard the
			 line numbers because reading them back with
			 no associated symbol (or associating them all
			 with symbol #0) will fail.  We can't regain
			 the space in the output file, but at least
			 they're dense.  */
		      skipping = true;
		    }
		  else
		    {
		      struct internal_syment is;
		      union internal_auxent ia;

		      /* Fix up the lnnoptr field in the aux entry of
			 the symbol.  It turns out that we can't do
			 this when we modify the symbol aux entries,
			 because gas sometimes screws up the lnnoptr
			 field and makes it an offset from the start
			 of the line numbers rather than an absolute
			 file index.  */
		      bfd_coff_swap_sym_in (output_bfd,
					    (PTR) (finfo->outsyms
						   + ((indx - syment_base)
						      * osymesz)),
					    (PTR) &is);
		      if ((ISFCN (is.n_type)
			   || is.n_sclass == C_BLOCK)
			  && is.n_numaux >= 1)
			{
			  PTR auxptr;

			  auxptr = (PTR) (finfo->outsyms
					  + ((indx - syment_base + 1)
					     * osymesz));
			  bfd_coff_swap_aux_in (output_bfd, auxptr,
						is.n_type, is.n_sclass,
						0, is.n_numaux, (PTR) &ia);
			  ia.x_sym.x_fcnary.x_fcn.x_lnnoptr =
			    (o->output_section->line_filepos
			     + o->output_section->lineno_count * linesz
			     + eline - finfo->linenos);
			  bfd_coff_swap_aux_out (output_bfd, (PTR) &ia,
						 is.n_type, is.n_sclass, 0,
						 is.n_numaux, auxptr);
			}

		      skipping = false;
		    }

		  iline.l_addr.l_symndx = indx;
		}

	      if (!skipping)
	        {
		  bfd_coff_swap_lineno_out (output_bfd, (PTR) &iline,
					    (PTR) oeline);
		  oeline += linesz;
		}
	    }

	  pos = o->output_section->line_filepos;
	  pos += o->output_section->lineno_count * linesz;
	  amt = oeline - finfo->linenos;
	  if (bfd_seek (output_bfd, pos, SEEK_SET) != 0
	      || bfd_bwrite (finfo->linenos, amt, output_bfd) != amt)
	    return false;

	  o->output_section->lineno_count += amt / linesz;
	}
    }

  /* If we swapped out a C_FILE symbol, guess that the next C_FILE
     symbol will be the first symbol in the next input file.  In the
     normal case, this will save us from writing out the C_FILE symbol
     again.  */
  if (finfo->last_file_index != -1
      && (bfd_size_type) finfo->last_file_index >= syment_base)
    {
      finfo->last_file.n_value = output_index;
      bfd_coff_swap_sym_out (output_bfd, (PTR) &finfo->last_file,
			     (PTR) (finfo->outsyms
 				    + ((finfo->last_file_index - syment_base)
 				       * osymesz)));
    }

  /* Write the modified symbols to the output file.  */
  if (outsym > finfo->outsyms)
    {
      file_ptr pos;
      bfd_size_type amt;

      pos = obj_sym_filepos (output_bfd) + syment_base * osymesz;
      amt = outsym - finfo->outsyms;
      if (bfd_seek (output_bfd, pos, SEEK_SET) != 0
	  || bfd_bwrite (finfo->outsyms, amt, output_bfd) != amt)
	return false;

      BFD_ASSERT ((obj_raw_syment_count (output_bfd)
		   + (outsym - finfo->outsyms) / osymesz)
		  == output_index);

      obj_raw_syment_count (output_bfd) = output_index;
    }

  /* Relocate the contents of each section.  */
  adjust_symndx = coff_backend_info (input_bfd)->_bfd_coff_adjust_symndx;
  for (o = input_bfd->sections; o != NULL; o = o->next)
    {
      bfd_byte *contents;
      struct coff_section_tdata *secdata;

      if (! o->linker_mark)
	{
	  /* This section was omitted from the link.  */
	  continue;
	}

#ifdef DYNAMIC_LINKING
      if ((o->flags & SEC_LINKER_CREATED) != 0)
        {
          /* Section was created by coff_link_create_dynamic_sections
             or somesuch.  */
          continue;
        }
#endif

      if ((o->flags & SEC_HAS_CONTENTS) == 0
	  || (o->_raw_size == 0 && (o->flags & SEC_RELOC) == 0))
	{
	  if ((o->flags & SEC_RELOC) != 0
	      && o->reloc_count != 0)
	    {
	      ((*_bfd_error_handler)
	       (_("%s: relocs in section `%s', but it has no contents"),
		bfd_archive_filename (input_bfd),
		bfd_get_section_name (input_bfd, o)));
	      bfd_set_error (bfd_error_no_contents);
	      return false;
	    }

	  continue;
	}

      secdata = coff_section_data (input_bfd, o);
      if (secdata != NULL && secdata->contents != NULL)
	contents = secdata->contents;
      else
	{
	  if (! bfd_get_section_contents (input_bfd, o, finfo->contents,
					  (file_ptr) 0, o->_raw_size))
	    return false;
	  contents = finfo->contents;
	}

      if ((o->flags & SEC_RELOC) != 0)
	{
	  int target_index;
	  struct internal_reloc *internal_relocs;
	  struct internal_reloc *irel;

	  /* Read in the relocs.  */
	  target_index = o->output_section->target_index;
	  internal_relocs = (_bfd_coff_read_internal_relocs
			     (input_bfd, o, false, finfo->external_relocs,
			      finfo->info->relocateable,
			      (finfo->info->relocateable
			       ? (finfo->section_info[target_index].relocs
				  + o->output_section->reloc_count)
			       : finfo->internal_relocs)));
	  if (internal_relocs == NULL)
	    return false;

	  /* Call processor specific code to relocate the section
             contents.  */
	  if (! bfd_coff_relocate_section (output_bfd, finfo->info,
					   input_bfd, o,
					   contents,
					   internal_relocs,
					   finfo->internal_syms,
					   finfo->sec_ptrs))
	    return false;

	  if (finfo->info->relocateable)
	    {
	      bfd_vma offset;
	      struct internal_reloc *irelend;
	      struct coff_link_hash_entry **rel_hash;

	      offset = o->output_section->vma + o->output_offset - o->vma;
	      irel = internal_relocs;
	      irelend = irel + o->reloc_count;
	      rel_hash = (finfo->section_info[target_index].rel_hashes
			  + o->output_section->reloc_count);
	      for (; irel < irelend; irel++, rel_hash++)
		{
		  struct coff_link_hash_entry *h;
		  boolean adjusted;

		  *rel_hash = NULL;

		  /* Adjust the reloc address and symbol index.  */

		  irel->r_vaddr += offset;

		  if (irel->r_symndx == -1)
		    continue;

		  if (adjust_symndx)
		    {
		      if (! (*adjust_symndx) (output_bfd, finfo->info,
					      input_bfd, o, irel,
					      &adjusted))
			return false;
		      if (adjusted)
			continue;
		    }

		  h = obj_coff_sym_hashes (input_bfd)[irel->r_symndx];
		  if (h != NULL)
		    {
		      /* This is a global symbol.  */
		      if (h->indx >= 0)
			irel->r_symndx = h->indx;
		      else
			{
			  /* This symbol is being written at the end
			     of the file, and we do not yet know the
			     symbol index.  We save the pointer to the
			     hash table entry in the rel_hash list.
			     We set the indx field to -2 to indicate
			     that this symbol must not be stripped.  */
			  *rel_hash = h;
			  h->indx = -2;
			}
		    }
		  else
		    {
		      long indx;

		      indx = finfo->sym_indices[irel->r_symndx];
		      if (indx != -1)
			irel->r_symndx = indx;
		      else
			{
			  struct internal_syment *is;
			  const char *name;
			  char buf[SYMNMLEN + 1];

			  /* This reloc is against a symbol we are
                             stripping.  This should have been handled
			     by the 'dont_skip_symbol' code in the while
			     loop at the top of this function.  */

			  is = finfo->internal_syms + irel->r_symndx;

			  name = (_bfd_coff_internal_syment_name
				  (input_bfd, is, buf));
			  if (name == NULL)
			    return false;

			  if (! ((*finfo->info->callbacks->unattached_reloc)
				 (finfo->info, name, input_bfd, o,
				  irel->r_vaddr)))
			    return false;
			}
		    }
		}

	      o->output_section->reloc_count += o->reloc_count;
	    }
	}

      /* Write out the modified section contents.  */
      if (secdata == NULL || secdata->stab_info == NULL)
	{
	  file_ptr loc = o->output_offset * bfd_octets_per_byte (output_bfd);
	  bfd_size_type amt = (o->_cooked_size != 0
			       ? o->_cooked_size : o->_raw_size);
	  if (! bfd_set_section_contents (output_bfd, o->output_section,
					  contents, loc, amt))
	    return false;
	}
      else
	{
	  if (! (_bfd_write_section_stabs
		 (output_bfd, &coff_hash_table (finfo->info)->stab_info,
		  o, &secdata->stab_info, contents)))
	    return false;
	}
    }

  if (! finfo->info->keep_memory)
    {
      if (! _bfd_coff_free_symbols (input_bfd))
	return false;
    }

  return true;
}

/* Write out a global symbol.  Called via coff_link_hash_traverse.  */

boolean
_bfd_coff_write_global_sym (h, data)
     struct coff_link_hash_entry *h;
     PTR data;
{
  struct coff_final_link_info *finfo = (struct coff_final_link_info *) data;
  bfd *output_bfd;
  struct internal_syment isym;
  bfd_size_type symesz;
  unsigned int i;
  file_ptr pos;

  boolean emit_classical = true;
  boolean emit_dynamic = true;
  struct coff_link_hash_entry *h_real = NULL;

  /* This function performs two apparently related, but distinct, functions:
     it outputs the global symbols for the conventional symbol table
     (function local symbols and function symbols were handled above).

     It also outputs the dynamic symbols needed for dynamic linking.

     The criteria for determining when a symbol is actually output to each of 
     the two symbol tables are somewhat different, but much of the preparation
     is shared.  Note that this code will output symbols for functions in
     the dynmaic symbol table, but never for the conventional one */

  output_bfd = finfo->output_bfd;

  if (h->root.type == bfd_link_hash_warning)
    {
      h = (struct coff_link_hash_entry *) h->root.u.i.link;
      if (h->root.type == bfd_link_hash_new)
	return true;
    }

  /* There are situations where we might get called more than once, and
     if that happens, the hash table (and plt) is, well..., hash. */
  if ((h->coff_link_hash_flags & COFF_LINK_HASH_EMITTED) != 0)
      return true;
  h->coff_link_hash_flags |= COFF_LINK_HASH_EMITTED;

  if (h->root.type == bfd_link_hash_indirect
	&& h->root.u.i.info.alias)
    {
      /* C_NT_WEAK needs to propigate the change in the AUX
	 entry.  To do that, the indx value must already be set.
	 If necessary, recursively call ourself to achieve that.
	 (That is, be sure the strong symbol is emitted before the
	 weak one, so we know its index.) */
      h_real = h;
      while (h_real->root.type == bfd_link_hash_indirect
	     && h_real->root.u.i.info.alias)
	h_real = (struct coff_link_hash_entry *) h_real->root.u.i.link;
      if (h_real->indx < 0)
	_bfd_coff_write_global_sym (h_real, data);
    }

#ifdef DYNAMIC_LINKING
  /* If we are not creating a shared library, and this symbol is
     referenced by a shared library but is not defined anywhere, then
     warn that it is undefined.  If we do not do this, the runtime
     linker will complain that the symbol is undefined when the
     program is run.  We don't have to worry about symbols that are
     referenced by regular files, because we will already have issued
     warnings for them.  

     Because of COFF order requirements, symbols are output in 2 places
     (here and in ...link_input_bfd).  However, we didn't output any
     undefined symbols there, so this is the only place we need the
     check for undefined.  */
  if (! finfo->info->relocateable
      && ! finfo->info->shared
      && h->root.type == bfd_link_hash_undefined
      && (h->coff_link_hash_flags & COFF_LINK_HASH_REF_DYNAMIC) != 0
      && (h->coff_link_hash_flags & COFF_LINK_HASH_REF_REGULAR) == 0)
    {
      if (! ((*finfo->info->callbacks->undefined_symbol)
	     (finfo->info, h->root.root.string, h->root.u.undef.abfd,
	      (asection *) NULL, 0, true)))
	{
	  return false;
	}
    }
#endif

  /* If it doesn't have a dynindx, we're surely not going to emit it */
  emit_dynamic = h->dynindx != -1;

  if (h->indx >= 0)
     /* If it was emitted previously..., skip it. */
     emit_classical = false;
  else if (h->indx == -2)
    /* used by a reloc... must keep */
    emit_classical = true;
#ifdef DYNAMIC_LINKING
  else if (((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) != 0
	    || (h->coff_link_hash_flags & COFF_LINK_HASH_REF_DYNAMIC) != 0)
	   && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) == 0
	   && (h->coff_link_hash_flags & COFF_LINK_HASH_REF_REGULAR) == 0)
    {
       /* Never mentioned by a regular file */
       /* there's still work to do, so we can't return */
       emit_classical = false;
       emit_dynamic = false;
    }
#endif
  else if (finfo->info->strip == strip_some
	   && bfd_hash_lookup (finfo->info->keep_hash,
				   h->root.root.string,
				   false, false) == NULL)
    /* it's a symbol we don't retain when selectively stripping */
    emit_classical = false;
  else if (finfo->info->strip == strip_all)
    /* or if we just strip everything */
    emit_classical = false;
  else
    /* otherwise, we'll keep it */
    emit_classical = true;

  switch (h->root.type)
    {
    default:
    case bfd_link_hash_new:
    case bfd_link_hash_warning:
      abort ();
      return false;

    case bfd_link_hash_undefined:
    case bfd_link_hash_undefweak:
      isym.n_scnum = N_UNDEF;
      isym.n_value = 0;
      break;

    case bfd_link_hash_defined:
    case bfd_link_hash_defweak:
      /* discard internally synthesized names (.idata$[45]^?nn) */
      if ((h->coff_link_hash_flags & COFF_LINK_HASH_PE_SECTION_SYMBOL) != 0 
	  && h->root.root.string[0]=='.' && h->root.root.string[1] == 'i'
	  && h->root.root.string[8]==0x7f
	  && (h->root.root.string[7]=='4'||h->root.root.string[7]=='5')
	  && strncmp(h->root.root.string, ".idata$",7)==0)
	{
	  /* phew, that's an ugly test, but it's right and fast.
	     if performance becomes an issue, it's probably safe to skip
	     testing name[7]. */
	  return true;
	}
      {
	asection *sec;

	sec = h->root.u.def.section->output_section;
#ifdef DYNAMIC_LINKING
#ifndef USE_COPY_RELOC
	/* When a symbol has no output section, it means that the symbol
	   came in from a prior .so, and is completely defined there.  
	   (That is, it's a data symbol, usually.  Code symbols get an
	   output section, that of the thunk.) (Shouldn't ever happen 
	   if COPY relocations are in use.)
	   We want to output the symbol (so our relocations can see it) 
	   but as an undefined symbol. */
	if (sec == NULL)
	  {
	     isym.n_scnum = N_UNDEF;
	     isym.n_value = 0;
	  }
        else
#endif
#endif
	  { // NOTE: deferred indentation fix
	if (bfd_is_abs_section (sec))
	  isym.n_scnum = N_ABS;
	else
	  isym.n_scnum = sec->target_index;
	/* for a section symbol, the value is noise, so we'll take what
	   we get. */
	isym.n_value = (h->root.u.def.value
			+ sec->vma
			+ h->root.u.def.section->output_offset);
	  } // END
      }
      break;

    case bfd_link_hash_common:
      isym.n_scnum = N_UNDEF;
      isym.n_value = h->root.u.c.size;
      break;

#ifdef DYNAMIC_LINKING
    case bfd_link_hash_indirect:
      /* These symbols are created in two ways... by symbol versioning
	 and by C_NT_WEAK symbols.

	 For symbol versioning, they point
         to the decorated version of the name.  For example, if the
         symbol foo@@GNU_1.2 is the default, which should be used when
         foo is used with no version, then we add an indirect symbol
         foo which points to foo@@GNU_1.2.  We ignore these symbols,
         since the indirected symbol is already in the hash table.  If
         the indirect symbol is non-COFF, fall through and output it.

	 If it's a weak symbol, it need to be treated more-or-less
	 normally.

	 */
      if (h->root.u.i.info.alias)
	{
	  isym.n_scnum = N_UNDEF;
	  isym.n_value = 0;
	  break;
	}
      if ((h->coff_link_hash_flags & COFF_LINK_NON_COFF) == 0)
        return true;

      if (h->root.u.i.link->type == bfd_link_hash_new)
        return true;

      return (_bfd_coff_write_global_sym
              ((struct coff_link_hash_entry *) h->root.u.i.link, data));
#else
    case bfd_link_hash_indirect:
        /* Just ignore these.  They can't be handled anyhow.  */
        return true;
#endif
    }

#ifdef DYNAMIC_LINKING /* [ */
  /* Give the processor backend a chance to tweak the symbol
     value, and also to finish up anything that needs to be done
     for this symbol.  All symbols require a peek, because some
     symbols are forced local, but need work here anyway. */
  if (coff_hash_table (finfo->info)->dynamic_sections_created
      && !bfd_coff_backend_finish_dynamic_symbol
	 (finfo->output_bfd, finfo->info, h, &isym))
    {
	emit_dynamic = false;
    }
#endif /* ] */

  if (strlen (h->root.root.string) <= SYMNMLEN)
    strncpy (isym._n._n_name, h->root.root.string, SYMNMLEN);
  else
    {
      boolean hash;
      bfd_size_type indx;

      hash = true;
      if ((output_bfd->flags & BFD_TRADITIONAL_FORMAT) != 0)
	hash = false;
      indx = _bfd_stringtab_add (finfo->strtab, h->root.root.string, hash,
				 false);
      if (indx == (bfd_size_type) -1)
	{
	  finfo->failed = true;
	  return false;
	}
      isym._n._n_n._n_zeroes = 0;
      isym._n._n_n._n_offset = STRING_SIZE_SIZE + indx;
    }

  isym.n_sclass = h->class;
  isym.n_type = h->type;

#ifdef COFF_WITH_PE  /// This is NOT DL; move elsewhere soon.  Yeah!
  if (h->root.type == bfd_link_hash_undefweak)
    isym.n_sclass = C_NT_WEAK;
#endif

  if (isym.n_sclass == C_NULL)
    isym.n_sclass = C_EXT;

  /* If doing task linking and this is the pass where we convert
     defined globals to statics, then do that conversion now.  If the
     symbol is not being converted, just ignore it and it will be
     output during a later pass.  */
  if (finfo->global_to_static)
    {
      if (! IS_EXTERNAL (output_bfd, isym))
	return true;

      isym.n_sclass = C_STAT;
    }

  /* When a weak symbol is not overriden by a strong one,
     turn it into an external symbol when not building a
     shared or relocateable object.  */
  if (! finfo->info->shared
      && ! finfo->info->relocateable
      && IS_WEAK_EXTERNAL (finfo->output_bfd, isym))
    isym.n_sclass = C_EXT;

  if (emit_classical)
    { // NOTE: Another deferred indentation.
  isym.n_numaux = h->numaux;

  bfd_coff_swap_sym_out (output_bfd, (PTR) &isym, (PTR) finfo->outsyms);

  symesz = bfd_coff_symesz (output_bfd);

  pos = obj_sym_filepos (output_bfd);
  pos += obj_raw_syment_count (output_bfd) * symesz;
  if (bfd_seek (output_bfd, pos, SEEK_SET) != 0
      || bfd_bwrite (finfo->outsyms, symesz, output_bfd) != symesz)
    {
      finfo->failed = true;
      return false;
    }

  h->indx = obj_raw_syment_count (output_bfd);

  ++obj_raw_syment_count (output_bfd);

  /* Write out any associated aux entries.  Most of the aux entries
     will have been modified in _bfd_coff_link_input_bfd.  We have to
     handle section aux entries here, now that we have the final
     relocation and line number counts.  */
  for (i = 0; i < isym.n_numaux; i++)
    {
      union internal_auxent *auxp;

      auxp = h->aux + i;

      /* Look for a section aux entry here using the same tests that
         coff_swap_aux_out uses.  */
      if (i == 0
	  && (isym.n_sclass == C_STAT
	      || isym.n_sclass == C_HIDDEN)
	  && isym.n_type == T_NULL
	  && (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak))
	{
	  asection *sec;

	  sec = h->root.u.def.section->output_section;
	  if (sec != NULL)
	    {
	      auxp->x_scn.x_scnlen = (sec->_cooked_size != 0
				      ? sec->_cooked_size
				      : sec->_raw_size);

	      /* For PE, an overflow on the final link reportedly does
                 not matter.  FIXME: Why not?  */

	      if (sec->reloc_count > 0xffff
		  && (! obj_pe (output_bfd)
		      || finfo->info->relocateable))
		(*_bfd_error_handler)
		  (_("%s: %s: reloc overflow: 0x%lx > 0xffff"),
		   bfd_get_filename (output_bfd),
		   bfd_get_section_name (output_bfd, sec),
		   sec->reloc_count);

	      if (sec->lineno_count > 0xffff
		  && (! obj_pe (output_bfd)
		      || finfo->info->relocateable))
		(*_bfd_error_handler)
		  (_("%s: warning: %s: line number overflow: 0x%lx > 0xffff"),
		   bfd_get_filename (output_bfd),
		   bfd_get_section_name (output_bfd, sec),
		   sec->lineno_count);

	      auxp->x_scn.x_nreloc = sec->reloc_count;
	      auxp->x_scn.x_nlinno = sec->lineno_count;
	      auxp->x_scn.x_checksum = 0;
	      auxp->x_scn.x_associated = 0;
	      auxp->x_scn.x_comdat = 0;
	    }
	}

      if (h->root.type == bfd_link_hash_indirect
	  && h->root.u.i.info.alias)
	{
          BFD_ASSERT(isym.n_sclass == C_NT_WEAK)
	  /* C_NT_WEAK needs to propigate the change in the AUX
	     entry.   We already have h_real from above. */
	  auxp->x_sym.x_tagndx.l = h_real->indx;
	}

      bfd_coff_swap_aux_out (output_bfd, (PTR) auxp, isym.n_type,
			     isym.n_sclass, (int) i, isym.n_numaux,
			     (PTR) finfo->outsyms);
      if (bfd_bwrite (finfo->outsyms, symesz, output_bfd) != symesz)
	{
	  finfo->failed = true;
	  return false;
	}
      ++obj_raw_syment_count (output_bfd);
    }
  }  // END DEFERRED INDENT

#ifdef DYNAMIC_LINKING /* [ */
  /* If this symbol should also be put in the .dynsym section, then put it
     there now.  We have already know the symbol index.  We also fill
     in the entry in the .hash section.  */
  if (emit_dynamic)  
    {
      if (h->dynindx != -1
	  && coff_hash_table (finfo->info)->dynamic_sections_created)
	{
	   _bfd_coff_output_dynamic_symbol(&isym, h, finfo);
	}
    }
#endif /* ] */

  return true;
}

/* Write out task global symbols, converting them to statics.  Called
   via coff_link_hash_traverse.  Calls bfd_coff_write_global_sym to do
   the dirty work, if the symbol we are processing needs conversion.  */

boolean
_bfd_coff_write_task_globals (h, data)
     struct coff_link_hash_entry *h;
     PTR data;
{
  struct coff_final_link_info *finfo = (struct coff_final_link_info *) data;
  boolean rtnval = true;
  boolean save_global_to_static;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct coff_link_hash_entry *) h->root.u.i.link;

  if (h->indx < 0)
    {
      switch (h->root.type)
	{
	case bfd_link_hash_defined:
	case bfd_link_hash_defweak:
	  save_global_to_static = finfo->global_to_static;
	  finfo->global_to_static = true;
	  rtnval = _bfd_coff_write_global_sym (h, data);
	  finfo->global_to_static = save_global_to_static;
	  break;
	default:
	  break;
	}
    }
  return (rtnval);
}

/* Handle a link order which is supposed to generate a reloc.  */

boolean
_bfd_coff_reloc_link_order (output_bfd, finfo, output_section, link_order)
     bfd *output_bfd;
     struct coff_final_link_info *finfo;
     asection *output_section;
     struct bfd_link_order *link_order;
{
  reloc_howto_type *howto;
  struct internal_reloc *irel;
  struct coff_link_hash_entry **rel_hash_ptr;

  howto = bfd_reloc_type_lookup (output_bfd, link_order->u.reloc.p->reloc);
  if (howto == NULL)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  if (link_order->u.reloc.p->addend != 0)
    {
      bfd_size_type size;
      bfd_byte *buf;
      bfd_reloc_status_type rstat;
      boolean ok;
      file_ptr loc;

      size = bfd_get_reloc_size (howto);
      buf = (bfd_byte *) bfd_zmalloc (size);
      if (buf == NULL)
	return false;

      rstat = _bfd_relocate_contents (howto, output_bfd,
				      (bfd_vma) link_order->u.reloc.p->addend,\
				      buf);
      switch (rstat)
	{
	case bfd_reloc_ok:
	  break;
	default:
	case bfd_reloc_outofrange:
	  abort ();
	case bfd_reloc_overflow:
	  if (! ((*finfo->info->callbacks->reloc_overflow)
		 (finfo->info,
		  (link_order->type == bfd_section_reloc_link_order
		   ? bfd_section_name (output_bfd,
				       link_order->u.reloc.p->u.section)
		   : link_order->u.reloc.p->u.name),
		  howto->name, link_order->u.reloc.p->addend,
		  (bfd *) NULL, (asection *) NULL, (bfd_vma) 0)))
	    {
	      free (buf);
	      return false;
	    }
	  break;
	}
      loc = link_order->offset * bfd_octets_per_byte (output_bfd);
      ok = bfd_set_section_contents (output_bfd, output_section, (PTR) buf,
                                     loc, size);
      free (buf);
      if (! ok)
	return false;
    }

  /* Store the reloc information in the right place.  It will get
     swapped and written out at the end of the final_link routine.  */

  irel = (finfo->section_info[output_section->target_index].relocs
	  + output_section->reloc_count);
  rel_hash_ptr = (finfo->section_info[output_section->target_index].rel_hashes
		  + output_section->reloc_count);

  memset (irel, 0, sizeof (struct internal_reloc));
  *rel_hash_ptr = NULL;

  irel->r_vaddr = output_section->vma + link_order->offset;

  if (link_order->type == bfd_section_reloc_link_order)
    {
      /* We need to somehow locate a symbol in the right section.  The
         symbol must either have a value of zero, or we must adjust
         the addend by the value of the symbol.  FIXME: Write this
         when we need it.  The old linker couldn't handle this anyhow.  */
      abort ();
      *rel_hash_ptr = NULL;
      irel->r_symndx = 0;
    }
  else
    {
      struct coff_link_hash_entry *h;

      h = ((struct coff_link_hash_entry *)
	   bfd_wrapped_link_hash_lookup (output_bfd, finfo->info,
					 link_order->u.reloc.p->u.name,
					 false, false, true));
      if (h != NULL)
	{
	  if (h->indx >= 0)
	    irel->r_symndx = h->indx;
	  else
	    {
	      /* Set the index to -2 to force this symbol to get
		 written out.  */
	      h->indx = -2;
	      *rel_hash_ptr = h;
	      irel->r_symndx = 0;
	    }
	}
      else
	{
	  if (! ((*finfo->info->callbacks->unattached_reloc)
		 (finfo->info, link_order->u.reloc.p->u.name, (bfd *) NULL,
		  (asection *) NULL, (bfd_vma) 0)))
	    return false;
	  irel->r_symndx = 0;
	}
    }

  /* FIXME: Is this always right?  */
  irel->r_type = howto->type;

  /* r_size is only used on the RS/6000, which needs its own linker
     routines anyhow.  r_extern is only used for ECOFF.  */

  /* FIXME: What is the right value for r_offset?  Is zero OK?  */

  ++output_section->reloc_count;

  return true;
}

/* A basic reloc handling routine which may be used by processors with
   simple relocs.  */

boolean
_bfd_coff_generic_relocate_section (output_bfd, info, input_bfd,
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
  bfd_vma imagebase = 0;
#ifdef DYNAMIC_LINKING
  asection *sgot;
  asection *splt = NULL;
  asection *sreloc = NULL;
  asection *srelgot = NULL;
  boolean dynamic;
  bfd_vma *local_got_offsets = NULL;
  bfd_size_type symrsz = bfd_coff_relsz(output_bfd);
  boolean is_stab_section = false;

  dynamic = coff_hash_table (info)->dynamic_sections_created;
  /* In case some PIC code slipped into our world when linking 
     statically.  */
  local_got_offsets = coff_local_got_offsets (input_bfd);
  is_stab_section = 
      strncmp(bfd_get_section_name(input_bfd,input_section),".stab",5) == 0;

  sreloc = coff_hash_table (info)->sreloc;
  splt = coff_hash_table(info)->splt;
  srelgot = coff_hash_table(info)->srelgot;
  /* BFD_ASSERT (sreloc != NULL); -- it gets made on demand */
  /* BFD_ASSERT (srelgot != NULL); -- it gets made on demand */
  sgot = coff_hash_table(info)->sgot;
  if (dynamic)
    {
      BFD_ASSERT (splt != NULL);
    }
#endif

  if (pe_data(output_bfd) != NULL)
      imagebase = pe_data(output_bfd)->pe_opthdr.ImageBase;

  rel = relocs;
  relend = rel + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      long symndx;
      struct coff_link_hash_entry *h;
      struct coff_link_hash_entry *h_real;
      struct internal_syment *sym;
      bfd_vma addend;
      bfd_vma val;
      reloc_howto_type *howto;
      bfd_reloc_status_type rstat;
      enum coff_symbol_classification classification;
      asection *sec;
      boolean need_imagebase;  /* Not all relocs get ImageBase */
      boolean need_static_reloc, need_dynamic_reloc;  /* whether to omit them */
      boolean valIsValid;  /* whether the computed val is to be trusted */
      struct internal_reloc outrel;

      symndx = rel->r_symndx;

      if (symndx == -1)
	{
	  h = NULL;
	  sym = NULL;
	}
      else if (symndx < 0
	       || (unsigned long) symndx >= obj_raw_syment_count (input_bfd))
	{
	  (*_bfd_error_handler)
	    ("%s: illegal symbol index %ld in relocs",
	     bfd_archive_filename (input_bfd), symndx);
	  return false;
	}
      else
	{
	  classification = bfd_coff_classify_symbol(input_bfd, &syms[symndx]);
	  if (classification == COFF_SYMBOL_PE_SECTION)
	    {
	      /* a reference to a section definition wants to just use the
		 section information (which is what this does).  We can't
		 look at h, because the (local) symbol table entry might
		 be an ordinary reference to a section symbol OR a section
		 definition, and those are treated differently.  Only
		 the local symbol table tells us which. */
	      h = NULL;
	    }
	  else
	    {
	      /* If this is a relocateable link, and we're dealing with
		 a relocation against a symbol (rather than a section),
		 leave it alone */
	      if (info->relocateable)
		continue;
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
	  if (sym != NULL && sym->n_scnum != 0)
	    addend += sym->n_value;
	}

      val = 0;

      valIsValid = true;
      need_static_reloc = true;
      need_dynamic_reloc = false;

      h_real = h;

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

	  /* Here we chase down both indirects and aliases if we 
	     find them; in the code below, we may want both
	     h and h_real (h for names, h_real for values). */
	  while (h_real->root.type == bfd_link_hash_indirect
	     || h_real->root.type == bfd_link_hash_warning)
	    h_real = (struct coff_link_hash_entry *) h_real->root.u.i.link;
  
	  switch (h_real->root.type)
	    {
	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      sec = h_real->root.u.def.section;
#ifdef DYNAMIC_LINKING
	      /* In some cases, we don't need the relocation
		 value.  We check specially because in some
		 obscure cases sec->output_section will be NULL.
		 We'll sort that out in the switch on relocation type
		 below, and then complain as needed.  */
	      if (sec == NULL)
		  valIsValid = false;
	      else if (sec->output_section == NULL) 
		{
	          if ((h_real->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC)==0)
		      valIsValid = false;
	        }
	      else
#endif
		  val = (h_real->root.u.def.value
		     + sec->output_section->vma
		     + sec->output_offset);
	      break;
  
	    case bfd_link_hash_undefweak:
	      sec = NULL;
	      val = 0;
	      break;

	    case bfd_link_hash_undefined:
	    case bfd_link_hash_new:
	    case bfd_link_hash_common:

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

	    /* Should be impossible. */
  	    case bfd_link_hash_warning:
  	    case bfd_link_hash_indirect:
	    default:
	      abort ();
	    }
	}

      /* For non-PE, this may end up either true or false, but it doesn't
	 matter because imagebase will always be zero. */
      need_imagebase = !howto->pc_relative
	&& sec != NULL
	&& !bfd_is_abs_section(sec->output_section);
  
#ifdef DYNAMIC_LINKING /* [ */
      /* In case some PIC code slipped in here, we have to do this */
      switch (rel->r_type)
	{
	case R_GNU_GOT32:
	  /* Relocation is to the entry for the original (not the resolved
	     strong one) symbol in the global offset table.  */

	  if (h != NULL)
	    {
	      bfd_vma off;

	      off = h->got_offset;
	      BFD_ASSERT (off != (bfd_vma) -1);

	      if ( ! dynamic 
		  || (info->shared
		      && info->symbolic
		      && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR))
	          || (h->coff_link_hash_flags & COFF_LINK_FORCED_LOCAL) != 0)
		{
		  /* This is actually a static link, or it is a
		     -Bsymbolic link and the symbol is defined
		     locally.  We must initialize this entry in the
		     global offset table.  Since the offset must
		     always be a multiple of 4, we use the least
		     significant bit to record whether we have
		     initialized it already.

		     When doing a dynamic link, we create a .rel.got
		     relocation entry to initialize the value.  This
		     is done in the finish_dynamic_symbol routine.  */

      	          BFD_ASSERT (sgot != NULL);

		  if ((off & 1) == 0)
		    {
		      /* All "relative" relocs are w.r.t. ImageBase,
			 for consistency. */
//fprintf(stderr, "got slot %d initialized for %s\n", off/4, h->root.root.string); //
		      bfd_put_32 (output_bfd, val + imagebase,
				  sgot->contents + off);
		      h->got_offset |= 1;
		    }
		  off &= ~1;
		  /* if !valIsValid, we'll compute trash above, and gripe
		     later */
		}
	      else
		{
		  /* If we got into the true branch above, val is
		     required to be valid.  This (false) case is
		     that for a true dynamic link, but it's possible
		     that the symbol doesn't have an output section,
		     and we don't want to report an error. */
		  valIsValid = true;
		}

	      val = sgot->output_offset + off;
	    }
	  else
	    {
	      /* h == NULL case */
	      bfd_vma off;

	      BFD_ASSERT (local_got_offsets != NULL
			  && local_got_offsets[symndx] != (bfd_vma) -1);

	      off = local_got_offsets[symndx];

	      /* The offset must always be a multiple of 4.  We use
                 the least significant bit to record whether we have
                 already generated the necessary reloc.  */
	      if ((off & 1) != 0)
		off &= ~1;
	      else
		{
		  /* All "relative" relocs are w.r.t. ImageBase,
		     for consistency. */
		  bfd_put_32 (output_bfd, val+imagebase, sgot->contents + off);

		  if (info->shared)
		    {
		      outrel.r_vaddr = (sgot->output_section->vma
					 + sgot->output_offset
					 + off);

		      /* Note... writing to the GOT relocs, not the rest of the
			 dynamic relocations */
		      outrel.r_type = R_GNU_RELATIVE;
		      outrel.r_symndx = 0;
		      bfd_coff_swap_reloc_out (output_bfd, &outrel,
			    (srelgot->contents + srelgot->reloc_count*symrsz));
#undef DEBUG_COUNTING
#ifdef DEBUG_COUNTING
fprintf(stderr, "relgot emits reloc #%d slot %d, anonymous\n", srelgot->reloc_count, off/4); //
#endif
		      ++srelgot->reloc_count;
		    }

		  local_got_offsets[symndx] |= 1;
		}

	      /* Since h==NULL, valIsValid is already true */
	      /* Gas puts the symbol's value in the instruction in this
		 case, so back it out. */
	      val = sgot->output_offset + off - sym->n_value;
	    }

	    need_imagebase = false;

	  break;

	case R_GNU_GOTOFF:
	  /* Relocation is relative to the start of the global offset
	     table.  */

	  /* Note that sgot->output_offset is not involved in this
	     calculation.  We always want the start of .got.  If we
	     defined _GLOBAL_OFFSET_TABLE in a different way, as is
	     permitted by the ABI, we might have to change this
	     calculation.  */

	  /* If !valIsValid, the expression below will still operate, but
	     the error will be detected below. */
	  val -= sgot->output_section->vma;
	  need_imagebase = false;

	  break;

	case R_GNU_GOTPC:
	  /* Use global offset table as symbol value; for synthesizing
	     the address of the GOT in procedure prologue */

	  need_imagebase = false;
	  val = sgot->output_section->vma;
	  valIsValid = true;
	  break;

	case R_GNU_PLT32:
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table.  */

	  /* Resolve a PLT32 reloc against a local symbol directly,
             without using the procedure linkage table.  */
	  if (h == NULL) {
	      /* Gas puts the symbol's value in the instruction in this
		 case, so back it out. */
	      val -= sym->n_value;
	      break;
	    }

	  if (h_real->plt_offset == (bfd_vma) -1)
	    {
	      /* We didn't make a PLT entry for this (strong) symbol.  This
                 happens when statically linking PIC code, or when
                 using -Bsymbolic.  */
	      break;
	    }

	  val = (splt->output_section->vma
			+ splt->output_offset
			+ h_real->plt_offset);
	  need_imagebase = false;
	  valIsValid = true;

	  break;

	case R_PCRLONG:

	   /* Skip local or localized (by symbolic mode) entries */
	   if (h == NULL
	     || (info->symbolic
		&& (h_real->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0))
	   break;

	  /* drop thru */
	case R_DIR32:
#ifdef USE_COPY_RELOC
	  /* Don't bother if not a shared lib */
	  if (info->shared)
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
	      if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR)!=0
	          && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC)==0)
		  break;
	    }
#endif

	  outrel.r_vaddr = rel->r_vaddr
			   + input_section->output_section->vma
			   + input_section->output_offset;

	  /* The basic idea here is that we may or may not want to
	     emit either class of relocation; for static relocations,
	     if this reloc is against an external symbol, we do
	     not want to fiddle with the addend.  Otherwise, we
	     need to include the symbol value so that it becomes
	     an addend for the dynamic reloc.  */

	  if (rel->r_type == R_PCRLONG)
	    {
	      if (h != NULL && h_real->dynindx == -1)
		{
		  /* The branch is already PC-relative so there's nothing to do
		     at runtime. */
		  need_static_reloc = true;
		}
	     else if (sec != NULL && bfd_is_abs_section(sec->output_section))
		{
		  need_static_reloc = true;
		  valIsValid = true;
		  need_dynamic_reloc = false;
		}
	      else
		{
		  need_static_reloc = false;
		  valIsValid = true;
		  need_dynamic_reloc = true;
 		  /* The side effect in this assert (and the similar ones below)
 		     is safe... we only count the counters back up for debug 
		     anyway. */
		  BFD_ASSERT(!h || ++h->num_relative_relocs_needed <= 0);
		  outrel.r_type = R_PCRLONG;
		  outrel.r_symndx = h_real->dynindx;
		}
	    }
	  else
	    {
	      /* The only alternative is DIR32 */

	      if (h == NULL)
		{
		  /* symbol is local to the .o */
		  need_static_reloc = true;
		  need_dynamic_reloc = info->shared &&
			!bfd_is_abs_section(sec->output_section);
		  outrel.r_type = R_GNU_RELATIVE;
		  outrel.r_symndx = 0;
		}
	      else if ((info->symbolic || h->dynindx == -1)
		      && (h->coff_link_hash_flags
			  & COFF_LINK_HASH_DEF_REGULAR) != 0)
		{
		  /* symbol is local to the module because it's symbolic;
		     h->dynindx may be -1 if this symbol was marked to
		     become local. */
		  need_static_reloc = true;
#ifdef USE_COPY_RELOC
		  /* Make this always true if main programs will be 
		     dynamically relocated (and if .reloc is still
		     not being generated (for main); currently this only 
		     happens if ld.so is used as a "run" command, and then
		     randomly.  It may be necessary to tweak the relocation
		     counting stuff accordingly. */
		  need_dynamic_reloc = true;
#else
		  need_dynamic_reloc = info->shared;
#endif
		  /* no relocs for module local, absolute symbols */
		  need_dynamic_reloc &= 
			!bfd_is_abs_section(sec->output_section);
		  BFD_ASSERT(!need_dynamic_reloc 
			     || ++h->num_long_relocs_needed <= 0);
		  outrel.r_type = R_GNU_RELATIVE;
		  outrel.r_symndx = 0;
		}
	      else if (((h->coff_link_hash_flags 
			  & COFF_LINK_HASH_DEF_REGULAR) == 0)
	               || ((h->coff_link_hash_flags 
			  & COFF_LINK_HASH_DEF_DYNAMIC) != 0)
		       || (info->shared && !info->symbolic))
		{
		  /* Not defined in this module, or this is a shared 
		     library (not symbolic), so we need a dynamic reloc;
		     if it is defined in a main, it always wins even
		     if there's also a dynamic version. */
		  BFD_ASSERT (h->dynindx != -1);
		  need_static_reloc = false;
		  need_dynamic_reloc = true;
		  BFD_ASSERT(++h->num_long_relocs_needed <= 0);
		  valIsValid = true;
		  outrel.r_type = R_DIR32;
		  outrel.r_symndx = h->dynindx;
		}
	      /* else it gets a static reloc */
	    }
	  break;

	default:
	  break;
	}
#endif /* ] */

      if (!valIsValid)
	{
	  if (h_real->root.type == bfd_link_hash_undefined)
	    {
	      if (!info->relocateable)
		{
		  if (!((*info->callbacks->undefined_symbol)
			(info, h->root.root.string, input_bfd, input_section,
			 rel->r_vaddr - input_section->vma, true)))
		    return false;
		}
	    }
	  else
	    {
	      (*_bfd_error_handler)
		(_("%s: warning: unresolvable relocation against symbol `%s' from %s section"),
		 bfd_get_filename (input_bfd), h->root.root.string,
		 bfd_get_section_name (input_bfd, input_section));
	    }

	  val = 0;
	}

#ifdef DYNAMIC_LINKING

      if (need_dynamic_reloc)
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
fprintf(stderr, "added reloc # %d (%d) for %s / %s ", sreloc->reloc_count, outrel.r_type, symndx==-1?"local-symbol":h_real?h->root.root.string:"suppressed-symbol",_bfd_coff_internal_syment_name (input_bfd, sym, buf));

if (symndx!=-1){
hh = obj_coff_sym_hashes (input_bfd)[symndx];
if (hh) fprintf(stderr, "%d %d %d\n", (hh->coff_link_hash_flags & COFF_LINK_HASH_REF_REGULAR) != 0, (hh->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) != 0, hh->root.type);
else fprintf(stderr, "nohash\n");
}
}
#endif
	}

      if (!need_static_reloc)
	  continue;
#endif

    if (need_imagebase)
	addend += imagebase;

      if (info->base_file)
	{
	  /* Emit a reloc if the backend thinks it needs it. */
	  /* Look for other instances of info->base_file in comments
	     where other relocations may be needed */
	  if (sym && pe_data (output_bfd)->in_reloc_p (output_bfd, howto))
	    {
	      /* Relocation to a symbol in a section which isn't
		 absolute.  We output the address here to a file.
		 This file is then read by dlltool when generating the
		 reloc section.  Note that the base file is not
		 portable between systems.  We write out a long here,
		 and dlltool reads in a long.  */
	      long addr = (rel->r_vaddr
			   - input_section->vma
			   + input_section->output_offset
			   + input_section->output_section->vma);
	      if (fwrite (&addr, 1, sizeof (long), (FILE *) info->base_file)
		  != sizeof (long))
		{
		  bfd_set_error (bfd_error_system_call);
		  return false;
		}
	    }
	}

      rstat = _bfd_final_link_relocate (howto, input_bfd, input_section,
					contents,
					rel->r_vaddr - input_section->vma,
					val, addend);

      switch (rstat)
	{
	default:
	  abort ();
	case bfd_reloc_ok:
	  break;
	case bfd_reloc_outofrange:
	  (*_bfd_error_handler)
	    (_("%s: bad reloc address 0x%lx in section `%s'"),
	     bfd_archive_filename (input_bfd),
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

#ifdef DYNAMIC_LINKING /* [ */
/* This struct is used to pass information to routines called via
   coff_link_hash_traverse which must return failure.  */
struct coff_info_failed
{
  boolean failed;
  struct bfd_link_info *info;
};
/* Create some sections which will be filled in with dynamic linking
   information.  ABFD is an input file which requires dynamic sections
   to be created.  The dynamic sections take up virtual memory space
   when the final executable is run, so we need to create them before
   addresses are assigned to the output sections.  We work out the
   actual contents and size of these sections later.  */

static boolean
coff_link_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;

  if (coff_hash_table (info)->dynamic_sections_created)
    return true;

  /* Make sure that all dynamic sections use the same input BFD.
     It's likely (almost inevitable) that check_relocs (backend part)
     captured some file for use as the anchor point for the dynamic sections
     that wasn't itself a dynamic library.  We'll take what it used. */
  if (coff_hash_table (info)->dynobj == NULL)
    coff_hash_table (info)->dynobj = abfd;
  else
    abfd = coff_hash_table (info)->dynobj;

  /* If this bfd doesn't have the dynamic data add-on, make one for it.
     (This might be an ordinary .o) */

  if (dyn_data(abfd) == NULL)
      dyn_data(abfd) =
        (struct dynamic_info *) bfd_zalloc(abfd, sizeof (struct dynamic_info));

  /* Note that we set the SEC_IN_MEMORY flag for all of these
     sections.  SEC_DEBUGGING was used to avoid unprotecting at runtime,
     at the cost of breaking the strip command. */
  flags = ( SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED | SEC_DATA);

  /* A dynamically linked executable has a .interp section, but a
     shared library does not.  */
  if (! info->shared)
    {
      s = bfd_make_section (abfd, ".interp");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY))
	return false;
    }

  /* Create sections to hold version informations.  These are removed
     if they are not needed.  */
  s = bfd_make_section (abfd, ".gnu.version_d");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, 
	      flags | SEC_READONLY | SEC_NEVER_LOAD)
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;
  coff_dynverdef(abfd) = s;

  s = bfd_make_section (abfd, ".gnu.version");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, 
	      flags | SEC_READONLY | SEC_NEVER_LOAD)
      || ! bfd_set_section_alignment (abfd, s, 1))
    return false;
  coff_dynversym(abfd) = s;

  s = bfd_make_section (abfd, ".gnu.version_r");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, 
	      flags | SEC_READONLY | SEC_NEVER_LOAD)
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;

  s = bfd_make_section (abfd, ".dynsym");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s,
	      flags | SEC_READONLY | SEC_ALLOC | SEC_LOAD)
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;
  coff_dynsymtab(abfd) = s;

  s = bfd_make_section (abfd, ".dynstr");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s,
	      flags | SEC_READONLY | SEC_ALLOC | SEC_LOAD))
    return false;
  coff_dynstrtab(abfd) = s;

  /* Create a strtab to hold the dynamic symbol names.  */
  if (coff_hash_table (info)->dynstr == NULL)
    {
      coff_hash_table (info)->dynstr = _bfd_coff_stringtab_init ();
      if (coff_hash_table (info)->dynstr == NULL)
	return false;
    }

  s = bfd_make_section (abfd, ".dynamic");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, 
	      flags | SEC_READONLY | SEC_ALLOC | SEC_LOAD)
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;
  coff_hash_table (info)->dynamic = s;

  /* symbol _DYNAMIC is handled in the back end */

  s = bfd_make_section (abfd, ".hash");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s,
	      flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;

  /* Let the backend create the rest of the sections.  This lets the
     backend set the right flags.  The backend will normally create
     the .got and .plt sections.  */
  if (! bfd_coff_backend_link_create_dynamic_sections(abfd, info))
    return false;

  coff_hash_table (info)->dynamic_sections_created = true;

  return true;
}

/* This structure is used to pass information to
   coff_link_assign_sym_version.  */

struct coff_assign_sym_version_info
{
  /* Output BFD.  */
  bfd *output_bfd;
  /* General link information.  */
  struct bfd_link_info *info;
  /* Version tree.  */
  struct bfd_elf_version_tree *verdefs;
  /* Whether we are exporting all dynamic symbols.  */
  boolean export_dynamic;
  /* Whether we removed any symbols from the dynamic symbol table.  */
  boolean removed_dynamic;
  /* Whether we had a failure.  */
  boolean failed;
};

/* This structure is used to pass information to
   coff_link_find_version_dependencies.  */

struct coff_find_verdep_info
{
  /* Output BFD.  */
  bfd *output_bfd;
  /* General link information.  */
  struct bfd_link_info *info;
  /* The number of dependencies.  */
  unsigned int vers;
  /* Whether we had a failure.  */
  boolean failed;
};

/* Figure out appropriate versions for all the symbols.  We may not
   have the version number script until we have read all of the input
   files, so until that point we don't know which symbols should be
   local.  This function is called via coff_link_hash_traverse.  */

static boolean coff_link_assign_sym_version 
  PARAMS((struct coff_link_hash_entry *, PTR));

static boolean
coff_link_assign_sym_version (h, data)
     struct coff_link_hash_entry *h;
     PTR data;
{
  struct coff_assign_sym_version_info *sinfo =
    (struct coff_assign_sym_version_info *) data;
  struct bfd_link_info *info = sinfo->info;
  char *p;

#ifdef USE_DLLS
  /* If this showed up in a DLL after we added it to the dynamic link
     table... drop it.  However, __imp_* symbols were already treated. */
  if ((h->coff_link_hash_flags & COFF_LINK_HASH_DLL_DEFINED) != 0 
       && h->dynindx != -1)
     {
	sinfo->removed_dynamic = true;
	h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
	h->dynindx = -1;
	/* FIXME: The name of the symbol (if long enough) has
	   already been recorded in the dynamic
	   string table section.  */
	return true;
     }
#endif

  /* If asked to strip specific definitions by name (actually, keep), we 
     will honor that, we just can't do that for references, however.
     This allows creation of shared libs with controlled export lists.
     ...MAYBE_FORCED_LOCAL got set in ...record_dynamic_symbol
     and found that it was one we might strip.  However, we
     can only strip locally defined symbols, and since we didn't
     know at the time, we check again now.   ...MAYBE_FORCED_LOCAL
     isn't strictly necessary (the hash lookup could be done here) but it
     helps a little in keeping trash out of the dynamic symbol table;
     if that's ever fixed, it might be done away with. */
  if ((h->coff_link_hash_flags & COFF_LINK_MAYBE_FORCED_LOCAL) != 0)
    {
      if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0 )
	{
	   sinfo->removed_dynamic = true;
	   h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
	   h->dynindx = -1;
	   /* FIXME: The name of the symbol (if long enough) has
	      already been recorded in the dynamic
	      string table section.  */
	   return true;
	}
      else
	{
	   /* The user asked us to strip an undef or dynamic;
	      we can't do that. */
	   h->coff_link_hash_flags &= ~COFF_LINK_MAYBE_FORCED_LOCAL;
	   (*_bfd_error_handler)
	     (_("Warning: --retain-symbols would strip %s but it is not "
	      "locally defined and must be retained."),
	      h->root.root.string);
	   h->indx = -2;
	}
    }

  /* We only need version numbers for symbols defined in regular
     objects.  */
  if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) == 0)
    return true;

  p = strchr (h->root.root.string, COFF_VER_CHR);
  if (h->root.root.string[0] != '?' && p != NULL && h->verinfo.vertree == NULL)
    {
      struct bfd_elf_version_tree *t;
      boolean hidden;

      hidden = true;

      /* There are two consecutive COFF_VER_CHR characters if this is
         not a hidden symbol.  */
      ++p;
      if (*p == COFF_VER_CHR)
	{
	  hidden = false;
	  ++p;
	}

      /* If there is no version string, we can just return out.  */
      if (*p == '\0')
	{
	  if (hidden)
	    h->coff_link_hash_flags |= COFF_LINK_HIDDEN;
	  return true;
	}

      /* Look for the version.  If we find it, it is no longer weak.  */
      for (t = sinfo->verdefs; t != NULL; t = t->next)
	{
	  if (strcmp (t->name, p) == 0)
	    {
	      int len;
	      char *alc;
	      struct bfd_elf_version_expr *d;

	      len = p - h->root.root.string;
	      alc = bfd_alloc (sinfo->output_bfd, len);
	      if (alc == NULL)
	        return false;
	      strncpy (alc, h->root.root.string, len - 1);
	      alc[len - 1] = '\0';
	      if (alc[len - 2] == COFF_VER_CHR)
	        alc[len - 2] = '\0';

	      h->verinfo.vertree = t;
	      t->used = true;
	      d = NULL;

	      if (t->globals != NULL)
		{
		  for (d = t->globals; d != NULL; d = d->next)
		    if ((*d->match) (d, alc))
		      break;
		}

	      /* See if there is anything to force this symbol to
                 local scope.  */
	      if (d == NULL && t->locals != NULL)
		{
		  for (d = t->locals; d != NULL; d = d->next)
		    {
		      if ((*d->match) (d, alc))
			{
			  if (h->dynindx != -1
			      && info->shared
			      && ! sinfo->export_dynamic)
			    {
			      sinfo->removed_dynamic = true;
			      h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
			      h->coff_link_hash_flags &=~
				COFF_LINK_HASH_NEEDS_PLT;
			      h->dynindx = -1;
			      // h->plt.offset = (bfd_vma) -1;
			      /* FIXME: The name of the symbol has
				 already been recorded in the dynamic
				 string table section.  */
			    }

			  break;
			}
		    }
		}

	      bfd_release (sinfo->output_bfd, alc);
	      break;
	    }
	}
#if 0 // discard
------------
      /* Look for the version.  If we find it, it is no longer weak.  */
      for (t = sinfo->verdefs; t != NULL; t = t->next)
	{
	  if (strcmp (t->name, p) == 0)
	    {
	      h->verinfo.vertree = t;
	      t->used = true;

	      /* See if there is anything to force this symbol to
                 local scope.  */
	      if (t->locals != NULL)
		{
		  int len;
		  char *alc;
		  struct bfd_elf_version_expr *d;

		  len = p - h->root.root.string;
		  alc = bfd_alloc (sinfo->output_bfd, len);
		  if (alc == NULL)
		    return false;
		  strncpy (alc, h->root.root.string, len - 1);
		  alc[len - 1] = '\0';
		  if (alc[len - 2] == COFF_VER_CHR)
		    alc[len - 2] = '\0';

		  for (d = t->locals; d != NULL; d = d->next)
		    {
		      if ((d->match[0] == '*' && d->match[1] == '\0')
			  || fnmatch (d->match, alc, 0) == 0)
			{
			  if (h->dynindx != -1
			      && info->shared
			      && ! sinfo->export_dynamic
			      && (h->coff_link_hash_flags
				  & COFF_LINK_HASH_NEEDS_PLT) == 0)
			    {
			      sinfo->removed_dynamic = true;
			      h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
			      h->dynindx = -1;
			      /* FIXME: The name of the symbol has
				 already been recorded in the dynamic
				 string table section.  */
			    }

			  break;
			}
		    }

		  bfd_release (sinfo->output_bfd, alc);
		}

	      break;
	    }
	}
#endif // end discard

      /* If we are building an application, we need to create a
         version node for this version.  */
      if (t == NULL && ! info->shared)
	{
	  struct bfd_elf_version_tree **pp;
	  int version_index;

	  /* If we aren't going to export this symbol, we don't need
             to worry about it. */
	  if (h->dynindx == -1)
	    return true;

	  t = ((struct bfd_elf_version_tree *)
	       bfd_alloc (sinfo->output_bfd, sizeof *t));
	  if (t == NULL)
	    {
	      sinfo->failed = true;
	      return false;
	    }

	  t->next = NULL;
	  t->name = p;
	  t->globals = NULL;
	  t->locals = NULL;
	  t->deps = NULL;
	  t->name_indx = (unsigned int) -1;
	  t->used = true;

	  version_index = 1;
	  for (pp = &sinfo->verdefs; *pp != NULL; pp = &(*pp)->next)
	    ++version_index;
	  t->vernum = version_index;

	  *pp = t;

	  h->verinfo.vertree = t;
	}
      else if (t == NULL)
	{
	  /* We could not find the version for a symbol when
             generating a shared archive.  Return an error.  */
	  (*_bfd_error_handler)
	    (_("%s: undefined version name %s"),
	     bfd_get_filename (sinfo->output_bfd), h->root.root.string);
	  bfd_set_error (bfd_error_bad_value);
	  sinfo->failed = true;
	  return false;
	}

      if (hidden)
	h->coff_link_hash_flags |= COFF_LINK_HIDDEN;
    }

  /* If we don't have a version for this symbol, see if we can find
     something.  */
  if (h->verinfo.vertree == NULL && sinfo->verdefs != NULL)
    {
      struct bfd_elf_version_tree *t;
      struct bfd_elf_version_tree *deflt;
      struct bfd_elf_version_expr *d;

      /* See if can find what version this symbol is in.  If the
         symbol is supposed to be local, then don't actually register
         it.  */
      deflt = NULL;
      for (t = sinfo->verdefs; t != NULL; t = t->next)
	{
	  if (t->globals != NULL)
	    {
	      for (d = t->globals; d != NULL; d = d->next)
		{
		  if ((*d->match) (d, h->root.root.string))
		    {
		      h->verinfo.vertree = t;
		      break;
		    }
		}

	      if (d != NULL)
		break;
	    }

	  if (t->locals != NULL)
	    {
	      for (d = t->locals; d != NULL; d = d->next)
		{
		  if (d->pattern[0] == '*' && d->pattern[1] == '\0')
		    deflt = t;
		  else if ((*d->match) (d, h->root.root.string))
		    {
		      h->verinfo.vertree = t;
		      if (h->dynindx != -1
			  && info->shared
			  && ! sinfo->export_dynamic)
			{
			  sinfo->removed_dynamic = true;
			  h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
			  h->coff_link_hash_flags &=~ COFF_LINK_HASH_NEEDS_PLT;
			  h->dynindx = -1;
			  // h->plt.offset = (bfd_vma) -1;
			  /* FIXME: The name of the symbol has already
			     been recorded in the dynamic string table
			     section.  */
			}
		      break;
		    }
		}

	      if (d != NULL)
		break;
	    }
	}

      if (deflt != NULL && h->verinfo.vertree == NULL)
	{
	  h->verinfo.vertree = deflt;
	  if (h->dynindx != -1
	      && info->shared
	      && ! sinfo->export_dynamic)
	    {
	      sinfo->removed_dynamic = true;
	      h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
	      h->coff_link_hash_flags &=~ COFF_LINK_HASH_NEEDS_PLT;
	      h->dynindx = -1;
	      //h->plt.offset = (bfd_vma) -1;
	      /* FIXME: The name of the symbol has already been
		 recorded in the dynamic string table section.  */
	    }
	}
    }
#if 0 // discard
  /* If we don't have a version for this symbol, see if we can find
     something.  */
  if (h->verinfo.vertree == NULL && sinfo->verdefs != NULL)
    {
      struct bfd_elf_version_tree *t;
      struct bfd_elf_version_tree *deflt;
      struct bfd_elf_version_expr *d;

      /* See if can find what version this symbol is in.  If the
         symbol is supposed to eb local, then don't actually register
         it.  */
      deflt = NULL;
      for (t = sinfo->verdefs; t != NULL; t = t->next)
	{
	  if (t->globals != NULL)
	    {
	      for (d = t->globals; d != NULL; d = d->next)
		{
		  if (fnmatch (d->match, h->root.root.string, 0) == 0)
		    {
		      h->verinfo.vertree = t;
		      break;
		    }
		}

	      if (d != NULL)
		break;
	    }

	  if (t->locals != NULL)
	    {
	      for (d = t->locals; d != NULL; d = d->next)
		{
		  if (d->match[0] == '*' && d->match[1] == '\0')
		    deflt = t;
		  else if (fnmatch (d->match, h->root.root.string, 0) == 0)
		    {
		      h->verinfo.vertree = t;
		      if (h->dynindx != -1
			  && info->shared
			  && ! sinfo->export_dynamic
			  && (h->coff_link_hash_flags
			      & COFF_LINK_HASH_NEEDS_PLT) == 0)
			{
			  sinfo->removed_dynamic = true;
			  h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
			  h->dynindx = -1;
			  /* FIXME: The name of the symbol has already
			     been recorded in the dynamic string table
			     section.  */
			}
		      break;
		    }
		}

	      if (d != NULL)
		break;
	    }
	}

      if (deflt != NULL && h->verinfo.vertree == NULL)
	{
	  h->verinfo.vertree = deflt;
	  if (h->dynindx != -1
	      && info->shared
	      && ! sinfo->export_dynamic
	      && (h->coff_link_hash_flags & COFF_LINK_HASH_NEEDS_PLT) == 0)
	    {
	      sinfo->removed_dynamic = true;
	      h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
	      h->dynindx = -1;
	      /* FIXME: The name of the symbol has already been
		 recorded in the dynamic string table section.  */
	    }
	}
    }
#endif // discard

  return true;
}

/* This routine is used to export all defined symbols into the dynamic
   symbol table.  It is called via coff_link_hash_traverse.  */

static boolean coff_export_symbol 
  PARAMS((struct coff_link_hash_entry *, PTR));

static boolean
coff_export_symbol (h, data)
     struct coff_link_hash_entry *h;
     PTR data;
{
  struct coff_info_failed *eif = (struct coff_info_failed *) data;

  /* Ignore indirect symbols.  These are added by the versioning code.  */
  if (h->root.type == bfd_link_hash_indirect
      && !h->root.u.i.info.alias)
    return true;

  if (h->dynindx == -1
      && (h->coff_link_hash_flags
          & (COFF_LINK_HASH_DEF_REGULAR | COFF_LINK_HASH_REF_REGULAR)) != 0)
    {
      if (! _bfd_coff_link_record_dynamic_symbol (eif->info, h))
        {
          eif->failed = true;
          return false;
        }    
    }

  return true;
}

/* This function is used to renumber the dynamic symbols, if some of
   them are removed because they are marked as local.  This is called
   via coff_link_hash_traverse.   Since we also want alias symbols
   to follow the "real" definition, we look for them and emit them
   in the proper order. */

static boolean coff_link_renumber_dynsyms
  PARAMS((struct coff_link_hash_entry *, PTR));

static boolean
coff_link_renumber_dynsyms (h, data)
     struct coff_link_hash_entry *h;
     PTR data;
{
  struct bfd_link_info *info = (struct bfd_link_info *) data;

  if (h->dynindx != -1
      && (h->coff_link_hash_flags & COFF_LINK_HASH_RENUMBERED) == 0)
    {
      if (h->root.type == bfd_link_hash_indirect
	  && h->root.u.i.info.alias) 
	{
	  struct coff_link_hash_entry *h_real = h;

	  if ((h->coff_link_hash_flags 
		   & (COFF_LINK_HASH_DEF_DYNAMIC | COFF_LINK_HASH_REF_DYNAMIC)) == 0
	       && !info->shared)
	    {
	       /* Under certain circumstances a weak symbol that starts
		  out as dynmamic can completely disappear into a static symbol.
		  (When it is a weak for a static strong, e.g. environ.) */
	       h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
	       h->dynindx = -1;
	       return true;
	    }

	  while (h_real->root.type == bfd_link_hash_indirect
		  && h_real->root.u.i.info.alias)
	    h_real = (struct coff_link_hash_entry *) h_real->root.u.i.link;

	  if (h_real->dynindx == -1)
	    {
              BFD_ASSERT(false); // leave for a while, then delete. (9/00)
              /* Someone tossed the strong symbol ... the weak one goes too,
                 or the symbol table structure is bad.
                 However the weak may have prevented the strong one from
                 being deleted earlier. */
	       h->dynindx = -1;
	       h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
	       return true;
	    }

          if ((h_real->coff_link_hash_flags & COFF_LINK_HASH_RENUMBERED) == 0)
	    {
              h_real->coff_link_hash_flags |= COFF_LINK_HASH_RENUMBERED;
	      h_real->dynindx = coff_hash_table (info)->dynsymcount;
	      ++coff_hash_table (info)->dynsymcount;
	    }

	  h->dynindx = coff_hash_table (info)->dynsymcount;
	  /* The alias itself takes another slot */
	  coff_hash_table (info)->dynsymcount += 2;
	}
      else 
	{
	  h->dynindx = coff_hash_table (info)->dynsymcount;
	  ++coff_hash_table (info)->dynsymcount;
	}
      h->coff_link_hash_flags |= COFF_LINK_HASH_RENUMBERED;
    }

  return true;
}


/* Look through the symbols which are defined in other shared
   libraries and referenced here.  Update the list of version
   dependencies.  This will be put into the .gnu.version_r section.
   This function is called via coff_link_hash_traverse.  */

static boolean coff_link_find_version_dependencies
  PARAMS((struct coff_link_hash_entry *, PTR));

static boolean
coff_link_find_version_dependencies (h, data)
     struct coff_link_hash_entry *h;
     PTR data;
{
  struct coff_find_verdep_info *rinfo = (struct coff_find_verdep_info *) data;
  coff_internal_verneed *t;
  coff_internal_vernaux *a;

  /* We only care about symbols defined in shared objects with version
     information.  */
  if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) == 0
      || (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0
      || h->dynindx == -1
      || h->verinfo.verdef == NULL)
    return true;

  /* See if we already know about this version.  */
  for (t = dyn_data (rinfo->output_bfd)->verref; t != NULL; t = t->vn_nextref)
    {
      if (t->vn_bfd == h->verinfo.verdef->vd_bfd)
	continue;

      for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
	if (a->vna_nodename == h->verinfo.verdef->vd_nodename)
	  return true;

      break;
    }

  /* This is a new version.  Add it to tree we are building.  */

  if (t == NULL)
    {
      t = (coff_internal_verneed *) bfd_zalloc (rinfo->output_bfd, sizeof *t);
      if (t == NULL)
	{
	  rinfo->failed = true;
	  return false;
	}

      t->vn_bfd = h->verinfo.verdef->vd_bfd;
      t->vn_nextref = dyn_data (rinfo->output_bfd)->verref;
      dyn_data (rinfo->output_bfd)->verref = t;
    }

  a = (coff_internal_vernaux *) bfd_zalloc (rinfo->output_bfd, sizeof *a);

  /* Note that we are copying a string pointer here, and testing it
     above.  If bfd_coff_string_from_coff_section is ever changed to
     discard the string data when low in memory, this will have to be
     fixed.  */
  a->vna_nodename = h->verinfo.verdef->vd_nodename;

  a->vna_flags = h->verinfo.verdef->vd_flags;
  a->vna_nextptr = t->vn_auxptr;

  h->verinfo.verdef->vd_exp_refno = rinfo->vers;
  ++rinfo->vers;

  a->vna_other = h->verinfo.verdef->vd_exp_refno + 1;

  t->vn_auxptr = a;

  return true;
}

static boolean coff_adjust_dynamic_symbol
  PARAMS ((struct coff_link_hash_entry *, PTR));

/* Make the backend pick a good value for a dynamic symbol.  This is
   called via coff_link_hash_traverse, and also calls itself
   recursively.  */
static boolean
coff_adjust_dynamic_symbol (h, data)
     struct coff_link_hash_entry *h;
     PTR data;
{
  struct coff_info_failed *eif = (struct coff_info_failed *) data;
  bfd *dynobj = coff_hash_table (eif->info)->dynobj;
  boolean skip = false;

  /* If it's a weak symbol, we may need to touch up the corresponding
     strong symbol, or the weak itself. */
  if (h->root.type == bfd_link_hash_indirect
      && h->root.u.i.info.alias)
    {
      struct coff_link_hash_entry *h_real = h;

      while (h_real->root.type == bfd_link_hash_indirect
             && h_real->root.u.i.info.alias)
       h_real = (struct coff_link_hash_entry *) h_real->root.u.i.link;

#ifdef USE_DLLS
      /* If it's a weak symbol that's not a DLL symbol then clear out the DLL 
	 flag on the strong one... the user really did want it. */
      if ((h->coff_link_hash_flags & COFF_LINK_HASH_DLL_DEFINED) == 0)
          h_real->coff_link_hash_flags &= ~COFF_LINK_HASH_DLL_DEFINED;
#endif

      /* The strong symbol needs to be referenced from anywhere the weak
	 one was. */
      h_real->coff_link_hash_flags 
	  |= h->coff_link_hash_flags 
	     & (COFF_LINK_HASH_REF_REGULAR | COFF_LINK_HASH_REF_DYNAMIC);

      /* The weak symbol needs to appear to come from the same source
	 as the strong one. */
      h->coff_link_hash_flags 
	  &= ~(COFF_LINK_HASH_DEF_REGULAR | COFF_LINK_HASH_DEF_DYNAMIC);
      h->coff_link_hash_flags 
	  |= h_real->coff_link_hash_flags 
	     & (COFF_LINK_HASH_DEF_REGULAR | COFF_LINK_HASH_DEF_DYNAMIC);

      if (!(eif->info)->shared
         && (h->coff_link_hash_flags 
	     & (COFF_LINK_HASH_REF_DYNAMIC | COFF_LINK_HASH_DEF_DYNAMIC)) == 0)
	{
	   /* Both the strong and weak symbol must be properly present,
	      or the symbol table structure is bad.
	      However the weak may have prevented the strong one from
	      being deleted earlier. */
	   h->dynindx = -1;
	   h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
	   skip = true;
	   goto done;
	}

      if ((!(eif->info)->shared || (eif->info)->symbolic)
	  && (h->coff_link_hash_flags 
	      & (COFF_LINK_HASH_DEF_DYNAMIC | COFF_LINK_HASH_REF_DYNAMIC)) != 0)
	{
	   /* We're building an executable; at this point weak refs
	      to dynamic symbols need to be converted to ordinary
	      references, so clone the critical info.   Only
	      weak symbols for which the strong one is also dynamic
	      get this treatment.  If the strong one is not dynamic,
	      we just use the weak one.

	      Gdb needs type, but so does subsequent code here. */

	    if (h_real->plt_offset != (bfd_vma)-1) 
	      {
	        h->root.type = h_real->root.type;
	        h->root.u = h_real->root.u;
	        h->type = h_real->type;
	        h->class = h_real->class;
	        h->numaux = 0;
	        h->plt_offset = h_real->plt_offset;

	        /* But we don't want a separate .plt entry. */
	        h->coff_link_hash_flags |= COFF_LINK_WEAK_PLT;
	        skip = true;
	        goto done;
	      }
	}
    }
  /* Ignore (remaining) indirect symbols (both).  
     Weak indirects don't need further processing.
     Real indirects are added by the versioning code. */
  if (h->root.type == bfd_link_hash_indirect)
    {
       skip=true;
       goto done;
    }

  /* If this symbol was mentioned in a non-COFF file, try to set
     DEF_REGULAR and REF_REGULAR correctly.  This is the only way to
     permit a non-COFF file to correctly refer to a symbol defined in
     an COFF dynamic object.  */
  if ((h->coff_link_hash_flags & COFF_LINK_NON_COFF) != 0)
    {
      if (h->root.type != bfd_link_hash_defined
	  && h->root.type != bfd_link_hash_defweak)
	h->coff_link_hash_flags |= COFF_LINK_HASH_REF_REGULAR;
      else
	{
	  if (h->root.u.def.section->owner != NULL
	      && (bfd_get_flavour (h->root.u.def.section->owner)
		  == bfd_target_coff_flavour)
		  )
	    h->coff_link_hash_flags |= COFF_LINK_HASH_REF_REGULAR;
	  else
	    h->coff_link_hash_flags |= COFF_LINK_HASH_DEF_REGULAR;
	}

      if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) != 0
	  || (h->coff_link_hash_flags & COFF_LINK_HASH_REF_DYNAMIC) != 0)
	{
	  if (! _bfd_coff_link_record_dynamic_symbol (eif->info, h))
	    {
	      eif->failed = true;
	      return false;
	    }
	}
    }

  /* If this is a final link, and the symbol was defined as a common
     symbol in a regular object file, and there was no definition in
     any dynamic object, then the linker will have allocated space for
     the symbol in a common section but the COFF_LINK_HASH_DEF_REGULAR
     flag will not have been set.  */
  if (h->root.type == bfd_link_hash_defined
      && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) == 0
      && (h->coff_link_hash_flags & COFF_LINK_HASH_REF_REGULAR) != 0
      && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) == 0
      && (h->root.u.def.section->owner->flags & DYNAMIC) == 0)
    h->coff_link_hash_flags |= COFF_LINK_HASH_DEF_REGULAR;

  /* If -Bsymbolic was used (which means to bind references to global
     symbols to the definition within the shared object), and this
     symbol was defined in a regular object, then it actually doesn't
     need a PLT entry.  */
  if ((h->coff_link_hash_flags & COFF_LINK_HASH_NEEDS_PLT) != 0
      && eif->info->shared
      && eif->info->symbolic
      && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0)
    h->coff_link_hash_flags &=~ COFF_LINK_HASH_NEEDS_PLT;

  /* If this symbol does not require a PLT entry, and it is not
     defined by a dynamic object, or is not referenced by a regular
     object, ignore it.  We do have to handle a weak defined symbol,
     even if no regular object refers to it, if we decided to add it
     to the dynamic symbol table.  FIXME: Do we normally need to worry
     about symbols which are defined by one dynamic object and
     referenced by another one?  */
  if ((h->coff_link_hash_flags & COFF_LINK_HASH_NEEDS_PLT) == 0
      && ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0
	  || (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC) == 0
	  || ((h->coff_link_hash_flags & COFF_LINK_HASH_REF_REGULAR) == 0
#ifdef USE_WEAK
	       && (h->weakdef == NULL || h->weakdef->dynindx == -1)
#endif
	      )))
    {
       skip=true;
       goto done;
    }

  /* if the symbol is defined in a dll, it doesn't need a PLT entry;
     it's ignorable */
  if ((h->coff_link_hash_flags & COFF_LINK_HASH_DLL_DEFINED) != 0)
    {
       h->coff_link_hash_flags &=~ COFF_LINK_HASH_NEEDS_PLT;
       skip=true;
       goto done;
    }

  /* If we've already adjusted this symbol, don't do it again.  This
     can happen via a recursive call.  */
  if ((h->coff_link_hash_flags & COFF_LINK_HASH_DYNAMIC_ADJUSTED) != 0)
    {
       skip=true;
       goto done;
    }

  /* Don't look at this symbol again.  Note that we must set this
     after checking the above conditions, because we may look at a
     symbol once, decide not to do anything, and then get called
     recursively later after REF_REGULAR is set below.  */
  h->coff_link_hash_flags |= COFF_LINK_HASH_DYNAMIC_ADJUSTED;

#ifdef USE_WEAK /* [ */
  /* If this is a weak definition, and we know a real definition, and
     the real symbol is not itself defined by a regular object file,
     then get a good value for the real definition.  We handle the
     real symbol first, for the convenience of the backend routine.

     Note that there is a confusing case here.  If the real definition
     is defined by a regular object file, we don't get the real symbol
     from the dynamic object, but we do get the weak symbol.  If the
     processor backend uses a COPY reloc, then if some routine in the
     dynamic object changes the real symbol, we will not see that
     change in the corresponding weak symbol.  This is the way other
     ELF linkers work as well, and seems to be a result of the shared
     library model.

     I will clarify this issue.  Most SVR4 shared libraries define the
     variable _timezone and define timezone as a weak synonym.  The
     tzset call changes _timezone.  If you write
       extern int timezone;
       int _timezone = 5;
       int main () { tzset (); printf ("%d %d\n", timezone, _timezone); }
     you might expect that, since timezone is a synonym for _timezone,
     the same number will print both times.  However, if the processor
     backend uses a COPY reloc, then actually timezone will be copied
     into your process image, and, since you define _timezone
     yourself, _timezone will not.  Thus timezone and _timezone will
     wind up at different memory locations.  The tzset call will set
     _timezone, leaving timezone unchanged.  */

  if (h->weakdef != NULL)
    {
      struct elf_link_hash_entry *weakdef;

      BFD_ASSERT (h->root.type == bfd_link_hash_defined
		  || h->root.type == bfd_link_hash_defweak);
      weakdef = h->weakdef;
      BFD_ASSERT (weakdef->root.type == bfd_link_hash_defined
		  || weakdef->root.type == bfd_link_hash_defweak);
      BFD_ASSERT (weakdef->elf_link_hash_flags & COFF_LINK_HASH_DEF_DYNAMIC);
      if ((weakdef->elf_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0)
	{
	  /* This symbol is defined by a regular object file, so we
	     will not do anything special.  Clear weakdef for the
	     convenience of the processor backend.  */
	  h->weakdef = NULL;
	}
      else
	{
	  /* There is an implicit reference by a regular object file
	     via the weak symbol.  */
	  weakdef->elf_link_hash_flags |= COFF_LINK_HASH_REF_REGULAR;
	  if (! elf_adjust_dynamic_symbol (weakdef, (PTR) eif))
	    return false;
	}
    }
#endif /* ] */

done:
  if (! bfd_coff_backend_adjust_dynamic_symbol (dynobj, eif->info, h, skip))
    {
      eif->failed = true;
      return false;
    }

  return true;
}

/* Array used to determine the number of hash table buckets to use
   based on the number of symbols there are.  If there are fewer than
   3 symbols we use 1 bucket, fewer than 17 symbols we use 3 buckets,
   fewer than 37 we use 17 buckets, and so forth.  We never use more
   than 32771 buckets.  */

static const size_t coff_buckets[] =
{
  1, 3, 17, 37, 67, 97, 131, 197, 263, 521, 1031, 2053, 4099, 8209,
  16411, 32771, 0
};


/* Set up the sizes and contents of the COFF dynamic sections.  This is
   called by the COFF linker emulation before_allocation routine.  We
   must set the sizes of the sections before the linker sets the
   addresses of the various sections.  */

boolean
bfd_coff_size_dynamic_sections (output_bfd, soname, rpath,
				     filter_shlib,
				     auxiliary_filters, info, sinterpptr,
				     verdefs)
     bfd *output_bfd;
     const char *soname;
     const char *rpath;
     const char *filter_shlib;
     const char * const *auxiliary_filters;
     struct bfd_link_info *info;
     asection **sinterpptr;
     struct bfd_elf_version_tree *verdefs;
{
  bfd_size_type soname_indx;
  bfd *dynobj;
  bfd_size_type old_dynsymcount;

  struct coff_info_failed eif;
  struct coff_link_hash_entry *h;
  bfd_size_type strsize;

  size_t dynsymcount;
  asection *verdef_section;
  asection *verref_section;
  asection *version_section;
  asection *dynsym_section;
  asection *hash_section;
  size_t i;
  size_t bucketcount = 0;
  struct internal_syment isym;
  struct coff_assign_sym_version_info sinfo;

  *sinterpptr = NULL;

  soname_indx = (bfd_size_type)-1;

  if (info->hash->creator->flavour != bfd_target_coff_flavour)
    return true;

  /* If there were no dynamic objects in the link, there is little to
     do here.  However, in case we got some PIC in the program, we
     need to create a GOT, etc. as needed.  We'll wait until
     we're ready, otherwise. */
  if (!coff_hash_table(info)->dynamic_sections_created)
    {
      if (!bfd_coff_backend_size_dynamic_sections (output_bfd, info))
	return false;
      return true;
    }

  if (dyn_data(output_bfd) == NULL)
      dyn_data(output_bfd) =
        (struct dynamic_info *) bfd_zalloc(output_bfd, sizeof (struct dynamic_info));

  output_bfd->flags |= DYNAMIC;
  dynobj = coff_hash_table (info)->dynobj;

  /* If we are supposed to export all symbols into the dynamic symbol
     table (this is not the normal case), then do so.  */
  if (info->export_dynamic)
    {
      struct coff_info_failed eif;

      eif.failed = false;
      eif.info = info;
      coff_link_hash_traverse (coff_hash_table (info), coff_export_symbol,
			      (PTR) &eif);
      if (eif.failed)
	return false;
    }


  *sinterpptr = bfd_get_section_by_name (dynobj, ".interp");
  BFD_ASSERT (*sinterpptr != NULL || info->shared);

  if (soname != NULL)
    {
      soname_indx = _bfd_stringtab_add (coff_hash_table (info)->dynstr,
					soname, true, true);
      if (soname_indx == (bfd_size_type) -1
	  || ! coff_add_dynamic_entry (info, DT_SONAME, soname_indx))
	return false;
    }

  if (info->symbolic)
    {
      if (! coff_add_dynamic_entry (info, DT_SYMBOLIC, 0))
	return false;
    }

  if (rpath != NULL)
    {
      bfd_size_type indx;

      indx = _bfd_stringtab_add (coff_hash_table (info)->dynstr, rpath,
				 true, true);
      if (indx == (bfd_size_type) -1
	  || ! coff_add_dynamic_entry (info, DT_RPATH, indx))
	return false;
    }

  if (filter_shlib != NULL)
    {
      bfd_size_type indx;

      indx = _bfd_stringtab_add (coff_hash_table (info)->dynstr,
				 filter_shlib, true, true);
      if (indx == (bfd_size_type) -1
	  || ! coff_add_dynamic_entry (info, DT_FILTER, indx))
	return false;
    }

  if (auxiliary_filters != NULL)
    {
      const char * const *p;

      for (p = auxiliary_filters; *p != NULL; p++)
	{
	  bfd_size_type indx;

	  indx = _bfd_stringtab_add (coff_hash_table (info)->dynstr,
				     *p, true, true);
	  if (indx == (bfd_size_type) -1
	      || ! coff_add_dynamic_entry (info, DT_AUXILIARY, indx))
	    return false;
	}
    }

  /* Find all symbols which were defined in a dynamic object and make
     the backend pick a reasonable value for them.  */
  eif.failed = false;
  eif.info = info;
  coff_link_hash_traverse (coff_hash_table (info),
			  coff_adjust_dynamic_symbol,
			  (PTR) &eif);
  if (eif.failed)
    return false;

  /* Add some entries to the .dynamic section.  We fill in some of the
     values later, in coff_bfd_final_link, but we must add the entries
     now so that we know the final size of the .dynamic section.  */
  h =  coff_link_hash_lookup (coff_hash_table (info), 
	      &"__init"[bfd_get_symbol_leading_char(dynobj)=='_'?0:1],
	      false, false, false);
  if (h != NULL
      && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0)
    {
      if (! coff_add_dynamic_entry (info, DT_INIT, 0))
	return false;
    }
  h =  coff_link_hash_lookup (coff_hash_table (info), 
	      &"__fini"[bfd_get_symbol_leading_char(dynobj)=='_'?0:1],
	      false, false, false);
  if (h != NULL
      && (h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0)
    {
      if (! coff_add_dynamic_entry (info, DT_FINI, 0))
	return false;
    }
  strsize = _bfd_stringtab_size (coff_hash_table (info)->dynstr);
  if (! coff_add_dynamic_entry (info, DT_HASH, 0)
      || ! coff_add_dynamic_entry (info, DT_STRTAB, 0)
      || ! coff_add_dynamic_entry (info, DT_SYMTAB, 0)
      || ! coff_add_dynamic_entry (info, DT_STRSZ, strsize)
      || ! coff_add_dynamic_entry (info, DT_SYMENT,
				  bfd_coff_symesz (output_bfd)))
    return false;

  /* The backend must work out the sizes of all the other dynamic
     sections.  */
  old_dynsymcount = coff_hash_table (info)->dynsymcount;
  if (!bfd_coff_backend_size_dynamic_sections (output_bfd, info))
    return false;

  /* Set up the version definition section.  */
  verdef_section = bfd_get_section_by_name (dynobj, ".gnu.version_d");
  BFD_ASSERT (verdef_section != NULL);

  /* Attach all the symbols to their version information.  This
     may cause some symbols to be unexported.  */
  sinfo.output_bfd = output_bfd;
  sinfo.info = info;
  sinfo.verdefs = verdefs;
  sinfo.export_dynamic = info->export_dynamic;
  sinfo.removed_dynamic = false;
  sinfo.failed = false;

  coff_link_hash_traverse (coff_hash_table (info),
			  coff_link_assign_sym_version,
			  (PTR) &sinfo);
  if (sinfo.failed)
    return false;

  /* Some dynamic symbols were changed to be local
     symbols.  In this case, we renumber all of the
     dynamic symbols, so that we don't have a hole.  If
     the backend changed dynsymcount, then assume that the
     new symbols are at the start.  This is the case on
     the MIPS.  FIXME: The names of the removed symbols
     will still be in the dynamic string table, wasting
     space.   Althoug sinfo.removed_dynamic tells us that
     it's needed for symbol deletions, we do it unconditionally
     to reorder alias symbols at the same time. */
  coff_hash_table (info)->dynsymcount =
    1 + (coff_hash_table (info)->dynsymcount - old_dynsymcount);
  coff_link_hash_traverse (coff_hash_table (info),
			  coff_link_renumber_dynsyms,
			  (PTR) info);

  /* We may have created additional version definitions if we are
     just linking a regular application.  */
  verdefs = sinfo.verdefs;

  if (verdefs == NULL)
    {
      asection **spp;

      /* Don't include this section in the output file.  */
      for (spp = &output_bfd->sections;
	   *spp != NULL;
	   spp = &(*spp)->next)
	{
	  if (*spp == verdef_section->output_section)
	    {
	      *spp = verdef_section->output_section->next;
	      --output_bfd->section_count;
	      break;
	    }
	}
    }
  else
    {
      unsigned int cdefs;
      bfd_size_type size;
      struct bfd_elf_version_tree *t;
      bfd_byte *p;
      coff_internal_verdef def;
      coff_internal_verdaux defaux;


      cdefs = 0;
      size = 0;

      /* Make space for the base version.  */
      size += sizeof (coff_external_verdef);
      size += sizeof (coff_external_verdaux);
      ++cdefs;

      for (t = verdefs; t != NULL; t = t->next)
	{
	  struct bfd_elf_version_deps *n;

	  size += sizeof (coff_external_verdef);
	  size += sizeof (coff_external_verdaux);
	  ++cdefs;

	  for (n = t->deps; n != NULL; n = n->next)
	    size += sizeof (coff_external_verdaux);
	}

      verdef_section->_raw_size = size;
      verdef_section->contents = (bfd_byte *) bfd_alloc (output_bfd, verdef_section->_raw_size);
      if (verdef_section->contents == NULL && verdef_section->_raw_size != 0)
	return false;

      /* Fill in the version definition section.  */

      p = verdef_section->contents;

      def.vd_version = VER_DEF_CURRENT;
      def.vd_flags = VER_FLG_BASE;
      def.vd_ndx = 1;
      def.vd_cnt = 1;
      def.vd_aux = sizeof (coff_external_verdef);
      def.vd_next = (sizeof (coff_external_verdef)
		     + sizeof (coff_external_verdaux));

      if (soname_indx != (bfd_size_type)-1)
	{
	  def.vd_hash = bfd_coff_hash ((const unsigned char *) soname);
	  defaux.vda_name = soname_indx;
	}
      else
	{
	  const char *name;
	  bfd_size_type indx;

	  name = output_bfd->filename;
	  def.vd_hash = bfd_coff_hash ((const unsigned char *) name);
	  indx = _bfd_stringtab_add (coff_hash_table (info)->dynstr,
					name, true, false);
	  if (indx == (bfd_size_type) -1)
	    return false;
	  defaux.vda_name = indx;
	}
      defaux.vda_next = 0;

      bfd_coff_swap_verdef_out (output_bfd, &def,
				(coff_external_verdef *)p);
      p += sizeof (coff_external_verdef);
      bfd_coff_swap_verdaux_out (output_bfd, &defaux,
				 (coff_external_verdaux *) p);
      p += sizeof (coff_external_verdaux);

      for (t = verdefs; t != NULL; t = t->next)
	{
	  unsigned int cdeps;
	  struct bfd_elf_version_deps *n;
	  struct coff_link_hash_entry *h;

	  cdeps = 0;
	  for (n = t->deps; n != NULL; n = n->next)
	    ++cdeps;

	  /* Add a symbol representing this version.  */
	  h = NULL;
	  if (! (bfd_coff_link_add_one_symbol
		 (info, dynobj, t->name, BSF_GLOBAL, bfd_abs_section_ptr,
		  (bfd_vma) 0, (const char *) NULL, 
		  false, false,
		  (struct bfd_link_hash_entry **) &h)))
	    return false;
	  h->coff_link_hash_flags &= ~ COFF_LINK_NON_COFF;
	  h->coff_link_hash_flags |= COFF_LINK_HASH_DEF_REGULAR;
	  h->type = 0;

	  h->verinfo.vertree = t;

	  if (! _bfd_coff_link_record_dynamic_symbol (info, h))
	    return false;

	  def.vd_version = VER_DEF_CURRENT;
	  def.vd_flags = 0;
	  if (t->globals == NULL && t->locals == NULL && ! t->used)
	    def.vd_flags |= VER_FLG_WEAK;
	  def.vd_ndx = t->vernum + 1;
	  def.vd_cnt = cdeps + 1;
	  def.vd_hash = bfd_coff_hash ((const unsigned char *) t->name);
	  def.vd_aux = sizeof (coff_external_verdef);
	  if (t->next != NULL)
	    def.vd_next = (sizeof (coff_external_verdef)
			   + (cdeps + 1) * sizeof (coff_external_verdaux));
	  else
	    def.vd_next = 0;

	  bfd_coff_swap_verdef_out (output_bfd, &def,
				    (coff_external_verdef *) p);
	  p += sizeof (coff_external_verdef);

	  defaux.vda_name = h->dynstr_index;
	  if (t->deps == NULL)
	    defaux.vda_next = 0;
	  else
	    defaux.vda_next = sizeof (coff_external_verdaux);
	  t->name_indx = defaux.vda_name;

	  bfd_coff_swap_verdaux_out (output_bfd, &defaux,
				     (coff_external_verdaux *) p);
	  p += sizeof (coff_external_verdaux);

	  for (n = t->deps; n != NULL; n = n->next)
	    {
	      defaux.vda_name = n->version_needed->name_indx;
	      if (n->next == NULL)
		defaux.vda_next = 0;
	      else
		defaux.vda_next = sizeof (coff_external_verdaux);

	      bfd_coff_swap_verdaux_out (output_bfd, &defaux,
					 (coff_external_verdaux *) p);
	      p += sizeof (coff_external_verdaux);
	    }
	}

      if (! coff_add_dynamic_entry (info, DT_VERDEF, 0)
	  || ! coff_add_dynamic_entry (info, DT_VERDEFNUM, cdefs))
	return false;

      dyn_data (output_bfd)->cverdefs = cdefs;
    }

  /* Work out the size of the version reference section.  */

  verref_section = bfd_get_section_by_name (dynobj, ".gnu.version_r");
  BFD_ASSERT (verref_section != NULL);
  {
    struct coff_find_verdep_info sinfo;

    sinfo.output_bfd = output_bfd;
    sinfo.info = info;
    sinfo.vers = dyn_data (output_bfd)->cverdefs;
    if (sinfo.vers == 0)
      sinfo.vers = 1;
    sinfo.failed = false;

    coff_link_hash_traverse (coff_hash_table (info),
			    coff_link_find_version_dependencies,
			    (PTR) &sinfo);

    if (dyn_data (output_bfd)->verref == NULL)
      {
	asection **spp;

	/* We don't have any version definitions, so we can just
	   remove the section.  */

	for (spp = &output_bfd->sections;
	     *spp != NULL;
	     spp = &(*spp)->next)
	  {
	     if (*spp == verref_section->output_section)
	       {
		 *spp = verref_section->output_section->next;
		 --output_bfd->section_count;
		 break;
	       }
	  }
      }
    else
      {
	coff_internal_verneed *t;
	unsigned int size;
	unsigned int crefs;
	bfd_byte *p;

	/* Build the version definition section.  */
	size = 0;
	crefs = 0;
	for (t = dyn_data (output_bfd)->verref;
	     t != NULL;
	     t = t->vn_nextref)
	  {
	    coff_internal_vernaux *a;

	    size += sizeof (coff_external_verneed);
	    ++crefs;
	    for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
	      size += sizeof (coff_external_vernaux);
	  }

	verref_section->_raw_size = size;
	verref_section->contents = (bfd_byte *) bfd_alloc (output_bfd, size);
	if (verref_section->contents == NULL)
	  return false;

	p = verref_section->contents;
	for (t = dyn_data (output_bfd)->verref;
	     t != NULL;
	     t = t->vn_nextref)
	  {
	    unsigned int caux;
	    coff_internal_vernaux *a;
	    bfd_size_type indx;

	    caux = 0;
	    for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
	      ++caux;

	    t->vn_version = VER_NEED_CURRENT;
	    t->vn_cnt = caux;
	    indx = _bfd_stringtab_add (coff_hash_table (info)->dynstr,
				       t->vn_bfd->filename, true, false);
	    if (indx == (bfd_size_type) -1)
	      return false;
	    t->vn_file = indx;
	    t->vn_aux = sizeof (coff_external_verneed);
	    if (t->vn_nextref == NULL)
	      t->vn_next = 0;
	    else
	      t->vn_next = (sizeof (coff_external_verneed)
			    + caux * sizeof (coff_external_vernaux));

	    bfd_coff_swap_verneed_out (output_bfd, t,
				       (coff_external_verneed *) p);
	    p += sizeof (coff_external_verneed);

	    for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
	      {
		a->vna_hash = bfd_coff_hash ((const unsigned char *)
					    a->vna_nodename);
		indx = _bfd_stringtab_add (coff_hash_table (info)->dynstr,
					   a->vna_nodename, true, false);
		if (indx == (bfd_size_type) -1)
		  return false;
		a->vna_name = indx;
		if (a->vna_nextptr == NULL)
		  a->vna_next = 0;
		else
		  a->vna_next = sizeof (coff_external_vernaux);

		bfd_coff_swap_vernaux_out (output_bfd, a,
					   (coff_external_vernaux *) p);
		p += sizeof (coff_external_vernaux);
	      }
	  }

	if (! coff_add_dynamic_entry (info, DT_VERNEED, 0)
	    || ! coff_add_dynamic_entry (info, DT_VERNEEDNUM, crefs))
	  return false;

	dyn_data (output_bfd)->cverrefs = crefs;
      }
  }

  dynsymcount = coff_hash_table (info)->dynsymcount;
  /* Work out the size of the symbol version section.  */
  version_section = bfd_get_section_by_name (dynobj, ".gnu.version");
  BFD_ASSERT (version_section != NULL);
  if (dynsymcount == 0
      || (verdefs == NULL && dyn_data (output_bfd)->verref == NULL))
    {
      asection **spp;

      /* We don't need any symbol versions; just discard the
	 section.  */
      for (spp = &output_bfd->sections;
	   *spp != NULL;
	   spp = &(*spp)->next)
	{
	   if (*spp == version_section->output_section)
	     {
      		*spp = version_section->output_section->next;
      		--output_bfd->section_count;
		break;
	     }
	}
    }
  else
    {
      version_section->_raw_size = dynsymcount * sizeof (coff_external_versym);
      version_section->contents = (bfd_byte *) bfd_zalloc (output_bfd, version_section->_raw_size);
      if (version_section->contents == NULL)
	return false;

      if (! coff_add_dynamic_entry (info, DT_VERSYM, 0))
	return false;
    }
  /* Set the size of the .dynsym and .hash sections.  We counted
     the number of dynamic symbols in coff_link_add_object_symbols.
     We will build the contents of .dynsym and .hash when we build
     the final symbol table, because until then we do not know the
     correct value to give the symbols.  We built the .dynstr
     section as we went along in coff_link_add_object_symbols.  */
  dynsym_section = bfd_get_section_by_name (dynobj, ".dynsym");
  BFD_ASSERT (dynsym_section != NULL);
  dynsym_section->_raw_size = dynsymcount * bfd_coff_symesz (output_bfd);
  dynsym_section->contents = (bfd_byte *) bfd_alloc (output_bfd, dynsym_section->_raw_size);
  if (dynsym_section->contents == NULL && dynsym_section->_raw_size != 0)
    return false;

  memset(&isym, 0, sizeof(isym));

  bfd_coff_swap_sym_out (output_bfd, &isym,
		       (PTR) (struct external_syment *) dynsym_section->contents);

  for (i = 0; coff_buckets[i] != 0; i++)
    {
      bucketcount = coff_buckets[i];
      if (dynsymcount < coff_buckets[i + 1])
	break;
    }

  hash_section = bfd_get_section_by_name (dynobj, ".hash");
  BFD_ASSERT (hash_section != NULL);
  hash_section->_raw_size = (2 + bucketcount + dynsymcount) * (ARCH_SIZE / 8);
  hash_section->contents = (bfd_byte *) bfd_alloc (output_bfd, hash_section->_raw_size);
  if (hash_section->contents == NULL)
    return false;

  memset (hash_section->contents, 0, (size_t) hash_section->_raw_size);

  bfd_h_put_32 (output_bfd, bucketcount, hash_section->contents);
  bfd_h_put_32 (output_bfd, dynsymcount, hash_section->contents + (ARCH_SIZE / 8));

  coff_hash_table (info)->bucketcount = bucketcount;

  hash_section = bfd_get_section_by_name (dynobj, ".dynstr");
  BFD_ASSERT (hash_section != NULL);
  hash_section->_raw_size = _bfd_stringtab_size (coff_hash_table (info)->dynstr);

  if (! coff_add_dynamic_entry (info, DT_NULL, 0))
    return false;

  return true;
}

boolean
_bfd_coff_create_got_section (abfd, info, gotname, want_got_plt)
     bfd *abfd;
     struct bfd_link_info *info;
     char *gotname;
     boolean want_got_plt;
{
  flagword flags;
  register asection *s;
  struct coff_link_hash_entry *h;

  /* This function may be called more than once.  */
  if (coff_hash_table (info)->sgot != NULL)
    return true;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED | SEC_DATA );

  s = bfd_make_section (abfd, ".got");
  coff_hash_table (info)->sgot = s;
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, flags)
      || !bfd_set_section_alignment (abfd, s, 2))
    return false;

  if (want_got_plt)
  {
    s = bfd_make_section (abfd, ".got.plt");
    coff_hash_table (info)->sgotplt = s;
    if (s == NULL
	|| !bfd_set_section_flags (abfd, s, flags)
	|| !bfd_set_section_alignment (abfd, s, 2))
      return false;

      /* The first three global offset table entries are reserved (which 
	 actually fall in .got.plt).  */
      s->_raw_size += 3 * 4;

  }

  /* Define the symbol _GLOBAL_OFFSET_TABLE_ at the start of the .got
     (or .got.plt) section.  We don't do this in the linker script
     because we don't want to define the symbol if we are not creating
     a global offset table.   Since the spelling varies per architecture,
     it's an argument. */
  h = NULL;
  if (!(bfd_coff_link_add_one_symbol
	(info, abfd, gotname, BSF_GLOBAL, s, (bfd_vma) 0,
	 (const char *) NULL, 
	 false, false,
	 (struct bfd_link_hash_entry **) &h)))
    return false;
  h->coff_link_hash_flags |= COFF_LINK_HASH_DEF_REGULAR;
  h->type = 0;

  if (info->shared
      && ! _bfd_coff_link_record_dynamic_symbol (info, h))
    return false;

  coff_hash_table (info)->hgot = h;

  return true;
}




/* Record a new dynamic symbol.  We record the dynamic symbols as we
   read the input files, since we need to have a list of all of them
   before we can determine the final sizes of the output sections.
   Note that we may actually call this function even though we are not
   going to output any dynamic symbols; in some cases we know that a
   symbol should be in the dynamic symbol table, but only if there is
   one.  */

boolean
_bfd_coff_link_record_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct coff_link_hash_entry *h;
{
  struct bfd_strtab_hash *dynstr;
  char *p, *alc;
  const char *name;
  boolean copy;
  bfd_size_type indx;

  /* Nothing to do, don't bother */
  if (h->dynindx != -1)
      return true;

  /* We're skipping this one for some outside reason; skip it again */
  if ((h->coff_link_hash_flags & COFF_LINK_FORCED_LOCAL) != 0)
      return true;

  if (info->strip == strip_some
	&& bfd_hash_lookup (info->keep_hash,
				h->root.root.string,
				false, false) == NULL)
  {
     /* We've been asked to strip this symbol; if we can (and sometimes
	we can't) we'll treat it as a local symbol.
        If it's already defined locally, we'll simply not record it;
	if it isn't defined, flag it for later analysis */
     if ((h->coff_link_hash_flags & COFF_LINK_HASH_DEF_REGULAR) != 0 )
       {
          h->coff_link_hash_flags |= COFF_LINK_FORCED_LOCAL;
          return true;
       }
     else 
       {
          h->coff_link_hash_flags |= COFF_LINK_MAYBE_FORCED_LOCAL;
       }
  }
  
  h->dynindx = coff_hash_table (info)->dynsymcount;
  ++coff_hash_table (info)->dynsymcount;

  /* If this is a PE weak symbol, we'll have to emit a weak alias AUX
     entry, so leave space for it here. */
  if (h->root.type == bfd_link_hash_indirect
      && h->root.u.i.info.alias)
    {
      ++coff_hash_table (info)->dynsymcount;
    }

  dynstr = coff_hash_table (info)->dynstr;
  if (dynstr == NULL)
    {
      /* Create a strtab to hold the dynamic symbol names.  */
      coff_hash_table (info)->dynstr = dynstr = _bfd_coff_stringtab_init ();
      if (dynstr == NULL)
	return false;
    }

  /* We don't put any version information in the dynamic string
     table.  */
  name = h->root.root.string;
  p = strchr (name, COFF_VER_CHR);
  if (p == NULL)
    {
      alc = NULL;
      copy = false;
    }
  else
    {
      alc = bfd_malloc (p - name + 1);
      if (alc == NULL)
	return false;
      strncpy (alc, name, p - name);
      alc[p - name] = '\0';
      name = alc;
      copy = true;
    }

  indx = _bfd_stringtab_add (dynstr, name, true, copy);

  if (alc != NULL)
    free (alc);

  if (indx == (bfd_size_type) -1)
    return false;
  h->dynstr_index = indx;

  return true;
}

/* Add an entry to the .dynamic table.  */

boolean
coff_add_dynamic_entry (info, tag, val)
     struct bfd_link_info *info;
     bfd_vma tag;
     bfd_vma val;
{
  coff_internal_dyn dyn;
  bfd *dynobj;
  asection *s;
  size_t newsize;
  bfd_byte *newcontents;

  dynobj = coff_hash_table (info)->dynobj;

  s = coff_hash_table (info)->dynamic;

  BFD_ASSERT (s != NULL);

  newsize = s->_raw_size + sizeof (coff_external_dyn);
  newcontents = (bfd_byte *) bfd_realloc (s->contents, newsize);
  if (newcontents == NULL)
    return false;

  dyn.d_tag = tag;
  dyn.d_un.d_val = val;
  bfd_coff_swap_dyn_out (dynobj, &dyn,
		    (coff_external_dyn *) (newcontents + s->_raw_size));

  s->_raw_size = newsize;
  s->contents = newcontents;

  return true;
}

/* This is a hook for the emulation code in the generic linker to
   tell the backend linker what file name to use for the DT_NEEDED
   entry for a dynamic object.  The generic linker passes name as an
   empty string to indicate that no DT_NEEDED entry should be made.  */

void
bfd_coff_set_dt_needed_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  if (bfd_get_flavour (abfd) == bfd_target_coff_flavour
      && bfd_get_format (abfd) == bfd_object)
    coff_dt_name (abfd) = name;
}

/* Get the list of DT_NEEDED entries for a link.  This is a hook for
   the emulation code.  */

struct bfd_link_needed_list *
bfd_coff_get_needed_list (abfd, info)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  if (info->hash->creator->flavour != bfd_target_coff_flavour)
    return NULL;
  return coff_hash_table (info)->needed;
}

/* Get the name actually used for a dynamic object for a link.  This
   is the SONAME entry if there is one.  Otherwise, it is the string
   passed to bfd_coff_set_dt_needed_name, or it is the filename.  */

const char *
bfd_coff_get_dt_soname (abfd)
     bfd *abfd;
{
  if (bfd_get_flavour (abfd) == bfd_target_coff_flavour
      && bfd_get_format (abfd) == bfd_object
      && dyn_data(abfd) != NULL)
    return coff_dt_name (abfd);
  return NULL;
}
#endif /* ] */
