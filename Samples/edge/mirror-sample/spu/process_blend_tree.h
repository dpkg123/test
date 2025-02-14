/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2007 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __PROCESS_BLEND_TREE_H__
#define __PROCESS_BLEND_TREE_H__

#include <stdlib.h>
#include "edge/anim/edgeanim_structs_ps3.h"

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

// this function is shared by PC / PS3
void processBlendTree(Vectormath::Aos::Transform3* outputMatrices, const EdgeAnimPpuContext* ppuContext, uint32_t spuId, const EdgeAnimSkeleton* skeleton, const EdgeAnimBlendBranch* blendBranches, uint32_t numBlendBranches, const EdgeAnimBlendLeaf* blendLeaves, uint32_t numBlendLeaves, const EdgeAnimJointTransform* rootJoint, void* scratchBuffer, uint32_t sizeScratchBuffer, const EdgeAnimMirrorPair* mirrorPairs, uint32_t numMirrorPairs);
#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __PROCESS_BLEND_TREE_H__
