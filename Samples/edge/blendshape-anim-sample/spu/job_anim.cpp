/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <spu_intrinsics.h>
#include <cell/dma.h>
#include <cell/spurs/common.h>
#include <cell/spurs/job_chain.h>
#include <cell/spurs/job_context.h>

#include "edge/edge_assert.h"
#include "edge/anim/edgeanim_spu.h"

using namespace Vectormath::Aos;

/* Animation processing
 * - initialise edgeAnim with skeleton size (to allocate slots for the pose stack, and other buffers, 
 *   in user supplied scratch-space)
 * - process the blend tree
 * - convert-local space joint transforms to world-space joint transforms
 * - convert world-space joint transforms to matrices and write them in the output buffer
 */
static void processBlendTree(void* outputMatrices, 
							 float* outputUserChannels,
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
    edgeAnimProcessBlendTree(&spuContext, 0 | EDGE_ANIM_BLEND_TREE_INDEX_LEAF, blendBranches, numBlendBranches, blendLeaves, 
		numBlendLeaves, skeleton, NULL, NULL);
    
    // Convert local joints to world joints 
    EdgeAnimPoseInfo poseLocal;
    EdgeAnimPoseInfo poseWorld;
    edgeAnimPoseStackPush(&spuContext);
    edgeAnimPoseStackGetPose(&spuContext, &poseLocal, 1);
    edgeAnimPoseStackGetPose(&spuContext, &poseWorld, 0);
    edgeAnimLocalJointsToWorldJoints(poseWorld.jointArray, poseLocal.jointArray, rootJoint, skeleton->simdHierarchy, 
		skeleton->numSimdHierarchyQuads * 4);

    // Convert world joint to matrices (in the output buffer)
    edgeAnimJointsToMatrices3x4(outputMatrices, poseWorld.jointArray, skeleton->numJoints);

	// Copy user channels to output buffer
	for(uint32_t i=0; i<skeleton->numUserChannels; i++)
		outputUserChannels[i] = poseLocal.userChannelArray[i];
    
    // Empty the stack
    edgeAnimPoseStackPop(&spuContext); // pop world joints
    edgeAnimPoseStackPop(&spuContext); // pop local joints

    // Finalize
    edgeAnimSpuFinalize(&spuContext);
}

/* Job main
 * - retrieves parameters from SPURS Job 2.0
 * - please refer to SampleApp::CreateAnimJob in ../main.cpp for details on buffer setup
 */
extern "C" 
void cellSpursJobMain2(CellSpursJobContext2* jobContext, CellSpursJob256 *jobAnim)
{
    // Input buffers
    struct __attribute__(aligned((16))) 
    {        
        const EdgeAnimBlendBranch* blendTreeBranches;
        const EdgeAnimBlendLeaf* blendTreeLeaves;
        const void* invBindMatrices;
    } spursBuffers;
    EDGE_ASSERT(sizeof(spursBuffers) >= jobContext->numIoBuffer * sizeof(void*));
    cellSpursJobGetPointerList((void**)&spursBuffers, &jobAnim->header, jobContext);

    // Cached read-only buffers (expected to be shared across multiple characters)
    EDGE_ASSERT(2 >= jobContext->numCacheBuffer);
    const EdgeAnimPpuContext* ppuContext = (const EdgeAnimPpuContext*) jobContext->cacheBuffer[0];
    const EdgeAnimSkeleton* skeleton = (const EdgeAnimSkeleton* ) jobContext->cacheBuffer[1];
    
    // Output buffer for matrices
	Vectormath::Aos::Transform3* outputMatrices = (Vectormath::Aos::Transform3*) jobContext->oBuffer;
	float* outputUserChannels = (float*)(outputMatrices + EDGE_ALIGN(skeleton->numJoints, 4));

    // Retrieve parameters, stored after the DMA lists element
    // warning: the following formula is only right because we have on DMA list element per buffer.
    const int numDmaListElements = jobContext->numIoBuffer + jobContext->numCacheBuffer;
    const uint32_t* params = (uint32_t*)(void*)(&jobAnim->workArea.userData[numDmaListElements+1]);

    // Root joint
    const EdgeAnimJointTransform* rootJoint = reinterpret_cast<const EdgeAnimJointTransform*>(params);
    params += sizeof(EdgeAnimJointTransform) / sizeof(*params);

    // Other parameters
    uint32_t numBlendBranchesAndLeaves = *params++;
    uint32_t numBlendBranches = numBlendBranchesAndLeaves >> 16;
    uint32_t numBlendLeaves = numBlendBranchesAndLeaves & 0xFFFFU;
	uint32_t eaOutputMatrices = *params++; 
	uint32_t eaOutputUserChannels = *params++; 
    
    // Process the blend tree 
	processBlendTree(outputMatrices, outputUserChannels, ppuContext, cellSpursGetCurrentSpuId(), skeleton, spursBuffers.blendTreeBranches, numBlendBranches, 
		spursBuffers.blendTreeLeaves, numBlendLeaves, rootJoint, jobContext->sBuffer, jobAnim->header.sizeScratch << 4U);

	// Multiply world matrices by inverse bind matrices to get final skinning matrices
	edgeAnimMultiplyMatrices3x4(outputMatrices, outputMatrices, spursBuffers.invBindMatrices, skeleton->numJoints);

    // Dma output buffer
	cellDmaPut(outputMatrices, eaOutputMatrices, skeleton->numJoints * 48, jobContext->dmaTag, 0, 0);
	cellDmaPut(outputUserChannels, eaOutputUserChannels, EDGE_ALIGN(skeleton->numUserChannels * sizeof(float), 16), jobContext->dmaTag, 0, 0);
}
