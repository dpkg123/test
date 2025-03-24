dnl GAS_CHECK_DECL_NEEDED(name, typedefname, typedef, headers)
AC_DEFUN(GAS_CHECK_DECL_NEEDED,[
AC_MSG_CHECKING(whether declaration is required for $1)
AC_CACHE_VAL(gas_cv_decl_needed_$1,
AC_TRY_LINK([$4],
[
typedef $3;
$2 x;
x = ($2) $1;
], gas_cv_decl_needed_$1=no, gas_cv_decl_needed_$1=yes))dnl
AC_MSG_RESULT($gas_cv_decl_needed_$1)
if test $gas_cv_decl_needed_$1 = yes; then
 AC_DEFINE([NEED_DECLARATION_]translit($1, [a-z], [A-Z]), 1,
	   [Define if $1 is not declared in system header files.])
fi
])dnl
dnl
dnl Some non-ANSI preprocessors botch requoting inside strings.  That's bad
dnl enough, but on some of those systems, the assert macro relies on requoting
dnl working properly!
dnl GAS_WORKING_ASSERT
AC_DEFUN(GAS_WORKING_ASSERT,
[AC_MSG_CHECKING([for working assert macro])
AC_CACHE_VAL(gas_cv_assert_ok,
AC_TRY_LINK([#include <assert.h>
#include <stdio.h>], [
/* check for requoting problems */
static int a, b, c, d;
static char *s;
assert (!strcmp(s, "foo bar baz quux"));
/* check for newline handling */
assert (a == b
        || c == d);
], gas_cv_assert_ok=yes, gas_cv_assert_ok=no))dnl
AC_MSG_RESULT($gas_cv_assert_ok)
test $gas_cv_assert_ok = yes || AC_DEFINE(BROKEN_ASSERT, 1, [assert broken?])
])dnl
dnl
dnl Since many Bourne shell implementations lack subroutines, use this
dnl hack to simplify the code in configure.in.
dnl GAS_UNIQ(listvar)
AC_DEFUN(GAS_UNIQ,
[_gas_uniq_list="[$]$1"
_gas_uniq_newlist=""
dnl Protect against empty input list.
for _gas_uniq_i in _gas_uniq_dummy [$]_gas_uniq_list ; do
  case [$]_gas_uniq_i in
  _gas_uniq_dummy) ;;
  *) case " [$]_gas_uniq_newlist " in
       *" [$]_gas_uniq_i "*) ;;
       *) _gas_uniq_newlist="[$]_gas_uniq_newlist [$]_gas_uniq_i" ;;
     esac ;;
  esac
done
$1=[$]_gas_uniq_newlist
])dnl

sinclude(../libtool.m4)
dnl The lines below arrange for aclocal not to bring libtool.m4
dnl AM_PROG_LIBTOOL into aclocal.m4, while still arranging for automake
dnl to add a definition of LIBTOOL to Makefile.in.
ifelse(yes,no,[
AC_DEFUN([AM_PROG_LIBTOOL],)
AC_DEFUN([AC_CHECK_LIBM],)
AC_SUBST(LIBTOOL)
])

sinclude(../gettext.m4)
ifelse(yes,no,[
AC_DEFUN([CY_WITH_NLS],)
AC_SUBST(INTLLIBS)
])

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
