# Makefile for GNU DIFF
# Copyright (C) 1988,1989,1991,1992,1993,1994 Free Software Foundation, Inc.
#
# This file is part of GNU DIFF.
#
# GNU DIFF is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# GNU DIFF is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU DIFF; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

#### Start of system configuration section. ####


CFLAGS+= -DNULL_DEVICE=\"$(NULL_DEVICE)\" -DDEFAULT_EDITOR_PROGRAM=\"$(DEFAULT_EDITOR_PROGRAM)\"  -DHAVE_STRERROR
PRE_INCLDIRS=-I.


# Some System V machines do not come with libPW.
# If this is true for you, use the GNU alloca.o here.
ALLOCA = alloca.o

DEFAULT_EDITOR_PROGRAM = ed
NULL_DEVICE = /dev/null

#### End of system configuration section. ####

CFLAGS+= -X/Wall
# The source files for all of the programs.
srcs=diff.c analyze.c cmpbuf.c cmpbuf.h io.c context.c ed.c normal.c ifdef.c \
	util.c dir.c memchr.c waitpid.c \
	version.c diff.h alloca.c \
	side.c system.h \
	diff3.c sdiff.c cmp.c error.c xmalloc.c getopt.c getopt1.c getopt.h \
	fnmatch.c fnmatch.h regex.c regex.h 

PROGRAMS = cmp diff diff3 sdiff dircmp 

all: $(PROGRAMS)

diff_o = analyze.o cmpbuf.o dir.o io.o util.o \
	context.o ed.o ifdef.o normal.o side.o \
	getopt.o getopt1.o version.o $(ALLOCA) $(LIBOBJS) \
	fnmatch.o regex.o 

sdiff_o = getopt.o getopt1.o version.o $(LIBOBJS)

diff: diff.o $(diff_o)
	$(BUILD)

sdiff: sdiff.o  $(sdiff_o)
	$(BUILD)

diff3_o = diff3.o getopt.o getopt1.o version.o $(LIBOBJS)
diff3: $(diff3_o)
	$(BUILD)

cmp_o = cmp.o cmpbuf.o error.o getopt.o getopt1.o xmalloc.o version.o $(LIBOBJS)
cmp: $(cmp_o)
	$(BUILD)

dircmp : diff
	cp diff dircmp

diff_wrap.o: diff.c diff_wrap.c regex.h fnmatch.h getopt.h diff.h system.h
diff.exe: diff_wrap.o $(diff_o)
	$(BUILD)

sdiff_wrap.o: sdiff.c sdiff_wrap.c system.h getopt.h
sdiff.exe : sdiff_wrap.o  $(sdiff_o)
	$(BUILD)

dircmp.exe : diff.exe
	cp diff.exe dircmp.exe

$(diff_o): diff.h system.h
cmp.o diff3.o sdiff.o: system.h
context.o diff.o regex.o: regex.h
cmp.o diff.o diff3.o sdiff.o getopt.o getopt1.o: getopt.h
diff.o fnmatch.o: fnmatch.h
analyze.o cmpbuf.o cmp.o: cmpbuf.h

clean:
	rm -f *.o core
	rm -f *.aux *.cp *.cps *.dvi *.fn *.fns *.ky *.kys *.log
	rm -f *.pg *.pgs *.toc *.tp *.tps *.vr *.vrs

clobber:
	rm -f $(PROGRAMS) *.o

mostlyclean: clean

distclean: clean
	rm -f Makefile config.cache config.h config.log config.status stamp-h

install:
	install -c diff $(DESTDIR)
	install -c diff3 $(DESTDIR)
	install -c dircmp $(DESTDIR)
	install -c sdiff $(DESTDIR)
	install -c diff.exe $(DESTDIR_BIN_WIN)
	install -c sdiff.exe $(DESTDIR_BIN_WIN)
	install -c dircmp.exe $(DESTDIR_BIN_WIN)
	$(INSTALL) diff.pdb $(DESTDIR_PDB)
	$(INSTALL) diff.exe.pdb $(DESTDIR_PDB)
	$(INSTALL) diff3.pdb $(DESTDIR_PDB)
	$(INSTALL) sdiff.exe.pdb $(DESTDIR_PDB)
	$(INSTALL) sdiff.pdb $(DESTDIR_PDB)

fullbuild: clobber sdiff diff diff3 dircmp sdiff.exe diff.exe dircmp.exe install

