/* Low level interface to ptrace, for the remote server for GDB.
   Copyright 1995, 1996, 1998, 1999, 2000, 2001, 2002
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

#include "server.h"
#include "linux-low.h"

#include <sys/wait.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* ``all_threads'' is keyed by the LWP ID - it should be the thread ID instead,
   however.  This requires changing the ID in place when we go from !using_threads
   to using_threads, immediately.

   ``all_processes'' is keyed by the process ID - which on Linux is (presently)
   the same as the LWP ID.  */

struct inferior_list all_processes;

/* FIXME this is a bit of a hack, and could be removed.  */
int stopping_threads;

/* FIXME make into a target method?  */
int using_threads;

static void linux_resume_one_process (struct inferior_list_entry *entry,
				      int step, int signal);
static void linux_resume (int step, int signal);
static void stop_all_processes (void);
static int linux_wait_for_event (struct thread_info *child);

struct pending_signals
{
  int signal;
  struct pending_signals *prev;
};

#define PTRACE_ARG3_TYPE long
#define PTRACE_XFER_TYPE long

#ifdef HAVE_LINUX_REGSETS
static int use_regsets_p = 1;
#endif

extern int errno;

int debug_threads = 0;

#define pid_of(proc) ((proc)->head.id)

/* FIXME: Delete eventually.  */
#define inferior_pid (pid_of (get_thread_process (current_inferior)))

/* This function should only be called if the process got a SIGTRAP.
   The SIGTRAP could mean several things.

   On i386, where decr_pc_after_break is non-zero:
   If we were single-stepping this process using PTRACE_SINGLESTEP,
   we will get only the one SIGTRAP (even if the instruction we
   stepped over was a breakpoint).  The value of $eip will be the
   next instruction.
   If we continue the process using PTRACE_CONT, we will get a
   SIGTRAP when we hit a breakpoint.  The value of $eip will be
   the instruction after the breakpoint (i.e. needs to be
   decremented).  If we report the SIGTRAP to GDB, we must also
   report the undecremented PC.  If we cancel the SIGTRAP, we
   must resume at the decremented PC.

   (Presumably, not yet tested) On a non-decr_pc_after_break machine
   with hardware or kernel single-step:
   If we single-step over a breakpoint instruction, our PC will
   point at the following instruction.  If we continue and hit a
   breakpoint instruction, our PC will point at the breakpoint
   instruction.  */

static CORE_ADDR
get_stop_pc (void)
{
  CORE_ADDR stop_pc = (*the_low_target.get_pc) ();

  if (get_thread_process (current_inferior)->stepping)
    return stop_pc;
  else
    return stop_pc - the_low_target.decr_pc_after_break;
}

static void *
add_process (int pid)
{
  struct process_info *process;

  process = (struct process_info *) malloc (sizeof (*process));
  memset (process, 0, sizeof (*process));

  process->head.id = pid;

  /* Default to tid == lwpid == pid.  */
  process->tid = pid;
  process->lwpid = pid;

  add_inferior_to_list (&all_processes, &process->head);

  return process;
}

/* Start an inferior process and returns its pid.
   ALLARGS is a vector of program-name and args. */

static int
linux_create_inferior (char *program, char **allargs)
{
  void *new_process;
  int pid;

  pid = fork ();
  if (pid < 0)
    perror_with_name ("fork");

  if (pid == 0)
    {
      ptrace (PTRACE_TRACEME, 0, 0, 0);

      signal (SIGRTMIN + 1, SIG_DFL);

      setpgid (0, 0);

      execv (program, allargs);

      fprintf (stderr, "Cannot exec %s: %s.\n", program,
	       strerror (errno));
      fflush (stderr);
      _exit (0177);
    }

  new_process = add_process (pid);
  add_thread (pid, new_process);

  return pid;
}

/* Attach to an inferior process.  */

void
linux_attach_lwp (int pid, int tid)
{
  struct process_info *new_process;

  if (ptrace (PTRACE_ATTACH, pid, 0, 0) != 0)
    {
      fprintf (stderr, "Cannot attach to process %d: %s (%d)\n", pid,
	       errno < sys_nerr ? sys_errlist[errno] : "unknown error",
	       errno);
      fflush (stderr);

      /* If we fail to attach to an LWP, just return.  */
      if (!using_threads)
	_exit (0177);
      return;
    }

  new_process = (struct process_info *) add_process (pid);
  add_thread (tid, new_process);

  /* The next time we wait for this LWP we'll see a SIGSTOP as PTRACE_ATTACH
     brings it to a halt.  We should ignore that SIGSTOP and resume the process
     (unless this is the first process, in which case the flag will be cleared
     in linux_attach).

     On the other hand, if we are currently trying to stop all threads, we
     should treat the new thread as if we had sent it a SIGSTOP.  This works
     because we are guaranteed that add_process added us to the end of the
     list, and so the new thread has not yet reached wait_for_sigstop (but
     will).  */
  if (! stopping_threads)
    new_process->stop_expected = 1;
}

int
linux_attach (int pid)
{
  struct process_info *process;

  linux_attach_lwp (pid, pid);

  /* Don't ignore the initial SIGSTOP if we just attached to this process.  */
  process = (struct process_info *) find_inferior_id (&all_processes, pid);
  process->stop_expected = 0;

  return 0;
}

/* Kill the inferior process.  Make us have no inferior.  */

static void
linux_kill_one_process (struct inferior_list_entry *entry)
{
  struct thread_info *thread = (struct thread_info *) entry;
  struct process_info *process = get_thread_process (thread);
  int wstat;

  do
    {
      ptrace (PTRACE_KILL, pid_of (process), 0, 0);

      /* Make sure it died.  The loop is most likely unnecessary.  */
      wstat = linux_wait_for_event (thread);
    } while (WIFSTOPPED (wstat));
}

/* Return nonzero if the given thread is still alive.  */
static void
linux_kill (void)
{
  for_each_inferior (&all_threads, linux_kill_one_process);
}

static int
linux_thread_alive (int tid)
{
  if (find_inferior_id (&all_threads, tid) != NULL)
    return 1;
  else
    return 0;
}

/* Return nonzero if this process stopped at a breakpoint which
   no longer appears to be inserted.  Also adjust the PC
   appropriately to resume where the breakpoint used to be.  */
static int
check_removed_breakpoint (struct process_info *event_child)
{
  CORE_ADDR stop_pc;
  struct thread_info *saved_inferior;

  if (event_child->pending_is_breakpoint == 0)
    return 0;

  if (debug_threads)
    fprintf (stderr, "Checking for breakpoint.\n");

  saved_inferior = current_inferior;
  current_inferior = get_process_thread (event_child);

  stop_pc = get_stop_pc ();

  /* If the PC has changed since we stopped, then we shouldn't do
     anything.  This happens if, for instance, GDB handled the
     decr_pc_after_break subtraction itself.  */
  if (stop_pc != event_child->pending_stop_pc)
    {
      if (debug_threads)
	fprintf (stderr, "Ignoring, PC was changed.\n");

      event_child->pending_is_breakpoint = 0;
      current_inferior = saved_inferior;
      return 0;
    }

  /* If the breakpoint is still there, we will report hitting it.  */
  if ((*the_low_target.breakpoint_at) (stop_pc))
    {
      if (debug_threads)
	fprintf (stderr, "Ignoring, breakpoint is still present.\n");
      current_inferior = saved_inferior;
      return 0;
    }

  if (debug_threads)
    fprintf (stderr, "Removed breakpoint.\n");

  /* For decr_pc_after_break targets, here is where we perform the
     decrement.  We go immediately from this function to resuming,
     and can not safely call get_stop_pc () again.  */
  if (the_low_target.set_pc != NULL)
    (*the_low_target.set_pc) (stop_pc);

  /* We consumed the pending SIGTRAP.  */
  event_child->status_pending_p = 0;
  event_child->status_pending = 0;

  current_inferior = saved_inferior;
  return 1;
}

/* Return 1 if this process has an interesting status pending.  This function
   may silently resume an inferior process.  */
static int
status_pending_p (struct inferior_list_entry *entry, void *dummy)
{
  struct process_info *process = (struct process_info *) entry;

  if (process->status_pending_p)
    if (check_removed_breakpoint (process))
      {
	/* This thread was stopped at a breakpoint, and the breakpoint
	   is now gone.  We were told to continue (or step...) all threads,
	   so GDB isn't trying to single-step past this breakpoint.
	   So instead of reporting the old SIGTRAP, pretend we got to
	   the breakpoint just after it was removed instead of just
	   before; resume the process.  */
	linux_resume_one_process (&process->head, 0, 0);
	return 0;
      }

  return process->status_pending_p;
}

static void
linux_wait_for_process (struct process_info **childp, int *wstatp)
{
  int ret;
  int to_wait_for = -1;

  if (*childp != NULL)
    to_wait_for = (*childp)->lwpid;

  while (1)
    {
      ret = waitpid (to_wait_for, wstatp, WNOHANG);

      if (ret == -1)
	{
	  if (errno != ECHILD)
	    perror_with_name ("waitpid");
	}
      else if (ret > 0)
	break;

      ret = waitpid (to_wait_for, wstatp, WNOHANG | __WCLONE);

      if (ret == -1)
	{
	  if (errno != ECHILD)
	    perror_with_name ("waitpid (WCLONE)");
	}
      else if (ret > 0)
	break;

      usleep (1000);
    }

  if (debug_threads
      && (!WIFSTOPPED (*wstatp)
	  || (WSTOPSIG (*wstatp) != 32
	      && WSTOPSIG (*wstatp) != 33)))
    fprintf (stderr, "Got an event from %d (%x)\n", ret, *wstatp);

  if (to_wait_for == -1)
    *childp = (struct process_info *) find_inferior_id (&all_processes, ret);

  (*childp)->stopped = 1;
  (*childp)->pending_is_breakpoint = 0;

  if (debug_threads
      && WIFSTOPPED (*wstatp))
    {
      current_inferior = (struct thread_info *)
	find_inferior_id (&all_threads, (*childp)->tid);
      /* For testing only; i386_stop_pc prints out a diagnostic.  */
      if (the_low_target.get_pc != NULL)
	get_stop_pc ();
    }
}

