#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################


.PHONY: all
all: $(EDGE_COPY_ASSETS) $(EDGE_BUILD_ASSETS)


##############################################
## Common defines
##############################################

EDGE_TARGET_ROOT		= ../../../../target
EDGE_HOST_BIN			= ../../../../host-win32/bin

EDGE_ANIM_COMPILER		= $(EDGE_HOST_BIN)/edgeanimcompiler.exe 
EDGE_GEOM_COMPILER		= $(EDGE_HOST_BIN)/edgegeomcompiler.exe 
EDGE_ZLIB_SEGCOMP_PATH	= $(EDGE_SAMPLES_ROOT)/zlib-segcomp-sample/host
EDGE_ZLIB_SEGCOMP_EXE	= $(EDGE_ZLIB_SEGCOMP_PATH)/zlib-segcomp-sample.exe
EDGE_LZMA_SEGCOMP_PATH	= $(EDGE_SAMPLES_ROOT)/lzma-segcomp-sample/host
EDGE_LZMA_SEGCOMP_EXE	= $(EDGE_LZMA_SEGCOMP_PATH)/lzma-segcomp-sample.exe
EDGE_LZO1X_SEGCOMP_PATH	= $(EDGE_SAMPLES_ROOT)/lzo1x-segcomp-sample/host
EDGE_LZO1X_SEGCOMP_EXE	= $(EDGE_LZO1X_SEGCOMP_PATH)/lzo1x-segcomp-sample.exe


EDGE_COMMON_ASSETS_DIR	= $(EDGE_SAMPLES_ROOT)/common/assets


##############################################
## Copy rules
##############################################

#Copy the source assets out of the EDGE_COMMON_ASSETS_DIR
$(EDGE_COPY_ASSETS) : % : $(EDGE_COMMON_ASSETS_DIR)/%
ifeq ($(findstring cmd,$(SHELL)),)
	cp $< $@
else
	copy $(subst /,\,$<) $(subst /,\,$@)
endif


##############################################
## Build rules
##############################################

# asset building rules : geometry
# - create partionned geometry with default "flavor"
# - also output inverse bind matrices (required for skinning)
%.edge : $(EDGE_COMMON_ASSETS_DIR)/%.dae $(EDGE_GEOM_COMPILER)
	$(EDGE_GEOM_COMPILER) --inv-bind-mats-out $@.invbind $< $@

#Build .zlib.segs data from the copied assets
%.zlib.segs : % $(EDGE_ZLIB_SEGCOMP_EXE)
	$(EDGE_ZLIB_SEGCOMP_EXE) $< -o $@ -9

#Build .lzma.segs data from the copied assets
%.lzma.segs : % $(EDGE_LZMA_SEGCOMP_EXE)
	$(EDGE_LZMA_SEGCOMP_EXE) $< -o $@ -9

#Build .lzo1x.segs data from the copied assets
%.lzo1x.segs : % $(EDGE_LZO1X_SEGCOMP_EXE)
	$(EDGE_LZO1X_SEGCOMP_EXE) $< -o $@ -9


##############################################
## Detect missing exe rules
##############################################

$(EDGE_ZLIB_SEGCOMP_EXE) :
	@echo ERROR: $(EDGE_ZLIB_SEGCOMP_EXE) doesn\'t exist
	@false

$(EDGE_LZMA_SEGCOMP_EXE) :
	@echo ERROR: $(EDGE_LZMA_SEGCOMP_EXE) doesn\'t exist
	@false

$(EDGE_LZO1X_SEGCOMP_EXE) :
	@echo ERROR: $(EDGE_LZO1X_SEGCOMP_EXE) doesn\'t exist
	@false

$(EDGE_ANIM_COMPILER) :
	@echo ERROR: $(EDGE_ANIM_COMPILER) doesn\'t exist
	@false

$(EDGE_GEOM_COMPILER) :
	@echo ERROR: $(EDGE_GEOM_COMPILER) doesn\'t exist
	@false


##############################################
## Clean rules
##############################################

EDGE_CLEAN_ASSETS		+= $(EDGE_BUILD_ASSETS)
EDGE_CLEAN_ASSETS		+= $(EDGE_COPY_ASSETS)
EDGE_CLEAN_ASSETS		+= $(EDGE_BUILD_ASSETS:%.edge=%.edge.invbind)
EDGE_CLEAN_ASSETS		+= $(EDGE_BUILD_ASSETS:%.edge=%.edge.h)

#clean everyting
clean:
ifeq ($(findstring cmd,$(SHELL)),)
	rm -f $(EDGE_CLEAN_ASSETS)
else
	del $(subst /,\,$(EDGE_CLEAN_ASSETS))
endif


# Local Variables:
# mode: Makefile
# End:
