/* bf-ms-layout-span.c */
/* Adapted from Donn Terry <donnte@microsoft.com> testcase
   posted to GCC-patches
   http://gcc.gnu.org/ml/gcc-patches/2000-08/msg00577.html */ 

/* { dg-do run { target *-*-mingw* *-*-cygwin*  } } */
/* { dg-options "-mms-bitfields -D_TEST_MS_LAYOUT" } */

#include <stddef.h>
#include <string.h>

extern void abort(void);

struct eleven {
  short d;
  int a1:7;
  int a2:7;
  int a3:7;
  int a4:7;
  int a5:7;
  int a6:7;
  short c;
};

struct twelve {
  short d;
  char a1:7;
  char a2:7;
  char a3:7;
  char a4:7;
  char a5:7;
  char a6:7;
  short c;
};

#define val(s,f) (s.f)

#define check_struct(_X) \
{ \
  if (sizeof (struct _X) != exp_sizeof_##_X )	\
    abort();                                    \
  memcpy(&test_##_X, filler, sizeof(test_##_X));\
  if (val(test_##_X,c) != exp_##_X##_c) 	\
    fprintf(stderr, "content of " #_X " %d != %d\n", val(test_##_X,c), exp_##_X##_c); \
}

#define check_union(_X) \
{ \
  if (sizeof (union _X) != exp_sizeof_##_X )	\
    abort();                                    \
  memcpy(&test_##_X, filler, sizeof(test_##_X));\
  if (val(test_##_X,c) != exp_##_X##_c) 	\
    abort();                                    \
}

#define check_struct_size(_X) \
{ \
  if (sizeof (struct _X) != exp_sizeof_##_X )	\
    abort();                                    \
}

#define check_struct_off(_X) \
{ \
  memcpy(&test_##_X, filler, sizeof(test_##_X));\
  if (val(test_##_X,c) != exp_##_X##_c) 	\
    printf("---%d\n", val(test_##_X,c))/*,abort()*/;                                    \
}

#define check_union_size(_X) \
{ \
  if (sizeof (union _X) != exp_sizeof_##_X )	\
    abort();                                    \
}

#define check_union_off(_X) \
{ \
  memcpy(&test_##_X, filler, sizeof(test_##_X));\
  if (val(test_##_X,c) != exp_##_X##_c) 	\
    abort();                                    \
}

int main(){

  unsigned char filler[32];
  struct eleven test_eleven;
  struct twelve test_twelve;

#if defined (_TEST_MS_LAYOUT) || defined (_MSC_VER)

  size_t exp_sizeof_eleven = 16;
  size_t exp_sizeof_twelve = 10;
  short exp_eleven_c = 3340;
  short exp_twelve_c = 2312;
 
#else /* testing -mno-ms-bitfields */

  size_t exp_sizeof_eleven = 8;
  char exp_eleven_c = 1;

#endif

  unsigned char i; 
  for ( i = 0; i < 32; i++ )
    filler[i] = i;

  check_struct_off(eleven);

  if (test_eleven.a1 != 4)
     abort();
  if (test_eleven.a2 != 10)
     abort();
  if (test_eleven.a3 != 24)
     abort();
  if (test_eleven.a4 != 56)
     abort();
  if (test_eleven.a5 != 8)
     abort();
  if (test_eleven.a6 != 18)
     abort();

  check_struct_off(twelve);

  if (test_twelve.a1 != 2)
     abort();
  if (test_twelve.a2 != 3)
     abort();
  if (test_twelve.a3 != 4)
     abort();
  if (test_twelve.a4 != 5)
     abort();
  if (test_twelve.a5 != 6)
     abort();
  if (test_twelve.a6 != 7)
     abort();

  check_struct_size (eleven);
  check_struct_size (twelve);

  return 0;
};
