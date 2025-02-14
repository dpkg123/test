[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample shows how an offline tool can work for splitting a file into segments and then compressing them offline. The tool
reconcatenates the compressed segments into a single smaller file which may be used in the place of the master file. This file
can then be decompressed by passing the separate segments into Edge Zlib in order to retreive the original master file.

Note that all output of this program is over TTY and there is nothing onscreen.


<Description of Sample Program>

There is an offline tool in the "host" folder which shows segmented compression.
There are also PPU and SPU libraries for interfacing with the segmented file created.  

The "zlib-segcomp-sample" program loads the master file and splits it into appropriate sized segments. Each segment is then compressed
separately. The program then writes out a file containing a header and TOC (Table of Contents),
followed by all the compressed data for the segments. If compression actually made the data larger, then the uncompressed
version of the data is used and this is flagged in the file format appropriately.

The PPU and SPU libraries provide functions for parsing the file and adding elements to the Inflate Queue for the file to be
decompressed.

This sample is used as a building block for other Edge Zlib samples.


<Files>
common/segcompfile.h                    : Shared header that defines the file format.
common/endian.h                         : Handles endian-ness issues.
host/edgesegcomp.cpp                    : The main code of the tool
host/edgecompress.cpp                   : Call zlib compression, but without 2 byte header and 4 byte footer output
host/edgecompress.h                     : Call zlib compression, but without 2 byte header and 4 byte footer output
host/file.cpp                           : File loading helper functions for the tool
host/file.h                             : File loading helper functions for the tool
host/Makefile                           : Makefile for edgesegcomp.exe
host/zlib-segcomp-sample.sln            : Visual Studio solution
host/zlib-segcomp-sample.vcproj         : Visual Studio project
host/zlib/adler32.c, compress.c, crc32.c, crc32.h, deflate.c, deflate.h, trees.c, trees.h, zconf.h,
	zlib.h,zutil.c, zutil.h             : Source for zlib compression (unmodified)
host/zlib/Makefile                      : Makefile for zlib host library compilation
host/zlib/zlibcompress.sln              : Visual Studio zlib solution	
host/zlib/zlibcompress.vcproj           : Visual Studio zlib project
ppu/seg_ppu.cpp                         : Functions for interfacing with a ".segs" file from PPU
ppu/seg_ppu.h                           : Functions for interfacing with a ".segs" file from PPU
ppu/Makefile                            : Makefile for PPU segs library compilation
ppu/segs_ppu.vcproj                     : Visual Studio project
spu/seg_spu.cpp                         : Functions for interfacing with a ".segs" file from SPU
spu/seg_spu.h                           : Functions for interfacing with a ".segs" file from SPU
spu/Makefile                            : Makefile for SPU segs library compilation
spu/segs_spu.vcproj                     : Visual Studio project

<Library & Executable files created after compilation for the executable>
host/zlib-segcomp-sample.exe            : The MinGW (optimized) version of the segmented compression tool
host/Debug/zlib-segcomp-sample.exe      : The Windows Debug (unoptimized) version of the segmented compression tool
host/Release/zlib-segcomp-sample.exe    : The Windows Release (optimized) version of the segmented compression tool
host/zlib/zlibcompress.a                : The MinGW (optimized) library of zlib
host/zlib/Debug/libcompress.lib         : The Windows Debug (unoptimized) library of zlib
host/zlib/Release/libcompress.lib       : The Windows Release (optimized) library of zlib

<Library files created after optional(not needed) compilation for the ppu/spu libraries>
ppu/libedgesegcomp.a                    : Library file from MinGW
ppu/PS3_PPU_Debug/segs_ppu.lib          : Library file from VS Debug
ppu/PS3_PPU_Release/segs_ppu.lib        : Library file from VS Release
spu/libedgesegcomp.a                    : Library file from MinGW
spu/PS3_Debug/segs_spu.lib              : Library file from VS Debug
spu/PS3_Release/segs_spu.lib            : Library file from VS Release

<Running the Program>
    - Run the zlib-segcomp-sample offline tool in order to convert an input file into a segmented compressed file.
    - Pass the name of the file to convert as a parameter.

<Notice>
Users using Windows Vista might get following error while compiling the host tool:
"g++.exe: installation problem, cannot exec `cc1plus': No such file or directory"
This is a known GCC bug with Vista. ( http://gcc.gnu.org/bugzilla/show_bug.cgi?id=33281 )

