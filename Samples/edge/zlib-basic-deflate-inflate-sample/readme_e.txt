[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample shows usage of Edge Zlib for run-time compression. It loads a file and passes this uncompressed data in to Edge
Zlib to be compressed by an SPU. This compressed data is then passed back through Edge Zlib a second time to be decompressed
again and is verified against the original master data.

Note that all output of this program is over TTY and there is nothing onscreen.


<Description of Sample Program>

The program loads the compressed file "assets/elephant-normal.bin".

The program creates a SPURS Taskset and a Deflate Queue, then creates a Deflate Task which it attaches to that Taskset. The
Deflate Queue will have compression work put onto it, and the Deflate Task(s) which run on SPU(s) will pull from it and process
the work.

The sample only creates 1 Deflate Task, but can create multiple tasks by setting kNumDeflateTasks to a greater value. You should
create one task for each SPU that you want the compression to be performed on in parallel. In this example, because there is
only a single element being put on the queue, creating more than one task wouldn't help, but in the general case having multiple
tasks would allow separate segments to be compressed simultaneously by multiple SPUs. There's no reason to ever want to create
more Deflate Tasks than there are SPUs though.

The counter "s_numElementsToCompress" is initialized to 1.

Next the element is put on the Deflate Queue to request that the data be compressed. The Deflate Task will wake up on an SPU and
take this element off the list. It will fetch the data from main memory into Local Store, compress it within Local Store, then
send out the compressed data back out to the specified Effective Address. It will also store out the size of the compressed data
into a chosen uint32_t in main memory.

Note that applying compression can in some cases cause the data to grow larger than the original master data. In this case there
is a choice between whether the Deflate Task should store the compressed data anyway (even if it is bigger), or whether it should
just store back out the original master data. If the original data was smaller than the compressed data, and was chosen for
storing, then this fact is communicated by the fact that the most-signifcant bit of the output size is set.

In this example, we choose to have the Deflate Task always store the compressed data (kEdgeZlibDeflateTask_DeflateStoreCompressed),
even if it is larger than the master data.

Once the SPU has finished sending out the compressed data it will then decrement the numElementsToCompress counter in main
memory. Because this decrement takes the counter to zero this means all the work we were waiting on has finished, so it signals
the specified Event Flag.

Meanwhile, the PPU has been sleeping on cellSpursEventFlagWait, waiting for the work to be signalled as done. Since the work is
now considered to the done, the PPU Thread can now wake up again. The PPU will shut down the Deflate Tasks and Defalte Queue.
This is the end of the compression side of the sample.

Finally, the sample then feed thiss compressed data back into Edge Zlib a second time in order to decompress it again and verify
that it returns the original master data again.


<Files>
main.cpp                : Main PPU code
gz.cpp                  : Functions for interfacing with a ".gz" file
gz.h                    : Functions for interfacing with a ".gz" file
assets/assets.mk        : Makefile that fetches assets from "common/assets" into the sample's local assets folder for use


<Running the Program>
    - Set fileserving root to the zlib-basic-deflate-inflate-sample directory.
    - Run the .self program.
