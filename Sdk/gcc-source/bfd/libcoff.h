/* DO NOT EDIT!  -*- buffer-read-only: t -*-  This file is automatically 
   generated from "libcoff-in.h" and "coffcode.h".
   Run "make headers" in your build bfd/ to regenerate.  */

/* BFD COFF object file private structure.
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

#include "bfdlink.h"

#define DYNAMIC_LINKING /* Always defined, but useful as documentation. */
#define USE_DLLS        /* Ditto */

/* Object file tdata; access macros */

#define coff_data(bfd)		((bfd)->tdata.coff_obj_data)
#define exec_hdr(bfd)		(coff_data(bfd)->hdr)
#define obj_pe(bfd)             (coff_data(bfd)->pe)
#define obj_symbols(bfd)	(coff_data(bfd)->symbols)
#define	obj_sym_filepos(bfd)	(coff_data(bfd)->sym_filepos)

#define obj_relocbase(bfd)	(coff_data(bfd)->relocbase)
#define obj_raw_syments(bfd)	(coff_data(bfd)->raw_syments)
#define obj_raw_syment_count(bfd)	(coff_data(bfd)->raw_syment_count)
#define obj_convert(bfd)	(coff_data(bfd)->conversion_table)
#define obj_conv_table_size(bfd) (coff_data(bfd)->conv_table_size)

#define obj_coff_external_syms(bfd) (coff_data (bfd)->external_syms)
#define obj_coff_keep_syms(bfd)	(coff_data (bfd)->keep_syms)
#define obj_coff_strings(bfd)	(coff_data (bfd)->strings)
#define obj_coff_keep_strings(bfd) (coff_data (bfd)->keep_strings)
#define obj_coff_sym_hashes(bfd) (coff_data (bfd)->sym_hashes)
#define obj_coff_strings_written(bfd) (coff_data (bfd)->strings_written)

#define obj_coff_local_toc_table(bfd) (coff_data(bfd)->local_toc_sym_map)

/* `Tdata' information kept for COFF files.  */

typedef struct coff_tdata
{
  struct   coff_symbol_struct *symbols;	/* symtab for input bfd */
  unsigned int *conversion_table;
  int conv_table_size;
  file_ptr sym_filepos;

  struct coff_ptr_struct *raw_syments;
  unsigned long raw_syment_count;

  /* These are only valid once writing has begun */
  long int relocbase;

  /* These members communicate important constants about the symbol table
     to GDB's symbol-reading code.  These `constants' unfortunately vary
     from coff implementation to implementation...  */
  unsigned local_n_btmask;
  unsigned local_n_btshft;
  unsigned local_n_tmask;
  unsigned local_n_tshift;
  unsigned local_symesz;
  unsigned local_auxesz;
  unsigned local_linesz;

  /* The unswapped external symbols.  May be NULL.  Read by
     _bfd_coff_get_external_symbols.  */
  PTR external_syms;
  /* If this is true, the external_syms may not be freed.  */
  boolean keep_syms;

  /* The string table.  May be NULL.  Read by
     _bfd_coff_read_string_table.  */
  char *strings;
  /* If this is true, the strings may not be freed.  */
  boolean keep_strings;
  /* If this is true, the strings have been written out already.  */
  boolean strings_written;

  /* is this a PE format coff file */
  int pe;
  /* Used by the COFF backend linker.  */
  struct coff_link_hash_entry **sym_hashes;

  /* used by the pe linker for PowerPC */
  int *local_toc_sym_map;

  struct bfd_link_info *link_info;

  /* Used by coff_find_nearest_line.  */
  PTR line_info;

  /* A place to stash dwarf2 info for this bfd. */
  PTR dwarf2_find_line_info;

  /* The timestamp from the COFF file header.  */
  long timestamp;

  /* list of sections (for quick access) */
  asection **coff_sec;

  /* Copy of some of the f_flags bits in the COFF filehdr structure,
     used by ARM code.  */
  flagword flags;

} coff_data_type;

#define coff_coffsections(bfd)		((bfd)->tdata.coff_obj_data->coff_sec)

/* Tdata for pe image files. */
typedef struct pe_tdata
{
  coff_data_type coff;
  struct internal_extra_pe_aouthdr pe_opthdr;
  int dll;
  int has_reloc_section;
  boolean (*in_reloc_p) PARAMS((bfd *, reloc_howto_type *));
  flagword real_flags;
  int target_subsystem;
  boolean force_minimum_alignment;
} pe_data_type;

#define pe_data(bfd)		((bfd)->tdata.pe_obj_data)

/* Tdata for XCOFF files.  */

struct xcoff_tdata
{
  /* Basic COFF information.  */
  coff_data_type coff;

  /* True if this is an XCOFF64 file. */
  boolean xcoff64;

  /* True if a large a.out header should be generated.  */
  boolean full_aouthdr;

  /* TOC value.  */
  bfd_vma toc;

  /* Index of section holding TOC.  */
  int sntoc;

  /* Index of section holding entry point.  */
  int snentry;

  /* .text alignment from optional header.  */
  int text_align_power;

  /* .data alignment from optional header.  */
  int data_align_power;

  /* modtype from optional header.  */
  short modtype;

  /* cputype from optional header.  */
  short cputype;

  /* maxdata from optional header.  */
  bfd_vma maxdata;

  /* maxstack from optional header.  */
  bfd_vma maxstack;

  /* Used by the XCOFF backend linker.  */
  asection **csects;
  unsigned long *debug_indices;
  unsigned int import_file_id;
};

#define xcoff_data(abfd) ((abfd)->tdata.xcoff_obj_data)

/* We take the address of the first element of an asymbol to ensure that the
 * macro is only ever applied to an asymbol.  */
#define coffsymbol(asymbol) ((coff_symbol_type *)(&((asymbol)->the_bfd)))

/* The used_by_bfd field of a section may be set to a pointer to this
   structure.  */

struct coff_section_tdata
{
  /* The relocs, swapped into COFF internal form.  This may be NULL.  */
  struct internal_reloc *relocs;
  /* If this is true, the relocs entry may not be freed.  */
  boolean keep_relocs;
  /* The section contents.  This may be NULL.  */
  bfd_byte *contents;
  /* If this is true, the contents entry may not be freed.  */
  boolean keep_contents;
  /* Information cached by coff_find_nearest_line.  */
  bfd_vma offset;
  unsigned int i;
  const char *function;
  int line_base;
  /* A pointer used for .stab linking optimizations.  */
  PTR stab_info;
  /* Available for individual backends.  */
  PTR tdata;
};

/* An accessor macro for the coff_section_tdata structure.  */
#define coff_section_data(abfd, sec) \
  ((struct coff_section_tdata *) (sec)->used_by_bfd)

/* Tdata for sections in XCOFF files.  This is used by the linker.  */

struct xcoff_section_tdata
{
  /* Used for XCOFF csects created by the linker; points to the real
     XCOFF section which contains this csect.  */
  asection *enclosing;
  /* The lineno_count field for the enclosing section, because we are
     going to clobber it there.  */
  unsigned int lineno_count;
  /* The first and one past the last symbol indices for symbols used
     by this csect.  */
  unsigned long first_symndx;
  unsigned long last_symndx;
};

/* An accessor macro the xcoff_section_tdata structure.  */
#define xcoff_section_data(abfd, sec) \
  ((struct xcoff_section_tdata *) coff_section_data ((abfd), (sec))->tdata)

/* Tdata for sections in PE files.  */

struct pei_section_tdata
{
  /* The virtual size of the section.  */
  bfd_size_type virt_size;
  /* The PE section flags.  */
  long pe_flags;
};

/* An accessor macro for the pei_section_tdata structure.  */
#define pei_section_data(abfd, sec) \
  ((struct pei_section_tdata *) coff_section_data ((abfd), (sec))->tdata)

#ifdef DYNAMIC_LINKING /* [ */
/* Dynamic section tags */

#define DT_NULL		0
#define DT_NEEDED	1
#define DT_PLTRELSZ	2
#define DT_PLTGOT	3
#define DT_HASH		4
#define DT_STRTAB	5
#define DT_SYMTAB	6
#define DT_RELA		7
#define DT_RELASZ	8
#define DT_RELAENT	9
#define DT_STRSZ	10
#define DT_SYMENT	11
#define DT_INIT		12
#define DT_FINI		13
#define DT_SONAME	14
#define DT_RPATH	15
#define DT_SYMBOLIC	16
#define DT_REL		17
#define DT_RELSZ	18
#define DT_RELENT	19
#define DT_PLTREL	20
#define DT_DEBUG	21
#define DT_TEXTREL	22
#define DT_JMPREL	23

/* The next four dynamic tags are used on Solaris.  We support them
   everywhere.  */
#define DT_VERDEF	0x6ffffffc
#define DT_VERDEFNUM	0x6ffffffd
#define DT_VERNEED	0x6ffffffe
#define DT_VERNEEDNUM	0x6fffffff

/* These section tags are used on Solaris.  We support them
   everywhere, and hope they do not conflict.  */

#define DT_AUXILIARY	0x7ffffffd
#define DT_FILTER	0x7fffffff

/* This tag is a GNU extension to the Solaris version scheme.  */
#define DT_VERSYM	0x6ffffff0


/* External structure definitions */
typedef struct {
  unsigned char	d_tag[4];		/* entry tag value */
  union {
    unsigned char	d_val[4];
    unsigned char	d_ptr[4];
  } d_un;
} coff_external_dyn;

/* This structure appears in a .gnu.version.d section.  */

typedef struct {
  unsigned char		vd_version[2];
  unsigned char		vd_flags[2];
  unsigned char		vd_ndx[2];
  unsigned char		vd_cnt[2];
  unsigned char		vd_hash[4];
  unsigned char		vd_aux[4];
  unsigned char		vd_next[4];
} coff_external_verdef;

/* This structure appears in a .gnu.version.d section.  */

typedef struct {
  unsigned char		vda_name[4];
  unsigned char		vda_next[4];
} coff_external_verdaux;

/* This structure appears in a .gnu.version.r section.  */

typedef struct {
  unsigned char		vn_version[2];
  unsigned char		vn_cnt[2];
  unsigned char		vn_file[4];
  unsigned char		vn_aux[4];
  unsigned char		vn_next[4];
} coff_external_verneed;

/* This structure appears in a .gnu.version.r section.  */

typedef struct {
  unsigned char		vna_hash[4];
  unsigned char		vna_flags[2];
  unsigned char		vna_other[2];
  unsigned char		vna_name[4];
  unsigned char		vna_next[4];
} coff_external_vernaux;

/* This structure appears in a .gnu.version.r section. */

typedef struct {
  unsigned char		vs_vers[2];
} coff_external_versym;

/* Internal structure definitions */
typedef struct coff_internal_dyn {
  bfd_vma d_tag;		/* entry tag value */
  union {
    bfd_vma	d_val;
    bfd_vma	d_ptr;
  } d_un;
} coff_internal_dyn;

/* This structure appears in a .gnu.version.d section.  */

typedef struct coff_internal_verdef {
  unsigned short vd_version;	/* Version number of structure.  */
  unsigned short vd_flags;	/* Flags (VER_FLG_*).  */
  unsigned short vd_ndx;	/* Version index.  */
  unsigned short vd_cnt;	/* Number of verdaux entries.  */
  unsigned long	 vd_hash;	/* Hash of name.  */
  unsigned long	 vd_aux;	/* Offset to verdaux entries.  */
  unsigned long	 vd_next;	/* Offset to next verdef.  */

  /* These fields are set up when BFD reads in the structure.  FIXME:
     It would be cleaner to store these in a different structure.  */
  bfd			       *vd_bfd;		/* BFD.  */
  const char 		       *vd_nodename;	/* Version name.  */
  struct coff_internal_verdef  *vd_nextdef;	/* vd_next as pointer.  */
  struct coff_internal_verdaux *vd_auxptr;	/* vd_aux as pointer.  */
  unsigned int		       vd_exp_refno;	/* Used by the linker.  */
} coff_internal_verdef;

/* This structure appears in a .gnu.version.d section.  */

typedef struct coff_internal_verdaux {
  unsigned long vda_name;	/* String table offset of name.  */
  unsigned long vda_next;	/* Offset to next verdaux.  */

  /* These fields are set up when BFD reads in the structure.  FIXME:
     It would be cleaner to store these in a different structure.  */
  const char *vda_nodename;			/* vda_name as pointer.  */
  struct coff_internal_verdaux *vda_nextptr;	/* vda_next as pointer.  */
} coff_internal_verdaux;
 
/* This structure appears in a .gnu.version.r section.  */

typedef struct coff_internal_verneed {
  unsigned short vn_version;	/* Version number of structure.  */
  unsigned short vn_cnt;	/* Number of vernaux entries.  */
  unsigned long	 vn_file;	/* String table offset of library name.  */
  unsigned long	 vn_aux;	/* Offset to vernaux entries.  */
  unsigned long	 vn_next;	/* Offset to next verneed.  */

  /* These fields are set up when BFD reads in the structure.  FIXME:
     It would be cleaner to store these in a different structure.  */
  bfd			       *vn_bfd;		/* BFD.  */
  const char                   *vn_filename;	/* vn_file as pointer.  */
  struct coff_internal_vernaux *vn_auxptr;	/* vn_aux as pointer.  */
  struct coff_internal_verneed *vn_nextref;	/* vn_nextref as pointer.  */
} coff_internal_verneed;

/* This structure appears in a .gnu.version.r section.  */

typedef struct coff_internal_vernaux {
  unsigned long	 vna_hash;	/* Hash of dependency name.  */
  unsigned short vna_flags;	/* Flags (VER_FLG_*).  */
  unsigned short vna_other;	/* Unused.  */
  unsigned long	 vna_name;	/* String table offset to version name.  */
  unsigned long	 vna_next;	/* Offset to next vernaux.  */

  /* These fields are set up when BFD reads in the structure.  FIXME:
     It would be cleaner to store these in a different structure.  */
  const char                  *vna_nodename;	/* vna_name as pointer.  */
  struct coff_internal_vernaux *vna_nextptr;	/* vna_next as pointer.  */
} coff_internal_vernaux;

/* This structure appears in a .gnu.version.r section. */

typedef struct coff_internal_versym {
  unsigned short vs_vers;
} coff_internal_versym;

#define LOADER_NAME "/usr/lib/ld.so"

#endif /* DYNAMIC_LINKING */ /* ] */

/* COFF linker hash table entries.  */

struct coff_link_hash_entry
{
  struct bfd_link_hash_entry root;

  /* Symbol index in output file.  Set to -1 initially.  Set to -2 if
     there is a reloc against this symbol.  */
  long indx;

  /* Symbol type.  */
  unsigned short type;

  /* Symbol class.  */
  unsigned char class;

  /* Number of auxiliary entries.  */
  char numaux;

  /* BFD to take auxiliary entries from.  */
  bfd *auxbfd;

  /* Pointer to array of auxiliary entries, if any.  */
  union internal_auxent *aux;

  /* Flag word; legal values follow.  */
  unsigned short coff_link_hash_flags;
  /* Symbol is a PE section symbol.  */
#define COFF_LINK_HASH_PE_SECTION_SYMBOL 0x8000
#ifdef DYNAMIC_LINKING /* [ */
  /* More flag word values: */
  /* Symbol is referenced by a non-shared object.  */
#define COFF_LINK_HASH_REF_REGULAR	0x1
  /* Symbol is defined by a non-shared object.  */
#define COFF_LINK_HASH_DEF_REGULAR	0x2
  /* Symbol is referenced by a shared object.  */
#define COFF_LINK_HASH_REF_DYNAMIC	0x4
  /* Symbol is defined by a shared object.  */
#define COFF_LINK_HASH_DEF_DYNAMIC	0x8
  /* Dynamic symbol has been adjustd.  */
#define COFF_LINK_HASH_DYNAMIC_ADJUSTED 0x10
  /* Symbol needs a copy reloc.  */
#define COFF_LINK_HASH_NEEDS_COPY	0x20
  /* Symbol needs a procedure linkage table entry.  */
#define COFF_LINK_HASH_NEEDS_PLT	0x40
  /* Symbol appears in a non-COFF input file.  */
#define COFF_LINK_NON_COFF		0x80
  /* Symbol should be marked as hidden in the version information.  */
#define COFF_LINK_HIDDEN		0x100
  /* Symbol was forced to local scope due to a version script file.  */
#define COFF_LINK_FORCED_LOCAL		0x200
  /* We'd like to force this local (via --retain-symbols-file), but don't 
     know if we can, yet. */
#define COFF_LINK_MAYBE_FORCED_LOCAL	0x400
  /* Symbol (subsequently, mostly) found in a DLL library. */
#define COFF_LINK_HASH_DLL_DEFINED	0x800
  /* Dynsym has been renumbered.  Set exactly once on all renumbered ents. */
#define COFF_LINK_HASH_RENUMBERED	0x1000
  /* Dynsym has been emitted.  Set exactly once on all ents. */
#define COFF_LINK_HASH_EMITTED          0x2000
  /* Plt is a dup of another, due to weak/indirect; don't emit reloc. */
#define COFF_LINK_WEAK_PLT              0x4000

#ifdef USE_SIZE
  /* Symbol size.  */
  bfd_size_type size;
#endif

  /* Symbol index as a dynamic symbol.  Initialized to -1, and remains
     -1 if this is not a dynamic symbol.  */
  long dynindx;

  /* String table index in .dynstr if this is a dynamic symbol.  */
  unsigned long dynstr_index;

#ifdef USE_WEAK
  /* If this is a weak defined symbol from a dynamic object, this
     field points to a defined symbol with the same value, if there is
     one.  Otherwise it is NULL.  */
  struct coff_link_hash_entry *weakdef;
#endif

  /* If this symbol requires an entry in the global offset table, the
     processor specific backend uses this field to hold the offset
     into the .got section.  If this field is -1, then the symbol does
     not require a global offset table entry.  */
  bfd_vma got_offset;

  /* If this symbol requires an entry in the procedure linkage table,
     the processor specific backend uses these two fields to hold the
     offset into the procedure linkage section and the offset into the
     .got section.  If plt_offset is -1, then the symbol does not
     require an entry in the procedure linkage table.  */
  bfd_vma plt_offset;

  /* The number of DIR32 (etc.) relocation entries for this symbol;
     used to allocate space in .rel.dyn section.  When linking
     without COPY relocs, we otherwise would badly overestimate,
     and it helps a little even for shared.  */
  int num_long_relocs_needed;

  /* Similarly; under some circumstances we can't know whether relative
     relocs will be needed when counting, so we count them separately,
     and apply once we do know. */
  int num_relative_relocs_needed;

  /* Version information.  */
  union
  {
    /* This field is used for a symbol which is not defined in a
       regular object.  It points to the version information read in
       from the dynamic object.  */
    coff_internal_verdef *verdef;

    /* This field is used for a symbol which is defined in a regular
       object.  It is set up in size_dynamic_sections.  It points to
       the version information we should write out for this symbol.

       Since it's already "global" (including in ld) as "elf_version_tree",
       we'll leave it as "elf", even though it now applies to PE/COFF, too.
       (Definition in bfdlink.h) */
    struct bfd_elf_version_tree *vertree;

  } verinfo;
#endif /* ] */
};

/* COFF linker hash table.  */

struct coff_link_hash_table
{
  struct bfd_link_hash_table root;
  /* A pointer to information used to link stabs in sections.  */
  PTR stab_info;

#ifdef DYNAMIC_LINKING /* [ */
  /* Whether we have created the special dynamic sections required
     when linking against or generating a shared object.  */
  boolean dynamic_sections_created;

  /* The BFD used to hold special sections created by the linker.
     This will be the first BFD found which requires these sections to
     be created.  */
  bfd *dynobj;

  /* The number of symbols found in the link which must be put into
     the .dynsym section.  */
  bfd_size_type dynsymcount;

  /* The string table of dynamic symbols, which becomes the .dynstr
     section.  */
  struct bfd_strtab_hash *dynstr;

  /* The number of buckets in the hash table in the .hash section.
     This is based on the number of dynamic symbols.  */
  bfd_size_type bucketcount;

  /* A linked list of DT_NEEDED names found in dynamic objects
     included in the link.  */
  struct bfd_link_needed_list *needed;

  /* The _GLOBAL_OFFSET_TABLE_ symbol.  */
  struct coff_link_hash_entry *hgot;

  /* Certain dynamic sections which are frequently accessed, to skip
     by-name lookup. (Apply to special cases hosted dynobj.) */
  asection *sreloc;		/* .rel.internal */
  asection *dynamic;		/* .dynamic */
  asection *sgot;		/* .got */
  asection *srelgot;		/* .rel.got */
  asection *splt;		/* .plt */
  asection *srelplt;		/* .rel.plt */
  asection *sgotplt;		/* .rel.plt */

#endif /* ] */
};

/* Look up an entry in a COFF linker hash table.  */

#define coff_link_hash_lookup(table, string, create, copy, follow)	\
  ((struct coff_link_hash_entry *)					\
   bfd_link_hash_lookup (&(table)->root, (string), (create),		\
			 (copy), (follow)))

/* Traverse a COFF linker hash table.  */

#define coff_link_hash_traverse(table, func, info)			\
  (bfd_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct bfd_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the COFF linker hash table from a link_info structure.  */

#define coff_hash_table(p) ((struct coff_link_hash_table *) ((p)->hash))

/* Functions in coffgen.c.  */
extern const bfd_target *coff_object_p
  PARAMS ((bfd *));
extern struct sec *coff_section_from_bfd_index
  PARAMS ((bfd *, int));
extern long coff_get_symtab_upper_bound
  PARAMS ((bfd *));
extern long coff_get_symtab
  PARAMS ((bfd *, asymbol **));
extern int coff_count_linenumbers
  PARAMS ((bfd *));
extern struct coff_symbol_struct *coff_symbol_from
  PARAMS ((bfd *, asymbol *));
extern boolean coff_renumber_symbols
  PARAMS ((bfd *, int *));
extern void coff_mangle_symbols
  PARAMS ((bfd *));
extern boolean coff_write_symbols
  PARAMS ((bfd *));
extern boolean coff_write_linenumbers
  PARAMS ((bfd *));
extern alent *coff_get_lineno
  PARAMS ((bfd *, asymbol *));
extern asymbol *coff_section_symbol
  PARAMS ((bfd *, char *));
extern boolean _bfd_coff_get_external_symbols
  PARAMS ((bfd *));
extern const char *_bfd_coff_read_string_table
  PARAMS ((bfd *));
extern boolean _bfd_coff_free_symbols
  PARAMS ((bfd *));
extern struct coff_ptr_struct *coff_get_normalized_symtab
  PARAMS ((bfd *));
extern long coff_get_reloc_upper_bound
  PARAMS ((bfd *, sec_ptr));
extern asymbol *coff_make_empty_symbol
  PARAMS ((bfd *));
extern void coff_print_symbol
  PARAMS ((bfd *, PTR filep, asymbol *, bfd_print_symbol_type, asymbol *));
extern void coff_get_symbol_info
  PARAMS ((bfd *, asymbol *, symbol_info *ret));
extern boolean _bfd_coff_is_local_label_name
  PARAMS ((bfd *, const char *));
extern asymbol *coff_bfd_make_debug_symbol
  PARAMS ((bfd *, PTR, unsigned long));
extern boolean coff_find_nearest_line
  PARAMS ((bfd *, asection *, asymbol **, bfd_vma, const char **,
	   const char **, unsigned int *));
extern int coff_sizeof_headers
  PARAMS ((bfd *, boolean));
extern boolean bfd_coff_reloc16_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, boolean *));
extern bfd_byte *bfd_coff_reloc16_get_relocated_section_contents
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *,
	   bfd_byte *, boolean, asymbol **));
extern bfd_vma bfd_coff_reloc16_get_value
   PARAMS ((arelent *, struct bfd_link_info *, asection *));
extern void bfd_perform_slip
  PARAMS ((bfd *, unsigned int, asection *, bfd_vma));

/* Functions and types in cofflink.c.  */

#define STRING_SIZE_SIZE (4)

/* We use a hash table to merge identical enum, struct, and union
   definitions in the linker.  */

/* Information we keep for a single element (an enum value, a
   structure or union field) in the debug merge hash table.  */

struct coff_debug_merge_element
{
  /* Next element.  */
  struct coff_debug_merge_element *next;

  /* Name.  */
  const char *name;

  /* Type.  */
  unsigned int type;

  /* Symbol index for complex type.  */
  long tagndx;
};

/* A linked list of debug merge entries for a given name.  */

struct coff_debug_merge_type
{
  /* Next type with the same name.  */
  struct coff_debug_merge_type *next;

  /* Class of type.  */
  int class;

  /* Symbol index where this type is defined.  */
  long indx;

  /* List of elements.  */
  struct coff_debug_merge_element *elements;
};

/* Information we store in the debug merge hash table.  */

struct coff_debug_merge_hash_entry
{
  struct bfd_hash_entry root;

  /* A list of types with this name.  */
  struct coff_debug_merge_type *types;
};

/* The debug merge hash table.  */

struct coff_debug_merge_hash_table
{
  struct bfd_hash_table root;
};

/* Initialize a COFF debug merge hash table.  */

#define coff_debug_merge_hash_table_init(table) \
  (bfd_hash_table_init (&(table)->root, _bfd_coff_debug_merge_hash_newfunc))

/* Free a COFF debug merge hash table.  */

#define coff_debug_merge_hash_table_free(table) \
  (bfd_hash_table_free (&(table)->root))

/* Look up an entry in a COFF debug merge hash table.  */

#define coff_debug_merge_hash_lookup(table, string, create, copy) \
  ((struct coff_debug_merge_hash_entry *) \
   bfd_hash_lookup (&(table)->root, (string), (create), (copy)))

/* Information we keep for each section in the output file when doing
   a relocateable link.  */

struct coff_link_section_info
{
  /* The relocs to be output.  */
  struct internal_reloc *relocs;
  /* For each reloc against a global symbol whose index was not known
     when the reloc was handled, the global hash table entry.  */
  struct coff_link_hash_entry **rel_hashes;
};

/* Information that we pass around while doing the final link step.  */

struct coff_final_link_info
{
  /* General link information.  */
  struct bfd_link_info *info;
  /* Output BFD.  */
  bfd *output_bfd;
  /* Used to indicate failure in traversal routine.  */
  boolean failed;
  /* If doing "task linking" set only during the time when we want the
     global symbol writer to convert the storage class of defined global
     symbols from global to static. */
  boolean global_to_static;
  /* Hash table for long symbol names.  */
  struct bfd_strtab_hash *strtab;
  /* When doing a relocateable link, an array of information kept for
     each output section, indexed by the target_index field.  */
  struct coff_link_section_info *section_info;
  /* Symbol index of last C_FILE symbol (-1 if none).  */
  long last_file_index;
  /* Contents of last C_FILE symbol.  */
  struct internal_syment last_file;
  /* Symbol index of first aux entry of last .bf symbol with an empty
     endndx field (-1 if none).  */
  long last_bf_index;
  /* Contents of last_bf_index aux entry.  */
  union internal_auxent last_bf;
  /* Hash table used to merge debug information.  */
  struct coff_debug_merge_hash_table debug_merge;
  /* Buffer large enough to hold swapped symbols of any input file.  */
  struct internal_syment *internal_syms;
  /* Buffer large enough to hold sections of symbols of any input file.  */
  asection **sec_ptrs;
  /* Buffer large enough to hold output indices of symbols of any
     input file.  */
  long *sym_indices;
  /* Buffer large enough to hold output symbols for any input file.  */
  bfd_byte *outsyms;
  /* Buffer large enough to hold external line numbers for any input
     section.  */
  bfd_byte *linenos;
  /* Buffer large enough to hold any input section.  */
  bfd_byte *contents;
  /* Buffer large enough to hold external relocs of any input section.  */
  bfd_byte *external_relocs;
  /* Buffer large enough to hold swapped relocs of any input section.  */
  struct internal_reloc *internal_relocs;

#ifdef DYNAMIC_LINKING
  /* .dynsym section.  */
  asection *dynsym_sec;

  /* .hash section.  */
  asection *hash_sec;

  /* symbol version section (.gnu.version).  */
  asection *symver_sec;
#endif
};

/* Most COFF variants have no way to record the alignment of a
   section.  This struct is used to set a specific alignment based on
   the name of the section.  */

struct coff_section_alignment_entry
{
  /* The section name.  */
  const char *name;

  /* This is either (unsigned int) -1, indicating that the section
     name must match exactly, or it is the number of letters which
     must match at the start of the name.  */
  unsigned int comparison_length;

  /* These macros may be used to fill in the first two fields in a
     structure initialization.  */
#define COFF_SECTION_NAME_EXACT_MATCH(name) (name), ((unsigned int) -1)
#define COFF_SECTION_NAME_PARTIAL_MATCH(name) (name), (sizeof (name) - 1)

