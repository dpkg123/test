There's a bug in either Interix or WinXP that causes one of the breakpoint
regressions to fail with a what value of "unknown"; while that's being
worked on, this works around it.  Problem also known as the 0x800000CC
bug.  

DO NOT SUBMIT, but needed for reasonable testing.


diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/procfs.c gdb/procfs.c
--- gdb.before/procfs.c	Thu Mar  7 09:19:01 2002
+++ gdb/procfs.c	Fri Mar  8 15:45:42 2002
@@ -4206,6 +4220,8 @@ wait_again:
 		wstat = (what << 8) | 0177;
 		break;
 	      case PR_FAULTED:
+/* TEMP HACKERY */
+if (what == 14) what = FLTBPT;
 		switch (what) {	/* FIXME: FAULTED_USE_SIGINFO */
 #ifdef FLTWATCH
 		case FLTWATCH:
