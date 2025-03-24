This is a hack: make check is broken in a lot of ways, so don't use
it until it gets fixed (by someone who knows the stuff better than I).
Use make test, instead.

--- /C/CVS/binutils.baseline/src/tcl/Makefile.in	Wed Sep 12 17:38:49 2001
+++ Makefile.in	Sun Feb 10 08:40:10 2002
@@ -54,14 +54,7 @@ RUNTEST = ` \
   else echo runtest ;  fi`
 
 check:
-	cd $(CONFIGDIR) && $(MAKE) tcltest
-	rootme=`pwd`; export rootme; \
-	srcdir=${SRC_DIR}; export srcdir ; \
-	EXPECT=${EXPECT} ; export EXPECT ; \
-	if [ -f $${rootme}/../expect/expect ] ; then  \
-	   TCL_LIBRARY=`cd $${srcdir}/library && pwd` ; \
-	   export TCL_LIBRARY ; fi ; \
-	$(RUNTEST) $(RUNTESTFLAGS) --tool tcl --srcdir $(SRC_DIR)/testsuite
+	cd $(CONFIGDIR) && $(MAKE) test
 
 install-info info installcheck:
 
