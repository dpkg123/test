[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample is an example of using both Edge Animation and Edge Geometry together to display a model with animated blendshapes.
It shows how user (scalar) channels can be exported by edgeAnimCompiler and evaluated using Edge Animation.

<Description of Sample Program>

The sample renders a simple mesh containing two blendshapes. The blendshape factors are animated and treated as user channels 
by Edge Animation. To export these channels, a text file containing their names (in the form node.targetshape) is passed to 
edgeAnimCompiler at skeleton export time. 

User channels are evaluated in the call to edgeAnimProcessBlendTree(). The animation job copies the evaluated channels to
the output buffer where they get dma'd to the array of blendshape factors in main memory.

The code for setting up and running animation and geometry jobs is mostly the same as anim-sample and geom-sample. Please refer to
these samples for more details.

Note: The mesh used in this sample is not skinned, yet the sample contains code to evaluate joints and skinning matrices. This is 
temporary and likely to change.

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
