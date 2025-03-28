dnl See whether we need to use fopen-bin.h rather than fopen-same.h.
AC_DEFUN(BFD_BINARY_FOPEN,
[AC_REQUIRE([AC_CANONICAL_SYSTEM])
case "${host}" in
changequote(,)dnl
*-*-msdos* | *-*-go32* | *-*-mingw32* | *-*-cygwin* | *-*-windows*)
changequote([,])dnl
  AC_DEFINE(USE_BINARY_FOPEN, 1, [Use b modifier when opening binary files?]) ;;
esac])dnl

dnl Get a default for CC_FOR_BUILD to put into Makefile.
AC_DEFUN(BFD_CC_FOR_BUILD,
[# Put a plausible default for CC_FOR_BUILD in Makefile.
if test -z "$CC_FOR_BUILD"; then
  if test "x$cross_compiling" = "xno"; then
    CC_FOR_BUILD='$(CC)'
  else
    CC_FOR_BUILD=gcc
  fi
fi
AC_SUBST(CC_FOR_BUILD)
# Also set EXEEXT_FOR_BUILD.
if test "x$cross_compiling" = "xno"; then
  EXEEXT_FOR_BUILD='$(EXEEXT)'
else
  AC_CACHE_CHECK([for build system executable suffix], bfd_cv_build_exeext,
    [rm -f conftest*
     echo 'int main () { return 0; }' > conftest.c
     bfd_cv_build_exeext=
     ${CC_FOR_BUILD} -o conftest conftest.c 1>&5 2>&5
     for file in conftest.*; do
       case $file in
       *.c | *.o | *.obj | *.ilk | *.pdb) ;;
       *) bfd_cv_build_exeext=`echo $file | sed -e s/conftest//` ;;
       esac
     done
     rm -f conftest*
     test x"${bfd_cv_build_exeext}" = x && bfd_cv_build_exeext=no])
  EXEEXT_FOR_BUILD=""
  test x"${bfd_cv_build_exeext}" != xno && EXEEXT_FOR_BUILD=${bfd_cv_build_exeext}
fi
AC_SUBST(EXEEXT_FOR_BUILD)])dnl

dnl See whether we need a declaration for a function.
AC_DEFUN(BFD_NEED_DECLARATION,
[AC_MSG_CHECKING([whether $1 must be declared])
AC_CACHE_VAL(bfd_cv_decl_needed_$1,
[AC_TRY_COMPILE([
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif],
[char *(*pfn) = (char *(*)) $1],
bfd_cv_decl_needed_$1=no, bfd_cv_decl_needed_$1=yes)])
AC_MSG_RESULT($bfd_cv_decl_needed_$1)
if test $bfd_cv_decl_needed_$1 = yes; then
  AC_DEFINE([NEED_DECLARATION_]translit($1, [a-z], [A-Z]), 1,
	    [Define if $1 is not declared in system header files.])
fi
])dnl

dnl Check for existence of a type $1 in sys/procfs.h

