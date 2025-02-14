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

SPU_SRCS	= job_intersect.cpp intersect.cpp
SPU_TARGET	= $(OBJS_DIR)/job_intersect.spu.elf
SPU_CFLAGS  += -fpic -Os
SPU_CXXFLAGS+= -fpic -Os
ifeq ($(EDGE_BUILD), debug)
SPU_CFLAGS  += -DEDGE_GEOM_DEBUG
SPU_CXXFLAGS+= -DEDGE_GEOM_DEBUG
endif
SPU_LDFLAGS	+= -fpic -mspurs-job-initialize -Ttext=0x0 -Wl,-q
SPU_LIBS	+= $(EDGE_TARGET_ROOT)/spu/lib/libedgegeom$(EDGE_BUILD_SUFFIX).a
SPU_LDLIBS	+= -ldma -lspurs

LEAF_FLAGS = -fcall-saved-3 -fcall-saved-4 -fcall-saved-5 -fcall-saved-6 \
	-fcall-saved-7 -fcall-saved-8 -fcall-saved-9 -fcall-saved-10 \
	-fcall-saved-11 -fcall-saved-12 -fcall-saved-13 -fcall-saved-14 \
	-fcall-saved-15 -fcall-saved-16 -fcall-saved-17 -fcall-saved-18 \
	-fcall-saved-19 -fcall-saved-20 -fcall-saved-21 -fcall-saved-22 \
	-fcall-saved-23 -fcall-saved-24 -fcall-saved-25 -fcall-saved-26 \
	-fcall-saved-27 -fcall-saved-28 -fcall-saved-29 -fcall-saved-30 \
	-fcall-saved-31 -fcall-saved-32 -fcall-saved-33 -fcall-saved-34 \
	-fcall-saved-35 -fcall-saved-36 -fcall-saved-37 -fcall-saved-38 \
	-fcall-saved-39 -fcall-saved-40 -fcall-saved-41 -fcall-saved-42 \
	-fcall-saved-43 -fcall-saved-44 -fcall-saved-45 -fcall-saved-46 \
	-fcall-saved-47 -fcall-saved-48 -fcall-saved-49 -fcall-saved-50 \
	-fcall-saved-51 -fcall-saved-52 -fcall-saved-53 -fcall-saved-54 \
	-fcall-saved-55 -fcall-saved-56 -fcall-saved-57 -fcall-saved-58 \
	-fcall-saved-59 -fcall-saved-60 -fcall-saved-61 -fcall-saved-62 \
	-fcall-saved-63 -fcall-saved-64 -fcall-saved-65 -fcall-saved-66 \
	-fcall-saved-67 -fcall-saved-68 -fcall-saved-69 -fcall-saved-70 \
	-fcall-saved-71 -fcall-saved-72 -fcall-saved-73 -fcall-saved-74 \
	-fcall-saved-75 -fcall-saved-76 -fcall-saved-77 -fcall-saved-78 \
	-fcall-saved-79

include $(CELL_MK_DIR)/sdk.target.mk
