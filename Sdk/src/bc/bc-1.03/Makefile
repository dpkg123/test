LEX=lex
LEXFLAGS=
LEXLIB=-ll
YACC=yacc
YACCFLAGS=-t -v

# Makefile for GNU bc and dc.	-*- Indented-Text -*-
# Copyright (C) 1993, 1994 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#### Start of system configuration section. ####


#
# Programs definitions for use by make.
#


# stuff for cflags

CFLAGS+= -DDOT_IS_LAST

DISTFILES = COPYING Makefile.in Install bc.y bcdefs.h const.h version.h \
            execute.c global.c global.h load.c main.c number.c storage.c \
	    number.h proto.h scan.l util.c vfprintf.c README bc.1 sbc.y \
	    fix_libmath.h libmath.b configure configure.in config.h.in \
	    acconfig.h ChangeLog \
	    dc.1 dc.texinfo \
 	    dc-array.c dc-eval.c dc-misc.c dc-number.c dc-stack.c \
	    dc-string.c dc.h dc-proto.h dc-regdef.h dc-version.h \
	    install-sh
#
BCOFILES = scan.o util.o main.o number.o storage.o load.o execute.o 
DCOFILES = dc-misc.o dc-eval.o dc-stack.o dc-array.o dc-string.o \
	   dc-number.o
#
SUBDIRS = Examples Test
#
EXTRAFILES = bc.c.dist scan.c.dist y.tab.h.dist libmath.h dc.info
#

all: bc dc dc.info


bc: bc.o $(BCOFILES) global.o
	$(BUILD)
	#$(CC) $(LDFLAGS) -o bc bc.o $(BCOFILES) global.o $(LIBS)

dc: $(DCOFILES) number.o
	$(BUILD)

#sbc: sbc.o $(BCOFILES) global.o
#	$(BUILD)


libmath.h: fbc Empty libmath.b
	./fbc -c libmath.b <Empty >libmath.h
	./fix_libmath.h
	rm -f ./fbc

#
# create a zero length empty file
Empty:
	rm -f Empty
	touch Empty

fbc: $(BCOFILES) bc.o fbc-global.o
	$(CC) -o fbc $(LDFLAGS) bc.o $(BCOFILES) fbc-global.o $(LEXLIB) $(LIBS)

fbc-global.o:  bcdefs.h global.h global.c
	rm -f fbc-global.c
	cp global.c fbc-global.c
	$(CC) -c $(CFLAGS) -DCOMPILE_FOR_FBC fbc-global.c
	rm -f fbc-global.c

install:
	$(INSTALL) bc.exe $(DESTDIR)
	$(INSTALL) dc.exe $(DESTDIR)
	$(INSTALL) bc.pdb $(DESTDIR_PDB)
	$(INSTALL) dc.pdb $(DESTDIR_PDB)

fullbuild: clobber flip_all bc dc install

flip_all:
	find . -type f -exec chmod +w {} \;
	find . -type f -exec flip -u {} \;

bc.c.dist: bc.c
	cp bc.c bc.c.dist

scan.c.dist: scan.c
	cp scan.c scan.c.dist

y.tab.h.dist: y.tab.h
	cp y.tab.h y.tab.h.dist

extra:  $(EXTRAFILES)

clean:
	rm -f *.o core ERRS *~ *.bak
	rm -f y.tab.c lex.yy.c fbc-global.c
	rm -f bc.c sbc.c scan.c y.tab.h libmath.h
	rm -f sbc fbc

clobber: clean
	rm -f *.exe bc dc y.output

bc.c y.tab.h: bc.y
	rm -f y.tab.c y.tab.h
	$(YACC) $(YACCFLAGS) -d bc.y
	rm -f bc.c
	mv y.tab.c bc.c

#sbc.c y.tab.h: sbc.y
#	rm -f y.tab.c y.tab.h
#	$(YACC) $(YACCFLAGS) -d sbc.y
#	rm -f sbc.c
#	mv y.tab.c sbc.c

scan.c: scan.l
	rm -f lex.yy.c
	$(LEX) $(LEXFLAGS) scan.l
	rm -f scan.c
	mv lex.yy.c scan.c

global.o:  bcdefs.h global.h libmath.h global.c
bc.o:	    bcdefs.h global.h bc.c
execute.o: bcdefs.h global.h execute.c
load.o:    bcdefs.h global.h load.c
main.o:    bcdefs.h global.h version.h main.c
number.o:  bcdefs.h number.c
#sbc.o:     bcdefs.h global.h sbc.y
scan.o:    bcdefs.h global.h y.tab.h scan.c
storage.o: bcdefs.h global.h storage.c
util.o:    bcdefs.h global.h version.h util.c

bcdefs.h: number.h const.h config.h
	touch bcdefs.h

dc-array.o : dc-array.c config.h dc.h dc-proto.h dc-regdef.h 
dc-eval.o : dc-eval.c config.h dc.h dc-proto.h 
dc-misc.o : dc-misc.c config.h dc.h dc-proto.h dc-version.h 
dc-number.o : dc-number.c config.h bcdefs.h const.h version.h number.h \
  proto.h global.h dc.h dc-proto.h 
dc-stack.o : dc-stack.c config.h dc.h dc-proto.h dc-regdef.h 
dc-string.o : dc-string.c config.h dc.h dc-proto.h 
number.o : number.c bcdefs.h config.h const.h version.h number.h proto.h 