  /* Only use this entry if the default section alignment for this
     target is at least that much (as a power of two).  If this field
     is COFF_ALIGNMENT_FIELD_EMPTY, it should be ignored.  */
  unsigned int default_alignment_min;

  /* Only use this entry if the default section alignment for this
     target is no greater than this (as a power of two).  If this
     field is COFF_ALIGNMENT_FIELD_EMPTY, it should be ignored.  */
  unsigned int default_alignment_max;

#define COFF_ALIGNMENT_FIELD_EMPTY ((unsigned int) -1)

  /* The desired alignment for this section (as a power of two).  */
  unsigned int alignment_power;
};

extern struct bfd_hash_entry *_bfd_coff_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
extern boolean _bfd_coff_link_hash_table_init
  PARAMS ((struct coff_link_hash_table *, bfd *,
	   struct bfd_hash_entry *(*) (struct bfd_hash_entry *,
				       struct bfd_hash_table *,
				       const char *)));
extern struct bfd_link_hash_table *_bfd_coff_link_hash_table_create
  PARAMS ((bfd *));
extern const char *_bfd_coff_internal_syment_name
  PARAMS ((bfd *, const struct internal_syment *, char *));
extern boolean _bfd_coff_link_add_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_coff_final_link
  PARAMS ((bfd *, struct bfd_link_info *));
extern struct internal_reloc *_bfd_coff_read_internal_relocs
  PARAMS ((bfd *, asection *, boolean, bfd_byte *, boolean,
	   struct internal_reloc *));
extern boolean _bfd_coff_generic_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   struct internal_reloc *, struct internal_syment *, asection **));

extern struct bfd_hash_entry *_bfd_coff_debug_merge_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
extern boolean _bfd_coff_write_global_sym
  PARAMS ((struct coff_link_hash_entry *, PTR));
extern boolean _bfd_coff_write_task_globals
  PARAMS ((struct coff_link_hash_entry *, PTR));
extern boolean _bfd_coff_link_input_bfd
  PARAMS ((struct coff_final_link_info *, bfd *));
extern boolean _bfd_coff_reloc_link_order
  PARAMS ((bfd *, struct coff_final_link_info *, asection *,
	   struct bfd_link_order *));


#define coff_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

/* Functions in xcofflink.c.  */

extern long _bfd_xcoff_get_dynamic_symtab_upper_bound
  PARAMS ((bfd *));
extern long _bfd_xcoff_canonicalize_dynamic_symtab
  PARAMS ((bfd *, asymbol **));
extern long _bfd_xcoff_get_dynamic_reloc_upper_bound
  PARAMS ((bfd *));
extern long _bfd_xcoff_canonicalize_dynamic_reloc
  PARAMS ((bfd *, arelent **, asymbol **));
extern struct bfd_link_hash_table *_bfd_xcoff_bfd_link_hash_table_create
  PARAMS ((bfd *));
extern void _bfd_xcoff_bfd_link_hash_table_free
  PARAMS ((struct bfd_link_hash_table *));
extern boolean _bfd_xcoff_bfd_link_add_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_xcoff_bfd_final_link
  PARAMS ((bfd *, struct bfd_link_info *));
extern boolean _bfd_ppc_xcoff_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   struct internal_reloc *, struct internal_syment *, asection **));

/* Functions in coff-ppc.c.  FIXME: These are called be pe.em in the
   linker, and so should start with bfd and be declared in bfd.h.  */

extern boolean ppc_allocate_toc_section
  PARAMS ((struct bfd_link_info *));
extern boolean ppc_process_before_allocation
  PARAMS ((bfd *, struct bfd_link_info *));

#ifdef DYNAMIC_LINKING /* [ */
/* in coffgen.c */
long coff_get_dynamic_symtab_upper_bound PARAMS((bfd *));
long coff_canonicalize_dynamic_symtab PARAMS((bfd *, asymbol **));
long coff_get_dynamic_reloc_upper_bound PARAMS((bfd *));
struct bfd_strtab_hash * _bfd_coff_stringtab_init PARAMS((void));
char * bfd_coff_get_str_section PARAMS((bfd *, unsigned int));
char * bfd_coff_string_from_coff_section 
    PARAMS((bfd *, unsigned int, unsigned int));
boolean _bfd_coff_slurp_version_tables PARAMS((bfd *abfd));

/* in cofflink.c */
boolean _bfd_coff_link_record_dynamic_symbol 
    PARAMS((struct bfd_link_info *, struct coff_link_hash_entry *h));
boolean coff_add_dynamic_entry 
    PARAMS((struct bfd_link_info *, bfd_vma, bfd_vma));
void bfd_coff_set_dt_needed_name PARAMS((bfd *, const char *));
struct bfd_link_needed_list * bfd_coff_get_needed_list 
    PARAMS((bfd *, struct bfd_link_info *));
const char * bfd_coff_get_dt_soname PARAMS((bfd *));
boolean _bfd_coff_create_got_section 
    PARAMS((bfd *, struct bfd_link_info *, char *, boolean));

#define dyn_data(bfd) ((bfd)->dynamic_info)

#define coff_local_got_offsets(bfd) ((bfd) -> local_got_offsets)
#define coff_dynsymtab(bfd)   (dyn_data(bfd) -> dynsymtab)
#define coff_dynstrtab(bfd)   (dyn_data(bfd) -> dynstrtab)
#define coff_dynverdef(bfd)   (dyn_data(bfd) -> dynverdef)
#define coff_dynverref(bfd)   (dyn_data(bfd) -> dynverref)
#define coff_dynversym(bfd)   (dyn_data(bfd) -> dynversym)
#define coff_dynamic(bfd)     (dyn_data(bfd) -> dynamic)
#define coff_dt_name(bfd)     (dyn_data(bfd) -> dt_name)


/* A cache of additional information if dynamic linking is actually
   happening. */
struct dynamic_info
{
  /* The "standard" secttions, where they're easy and quick to find */
  asection *dynsymtab;
  asection *dynstrtab;
  asection *dynverdef;
  asection *dynverref;
  asection *dynversym;
  asection *dynamic;

  /* Number of symbol version definitions we are about to emit.  */
  unsigned int cverdefs;

  /* Number of symbol version references we are about to emit.  */
  unsigned int cverrefs;

  /* Symbol version definitions in external objects.  */
  coff_internal_verdef *verdef;

  /* Symbol version references to external objects.  */
  coff_internal_verneed *verref;

  /* A mapping from local symbols to offsets into the global offset
     table, used when linking.  This is indexed by the symbol index.  */
  bfd_vma *local_got_offsets;

  /* The front end needs to let the back end know
     what filename should be used for a dynamic object if the
     dynamic object is found using a search.  The front end then
     sometimes needs to know what name was actually used.  Until the
     file has been added to the linker symbol table, this field holds
     the name the linker wants.  After it has been added, it holds the
     name actually used, which will be the DT_SONAME entry if there is
     one.  */
  const char *dt_name;

};

/* These constants are used for the version number of a Verdef structure.  */

#define VER_DEF_NONE		0
#define VER_DEF_CURRENT		1

/* These constants appear in the vd_flags field of a Verdef structure.  */

#define VER_FLG_BASE		0x1
#define VER_FLG_WEAK		0x2

/* These special constants can be found in an Versym field.  */

#define VER_NDX_LOCAL		0
#define VER_NDX_GLOBAL		1

/* These constants are used for the version number of a Verneed structure.  */

#define VER_NEED_NONE		0
#define VER_NEED_CURRENT	1

/* This flag appears in a Versym structure.  It means that the symbol
   is hidden, and is only visible with an explicit version number.
   This is a GNU extension.  */

#define VERSYM_HIDDEN		0x8000

/* This is the mask for the rest of the Versym information.  */

#define VERSYM_VERSION		0x7fff

/* This is a special token which appears as part of a symbol name.  It
   indictes that the rest of the name is actually the name of a
   version node, and is not part of the actual name.  This is a GNU
   extension.  For example, the symbol name `stat%ver2' is taken to
   mean the symbol `stat' in version `ver2'.  The historical use
   of "@" doesn't work in PE, because @ is used by the Microsoft
   tools. */
#define COFF_VER_CHR '%'

#define ARCH_SIZE 32   /* someday it might be 64 */

#endif /* ] */

/* Extracted from coffcode.h.  */
typedef struct coff_ptr_struct
{
  /* Remembers the offset from the first symbol in the file for
     this symbol. Generated by coff_renumber_symbols. */
  unsigned int offset;

  /* Should the value of this symbol be renumbered.  Used for
     XCOFF C_BSTAT symbols.  Set by coff_slurp_symbol_table.  */
  unsigned int fix_value : 1;

  /* Should the tag field of this symbol be renumbered.
     Created by coff_pointerize_aux. */
  unsigned int fix_tag : 1;

  /* Should the endidx field of this symbol be renumbered.
     Created by coff_pointerize_aux. */
  unsigned int fix_end : 1;

  /* Should the x_csect.x_scnlen field be renumbered.
     Created by coff_pointerize_aux. */
  unsigned int fix_scnlen : 1;

  /* Fix up an XCOFF C_BINCL/C_EINCL symbol.  The value is the
     index into the line number entries.  Set by coff_slurp_symbol_table.  */
  unsigned int fix_line : 1;

  /* The container for the symbol structure as read and translated
     from the file. */
  union
  {
    union internal_auxent auxent;
    struct internal_syment syment;
  } u;
} combined_entry_type;


/* Each canonical asymbol really looks like this: */

typedef struct coff_symbol_struct
{
  /* The actual symbol which the rest of BFD works with */
  asymbol symbol;

  /* A pointer to the hidden information for this symbol */
  combined_entry_type *native;

  /* A pointer to the linenumber information for this symbol */
  struct lineno_cache_entry *lineno;

  /* Have the line numbers been relocated yet ? */
  boolean done_lineno;
} coff_symbol_type;
/* COFF symbol classifications.  */

enum coff_symbol_classification
{
  /* Global symbol.  */
  COFF_SYMBOL_GLOBAL,
  /* Common symbol.  */
  COFF_SYMBOL_COMMON,
  /* Undefined symbol.  */
  COFF_SYMBOL_UNDEFINED,
  /* Local symbol.  */
  COFF_SYMBOL_LOCAL,
  /* PE section symbol.  */
  COFF_SYMBOL_PE_SECTION
};

typedef struct
{
  void (*_bfd_coff_swap_aux_in)
    PARAMS ((bfd *, PTR, int, int, int, int, PTR));

  void (*_bfd_coff_swap_sym_in)
    PARAMS ((bfd *, PTR, PTR));

  void (*_bfd_coff_swap_lineno_in)
    PARAMS ((bfd *, PTR, PTR));

  unsigned int (*_bfd_coff_swap_aux_out)
    PARAMS ((bfd *, PTR, int, int, int, int, PTR));

  unsigned int (*_bfd_coff_swap_sym_out)
    PARAMS ((bfd *, PTR, PTR));

  unsigned int (*_bfd_coff_swap_lineno_out)
    PARAMS ((bfd *, PTR, PTR));

  unsigned int (*_bfd_coff_swap_reloc_out)
    PARAMS ((bfd *, PTR, PTR));

  unsigned int (*_bfd_coff_swap_filehdr_out)
    PARAMS ((bfd *, PTR, PTR));

  unsigned int (*_bfd_coff_swap_aouthdr_out)
    PARAMS ((bfd *, PTR, PTR));

  unsigned int (*_bfd_coff_swap_scnhdr_out)
    PARAMS ((bfd *, PTR, PTR));

  unsigned int _bfd_filhsz;
  unsigned int _bfd_aoutsz;
  unsigned int _bfd_scnhsz;
  unsigned int _bfd_symesz;
  unsigned int _bfd_auxesz;
  unsigned int _bfd_relsz;
  unsigned int _bfd_linesz;
  unsigned int _bfd_filnmlen;
  boolean _bfd_coff_long_filenames;
  boolean _bfd_coff_long_section_names;
  unsigned int _bfd_coff_default_section_alignment_power;
  boolean _bfd_coff_force_symnames_in_strings;
  unsigned int _bfd_coff_debug_string_prefix_length;

  void (*_bfd_coff_swap_filehdr_in)
    PARAMS ((bfd *, PTR, PTR));

  void (*_bfd_coff_swap_aouthdr_in)
    PARAMS ((bfd *, PTR, PTR));

  void (*_bfd_coff_swap_scnhdr_in)
    PARAMS ((bfd *, PTR, PTR));

  void (*_bfd_coff_swap_reloc_in)
    PARAMS ((bfd *abfd, PTR, PTR));

  boolean (*_bfd_coff_bad_format_hook)
    PARAMS ((bfd *, PTR));

  boolean (*_bfd_coff_set_arch_mach_hook)
    PARAMS ((bfd *, PTR));

  PTR (*_bfd_coff_mkobject_hook)
    PARAMS ((bfd *, PTR, PTR));

  boolean (*_bfd_styp_to_sec_flags_hook)
    PARAMS ((bfd *, PTR, const char *, asection *, flagword *));

  void (*_bfd_set_alignment_hook)
    PARAMS ((bfd *, asection *, PTR));

  boolean (*_bfd_coff_slurp_symbol_table)
    PARAMS ((bfd *));

  boolean (*_bfd_coff_symname_in_debug)
    PARAMS ((bfd *, struct internal_syment *));

  boolean (*_bfd_coff_pointerize_aux_hook)
    PARAMS ((bfd *, combined_entry_type *, combined_entry_type *,
            unsigned int, combined_entry_type *));

  boolean (*_bfd_coff_print_aux)
    PARAMS ((bfd *, FILE *, combined_entry_type *, combined_entry_type *,
            combined_entry_type *, unsigned int));

  void (*_bfd_coff_reloc16_extra_cases)
    PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *, arelent *,
           bfd_byte *, unsigned int *, unsigned int *));

  int (*_bfd_coff_reloc16_estimate)
    PARAMS ((bfd *, asection *, arelent *, unsigned int,
            struct bfd_link_info *));

  enum coff_symbol_classification (*_bfd_coff_classify_symbol)
    PARAMS ((bfd *, struct internal_syment *));

  boolean (*_bfd_coff_compute_section_file_positions)
    PARAMS ((bfd *));

  boolean (*_bfd_coff_start_final_link)
    PARAMS ((bfd *, struct bfd_link_info *));

  boolean (*_bfd_coff_relocate_section)
    PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
            struct internal_reloc *, struct internal_syment *, asection **));

  reloc_howto_type *(*_bfd_coff_rtype_to_howto)
    PARAMS ((bfd *, asection *, struct internal_reloc *,
            struct coff_link_hash_entry *, struct internal_syment *,
            bfd_vma *));

  boolean (*_bfd_coff_adjust_symndx)
    PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *,
            struct internal_reloc *, boolean *));

  boolean (*_bfd_coff_link_add_one_symbol)
    PARAMS ((struct bfd_link_info *, bfd *, const char *, flagword,
            asection *, bfd_vma, const char *, boolean, boolean,
            struct bfd_link_hash_entry **));

  boolean (*_bfd_coff_link_output_has_begun)
    PARAMS ((bfd *, struct coff_final_link_info *));

  boolean (*_bfd_coff_final_link_postscript)
    PARAMS ((bfd *, struct coff_final_link_info *));

 void (*_bfd_coff_canonicalize_one_symbol) PARAMS((
       bfd *,
       combined_entry_type *,
       coff_symbol_type *));
