Turns off some of DJs stuff for .idata that works wrong for Interix
(and LIB.EXE generated DLL libs) but right for me.  See other .idata
patches in bfd for more.

Currently being applied.

diff -drup ldlang.c.orig ldlang.c
--- ldlang.c.orig	Mon Jun 14 20:38:22 1999
+++ ldlang.c	Mon Jun 14 20:35:50 1999
@@ -991,6 +991,7 @@ wild_sort (wild, file, section)
   if (! wild->filenames_sorted && ! wild->sections_sorted)
     return NULL;
 
+return NULL;  // 
   section_name = bfd_get_section_name (file->the_bfd, section);
   for (l = wild->children.head; l != NULL; l = l->next)
     {
