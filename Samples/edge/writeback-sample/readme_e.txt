[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2006 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.


<Point of Sample Program>

This sample is an example of writing data computed by an Edge Geometry job back to main memory, for use in subsequent frames.  It also demonstrates
custom vertex decompression and compression via user-provided callbacks.  It is presented in the form of a particle system, with the particles 
simulated on the SPU.

<Description of Sample Program>

This sample program runs a particle system, with the particles simulated on the SPU.  Like the vertexes-only sample, segments are broken up
on the fly on the PPU according to input/output strides.  Also like the vertexes-only sample, the draw calls are placed in the command buffer
on the PPU, rather than on the SPU (as they are in the elephant- and geom- samples).  The PPU creates two separate job chains each frame.  The 
first chain uses a custom callback to decompresses the particles' positions and velocities into the uniform tables, then timesteps the particles 
(using a simple forward Euler integrator and gravity, with some obstacles in the particles' path), compresses the particles using a custom callback,
and outputs the compressed particle data back to main memory.  The second job chain (using the same SPU program, but with a different flag passed in
as user data) decompresses the particles with the same custom callback, and then compresses the particle positions as vertex data (using one of the
provided output flavors), which is output for rendering.

<Files>
fpshader.cg		: Fragment shader program
main.cpp		: Main program
main.h			: Header file for main program
Makefile		: Makefile
readme_e.txt		: readme text file (English)
readme_j.txt		: readme text file (Japanese)
vpshader.cg		: Vertex shader program
writeback-sample.sln	: Visual Studio Solution
writeback-sample.vcproj	: Visual Studio Project
spu/job_geom.cpp	: Main for Edge Geometry job
spu/job_geom.mk		: Makefile for Edge Geometry job
spu/job_geom.vcproj 	: Edge Geometry SPURS job Visual Studio project
spu/Makefile		: Makefile for all SPU programs
spu/simulate.cpp	: Code for computing particle derivatives and integrating particles
spu/simulate.h		: Header for simulator

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.  
	- SQUARE pauses/unpauses the simulation.

<Notes>
