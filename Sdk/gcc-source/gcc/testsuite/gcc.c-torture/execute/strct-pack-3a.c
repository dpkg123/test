/* This is the same test as strct-pack-3.c, but with a different alignment
   of the stack frame; there was (and at this writing, is) a bug that will
   show up under one, but NOT both, of strct-pack-3*, depending on exactly
   what's in crt0.o */
typedef struct
{
  short i __attribute__ ((aligned (2),packed));
  int f[2] __attribute__ ((aligned (2),packed));
} A;

f (ap)
  A *ap;
{
  short i, j = 1;

  i = ap->f[1];
  i += ap->f[j];
  for (j = 0; j < 2; j++)
    i += ap->f[j];

  return i;
}
main() {
   int foo;
   xmain();
}
xmain ()
{
  A a;
  a.f[0] = 100;
  a.f[1] = 13;
  if (f (&a) != 139)
    abort ();
  exit (0);
}
