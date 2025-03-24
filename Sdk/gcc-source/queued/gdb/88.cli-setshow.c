--- cli/cli-setshow.c.orig      Wed Sep 17 14:25:32 2003
+++ cli/cli-setshow.c   Wed Sep 17 14:45:23 2003
@@ -352,28 +352,31 @@ do_setshow_command (char *arg, int from_
 void
 cmd_show_list (struct cmd_list_element *list, int from_tty, char *prefix)
 {
-  ui_out_tuple_begin (uiout, "showlist");
+    struct cleanup *showlist_chain =
+       make_cleanup_ui_out_tuple_begin_end(uiout, "showlist");
   for (; list != NULL; list = list->next)
     {
       /* If we find a prefix, run its list, prefixing our output by its
          prefix (with "show " skipped).  */
       if (list->prefixlist && !list->abbrev_flag)
        {
-         ui_out_tuple_begin (uiout, "optionlist");
+         struct cleanup *optionlist_chain =
+               make_cleanup_ui_out_tuple_begin_end(uiout, "optionlist");
          ui_out_field_string (uiout, "prefix", list->prefixname + 5);
          cmd_show_list (*list->prefixlist, from_tty, list->prefixname + 5);
-         ui_out_tuple_end (uiout);
+         do_cleanups (optionlist_chain);
        }
       if (list->type == show_cmd)
        {
-         ui_out_tuple_begin (uiout, "option");
+         struct cleanup *option_chain =
+           make_cleanup_ui_out_tuple_begin_end(uiout, "option");
          ui_out_text (uiout, prefix);
          ui_out_field_string (uiout, "name", list->name);
          ui_out_text (uiout, ":  ");
          do_setshow_command ((char *) NULL, from_tty, list);
-         ui_out_tuple_end (uiout);
+         do_cleanups (option_chain);
        }
     }
-  ui_out_tuple_end (uiout);
+ do_cleanups (showlist_chain);
 }
