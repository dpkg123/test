#!/usr/bin/make

include ../MakeDefs

DIR=LibXML/

LIBNAME = libxml2

prefix=..

INCPATH=$(prefix)/include
LIBPATH=$(prefix)/lib

PPU_INCDIRS=-I$(INCPATH) -Iinclude
PPU_OPTIMIZE_LV= -O3 -funroll-loops

PPU_LIB_TARGET=$(LIBNAME).a
PPU_SRCS = DOCBparser.c \
	HTMLparser.c \
	HTMLtree.c \
	SAX.c \
	SAX2.c \
	c14n.c \
	catalog.c \
	chvalid.c \
	debugXML.c \
	dict.c \
	encoding.c \
	entities.c \
	error.c \
	globals.c \
	hash.c \
	legacy.c \
	list.c \
	nanoftp.c \
	nanohttp.c \
	parser.c \
	parserInternals.c \
	pattern.c \
	relaxng.c \
	threads.c \
	tree.c \
	uri.c \
	valid.c \
	xinclude.c \
	xlink.c \
	xmlIO.c \
	xmlcatalog.c \
	xmlmemory.c \
	xmlmodule.c \
	xmlreader.c \
	xmlregexp.c \
	xmlsave.c \
	xmlstring.c \
	xmlunicode.c \
	xmlwriter.c
	
%xmlsave.o:%xmlsave.c
	$(CC) $(CCFLAGS) -O0 $< -o $@

include ../MakeRules

