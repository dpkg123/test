[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2009 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample demonstrates image post-processing on SPU performed with HDR values.


<Description of Sample Program>

The scene is rendered to a multiple render target, which is organized as follows:
mrt0: colors (as FP16 luminance, u8n U,V)
mrt1: motion vectors + linear depth

Before the flip happens the current render targets are copied from local memory to XDR memory and
the previous post-processing output is copied from XDR to localmemory for display.
Then SPU post-processing is kicked on SPU.

Post-processing on SPU is done in the following order:
1) depth of field
2) motion blur
3) Average luminance calculation
4) bloom
5) internal lens reflection


<Files>
main.cpp                          : Main PPU code
main.h                            : Main class definition
shaders/vpsky.cg                  : Sky vertex program
shaders/fpsky.cg                  : Sky fragment program
shaders/vpshader.cg               : Scene vertex program
shaders/fpshader.cg               : Scene fragment program
shaders/downsample_luvfp.cg       : Color downsample fragment program
shaders/post_depthdownsamplefp.cg : Depth downsample fragment program
shaders/tonemap_compositefp.cg    : Tonemapping and composition fragment program
shaders/postvp.cg                 : postprocessing generic vertex program
spu/job_geom_interface.h          : Shared code between PPU and SPU
spu/edgepostjobheader.h           : Shared code between PPU and SPU
spu/postParams.h                  : Shared code between PPU and SPU
spu/jobgeom.cpp                   : EdgeGeom job
spu/avgluminance.cpp              : Average luminance stage job
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
spu/finalcombine.cpp              : Final stage

<Running the Program>
	- The left and right analog sticks, and the dpad up/down move the camera.
	- The square button toggle animation
	- Right/Left buttons cycle through tweakables
	- Triangle/Cross buttons alter current tweakable
