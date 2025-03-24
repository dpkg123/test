/* Handle Interix (pei) shared libraries for GDB, the GNU Debugger.
   Derived from the similar (but rather different) PEI variant.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000,
   2001, 2002
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"

#include "symtab.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "target.h"
#include "inferior.h"

#include "solist.h"
#include "solib-pei.h"

static struct r_debug debug_copy;

static CORE_ADDR debug_base;	/* Base of dynamic linker structures */
static CORE_ADDR breakpoint_addr;	/* Address where end bkpt is set */

static int enable_break (int mode);

/*

LOCAL FUNCTION
	locate_base -- locate the base address of dynamic linker structs
		       and set debug_base to it (if it finds it!)

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

  /* Check to see if we have a currently valid address, and if so, avoid
     doing all this work again and just return the cached address.  If
     we have no cached address, try to locate it in the dynamic info
     section.  */

  if (debug_base != 0)
      return debug_base;

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
		 + NONZERO_LINK_BASE(exec_bfd);

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
          break;
	}
      else if (dyn_tag == DT_DEBUG)
	{
	  debug_base = 
	     bfd_h_get_32 (exec_bfd, (bfd_byte *) &x_dynp->d_un.d_ptr);
	  return
	     debug_base;
	}
    }
  return ERROR_ADDR;
}

/*

   LOCAL FUNCTION

   first_link_map_member -- locate first member in dynamic linker's map

   SYNOPSIS

   static CORE_ADDR first_link_map_member (void)

   DESCRIPTION

   Find the first element in the inferior's dynamic link map, and
   return its address in the inferior.  This function doesn't copy the
   link map entry itself into our address space; current_sos actually
   does the reading.  */

static int debug_sl_state = 0;   /* to be set with the debugger, 'natch. */

static CORE_ADDR
first_link_map_member (void)
{
  CORE_ADDR lm = 0;

  read_memory (debug_base, (char *) &debug_copy, sizeof (debug_copy));
  if (debug_copy.r_version != DYNAMIC_VERSION)
     warning("Mismatched runtime loader version; attempting to proceed\n");

  switch(debug_copy.r_state)
    {
    case RT_ADD:
       if (debug_sl_state)
	   fprintf(stderr, "Add state reported\n");
       disable_breakpoints_in_shlibs(1);
       break;
    case RT_DELETE:
       if (debug_sl_state)
	   fprintf(stderr, "Delete state reported\n");
       disable_breakpoints_in_shlibs(1);
       break;
    case RT_CONSISTENT:
       if (debug_sl_state)
	   fprintf(stderr, "Consistent state reported; rebuilding\n");
       re_enable_breakpoints_in_shlibs();
       break;
    default:
       if (debug_sl_state)
	   fprintf(stderr, "No state reported\n");
       break;
    }

  lm = (CORE_ADDR) debug_copy.r_map;

  return (lm);
}

/*

  LOCAL FUNCTION

  open_symbol_file_object

  SYNOPSIS

  void open_symbol_file_object (void *from_tty)

  DESCRIPTION

  If no open symbol file, attempt to locate and open the main symbol
  file.  On PEI systems, this is the first link map entry.  If its
  name is here, we can open it.  Useful when attaching to a process
  without first loading its symbol file.

  If FROM_TTYP dereferences to a non-zero integer, allow messages to
  be printed.  This parameter is a pointer rather than an int because
  open_symbol_file_object() is called via catch_errors() and
  catch_errors() requires a pointer argument. */

