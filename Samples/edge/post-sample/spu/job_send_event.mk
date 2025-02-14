#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2009 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

EXTRA_TARGET	+= $(OBJS_DIR)/job_send_event.spu.elf 
CLEANFILES		+= $(OBJS_DIR)/job_send_event.spu.elf 

$(OBJS_DIR)/job_send_event.spu.elf : ../../common/spu/$(OBJS_DIR)/job_send_event.spu.elf
	cp $< $@

include $(CELL_MK_DIR)/sdk.target.mk

