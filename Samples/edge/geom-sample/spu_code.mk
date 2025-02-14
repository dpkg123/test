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

# The .bin files in the PPU_SRCS will be converted to .ppu.obj files
# according to rules included in the Cell SDK makefiles included above.
PPU_SRCS    = spu_code.cpp spu/$(OBJS_DIR)/job_geom.spu.elf

# We don't need to build a stub library since we don't export
# anything.  So we overload the default PRX flags to *not* include the
# PPU_PRX_GENSTUB string.
PPU_PRX_LDFLAGS         = $(PPU_PRX_LEVEL) $(PPU_PRX_GENPRX)

PPU_PRX_TARGET  = $(OBJS_DIR)/spu_code.prx

all: $(PPU_PRX_TARGET)

include $(CELL_MK_DIR)/sdk.target.mk
