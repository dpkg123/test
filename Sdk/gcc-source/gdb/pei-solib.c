/* Handle coff-pei (rather like SVR4) shared libraries for GDB, the 
   GNU Debugger.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996
   Free Software Foundation, Inc.
   
This file is part of GDB.

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


#include "defs.h"

#include <sys/types.h>
#include <signal.h>
#include "gdb_string.h"
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>

#include "../libexec/libdl/include/link.h" // in libexec for now, whence?

#include "symtab.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "command.h"
#include "target.h"
#include "frame.h"
#include "gnu-regex.h"
#include "inferior.h"
#include "environ.h"
#include "language.h"
#include "gdbcmd.h"

#define MAX_PATH_SIZE 512		/* FIXME: Should be dynamic */

#define ERROR_ADDR  0xffffffff          /* a non-zero address that's an error */

#define BKPT_AT_SYMBOL 1

/* local data declarations */

static struct r_debug debug_copy;

struct so_list {
  struct so_list *next;			/* next structure in linked list */
  struct link_map lm;			/* copy of link map from inferior */
  struct link_map *lmaddr;		/* addr in inferior lm was read from */
  CORE_ADDR lmend;			/* upper addr bound of mapped object */
  char so_name[MAX_PATH_SIZE];		/* shared object lib name (FIXME) */
  char symbols_loaded;			/* flag: symbols read in yet? */
  char from_tty;			/* flag: print msgs? */
  struct objfile *objfile;		/* objfile for loaded lib */
  struct section_table *sections;
  struct section_table *sections_end;
  struct section_table *textsection;
  bfd *abfd;
};

static struct so_list *so_list_head;	/* List of known shared objects */
static struct so_list *so_recycle_list;	/* ... those not currently active */
static CORE_ADDR debug_base;		/* Base of dynamic linker structures */

/* Local function prototypes */

static void
sharedlibrary_command PARAMS ((char *, int));

static int
enable_break PARAMS ((int));

static void
info_sharedlibrary_command PARAMS ((char *, int));

static int
symbol_add_stub PARAMS ((char *));

static CORE_ADDR
locate_base PARAMS ((void));

static void
solib_map_sections PARAMS ((struct so_list *, int));

/* If non-zero, this is a prefix that will be added to the front of the name
   shared libraries with an absolute filename for loading.  */
static char *solib_absolute_prefix = NULL;

/* If non-empty, this is a search path for loading non-absolute shared library
   symbol files.  This takes precedence over the environment variables PATH
   and LD_LIBRARY_PATH.  */
static char *solib_search_path = NULL;

/*

LOCAL FUNCTION
	solib_map_sections -- open bfd and build sections for shared lib

SYNOPSIS
	static void solib_map_sections (struct so_list *so)

DESCRIPTION
	Given a pointer to one of the shared objects in our list
	of mapped objects, use the recorded name to open a bfd
	descriptor for the object, build a section table, and then
	relocate all the section addresses by the base address at
	which the shared object was mapped.

FIXMES
	In most (all?) cases the shared object file name recorded in the
	dynamic linkage tables will be a fully qualified pathname.  For
	cases where it isn't, do we really mimic the systems search
	mechanism correctly in the below code (particularly the tilde
	expansion stuff?).
*/

