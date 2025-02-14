#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

CC					= g++
INCDIR				= -I ../../../../target/common/include/

ZLIBCOMPRESS_DIR	= zlib
ZLIB_LIB			= $(ZLIBCOMPRESS_DIR)/zlibcompress.a
LIBS				= $(ZLIB_LIB)

CCFLAGS				= -c -g -O3 $< -o $@ $(INCDIR) -MD -Wall
LINKFLAGS			=

OUTEXE				= zlib-segcomp-sample.exe

OBJS				= objs/edgesegcomp.o \
					  objs/file.o \
					  objs/edgecompress.o

.PHONY: all
all: $(OUTEXE)

.PHONY: MakeZlibLib
MakeZlibLib:
	$(MAKE) -C $(ZLIBCOMPRESS_DIR)

$(ZLIB_LIB): MakeZlibLib
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
	$(MAKE) -C $(ZLIBCOMPRESS_DIR) clean


-include $(subst .o,.d,$(OBJS))

# Local Variables:
# mode: Makefile
# End:
