#include <stdio.h>
#include <string.h>
/* A simple glue file for embedded targets so we can get the real exit
   status from the program. This assumes we're using GNU ld and can use
   the -wrap option, and that write(1, ...) does something useful. */

/* There is a bunch of weird cruft with #ifdef UNDERSCORES. This is needed
   because currently GNU ld doesn't deal well with a.out targets and
   the -wrap option. When GNU ld is fixed, this should definitely be
   removed. Note that we actually wrap __exit, not _exit on a target
   that has UNDERSCORES defined. */

#ifdef UNDERSCORES
#define REAL_EXIT _real___exit
#define REAL_MAIN _real__main
#define REAL_ABORT _real__abort
#define ORIG_EXIT _wrap___exit
#define ORIG_ABORT _wrap__abort
#define ORIG_MAIN _wrap__main

#else
#define REAL_EXIT __real_exit
#define REAL_MAIN __real_main
#define REAL_ABORT __real_abort
#define ORIG_EXIT __wrap_exit
#define ORIG_ABORT __wrap_abort
#define ORIG_MAIN __wrap_main
#endif

extern void REAL_EXIT ();
extern void REAL_ABORT ();
extern int REAL_MAIN (int argc, char **argv, char **envp);
int ___constval = 1;

static char *
write_int(val, ptr)
     int val;
     char *ptr;
{
  char c;
  if (val<0) {
    *(ptr++) = '-';
    val = -val;
  }
  if (val>9) {
    ptr = write_int (val/10);
  }
  c = (val%10)+'0';
  *(ptr++) = c;
  return ptr;
}

void
ORIG_EXIT (code)
     int code;
{
  char buf[30];
  char *ptr;

  strcpy (buf, "\n*** EXIT code ");
  ptr = write_int (code, buf + strlen(buf));
  *(ptr++) = '\n';
  write (1, buf, ptr-buf);
  REAL_EXIT (code);
  while (___constval);
}

void
ORIG_ABORT ()
{
  write (1, "\n*** EXIT code 4242\n", 20);
  REAL_ABORT ();
  while (___constval);
  abort ();
}

int
ORIG_MAIN (argc, argv, envp)
     int argc;
     char **argv;
     char **envp;
{
#ifdef WRAP_FILE_ARGS
  extern int __argc;
  extern char *__args[];

  exit (REAL_MAIN (__argc,__args,envp));
#else
  exit (REAL_MAIN (argc, argv, envp));
#endif
  while (___constval);
}
