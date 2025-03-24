--- procfs.c	Sun Jun  8 12:53:46 2003
+++ procfs.c.new	Sun Jun  8 12:58:40 2003
@@ -184,7 +184,12 @@ init_procfs_ops (void)
   procfs_ops.to_has_stack           = 1;
   procfs_ops.to_has_registers       = 1;
   procfs_ops.to_stratum             = process_stratum;
-  procfs_ops.to_has_thread_control  = tc_schedlock;
+
+#ifdef __INTERIX  
+  procfs_ops.to_has_thread_control  = tc_none;
+#else
+   procfs_ops.to_has_thread_control  = tc_schedlock;
+#endif
   procfs_ops.to_find_memory_regions = proc_find_memory_regions;
   procfs_ops.to_make_corefile_notes = procfs_make_note_section;
   procfs_ops.to_can_use_hw_breakpoint = procfs_can_use_hw_breakpoint;
@@ -3101,7 +3106,7 @@ proc_get_nthreads (procinfo *pi)
 }
 
 #else
-#if defined (SYS_lwpcreate) || defined (SYS_lwp_create) /* FIXME: multiple */
+#if defined (SYS_lwpcreate) || defined (SYS_lwp_create) || defined(__INTERIX)/* FIXME: multiple */
 /*
  * Solaris and Unixware version
  */
@@ -3144,7 +3149,7 @@ proc_get_nthreads (procinfo *pi)
  * thread that is currently executing.
  */
 
-#if defined (SYS_lwpcreate) || defined (SYS_lwp_create) /* FIXME: multiple */
+#if defined (SYS_lwpcreate) || defined (SYS_lwp_create) || defined(__INTERIX)/* FIXME: multiple */
 /*
  * Solaris and Unixware version
  */

