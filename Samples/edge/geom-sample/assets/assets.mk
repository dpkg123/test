#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################


EDGE_SAMPLES_ROOT		= ../..

EDGE_COPY_ASSETS		+= logo-color.bin
EDGE_COPY_ASSETS		+= logo-normal.bin

EDGE_BUILD_ASSETS		+= bs_cube.edge
EDGE_BUILD_ASSETS		+= bs_cube2.edge
EDGE_BUILD_ASSETS		+= bs_skin_cyl.edge
EDGE_BUILD_ASSETS		+= skin_cyl.edge
EDGE_BUILD_ASSETS		+= skin_cyl_customcompress.edge
EDGE_BUILD_ASSETS		+= teapot.edge

include $(EDGE_SAMPLES_ROOT)/common/assets/asset_rules.mk


# Local Variables:
# mode: Makefile
# End:
