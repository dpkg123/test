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
#include "process_blend_tree.h"

/* Job main
 * - retrieves parameters from SPURS Job 2.0
 * - actual animation processing is in processBlendTree(), in ./process_blend_tree.cpp (shared between SPU and PC versions)
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
    } spursBuffers;
    EDGE_ASSERT(sizeof(spursBuffers) >= jobContext->numIoBuffer * sizeof(void*));
    cellSpursJobGetPointerList((void**)(void*)&spursBuffers, &jobAnim->header, jobContext);

    // Cached read-only buffers (expected to be shared across multiple characters)
    EDGE_ASSERT(3 >= jobContext->numCacheBuffer);
    const EdgeAnimPpuContext* ppuContext = (const EdgeAnimPpuContext*) jobContext->cacheBuffer[0];
	const EdgeAnimSkeleton* skeleton = (const EdgeAnimSkeleton* ) jobContext->cacheBuffer[1];
	const EdgeAnimMirrorPair* mirrorPairs = (const EdgeAnimMirrorPair* ) jobContext->cacheBuffer[2];

    // Output buffer for matrices
    Vectormath::Aos::Transform3* outputMatrices = (Vectormath::Aos::Transform3*) jobContext->oBuffer;

    // Retrieve parameters, stored after the DMA lists element
    // warning: the following formula is only right because we have on DMA list element per buffer.
    const int numDmaListElements = jobContext->numIoBuffer + jobContext->numCacheBuffer;
    const uint32_t* params = (const uint32_t*)(void*)(&jobAnim->workArea.userData[numDmaListElements]);

	//pad
	params += 2;

    // Root joint
    const EdgeAnimJointTransform* rootJoint = reinterpret_cast<const EdgeAnimJointTransform*>(params);
    params += sizeof(EdgeAnimJointTransform) / sizeof(*params);

    // Other parameters
	uint32_t numBlendBranchesAndLeaves = *params++;
    uint32_t numBlendBranches = numBlendBranchesAndLeaves >> 16;
    uint32_t numBlendLeaves = numBlendBranchesAndLeaves & 0xFFFFU;
    uint32_t eaOutputMatrices = *params++; 
	uint32_t numMirrorPairs = *params++;

    // Process the blend tree 
    processBlendTree(outputMatrices, ppuContext, cellSpursGetCurrentSpuId(), skeleton, spursBuffers.blendTreeBranches, numBlendBranches, 
        spursBuffers.blendTreeLeaves, numBlendLeaves, rootJoint, jobContext->sBuffer, jobAnim->header.sizeScratch << 4U, mirrorPairs, numMirrorPairs);

    // Dma output buffer
    cellDmaLargePut(outputMatrices, eaOutputMatrices, skeleton->numJoints * sizeof(Vectormath::Aos::Transform3), jobContext->dmaTag, 0, 0);
}

