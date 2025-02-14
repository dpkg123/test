#!/usr/bin/make

# FCollada makefile for PS3

SUBDIRS=LibXML FMath FUtils FCDocument FArchiveXML

all:
	for i in $(SUBDIRS); do make -C $$i all; done

clean:
	for i in $(SUBDIRS); do make -C $$i clean; done

