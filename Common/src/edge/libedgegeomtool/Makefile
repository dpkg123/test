#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

EDGE_TARGET_ROOT = ../../../../target
CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

include $(EDGE_TARGET_ROOT)/common/include/edge/edge_common.mk

PPU_SRCS	= \
	libedgegeomtool_compressindexes.cpp \
	libedgegeomtool_internal.cpp \
	libedgegeomtool_kcacheoptimizer.cpp \
	libedgegeomtool_makeblendshapebuffers.cpp \
	libedgegeomtool_makebuffers.cpp \
	libedgegeomtool_partitioner.cpp \
	libedgegeomtool_triangulator.cpp \
	libedgegeomtool_vertexformats.cpp \
	libedgegeomtool_vertexmerger.cpp \
	libedgegeomtool_wrap.cpp

PPU_LIB_TARGET	= $(EDGE_TARGET_ROOT)/ppu/lib/libedgegeomtool$(EDGE_BUILD_SUFFIX).a

PPU_INCDIRS			+= -I../../../include

include $(CELL_MK_DIR)/sdk.target.mk

# Local Variables:
# mode: Makefile
# End:
