#!/usr/bin/make

include ../MakeDefs

DIR=../FColladaPlugins/FArchiveXML/

LIBNAME = libfarchivexml

prefix=..

INCPATH=$(prefix)/include
LIBPATH=$(prefix)/lib

PPU_INCDIRS=-I$(INCPATH) -Iinclude
PPU_OPTIMIZE_LV= -O3 -funroll-loops

PPU_LIB_TARGET=$(LIBNAME).a
PPU_SRCS = FAXAnimationExport.cpp \
	FAXAnimationImport.cpp \
	FAXCameraExport.cpp \
	FAXCameraImport.cpp \
	FAXColladaParser.cpp \
	FAXColladaWriter.cpp \
	FAXControllerExport.cpp \
	FAXControllerImport.cpp \
	FAXEmitterExport.cpp \
	FAXEmitterImport.cpp \
	FAXEntityExport.cpp \
	FAXEntityImport.cpp \
	FAXForceFieldExport.cpp \
	FAXForceFieldImport.cpp \
	FAXGeometryExport.cpp \
	FAXGeometryImport.cpp \
	FAXImportLinking.cpp \
	FAXInstanceExport.cpp \
	FAXInstanceImport.cpp \
	FAXLightExport.cpp \
	FAXLightImport.cpp \
	FAXMaterialExport.cpp \
	FAXMaterialImport.cpp \
	FAXPhysicsExport.cpp \
	FAXPhysicsImport.cpp \
	FAXSceneExport.cpp \
	FAXSceneImport.cpp \
	FArchiveXML.cpp \
	StdAfx.cpp

	
%xmlsave.o:%xmlsave.c
	$(CC) $(CCFLAGS) -O0 $< -o $@

include ../MakeRules

