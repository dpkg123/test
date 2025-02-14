[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2006 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample is an example of adding user code to an Edge Geometry job, in the form of a Ray-Triangle
collision test.  

<Description of Sample Program>

This sample creates two types of EdgeGeom jobs.  The first is similar to the jobs in
the geom- and elephant- samples, decompressing data, skinning and culling, compressing output, and filling
the command buffer hole.  The second job is specific to the intersection tests; it decompresses data and
skins the geometry, then transforms the vertices into the ray's local space for the intersection tests.  
Once a segment's nearest intersection has been computed, the job uses atomic operations to read the current 
globally nearest intersection point from main memory, and if appropriate, write the current segment's 
nearest collision point back to main memory.  A line will be drawn along the normal at the nearest 
intersection point.  A '+' is displayed in the center of the screen to indicate where the ray is being
shot from.  The position is hardcoded for a 720p resolution.  

<Files>
../common/assets/elephant.dae			: Source geometry scene data
../common/assets/elephant-color.bin		: Binary file containing color texture (raw pixel data)
../common/assets/elephant-matrices.bin	: Binary file containing skinning matrices 
../common/assets/elephant-normal.bin	: Binary file containing normal texture (raw pixel data)
assets/assets.mk		: Makefile for building scene data
collision-sample.sln	: Visual Studio Solution
collision-sample.vcproj	: Visual Sutdio Project
fpshader.cg			: Fragment shader program
main.cpp			: Main program
main.h			: Header file for main program
Makefile			: Makefile
readme_e.txt		: readme text file (English)
readme_j.txt		: readme text file (Japanese)
vpshader.cg			: Vertex shader program
spu/intersect.cpp		: Intersection tests file
spu/intersect.h		: Header for intersection tests
spu/job_geom.cpp	: Main for Edge Geometry job
spu/job_geom.mk		: Makefile for Edge Geometry job
spu/job_geom.vcproj	: Edge Geometry SPURS job Visual Studio project
spu/job_intersect.cpp	: Main for user code Edge Geometry intersection job
spu/job_intersect.mk	: Makefile for user code Edge Geometry intersection jobs
spu/job_intersect.vcproj: Intersection test SPURS job Visual Studio Project
spu/Makefile		: Makefile for all SPU programs

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.  
	- The ray used in intersection tests is the camera look-at vector.

<Notes>
