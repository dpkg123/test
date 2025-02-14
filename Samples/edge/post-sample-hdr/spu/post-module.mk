#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2009 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

EDGE_TARGET_ROOT = ../../../../target
CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

include $(EDGE_TARGET_ROOT)/common/include/edge/edge_common.mk

SPU_SRCS		= $(EDGEPOST_MODULE_NAME).cpp $(EXTRA_OBJS)
SPU_TARGET		= $(OBJS_DIR)/$(EDGEPOST_MODULE_NAME).spu.elf
SPU_CFLAGS		+= -fpic -O2
SPU_CXXFLAGS	+= -fpic -O2
ifeq ($(EDGE_BUILD), debug)
SPU_CFLAGS		+= -DEDGE_GEOM_DEBUG
SPU_CXXFLAGS	+= -DEDGE_GEOM_DEBUG
endif
SPU_LDFLAGS		+= -fpic -nostartfiles -Ttext=0x0 -Wl,-T edgepost_job.ld
SPU_LIBS		+= $(EDGE_TARGET_ROOT)/spu/lib/libedgepost$(EDGE_BUILD_SUFFIX).a
SPU_LDLIBS		+= -ldma -lspurs $(EXTRA_OBJS)

EXTRA_TARGET	+= $(OBJS_DIR)/$(EDGEPOST_MODULE_NAME).spu.bin
$(OBJS_DIR)/$(EDGEPOST_MODULE_NAME).spu.bin : $(OBJS_DIR)/$(EDGEPOST_MODULE_NAME).spu.elf
	$(SPU_OBJCOPY) --set-section-flags .bss=load,contents -O binary $< $@

CLEANFILES		+= $(EXTRA_TARGET) $(EXTRA_OBJS) $(OBJS_DIR)/*.tmp.spa

include $(CELL_MK_DIR)/sdk.target.mk

# how to build filter_spec files
$(OBJS_DIR)/%.o: %.filter_spec $(SPA)
	@mkdir -p $(dir $(@))
	@$(EDGE_FILTERGEN) -o $@.tmp.spa $<
	@$(EDGE_SPA) -o $@ $@.tmp.spa

# how to build spa files
$(OBJS_DIR)/%.o: %.spa $(SPA)
	@mkdir -p $(dir $(@))
	@$(EDGE_SPA) -o $@ $<


# Local Variables:
# mode: Makefile
# End:
