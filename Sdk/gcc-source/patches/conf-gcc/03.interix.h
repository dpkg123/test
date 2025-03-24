--- gcc/config/interix.h   Mon Mar 14 18:30:32 2005
+++ gcc/config/interix.h.new       Fri Apr  1 20:48:15 2005
@@ -47,7 +47,8 @@

 #undef LINK_SPEC
 #define LINK_SPEC "%{!shared:-stack 0x400000,0x10000} \
-                  -subsystem posix \
+                  %{R:-subsystem console -e ___MixedModeProcessStartup} \
+                  %{!R:-subsystem posix} \
                   %{g} \
                   %{dynamic:-Bdynamic} \
                   %{static:-Bstatic} \
@@ -83,8 +83,6 @@ for windows/multi thread */
 
 #endif /* 0 */
 
-#define STDC_0_IN_SYSTEM_HEADERS 1
-
 #define HANDLE_SYSV_PRAGMA 1
 #undef HANDLE_PRAGMA_WEAK  /* until the link format can handle it */
 
