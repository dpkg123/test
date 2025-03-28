The gdb.asm test made a number of assumptions that weren't, for various
reasons, true in all cases where the test could pass.  The changes below
allow it to pass in those cases.  Also, add a comment to dbxread.c
resolving an unknown related to this.

The imbedded comment gives the details.

diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/testsuite/gdb.asm/Makefile.in gdb/testsuite/gdb.asm/Makefile.in
--- gdb.before/testsuite/gdb.asm/Makefile.in	Thu Mar  7 09:18:57 2002
+++ gdb/testsuite/gdb.asm/Makefile.in	Fri Mar  8 20:38:51 2002
@@ -25,7 +25,7 @@ clean mostlyclean:
 distclean maintainer-clean realclean: clean
 	-rm -f *~ core
 	-rm -f Makefile config.status config.log
-	-rm -f arch.inc
+	-rm -f arch.inc object.inc
 	-rm -f *-init.exp
 	-rm -fr *.log summary detail *.plog *.sum *.psum site.*
 
diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/testsuite/gdb.asm/asm-source.exp gdb/testsuite/gdb.asm/asm-source.exp
--- gdb.before/testsuite/gdb.asm/asm-source.exp	Thu Mar  7 09:18:57 2002
+++ gdb/testsuite/gdb.asm/asm-source.exp	Mon Mar 11 13:25:11 2002
@@ -25,41 +25,78 @@ if $tracelevel then {
 
 #
 # Test debugging assembly level programs.
-# This file uses asmsrc[12].s for input.
-#
-
+# This file uses asmsrc[123].s for input.
+#
+# Some comments on "corner cases": On some systems it's not possible to
+# link an application without both crt0.o and the default libc, because
+# they mutually refer.  For this test to work, it must either use crt0 and libc
+# or stand completely alone.  Since there are good reasons not to use crt0,
+# it needs to stand completely alone.  Since the stabs entries generated
+# by the assembler are mostly SLINE entries (no functions or blocks), there's
+# nothing but the "next" SO entry to terminate the blocks (and thus generate
+# partial symtabs with a proper end value).  That happens "accidentally"
+# if linking with more complete libraries, but not in this case.  (This
+# would also not be a problem if FUN entries were generated.) To cause
+# the insertion of a "next" SO entry follwing asmsrc2.o, we use the (nearly)
+# empty asmsrc4.o. (Ref: read_dbx_symtab.)
+#
+# Additionally, the stack unwinder logic (at least for COFF) looks to see
+# if the PC is in the "entry file".  Putting _start (which identifies
+# the entry file) in the same file as main confuses the unwinder, and it
+# omits unwinding main as well, causing test failures.  Thus, asmsrc3.s
+# contains _start (alone).
+#
+# Not all systems have their default entry point as _start, so we set it.
+#
+# Everything appears to work correctly for "leading underscore" machines,
+# but there is some risk that it might not under some circumstances; if
+# this test is failing by not finding symbols, look for that.
+
 set prms_id 0
 set bug_id 0
 
 set asm-arch ""
 set asm-flags ""
 set link-flags ""
-
+set asm-obj ""
+
 if [istarget "*arm-*-*"] then {
     set asm-arch arm
-}
+    set asm-obj otherobj
+}
 if [istarget "xscale-*-*"] then {
     set asm-arch arm
-}
+    set asm-obj otherobj
+}
 if [istarget "d10v-*-*"] then {
     set asm-arch d10v
-}
+    set asm-obj otherobj
+}
 if [istarget "s390-*-*"] then {
     set asm-arch s390
-}
+    set asm-obj otherobj
+}
 if [istarget "i\[3456\]86-*-*"] then {
     set asm-arch i386
-}
+    set asm-obj otherobj
+}
 if [istarget "m32r*-*"] then {
     set asm-arch m32r
-}
+    set asm-obj otherobj
+}
 if [istarget "sparc-*-*"] then {
     set asm-arch sparc
-}
+    set asm-obj otherobj
+}
 if [istarget "xstormy16-*-*"] then {
     set asm-arch xstormy16
+    set asm-obj otherobj
     set asm-flags "-gdwarf2 -I${srcdir}/${subdir} -I${objdir}/${subdir}"
 }
+if [istarget "i386-*-interix*"] then {
+    set asm-arch i386
+    set asm-obj coff
+}
 if { "${asm-arch}" == "" } {
     gdb_suppress_entire_file "Assembly source test -- not implemented for this target."
 }
@@ -68,7 +105,9 @@ set testfile "asm-source"
 set binfile ${objdir}/${subdir}/${testfile}
 set src1 ${srcdir}/${subdir}/asmsrc1.s
 set src2 ${srcdir}/${subdir}/asmsrc2.s
-
+set src3 ${srcdir}/${subdir}/asmsrc3.s
+set src4 ${srcdir}/${subdir}/asmsrc4.s
+
 if { "${asm-flags}" == "" } {
     #set asm-flags "-Wa,-gstabs,-I${srcdir}/${subdir},-I${objdir}/${subdir}"
     set asm-flags "-gstabs -I${srcdir}/${subdir} -I${objdir}/${subdir}"
@@ -80,20 +119,27 @@ if {[target_assemble ${src1} asmsrc1.o "
 if {[target_assemble ${src2} asmsrc2.o "${asm-flags}"] != ""} then {
      gdb_suppress_entire_file "Testcase compile failed, so all tests in this file will automatically fail."
 }
+if {[target_assemble ${src3} asmsrc3.o "${asm-flags}"] != ""} then {
+     gdb_suppress_entire_file "Testcase compile failed, so all tests in this file will automatically fail."
+}
+if {[target_assemble ${src4} asmsrc4.o "${asm-flags}"] != ""} then {
+     gdb_suppress_entire_file "Testcase compile failed, so all tests in this file will automatically fail."
+}
 
 # Some shared libs won't stand alone (without the startup file), so be sure
 # it's static.  And not all systems start at _start.
+
 set opts "debug ldflags=-nostartfiles ldflags=-nostdlib ldflags=-Wl,-entry,_start ldflags=--static "
 foreach i ${link-flags} {
     append opts " ldflags=$i"
 }
-if { [gdb_compile "asmsrc1.o asmsrc2.o" "${binfile}" executable $opts] != "" } {
+if { [gdb_compile "asmsrc1.o asmsrc2.o asmsrc3.o asmsrc4.o" "${binfile}" executable $opts] != "" } {
      gdb_suppress_entire_file "Testcase compile failed, so all tests in this file will automatically fail."
 }
 
-remote_exec build "mv asmsrc1.o asmsrc2.o ${objdir}/${subdir}"
-
-
+remote_exec build "mv asmsrc1.o asmsrc2.o asmsrc3.o asmsrc4.o ${objdir}/${subdir}"
+
+
 gdb_start
 gdb_reinitialize_dir $srcdir/$subdir
 gdb_load ${binfile}
@@ -162,8 +208,8 @@ gdb_expect {
 gdb_test "list $entry_symbol" ".*gdbasm_startup.*" "list"
 
 # Now try a source file search
-gdb_test "search A routine for foo2 to call" \
-	"39\[ \t\]+comment \"A routine for foo2 to call.\"" "search"
+gdb_test "search Provide something to search for" \
+	"20\[ \t\]+comment \"Provide something to search for\"" "search"
 
 # See if `f' prints the right source file.
 gdb_test "f" ".*asmsrc2\[.\]s:8.*" "f in foo2"
diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/testsuite/gdb.asm/asmsrc1.s gdb/testsuite/gdb.asm/asmsrc1.s
--- gdb.before/testsuite/gdb.asm/asmsrc1.s	Thu Mar  7 09:18:57 2002
+++ gdb/testsuite/gdb.asm/asmsrc1.s	Sun Mar 10 09:40:53 2002
@@ -1,6 +1,6 @@
 	.include "common.inc"
 	.include "arch.inc"
-
+	.include "object.inc"
 comment "WARNING: asm-source.exp checks for line numbers printed by gdb."
 comment "Be careful about changing this file without also changing"
 comment "asm-source.exp."
@@ -9,19 +9,19 @@ comment "asm-source.exp."
 comment	"This file is not linked with crt0."
 comment	"Provide very simplistic equivalent."
 	
-	.global _start
-_start:
-	gdbasm_startup
-	gdbasm_call main
-	gdbasm_exit0
-
-
+comment "	.global _start"
+comment "	function _start"
+comment "	gdbasm_startup"
+comment "	gdbasm_call main"
+comment "	gdbasm_exit0"
+
+
 comment "main routine for assembly source debugging test"
 comment "This particular testcase uses macros in <arch>.inc to achieve"
 comment "machine independence."
 
 	.global main
-main:
+	function main
 	gdbasm_enter
 
 comment "Call a macro that consists of several lines of assembler code."
@@ -39,25 +39,25 @@ comment "All done."
 comment "A routine for foo2 to call."
 
 	.global foo3
-foo3:
+	function foo3
 	gdbasm_enter
 	gdbasm_leave
 
 	.global exit
-exit:
+	function exit
 	gdbasm_exit0
 
 comment "A static function"
 
-foostatic:
+        function _foostatic
 	gdbasm_enter
 	gdbasm_leave
 
 comment "A global variable"
 
-	.global globalvar
-gdbasm_datavar	globalvar	11
-
+	.global _globalvar
+gdbasm_datavar	_globalvar	11
+
 comment "A static variable"
 
-gdbasm_datavar	staticvar	5
+gdbasm_datavar	_staticvar	5
diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/testsuite/gdb.asm/asmsrc2.s gdb/testsuite/gdb.asm/asmsrc2.s
--- gdb.before/testsuite/gdb.asm/asmsrc2.s	Thu Mar  7 09:18:57 2002
+++ gdb/testsuite/gdb.asm/asmsrc2.s	Sat Mar  9 20:36:26 2002
@@ -1,10 +1,10 @@
 	.include "common.inc"
 	.include "arch.inc"
-
+        .include "object.inc"
+ 
 comment "Second file in assembly source debugging testcase."
-
 	.global foo2
-foo2:
+	function foo2
 	gdbasm_enter
 
 comment "Call someplace else (several times)."
diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/testsuite/gdb.asm/asmsrc3.s gdb/testsuite/gdb.asm/asmsrc3.s
--- gdb.before/testsuite/gdb.asm/asmsrc3.s	Wed Dec 31 16:00:00 1969
+++ gdb/testsuite/gdb.asm/asmsrc3.s	Sun Mar 10 10:10:33 2002
@@ -0,0 +1,21 @@
+	.include "common.inc"
+	.include "arch.inc"
+	.include "object.inc"
+	comment "empty file, to provide EOF entry in stabs"
+
+comment	"This file is not linked with crt0."
+comment	"Provide very simplistic equivalent."
+	
+	.global _start
+	function _start
+	gdbasm_startup
+	gdbasm_call main
+	gdbasm_exit0
+
+comment "The next nonblank line must be 'offscreen' w.r.t. start"
+comment "and it must not be the last line in the file (I don't know why)"
+
+
+
+comment "Provide something to search for"
+
diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/testsuite/gdb.asm/asmsrc4.s gdb/testsuite/gdb.asm/asmsrc4.s
--- gdb.before/testsuite/gdb.asm/asmsrc4.s	Wed Dec 31 16:00:00 1969
+++ gdb/testsuite/gdb.asm/asmsrc4.s	Sun Mar 10 09:47:38 2002
@@ -0,0 +1,6 @@
+	.include "common.inc"
+	.include "arch.inc"
+	.include "object.inc"
+	comment "empty file, to provide EOF entry in stabs"
+
+	function ender
diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/testsuite/gdb.asm/coff.inc gdb/testsuite/gdb.asm/coff.inc
--- gdb.before/testsuite/gdb.asm/coff.inc	Wed Dec 31 16:00:00 1969
+++ gdb/testsuite/gdb.asm/coff.inc	Sat Mar  9 11:28:39 2002
@@ -0,0 +1,6 @@
+	comment "For COFF, functions should be flagged as such"
+
+	.macro function name
+	.def \name; .val \name; .scl 2; .type 040; .endef
+\name:
+	.endm
diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/testsuite/gdb.asm/common.inc gdb/testsuite/gdb.asm/common.inc
--- gdb.before/testsuite/gdb.asm/common.inc	Thu Mar  7 09:18:57 2002
+++ gdb/testsuite/gdb.asm/common.inc	Fri Mar  8 20:15:02 2002
@@ -26,3 +26,4 @@ comment "datavar - define a data variabl
 
 comment "macros to label a subroutine may also eventually be needed"
 comment "i.e. .global foo\nfoo:\n"
+
diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/testsuite/gdb.asm/configure.in gdb/testsuite/gdb.asm/configure.in
--- gdb.before/testsuite/gdb.asm/configure.in	Thu Mar  7 09:18:57 2002
+++ gdb/testsuite/gdb.asm/configure.in	Fri Mar  8 20:41:29 2002
@@ -14,16 +14,19 @@ AC_CANONICAL_SYSTEM
 
 dnl In default case we need to link with some file so use common.inc.
 archinc=common.inc
+objinc=otherobj.inc
 case ${target} in
 *arm-*-*) archinc=arm.inc ;;
 xscale-*-*) archinc=arm.inc ;;
 d10v-*-*) archinc=d10v.inc ;;
 s390-*-*) archinc=s390.inc ;;
+i[[3456]]86-*-interix*) archinc=i386.inc;objinc=coff.inc ;;
 i[[3456]]86*) archinc=i386.inc ;;
 m32r*-*) archinc=m32r.inc ;;
 sparc-*-*) archinc=sparc.inc ;;
 xstormy16-*-*) archinc=xstormy16.inc ;;
 esac
 AC_LINK_FILES($archinc,arch.inc)
+AC_LINK_FILES($objinc,object.inc)
 
 AC_OUTPUT(Makefile)
diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/testsuite/gdb.asm/otherobj.inc gdb/testsuite/gdb.asm/otherobj.inc
--- gdb.before/testsuite/gdb.asm/otherobj.inc	Wed Dec 31 16:00:00 1969
+++ gdb/testsuite/gdb.asm/otherobj.inc	Fri Mar  8 20:45:45 2002
@@ -0,0 +1,5 @@
+	comment "Just define the label"
+
+	.macro function name
+\name:
+	.endm
diff -drupP --exclude-from=/dev/fs/M/donn/diffs/exclude.files gdb.before/dbxread.c gdb/dbxread.c
--- gdb.before/dbxread.c	Thu Mar  7 09:18:59 2002
+++ gdb/dbxread.c	Sat Mar  9 20:13:58 2002
@@ -2568,8 +2568,9 @@ read_ofile_symtab (struct partial_symtab
   else
     {
       /* The N_SO starting this symtab is the first symbol, so we
-         better not check the symbol before it.  I'm not this can
-         happen, but it doesn't hurt to check for it.  */
+         better not check the symbol before it.  This is rare, but it
+	 does occur, particularly with debug symbols from the assembler,
+	 which would not (of course) have a compiler name entry. */
       bfd_seek (symfile_bfd, sym_offset, SEEK_CUR);
       processing_gcc_compilation = 0;
     }
