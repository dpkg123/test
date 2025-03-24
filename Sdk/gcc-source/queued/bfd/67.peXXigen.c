Vibhas	8/9/2005
=====================================================
Here we find the start address of import table which is supposed to
be address of idata$2 and which has to be first here. And by adding offset 
we miss some DLL entries. So not doing that.
===============================================================================
--- peXXigen.c.orig     Tue Aug  9 17:37:28 2005
+++ peXXigen.c  Tue Aug  9 17:37:43 2005
@@ -1970,8 +1970,12 @@
     {
       pe_data (abfd)->pe_opthdr.DataDirectory[1].VirtualAddress =
        (h1->root.u.def.value
-        + h1->root.u.def.section->output_section->vma
-        + h1->root.u.def.section->output_offset);
+        + h1->root.u.def.section->output_section->vma);
+/*     #25549 idata$2 or import table is first  in this section and
+ *     we need not to add the offset at all here.
+ *     + h1->root.u.def.section->output_offset);
+ */
+
       h1 = coff_link_hash_lookup (coff_hash_table (info),
                                  ".idata$4", false, false, true);
       pe_data (abfd)->pe_opthdr.DataDirectory[1].Size =
