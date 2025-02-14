[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2006 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.


<Point of Sample Program>
  This sample builds on the concepts in the Edge geom-sample to demonstrate
the use of the occluders using the animated elephant.

<Description of Sample Program>
  This program.

<Files>
../common/assets/elephant.dae			: Source geometry scene data
../common/assets/elephant-color.bin		: Binary file containing color texture (raw pixel data)
../common/assets/elephant-matrices.bin	: Binary file containing skinning matrices 
../common/assets/elephant-normal.bin	: Binary file containing normal texture (raw pixel data)
assets.mk	   : Makefile for building scene data
elephant-sample.sln	: Visual Studio Solution
elephant-sample.vcproj	: Visual Studio Project
fpdebug.cg         : Fragment shader program
fpshader.cg        : Fragment shader program
main.cpp           : Main sample source
main.h             : Main sample header
Makefile           : Makefile
readme_e.txt       : readme text file (English)
readme_j.txt       : readme text file (Japanese)
vpdebug.cg         : Vertex shader program
vpshader.cg        : Vertex shader program
spu/job_geom.cpp    : Edge Geometry SPURS job source
spu/job_geom.mk     : Edge Geometry SPURS job makefile
spu/job_geom.vcproj : Edge Geometry SPURS job Visual Studio project
spu/Makefile		: Makefile for all SPU programs

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.  
	- The start button display a menu which allow the user to:
	    . enable/disable the occluders
	    . render the scene in wireframe
	    . make the occluders opaque or transparent
	    . pause the animation

<Notes>