static int
solib_normalize_name (char *filename, char **scratch_pathname)
{
  int scratch_chan;
  struct cleanup *old_chain;
  
  filename = tilde_expand (filename);

  old_chain = make_cleanup (free, filename);
  
  if (solib_absolute_prefix && ROOTED_P (filename))
    /* Prefix shared libraries with absolute filenames with
       SOLIB_ABSOLUTE_PREFIX.  */
    {
      char *pfxed_fn;
      int pfx_len;

      pfx_len = strlen (solib_absolute_prefix);

      /* Remove trailing slashes.  */
      while (pfx_len > 0 && SLASH_P (solib_absolute_prefix[pfx_len - 1]))
	pfx_len--;

      pfxed_fn = xmalloc (pfx_len + strlen (filename) + 1);
      strcpy (pfxed_fn, solib_absolute_prefix);
      strcat (pfxed_fn, filename);
      free (filename);

      filename = pfxed_fn;
    }

  scratch_chan = -1;

  if (solib_search_path)
    scratch_chan = openp (solib_search_path,
			  1, filename, O_RDONLY, 0, scratch_pathname);
  if (scratch_chan < 0)
    scratch_chan = openp (get_in_environ (inferior_environ, "PATH"), 
			  1, filename, O_RDONLY, 0, scratch_pathname);
  if (scratch_chan < 0)
    {
      scratch_chan = openp (get_in_environ 
			    (inferior_environ, "LD_LIBRARY_PATH"), 
			    1, filename, O_RDONLY, 0, scratch_pathname);
    }
  if (scratch_chan < 0)
    {
       /* so it's always freeable */
       *scratch_pathname = strdup(filename);
    }

  /* Free the file name from tilde_expand. */
  do_cleanups (old_chain);

  return scratch_chan;
}

static void
solib_map_sections (so, scratch_chan)
     struct so_list *so;
     int scratch_chan;
{
  char *filename;
  struct section_table *p;
  bfd *abfd;
  
  if (scratch_chan < 0)
    {
      perror_with_name (filename);
    }

  filename = so->so_name;
  abfd = bfd_fdopenr (filename, gnutarget, scratch_chan);
  if (!abfd)
    {
      close (scratch_chan);
      error ("Could not open `%s' as an executable file: %s",
	     filename, bfd_errmsg (bfd_get_error ()));
    }
  /* Leave bfd open, core_xfer_memory and "info files" need it.  */
  so->abfd = abfd;
  abfd->cacheable = true;

  if (!bfd_check_format (abfd, bfd_object))
    {
      error ("\"%s\": not in executable format: %s.",
	     filename, bfd_errmsg (bfd_get_error ()));
    }
  if ((*(lookup_sym_fns(exec_bfd)->build_section_table))
             (abfd, &so->sections, &so->sections_end))
    {
      error ("Can't find the file sections in `%s': %s", 
	     bfd_get_filename (abfd), bfd_errmsg (bfd_get_error ()));
    }

  for (p = so->sections; p < so->sections_end; p++)
    {
      /* Relocate the section binding addresses as recorded in the shared
	 object's file by the base address to which the object was actually
	 mapped. */
      p->addr += (CORE_ADDR) so->lm.l_offs;
      p->endaddr += (CORE_ADDR) so->lm.l_offs;
      so->lmend = (CORE_ADDR) max (p->endaddr, so->lmend);
      if (STREQ (p->the_bfd_section->name, ".text"))
	{
	  so->textsection = p;
	}
    }
}


/*

LOCAL FUNCTION
	locate_base -- locate the base address of dynamic linker structs

SYNOPSIS
	CORE_ADDR locate_base (void)

DESCRIPTION
	If the inferior executable has been linked dynamically,
	there is a single address somewhere in the inferior's data
	space which is the key to locating all of the dynamic
	linker's runtime structures.  This address is the value of
	the debug base symbol.  The job of this function is to find
	and return that address, or to return 0 if there is no such
	address (the executable is statically linked for example).

	The address of the dynamic linker's runtime structure is
	contained within the dynamic info section in the executable
	file.  The dynamic section is also mapped into the inferior
	address space.  Because the runtime loader fills in the
	real address before starting the inferior, we have to read
	in the dynamic info section from the inferior address space.

	If we find a condition indicating that there is no dynamic debug
	info, we return ERROR_ADDR.  If it's there, but the address isn't
	filled in yet, we return 0.  If it's damaged or incorrect, we 
	report an error and return ERROR_ADDR.  (In effect, 0 means
	"try again later".)
*/

