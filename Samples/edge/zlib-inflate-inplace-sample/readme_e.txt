[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

"zlib-large-segmented-inflate-sample" showed an example of using Edge Zlib to decompress a large file which had been segmented and
compressed offline (see "zlib-segcomp-sample"). It did this by loading the compressed version of the file into one buffer and then
decompressing and rebuilding the uncompressed file in a different buffer.

"zlib-inflate-inplace-sample" loads the compressed version of the data into the same buffer that the uncompressed data will later be
written into. This means the uncompressed version of the data is written over the top of the compressed version of the data and so
it is not necessary to have two separate buffers.

Note that all output of this program is over TTY and there is nothing onscreen.


<Description of Sample Program>

The program loads the compressed segmented file ("assets/elephant-color.bin.zlib.segs") which will be being decompressed into the same
buffer that it will be being decompressed into.

This sample creates the Inflate Queue and five Inflate Tasks (one for every SPU the decompression will be running on).

It also creates an "Inflate Segs File Task" and passes the address of where the ".segs" file has been been loaded into this task.

The "Inflate Segs File Task" performs a first linear pass over the segments in order to space them out inside the buffer,
so that each compressed segment is at the location where the uncompressed segment will be output to.  It
then adds the segment elements to the Inflate Queue so that the Inflate Tasks will decompress them.

Once the decompression is complete the decompressed output data is compared against the original master data
("data/elephant-color.bin") to verify it is correct.


<Files>
main.cpp                                                 : Main PPU code
inflate_segs_file.cpp                                    : Interface to the "Inflate Segs File Task" from the PPU
inflate_segs_file.h                                      : Interface to the "Inflate Segs File Task" from the PPU
inflate-segs-file-task/inflate-segs-file-task.cpp        : Main SPU code of the "Inflate Segs File Task"
inflate-segs-file-task/inflate_segs_file_queue_element.h : Shared header file for communication between PPU and SPU
assets/assets.mk                                         : Makefile that fetches assets from "common/assets" into the sample's
                                                           local assets folder and builds ready for use
assets/elephant-color.bin.segs                           : The compressed version of the test file
assets/elephant-color.bin                                : The master binary uncompressed file


<Running the Program>
    - Set fileserving root to the zlib-inflate-inplace-sample directory.
    - Run the .self program.
