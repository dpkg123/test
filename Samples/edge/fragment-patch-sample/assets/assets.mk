#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################


EDGE_SAMPLES_ROOT		= ../..

EDGE_COPY_ASSETS		+= elephant-color.bin
EDGE_COPY_ASSETS		+= elephant-normal.bin
EDGE_COPY_ASSETS		+= elephant-matrices.bin

EDGE_BUILD_ASSETS		+= elephant.edge

include $(EDGE_SAMPLES_ROOT)/common/assets/asset_rules.mk


# Local Variables:
# mode: Makefile
# End:
