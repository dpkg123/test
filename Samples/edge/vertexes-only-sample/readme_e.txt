[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2006 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample is an example of running Edge Geometry jobs with data that has not been prepartioned by the offline tools.
Only vertex related processing (no culling) is preformed on the SPUs.  

<Description of Sample Program>

This sample uses Edge Geometry jobs to decompress input vertex data, and compress the data into an output format, operating
on segments generated on the fly by the PPU to fit into local store (as limited by input/output vertex strides).  

<Files>
../common/assets/logo-color.bin		: Binary file containing color texture (raw pixel data)
../common/assets/logo-normal.bin	: Binary file containing normal texture (raw pixel data)
fpshader.cg		: Fragment shader program
main.cpp		: Main program
main.h			: Header file for main program
Makefile		: Makefile
readme_e.txt		: readme text file (English)
readme_j.txt		: readme text file (Japanese)
vertexes-only-sample.sln	: Visual Studio Solution
vertexes-only-sample.vcproj	: Visual Studio Project
vpshader.cg		: Vertex shader program
spu/job_geom.cpp	: Main for Edge Geometry job
spu/job_geom.mk		: Makefile for Edge Geometry job
spu/job_geom.vcproj 	: Edge Geometry SPURS job Visual Studio project
spu/Makefile		: Makefile for all SPU programs

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.  

<Notes>

Vertex only processing with Edge Geometry need not be limited to decompression and compression.  By adding user code, some functionality
of vertex shader programs can be moved onto the SPUs.  
