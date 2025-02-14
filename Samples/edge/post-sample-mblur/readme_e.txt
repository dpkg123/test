[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2009 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample allows to compare fullscreen vs quarter-resolution motion blur.


<Description of Sample Program>

The scene is rendered to a multiple render target, which is organized as follows:
mrt0: colors
mrt1: motion vectors

Before the flip happens the current render targets are copied from local memory to XDR memory and
the previous post-processing output is copied from XDR to local memory for display.
Then SPU post-processing is kicked on SPU.


<Files>
main.cpp                          : Main PPU code
main.h                            : Main class definition
vpsky.cg                          : Sky vertex program
fpsky.cg                          : Sky fragment program
vpshader.cg                       : Scene vertex program
fpshader.cg                       : Scene fragment program
postvp.cg                         : Resample vertex program
postfp.cg                         : Resample fragment program
spu/job_geom_interface.h          : Shared code between PPU and SPU
spu/edgepostjobheader.h           : Shared code between PPU and SPU
spu/jobgeom.cpp                   : EdgeGeom job
spu/motionblur.cpp                : Motionblur stage job

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.
	- The square button toggle animation
	- The triangle button toggle fullscreen vs qurter resolution motionblur
	- The start button enable/disable the on screen text
