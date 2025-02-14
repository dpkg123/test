#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################

CC					= g++
INCDIR				= -I ../../../../target/common/include/
INCDIR				+= -I lzo1x/include/

LZO1XCOMPRESS_DIR	= lzo1x
LZO1X_LIB			= $(LZO1XCOMPRESS_DIR)/lzo1xcompress.a
LIBS				= $(LZO1X_LIB)

CCFLAGS				= -c -g -O3 $< -o $@ $(INCDIR) -MD -Wall
LINKFLAGS			=

OUTEXE				= lzo1x-segcomp-sample.exe

OBJS				= objs/edgesegcomp.o \
					  objs/file.o \
					  objs/edgecompress.o

.PHONY: all
all: $(OUTEXE)

.PHONY: MakeLzo1xLib
MakeLzo1xLib:
	$(MAKE) -C $(LZO1XCOMPRESS_DIR)

$(LZO1X_LIB): MakeLzo1xLib
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
	$(MAKE) -C $(LZO1XCOMPRESS_DIR) clean


-include $(subst .o,.d,$(OBJS))

# Local Variables:
# mode: Makefile
# End:
