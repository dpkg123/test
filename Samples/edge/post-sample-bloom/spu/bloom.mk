#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2009 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

EDGEPOST_MODULE_NAME = bloom
EXTRA_OBJS = $(OBJS_DIR)/horizontalgauss_filter.o $(OBJS_DIR)/verticalgauss_filter.o
include post-module.mk

# Local Variables:
# mode: Makefile
# End: