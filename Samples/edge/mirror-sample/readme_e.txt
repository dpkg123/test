[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample demonstrates animation mirroring. For each character, the animation is evaluated at a constant time and blending occurs between 
that frame and its mirror across the x=0 plane.

<Description of Sample Program>

At initialisation the mirroring specification is constructed. This consists of an array of structures describing the mirror operation for each 
joint. 

The blend tree has two leaves which both evaluate to the same point in the animation. For the second leaf, the flag EDGE_ANIM_FLAG_MIRROR is
specified to enable mirroring for this leaf. The two leaves get blended via a simple sine function.

At job creation time the mirroring specification gets uploaded to the spurs cached buffer. Within the spu job itself it then gets passed to 
edgeAnimProcessBlendTree()

Note: it's also possible to mirror the result of a blend by specifying the EDGE_ANIM_FLAG_MIRROR flag when creating a branch structure. This 
is not demonstrated in this sample.

<Files>
main.cpp					: Main PPU code (includes job setup, blend tree setup etc.)
spu/process_blend_tree.cpp	: Main animation processing function (split to be called from either the SPU job or the PC/win32 version)
spu/job_anim.cpp			: Animation job main()
spu/job_anim.mk				: Animation job makefile
assets/*.dae				: Source Collada files used to build the runtime data
assets.mk					: Makefile that builds runtime data, spawned by the primary makefile
 
<Running the Program>
    - Set fileserving root to the self directory 
    - Run the program
