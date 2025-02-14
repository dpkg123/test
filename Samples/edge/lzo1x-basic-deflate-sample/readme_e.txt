[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample shows a very simple use of Edge LZO compression. It loads an uncompressed file, compresses it with Edge LZO,
and writes output the compressed file.

Note that all output of this program is over TTY and there is nothing onscreen.

The compressed data in this sample must be no greater 64K.


<Description of Sample Program>

The program loads the uncompressed file ("assets/bs_cube.dae"), and allocates space for the uncompressed data.

The program creates a SPURS Taskset and a Deflate Queue, then creates an Edge LZO1X Deflate Task which it attaches to that Taskset.
The Deflate Queue will have compression work put onto it, and the Deflate Task(s) which run on SPU(s) will pull from it and process
the work.

The sample only creates 1 Deflate Task, but can create multiple tasks by setting kNumDeflateTasks to a greater value. You should
create one task for each SPU that you want the compression to be performed on in parallel. In this example, because there is
only a single element being put on the queue, creating more than one task wouldn't help, but in the general case having multiple
tasks would allow separate segments to be compressed simultaneously by multiple SPUs. There's no reason to ever want to create
more Deflate Tasks than there are SPUs though.

The counter "s_numElementsToCompress" is initialized to 1.

Next the element is put on the Deflate Queue to request that the uncompressed data be compressed. The Deflate Task will wake up
on an SPU and take this element off the list. It will fetch the uncompressed data from main memory into Local Store, compress
it within Local Store, then send out the compressed data back out to the specified Effective Address.

Once the SPU has finished sending out the compressed data it will then decrement the numElementsToCompress counter in main
memory. Because this decrement takes the counter to zero this means all the work we were waiting on has finished, so it signals
the specified Event Flag.

Meanwhile, the PPU has been sleeping on cellSpursEventFlagWait, waiting for the work to be signalled as done. Since the work is
now considered to the done, the PPU Thread can now write out the compressed data.  Note that the first four bytes of the compressed
output file will be the size of the uncompressed file, so that the subsequent lzo1x-basic-inflate-sample (which uses a copy of this file)
will know how big the uncompressed data was supposed to be.  It is possible for the compressed data to be bigger (by over 6%) than the
uncompressed data in the worst case.  If this happens you could instruct the Deflate Task to store the (smaller) uncompressed data and
return a flag indicating this.  However, the point of this sample is to always create compressed data, even if bigger.  Therefore the
Deflate Task is instructed to always store the compressed data.


<Files>
main.cpp                : Main PPU code
assets/assets.mk        : Makefile that fetches assets from "common/assets" into the sample's local assets folder for use


<Running the Program>
    - Set fileserving root to the lzo1x-basic-deflate-sample directory.
    - Run the .self program.
    - (If you have used lzo1x-basic-deflate-sample to compress different data, then you may wish to copy its data folder to
        lzo1x-simple-inflate-sample/assets if you want to test decompression of that data)
