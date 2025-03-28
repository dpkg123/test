#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

EDGE_TARGET_ROOT	= ../../../target

CELL_SDK ?= /usr/local/cell
CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

include $(EDGE_TARGET_ROOT)/common/include/edge/edge_common.mk

PPU_TARGET	= zlib-streaming-inflate-sample.$(EDGE_BUILD).ppu.elf

PPU_SRCS	= \
	main.cpp streaming.cpp ppu2ppu_queue.cpp

GCC_PPU_CXXFLAGS 	+= --param large-function-growth=800
GCC_PPU_CFLAGS	 	+= --param large-function-growth=800
SNC_PPU_CXXFLAGS 	+= 
SNC_PPU_CFLAGS		+= 

PPU_LIBS			+= $(EDGE_TARGET_ROOT)/ppu/lib/libedgezlib$(EDGE_BUILD_SUFFIX).a \
                       $(EDGESEGCOMPPPUDIR)/libedgezlibsegcomp$(EDGE_BUILD_SUFFIX).a
PPU_LDLIBS			+= -lspurs_stub -lsync_stub -lsysmodule_stub -lfs_stub -lpthread

# Copy library ELFs locally so the debugger finds them automatically
LOCAL_EMBEDDED_ELFS += edgezlib_inflate_task.$(EDGE_BUILD).spu.elf

include $(CELL_MK_DIR)/sdk.target.mk

# Local Variables:
# mode: Makefile
# End:
