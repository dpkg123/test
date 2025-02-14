#!/usr/bin/make
# FMath Makefile for PS3

include ../MakeDefs

DIR=FMath/

LIBNAME = libfmath

prefix=..

INCPATH=$(prefix)/include
LIBPATH=$(prefix)/lib

PPU_INCDIRS=-I$(INCPATH) -I..
PPU_OPTIMIZE_LV= -O3 -funroll-loops

PPU_LIB_TARGET=$(LIBNAME).a
PPU_SRCS = \
	FMAllocator.cpp \
	FMArrayTest.cpp \
	FMAngleAxis.cpp \
	FMLookAt.cpp \
	FMColor.cpp \
	FMInterpolation.cpp \
	FMMatrix33.cpp \
	FMMatrix44.cpp \
	FMQuaternion.cpp \
	FMQuaternionTest.cpp \
	FMRandom.cpp \
	FMSkew.cpp \
	FMTreeTest.cpp \
	FMVector3.cpp \
	FMVolume.cpp \
	StdAfx.cpp

include ../MakeRules
