#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

EDGE_TARGET_ROOT = ../../../target
export EDGE_SAMPLE_COMMON_DIR = ../common

CELL_SDK ?= /usr/local/cell
CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

include $(EDGE_TARGET_ROOT)/common/include/edge/edge_common.mk

SUBDIRS		=	$(CELL_FW_DIR) $(GCMCOMMONDIR) $(EDGEANIMPPUDIR) $(EDGE_SAMPLE_COMMON_DIR) spu assets

MK_TARGET	+= blendshape-anim-sample.mk

include $(CELL_MK_DIR)/sdk.target.mk

# Local Variables:
# mode: Makefile
# End:
