Add rules so that runtest and runtest.exp get copied if the build
is occurring in a "split" environment.  (One where the source is
separated from the object, a-la gcc.)  Otherwise dejagnu's own
regression can't find the tool UNDER TEST (runtest) even though
it can find it as the tool DOING the test.


diff -drup dejagnu-19981026/dejagnu/Makefile.am dejagnu/dejagnu/unix.exp
--- dejagnu-19981026/dejagnu/Makefile.am	Fri Oct  2 00:17:30 1998
+++ dejagnu/dejagnu/Makefile.am	Sun May 23 15:39:44 1999
@@ -5,7 +5,7 @@ AUTOMAKE_OPTIONS = cygnus
 SUBDIRS = doc testsuite
 
 # driver script goes in /usr/local/bin
-bin_SCRIPTS = runtest
+bin_SCRIPTS = runtest runtest.exp
 
 # auxiliary scripts go in /usr/local/share/dejagnu
 pkgdata_SCRIPTS = config.guess runtest.exp
@@ -21,6 +21,12 @@ baseboards_files = $(srcdir)/baseboards/
 
 config_dest = $(DESTDIR)$(pkgdatadir)/config
 config_files = $(srcdir)/config/README $(srcdir)/config/*.exp
+
+runtest:
+	cp $(srcdir)/runtest .
+
+runtest.exp:
+	cp $(srcdir)/runtest.exp .
 
 install-data-local:
 	$(mkinstalldirs) $(lib_dest)
