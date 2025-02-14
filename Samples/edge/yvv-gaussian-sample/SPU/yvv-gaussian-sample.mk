#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2010 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

EDGE_TARGET_ROOT	= ../../../../target

CELL_SDK ?= /usr/local/cell
CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

include $(EDGE_TARGET_ROOT)/common/include/edge/edge_common.mk

SPU_TARGET	= $(OBJS_DIR)/yvv-gaussian.spu.elf

SPU_SRCS		=	yvv-gaussian.cpp
SPA_SRCS		=	yvv-gaussian-argb.spa \
					yvv-gaussian-xrgb.spa

SPU_LIBS		+= $(EDGE_TARGET_ROOT)/spu/lib/libedgepost$(EDGE_BUILD_SUFFIX).a
SPU_LDLIBS		+= -ldma -lspurs -lsync -latomic
SPU_LDFLAGS		+= -mspurs-task -Wl,--gc-sections

include $(CELL_MK_DIR)/sdk.target.mk

SPA_SPU_OBJS	= $(patsubst %.spa, $(OBJS_DIR)/%.spu.o, $(SPA_SRCS))
SPU_OBJS		+= $(SPA_SPU_OBJS)
$(SPU_TARGET): $(SPA_SPU_OBJS)

$(SPA_SPU_OBJS): $(OBJS_DIR)/%.spu.o: %.spa $(SPA)
	@mkdir -p $(dir $(@))
	@$(EDGE_SPA) $< -o $@ --dwarf2 true

# Local Variables:
# mode: Makefile
# End:
