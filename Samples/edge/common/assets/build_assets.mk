#########################################################
# SCE CONFIDENTIAL
# PlayStation(R)Edge 1.2.0
# Copyright (C) 2007 Sony Computer Entertainment Inc.
# All Rights Reserved.
#########################################################


# This file exists for rebuilding any data in "samples/edge/common/assets"
# which we do not normally build in the samples build process (eg. because
# the user may not have gzip on their path).


FILES_FOR_GZIP		= 
FILES_FOR_GZIP		+= bs_cube.dae
FILES_FOR_GZIP		+= elephant-color.bin

TARGET_GZIP_FILES	= 
TARGET_GZIP_FILES	+= $(patsubst %,%.gz,$(FILES_FOR_GZIP))


FILES_FOR_LZMA		= 
FILES_FOR_LZMA		+= bs_cube.dae

TARGET_LZMA_FILES	= 
TARGET_LZMA_FILES	+= $(patsubst %,%.lzma,$(FILES_FOR_LZMA))



all: $(TARGET_GZIP_FILES) $(TARGET_LZMA_FILES)


$(TARGET_GZIP_FILES) : %.gz : %
	gzip $< -9 -c > $@


$(TARGET_LZMA_FILES) : %.lzma : %
	lzma.exe e $< $@ -a1 -d16


clean:
	rm -f $(TARGET_GZIP_FILES)
	rm -f $(TARGET_LZMA_FILES)


# Local Variables:
# mode: Makefile
# End:
