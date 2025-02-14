[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample demonstrates how to use RSX reports to conditionally kill Edge Geometry jobs whose results will not affect the final image.
There are two rendering passes.  The first pass draws a rectangular occluding plane (shown in wireframe if conditional rendering is not
enabled) and then, with depth and color writes off, renders a precomputed bounding box for each of the elephants.  The second pass draws
all of the elephants, using Edge Geometry processing.  When conditional job execution is used, the report index corresponding to the pixel
count reports for the appropriate bounding box in the first pass is provided as input to the Edge Geometry SPU job.  The Edge job reads
the report value at the start of the job.  If the report has been written and no pixels were written, the Edge job fills the command buffer
hole and exits without processing.  If the report has not yet been written or pixels were written, processing continues normally.  If the
report had not been written at the start of the job, the job rereads the report value before allocating output space, spinning until the
report has been written.  If, when written, the report indicates that no pixels were written, the job will clear the command buffer hole
and exit, otherwise the job will complete as normal.

The sample demonstrates how to implement a conditional job execution scheme which can save on SPU and RSX processing, by killing SPU jobs
and avoiding adding ineffectual commands to the command buffer.

<Description of Sample Program>

This program 

<Files>
../common/assets/elephant.dae			: Source geometry scene data
../common/assets/elephant-color.bin		: Binary file containing color texture (raw pixel data)
../common/assets/elephant-matrices.bin	: Binary file containing skinning matrices 
../common/assets/elephant-normal.bin	: Binary file containing normal texture (raw pixel data)
assets.mk		        : Makefile for building scene data
fpshader.cg			: Fragment shader program
fpshadersimple.cg		: A simple fragment shader program used in the prepass
conditional-sample.sln	        : Visual Studio Solution
conditional-sample.vcproj	: Visual Studio Project
main.cpp		        : Main program
main.h			        : Header file for main program
Makefile		        : Makefile
readme_e.txt		        : readme text file (English)
readme_j.txt		        : readme text file (Japanese)
vpshader.cg			: Vertex shader program
vpshader.cg			: A simple vertex shader program used in the prepass
spu/job_geom.cpp	: Main for Edge Geometry job, with additional code for reading RSX report data and conditionally early-exiting from job
spu/job_geom.mk		: Makefile for Edge Geometry job
spu/job_geom.vcproj 	: Edge Geometry SPURS job Visual Studio project
spu/Makefile		: Makefile for all SPU programs

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.
        - Press and hold triangle to enable conditional job execution with reports in main memory.
        - Press and hold square to enable conditional job execution with reports in video memory.

<Notes>
The sample is set to run in HSYNC.  This is not a requirement or limitation of the technique.  
For a handful of reasons, largely related to the restrictions of the sample framework and the 
associated positions of the SPU and RSX kicks, the timings displayed by the profile bars are 
unintuitive when the sample is run in VSYNC.  Running in HSYNC lets us bypass these framework
limitations and display performance bars that depict intuitive results.