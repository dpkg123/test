/* Read coff symbol tables and convert to internal format, for GDB.
   Copyright 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
   1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.
   Contributed by David D. Johnson, Brown University (ddj@cs.brown.edu).

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
#include "gdbtypes.h"
#include "demangle.h"
#include "breakpoint.h"

#include "bfd.h"
#include "gdb_obstack.h"

#include "gdb_string.h"
#include <ctype.h>

#include "coff/internal.h"	/* Internal format of COFF symbols in BFD */
#include "libcoff.h"		/* FIXME secret internal data from BFD */

#include "symfile.h"
#include "objfiles.h"
#include "buildsym.h"
#include "gdb-stabs.h"
#include "stabsread.h"
#include "complaints.h"
#include "target.h"
#include "gdb_assert.h"

extern void _initialize_coffread (void);

#ifndef ADJUST_OBJFILE_OFFSETS
#define ADJUST_OBJFILE_OFFSETS(objfile, type) \
    gnu_pei_adjust_objfile_offsets(objfile, type)
#endif

/* This little tapdance gives us a declaration.  Until this is more
   pervasive, the enum isn't in scope at the right time. */
void ADJUST_OBJFILE_OFFSETS(struct objfile *, enum objfile_adjusts);
struct coff_symfile_info
  {
    file_ptr min_lineno_offset;	/* Where in file lowest line#s are */
    file_ptr max_lineno_offset;	/* 1+last byte of line#s in file */

    CORE_ADDR textaddr;		/* Addr of .text section. */
    unsigned int textsize;	/* Size of .text section. */
    struct stab_section_list *stabsects;	/* .stab sections.  */
    asection *stabstrsect;	/* Section pointer for .stab section */
    char *stabstrdata;
  };

/* Translate an external name string into a user-visible name.  */
#define	EXTERNAL_NAME(string, abfd) \
	(string[0] == bfd_get_symbol_leading_char(abfd)? string+1: string)

/* To be an sdb debug type, type must have at least a basic or primary
   derived type.  Using this rather than checking against T_NULL is
   said to prevent core dumps if we try to operate on Michael Bloom
   dbx-in-coff file.  */

#define SDB_TYPE(type) (BTYPE(type) | (type & N_TMASK))

/* Core address of start and end of text of current source file.
   This comes from a ".text" symbol where x_nlinno > 0.  */

static CORE_ADDR current_source_start_addr;
static CORE_ADDR current_source_end_addr;

/* The addresses of the symbol table stream and number of symbols
   of the object file we are reading (as copied into core).  */

static bfd *nlist_bfd_global;
static int nlist_nsyms_global;


/* Pointers to scratch storage, used for reading raw symbols and auxents.  */

static char *temp_sym;
static char *temp_aux;

/* Local variables that hold the shift and mask values for the
   COFF file that we are currently reading.  These come back to us
   from BFD, and are referenced by their macro names, as well as
   internally to the BTYPE, ISPTR, ISFCN, ISARY, ISTAG, and DECREF
   macros from include/coff/internal.h .  */

static unsigned local_n_btmask;
static unsigned local_n_btshft;
static unsigned local_n_tmask;
static unsigned local_n_tshift;

#define	N_BTMASK	local_n_btmask
#define	N_BTSHFT	local_n_btshft
#define	N_TMASK		local_n_tmask
#define	N_TSHIFT	local_n_tshift

/* Local variables that hold the sizes in the file of various COFF structures.
   (We only need to know this to read them from the file -- BFD will then
   translate the data in them, into `internal_xxx' structs in the right
   byte order, alignment, etc.)  */

static unsigned local_linesz;
static unsigned local_symesz;
static unsigned local_auxesz;

/* This is set if this is a PE format file.  */

static int pe_file;

/* Chain of typedefs of pointers to empty struct/union types.
   They are chained thru the SYMBOL_VALUE_CHAIN.  */

static struct symbol *opaque_type_chain[HASHSIZE];

/* Complaints about various problems in the file being read  */

struct deprecated_complaint ef_complaint =
{"Unmatched .ef symbol(s) ignored starting at symnum %d", 0, 0};

struct deprecated_complaint ef_stack_complaint =
{"`.ef' symbol without matching `.bf' symbol ignored starting at symnum %d", 0, 0};

struct deprecated_complaint eb_stack_complaint =
{"`.eb' symbol without matching `.bb' symbol ignored starting at symnum %d", 0, 0};

struct deprecated_complaint bf_no_aux_complaint =
{"`.bf' symbol %d has no aux entry", 0, 0};

struct deprecated_complaint ef_no_aux_complaint =
{"`.ef' symbol %d has no aux entry", 0, 0};

struct deprecated_complaint lineno_complaint =
{"Line number pointer %d lower than start of line numbers", 0, 0};

struct deprecated_complaint unexpected_type_complaint =
{"Unexpected type for symbol %s", 0, 0};

struct deprecated_complaint bad_sclass_complaint =
{"Bad n_sclass for symbol %s", 0, 0};

struct deprecated_complaint misordered_blocks_complaint =
{"Blocks out of order at address %x", 0, 0};

struct deprecated_complaint tagndx_bad_complaint =
{"Symbol table entry for %s has bad tagndx value", 0, 0};

struct deprecated_complaint eb_complaint =
{"Mismatched .eb symbol ignored starting at symnum %d", 0, 0};

/* Simplified internal version of coff symbol table information */

struct coff_symbol
  {
    char *c_name;
    int c_symnum;		/* symbol number of this entry */
    int c_naux;			/* 0 if syment only, 1 if syment + auxent, etc */
    long c_value;
    int c_sclass;
    int c_secnum;
    unsigned int c_type;
  };

extern void stabsread_clear_cache (void);

static struct type *coff_read_struct_type (int, int, int);

static struct type *decode_base_type (struct coff_symbol *,
				      unsigned int, union internal_auxent *);

static struct type *decode_type (struct coff_symbol *, unsigned int,
				 union internal_auxent *);

static struct type *decode_function_type (struct coff_symbol *,
					  unsigned int,
					  union internal_auxent *);

static struct type *coff_read_enum_type (int, int, int);

static struct symbol *process_coff_symbol (struct coff_symbol *,
					   union internal_auxent *,
					   struct objfile *);

static void patch_opaque_types (struct symtab *);

static void enter_linenos (long, int, int, struct objfile *);

static void free_linetab (void);

static void free_linetab_cleanup (void *ignore);

static int init_lineno (bfd *, long, int);

static char *getsymname (struct internal_syment *);

static char *coff_getfilename (union internal_auxent *, int);

static void free_stringtab (void);

static void free_stringtab_cleanup (void *ignore);

static int init_stringtab (bfd *, long);

static void read_one_sym (struct coff_symbol *,
			  struct internal_syment *, union internal_auxent *);

static void coff_symtab_read (long, unsigned int, struct objfile *);

/* We are called once per section from coff_symfile_read.  We
   need to examine each section we are passed, check to see
   if it is something we are interested in processing, and
   if so, stash away some access information for the section.

   FIXME: The section names should not be hardwired strings (what
   should they be?  I don't think most object file formats have enough
   section flags to specify what kind of debug section it is
   -kingdon).  */

static void
coff_locate_sections (bfd *abfd, asection *sectp, void *csip)
{
  register struct coff_symfile_info *csi;
  const char *name;

  csi = (struct coff_symfile_info *) csip;
  name = bfd_get_section_name (abfd, sectp);
  if (STREQ (name, ".text"))
    {
      csi->textaddr = bfd_section_vma (abfd, sectp);
      csi->textsize += bfd_section_size (abfd, sectp);
    }
  else if (strncmp (name, ".text", sizeof ".text" - 1) == 0)
    {
      csi->textsize += bfd_section_size (abfd, sectp);
    }
  else if (STREQ (name, ".stabstr"))
    {
      csi->stabstrsect = sectp;
    }
  else if (strncmp (name, ".stab", sizeof ".stab" - 1) == 0)
    {
      const char *s;

      /* We can have multiple .stab sections if linked with
         --split-by-reloc.  */
      for (s = name + sizeof ".stab" - 1; *s != '\0'; s++)
	if (!isdigit (*s))
	  break;
      if (*s == '\0')
	{
	  struct stab_section_list *n, **pn;

	  n = ((struct stab_section_list *)
	       xmalloc (sizeof (struct stab_section_list)));
	  n->section = sectp;
	  n->next = NULL;
	  for (pn = &csi->stabsects; *pn != NULL; pn = &(*pn)->next)
	    ;
	  *pn = n;

	  /* This will be run after coffstab_build_psymtabs is called
	     in coff_symfile_read, at which point we no longer need
	     the information.  */
	  make_cleanup (xfree, n);
	}
    }
}

/* Return the section_offsets* that CS points to.  */
static int cs_to_section (struct coff_symbol *, struct objfile *);

struct find_targ_sec_arg
  {
    int targ_index;
    asection **resultp;
  };

static void
find_targ_sec (bfd *abfd, asection *sect, void *obj)
{
  struct find_targ_sec_arg *args = (struct find_targ_sec_arg *) obj;
  if (sect->target_index == args->targ_index)
    *args->resultp = sect;
}

/* Return the section number (SECT_OFF_*) that CS points to.  */
static int
cs_to_section (struct coff_symbol *cs, struct objfile *objfile)
{
  asection *sect = NULL;
  struct find_targ_sec_arg args;
  int off = SECT_OFF_TEXT (objfile);

  args.targ_index = cs->c_secnum;
  args.resultp = &sect;
  bfd_map_over_sections (objfile->obfd, find_targ_sec, &args);
  if (sect != NULL)
    {
      /* This is the section.  Figure out what SECT_OFF_* code it is.  */

      if (SECT_OFF_TEXT_P (objfile, sect->index))
	off = SECT_OFF_TEXT (objfile);
      else if (SECT_OFF_DATA_P (objfile, sect->index))
	off = SECT_OFF_DATA (objfile);
      else if (SECT_OFF_BSS_P (objfile, sect->index))
	off = SECT_OFF_BSS (objfile);
      else if (SECT_OFF_RODATA_P (objfile, sect->index))
	off = SECT_OFF_RODATA (objfile);
      else
	/* Just return the bfd section index. */
	off = sect->index;
    }
  return off;
}

/* Look up a coff type-number index.  Return the address of the slot
   where the type for that index is stored.
   The type-number is in INDEX. 

   This can be used for finding the type associated with that index
   or for associating a new type with the index.  */

