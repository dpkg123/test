#define NATIVE 
//#define NATIVE __attribute__((__native_struct__))
#ifdef PACKED
#pragma pack(1)
#endif
#include "bittest.h"
#include <stddef.h>

#define check(s,f,v) if (s.f != v) { \
        printf(#s"."#f " was %d not " #v "\n", s.f); exit_code = 1;}

#define ckoff(a) for (ii=0; ii<3; ii++) { \
		if (a##_offsets[ii] != my_##a##_offsets[ii]) {\
		    printf(#a" offset field %d did not match: nat: %d v gcc: %d\n", \
			ii, a##_offsets[ii], my_##a##_offsets[ii]); \
			exit_code = 1; } }

#define cksize(a) if (a##_size != my_##a##_size) { \
		    printf(#a" size did not match: nat %d v gcc %d\n", \
		       a##_size, my_##a##_size); \
		       exit_code = 1; } 

extern int a_offsets[];
extern int b_offsets[];
extern int c_offsets[];

int my_a_offsets[]={offsetof(struct a_type,b),
             offsetof(struct a_type,d),
             offsetof(struct a_type,h),
	     sizeof(struct a_type)};

int my_b_offsets[]={offsetof(struct b_type,b),
             offsetof(struct b_type,d),
             offsetof(struct b_type,h),
	     sizeof(struct b_type)};

int my_c_offsets[]={offsetof(struct c_type,b),
             offsetof(struct c_type,d),
             offsetof(struct c_type,h),
	     sizeof(struct c_type)};

int my_d_size = sizeof(struct d_type);
int my_e_size = sizeof(struct e_type);
int my_f_size = sizeof(struct f_type);
int my_g_size = sizeof(struct g_type);
int my_h_size = sizeof(struct h_type);
int my_i_size = sizeof(union i_type);

extern int d_size;
extern int e_size;
extern int f_size;
extern int g_size;
extern int h_size;
extern int i_size;

extern struct a_type a;
extern struct b_type b;
extern struct c_type c;
extern struct d_type d;
extern struct e_type e;
extern struct f_type f;
extern struct g_type g;
extern struct h_type h;
extern union i_type i;

check_results()
{
    int ii;
    int exit_code = 0;

    ckoff(a);
    ckoff(b);
    ckoff(c);

    cksize(d);
    cksize(e);
    cksize(f);
    cksize(g);
    cksize(h);
    cksize(i);

    check(a,a,1);
    check(a,b,2);
    check(a,c,3);
    check(a,d,4);
    check(a,e,5);
    check(a,f,6);
    check(a,g,7);
    check(a,h,8);
    check(a,i,9);
    check(a,j,10);

    check(b,a,1);
    check(b,b,2);
    check(b,c,3);
    check(b,d,4);
    check(b,e,5);
    check(b,f,6);
    check(b,g,7);
    check(b,h,8);
    check(b,i,9);
    check(b,j,10);

    check(c,a,1);
    check(c,b,2);
    check(c,c,3);
    check(c,d,4);
    check(c,e,5);
    check(c,f,6);
    check(c,g,7);
    check(c,h,8);
    check(c,i,9);
    check(c,j,10);

    check(d,a,1);
    check(d,b,2);
    check(d,c,3);
    check(d,d,4);
    check(d,e,5);
    check(d,f,6);
    check(d,g,7);

    check(e,a,1);
    check(e,b,2);

    check(f,a,1);
    check(f,b,2);

    check(g,a,1);
    check(g,b,2);

    check(h,a,1);
    check(h,b,2);

    check(i,b,2);

    exit(exit_code);
}
