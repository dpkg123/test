The Multiarch stuff also didn't get floating point right; there may
still be multiarch issues with this, but it should be close.

--- i386-interix-nat.c.bak	Thu Nov  7 12:23:30 2002
+++ i386-interix-nat.c	Thu Nov  7 16:59:28 2002
@@ -74,23 +74,135 @@ fill_gregset (gregset_t *gregsetp, int r
       regcache_collect (regi, (void *) (regp + regmap[regi]));
 }
 
+/*  Given a pointer to a floating point register set in Interix /proc format
+    (fpregset_t *), unpack the register contents and supply them as gdb's
+    idea of the current floating point register values. */
+
+/* What is the address of st(N) within the fpregset_t structure F?  */
+#define FP_REG_SIZE 10
+#define FPREGSET_T_FPREG_ADDR(f, n) \
+        ((char *) &(f)->fpstate.fpregs + (n) * FP_REG_SIZE)
+
 /* Fill GDB's register file with the floating-point register values in
    *FPREGSETP.  */
 
-void
+void 
 supply_fpregset (fpregset_t *fpregsetp)
 {
-  i387_supply_fsave ((char *) fpregsetp);
-}
-
+  int i;
+  long l;
+
+  /* Supply the floating-point registers.  */
+  for (i = 0; i < 8; i++)
+    supply_register (FP0_REGNUM + i, FPREGSET_T_FPREG_ADDR (fpregsetp, i));
+
+  l = fpregsetp->fpstate.control & 0xffff;
+  supply_register (FCTRL_REGNUM, (char *) &l);
+  l = fpregsetp->fpstate.status & 0xffff;
+  supply_register (FSTAT_REGNUM, (char *) &l);
+  l = fpregsetp->fpstate.tag & 0xffff;
+  supply_register (FTAG_REGNUM,  (char *) &l);
+  supply_register (FCOFF_REGNUM, (char *) &fpregsetp->fpstate.erroroffset);
+  l = fpregsetp->fpstate.dataselector & 0xffff;
+  supply_register (FDS_REGNUM,   (char *) &l);
+  supply_register (FDOFF_REGNUM, (char *) &fpregsetp->fpstate.dataoffset);
+  
+  /* Extract the code segment and opcode from the "fcs" member.  */
+
+  l = fpregsetp->fpstate.errorselector & 0xffff;
+  supply_register (FCS_REGNUM, (char *) &l);
+
+  l = (fpregsetp->fpstate.errorselector >> 16) & ((1 << 11) - 1);
+  supply_register (FOP_REGNUM, (char *) &l);
+}
+
+
+extern char * register_buffer (struct regcache *regcache, int regnum);
+
 /* Given a pointer to a floating point register set in (fpregset_t *)
    format, update all of the registers from gdb's idea of the current
-   floating point register set.  */
-
+   floating point register set. 
+   - FPREGSETP points to the structure to be filled. 
+     NOTE: this is a structure, and altho we use memcpy to move the
+     data around, it should be treated as a structure as much as possible. */
+
 void
-fill_fpregset (fpregset_t *fpregsetp, int regno)
-{
-  i387_fill_fsave ((char *) fpregsetp, regno);
+fill_fpregset (fpregset_t *fpregsetp,
+	       int regno)
+{
+  int i;
+
+#define fill(MEMBER, REGNO) memcpy (&fpregsetp->fpstate.MEMBER, 	      \
+	    register_buffer (current_regcache, REGNO),			      \
+	    sizeof (fpregsetp->fpstate.MEMBER))
+
+  for (i = FP0_REGNUM; i < XMM0_REGNUM; i++)
+    {
+      /* Only do the registers we were asked to. */
+      if (regno != -1 && regno != i)
+	{
+	  continue;
+	}
+
+      if (i == FCTRL_REGNUM)
+	{
+	  fill (control, FCTRL_REGNUM); 
+	  continue;
+	}
+      if (i == FSTAT_REGNUM)
+	{
+	  fill (status, FSTAT_REGNUM); 
+	  continue;
+	}
+      if (i == FTAG_REGNUM)
+	{
+	  fill (tag, FTAG_REGNUM); 
+	  continue;
+	}
+      if (i == FCOFF_REGNUM)
+	{
+	  fill (erroroffset, FCOFF_REGNUM); 
+	  continue;
+	}
+      if (i == FDOFF_REGNUM)
+	{
+	  fill (dataoffset, FDOFF_REGNUM); 
+	  continue;
+	}
+      if (i == FDS_REGNUM)
+	{
+	  fill (dataselector, FDS_REGNUM); 
+	  continue;
+	}
+
+      if (i >= FP0_REGNUM && i <= FP0_REGNUM+7)
+	{
+
+	  memcpy (FPREGSET_T_FPREG_ADDR (fpregsetp, i-FP0_REGNUM),
+		  register_buffer (current_regcache, i),
+		  FP_REG_SIZE);
+	  continue;
+	}
+      if (i == FCS_REGNUM)
+	{
+	  fpregsetp->fpstate.errorselector
+	    = (fpregsetp->fpstate.errorselector & ~0xffff)
+	       | ((* (int *) register_buffer (current_regcache, FCS_REGNUM)) 
+		 & 0xffff);
+	  continue;
+	}
+
+      if (i == FOP_REGNUM)
+	{
+	  fpregsetp->fpstate.errorselector
+	    = (fpregsetp->fpstate.errorselector & 0xffff)
+	       | (((*(int *) register_buffer (current_regcache, FOP_REGNUM)) 
+		 & ((1 << 11) - 1)) << 16);
+	  continue;
+	}
+    }
+
+#undef fill
 }
 
 /* Read the values of either the general register set (WHICH equals 0)
--- regcache.c.bak	Thu Nov  7 15:33:51 2002
+++ regcache.c	Thu Nov  7 15:33:54 2002
@@ -486,7 +486,7 @@ register_changed (int regnum)
 /* If REGNUM >= 0, return a pointer to register REGNUM's cache buffer area,
    else return a pointer to the start of the cache buffer.  */
 
-static char *
+char *
 register_buffer (struct regcache *regcache, int regnum)
 {
   return regcache->raw_registers + regcache->descr->register_offset[regnum];
