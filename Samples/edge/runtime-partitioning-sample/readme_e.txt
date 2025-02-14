[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2006 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.


Procedurally generates some simple geometry , which is then partitioned at runtime
on the PPU, and rendered using Edge Geometry on the SPUs.

<Point of Sample Program>
  This sample builds on the concepts in the Edge vertexes-only-sample to demonstrate
  the integration of the traditionally offline libedgegeomtool partitioner into PPU
  runtime code.  This technique is *not* recommended for final release builds, but is
  suitable for quick internal prototypes, or to facilitate runtime hot-swapping of
  geometry assets during development.

<Description of Sample Program>
  The output of the program should be a simple textured sphere.  The sphere is animated
  using both skinning (to rotate the sphere back and forth along the Z axis) and blend
  shapes (to grow and contract the sphere along the Y axis).

<Files>
assets.mk	   : Makefile for building scene data
fpshader.cg        : Fragment shader program
main.cpp           : Main sample source
main.h             : Main sample header
Makefile           : Makefile
readme_e.txt       : readme text file (English)
readme_j.txt       : readme text file (Japanese)
runtime-partitioning-sample.sln	: Visual Studio Solution
runtime-partitioning-sample.vcproj	: Visual Studio Project
vpshader.cg        : Vertex shader program
spu/job_geom.cpp    : Edge Geometry SPURS job source
spu/job_geom.mk     : Edge Geometry SPURS job makefile
spu/job_geom.vcproj : Edge Geometry SPURS job Visual Studio project
spu/Makefile		: Makefile for all SPU programs

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.  

<Notes>