static CORE_ADDR
locate_base ()
{
  sec_ptr dyninfo_sect;
  int dyninfo_sect_size;
  CORE_ADDR dyninfo_addr;
  char *buf;
  char *bufend;

  if (exec_bfd == NULL
      || bfd_get_flavour (exec_bfd) != bfd_target_coff_flavour)
    {
       error("locate_base: file of wrong type; cannot look up shared libs.\n");
       return ERROR_ADDR;
    }

  /* Find the start address of the .dynamic section.  */
  dyninfo_sect = bfd_get_section_by_name (exec_bfd, ".dynamic");
  if (dyninfo_sect == NULL)
    return ERROR_ADDR;

  dyninfo_addr = bfd_section_vma (exec_bfd, dyninfo_sect) 
		 + bfd_getImageBase(exec_bfd);

  /* Read in .dynamic section; if we got here, it's an error if we couldn't
     read it. */
  dyninfo_sect_size = bfd_section_size (exec_bfd, dyninfo_sect);
  buf = alloca (dyninfo_sect_size);
  if (target_read_memory (dyninfo_addr, buf, dyninfo_sect_size))
    {
      error ("locate_base: Can't read dynamic section\n");
      return ERROR_ADDR;
    }

  /* Find the DT_DEBUG entry in the the .dynamic section. */
  for (bufend = buf + dyninfo_sect_size;
       buf < bufend;
       buf += sizeof (PE_dyn))
    {
      PE_dyn *x_dynp = (PE_dyn *)buf;
      long dyn_tag;
      CORE_ADDR dyn_ptr;

      dyn_tag = bfd_h_get_32 (exec_bfd, (bfd_byte *) &x_dynp->d_tag);
      if (dyn_tag == DT_NULL)
	{
	  /* last entry; if we get here we didn't find it */
          return ERROR_ADDR;
	}
      else if (dyn_tag == DT_DEBUG)
	{
	  return
	     bfd_h_get_32 (exec_bfd, (bfd_byte *) &x_dynp->d_un.d_ptr);
	  break;
	}
    }
}

/*

LOCAL FUNCTION
	rebuild_solib_list -- read user memory and rebuild list

SYNOPSIS
	void rebuild_solib_list (add_symbols)

DESCRIPTION
	Walk the uerspace list of shared library modules and build the
	in-gdb version.  We try to recycle as much information as possible,
	but since the only information we get is "it changed" (and an
	indication that an addition or deletion is about to occur) we
	have to grub thru user memory anyway.  (Since we don't know which
	items were added or deleted (and can only guess that it was only
	one) it's just as easy to just do the whole thing.  We only need
	do this when notified of a change.

	ADD_SYMBOLS: whether or not to actually add the symbols.
*/