#ifdef DYNAMIC_LINKING
 void (*_bfd_coff_swap_dyn_in) PARAMS ((
       bfd *, 
       const PTR, 
       coff_internal_dyn *));
 void (*_bfd_coff_swap_dyn_out) PARAMS ((
       bfd *, 
       const coff_internal_dyn *, 
       coff_external_dyn *));
 void (*_bfd_coff_swap_verdef_in) PARAMS ((
       bfd *abfd,
       const coff_external_verdef *src,
       coff_internal_verdef *dst));
 void (*_bfd_coff_swap_verdef_out) PARAMS ((
       bfd *abfd,
       const coff_internal_verdef *src,
       coff_external_verdef *dst));
 void (*_bfd_coff_swap_verdaux_in) PARAMS ((
       bfd *abfd,
       const coff_external_verdaux *src,
       coff_internal_verdaux *dst));
 void (*_bfd_coff_swap_verdaux_out) PARAMS ((
       bfd *abfd,
       const coff_internal_verdaux *src,
       coff_external_verdaux *dst));
 void (*_bfd_coff_swap_verneed_in) PARAMS ((
       bfd *abfd,
       const coff_external_verneed *src,
       coff_internal_verneed *dst));
 void (*_bfd_coff_swap_verneed_out) PARAMS ((
       bfd *abfd,
       const coff_internal_verneed *src,
       coff_external_verneed *dst));
 void (*_bfd_coff_swap_vernaux_in) PARAMS ((
       bfd *abfd,
       const coff_external_vernaux *src,
       coff_internal_vernaux *dst));
 void (*_bfd_coff_swap_vernaux_out) PARAMS ((
       bfd *abfd,
       const coff_internal_vernaux *src,
       coff_external_vernaux *dst));
 void (*_bfd_coff_swap_versym_in) PARAMS ((
       bfd *abfd,
       const coff_external_versym *src,
       coff_internal_versym *dst));
 void (*_bfd_coff_swap_versym_out) PARAMS ((
       bfd *abfd,
       const coff_internal_versym *src,
       coff_external_versym *dst));
 boolean (*_bfd_coff_backend_link_create_dynamic_sections) PARAMS ((
       bfd *,
       struct bfd_link_info *));
 boolean (*_bfd_coff_backend_check_relocs) PARAMS ((
       bfd *,
       struct bfd_link_info *,
       asection *,
       const struct internal_reloc *relocs));
 boolean (*_bfd_coff_backend_adjust_dynamic_symbol) PARAMS ((
       bfd *,
       struct bfd_link_info *,
       struct coff_link_hash_entry *,
       boolean));
 boolean (*_bfd_coff_backend_size_dynamic_sections) PARAMS ((
       bfd *,
       struct bfd_link_info *));
 boolean (*_bfd_coff_backend_finish_dynamic_symbol) PARAMS ((
       bfd *,
       struct bfd_link_info *,
       struct coff_link_hash_entry *,
       struct internal_syment *));
 boolean (*_bfd_coff_backend_finish_dynamic_sections) PARAMS ((
       bfd *,
       struct bfd_link_info *));
#endif

} bfd_coff_backend_data;

#define coff_backend_info(abfd) \
  ((bfd_coff_backend_data *) (abfd)->xvec->backend_data)

#define bfd_coff_swap_aux_in(a,e,t,c,ind,num,i) \
  ((coff_backend_info (a)->_bfd_coff_swap_aux_in) (a,e,t,c,ind,num,i))

#define bfd_coff_swap_sym_in(a,e,i) \
  ((coff_backend_info (a)->_bfd_coff_swap_sym_in) (a,e,i))

#define bfd_coff_swap_lineno_in(a,e,i) \
  ((coff_backend_info ( a)->_bfd_coff_swap_lineno_in) (a,e,i))

#define bfd_coff_swap_reloc_out(abfd, i, o) \
  ((coff_backend_info (abfd)->_bfd_coff_swap_reloc_out) (abfd, i, o))

#define bfd_coff_swap_lineno_out(abfd, i, o) \
  ((coff_backend_info (abfd)->_bfd_coff_swap_lineno_out) (abfd, i, o))

#define bfd_coff_swap_aux_out(a,i,t,c,ind,num,o) \
  ((coff_backend_info (a)->_bfd_coff_swap_aux_out) (a,i,t,c,ind,num,o))

#define bfd_coff_swap_sym_out(abfd, i,o) \
  ((coff_backend_info (abfd)->_bfd_coff_swap_sym_out) (abfd, i, o))

#define bfd_coff_swap_scnhdr_out(abfd, i,o) \
  ((coff_backend_info (abfd)->_bfd_coff_swap_scnhdr_out) (abfd, i, o))

#define bfd_coff_swap_filehdr_out(abfd, i,o) \
  ((coff_backend_info (abfd)->_bfd_coff_swap_filehdr_out) (abfd, i, o))

#define bfd_coff_swap_aouthdr_out(abfd, i,o) \
  ((coff_backend_info (abfd)->_bfd_coff_swap_aouthdr_out) (abfd, i, o))

#ifdef DYNAMIC_LINKING
#define bfd_coff_swap_dyn_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_dyn_in) (abfd, i, o))
#define bfd_coff_swap_dyn_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_dyn_out) (abfd, i, o))
#define bfd_coff_swap_verdef_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_verdef_in) (abfd, i, o))
#define bfd_coff_swap_verdef_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_verdef_out) (abfd, i, o))
#define bfd_coff_swap_verdaux_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_verdaux_in) (abfd, i, o))
#define bfd_coff_swap_verdaux_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_verdaux_out) (abfd, i, o))
#define bfd_coff_swap_verneed_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_verneed_in) (abfd, i, o))
#define bfd_coff_swap_verneed_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_verneed_out) (abfd, i, o))
#define bfd_coff_swap_vernaux_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_vernaux_in) (abfd, i, o))
#define bfd_coff_swap_vernaux_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_vernaux_out) (abfd, i, o))
#define bfd_coff_swap_versym_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_versym_in) (abfd, i, o))
#define bfd_coff_swap_versym_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_versym_out) (abfd, i, o))
#define bfd_coff_backend_link_create_dynamic_sections(abfd, i) \
        ((coff_backend_info (abfd)-> \
           _bfd_coff_backend_link_create_dynamic_sections) (abfd, i))
#define bfd_coff_backend_check_relocs(abfd, i, s, r) \
        ((coff_backend_info (abfd)->_bfd_coff_backend_check_relocs) \
           (abfd, i, s, r))
