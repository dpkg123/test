[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample shows how to detect and retry a decompression if there is a data error. It loads a compressed file and passes the
compressed data in to Edge LZMA to be decompressed by an SPU. However, before doing that it simulates a file read error by
corrupting a byte in the compressed data.  This corrupt data will cause the SPU to report back that there was a data error. The
PPU will reload the data and repeat the decompression until a succesful decompression is done. It then checks that the output
uncompressed data agrees with the master file which was previously used to create the compressed version offline.

Note that all output of this program is over TTY and there is nothing onscreen.


<Description of Sample Program>

The program loads the compressed file ("assets/bs_cube.dae.lzma") and parses it to extract the components of the file format
ready for submission to Edge LZMA.

The compressed data read in is intentionally damaged so that the SPU will report back an error.

The counter "s_numElementsToDecompress" is initialized to 1.

The element is put on the Inflate Queue to request that the compressed data be decompressed. The Inflate Task will wake up on an
SPU and take this element off the list. It will fetch the compressed data from main memory into Local Store, decompress it
within Local Store, then send out the uncompressed data back out to the specified Effective Address.

Once the SPU has finished sending out the uncompressed data it will then decrement the numElementsToDecompress counter in main
memory. If there was an error during decompression, the SPU will set the high bit of numElementsToDecompress (which is normally
zero). Because this decrement takes (the low 31 bits of) the counter to zero this means all the work we were waiting for has
finished, so it signals the specified Event Flag.

Meanwhile, the PPU has been sleeping on cellSpursEventFlagWait, waiting for the work to be signalled as done. Since the work is
now considered to the done, the PPU Thread can now wake up again. It checks the high bit of numElementsToDecompress. If it is
set then there was an error in the compressed data, and it will try again. After 5 forced failed times, we simulate a successful
disc read and leave the data unmodified so that it can be decompressed successfully. When the PPU sees the successful
decompression (because the high bit of the counter didn't get set) it will shut down everything and verify that decompression
gave the correct results.


<Files>
main.cpp                : Main PPU code
assets/assets.mk        : Makefile that fetches assets from "common/assets" into the sample's local assets folder for use



<Running the Program>
    - Set fileserving root to the lzma-inflate-retry-sample directory.
    - Run the .self program.