static struct type **
coff_lookup_type (register int index)
{
  if (index >= type_vector_length)
    {
      int old_vector_length = type_vector_length;

      type_vector_length *= 2;
      if (index /* is still */  >= type_vector_length)
	type_vector_length = index * 2;

      type_vector = (struct type **)
	xrealloc ((char *) type_vector,
		  type_vector_length * sizeof (struct type *));
      memset (&type_vector[old_vector_length], 0,
	 (type_vector_length - old_vector_length) * sizeof (struct type *));
    }
  return &type_vector[index];
}

/* Make sure there is a type allocated for type number index
   and return the type object.
   This can create an empty (zeroed) type object.  */

static struct type *
coff_alloc_type (int index)
{
  register struct type **type_addr = coff_lookup_type (index);
  register struct type *type = *type_addr;

  /* If we are referring to a type not known at all yet,
     allocate an empty type for it.
     We will fill it in later if we find out how.  */
  if (type == NULL)
    {
      type = alloc_type (current_objfile);
      *type_addr = type;
    }
  return type;
}

/* Start a new symtab for a new source file.
   This is called when a COFF ".file" symbol is seen;
   it indicates the start of data for one original source file.  */

static void
coff_start_symtab (char *name)
{
  start_symtab (
  /* We fill in the filename later.  start_symtab puts
     this pointer into last_source_file and we put it in
     subfiles->name, which end_symtab frees; that's why
     it must be malloc'd.  */
		 savestring (name, strlen (name)),
  /* We never know the directory name for COFF.  */
		 NULL,
  /* The start address is irrelevant, since we set
     last_source_start_addr in coff_end_symtab.  */
		 0);
  record_debugformat ("COFF");
}

static void
range_symtab(start_addr, end_addr)
    CORE_ADDR start_addr;
    CORE_ADDR end_addr;
{
  if (current_objfile->ei.entry_point >= start_addr &&
      current_objfile->ei.entry_point <  end_addr)
    {
      current_objfile->ei.entry_file_lowpc = start_addr;
      current_objfile->ei.entry_file_highpc = end_addr;
    }
}
/* Save the vital information from when starting to read a file,
   for use when closing off the current file.
   NAME is the file name the symbols came from, START_ADDR is the first
   text address for the file, and SIZE is the number of bytes of text.  */

static void
complete_symtab (char *name, CORE_ADDR start_addr, unsigned int size)
{
  if (last_source_file != NULL)
    xfree (last_source_file);
  last_source_file = savestring (name, strlen (name));
  current_source_start_addr = start_addr;
  current_source_end_addr = start_addr + size;

  range_symtab(current_source_start_addr,current_source_end_addr);
}

/* Finish the symbol definitions for one main source file,
   close off all the lexical contexts for that file
   (creating struct block's for them), then make the
   struct symtab for that file and put it in the list of all such. */

static void
coff_end_symtab (struct objfile *objfile)
{
  struct symtab *symtab;

  last_source_start_addr = current_source_start_addr;

  symtab = end_symtab (current_source_end_addr, objfile, SECT_OFF_TEXT (objfile));

  if (symtab != NULL)
    free_named_symtabs (symtab->filename);

  /* Reinitialize for beginning of new file. */
  last_source_file = NULL;
}

static struct minimal_symbol *
record_minimal_symbol (char *name, CORE_ADDR address,
		       enum minimal_symbol_type type, struct objfile *objfile,
		       asection *bfd_section)
{
  /* We don't want TDESC entry points in the minimal symbol table */
  if (name[0] == '@')
    return NULL;

  return prim_record_minimal_symbol (name, address, type,
				     objfile, bfd_section);
}

/* coff_symfile_init ()
   is the coff-specific initialization routine for reading symbols.
   It is passed a struct objfile which contains, among other things,
   the BFD for the file whose symbols are being read, and a slot for
   a pointer to "private data" which we fill with cookies and other
   treats for coff_symfile_read ().

   We will only be called if this is a COFF or COFF-like file.
   BFD handles figuring out the format of the file, and code in symtab.c
   uses BFD's determination to vector to us.

   The ultimate result is a new symtab (or, FIXME, eventually a psymtab).  */

static void
coff_symfile_init (struct objfile *objfile)
{
  /* Allocate struct to keep track of stab reading. */
  objfile->sym_stab_info = (struct dbx_symfile_info *)
    xmmalloc (objfile->md, sizeof (struct dbx_symfile_info));

  memset (objfile->sym_stab_info, 0,
	  sizeof (struct dbx_symfile_info));

  /* Allocate struct to keep track of the symfile */
  objfile->sym_private = xmmalloc (objfile->md,
				   sizeof (struct coff_symfile_info));

  memset (objfile->sym_private, 0, sizeof (struct coff_symfile_info));

  /* COFF objects may be reordered, so set OBJF_REORDERED.  If we
     find this causes a significant slowdown in gdb then we could
     set it in the debug symbol readers only when necessary.  */
  objfile->flags |= OBJF_REORDERED;

  init_entry_point_info (objfile);
}

/* This function is called for every section; it finds the outer limits
   of the line table (minimum and maximum file offset) so that the
   mainline code can read the whole thing for efficiency.  */

/* ARGSUSED */
static void
find_linenos (bfd *abfd, sec_ptr asect, void *vpinfo)
{
  struct coff_symfile_info *info;
  int size, count;
  file_ptr offset, maxoff;

/* WARNING WILL ROBINSON!  ACCESSING BFD-PRIVATE DATA HERE!  FIXME!  */
  count = asect->lineno_count;
/* End of warning */

  if (count == 0)
    return;
  size = count * local_linesz;

  info = (struct coff_symfile_info *) vpinfo;
/* WARNING WILL ROBINSON!  ACCESSING BFD-PRIVATE DATA HERE!  FIXME!  */
  offset = asect->line_filepos;
/* End of warning */

  if (offset < info->min_lineno_offset || info->min_lineno_offset == 0)
    info->min_lineno_offset = offset;

  maxoff = offset + size;
  if (maxoff > info->max_lineno_offset)
    info->max_lineno_offset = maxoff;
}


/* The BFD for this file -- only good while we're actively reading
   symbols into a psymtab or a symtab.  */

static bfd *symfile_bfd;

/* Read a symbol file, after initialization by coff_symfile_init.  */

/* ARGSUSED */
static void
coff_symfile_read (struct objfile *objfile, int mainline)
{
  struct coff_symfile_info *info;
  struct dbx_symfile_info *dbxinfo;
  bfd *abfd = objfile->obfd;
  coff_data_type *cdata = coff_data (abfd);
  char *name = bfd_get_filename (abfd);
  register int val;
  unsigned int num_symbols;
  int symtab_offset;
  int stringtab_offset;
  struct cleanup *back_to;
  int stabstrsize;
  int len;
  char * target;
  
  info = (struct coff_symfile_info *) objfile->sym_private;
  dbxinfo = objfile->sym_stab_info;
  symfile_bfd = abfd;		/* Kludge for swap routines */

/* WARNING WILL ROBINSON!  ACCESSING BFD-PRIVATE DATA HERE!  FIXME!  */
  num_symbols = bfd_get_symcount (abfd);	/* How many syms */
  symtab_offset = cdata->sym_filepos;	/* Symbol table file offset */
  stringtab_offset = symtab_offset +	/* String table file offset */
    num_symbols * cdata->local_symesz;

  /* Set a few file-statics that give us specific information about
     the particular COFF file format we're reading.  */
  local_n_btmask = cdata->local_n_btmask;
  local_n_btshft = cdata->local_n_btshft;
  local_n_tmask = cdata->local_n_tmask;
  local_n_tshift = cdata->local_n_tshift;
  local_linesz = cdata->local_linesz;
  local_symesz = cdata->local_symesz;
  local_auxesz = cdata->local_auxesz;

  /* Allocate space for raw symbol and aux entries, based on their
     space requirements as reported by BFD.  */
  /* allocate BUFSIZ worth more entries... see C_FILE for why */
  temp_sym = (char *) xmalloc
	 (cdata->local_symesz + local_auxesz*(BUFSIZ/local_auxesz+1));
  temp_aux = temp_sym + cdata->local_symesz;
  back_to = make_cleanup (free_current_contents, &temp_sym);

  /* We need to know whether this is a PE file, because in PE files,
     unlike standard COFF files, symbol values are stored as offsets
     from the section address, rather than as absolute addresses.
     FIXME: We should use BFD to read the symbol table, and thus avoid
     this problem.  */
  pe_file =
    strncmp (bfd_get_target (objfile->obfd), "pe", 2) == 0
    || strncmp (bfd_get_target (objfile->obfd), "epoc-pe", 7) == 0;

/* End of warning */

  info->min_lineno_offset = 0;
  info->max_lineno_offset = 0;

  /* Only read line number information if we have symbols.

     On Windows NT, some of the system's DLL's have sections with
     PointerToLinenumbers fields that are non-zero, but point at
     random places within the image file.  (In the case I found,
     KERNEL32.DLL's .text section has a line number info pointer that
     points into the middle of the string `lib\\i386\kernel32.dll'.)

     However, these DLL's also have no symbols.  The line number
     tables are meaningless without symbols.  And in fact, GDB never
     uses the line number information unless there are symbols.  So we
     can avoid spurious error messages (and maybe run a little
     faster!) by not even reading the line number table unless we have
     symbols.  */
  if (num_symbols > 0)
    {
      /* Read the line number table, all at once.  */
      bfd_map_over_sections (abfd, find_linenos, (void *) info);

      make_cleanup (free_linetab_cleanup, 0 /*ignore*/);
      val = init_lineno (abfd, info->min_lineno_offset,
                         info->max_lineno_offset - info->min_lineno_offset);
      if (val < 0)
        error ("\"%s\": error reading line numbers\n", name);
    }

  /* Now read the string table, all at once.  */

  make_cleanup (free_stringtab_cleanup, 0 /*ignore*/);
  val = init_stringtab (abfd, stringtab_offset);
  if (val < 0)
    error ("\"%s\": can't get string table", name);

  init_minimal_symbol_collection ();
  make_cleanup_discard_minimal_symbols ();

  /* Now that the executable file is positioned at symbol table,
     process it and define symbols accordingly.  */

  /* Some implementations (e.g. PE) have their symbols stored as
     relative to some base other than zero.  Adjust the offset
     array to compensate.  (And then put things back like we found
     them.)   (Doing this here saves lots of execution time while
     reading the symbols, because it's automatic.) */

  ADJUST_OBJFILE_OFFSETS(objfile, adjust_for_symtab);
  coff_symtab_read ((long) symtab_offset, num_symbols, objfile);
  ADJUST_OBJFILE_OFFSETS(objfile, adjust_for_symtab_restore);

  /* Sort symbols alphabetically within each block.  */

  {
    struct symtab *s;

    for (s = objfile->symtabs; s != NULL; s = s->next)
      sort_symtab_syms (s);
  }

  /* Install any minimal symbols that have been collected as the current
     minimal symbols for this objfile.  */

  install_minimal_symbols (objfile);

  bfd_map_over_sections (abfd, coff_locate_sections, (void *) info);

  if (info->stabsects)
    {
      if (!info->stabstrsect)
	{
	  error (("The debugging information in `%s' is corrupted.\n"
		  "The file has a `.stabs' section, but no `.stabstr' "
		  "section."),
		 name);
	}

      /* FIXME: dubious.  Why can't we use something normal like
         bfd_get_section_contents?  */
      bfd_seek (abfd, abfd->where, 0);

      stabstrsize = bfd_section_size (abfd, info->stabstrsect);

      ADJUST_OBJFILE_OFFSETS(objfile, adjust_for_stabs);
      coffstab_build_psymtabs (objfile,
			       mainline,
			       info->textaddr, info->textsize,
			       info->stabsects,
			       info->stabstrsect->filepos, stabstrsize);
      ADJUST_OBJFILE_OFFSETS(objfile, adjust_for_stabs_restore);

    }
  if (dwarf2_has_info (abfd))
    {
      /* DWARF2 sections.  */
      ADJUST_OBJFILE_OFFSETS(objfile, adjust_for_dwarf);
      dwarf2_build_psymtabs (objfile, mainline);
      ADJUST_OBJFILE_OFFSETS(objfile, adjust_for_dwarf_restore);
    }

  do_cleanups (back_to);
}

static void
coff_new_init (struct objfile *ignore)
{
}

/* Perform any local cleanups required when we are done with a particular
   objfile.  I.E, we are in the process of discarding all symbol information
   for an objfile, freeing up all memory held for it, and unlinking the
   objfile struct from the global list of known objfiles. */

static void
coff_symfile_finish (struct objfile *objfile)
{
  if (objfile->sym_private != NULL)
    {
      xmfree (objfile->md, objfile->sym_private);
    }

  /* Let stabs reader clean up */
  stabsread_clear_cache ();
}


/* Given pointers to a symbol table in coff style exec file,
   analyze them and create struct symtab's describing the symbols.
   NSYMS is the number of symbols in the symbol table.
   We read them one at a time using read_one_sym ().  */

static void
coff_symtab_read (long symtab_offset, unsigned int nsyms,
		  struct objfile *objfile)
{
  register struct context_stack *new;
  struct coff_symbol coff_symbol;
  register struct coff_symbol *cs = &coff_symbol;
  static struct internal_syment main_sym;
  static union internal_auxent main_aux;
  struct coff_symbol fcn_cs_saved;
  static struct internal_syment fcn_sym_saved;
  static union internal_auxent fcn_aux_saved;
  struct symtab *s;
  /* A .file is open.  */
  int in_source_file = 0;
  int next_file_symnum = -1;
  /* Name of the current file.  */
  char *filestring = "";
  int depth = 0;
  int fcn_first_line = 0;
  CORE_ADDR fcn_first_line_addr = 0;
  int fcn_last_line = 0;
  int fcn_start_addr = 0;
  long fcn_line_ptr = 0;
  int val;
  CORE_ADDR tmpaddr;
  struct minimal_symbol **msyms;   /* A temporary array of the results,
				       see C_NT_WEAK. */

  /* On the use of the following symbols:  Earlier versions of this routine
     tried to key off .text entries to determine the size of a "file"
     (an original source file).  This implied that there were as many
     .text entries as there were .o files in the link.  That was OK as
     long as ld -r wasn't involved.  Linking the result of ld -r requires
     that there be only one .text symbol (otherwise, which one do you
     relocate against?)  (There was also a FIXME against using the symbol
     ".text" for that comparison.)  The only information used from the
     multiple .text symbols was the section size to determine where the
     code ended.  With the .text symbols gone, some other way to determine
     the size had to be found.  The three symbols below accomplish that
     based on the function symbols themselves (and the .ef, if present).
     first_function_this_file is the RVA of the first function
     found after a C_FILE entry.  most_recent_function_end is the
     RVA of the end of the most recent function found.  The size
     of the "file" can be determined from the difference between
     most_recent_function_end and first_function_this_file.

     A zero value for first_function_this_file indicates that no function
     has been found since the last C_FILE .  That's used both to capture
     the first one, and to indicate a "file" with no content, which needs
     to be handled slightly specially.  (More on that below.) */

  CORE_ADDR first_function_this_file;
  CORE_ADDR first_function_last_file;
  CORE_ADDR most_recent_function_end;

  /* Work around a stdio bug in SunOS4.1.1 (this makes me nervous....
     it's hard to know I've really worked around it.  The fix should be
     harmless, anyway).  The symptom of the bug is that the first
     fread (in read_one_sym), will (in my example) actually get data
     from file offset 268, when the fseek was to 264 (and ftell shows
     264).  This causes all hell to break loose.  I was unable to
     reproduce this on a short test program which operated on the same
     file, performing (I think) the same sequence of operations.

     It stopped happening when I put in this (former) rewind().

     FIXME: Find out if this has been reported to Sun, whether it has
     been fixed in a later release, etc.  */

  bfd_seek (objfile->obfd, 0, 0);

  /* Position to read the symbol table. */
  val = bfd_seek (objfile->obfd, (long) symtab_offset, 0);
  if (val < 0)
    perror_with_name (objfile->name);

  current_objfile = objfile;
  nlist_bfd_global = objfile->obfd;
  nlist_nsyms_global = nsyms;
  last_source_file = NULL;
  memset (opaque_type_chain, 0, sizeof opaque_type_chain);

  if (type_vector)		/* Get rid of previous one */
    xfree (type_vector);
  type_vector_length = 160;
  type_vector = (struct type **)
    xmalloc (type_vector_length * sizeof (struct type *));
  memset (type_vector, 0, type_vector_length * sizeof (struct type *));

  /* If the first line isn't a C_FILE, it's benign to create an empty symtab;
     it'll get discarded almost immediately, so if it simplifies the code.... */
  coff_start_symtab ("");

  first_function_this_file = most_recent_function_end = 0;
  /* If the first file (crt0.o) is not debuggable (which is probably
     the case) and starts with static functions (also likely), we'll
     want to use the beginning of the text section instead. */
  first_function_last_file = ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT(objfile));

  symnum = 0;
  msyms = xmalloc (sizeof(struct msymbol *) * nsyms);
  memset(msyms, 0, sizeof(struct msymbol *) * nsyms);

  while (symnum < nsyms)
    {
      QUIT;			/* Make this command interruptable.  */

      read_one_sym (cs, &main_sym, &main_aux);

      /* Global symbols after the last C_FILE */
      if (cs->c_symnum == next_file_symnum && cs->c_sclass != C_FILE)
	{
	  if (first_function_this_file != 0)
	    complete_symtab (filestring,
		   first_function_this_file
			    + ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT(objfile)),
		   most_recent_function_end == 0 
		      ? 0:most_recent_function_end - first_function_this_file);
	  else
	    complete_symtab (filestring, 0, 0);

	  if (last_source_file)
	    coff_end_symtab (objfile);

	  coff_start_symtab ("_globals_");
	  complete_symtab ("_globals_", 0, 0);
	  /* done with all files, everything from here on out is globals */
	}

      /* Special case for file with type declarations only, no text.  */
      if (!last_source_file && SDB_TYPE (cs->c_type)
	  && cs->c_secnum == N_DEBUG)
	complete_symtab (filestring, 0, 0);

      /* Typedefs should not be treated as symbol definitions.  */
      if (ISFCN (cs->c_type) && cs->c_sclass != C_TPDEF)
	{
	  struct minimal_symbol *msym;

	  if (first_function_this_file == 0)
	     first_function_this_file = cs->c_value;
	  else 
	    {
	      /* most_recent_function_end should be AT LEAST the beginning
		 of the current function if we're on other than the first
		 function in the file.  We'll try to do better below. */
	     most_recent_function_end = cs->c_value;
	    }

	  /* Record all functions -- external and static -- in minsyms. */
	  tmpaddr = cs->c_value + ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));
	  /* We also need to flag dynamic symbols, which are undefined
	     functions (usually) so that the special handling of them
	     works correctly.  At this point, dynamic == undefined;
	     if it's really undefined, we deal with that later. */
 	  msym = record_minimal_symbol (cs->c_name, tmpaddr, 
	     cs->c_secnum == 0 ? mst_solib_trampoline : mst_text, objfile,
  	     coff_section_from_bfd_index(objfile->obfd, cs->c_secnum));
	  msyms[cs->c_symnum] = msym;

	  if (cs->c_naux > 0) 
	    {
	      fcn_line_ptr = main_aux.x_sym.x_fcnary.x_fcn.x_lnnoptr;
	      fcn_aux_saved = main_aux;
	      most_recent_function_end = cs->c_value +
		  main_aux.x_sym.x_misc.x_fsize;
	    }
	  else
	    {
	      /* Aux is trash; pretend it's zeros; this can happen
		 if debugging is turned off (at least on MSVC); for
		 most_recent_function_end we'll use zero (as a flag).
		 In some cases, we use the beginning as it's good enough,
		 but when we can infer a better size at the beginning of
		 the next function, we'll use that.  That's OK since we can't 
		 debug symbolically anyway.  (In some instances (PE) a size 
		 might be in an .ef record, but so far, .ef and this aux go 
		 together so there's no point in trying that.)

		 FIXME: if/when we start parsing .pdata, we can do MUCH
		 better if it's available.  */

	      fcn_line_ptr = 0;
	      memset((void *)&fcn_aux_saved, 0, sizeof(fcn_aux_saved));
	    }

	  fcn_line_ptr = main_aux.x_sym.x_fcnary.x_fcn.x_lnnoptr;
	  fcn_start_addr = tmpaddr;
	  fcn_cs_saved = *cs;
	  fcn_sym_saved = main_sym;
	  fcn_aux_saved = main_aux;
 
	  /* Shared lib symbols need not apply. */
	  if (cs->c_secnum != 0)
	    {
	      /* If first_function_last_file != zero, we had gotten a sizeless
		 function earlier;  we keep going until we DO get a size, and 
		 presume that everthing in the range up to the current function
		 is a single file. */
	      if (most_recent_function_end != 0)
		{
		  if (first_function_last_file != 0)
		     range_symtab(first_function_last_file, tmpaddr-1);
		  first_function_last_file = 0;
		}
	      else
		{
		  if (first_function_last_file == 0)
		     first_function_last_file = tmpaddr;
		}
	    }
 
	  continue;
	}

      switch (cs->c_sclass)
	{
	case C_EFCN:
	case C_EXTDEF:
	case C_ULABEL:
	case C_USTATIC:
#ifndef COFF_IMAGE_WITH_PE
	case C_LINE:
	case C_ALIAS:
#endif
	case C_HIDDEN:
	  complain (&bad_sclass_complaint, cs->c_name);
	  break;

	case C_FILE:
	  /* Complete symbol table for last object file
	     containing debugging information.  */
	  if (first_function_this_file != 0)
		complete_symtab (filestring,
		   first_function_this_file
		   + ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile)),
		   most_recent_function_end == 0 
		     ? 0:most_recent_function_end - first_function_this_file);
	  else
		complete_symtab (filestring, 0, 0);
	  coff_end_symtab (objfile);

	  /* c_value field contains symnum of next .file entry in table
	     or symnum of first global after last .file.  */
	  next_file_symnum = cs->c_value;
	  if (cs->c_naux > 0)
	    filestring = coff_getfilename (&main_aux, cs->c_naux);
	  else
	    filestring = "";

	  coff_start_symtab (filestring);

	  first_function_this_file = 0;
	  break;

	/* C_LABEL is used for labels and static functions.  Including
	   it here allows gdb to see static functions when no debug
	   info is available.  */
	case C_LABEL:
	  /* However, labels within a function can make weird backtraces,
	     so filter them out (from phdm@macqel.be). */
	  if (within_function)
	    break;
#ifdef COFF_IMAGE_WITH_PE
	case C_SECTION:  /* found in some DLL libs */
#endif
	case C_STAT:
	case C_THUMBLABEL:
	case C_THUMBSTAT:
	case C_THUMBSTATFUNC:
	  if (cs->c_name[0] == '.')
	    {
	      /* flush '.' symbols */
	      break;
	    }
	  else if (!SDB_TYPE (cs->c_type)
		   && cs->c_name[0] == 'L'
		   && (strncmp (cs->c_name, "LI%", 3) == 0
		       || strncmp (cs->c_name, "LF%", 3) == 0
		       || strncmp (cs->c_name, "LC%", 3) == 0
		       || strncmp (cs->c_name, "LP%", 3) == 0
		       || strncmp (cs->c_name, "LPB%", 4) == 0
		       || strncmp (cs->c_name, "LBB%", 4) == 0
		       || strncmp (cs->c_name, "LBE%", 4) == 0
		       || strncmp (cs->c_name, "LPBX%", 5) == 0))
	    /* At least on a 3b1, gcc generates swbeg and string labels
	       that look like this.  Ignore them.  */
	    break;
	  /* On Intel, local names can be Lnnn; we can discard them
	     (and lots of other junk) if there's a leading char. */
	  if (bfd_get_symbol_leading_char(objfile->obfd) &&
	      cs->c_name[0] != bfd_get_symbol_leading_char(objfile->obfd))
	      break;
	  /* fall in for static symbols that don't start with '.' */
	case C_THUMBEXT:
	case C_THUMBEXTFUNC:
	case C_EXT:
	  {
	    /* Record it in the minimal symbols regardless of
	       SDB_TYPE.  This parallels what we do for other debug
	       formats, and probably is needed to make
	       print_address_symbolic work right without the (now
	       gone) "set fast-symbolic-addr off" kludge.  */

	    enum minimal_symbol_type ms_type;
	    int sec;

	    if (cs->c_secnum == N_ABS)
	      {
		tmpaddr = cs->c_value;
		sec = N_ABS;
		ms_type =  mst_abs;
	      }
	    else if (cs->c_secnum == N_UNDEF)
	      {
		/* This is a common symbol.  See if the target
		   environment knows where it has been relocated to.  */
		CORE_ADDR reladdr;
		if (target_lookup_symbol (cs->c_name, &reladdr))
		  {
		    /* Error in lookup; ignore symbol.  */
                    msyms[cs->c_symnum] = (struct minimal_symbol *)-1;
		    break;
		  }
		tmpaddr = reladdr;
		/* The address has already been relocated; make sure that
		   objfile_relocate doesn't relocate it again.  */
		sec = N_DEBUG;
		ms_type = cs->c_sclass == C_EXT
		  || cs->c_sclass == C_THUMBEXT ?
		  mst_bss : mst_file_bss;
	      }
	    else
	      {
	        int external_class = 0;
		sec = cs_to_section (cs, objfile);
		tmpaddr = cs->c_value;
		if (cs->c_sclass == C_EXT || cs->c_sclass == C_THUMBEXTFUNC
		    || cs->c_sclass == C_THUMBEXT)
		  {
		    tmpaddr += ANOFFSET (objfile->section_offsets, sec);
		    external_class = 1;
		  }
#ifdef COFF_IMAGE_WITH_PE
		/* this may be a general change */
		if (cs->c_sclass == C_STAT || cs->c_sclass == C_LABEL ||
		    cs->c_sclass == C_SECTION)
		  {
		    tmpaddr += ANOFFSET (objfile->section_offsets, sec);
		    external_class = 1;
		  }
#endif

		if (SECT_OFF_TEXT_P (objfile, sec)
		    || SECT_OFF_RODATA_P (objfile, sec)
		    )
		  {
		    ms_type =
		      cs->c_sclass == external_class ? 
		      mst_text : mst_file_text;
		    tmpaddr = SMASH_TEXT_ADDRESS (tmpaddr);
		  }
		else if (SECT_OFF_DATA_P (objfile, sec))
		  {
		    ms_type =
		      cs->c_sclass == external_class ?
		      mst_data : mst_file_data;
		  }
		else if (SECT_OFF_BSS_P (objfile, sec))
		  {
		    ms_type =
		      cs->c_sclass == external_class ?
		      mst_data : mst_file_data;
		  }
		else
		  ms_type = mst_unknown;
	      }

	    if (cs->c_name[0] != '@' /* Skip tdesc symbols */ )
	      {
		struct minimal_symbol *msym;

 		/* FIXME: cagney/2001-02-01: The nasty (int) -> (long)
                   -> (void*) cast is to ensure that that the value of
                   cs->c_sclass can be correctly stored in a void
                   pointer in MSYMBOL_INFO.  Better solutions
                   welcome. */
		gdb_assert (sizeof (void *) >= sizeof (cs->c_sclass));
		msym = prim_record_minimal_symbol_and_info
		 (cs->c_name, tmpaddr, ms_type, (void *) (long) cs->c_sclass,
	         sec, coff_section_from_bfd_index (objfile->obfd, cs->c_secnum),
	         objfile);
		msyms[cs->c_symnum] = msym;
		if (msym)
		  COFF_MAKE_MSYMBOL_SPECIAL (cs->c_sclass, msym);
	      }
	    if (SDB_TYPE (cs->c_type))
	      {
		struct symbol *sym;
		sym = process_coff_symbol
		  (cs, &main_aux, objfile);
		SYMBOL_VALUE (sym) = tmpaddr;
		SYMBOL_SECTION (sym) = sec;
	      }
	  }
	  break;

	case C_FCN:
	  if (STREQ (cs->c_name, ".bf"))
	    {
	      within_function = 1;

	      /* value contains address of first non-init type code */
	      /* main_aux.x_sym.x_misc.x_lnsz.x_lnno
	         contains line number of '{' } */
	      if (cs->c_naux != 1)
		complain (&bf_no_aux_complaint, cs->c_symnum);
	      fcn_first_line = main_aux.x_sym.x_misc.x_lnsz.x_lnno;
	      fcn_first_line_addr = cs->c_value;

	      /* Might want to check that locals are 0 and
	         context_stack_depth is zero, and complain if not.  */

	      depth = 0;
	      new = push_context (depth, fcn_start_addr);
	      fcn_cs_saved.c_name = getsymname (&fcn_sym_saved);
	      new->name =
		process_coff_symbol (&fcn_cs_saved, &fcn_aux_saved, objfile);
	    }
	  else if (STREQ (cs->c_name, ".ef"))
	    {
	      if (!within_function)
		error ("Bad coff function information\n");
	      /* the value of .ef is the address of epilogue code;
	         not useful for gdb.  */
	      /* { main_aux.x_sym.x_misc.x_lnsz.x_lnno
	         contains number of lines to '}' */

	      if (context_stack_depth <= 0)
		{		/* We attempted to pop an empty context stack */
		  complain (&ef_stack_complaint, cs->c_symnum);
		  within_function = 0;
		  break;
		}

	      new = pop_context ();
	      /* Stack must be empty now.  */
	      if (context_stack_depth > 0 || new == NULL)
		{
		  complain (&ef_complaint, cs->c_symnum);
		  within_function = 0;
		  break;
		}
	      if (cs->c_naux != 1)
		{
		  complain (&ef_no_aux_complaint, cs->c_symnum);
		  fcn_last_line = 0x7FFFFFFF;
		}
	      else
		{
		  fcn_last_line = main_aux.x_sym.x_misc.x_lnsz.x_lnno;
		}
	      /* fcn_first_line is the line number of the opening '{'.
	         Do not record it - because it would affect gdb's idea
	         of the line number of the first statement of the function -
	         except for one-line functions, for which it is also the line
	         number of all the statements and of the closing '}', and
	         for which we do not have any other statement-line-number. */
	      if (fcn_last_line == 1)
		record_line (current_subfile, fcn_first_line,
			     fcn_first_line_addr);
	      else
		enter_linenos (fcn_line_ptr, fcn_first_line, fcn_last_line,
			       objfile);

	      finish_block (new->name, &local_symbols, new->old_blocks,
			    new->start_addr,
#if defined (FUNCTION_EPILOGUE_SIZE)
	      /* This macro should be defined only on
	         machines where the
	         fcn_aux_saved.x_sym.x_misc.x_fsize
	         field is always zero.
	         So use the .bf record information that
	         points to the epilogue and add the size
	         of the epilogue.  */
			    cs->c_value
			    + FUNCTION_EPILOGUE_SIZE
			    + ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile)),
#else
			    fcn_cs_saved.c_value
			    + fcn_aux_saved.x_sym.x_misc.x_fsize
			    + ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile)),
#endif
			    objfile
		);
	      within_function = 0;
	    }
	  break;

	case C_BLOCK:
	  if (STREQ (cs->c_name, ".bb"))
	    {
	      tmpaddr = cs->c_value;
	      tmpaddr += ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));
	      push_context (++depth, tmpaddr);
	    }
	  else if (STREQ (cs->c_name, ".eb"))
	    {
	      if (context_stack_depth <= 0)
		{		/* We attempted to pop an empty context stack */
		  complain (&eb_stack_complaint, cs->c_symnum);
		  break;
		}

	      new = pop_context ();
	      if (depth-- != new->depth)
		{
		  complain (&eb_complaint, symnum);
		  break;
		}
	      if (local_symbols && context_stack_depth > 0)
		{
		  tmpaddr =
		    cs->c_value + ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));
		  /* Make a block for the local symbols within.  */
		  finish_block (0, &local_symbols, new->old_blocks,
				new->start_addr, tmpaddr, objfile);
		}
	      /* Now pop locals of block just finished.  */
	      local_symbols = new->locals;
	    }
	  break;

	case C_NT_WEAK:
	  {
	    /* NT Weaks are really indirect symbols; aux contains the target. */
	    int realsym;
	    struct minimal_symbol *msym;

	    realsym = main_aux.x_sym.x_tagndx.l;
	    if (0 && cs->c_secnum == 0)  // breaks b printf; absence breaks ?
	      {
		 /* nothing; ignore it (it's an undefined weak; the definition
		    will take care of things later.) */
	      }
	    else if (realsym < 0 || realsym >= nsyms || msyms[realsym] == NULL)
	      {
		warning ("\"%s\": indirect symbol does not have real one (%d)\n", 
		  cs->c_name, realsym);
	      }
	    else if (msyms[realsym] == (struct minimal_symbol *)-1)
	      {
		/* nothing; just ignore it. (We discarded the undefined
		   real symbol, so do the same with this.) */
	      }
	    else
	      {
		msym = prim_record_minimal_symbol_and_info
		  (cs->c_name, 
		  SYMBOL_VALUE_ADDRESS(msyms[realsym]),
		  MSYMBOL_TYPE (msyms[realsym]),
		  MSYMBOL_INFO (msyms[realsym]),  /* c_sclass for us. */
		  SYMBOL_SECTION (msyms[realsym]),
		  SYMBOL_BFD_SECTION (msyms[realsym]),
		  objfile);
		msyms[cs->c_symnum] = msym;
	      }
	    break;
	  }

	default:
	  process_coff_symbol (cs, &main_aux, objfile);
	  break;
	}
    }

    /* All we needed these for was a lookup, we're done now. */
    xfree (msyms);

    if (first_function_this_file != 0)
	  complete_symtab (filestring,
	     first_function_this_file
		    + ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT(objfile)),
	     most_recent_function_end == 0 
		    ? 0:most_recent_function_end - first_function_this_file);
    else
	  complete_symtab (filestring, 0, 0);

  /* Patch up any opaque types (references to types that are not defined
     in the file where they are referenced, e.g. "struct foo *bar").  */
  ALL_OBJFILE_SYMTABS (objfile, s)
    patch_opaque_types (s);

  current_objfile = NULL;
}

/* Routines for reading headers and symbols from executable.  */

/* Read the next symbol, swap it, and return it in both internal_syment
   form, and coff_symbol form.  Also return its first auxent, if any,
   in internal_auxent form, and skip any other auxents.  */

static void
read_one_sym (register struct coff_symbol *cs,
	      register struct internal_syment *sym,
	      register union internal_auxent *aux)
{
  int i;

  cs->c_symnum = symnum;
  bfd_bread (temp_sym, local_symesz, nlist_bfd_global);
  bfd_coff_swap_sym_in (symfile_bfd, temp_sym, (char *) sym);
  cs->c_naux = sym->n_numaux & 0xff;
  if (cs->c_naux >= 1)
    {
      bfd_bread (temp_aux, local_auxesz, nlist_bfd_global);
      bfd_coff_swap_aux_in (symfile_bfd, temp_aux, sym->n_type, sym->n_sclass,
			    0, cs->c_naux, (char *) aux);
      /* If more than one aux entry, read past it (only the first aux
         is important). */
      /* not really... put the rest where we can find them for the moment,
         see C_FILE */
      for (i = 1; i < cs->c_naux; i++)
	bfd_bread (&temp_aux[i*local_auxesz], local_auxesz, nlist_bfd_global);
    }
  cs->c_name = getsymname (sym);
  cs->c_value = sym->n_value;
  cs->c_sclass = (sym->n_sclass & 0xff);
  cs->c_secnum = sym->n_scnum;
  cs->c_type = (unsigned) sym->n_type;
  if (!SDB_TYPE (cs->c_type))
    cs->c_type = 0;

#if 0
  if (cs->c_sclass & 128)
    printf ("thumb symbol %s, class 0x%x\n", cs->c_name, cs->c_sclass);
#endif

  symnum += 1 + cs->c_naux;
}

/* Support for string table handling */

static char *stringtab = NULL;

static int
init_stringtab (bfd *abfd, long offset)
{
  long length;
  int val;
  unsigned char lengthbuf[4];

  free_stringtab ();

  /* If the file is stripped, the offset might be zero, indicating no
     string table.  Just return with `stringtab' set to null. */
  if (offset == 0)
    return 0;

  if (bfd_seek (abfd, offset, 0) < 0)
    return -1;

  val = bfd_bread ((char *) lengthbuf, sizeof lengthbuf, abfd);
  length = bfd_h_get_32 (symfile_bfd, lengthbuf);

  /* If no string table is needed, then the file may end immediately
     after the symbols.  Just return with `stringtab' set to null. */
  if (val != sizeof lengthbuf || length < sizeof lengthbuf)
    return 0;

  stringtab = (char *) xmalloc (length);
  /* This is in target format (probably not very useful, and not currently
     used), not host format.  */
  memcpy (stringtab, lengthbuf, sizeof lengthbuf);
  if (length == sizeof length)	/* Empty table -- just the count */
    return 0;

  val = bfd_bread (stringtab + sizeof lengthbuf, length - sizeof lengthbuf,
		   abfd);
  if (val != length - sizeof lengthbuf || stringtab[length - 1] != '\0')
    return -1;

  return 0;
}

static void
free_stringtab (void)
{
  if (stringtab)
    xfree (stringtab);
  stringtab = NULL;
}

static void
free_stringtab_cleanup (void *ignore)
{
  free_stringtab ();
}

static char *
getsymname (struct internal_syment *symbol_entry)
{
  static char buffer[SYMNMLEN + 1];
  char *result;

  if (symbol_entry->_n._n_n._n_zeroes == 0)
    {
      /* FIXME: Probably should be detecting corrupt symbol files by
         seeing whether offset points to within the stringtab.  */
      result = stringtab + symbol_entry->_n._n_n._n_offset;
    }
  else
    {
      strncpy (buffer, symbol_entry->_n._n_name, SYMNMLEN);
      buffer[SYMNMLEN] = '\0';
      result = buffer;
    }
  return result;
}

/* Extract the file name from the aux entry of a C_FILE symbol.  Return
   only the last component of the name.  Result is in static storage and
   is only good for temporary use.  */

/* The MSVC C might choose to use more than one AUX entry ! */

static char *
coff_getfilename (union internal_auxent *aux_entry, int naux)
{
  static char buffer[BUFSIZ];
  char *temp1, *temp2;
  char *result;

  if (aux_entry->x_file.x_n.x_zeroes == 0)
    strcpy (buffer, stringtab + aux_entry->x_file.x_n.x_offset);
  else
    {
      /* MS C stuffs multiple AUX entries rather than adding
	 to the string table.  Fish it out of temp_aux */
      strncpy (buffer, temp_aux, local_auxesz*naux);
      buffer[local_auxesz*naux] = '\0';
    }
  result = buffer;

  /* FIXME: We should not be throwing away the information about what
     directory.  It should go into dirname of the symtab, or some such
     place.  */
  temp1 = strrchr (result, '/');
  temp2 = strrchr (result, '\\');
  if (temp2 != NULL && (temp1 == NULL || temp2 > temp1))
    temp1 = temp2;
  if (temp1 != NULL)
    result = temp1 + 1;
  return (result);
}

/* Support for line number handling.  */

static char *linetab = NULL;
static long linetab_offset;
static unsigned long linetab_size;

/* Read in all the line numbers for fast lookups later.  Leave them in
   external (unswapped) format in memory; we'll swap them as we enter
   them into GDB's data structures.  */

static int
init_lineno (bfd *abfd, long offset, int size)
{
  int val;

  linetab_offset = offset;
  linetab_size = size;

  free_linetab ();

  if (size == 0)
    return 0;

  if (bfd_seek (abfd, offset, 0) < 0)
    return -1;

  /* Allocate the desired table, plus a sentinel */
  linetab = (char *) xmalloc (size + local_linesz);

  val = bfd_bread (linetab, size, abfd);
  if (val != size)
    return -1;

  /* Terminate it with an all-zero sentinel record */
  memset (linetab + size, 0, local_linesz);

  return 0;
}


/* This does the equivalent of what older code did (but both more
   efficiently and with a finer grain of control), but I'm not
   convinced this was right to begin with: the offset of a symbol
   is NOT the offset of its section, but rather of the image base. */

struct adjust_sec_objfile
  {
    struct objfile *objfile;
    int sign;
  };

static void
adjust_sec_objfile_offset (bfd *abfd, asection *sect, PTR obj)
{
  struct adjust_sec_objfile *args = (struct adjust_sec_objfile *) obj;
  int off;
  CORE_ADDR symbols_offset;

  /* This is the section.  Figure out what SECT_OFF_* code it is.  */
  if (SECT_OFF_TEXT_P (args->objfile, sect->index))
    off = SECT_OFF_TEXT (args->objfile);
  else if (SECT_OFF_DATA_P (args->objfile, sect->index))
    off = SECT_OFF_DATA (args->objfile);
  else if (SECT_OFF_BSS_P (args->objfile, sect->index))
    off = SECT_OFF_BSS (args->objfile);
  else if (SECT_OFF_RODATA_P (args->objfile, sect->index))
    off = SECT_OFF_RODATA (args->objfile);
  else
    /* Just use the bfd section index. */
    off = sect->index;

  symbols_offset = bfd_get_section_vma (objfile->obfd, sect) * args->sign;

  args->objfile->section_offsets->offsets[off] += symbols_offset;
}

static void
gnu_pei_adjust_objfile_offsets(struct objfile *objfile, 
                               enum objfile_adjusts type)
{
  struct adjust_sec_objfile args;

  if (!pe_file)
    return;

  args.objfile = objfile;

  switch (type) {
  case adjust_for_symtab:
    args.sign = 1;
    break;
  case adjust_for_symtab_restore:
    args.sign = -1;
    break;
  case adjust_for_stabs:
  case adjust_for_stabs_restore:
  case adjust_for_dwarf:
  case adjust_for_dwarf_restore:
  default:
    return;
  }

  bfd_map_over_sections (objfile->obfd, adjust_sec_objfile_offset, &args);
}

static void
free_linetab (void)
{
  if (linetab)
    xfree (linetab);
  linetab = NULL;
}

static void
free_linetab_cleanup (void *ignore)
{
  free_linetab ();
}

#if !defined (L_LNNO32)
#define L_LNNO32(lp) ((lp)->l_lnno)
#endif

static void
enter_linenos (long file_offset, register int first_line,
	       register int last_line, struct objfile *objfile)
{
  register char *rawptr;
  struct internal_lineno lptr;

  if (!linetab)
    return;
  /* no line numbers recorded in the function (-g1) */
  if (file_offset == 0)
    return;
  if (file_offset < linetab_offset)
    {
      complain (&lineno_complaint, file_offset);
      if (file_offset > linetab_size)	/* Too big to be an offset? */
	return;
      file_offset += linetab_offset;	/* Try reading at that linetab offset */
    }

  rawptr = &linetab[file_offset - linetab_offset];

  /* skip first line entry for each function */
  rawptr += local_linesz;
#ifdef COFF_IMAGE_WITH_PE
  /* MS PE format has the last line (in .ef) as a file line number, not
     a relative one; compensate.  Also, the start is 0 based already. */
  last_line = (last_line - first_line) + 1;
#else
  /* line numbers start at one for the first line of the function */
  first_line--;
#endif

  for (;;)
    {
      bfd_coff_swap_lineno_in (symfile_bfd, rawptr, &lptr);
      rawptr += local_linesz;
      /* The next function, or the sentinel, will have L_LNNO32 zero; we exit. */
      if (L_LNNO32 (&lptr) && L_LNNO32 (&lptr) <= last_line)
	record_line (current_subfile, first_line + L_LNNO32 (&lptr),
		     lptr.l_addr.l_paddr
		     + ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile)));
      else
	break;
    }
}

static void
patch_type (struct type *type, struct type *real_type)
{
  register struct type *target = TYPE_TARGET_TYPE (type);
  register struct type *real_target = TYPE_TARGET_TYPE (real_type);
  int field_size = TYPE_NFIELDS (real_target) * sizeof (struct field);

  TYPE_LENGTH (target) = TYPE_LENGTH (real_target);
  TYPE_NFIELDS (target) = TYPE_NFIELDS (real_target);
  TYPE_FIELDS (target) = (struct field *) TYPE_ALLOC (target, field_size);

  memcpy (TYPE_FIELDS (target), TYPE_FIELDS (real_target), field_size);

  if (TYPE_NAME (real_target))
    {
      if (TYPE_NAME (target))
	xfree (TYPE_NAME (target));
      TYPE_NAME (target) = concat (TYPE_NAME (real_target), NULL);
    }
}

/* Patch up all appropriate typedef symbols in the opaque_type_chains
   so that they can be used to print out opaque data structures properly.  */

static void
patch_opaque_types (struct symtab *s)
{
  register struct block *b;
  register int i;
  register struct symbol *real_sym;

  /* Go through the per-file symbols only */
  b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK);
  ALL_BLOCK_SYMBOLS (b, i, real_sym)
    {
      /* Find completed typedefs to use to fix opaque ones.
         Remove syms from the chain when their types are stored,
         but search the whole chain, as there may be several syms
         from different files with the same name.  */
      if (SYMBOL_CLASS (real_sym) == LOC_TYPEDEF &&
	  SYMBOL_NAMESPACE (real_sym) == VAR_NAMESPACE &&
	  TYPE_CODE (SYMBOL_TYPE (real_sym)) == TYPE_CODE_PTR &&
	  TYPE_LENGTH (TYPE_TARGET_TYPE (SYMBOL_TYPE (real_sym))) != 0)
	{
	  register char *name = SYMBOL_NAME (real_sym);
	  register int hash = hashname (name);
	  register struct symbol *sym, *prev;

	  prev = 0;
	  for (sym = opaque_type_chain[hash]; sym;)
	    {
	      if (name[0] == SYMBOL_NAME (sym)[0] &&
		  STREQ (name + 1, SYMBOL_NAME (sym) + 1))
		{
		  if (prev)
		    {
		      SYMBOL_VALUE_CHAIN (prev) = SYMBOL_VALUE_CHAIN (sym);
		    }
		  else
		    {
		      opaque_type_chain[hash] = SYMBOL_VALUE_CHAIN (sym);
		    }

		  patch_type (SYMBOL_TYPE (sym), SYMBOL_TYPE (real_sym));

		  if (prev)
		    {
		      sym = SYMBOL_VALUE_CHAIN (prev);
		    }
		  else
		    {
		      sym = opaque_type_chain[hash];
		    }
		}
	      else
		{
		  prev = sym;
		  sym = SYMBOL_VALUE_CHAIN (sym);
		}
	    }
	}
    }
}

static struct symbol *
process_coff_symbol (register struct coff_symbol *cs,
		     register union internal_auxent *aux,
		     struct objfile *objfile)
{
  register struct symbol *sym
  = (struct symbol *) obstack_alloc (&objfile->symbol_obstack,
				     sizeof (struct symbol));
  char *name;

  memset (sym, 0, sizeof (struct symbol));
  name = cs->c_name;
  name = EXTERNAL_NAME (name, objfile->obfd);
  SYMBOL_NAME (sym) = obsavestring (name, strlen (name),
				    &objfile->symbol_obstack);
  SYMBOL_LANGUAGE (sym) = language_auto;
  SYMBOL_INIT_DEMANGLED_NAME (sym, &objfile->symbol_obstack);

  /* default assumptions */
  SYMBOL_VALUE (sym) = cs->c_value;
  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
  SYMBOL_SECTION (sym) = cs_to_section (cs, objfile);

  if (ISFCN (cs->c_type))
    {
      SYMBOL_VALUE (sym) += ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));
#ifdef COFF_IMAGE_WITH_PE
      /* If we get a "function returning T_NULL" it's probably from
	 MSVC, and altho we can't say much about it, it's unlikely
	 that it really returns nothing; so we'll guess and say it
	 returns an integer (or pointer).  This would mostly be a
	 nuisance except that value_allocate_space_in_inferior
	 needs somewhat meaningful type information (at least a size)
	 to successfully do things such as 'print "foo"'. */
      if (cs->c_type == (T_NULL | (DT_FCN << N_BTSHFT)))
	  SYMBOL_TYPE(sym) = lookup_function_type (
	    decode_function_type (cs, (T_INT | (DT_FCN << N_BTSHFT)), aux));
      else
#endif
	  SYMBOL_TYPE (sym) =
	    lookup_function_type (decode_function_type (cs, cs->c_type, aux));

      SYMBOL_CLASS (sym) = LOC_BLOCK;
      if (cs->c_sclass == C_STAT || cs->c_sclass == C_THUMBSTAT
	  || cs->c_sclass == C_THUMBSTATFUNC)
	add_symbol_to_list (sym, &file_symbols);
      else if (cs->c_sclass == C_EXT || cs->c_sclass == C_THUMBEXT
	       || cs->c_sclass == C_THUMBEXTFUNC)
	add_symbol_to_list (sym, &global_symbols);
    }
  else
    {
      SYMBOL_TYPE (sym) = decode_type (cs, cs->c_type, aux);
      switch (cs->c_sclass)
	{
	case C_NULL:
	  break;

	case C_AUTO:
	  SYMBOL_CLASS (sym) = LOC_LOCAL;
	  add_symbol_to_list (sym, &local_symbols);
	  break;

	case C_THUMBEXT:
	case C_THUMBEXTFUNC:
	case C_EXT:
	  SYMBOL_CLASS (sym) = LOC_STATIC;
	  SYMBOL_VALUE_ADDRESS (sym) = (CORE_ADDR) cs->c_value;
	  SYMBOL_VALUE_ADDRESS (sym) += ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));
	  add_symbol_to_list (sym, &global_symbols);
	  break;

	case C_THUMBSTAT:
	case C_THUMBSTATFUNC:
	case C_STAT:
	  SYMBOL_CLASS (sym) = LOC_STATIC;
	  SYMBOL_VALUE_ADDRESS (sym) = (CORE_ADDR) cs->c_value;
	  SYMBOL_VALUE_ADDRESS (sym) += ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));
	  if (within_function)
	    {
	      /* Static symbol of local scope */
	      add_symbol_to_list (sym, &local_symbols);
	    }
	  else
	    {
	      /* Static symbol at top level of file */
	      add_symbol_to_list (sym, &file_symbols);
	    }
	  break;

#ifdef C_GLBLREG		/* AMD coff */
	case C_GLBLREG:
#endif
	case C_REG:
	  SYMBOL_CLASS (sym) = LOC_REGISTER;
	  SYMBOL_VALUE (sym) = SDB_REG_TO_REGNUM (cs->c_value);
	  add_symbol_to_list (sym, &local_symbols);
	  break;

	case C_THUMBLABEL:
	case C_LABEL:
	  break;

	case C_ARG:
	  SYMBOL_CLASS (sym) = LOC_ARG;
	  add_symbol_to_list (sym, &local_symbols);
#if !defined (BELIEVE_PCC_PROMOTION)
	  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
	    {
	      /* If PCC says a parameter is a short or a char,
	         aligned on an int boundary, realign it to the
	         "little end" of the int.  */
	      struct type *temptype;
	      temptype = lookup_fundamental_type (current_objfile,
						  FT_INTEGER);
	      if (TYPE_LENGTH (SYMBOL_TYPE (sym)) < TYPE_LENGTH (temptype)
		  && TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_INT
		  && 0 == SYMBOL_VALUE (sym) % TYPE_LENGTH (temptype))
		{
		  SYMBOL_VALUE (sym) +=
		    TYPE_LENGTH (temptype)
		    - TYPE_LENGTH (SYMBOL_TYPE (sym));
		}
	    }
#endif
	  break;

	case C_REGPARM:
	  SYMBOL_CLASS (sym) = LOC_REGPARM;
	  SYMBOL_VALUE (sym) = SDB_REG_TO_REGNUM (cs->c_value);
	  add_symbol_to_list (sym, &local_symbols);
#if !defined (BELIEVE_PCC_PROMOTION)
	  /* FIXME:  This should retain the current type, since it's just
	     a register value.  gnu@adobe, 26Feb93 */
	  {
	    /* If PCC says a parameter is a short or a char,
	       it is really an int.  */
	    struct type *temptype;
	    temptype =
	      lookup_fundamental_type (current_objfile, FT_INTEGER);
	    if (TYPE_LENGTH (SYMBOL_TYPE (sym)) < TYPE_LENGTH (temptype)
		&& TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_INT)
	      {
		SYMBOL_TYPE (sym) =
		  (TYPE_UNSIGNED (SYMBOL_TYPE (sym))
		   ? lookup_fundamental_type (current_objfile,
					      FT_UNSIGNED_INTEGER)
		   : temptype);
	      }
	  }
#endif
	  break;

	case C_TPDEF:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;

	  /* If type has no name, give it one */
	  if (TYPE_NAME (SYMBOL_TYPE (sym)) == 0)
	    {
	      if (TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_PTR
		  || TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_FUNC)
		{
		  /* If we are giving a name to a type such as "pointer to
		     foo" or "function returning foo", we better not set
		     the TYPE_NAME.  If the program contains "typedef char
		     *caddr_t;", we don't want all variables of type char
		     * to print as caddr_t.  This is not just a
		     consequence of GDB's type management; CC and GCC (at
		     least through version 2.4) both output variables of
		     either type char * or caddr_t with the type
		     refering to the C_TPDEF symbol for caddr_t.  If a future
		     compiler cleans this up it GDB is not ready for it
		     yet, but if it becomes ready we somehow need to
		     disable this check (without breaking the PCC/GCC2.4
		     case).

		     Sigh.

		     Fortunately, this check seems not to be necessary
		     for anything except pointers or functions.  */
		  ;
		}
	      else
		TYPE_NAME (SYMBOL_TYPE (sym)) =
		  concat (SYMBOL_NAME (sym), NULL);
	    }
#ifdef CXUX_TARGET
	  /* Ignore vendor section for Harris CX/UX targets. */
	  else if (cs->c_name[0] == '$')
	    break;
#endif /* CXUX_TARGET */

	  /* Keep track of any type which points to empty structured type,
	     so it can be filled from a definition from another file.  A
	     simple forward reference (TYPE_CODE_UNDEF) is not an
	     empty structured type, though; the forward references
	     work themselves out via the magic of coff_lookup_type.  */
	  if (TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_PTR &&
	      TYPE_LENGTH (TYPE_TARGET_TYPE (SYMBOL_TYPE (sym))) == 0 &&
	      TYPE_CODE (TYPE_TARGET_TYPE (SYMBOL_TYPE (sym))) !=
	      TYPE_CODE_UNDEF)
	    {
	      register int i = hashname (SYMBOL_NAME (sym));

	      SYMBOL_VALUE_CHAIN (sym) = opaque_type_chain[i];
	      opaque_type_chain[i] = sym;
	    }
	  add_symbol_to_list (sym, &file_symbols);
	  break;

	case C_STRTAG:
	case C_UNTAG:
	case C_ENTAG:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_NAMESPACE (sym) = STRUCT_NAMESPACE;

	  /* Some compilers try to be helpful by inventing "fake"
	     names for anonymous enums, structures, and unions, like
	     "~0fake" or ".0fake".  Thanks, but no thanks... */
	  if (TYPE_TAG_NAME (SYMBOL_TYPE (sym)) == 0)
	    if (SYMBOL_NAME (sym) != NULL
		&& *SYMBOL_NAME (sym) != '~'
		&& *SYMBOL_NAME (sym) != '.')
	      TYPE_TAG_NAME (SYMBOL_TYPE (sym)) =
		concat (SYMBOL_NAME (sym), NULL);

	  add_symbol_to_list (sym, &file_symbols);
	  break;

	default:
	  break;
	}
    }
  return sym;
}

/* Decode a coff type specifier;  return the type that is meant.  */

static struct type *
decode_type (register struct coff_symbol *cs, unsigned int c_type,
	     register union internal_auxent *aux)
{
  register struct type *type = 0;
  unsigned int new_c_type;

  if (c_type & ~N_BTMASK)
    {
      new_c_type = DECREF (c_type);
      if (ISPTR (c_type))
	{
	  type = decode_type (cs, new_c_type, aux);
	  type = lookup_pointer_type (type);
	}
      else if (ISFCN (c_type))
	{
	  type = decode_type (cs, new_c_type, aux);
	  type = lookup_function_type (type);
	}
      else if (ISARY (c_type))
	{
	  int i, n;
	  register unsigned short *dim;
	  struct type *base_type, *index_type, *range_type;

	  /* Define an array type.  */
	  /* auxent refers to array, not base type */
	  if (aux->x_sym.x_tagndx.l == 0)
	    cs->c_naux = 0;

	  /* shift the indices down */
	  dim = &aux->x_sym.x_fcnary.x_ary.x_dimen[0];
	  i = 1;
	  n = dim[0];
	  for (i = 0; *dim && i < DIMNUM - 1; i++, dim++)
	    *dim = *(dim + 1);
	  *dim = 0;

	  base_type = decode_type (cs, new_c_type, aux);
	  index_type = lookup_fundamental_type (current_objfile, FT_INTEGER);
	  range_type =
	    create_range_type ((struct type *) NULL, index_type, 0, n - 1);
	  type =
	    create_array_type ((struct type *) NULL, base_type, range_type);
	}
      return type;
    }

  /* Reference to existing type.  This only occurs with the
     struct, union, and enum types.  EPI a29k coff
     fakes us out by producing aux entries with a nonzero
     x_tagndx for definitions of structs, unions, and enums, so we
     have to check the c_sclass field.  SCO 3.2v4 cc gets confused
     with pointers to pointers to defined structs, and generates
     negative x_tagndx fields.  */
  if (cs->c_naux > 0 && aux->x_sym.x_tagndx.l != 0)
    {
      if (cs->c_sclass != C_STRTAG
	  && cs->c_sclass != C_UNTAG
	  && cs->c_sclass != C_ENTAG
	  && aux->x_sym.x_tagndx.l >= 0)
	{
	  type = coff_alloc_type (aux->x_sym.x_tagndx.l);
	  return type;
	}
      else
	{
	  complain (&tagndx_bad_complaint, cs->c_name);
	  /* And fall through to decode_base_type... */
	}
    }

  return decode_base_type (cs, BTYPE (c_type), aux);
}

/* Decode a coff type specifier for function definition;
   return the type that the function returns.  */

static struct type *
decode_function_type (register struct coff_symbol *cs, unsigned int c_type,
		      register union internal_auxent *aux)
{
  /* if (aux->x_sym.x_tagndx.l == 0) */
  cs->c_naux = 0;	/* auxent refers to function, not base type;
     don't get confused by the line numbers, etc. */

  return decode_type (cs, DECREF (c_type), aux);
}

/* basic C types */

static struct type *
decode_base_type (register struct coff_symbol *cs, unsigned int c_type,
		  register union internal_auxent *aux)
{
  struct type *type;

  switch (c_type)
    {
    case T_NULL:
      /* shows up with "void (*foo)();" structure members */
      return lookup_fundamental_type (current_objfile, FT_VOID);

#if 0
/* DGUX actually defines both T_ARG and T_VOID to the same value.  */
#ifdef T_ARG
    case T_ARG:
      /* Shows up in DGUX, I think.  Not sure where.  */
      return lookup_fundamental_type (current_objfile, FT_VOID);	/* shouldn't show up here */
#endif
#endif /* 0 */

#ifdef T_VOID
    case T_VOID:
      /* Intel 960 COFF has this symbol and meaning.  */
      return lookup_fundamental_type (current_objfile, FT_VOID);
#endif

    case T_CHAR:
      return lookup_fundamental_type (current_objfile, FT_CHAR);

    case T_SHORT:
      return lookup_fundamental_type (current_objfile, FT_SHORT);

    case T_INT:
      return lookup_fundamental_type (current_objfile, FT_INTEGER);

    case T_LONG:
      if (cs->c_sclass == C_FIELD
	  && aux->x_sym.x_misc.x_lnsz.x_size > TARGET_LONG_BIT)
	return lookup_fundamental_type (current_objfile, FT_LONG_LONG);
      else
	return lookup_fundamental_type (current_objfile, FT_LONG);

    case T_FLOAT:
      return lookup_fundamental_type (current_objfile, FT_FLOAT);

    case T_DOUBLE:
      return lookup_fundamental_type (current_objfile, FT_DBL_PREC_FLOAT);

    case T_LNGDBL:
      return lookup_fundamental_type (current_objfile, FT_EXT_PREC_FLOAT);

    case T_STRUCT:
      if (cs->c_naux != 1)
	{
	  /* anonymous structure type */
	  type = coff_alloc_type (cs->c_symnum);
	  TYPE_CODE (type) = TYPE_CODE_STRUCT;
	  TYPE_NAME (type) = NULL;
	  /* This used to set the tag to "<opaque>".  But I think setting it
	     to NULL is right, and the printing code can print it as
	     "struct {...}".  */
	  TYPE_TAG_NAME (type) = NULL;
	  INIT_CPLUS_SPECIFIC (type);
	  TYPE_LENGTH (type) = 0;
	  TYPE_FIELDS (type) = 0;
	  TYPE_NFIELDS (type) = 0;
	}
      else
	{
	  type = coff_read_struct_type (cs->c_symnum,
					aux->x_sym.x_misc.x_lnsz.x_size,
				      aux->x_sym.x_fcnary.x_fcn.x_endndx.l);
	}
      return type;

    case T_UNION:
      if (cs->c_naux != 1)
	{
	  /* anonymous union type */
	  type = coff_alloc_type (cs->c_symnum);
	  TYPE_NAME (type) = NULL;
	  /* This used to set the tag to "<opaque>".  But I think setting it
	     to NULL is right, and the printing code can print it as
	     "union {...}".  */
	  TYPE_TAG_NAME (type) = NULL;
	  INIT_CPLUS_SPECIFIC (type);
	  TYPE_LENGTH (type) = 0;
	  TYPE_FIELDS (type) = 0;
	  TYPE_NFIELDS (type) = 0;
	}
      else
	{
	  type = coff_read_struct_type (cs->c_symnum,
					aux->x_sym.x_misc.x_lnsz.x_size,
				      aux->x_sym.x_fcnary.x_fcn.x_endndx.l);
	}
      TYPE_CODE (type) = TYPE_CODE_UNION;
      return type;

    case T_ENUM:
      if (cs->c_naux != 1)
	{
	  /* anonymous enum type */
	  type = coff_alloc_type (cs->c_symnum);
	  TYPE_CODE (type) = TYPE_CODE_ENUM;
	  TYPE_NAME (type) = NULL;
	  /* This used to set the tag to "<opaque>".  But I think setting it
	     to NULL is right, and the printing code can print it as
	     "enum {...}".  */
	  TYPE_TAG_NAME (type) = NULL;
	  TYPE_LENGTH (type) = 0;
	  TYPE_FIELDS (type) = 0;
	  TYPE_NFIELDS (type) = 0;
	}
      else
	{
	  type = coff_read_enum_type (cs->c_symnum,
				      aux->x_sym.x_misc.x_lnsz.x_size,
				      aux->x_sym.x_fcnary.x_fcn.x_endndx.l);
	}
      return type;

    case T_MOE:
      /* shouldn't show up here */
      break;

    case T_UCHAR:
      return lookup_fundamental_type (current_objfile, FT_UNSIGNED_CHAR);

    case T_USHORT:
      return lookup_fundamental_type (current_objfile, FT_UNSIGNED_SHORT);

    case T_UINT:
      return lookup_fundamental_type (current_objfile, FT_UNSIGNED_INTEGER);

    case T_ULONG:
      if (cs->c_sclass == C_FIELD
	  && aux->x_sym.x_misc.x_lnsz.x_size > TARGET_LONG_BIT)
	return lookup_fundamental_type (current_objfile, FT_UNSIGNED_LONG_LONG);
      else
	return lookup_fundamental_type (current_objfile, FT_UNSIGNED_LONG);
    }
  complain (&unexpected_type_complaint, cs->c_name);
  return lookup_fundamental_type (current_objfile, FT_VOID);
}

