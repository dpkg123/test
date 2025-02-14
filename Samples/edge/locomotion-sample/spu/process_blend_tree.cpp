/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef _MSC_VER
#include <cell/dma.h>
#include "edge/anim/edgeanim_spu.h"
#else
#define cellDmaPut(ls, ea, size, tag, tid, rid) memcpy((void*)(ea), (void*)(ls), size)
#define cellDmaGet(ls, ea, size, tag, tid, rid) memcpy((void*)(ls), (void*)(ea), size)
#define cellDmaWaitTagStatusAll(c)
#include <memory.h>
#include <malloc.h>
#include "edge/anim/edgeanim_windows.h"
#endif

#include "process_blend_tree.h"

/* Callback to compute locomotion deltas
 * - this callback is called at each stage of the pipeline for an evaluation command (leaf node)
 * - pipelineStage can be -2 (fetch headers; ignored here),  -1 (prefetch), 0 (evaluation)
 */
static void LeafCallback(EdgeAnimSpuContext* spuContext, const EdgeAnimBlendLeaf* const leaf, 
    const EdgeAnimAnimation* anim, const EdgeAnimSkeleton* const skel, 
    const int pipelineStage, const unsigned int dmaTag, void* userScratchBuffer)
{
    LocomotionState* locState = (LocomotionState*)userScratchBuffer;

    // prefetch stage 
    if(pipelineStage == -1)
    {
        // prefetch locomotion state
        cellDmaGet(locState, leaf->userVal, sizeof(LocomotionState), dmaTag, 0, 0);
        return;
    }

    // interested in final pipeline stage (post evaluation)
    if(pipelineStage != 0) {
        return;
    }

    // stall for locomotion state
    cellDmaWaitTagStatusAll(1 << dmaTag);

    // compute deltas
	float *locoDeltaQuatTrans = EDGE_OFFSET_GET_POINTER(float,anim->offsetLocomotionDelta);
	EDGE_ASSERT( locoDeltaQuatTrans );
	EDGE_ASSERT( EDGE_IS_ALIGNED_PS3( locoDeltaQuatTrans, 16 ) );

	Vectormath::Aos::Quat locoDeltaQuat = *((const Vectormath::Aos::Quat*) (locoDeltaQuatTrans+0));
	Vectormath::Aos::Vector3 locoDeltaTrans = *((const Vectormath::Aos::Vector3*) (locoDeltaQuatTrans+4));

    EdgeAnimPoseInfo pose;
    edgeAnimPoseStackGetPose(spuContext, &pose, 0);

	uint32_t locoJointIndex = skel->locomotionJointIndex;

    Vectormath::Aos::Quat loopedRotation = pose.jointArray[locoJointIndex].rotation;
    Vectormath::Aos::Point3 loopedTranslation = pose.jointArray[locoJointIndex].translation;

    Vectormath::Aos::Quat lastRotation = locState->lastRootRotation;
    Vectormath::Aos::Point3 lastTranslation = locState->lastRootTranslation;
    int32_t loopCount = locState->loopCount;

    locState->lastRootRotation = pose.jointArray[locoJointIndex].rotation;
    locState->lastRootTranslation = pose.jointArray[locoJointIndex].translation;

    if(__builtin_expect(loopCount != 0, false)) {
        if(loopCount > 0) {
            for(int32_t loop = 0; loop < loopCount; loop++) {
                Vectormath::Aos::Transform3 deltaTransform(locoDeltaQuat, locoDeltaTrans);
                loopedTranslation = deltaTransform * loopedTranslation;
                loopedRotation = loopedRotation * locoDeltaQuat;
            }
        }
        else {
            for(int32_t loop = 0; loop < -loopCount; loop++) {
                Vectormath::Aos::Quat endDeltaConj = Vectormath::Aos::conj(locoDeltaQuat);
                Vectormath::Aos::Transform3 deltaTransform(endDeltaConj, Vectormath::Aos::Vector3(0,0,0));
                loopedTranslation = deltaTransform * (loopedTranslation - locoDeltaTrans);
                loopedRotation = endDeltaConj * loopedRotation;
            }
        }
    }

    Vectormath::Aos::Transform3 conjRot(Vectormath::Aos::conj(lastRotation), Vectormath::Aos::Vector3(0,0,0));
    Vectormath::Aos::Quat deltaRotation = loopedRotation * Vectormath::Aos::conj(lastRotation);
    Vectormath::Aos::Vector3 deltaTranslation = conjRot * (loopedTranslation - lastTranslation);

    // overwrite root joint transform with deltas
    pose.jointArray[locoJointIndex].rotation = Vectormath::Aos::normalize(deltaRotation);
    pose.jointArray[locoJointIndex].translation = Vectormath::Aos::Point3(deltaTranslation);

    // dma updated locomotion state
    cellDmaPut(locState, leaf->userVal, sizeof(LocomotionState), dmaTag, 0, 0);
}

/* Animation processing
 * - initialise edgeAnim with skeleton size (to allocate slots for the pose stack, and other buffers, 
 *   in user supplied scratch-space)
 * - process the blend tree with callbacks to handle location. please refer to ../readme_e.txt for
 *   more details on the technique used.
 * - convert-local space joint transforms to world-space joint transforms
 * - convert world-space joint transforms to matrices and write them in the output buffer
 *
 * Notes
 * - this function is used in both SPU and PC versions of this sample.
 */
extern "C"
void processBlendTree(Vectormath::Aos::Transform3* outputMatrices, 
                      EdgeAnimJointTransform* outputRootJoint,
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
    edgeAnimSpuInitialize(&spuContext, ppuContext, spuId, scratchBuffer, sizeScratchBuffer, skeleton->numJoints, skeleton->numUserChannels,
        16384U, sizeof(LocomotionState));
    edgeAnimProcessBlendTree(&spuContext, 0 | EDGE_ANIM_BLEND_TREE_INDEX_BRANCH, blendBranches, numBlendBranches, blendLeaves, 
        numBlendLeaves, skeleton, 0, 0, NULL, LeafCallback);
    
    // Convert local joints to world joints 
    EdgeAnimPoseInfo poseLocal;
    EdgeAnimPoseInfo poseWorld;
    edgeAnimPoseStackPush(&spuContext);
    edgeAnimPoseStackGetPose(&spuContext, &poseLocal, 1);
    edgeAnimPoseStackGetPose(&spuContext, &poseWorld, 0);
    edgeAnimLocalJointsToWorldJoints(poseWorld.jointArray, poseLocal.jointArray, rootJoint, skeleton->simdHierarchy, 
        skeleton->numSimdHierarchyQuads * 4);

    // Convert world joint to matrices (in the output buffer)
    edgeAnimJointsToMatrices4x4(outputMatrices, poseWorld.jointArray, skeleton->numJoints);

    // Get updated root joint
	unsigned int locoJointIndex = skeleton->locomotionJointIndex;
    *outputRootJoint = poseWorld.jointArray[locoJointIndex];

    // Empty the stack
    edgeAnimPoseStackPop(&spuContext); // pop world joints
    edgeAnimPoseStackPop(&spuContext); // pop local joints

    // Finalize
    edgeAnimSpuFinalize(&spuContext);
}

