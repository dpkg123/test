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

PPU_SRCS		=	main.cpp $(VPSHADER_PPU_OBJS) $(FPSHADER_PPU_OBJS) \
					spu/$(OBJS_DIR)/job_anim.spu.elf spu/$(OBJS_DIR)/job_send_event.spu.elf
PPU_TARGET		=	anim-sample.$(EDGE_BUILD).ppu.elf

PPU_INCDIRS		+=	-I$(CELL_FW_DIR)/include -I$(GCMCOMMONDIR)
PPU_LIBS		+=	$(CELL_FW_DIR)/libfw.a \
                    $(PSGLDIR)/libpsgl.a \
                    $(PSGLDIR)/libpsglu.a \
                    $(PSGLDIR)/libpsglfx.a \
                    $(CELL_SDK)/target/ppu/lib/libresc_stub.a \
                    $(EDGE_TARGET_ROOT)/ppu/lib/libedgeanim$(EDGE_BUILD_SUFFIX).a
PPU_LDLIBS	 	+=	-lusbd_stub -lnet_stub -lfs_stub -lio_stub -lm -lstdc++ -lspurs_stub -lgcm_cmd -lgcm_sys_stub -lsysutil_stub -lsysmodule_stub

GCC_PPU_CXXFLAGS 	+= --param large-function-growth=800 -DPSGL
GCC_PPU_CFLAGS	 	+= --param large-function-growth=800 -DPSGL
SNC_PPU_CXXFLAGS 	+= -DPSGL
SNC_PPU_CFLAGS		+= -DPSGL

include $(CELL_MK_DIR)/sdk.target.mk

# Local Variables:
# mode: Makefile
# End:
