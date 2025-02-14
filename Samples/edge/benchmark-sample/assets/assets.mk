#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################


EDGE_SAMPLES_ROOT		= ../..

EDGE_COPY_ASSETS		+= elephant-color.bin
EDGE_COPY_ASSETS		+= elephant-normal.bin

SKELETON			= elephant.skel

EDGE_BUILD_ASSETS		+= $(SKELETON)
EDGE_BUILD_ASSETS		+= elephant.edge
EDGE_BUILD_ASSETS		+= idle.anim
EDGE_BUILD_ASSETS		+= trunk.anim
EDGE_BUILD_ASSETS		+= rear.anim

include $(EDGE_SAMPLES_ROOT)/common/assets/asset_rules.mk

# asset building rules : animations
# - build animations from collada files 
# - since binding is done in tools (not in the runtime) also dependent on skeletons
%.anim : $(EDGE_COMMON_ASSETS_DIR)/%.dae $(SKELETON) $(EDGE_ANIM_COMPILER) 
	$(EDGE_ANIM_COMPILER) -anim $< $(SKELETON) $@ -bitpacked

# asset building rules : skeletons 
# - build skeletons 
%.skel : $(EDGE_COMMON_ASSETS_DIR)/%.dae $(EDGE_ANIM_COMPILER)
	$(EDGE_ANIM_COMPILER) -skel $< $@

# Local Variables:
# mode: Makefile
# End:
