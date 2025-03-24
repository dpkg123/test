struct a_type {
   int a;
   unsigned char b;
   unsigned c:7;
   int d;
   unsigned e:3;
   unsigned f:9;
   unsigned char g:7;
   int h;
   unsigned int i:6;
   unsigned int :0;
   unsigned int j:6;
} NATIVE ;

struct b_type {
   int a;
   unsigned char b;
   unsigned int c:7;
   int d;
   unsigned int e:3;
   unsigned int f:9;
   unsigned char g:7;
   int h;
   unsigned char i:6;
   unsigned char :0;
   unsigned char j:6;

} NATIVE ;

struct c_type {
   int a;
   unsigned char b;
   unsigned short c:7;
   int d;
   unsigned short e:3;
   unsigned short f:9;
   unsigned char g:7;
   short h;
   unsigned short i:6;
   unsigned short :0;
   unsigned short j:6;
} NATIVE ;

struct d_type {
   int a:3;
   int b:4;
   int c:4;
   int d:6;
   int e:5;
   int f:5;
   int g:5;
} NATIVE ;

/* Bitfields of size 0 have some truly odd behaviors. */

struct e_type {   /* should be size 2! */
   char a;
   int :0;        /* ignored; prior field is not a bitfield. */
   char b;
};

struct f_type {   /* should be size 8! */
   char a:8;
   int :0;	  /* not ignored; prior field IS a bitfield, causes struct
		     alignment as well. */
   char b;
};

struct g_type {   /* should be size 2! */
   char a:8;
   char :0;
   int  :0;	  /* Ignored; prior field is zero size bitfield. */
   char b;
};

struct h_type {   /* should be size 3! */
   short a:3;
   char  b;
};

#ifdef _MSC_VER
#define LONGLONG __int64
#else
#define LONGLONG long long
#endif

union i_type {   /* should be size 8! */
   LONGLONG a:3;
   char  b;
};
