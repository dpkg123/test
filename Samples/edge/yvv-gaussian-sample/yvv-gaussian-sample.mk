#########################################################
# SCE CONFIDENTIAL
# $EdgeLibId$
# Copyright (C) 2009 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

EDGE_TARGET_ROOT = ../../../target

CELL_SDK ?= /usr/local/cell
CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
GTF_LOADER_DIR  = $(GCMCOMMONDIR)/../gtf

include $(CELL_MK_DIR)/sdk.makedef.mk

include $(EDGE_TARGET_ROOT)/common/include/edge/edge_common.mk


PPU_SRCS		=	main.cpp \
				spupostprocessing.cpp \
				spu/$(OBJS_DIR)/yvv-gaussian.spu.elf
					
PPU_TARGET		=	yvv-gaussian-sample.$(EDGE_BUILD).ppu.elf

PPU_INCDIRS		+=	-I$(CELL_FW_DIR)/include -I$(GCMCOMMONDIR) -I$(GTF_LOADER_DIR)

PPU_LIBS		+=	$(CELL_FW_DIR)/libfwgcm.a \
					$(GCMCOMMONDIR)/gcmutil.a \
					$(GTF_LOADER_DIR)/cellGtfLoader.a \
					$(EDGE_SAMPLE_COMMON_DIR)/libedgesamplecommon$(EDGE_BUILD_SUFFIX).a \
					$(EDGE_TARGET_ROOT)/ppu/lib/libedgepost$(EDGE_BUILD_SUFFIX).a
					
PPU_LDLIBS	 	+=	-lusbd_stub -lnet_stub -lio_stub -lm -lstdc++ -lspurs_stub -lgcm_cmd -lgcm_sys_stub -lsysutil_stub -lfs_stub -lsysmodule_stub

GCC_PPU_CXXFLAGS 	+= --param large-function-growth=800
GCC_PPU_CFLAGS	 	+= --param large-function-growth=800
SNC_PPU_CXXFLAGS 	+= 
SNC_PPU_CFLAGS		+= 

# Copy the ELFs for the embedded SPU binary files locally so the debugger finds them automatically
LOCAL_EMBEDDED_ELFS	+= $(patsubst %.spu.bin,%.$(EDGE_BUILD).spu.elf,$(notdir $(EMBEDDED_BINS)))

include $(CELL_MK_DIR)/sdk.target.mk


# Local Variables:
# mode: Makefile
# End:
