// 2002-01-23  Loren J. Rittle <rittle@labs.mot.com> <ljrittle@acm.org>
// Adapted from http://gcc.gnu.org/ml/gcc-bugs/2002-01/msg00679.html
// which was adapted from pthread1.cc by Mike Lu <MLu@dynamicsoft.com>
//
// Copyright (C) 2002, 2003 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// { dg-do run { target *-*-freebsd* *-*-netbsd* *-*-linux* *-*-solaris* *-*-cygwin *-*-darwin* } }
// { dg-options "-pthread" { target *-*-freebsd* *-*-netbsd* *-*-linux* } }
// { dg-options "-pthreads" { target *-*-solaris* } }

#include <string>
#include <list>

// Do not include <pthread.h> explicitly; if threads are properly
// configured for the port, then it is picked up free from STL headers.

#if __GTHREADS
using namespace std;

static list<string> foo;
static pthread_mutex_t fooLock = PTHREAD_MUTEX_INITIALIZER;
static unsigned max_size = 10;
#if defined(__CYGWIN__)
static int iters = 10000;
#else
static int iters = 300000;
#endif

void*
produce (void*)
{
  for (int num = 0; num < iters; )
    {
      string str ("test string");

      pthread_mutex_lock (&fooLock);
      if (foo.size () < max_size)
	{
	  foo.push_back (str);
	  num++;
	}
      pthread_mutex_unlock (&fooLock);
    }

  return 0;
}

void*
consume (void*)
{
  for (int num = 0; num < iters; )
    {
      pthread_mutex_lock (&fooLock);
      while (foo.size () > 0)
	{
	  string str = foo.back ();
	  foo.pop_back ();
	  num++;
	}
      pthread_mutex_unlock (&fooLock);
    }

  return 0;
}

int
main (void)
{
#if defined(__sun) && defined(__svr4__)
  pthread_setconcurrency (2);
#endif

  pthread_t prod;
  pthread_create (&prod, NULL, produce, NULL);
  pthread_t cons;
  pthread_create (&cons, NULL, consume, NULL);

  pthread_join (prod, NULL);
  pthread_join (cons, NULL);

  return 0;
}
#else
int main (void) {}
#endif
