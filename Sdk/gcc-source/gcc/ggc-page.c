/* "Bag-of-pages" garbage collector for the GNU compiler.
   Copyright (C) 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "tree.h"
#include "rtl.h"
#include "tm_p.h"
#include "toplev.h"
#include "flags.h"
#include "ggc.h"
#include "timevar.h"
#include "params.h"
#ifdef ENABLE_VALGRIND_CHECKING
#include <valgrind.h>
#else
/* Avoid #ifdef:s when we can help it.  */
#define VALGRIND_DISCARD(x)
#endif

/* Prefer MAP_ANON(YMOUS) to /dev/zero, since we don't need to keep a
   file open.  Prefer either to valloc.  */
#ifdef HAVE_MMAP_ANON
# undef HAVE_MMAP_DEV_ZERO

# include <sys/mman.h>
# ifndef MAP_FAILED
#  define MAP_FAILED -1
# endif
# if !defined (MAP_ANONYMOUS) && defined (MAP_ANON)
#  define MAP_ANONYMOUS MAP_ANON
# endif
# define USING_MMAP

#endif

#ifdef HAVE_MMAP_DEV_ZERO

# include <sys/mman.h>
# ifndef MAP_FAILED
#  define MAP_FAILED -1
# endif
# define USING_MMAP

#endif

#undef USING_MMAP  // INTERIX ONLY, and temporary

#ifndef USING_MMAP
#define USING_MALLOC_PAGE_GROUPS
#endif

/* Stategy:

   This garbage-collecting allocator allocates objects on one of a set
   of pages.  Each page can allocate objects of a single size only;
   available sizes are powers of two starting at four bytes.  The size
   of an allocation request is rounded up to the next power of two
   (`order'), and satisfied from the appropriate page.

   Each page is recorded in a page-entry, which also maintains an
   in-use bitmap of object positions on the page.  This allows the
   allocation state of a particular object to be flipped without
   touching the page itself.

   Each page-entry also has a context depth, which is used to track
   pushing and popping of allocation contexts.  Only objects allocated
   in the current (highest-numbered) context may be collected.

   Page entries are arranged in an array of singly-linked lists.  The
   array is indexed by the allocation size, in bits, of the pages on
   it; i.e. all pages on a list allocate objects of the same size.
   Pages are ordered on the list such that all non-full pages precede
   all full pages, with non-full pages arranged in order of decreasing
   context depth.

   Empty pages (of all orders) are kept on a single page cache list,
   and are considered first when new pages are required; they are
   deallocated at the start of the next collection if they haven't
   been recycled by then.  */

/* Define GGC_DEBUG_LEVEL to print debugging information.
     0: No debugging output.
     1: GC statistics only.
     2: Page-entry allocations/deallocations as well.
     3: Object allocations as well.
     4: Object marks as well.  */
#define GGC_DEBUG_LEVEL (0)

#ifndef HOST_BITS_PER_PTR
#define HOST_BITS_PER_PTR  HOST_BITS_PER_LONG
#endif


/* A two-level tree is used to look up the page-entry for a given
   pointer.  Two chunks of the pointer's bits are extracted to index
   the first and second levels of the tree, as follows:

				   HOST_PAGE_SIZE_BITS
			   32		|      |
       msb +----------------+----+------+------+ lsb
			    |    |      |
			 PAGE_L1_BITS   |
				 |      |
			       PAGE_L2_BITS

   The bottommost HOST_PAGE_SIZE_BITS are ignored, since page-entry
   pages are aligned on system page boundaries.  The next most
   significant PAGE_L2_BITS and PAGE_L1_BITS are the second and first
   index values in the lookup table, respectively.

   For 32-bit architectures and the settings below, there are no
   leftover bits.  For architectures with wider pointers, the lookup
   tree points to a list of pages, which must be scanned to find the
   correct one.  */

#define PAGE_L1_BITS	(8)
#define PAGE_L2_BITS	(32 - PAGE_L1_BITS - G.lg_pagesize)
#define PAGE_L1_SIZE	((size_t) 1 << PAGE_L1_BITS)
#define PAGE_L2_SIZE	((size_t) 1 << PAGE_L2_BITS)

#define LOOKUP_L1(p) \
  (((size_t) (p) >> (32 - PAGE_L1_BITS)) & ((1 << PAGE_L1_BITS) - 1))

#define LOOKUP_L2(p) \
  (((size_t) (p) >> G.lg_pagesize) & ((1 << PAGE_L2_BITS) - 1))

/* The number of objects per allocation page, for objects on a page of
   the indicated ORDER.  */
#define OBJECTS_PER_PAGE(ORDER) objects_per_page_table[ORDER]

/* The size of an object on a page of the indicated ORDER.  */
#define OBJECT_SIZE(ORDER) object_size_table[ORDER]

/* For speed, we avoid doing a general integer divide to locate the
   offset in the allocation bitmap, by precalculating numbers M, S
   such that (O * M) >> S == O / Z (modulo 2^32), for any offset O
   within the page which is evenly divisible by the object size Z.  */
#define DIV_MULT(ORDER) inverse_table[ORDER].mult
#define DIV_SHIFT(ORDER) inverse_table[ORDER].shift
#define OFFSET_TO_BIT(OFFSET, ORDER) \
  (((OFFSET) * DIV_MULT (ORDER)) >> DIV_SHIFT (ORDER))

/* The number of extra orders, not corresponding to power-of-two sized
   objects.  */

#define NUM_EXTRA_ORDERS ARRAY_SIZE (extra_order_size_table)

#define RTL_SIZE(NSLOTS) \
  (sizeof (struct rtx_def) + ((NSLOTS) - 1) * sizeof (rtunion))

/* The Ith entry is the maximum size of an object to be stored in the
   Ith extra order.  Adding a new entry to this array is the *only*
   thing you need to do to add a new special allocation size.  */

static const size_t extra_order_size_table[] = {
  sizeof (struct tree_decl),
  sizeof (struct tree_list),
  RTL_SIZE (2),			/* REG, MEM, PLUS, etc.  */
  RTL_SIZE (10),		/* INSN, CALL_INSN, JUMP_INSN */
};

/* The total number of orders.  */

#define NUM_ORDERS (HOST_BITS_PER_PTR + NUM_EXTRA_ORDERS)

/* We use this structure to determine the alignment required for
   allocations.  For power-of-two sized allocations, that's not a
   problem, but it does matter for odd-sized allocations.  */

struct max_alignment {
  char c;
  union {
    HOST_WIDEST_INT i;
#ifdef HAVE_LONG_DOUBLE
    long double d;
#else
    double d;
#endif
  } u;
};

/* The biggest alignment required.  */

#define MAX_ALIGNMENT (offsetof (struct max_alignment, u))

/* The Ith entry is the number of objects on a page or order I.  */

static unsigned objects_per_page_table[NUM_ORDERS];

/* The Ith entry is the size of an object on a page of order I.  */

static size_t object_size_table[NUM_ORDERS];

/* The Ith entry is a pair of numbers (mult, shift) such that
   ((k * mult) >> shift) mod 2^32 == (k / OBJECT_SIZE(I)) mod 2^32,
   for all k evenly divisible by OBJECT_SIZE(I).  */

static struct
{
  unsigned int mult;
  unsigned int shift;
}
inverse_table[NUM_ORDERS];

/* A page_entry records the status of an allocation page.  This
   structure is dynamically sized to fit the bitmap in_use_p.  */
typedef struct page_entry
{
  /* The next page-entry with objects of the same size, or NULL if
     this is the last page-entry.  */
  struct page_entry *next;

  /* The number of bytes allocated.  (This will always be a multiple
     of the host system page size.)  */
  size_t bytes;

  /* The address at which the memory is allocated.  */
  char *page;

#ifdef USING_MALLOC_PAGE_GROUPS
  /* Back pointer to the page group this page came from.  */
  struct page_group *group;
#endif

  /* This is the index in the by_depth varray where this page table
     can be found.  */
  unsigned long index_by_depth;

  /* Context depth of this page.  */
  unsigned short context_depth;

  /* The number of free objects remaining on this page.  */
  unsigned short num_free_objects;

  /* A likely candidate for the bit position of a free object for the
     next allocation from this page.  */
  unsigned short next_bit_hint;

  /* The lg of size of objects allocated from this page.  */
  unsigned char order;

  /* A bit vector indicating whether or not objects are in use.  The
     Nth bit is one if the Nth object on this page is allocated.  This
     array is dynamically sized.  */
  unsigned long in_use_p[1];
} page_entry;

#ifdef USING_MALLOC_PAGE_GROUPS
/* A page_group describes a large allocation from malloc, from which
   we parcel out aligned pages.  */
typedef struct page_group
{
  /* A linked list of all extant page groups.  */
  struct page_group *next;

  /* The address we received from malloc.  */
  char *allocation;

  /* The size of the block.  */
  size_t alloc_size;

  /* A bitmask of pages in use.  */
  unsigned int in_use;
} page_group;
#endif

#if HOST_BITS_PER_PTR <= 32

/* On 32-bit hosts, we use a two level page table, as pictured above.  */
typedef page_entry **page_table[PAGE_L1_SIZE];

#else

/* On 64-bit hosts, we use the same two level page tables plus a linked
   list that disambiguates the top 32-bits.  There will almost always be
   exactly one entry in the list.  */
typedef struct page_table_chain
{
  struct page_table_chain *next;
  size_t high_bits;
  page_entry **table[PAGE_L1_SIZE];
} *page_table;

#endif

/* The rest of the global variables.  */
static struct globals
{
  /* The Nth element in this array is a page with objects of size 2^N.
     If there are any pages with free objects, they will be at the
     head of the list.  NULL if there are no page-entries for this
     object size.  */
  page_entry *pages[NUM_ORDERS];

  /* The Nth element in this array is the last page with objects of
     size 2^N.  NULL if there are no page-entries for this object
     size.  */
  page_entry *page_tails[NUM_ORDERS];

  /* Lookup table for associating allocation pages with object addresses.  */
  page_table lookup;

  /* The system's page size.  */
  size_t pagesize;
  size_t lg_pagesize;

  /* Bytes currently allocated.  */
  size_t allocated;

  /* Bytes currently allocated at the end of the last collection.  */
  size_t allocated_last_gc;

  /* Total amount of memory mapped.  */
  size_t bytes_mapped;

  /* Bit N set if any allocations have been done at context depth N.  */
  unsigned long context_depth_allocations;

  /* Bit N set if any collections have been done at context depth N.  */
  unsigned long context_depth_collections;

  /* The current depth in the context stack.  */
  unsigned short context_depth;

  /* A file descriptor open to /dev/zero for reading.  */
#if defined (HAVE_MMAP_DEV_ZERO)
  int dev_zero_fd;
#endif

  /* A cache of free system pages.  */
  page_entry *free_pages;

#ifdef USING_MALLOC_PAGE_GROUPS
  page_group *page_groups;
#endif

  /* The file descriptor for debugging output.  */
  FILE *debug_file;

  /* Current number of elements in use in depth below.  */
  unsigned int depth_in_use;

  /* Maximum number of elements that can be used before resizing.  */
  unsigned int depth_max;

  /* Each element of this arry is an index in by_depth where the given
     depth starts.  This structure is indexed by that given depth we
     are interested in.  */
  unsigned int *depth;

  /* Current number of elements in use in by_depth below.  */
  unsigned int by_depth_in_use;

  /* Maximum number of elements that can be used before resizing.  */
  unsigned int by_depth_max;

  /* Each element of this array is a pointer to a page_entry, all
     page_entries can be found in here by increasing depth.
     index_by_depth in the page_entry is the index into this data
     structure where that page_entry can be found.  This is used to
     speed up finding all page_entries at a particular depth.  */
  page_entry **by_depth;

  /* Each element is a pointer to the saved in_use_p bits, if any,
     zero otherwise.  We allocate them all together, to enable a
     better runtime data access pattern.  */
  unsigned long **save_in_use;

} G;

/* The size in bytes required to maintain a bitmap for the objects
   on a page-entry.  */
#define BITMAP_SIZE(Num_objects) \
  (CEIL ((Num_objects), HOST_BITS_PER_LONG) * sizeof(long))

/* Allocate pages in chunks of this size, to throttle calls to memory
   allocation routines.  The first page is used, the rest go onto the
   free list.  This cannot be larger than HOST_BITS_PER_INT for the
   in_use bitmask for page_group.  */
#define GGC_QUIRE_SIZE 16

/* Initial guess as to how many page table entries we might need.  */
#define INITIAL_PTE_COUNT 128

static int ggc_allocated_p PARAMS ((const void *));
static page_entry *lookup_page_table_entry PARAMS ((const void *));
static void set_page_table_entry PARAMS ((void *, page_entry *));
#ifdef USING_MMAP
static char *alloc_anon PARAMS ((char *, size_t));
#endif
#ifdef USING_MALLOC_PAGE_GROUPS
static size_t page_group_index PARAMS ((char *, char *));
static void set_page_group_in_use PARAMS ((page_group *, char *));
static void clear_page_group_in_use PARAMS ((page_group *, char *));
#endif
static struct page_entry * alloc_page PARAMS ((unsigned));
static void free_page PARAMS ((struct page_entry *));
static void release_pages PARAMS ((void));
static void clear_marks PARAMS ((void));
static void sweep_pages PARAMS ((void));
static void ggc_recalculate_in_use_p PARAMS ((page_entry *));
static void compute_inverse PARAMS ((unsigned));
static inline void adjust_depth PARAMS ((void));

#ifdef ENABLE_GC_CHECKING
static void poison_pages PARAMS ((void));
#endif