AC_DEFUN(BFD_HAVE_SYS_PROCFS_TYPE,
[AC_MSG_CHECKING([for $1 in sys/procfs.h])
 AC_CACHE_VAL(bfd_cv_have_sys_procfs_type_$1,
   [AC_TRY_COMPILE([
#define _SYSCALL32
#include <sys/procfs.h>],
      [$1 avar],
      bfd_cv_have_sys_procfs_type_$1=yes,
      bfd_cv_have_sys_procfs_type_$1=no
   )])
 if test $bfd_cv_have_sys_procfs_type_$1 = yes; then
   AC_DEFINE([HAVE_]translit($1, [a-z], [A-Z]), 1,
	     [Define if <sys/procfs.h> has $1.])
 fi
 AC_MSG_RESULT($bfd_cv_have_sys_procfs_type_$1)
])


dnl Check for existence of member $2 in type $1 in sys/procfs.h

AC_DEFUN(BFD_HAVE_SYS_PROCFS_TYPE_MEMBER,
[AC_MSG_CHECKING([for $1.$2 in sys/procfs.h])
 AC_CACHE_VAL(bfd_cv_have_sys_procfs_type_member_$1_$2,
   [AC_TRY_COMPILE([
#define _SYSCALL32
#include <sys/procfs.h>],
      [$1 avar; void* aref = (void*) &avar.$2],
      bfd_cv_have_sys_procfs_type_member_$1_$2=yes,
      bfd_cv_have_sys_procfs_type_member_$1_$2=no
   )])
 if test $bfd_cv_have_sys_procfs_type_member_$1_$2 = yes; then
   AC_DEFINE([HAVE_]translit($1, [a-z], [A-Z])[_]translit($2, [a-z], [A-Z]), 1,
	     [Define if <sys/procfs.h> has $1.$2.])
 fi
 AC_MSG_RESULT($bfd_cv_have_sys_procfs_type_member_$1_$2)
])

sinclude(../libtool.m4)
dnl The lines below arrange for aclocal not to bring libtool.m4
dnl AM_PROG_LIBTOOL into aclocal.m4, while still arranging for automake
dnl to add a definition of LIBTOOL to Makefile.in.
ifelse(yes,no,[
AC_DEFUN([AM_PROG_LIBTOOL],)
AC_DEFUN([AM_DISABLE_SHARED],)
AC_SUBST(LIBTOOL)
])

sinclude(../gettext.m4)
ifelse(yes,no,[
AC_DEFUN([CY_WITH_NLS],)
AC_SUBST(INTLLIBS)
])

AC_DEFUN([AM_INSTALL_LIBBFD],
[AC_MSG_CHECKING([whether to install libbfd])
  AC_ARG_ENABLE(install-libbfd,
[  --install-libbfd controls installation of libbfd and related headers],
      install_libbfd_p=$enableval,
      if test "${host}" = "${target}" -o "$enable_shared" = "yes"; then
        install_libbfd_p=yes
      else
        install_libbfd_p=no
      fi)
  AC_MSG_RESULT($install_libbfd_p)
  AM_CONDITIONAL(INSTALL_LIBBFD, test $install_libbfd_p = yes)
  # libbfd.a is a host library containing target dependent code
  bfdlibdir='$(libdir)'
  bfdincludedir='$(includedir)'
  if test "${host}" != "${target}"; then
    bfdlibdir='$(exec_prefix)/$(host_alias)/$(target_alias)/lib'
    bfdincludedir='$(exec_prefix)/$(host_alias)/$(target_alias)/include'
  fi
  AC_SUBST(bfdlibdir)
  AC_SUBST(bfdincludedir)
]
)

dnl The following are taken from gcc (thus their name) so that when
dnl compiling libiberty.h and libiberty/getopt.h the right things happen.

dnl See whether we need a declaration for a function.
dnl The result is highly dependent on the INCLUDES passed in, so make sure
dnl to use a different cache variable name in this macro if it is invoked
dnl in a different context somewhere else.
dnl gcc_AC_CHECK_DECL(SYMBOL,
dnl 	[ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, INCLUDES]]])
AC_DEFUN(gcc_AC_CHECK_DECL,
[AC_MSG_CHECKING([whether $1 is declared])
AC_CACHE_VAL(gcc_cv_have_decl_$1,
[AC_TRY_COMPILE([$4],
[#ifndef $1
char *(*pfn) = (char *(*)) $1 ;
#endif], eval "gcc_cv_have_decl_$1=yes", eval "gcc_cv_have_decl_$1=no")])
if eval "test \"`echo '$gcc_cv_have_decl_'$1`\" = yes"; then
  AC_MSG_RESULT(yes) ; ifelse([$2], , :, [$2])
else
  AC_MSG_RESULT(no) ; ifelse([$3], , :, [$3])
fi
])

dnl
dnl Check multiple functions to see whether each needs a declaration.
dnl Arrange to define HAVE_DECL_<FUNCTION> to 0 or 1 as appropriate.
dnl gcc_AC_CHECK_DECLS(SYMBOLS,
dnl 	[ACTION-IF-NEEDED [, ACTION-IF-NOT-NEEDED [, INCLUDES]]])
AC_DEFUN(gcc_AC_CHECK_DECLS,
[for ac_func in $1
do
changequote(, )dnl
  ac_tr_decl=HAVE_DECL_`echo $ac_func | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
changequote([, ])dnl
gcc_AC_CHECK_DECL($ac_func,
  [AC_DEFINE_UNQUOTED($ac_tr_decl, 1) $2],
  [AC_DEFINE_UNQUOTED($ac_tr_decl, 0) $3],
dnl It is possible that the include files passed in here are local headers
dnl which supply a backup declaration for the relevant prototype based on
dnl the definition of (or lack of) the HAVE_DECL_ macro.  If so, this test
dnl will always return success.  E.g. see libiberty.h's handling of
dnl `basename'.  To avoid this, we define the relevant HAVE_DECL_ macro to
dnl 1 so that any local headers used do not provide their own prototype
dnl during this test.
#undef $ac_tr_decl
#define $ac_tr_decl 1
  $4
)
done
dnl Automatically generate config.h entries via autoheader.
if test x = y ; then
  patsubst(translit([$1], [a-z], [A-Z]), [\w+],
    [AC_DEFINE([HAVE_DECL_\&], 1,
      [Define to 1 if we found this declaration otherwise define to 0.])])dnl
fi
])