void
rebuild_solib_list (int add_symbols)
{
  struct link_map *lm = NULL;
  struct so_list *new;
  struct so_list **so_list_last;
  struct so_list *so_list_ptr;
  char *fullname;
  int chan;
  int new_file_read = 0;
  
  /* Scan the current list to its end and plub it into the front of the
     recycle list.  */
  so_list_ptr = so_list_head;
  so_list_last = &so_list_head;
  while (so_list_ptr)
    {
       so_list_last = &so_list_ptr->next;
       so_list_ptr = so_list_ptr->next;
    }

  /* Hook this on to the beginning of the recycle list */
  *so_list_last = so_recycle_list;
  so_recycle_list = so_list_head;

  /* and null out the currently active list */
  so_list_head = NULL;

  /* It's a good bet that things didn't change much, so we do things
     assuming that (for efficiency) but prepared for the worst. */
  
  /* If we got here, debug_base must reflect the current stat of affairs,
     as the decision to rebuild depends on reading the r_data field. */

  lm = debug_copy.r_map;
  so_list_last = &so_list_head;

  while (lm != NULL)
    {
      struct link_map t_lm;
      char *buffer;
      int status;
      struct so_list *so_recycle_ptr;
      struct so_list **so_recycle_last;

      /* read the current userspace entry into temporary space */
      status = target_read_memory ((CORE_ADDR) lm, (char *) &(t_lm),
				       sizeof (struct link_map));
      if (status != 0)
	error ("rebuild_solib_list: Can't read pathname for load map: %s\n",
	       safe_strerror (status));

      /* The first entry in the link map is for the
	 inferior executable, so we must ignore it. */
      if (t_lm.l_prev == NULL)
	{
	  lm = t_lm.l_next;
	  continue;
	}      

      /* Get the name since we have to match on it. */
      target_read_string ((CORE_ADDR) t_lm.l_name, &buffer,
			  MAX_PATH_SIZE - 1, &status);
      if (status != 0)
	error ("rebuild_solib_list: Can't read pathname for load map: %s\n",
	       safe_strerror (status));

      chan=solib_normalize_name(buffer,  &fullname);
      /* solib ends up opening the filename, and since we may want it,
	 we'll hang on to it until we know for sure */
      free(buffer);

      new = NULL;
      so_recycle_ptr = so_recycle_list;
      so_recycle_last = &so_recycle_list;
      /* try to match on load address, check name to b sure */
      while (so_recycle_ptr) 
	{
	  if (so_recycle_ptr->lm.l_addr == t_lm.l_addr &&
	     strcmp (so_recycle_ptr->so_name, fullname) == 0)
	    {
	       new = so_recycle_ptr;
	       *so_recycle_last = so_recycle_ptr->next;
	       so_recycle_ptr->next = NULL;
	       break;
	    }
	  so_recycle_last=&so_recycle_ptr->next;
	  so_recycle_ptr = so_recycle_ptr->next;
        }

      if (new == NULL)
	{
	  /* no match on address and name; try to match just on name;
	     getting here should be truly rare! */
	  so_recycle_ptr = so_recycle_list;
	  so_recycle_last = &so_recycle_list;
	  while (so_recycle_ptr) 
	    {
	      if (strcmp (so_recycle_ptr->so_name, fullname) == 0)
		{
		   new = so_recycle_ptr;
		   *so_recycle_last = so_recycle_ptr->next;
	           so_recycle_ptr->next = NULL;
		   break;
		}
	      so_recycle_last=&so_recycle_ptr->next;
	      so_recycle_ptr = so_recycle_ptr->next;
	    }
	}

      if (new == NULL)
	{
	  /* recycling failed... make a new one */
	  new = (struct so_list *) xmalloc (sizeof (struct so_list));
	  memset ((char *) new, 0, sizeof (struct so_list));
	  if (strlen (fullname) >= MAX_PATH_SIZE)
	    error ("Full path name length of shared library exceeds MAX_PATH_SIZE in so_list structure.");
	  strncpy (new->so_name, fullname, MAX_PATH_SIZE - 1);

	  new->so_name[MAX_PATH_SIZE - 1] = '\0';
	  new->lmaddr = lm;
	  new->lm = t_lm;
	  solib_map_sections (new, chan);
          /* Leave fullname allocated.  new->abfd->name will point to it.  */
	  if (add_symbols && catch_errors
		   (symbol_add_stub, (char *) new,
		    "Error while reading shared library symbols:\n",
		    RETURN_MASK_ALL))
	    {
	      new->symbols_loaded = 1;
	    }
	  new_file_read = 1;
	}
      else
	{
	  close(chan);
	  new->lmaddr = lm;
	  new->lm = t_lm;
	  // check file date and reload as needed.
          free (fullname);
	}

      *so_list_last = new;
      so_list_last = &new->next;

      lm = new->lm.l_next;
    }

  /* Getting new symbols may change our opinion about what is
     frameless.  */
  if (new_file_read)
    reinit_frame_cache ();

  /* as a consequence of a deletion or because the user hasn't loaded an entry
     retained on the recycle list from a previous exection, the recycle
     list may not be empty. It's a decent bet we'll need the entry later. */
  return;
}

/* A small stub to get us past the arg-passing pinhole of catch_errors.  */

static int
symbol_add_stub (arg)
     char *arg;
{
  register struct so_list *so = (struct so_list *) arg;	/* catch_errs bogon */
  CORE_ADDR text_addr = 0;

  if (so->textsection)
    text_addr = so->textsection->addr;
  else
    {
      asection *lowest_sect;

      /* If we didn't find a mapped non zero sized .text section, set up
	 text_addr so that the relocation in symbol_file_add does no harm.  */

      lowest_sect = bfd_get_section_by_name (so->abfd, ".text");
      if (lowest_sect == NULL)
	bfd_map_over_sections (so->abfd, find_lowest_section,
			       (PTR) &lowest_sect);
      if (lowest_sect)
	text_addr = bfd_section_vma (so->abfd, lowest_sect)
		    + (CORE_ADDR) so->lm.l_addr;
    }
  
  text_addr -= bfd_getImageBase(so->abfd);
  so->objfile =
    symbol_file_add (so->so_name, so->from_tty,
		     text_addr,
		     0, 0, 0);
  return (1);
}

/*

GLOBAL FUNCTION
	solib_add_entries -- add a shared library file to the 
	      symtab and section list

	This routine is called internally to this file, and also from
	infrun when a shared library breakpoint is hit.

SYNOPSIS
	void solib_add_entries (char *arg_string, int from_tty,
			struct target_ops *target)

DESCRIPTION

*/

void
solib_add_entries (arg_string, from_tty, target)
     char *arg_string;
     int from_tty;
     struct target_ops *target;
{	
  register struct so_list *so = NULL;   	/* link map state variable */

  /* Last shared library that we read.  */
  struct so_list *so_last = NULL;

  char *re_err;
  int count;
  int old;

  /* We may never have built a list if we're not in auto-add mode */
  if (!auto_solib_add)
      solib_add("", 0, NULL);
  
  if ((re_err = re_comp (arg_string ? arg_string : ".")) != NULL)
    {
      error ("Invalid regexp: %s", re_err);
    }
  
  /* It appears that target is always NULL in all calls */

  /* Add the shared library sections to the section table of the
     specified target, if any.  */
  if (target)
    {
      /* Count how many new section_table entries there are.  */
      count = 0;
      so = so_list_head;
      while (so != NULL)
	{
	  count += so->sections_end - so->sections;
	  so = so->next;
	}
      
      if (count)
	{
	  int update_coreops;

	  /* We must update the to_sections field in the core_ops structure
	     here, otherwise we dereference a potential dangling pointer
	     for each call to target_read/write_memory within this routine.  */
	  update_coreops = core_ops.to_sections == target->to_sections;
	     
	  /* Reallocate the target's section table including the new size.  */
	  if (target->to_sections)
	    {
	      old = target->to_sections_end - target->to_sections;
	      target->to_sections = (struct section_table *)
		xrealloc ((char *)target->to_sections,
			 (sizeof (struct section_table)) * (count + old));
	    }
	  else
	    {
	      old = 0;
	      target->to_sections = (struct section_table *)
		xmalloc ((sizeof (struct section_table)) * count);
	    }
	  target->to_sections_end = target->to_sections + (count + old);
	  
	  /* Update the to_sections field in the core_ops structure
	     if needed.  */
	  if (update_coreops)
	    {
	      core_ops.to_sections = target->to_sections;
	      core_ops.to_sections_end = target->to_sections_end;
	    }

	  /* Add these section table entries to the target's table.  */
          so = so_list_head;
	  while (so != NULL)
	    {
	      count = so->sections_end - so->sections;
	      memcpy ((char *) (target->to_sections + old),
		      so->sections, 
		      (sizeof (struct section_table)) * count);
	      old += count;
	      so = so->next;
	    }
	}
    }
  
  /* Now add the symbol files.  */
  so = so_list_head;
  while (so != NULL)
    {
      if (re_exec (so->so_name))
	{
	  so->from_tty = from_tty;
	  if (so->symbols_loaded)
	    {
	      if (from_tty)
		{
		  printf_unfiltered ("Symbols already loaded for %s\n", so->so_name);
		}
	    }
	  else if (catch_errors
		   (symbol_add_stub, (char *) so,
		    "Error while reading shared library symbols:\n",
		    RETURN_MASK_ALL))
	    {
	      so_last = so;
	      so->symbols_loaded = 1;
	    }
	}
      so = so->next;
    }

