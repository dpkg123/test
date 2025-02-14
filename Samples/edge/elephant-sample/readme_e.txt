[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2006 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.


Renders a non-trivial animated character using Edge Geometry.


<Point of Sample Program>
  This sample builds on the concepts in the Edge geom-sample to load and
  render a large, multi-segment animated elephant.

<Description of Sample Program>
  For simplicity, the elephant's animation matrices are loaded from
  a flat precomputed array.

<Files>
../common/assets/elephant.dae			: Source geometry scene data
../common/assets/elephant-color.bin		: Binary file containing color texture (raw pixel data)
../common/assets/elephant-matrices.bin	: Binary file containing skinning matrices 
../common/assets/elephant-normal.bin	: Binary file containing normal texture (raw pixel data)
assets.mk	   : Makefile for building scene data
elephant-sample.sln	: Visual Studio Solution
elephant-sample.vcproj	: Visual Studio Project
fpshader.cg        : Fragment shader program
main.cpp           : Main sample source
main.h             : Main sample header
Makefile           : Makefile
readme_e.txt       : readme text file (English)
readme_j.txt       : readme text file (Japanese)
vpshader.cg        : Vertex shader program
spu/job_geom.cpp    : Edge Geometry SPURS job source
spu/job_geom.mk     : Edge Geometry SPURS job makefile
spu/job_geom.vcproj : Edge Geometry SPURS job Visual Studio project
spu/Makefile		: Makefile for all SPU programs

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.  

<Notes>
