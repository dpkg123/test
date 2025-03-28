/* Select target systems and architectures at runtime for GDB.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002
   Free Software Foundation, Inc.
   Contributed by Cygnus Support.

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
#include <errno.h>
#include "gdb_string.h"
#include "target.h"
#include "gdbcmd.h"
#include "symtab.h"
#include "inferior.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb_wait.h"
#include "dcache.h"
#include <signal.h>
#include "regcache.h"

extern int errno;

static void target_info (char *, int);

static void cleanup_target (struct target_ops *);

static void maybe_kill_then_create_inferior (char *, char *, char **);

static void default_clone_and_follow_inferior (int, int *);

static void maybe_kill_then_attach (char *, int);

static void kill_or_be_killed (int);

static void default_terminal_info (char *, int);

static int default_region_size_ok_for_hw_watchpoint (int);

static int nosymbol (char *, CORE_ADDR *);

static void tcomplain (void);

static int nomemory (CORE_ADDR, char *, int, int, struct target_ops *);

static int return_zero (void);

static int return_one (void);

static int return_minus_one (void);

void target_ignore (void);

static void target_command (char *, int);

static struct target_ops *find_default_run_target (char *);

static void update_current_target (void);

static void nosupport_runtime (void);

static void normal_target_post_startup_inferior (ptid_t ptid);

/* Transfer LEN bytes between target address MEMADDR and GDB address
   MYADDR.  Returns 0 for success, errno code for failure (which
   includes partial transfers -- if you want a more useful response to
   partial transfers, try either target_read_memory_partial or
   target_write_memory_partial).  */

static int
target_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len, int write);

static void init_dummy_target (void);

static void debug_to_open (char *, int);

static void debug_to_close (int);

static void debug_to_attach (char *, int);

static void debug_to_detach (char *, int);

static void debug_to_resume (ptid_t, int, enum target_signal);

static ptid_t debug_to_wait (ptid_t, struct target_waitstatus *);

static void debug_to_fetch_registers (int);

static void debug_to_store_registers (int);

static void debug_to_prepare_to_store (void);

static int debug_to_xfer_memory (CORE_ADDR, char *, int, int,
				 struct mem_attrib *, struct target_ops *);

static void debug_to_files_info (struct target_ops *);

static int debug_to_insert_breakpoint (CORE_ADDR, char *);

static int debug_to_remove_breakpoint (CORE_ADDR, char *);

static int debug_to_can_use_hw_breakpoint (int, int, int);

static int debug_to_insert_hw_breakpoint (CORE_ADDR, char *);

static int debug_to_remove_hw_breakpoint (CORE_ADDR, char *);

static int debug_to_insert_watchpoint (CORE_ADDR, int, int);

static int debug_to_remove_watchpoint (CORE_ADDR, int, int);

static int debug_to_stopped_by_watchpoint (void);

static CORE_ADDR debug_to_stopped_data_address (void);

static int debug_to_region_size_ok_for_hw_watchpoint (int);

static void debug_to_terminal_init (void);

static void debug_to_terminal_inferior (void);

static void debug_to_terminal_ours_for_output (void);

static void debug_to_terminal_save_ours (void);

static void debug_to_terminal_ours (void);

static void debug_to_terminal_info (char *, int);

static void debug_to_kill (void);

static void debug_to_load (char *, int);

static int debug_to_lookup_symbol (char *, CORE_ADDR *);

static void debug_to_create_inferior (char *, char *, char **);

static void debug_to_mourn_inferior (void);

static int debug_to_can_run (void);

static void debug_to_notice_signals (ptid_t);

static int debug_to_thread_alive (ptid_t);

static void debug_to_stop (void);

static int debug_to_query (int /*char */ , char *, char *, int *);

/* Pointer to array of target architecture structures; the size of the
   array; the current index into the array; the allocated size of the 
   array.  */
struct target_ops **target_structs;
unsigned target_struct_size;
unsigned target_struct_index;
unsigned target_struct_allocsize;
#define	DEFAULT_ALLOCSIZE	10

/* The initial current target, so that there is always a semi-valid
   current target.  */

static struct target_ops dummy_target;

/* Top of target stack.  */

struct target_stack_item *target_stack;

/* The target structure we are currently using to talk to a process
   or file or whatever "inferior" we have.  */

struct target_ops current_target;

/* Command list for target.  */

static struct cmd_list_element *targetlist = NULL;

/* Nonzero if we are debugging an attached outside process
   rather than an inferior.  */

int attach_flag;

/* Non-zero if we want to see trace of target level stuff.  */

static int targetdebug = 0;

static void setup_target_debug (void);

DCACHE *target_dcache;

/* The user just typed 'target' without the name of a target.  */

/* ARGSUSED */
static void
target_command (char *arg, int from_tty)
{
  fputs_filtered ("Argument required (target name).  Try `help target'\n",
		  gdb_stdout);
}

/* Add a possible target architecture to the list.  */

void
add_target (struct target_ops *t)
{
  if (!target_structs)
    {
      target_struct_allocsize = DEFAULT_ALLOCSIZE;
      target_structs = (struct target_ops **) xmalloc
	(target_struct_allocsize * sizeof (*target_structs));
    }
  if (target_struct_size >= target_struct_allocsize)
    {
      target_struct_allocsize *= 2;
      target_structs = (struct target_ops **)
	xrealloc ((char *) target_structs,
		  target_struct_allocsize * sizeof (*target_structs));
    }
  target_structs[target_struct_size++] = t;
/*  cleanup_target (t); */

  if (targetlist == NULL)
    add_prefix_cmd ("target", class_run, target_command,
		    "Connect to a target machine or process.\n\
The first argument is the type or protocol of the target machine.\n\
Remaining arguments are interpreted by the target protocol.  For more\n\
information on the arguments for a particular protocol, type\n\
`help target ' followed by the protocol name.",
		    &targetlist, "target ", 0, &cmdlist);
  add_cmd (t->to_shortname, no_class, t->to_open, t->to_doc, &targetlist);
}

/* Stub functions */

void
target_ignore (void)
{
}

void
target_load (char *arg, int from_tty)
{
  dcache_invalidate (target_dcache);
  (*current_target.to_load) (arg, from_tty);
}

/* ARGSUSED */
static int
nomemory (CORE_ADDR memaddr, char *myaddr, int len, int write,
	  struct target_ops *t)
{
  errno = EIO;			/* Can't read/write this location */
  return 0;			/* No bytes handled */
}

static void
tcomplain (void)
{
  error ("You can't do that when your target is `%s'",
	 current_target.to_shortname);
}

void
noprocess (void)
{
  error ("You can't do that without a process to debug.");
}

/* ARGSUSED */
static int
nosymbol (char *name, CORE_ADDR *addrp)
{
  return 1;			/* Symbol does not exist in target env */
}

/* ARGSUSED */
static void
nosupport_runtime (void)
{
  if (ptid_equal (inferior_ptid, null_ptid))
    noprocess ();
  else
    error ("No run-time support for this");
}


/* ARGSUSED */
static void
default_terminal_info (char *args, int from_tty)
{
  printf_unfiltered ("No saved terminal information.\n");
}

/* This is the default target_create_inferior and target_attach function.
   If the current target is executing, it asks whether to kill it off.
   If this function returns without calling error(), it has killed off
   the target, and the operation should be attempted.  */

static void
kill_or_be_killed (int from_tty)
{
  if (target_has_execution)
    {
      printf_unfiltered ("You are already running a program:\n");
      target_files_info ();
      if (query ("Kill it? "))
	{
	  target_kill ();
	  if (target_has_execution)
	    error ("Killing the program did not help.");
	  return;
	}
      else
	{
	  error ("Program not killed.");
	}
    }
  tcomplain ();
}

static void
maybe_kill_then_attach (char *args, int from_tty)
{
  kill_or_be_killed (from_tty);
  target_attach (args, from_tty);
}

static void
maybe_kill_then_create_inferior (char *exec, char *args, char **env)
{
  kill_or_be_killed (0);
  target_create_inferior (exec, args, env);
}

static void
default_clone_and_follow_inferior (int child_pid, int *followed_child)
{
  target_clone_and_follow_inferior (child_pid, followed_child);
}

/* Clean up a target struct so it no longer has any zero pointers in it.
   We default entries, at least to stubs that print error messages.  */

static void
cleanup_target (struct target_ops *t)
{

#define de_fault(field, value) \
  if (!t->field)               \
    t->field = value

  de_fault (to_open, 
	    (void (*) (char *, int)) 
	    tcomplain);
  de_fault (to_close, 
	    (void (*) (int)) 
	    target_ignore);
  de_fault (to_attach, 
	    maybe_kill_then_attach);
  de_fault (to_post_attach, 
	    (void (*) (int)) 
	    target_ignore);
  de_fault (to_require_attach, 
	    maybe_kill_then_attach);
  de_fault (to_detach, 
	    (void (*) (char *, int)) 
	    target_ignore);
  de_fault (to_require_detach, 
	    (void (*) (int, char *, int)) 
	    target_ignore);
  de_fault (to_resume, 
	    (void (*) (ptid_t, int, enum target_signal)) 
	    noprocess);
  de_fault (to_wait, 
	    (ptid_t (*) (ptid_t, struct target_waitstatus *)) 
	    noprocess);
  de_fault (to_post_wait, 
	    (void (*) (ptid_t, int)) 
	    target_ignore);
  de_fault (to_fetch_registers, 
	    (void (*) (int)) 
	    target_ignore);
  de_fault (to_store_registers, 
	    (void (*) (int)) 
	    noprocess);
  de_fault (to_prepare_to_store, 
	    (void (*) (void)) 
	    noprocess);
  de_fault (to_xfer_memory, 
	    (int (*) (CORE_ADDR, char *, int, int, struct mem_attrib *, struct target_ops *)) 
	    nomemory);
  de_fault (to_files_info, 
	    (void (*) (struct target_ops *)) 
	    target_ignore);
  de_fault (to_insert_breakpoint, 
	    memory_insert_breakpoint);
  de_fault (to_remove_breakpoint, 
	    memory_remove_breakpoint);
  de_fault (to_can_use_hw_breakpoint,
	    (int (*) (int, int, int))
	    return_zero);
  de_fault (to_insert_hw_breakpoint,
	    (int (*) (CORE_ADDR, char *))
	    return_minus_one);
  de_fault (to_remove_hw_breakpoint,
	    (int (*) (CORE_ADDR, char *))
	    return_minus_one);
  de_fault (to_insert_watchpoint,
	    (int (*) (CORE_ADDR, int, int))
	    return_minus_one);
  de_fault (to_remove_watchpoint,
	    (int (*) (CORE_ADDR, int, int))
	    return_minus_one);
  de_fault (to_stopped_by_watchpoint,
	    (int (*) (void))
	    return_zero);
  de_fault (to_stopped_data_address,
	    (CORE_ADDR (*) (void))
	    return_zero);
  de_fault (to_region_size_ok_for_hw_watchpoint,
	    default_region_size_ok_for_hw_watchpoint);
  de_fault (to_terminal_init, 
	    (void (*) (void)) 
	    target_ignore);
  de_fault (to_terminal_inferior, 
	    (void (*) (void)) 
	    target_ignore);
  de_fault (to_terminal_ours_for_output, 
	    (void (*) (void)) 
	    target_ignore);
  de_fault (to_terminal_ours, 
	    (void (*) (void)) 
	    target_ignore);
  de_fault (to_terminal_save_ours, 
	    (void (*) (void)) 
	    target_ignore);
  de_fault (to_terminal_info, 
	    default_terminal_info);
  de_fault (to_kill, 
	    (void (*) (void)) 
	    noprocess);
  de_fault (to_load, 
	    (void (*) (char *, int)) 
	    tcomplain);
  de_fault (to_lookup_symbol, 
	    (int (*) (char *, CORE_ADDR *)) 
	    nosymbol);
  de_fault (to_create_inferior, 
	    maybe_kill_then_create_inferior);
  de_fault (to_post_startup_inferior, 
	    (void (*) (ptid_t)) 
	    target_ignore);
  de_fault (to_acknowledge_created_inferior, 
	    (void (*) (int)) 
	    target_ignore);
  de_fault (to_clone_and_follow_inferior, 
	    default_clone_and_follow_inferior);
  de_fault (to_post_follow_inferior_by_clone, 
	    (void (*) (void)) 
	    target_ignore);
  de_fault (to_insert_fork_catchpoint, 
	    (int (*) (int)) 
	    tcomplain);
  de_fault (to_remove_fork_catchpoint, 
	    (int (*) (int)) 
	    tcomplain);
  de_fault (to_insert_vfork_catchpoint, 
	    (int (*) (int)) 
	    tcomplain);
  de_fault (to_remove_vfork_catchpoint, 
	    (int (*) (int)) 
	    tcomplain);
  de_fault (to_has_forked, 
	    (int (*) (int, int *)) 
	    return_zero);
  de_fault (to_has_vforked, 
	    (int (*) (int, int *)) 
	    return_zero);
  de_fault (to_can_follow_vfork_prior_to_exec, 
	    (int (*) (void)) 
	    return_zero);
  de_fault (to_post_follow_vfork, 
	    (void (*) (int, int, int, int)) 
	    target_ignore);
  de_fault (to_insert_exec_catchpoint, 
	    (int (*) (int)) 
	    tcomplain);
  de_fault (to_remove_exec_catchpoint, 
	    (int (*) (int)) 
	    tcomplain);
  de_fault (to_has_execd, 
	    (int (*) (int, char **)) 
	    return_zero);
  de_fault (to_reported_exec_events_per_exec_call, 
	    (int (*) (void)) 
	    return_one);
  de_fault (to_has_syscall_event, 
	    (int (*) (int, enum target_waitkind *, int *)) 
	    return_zero);
  de_fault (to_has_exited, 
	    (int (*) (int, int, int *)) 
	    return_zero);
  de_fault (to_mourn_inferior, 
	    (void (*) (void)) 
	    noprocess);
  de_fault (to_can_run, 
	    return_zero);
  de_fault (to_notice_signals, 
	    (void (*) (ptid_t)) 
	    target_ignore);
  de_fault (to_thread_alive, 
	    (int (*) (ptid_t)) 
	    return_zero);
  de_fault (to_find_new_threads, 
	    (void (*) (void)) 
	    target_ignore);
  de_fault (to_extra_thread_info, 
	    (char *(*) (struct thread_info *)) 
	    return_zero);
  de_fault (to_stop, 
	    (void (*) (void)) 
	    target_ignore);
  de_fault (to_rcmd, 
	    (void (*) (char *, struct ui_file *)) 
	    tcomplain);
  de_fault (to_enable_exception_callback, 
	    (struct symtab_and_line * (*) (enum exception_event_kind, int)) 
	    nosupport_runtime);
  de_fault (to_get_current_exception_event, 
	    (struct exception_event_record * (*) (void)) 
	    nosupport_runtime);
  de_fault (to_pid_to_exec_file, 
	    (char *(*) (int)) 
	    return_zero);
  de_fault (to_can_async_p, 
	    (int (*) (void)) 
	    return_zero);
  de_fault (to_is_async_p, 
	    (int (*) (void)) 
	    return_zero);
  de_fault (to_async, 
	    (void (*) (void (*) (enum inferior_event_type, void*), void*)) 
	    tcomplain);
#undef de_fault
}

/* Go through the target stack from top to bottom, copying over zero entries in
   current_target.  In effect, we are doing class inheritance through the
   pushed target vectors.  */

