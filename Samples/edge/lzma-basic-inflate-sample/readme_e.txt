[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample shows a very simple use of Edge LZMA. It loads a compressed file and passes the compressed data in to Edge LZMA to be
decompressed by an SPU. It then checks that the output uncompressed data agrees with the master file which was previously used to
create the compressed version offline.

Note that all output of this program is over TTY and there is nothing onscreen.

The compressed data and the uncompressed data used in this sample must each be not more than 64K.


<Description of Sample Program>

The master file ("assets/bs_cube.dae") was compressed with lzma.exe to create the compressed file ("assets/bs_cube.dae.lzma").
The sample loads this file and parses it to extract expected uncompressed size, the LZMA properties, and the raw data.

The program creates a SPURS Taskset and an Inflate Queue, then creates an Inflate Task which it attaches to that Taskset. The
Inflate Queue will have decompression work put onto it, and the Inflate Task(s) which run on SPU(s) will pull from it and
process the work.

The sample only creates 1 Inflate Task, but can create multiple tasks by setting kNumInflateTasks to a greater value. You should
create one task for each SPU that you want the decompression to be performed on in parallel. In this example, because there is
only a single element being put on the queue, creating more than one task wouldn't help, but in the general case having multiple
tasks would allow separate segments to be decompressed simultaneously by multiple SPUs. There's no reason to ever want to create
more Inflate Tasks than there are SPUs though.

The counter "s_numElementsToDecompress" is initialized to 1.

Next the element is put on the Inflate Queue to request that the compressed data be decompressed. The Inflate Task will wake up
on an SPU and take this element off the list. It will fetch the compressed data from main memory into Local Store, decompress
it within Local Store, then send out the uncompressed data back out to the specified Effective Address.

Once the SPU has finished sending out the uncompressed data it will then decrement the numElementsToDecompress counter in main
memory. Because this decrement takes the counter to zero this means all the work we were waiting on has finished, so it signals
the specified Event Flag.

Meanwhile, the PPU has been sleeping on cellSpursEventFlagWait, waiting for the work to be signalled as done. Since the work is
now considered to the done, the PPU Thread can now wake up again and it will shut down everything and verify that decompression
gave the correct results.


<Files>
main.cpp                : Main PPU code
assets/assets.mk        : Makefile that fetches assets from "common/assets" into the sample's local assets folder for use


<Running the Program>
    - Set fileserving root to the lzma-basic-inflate-sample directory.
    - Run the .self program.
