[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample demonstrates the effects of varying the number of SPUs, the SPURS Max Grab, and the total ring buffer size
on performance.  Users can build intuition about the effects that modifying parameters have on RSX and SPU processing
times.

<Description of Sample Program>

This program 

<Files>
../common/assets/elephant.dae	        : Source geometry scene data
assets.mk		        : Makefile for building scene data
elephant-color.bin	        : Binary file containing color texture
elephant-matrices.bin	        : Binary file containing skinning matrices 
elephant-normal.bin	        : Binary file containing normal texture
fpshader.cg			: Fragment shader program
tunable-sample.sln	        : Visual Studio Solution
tunable-sample.vcproj	        : Visual Studio Project
main.cpp		        : Main program
main.h			        : Header file for main program
Makefile		        : Makefile
readme_e.txt		        : readme text file (English)
readme_j.txt		        : readme text file (Japanese)
vpshader.cg			: Vertex shader program
spu/job_geom.cpp	: Main for Edge Geometry job
spu/job_geom.mk		: Makefile for Edge Geometry job
spu/job_geom.vcproj 	: Edge Geometry SPURS job Visual Studio project
spu/Makefile		: Makefile for all SPU programs

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.
        - Press cross to enable/disable menu.  (Active menu is indicated by ">" icon.  Camera control is disabled while menu is active.)
        - When menu is active, dpad up and down change selection, left and right change parameter.

<Notes>
