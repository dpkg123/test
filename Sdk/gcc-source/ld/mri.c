/* mri.c -- handle MRI style linker scripts
   Copyright 1991, 1992, 1993, 1994, 1996, 1997, 1998, 1999, 2000, 2002
   Free Software Foundation, Inc.

This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.

   This bit does the tree decoration when MRI style link scripts
   are parsed.

   Contributed by Steve Chamberlain <sac@cygnus.com>.  */

#include "bfd.h"
#include "sysdep.h"
#include "ld.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldmisc.h"
#include "mri.h"
#include <ldgram.h>
#include "libiberty.h"

struct section_name_struct {
  struct section_name_struct *next;
  const char *name;
  const char *alias;
  etree_type *vma;
  etree_type *align;
  etree_type *subalign;
  int ok_to_load;
};

unsigned int symbol_truncate = 10000;
struct section_name_struct *order;
struct section_name_struct *only_load;
struct section_name_struct *address;
struct section_name_struct *alias;

struct section_name_struct *alignment;
struct section_name_struct *subalignment;

static struct section_name_struct **lookup
  PARAMS ((const char *name, struct section_name_struct **list));
static void mri_add_to_list PARAMS ((struct section_name_struct **list,
				     const char *name, etree_type *vma,
				     const char *zalias, etree_type *align,
				     etree_type *subalign));

static struct section_name_struct **
lookup (name, list)
     const char *name;
     struct section_name_struct **list;
{
  struct section_name_struct **ptr = list;

  while (*ptr)
    {
      if (strcmp (name, (*ptr)->name) == 0)
	/* If this is a match, delete it, we only keep the last instance
	   of any name.  */
	*ptr = (*ptr)->next;
      else
	ptr = &((*ptr)->next);
    }

  *ptr = (struct section_name_struct *) xmalloc (sizeof (struct section_name_struct));
  return ptr;
}

static void
mri_add_to_list (list, name, vma, zalias, align, subalign)
     struct section_name_struct **list;
     const char *name;
     etree_type *vma;
     const char *zalias;
     etree_type *align;
     etree_type *subalign;
{
  struct section_name_struct **ptr = lookup (name, list);

  (*ptr)->name = name;
  (*ptr)->vma = vma;
  (*ptr)->next = (struct section_name_struct *) NULL;
  (*ptr)->ok_to_load = 0;
  (*ptr)->alias = zalias;
  (*ptr)->align = align;
  (*ptr)->subalign = subalign;
}

void
mri_output_section (name, vma)
     const char *name;
     etree_type *vma;
{
  mri_add_to_list (&address, name, vma, 0, 0, 0);
}

/* If any ABSOLUTE <name> are in the script, only load those files
   marked thus.  */

void
mri_only_load (name)
     const char *name;
{
  mri_add_to_list (&only_load, name, 0, 0, 0, 0);
}

void
mri_base (exp)
     etree_type *exp;
{
  base = exp;
}

static int done_tree = 0;

void
mri_draw_tree ()
{
  if (done_tree)
    return;

#if 0   /* We don't bother with memory regions.  */
  /* Create the regions.  */
  {
    lang_memory_region_type *r;

    r = lang_memory_region_lookup("long");
    r->current = r->origin = exp_get_vma (base, (bfd_vma)0, "origin",
					  lang_first_phase_enum);
    r->length = (bfd_size_type) exp_get_vma (0, (bfd_vma) ~((bfd_size_type)0),
					     "length", lang_first_phase_enum);
  }
#endif

  /* Now build the statements for the ldlang machine.  */

  /* Attatch the addresses of any which have addresses,
     and add the ones not mentioned.  */
  if (address != (struct section_name_struct *) NULL)
    {
      struct section_name_struct *alist;
      struct section_name_struct *olist;

      if (order == (struct section_name_struct *) NULL)
	order = address;

      for (alist = address;
	   alist != (struct section_name_struct *) NULL;
	   alist = alist->next)
	{
	  int done = 0;

	  for (olist = order;
	       done == 0 && olist != (struct section_name_struct *) NULL;
	       olist = olist->next)
	    {
	      if (strcmp (alist->name, olist->name) == 0)
		{
		  olist->vma = alist->vma;
		  done = 1;
		}
	    }

	  if (!done)
	    {
	      /* Add this onto end of order list.  */
	      mri_add_to_list (&order, alist->name, alist->vma, 0, 0, 0);
	    }
	}
    }

  /* If we're only supposed to load a subset of them in, then prune
     the list.  */
  if (only_load != (struct section_name_struct *) NULL)
    {
      struct section_name_struct *ptr1;
      struct section_name_struct *ptr2;

      if (order == (struct section_name_struct *) NULL)
	order = only_load;

      /* See if this name is in the list, if it is then we can load it.  */
      for (ptr1 = only_load; ptr1; ptr1 = ptr1->next)
	for (ptr2 = order; ptr2; ptr2 = ptr2->next)
	  if (strcmp (ptr2->name, ptr1->name) == 0)
	    ptr2->ok_to_load = 1;
    }
  else
    {
      /* No only load list, so everything is ok to load.  */
      struct section_name_struct *ptr;

      for (ptr = order; ptr; ptr = ptr->next)
	ptr->ok_to_load = 1;
    }

  /* Create the order of sections to load.  */
  if (order != (struct section_name_struct *) NULL)
    {
      /* Been told to output the sections in a certain order.  */
      struct section_name_struct *p = order;

      while (p)
	{
	  struct section_name_struct *aptr;
	  etree_type *align = 0;
	  etree_type *subalign = 0;
	  struct wildcard_list *tmp;

	  /* See if an alignment has been specified.  */
	  for (aptr = alignment; aptr; aptr = aptr->next)
	    if (strcmp (aptr->name, p->name) == 0)
	      align = aptr->align;

	  for (aptr = subalignment; aptr; aptr = aptr->next)
	    if (strcmp (aptr->name, p->name) == 0)
	      subalign = aptr->subalign;

	  if (base == 0)
	    base = p->vma ? p->vma : exp_nameop (NAME, ".");

	  lang_enter_output_section_statement (p->name, false, base,
					       p->ok_to_load ? 0 : noload_section,
					       1, align, subalign,
					       (etree_type *) NULL);
	  base = 0;
	  tmp = (struct wildcard_list *) xmalloc (sizeof *tmp);
	  tmp->next = NULL;
	  tmp->spec.name = p->name;
	  tmp->spec.exclude_name_list = NULL;
	  tmp->spec.sorted = false;
	  lang_add_wild (NULL, tmp, false);

	  /* If there is an alias for this section, add it too.  */
	  for (aptr = alias; aptr; aptr = aptr->next)
	    if (strcmp (aptr->alias, p->name) == 0)
	      {
		tmp = (struct wildcard_list *) xmalloc (sizeof *tmp);
		tmp->next = NULL;
		tmp->spec.name = aptr->name;
		tmp->spec.exclude_name_list = NULL;
		tmp->spec.sorted = false;
		lang_add_wild (NULL, tmp, false);
	      }

	  lang_leave_output_section_statement
	    (0, "*default*", (struct lang_output_section_phdr_list *) NULL,
	     NULL);

	  p = p->next;
	}
    }

  done_tree = 1;
}

void
mri_load (name)
     const char *name;
{
  base = 0;
  lang_add_input_file (name,
		       lang_input_file_is_file_enum, (char *) NULL);
#if 0
  lang_leave_output_section_statement (0, "*default*");
#endif
}

void
mri_order (name)
     const char *name;
{
  mri_add_to_list (&order, name, 0, 0, 0, 0);
}

void
mri_alias (want, is, isn)
     const char *want;
     const char *is;
     int isn;
{
  if (!is)
    {
      char buf[20];

      /* Some sections are digits.  */
      sprintf (buf, "%d", isn);

      is = xstrdup (buf);

      if (is == NULL)
	abort ();
    }

  mri_add_to_list (&alias, is, 0, want, 0, 0);
}

void
mri_name (name)
     const char *name;
{
  lang_add_output (name, 1);
}

void
mri_format (name)
     const char *name;
{
  if (strcmp (name, "S") == 0)
    lang_add_output_format ("srec", (char *) NULL, (char *) NULL, 1);

  else if (strcmp (name, "IEEE") == 0)
    lang_add_output_format ("ieee", (char *) NULL, (char *) NULL, 1);

  else if (strcmp (name, "COFF") == 0)
    lang_add_output_format ("coff-m68k", (char *) NULL, (char *) NULL, 1);

  else
    einfo (_("%P%F: unknown format type %s\n"), name);
}

void
mri_public (name, exp)
     const char *name;
     etree_type *exp;
{
  lang_add_assignment (exp_assop ('=', name, exp));
}

void
mri_align (name, exp)
     const char *name;
     etree_type *exp;
{
  mri_add_to_list (&alignment, name, 0, 0, exp, 0);
}

void
mri_alignmod (name, exp)
     const char *name;
     etree_type *exp;
{
  mri_add_to_list (&subalignment, name, 0, 0, 0, exp);
}

void
mri_truncate (exp)
     unsigned int exp;
{
  symbol_truncate = exp;
}