static int
linux_wait_for_event (struct thread_info *child)
{
  CORE_ADDR stop_pc;
  struct process_info *event_child;
  int wstat;

  /* Check for a process with a pending status.  */
  /* It is possible that the user changed the pending task's registers since
     it stopped.  We correctly handle the change of PC if we hit a breakpoint
     (in check_removed_breakpoints); signals should be reported anyway.  */
  if (child == NULL)
    {
      event_child = (struct process_info *)
	find_inferior (&all_processes, status_pending_p, NULL);
      if (debug_threads && event_child)
	fprintf (stderr, "Got a pending child %d\n", event_child->lwpid);
    }
  else
    {
      event_child = get_thread_process (child);
      if (event_child->status_pending_p
	  && check_removed_breakpoint (event_child))
	event_child = NULL;
    }

  if (event_child != NULL)
    {
      if (event_child->status_pending_p)
	{
	  if (debug_threads)
	    fprintf (stderr, "Got an event from pending child %d (%04x)\n",
		     event_child->lwpid, event_child->status_pending);
	  wstat = event_child->status_pending;
	  event_child->status_pending_p = 0;
	  event_child->status_pending = 0;
	  current_inferior = get_process_thread (event_child);
	  return wstat;
	}
    }

  /* We only enter this loop if no process has a pending wait status.  Thus
     any action taken in response to a wait status inside this loop is
     responding as soon as we detect the status, not after any pending
     events.  */
  while (1)
    {
      if (child == NULL)
	event_child = NULL;
      else
	event_child = get_thread_process (child);

      linux_wait_for_process (&event_child, &wstat);

      if (event_child == NULL)
	error ("event from unknown child");

      current_inferior = (struct thread_info *)
	find_inferior_id (&all_threads, event_child->tid);

      if (using_threads)
	{
	  /* Check for thread exit.  */
	  if (! WIFSTOPPED (wstat))
	    {
	      if (debug_threads)
		fprintf (stderr, "Thread %d (LWP %d) exiting\n",
			 event_child->tid, event_child->head.id);

	      /* If the last thread is exiting, just return.  */
	      if (all_threads.head == all_threads.tail)
		return wstat;

	      dead_thread_notify (event_child->tid);

	      remove_inferior (&all_processes, &event_child->head);
	      free (event_child);
	      remove_thread (current_inferior);
	      current_inferior = (struct thread_info *) all_threads.head;

	      /* If we were waiting for this particular child to do something...
		 well, it did something.  */
	      if (child != NULL)
		return wstat;

	      /* Wait for a more interesting event.  */
	      continue;
	    }

	  if (WIFSTOPPED (wstat)
	      && WSTOPSIG (wstat) == SIGSTOP
	      && event_child->stop_expected)
	    {
	      if (debug_threads)
		fprintf (stderr, "Expected stop.\n");
	      event_child->stop_expected = 0;
	      linux_resume_one_process (&event_child->head,
					event_child->stepping, 0);
	      continue;
	    }

	  /* FIXME drow/2002-06-09: Get signal numbers from the inferior's
	     thread library?  */
	  if (WIFSTOPPED (wstat)
	      && (WSTOPSIG (wstat) == SIGRTMIN
		  || WSTOPSIG (wstat) == SIGRTMIN + 1))
	    {
	      if (debug_threads)
		fprintf (stderr, "Ignored signal %d for %d (LWP %d).\n",
			 WSTOPSIG (wstat), event_child->tid,
			 event_child->head.id);
	      linux_resume_one_process (&event_child->head,
					event_child->stepping,
					WSTOPSIG (wstat));
	      continue;
	    }
	}

      /* If this event was not handled above, and is not a SIGTRAP, report
	 it.  */
      if (!WIFSTOPPED (wstat) || WSTOPSIG (wstat) != SIGTRAP)
	return wstat;

      /* If this target does not support breakpoints, we simply report the
	 SIGTRAP; it's of no concern to us.  */
      if (the_low_target.get_pc == NULL)
	return wstat;

      stop_pc = get_stop_pc ();

      /* bp_reinsert will only be set if we were single-stepping.
	 Notice that we will resume the process after hitting
	 a gdbserver breakpoint; single-stepping to/over one
	 is not supported (yet).  */
      if (event_child->bp_reinsert != 0)
	{
	  if (debug_threads)
	    fprintf (stderr, "Reinserted breakpoint.\n");
	  reinsert_breakpoint (event_child->bp_reinsert);
	  event_child->bp_reinsert = 0;

	  /* Clear the single-stepping flag and SIGTRAP as we resume.  */
	  linux_resume_one_process (&event_child->head, 0, 0);
	  continue;
	}

      if (debug_threads)
	fprintf (stderr, "Hit a (non-reinsert) breakpoint.\n");

      if (check_breakpoints (stop_pc) != 0)
	{
	  /* We hit one of our own breakpoints.  We mark it as a pending
	     breakpoint, so that check_removed_breakpoints () will do the PC
	     adjustment for us at the appropriate time.  */
	  event_child->pending_is_breakpoint = 1;
	  event_child->pending_stop_pc = stop_pc;

	  /* Now we need to put the breakpoint back.  We continue in the event
	     loop instead of simply replacing the breakpoint right away,
	     in order to not lose signals sent to the thread that hit the
	     breakpoint.  Unfortunately this increases the window where another
	     thread could sneak past the removed breakpoint.  For the current
	     use of server-side breakpoints (thread creation) this is
	     acceptable; but it needs to be considered before this breakpoint
	     mechanism can be used in more general ways.  For some breakpoints
	     it may be necessary to stop all other threads, but that should
	     be avoided where possible.

	     If breakpoint_reinsert_addr is NULL, that means that we can
	     use PTRACE_SINGLESTEP on this platform.  Uninsert the breakpoint,
	     mark it for reinsertion, and single-step.

	     Otherwise, call the target function to figure out where we need
	     our temporary breakpoint, create it, and continue executing this
	     process.  */
	  if (the_low_target.breakpoint_reinsert_addr == NULL)
	    {
	      event_child->bp_reinsert = stop_pc;
	      uninsert_breakpoint (stop_pc);
	      linux_resume_one_process (&event_child->head, 1, 0);
	    }
	  else
	    {
	      reinsert_breakpoint_by_bp
		(stop_pc, (*the_low_target.breakpoint_reinsert_addr) ());
	      linux_resume_one_process (&event_child->head, 0, 0);
	    }

	  continue;
	}

      /* If we were single-stepping, we definitely want to report the
	 SIGTRAP.  The single-step operation has completed, so also
         clear the stepping flag; in general this does not matter, 
	 because the SIGTRAP will be reported to the client, which
	 will give us a new action for this thread, but clear it for
	 consistency anyway.  It's safe to clear the stepping flag
         because the only consumer of get_stop_pc () after this point
	 is check_removed_breakpoints, and pending_is_breakpoint is not
	 set.  It might be wiser to use a step_completed flag instead.  */
      if (event_child->stepping)
	{
	  event_child->stepping = 0;
	  return wstat;
	}

      /* A SIGTRAP that we can't explain.  It may have been a breakpoint.
	 Check if it is a breakpoint, and if so mark the process information
	 accordingly.  This will handle both the necessary fiddling with the
	 PC on decr_pc_after_break targets and suppressing extra threads
	 hitting a breakpoint if two hit it at once and then GDB removes it
	 after the first is reported.  Arguably it would be better to report
	 multiple threads hitting breakpoints simultaneously, but the current
	 remote protocol does not allow this.  */
      if ((*the_low_target.breakpoint_at) (stop_pc))
	{
	  event_child->pending_is_breakpoint = 1;
	  event_child->pending_stop_pc = stop_pc;
	}

      return wstat;
    }

  /* NOTREACHED */
  return 0;
}

/* Wait for process, returns status.  */

