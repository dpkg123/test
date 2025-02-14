[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

"zlib-streaming-inflate-sample" showed that multiple large files could have been segmented and
compressed offline (see "zlib-segcomp-sample") and then streamed in and decompressed in segments and
recombined to give the original master file again.
"zlib-localmemory-streaming-inflate-sample" does the same, except that the final destination of the
uncompressed file is in local memory (VRAM) which has an effect on how you would want to stream the
data in.

Note that all output of this program is over TTY and there is nothing onscreen.


<Description of Sample Program>

The main PPU thread creates a new PPU thread for file loading and decompression. The main thread
issues two calls to start streaming decompression of two files ("assets/elephant-color.bin.zlib.segs"
and "assets/elephant-normal.bin.zlib.segs") which adds two entries to a streaming queue. The streaming
thread code then fetches the entries from this queue, one at a time, and decompresses them via
the SPU Inflate Task.

The streaming decompression operates a segment at a time. The compressed segment is loaded from disk
into one of two input buffers in main memory. This "buffered input" is done since you should not
load the compressed input directly into local memory since it is then extremely slow for the
Inflate task decompression code to read from local memory.
While the segment is loading other threads can execute.
After the segment loads, it will call "edgeZlibAddInflateQueueElement()" to have the SPU Inflate
Task decompress (or move if already uncompressed) the segment from the input buffer in main memory
to the destination in local memory.
When the file is completely decompressed, the user supplied PPU callback is called.
The callBack verifies that the decompressed buffer contains the same data as the original larger
master file (eg. "assets/elephant-color.bin"), and prints statistics.
While this is going on, the original main PPU thread is available to do misc. game code.

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
    - Set fileserving root to the zlib-localmemory-streaming-inflate-sample directory.
    - Run the .self program.