void debug_print_page_list PARAMS ((int));
static void push_depth PARAMS ((unsigned int));
static void push_by_depth PARAMS ((page_entry *, unsigned long *));

/* Push an entry onto G.depth.  */

inline static void
push_depth (i)
     unsigned int i;
{
  if (G.depth_in_use >= G.depth_max)
    {
      G.depth_max *= 2;
      G.depth = (unsigned int *) xrealloc ((char *) G.depth,
					   G.depth_max * sizeof (unsigned int));
    }
  G.depth[G.depth_in_use++] = i;
}

/* Push an entry onto G.by_depth and G.save_in_use.  */

inline static void
push_by_depth (p, s)
     page_entry *p;
     unsigned long *s;
{
  if (G.by_depth_in_use >= G.by_depth_max)
    {
      G.by_depth_max *= 2;
      G.by_depth = (page_entry **) xrealloc ((char *) G.by_depth,
					     G.by_depth_max * sizeof (page_entry *));
      G.save_in_use = (unsigned long **) xrealloc ((char *) G.save_in_use,
						   G.by_depth_max * sizeof (unsigned long *));
    }
  G.by_depth[G.by_depth_in_use] = p;
  G.save_in_use[G.by_depth_in_use++] = s;
}

/* For the 3.3 release, we will avoid prefetch, as it isn't tested widely.  */
#define prefetch(X) ((void) X)

#define save_in_use_p_i(__i) \
  (G.save_in_use[__i])
#define save_in_use_p(__p) \
  (save_in_use_p_i (__p->index_by_depth))

/* Returns nonzero if P was allocated in GC'able memory.  */

static inline int
ggc_allocated_p (p)
     const void *p;
{
  page_entry ***base;
  size_t L1, L2;

#if HOST_BITS_PER_PTR <= 32
  base = &G.lookup[0];
#else
  page_table table = G.lookup;
  size_t high_bits = (size_t) p & ~ (size_t) 0xffffffff;
  while (1)
    {
      if (table == NULL)
	return 0;
      if (table->high_bits == high_bits)
	break;
      table = table->next;
    }
  base = &table->table[0];
#endif

  /* Extract the level 1 and 2 indices.  */
  L1 = LOOKUP_L1 (p);
  L2 = LOOKUP_L2 (p);

  return base[L1] && base[L1][L2];
}

/* Traverse the page table and find the entry for a page.
   Die (probably) if the object wasn't allocated via GC.  */

static inline page_entry *
lookup_page_table_entry(p)
     const void *p;
{
  page_entry ***base;
  size_t L1, L2;

#if HOST_BITS_PER_PTR <= 32
  base = &G.lookup[0];
#else
  page_table table = G.lookup;
  size_t high_bits = (size_t) p & ~ (size_t) 0xffffffff;
  while (table->high_bits != high_bits)
    table = table->next;
  base = &table->table[0];
#endif

  /* Extract the level 1 and 2 indices.  */
  L1 = LOOKUP_L1 (p);
  L2 = LOOKUP_L2 (p);

  return base[L1][L2];
}

/* Set the page table entry for a page.  */

static void
set_page_table_entry(p, entry)
     void *p;
     page_entry *entry;
{
  page_entry ***base;
  size_t L1, L2;

#if HOST_BITS_PER_PTR <= 32
  base = &G.lookup[0];
#else
  page_table table;
  size_t high_bits = (size_t) p & ~ (size_t) 0xffffffff;
  for (table = G.lookup; table; table = table->next)
    if (table->high_bits == high_bits)
      goto found;

  /* Not found -- allocate a new table.  */
  table = (page_table) xcalloc (1, sizeof(*table));
  table->next = G.lookup;
  table->high_bits = high_bits;
  G.lookup = table;
found:
  base = &table->table[0];
#endif

  /* Extract the level 1 and 2 indices.  */
  L1 = LOOKUP_L1 (p);
  L2 = LOOKUP_L2 (p);

  if (base[L1] == NULL)
    base[L1] = (page_entry **) xcalloc (PAGE_L2_SIZE, sizeof (page_entry *));

  base[L1][L2] = entry;
}

/* Prints the page-entry for object size ORDER, for debugging.  */

void
debug_print_page_list (order)
     int order;
{
  page_entry *p;
  printf ("Head=%p, Tail=%p:\n", (PTR) G.pages[order],
	  (PTR) G.page_tails[order]);
  p = G.pages[order];
  while (p != NULL)
    {
      printf ("%p(%1d|%3d) -> ", (PTR) p, p->context_depth,
	      p->num_free_objects);
      p = p->next;
    }
  printf ("NULL\n");
  fflush (stdout);
}

#ifdef USING_MMAP
/* Allocate SIZE bytes of anonymous memory, preferably near PREF,
   (if non-null).  The ifdef structure here is intended to cause a
   compile error unless exactly one of the HAVE_* is defined.  */

static inline char *
alloc_anon (pref, size)
     char *pref ATTRIBUTE_UNUSED;
     size_t size;
{
#ifdef HAVE_MMAP_ANON
  char *page = (char *) mmap (pref, size, PROT_READ | PROT_WRITE,
			      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
#ifdef HAVE_MMAP_DEV_ZERO
  char *page = (char *) mmap (pref, size, PROT_READ | PROT_WRITE,
			      MAP_PRIVATE, G.dev_zero_fd, 0);
#endif

  if (page == (char *) MAP_FAILED)
    {
      perror ("virtual memory exhausted");
      exit (FATAL_EXIT_CODE);
    }

  /* Remember that we allocated this memory.  */
  G.bytes_mapped += size;

  /* Pretend we don't have access to the allocated pages.  We'll enable
     access to smaller pieces of the area in ggc_alloc.  Discard the
     handle to avoid handle leak.  */
  VALGRIND_DISCARD (VALGRIND_MAKE_NOACCESS (page, size));

  return page;
}
#endif
#ifdef USING_MALLOC_PAGE_GROUPS
/* Compute the index for this page into the page group.  */

static inline size_t
page_group_index (allocation, page)
     char *allocation, *page;
{
  return (size_t) (page - allocation) >> G.lg_pagesize;
}

/* Set and clear the in_use bit for this page in the page group.  */

static inline void
set_page_group_in_use (group, page)
     page_group *group;
     char *page;
{
  group->in_use |= 1 << page_group_index (group->allocation, page);
}

static inline void
clear_page_group_in_use (group, page)
     page_group *group;
     char *page;
{
  group->in_use &= ~(1 << page_group_index (group->allocation, page));
}
#endif

/* Allocate a new page for allocating objects of size 2^ORDER,
   and return an entry for it.  The entry is not added to the
   appropriate page_table list.  */

static inline struct page_entry *
alloc_page (order)
     unsigned order;
{
  struct page_entry *entry, *p, **pp;
  char *page;
  size_t num_objects;
  size_t bitmap_size;
  size_t page_entry_size;
  size_t entry_size;
#ifdef USING_MALLOC_PAGE_GROUPS
  page_group *group;
#endif

  num_objects = OBJECTS_PER_PAGE (order);
  bitmap_size = BITMAP_SIZE (num_objects + 1);
  page_entry_size = sizeof (page_entry) - sizeof (long) + bitmap_size;
  entry_size = num_objects * OBJECT_SIZE (order);
  if (entry_size < G.pagesize)
    entry_size = G.pagesize;

  entry = NULL;
  page = NULL;

  /* Check the list of free pages for one we can use.  */
  for (pp = &G.free_pages, p = *pp; p; pp = &p->next, p = *pp)
    if (p->bytes == entry_size)
      break;

  if (p != NULL)
    {
      /* Recycle the allocated memory from this page ...  */
      *pp = p->next;
      page = p->page;

#ifdef USING_MALLOC_PAGE_GROUPS
      group = p->group;
#endif

      /* ... and, if possible, the page entry itself.  */
      if (p->order == order)
	{
	  entry = p;
	  memset (entry, 0, page_entry_size);
	}
      else
	free (p);
    }
#ifdef USING_MMAP
  else if (entry_size == G.pagesize)
    {
      /* We want just one page.  Allocate a bunch of them and put the
	 extras on the freelist.  (Can only do this optimization with
	 mmap for backing store.)  */
      struct page_entry *e, *f = G.free_pages;
      int i;

      page = alloc_anon (NULL, G.pagesize * GGC_QUIRE_SIZE);

      /* This loop counts down so that the chain will be in ascending
	 memory order.  */
      for (i = GGC_QUIRE_SIZE - 1; i >= 1; i--)
	{
	  e = (struct page_entry *) xcalloc (1, page_entry_size);
	  e->order = order;
	  e->bytes = G.pagesize;
	  e->page = page + (i << G.lg_pagesize);
	  e->next = f;
	  f = e;
	}

      G.free_pages = f;
    }
  else
    page = alloc_anon (NULL, entry_size);
#endif
#ifdef USING_MALLOC_PAGE_GROUPS
  else
    {
      /* Allocate a large block of memory and serve out the aligned
	 pages therein.  This results in much less memory wastage
	 than the traditional implementation of valloc.  */

      char *allocation, *a, *enda;
      size_t alloc_size, head_slop, tail_slop;
      int multiple_pages = (entry_size == G.pagesize);

      if (multiple_pages)
	alloc_size = GGC_QUIRE_SIZE * G.pagesize;
      else
	alloc_size = entry_size + G.pagesize - 1;
      allocation = xmalloc (alloc_size);

      page = (char *) (((size_t) allocation + G.pagesize - 1) & -G.pagesize);
      head_slop = page - allocation;
      if (multiple_pages)
	tail_slop = ((size_t) allocation + alloc_size) & (G.pagesize - 1);
      else
	tail_slop = alloc_size - entry_size - head_slop;
      enda = allocation + alloc_size - tail_slop;

      /* We allocated N pages, which are likely not aligned, leaving
	 us with N-1 usable pages.  We plan to place the page_group
	 structure somewhere in the slop.  */
      if (head_slop >= sizeof (page_group))
	group = (page_group *)page - 1;
      else
	{
	  /* We magically got an aligned allocation.  Too bad, we have
	     to waste a page anyway.  */
	  if (tail_slop == 0)
	    {
	      enda -= G.pagesize;
	      tail_slop += G.pagesize;
	    }
	  if (tail_slop < sizeof (page_group))
	    abort ();
	  group = (page_group *)enda;
	  tail_slop -= sizeof (page_group);
	}

      /* Remember that we allocated this memory.  */
      group->next = G.page_groups;
      group->allocation = allocation;
      group->alloc_size = alloc_size;
      group->in_use = 0;
      G.page_groups = group;
      G.bytes_mapped += alloc_size;

      /* If we allocated multiple pages, put the rest on the free list.  */
      if (multiple_pages)
	{
	  struct page_entry *e, *f = G.free_pages;
	  for (a = enda - G.pagesize; a != page; a -= G.pagesize)
	    {
	      e = (struct page_entry *) xcalloc (1, page_entry_size);
	      e->order = order;
	      e->bytes = G.pagesize;
	      e->page = a;
	      e->group = group;
	      e->next = f;
	      f = e;
	    }
	  G.free_pages = f;
	}
    }
#endif

  if (entry == NULL)
    entry = (struct page_entry *) xcalloc (1, page_entry_size);

  entry->bytes = entry_size;
  entry->page = page;
  entry->context_depth = G.context_depth;
  entry->order = order;
  entry->num_free_objects = num_objects;
  entry->next_bit_hint = 1;

  G.context_depth_allocations |= (unsigned long)1 << G.context_depth;

#ifdef USING_MALLOC_PAGE_GROUPS
  entry->group = group;
  set_page_group_in_use (group, page);
#endif

  /* Set the one-past-the-end in-use bit.  This acts as a sentry as we
     increment the hint.  */
  entry->in_use_p[num_objects / HOST_BITS_PER_LONG]
    = (unsigned long) 1 << (num_objects % HOST_BITS_PER_LONG);

  set_page_table_entry (page, entry);

  if (GGC_DEBUG_LEVEL >= 2)
    fprintf (G.debug_file,
	     "Allocating page at %p, object size=%lu, data %p-%p\n",
	     (PTR) entry, (unsigned long) OBJECT_SIZE (order), page,
	     page + entry_size - 1);

  return entry;
}

/* Adjust the size of G.depth so that no index greater than the one
   used by the top of the G.by_depth is used.  */

static inline void
adjust_depth ()
{
  page_entry *top;

  if (G.by_depth_in_use)
    {
      top = G.by_depth[G.by_depth_in_use-1];

      /* Peel back indicies in depth that index into by_depth, so that
	 as new elements are added to by_depth, we note the indicies
	 of those elements, if they are for new context depths.  */
      while (G.depth_in_use > (size_t)top->context_depth+1)
	--G.depth_in_use;
    }
}

/* For a page that is no longer needed, put it on the free page list.  */

static inline void
free_page (entry)
     page_entry *entry;
{
  if (GGC_DEBUG_LEVEL >= 2)
    fprintf (G.debug_file,
	     "Deallocating page at %p, data %p-%p\n", (PTR) entry,
	     entry->page, entry->page + entry->bytes - 1);

  /* Mark the page as inaccessible.  Discard the handle to avoid handle
     leak.  */
  VALGRIND_DISCARD (VALGRIND_MAKE_NOACCESS (entry->page, entry->bytes));

  set_page_table_entry (entry->page, NULL);

#ifdef USING_MALLOC_PAGE_GROUPS
  clear_page_group_in_use (entry->group, entry->page);
#endif

  if (G.by_depth_in_use > 1)
    {
      page_entry *top = G.by_depth[G.by_depth_in_use-1];

      /* If they are at the same depth, put top element into freed
	 slot.  */
      if (entry->context_depth == top->context_depth)
	{
	  int i = entry->index_by_depth;
	  G.by_depth[i] = top;
	  G.save_in_use[i] = G.save_in_use[G.by_depth_in_use-1];
	  top->index_by_depth = i;
	}
      else
	{
	  /* We cannot free a page from a context deeper than the
	     current one.  */
	  abort ();
	}
    }
  --G.by_depth_in_use;

  adjust_depth ();

  entry->next = G.free_pages;
  G.free_pages = entry;
}

/* Release the free page cache to the system.  */

static void
release_pages ()
{
#ifdef USING_MMAP
  page_entry *p, *next;
  char *start;
  size_t len;

  /* Gather up adjacent pages so they are unmapped together.  */
  p = G.free_pages;

  while (p)
    {
      start = p->page;
      next = p->next;
      len = p->bytes;
      free (p);
      p = next;

      while (p && p->page == start + len)
	{
	  next = p->next;
	  len += p->bytes;
	  free (p);
	  p = next;
	}

      munmap (start, len);
      G.bytes_mapped -= len;
    }

  G.free_pages = NULL;
#endif
#ifdef USING_MALLOC_PAGE_GROUPS
  page_entry **pp, *p;
  page_group **gp, *g;

  /* Remove all pages from free page groups from the list.  */
  pp = &G.free_pages;
  while ((p = *pp) != NULL)
    if (p->group->in_use == 0)
      {
	*pp = p->next;
	free (p);
      }
    else
      pp = &p->next;

  /* Remove all free page groups, and release the storage.  */
  gp = &G.page_groups;
  while ((g = *gp) != NULL)
    if (g->in_use == 0)
      {
	*gp = g->next;
	G.bytes_mapped -= g->alloc_size;
	free (g->allocation);
      }
    else
      gp = &g->next;
#endif
}

/* This table provides a fast way to determine ceil(log_2(size)) for
   allocation requests.  The minimum allocation size is eight bytes.  */

static unsigned char size_lookup[257] =
{
  3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4,
  4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8
};

/* Allocate a chunk of memory of SIZE bytes.  If ZERO is nonzero, the
   memory is zeroed; otherwise, its contents are undefined.  */

void *
ggc_alloc (size)
     size_t size;
{
  unsigned order, word, bit, object_offset;
  struct page_entry *entry;
  void *result;

  if (size <= 256)
    order = size_lookup[size];
  else
    {
      order = 9;
      while (size > OBJECT_SIZE (order))
	order++;
    }

  /* If there are non-full pages for this size allocation, they are at
     the head of the list.  */
  entry = G.pages[order];

  /* If there is no page for this object size, or all pages in this
     context are full, allocate a new page.  */
  if (entry == NULL || entry->num_free_objects == 0)
    {
      struct page_entry *new_entry;
      new_entry = alloc_page (order);

      new_entry->index_by_depth = G.by_depth_in_use;
      push_by_depth (new_entry, 0);

      /* We can skip context depths, if we do, make sure we go all the
	 way to the new depth.  */
      while (new_entry->context_depth >= G.depth_in_use)
	push_depth (G.by_depth_in_use-1);

      /* If this is the only entry, it's also the tail.  */
      if (entry == NULL)
	G.page_tails[order] = new_entry;

      /* Put new pages at the head of the page list.  */
      new_entry->next = entry;
      entry = new_entry;
      G.pages[order] = new_entry;

      /* For a new page, we know the word and bit positions (in the
	 in_use bitmap) of the first available object -- they're zero.  */
      new_entry->next_bit_hint = 1;
      word = 0;
      bit = 0;
      object_offset = 0;
    }
  else
    {
      /* First try to use the hint left from the previous allocation
	 to locate a clear bit in the in-use bitmap.  We've made sure
	 that the one-past-the-end bit is always set, so if the hint
	 has run over, this test will fail.  */
      unsigned hint = entry->next_bit_hint;
      word = hint / HOST_BITS_PER_LONG;
      bit = hint % HOST_BITS_PER_LONG;

      /* If the hint didn't work, scan the bitmap from the beginning.  */
      if ((entry->in_use_p[word] >> bit) & 1)
	{
	  word = bit = 0;
	  while (~entry->in_use_p[word] == 0)
	    ++word;
	  while ((entry->in_use_p[word] >> bit) & 1)
	    ++bit;
	  hint = word * HOST_BITS_PER_LONG + bit;
	}

      /* Next time, try the next bit.  */
      entry->next_bit_hint = hint + 1;

      object_offset = hint * OBJECT_SIZE (order);
    }

  /* Set the in-use bit.  */
  entry->in_use_p[word] |= ((unsigned long) 1 << bit);

  /* Keep a running total of the number of free objects.  If this page
     fills up, we may have to move it to the end of the list if the
     next page isn't full.  If the next page is full, all subsequent
     pages are full, so there's no need to move it.  */
  if (--entry->num_free_objects == 0
      && entry->next != NULL
      && entry->next->num_free_objects > 0)
    {
      G.pages[order] = entry->next;
      entry->next = NULL;
      G.page_tails[order]->next = entry;
      G.page_tails[order] = entry;
    }

  /* Calculate the object's address.  */
  result = entry->page + object_offset;

#ifdef ENABLE_GC_CHECKING
  /* Keep poisoning-by-writing-0xaf the object, in an attempt to keep the
     exact same semantics in presence of memory bugs, regardless of
     ENABLE_VALGRIND_CHECKING.  We override this request below.  Drop the
     handle to avoid handle leak.  */
  VALGRIND_DISCARD (VALGRIND_MAKE_WRITABLE (result, OBJECT_SIZE (order)));

  /* `Poison' the entire allocated object, including any padding at
     the end.  */
  memset (result, 0xaf, OBJECT_SIZE (order));

  /* Make the bytes after the end of the object unaccessible.  Discard the
     handle to avoid handle leak.  */
  VALGRIND_DISCARD (VALGRIND_MAKE_NOACCESS ((char *) result + size,
					    OBJECT_SIZE (order) - size));
#endif

  /* Tell Valgrind that the memory is there, but its content isn't
     defined.  The bytes at the end of the object are still marked
     unaccessible.  */
  VALGRIND_DISCARD (VALGRIND_MAKE_WRITABLE (result, size));

  /* Keep track of how many bytes are being allocated.  This
     information is used in deciding when to collect.  */
  G.allocated += OBJECT_SIZE (order);

  if (GGC_DEBUG_LEVEL >= 3)
    fprintf (G.debug_file,
	     "Allocating object, requested size=%lu, actual=%lu at %p on %p\n",
	     (unsigned long) size, (unsigned long) OBJECT_SIZE (order), result,
	     (PTR) entry);

  return result;
}

/* If P is not marked, marks it and return false.  Otherwise return true.
   P must have been allocated by the GC allocator; it mustn't point to
   static objects, stack variables, or memory allocated with malloc.  */

int
ggc_set_mark (p)
     const void *p;
{
  page_entry *entry;
  unsigned bit, word;
  unsigned long mask;

  /* Look up the page on which the object is alloced.  If the object
     wasn't allocated by the collector, we'll probably die.  */
  entry = lookup_page_table_entry (p);
#ifdef ENABLE_CHECKING
  if (entry == NULL)
    abort ();
#endif

  /* Calculate the index of the object on the page; this is its bit
     position in the in_use_p bitmap.  */
  bit = OFFSET_TO_BIT (((const char *) p) - entry->page, entry->order);
  word = bit / HOST_BITS_PER_LONG;
  mask = (unsigned long) 1 << (bit % HOST_BITS_PER_LONG);

  /* If the bit was previously set, skip it.  */
  if (entry->in_use_p[word] & mask)
    return 1;

  /* Otherwise set it, and decrement the free object count.  */
  entry->in_use_p[word] |= mask;
  entry->num_free_objects -= 1;

  if (GGC_DEBUG_LEVEL >= 4)
    fprintf (G.debug_file, "Marking %p\n", p);

  return 0;
}

/* Return 1 if P has been marked, zero otherwise.
   P must have been allocated by the GC allocator; it mustn't point to
   static objects, stack variables, or memory allocated with malloc.  */

int
ggc_marked_p (p)
     const void *p;
{
  page_entry *entry;
  unsigned bit, word;
  unsigned long mask;

  /* Look up the page on which the object is alloced.  If the object
     wasn't allocated by the collector, we'll probably die.  */
  entry = lookup_page_table_entry (p);
#ifdef ENABLE_CHECKING
  if (entry == NULL)
    abort ();
#endif

  /* Calculate the index of the object on the page; this is its bit
     position in the in_use_p bitmap.  */
  bit = OFFSET_TO_BIT (((const char *) p) - entry->page, entry->order);
  word = bit / HOST_BITS_PER_LONG;
  mask = (unsigned long) 1 << (bit % HOST_BITS_PER_LONG);

  return (entry->in_use_p[word] & mask) != 0;
}

/* Return the size of the gc-able object P.  */

size_t
ggc_get_size (p)
     const void *p;
{
  page_entry *pe = lookup_page_table_entry (p);
  return OBJECT_SIZE (pe->order);
}

/* Subroutine of init_ggc which computes the pair of numbers used to
   perform division by OBJECT_SIZE (order) and fills in inverse_table[].

   This algorithm is taken from Granlund and Montgomery's paper
   "Division by Invariant Integers using Multiplication"
   (Proc. SIGPLAN PLDI, 1994), section 9 (Exact division by
   constants).  */

static void
compute_inverse (order)
     unsigned order;
{
  unsigned size, inv, e;

  /* There can be only one object per "page" in a bucket for sizes
     larger than half a machine page; it will always have offset zero.  */
  if (OBJECT_SIZE (order) > G.pagesize/2)
    {
      if (OBJECTS_PER_PAGE (order) != 1)
	abort ();

      DIV_MULT (order) = 1;
      DIV_SHIFT (order) = 0;
      return;
    }

  size = OBJECT_SIZE (order);
  e = 0;
  while (size % 2 == 0)
    {
      e++;
      size >>= 1;
    }

  inv = size;
  while (inv * size != 1)
    inv = inv * (2 - inv*size);

  DIV_MULT (order) = inv;
  DIV_SHIFT (order) = e;
}

/* Initialize the ggc-mmap allocator.  */
void
init_ggc ()
{
  unsigned order;

  G.pagesize = getpagesize();
  G.lg_pagesize = exact_log2 (G.pagesize);

#ifdef HAVE_MMAP_DEV_ZERO
  G.dev_zero_fd = open ("/dev/zero", O_RDONLY);
  if (G.dev_zero_fd == -1)
    abort ();
#endif

#if 0
  G.debug_file = fopen ("ggc-mmap.debug", "w");
#else
  G.debug_file = stdout;
#endif

#ifdef USING_MMAP
  /* StunOS has an amazing off-by-one error for the first mmap allocation
     after fiddling with RLIMIT_STACK.  The result, as hard as it is to
     believe, is an unaligned page allocation, which would cause us to
     hork badly if we tried to use it.  */
  {
    char *p = alloc_anon (NULL, G.pagesize);
    struct page_entry *e;
    if ((size_t)p & (G.pagesize - 1))
      {
	/* How losing.  Discard this one and try another.  If we still
	   can't get something useful, give up.  */

	p = alloc_anon (NULL, G.pagesize);
	if ((size_t)p & (G.pagesize - 1))
	  abort ();
      }

    /* We have a good page, might as well hold onto it...  */
    e = (struct page_entry *) xcalloc (1, sizeof (struct page_entry));
    e->bytes = G.pagesize;
    e->page = p;
    e->next = G.free_pages;
    G.free_pages = e;
  }
#endif

  /* Initialize the object size table.  */
  for (order = 0; order < HOST_BITS_PER_PTR; ++order)
    object_size_table[order] = (size_t) 1 << order;
  for (order = HOST_BITS_PER_PTR; order < NUM_ORDERS; ++order)
    {
      size_t s = extra_order_size_table[order - HOST_BITS_PER_PTR];

      /* If S is not a multiple of the MAX_ALIGNMENT, then round it up
	 so that we're sure of getting aligned memory.  */
      s = CEIL (s, MAX_ALIGNMENT) * MAX_ALIGNMENT;
      object_size_table[order] = s;
    }

  /* Initialize the objects-per-page and inverse tables.  */
  for (order = 0; order < NUM_ORDERS; ++order)
    {
      objects_per_page_table[order] = G.pagesize / OBJECT_SIZE (order);
      if (objects_per_page_table[order] == 0)
	objects_per_page_table[order] = 1;
      compute_inverse (order);
    }

  /* Reset the size_lookup array to put appropriately sized objects in
     the special orders.  All objects bigger than the previous power
     of two, but no greater than the special size, should go in the
     new order.  */
  for (order = HOST_BITS_PER_PTR; order < NUM_ORDERS; ++order)
    {
      int o;
      int i;

      o = size_lookup[OBJECT_SIZE (order)];
      for (i = OBJECT_SIZE (order); size_lookup [i] == o; --i)
	size_lookup[i] = order;
    }

  G.depth_in_use = 0;
  G.depth_max = 10;
  G.depth = (unsigned int *) xmalloc (G.depth_max * sizeof (unsigned int));

  G.by_depth_in_use = 0;
  G.by_depth_max = INITIAL_PTE_COUNT;
  G.by_depth = (page_entry **) xmalloc (G.by_depth_max * sizeof (page_entry *));
  G.save_in_use = (unsigned long **) xmalloc (G.by_depth_max * sizeof (unsigned long *));
}

/* Increment the `GC context'.  Objects allocated in an outer context
   are never freed, eliminating the need to register their roots.  */

void
ggc_push_context ()
{
  ++G.context_depth;

  /* Die on wrap.  */
  if (G.context_depth >= HOST_BITS_PER_LONG)
    abort ();
}

/* Merge the SAVE_IN_USE_P and IN_USE_P arrays in P so that IN_USE_P
   reflects reality.  Recalculate NUM_FREE_OBJECTS as well.  */

static void
ggc_recalculate_in_use_p (p)
     page_entry *p;
{
  unsigned int i;
  size_t num_objects;

  /* Because the past-the-end bit in in_use_p is always set, we
     pretend there is one additional object.  */
  num_objects = OBJECTS_PER_PAGE (p->order) + 1;

  /* Reset the free object count.  */
  p->num_free_objects = num_objects;

  /* Combine the IN_USE_P and SAVE_IN_USE_P arrays.  */
  for (i = 0;
       i < CEIL (BITMAP_SIZE (num_objects),
		 sizeof (*p->in_use_p));
       ++i)
    {
      unsigned long j;

      /* Something is in use if it is marked, or if it was in use in a
	 context further down the context stack.  */
      p->in_use_p[i] |= save_in_use_p (p)[i];

      /* Decrement the free object count for every object allocated.  */
      for (j = p->in_use_p[i]; j; j >>= 1)
	p->num_free_objects -= (j & 1);
    }

  if (p->num_free_objects >= num_objects)
    abort ();
}

/* Decrement the `GC context'.  All objects allocated since the
   previous ggc_push_context are migrated to the outer context.  */

void
ggc_pop_context ()
{
  unsigned long omask;
  unsigned int depth, i, e;
#ifdef ENABLE_CHECKING
  unsigned int order;
#endif

  depth = --G.context_depth;
  omask = (unsigned long)1 << (depth + 1);

  if (!((G.context_depth_allocations | G.context_depth_collections) & omask))
    return;

  G.context_depth_allocations |= (G.context_depth_allocations & omask) >> 1;
  G.context_depth_allocations &= omask - 1;
  G.context_depth_collections &= omask - 1;

  /* The G.depth array is shortend so that the last index is the
     context_depth of the top element of by_depth.  */
  if (depth+1 < G.depth_in_use)
    e = G.depth[depth+1];
  else
    e = G.by_depth_in_use;

  /* We might not have any PTEs of depth depth.  */
  if (depth < G.depth_in_use)
    {    

      /* First we go through all the pages at depth depth to
	 recalculate the in use bits.  */
      for (i = G.depth[depth]; i < e; ++i)
	{
	  page_entry *p;

#ifdef ENABLE_CHECKING
	  p = G.by_depth[i];

	  /* Check that all of the pages really are at the depth that
	     we expect.  */
	  if (p->context_depth != depth)
	    abort ();
	  if (p->index_by_depth != i)
	    abort ();
#endif

	  prefetch (&save_in_use_p_i (i+8));
	  prefetch (&save_in_use_p_i (i+16));
	  if (save_in_use_p_i (i))
	    {
	      p = G.by_depth[i];
	      ggc_recalculate_in_use_p (p);
	      free (save_in_use_p_i (i));
	      save_in_use_p_i (i) = 0;
	    }
	}
    }

  /* Then, we reset all page_entries with a depth greater than depth
     to be at depth.  */
  for (i = e; i < G.by_depth_in_use; ++i)
    {
      page_entry *p = G.by_depth[i];

      /* Check that all of the pages really are at the depth we
	 expect.  */
#ifdef ENABLE_CHECKING
      if (p->context_depth <= depth)
	abort ();
      if (p->index_by_depth != i)
	abort ();
#endif
      p->context_depth = depth;
    }

  adjust_depth ();

#ifdef ENABLE_CHECKING
  for (order = 2; order < NUM_ORDERS; order++)
    {
      page_entry *p;

      for (p = G.pages[order]; p != NULL; p = p->next)
	{
	  if (p->context_depth > depth)
	    abort ();
	  else if (p->context_depth == depth && save_in_use_p (p))
	    abort ();
	}
    }
#endif
}

/* Unmark all objects.  */

static inline void
clear_marks ()
{
  unsigned order;

  for (order = 2; order < NUM_ORDERS; order++)
    {
      size_t num_objects = OBJECTS_PER_PAGE (order);
      size_t bitmap_size = BITMAP_SIZE (num_objects + 1);
      page_entry *p;

      for (p = G.pages[order]; p != NULL; p = p->next)
	{
#ifdef ENABLE_CHECKING
	  /* The data should be page-aligned.  */
	  if ((size_t) p->page & (G.pagesize - 1))
	    abort ();
#endif

	  /* Pages that aren't in the topmost context are not collected;
	     nevertheless, we need their in-use bit vectors to store GC
	     marks.  So, back them up first.  */
	  if (p->context_depth < G.context_depth)
	    {
	      if (! save_in_use_p (p))
		save_in_use_p (p) = xmalloc (bitmap_size);
	      memcpy (save_in_use_p (p), p->in_use_p, bitmap_size);
	    }

	  /* Reset reset the number of free objects and clear the
             in-use bits.  These will be adjusted by mark_obj.  */
	  p->num_free_objects = num_objects;
	  memset (p->in_use_p, 0, bitmap_size);

	  /* Make sure the one-past-the-end bit is always set.  */
	  p->in_use_p[num_objects / HOST_BITS_PER_LONG]
	    = ((unsigned long) 1 << (num_objects % HOST_BITS_PER_LONG));
	}
    }
}

/* Free all empty pages.  Partially empty pages need no attention
   because the `mark' bit doubles as an `unused' bit.  */

static inline void
sweep_pages ()
{
  unsigned order;

  for (order = 2; order < NUM_ORDERS; order++)
    {
      /* The last page-entry to consider, regardless of entries
	 placed at the end of the list.  */
      page_entry * const last = G.page_tails[order];

      size_t num_objects = OBJECTS_PER_PAGE (order);
      size_t live_objects;
      page_entry *p, *previous;
      int done;

      p = G.pages[order];
      if (p == NULL)
	continue;

      previous = NULL;
      do
	{
	  page_entry *next = p->next;

	  /* Loop until all entries have been examined.  */
	  done = (p == last);

	  /* Add all live objects on this page to the count of
             allocated memory.  */
	  live_objects = num_objects - p->num_free_objects;

	  G.allocated += OBJECT_SIZE (order) * live_objects;

	  /* Only objects on pages in the topmost context should get
	     collected.  */
	  if (p->context_depth < G.context_depth)
	    ;

	  /* Remove the page if it's empty.  */
	  else if (live_objects == 0)
	    {
	      if (! previous)
		G.pages[order] = next;
	      else
		previous->next = next;

	      /* Are we removing the last element?  */
	      if (p == G.page_tails[order])
		G.page_tails[order] = previous;
	      free_page (p);
	      p = previous;
	    }

	  /* If the page is full, move it to the end.  */
	  else if (p->num_free_objects == 0)
	    {
	      /* Don't move it if it's already at the end.  */
	      if (p != G.page_tails[order])
		{
		  /* Move p to the end of the list.  */
		  p->next = NULL;
		  G.page_tails[order]->next = p;

		  /* Update the tail pointer...  */
		  G.page_tails[order] = p;

		  /* ... and the head pointer, if necessary.  */
		  if (! previous)
		    G.pages[order] = next;
		  else
		    previous->next = next;
		  p = previous;
		}
	    }

	  /* If we've fallen through to here, it's a page in the
	     topmost context that is neither full nor empty.  Such a
	     page must precede pages at lesser context depth in the
	     list, so move it to the head.  */
	  else if (p != G.pages[order])
	    {
	      previous->next = p->next;
	      p->next = G.pages[order];
	      G.pages[order] = p;
	      /* Are we moving the last element?  */
	      if (G.page_tails[order] == p)
	        G.page_tails[order] = previous;
	      p = previous;
	    }

	  previous = p;
	  p = next;
	}
      while (! done);

      /* Now, restore the in_use_p vectors for any pages from contexts
         other than the current one.  */
      for (p = G.pages[order]; p; p = p->next)
	if (p->context_depth != G.context_depth)
	  ggc_recalculate_in_use_p (p);
    }
}

#ifdef ENABLE_GC_CHECKING
/* Clobber all free objects.  */

static inline void
poison_pages ()
{
  unsigned order;

  for (order = 2; order < NUM_ORDERS; order++)
    {
      size_t num_objects = OBJECTS_PER_PAGE (order);
      size_t size = OBJECT_SIZE (order);
      page_entry *p;

      for (p = G.pages[order]; p != NULL; p = p->next)
	{
	  size_t i;

	  if (p->context_depth != G.context_depth)
	    /* Since we don't do any collection for pages in pushed
	       contexts, there's no need to do any poisoning.  And
	       besides, the IN_USE_P array isn't valid until we pop
	       contexts.  */
	    continue;

	  for (i = 0; i < num_objects; i++)
	    {
	      size_t word, bit;
	      word = i / HOST_BITS_PER_LONG;
	      bit = i % HOST_BITS_PER_LONG;
	      if (((p->in_use_p[word] >> bit) & 1) == 0)
		{
		  char *object = p->page + i * size;

		  /* Keep poison-by-write when we expect to use Valgrind,
		     so the exact same memory semantics is kept, in case
		     there are memory errors.  We override this request
		     below.  */
		  VALGRIND_DISCARD (VALGRIND_MAKE_WRITABLE (object, size));
		  memset (object, 0xa5, size);

		  /* Drop the handle to avoid handle leak.  */
		  VALGRIND_DISCARD (VALGRIND_MAKE_NOACCESS (object, size));
		}
	    }
	}
    }
}
#endif

/* Top level mark-and-sweep routine.  */

void
ggc_collect ()
{
  /* Avoid frequent unnecessary work by skipping collection if the
     total allocations haven't expanded much since the last
     collection.  */
  float allocated_last_gc =
    MAX (G.allocated_last_gc, (size_t)PARAM_VALUE (GGC_MIN_HEAPSIZE) * 1024);

  float min_expand = allocated_last_gc * PARAM_VALUE (GGC_MIN_EXPAND) / 100;

  if (G.allocated < allocated_last_gc + min_expand)
    return;

  timevar_push (TV_GC);
  if (!quiet_flag)
    fprintf (stderr, " {GC %luk -> ", (unsigned long) G.allocated / 1024);

  /* Zero the total allocated bytes.  This will be recalculated in the
     sweep phase.  */
  G.allocated = 0;

  /* Release the pages we freed the last time we collected, but didn't
     reuse in the interim.  */
  release_pages ();

  /* Indicate that we've seen collections at this context depth.  */
  G.context_depth_collections = ((unsigned long)1 << (G.context_depth + 1)) - 1;

  clear_marks ();
  ggc_mark_roots ();

#ifdef ENABLE_GC_CHECKING
  poison_pages ();
#endif

  sweep_pages ();

  G.allocated_last_gc = G.allocated;

  timevar_pop (TV_GC);

  if (!quiet_flag)
    fprintf (stderr, "%luk}", (unsigned long) G.allocated / 1024);
}

/* Print allocation statistics.  */
#define SCALE(x) ((unsigned long) ((x) < 1024*10 \
		  ? (x) \
		  : ((x) < 1024*1024*10 \
		     ? (x) / 1024 \
		     : (x) / (1024*1024))))
#define LABEL(x) ((x) < 1024*10 ? ' ' : ((x) < 1024*1024*10 ? 'k' : 'M'))

void
ggc_print_statistics ()
{
  struct ggc_statistics stats;
  unsigned int i;
  size_t total_overhead = 0;

  /* Clear the statistics.  */
  memset (&stats, 0, sizeof (stats));

  /* Make sure collection will really occur.  */
  G.allocated_last_gc = 0;

  /* Collect and print the statistics common across collectors.  */
  ggc_print_common_statistics (stderr, &stats);

  /* Release free pages so that we will not count the bytes allocated
     there as part of the total allocated memory.  */
  release_pages ();

  /* Collect some information about the various sizes of
     allocation.  */
  fprintf (stderr, "\n%-5s %10s  %10s  %10s\n",
	   "Size", "Allocated", "Used", "Overhead");
  for (i = 0; i < NUM_ORDERS; ++i)
    {
      page_entry *p;
      size_t allocated;
      size_t in_use;
      size_t overhead;

      /* Skip empty entries.  */
      if (!G.pages[i])
	continue;

      overhead = allocated = in_use = 0;

      /* Figure out the total number of bytes allocated for objects of
	 this size, and how many of them are actually in use.  Also figure
	 out how much memory the page table is using.  */
      for (p = G.pages[i]; p; p = p->next)
	{
	  allocated += p->bytes;
	  in_use +=
	    (OBJECTS_PER_PAGE (i) - p->num_free_objects) * OBJECT_SIZE (i);

	  overhead += (sizeof (page_entry) - sizeof (long)
		       + BITMAP_SIZE (OBJECTS_PER_PAGE (i) + 1));
	}
      fprintf (stderr, "%-5lu %10lu%c %10lu%c %10lu%c\n",
	       (unsigned long) OBJECT_SIZE (i),
	       SCALE (allocated), LABEL (allocated),
	       SCALE (in_use), LABEL (in_use),
	       SCALE (overhead), LABEL (overhead));
      total_overhead += overhead;
    }
  fprintf (stderr, "%-5s %10lu%c %10lu%c %10lu%c\n", "Total",
	   SCALE (G.bytes_mapped), LABEL (G.bytes_mapped),
	   SCALE (G.allocated), LABEL(G.allocated),
	   SCALE (total_overhead), LABEL (total_overhead));
}