  /* Getting new symbols may change our opinion about what is
     frameless.  */
  if (so_last)
    reinit_frame_cache ();

}

/*

GLOBAL FUNCTION
	solib_add -- add a shared library file to the symtab and section list

	This routine is called from infrun when a shared library breakpoint
	is hit.  As a special case, it's also called (once) from a
	breakpoint set at _mainCRTstartup(), just to get things started.
	(We use _mainCRTstartup() rather than main() because it's in
	crt0.o; in the case of Fortran, main() ends up in the shared
	lib, which in turn calls MAIN__, but that's too messy to figure
	out in gdb.)

	If an initial call to locate_base() concludes that there's no
	possibility of a shared library, then the breakpoints are not
	set, and we are never called.

SYNOPSIS
	void solib_add (char *arg_string, int from_tty,
			struct target_ops *target)

DESCRIPTION

*/

/* How we tell re_enable_breakpoints_in_shlib not to do anything just
   yet: infrun calls this, and when deleting a shared lib, we want to
   skip that until the deletion is complete. */

extern int solib_is_deleting;

void
solib_add (arg_string, from_tty, target)
     char *arg_string;
     int from_tty;
     struct target_ops *target;
{	
  /* are thigs set up right, yet */
  if (debug_base == 0) 
     debug_base = locate_base();

  if (debug_base == ERROR_ADDR)
     return;

  if (debug_base != 0)
    {
      /* it's set up; we want to get the debug section to find out what's
	 going on. */
      read_memory (debug_base, (char *) &debug_copy, sizeof (struct r_debug));
      if (debug_copy.r_version != DYNAMIC_VERSION)
	 warning("Mismatched runtime loader version; attempting to proceed\n");
      // This is too useful to toss, yet,
      switch(debug_copy.r_state) //
	{
	case RT_ADD:
	   //fprintf(stderr, "Add state reported\n");
	   solib_is_deleting = 0;
           disable_breakpoints_in_shlibs();
	   break;
	case RT_DELETE:
	   //fprintf(stderr, "Delete state reported\n");
	   solib_is_deleting = 1;
           disable_breakpoints_in_shlibs();
	   break;
	case RT_CONSISTENT:
	   //fprintf(stderr, "Consistent state reported; rebuilding\n");
	   solib_is_deleting = 0;
           rebuild_solib_list(auto_solib_add);
	   re_enable_breakpoints_in_shlibs();
	   break;
	default:
	   //fprintf(stderr, "No state reported\n");
	   break;
	}
    }
  /* Else, things are still not set up; keep trying */

  enable_break(1);
}

/*

LOCAL FUNCTION
	info_sharedlibrary_command -- code for "info sharedlibrary"

SYNOPSIS
	static void info_sharedlibrary_command ()

DESCRIPTION
	Walk through the shared library list and print information
	about each attached library.
*/

static void
info_sharedlibrary_command (ignore, from_tty)
     char *ignore;
     int from_tty;
{
  register struct so_list *so;  	/* link map state variable */
  
  if (exec_bfd == NULL)
    {
      printf_unfiltered ("No exec file.\n");
      return;
    }

  /* We don't, now, want to remain totally ignorant of shared libs */
  if (!auto_solib_add)
      solib_add("", 0, NULL);

  if (so_list_head == NULL && so_recycle_list == NULL)
    {
      printf_unfiltered ("No shared libraries have been loaded.\n");	
      return;
    }

  printf_unfiltered("%-12s%-12s%-12s%s\n", "From", "To", "Syms Read",
	 "Shared Object Library");

  so = so_list_head;
  while (so != NULL)
    {
      /* FIXME-32x64: need print_address_numeric with field width or
	 some such.  */
      printf_unfiltered ("%-12s",
	      local_hex_string_custom ((unsigned long) so->lm.l_addr,
				       "08l"));
      printf_unfiltered ("%-12s",
	      local_hex_string_custom ((unsigned long) so->lmend,
				       "08l"));
      printf_unfiltered ("%-12s", so->symbols_loaded ? "Yes" : "No");
      printf_unfiltered ("%s\n",  so->so_name);
      so = so->next;
    }

  so = so_recycle_list;
  while (so != NULL)
    {
      if (so->so_name[0])
	{
	  /* FIXME-32x64: need print_address_numeric with field width or
	     some such.  */
	  printf_unfiltered ("%-24s", "<not currently loaded>");
	  printf_unfiltered ("%-12s", so->symbols_loaded ? "Yes" : "No");
	  printf_unfiltered ("%s\n",  so->so_name);
	}
      so = so->next;
    }
}

