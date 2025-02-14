#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################


EDGE_SAMPLES_ROOT		= ../..

EDGE_COPY_ASSETS		+= character-color.bin
EDGE_COPY_ASSETS		+= character-normal.bin

SKELETON				= anim-run.skel

EDGE_BUILD_ASSETS		+= $(SKELETON)
EDGE_BUILD_ASSETS		+= anim-run.edge
EDGE_BUILD_ASSETS		+= anim-walk.anim
EDGE_BUILD_ASSETS		+= anim-walkfast.anim
EDGE_BUILD_ASSETS		+= anim-run.anim
EDGE_BUILD_ASSETS		+= anim-gorilla1.anim
EDGE_BUILD_ASSETS		+= anim-gorilla2.anim
EDGE_BUILD_ASSETS		+= anim-gorilla3.anim
EDGE_BUILD_ASSETS		+= anim-gorilla4.anim
EDGE_BUILD_ASSETS		+= anim-dance.anim

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
