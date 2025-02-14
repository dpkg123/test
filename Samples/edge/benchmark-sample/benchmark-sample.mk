#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

EDGE_TARGET_ROOT = ../../../target

CELL_SDK ?= /usr/local/cell
CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

include $(EDGE_TARGET_ROOT)/common/include/edge/edge_common.mk

PPU_SRCS		=	main.cpp shaderDriver.cpp msaaResolveDriver.cpp \
					$(VPSHADER_PPU_OBJS) $(FPSHADER_PPU_OBJS) \
					spu/$(OBJS_DIR)/job_send_event.spu.elf spu/$(OBJS_DIR)/job_anim.spu.elf spu/$(OBJS_DIR)/job_geom.spu.elf 
PPU_TARGET		=	benchmark-sample.$(EDGE_BUILD).ppu.elf

PPU_INCDIRS		+=	-I$(CELL_FW_DIR)/include \
					-I$(GCMCOMMONDIR)

PPU_LIBS		+=	$(CELL_FW_DIR)/libfwgcm.a \
					$(GCMCOMMONDIR)/gcmutil.a \
					$(EDGE_SAMPLE_COMMON_DIR)/libedgesamplecommon$(EDGE_BUILD_SUFFIX).a \
					$(EDGE_TARGET_ROOT)/ppu/lib/libedgeanim$(EDGE_BUILD_SUFFIX).a

PPU_LDLIBS	 	+=	-lusbd_stub -lnet_stub -lio_stub -lm -lstdc++ -lspurs_stub -lgcm_cmd -lgcm_sys_stub -lsysutil_stub -lsysmodule_stub -lfs_stub

GCC_PPU_CXXFLAGS 	+= --param large-function-growth=800 
GCC_PPU_CFLAGS	 	+= --param large-function-growth=800 
SNC_PPU_CXXFLAGS 	+= 
SNC_PPU_CFLAGS		+= 

VPSHADER_SRCS	=	vpshader.cg vpshader-skinning.cg msaaResolveVp.cg
FPSHADER_SRCS	=	fpshader.cg msaaResolveFp.cg

VPSHADER_PPU_OBJS = $(patsubst %.cg, $(OBJS_DIR)/%.ppu.o, $(VPSHADER_SRCS))
FPSHADER_PPU_OBJS = $(patsubst %.cg, $(OBJS_DIR)/%.ppu.o, $(FPSHADER_SRCS))

include $(CELL_MK_DIR)/sdk.target.mk

PPU_OBJS += $(VPSHADER_PPU_OBJS) $(FPSHADER_PPU_OBJS)

$(VPSHADER_PPU_OBJS): $(OBJS_DIR)/%.ppu.o : %.vpo
	@mkdir -p $(dir $(@))
	$(PPU_OBJCOPY)  -I binary -O elf64-powerpc-celloslv2 -B powerpc $< $@

$(FPSHADER_PPU_OBJS): $(OBJS_DIR)/%.ppu.o : %.fpo
	@mkdir -p $(dir $(@))
	$(PPU_OBJCOPY)  -I binary -O elf64-powerpc-celloslv2 -B powerpc $< $@

# Local Variables:
# mode: Makefile
# End:
