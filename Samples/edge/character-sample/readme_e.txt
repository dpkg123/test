[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample is an example of using both Edge Animation and Edge Geometry together to display an animated and skinned character.

<Description of Sample Program>

At the start of the frame the animation jobs are created (one per character) and kicked off. The animation job itself is very 
similar to the one in anim-sample, the difference being that instead of outputing 4x4 world matrices, we output the 3x4 skinning 
matrices. These matrices are required later by Edge Geometry and are computed as the product of the 3x4 world matrices and the 
3x4 inverse bind pose matrices (Note: the inverse bind pose array is exported as a separate file by edgeGeomCompiler)

The sample stalls for the animation jobs to complete and then kicks off the geometry jobs using the skinning matrices associated 
with each character.

The code for setting up and running animation and geometry jobs is mostly the same as anim-sample and geom-sample. Please refer to
these samples for more details.

<Files>
main.cpp				    : Main PPU code (includes job setup, blend tree setup etc.)
spu/job_anim.cpp			: Animation job
spu/job_anim.mk				: Animation job makefile
spu/job_geom.cpp			: Geometry job
spu/job_geom.mk				: Geometry job makefile
assets/*.dae				: Source Collada files used to build the runtime data
assets.mk					: Makefile that builds runtime data, spawned by the primary makefile
 
<Running the Program>
	- Set fileserving root to the self directory 
	- Run the program
