#!/usr/bin/make
# FUtils makefile for PS3

include ../MakeDefs

DIR=FUtils/

LIBNAME = libfutils

prefix=..

INCPATH=$(prefix)/include
LIBPATH=$(prefix)/lib

PPU_INCDIRS=-I$(INCPATH) -I.. -I../LibXML/include -I./
PPU_OPTIMIZE_LV= -O3 -funroll-loops

PPU_LIB_TARGET=$(LIBNAME).a
PPU_SRCS = \
	FUAssert.cpp \
	FUBase64.cpp \
	FUBoundingBox.cpp \
	FUBoundingSphere.cpp \
	FUBoundingTest.cpp \
	FUCrc32.cpp \
	FUCrc32Test.cpp \
	FUCriticalSection.cpp \
	FUDaeEnum.cpp \
	FUDateTime.cpp \
	FUDebug.cpp \
	FUError.cpp \
	FUErrorLog.cpp \
	FUEventTest.cpp \
	FUFile.cpp \
	FUFileManager.cpp \
	FUFileManagerTest.cpp \
	FUFunctorTest.cpp \
	FULogFile.cpp \
	FUObject.cpp \
	FUObjectTest.cpp \
	FUObjectType.cpp \
	FUParameter.cpp \
	FUParameterizable.cpp \
	FUPluginManager.cpp \
	FUSemaphore.cpp \
	FUStringBuilder.cpp \
	FUStringBuilderTest.cpp \
	FUStringConversion.cpp \
	FUStringConversionTest.cpp \
	FUStringTest.cpp \
	FUSynchronizableObject.cpp \
	FUTestBed.cpp \
	FUThread.cpp \
	FUTracker.cpp \
	FUUniqueStringMap.cpp \
	FUUniqueStringMapTest.cpp \
	FUUri.cpp \
	FUXmlDocument.cpp \
	FUXmlParser.cpp \
	FUXmlWriter.cpp \
	StdAfx.cpp

%FUStringConversion.o:%FUStringConversion.cpp
	$(CPP) $(CCFLAGS) -O0 $< -o $@

include ../MakeRules