static void
update_current_target (void)
{
  struct target_stack_item *item;
  struct target_ops *t;

  /* First, reset current_target */
  memset (&current_target, 0, sizeof current_target);

  for (item = target_stack; item; item = item->next)
    {
      t = item->target_ops;

#define INHERIT(FIELD, TARGET) \
      if (!current_target.FIELD) \
	current_target.FIELD = TARGET->FIELD

      INHERIT (to_shortname, t);
      INHERIT (to_longname, t);
      INHERIT (to_doc, t);
      INHERIT (to_open, t);
      INHERIT (to_close, t);
      INHERIT (to_attach, t);
      INHERIT (to_post_attach, t);
      INHERIT (to_require_attach, t);
      INHERIT (to_detach, t);
      INHERIT (to_require_detach, t);
      INHERIT (to_resume, t);
      INHERIT (to_wait, t);
      INHERIT (to_post_wait, t);
      INHERIT (to_fetch_registers, t);
      INHERIT (to_store_registers, t);
      INHERIT (to_prepare_to_store, t);
      INHERIT (to_xfer_memory, t);
      INHERIT (to_files_info, t);
      INHERIT (to_insert_breakpoint, t);
      INHERIT (to_remove_breakpoint, t);
      INHERIT (to_can_use_hw_breakpoint, t);
      INHERIT (to_insert_hw_breakpoint, t);
      INHERIT (to_remove_hw_breakpoint, t);
      INHERIT (to_insert_watchpoint, t);
      INHERIT (to_remove_watchpoint, t);
      INHERIT (to_stopped_data_address, t);
      INHERIT (to_stopped_by_watchpoint, t);
      INHERIT (to_region_size_ok_for_hw_watchpoint, t);
      INHERIT (to_terminal_init, t);
      INHERIT (to_terminal_inferior, t);
      INHERIT (to_terminal_ours_for_output, t);
      INHERIT (to_terminal_ours, t);
      INHERIT (to_terminal_save_ours, t);
      INHERIT (to_terminal_info, t);
      INHERIT (to_kill, t);
      INHERIT (to_load, t);
      INHERIT (to_lookup_symbol, t);
      INHERIT (to_create_inferior, t);
      INHERIT (to_post_startup_inferior, t);
      INHERIT (to_acknowledge_created_inferior, t);
      INHERIT (to_clone_and_follow_inferior, t);
      INHERIT (to_post_follow_inferior_by_clone, t);
      INHERIT (to_insert_fork_catchpoint, t);
      INHERIT (to_remove_fork_catchpoint, t);
      INHERIT (to_insert_vfork_catchpoint, t);
      INHERIT (to_remove_vfork_catchpoint, t);
      INHERIT (to_has_forked, t);
      INHERIT (to_has_vforked, t);
      INHERIT (to_can_follow_vfork_prior_to_exec, t);
      INHERIT (to_post_follow_vfork, t);
      INHERIT (to_insert_exec_catchpoint, t);
      INHERIT (to_remove_exec_catchpoint, t);
      INHERIT (to_has_execd, t);
      INHERIT (to_reported_exec_events_per_exec_call, t);
      INHERIT (to_has_syscall_event, t);
      INHERIT (to_has_exited, t);
      INHERIT (to_mourn_inferior, t);
      INHERIT (to_can_run, t);
      INHERIT (to_notice_signals, t);
      INHERIT (to_thread_alive, t);
      INHERIT (to_find_new_threads, t);
      INHERIT (to_pid_to_str, t);
      INHERIT (to_extra_thread_info, t);
      INHERIT (to_stop, t);
      INHERIT (to_query, t);
      INHERIT (to_rcmd, t);
      INHERIT (to_enable_exception_callback, t);
      INHERIT (to_get_current_exception_event, t);
      INHERIT (to_pid_to_exec_file, t);
      INHERIT (to_stratum, t);
      INHERIT (DONT_USE, t);
      INHERIT (to_has_all_memory, t);
      INHERIT (to_has_memory, t);
      INHERIT (to_has_stack, t);
      INHERIT (to_has_registers, t);
      INHERIT (to_has_execution, t);
      INHERIT (to_has_thread_control, t);
      INHERIT (to_sections, t);
      INHERIT (to_sections_end, t);
      INHERIT (to_can_async_p, t);
      INHERIT (to_is_async_p, t);
      INHERIT (to_async, t);
      INHERIT (to_async_mask_value, t);
      INHERIT (to_find_memory_regions, t);
      INHERIT (to_make_corefile_notes, t);
      INHERIT (to_magic, t);

#undef INHERIT
    }
}

/* Push a new target type into the stack of the existing target accessors,
   possibly superseding some of the existing accessors.

   Result is zero if the pushed target ended up on top of the stack,
   nonzero if at least one target is on top of it.

   Rather than allow an empty stack, we always have the dummy target at
   the bottom stratum, so we can call the function vectors without
   checking them.  */

int
push_target (struct target_ops *t)
{
  struct target_stack_item *cur, *prev, *tmp;

  /* Check magic number.  If wrong, it probably means someone changed
     the struct definition, but not all the places that initialize one.  */
  if (t->to_magic != OPS_MAGIC)
    {
      fprintf_unfiltered (gdb_stderr,
			  "Magic number of %s target struct wrong\n",
			  t->to_shortname);
      internal_error (__FILE__, __LINE__, "failed internal consistency check");
    }

  /* Find the proper stratum to install this target in. */

  for (prev = NULL, cur = target_stack; cur; prev = cur, cur = cur->next)
    {
      if ((int) (t->to_stratum) >= (int) (cur->target_ops->to_stratum))
	break;
    }

  /* If there's already targets at this stratum, remove them. */

  if (cur)
    while (t->to_stratum == cur->target_ops->to_stratum)
      {
	/* There's already something on this stratum.  Close it off.  */
	if (cur->target_ops->to_close)
	  (cur->target_ops->to_close) (0);
	if (prev)
	  prev->next = cur->next;	/* Unchain old target_ops */
	else
	  target_stack = cur->next;	/* Unchain first on list */
	tmp = cur->next;
	xfree (cur);
	cur = tmp;
      }

  /* We have removed all targets in our stratum, now add the new one.  */

  tmp = (struct target_stack_item *)
    xmalloc (sizeof (struct target_stack_item));
  tmp->next = cur;
  tmp->target_ops = t;

  if (prev)
    prev->next = tmp;
  else
    target_stack = tmp;

  update_current_target ();

  cleanup_target (&current_target);	/* Fill in the gaps */

  if (targetdebug)
    setup_target_debug ();

  return prev != 0;
}

/* Remove a target_ops vector from the stack, wherever it may be. 
   Return how many times it was removed (0 or 1).  */

int
unpush_target (struct target_ops *t)
{
  struct target_stack_item *cur, *prev;

  if (t->to_close)
    t->to_close (0);		/* Let it clean up */

  /* Look for the specified target.  Note that we assume that a target
     can only occur once in the target stack. */

  for (cur = target_stack, prev = NULL; cur; prev = cur, cur = cur->next)
    if (cur->target_ops == t)
      break;

  if (!cur)
    return 0;			/* Didn't find target_ops, quit now */

  /* Unchain the target */

  if (!prev)
    target_stack = cur->next;
  else
    prev->next = cur->next;

  xfree (cur);			/* Release the target_stack_item */

  update_current_target ();
  cleanup_target (&current_target);

  return 1;
}

void
pop_target (void)
{
  (current_target.to_close) (0);	/* Let it clean up */
  if (unpush_target (target_stack->target_ops) == 1)
    return;

  fprintf_unfiltered (gdb_stderr,
		      "pop_target couldn't find target %s\n",
		      current_target.to_shortname);
  internal_error (__FILE__, __LINE__, "failed internal consistency check");
}

#undef	MIN
#define MIN(A, B) (((A) <= (B)) ? (A) : (B))

/* target_read_string -- read a null terminated string, up to LEN bytes,
   from MEMADDR in target.  Set *ERRNOP to the errno code, or 0 if successful.
   Set *STRING to a pointer to malloc'd memory containing the data; the caller
   is responsible for freeing it.  Return the number of bytes successfully
   read.  */

int
target_read_string (CORE_ADDR memaddr, char **string, int len, int *errnop)
{
  int tlen, origlen, offset, i;
  char buf[4];
  int errcode = 0;
  char *buffer;
  int buffer_allocated;
  char *bufptr;
  unsigned int nbytes_read = 0;

  /* Small for testing.  */
  buffer_allocated = 4;
  buffer = xmalloc (buffer_allocated);
  bufptr = buffer;

  origlen = len;

  while (len > 0)
    {
      tlen = MIN (len, 4 - (memaddr & 3));
      offset = memaddr & 3;

      errcode = target_xfer_memory (memaddr & ~3, buf, 4, 0);
      if (errcode != 0)
	{
	  /* The transfer request might have crossed the boundary to an
	     unallocated region of memory. Retry the transfer, requesting
	     a single byte.  */
	  tlen = 1;
	  offset = 0;
	  errcode = target_xfer_memory (memaddr, buf, 1, 0);
	  if (errcode != 0)
	    goto done;
	}

      if (bufptr - buffer + tlen > buffer_allocated)
	{
	  unsigned int bytes;
	  bytes = bufptr - buffer;
	  buffer_allocated *= 2;
	  buffer = xrealloc (buffer, buffer_allocated);
	  bufptr = buffer + bytes;
	}

      for (i = 0; i < tlen; i++)
	{
	  *bufptr++ = buf[i + offset];
	  if (buf[i + offset] == '\000')
	    {
	      nbytes_read += i + 1;
	      goto done;
	    }
	}

      memaddr += tlen;
      len -= tlen;
      nbytes_read += tlen;
    }
done:
  if (errnop != NULL)
    *errnop = errcode;
  if (string != NULL)
    *string = buffer;
  return nbytes_read;
}

/* Read LEN bytes of target memory at address MEMADDR, placing the results in
   GDB's memory at MYADDR.  Returns either 0 for success or an errno value
   if any error occurs.

   If an error occurs, no guarantee is made about the contents of the data at
   MYADDR.  In particular, the caller should not depend upon partial reads
   filling the buffer with good data.  There is no way for the caller to know
   how much good data might have been transfered anyway.  Callers that can
   deal with partial reads should call target_read_memory_partial. */

int
target_read_memory (CORE_ADDR memaddr, char *myaddr, int len)
{
  return target_xfer_memory (memaddr, myaddr, len, 0);
}

int
target_write_memory (CORE_ADDR memaddr, char *myaddr, int len)
{
  return target_xfer_memory (memaddr, myaddr, len, 1);
}

static int trust_readonly = 0;

/* Move memory to or from the targets.  The top target gets priority;
   if it cannot handle it, it is offered to the next one down, etc.

   Result is -1 on error, or the number of bytes transfered.  */

int
do_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len, int write,
		struct mem_attrib *attrib)
{
  int res;
  int done = 0;
  struct target_ops *t;
  struct target_stack_item *item;

  /* Zero length requests are ok and require no work.  */
  if (len == 0)
    return 0;

