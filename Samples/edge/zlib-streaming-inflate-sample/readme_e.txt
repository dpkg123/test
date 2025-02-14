[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

"zlib-large-segmented-inflate-sample" showed how a larger file could have been segmented and 
compressed offline (see "zlib-segcomp-sample") and then decompressed at run-time and recombined to
give the original master file again.
"zlib-streaming-inflate-sample" does the same, except that instead of loading the entire compressed
file into a separate buffer before decompressing to the output buffer (as in
zlib-large-segmented-inflate-sample), it creates a streaming thread which loads and decompresses
several files directly into their destination, a segment at a time.

Note that all output of this program is over TTY and there is nothing onscreen.


<Description of Sample Program>

The main PPU thread creates a new PPU thread for file loading and decompression. The main thread
issues two calls to start streaming decompression of two files ("assets/elephant-color.bin.zlib.segs"
and "assets/elephant-normal.bin.zlib.segs") which adds two entries to a streaming queue. The streaming
thread code then fetches the entries from this queue, one at a time, and decompresses them via
the SPU Inflate Task.

The streaming decompression operates a segment at a time. The segment is loaded from disk into
the destination where the decompressed data is expected.
While the segment is loading other threads can execute.
After the segment loads, if it is compressed it will call "edgeZlibAddInflateQueueElement()" to
have the SPU Inflate Task decompress the segment in place.
When the file is completely decompressed, the user supplied PPU callback is called.
The callBack verifies that the decompressed buffer contains the same data as the original larger
master file (eg. "assets/elephant-color.bin"), prints statistics and frees the decompressed memory.
While this is going on, the original main PPU thread is available to do other game code.

The ppu2ppu_queue code has been written to provide a generic queue
for PPU to PPU communication.  If it has to wait for a queue entry
to be available then it will wait, while allowing other threads to execute,
and will wake up when the queue entry is available.

This sample only creates one SPU Inflate Task. You can have it use more SPUs in parallel by
creating more Inflate Tasks, but there is no benefit in this sample since it takes so long for
the segmented input to be loaded from the disk.


<Files>
main.cpp                        : Main PPU thread and main callback, file verification and statistics.
streaming.cpp                   : Streaming initialization & shutdown, start decompression interface
streaming.h                     :   and streaming thread decompressor with SPU Inflate Task control.
ppu2ppu_queue.cpp               : A generic PPU-to-PPU queue (ring buffer) which will not busy-wait
ppu2ppu_queue.h                 :   when it has to wait for a queue entry being available.
assets/assets.mk                : Makefile that fetches assets from "common/assets" into the sample's
                                  local assets folder and builds ready for use
assets/elephant-color.bin.segs  : The compressed version of the first test file
assets/elephant-color.bin       : The master binary uncompressed file
assets/elephant-normal.bin.segs : The compressed version of the second test file
assets/elephant-normal.bin      : The master binary uncompressed file


<Running the Program>
    - Set fileserving root to the zlib-streaming-inflate-sample directory.
    - Run the .self program.
