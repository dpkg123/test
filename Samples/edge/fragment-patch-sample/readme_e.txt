[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample demonstrates how to patch fragment program constants on the SPU, through the use of user code added to an Edge Geometry job.

<Description of Sample Program>

This program patches fragment program constants for each object (of which there are 128 -elephants-) in the scene.  Since each elephant
contains multiple prepartitioned segments (and thus, corresponds to multiple Edge Geometry jobs), the patch is applied during the job
corresponding to the first segment of each object.  On the SPU side, the Edge Geometry job has been modified to DMA a structure containing
information for the patch, the full fragment program micro code, and the patch values from main memory to SPU local store, to copy the patch
values into the appropriate offsets in the microcode, to DMA the resulting ucode to free space in video memory (allocated from a hybrid buffer),
and to add the appropriate commands to update the parameter to the command buffer via cellGcmSetUpdateFragmentProgramParameter().  On the PPU
side, the sizes of the command buffer holes precomputed in the offline tools are increased to hold the additional command (although, in a 
production situation, this would be best handled by the tools themselves).  

<Files>
../common/assets/elephant.dae			: Source geometry scene data
../common/assets/elephant-color.bin		: Binary file containing color texture (raw pixel data)
../common/assets/elephant-matrices.bin	: Binary file containing skinning matrices 
../common/assets/elephant-normal.bin	: Binary file containing normal texture (raw pixel data)
assets.mk		: Makefile for building scene data
fpshader.cg			: Fragment shader program to be patched
fragment-patch-sample.sln	: Visual Studio Solution
fragment-patch-sample.vcproj	: Visual Studio Project
main.cpp		: Main program
main.h			: Header file for main program
Makefile		: Makefile
readme_e.txt		: readme text file (English)
readme_j.txt		: readme text file (Japanese)
vpshader.cg			: Vertex shader program
spu/job_geom.cpp	: Main for Edge Geometry job, with additional code for patching fragment program
spu/job_geom.mk		: Makefile for Edge Geometry job
spu/job_geom.vcproj 	: Edge Geometry SPURS job Visual Studio project
spu/Makefile		: Makefile for all SPU programs

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.

<Notes>