static int
open_symbol_file_object (void *from_ttyp)
{
  CORE_ADDR lm, l_name;
  CORE_ADDR t;
  char *filename;
  int errcode;
  int from_tty = *(int *)from_ttyp;
  struct link_map *tm;  /* Needed, but not really used. */
  CORE_ADDR res;

  if (symfile_objfile)
    if (!query ("Attempt to reload symbols from process? "))
      return 0;

  res = locate_base ();
  if (res == ERROR_ADDR)
    return 0;	/* failed somehow... */

  /* First link map member should be the executable.  */ // ?????
  lm = first_link_map_member ();
  if (lm == 0)
    return 0;	/* failed somehow... */

  /* Read address of name from target memory to GDB.  */
  read_memory (lm + offsetof(struct link_map, l_name), 
	       (void *)&l_name, sizeof(tm->l_name));

  if (l_name == 0)
    return 0;		/* No filename.  */

  /* Now fetch the filename from target memory.  */
  target_read_string (l_name, &filename, SO_NAME_MAX_PATH_SIZE - 1, &errcode);

  if (errcode)
    {
      warning ("failed to read exec filename from attached file: %s",
	       safe_strerror (errcode));
      return 0;
    }

  make_cleanup (xfree, filename);
  /* Have a pathname: read the symbol file.  */
  symbol_file_add_main (filename, from_tty);

  return 1;
}

/* LOCAL FUNCTION

   current_sos -- build a list of currently loaded shared objects

   SYNOPSIS

   struct so_list *current_sos ()

   DESCRIPTION

   Build a list of `struct so_list' objects describing the shared
   objects currently loaded in the inferior.  This list does not
   include an entry for the main executable file.

   Note that we only gather information directly available from the
   inferior --- we don't examine any of the shared library files
   themselves.  The declaration of `struct so_list' says which fields
   we provide values for.  */

static struct so_list *
pei_current_sos (void)
{
  CORE_ADDR lm;
  CORE_ADDR res;
  struct so_list *head = 0;
  struct so_list **link_ptr = &head;

  /* Make sure we've looked up the inferior's dynamic linker's base
     structure.  */
  res = locate_base ();

  if (res == ERROR_ADDR)
    return 0;

  /* Walk the inferior's link map list, and build our list of
     `struct so_list' nodes.  */
  lm = first_link_map_member ();  
  while (lm != 0)
    {
      struct so_list *new
	= (struct so_list *) xmalloc (sizeof (struct so_list));
      struct cleanup *old_chain = make_cleanup (xfree, new);
      int errcode;
      char *buffer;
      struct link_map lm_entry;

      memset (new, 0, sizeof (*new));

      new->lm_info = xmalloc (sizeof (struct lm_info));
      make_cleanup (xfree, new->lm_info);

      read_memory (lm, (void *)&lm_entry, sizeof(struct link_map));

      /* Extract this shared object's name.  */
      target_read_string ((CORE_ADDR)lm_entry.l_name, &buffer,
			  SO_NAME_MAX_PATH_SIZE - 1, &errcode);
      if (errcode != 0)
	{
	  warning ("current_sos: Can't read pathname for load map: %s\n",
		   safe_strerror (errcode));
	}
      else
	{
	  strncpy (new->so_name, buffer, SO_NAME_MAX_PATH_SIZE - 1);
	  new->so_name[SO_NAME_MAX_PATH_SIZE - 1] = '\0';
	  xfree (buffer);
	  strcpy (new->so_original_name, new->so_name);
	}

      /* Get the next pointer now. */
      lm = (CORE_ADDR)lm_entry.l_next;

      new->lm_info->lmoffset = (CORE_ADDR)lm_entry.l_offs;

      /* If this entry has no name, include it in the list.  It's the one
	 for the main program.  */
      if (new->so_name[0] == 0)
	{
	   free_so (new);
	}
      else
	{
	  new->next = 0;
	  *link_ptr = new;
	  link_ptr = &new->next;
	}

      discard_cleanups (old_chain);

    }

    /* Set up so we'll be told that dlopen() calls have occurred. */
    enable_break(1);

  return head;
}


/* Return 1 if PC lies in the dynamic symbol resolution code of the
   run time loader.  */
static CORE_ADDR interp_text_sect_low;
static CORE_ADDR interp_text_sect_high;
static CORE_ADDR interp_plt_sect_low;
static CORE_ADDR interp_plt_sect_high;

static int
pei_in_dynsym_resolve_code (CORE_ADDR pc)
{
  return ((pc >= interp_text_sect_low && pc < interp_text_sect_high)
	  || (pc >= interp_plt_sect_low && pc < interp_plt_sect_high)
	  || in_plt_section (pc, NULL));
}

/*

LOCAL FUNCTION
	enable_break -- arrange for dynamic linker to hit breakpoint
	   solib_add_xyzzy will be called when that breakpoint is hit;
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

  msymbol = lookup_minimal_symbol("_mainCRTStartup", NULL, symfile_objfile);
  if (mode == 0 && msymbol != NULL && SYMBOL_VALUE_ADDRESS (msymbol) != 0)
    {
      disable_breakpoints_in_shlibs(0);
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
      warning ("Unable to find dynamic linker breakpoint function.\nGDB will be unable to debug shared library initializers\nand track explicitly loaded dynamic code.");
    }

  return (1);
}

/*

   LOCAL FUNCTION

   special_symbol_handling -- additional shared library symbol handling

   SYNOPSIS

   void special_symbol_handling ()

   DESCRIPTION

   Once the symbols from a shared object have been loaded in the usual
   way, we are called to do any system specific symbol handling that 
   is needed.

   For SunOS4, this consisted of grunging around in the dynamic
   linkers structures to find symbol definitions for "common" symbols
   and adding them to the minimal symbol table for the runtime common
   objfile.

   However, for PEI, there's nothing to do.

 */

static void
pei_special_symbol_handling (void)
{
}

/* Relocate the main executable.  This function should be called upon
   stopping the inferior process at the entry point to the program. 
   The entry point from BFD is compared to the PC and if they are
   different, the main executable is relocated by the proper amount. 
   
   As written it will only attempt to relocate executables which
   lack interpreter sections.  It seems likely that only dynamic
   linker executables will get relocated, though it should work
   properly for a position-independent static executable as well.  */

static void
pei_relocate_main_executable (void)
{
#if 0 /// poss. a no-op.; however may make life simpler w.r.t. "free" relocs
  asection *interp_sect;
  CORE_ADDR pc = read_pc ();

  /* Decide if the objfile needs to be relocated.  As indicated above,
     we will only be here when execution is stopped at the beginning
     of the program.  Relocation is necessary if the address at which
     we are presently stopped differs from the start address stored in
     the executable AND there's no interpreter section.  The condition
     regarding the interpreter section is very important because if
     there *is* an interpreter section, execution will begin there
     instead.  When there is an interpreter section, the start address
     is (presumably) used by the interpreter at some point to start
     execution of the program.

     If there is an interpreter, it is normal for it to be set to an
     arbitrary address at the outset.  The job of finding it is
     handled in enable_break().

     So, to summarize, relocations are necessary when there is no
     interpreter section and the start address obtained from the
     executable is different from the address at which GDB is
     currently stopped.
     
     [ The astute reader will note that we also test to make sure that
       the executable in question has the DYNAMIC flag set.  It is my
       opinion that this test is unnecessary (undesirable even).  It
       was added to avoid inadvertent relocation of an executable
       whose e_type member in the ELF header is not ET_DYN.  There may
       be a time in the future when it is desirable to do relocations
       on other types of files as well in which case this condition
       should either be removed or modified to accomodate the new file
       type.  (E.g, an ET_EXEC executable which has been built to be
       position-independent could safely be relocated by the OS if
       desired.  It is true that this violates the ABI, but the ABI
       has been known to be bent from time to time.)  - Kevin, Nov 2000. ]
     */

  interp_sect = bfd_get_section_by_name (exec_bfd, ".interp");
  if (interp_sect == NULL 
      && (bfd_get_file_flags (exec_bfd) & DYNAMIC) != 0
      && bfd_get_start_address (exec_bfd) != pc)
    {
      struct cleanup *old_chain;
      struct section_offsets *new_offsets;
      int i, changed;
      CORE_ADDR displacement;
      
      /* It is necessary to relocate the objfile.  The amount to
	 relocate by is simply the address at which we are stopped
	 minus the starting address from the executable.

	 We relocate all of the sections by the same amount.  This
	 behavior is mandated by recent editions of the System V ABI. 
	 According to the System V Application Binary Interface,
	 Edition 4.1, page 5-5:

	   ...  Though the system chooses virtual addresses for
	   individual processes, it maintains the segments' relative
	   positions.  Because position-independent code uses relative
	   addressesing between segments, the difference between
	   virtual addresses in memory must match the difference
	   between virtual addresses in the file.  The difference
	   between the virtual address of any segment in memory and
	   the corresponding virtual address in the file is thus a
	   single constant value for any one executable or shared
	   object in a given process.  This difference is the base
	   address.  One use of the base address is to relocate the
	   memory image of the program during dynamic linking.

	 The same language also appears in Edition 4.0 of the System V
	 ABI and is left unspecified in some of the earlier editions.  */

      displacement = pc - bfd_get_start_address (exec_bfd);
      changed = 0;

      new_offsets = xcalloc (symfile_objfile->num_sections,
			     sizeof (struct section_offsets));
      old_chain = make_cleanup (xfree, new_offsets);

      for (i = 0; i < symfile_objfile->num_sections; i++)
	{
	  if (displacement != ANOFFSET (symfile_objfile->section_offsets, i))
	    changed = 1;
	  new_offsets->offsets[i] = displacement;
	}

      if (changed)
	objfile_relocate (symfile_objfile, new_offsets);

      do_cleanups (old_chain);
    }
#endif
}

/*

   GLOBAL FUNCTION

   pei_solib_create_inferior_hook -- shared library startup support

   SYNOPSIS

   void pei_solib_create_inferior_hook()

   DESCRIPTION

   When gdb starts up the inferior, it nurses it along (through the
   shell) until it is ready to execute it's first instruction.  At this
   point, this function gets called via expansion of the macro
   SOLIB_CREATE_INFERIOR_HOOK.

   This is at __mainCRTstartup.

   We arrange to cooperate with the dynamic linker to discover the
   names of shared libraries that are dynamically linked, and the
   base addresses to which they are linked.

 */

static void
pei_solib_create_inferior_hook (void)
{
  /* Relocate the main executable if necessary.  */
  pei_relocate_main_executable ();

  if (!enable_break (0))
    {
      warning ("shared library handler failed to enable breakpoint");
      return;
    }
}

/* This works because of the side-effects of setting debug_base to zero */
static void
pei_clear_solib (void)
{
  debug_base = 0;
}

static void
pei_free_so (struct so_list *so)
{
  xfree (so->lm_info);
}

static void
pei_relocate_section_addresses (struct so_list *so,
                                 struct section_table *sec)
{
  /* Since when reading symbols we put the ImageBase into them
     (which is required for statically linked programs), we need to
     back it out here, so it doesn't get added twice. */
  sec->addr    += so->lm_info->lmoffset;
  sec->endaddr += so->lm_info->lmoffset;
}

static struct target_so_ops pei_so_ops;

void
_initialize_pei_solib (void)
{
  pei_so_ops.relocate_section_addresses = pei_relocate_section_addresses;
  pei_so_ops.free_so = pei_free_so;
  pei_so_ops.clear_solib = pei_clear_solib;
  pei_so_ops.solib_create_inferior_hook = pei_solib_create_inferior_hook;
  pei_so_ops.special_symbol_handling = pei_special_symbol_handling;
  pei_so_ops.current_sos = pei_current_sos;
  pei_so_ops.open_symbol_file_object = open_symbol_file_object;
  pei_so_ops.in_dynsym_resolve_code = pei_in_dynsym_resolve_code;

  /* FIXME: Don't do this here.  *_gdbarch_init() should set so_ops. */
  current_target_so_ops = &pei_so_ops;
}