  /* to_xfer_memory is not guaranteed to set errno, even when it returns
     0.  */
  errno = 0;

  if (!write && trust_readonly)
    {
      /* User-settable option, "trust-readonly-sections".  If true,
         then memory from any SEC_READONLY bfd section may be read
         directly from the bfd file. */

      struct section_table *secp;

      for (secp = current_target.to_sections;
	   secp < current_target.to_sections_end;
	   secp++)
	{
	  if (bfd_get_section_flags (secp->bfd, secp->the_bfd_section) 
	      & SEC_READONLY)
	    if (memaddr >= secp->addr && memaddr < secp->endaddr)
	      return xfer_memory (memaddr, myaddr, len, 0, 
				  attrib, &current_target);
	}
    }

  /* The quick case is that the top target can handle the transfer.  */
  res = current_target.to_xfer_memory
    (memaddr, myaddr, len, write, attrib, &current_target);

  /* If res <= 0 then we call it again in the loop.  Ah well. */
  if (res <= 0)
    {
      for (item = target_stack; item; item = item->next)
	{
	  t = item->target_ops;
	  if (!t->to_has_memory)
	    continue;

	  res = t->to_xfer_memory (memaddr, myaddr, len, write, attrib, t);
	  if (res > 0)
	    break;		/* Handled all or part of xfer */
	  if (t->to_has_all_memory)
	    break;
	}

      if (res <= 0)
	return -1;
    }

  return res;
}


/* Perform a memory transfer.  Iterate until the entire region has
   been transfered.

   Result is 0 or errno value.  */

static int
target_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len, int write)
{
  int res;
  int reg_len;
  struct mem_region *region;

  /* Zero length requests are ok and require no work.  */
  if (len == 0)
    {
      return 0;
    }

  while (len > 0)
    {
      region = lookup_mem_region(memaddr);
      if (memaddr + len < region->hi)
	reg_len = len;
      else
	reg_len = region->hi - memaddr;

      switch (region->attrib.mode)
	{
	case MEM_RO:
	  if (write)
	    return EIO;
	  break;
	  
	case MEM_WO:
	  if (!write)
	    return EIO;
	  break;
	}

      while (reg_len > 0)
	{
	  if (region->attrib.cache)
	    res = dcache_xfer_memory (target_dcache, memaddr, myaddr,
				     reg_len, write);
	  else
	    res = do_xfer_memory (memaddr, myaddr, reg_len, write,
				 &region->attrib);
	      
	  if (res <= 0)
	    {
	      /* If this address is for nonexistent memory, read zeros
		 if reading, or do nothing if writing.  Return
		 error. */
	      if (!write)
		memset (myaddr, 0, len);
	      if (errno == 0)
		return EIO;
	      else
		return errno;
	    }

	  memaddr += res;
	  myaddr  += res;
	  len     -= res;
	  reg_len -= res;
	}
    }
  
  return 0;			/* We managed to cover it all somehow. */
}


/* Perform a partial memory transfer.

   Result is -1 on error, or the number of bytes transfered.  */

static int
target_xfer_memory_partial (CORE_ADDR memaddr, char *myaddr, int len,
			    int write_p, int *err)
{
  int res;
  int reg_len;
  struct mem_region *region;

  /* Zero length requests are ok and require no work.  */
  if (len == 0)
    {
      *err = 0;
      return 0;
    }

  region = lookup_mem_region(memaddr);
  if (memaddr + len < region->hi)
    reg_len = len;
  else
    reg_len = region->hi - memaddr;

  switch (region->attrib.mode)
    {
    case MEM_RO:
      if (write_p)
	{
	  *err = EIO;
	  return -1;
	}
      break;

    case MEM_WO:
      if (write_p)
	{
	  *err = EIO;
	  return -1;
	}
      break;
    }

  if (region->attrib.cache)
    res = dcache_xfer_memory (target_dcache, memaddr, myaddr,
			      reg_len, write_p);
  else
    res = do_xfer_memory (memaddr, myaddr, reg_len, write_p,
			  &region->attrib);
      
  if (res <= 0)
    {
      if (errno != 0)
	*err = errno;
      else
	*err = EIO;

        return -1;
    }

  *err = 0;
  return res;
}

int
target_read_memory_partial (CORE_ADDR memaddr, char *buf, int len, int *err)
{
  return target_xfer_memory_partial (memaddr, buf, len, 0, err);
}

int
target_write_memory_partial (CORE_ADDR memaddr, char *buf, int len, int *err)
{
  return target_xfer_memory_partial (memaddr, buf, len, 1, err);
}

/* ARGSUSED */
static void
target_info (char *args, int from_tty)
{
  struct target_ops *t;
  struct target_stack_item *item;
  int has_all_mem = 0;

  if (symfile_objfile != NULL)
    printf_unfiltered ("Symbols from \"%s\".\n", symfile_objfile->name);

#ifdef FILES_INFO_HOOK
  if (FILES_INFO_HOOK ())
    return;
#endif

  for (item = target_stack; item; item = item->next)
    {
      t = item->target_ops;

      if (!t->to_has_memory)
	continue;

      if ((int) (t->to_stratum) <= (int) dummy_stratum)
	continue;
      if (has_all_mem)
	printf_unfiltered ("\tWhile running this, GDB does not access memory from...\n");
      printf_unfiltered ("%s:\n", t->to_longname);
      (t->to_files_info) (t);
      has_all_mem = t->to_has_all_memory;
    }
}

/* This is to be called by the open routine before it does
   anything.  */

void
target_preopen (int from_tty)
{
  dont_repeat ();

  if (target_has_execution)
    {
      if (!from_tty
          || query ("A program is being debugged already.  Kill it? "))
	target_kill ();
      else
	error ("Program not killed.");
    }

  /* Calling target_kill may remove the target from the stack.  But if
     it doesn't (which seems like a win for UDI), remove it now.  */

  if (target_has_execution)
    pop_target ();
}

/* Detach a target after doing deferred register stores.  */

void
target_detach (char *args, int from_tty)
{
  /* Handle any optimized stores to the inferior.  */
#ifdef DO_DEFERRED_STORES
  DO_DEFERRED_STORES;
#endif
  (current_target.to_detach) (args, from_tty);
}

void
target_link (char *modname, CORE_ADDR *t_reloc)
{
  if (STREQ (current_target.to_shortname, "rombug"))
    {
      (current_target.to_lookup_symbol) (modname, t_reloc);
      if (*t_reloc == 0)
	error ("Unable to link to %s and get relocation in rombug", modname);
    }
  else
    *t_reloc = (CORE_ADDR) -1;
}

int
target_async_mask (int mask)
{
  int saved_async_masked_status = target_async_mask_value;
  target_async_mask_value = mask;
  return saved_async_masked_status;
}

/* Look through the list of possible targets for a target that can
   execute a run or attach command without any other data.  This is
   used to locate the default process stratum.

   Result is always valid (error() is called for errors).  */

static struct target_ops *
find_default_run_target (char *do_mesg)
{
  struct target_ops **t;
  struct target_ops *runable = NULL;
  int count;

  count = 0;

  for (t = target_structs; t < target_structs + target_struct_size;
       ++t)
    {
      if ((*t)->to_can_run && target_can_run (*t))
	{
	  runable = *t;
	  ++count;
	}
    }

  if (count != 1)
    error ("Don't know how to %s.  Try \"help target\".", do_mesg);

  return runable;
}

void
find_default_attach (char *args, int from_tty)
{
  struct target_ops *t;

  t = find_default_run_target ("attach");
  (t->to_attach) (args, from_tty);
  return;
}

void
find_default_require_attach (char *args, int from_tty)
{
  struct target_ops *t;

  t = find_default_run_target ("require_attach");
  (t->to_require_attach) (args, from_tty);
  return;
}

void
find_default_require_detach (int pid, char *args, int from_tty)
{
  struct target_ops *t;

  t = find_default_run_target ("require_detach");
  (t->to_require_detach) (pid, args, from_tty);
  return;
}

void
find_default_create_inferior (char *exec_file, char *allargs, char **env)
{
  struct target_ops *t;

  t = find_default_run_target ("run");
  (t->to_create_inferior) (exec_file, allargs, env);
  return;
}

void
find_default_clone_and_follow_inferior (int child_pid, int *followed_child)
{
  struct target_ops *t;

  t = find_default_run_target ("run");
  (t->to_clone_and_follow_inferior) (child_pid, followed_child);
  return;
}

static int
default_region_size_ok_for_hw_watchpoint (int byte_count)
{
  return (byte_count <= REGISTER_SIZE);
}

static int
return_zero (void)
{
  return 0;
}

static int
return_one (void)
{
  return 1;
}

static int
return_minus_one (void)
{
  return -1;
}

/*
 * Resize the to_sections pointer.  Also make sure that anyone that
 * was holding on to an old value of it gets updated.
 * Returns the old size.
 */