/*

GLOBAL FUNCTION
	solib_address -- check to see if an address is in a shared lib

SYNOPSIS
	char * solib_address (CORE_ADDR address)

DESCRIPTION
	Provides a hook for other gdb routines to discover whether or
	not a particular address is within the mapped address space of
	a shared library.  Any address between the base mapping address
	and the first address beyond the end of the last mapping, is
	considered to be within the shared library address space, for
	our purposes.

	For example, this routine is called at one point to disable
	breakpoints which are in shared libraries that are not currently
	mapped in.
 */

char *
solib_address (address)
     CORE_ADDR address;
{
  register struct so_list *so;   	/* link map state variable */
  
  so = so_list_head;
  while (so != NULL)
    {
      if ((address >= (CORE_ADDR) so->lm.l_addr) &&
	  (address < (CORE_ADDR) so->lmend))
	return (so->so_name);
      so = so->next;
    }
  return (0);
}

/* Called by free_all_symtabs */

void 
clear_solib()
{
  struct so_list *next;
  char *bfd_filename;
  
  while (so_list_head)
    {
      if (so_list_head->sections)
	{
	  free ((PTR)so_list_head->sections);
	}
      if (so_list_head->abfd)
	{
	  bfd_filename = bfd_get_filename (so_list_head->abfd);
	  if (!bfd_close (so_list_head->abfd))
	    warning ("cannot close \"%s\": %s",
		     bfd_filename, bfd_errmsg (bfd_get_error ()));
	}
      else
	/* This happens for the executable on SVR4.  */
	bfd_filename = NULL;
      
      next = so_list_head->next;
      if (bfd_filename)
	free ((PTR)bfd_filename);
      free ((PTR)so_list_head);
      so_list_head = next;
    }
  debug_base = 0;
  memset(&debug_copy, 0, sizeof(struct r_debug));
}

/*

LOCAL FUNCTION
	enable_break -- arrange for dynamic linker to hit breakpoint
	   solib_add will be called when that breakpoint is hit;
	   infrun.c "knows" this.

SYNOPSIS
	int enable_break (mode)

DESCRIPTION
	The dynamic linker has, as part of the debugger interface,
	support for arranging for the inferior to hit a breakpoint
	after mapping in the shared libraries.  This function
	enables that breakpoint.

	The debugger interface structure contains a member (r_brk)
	which is statically initialized at the time the shared library is
	built, to the offset of a function (_r_debug_state) which is guaran-
	teed to be called once before mapping in a library, and again when
	the mapping is complete.  At the time we are examining this member,
	it contains only the unrelocated offset of the function, so we have
	to do our own relocation.  Later, when the dynamic linker actually
	runs, it relocates r_brk to be the actual address of _r_debug_state().

	The debugger interface structure also contains an enumeration which
	is set to either RT_ADD or RT_DELETE prior to changing the mapping,
	depending upon whether or not the library is being mapped or unmapped,
	and then set to RT_CONSISTENT after the library is mapped/unmapped.

	MODE indicates whether the break should be at _mainCRTstartup (for 
	the very first time) or at the location the dynamic linker set up.
	Extra calls in mode 1 (without mode 0) are turned into noops.
*/

