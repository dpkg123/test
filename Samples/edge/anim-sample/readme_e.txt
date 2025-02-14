[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<Point of Sample Program>

This samples shows a very simple use of Edge Animation to render an animated wireframe skeleton. The PPU code builds one blend-tree
per character and shows typical SPURS job setup. The SPU code processes the blend tree and then converts the result into world-space
matrices before writing them back to main memory.

<Description of Sample Program>

Below is a more detailed description of the main functions of interest.

SampleApp::onInit()

	Initialises spurs.
	Initialises Edge Animation. 128k of external pose cache memory is allocated per spu. Note that the blend tree used in this sample 
	is very simple and doesn't require an external pose cache, so this is really for demonstration purposes only.
	The skeleton and four animation binaries are loaded (2 idles, a walk, and a jog)
	Each character is allocated a world matrix array. These arrays will get updated each frame by the SPU jobs.

SampleApp::onUpdate()

	The evaluation time and final blend factor are advanced. The characters will blend over time from idle to walk/jog and back again. 
	Note that the other two blend factors used in the blend tree (between the two idles, and between walk/jog) are constant per character.

SampleApp::runAnimJobs()

	For each character, calls createBlendTree() and createAnimJob(). The evaluation time and blend factors are jittered to give some 
	variation per character.
    The job are kicked and the sample stalls for the results.
    
SampleApp::createTestBlendTree()

    Creates a simple blend tree that blends between the four animations using the passed in blend factors. Note that the walk and jog 
    cycles have different durations and would normally be synchronised during blending - we haven't done so here for simplicity.

SampleApp::createAnimJob()

	Creates a single SPU job that evaluates a blend tree and outputs an array of world matrices. The spurs job is set up as follows:

    Input buffer: Blend tree branch/leaf arrays
    Cached buffer: Skeleton and global context (these are shared by all characters)
    Output buffer: The evaluated world matrix array
    Scratch buffer: 128k is reserved for use by Edge Animation
	Parameters: Root joint, blend branch count, blend leaf count, effective address of output matrix array

cellSpursJobMain2()

    The entry point of the job. The job parameters and pointers to all the buffers are retrieved.
    processBlendTree() is called to do the processing and the final world matrices are written back to main memory.

processBlendTree()

	edgeAnimSpuInitialize() is called to initialize the Edge Animation SPU library.
	edgeAnimProcessBlendTree() is called to process the blend tree. The root of the blend tree is a branch, so 
	EDGE_ANIM_BLEND_TREE_INDEX_BRANCH is passed as the root flag.
	At this stage, the evaluated local space joints exist at the top of the pose stack. 
	An entry is pushed onto the pose stack for the world space joints and edgeAnimLocalJointsToWorldJoints() called to do the 
	conversion.
	edgeAnimJointsToMatrices4x4() is called to convert the world joints to matrices (directly into the output buffer)
	Finally, the pose stack is reset and the Edge Animation SPU library is finalized.


<Files>
main.cpp				    : Main PPU code (includes job setup, blend tree setup etc.)
spu/process_blend_tree.cpp	: Main animation processing function (split to be called from either the SPU job or the PC/win32 version)
spu/job_anim.cpp			: Animation job main()
spu/job_anim.mk				: Animation job makefile
assets/*.dae				: Source Collada files used to build the runtime data
assets.mk					: Makefile that builds runtime data, spawned by the primary makefile
 
<Running the Program>
    - Set fileserving root to the self directory 
    - Run the program
