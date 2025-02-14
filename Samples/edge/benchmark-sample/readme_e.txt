[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This sample is used to benchmark the performance of various Edge Geometry runtime functions.

<Description of Sample Program>

Functionality-wise, this sample is very similar to the Edge character-sample.  This sample features additional timing
functionality in the Edge Geometry job, which times the performance of each function called.  The results are collected
and displayed as a text overlay.  In addition to the raw SPU cycles spent in each function, the results are displayed
as a cycles/<element> count and a percentage of the total Edge Geometry job time.  Data is also display regarding job
manager overhead and SPU ring buffer stalls.

<Files>
main.cpp				: Main PPU code (includes job setup, blend tree setup etc.)
spu/job_anim.cpp			: Animation job
spu/job_anim.mk				: Animation job makefile
spu/job_geom.cpp			: Geometry job
spu/job_geom.mk				: Geometry job makefile
assets/*.dae				: Source Collada files used to build the runtime data
assets.mk				: Makefile that builds runtime data, spawned by the primary makefile
 
<Running the Program>
	- Set fileserving root to the self directory 
	- Run the program
	- Press the X button to cycle through several different skinning flavors.
	- Press the Triangle button to freeze/unfreeze the animation on a single frame.
	- Press the Square button to show/hide the benchmark overlay.