int
target_resize_to_sections (struct target_ops *target, int num_added)
{
  struct target_ops **t;
  struct section_table *old_value;
  int old_count;

  old_value = target->to_sections;

  if (target->to_sections)
    {
      old_count = target->to_sections_end - target->to_sections;
      target->to_sections = (struct section_table *)
	xrealloc ((char *) target->to_sections,
		  (sizeof (struct section_table)) * (num_added + old_count));
    }
  else
    {
      old_count = 0;
      target->to_sections = (struct section_table *)
	xmalloc ((sizeof (struct section_table)) * num_added);
    }
  target->to_sections_end = target->to_sections + (num_added + old_count);

  /* Check to see if anyone else was pointing to this structure.
     If old_value was null, then no one was. */
     
  if (old_value)
    {
      for (t = target_structs; t < target_structs + target_struct_size;
	   ++t)
	{
	  if ((*t)->to_sections == old_value)
	    {
	      (*t)->to_sections = target->to_sections;
	      (*t)->to_sections_end = target->to_sections_end;
	    }
	}
    }
  
  return old_count;

}

/* Remove all target sections taken from ABFD.

   Scan the current target stack for targets whose section tables
   refer to sections from BFD, and remove those sections.  We use this
   when we notice that the inferior has unloaded a shared object, for
   example.  */
void
remove_target_sections (bfd *abfd)
{
  struct target_ops **t;

  for (t = target_structs; t < target_structs + target_struct_size; t++)
    {
      struct section_table *src, *dest;

      dest = (*t)->to_sections;
      for (src = (*t)->to_sections; src < (*t)->to_sections_end; src++)
	if (src->bfd != abfd)
	  {
	    /* Keep this section.  */
	    if (dest < src) *dest = *src;
	    dest++;
	  }

      /* If we've dropped any sections, resize the section table.  */
      if (dest < src)
	target_resize_to_sections (*t, dest - src);
    }
}




/* Find a single runnable target in the stack and return it.  If for
   some reason there is more than one, return NULL.  */

struct target_ops *
find_run_target (void)
{
  struct target_ops **t;
  struct target_ops *runable = NULL;
  int count;

  count = 0;

  for (t = target_structs; t < target_structs + target_struct_size; ++t)
    {
      if ((*t)->to_can_run && target_can_run (*t))
	{
	  runable = *t;
	  ++count;
	}
    }

  return (count == 1 ? runable : NULL);
}

/* Find a single core_stratum target in the list of targets and return it.
   If for some reason there is more than one, return NULL.  */

struct target_ops *
find_core_target (void)
{
  struct target_ops **t;
  struct target_ops *runable = NULL;
  int count;

  count = 0;

  for (t = target_structs; t < target_structs + target_struct_size;
       ++t)
    {
      if ((*t)->to_stratum == core_stratum)
	{
	  runable = *t;
	  ++count;
	}
    }

  return (count == 1 ? runable : NULL);
}

/*
 * Find the next target down the stack from the specified target.
 */

struct target_ops *
find_target_beneath (struct target_ops *t)
{
  struct target_stack_item *cur;

  for (cur = target_stack; cur; cur = cur->next)
    if (cur->target_ops == t)
      break;

  if (cur == NULL || cur->next == NULL)
    return NULL;
  else
    return cur->next->target_ops;
}


/* The inferior process has died.  Long live the inferior!  */

void
generic_mourn_inferior (void)
{
  extern int show_breakpoint_hit_counts;

  inferior_ptid = null_ptid;
  attach_flag = 0;
  breakpoint_init_inferior (inf_exited);
  registers_changed ();

#ifdef CLEAR_DEFERRED_STORES
  /* Delete any pending stores to the inferior... */
  CLEAR_DEFERRED_STORES;
#endif

  reopen_exec_file ();
  reinit_frame_cache ();

  /* It is confusing to the user for ignore counts to stick around
     from previous runs of the inferior.  So clear them.  */
  /* However, it is more confusing for the ignore counts to disappear when
     using hit counts.  So don't clear them if we're counting hits.  */
  if (!show_breakpoint_hit_counts)
    breakpoint_clear_ignore_counts ();

  if (detach_hook)
    detach_hook ();
}

/* Helper function for child_wait and the Lynx derivatives of child_wait.
   HOSTSTATUS is the waitstatus from wait() or the equivalent; store our
   translation of that in OURSTATUS.  */
void
store_waitstatus (struct target_waitstatus *ourstatus, int hoststatus)
{
#ifdef CHILD_SPECIAL_WAITSTATUS
  /* CHILD_SPECIAL_WAITSTATUS should return nonzero and set *OURSTATUS
     if it wants to deal with hoststatus.  */
  if (CHILD_SPECIAL_WAITSTATUS (ourstatus, hoststatus))
    return;
#endif

  if (WIFEXITED (hoststatus))
    {
      ourstatus->kind = TARGET_WAITKIND_EXITED;
      ourstatus->value.integer = WEXITSTATUS (hoststatus);
    }
  else if (!WIFSTOPPED (hoststatus))
    {
      ourstatus->kind = TARGET_WAITKIND_SIGNALLED;
      ourstatus->value.sig = target_signal_from_host (WTERMSIG (hoststatus));
    }
  else
    {
      ourstatus->kind = TARGET_WAITKIND_STOPPED;
      ourstatus->value.sig = target_signal_from_host (WSTOPSIG (hoststatus));
    }
}

/* Returns zero to leave the inferior alone, one to interrupt it.  */
int (*target_activity_function) (void);
int target_activity_fd;

/* Convert a normal process ID to a string.  Returns the string in a static
   buffer.  */

char *
normal_pid_to_str (ptid_t ptid)
{
  static char buf[30];

  sprintf (buf, "process %d", PIDGET (ptid));
  return buf;
}

/* Some targets (such as ttrace-based HPUX) don't allow us to request
   notification of inferior events such as fork and vork immediately
   after the inferior is created.  (This because of how gdb gets an
   inferior created via invoking a shell to do it.  In such a scenario,
   if the shell init file has commands in it, the shell will fork and
   exec for each of those commands, and we will see each such fork
   event.  Very bad.)

   This function is used by all targets that allow us to request
   notification of forks, etc at inferior creation time; e.g., in
   target_acknowledge_forked_child.
 */
static void
normal_target_post_startup_inferior (ptid_t ptid)
{
  /* This space intentionally left blank. */
}

/* Error-catcher for target_find_memory_regions */
/* ARGSUSED */
static int dummy_find_memory_regions (int (*ignore1) (), void *ignore2)
{
  error ("No target.");
  return 0;
}

/* Error-catcher for target_make_corefile_notes */
/* ARGSUSED */
static char * dummy_make_corefile_notes (bfd *ignore1, int *ignore2)
{
  error ("No target.");
  return NULL;
}

/* Set up the handful of non-empty slots needed by the dummy target
   vector.  */

static void
init_dummy_target (void)
{
  dummy_target.to_shortname = "None";
  dummy_target.to_longname = "None";
  dummy_target.to_doc = "";
  dummy_target.to_attach = find_default_attach;
  dummy_target.to_require_attach = find_default_require_attach;
  dummy_target.to_require_detach = find_default_require_detach;
  dummy_target.to_create_inferior = find_default_create_inferior;
  dummy_target.to_clone_and_follow_inferior = find_default_clone_and_follow_inferior;
  dummy_target.to_pid_to_str = normal_pid_to_str;
  dummy_target.to_stratum = dummy_stratum;
  dummy_target.to_find_memory_regions = dummy_find_memory_regions;
  dummy_target.to_make_corefile_notes = dummy_make_corefile_notes;
  dummy_target.to_magic = OPS_MAGIC;
}


static struct target_ops debug_target;

static void
debug_to_open (char *args, int from_tty)
{
  debug_target.to_open (args, from_tty);

  fprintf_unfiltered (gdb_stdlog, "target_open (%s, %d)\n", args, from_tty);
}

static void
debug_to_close (int quitting)
{
  debug_target.to_close (quitting);

  fprintf_unfiltered (gdb_stdlog, "target_close (%d)\n", quitting);
}

static void
debug_to_attach (char *args, int from_tty)
{
  debug_target.to_attach (args, from_tty);

  fprintf_unfiltered (gdb_stdlog, "target_attach (%s, %d)\n", args, from_tty);
}


static void
debug_to_post_attach (int pid)
{
  debug_target.to_post_attach (pid);

  fprintf_unfiltered (gdb_stdlog, "target_post_attach (%d)\n", pid);
}

static void
debug_to_require_attach (char *args, int from_tty)
{
  debug_target.to_require_attach (args, from_tty);

  fprintf_unfiltered (gdb_stdlog,
		      "target_require_attach (%s, %d)\n", args, from_tty);
}

static void
debug_to_detach (char *args, int from_tty)
{
  debug_target.to_detach (args, from_tty);

  fprintf_unfiltered (gdb_stdlog, "target_detach (%s, %d)\n", args, from_tty);
}

