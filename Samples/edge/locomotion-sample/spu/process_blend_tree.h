/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdlib.h>
#include "edge/edge_stdint.h"
#include "edge/anim/edgeanim_structs_ps3.h"

// Shared PPU / SPU structures

struct EDGE_ALIGNED(16) LocomotionState
{
	int32_t						loopCount;
	uint32_t					pad[3];

	Vectormath::Aos::Quat		lastRootRotation;
	Vectormath::Aos::Point3		lastRootTranslation;
};

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

// This SPU function is also called from the PC version of the sample
void processBlendTree(Vectormath::Aos::Transform3* outputMatrices, EdgeAnimJointTransform* outputRootJoint, const EdgeAnimPpuContext* ppuContext, uint32_t spuId, const EdgeAnimSkeleton* skeleton, const EdgeAnimBlendBranch* blendBranches, uint32_t numBlendBranches, const EdgeAnimBlendLeaf* blendLeaves, uint32_t numBlendLeaves, const EdgeAnimJointTransform* rootJoint, void* scratchBuffer, uint32_t sizeScratchBuffer);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __COMMON_H__
