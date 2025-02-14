[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

"lzma-basic-inflate-sample" showed a simple example of decompressing a single small file using Edge LZMA.
"lzma-large-segmented-inflate-sample" now shows how a larger file could have been segmented and compressed offline
(see "lzma-segcomp-sample") and then decompressed at run-time and recombined to give the original master file again.

Note that all output of this program is over TTY and there is nothing onscreen.


<Description of Sample Program>

The program loads the compressed segmented file ("assets/elephant-color.bin.lzma.segs") which will be decompressed.

This sample creates the Inflate Queue and five Inflate Tasks (one for every SPU the decompression will be running on).

The program then calls "AddLzmaSegsFileItemsToInflateQueue()" which will parse the TOC (Table of Contents)
of the ".segs" file and add elements to the Inflate Queue to request that the segments be decompressed.

As the SPUs decompress the segments they write the segments out to the appropriate locations within a buffer in main memory
so that once all the segments have finished decompressing this buffer will contain the original larger master file
("data/elephant-color.bin") reassembled in it, which is then verified against the original uncompressed file.


<Files>
main.cpp                        : Main PPU code
assets/assets.mk                : Makefile that fetches assets from "common/assets" into the sample's local assets folder and
                                  builds ready for use
assets/elephant-color.bin.segs  : The compressed version of the test file
assets/elephant-color.bin       : The master binary uncompressed file


<Running the Program>
    - Set fileserving root to the lzma-large-segmented-inflate-sample directory.
    - Run the .self program.