static unsigned char
linux_wait (char *status)
{
  int w;
  struct thread_info *child = NULL;

retry:
  /* If we were only supposed to resume one thread, only wait for
     that thread - if it's still alive.  If it died, however - which
     can happen if we're coming from the thread death case below -
     then we need to make sure we restart the other threads.  We could
     pick a thread at random or restart all; restarting all is less
     arbitrary.  */
  if (cont_thread > 0)
    {
      child = (struct thread_info *) find_inferior_id (&all_threads,
						       cont_thread);

      /* No stepping, no signal - unless one is pending already, of course.  */
      if (child == NULL)
	linux_resume (0, 0);
    }

  enable_async_io ();
  w = linux_wait_for_event (child);
  stop_all_processes ();
  disable_async_io ();

  /* If we are waiting for a particular child, and it exited,
     linux_wait_for_event will return its exit status.  Similarly if
     the last child exited.  If this is not the last child, however,
     do not report it as exited until there is a 'thread exited' response
     available in the remote protocol.  Instead, just wait for another event.
     This should be safe, because if the thread crashed we will already
     have reported the termination signal to GDB; that should stop any
     in-progress stepping operations, etc.

     Report the exit status of the last thread to exit.  This matches
     LinuxThreads' behavior.  */

  if (all_threads.head == all_threads.tail)
    {
      if (WIFEXITED (w))
	{
	  fprintf (stderr, "\nChild exited with retcode = %x \n", WEXITSTATUS (w));
	  *status = 'W';
	  clear_inferiors ();
	  return ((unsigned char) WEXITSTATUS (w));
	}
      else if (!WIFSTOPPED (w))
	{
	  fprintf (stderr, "\nChild terminated with signal = %x \n", WTERMSIG (w));
	  clear_inferiors ();
	  *status = 'X';
	  return ((unsigned char) WTERMSIG (w));
	}
    }
  else
    {
      if (!WIFSTOPPED (w))
	goto retry;
    }

  *status = 'T';
  return ((unsigned char) WSTOPSIG (w));
}

static void
send_sigstop (struct inferior_list_entry *entry)
{
  struct process_info *process = (struct process_info *) entry;

  if (process->stopped)
    return;

  /* If we already have a pending stop signal for this process, don't
     send another.  */
  if (process->stop_expected)
    {
      process->stop_expected = 0;
      return;
    }

  if (debug_threads)
    fprintf (stderr, "Sending sigstop to process %d\n", process->head.id);

  kill (process->head.id, SIGSTOP);
  process->sigstop_sent = 1;
}

static void
wait_for_sigstop (struct inferior_list_entry *entry)
{
  struct process_info *process = (struct process_info *) entry;
  struct thread_info *saved_inferior, *thread;
  int wstat, saved_tid;

  if (process->stopped)
    return;

  saved_inferior = current_inferior;
  saved_tid = ((struct inferior_list_entry *) saved_inferior)->id;
  thread = (struct thread_info *) find_inferior_id (&all_threads,
						    process->tid);
  wstat = linux_wait_for_event (thread);

  /* If we stopped with a non-SIGSTOP signal, save it for later
     and record the pending SIGSTOP.  If the process exited, just
     return.  */
  if (WIFSTOPPED (wstat)
      && WSTOPSIG (wstat) != SIGSTOP)
    {
      if (debug_threads)
	fprintf (stderr, "Stopped with non-sigstop signal\n");
      process->status_pending_p = 1;
      process->status_pending = wstat;
      process->stop_expected = 1;
    }

  if (linux_thread_alive (saved_tid))
    current_inferior = saved_inferior;
  else
    {
      if (debug_threads)
	fprintf (stderr, "Previously current thread died.\n");

      /* Set a valid thread as current.  */
      set_desired_inferior (0);
    }
}

static void
stop_all_processes (void)
{
  stopping_threads = 1;
  for_each_inferior (&all_processes, send_sigstop);
  for_each_inferior (&all_processes, wait_for_sigstop);
  stopping_threads = 0;
}

/* Resume execution of the inferior process.
   If STEP is nonzero, single-step it.
   If SIGNAL is nonzero, give it that signal.  */

static void
linux_resume_one_process (struct inferior_list_entry *entry,
			  int step, int signal)
{
  struct process_info *process = (struct process_info *) entry;
  struct thread_info *saved_inferior;

  if (process->stopped == 0)
    return;

  /* If we have pending signals or status, and a new signal, enqueue the
     signal.  Also enqueue the signal if we are waiting to reinsert a
     breakpoint; it will be picked up again below.  */
  if (signal != 0
      && (process->status_pending_p || process->pending_signals != NULL
	  || process->bp_reinsert != 0))
    {
      struct pending_signals *p_sig;
      p_sig = malloc (sizeof (*p_sig));
      p_sig->prev = process->pending_signals;
      p_sig->signal = signal;
      process->pending_signals = p_sig;
    }

  if (process->status_pending_p)
    return;

  saved_inferior = current_inferior;
  current_inferior = get_process_thread (process);

  if (debug_threads)
    fprintf (stderr, "Resuming process %d (%s, signal %d, stop %s)\n", inferior_pid,
	     step ? "step" : "continue", signal,
	     process->stop_expected ? "expected" : "not expected");

  /* This bit needs some thinking about.  If we get a signal that
     we must report while a single-step reinsert is still pending,
     we often end up resuming the thread.  It might be better to
     (ew) allow a stack of pending events; then we could be sure that
     the reinsert happened right away and not lose any signals.

     Making this stack would also shrink the window in which breakpoints are
     uninserted (see comment in linux_wait_for_process) but not enough for
     complete correctness, so it won't solve that problem.  It may be
     worthwhile just to solve this one, however.  */
  if (process->bp_reinsert != 0)
    {
      if (debug_threads)
	fprintf (stderr, "  pending reinsert at %08lx", (long)process->bp_reinsert);
      if (step == 0)
	fprintf (stderr, "BAD - reinserting but not stepping.\n");
      step = 1;

      /* Postpone any pending signal.  It was enqueued above.  */
      signal = 0;
    }

  check_removed_breakpoint (process);

  if (debug_threads && the_low_target.get_pc != NULL) 
    {
      fprintf (stderr, "  ");
      (long) (*the_low_target.get_pc) ();
    }

  /* If we have pending signals, consume one unless we are trying to reinsert
     a breakpoint.  */
  if (process->pending_signals != NULL && process->bp_reinsert == 0)
    {
      struct pending_signals **p_sig;

      p_sig = &process->pending_signals;
      while ((*p_sig)->prev != NULL)
	p_sig = &(*p_sig)->prev;

      signal = (*p_sig)->signal;
      free (*p_sig);
      *p_sig = NULL;
    }

  regcache_invalidate_one ((struct inferior_list_entry *)
			   get_process_thread (process));
  errno = 0;
  process->stopped = 0;
  process->stepping = step;
  ptrace (step ? PTRACE_SINGLESTEP : PTRACE_CONT, process->lwpid, 0, signal);

  current_inferior = saved_inferior;
  if (errno)
    perror_with_name ("ptrace");
}

/* This function is called once per process other than the first
   one.  The first process we are told the signal to continue
   with, and whether to step or continue; for all others, any
   existing signals will be marked in status_pending_p to be
   reported momentarily, and we preserve the stepping flag.  */
static void
linux_continue_one_process (struct inferior_list_entry *entry)
{
  struct process_info *process;

  process = (struct process_info *) entry;
  linux_resume_one_process (entry, process->stepping, 0);
}

static void
linux_resume (int step, int signal)
{
  struct process_info *process;

  process = get_thread_process (current_inferior);

  /* If the current process has a status pending, this signal will
     be enqueued and sent later.  */
  linux_resume_one_process (&process->head, step, signal);

  if (cont_thread == 0 || cont_thread == -1)
    for_each_inferior (&all_processes, linux_continue_one_process);
}

#ifdef HAVE_LINUX_USRREGS

int
register_addr (int regnum)
{
  int addr;

  if (regnum < 0 || regnum >= the_low_target.num_regs)
    error ("Invalid register number %d.", regnum);

  addr = the_low_target.regmap[regnum];
  if (addr == -1)
    addr = 0;

  return addr;
}

/* Fetch one register.  */
static void
fetch_register (int regno)
{
  CORE_ADDR regaddr;
  register int i;
  char *buf;

  if (regno >= the_low_target.num_regs)
    return;
  if ((*the_low_target.cannot_fetch_register) (regno))
    return;

  regaddr = register_addr (regno);
  if (regaddr == -1)
    return;
  buf = alloca (register_size (regno));
  for (i = 0; i < register_size (regno); i += sizeof (PTRACE_XFER_TYPE))
    {
      errno = 0;
      *(PTRACE_XFER_TYPE *) (buf + i) =
	ptrace (PTRACE_PEEKUSER, inferior_pid, (PTRACE_ARG3_TYPE) regaddr, 0);
      regaddr += sizeof (PTRACE_XFER_TYPE);
      if (errno != 0)
	{
	  /* Warning, not error, in case we are attached; sometimes the
	     kernel doesn't let us at the registers.  */
	  char *err = strerror (errno);
	  char *msg = alloca (strlen (err) + 128);
	  sprintf (msg, "reading register %d: %s", regno, err);
	  error (msg);
	  goto error_exit;
	}
    }
  supply_register (regno, buf);

error_exit:;
}

