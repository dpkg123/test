#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

EDGE_TARGET_ROOT = ../../../target
CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

include $(EDGE_TARGET_ROOT)/common/include/edge/edge_common.mk

PPU_SRCS	= \
	profile_bars.cpp \
	vsync.cpp

PPU_LIB_TARGET	= libedgesamplecommon$(EDGE_BUILD_SUFFIX).a

PPU_INCDIRS		+=	-I$(GCMCOMMONDIR)

include $(CELL_MK_DIR)/sdk.target.mk

# Local Variables:
# mode: Makefile
# End:
