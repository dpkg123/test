/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdlib.h>
#ifdef __SPU__
#include "edge/anim/edgeanim_spu.h"
#else
#include "edge/anim/edgeanim_windows.h"
#endif

/* Animation processing
 * - initialize edgeAnim with skeleton size (to allocate slots for the pose stack, and other buffers, 
 *   in user supplied scratch-space)
 * - process the blend tree
 * - convert-local space joint transforms to world-space matrices and write them in the output buffer
 *
 * Notes
 * - this function is used in both SPU and PC versions of this sample.
 */
extern "C" 
void processBlendTree(Vectormath::Aos::Transform3* outputMatrices, 
                      const EdgeAnimPpuContext* ppuContext,
                      uint32_t spuId,
                      const EdgeAnimSkeleton* skeleton,
                      const EdgeAnimBlendBranch* blendBranches, 
                      uint32_t numBlendBranches,
                      const EdgeAnimBlendLeaf* blendLeaves, 
                      uint32_t numBlendLeaves, 
                      const EdgeAnimJointTransform* rootJoint,
                      void* scratchBuffer, 
                      uint32_t sizeScratchBuffer)
{
    EdgeAnimSpuContext spuContext;
    // TODO: fix max anim header size 
    edgeAnimSpuInitialize(&spuContext, ppuContext, spuId, scratchBuffer, sizeScratchBuffer, skeleton->numJoints, skeleton->numUserChannels);
    edgeAnimProcessBlendTree(&spuContext, 0 | EDGE_ANIM_BLEND_TREE_INDEX_BRANCH, blendBranches, numBlendBranches, blendLeaves, 
        numBlendLeaves, skeleton, NULL, NULL);
    
    // Convert local joints to world matrices (in the output buffer) 
    EdgeAnimPoseInfo poseLocal;
    edgeAnimPoseStackGetPose(&spuContext, &poseLocal, 0);
    edgeAnimLocalJointsToWorldMatrices4x4(outputMatrices, poseLocal.jointArray, rootJoint, skeleton->simdHierarchy, 
        skeleton->numSimdHierarchyQuads * 4);
    
    // Empty the stack
    edgeAnimPoseStackPop(&spuContext); // pop local joints

    // Finalize
    edgeAnimSpuFinalize(&spuContext);
}