#define bfd_coff_backend_adjust_dynamic_symbol(abfd, i, h, s) \
        ((coff_backend_info (abfd)->_bfd_coff_backend_adjust_dynamic_symbol) \
           (abfd, i, h, s))
#define bfd_coff_backend_size_dynamic_sections(abfd, i) \
        ((coff_backend_info (abfd)->_bfd_coff_backend_size_dynamic_sections) \
           (abfd, i))
#define bfd_coff_backend_finish_dynamic_symbol(abfd, i, h, s) \
        ((coff_backend_info (abfd)->_bfd_coff_backend_finish_dynamic_symbol) \
           (abfd, i, h, s))
#define bfd_coff_backend_finish_dynamic_sections(abfd, i) \
        ((coff_backend_info(abfd)->_bfd_coff_backend_finish_dynamic_sections) \
           (abfd, i))
#endif

#define bfd_coff_filhsz(abfd) (coff_backend_info (abfd)->_bfd_filhsz)
#define bfd_coff_aoutsz(abfd) (coff_backend_info (abfd)->_bfd_aoutsz)
#define bfd_coff_scnhsz(abfd) (coff_backend_info (abfd)->_bfd_scnhsz)
#define bfd_coff_symesz(abfd) (coff_backend_info (abfd)->_bfd_symesz)
#define bfd_coff_auxesz(abfd) (coff_backend_info (abfd)->_bfd_auxesz)
#define bfd_coff_relsz(abfd)  (coff_backend_info (abfd)->_bfd_relsz)
#define bfd_coff_linesz(abfd) (coff_backend_info (abfd)->_bfd_linesz)
#define bfd_coff_filnmlen(abfd) (coff_backend_info (abfd)->_bfd_filnmlen)
#define bfd_coff_long_filenames(abfd) \
  (coff_backend_info (abfd)->_bfd_coff_long_filenames)
#define bfd_coff_long_section_names(abfd) \
  (coff_backend_info (abfd)->_bfd_coff_long_section_names)
#define bfd_coff_default_section_alignment_power(abfd) \
  (coff_backend_info (abfd)->_bfd_coff_default_section_alignment_power)
#define bfd_coff_swap_filehdr_in(abfd, i,o) \
  ((coff_backend_info (abfd)->_bfd_coff_swap_filehdr_in) (abfd, i, o))

#define bfd_coff_swap_aouthdr_in(abfd, i,o) \
  ((coff_backend_info (abfd)->_bfd_coff_swap_aouthdr_in) (abfd, i, o))

#define bfd_coff_swap_scnhdr_in(abfd, i,o) \
  ((coff_backend_info (abfd)->_bfd_coff_swap_scnhdr_in) (abfd, i, o))

#define bfd_coff_swap_reloc_in(abfd, i, o) \
  ((coff_backend_info (abfd)->_bfd_coff_swap_reloc_in) (abfd, i, o))

#define bfd_coff_bad_format_hook(abfd, filehdr) \
  ((coff_backend_info (abfd)->_bfd_coff_bad_format_hook) (abfd, filehdr))

#define bfd_coff_set_arch_mach_hook(abfd, filehdr)\
  ((coff_backend_info (abfd)->_bfd_coff_set_arch_mach_hook) (abfd, filehdr))
#define bfd_coff_mkobject_hook(abfd, filehdr, aouthdr)\
  ((coff_backend_info (abfd)->_bfd_coff_mkobject_hook) (abfd, filehdr, aouthdr))

#define bfd_coff_styp_to_sec_flags_hook(abfd, scnhdr, name, section, flags_ptr)\
  ((coff_backend_info (abfd)->_bfd_styp_to_sec_flags_hook)\
   (abfd, scnhdr, name, section, flags_ptr))

#define bfd_coff_set_alignment_hook(abfd, sec, scnhdr)\
  ((coff_backend_info (abfd)->_bfd_set_alignment_hook) (abfd, sec, scnhdr))

#define bfd_coff_slurp_symbol_table(abfd)\
  ((coff_backend_info (abfd)->_bfd_coff_slurp_symbol_table) (abfd))

#define bfd_coff_symname_in_debug(abfd, sym)\
  ((coff_backend_info (abfd)->_bfd_coff_symname_in_debug) (abfd, sym))

#define bfd_coff_force_symnames_in_strings(abfd)\
  (coff_backend_info (abfd)->_bfd_coff_force_symnames_in_strings)

#define bfd_coff_debug_string_prefix_length(abfd)\
  (coff_backend_info (abfd)->_bfd_coff_debug_string_prefix_length)

#define bfd_coff_print_aux(abfd, file, base, symbol, aux, indaux)\
  ((coff_backend_info (abfd)->_bfd_coff_print_aux)\
   (abfd, file, base, symbol, aux, indaux))

#define bfd_coff_reloc16_extra_cases(abfd, link_info, link_order, reloc, data, src_ptr, dst_ptr)\
  ((coff_backend_info (abfd)->_bfd_coff_reloc16_extra_cases)\
   (abfd, link_info, link_order, reloc, data, src_ptr, dst_ptr))

#define bfd_coff_reloc16_estimate(abfd, section, reloc, shrink, link_info)\
  ((coff_backend_info (abfd)->_bfd_coff_reloc16_estimate)\
   (abfd, section, reloc, shrink, link_info))

#define bfd_coff_classify_symbol(abfd, sym)\
  ((coff_backend_info (abfd)->_bfd_coff_classify_symbol)\
   (abfd, sym))

#define bfd_coff_compute_section_file_positions(abfd)\
  ((coff_backend_info (abfd)->_bfd_coff_compute_section_file_positions)\
   (abfd))

#define bfd_coff_start_final_link(obfd, info)\
  ((coff_backend_info (obfd)->_bfd_coff_start_final_link)\
   (obfd, info))
#define bfd_coff_relocate_section(obfd,info,ibfd,o,con,rel,isyms,secs)\
  ((coff_backend_info (ibfd)->_bfd_coff_relocate_section)\
   (obfd, info, ibfd, o, con, rel, isyms, secs))
#define bfd_coff_rtype_to_howto(abfd, sec, rel, h, sym, addendp)\
  ((coff_backend_info (abfd)->_bfd_coff_rtype_to_howto)\
   (abfd, sec, rel, h, sym, addendp))
#define bfd_coff_adjust_symndx(obfd, info, ibfd, sec, rel, adjustedp)\
  ((coff_backend_info (abfd)->_bfd_coff_adjust_symndx)\
   (obfd, info, ibfd, sec, rel, adjustedp))
#define bfd_coff_link_add_one_symbol(info,abfd,name,flags,section,value,string,cp,coll,hashp)\
  ((coff_backend_info (abfd)->_bfd_coff_link_add_one_symbol)\
   (info, abfd, name, flags, section, value, string, cp, coll, hashp))
#define bfd_coff_canonicalize_one_symbol(abfd, src, dest) \
        ((coff_backend_info (abfd)->_bfd_coff_canonicalize_one_symbol)\
         (abfd, src, dest))

#define bfd_coff_link_output_has_begun(a,p) \
  ((coff_backend_info (a)->_bfd_coff_link_output_has_begun) (a,p))
#define bfd_coff_final_link_postscript(a,p) \
  ((coff_backend_info (a)->_bfd_coff_final_link_postscript) (a,p))

