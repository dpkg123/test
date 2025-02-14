#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

CC					= g++
INCDIR				= -I ../../../../target/common/include/

LZMACOMPRESS_DIR	= lzma
LZMA_LIB			= $(LZMACOMPRESS_DIR)/lzmacompress.a
LIBS				= $(LZMA_LIB)

CCFLAGS				= -c -g -O3 $< -o $@ $(INCDIR) -MD -Wall
LINKFLAGS			= -luuid

OUTEXE				= lzma-segcomp-sample.exe

OBJS				= objs/edgesegcomp.o \
					  objs/file.o

.PHONY: all
all: $(OUTEXE)

.PHONY: MakeLzmaLib
MakeLzmaLib:
	$(MAKE) -C $(LZMACOMPRESS_DIR)

$(LZMA_LIB): MakeLzmaLib
	true;

$(OUTEXE): $(OBJS) $(LIBS)
	@mkdir -p $(dir $(OUTEXE))
	@echo
	@echo link $(notdir $@)
	$(CC) -o $@ $(OBJS) $(LIBS) $(LINKFLAGS)

objs/%.o: %.cpp
	@echo
	@echo compile $(notdir $<)
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS)

clean:
	rm -f $(OBJS) $(OUTEXE)
	rm -f $(subst .o,.d,$(OBJS))
	$(MAKE) -C $(LZMACOMPRESS_DIR) clean


-include $(subst .o,.d,$(OBJS))

# Local Variables:
# mode: Makefile
# End:
