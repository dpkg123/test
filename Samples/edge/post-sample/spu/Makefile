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

SUBDIRS = $(EDGEPOSTSPUDIR) $(EDGEGEOMSPUDIR) ../../common/spu

MK_TARGET = job_geom.mk \
			job_send_event.mk \
			downsample.mk \
			upsample.mk \
			horizontalgauss.mk \
			verticalgauss.mk \
			bloomcapture.mk \
			dof.mk \
			motionblur.mk \
			rop.mk \
			fuzziness.mk \
			ilr.mk \
			posttask.mk

include $(CELL_MK_DIR)/sdk.target.mk
