/* bf-ms-attrib.c */
/* Adapted from Donn Terry <donnte@microsoft.com> testcase
   posted to GCC-patches
   http://gcc.gnu.org/ml/gcc-patches/2000-08/msg00577.html */ 

/* { dg-do run { target *-*-mingw* *-*-cygwin* *-*-interix* } } */

#include <stdio.h>

struct one_gcc {
  int d;
  unsigned char a;
  unsigned short b:7;
  char c;	
} __attribute__((__gcc_struct__)) ;


struct one_nat {
  int d;
  unsigned char a;
  unsigned short b:7;
  char c;	
} __attribute__((__native_struct__));


main() 
  {
    /* As long as the sizes are as expected, we know attributes are working.
       bf-ms-layout.c makes sure the right thing happens when the attribute
       is on. */
    if (sizeof(struct one_nat) != 12)
	abort();
    if (sizeof(struct one_gcc) != 8)
	abort();
  }