/* Fetch all registers, or just one, from the child process.  */
static void
usr_fetch_inferior_registers (int regno)
{
  if (regno == -1 || regno == 0)
    for (regno = 0; regno < the_low_target.num_regs; regno++)
      fetch_register (regno);
  else
    fetch_register (regno);
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */
static void
usr_store_inferior_registers (int regno)
{
  CORE_ADDR regaddr;
  int i;
  char *buf;

  if (regno >= 0)
    {
      if (regno >= the_low_target.num_regs)
	return;

      if ((*the_low_target.cannot_store_register) (regno) == 1)
	return;

      regaddr = register_addr (regno);
      if (regaddr == -1)
	return;
      errno = 0;
      buf = alloca (register_size (regno));
      collect_register (regno, buf);
      for (i = 0; i < register_size (regno); i += sizeof (PTRACE_XFER_TYPE))
	{
	  errno = 0;
	  ptrace (PTRACE_POKEUSER, inferior_pid, (PTRACE_ARG3_TYPE) regaddr,
		  *(int *) (buf + i));
	  if (errno != 0)
	    {
	      if ((*the_low_target.cannot_store_register) (regno) == 0)
		{
		  char *err = strerror (errno);
		  char *msg = alloca (strlen (err) + 128);
		  sprintf (msg, "writing register %d: %s",
			   regno, err);
		  error (msg);
		  return;
		}
	    }
	  regaddr += sizeof (int);
	}
    }
  else
    for (regno = 0; regno < the_low_target.num_regs; regno++)
      usr_store_inferior_registers (regno);
}
#endif /* HAVE_LINUX_USRREGS */



#ifdef HAVE_LINUX_REGSETS

static int
regsets_fetch_inferior_registers ()
{
  struct regset_info *regset;

  regset = target_regsets;

  while (regset->size >= 0)
    {
      void *buf;
      int res;

      if (regset->size == 0)
	{
	  regset ++;
	  continue;
	}

      buf = malloc (regset->size);
      res = ptrace (regset->get_request, inferior_pid, 0, buf);
      if (res < 0)
	{
	  if (errno == EIO)
	    {
	      /* If we get EIO on the first regset, do not try regsets again.
		 If we get EIO on a later regset, disable that regset.  */
	      if (regset == target_regsets)
		{
		  use_regsets_p = 0;
		  return -1;
		}
	      else
		{
		  regset->size = 0;
		  continue;
		}
	    }
	  else
	    {
	      char s[256];
	      sprintf (s, "ptrace(regsets_fetch_inferior_registers) PID=%d",
		       inferior_pid);
	      perror (s);
	    }
	}
      regset->store_function (buf);
      regset ++;
    }
  return 0;
}

static int
regsets_store_inferior_registers ()
{
  struct regset_info *regset;

  regset = target_regsets;

  while (regset->size >= 0)
    {
      void *buf;
      int res;

      if (regset->size == 0)
	{
	  regset ++;
	  continue;
	}

      buf = malloc (regset->size);
      regset->fill_function (buf);
      res = ptrace (regset->set_request, inferior_pid, 0, buf);
      if (res < 0)
	{
	  if (errno == EIO)
	    {
	      /* If we get EIO on the first regset, do not try regsets again.
		 If we get EIO on a later regset, disable that regset.  */
	      if (regset == target_regsets)
		{
		  use_regsets_p = 0;
		  return -1;
		}
	      else
		{
		  regset->size = 0;
		  continue;
		}
	    }
	  else
	    {
	      perror ("Warning: ptrace(regsets_store_inferior_registers)");
	    }
	}
      regset ++;
      free (buf);
    }
  return 0;
}

#endif /* HAVE_LINUX_REGSETS */


void
linux_fetch_registers (int regno)
{
#ifdef HAVE_LINUX_REGSETS
  if (use_regsets_p)
    {
      if (regsets_fetch_inferior_registers () == 0)
	return;
    }
#endif
#ifdef HAVE_LINUX_USRREGS
  usr_fetch_inferior_registers (regno);
#endif
}

void
linux_store_registers (int regno)
{
#ifdef HAVE_LINUX_REGSETS
  if (use_regsets_p)
    {
      if (regsets_store_inferior_registers () == 0)
	return;
    }
#endif
#ifdef HAVE_LINUX_USRREGS
  usr_store_inferior_registers (regno);
#endif
}


/* Copy LEN bytes from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.  */

static void
linux_read_memory (CORE_ADDR memaddr, char *myaddr, int len)
{
  register int i;
  /* Round starting address down to longword boundary.  */
  register CORE_ADDR addr = memaddr & -(CORE_ADDR) sizeof (PTRACE_XFER_TYPE);
  /* Round ending address up; get number of longwords that makes.  */
  register int count 
    = (((memaddr + len) - addr) + sizeof (PTRACE_XFER_TYPE) - 1) 
      / sizeof (PTRACE_XFER_TYPE);
  /* Allocate buffer of that many longwords.  */
  register PTRACE_XFER_TYPE *buffer 
    = (PTRACE_XFER_TYPE *) alloca (count * sizeof (PTRACE_XFER_TYPE));

  /* Read all the longwords */
  for (i = 0; i < count; i++, addr += sizeof (PTRACE_XFER_TYPE))
    {
      buffer[i] = ptrace (PTRACE_PEEKTEXT, inferior_pid, (PTRACE_ARG3_TYPE) addr, 0);
    }

  /* Copy appropriate bytes out of the buffer.  */
  memcpy (myaddr, (char *) buffer + (memaddr & (sizeof (PTRACE_XFER_TYPE) - 1)), len);
}

/* Copy LEN bytes of data from debugger memory at MYADDR
   to inferior's memory at MEMADDR.
   On failure (cannot write the inferior)
   returns the value of errno.  */

static int
linux_write_memory (CORE_ADDR memaddr, const char *myaddr, int len)
{
  register int i;
  /* Round starting address down to longword boundary.  */
  register CORE_ADDR addr = memaddr & -(CORE_ADDR) sizeof (PTRACE_XFER_TYPE);
  /* Round ending address up; get number of longwords that makes.  */
  register int count
  = (((memaddr + len) - addr) + sizeof (PTRACE_XFER_TYPE) - 1) / sizeof (PTRACE_XFER_TYPE);
  /* Allocate buffer of that many longwords.  */
  register PTRACE_XFER_TYPE *buffer = (PTRACE_XFER_TYPE *) alloca (count * sizeof (PTRACE_XFER_TYPE));
  extern int errno;

  if (debug_threads)
    {
      fprintf (stderr, "Writing %02x to %08lx\n", (unsigned)myaddr[0], (long)memaddr);
    }

  /* Fill start and end extra bytes of buffer with existing memory data.  */

  buffer[0] = ptrace (PTRACE_PEEKTEXT, inferior_pid,
		      (PTRACE_ARG3_TYPE) addr, 0);

  if (count > 1)
    {
      buffer[count - 1]
	= ptrace (PTRACE_PEEKTEXT, inferior_pid,
		  (PTRACE_ARG3_TYPE) (addr + (count - 1)
				      * sizeof (PTRACE_XFER_TYPE)),
		  0);
    }

  /* Copy data to be written over corresponding part of buffer */

  memcpy ((char *) buffer + (memaddr & (sizeof (PTRACE_XFER_TYPE) - 1)), myaddr, len);

  /* Write the entire buffer.  */

  for (i = 0; i < count; i++, addr += sizeof (PTRACE_XFER_TYPE))
    {
      errno = 0;
      ptrace (PTRACE_POKETEXT, inferior_pid, (PTRACE_ARG3_TYPE) addr, buffer[i]);
      if (errno)
	return errno;
    }

  return 0;
}

static void
linux_look_up_symbols (void)
{
#ifdef USE_THREAD_DB
  if (using_threads)
    return;

  using_threads = thread_db_init ();
#endif
}


static struct target_ops linux_target_ops = {
  linux_create_inferior,
  linux_attach,
  linux_kill,
  linux_thread_alive,
  linux_resume,
  linux_wait,
  linux_fetch_registers,
  linux_store_registers,
  linux_read_memory,
  linux_write_memory,
  linux_look_up_symbols,
};

static void
linux_init_signals ()
{
  /* FIXME drow/2002-06-09: As above, we should check with LinuxThreads
     to find what the cancel signal actually is.  */
  signal (SIGRTMIN+1, SIG_IGN);
}

void
initialize_low (void)
{
  using_threads = 0;
  set_target_ops (&linux_target_ops);
  set_breakpoint_data (the_low_target.breakpoint,
		       the_low_target.breakpoint_len);
  init_registers ();
  linux_init_signals ();
}
