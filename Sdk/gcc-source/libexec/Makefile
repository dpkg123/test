# Generated automatically from Makefile.in by configure.
# Copyright (C) 1992 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.

# You should have received a copy of the GNU Library General Public
# License along with the GNU C Library; see the file COPYING.LIB.  If
# not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

#
# Makefile for libexec directory
#

# Directory containing source files.  Don't clean up the spacing,
# this exact string is matched for by the "configure" script.

VPATH = /dev/fs/D/gnu2.intel//libexec
srcdir = /dev/fs/D/gnu2.intel//libexec

prefix =	/opt/gcc.3.3
exec_prefix =	${prefix}

bindir =	${exec_prefix}/bin
libdir =	${exec_prefix}/lib

datadir =	${prefix}/share
mandir =	${prefix}/man
man1dir =	$(mandir)/man1
man2dir =	$(mandir)/man2
man3dir =	$(mandir)/man3
man4dir =	$(mandir)/man4
man5dir =	$(mandir)/man5
man6dir =	$(mandir)/man6
man7dir =	$(mandir)/man7
man8dir =	$(mandir)/man8
man9dir =	$(mandir)/man9
infodir =	${prefix}/info
includedir =	${prefix}/include

SHELL =		/bin/sh

INSTALL =	/bin/install -c
INSTALL_PROGRAM=${INSTALL}
INSTALL_DATA =	${INSTALL} -m 644

AR =		@AR@
AR_FLAGS =	qv
CFLAGS =	-g
RANLIB =	@RANLIB@
RM =		rm

CC = /dev/fs/D/gnu2.intel/local_bin/gcc -B/dev/fs/D/gnu2.intel/local_bin/ 
CFLAGS = -g

SUBDIRS = libdl

all:	force
	@$(MAKE) subdir_do DO=$@  $(FLAGS_TO_PASS)

mostlyclean clean:
	cd libdl; $(MAKE) $@
	rm -f *.a *.o core errs *~ \#* TAGS *.E a.out errors 

distclean:	clean 
	cd libdl; $(MAKE) $@
	rm -f config.cache config.log config.status
	rm -f Makefile depend

maintainer-clean realclean: distclean clean
	cd libdl; $(MAKE) $@
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."

subdir_do: force
	@for i in $(SUBDIRS); do \
	   echo "Making $(DO) in $${i}..." ; \
	   if [ -d ./$$i ] ; then \
	     if (rootme=`pwd`/ ; export rootme ; \
	       rootsrc=`cd $(srcdir); pwd`/ ; export rootsrc ; \
	       cd ./$$i; \
	       $(MAKE) $(FLAGS_TO_PASS) $(DO)) ; then true ; \
	     else exit 1 ; fi ; \
	   else true ; fi ; \
	done

force:

Makefile:	Makefile.in config.status
		$(SHELL) config.status

config.status:	configure
		$(SHELL) config.status --recheck
