[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

Animates a number of locomoting characters using the Edge Animation Library.

<Point of Sample Program>

This sample is an extension of anim-sample and shows how "locomotion processing" (root translation extraction) can be performed with only 
user-space code and no modification to the Edge Animation code. Because of looping animations, a context must be maintained for each
animation instance in the blend tree. It is meant to be used both as a reference implementation of locomotion processing, and an example 
of more advanced use of edgeAnimProcessBlendTree() callbacks.

<Description of Sample Program>

For a locomoting animation, we require that the *delta* of the root joint be used during blending. That is, the difference between this 
frame's value and the last, taking into account any looping that may have occured. We achieve this through leaf callbacks, which allow us to 
modify the root joint of the animation after evaluation, but before blending has occured. We also need some additional information to 
compute the delta, specifically the previous root value and a count of how many times the animation has looped since the last evaluation. 
This state is stored for each instance of the animation, and is uploaded by the callback. Since it gets called at each pipeline stage, the 
callback can prefetch this data in order to minimise stalls.

When blend tree processing is complete, the final blended root joint delta is used to update the world position of the character. 

Below is a more detailed description of the main functions of interest. (Please refer to anim-sample for the parts of the code not
documented here)

SampleApp::onInit()

    Loads a skeleton and two animations (walk & jog) and computes the root end delta of each animation - this value is used by the callback
    function to take account of looping when computing the per-frame deltas. 
    A grid of characters is initialised, each with it's own per-animation locomotion state. 

SampleApp::onUpdate()

    Each character is rotated slightly around the y-axis. Combined with the motion driven by the animation, this results in each character
    following a circular path. 
    Each character's animation time is advanced, and the loop count set to the number of loops that occur in the animation since the last
    update. 

SampleApp::runAnimJobs()

    For each character, constructs a simple blend tree and calls createAnimJob() to create the animation job. The 'userVal' member in 
    EdgeAnimBlendLeaf is set to the address of the locomotion state. This value will get passed to the leaf callback, allowing us to 
    access the state data.

SampleApp::createAnimJob()

    Mostly unchanged from anim-sample. The job now updates the root joint, so some space is allocated for it in the output buffer, 
    and its effective address is passed as an additional parameter.

cellSpursJobMain2()

    The entry point of the job. Mostly unchanged from anim-sample. processBlendTree() now updates the root joint which the job will  
    then dma out.

processBlendTree()

    The leaf callback is setup. The size of the user callback buffer is specified in the call to edgeAnimSpuInitialize(). This is the
    buffer we use to dma in the locomotion state, so it is set to the size of the LocomotionState structure. The actual callback function, 
    LeafCallback(), is specified in the call to edgeAnimProcessBlendTree(). After blend tree processing, the local space joints are 
    converted to world space matrices as usual. Note that the we can get the updated root joint transform from the first entry in the 
    world joint array.

LeafCallback()

    This is where the deltas are computed for each locomoting animation in the blend tree.
    At pipeline stage -1, the locomotion state is prefetched into the user buffer.
    At pipeline stage 0, the delta is calculated as follows and will overwrite the root joint in the current pose:

            root delta = (current evaluated root - last evaluated root) + loop count * root end delta

    Finally the locomotion state is updated and dma'd back to main memory.


<Files>
spu/process_blend_tree.cpp	: Main animation processing function (split to be called from either the SPU job or the PC/win32 version)
spu/process_blend_tree.h	: Headers for the above function
spu/job_anim.cpp			: Animation job
spu/job_anim.mk				: Animation job makefile
 
<Running the Program>
    - Set fileserving root to the self directory 
    - Run the program