static int
enable_break (int mode)
{
  struct minimal_symbol *msymbol;

  /* First, remove all the solib event breakpoints.  Their addresses
     may have changed since the last time we ran the program.  */
  remove_solib_event_breakpoints ();

  /* If we have a meaningful r_brk, use it; if not, use main on the
     assumption that it's the first time and r_brk isn't set up yet. */

  if (mode == 0 
    && (msymbol = lookup_minimal_symbol("_mainCRTStartup", NULL, symfile_objfile)) != NULL
    && SYMBOL_VALUE_ADDRESS (msymbol) != 0)
    {
      disable_breakpoints_in_shlibs();
      create_solib_event_breakpoint (SYMBOL_VALUE_ADDRESS (msymbol));
    }
  else if (mode == 1 && debug_copy.r_brk != 0)
    {
      create_solib_event_breakpoint (debug_copy.r_brk);
    }
  else
    {
      /* For whatever reason we couldn't set a breakpoint in the dynamic
	 linker.  Warn.  */
      warning ("Unable to find dynamic linker breakpoint function.");
      warning ("GDB will be unable to debug shared library initializers");
      warning ("and track explicitly loaded dynamic code.");
    }

  return (1);
}
  
/*
  
GLOBAL FUNCTION
	solib_create_inferior_hook -- shared library startup support
  
SYNOPSIS
	void solib_create_inferior_hook()
  
DESCRIPTION
	When gdb starts up the inferior, it nurses it along (through the
	shell) until it is ready to execute it's first instruction.  At this
	point, this function gets called via expansion of the macro
	SOLIB_CREATE_INFERIOR_HOOK.

	The first instruction is typically the
	one at "_start", or a similar text label, regardless of whether
	the executable is statically or dynamically linked.  The runtime
	startup code takes care of dynamically linking in any shared
	libraries, once gdb allows the inferior to continue.

	We can arrange to cooperate with the dynamic linker to
	discover the names of shared libraries that are dynamically
	linked, and the base addresses to which they are linked.

	This function is responsible for discovering those names and
	addresses, and saving sufficient information about them to allow
	their symbols to be read at a later time.
  */

void 
solib_create_inferior_hook()
{
  debug_base = locate_base ();

  if (debug_base == ERROR_ADDR)
    {
      /* No shared libs possible; we're done */
      return;
    }

  /* Break at _mainCRTStartup (probably), this once. */
  enable_break (0);
  return;
}

/*

LOCAL FUNCTION
	sharedlibrary_command -- handle command to explicitly add library

SYNOPSIS
	static void sharedlibrary_command (char *args, int from_tty)

DESCRIPTION

*/

static void
sharedlibrary_command (args, from_tty)
char *args;
int from_tty;
{
  dont_repeat ();
  solib_add_entries (args, from_tty, (struct target_ops *) 0);
}


void
_initialize_solib()
{

  add_com ("sharedlibrary", class_files, sharedlibrary_command,
	   "Load shared object library symbols for files matching REGEXP.");
  add_info ("sharedlibrary", info_sharedlibrary_command, 
	    "Status of loaded shared object libraries.");

  add_show_from_set
    (add_set_cmd ("auto-solib-add", class_support, var_zinteger,
		  (char *) &auto_solib_add,
		  "Set autoloading of shared library symbols.\n\
If nonzero, symbols from all shared object libraries will be loaded\n\
automatically when the inferior begins execution or when the dynamic linker\n\
informs gdb that a new library has been loaded.  Otherwise, symbols\n\
must be loaded manually, using `sharedlibrary'.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("solib-absolute-prefix", class_support, var_filename,
		  (char *) &solib_absolute_prefix,
		  "Set prefix for loading absolute shared library symbol files.\n\
For other (relative) files, you can add values using `set solib-search-path'.",
		  &setlist),
     &showlist);
  add_show_from_set
    (add_set_cmd ("solib-search-path", class_support, var_string,
		  (char *) &solib_search_path,
		  "Set the search path for loading non-absolute shared library symbol files.\n\
This takes precedence over the environment variables PATH and LD_LIBRARY_PATH.",
		  &setlist),
     &showlist);

}