static void
debug_to_require_detach (int pid, char *args, int from_tty)
{
  debug_target.to_require_detach (pid, args, from_tty);

  fprintf_unfiltered (gdb_stdlog,
	       "target_require_detach (%d, %s, %d)\n", pid, args, from_tty);
}

static void
debug_to_resume (ptid_t ptid, int step, enum target_signal siggnal)
{
  debug_target.to_resume (ptid, step, siggnal);

  fprintf_unfiltered (gdb_stdlog, "target_resume (%d, %s, %s)\n", PIDGET (ptid),
		      step ? "step" : "continue",
		      target_signal_to_name (siggnal));
}

static ptid_t
debug_to_wait (ptid_t ptid, struct target_waitstatus *status)
{
  ptid_t retval;

  retval = debug_target.to_wait (ptid, status);

  fprintf_unfiltered (gdb_stdlog,
		      "target_wait (%d, status) = %d,   ", PIDGET (ptid),
		      PIDGET (retval));
  fprintf_unfiltered (gdb_stdlog, "status->kind = ");
  switch (status->kind)
    {
    case TARGET_WAITKIND_EXITED:
      fprintf_unfiltered (gdb_stdlog, "exited, status = %d\n",
			  status->value.integer);
      break;
    case TARGET_WAITKIND_STOPPED:
      fprintf_unfiltered (gdb_stdlog, "stopped, signal = %s\n",
			  target_signal_to_name (status->value.sig));
      break;
    case TARGET_WAITKIND_SIGNALLED:
      fprintf_unfiltered (gdb_stdlog, "signalled, signal = %s\n",
			  target_signal_to_name (status->value.sig));
      break;
    case TARGET_WAITKIND_LOADED:
      fprintf_unfiltered (gdb_stdlog, "loaded\n");
      break;
    case TARGET_WAITKIND_FORKED:
      fprintf_unfiltered (gdb_stdlog, "forked\n");
      break;
    case TARGET_WAITKIND_VFORKED:
      fprintf_unfiltered (gdb_stdlog, "vforked\n");
      break;
    case TARGET_WAITKIND_EXECD:
      fprintf_unfiltered (gdb_stdlog, "execd\n");
      break;
    case TARGET_WAITKIND_SPURIOUS:
      fprintf_unfiltered (gdb_stdlog, "spurious\n");
      break;
    default:
      fprintf_unfiltered (gdb_stdlog, "unknown???\n");
      break;
    }

  return retval;
}

static void
debug_to_post_wait (ptid_t ptid, int status)
{
  debug_target.to_post_wait (ptid, status);

  fprintf_unfiltered (gdb_stdlog, "target_post_wait (%d, %d)\n",
		      PIDGET (ptid), status);
}

static void
debug_print_register (const char * func, int regno)
{
  fprintf_unfiltered (gdb_stdlog, "%s ", func);
  if (regno >= 0 && regno < NUM_REGS + NUM_PSEUDO_REGS
      && REGISTER_NAME (regno) != NULL && REGISTER_NAME (regno)[0] != '\0')
    fprintf_unfiltered (gdb_stdlog, "(%s)", REGISTER_NAME (regno));
  else
    fprintf_unfiltered (gdb_stdlog, "(%d)", regno);
  if (regno >= 0)
    {
      int i;
      unsigned char *buf = alloca (MAX_REGISTER_RAW_SIZE);
      deprecated_read_register_gen (regno, buf);
      fprintf_unfiltered (gdb_stdlog, " = ");
      for (i = 0; i < REGISTER_RAW_SIZE (regno); i++)
	{
	  fprintf_unfiltered (gdb_stdlog, "%02x", buf[i]);
	}
      if (REGISTER_RAW_SIZE (regno) <= sizeof (LONGEST))
	{
	  fprintf_unfiltered (gdb_stdlog, " 0x%s %s",
			      paddr_nz (read_register (regno)),
			      paddr_d (read_register (regno)));
	}
    }
  fprintf_unfiltered (gdb_stdlog, "\n");
}

static void
debug_to_fetch_registers (int regno)
{
  debug_target.to_fetch_registers (regno);
  debug_print_register ("target_fetch_registers", regno);
}

static void
debug_to_store_registers (int regno)
{
  debug_target.to_store_registers (regno);
  debug_print_register ("target_store_registers", regno);
  fprintf_unfiltered (gdb_stdlog, "\n");
}

static void
debug_to_prepare_to_store (void)
{
  debug_target.to_prepare_to_store ();

  fprintf_unfiltered (gdb_stdlog, "target_prepare_to_store ()\n");
}

static int
debug_to_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len, int write,
		      struct mem_attrib *attrib,
		      struct target_ops *target)
{
  int retval;

  retval = debug_target.to_xfer_memory (memaddr, myaddr, len, write,
					attrib, target);

  fprintf_unfiltered (gdb_stdlog,
		      "target_xfer_memory (0x%x, xxx, %d, %s, xxx) = %d",
		      (unsigned int) memaddr,	/* possable truncate long long */
		      len, write ? "write" : "read", retval);



  if (retval > 0)
    {
      int i;

      fputs_unfiltered (", bytes =", gdb_stdlog);
      for (i = 0; i < retval; i++)
	{
	  if ((((long) &(myaddr[i])) & 0xf) == 0)
	    fprintf_unfiltered (gdb_stdlog, "\n");
	  fprintf_unfiltered (gdb_stdlog, " %02x", myaddr[i] & 0xff);
	}
    }

  fputc_unfiltered ('\n', gdb_stdlog);

  return retval;
}

static void
debug_to_files_info (struct target_ops *target)
{
  debug_target.to_files_info (target);

  fprintf_unfiltered (gdb_stdlog, "target_files_info (xxx)\n");
}