/* This page contains subroutines of read_type.  */

/* Read the description of a structure (or union type) and return an
   object describing the type.  */

static struct type *
coff_read_struct_type (int index, int length, int lastsym)
{
  struct nextfield
    {
      struct nextfield *next;
      struct field field;
    };

  register struct type *type;
  register struct nextfield *list = 0;
  struct nextfield *new;
  int nfields = 0;
  register int n;
  char *name;
  struct coff_symbol member_sym;
  register struct coff_symbol *ms = &member_sym;
  struct internal_syment sub_sym;
  union internal_auxent sub_aux;
  int done = 0;

  type = coff_alloc_type (index);
  TYPE_CODE (type) = TYPE_CODE_STRUCT;
  INIT_CPLUS_SPECIFIC (type);
  TYPE_LENGTH (type) = length;

  while (!done && symnum < lastsym && symnum < nlist_nsyms_global)
    {
      read_one_sym (ms, &sub_sym, &sub_aux);
      name = ms->c_name;
      name = EXTERNAL_NAME (name, current_objfile->obfd);

      switch (ms->c_sclass)
	{
	case C_MOS:
	case C_MOU:

	  /* Get space to record the next field's data.  */
	  new = (struct nextfield *) alloca (sizeof (struct nextfield));
	  new->next = list;
	  list = new;

	  /* Save the data.  */
	  list->field.name =
	    obsavestring (name,
			  strlen (name),
			  &current_objfile->symbol_obstack);
	  FIELD_TYPE (list->field) = decode_type (ms, ms->c_type, &sub_aux);
	  FIELD_BITPOS (list->field) = 8 * ms->c_value;
	  FIELD_BITSIZE (list->field) = 0;
	  FIELD_STATIC_KIND (list->field) = 0;
	  nfields++;
	  break;

	case C_FIELD:

	  /* Get space to record the next field's data.  */
	  new = (struct nextfield *) alloca (sizeof (struct nextfield));
	  new->next = list;
	  list = new;

	  /* Save the data.  */
	  list->field.name =
	    obsavestring (name,
			  strlen (name),
			  &current_objfile->symbol_obstack);
	  FIELD_TYPE (list->field) = decode_type (ms, ms->c_type, &sub_aux);
	  FIELD_BITPOS (list->field) = ms->c_value;
	  FIELD_BITSIZE (list->field) = sub_aux.x_sym.x_misc.x_lnsz.x_size;
	  FIELD_STATIC_KIND (list->field) = 0;
	  nfields++;
	  break;

	case C_EOS:
	  done = 1;
	  break;
	}
    }
  /* Now create the vector of fields, and record how big it is.  */

  TYPE_NFIELDS (type) = nfields;
  TYPE_FIELDS (type) = (struct field *)
    TYPE_ALLOC (type, sizeof (struct field) * nfields);

  /* Copy the saved-up fields into the field vector.  */

  for (n = nfields; list; list = list->next)
    TYPE_FIELD (type, --n) = list->field;

  return type;
}

/* Read a definition of an enumeration type,
   and create and return a suitable type object.
   Also defines the symbols that represent the values of the type.  */

/* ARGSUSED */
static struct type *
coff_read_enum_type (int index, int length, int lastsym)
{
  register struct symbol *sym;
  register struct type *type;
  int nsyms = 0;
  int done = 0;
  struct pending **symlist;
  struct coff_symbol member_sym;
  register struct coff_symbol *ms = &member_sym;
  struct internal_syment sub_sym;
  union internal_auxent sub_aux;
  struct pending *osyms, *syms;
  int o_nsyms;
  register int n;
  char *name;
  int unsigned_enum = 1;

  type = coff_alloc_type (index);
  if (within_function)
    symlist = &local_symbols;
  else
    symlist = &file_symbols;
  osyms = *symlist;
  o_nsyms = osyms ? osyms->nsyms : 0;

  while (!done && symnum < lastsym && symnum < nlist_nsyms_global)
    {
      read_one_sym (ms, &sub_sym, &sub_aux);
      name = ms->c_name;
      name = EXTERNAL_NAME (name, current_objfile->obfd);

      switch (ms->c_sclass)
	{
	case C_MOE:
	  sym = (struct symbol *) obstack_alloc
	    (&current_objfile->symbol_obstack,
	     sizeof (struct symbol));
	  memset (sym, 0, sizeof (struct symbol));

	  SYMBOL_NAME (sym) =
	    obsavestring (name, strlen (name),
			  &current_objfile->symbol_obstack);
	  SYMBOL_CLASS (sym) = LOC_CONST;
	  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
	  SYMBOL_VALUE (sym) = ms->c_value;
	  add_symbol_to_list (sym, symlist);
	  nsyms++;
	  break;

	case C_EOS:
	  /* Sometimes the linker (on 386/ix 2.0.2 at least) screws
	     up the count of how many symbols to read.  So stop
	     on .eos.  */
	  done = 1;
	  break;
	}
    }

  /* Now fill in the fields of the type-structure.  */

  if (length > 0)
    TYPE_LENGTH (type) = length;
  else
    TYPE_LENGTH (type) = TARGET_INT_BIT / TARGET_CHAR_BIT;	/* Assume ints */
  TYPE_CODE (type) = TYPE_CODE_ENUM;
  TYPE_NFIELDS (type) = nsyms;
  TYPE_FIELDS (type) = (struct field *)
    TYPE_ALLOC (type, sizeof (struct field) * nsyms);

  /* Find the symbols for the values and put them into the type.
     The symbols can be found in the symlist that we put them on
     to cause them to be defined.  osyms contains the old value
     of that symlist; everything up to there was defined by us.  */
  /* Note that we preserve the order of the enum constants, so
     that in something like "enum {FOO, LAST_THING=FOO}" we print
     FOO, not LAST_THING.  */

  for (syms = *symlist, n = 0; syms; syms = syms->next)
    {
      int j = 0;

      if (syms == osyms)
	j = o_nsyms;
      for (; j < syms->nsyms; j++, n++)
	{
	  struct symbol *xsym = syms->symbol[j];
	  SYMBOL_TYPE (xsym) = type;
	  TYPE_FIELD_NAME (type, n) = SYMBOL_NAME (xsym);
	  TYPE_FIELD_BITPOS (type, n) = SYMBOL_VALUE (xsym);
	  if (SYMBOL_VALUE (xsym) < 0)
	    unsigned_enum = 0;
	  TYPE_FIELD_BITSIZE (type, n) = 0;
	  TYPE_FIELD_STATIC_KIND (type, n) = 0;
	}
      if (syms == osyms)
	break;
    }

  if (unsigned_enum)
    TYPE_FLAGS (type) |= TYPE_FLAG_UNSIGNED;

  return type;
}

/* Parse the user's idea of an offset for dynamic linking, into our idea
   of how to represent it for fast symbol reading.   This is essentially
   the same as the default version of the function, but it adds in ImageBase.*/

void
coff_symfile_offsets (objfile, addrs)
     struct objfile *objfile;
     struct section_addr_info *addrs;
{
  asection *sect = NULL;
  struct section_offsets *section_offsets;
  struct obj_section *s;
  int i;

#ifdef COFF_IMAGE_WITH_PE
  CORE_ADDR image_base;

  /* We must make sure that symbol lookup uses the same idea of offsets
     that everyone else does. */
  image_base = NONZERO_LINK_BASE(objfile->obfd);
  for (s = objfile->sections; s < objfile->sections_end; ++s)
     {
       s->addr += image_base;
       s->endaddr += image_base;
     }
#endif

  objfile->num_sections = SECT_OFF_MAX;
  objfile->section_offsets = (struct section_offsets *)
    obstack_alloc (&objfile -> psymbol_obstack, SIZEOF_SECTION_OFFSETS);
  memset (objfile->section_offsets, 0, SIZEOF_SECTION_OFFSETS);

  /* Initialize the section indexes for future use. */
  sect = bfd_get_section_by_name (objfile->obfd, ".text");
  if (sect) 
    objfile->sect_index_text = sect->index;

  sect = bfd_get_section_by_name (objfile->obfd, ".data");
  if (sect) 
    objfile->sect_index_data = sect->index;

  sect = bfd_get_section_by_name (objfile->obfd, ".bss");
  if (sect) 
    objfile->sect_index_bss = sect->index;

  sect = bfd_get_section_by_name (objfile->obfd, ".rodata");
  if (sect) 
    objfile->sect_index_rodata = sect->index;
  else
    /* Doesn't always have to exist, so fake out the sanity test. */
    objfile->sect_index_rodata = -2;
    

  /* Now calculate offsets for other sections. */
  for (i = 0; i < MAX_SECTIONS && addrs->other[i].name; i++)
    {
      struct other_sections *osp ;

      osp = &addrs->other[i] ;
      if (addrs->other[i].addr == 0)
  	continue;
      /* The section_offsets in the objfile are here filled in using
         the BFD index. */
      (objfile->section_offsets)->offsets[osp->sectindex] = osp->addr;
    }
}
/* Register our ability to parse symbols for coff BFD files. */

static struct sym_fns coff_sym_fns =
{
  bfd_target_coff_flavour,
  coff_new_init,		/* sym_new_init: init anything gbl to entire symtab */
  coff_symfile_init,		/* sym_init: read initial info, setup for sym_read() */
  coff_symfile_read,		/* sym_read: read a symbol file into symtab */
  coff_symfile_finish,		/* sym_finish: finished with file, cleanup */
  coff_symfile_offsets,		/* sym_offsets:  xlate external to internal form */
  NULL				/* next: pointer to next struct sym_fns */
};

void
_initialize_coffread (void)
{
  add_symtab_fns (&coff_sym_fns);
}
