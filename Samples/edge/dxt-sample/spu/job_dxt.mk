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

SPU_SRCS		= job_dxt.cpp
SPURS_JOB_TARGET= $(OBJS_DIR)/job_dxt.spu.elf
SPU_CFLAGS		+= -fpic
SPU_CXXFLAGS	+= -fpic
SPU_LDFLAGS		+= -fpic -mspurs-job-initialize -Ttext=0x0 -Wl,-q
SPU_LIBS		+= $(EDGE_TARGET_ROOT)/spu/lib/libedgedxt$(EDGE_BUILD_SUFFIX).a
SPU_LDLIBS		+= -ldma -lspurs 

include $(CELL_MK_DIR)/sdk.target.mk

# Local Variables:
# mode: Makefile
# End:
