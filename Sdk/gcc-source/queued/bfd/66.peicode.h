Amit Kumar Ghosh (amitgho@microsoft.com) 09-July-2004 
=====================================================

It seems that there is some coding problem in the function pe_ILF_build_a_bfd()
in bfd\peicode.h. In certain condition we try to access id6 which remains NULL.
Hence causing access violation. The condition being 

      (import_name_type == IMPORT_ORDINAL)
  &&  ( (import_type == IMPORT_CODE) || (import_type ==IMPORT_CONST) )

Solution : id6 is allocated when (import_name_type != IMPORT_ORDINAL). Hence  
subsequently if we could ensure that we will access id6 only if the said 
condition is true then we are done. Thats precisely what the patch is doing.

===============================================================================

--- peicode.orig.h	Thu Jul  8 14:04:47 2004
+++ src/bfd/peicode.h	Wed Jul  7 20:14:29 2004
@@ -1136,11 +1136,13 @@
       id4->comdat->symbol = imp_index;
       id4->comdat->name = (*imp_sym)->symbol.name;
 
-      id6->comdat = bfd_zalloc(abfd, sizeof(struct bfd_comdat_info));
-      if (id6->comdat == NULL)
-	return false;
-      id6->comdat->symbol = imp_index;
-      id6->comdat->name = (*imp_sym)->symbol.name;
+      if (import_name_type != IMPORT_ORDINAL) { 
+          id6->comdat = bfd_zalloc(abfd, sizeof(struct bfd_comdat_info));
+          if (id6->comdat == NULL)
+	    return false;
+          id6->comdat->symbol = imp_index;
+          id6->comdat->name = (*imp_sym)->symbol.name;
+      }
 
       pe_ILF_save_relocs (& vars, text);
       break;
@@ -1170,9 +1172,11 @@
       id4->comdat->name = (char *)(*imp_sym)->symbol.name;
 
       /* Fill in the comdat info. */
-      id6->comdat = bfd_zalloc(abfd, sizeof(struct bfd_comdat_info));
-      id6->comdat->symbol = imp_index;
-      id6->comdat->name = (char *)(*imp_sym)->symbol.name;
+      if (import_name_type != IMPORT_ORDINAL) { 
+          id6->comdat = bfd_zalloc(abfd, sizeof(struct bfd_comdat_info));
+          id6->comdat->symbol = imp_index;
+          id6->comdat->name = (char *)(*imp_sym)->symbol.name;
+      }
       break;
 
     case IMPORT_DATA:

