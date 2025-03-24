# Make RCS.
# __INTERIX note:
# This file has been hand modified to stop it from auto regenerating itself
# and clobbering "conf.h" -- both of which have been hand tweaked.

#	$Id: Makefile.in,v 5.26 1995/06/16 06:19:24 eggert Exp $

# Copyright 1982, 1988, 1989 Walter Tichy
# Copyright 1990, 1991, 1992, 1993, 1994, 1995 Paul Eggert
#   Distributed under license by the Free Software Foundation, Inc.
#
# This file is part of RCS.
#
# RCS is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# RCS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with RCS; see the file COPYING.
# If not, write to the Free Software Foundation,
# 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# Report problems and direct all questions to:
#
#    rcs-bugs@cs.purdue.edu


# default target
default :: all

# See the file INSTALL.RCS for more information on the configuration section.
# ----- start of configuration section -----

#(Unix

srcdir = .

prefix = /usr/local
exec_prefix = ${prefix}

CC = c89
#CPPFLAGS = -D_ALL_SOURCE -I/usr/local/include  -I/usr/local/ssl/include
CPPFLAGS = -D_ALL_SOURCE
CFLAGS = -O2 -D_ALL_SOURCE
DEFS = -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" 
DIFF = /bin/diff
DIFF3 = /bin/diff3
DIFF3_BIN = 1
DIFFFLAGS = -an
DIFF_L = 1
DIFF_FAILURE = 1
DIFF_SUCCESS = 0
DIFF_TROUBLE = 2
ED = /bin/ed
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL}
#LDFLAGS =
#LDFLAGS = -L/usr/local/lib -L/usr/local/ssl/lib
#LIBS = 
SENDMAIL = "/usr/sbin/sendmail"


# The following definitions can be tailored by hand;
# this shouldn't be needed for ordinary installations.

 bindir = $(exec_prefix)/bin

 ALL_CFLAGS = $(CPPFLAGS) -Dhas_conf_h $(DEFS) -I. -I$(srcdir) $(CFLAGS)

 COMPAT2 = 0
#COMPAT2 = 1

 LIBOBJS =

 LINK = $(CC) $(LDFLAGS) $(CFLAGS)

 LINT = lint -abchx# traditional and BSD lint
#LINT = lint# System V lint

#RCSPREFIX should end in `/' if it is not empty.
 RCSPREFIX = $(bindir)/

 REMOVE = rm -f

 o = .o
#o = .s# Minix/PC with ACK cc

 x = .exe

#)
# On non-Unix hosts you must manually create and edit conf.h from conf.heg.

# ----- end of configuration section -----
# You shouldn't have to change anything past this point.


# Avoid brain damage in some versions of 'make'.
SHELL = /bin/sh

PROGRAMS = ci$x co$x ident$x merge$x \
	rcs$x rcsclean$x rcsdiff$x rcsmerge$x rlog$x

all :: $(PROGRAMS)

.SUFFIXES :
.SUFFIXES : .c $o
.c$o :
	$(CC) -c $(ALL_CFLAGS) $<

installdirs :: ../mkinstalldirs
	$(srcdir)/../mkinstalldirs $(bindir)

install :: all installdirs
	for p in $(PROGRAMS); do \
	  $(INSTALL_PROGRAM) $$p $(DESTDIR_CONTRIB_BIN); \
	done
	for p in $(PROGRAMS); do \
	  $(INSTALL_PROGRAM) $$p.pdb $(DESTDIR_PDB); \
	done

build: $(PROGRAMS)

fullbuild: clean build install

uninstall ::
	for p in $(PROGRAMS); do \
	  $(REMOVE) $(bindir)/$$p; \
	done

# Install RCS and (if applicable) GNU diff before running these tests.
# To test RCS before installing it, see the file INSTALL.RCS.
RCSTEST = \
	ALL_CFLAGS='$(ALL_CFLAGS)' CC='$(CC)' DIFF='$(DIFF)' \
	LDFLAGS='$(LDFLAGS)' LIBS='$(LIBS)' \
	PATH=$(bindir):$$PATH \
	sh $(srcdir)/rcstest
installcheck ::
	$(RCSTEST)
installdebug ::
	$(RCSTEST) -v

clean ::
	$(REMOVE) a.* RCS/a.*
	$(REMOVE) *$o
	$(REMOVE) $(PROGRAMS)
	$(REMOVE) conf.err core core.* *.core

mostlyclean :: clean

distclean :: mostlyclean
	$(REMOVE) conf.h Makefile

maintainer-clean :: distclean
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."
	$(REMOVE) TAGS

#(Unix
#conf.h : conf.sh Makefile
#	$(REMOVE) a.*
#	ALL_CFLAGS='$(ALL_CFLAGS)' \
#	CC='$(CC)' \
#	COMPAT2='$(COMPAT2)' \
#	DIFF3='$(DIFF3)' DIFF3_BIN='$(DIFF3_BIN)' \
#	DIFF='$(DIFF)' DIFFFLAGS='$(DIFFFLAGS)' DIFF_L='$(DIFF_L)' \
#	DIFF_SUCCESS='$(DIFF_SUCCESS)' \
#	DIFF_FAILURE='$(DIFF_FAILURE)' \
#	DIFF_TROUBLE='$(DIFF_TROUBLE)' \
#	ED='$(ED)' \
#	LDFLAGS='$(LDFLAGS)' LIBS='$(LIBS)' \
#	RCSPREFIX='$(RCSPREFIX)' \
#	SENDMAIL='$(SENDMAIL)' \
#	$(SHELL) -x $(srcdir)/conf.sh 3>&1 >a.h 2>conf.err
#	mv a.h $@
#	$(REMOVE) a.* core core.* *.core
#)

ci = ci$o rcslex$o rcssyn$o rcsgen$o rcsedit$o rcskeys$o rcsmap$o \
	rcsrev$o rcsutil$o rcsfnms$o maketime$o partime$o rcstime$o rcskeep$o \
	rcsfcmp$o version$o $(LIBOBJS)
ci$x : $(ci)
	$(BUILD)

co = co$o rcslex$o rcssyn$o rcsgen$o rcsedit$o rcskeys$o rcsmap$o \
	rcsrev$o rcsutil$o rcsfnms$o maketime$o partime$o rcstime$o rcskeep$o \
	version$o $(LIBOBJS)
co$x : $(co)
	$(BUILD)

ident = ident$o rcsmap$o version$o $(LIBOBJS)
ident$x : $(ident)
	$(BUILD)

merge = merge$o merger$o rcsfnms$o rcslex$o \
	rcsmap$o rcsrev$o rcssyn$o rcsutil$o \
	rcskeep$o rcskeys$o maketime$o partime$o rcstime$o version$o \
	$(LIBOBJS)
merge$x : $(merge)
	$(BUILD)

rlog = rlog$o rcslex$o rcsmap$o rcssyn$o rcsrev$o rcsutil$o \
	maketime$o partime$o rcstime$o rcsfnms$o rcskeep$o rcskeys$o \
	version$o $(LIBOBJS)
rlog$x : $(rlog)
	$(BUILD)

rcs = rcs$o rcslex$o rcssyn$o rcsrev$o rcsutil$o rcsgen$o \
	rcsedit$o rcskeys$o rcsmap$o rcsfnms$o rcskeep$o \
	maketime$o partime$o rcstime$o version$o $(LIBOBJS)
rcs$x : $(rcs)
	$(BUILD)

rcsclean = rcsclean$o rcsedit$o rcsfcmp$o rcsfnms$o rcsgen$o rcskeys$o \
	rcslex$o rcsmap$o rcsrev$o rcssyn$o rcsutil$o rcskeep$o \
	maketime$o partime$o rcstime$o version$o $(LIBOBJS)
rcsclean$x : $(rcsclean)
	$(BUILD)

rcsdiff = rcsdiff$o rcsutil$o rcsfnms$o rcsmap$o rcsrev$o rcssyn$o \
	rcslex$o maketime$o partime$o rcstime$o rcskeep$o rcskeys$o \
	version$o $(LIBOBJS)
rcsdiff$x : $(rcsdiff)
	$(BUILD)

rcsmerge = rcsmerge$o merger$o rcsutil$o rcsfnms$o rcsmap$o rcsrev$o \
	rcssyn$o rcslex$o rcskeep$o rcskeys$o \
	maketime$o partime$o rcstime$o version$o $(LIBOBJS)
rcsmerge$x : $(rcsmerge)
	$(BUILD)

SOURCE=	ci.c co.c ident.c maketime.c merge.c merger.c partime.c rcs.c \
	rcsclean.c rcsdiff.c rcsedit.c rcsfcmp.c rcsfnms.c rcsgen.c \
	rcskeep.c rcskeys.c rcslex.c rcsmap.c rcsmerge.c rcsrev.c rcssyn.c \
	rcstime.c rcsutil.c rlog.c version.c
OBJECT=	ci$o co$o ident$o maketime$o merge$o merger$o partime$o rcs$o \
	rcsclean$o rcsdiff$o rcsedit$o rcsfcmp$o rcsfnms$o rcsgen$o \
	rcskeep$o rcskeys$o rcslex$o rcsmap$o rcsmerge$o rcsrev$o rcssyn$o \
	rcstime$o rcsutil$o rlog$o version$o

lint :: conf.h
	$(LINT) -DRCS_lint=1 $(ALL_CFLAGS) $(SOURCE)

TAGS : $(SOURCE)
	etags $(SOURCE)

dvi info ::

conf_h = conf.h
$(OBJECT) : $(conf_h) rcsbase.h
maketime$o partime$o rcstime$o : partime.h
maketime$o rcstime$o : maketime.h
