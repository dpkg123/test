[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2009 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample demonstrates average luminance calculation and bloom effect image post-processing on SPU.
This sample also shows how to optimize an Edge Post effect chain by:
- minimizing DMA usage by coalescing multiple passes together
- choosing appropiate intermediate pixel format to avoid redundant conversions

<Description of Sample Program>

Before the flip happens the current render target is copied from local memory to XDR memory and
the previous post-processing output is copied from XDR to localmemory for display.
Then SPU post-processing is kicked on SPU.

This sample only implements the Bloom effect and average luminance calculation.


<Files>
main.cpp                          : Main PPU code
spupostprocessing.cpp             : Additional PPU code
gpupostprocessing.cpp             : Additional PPU code
main.h                            : Main class definition
shaders/vpsky.cg                  : Sky vertex program
shaders/fpsky.cg                  : Sky fragment program
shaders/vpshader.cg               : Scene vertex program
shaders/fpshader.cg               : Scene fragment program
shaders/post_postvp.cg            : postprocessing generic vertex program
shaders/downsample_luvfp.cg       : Luv downsample fragment shader
shaders/tonemap_compositefp.cg    : Final composition + tonemapping shader
spu/job_geom_interface.h          : Shared code between PPU and SPU
spu/edgepostjobheader.h           : Shared code between PPU and SPU
spu/postParams.h                  : Shared code between PPU and SPU
spu/jobgeom.cpp                   : EdgeGeom job
spu/bloom.cpp			          : Bloom effect
spu/posttask.cpp			      : Edge Post task

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.
	- The square button toggle animation
	- Right/Left buttons cycle through tweakables
	- Triangle/Cross buttons alter current tweakable
