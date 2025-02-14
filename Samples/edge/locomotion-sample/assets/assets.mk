#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################


EDGE_SAMPLES_ROOT		= ../..

SKELETON_PS3			= M_STA_M_03_P2.skel 
ANIMATIONS_PS3			= M_STA_M_03_P2_L.anim M_STA_M_04_P2_L.anim 

SKELETON_PC				= $(SKELETON_PS3:%.skel=%.skelpc)
ANIMATIONS_PC			= $(ANIMATIONS_PS3:%.anim=%.animpc)

EDGE_BUILD_ASSETS		+= $(SKELETON_PS3)
EDGE_BUILD_ASSETS		+= $(ANIMATIONS_PS3)
EDGE_BUILD_ASSETS		+= $(SKELETON_PC)
EDGE_BUILD_ASSETS		+= $(ANIMATIONS_PC)

LOCOMOTION_JOINT		= PelvisRoot

include $(EDGE_SAMPLES_ROOT)/common/assets/asset_rules.mk


# asset building rules : skeletons
# - build skeletons 
%.skel : $(EDGE_COMMON_ASSETS_DIR)/%.dae $(EDGE_ANIM_COMPILER)
	$(EDGE_ANIM_COMPILER) -skel $< $@ -locojoint $(LOCOMOTION_JOINT)
%.skelpc : $(EDGE_COMMON_ASSETS_DIR)/%.dae $(EDGE_ANIM_COMPILER)
	$(EDGE_ANIM_COMPILER) -pc -skel $< $@ -locojoint $(LOCOMOTION_JOINT)

# asset building rules : animations
# - build animations from collada files 
# - since binding is done in tools (not in the runtime) also dependent on skeletons
%.anim : $(EDGE_COMMON_ASSETS_DIR)/%.dae $(SKELETON_PS3) $(EDGE_ANIM_COMPILER) 
	$(EDGE_ANIM_COMPILER) -anim $< $(SKELETON_PS3) $@ -locojoint $(LOCOMOTION_JOINT)
%.animpc : $(EDGE_COMMON_ASSETS_DIR)/%.dae $(SKELETON_PC) $(EDGE_ANIM_COMPILER) 
	$(EDGE_ANIM_COMPILER) -pc -anim $< $(SKELETON_PC) $@ -locojoint $(LOCOMOTION_JOINT)


# Local Variables:
# mode: Makefile
# End:
