#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2009 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

EDGE_TARGET_ROOT = ../../../target

CELL_SDK ?= /usr/local/cell
CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

include $(EDGE_TARGET_ROOT)/common/include/edge/edge_common.mk

GTF_LOADER_DIR  = $(GCMCOMMONDIR)/../gtf

EMBEDDED_BINS	=	spu/$(OBJS_DIR)/downsample.spu.bin \
					spu/$(OBJS_DIR)/upsample.spu.bin \
					spu/$(OBJS_DIR)/horizontalgauss.spu.bin \
					spu/$(OBJS_DIR)/verticalgauss.spu.bin \
					spu/$(OBJS_DIR)/bloomcapture.spu.bin \
					spu/$(OBJS_DIR)/dof.spu.bin \
					spu/$(OBJS_DIR)/fuzziness.spu.bin \
					spu/$(OBJS_DIR)/motionblur.spu.bin \
					spu/$(OBJS_DIR)/rop.spu.bin \
					spu/$(OBJS_DIR)/ilr.spu.bin

PPU_SRCS		=	main.cpp \
					spupostprocessing.cpp \
					gpupostprocessing.cpp \
					$(VPSHADER_PPU_OBJS) $(FPSHADER_PPU_OBJS) \
					spu/$(OBJS_DIR)/job_geom.spu.elf \
					spu/$(OBJS_DIR)/job_send_event.spu.elf \
					spu/$(OBJS_DIR)/posttask.spu.elf \
					$(EMBEDDED_BINS) \
					assets/elephant-color.bin \
					assets/elephant-normal.bin
					
PPU_TARGET		=	post-sample.$(EDGE_BUILD).ppu.elf

PPU_INCDIRS		+=	-I$(CELL_FW_DIR)/include -I$(GCMCOMMONDIR) -I$(GTF_LOADER_DIR)

PPU_LIBS		+=	$(CELL_FW_DIR)/libfwgcm.a \
					$(GCMCOMMONDIR)/gcmutil.a \
					$(GTF_LOADER_DIR)/cellGtfLoader.a \
					$(EDGE_SAMPLE_COMMON_DIR)/libedgesamplecommon$(EDGE_BUILD_SUFFIX).a \
					$(EDGE_TARGET_ROOT)/ppu/lib/libedgepost$(EDGE_BUILD_SUFFIX).a
					
PPU_LDLIBS	 	+=	-lusbd_stub -lnet_stub -lio_stub -lm -lstdc++ -lspurs_stub -lgcm_cmd -lgcm_sys_stub -lsysutil_stub -lfs_stub -lsysmodule_stub

GCC_PPU_CXXFLAGS 	+= --param large-function-growth=800
GCC_PPU_CFLAGS	 	+= --param large-function-growth=800
SNC_PPU_CXXFLAGS 	+= 
SNC_PPU_CFLAGS		+= 

VPSHADER_SRCS	=	vpshader.cg \
					vpsky.cg \
					shaders/postvp.cg

FPSHADER_SRCS	=	fpshader.cg \
					fpsky.cg \
					shaders/post_copyfp.cg \
					shaders/post_spu_compositefp.cg \
					shaders/post_bloomcapturefp.cg \
					shaders/post_bloomcompositefp.cg \
					shaders/post_depthdownsamplefp.cg \
					shaders/post_doffp.cg \
					shaders/post_nearfuzzinessfp.cg \
					shaders/post_farfuzzinessfp.cg \
					shaders/post_gauss7x1fp.cg \
					shaders/post_gauss1x7fp.cg \
					shaders/post_ilrfp.cg \
					shaders/post_motionblurfp.cg

vpshader.cg: utils.cg
vpsky.cg: utils.cg

VPSHADER_PPU_OBJS = $(patsubst %.cg, $(OBJS_DIR)/%.ppu.o, $(VPSHADER_SRCS))
FPSHADER_PPU_OBJS = $(patsubst %.cg, $(OBJS_DIR)/%.ppu.o, $(FPSHADER_SRCS))

# Copy the ELFs for the embedded SPU binary files locally so the debugger finds them automatically
LOCAL_EMBEDDED_ELFS	+= $(patsubst %.spu.bin,%.$(EDGE_BUILD).spu.elf,$(notdir $(EMBEDDED_BINS)))

include $(CELL_MK_DIR)/sdk.target.mk

PPU_OBJS += $(VPSHADER_PPU_OBJS) $(FPSHADER_PPU_OBJS)

$(VPSHADER_PPU_OBJS): $(OBJS_DIR)/%.ppu.o : %.vpo
	@mkdir -p $(dir $(@))
	$(PPU_OBJCOPY)  -I binary -O elf64-powerpc-celloslv2 -B powerpc $< $@

$(FPSHADER_PPU_OBJS): $(OBJS_DIR)/%.ppu.o : %.fpo
	@mkdir -p $(dir $(@))
	$(PPU_OBJCOPY)  -I binary -O elf64-powerpc-celloslv2 -B powerpc $< $@

# Local Variables:
# mode: Makefile
# End:
