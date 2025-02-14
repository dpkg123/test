[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2009 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample demonstrates image post-processing on SPU.


<Description of Sample Program>

The scene is rendered to a multiple render target, which is organized as follows:
mrt0: colors
mrt1: motion vectors + linear depth

Before the flip happens the current render targets are copied from local memory to XDR memory and
the previous post-processing output is copied from XDR to localmemory for display.
Then SPU post-processing is kicked on SPU.

Post-processing on SPU is done in the following order:
1) Morphological Anti-ALiasing
2) depth of field
3) motion blur
4) bloom
5) internal lens reflection


<Files>
main.cpp                          : Main PPU code
main.h                            : Main class definition
vpsky.cg                          : Sky vertex program
fpsky.cg                          : Sky fragment program
vpshader.cg                       : Scene vertex program
fpshader.cg                       : Scene fragment program
shaders/post_bloomcapturefp.cg    : Bloom fragment program
shaders/post_bloomcompositefp.cg  : Bloom fragment program
shaders/post_copyfp.cg            : Resample fragment program
shaders/post_depthdownsamplefp.cg : Depth downsample fragment program
shaders/post_doffp.cg             : Depth of Field fragment program
shaders/post_farfuzzinessfp.cg    : Far fuzziness fragment program
shaders/post_nearfuzzinessfp.cg   : Near fuzziness fragment program
shaders/post_gauss1x7fp.cg        : Vertical gauss fragment program
shaders/post_gauss7x1fp.cg        : horizontal gauss fragment program
shaders/post_ilrfp.cg             : internal lens reflection fragment program
shaders/post_motionblurfp.cg      : motion blur fragment program
shaders/post_postvp.cg            : postprocessing generic vertex program
spu/job_geom_interface.h          : Shared code between PPU and SPU
spu/edgepostjobheader.h           : Shared code between PPU and SPU
spu/postParams.h                  : Shared code between PPU and SPU
spu/jobgeom.cpp                   : EdgeGeom job
spu/bloomcapture.cpp              : Bloom capture stage job
spu/dof.cpp                       : Depth of field stage job
spu/downsample.cpp                : Downsample stage job
spu/fuzziness.cpp                 : Fuzziness calculation stage job
spu/horizontalgauss.cpp           : Horizontal gauss effect job
spu/verticalgauss.cpp             : Vertical gauss effect job
spu/motionblur.cpp                : Motionblur stage job
spu/rop.cpp                       : Rops stage job
spu/upsample.cpp                  : Upsample stage job
spu/ilr.cpp                       : Internal Lens Reflection code

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.
	- The circle button toggle between SPU/GPU/NONE post-processing
	- The square button toggle animation
	- The start button enable/disable the on screen text
	- Right/Left buttons cycle through tweakables
	- Triangle/Cross buttons alter current tweakable
	- L1 toggles MLAA, R1 toggles edge visualization
