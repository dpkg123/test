In fork-child.c, startup_inferior():  The loop which counts down 
until the beginning of the target executable is reached treats
unexpected signals (etc.) specially, and continues without counting
them.  Such signals CAN cause surprises if something else goes wrong,
so report that they occurred to the user.  (In particular, if 
SIGTTIN or SIGTTOUT should occur, there could be some blockage of
I/O.)  Count them, and if way too many, quit. Also...

Avoid infinite loop: startup_inferior() should test for non-negative
pending_execs, not exactly zero.  It has been observed to go negative
with other errors.

inflow.c/terminal_inferior(): it's possible for startup_inferior()
to call resume() (under the conditions above) before it
calls terminal_init_inferior().  This can cause terminal_inferior
to be called with inferior_ttystate == NULL.  Add code to simply
return if inferior_ttystate is NULL.

Fri Dec  3 10:07:20 PST 1999  Donn Terry <donnte@microsoft.com>
	* fork-child.c (startup_inferior): Count/report unexpected traps.
	  Be sure pending_execs < 0 won't loop forever.
	* inflow.c (terminal_inferior): if arg is NULL, return.

Index: src/gdb/fork-child.c
===================================================================
RCS file: /dev/fs/H/rupp/devel-local-repository/src/gdb/fork-child.c,v
retrieving revision 1.1.1.1
diff -p -u -r1.1.1.1 fork-child.c
--- src/gdb/fork-child.c	2001/12/23 00:34:46	1.1.1.1
+++ src/gdb/fork-child.c	2001/12/23 23:43:06
@@ -508,7 +508,9 @@ startup_inferior (int ntraps)
 {
   int pending_execs = ntraps;
   int terminal_initted;
+  int bad_traps = 0;
 
+
   /* The process was started by the fork that created it,
      but it will have stopped one instruction after execing the shell.
      Here we must get it up to actual execution of the real program.  */
@@ -535,8 +537,15 @@ startup_inferior (int ntraps)
       wait_for_inferior ();
       if (stop_signal != TARGET_SIGNAL_TRAP)
 	{
+          fprintf_unfiltered (gdb_stderr, 
+	    "Got unexpected stop signal %d during inferior startup; ignoring\n",
+		 stop_signal);
 	  /* Let shell child handle its own signals in its own way */
 	  /* FIXME, what if child has exit()ed?  Must exit loop somehow */
+	  if (bad_traps++ > 2*ntraps) {
+	      fprintf_unfiltered(gdb_stderr, "... giving up... too many\n");
+	      break;
+	  }
 	  resume (0, stop_signal);
 	}
       else
@@ -559,7 +568,7 @@ startup_inferior (int ntraps)
 	    }
 
 	  pending_execs = pending_execs - 1;
-	  if (0 == pending_execs)
+	  if (0 >= pending_execs)
 	    break;
 
 	  resume (0, TARGET_SIGNAL_0);	/* Just make it go on */
Index: src/gdb/inflow.c
===================================================================
RCS file: /dev/fs/H/rupp/devel-local-repository/src/gdb/inflow.c,v
retrieving revision 1.1.1.1
diff -p -u -r1.1.1.1 inflow.c
--- src/gdb/inflow.c	2001/12/23 00:34:49	1.1.1.1
+++ src/gdb/inflow.c	2001/12/23 23:43:06
@@ -220,6 +220,12 @@ terminal_init_inferior (void)
 void
 terminal_inferior (void)
 {
+
+  /* not yet initialized; called before init was called. This is reasonable:
+     startup_inferior can do this if it gets an unexpected signal (directed
+     at the intermediate shell).  Ignore it until we're initialized */
+  if (inferior_ttystate == NULL) return;
+
   if (gdb_has_a_terminal () && terminal_is_ours
       && inferior_thisrun_terminal == 0)
     {