static int
debug_to_insert_breakpoint (CORE_ADDR addr, char *save)
{
  int retval;

  retval = debug_target.to_insert_breakpoint (addr, save);

  fprintf_unfiltered (gdb_stdlog,
		      "target_insert_breakpoint (0x%lx, xxx) = %ld\n",
		      (unsigned long) addr,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_remove_breakpoint (CORE_ADDR addr, char *save)
{
  int retval;

  retval = debug_target.to_remove_breakpoint (addr, save);

  fprintf_unfiltered (gdb_stdlog,
		      "target_remove_breakpoint (0x%lx, xxx) = %ld\n",
		      (unsigned long) addr,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_can_use_hw_breakpoint (int type, int cnt, int from_tty)
{
  int retval;

  retval = debug_target.to_can_use_hw_breakpoint (type, cnt, from_tty);

  fprintf_unfiltered (gdb_stdlog,
		      "target_can_use_hw_breakpoint (%ld, %ld, %ld) = %ld\n",
		      (unsigned long) type,
		      (unsigned long) cnt,
		      (unsigned long) from_tty,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_region_size_ok_for_hw_watchpoint (int byte_count)
{
  CORE_ADDR retval;

  retval = debug_target.to_region_size_ok_for_hw_watchpoint (byte_count);

  fprintf_unfiltered (gdb_stdlog,
		      "TARGET_REGION_SIZE_OK_FOR_HW_WATCHPOINT (%ld) = 0x%lx\n",
		      (unsigned long) byte_count,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_stopped_by_watchpoint (void)
{
  int retval;

  retval = debug_target.to_stopped_by_watchpoint ();

  fprintf_unfiltered (gdb_stdlog,
		      "STOPPED_BY_WATCHPOINT () = %ld\n",
		      (unsigned long) retval);
  return retval;
}

static CORE_ADDR
debug_to_stopped_data_address (void)
{
  CORE_ADDR retval;

  retval = debug_target.to_stopped_data_address ();

  fprintf_unfiltered (gdb_stdlog,
		      "target_stopped_data_address () = 0x%lx\n",
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_insert_hw_breakpoint (CORE_ADDR addr, char *save)
{
  int retval;

  retval = debug_target.to_insert_hw_breakpoint (addr, save);

  fprintf_unfiltered (gdb_stdlog,
		      "target_insert_hw_breakpoint (0x%lx, xxx) = %ld\n",
		      (unsigned long) addr,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_remove_hw_breakpoint (CORE_ADDR addr, char *save)
{
  int retval;

  retval = debug_target.to_remove_hw_breakpoint (addr, save);

  fprintf_unfiltered (gdb_stdlog,
		      "target_remove_hw_breakpoint (0x%lx, xxx) = %ld\n",
		      (unsigned long) addr,
		      (unsigned long) retval);
  return retval;
}

static int
debug_to_insert_watchpoint (CORE_ADDR addr, int len, int type)
{
  int retval;

  retval = debug_target.to_insert_watchpoint (addr, len, type);

  fprintf_unfiltered (gdb_stdlog,
		      "target_insert_watchpoint (0x%lx, %d, %d) = %ld\n",
		      (unsigned long) addr, len, type, (unsigned long) retval);
  return retval;
}

static int
debug_to_remove_watchpoint (CORE_ADDR addr, int len, int type)
{
  int retval;

  retval = debug_target.to_insert_watchpoint (addr, len, type);

  fprintf_unfiltered (gdb_stdlog,
		      "target_insert_watchpoint (0x%lx, %d, %d) = %ld\n",
		      (unsigned long) addr, len, type, (unsigned long) retval);
  return retval;
}

static void
debug_to_terminal_init (void)
{
  debug_target.to_terminal_init ();

  fprintf_unfiltered (gdb_stdlog, "target_terminal_init ()\n");
}

static void
debug_to_terminal_inferior (void)
{
  debug_target.to_terminal_inferior ();

  fprintf_unfiltered (gdb_stdlog, "target_terminal_inferior ()\n");
}

static void
debug_to_terminal_ours_for_output (void)
{
  debug_target.to_terminal_ours_for_output ();

  fprintf_unfiltered (gdb_stdlog, "target_terminal_ours_for_output ()\n");
}

static void
debug_to_terminal_ours (void)
{
  debug_target.to_terminal_ours ();

  fprintf_unfiltered (gdb_stdlog, "target_terminal_ours ()\n");
}

static void
debug_to_terminal_save_ours (void)
{
  debug_target.to_terminal_save_ours ();

  fprintf_unfiltered (gdb_stdlog, "target_terminal_save_ours ()\n");
}

static void
debug_to_terminal_info (char *arg, int from_tty)
{
  debug_target.to_terminal_info (arg, from_tty);

  fprintf_unfiltered (gdb_stdlog, "target_terminal_info (%s, %d)\n", arg,
		      from_tty);
}

static void
debug_to_kill (void)
{
  debug_target.to_kill ();

  fprintf_unfiltered (gdb_stdlog, "target_kill ()\n");
}

static void
debug_to_load (char *args, int from_tty)
{
  debug_target.to_load (args, from_tty);

  fprintf_unfiltered (gdb_stdlog, "target_load (%s, %d)\n", args, from_tty);
}

static int
debug_to_lookup_symbol (char *name, CORE_ADDR *addrp)
{
  int retval;

  retval = debug_target.to_lookup_symbol (name, addrp);

  fprintf_unfiltered (gdb_stdlog, "target_lookup_symbol (%s, xxx)\n", name);

  return retval;
}

static void
debug_to_create_inferior (char *exec_file, char *args, char **env)
{
  debug_target.to_create_inferior (exec_file, args, env);

  fprintf_unfiltered (gdb_stdlog, "target_create_inferior (%s, %s, xxx)\n",
		      exec_file, args);
}

static void
debug_to_post_startup_inferior (ptid_t ptid)
{
  debug_target.to_post_startup_inferior (ptid);

  fprintf_unfiltered (gdb_stdlog, "target_post_startup_inferior (%d)\n",
		      PIDGET (ptid));
}

static void
debug_to_acknowledge_created_inferior (int pid)
{
  debug_target.to_acknowledge_created_inferior (pid);

  fprintf_unfiltered (gdb_stdlog, "target_acknowledge_created_inferior (%d)\n",
		      pid);
}

static void
debug_to_clone_and_follow_inferior (int child_pid, int *followed_child)
{
  debug_target.to_clone_and_follow_inferior (child_pid, followed_child);

  fprintf_unfiltered (gdb_stdlog,
		      "target_clone_and_follow_inferior (%d, %d)\n",
		      child_pid, *followed_child);
}

static void
debug_to_post_follow_inferior_by_clone (void)
{
  debug_target.to_post_follow_inferior_by_clone ();

  fprintf_unfiltered (gdb_stdlog, "target_post_follow_inferior_by_clone ()\n");
}

static int
debug_to_insert_fork_catchpoint (int pid)
{
  int retval;

  retval = debug_target.to_insert_fork_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_insert_fork_catchpoint (%d) = %d\n",
		      pid, retval);

  return retval;
}

static int
debug_to_remove_fork_catchpoint (int pid)
{
  int retval;

  retval = debug_target.to_remove_fork_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_remove_fork_catchpoint (%d) = %d\n",
		      pid, retval);

  return retval;
}

static int
debug_to_insert_vfork_catchpoint (int pid)
{
  int retval;

  retval = debug_target.to_insert_vfork_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_insert_vfork_catchpoint (%d)= %d\n",
		      pid, retval);

  return retval;
}

static int
debug_to_remove_vfork_catchpoint (int pid)
{
  int retval;

  retval = debug_target.to_remove_vfork_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_remove_vfork_catchpoint (%d) = %d\n",
		      pid, retval);

  return retval;
}

static int
debug_to_has_forked (int pid, int *child_pid)
{
  int has_forked;

  has_forked = debug_target.to_has_forked (pid, child_pid);

  fprintf_unfiltered (gdb_stdlog, "target_has_forked (%d, %d) = %d\n",
		      pid, *child_pid, has_forked);

  return has_forked;
}

static int
debug_to_has_vforked (int pid, int *child_pid)
{
  int has_vforked;

  has_vforked = debug_target.to_has_vforked (pid, child_pid);

  fprintf_unfiltered (gdb_stdlog, "target_has_vforked (%d, %d) = %d\n",
		      pid, *child_pid, has_vforked);

  return has_vforked;
}

static int
debug_to_can_follow_vfork_prior_to_exec (void)
{
  int can_immediately_follow_vfork;

  can_immediately_follow_vfork = debug_target.to_can_follow_vfork_prior_to_exec ();

  fprintf_unfiltered (gdb_stdlog, "target_can_follow_vfork_prior_to_exec () = %d\n",
		      can_immediately_follow_vfork);

  return can_immediately_follow_vfork;
}

static void
debug_to_post_follow_vfork (int parent_pid, int followed_parent, int child_pid,
			    int followed_child)
{
  debug_target.to_post_follow_vfork (parent_pid, followed_parent, child_pid, followed_child);

  fprintf_unfiltered (gdb_stdlog,
		      "target_post_follow_vfork (%d, %d, %d, %d)\n",
		    parent_pid, followed_parent, child_pid, followed_child);
}

static int
debug_to_insert_exec_catchpoint (int pid)
{
  int retval;

  retval = debug_target.to_insert_exec_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_insert_exec_catchpoint (%d) = %d\n",
		      pid, retval);

  return retval;
}

static int
debug_to_remove_exec_catchpoint (int pid)
{
  int retval;

  retval = debug_target.to_remove_exec_catchpoint (pid);

  fprintf_unfiltered (gdb_stdlog, "target_remove_exec_catchpoint (%d) = %d\n",
		      pid, retval);

  return retval;
}

static int
debug_to_has_execd (int pid, char **execd_pathname)
{
  int has_execd;

  has_execd = debug_target.to_has_execd (pid, execd_pathname);

  fprintf_unfiltered (gdb_stdlog, "target_has_execd (%d, %s) = %d\n",
		      pid, (*execd_pathname ? *execd_pathname : "<NULL>"),
		      has_execd);

  return has_execd;
}

static int
debug_to_reported_exec_events_per_exec_call (void)
{
  int reported_exec_events;

  reported_exec_events = debug_target.to_reported_exec_events_per_exec_call ();

  fprintf_unfiltered (gdb_stdlog,
		      "target_reported_exec_events_per_exec_call () = %d\n",
		      reported_exec_events);

  return reported_exec_events;
}

static int
debug_to_has_syscall_event (int pid, enum target_waitkind *kind,
			    int *syscall_id)
{
  int has_syscall_event;
  char *kind_spelling = "??";

  has_syscall_event = debug_target.to_has_syscall_event (pid, kind, syscall_id);
  if (has_syscall_event)
    {
      switch (*kind)
	{
	case TARGET_WAITKIND_SYSCALL_ENTRY:
	  kind_spelling = "SYSCALL_ENTRY";
	  break;
	case TARGET_WAITKIND_SYSCALL_RETURN:
	  kind_spelling = "SYSCALL_RETURN";
	  break;
	default:
	  break;
	}
    }

  fprintf_unfiltered (gdb_stdlog,
		      "target_has_syscall_event (%d, %s, %d) = %d\n",
		      pid, kind_spelling, *syscall_id, has_syscall_event);

  return has_syscall_event;
}

static int
debug_to_has_exited (int pid, int wait_status, int *exit_status)
{
  int has_exited;

  has_exited = debug_target.to_has_exited (pid, wait_status, exit_status);

  fprintf_unfiltered (gdb_stdlog, "target_has_exited (%d, %d, %d) = %d\n",
		      pid, wait_status, *exit_status, has_exited);

  return has_exited;
}

static void
debug_to_mourn_inferior (void)
{
  debug_target.to_mourn_inferior ();

  fprintf_unfiltered (gdb_stdlog, "target_mourn_inferior ()\n");
}

static int
debug_to_can_run (void)
{
  int retval;

  retval = debug_target.to_can_run ();

  fprintf_unfiltered (gdb_stdlog, "target_can_run () = %d\n", retval);

  return retval;
}

static void
debug_to_notice_signals (ptid_t ptid)
{
  debug_target.to_notice_signals (ptid);

  fprintf_unfiltered (gdb_stdlog, "target_notice_signals (%d)\n",
                      PIDGET (ptid));
}

static int
debug_to_thread_alive (ptid_t ptid)
{
  int retval;

  retval = debug_target.to_thread_alive (ptid);

  fprintf_unfiltered (gdb_stdlog, "target_thread_alive (%d) = %d\n",
		      PIDGET (ptid), retval);

  return retval;
}

static void
debug_to_find_new_threads (void)
{
  debug_target.to_find_new_threads ();

  fputs_unfiltered ("target_find_new_threads ()\n", gdb_stdlog);
}

static void
debug_to_stop (void)
{
  debug_target.to_stop ();

  fprintf_unfiltered (gdb_stdlog, "target_stop ()\n");
}

static int
debug_to_query (int type, char *req, char *resp, int *siz)
{
  int retval;

  retval = debug_target.to_query (type, req, resp, siz);

  fprintf_unfiltered (gdb_stdlog, "target_query (%c, %s, %s,  %d) = %d\n", type, req, resp, *siz, retval);

  return retval;
}

static void
debug_to_rcmd (char *command,
	       struct ui_file *outbuf)
{
  debug_target.to_rcmd (command, outbuf);
  fprintf_unfiltered (gdb_stdlog, "target_rcmd (%s, ...)\n", command);
}

static struct symtab_and_line *
debug_to_enable_exception_callback (enum exception_event_kind kind, int enable)
{
  struct symtab_and_line *result;
  result = debug_target.to_enable_exception_callback (kind, enable);
  fprintf_unfiltered (gdb_stdlog,
		      "target get_exception_callback_sal (%d, %d)\n",
		      kind, enable);
  return result;
}

static struct exception_event_record *
debug_to_get_current_exception_event (void)
{
  struct exception_event_record *result;
  result = debug_target.to_get_current_exception_event ();
  fprintf_unfiltered (gdb_stdlog, "target get_current_exception_event ()\n");
  return result;
}

static char *
debug_to_pid_to_exec_file (int pid)
{
  char *exec_file;

  exec_file = debug_target.to_pid_to_exec_file (pid);

  fprintf_unfiltered (gdb_stdlog, "target_pid_to_exec_file (%d) = %s\n",
		      pid, exec_file);

  return exec_file;
}

static void
setup_target_debug (void)
{
  memcpy (&debug_target, &current_target, sizeof debug_target);

  current_target.to_open = debug_to_open;
  current_target.to_close = debug_to_close;
  current_target.to_attach = debug_to_attach;
  current_target.to_post_attach = debug_to_post_attach;
  current_target.to_require_attach = debug_to_require_attach;
  current_target.to_detach = debug_to_detach;
  current_target.to_require_detach = debug_to_require_detach;
  current_target.to_resume = debug_to_resume;
  current_target.to_wait = debug_to_wait;
  current_target.to_post_wait = debug_to_post_wait;
  current_target.to_fetch_registers = debug_to_fetch_registers;
  current_target.to_store_registers = debug_to_store_registers;
  current_target.to_prepare_to_store = debug_to_prepare_to_store;
  current_target.to_xfer_memory = debug_to_xfer_memory;
  current_target.to_files_info = debug_to_files_info;
  current_target.to_insert_breakpoint = debug_to_insert_breakpoint;
  current_target.to_remove_breakpoint = debug_to_remove_breakpoint;
  current_target.to_can_use_hw_breakpoint = debug_to_can_use_hw_breakpoint;
  current_target.to_insert_hw_breakpoint = debug_to_insert_hw_breakpoint;
  current_target.to_remove_hw_breakpoint = debug_to_remove_hw_breakpoint;
  current_target.to_insert_watchpoint = debug_to_insert_watchpoint;
  current_target.to_remove_watchpoint = debug_to_remove_watchpoint;
  current_target.to_stopped_by_watchpoint = debug_to_stopped_by_watchpoint;
  current_target.to_stopped_data_address = debug_to_stopped_data_address;
  current_target.to_region_size_ok_for_hw_watchpoint = debug_to_region_size_ok_for_hw_watchpoint;
  current_target.to_terminal_init = debug_to_terminal_init;
  current_target.to_terminal_inferior = debug_to_terminal_inferior;
  current_target.to_terminal_ours_for_output = debug_to_terminal_ours_for_output;
  current_target.to_terminal_ours = debug_to_terminal_ours;
  current_target.to_terminal_save_ours = debug_to_terminal_save_ours;
  current_target.to_terminal_info = debug_to_terminal_info;
  current_target.to_kill = debug_to_kill;
  current_target.to_load = debug_to_load;
  current_target.to_lookup_symbol = debug_to_lookup_symbol;
  current_target.to_create_inferior = debug_to_create_inferior;
  current_target.to_post_startup_inferior = debug_to_post_startup_inferior;
  current_target.to_acknowledge_created_inferior = debug_to_acknowledge_created_inferior;
  current_target.to_clone_and_follow_inferior = debug_to_clone_and_follow_inferior;
  current_target.to_post_follow_inferior_by_clone = debug_to_post_follow_inferior_by_clone;
  current_target.to_insert_fork_catchpoint = debug_to_insert_fork_catchpoint;
  current_target.to_remove_fork_catchpoint = debug_to_remove_fork_catchpoint;
  current_target.to_insert_vfork_catchpoint = debug_to_insert_vfork_catchpoint;
  current_target.to_remove_vfork_catchpoint = debug_to_remove_vfork_catchpoint;
  current_target.to_has_forked = debug_to_has_forked;
  current_target.to_has_vforked = debug_to_has_vforked;
  current_target.to_can_follow_vfork_prior_to_exec = debug_to_can_follow_vfork_prior_to_exec;
  current_target.to_post_follow_vfork = debug_to_post_follow_vfork;
  current_target.to_insert_exec_catchpoint = debug_to_insert_exec_catchpoint;
  current_target.to_remove_exec_catchpoint = debug_to_remove_exec_catchpoint;
  current_target.to_has_execd = debug_to_has_execd;
  current_target.to_reported_exec_events_per_exec_call = debug_to_reported_exec_events_per_exec_call;
  current_target.to_has_syscall_event = debug_to_has_syscall_event;
  current_target.to_has_exited = debug_to_has_exited;
  current_target.to_mourn_inferior = debug_to_mourn_inferior;
  current_target.to_can_run = debug_to_can_run;
  current_target.to_notice_signals = debug_to_notice_signals;
  current_target.to_thread_alive = debug_to_thread_alive;
  current_target.to_find_new_threads = debug_to_find_new_threads;
  current_target.to_stop = debug_to_stop;
  current_target.to_query = debug_to_query;
  current_target.to_rcmd = debug_to_rcmd;
  current_target.to_enable_exception_callback = debug_to_enable_exception_callback;
  current_target.to_get_current_exception_event = debug_to_get_current_exception_event;
  current_target.to_pid_to_exec_file = debug_to_pid_to_exec_file;

}


static char targ_desc[] =
"Names of targets and files being debugged.\n\
Shows the entire stack of targets currently in use (including the exec-file,\n\
core-file, and process, if any), as well as the symbol file name.";

static void
do_monitor_command (char *cmd,
		 int from_tty)
{
  if ((current_target.to_rcmd
       == (void (*) (char *, struct ui_file *)) tcomplain)
      || (current_target.to_rcmd == debug_to_rcmd
	  && (debug_target.to_rcmd
	      == (void (*) (char *, struct ui_file *)) tcomplain)))
    {
      error ("\"monitor\" command not supported by this target.\n");
    }
  target_rcmd (cmd, gdb_stdtarg);
}

void
initialize_targets (void)
{
  init_dummy_target ();
  push_target (&dummy_target);

  add_info ("target", target_info, targ_desc);
  add_info ("files", target_info, targ_desc);

  add_show_from_set 
    (add_set_cmd ("target", class_maintenance, var_zinteger,
		  (char *) &targetdebug,
		  "Set target debugging.\n\
When non-zero, target debugging is enabled.", &setdebuglist),
     &showdebuglist);

  add_setshow_boolean_cmd ("trust-readonly-sections", class_support, 
			   &trust_readonly, "\
Set mode for reading from readonly sections.\n\
When this mode is on, memory reads from readonly sections (such as .text)\n\
will be read from the object file instead of from the target.  This will\n\
result in significant performance improvement for remote targets.", "\
Show mode for reading from readonly sections.\n",
			   NULL, NULL,
			   &setlist, &showlist);

  add_com ("monitor", class_obscure, do_monitor_command,
	   "Send a command to the remote monitor (remote targets only).");

  target_dcache = dcache_init ();
}
