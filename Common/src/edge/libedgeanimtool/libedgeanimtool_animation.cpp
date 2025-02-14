/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */


#ifdef _MSC_VER
#pragma warning( disable : 4702 )       // unreachable code
#endif//_MSC_VER

#include <fstream>
#include <sstream>
#include <algorithm>

#include <math.h>
#include <float.h>

#include "edge/libedgeanimtool/libedgeanimtool_animation.h"
#include "edge/libedgeanimtool/libedgeanimtool_skeleton.h"
#include "edge/libedgeanimtool/libedgeanimtool_filewriter.h"
#include "edge/libedgeanimtool/libedgeanimtool_bitstream.h"
#include "edge/libedgeanimtool/libedgeanimtool_bitpacking.h"

//--------------------------------------------------------------------------------------------------

namespace
{
	// statistics gathering
	struct AnimStats
	{
		unsigned headerSize;
		unsigned tablesSize;
		unsigned framesetDmaSize;
		unsigned framesetInfoSize;
		unsigned specsSize;
		unsigned bitStreamSize;
		unsigned constRSize;
		unsigned animRSize;
		unsigned constTSize;
		unsigned animTSize;
		unsigned constSSize;
		unsigned animSSize;
		unsigned constUSize;
		unsigned animUSize;
		unsigned locomotionSize;
		unsigned customDataSize;
	};

	// DMA list element
	struct EdgeDmaListElement {
		uint16_t reserved;
		uint16_t size;
		uint32_t eal;
	};

//--------------------------------------------------------------------------------------------------
/**
    Squared error between quaternion rotations.
**/
//--------------------------------------------------------------------------------------------------

float QuatSquaredError(const float* pQa, const float* pQb)
{
    float posSqErrorSum = 0.0f;
    float negSqErrorSum = 0.0f;
    for(int i = 0; i < 4; ++i) {
        float posSqError = pQa[i] - pQb[i];
        float negSqError = pQa[i] + pQb[i];
        posSqErrorSum += posSqError * posSqError;
        negSqErrorSum += negSqError * negSqError;
    }
    return (posSqErrorSum < negSqErrorSum) ? posSqErrorSum : negSqErrorSum;
}

//--------------------------------------------------------------------------------------------------
/**
    Squared error between two 4-vectors.
**/
//--------------------------------------------------------------------------------------------------

float VecSquaredError(const float* pVa, const float* pVb)
{
    float posSqErrorSum = 0.0f;
    for(int i = 0; i < 4; ++i) {
        float posSqError = pVa[i] - pVb[i];
        posSqErrorSum += posSqError * posSqError;
    }
    return posSqErrorSum;
}

//--------------------------------------------------------------------------------------------------
/**
    Quaternion slerp
**/
//--------------------------------------------------------------------------------------------------

void QuatSlerp(float* pRes, const float* pQuat0, const float* pQuat1, float t)
{
    const float kSlerpTol = 0.999f;
    float start[4];
    float recipSinAngle, scale0, scale1, cosAngle, angle;

    cosAngle = 0.0f;
    for(int i=0; i<4; i++) {
        cosAngle += pQuat0[i] * pQuat1[i];
    }
    if (cosAngle < 0.0f) {
        cosAngle = -cosAngle;
        for(int i=0; i<4; i++) {
            start[i] = -pQuat0[i];
        }
    } else {
        for(int i=0; i<4; i++) {
            start[i] = pQuat0[i];
        }
    }
    if (cosAngle < kSlerpTol) {
        angle = acosf(cosAngle);
        recipSinAngle = (1.0f / sinf(angle));
        scale0 = (sinf(((1.0f - t) * angle)) * recipSinAngle);
        scale1 = (sinf((t * angle)) * recipSinAngle);
    } else {
        scale0 = (1.0f - t);
        scale1 = t;
    }
    for(int i=0; i<4; i++) {
        pRes[i] = (start[i] * scale0) + (pQuat1[i] * scale1);
    }
}

//--------------------------------------------------------------------------------------------------
/**
    Lerp
**/
//--------------------------------------------------------------------------------------------------

void Lerp(float* pRes, const float* pVec0, const float* pVec1, float t)
{
    for(int i=0; i<4; i++) {
        pRes[i] = (pVec1[i] - pVec0[i]) * t + pVec0[i];
    }
}

//--------------------------------------------------------------------------------------------------
/**
    Quaternion multiply
**/
//--------------------------------------------------------------------------------------------------

void QuatMultiply(float* pRes, const float* pQuat0, const float* pQuat1)
{
	float res[4];
    res[0] = (pQuat0[3] * pQuat1[0]) + (pQuat0[0] * pQuat1[3]) + (pQuat0[1] * pQuat1[2]) - (pQuat0[2] * pQuat1[1]);
    res[1] = (pQuat0[3] * pQuat1[1]) + (pQuat0[1] * pQuat1[3]) + (pQuat0[2] * pQuat1[0]) - (pQuat0[0] * pQuat1[2]);
    res[2] = (pQuat0[3] * pQuat1[2]) + (pQuat0[2] * pQuat1[3]) + (pQuat0[0] * pQuat1[1]) - (pQuat0[1] * pQuat1[0]);
    res[3] = (pQuat0[3] * pQuat1[3]) - (pQuat0[0] * pQuat1[0]) - (pQuat0[1] * pQuat1[1]) - (pQuat0[2] * pQuat1[2]);
    pRes[0] = res[0];
    pRes[1] = res[1];
    pRes[2] = res[2];
    pRes[3] = res[3];	
}

//--------------------------------------------------------------------------------------------------
/**
    Quaternion conjugate
**/
//--------------------------------------------------------------------------------------------------

void QuatConjugate(float* pRes, const float* pQuat)
{
    pRes[0] = -pQuat[0];
    pRes[1] = -pQuat[1];
    pRes[2] = -pQuat[2];
    pRes[3] = pQuat[3];
}
//--------------------------------------------------------------------------------------------------

struct KeyframeMarker
{
    const Edge::Tools::AnimationKeyframe*   m_keyFrame;
    float                                   m_errorLeft;
    float                                   m_errorRight;
};

typedef std::list< KeyframeMarker > MarkerList;

struct MergeCandidate
{
    MarkerList::iterator    m_markerIt;
    float                   m_mergedError;
    bool                    m_isTolerated;
    float                   m_mergeCost;
};

typedef std::list< MergeCandidate > MergeCandidateList;

//--------------------------------------------------------------------------------------------------
/**
    Recomputes the mean error over a keyframe range.  If any of the original keyframes break the
    tolerance value then the merge is rejected.
**/
//--------------------------------------------------------------------------------------------------

void RecomputeMergeError(MergeCandidate& merge, 
                         void (*pfnInterp) (float*, const float*, const float*, float),
                         float (*pfnError) (const float*, const float*), float tolerance)
{
    // get the keyframes either side
    MarkerList::iterator leftMarkerIt = merge.m_markerIt;   
    MarkerList::iterator rightMarkerIt = merge.m_markerIt;
    --leftMarkerIt;
    ++rightMarkerIt;

    // loop over sample points within this range and get the mean error
    const Edge::Tools::AnimationKeyframe *firstKey = leftMarkerIt->m_keyFrame;
    const Edge::Tools::AnimationKeyframe *lastKey = rightMarkerIt->m_keyFrame;
    float errorSum = 0.0f;
    int errorCount = 0;
    const float deltaTimeRcp = 1.0f/(lastKey->m_keyTime - firstKey->m_keyTime);
    bool isTolerated = true;
    for(const Edge::Tools::AnimationKeyframe *key = firstKey + 1; key != lastKey; ++key) {
        float alpha = (key->m_keyTime - firstKey->m_keyTime)*deltaTimeRcp;

        float interpolated[4];
        (*pfnInterp)(interpolated, &firstKey->m_keyData[0], &lastKey->m_keyData[0], alpha);

        float error = (*pfnError)(interpolated, &key->m_keyData[0]);
        if(error > tolerance)
            isTolerated = false;

        errorSum += error;
        errorCount += 1;
    }

    // record the error
    merge.m_mergedError = errorSum;
    merge.m_isTolerated = isTolerated;

    // compute the merge cost
    merge.m_mergeCost = errorSum - (merge.m_markerIt->m_errorLeft + merge.m_markerIt->m_errorRight);
}   

//--------------------------------------------------------------------------------------------------
/**
    Priority-queue merge.
**/
//--------------------------------------------------------------------------------------------------

void OptimizeTweeningQueue(std::vector<Edge::Tools::AnimationKeyframe>& channel, 
                           void (*pfnInterp) (float*, const float*, const float*, float),
                           float (*pfnError) (const float*, const float*), float tolerance)
{
    // we need more than 2 keyframes
    if(channel.size() > 2)
    {
        // initialise the marker list
        MarkerList markerList;
        for(std::vector<Edge::Tools::AnimationKeyframe>::const_iterator ite = channel.begin(); ite != channel.end(); ++ite) {
            markerList.push_back(KeyframeMarker());
            markerList.back().m_keyFrame = &(*ite);
            markerList.back().m_errorLeft = 0.0f;
            markerList.back().m_errorRight = 0.0f;
        }
        
        // initialise the merge candidates by trying every interior marker
        MergeCandidateList mergeCandidates;
        MarkerList::iterator firstMarkerIt = ++markerList.begin();
        MarkerList::iterator lastMarkerIt = --markerList.end();
        for(MarkerList::iterator markerIt = firstMarkerIt; markerIt != lastMarkerIt; ++markerIt) {
            mergeCandidates.push_back(MergeCandidate());
            mergeCandidates.back().m_markerIt = markerIt;
            RecomputeMergeError(mergeCandidates.back(), pfnInterp, pfnError, tolerance);
        }

        // do all merges in order of least error
        MarkerList bestMarkerList;
        for(;;)
        {
            // find the merge with least error (TODO: speed up?)
            MergeCandidateList::iterator bestMergeIt = mergeCandidates.begin();
            for(MergeCandidateList::iterator mergeIt = mergeCandidates.begin(); mergeIt != mergeCandidates.end(); ++mergeIt) {
                RecomputeMergeError(*mergeIt, pfnInterp, pfnError, tolerance);
                if(mergeIt->m_mergeCost < bestMergeIt->m_mergeCost || (mergeIt->m_isTolerated && !bestMergeIt->m_isTolerated))
                    bestMergeIt = mergeIt;
            }
            if(!bestMergeIt->m_isTolerated)
                break;

            // get the neighbouring markers (we always have them because we only merge interior keys)
            MarkerList::iterator leftMarkerIt = bestMergeIt->m_markerIt;
            MarkerList::iterator rightMarkerIt = bestMergeIt->m_markerIt;
            --leftMarkerIt;
            ++rightMarkerIt;

            // update their error terms
            leftMarkerIt->m_errorRight = bestMergeIt->m_mergedError;
            rightMarkerIt->m_errorLeft = bestMergeIt->m_mergedError;

            // remove this marker (performing the merge)
            markerList.erase(bestMergeIt->m_markerIt);

            // update neighbouring merge candidates
            MergeCandidateList::iterator leftMergeIt = bestMergeIt;
            MergeCandidateList::iterator rightMergeIt = bestMergeIt;
            if(leftMergeIt != mergeCandidates.begin()) {
                --leftMergeIt;
                RecomputeMergeError(*leftMergeIt, pfnInterp, pfnError, tolerance);
            }
            if(++rightMergeIt != mergeCandidates.end()) {
                RecomputeMergeError(*rightMergeIt, pfnInterp, pfnError, tolerance);
            }

            // done
            mergeCandidates.erase(bestMergeIt);
        }

        // keep the keyframes we didn't merge away
        std::vector< Edge::Tools::AnimationKeyframe > channelTmp;
        channelTmp.reserve(markerList.size());
        for(MarkerList::iterator markerIt = markerList.begin(); markerIt != markerList.end(); ++markerIt) {
            channelTmp.push_back(*markerIt->m_keyFrame);
        }
        channel.swap(channelTmp);
    }

    // if all remaining keyframes are equal then only keep the first one
    if(channel.size() > 1)
    {
        bool allEqual = true;
        for(std::vector< Edge::Tools::AnimationKeyframe >::const_iterator keyIt = ++channel.begin(); keyIt != channel.end(); ++keyIt) {
            float error = (*pfnError)(&channel.front().m_keyData[0], &keyIt->m_keyData[0]);
            if(error > tolerance) {
                allEqual = false;
                break;
            }
        }
        if(allEqual)
            channel.erase(++channel.begin(), channel.end());
    }
}

//--------------------------------------------------------------------------------------------------
/**
    Greedy optimiser
**/
//--------------------------------------------------------------------------------------------------

void OptimizeTweeningGreedy(std::vector<Edge::Tools::AnimationKeyframe>& channel, 
                            void (*pfnInterp) (float*, const float*, const float*, float),
                            float (*pfnError) (const float*, const float*), float tolerance)
{
    // early out empty channels
    if(channel.empty())
        return;

    // storage for new keyframes
    std::vector<Edge::Tools::AnimationKeyframe> channelTmp;
    channelTmp.reserve(channel.size());

    // first pass - remove interpolable keyframes
    std::vector<Edge::Tools::AnimationKeyframe>::const_iterator iteEnd = channel.end();
    std::vector<Edge::Tools::AnimationKeyframe>::const_iterator iteLast = iteEnd;
    for(std::vector<Edge::Tools::AnimationKeyframe>::const_iterator ite = channel.begin();;)  {
        // store this kf
        channelTmp.push_back(*ite);
        iteLast = ite;

        // advance
        if(++ite == iteEnd)
            break;

        // search forward
        bool isEquivalentToInterpolated = true;
        for(;;) {
            // check error and advance
            if(!isEquivalentToInterpolated || ++ite == iteEnd) {
                // backtrack to last valid keyframe and stop
                --ite;
                break;
            }

			// we don't allow consecutive keys to be 180 degrees apart
			// as this can cause ambiguous slerp direction (due to small numerical 
			// differences between tools and runtime)
			if(pfnInterp == QuatSlerp)
			{
				const float* q0 = &iteLast->m_keyData[0];
				const float* q1 = &ite->m_keyData[0];
				float dot = q0[0]*q1[0] + q0[1]*q1[1] + q0[2]*q1[2] + q0[3]*q1[3];
				if(fabs(dot) < tolerance)
				{
					--ite;
					break;
				}
			}

            // build error
            float deltaTime = ite->m_keyTime - iteLast->m_keyTime;          
            for (std::vector<Edge::Tools::AnimationKeyframe>::const_iterator iteTest = iteLast; iteTest != ite && isEquivalentToInterpolated; iteTest++) {

                // skip start/end keyframes
                if (ite==iteLast || iteTest==ite)
                    continue;

                // check interpolated value / error
                float alpha = (iteTest->m_keyTime - iteLast->m_keyTime) / deltaTime;                
                float interpolated[4];
                (*pfnInterp)(interpolated, &iteLast->m_keyData[0], &ite->m_keyData[0], alpha);
                isEquivalentToInterpolated = isEquivalentToInterpolated && ((*pfnError)(interpolated, &iteTest->m_keyData[0]) <= tolerance);                               
            }
        }       
    }

    // second pass - if all remaining keyframes are equal, just keep the first one
    if (channelTmp.size())
    {
        bool allEqual = true;
        for(std::vector<Edge::Tools::AnimationKeyframe>::const_iterator ite = channelTmp.begin(); allEqual && (ite != channelTmp.end()); ite++) {
            allEqual = allEqual && ((*pfnError)(&channelTmp.front().m_keyData[0], &ite->m_keyData[0]) <= tolerance);
        }
        if (allEqual) {
            channelTmp.erase(channelTmp.begin() + 1, channelTmp.end());
        }
    }

    // swap to new keyframes
    channel.swap(channelTmp);
}

//--------------------------------------------------------------------------------------------------
/**
    Keyframe optimizer 
**/
//--------------------------------------------------------------------------------------------------

void Optimize(Edge::Tools::Animation& animation, float tolerance)
{
    unsigned int numJoints = (unsigned int) animation.m_jointAnimations.size();
    unsigned int numUserChannels = (unsigned int) animation.m_userChannelAnimations.size();

    // if joint weight < 1/255.0f remove it, since we're going to ignore any data coming out of it
    for (unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
        Edge::Tools::JointAnimation* pJointAnim = &animation.m_jointAnimations[jointLoop];

        if (pJointAnim->m_jointWeight < 1.0f/255.0f) {
            pJointAnim->m_rotationAnimation.clear();
            pJointAnim->m_translationAnimation.clear();
            pJointAnim->m_scaleAnimation.clear();
        }
    }
  
    // Regular interpolation optimizations (greedy performs much better on test anim)
#define OptimizeTweening OptimizeTweeningGreedy
//#define OptimizeTweening OptimizeTweeningQueue

    // optimise joint RTS channels
    for (unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
        Edge::Tools::JointAnimation* pJointAnim = &animation.m_jointAnimations[jointLoop];

        OptimizeTweening(pJointAnim->m_rotationAnimation, QuatSlerp, Edge::Tools::QuatAbsError, tolerance);
        OptimizeTweening(pJointAnim->m_translationAnimation, Lerp, Edge::Tools::VecAbsError, tolerance);
        OptimizeTweening(pJointAnim->m_scaleAnimation, Lerp, Edge::Tools::VecAbsError, tolerance);
    }

    // optimise user channels
    for (unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++) {
        Edge::Tools::UserChannelAnimation* pUserChannelAnim = &animation.m_userChannelAnimations[userChannelLoop];
        OptimizeTweening(pUserChannelAnim->m_animation, Lerp, Edge::Tools::VecAbsError, tolerance);
    }
#undef OptimizeTweening
}

//--------------------------------------------------------------------------------------------------
/**
    Remove constant channels equal to the base pose value
**/
//--------------------------------------------------------------------------------------------------

void OptimizeBasePose(std::vector<Edge::Tools::AnimationKeyframe>& channel, const Edge::Tools::Float4& basePoseValue,
                      float (*pfnError) (const float*, const float*), float tolerance)
{
    // remove constant channels that are equal to the base pose
    bool isEquivalent = true;
    for(std::vector<Edge::Tools::AnimationKeyframe>::const_iterator ite = channel.begin(); 
        (ite != channel.end()) && isEquivalent; ite++) {             
        isEquivalent &= ((*pfnError) (&ite->m_keyData[0], &basePoseValue[0]) <= tolerance);
    }
    if (isEquivalent) {
        channel.clear();
    }   
}

//--------------------------------------------------------------------------------------------------
/**
    Remove constant channels equal to the base pose value
**/
//--------------------------------------------------------------------------------------------------

void OptimizeBasePose(Edge::Tools::Animation& animation, const std::vector<Edge::Tools::Joint>& basePose, float tolerance)
{
    unsigned int numJoints = (unsigned int) animation.m_jointAnimations.size();

    for (unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
        Edge::Tools::JointAnimation* pJointAnim = &animation.m_jointAnimations[jointLoop];
		OptimizeBasePose(pJointAnim->m_rotationAnimation, basePose[jointLoop].m_rotation, Edge::Tools::QuatAbsError, tolerance);
        OptimizeBasePose(pJointAnim->m_translationAnimation, basePose[jointLoop].m_translation, Edge::Tools::VecAbsError, tolerance);
        OptimizeBasePose(pJointAnim->m_scaleAnimation, basePose[jointLoop].m_scale, Edge::Tools::VecAbsError, tolerance);
    }
}

//--------------------------------------------------------------------------------------------------
/**
	Reduce constant channels to a single keyframe
**/
//--------------------------------------------------------------------------------------------------

void OptimizeConstantChannels(std::vector<Edge::Tools::AnimationKeyframe>& channel, float (*pfnError) (const float*, const float*), float tolerance)
{
	if(channel.size())
	{
		bool isEquivalent = true;
		for(std::vector<Edge::Tools::AnimationKeyframe>::const_iterator ite = channel.begin(); 
			(ite != channel.end()) && isEquivalent; ite++) {             
				isEquivalent &= ((*pfnError) (&ite->m_keyData[0], &channel[0].m_keyData[0]) <= tolerance);
		}
		if (isEquivalent) {
			channel.resize(1);
		}   
	}
}

//--------------------------------------------------------------------------------------------------
/**
	Reduce constant channels to a single keyframe
**/
//--------------------------------------------------------------------------------------------------

void OptimizeConstantChannels(Edge::Tools::Animation& animation, float tolerance)
{
	unsigned int numJoints = (unsigned int) animation.m_jointAnimations.size();

	for (unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
		Edge::Tools::JointAnimation* pJointAnim = &animation.m_jointAnimations[jointLoop];
		OptimizeConstantChannels(pJointAnim->m_rotationAnimation, Edge::Tools::QuatAbsError, tolerance);
		OptimizeConstantChannels(pJointAnim->m_translationAnimation, Edge::Tools::VecAbsError, tolerance);
		OptimizeConstantChannels(pJointAnim->m_scaleAnimation, Edge::Tools::VecAbsError, tolerance);
	}
}

//--------------------------------------------------------------------------------------------------
/**
    Work out SPU evaluation buffer size for a given frameset

	The evaluation buffer contains the animation header, channel tables, packing specs, and frameset data

	NOTE: This is closely tied in with the way the data is written out by ExportAnimation(), so any 
		  changes there must be reflected here (including padding etc..)
**/
//--------------------------------------------------------------------------------------------------

unsigned int GetSpuEvalBufferSize(const Edge::Tools::Animation& animation, const Edge::Tools::FrameSet& frameSet,
								  unsigned int numConstRChannels, unsigned int numConstTChannels, unsigned int numConstSChannels, unsigned int numConstUChannels, 
								  unsigned int numAnimRChannels, unsigned int numAnimTChannels, unsigned int numAnimSChannels, unsigned int numAnimUChannels, 
								  const Edge::Tools::CompressionInfo& compressionInfo,
								  const Edge::Tools::KeyframePackingSpec& constRSpec, const Edge::Tools::KeyframePackingSpec& constTSpec, 
								  const Edge::Tools::KeyframePackingSpec& constSSpec, const Edge::Tools::KeyframePackingSpec& constUSpec,
								  const std::vector<Edge::Tools::KeyframePackingSpec>& RSpecs, const std::vector<Edge::Tools::KeyframePackingSpec>& TSpecs, 
								  const std::vector<Edge::Tools::KeyframePackingSpec>& SSpecs, const std::vector<Edge::Tools::KeyframePackingSpec>& USpecs)
{
	const unsigned kSizeNonBitPackedR = 48;
	const unsigned kSizeNonBitPackedT = 96;
	const unsigned kSizeNonBitPackedS = 96;
	const unsigned kSizeNonBitPackedU = 32;

	unsigned sizeInitialR = 0;
	unsigned sizeInitialT = 0;
	unsigned sizeInitialS = 0;
	unsigned sizeInitialU = 0;
	unsigned sizeAnimR = 0;
	unsigned sizeAnimT = 0;
	unsigned sizeAnimS = 0;
	unsigned sizeAnimU = 0;

	bool bitpackedR = compressionInfo.m_compressionTypeRotation == Edge::Tools::COMPRESSION_TYPE_BITPACKED;
	bool bitpackedT = compressionInfo.m_compressionTypeTranslation == Edge::Tools::COMPRESSION_TYPE_BITPACKED;
	bool bitpackedS = compressionInfo.m_compressionTypeScale == Edge::Tools::COMPRESSION_TYPE_BITPACKED;
	bool bitpackedU = compressionInfo.m_compressionTypeUser == Edge::Tools::COMPRESSION_TYPE_BITPACKED;

	unsigned sizeConstR = numConstRChannels * (bitpackedR ? constRSpec.GetNumBits() : kSizeNonBitPackedR);
	unsigned sizeConstT = numConstTChannels * (bitpackedT ? constTSpec.GetNumBits() : kSizeNonBitPackedT);
	unsigned sizeConstS = numConstSChannels * (bitpackedS ? constSSpec.GetNumBits() : kSizeNonBitPackedS);
	unsigned sizeConstU = numConstUChannels * (bitpackedU ? constUSpec.GetNumBits() : kSizeNonBitPackedU);

	unsigned numJoints = (unsigned)animation.m_jointAnimations.size();
	for(unsigned i=0; i<numJoints; i++)
	{
		if(animation.m_jointAnimations[i].m_rotationAnimation.size() > 1)
		{
			unsigned keySize =  bitpackedR ? RSpecs[i].GetNumBits() : kSizeNonBitPackedR;
			sizeInitialR += keySize;
			sizeAnimR += frameSet.m_jointFrameSets[i].m_numIntraRFrames * keySize;
		}
		if(animation.m_jointAnimations[i].m_translationAnimation.size() > 1)
		{
			unsigned keySize =  bitpackedT ? TSpecs[i].GetNumBits() : kSizeNonBitPackedT;
			sizeInitialT += keySize;
			sizeAnimT += frameSet.m_jointFrameSets[i].m_numIntraTFrames * keySize;
		}
		if(animation.m_jointAnimations[i].m_scaleAnimation.size() > 1)
		{
			unsigned keySize =  bitpackedS ? SSpecs[i].GetNumBits() : kSizeNonBitPackedS;
			sizeInitialS += keySize;
			sizeAnimS += frameSet.m_jointFrameSets[i].m_numIntraSFrames * keySize;
		}
	}

	unsigned numUserChannels = (unsigned)animation.m_userChannelAnimations.size();
	for(unsigned i=0; i<numUserChannels; i++)
	{
		if(animation.m_userChannelAnimations[i].m_animation.size() > 1)
		{
			unsigned keySize =  bitpackedU ? USpecs[i].GetNumBits() : kSizeNonBitPackedU;
			sizeInitialU += keySize;
			sizeAnimU += frameSet.m_userChannelFrameSets[i].m_numIntraFrames * keySize;
		}
	}

	sizeConstR = (sizeConstR + 7)/8;
	sizeConstT = (sizeConstT + 7)/8;
	sizeConstS = (sizeConstS + 7)/8;
	sizeConstU = (sizeConstU + 7)/8;
	sizeInitialR = (sizeInitialR + 7)/8;
	sizeInitialT = (sizeInitialT + 7)/8;
	sizeInitialS = (sizeInitialS + 7)/8;
	sizeInitialU = (sizeInitialU + 7)/8;
	sizeAnimR = (sizeAnimR + 7)/8;
	sizeAnimT = (sizeAnimT + 7)/8;
	sizeAnimS = (sizeAnimS + 7)/8;
	sizeAnimU = (sizeAnimU + 7)/8;

	unsigned int size;

	// header, channel tables, packing specs
	size = sizeof(EdgeAnimAnimation);                                   // header
	size = (size + 15) & (~15);                                         // 16 byte align
	size += sizeof(short) * ((numConstRChannels + 7) & (~7));			// const R table
	size += sizeof(short) * ((numConstTChannels + 3) & (~3));			// const T table
	size += sizeof(short) * ((numConstSChannels + 3) & (~3));			// const S table
	size += sizeof(short) * ((numConstUChannels + 3) & (~3));			// const U table
	size += sizeof(short) * ((numAnimRChannels + 3) & (~3));            // anim R table
	size += sizeof(short) * ((numAnimTChannels + 3) & (~3));            // anim T table
	size += sizeof(short) * ((numAnimSChannels + 3) & (~3));            // anim S table
	size += sizeof(short) * ((numAnimUChannels + 3) & (~3));			// anim U table
	size = (size + 15) & (~15);                                         // 16 byte align
	size += sizeConstR;												    // constant R data
	size = (size + 15) & (~15);                                         // 16 byte align
	size += sizeConstT;													// constant T data
	size = (size + 15) & (~15);                                         // 16 byte align
	size += sizeConstS;										            // constant S data
	size = (size + 15) & (~15);                                         // 16 byte align
	size += sizeConstU;													// constant user data
	size = (size + 3) & (~3);	                                        // 4 byte align
	if(bitpackedR)
		size += sizeof(uint32_t) * (numAnimRChannels + 1);				// R packing specs
	if(bitpackedT)
		size += sizeof(uint32_t) * (numAnimTChannels + 1);				// T packing specs
	if(bitpackedS)
		size += sizeof(uint32_t) * (numAnimSChannels + 1);				// S packing specs
	if(bitpackedU)
		size += sizeof(uint32_t) * (numAnimUChannels + 1);				// U packing specs
	size = (size + 15) & (~15);                                         // 16 byte align
	
	// frameset data
	size += 128;														// worst case frameset dma alignment
	size += sizeof(short) * 8;											// frameset offsets
	size += sizeInitialR;		                                        // initial R data
	size += sizeInitialT;						                        // initial T data
	size += sizeInitialS;												// initial S data
	size = (size + 3) & (~3);											// 4 byte align
	size += sizeInitialU;												// initial user data
	size += ((numAnimRChannels + numAnimTChannels + numAnimSChannels +  // intra frame bitstream
		numAnimUChannels) * frameSet.m_numIntraFrames + 7)/8;
	size += sizeAnimR;													// intra R data
	size += sizeAnimT;													// intra T data
	size += sizeAnimS;													// intra S data
	size = (size + 3) & (~3);											// 4 byte align
	size += sizeAnimU;													// intra user data
	size = (size + 15) & (~15);                                         // 16 byte align

	// final data ( = next frameset initial data)
	size += sizeof(short) * 8;											// frameset offsets
	size += sizeInitialR;												// final R data
	size += sizeInitialT;												// final T data
	size += sizeInitialS;												// final S data
	size = (size + 3) & (~3);											// 4 byte align
	size += sizeInitialU;												// final user data
	size = (size + 15) & (~15);                                         // 16 byte align

	return size;
}

//--------------------------------------------------------------------------------------------------
/**
    Generate framesets
**/
//--------------------------------------------------------------------------------------------------

void GenerateFramesets(std::vector<Edge::Tools::FrameSet>& frameSets, const Edge::Tools::Animation& animation, unsigned int maxEvalBufferSize,
					   const Edge::Tools::CompressionInfo& compressionInfo, 
					   const Edge::Tools::KeyframePackingSpec& constRSpec, const Edge::Tools::KeyframePackingSpec& constTSpec, 
					   const Edge::Tools::KeyframePackingSpec& constSSpec, const Edge::Tools::KeyframePackingSpec& constUSpec,
					   const std::vector<Edge::Tools::KeyframePackingSpec>& RSpecs, const std::vector<Edge::Tools::KeyframePackingSpec>& TSpecs, 
					   const std::vector<Edge::Tools::KeyframePackingSpec>& SSpecs, const std::vector<Edge::Tools::KeyframePackingSpec>& USpecs)
{
    float period = animation.m_period;
    unsigned int numJoints = (unsigned int) animation.m_jointAnimations.size();
    unsigned int numUserChannels = (unsigned int) animation.m_userChannelAnimations.size();
    unsigned int numFrames = (unsigned int) animation.m_numFrames;

    std::vector<unsigned int> jointCurRFrame;
    std::vector<unsigned int> jointCurTFrame;
    std::vector<unsigned int> jointCurSFrame;
    jointCurRFrame.reserve(numJoints);
    jointCurTFrame.reserve(numJoints);
    jointCurSFrame.reserve(numJoints);

    std::vector<unsigned int> userChannelCurFrame;
    userChannelCurFrame.reserve(numUserChannels);

    for(unsigned int i=0; i<numJoints; i++) {
        jointCurRFrame.push_back(0);
        jointCurTFrame.push_back(0);
        jointCurSFrame.push_back(0);
    }

    for(unsigned int i=0; i<numUserChannels; i++) {
        userChannelCurFrame.push_back(0);
    }

    unsigned int curFrame = 0;

    while(curFrame < numFrames) {

        Edge::Tools::FrameSet frameSet;
        frameSet.m_baseFrame = curFrame;
        frameSet.m_jointFrameSets.reserve(numJoints);
        frameSet.m_userChannelFrameSets.reserve(numUserChannels);
		frameSet.m_numIntraFrames = 0;

        unsigned int numConstRChannels = 0;
        unsigned int numConstTChannels = 0;
        unsigned int numConstSChannels = 0;
        unsigned int numConstUserChannels = 0;
        unsigned int numAnimRChannels = 0;
        unsigned int numAnimTChannels = 0;
        unsigned int numAnimSChannels = 0;
        unsigned int numAnimUserChannels = 0;

        float t0 = curFrame * period;

        // store initial joint keyframe data
        for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
            Edge::Tools::JointFrameSet jointFrameSet;

			jointFrameSet.m_numIntraRFrames = 0;
			jointFrameSet.m_numIntraTFrames = 0;
			jointFrameSet.m_numIntraSFrames = 0;

            const std::vector<Edge::Tools::AnimationKeyframe>& rotations = animation.m_jointAnimations[jointLoop].m_rotationAnimation;
            const std::vector<Edge::Tools::AnimationKeyframe>& translations = animation.m_jointAnimations[jointLoop].m_translationAnimation;
            const std::vector<Edge::Tools::AnimationKeyframe>& scales = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

            if(rotations.size() > 1) {
                unsigned int rIdx = jointCurRFrame[jointLoop];

                Edge::Tools::Float4 r;

                // does R keyframe exist?
                if(fabs(rotations[rIdx].m_keyTime - t0) < period/2) {
                    r = rotations[rIdx].m_keyData;
                    jointCurRFrame[jointLoop]++;
                }
                // no R keyframe, so slerp
                else {
                    const Edge::Tools::AnimationKeyframe& ka = rotations[rIdx-1];
                    const Edge::Tools::AnimationKeyframe& kb = rotations[rIdx];
                    QuatSlerp(r.m_data, ka.m_keyData.m_data, kb.m_keyData.m_data, 
                        (t0 - ka.m_keyTime) / (kb.m_keyTime - ka.m_keyTime));
                }

                jointFrameSet.m_initialRData = r;
                numAnimRChannels++;
            }
            else if(rotations.size() == 1) {
                numConstRChannels++;
            }

            if(translations.size() > 1) {
                unsigned int tIdx = jointCurTFrame[jointLoop];

                Edge::Tools::Float4 t;

                // does T keyframe exist?
                if(fabs(translations[tIdx].m_keyTime - t0) < period/2) {
                    t = translations[tIdx].m_keyData;
                    jointCurTFrame[jointLoop]++;
                }
                // no T keyframe, so lerp
                else {
                    const Edge::Tools::AnimationKeyframe& ta = translations[tIdx-1];
                    const Edge::Tools::AnimationKeyframe& tb = translations[tIdx];
                    Lerp(t.m_data, ta.m_keyData.m_data, tb.m_keyData.m_data, 
                        (t0 - ta.m_keyTime) / (tb.m_keyTime - ta.m_keyTime));
                }

                jointFrameSet.m_initialTData = t;

                numAnimTChannels++;
            }
            else if(translations.size() == 1) {
                numConstTChannels++;
            }

            if(scales.size() > 1) {
                unsigned int sIdx = jointCurSFrame[jointLoop];

                Edge::Tools::Float4 s;

                // does S keyframe exist?
                if(fabs(scales[sIdx].m_keyTime - t0) < period/2) {
                    s = scales[sIdx].m_keyData;
                    jointCurSFrame[jointLoop]++;
                }
                // no S keyframe, so lerp
                else {
                    const Edge::Tools::AnimationKeyframe& sa = scales[sIdx-1];
                    const Edge::Tools::AnimationKeyframe& sb = scales[sIdx];
                    Lerp(s.m_data, sa.m_keyData.m_data, sb.m_keyData.m_data, 
                        (t0 - sa.m_keyTime) / (sb.m_keyTime - sa.m_keyTime));
                }

                jointFrameSet.m_initialSData = s;

                numAnimSChannels++;
            }
            else if(scales.size() == 1) {
                numConstSChannels++;
            }

            frameSet.m_jointFrameSets.push_back(jointFrameSet);
        }

        // store initial user channel keyframe data
        for(unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++) {
            Edge::Tools::UserChannelFrameSet userChannelFrameSet;

			userChannelFrameSet.m_numIntraFrames = 0;

            const std::vector<Edge::Tools::AnimationKeyframe>& userData = animation.m_userChannelAnimations[userChannelLoop].m_animation;

            if(userData.size() > 1) {
                unsigned int idx = userChannelCurFrame[userChannelLoop];

                float k;

                // does keyframe exist?
                if(fabs(userData[idx].m_keyTime - t0) < period/2) {
                    k = userData[idx].m_keyData[0];
                    userChannelCurFrame[userChannelLoop]++;
                }
                // no keyframe, so lerp
                else {
                    const Edge::Tools::AnimationKeyframe& a = userData[idx-1];
                    const Edge::Tools::AnimationKeyframe& b = userData[idx];
                    Edge::Tools::Float4 c;
                    Lerp(c.m_data, a.m_keyData.m_data, b.m_keyData.m_data, 
                        (t0 - a.m_keyTime) / (b.m_keyTime - a.m_keyTime));
                    k = c[0];
                }

                userChannelFrameSet.m_initialData = k;
                numAnimUserChannels++;
            }
            else if(userData.size() == 1) {
                numConstUserChannels++;
            }

            frameSet.m_userChannelFrameSets.push_back(userChannelFrameSet);
        }

        curFrame++;

        // store index of intra frame 0 for each channel
        for(unsigned int jointLoop=0; jointLoop<numJoints; jointLoop++) {
            Edge::Tools::JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];

            jointFrameSet.m_intraRFrame0 = jointCurRFrame[jointLoop];
            jointFrameSet.m_intraTFrame0 = jointCurTFrame[jointLoop];
            jointFrameSet.m_intraSFrame0 = jointCurSFrame[jointLoop];
        }

        for(unsigned int userChannelLoop=0; userChannelLoop<numUserChannels; userChannelLoop++) {
            Edge::Tools::UserChannelFrameSet& userChannelFrameSet = frameSet.m_userChannelFrameSets[userChannelLoop];

            userChannelFrameSet.m_intraFrame0 = userChannelCurFrame[userChannelLoop];
        }

		// get size of buffer without any intra frames
        unsigned int evalBufferSize = GetSpuEvalBufferSize(
            animation, frameSet, 
            numConstRChannels, numConstTChannels, numConstSChannels, numConstUserChannels,
            numAnimRChannels, numAnimTChannels, numAnimSChannels, numAnimUserChannels, 
			compressionInfo, constRSpec, constTSpec, constSSpec, constUSpec, RSpecs, TSpecs, SSpecs, USpecs);

        // now keep adding intra frames until the buffer size exceeds maxEvalBufferSize
        while((frameSet.m_numIntraFrames < 128) && ((curFrame + frameSet.m_numIntraFrames) < numFrames - 1) && (evalBufferSize < maxEvalBufferSize)) {

            float t = (curFrame + frameSet.m_numIntraFrames) * period;

            for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
                Edge::Tools::JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];

                const std::vector<Edge::Tools::AnimationKeyframe>& rotations = animation.m_jointAnimations[jointLoop].m_rotationAnimation;
                const std::vector<Edge::Tools::AnimationKeyframe>& translations = animation.m_jointAnimations[jointLoop].m_translationAnimation;
                const std::vector<Edge::Tools::AnimationKeyframe>& scales = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

                if((rotations.size() > 1) && (fabs(rotations[jointCurRFrame[jointLoop]].m_keyTime - t) < period/2)) {
                    jointFrameSet.m_hasRFrame.push_back(1);
					jointFrameSet.m_numIntraRFrames++;
                    jointCurRFrame[jointLoop]++;
                }
                else {
                    jointFrameSet.m_hasRFrame.push_back(0);
                }

                if((translations.size() > 1) && (fabs(translations[jointCurTFrame[jointLoop]].m_keyTime - t) < period/2)) {
                    jointFrameSet.m_hasTFrame.push_back(1);
					jointFrameSet.m_numIntraTFrames++;
                    jointCurTFrame[jointLoop]++;
                }
                else {
                    jointFrameSet.m_hasTFrame.push_back(0);
                }

               if((scales.size() > 1) && (fabs(scales[jointCurSFrame[jointLoop]].m_keyTime - t) < period/2)) {
                    jointFrameSet.m_hasSFrame.push_back(1);
					jointFrameSet.m_numIntraSFrames++;
                    jointCurSFrame[jointLoop]++;
                }
                else {
                    jointFrameSet.m_hasSFrame.push_back(0);
                }
            }

            for(unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++) {
                Edge::Tools::UserChannelFrameSet& userChannelFrameSet = frameSet.m_userChannelFrameSets[userChannelLoop];

                const std::vector<Edge::Tools::AnimationKeyframe>& userData = 
                    animation.m_userChannelAnimations[userChannelLoop].m_animation;

                if((userData.size() > 1) && (fabs(userData[userChannelCurFrame[userChannelLoop]].m_keyTime - t) < period/2)) {
                    userChannelFrameSet.m_hasFrame.push_back(1);
					userChannelFrameSet.m_numIntraFrames++;
                    userChannelCurFrame[userChannelLoop]++;
                }
                else {
                    userChannelFrameSet.m_hasFrame.push_back(0);
                }
            }

            frameSet.m_numIntraFrames++;

			evalBufferSize = GetSpuEvalBufferSize(
				animation, frameSet, 
				numConstRChannels, numConstTChannels, numConstSChannels, numConstUserChannels,
				numAnimRChannels, numAnimTChannels, numAnimSChannels, numAnimUserChannels, 
				compressionInfo, constRSpec, constTSpec, constSSpec, constUSpec, RSpecs, TSpecs, SSpecs, USpecs);
        }

        // check if we are over the maximum size for a frameset
        if(evalBufferSize > maxEvalBufferSize)
        {
            // max frameset size must be at least large enough to store the initial pose
            EDGEERROR(frameSet.m_numIntraFrames > 0)

            // remove the last intra frame
            frameSet.m_numIntraFrames--;

            // adjust channel counts
            for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
                Edge::Tools::JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];

                unsigned char hasRFrame = jointFrameSet.m_hasRFrame.back();
                unsigned char hasTFrame = jointFrameSet.m_hasTFrame.back();
                unsigned char hasSFrame = jointFrameSet.m_hasSFrame.back();

                if(hasRFrame) {
					jointFrameSet.m_numIntraRFrames--;
                    jointCurRFrame[jointLoop]--;
                }

                if(hasTFrame) {
					jointFrameSet.m_numIntraTFrames--;
                    jointCurTFrame[jointLoop]--;
                }

                if(hasSFrame) {
					jointFrameSet.m_numIntraSFrames--;
                    jointCurSFrame[jointLoop]--;
                }

                jointFrameSet.m_hasRFrame.pop_back();
                jointFrameSet.m_hasTFrame.pop_back();
                jointFrameSet.m_hasSFrame.pop_back();
            }

			for(unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++) {
				Edge::Tools::UserChannelFrameSet& userChannelFrameSet = frameSet.m_userChannelFrameSets[userChannelLoop];

				unsigned char hasFrame = userChannelFrameSet.m_hasFrame.back();

				if(hasFrame) {
					userChannelFrameSet.m_numIntraFrames--;
					userChannelCurFrame[userChannelLoop]--;
				}

				userChannelFrameSet.m_hasFrame.pop_back();
			}

			evalBufferSize = GetSpuEvalBufferSize(
				animation, frameSet,  
				numConstRChannels, numConstTChannels, numConstSChannels, numConstUserChannels,
				numAnimRChannels, numAnimTChannels, numAnimSChannels, numAnimUserChannels, 
				compressionInfo, constRSpec, constTSpec, constSSpec, constUSpec, RSpecs, TSpecs, SSpecs, USpecs);
        }

        frameSets.push_back(frameSet);
        curFrame += frameSet.m_numIntraFrames;
    }
}

//--------------------------------------------------------------------------------------------------
/**
    Get index of nearest keyframe prior or equal to time t
**/
//--------------------------------------------------------------------------------------------------

int FindNearestPriorKeyframe(const std::vector<Edge::Tools::AnimationKeyframe>& keyframes, float t, float period)
{
    // TODO: binary search
    int n = (int) keyframes.size();
    int leftIdx = n-1;

    for(int i=0; i<n; i++) {
        if(keyframes[i].m_keyTime > t + period/2) {
            leftIdx = i-1;
            break;
        }
    }

    return leftIdx;
}

//--------------------------------------------------------------------------------------------------
/**
    Get interpolated keyframe data at time t
**/
//--------------------------------------------------------------------------------------------------

void GetInterpolatedKeyframeData(float* pRes, const std::vector<Edge::Tools::AnimationKeyframe>& keyframes, float t, float period, bool rotation)
{
    EDGEERROR(keyframes.size() > 0);

    int leftIdx = FindNearestPriorKeyframe(keyframes, t, period);

    // if no keyframes prior to this time, return first keyframe
    if(leftIdx == -1) {
        const float* a = keyframes[0].m_keyData.m_data;
        pRes[0] = a[0];
        pRes[1] = a[1];
        pRes[2] = a[2];
        pRes[3] = a[3];
        return;
    }

    // if no keyframes after this time, return last keyframe
    if(leftIdx == (int)(keyframes.size() - 1)) {
        const float* a = keyframes[leftIdx].m_keyData.m_data;
        pRes[0] = a[0];
        pRes[1] = a[1];
        pRes[2] = a[2];
        pRes[3] = a[3];
        return;
    }

    const float* a = keyframes[leftIdx].m_keyData.m_data;
    const float* b = keyframes[leftIdx+1].m_keyData.m_data;

    float t0 = keyframes[leftIdx].m_keyTime;
    float t1 = keyframes[leftIdx+1].m_keyTime;

    float alpha = (t - t0)/(t1 - t0);

    if(rotation)
        QuatSlerp(pRes, a, b, alpha);
    else
        Lerp(pRes, a, b, alpha);
}

//--------------------------------------------------------------------------------------------------
/**
    Normalise all rotation keyframes in an animation
**/
//--------------------------------------------------------------------------------------------------

void NormaliseAllRotations(Edge::Tools::Animation& animation)
{
	for(unsigned jointLoop=0; jointLoop<animation.m_jointAnimations.size(); jointLoop++)
	{
		Edge::Tools::JointAnimation& jointAnim = animation.m_jointAnimations[jointLoop];
		for(unsigned keyLoop=0; keyLoop<jointAnim.m_rotationAnimation.size(); keyLoop++)
		{
			Edge::Tools::Float4& rot = jointAnim.m_rotationAnimation[keyLoop].m_keyData;
			float n = 1.0f/sqrtf(rot[0]*rot[0] + rot[1]*rot[1] + rot[2]*rot[2] + rot[3]*rot[3]);
			rot[0] *= n;
			rot[1] *= n;
			rot[2] *= n;
			rot[3] *= n;
		}
	}
}

//--------------------------------------------------------------------------------------------------
/**
    Normalise all rotations in a skeleton
**/
//--------------------------------------------------------------------------------------------------

void NormaliseSkeletonRotations(Edge::Tools::Skeleton& skeleton)
{
	for(unsigned jointLoop=0; jointLoop<skeleton.m_basePose.size(); jointLoop++)
	{
		Edge::Tools::Float4& rot = skeleton.m_basePose[jointLoop].m_rotation;
		float n = 1.0f/sqrtf(rot[0]*rot[0] + rot[1]*rot[1] + rot[2]*rot[2] + rot[3]*rot[3]);
		rot[0] *= n;
		rot[1] *= n;
		rot[2] *= n;
		rot[3] *= n;
	}
}

//--------------------------------------------------------------------------------------------------
/**
    Validate rotation keyframes
	Returns false if any consecutive rotations are 180 degrees apart within the given tolerance
**/
//--------------------------------------------------------------------------------------------------

bool ValidateRotationKeys(const Edge::Tools::Animation& animation, float tolerance)
{
	bool ok = true;
	for(unsigned jointLoop=0; jointLoop<animation.m_jointAnimations.size() && ok; jointLoop++)
	{
		const Edge::Tools::JointAnimation& jointAnim = animation.m_jointAnimations[jointLoop];
		Edge::Tools::Float4 lastRot;
		for(unsigned keyLoop=0; keyLoop<jointAnim.m_rotationAnimation.size() && ok; keyLoop++)
		{
			Edge::Tools::Float4 rot = jointAnim.m_rotationAnimation[keyLoop].m_keyData;
			if(keyLoop > 0)
			{
				float dot = rot[0] * lastRot[0] + rot[1] * lastRot[1] + rot[2] * lastRot[2] + rot[3] * lastRot[3];
				if(fabs(dot) < tolerance)
				{
					printf("Warning: consecutive rotation keys are 180 degrees apart (joint %d, keys %d & %d)\n", jointLoop, keyLoop-1, keyLoop);
					ok = false;
				}
			}
			lastRot = rot;
		}
	}

	return ok;
}

//--------------------------------------------------------------------------------------------------

} // anonymous namespace

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{
	
//--------------------------------------------------------------------------------------------------
/**
    Retrieves the animation from a binary (.anim) resource.
**/
//--------------------------------------------------------------------------------------------------
void ExtractAnimation(const EdgeAnimAnimation* pAnim, const Skeleton& bindSkeleton, CompressedAnimation& animation)
{
#define EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(type,offset)                           \
	(                                                                             \
	(offset)? (type*)((intptr_t)(&offset)+(intptr_t)(Reverse(offset, bigEndian))) \
	: (type*)0                                                                    \
	)                                                                             \

	// check tag + determine endian-ness
	bool bigEndian = false;
	if( Reverse( pAnim->tag, bigEndian ) != edgeAnimGetAnimTag() )
	{
		bigEndian = true;
		if( Reverse( pAnim->tag, bigEndian ) != edgeAnimGetAnimTag() )
		{
			std::cerr << "Invalid edge animation binary" << std::endl;
			EDGEERROR_F();
		}
	}

	// check joint count is consistent with the bind skeleton
	animation.m_numJoints = Reverse( pAnim->numJoints, bigEndian );
	if( animation.m_numJoints != bindSkeleton.m_numJoints )
	{
		std::cerr << "Invalid edge animation binary contains different joint count to bind skeleton" << std::endl;
		EDGEERROR_F();
	}

	// Extract header details
	animation.m_duration = Reverse( pAnim->duration, bigEndian );
	animation.m_sampleFrequency = Reverse( pAnim->sampleFrequency, bigEndian );
	animation.m_numFrames = Reverse( pAnim->numFrames, bigEndian);
	animation.m_numFrameSets = Reverse( pAnim->numFrameSets, bigEndian );
	animation.m_evalBufferSizeRequired = Reverse( pAnim->evalBufferSizeRequired, bigEndian );
	animation.m_numConstRChannels = Reverse( pAnim->numConstRChannels, bigEndian );
	animation.m_numConstTChannels = Reverse( pAnim->numConstTChannels, bigEndian );
	animation.m_numConstSChannels = Reverse( pAnim->numConstSChannels, bigEndian );
	animation.m_numConstUChannels = Reverse( pAnim->numConstUserChannels, bigEndian );
	animation.m_numAnimRChannels = Reverse( pAnim->numAnimRChannels, bigEndian );
	animation.m_numAnimTChannels = Reverse( pAnim->numAnimTChannels, bigEndian );
	animation.m_numAnimSChannels = Reverse( pAnim->numAnimSChannels, bigEndian );
	animation.m_numAnimUChannels = Reverse( pAnim->numAnimUserChannels, bigEndian );
	const unsigned int flags = Reverse( pAnim->flags, bigEndian );

	// joint + user weights
	animation.m_enableWeights = false;
	animation.m_jointWeights.clear();
	animation.m_userChannelWeights.clear();

	const unsigned int sizeJointsWeightArray = Reverse( pAnim->sizeJointsWeightArray, bigEndian );
	if( sizeJointsWeightArray > 0 )
	{
		animation.m_enableWeights = true;

		const unsigned int offsetJointsWeightArray = Reverse( pAnim->offsetJointsWeightArray, bigEndian );
		if( 0 == offsetJointsWeightArray )
		{
			std::cerr << "Invalid edge animation binary missing joints weighting data" << std::endl;
			EDGEERROR_F();
		}

		const uint8_t* pJointsWeightArray = (const uint8_t*)(((uintptr_t) pAnim) + offsetJointsWeightArray);
		for( unsigned int i = 0; i < bindSkeleton.m_numJoints; i++ )
		{
			uint8_t wt = pJointsWeightArray[i];
			animation.m_jointWeights.push_back( wt );
		}

		const uint8_t* pUserChannelsWeightArray = (const uint8_t*)(((uintptr_t) pJointsWeightArray) + EDGE_ALIGN(bindSkeleton.m_numJoints,4));
		for( unsigned int i = 0; i < bindSkeleton.m_numUserChannels; i++ )
		{
			uint8_t wt = pUserChannelsWeightArray[i];
			animation.m_userChannelWeights.push_back( wt );
		}
	}

	// locomotion delta
	animation.m_enableLocoDelta = false;
	const float *pLocomotionDelta = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(float,pAnim->offsetLocomotionDelta);
	if( NULL != pLocomotionDelta )
	{
		animation.m_enableLocoDelta = true;
		animation.m_locoDeltaQuat[0]  = Reverse( pLocomotionDelta[0], bigEndian );
		animation.m_locoDeltaQuat[1]  = Reverse( pLocomotionDelta[1], bigEndian );
		animation.m_locoDeltaQuat[2]  = Reverse( pLocomotionDelta[2], bigEndian );
		animation.m_locoDeltaQuat[3]  = Reverse( pLocomotionDelta[3], bigEndian );
		animation.m_locoDeltaTrans[0] = Reverse( pLocomotionDelta[4], bigEndian );
		animation.m_locoDeltaTrans[1] = Reverse( pLocomotionDelta[5], bigEndian );
		animation.m_locoDeltaTrans[2] = Reverse( pLocomotionDelta[6], bigEndian );
		animation.m_locoDeltaTrans[3] = Reverse( pLocomotionDelta[7], bigEndian );
	}

	// custom data warning
	const unsigned int sizeCustomData = Reverse( pAnim->sizeCustomData, bigEndian );
	if( sizeCustomData > 0 )
	{
		std::cerr << "WARNING: edge animation binary contains custom data that can't be extracted" << std::endl;
	}

	// channel tables
	const uint16_t* constRTable = pAnim->channelTables;
	const uint16_t* constTTable = constRTable + EDGE_ALIGN(animation.m_numConstRChannels, 8);
	const uint16_t* constSTable = constTTable + EDGE_ALIGN(animation.m_numConstTChannels, 4);
	const uint16_t* constUTable = constSTable + EDGE_ALIGN(animation.m_numConstSChannels, 4);
	const uint16_t* animRTable = constUTable + EDGE_ALIGN(animation.m_numConstUChannels, 4);
	const uint16_t* animTTable = animRTable + EDGE_ALIGN(animation.m_numAnimRChannels, 4);
	const uint16_t* animSTable = animTTable + EDGE_ALIGN(animation.m_numAnimTChannels, 4);
	const uint16_t* animUTable = animSTable + EDGE_ALIGN(animation.m_numAnimSChannels, 4);

	// extract compression modes
	animation.m_compressionInfo.m_compressionTypeRotation = COMPRESSION_TYPE_SMALLEST_3;
	animation.m_compressionInfo.m_compressionTypeTranslation = COMPRESSION_TYPE_NONE;
	animation.m_compressionInfo.m_compressionTypeScale = COMPRESSION_TYPE_NONE;
	animation.m_compressionInfo.m_compressionTypeUser = COMPRESSION_TYPE_NONE;

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_R )
	{
		animation.m_compressionInfo.m_compressionTypeRotation = COMPRESSION_TYPE_BITPACKED;
	}

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_T )
	{
		animation.m_compressionInfo.m_compressionTypeTranslation = COMPRESSION_TYPE_BITPACKED;
	}

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_S )
	{
		animation.m_compressionInfo.m_compressionTypeScale = COMPRESSION_TYPE_BITPACKED;
	}

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_U )
	{
		animation.m_compressionInfo.m_compressionTypeUser = COMPRESSION_TYPE_BITPACKED;
	}

	// extract constant channel tables
	animation.m_constRChannels.clear();
	animation.m_constTChannels.clear();
	animation.m_constSChannels.clear();
	animation.m_constUChannels.clear();

	for( unsigned i = 0; i < animation.m_numConstRChannels; i++ )
	{
		animation.m_constRChannels.push_back( Reverse( constRTable[i], bigEndian ) );
	}

	for( unsigned i = 0; i < animation.m_numConstTChannels; i++ )
	{
		animation.m_constTChannels.push_back( Reverse( constTTable[i], bigEndian ) );
	}

	for( unsigned i = 0; i < animation.m_numConstSChannels; i++ )
	{
		animation.m_constSChannels.push_back( Reverse( constSTable[i], bigEndian ) );
	}

	for( unsigned i = 0; i < animation.m_numConstUChannels; i++ )
	{
		animation.m_constUChannels.push_back( Reverse( constUTable[i], bigEndian ) );
	}

	// extract animated channel tables
	animation.m_animRChannels.clear();
	animation.m_animTChannels.clear();
	animation.m_animSChannels.clear();
	animation.m_animUChannels.clear();

	for( unsigned i = 0; i < animation.m_numAnimRChannels; i++ )
	{
		animation.m_animRChannels.push_back( Reverse( animRTable[i], bigEndian ) );
	}

	for( unsigned i = 0; i < animation.m_numAnimTChannels; i++ )
	{
		animation.m_animTChannels.push_back( Reverse( animTTable[i], bigEndian ) );
	}

	for( unsigned i = 0; i < animation.m_numAnimSChannels; i++ )
	{
		animation.m_animSChannels.push_back( Reverse( animSTable[i], bigEndian ) );
	}

	for( unsigned i = 0; i < animation.m_numAnimUChannels; i++ )
	{
		animation.m_animUChannels.push_back( Reverse( animUTable[i], bigEndian ) );
	}

	// extract packing specs
	const uint32_t* pPackingSpecs = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(uint32_t, pAnim->offsetPackingSpecs);
	animation.m_packingSpecsR.clear();
	animation.m_packingSpecsS.clear();
	animation.m_packingSpecsT.clear();
	animation.m_packingSpecsU.clear();

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_R )
	{
		animation.m_packingSpecsR.push_back( Reverse( *pPackingSpecs++, bigEndian ) );
		for( unsigned int i = 0; i < animation.m_numAnimRChannels; i++ )
		{
			KeyframePackingSpec spec( Reverse( *pPackingSpecs++, bigEndian ) );
			animation.m_packingSpecsR.push_back(  spec );
		}
	}

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_T )
	{
		animation.m_packingSpecsT.push_back( Reverse( *pPackingSpecs++, bigEndian ) );
		for( unsigned int i = 0; i < animation.m_numAnimTChannels; i++ )
		{
			KeyframePackingSpec spec( Reverse( *pPackingSpecs++, bigEndian ) );
			animation.m_packingSpecsT.push_back( spec );
		}
	}

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_S )
	{
		animation.m_packingSpecsS.push_back( Reverse( *pPackingSpecs++, bigEndian ) );
		for( unsigned int i = 0; i < animation.m_numAnimSChannels; i++ )
		{
			KeyframePackingSpec spec( Reverse( *pPackingSpecs++, bigEndian ) );
			animation.m_packingSpecsS.push_back( spec );
		}
	}

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_U )
	{
		animation.m_packingSpecsU.push_back( Reverse( *pPackingSpecs++, bigEndian ) );
		for( unsigned int i = 0; i < animation.m_numAnimUChannels; i++ )
		{
			KeyframePackingSpec spec( Reverse( *pPackingSpecs++, bigEndian ) );
			animation.m_packingSpecsU.push_back( spec );
		}
	}

	// extract constant data
	animation.m_constRBitpacked.clear();
	animation.m_constTBitpacked.clear();
	animation.m_constSBitpacked.clear();
	animation.m_constUBitpacked.clear();
	animation.m_constRSmallest3.clear();
	animation.m_constTRaw.clear();
	animation.m_constSRaw.clear();
	animation.m_constURaw.clear();

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_R )
	{
		const KeyframePackingSpec& spec = animation.m_packingSpecsR[0];
		size_t bit8Size = EDGE_ALIGN( animation.m_numConstRChannels * spec.GetNumBits(), 8 );
		size_t byteSize = bit8Size >> 3;

		const uint8_t* pConstData = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(uint8_t,pAnim->offsetConstRData);
		for( size_t i = 0; i < byteSize; i++ )
		{
			animation.m_constRBitpacked.push_back( *pConstData++ );
		}
	}
	else
	{
		const uint8_t* pConstData = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(uint8_t,pAnim->offsetConstRData);
		for( unsigned i = 0; i < animation.m_numConstRChannels; i++ )
		{
			uint64_t constData = 0;
			constData |= ((uint64_t)*pConstData++) << 40;
			constData |= ((uint64_t)*pConstData++) << 32;
			constData |= ((uint64_t)*pConstData++) << 24;
			constData |= ((uint64_t)*pConstData++) << 16;
			constData |= ((uint64_t)*pConstData++) << 8;
			constData |= ((uint64_t)*pConstData++);
			animation.m_constRSmallest3.push_back( constData );
		}
	}

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_T )
	{
		const KeyframePackingSpec& spec = animation.m_packingSpecsT[0];
		size_t bit8Size = EDGE_ALIGN( animation.m_numConstTChannels * spec.GetNumBits(), 8 );
		size_t byteSize = bit8Size >> 3;

		const uint8_t* pConstData = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(uint8_t,pAnim->offsetConstTData);
		for( size_t i = 0; i < byteSize; i++ )
		{
			animation.m_constTBitpacked.push_back( *pConstData++ );
		}
	}
	else
	{
		const float* pConstData = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(float,pAnim->offsetConstTData);
		for( unsigned i = 0; i < animation.m_numConstTChannels; i++ )
		{
			Float4 constData;
			constData.m_data[0] = Reverse( *pConstData++, bigEndian );
			constData.m_data[1] = Reverse( *pConstData++, bigEndian );
			constData.m_data[2] = Reverse( *pConstData++, bigEndian );
			constData.m_data[3] = 1.0f;
			animation.m_constTRaw.push_back( constData );
		}
	}

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_S )
	{
		const KeyframePackingSpec& spec = animation.m_packingSpecsS[0];
		size_t bit8Size = EDGE_ALIGN( animation.m_numConstSChannels * spec.GetNumBits(), 8 );
		size_t byteSize = bit8Size >> 3;

		const uint8_t* pConstData = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(uint8_t,pAnim->offsetConstSData);
		for( size_t i = 0; i < byteSize; i++ )
		{
			animation.m_constSBitpacked.push_back( *pConstData++ );
		}
	}
	else
	{
		const float* pConstData = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(float,pAnim->offsetConstSData);
		for( unsigned i = 0; i < animation.m_numConstSChannels; i++ )
		{
			Float4 constData;
			constData.m_data[0] = Reverse( *pConstData++, bigEndian );
			constData.m_data[1] = Reverse( *pConstData++, bigEndian );
			constData.m_data[2] = Reverse( *pConstData++, bigEndian );
			constData.m_data[3] = 1.0f;
			animation.m_constSRaw.push_back( constData );
		}
	}

	if( flags & EDGE_ANIM_FLAG_BIT_PACKED_U )
	{
		const KeyframePackingSpec& spec = animation.m_packingSpecsU[0];
		size_t bit8Size = EDGE_ALIGN( animation.m_numConstUChannels * spec.m_componentSpecs[0].GetNumBits(), 8 );
		size_t byteSize = bit8Size >> 3;

		const uint8_t* pConstData = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(uint8_t,pAnim->offsetConstUserData);
		for( size_t i = 0; i < byteSize; i++ )
		{
			animation.m_constUBitpacked.push_back( *pConstData++ );
		}
	}
	else
	{
		const float* pConstData = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(float,pAnim->offsetConstUserData);
		for( unsigned i = 0; i < animation.m_numConstUChannels; i++ )
		{
			float constData;
			constData = Reverse( *pConstData++, bigEndian );
			animation.m_constURaw.push_back( constData );
		}
	}

	// extract frame-sets
	const EdgeDmaListElement *pFrameSetDmaArray = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER( EdgeDmaListElement, pAnim->offsetFrameSetDmaArray );
	const EdgeAnimFrameSetInfo *pFrameSetInfoArray = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER( EdgeAnimFrameSetInfo, pAnim->offsetFrameSetInfoArray );
	for(unsigned int i = 0; i < animation.m_numFrameSets; i++)
	{
		CompressedFrameSet frameSet;
		frameSet.m_baseFrame = Reverse( pFrameSetInfoArray[i].baseFrame, bigEndian );
		frameSet.m_numIntraFrames = Reverse( pFrameSetInfoArray[i].numIntraFrames, bigEndian );

		uintptr_t frameSetData = (uintptr_t) pAnim;
		frameSetData += Reverse( pFrameSetDmaArray[i].eal, bigEndian );

		uint16_t* dataSizes = (uint16_t*)frameSetData;
		uint32_t sizeInitialRData = Reverse( dataSizes[0], bigEndian );
		uint32_t sizeInitialTData = Reverse( dataSizes[1], bigEndian );
		uint32_t sizeInitialSData = Reverse( dataSizes[2], bigEndian );
		uint32_t sizeInitialUData = Reverse( dataSizes[3], bigEndian );
		uint32_t sizeIntraRData = Reverse( dataSizes[4], bigEndian );
		uint32_t sizeIntraTData = Reverse( dataSizes[5], bigEndian );
		uint32_t sizeIntraSData = Reverse( dataSizes[6], bigEndian );
		uint32_t sizeIntraUData = Reverse( dataSizes[7], bigEndian );

		uint32_t sizeIntraBits = animation.m_numAnimRChannels;
		sizeIntraBits += animation.m_numAnimTChannels;
		sizeIntraBits += animation.m_numAnimSChannels;
		sizeIntraBits += animation.m_numAnimUChannels;
		sizeIntraBits *= frameSet.m_numIntraFrames;
		sizeIntraBits  = (uint32_t) EDGE_ALIGN( sizeIntraBits, 8 );
		sizeIntraBits >>= 3;

		uintptr_t initialRAdr = frameSetData + 16;
		uintptr_t initialTAdr = initialRAdr + sizeInitialRData;
		uintptr_t initialSAdr = initialTAdr + sizeInitialTData;
		uintptr_t initialUAdr = initialSAdr + sizeInitialSData;
		uintptr_t intraBitsAdr = initialUAdr + sizeInitialUData;
		uintptr_t intraRAdr = intraBitsAdr + sizeIntraBits;
		uintptr_t intraTAdr = intraRAdr + sizeIntraRData;
		uintptr_t intraSAdr = intraTAdr + sizeIntraTData;
		uintptr_t intraUAdr = EDGE_ALIGN(intraSAdr + sizeIntraSData, 4);
		
		// initial joint data
		if( flags & EDGE_ANIM_FLAG_BIT_PACKED_R )
		{
			const uint8_t *pAnimData = (const uint8_t *) initialRAdr;
			const size_t countAnimData = sizeInitialRData / sizeof(*pAnimData);
			for( size_t i = 0; i < countAnimData; i++ )
			{
				frameSet.m_initialRBitpacked.push_back( *pAnimData++ );
			}
		}
		else
		{
			const uint8_t *pAnimData = (const uint8_t *) initialRAdr;
			const size_t countAnimData = sizeInitialRData / (6*sizeof(*pAnimData));
			for( size_t i = 0; i < countAnimData; i++ )
			{
				uint64_t animData = 0;
				animData |= ((uint64_t)*pAnimData++) << 40;
				animData |= ((uint64_t)*pAnimData++) << 32;
				animData |= ((uint64_t)*pAnimData++) << 24;
				animData |= ((uint64_t)*pAnimData++) << 16;
				animData |= ((uint64_t)*pAnimData++) << 8;
				animData |= ((uint64_t)*pAnimData++);
				frameSet.m_initialRSmallest3.push_back( animData );
			}
		}

		if( flags & EDGE_ANIM_FLAG_BIT_PACKED_T )
		{
			const uint8_t *pAnimData = (const uint8_t *) initialTAdr;
			const size_t countAnimData = sizeInitialTData / sizeof(*pAnimData);
			for( size_t i = 0; i < countAnimData; i++ )
			{
				frameSet.m_initialTBitpacked.push_back( *pAnimData++ );
			}
		}
		else
		{
			const float *pAnimData = (const float *) initialTAdr;
			const size_t countAnimData = sizeInitialTData / (3*sizeof(*pAnimData));
			for( size_t i = 0; i < countAnimData; i++ )
			{
				Float4 animData;
				animData.m_data[0] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[1] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[2] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[3] = 1.0f;
				frameSet.m_initialTRaw.push_back( animData );
			}
		}

		if( flags & EDGE_ANIM_FLAG_BIT_PACKED_S )
		{
			const uint8_t *pAnimData = (const uint8_t *) initialSAdr;
			const size_t countAnimData = sizeInitialSData / sizeof(*pAnimData);
			for( size_t i = 0; i < countAnimData; i++ )
			{
				frameSet.m_initialSBitpacked.push_back( *pAnimData++ );
			}
		}
		else
		{
			const float *pAnimData = (const float *) initialSAdr;
			const size_t countAnimData = sizeInitialSData / (3*sizeof(*pAnimData));
			for( size_t i = 0; i < countAnimData; i++ )
			{
				Float4 animData;
				animData.m_data[0] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[1] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[2] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[3] = 1.0f;
				frameSet.m_initialSRaw.push_back( animData );
			}
		}

		if( flags & EDGE_ANIM_FLAG_BIT_PACKED_U )
		{
			const uint8_t *pAnimData = (const uint8_t *) initialUAdr;
			const size_t countAnimData = sizeInitialUData / sizeof(*pAnimData);
			for( size_t i = 0; i < countAnimData; i++ )
			{
				frameSet.m_initialUBitpacked.push_back( *pAnimData++ );
			}
		}
		else
		{
			const float *pAnimData = (const float *) initialUAdr;
			const size_t countAnimData = sizeInitialUData / sizeof(*pAnimData);
			for( size_t i = 0; i < countAnimData; i++ )
			{
				float animData;
				animData = Reverse( *pAnimData++, bigEndian );
				frameSet.m_initialURaw.push_back( animData );
			}
		}

		// intra frame bit stream
		{
			const uint8_t *pAnimData = (const uint8_t *) intraBitsAdr;
			const size_t countAnimData = sizeIntraBits / sizeof(*pAnimData);
			for( size_t i = 0; i < countAnimData; i++ )
			{
				frameSet.m_intraBits.Write( *pAnimData++, 8 );
			}
		}

		// intra joint data
		if( flags & EDGE_ANIM_FLAG_BIT_PACKED_R )
		{
			const uint8_t *pAnimData = (const uint8_t *) intraRAdr;
			const size_t countAnimData = sizeIntraRData / sizeof(*pAnimData);
			for( size_t i = 0; i < countAnimData; i++ )
			{
				frameSet.m_intraRBitpacked.push_back( *pAnimData++ );
			}
		}
		else
		{
			const uint8_t *pAnimData = (const uint8_t *) intraRAdr;
			const size_t countAnimData = sizeIntraRData / (6*sizeof(*pAnimData));
			for( size_t i = 0; i < countAnimData; i++ )
			{
				uint64_t animData = 0;
				animData |= ((uint64_t)*pAnimData++) << 40;
				animData |= ((uint64_t)*pAnimData++) << 32;
				animData |= ((uint64_t)*pAnimData++) << 24;
				animData |= ((uint64_t)*pAnimData++) << 16;
				animData |= ((uint64_t)*pAnimData++) << 8;
				animData |= ((uint64_t)*pAnimData++);
				frameSet.m_intraRSmallest3.push_back( animData );
			}
		}

		if( flags & EDGE_ANIM_FLAG_BIT_PACKED_T )
		{
			const uint8_t *pAnimData = (const uint8_t *) intraTAdr;
			const size_t countAnimData = sizeIntraTData / sizeof(*pAnimData);
			for( size_t i = 0; i < countAnimData; i++ )
			{
				frameSet.m_intraTBitpacked.push_back( *pAnimData++ );
			}
		}
		else
		{
			const float *pAnimData = (const float *) intraTAdr;
			const size_t countAnimData = sizeIntraTData / (3*sizeof(*pAnimData));
			for( size_t i = 0; i < countAnimData; i++ )
			{
				Float4 animData;
				animData.m_data[0] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[1] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[2] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[3] = 1.0f;
				frameSet.m_intraTRaw.push_back( animData );
			}
		}

		if( flags & EDGE_ANIM_FLAG_BIT_PACKED_S )
		{
			const uint8_t *pAnimData = (const uint8_t *) intraSAdr;
			const size_t countAnimData = sizeIntraSData / sizeof(*pAnimData);
			for( size_t i = 0; i < countAnimData; i++ )
			{
				frameSet.m_intraSBitpacked.push_back( *pAnimData++ );
			}
		}
		else
		{
			const float *pAnimData = (const float *) intraSAdr;
			const size_t countAnimData = sizeIntraSData / (3*sizeof(*pAnimData));
			for( size_t i = 0; i < countAnimData; i++ )
			{
				Float4 animData;
				animData.m_data[0] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[1] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[2] = Reverse( *pAnimData++, bigEndian );
				animData.m_data[3] = 1.0f;
				frameSet.m_intraSRaw.push_back( animData );
			}
		}

		if( flags & EDGE_ANIM_FLAG_BIT_PACKED_U )
		{
			const uint8_t *pAnimData = (const uint8_t *) intraUAdr;
			const size_t countAnimData = sizeIntraUData / sizeof(*pAnimData);
			for( size_t i = 0; i < countAnimData; i++ )
			{
				frameSet.m_intraUBitpacked.push_back( *pAnimData++ );
			}
		}
		else
		{
			const float *pAnimData = (const float *) intraUAdr;
			const size_t countAnimData = sizeIntraUData / sizeof(*pAnimData);
			for( size_t i = 0; i < countAnimData; i++ )
			{
				float animData;
				animData = Reverse( *pAnimData++, bigEndian );
				frameSet.m_intraURaw.push_back( animData );
			}
		}

		animation.m_frameSets.push_back( frameSet );
	}
}

//--------------------------------------------------------------------------------------------------
/** 
	Returns the period computed from the input keyTimes. The sampling frequency is then 1/period.
 
		const std::vector<float>& keyTimes	- reference to a std::vector of key times
		const double* pHints				- pointer to an array of period "hints"
		const unsigned int hintCount		- number of elements in the hint array
 
    NOTE: This function assumes everything starts at 0.
 
 	Due to the nature of the floating point representation used, this function is not numerically
 	stable and can produce an incorrect result if the input array contains very large time values.
	Period hints can be passed into this function to help avoid these incorrect results.
 
  	The hint array can be used to pass hintCount number of "period hints". A period hint is given
 	as 1/sampling_rate where typical sampling_rate's are:-
 
 		25, 50, 100, 15, 30, 60, 120
 
 	After computing the period from the input keyTimes, ComputePeriod() will compare each hint
 	against the computed period and selects the best match. The best match is defined as the
	largest period which doesn't cause a greater absolute error than the computed value.
 
	TODO: We need to get the real sampling frequency out of fcollada if possible.
**/
//--------------------------------------------------------------------------------------------------

float ComputePeriod(const std::vector<float>& keyTimes, const double* pHints, const unsigned int hintCount)
{
    double gcd = 1/30.0;    
    double kEpsilon = 1.0 / 1024.0; 
    
    std::vector<float> sortedKeyTimes = keyTimes;
    std::sort(sortedKeyTimes.begin(), sortedKeyTimes.end());
    double startTime = 0.0; 
    if (sortedKeyTimes.size()) {
        startTime = sortedKeyTimes.front();
    }   

    std::vector<float>::const_iterator ite = sortedKeyTimes.begin();
    std::vector<float>::const_iterator iteEnd = sortedKeyTimes.end();
    bool init = false;
    for(; ite != iteEnd; ite++) {
        double val = ((double)*ite) - startTime;
        if (!init) {
            double t0 = *ite - startTime;
            if (t0 > kEpsilon) {
                gcd = t0;
                init = true;
            }
        }
        else {
            while (fabs(val - gcd) > kEpsilon) {
                if (gcd > val) {
                    double n;
                    modf(gcd / val, &n);                    
                    double tmp = gcd - (n - 1) * val;
                    if ((n > 1) && (tmp > val)) {
                        gcd = tmp;
                    }
                    else {
                        gcd -= val;                                     
                    }
                }
                else  {
                    double n;
                    modf(val / gcd, &n);
                    double tmp = val - (n - 1) * gcd;
                    if ((n > 1) && (tmp < gcd)) {
                        val = tmp;
                    }
                    else {
                        val -= gcd;
                    }
                }
            }
        }
    }       
	// Compare the calculated period with any hints.
	double bestResult = gcd;
	if (hintCount)
	{
		// Combine the gcd and hints in a vector.
		std::vector<double> resultCandidates;
		resultCandidates.push_back(gcd);
		for (unsigned int i=0; i<hintCount; ++i)
		{
			resultCandidates.push_back(pHints[i]);
		}
		// Work out the maximum error for the gcd and hints.
		std::vector<double> maxErrors(resultCandidates.size(), 0.0);
		for (std::vector<float>::const_iterator timeIt = keyTimes.begin();
			 timeIt != keyTimes.end();
			 ++timeIt)
		{
			const double timeSeconds = *timeIt;
			for (std::size_t i=0; i<resultCandidates.size(); ++i)
			{
				const double candidate = resultCandidates[i];
				const double timeFrames = timeSeconds / candidate;
				const int nearestFrame = (int)(timeFrames + 0.5);
				const double nearestSeconds = nearestFrame * candidate;
				const double absoluteError = abs(timeSeconds - nearestSeconds);
				if (absoluteError > maxErrors[i])
				{
					maxErrors[i] = absoluteError;
				}
			}
		}
		// Check if any of the hints are as accurate as the gcd and larger (i.e. better).
		for (std::size_t i=1; i<resultCandidates.size(); ++i)
		{
			double candidate = resultCandidates[i];
			double error = maxErrors[i];
			if (error <= maxErrors[0] && candidate > bestResult)
			{
				bestResult = candidate;
			}
		}
	}

	return (float)bestResult;
}

//--------------------------------------------------------------------------------------------------
/**
    Generate animation representing additive delta from baseAnimation to animation. 
**/
//--------------------------------------------------------------------------------------------------

void    GenerateAdditiveAnimation(Animation& outAnimation, const Animation& baseAnimation, const Animation& animation, const Skeleton& bindSkeleton, bool baseUsesFirstFrame)
{
    // We work on a copy of our main animation..
    outAnimation = animation;

    for (unsigned int jointLoop = 0; jointLoop < outAnimation.m_jointAnimations.size(); jointLoop++) {
        // Keep a pointer to current joint around..
        JointAnimation* pOutJoint = &outAnimation.m_jointAnimations[jointLoop];
        const JointAnimation* pBaseJoint = NULL;

        // Look for the joint in the base animation
        for (unsigned int baseJointLoop = 0; baseJointLoop < baseAnimation.m_jointAnimations.size(); baseJointLoop++) {
            if (baseAnimation.m_jointAnimations[baseJointLoop].m_jointName == pOutJoint->m_jointName) {
                pBaseJoint = &baseAnimation.m_jointAnimations[baseJointLoop];
                break;
            }
        }

        if (pBaseJoint)
        {
            // We found the joint in the base animation, so adjust all R/T/S components so that they
            // contain the deltas from the base animation (at the appropriate sampled times).

            for (unsigned int rotLoop = 0; rotLoop < pOutJoint->m_rotationAnimation.size(); rotLoop++)
            {
                // Get base rotation
                float   baseRotation[4];
                if ( pBaseJoint->m_rotationAnimation.size() > 0 ) {
					float t = baseUsesFirstFrame? 0.0f : std::min( pOutJoint->m_rotationAnimation[rotLoop].m_keyTime, baseAnimation.m_endTime );
                    GetInterpolatedKeyframeData(baseRotation, pBaseJoint->m_rotationAnimation,
                                                t, baseAnimation.m_period, true);
                }
                else {
                    for ( int d = 0; d < 4; d++ )
                        baseRotation[d] = bindSkeleton.m_basePose[ jointLoop ].m_rotation[d];
                }

                // Compute delta from base->main
                float   conjugateBaseRotation[4];
                float   deltaRotation[4];
                QuatConjugate( conjugateBaseRotation, baseRotation  );
                QuatMultiply( deltaRotation, conjugateBaseRotation, &( pOutJoint->m_rotationAnimation[rotLoop].m_keyData[0] ) );

                // Write delta over output rotation
                for ( int e = 0; e < 4; e++ ) {
                    pOutJoint->m_rotationAnimation[rotLoop].m_keyData[e] = deltaRotation[e];
                }
            }

            for (unsigned int transLoop = 0; transLoop < pOutJoint->m_translationAnimation.size(); transLoop++)
            {
                // Get base translation
                float   baseTranslation[4];

                if ( pBaseJoint->m_translationAnimation.size() > 0 ) {					
					float t = baseUsesFirstFrame? 0.0f : std::min( pOutJoint->m_translationAnimation[transLoop].m_keyTime, baseAnimation.m_endTime );
                    GetInterpolatedKeyframeData(baseTranslation, pBaseJoint->m_translationAnimation,
                                                t, baseAnimation.m_period, false);
                }
                else {
                    for ( int d = 0; d < 4; d++ )
                        baseTranslation[d] = bindSkeleton.m_basePose[ jointLoop ].m_translation[d];
                }

                // Compute delta from base->main, writing directly to output area
                for ( int e = 0; e < 4; e++ ) {
                    pOutJoint->m_translationAnimation[transLoop].m_keyData[e] -= baseTranslation[e];
                }
            }

            for (unsigned int scaleLoop = 0; scaleLoop < pOutJoint->m_scaleAnimation.size(); scaleLoop++)
            {
                // Get base scale
                float   baseScale[4];
        
                if ( pBaseJoint->m_scaleAnimation.size() > 0 ) {
					float t = baseUsesFirstFrame? 0.0f : std::min( pOutJoint->m_scaleAnimation[scaleLoop].m_keyTime, baseAnimation.m_endTime );
                    GetInterpolatedKeyframeData(baseScale, pBaseJoint->m_scaleAnimation,
                                                t, baseAnimation.m_period, false);

                }
                else {
                    for ( int d = 0; d < 4; d++ ) {
                        baseScale[d] = bindSkeleton.m_basePose[ jointLoop ].m_scale[ d ];
                    }
                }

                // Compute delta from base->main
                for ( int e=0; e < 4; e++ )
                    pOutJoint->m_scaleAnimation[scaleLoop].m_keyData[e] *= ( 1.0f / baseScale[e] );
            }
        }
        else {
            // We didn't find the joint, so leave the data alone.. just mark it as having a zero weight.
            pOutJoint->m_jointWeight = 0.0f;
        }
    }  

    // Now the user channels
    for (unsigned int userChannelLoop = 0; userChannelLoop < outAnimation.m_userChannelAnimations.size(); userChannelLoop++) {
        // Keep a pointer to current channel around..
        UserChannelAnimation* pOutChannel = &outAnimation.m_userChannelAnimations[userChannelLoop];
        const UserChannelAnimation* pBaseChannel = NULL;

        // Look for the channel in the base animation
        for (unsigned int baseChannelLoop = 0; baseChannelLoop < baseAnimation.m_userChannelAnimations.size(); baseChannelLoop++) {
            if (baseAnimation.m_userChannelAnimations[baseChannelLoop].m_channelName == pOutChannel->m_channelName) {
                pBaseChannel = &baseAnimation.m_userChannelAnimations[baseChannelLoop];
                break;
            }
        }

        if (pBaseChannel)
        {
            // We found the channel in the base animation, so adjust the channel data to 
            // contain the deltas from the base animation (at the appropriate sampled times).

            for (unsigned int userLoop = 0; userLoop < pOutChannel->m_animation.size(); userLoop++) {
                // Get base user data
                // Note: Unlike joints, we don't store a default value for user channels in the bind skeleton. 
                // Any channels which are undefined in the base pose get a value of 0.0f
                float   baseUser[4] = {0, 0, 0, 0};

                if ( pBaseChannel->m_animation.size() > 0 ) {
					float t = baseUsesFirstFrame? 0.0f : std::min( pOutChannel->m_animation[userLoop].m_keyTime, baseAnimation.m_endTime );
                    GetInterpolatedKeyframeData(baseUser, pBaseChannel->m_animation,
                                                t, baseAnimation.m_period, false);
                }

                // Compute delta from base->main, writing directly to output area
                for ( int e = 0; e < 4; e++ ) {
                    pOutChannel->m_animation[userLoop].m_keyData[e] -= baseUser[e];
                }
            }
        }
        else {
            // We didn't find the channel, so leave the data alone.. just mark it as having a zero weight.
            pOutChannel->m_weight = 0.0f;
        }
    }  

	// Now the locomotion delta
	outAnimation.m_enableLocoDelta = false;
	if( baseAnimation.m_enableLocoDelta ) {
		outAnimation.m_enableLocoDelta = true;

		// Compute delta from base->main
		float   conjugateBaseRotation[4];
		QuatConjugate( conjugateBaseRotation, (const float*) &baseAnimation.m_locoDeltaQuat );
		QuatMultiply( (float*) &outAnimation.m_locoDeltaQuat, conjugateBaseRotation, (const float*) &animation.m_locoDeltaQuat );

		outAnimation.m_locoDeltaTrans[0] = animation.m_locoDeltaTrans[0] - baseAnimation.m_locoDeltaTrans[0];
		outAnimation.m_locoDeltaTrans[1] = animation.m_locoDeltaTrans[1] - baseAnimation.m_locoDeltaTrans[1];
		outAnimation.m_locoDeltaTrans[2] = animation.m_locoDeltaTrans[2] - baseAnimation.m_locoDeltaTrans[2];
		outAnimation.m_locoDeltaTrans[3] = 0.0f;
	}
}

//--------------------------------------------------------------------------------------------------
/**
    Export binary animation file
**/
//--------------------------------------------------------------------------------------------------

void    ExportAnimation(const std::string outputAnimFilename, const Animation& animationIn, 
                        const Skeleton& bindSkeleton, bool bigEndian, bool writeStats,
						const CompressionInfo& compressionInfo, bool optimize, float optimizerTolerance,
						CustomDataCallback customDataCallback, void* customData)
{
	std::ofstream outStream(outputAnimFilename.c_str(), std::ios_base::out|std::ios_base::binary);
	ExportAnimation( outputAnimFilename, &outStream, animationIn, bindSkeleton, bigEndian, writeStats, 
					 compressionInfo, optimize, optimizerTolerance, customDataCallback, customData );
}

//--------------------------------------------------------------------------------------------------
/**
    Export binary animation file
**/
//--------------------------------------------------------------------------------------------------

void    ExportAnimation(const std::string outputAnimFilename, std::ostream* pStream, const Animation& animationIn, 
                        const Skeleton& bindSkeletonIn, bool bigEndian, bool writeStats, 
						const CompressionInfo& compressionInfo, bool optimize, float optimizerTolerance,
						CustomDataCallback customDataCallback, void* customData)
{
	// some compression modes are not supported/valid for certain channels
	if(compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_NONE)
	{
		std::cerr << "COMPRESSION_TYPE_NONE is not a valid compression mode for rotation channels" << std::endl;
		EDGEERROR_F();
	}
	if(	compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_SMALLEST_3 ||
		compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_SMALLEST_3 ||
		compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_SMALLEST_3)
	{
		std::cerr << "COMPRESSION_TYPE_SMALLEST_3 is not a valid compression mode for non-rotation channels" << std::endl;
		EDGEERROR_F();
	}

	EDGEERROR(animationIn.m_jointAnimations.size() == bindSkeletonIn.m_numJoints);
	EDGEERROR(animationIn.m_userChannelAnimations.size() == bindSkeletonIn.m_numUserChannels);

    Animation animation = animationIn;
	Skeleton bindSkeleton = bindSkeletonIn;

	NormaliseSkeletonRotations(bindSkeleton);
	NormaliseAllRotations(animation);

	unsigned int numJoints = (unsigned int) animation.m_jointAnimations.size();
	unsigned int numUserChannels = (unsigned int) animation.m_userChannelAnimations.size();

	std::vector<KeyframePackingSpec> RSpecs;
	std::vector<KeyframePackingSpec> TSpecs;
	std::vector<KeyframePackingSpec> SSpecs;
	std::vector<KeyframePackingSpec> USpecs;
	KeyframePackingSpec constRSpec;
	KeyframePackingSpec constTSpec;
	KeyframePackingSpec constSSpec;
	KeyframePackingSpec constUSpec;

	std::vector<float> tolerancesR(numJoints, compressionInfo.m_defaultToleranceRotation);
	std::vector<float> tolerancesT(numJoints, compressionInfo.m_defaultToleranceTranslation);
	std::vector<float> tolerancesS(numJoints, compressionInfo.m_defaultToleranceScale);
	std::vector<float> tolerancesU(numUserChannels, compressionInfo.m_defaultToleranceUser);

	// per-joint tolerances
	if(compressionInfo.m_jointTolerancesRotation.size() != 0)
	{
		EDGEERROR(compressionInfo.m_jointTolerancesRotation.size() == numJoints);
		tolerancesR = compressionInfo.m_jointTolerancesRotation;
	}
	if(compressionInfo.m_jointTolerancesTranslation.size() != 0)
	{
		EDGEERROR(compressionInfo.m_jointTolerancesTranslation.size() == numJoints);
		tolerancesT = compressionInfo.m_jointTolerancesTranslation;
	}
	if(compressionInfo.m_jointTolerancesScale.size() != 0)
	{
		EDGEERROR(compressionInfo.m_jointTolerancesScale.size() == numJoints);
		tolerancesS = compressionInfo.m_jointTolerancesScale;
	}
	if(compressionInfo.m_userChannelTolerances.size() != 0)
	{
		EDGEERROR(compressionInfo.m_userChannelTolerances.size() == numUserChannels);
		tolerancesU = compressionInfo.m_userChannelTolerances;
	}

	// Validate rotation keyframes
	ValidateRotationKeys(animation, optimizerTolerance);

	// Remove constant channels equal to the base pose value
	OptimizeBasePose(animation, bindSkeleton.m_basePose, optimizerTolerance);

	// Reduce constant channels to a single keyframe
	OptimizeConstantChannels(animation, optimizerTolerance);

	Animation animationCompressed = animation;

	// find best packing specs for bitpacked channels
	// must be called before Optimize()
	if(compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED)
		FindBestPackingSpecRotations(RSpecs, animation, tolerancesR);
	if(compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_BITPACKED)
		FindBestPackingSpecTranslations(TSpecs, animation, tolerancesT);
	if(compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_BITPACKED)
		FindBestPackingSpecScales(SSpecs, animation, tolerancesS);
	if(compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_BITPACKED)
		FindBestPackingSpecUserChannels(USpecs, animation, tolerancesU);

	// Optimise keyframes
	if(optimize)
		Optimize( animation, optimizerTolerance );

	// find best packing specs for constant bitpacked channels
	// must be called after Optimize()
	// the tolerance used is the smallest tolerance found for all constant joints
	float toleranceRConst = FLT_MAX;
	float toleranceTConst = FLT_MAX;
	float toleranceSConst = FLT_MAX;
	float toleranceUConst = FLT_MAX;
	for(unsigned jointLoop=0; jointLoop<numJoints; jointLoop++)
	{
		if(animation.m_jointAnimations[jointLoop].m_rotationAnimation.size() == 1)
			toleranceRConst = std::min(toleranceRConst, tolerancesR[jointLoop]);
		if(animation.m_jointAnimations[jointLoop].m_translationAnimation.size() == 1)
			toleranceTConst = std::min(toleranceTConst, tolerancesT[jointLoop]);
		if(animation.m_jointAnimations[jointLoop].m_scaleAnimation.size() == 1)
			toleranceSConst = std::min(toleranceSConst, tolerancesS[jointLoop]);
	}
	for(unsigned chanLoop=0; chanLoop<numUserChannels; chanLoop++)
	{
		if(animation.m_userChannelAnimations[chanLoop].m_animation.size() == 1)
			toleranceUConst = std::min(toleranceUConst, tolerancesU[chanLoop]);
	}
	if(compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED)
		FindBestPackingSpecConstRotations(constRSpec, animation, toleranceRConst);
	if(compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_BITPACKED)
		FindBestPackingSpecConstTranslations(constTSpec, animation, toleranceTConst);
	if(compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_BITPACKED)
		FindBestPackingSpecConstScales(constSSpec, animation, toleranceSConst);
	if(compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_BITPACKED)
		FindBestPackingSpecConstUserChannels(constUSpec, animation, toleranceUConst);

	// Generate the framesets
	unsigned int evalBufferSize = EDGE_ANIM_EVAL_BUFFER_SIZE;
	std::vector<FrameSet> frameSets;
	GenerateFramesets(frameSets, animation, evalBufferSize, compressionInfo, 
					  constRSpec, constTSpec, constSSpec, constUSpec, RSpecs, TSpecs, SSpecs, USpecs);

    float period = animation.m_period;
    float frequency = 1.0f / period;
    float duration = animation.m_endTime - animation.m_startTime;
    unsigned int numFrames = (unsigned int) animation.m_numFrames;
    unsigned int numFrameSets = (unsigned int) frameSets.size();

    // Count number of constant/animated RTS channels
    unsigned int numConstRChannels = 0;
    unsigned int numConstTChannels = 0;
    unsigned int numConstSChannels = 0;
    unsigned int numAnimRChannels = 0;
    unsigned int numAnimTChannels = 0;
    unsigned int numAnimSChannels = 0;

    for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
        const std::vector<AnimationKeyframe>& rotations = animation.m_jointAnimations[jointLoop].m_rotationAnimation;
        const std::vector<AnimationKeyframe>& translations = animation.m_jointAnimations[jointLoop].m_translationAnimation;
        const std::vector<AnimationKeyframe>& scales = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

        if(rotations.size() > 1)
            numAnimRChannels++;
        else if(rotations.size() == 1)
            numConstRChannels++;

        if(translations.size() > 1)
            numAnimTChannels++;
        else if(translations.size() == 1)
            numConstTChannels++;

        if(scales.size() > 1)
            numAnimSChannels++;
        else if(scales.size() == 1)
            numConstSChannels++;
    }

    // Count number of constant/animated user channels
    unsigned int numConstUserChannels = 0;
    unsigned int numAnimUserChannels = 0;

    for(unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++) {
        const std::vector<AnimationKeyframe>& userData = animation.m_userChannelAnimations[userChannelLoop].m_animation;

        if(userData.size() > 1)
            numAnimUserChannels++;
        else if(userData.size() == 1)
            numConstUserChannels++;
    }

    if((numAnimRChannels == 0) && (numConstRChannels == 0) && (numAnimTChannels == 0) && (numConstTChannels == 0) && 
       (numAnimSChannels == 0) && (numConstSChannels == 0) && (numConstUserChannels == 0) && (numAnimUserChannels == 0)) {
	    std::cout << "Warning: no channels found" << std::endl;
    }

	uint16_t flags = 0;
	if(compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED)
		flags |= EDGE_ANIM_FLAG_BIT_PACKED_R;
	if(compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_BITPACKED)
		flags |= EDGE_ANIM_FLAG_BIT_PACKED_T;
	if(compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_BITPACKED)
		flags |= EDGE_ANIM_FLAG_BIT_PACKED_S;
	if(compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_BITPACKED)
		flags |= EDGE_ANIM_FLAG_BIT_PACKED_U;

	AnimStats stats;
	memset(&stats, 0, sizeof(AnimStats));

	long filePos;

    // binary output : header
	FileWriter fileWriter(pStream, bigEndian);
	filePos = fileWriter.GetPos();
    fileWriter.Label("StartHeader");

    fileWriter.Write(edgeAnimGetAnimTag());
    fileWriter.Write(duration);
    fileWriter.Write(frequency);
    fileWriter.WriteOffset16("EndHeader", "StartHeader");
	fileWriter.Write((unsigned short)numJoints);
    fileWriter.Write((unsigned short)numFrames);
    fileWriter.Write((unsigned short)numFrameSets);
    fileWriter.Write((unsigned short)evalBufferSize);
    fileWriter.Write((unsigned short)numConstRChannels);
    fileWriter.Write((unsigned short)numConstTChannels);
    fileWriter.Write((unsigned short)numConstSChannels);
    fileWriter.Write((unsigned short)numConstUserChannels);
    fileWriter.Write((unsigned short)numAnimRChannels);
    fileWriter.Write((unsigned short)numAnimTChannels);
    fileWriter.Write((unsigned short)numAnimSChannels);
	fileWriter.Write((unsigned short)numAnimUserChannels);
	fileWriter.Write((unsigned short)flags);
	fileWriter.WriteOffset32("WeightsEnd", "WeightsStart");
	fileWriter.Write((unsigned int)0); /* eaUserJointWeightArray */
	fileWriter.Write((unsigned int)0); /* pad1 */
    fileWriter.WriteOffset32("WeightsStart", "StartHeader");
    fileWriter.WriteOffset32("FrameSetDmaEntries");
    fileWriter.WriteOffset32("FrameSetInfo");
    fileWriter.WriteOffset32("ConstantRData");
    fileWriter.WriteOffset32("ConstantTData");
    fileWriter.WriteOffset32("ConstantSData");
    fileWriter.WriteOffset32("ConstantUserData");
	fileWriter.WriteOffset32("PackingSpecs");
	fileWriter.WriteOffset32("CustomData");
	fileWriter.WriteOffset32("EndCustomData", "StartCustomData");
	fileWriter.WriteOffset32("LocomotionDelta");

	stats.headerSize += fileWriter.GetPos() - filePos;

    // binary output : channel tables
    // Note: due to simd processing, the padding entries at the end of each table are filled with dummy indexes
    // ('numJoints' and 'numUserChannels' for joint and user channels respectively).
    // Currently up to 7 dummy entries are required for constant rotation channels, and 3 for all other channels.
    fileWriter.Align(16);
	filePos = fileWriter.GetPos();
    fileWriter.Label("ChannelIndexTables");
    unsigned int padding;

    // const R table
    for(unsigned int loop = 0; loop < numJoints; loop++) {
        if(animation.m_jointAnimations[loop].m_rotationAnimation.size() == 1)
            fileWriter.Write((unsigned short)loop);
    }
    padding = 7 - ((numConstRChannels + 7) & 7);
    for(unsigned int loop = 0; loop < padding; loop++)
        fileWriter.Write((unsigned short)numJoints);

    // const T table
    for(unsigned int loop = 0; loop < numJoints; loop++) {
        if(animation.m_jointAnimations[loop].m_translationAnimation.size() == 1)
            fileWriter.Write((unsigned short)loop);
    }
    padding = 3 - ((numConstTChannels + 3) & 3);
    for(unsigned int loop = 0; loop < padding; loop++)
        fileWriter.Write((unsigned short)numJoints);

    // const S table
    for(unsigned int loop = 0; loop < numJoints; loop++) {
        if(animation.m_jointAnimations[loop].m_scaleAnimation.size() == 1)
            fileWriter.Write((unsigned short)loop);
    }
    padding = 3 - ((numConstSChannels + 3) & 3);
    for(unsigned int loop = 0; loop < padding; loop++)
        fileWriter.Write((unsigned short)numJoints);

    // const user channel table
    for(unsigned int loop = 0; loop < numUserChannels; loop++) {
        if(animation.m_userChannelAnimations[loop].m_animation.size() == 1)
            fileWriter.Write((unsigned short)loop);
    }
    padding = 3 - ((numConstUserChannels + 3) & 3);
    for(unsigned int loop = 0; loop < padding; loop++)
        fileWriter.Write((unsigned short)numUserChannels);

    // anim R table
    for(unsigned int loop = 0; loop < numJoints; loop++) {
        if(animation.m_jointAnimations[loop].m_rotationAnimation.size() > 1) {
            fileWriter.Write((unsigned short)loop);
        }
    }
    padding = 3 - ((numAnimRChannels + 3) & 3);
    for(unsigned int loop = 0; loop < padding; loop++) {
        fileWriter.Write((unsigned short)numJoints);
    }

    // anim T table
    for(unsigned int loop = 0; loop < numJoints; loop++) {
        if(animation.m_jointAnimations[loop].m_translationAnimation.size() > 1) {
            fileWriter.Write((unsigned short)loop);
        }
    }
    padding = 3 - ((numAnimTChannels + 3) & 3);
    for(unsigned int loop = 0; loop < padding; loop++) {
        fileWriter.Write((unsigned short)numJoints);
    }

    // anim S table
    for(unsigned int loop = 0; loop < numJoints; loop++) {
        if(animation.m_jointAnimations[loop].m_scaleAnimation.size() > 1) {
            fileWriter.Write((unsigned short)loop);
        }
    }
    padding = 3 - ((numAnimSChannels + 3) & 3);
    for(unsigned int loop = 0; loop < padding; loop++) {
        fileWriter.Write((unsigned short)numJoints);
    }

    // anim user channel table
    for(unsigned int loop = 0; loop < numUserChannels; loop++){
        if(animation.m_userChannelAnimations[loop].m_animation.size() > 1) {
            fileWriter.Write((unsigned short)loop);
        }
    }
    padding = 3 - ((numAnimUserChannels + 3) & 3);
    for(unsigned int loop = 0; loop < padding; loop++) {
        fileWriter.Write((unsigned short)numUserChannels);
    }

	stats.tablesSize += fileWriter.GetPos() - filePos;

    // binary output : constant joint rotation data
	if(compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_SMALLEST_3)
		fileWriter.Align(16);
	filePos = fileWriter.GetPos();
    fileWriter.Label("ConstantRData" ); 
    BitStream constRData;
    for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
        const std::vector<AnimationKeyframe>& rotations = animation.m_jointAnimations[jointLoop].m_rotationAnimation;

        // write rotation if channel is constant and non-empty
        if(rotations.size() == 1)
		{
			const float* q = rotations[0].m_keyData.m_data;
			if(compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_SMALLEST_3)
			{
				// smallest 3 compressed
				uint64_t packed = CompressQuat(q);
				constRData.Write(packed, 48);
			}
			else
			{
				// bitpacked
				BitCompressQuat(constRData, q, constRSpec);
			}
        }
    }
    WriteBitStreamToFile(constRData, fileWriter);
	stats.constRSize += fileWriter.GetPos() - filePos;

    // binary output : constant joint translation data
	if(compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_NONE)
	{
		// uncompressed
		fileWriter.Align(16);
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantTData"); 
		for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
			const std::vector<AnimationKeyframe>& translations = animation.m_jointAnimations[jointLoop].m_translationAnimation;

			// write translation if channel is constant and non-empty
			if(translations.size() == 1) {
				for(int eltLoop = 0; eltLoop < 3; eltLoop++) {
					fileWriter.Write(translations[0].m_keyData[eltLoop]);
				}
			}
		}
	}
	else
	{
		// bit packed
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantTData"); 
		BitStream constTData;
		for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
			const std::vector<AnimationKeyframe>& translations = animation.m_jointAnimations[jointLoop].m_translationAnimation;

			// write translation if channel is constant and non-empty
			if(translations.size() == 1) {
				BitCompressVec3(constTData, translations[0].m_keyData.m_data, constTSpec);
			}
		}
		WriteBitStreamToFile(constTData, fileWriter);
	}
	stats.constTSize += fileWriter.GetPos() - filePos;

    // binary output : constant joint scale data
	if(compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_NONE)
	{
		// uncompressed
		fileWriter.Align(16);
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantSData"); 
		for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
			const std::vector<AnimationKeyframe>& scales = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

			// write scale if channel is constant and non-empty
			if(scales.size() == 1) {
				for(int eltLoop = 0; eltLoop < 3; eltLoop++) {
					fileWriter.Write(scales[0].m_keyData[eltLoop]);
				}
			}
		}
	}
	else
	{
		// bit packed
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantSData"); 
		BitStream constSData;
		for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
			const std::vector<AnimationKeyframe>& scales = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

			// write scale if channel is constant and non-empty
			if(scales.size() == 1) {
				BitCompressVec3(constSData, scales[0].m_keyData.m_data, constSSpec);
			}
		}
		WriteBitStreamToFile(constSData, fileWriter);
	}

	stats.constSSize += fileWriter.GetPos() - filePos;

    // binary output : constant user data
	if(compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_NONE)
	{
		// uncompressed
		fileWriter.Align(4);
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantUserData"); 
		for(unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++) {
			const std::vector<AnimationKeyframe>& userData = animation.m_userChannelAnimations[userChannelLoop].m_animation;

			// write user channel if constant and non-empty
			if(userData.size() == 1) {
				fileWriter.Write(userData[0].m_keyData[0]);
			}
		}
	}
	else
	{
		// bit packed
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantUserData"); 
		BitStream constUData;
		for(unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++) {
			const std::vector<AnimationKeyframe>& userData = animation.m_userChannelAnimations[userChannelLoop].m_animation;

			// write user channel if constant and non-empty
			if(userData.size() == 1) {
				BitCompressFloat(constUData, userData[0].m_keyData[0], constUSpec);
			}
		}
		WriteBitStreamToFile(constUData, fileWriter);
	}
	stats.constUSize += fileWriter.GetPos() - filePos;

	// binary output : locomotion delta
	filePos = fileWriter.GetPos();
	if( animation.m_enableLocoDelta ) {
		fileWriter.Align(16);
		fileWriter.Label("LocomotionDelta");
		fileWriter.Write(animation.m_locoDeltaQuat[0]);
		fileWriter.Write(animation.m_locoDeltaQuat[1]);
		fileWriter.Write(animation.m_locoDeltaQuat[2]);
		fileWriter.Write(animation.m_locoDeltaQuat[3]);
		fileWriter.Write(animation.m_locoDeltaTrans[0]);
		fileWriter.Write(animation.m_locoDeltaTrans[1]);
		fileWriter.Write(animation.m_locoDeltaTrans[2]);
		fileWriter.Write(animation.m_locoDeltaTrans[3]);
	}
	stats.locomotionSize += fileWriter.GetPos() - filePos;

	// binary output : packing specs
	if( compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED ||
		compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_BITPACKED ||
		compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_BITPACKED ||
		compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_BITPACKED)
	{
		fileWriter.Align(4);
		filePos = fileWriter.GetPos();
		fileWriter.Label("PackingSpecs"); 

		if(compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED)
		{
			// R specs
			fileWriter.Write(constRSpec.GetPackedSpec());

			for(unsigned i=0; i<numJoints; i++)
			{
				const std::vector<AnimationKeyframe>& rotations = animation.m_jointAnimations[i].m_rotationAnimation;
				if(rotations.size() > 1)
					fileWriter.Write(RSpecs[i].GetPackedSpec());
			}
		}

		if(compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_BITPACKED)
		{
			// T specs
			fileWriter.Write(constTSpec.GetPackedSpec());

			for(unsigned i=0; i<numJoints; i++)
			{
				const std::vector<AnimationKeyframe>& translations = animation.m_jointAnimations[i].m_translationAnimation;
				if(translations.size() > 1)
					fileWriter.Write(TSpecs[i].GetPackedSpec());
			}
		}

		if(compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_BITPACKED)
		{
			// S specs
			fileWriter.Write(constSSpec.GetPackedSpec());

			for(unsigned i=0; i<numJoints; i++)
			{
				const std::vector<AnimationKeyframe>& scales = animation.m_jointAnimations[i].m_scaleAnimation;
				if(scales.size() > 1)
					fileWriter.Write(SSpecs[i].GetPackedSpec());
			}
		}

		if(compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_BITPACKED)
		{
			// U specs
			fileWriter.Write(constUSpec.GetPackedSpec());

			for(unsigned i=0; i<numUserChannels; i++)
			{
				const std::vector<AnimationKeyframe>& userData = animation.m_userChannelAnimations[i].m_animation;
				if(userData.size() > 1)
					fileWriter.Write(USpecs[i].GetPackedSpec());
			}
			stats.specsSize += fileWriter.GetPos() - filePos;
		}
	}

    // binary output : dma entries
    // Each dma encompasses a frameset and the initial pose of the frameset that follows it.
    fileWriter.Align(16);
	filePos = fileWriter.GetPos();
    fileWriter.Label("FrameSetDmaEntries");
    for(unsigned int i = 0; i < numFrameSets; i++) {
        std::string labelStart("FrameSetStart");
        std::string labelEnd("FrameSetInitialPoseEnd");
        std::ostringstream oss1; oss1 << i;
        labelStart = labelStart + oss1.str();
        std::ostringstream oss2; oss2 << ( i + 1 );
        labelEnd = labelEnd + oss2.str();
        fileWriter.Write((unsigned short)0);
        fileWriter.WriteOffset16(labelEnd, labelStart);
        fileWriter.WriteOffset32(labelStart, "StartHeader");      // requires runtime relocation      
    }
	stats.framesetDmaSize += fileWriter.GetPos() - filePos;

    // binary output : frameset info
    fileWriter.Align(4);
	filePos = fileWriter.GetPos();
    fileWriter.Label("FrameSetInfo");
    for(unsigned int i = 0; i < numFrameSets; i++) {
        fileWriter.Write((unsigned short) frameSets[i].m_baseFrame);        
        fileWriter.Write((unsigned short) frameSets[i].m_numIntraFrames);                
    }
	stats.framesetInfoSize += fileWriter.GetPos() - filePos;

    // binary output : end label (to compute total size)
    fileWriter.Align(16);
    fileWriter.Label("EndHeader");

    // binary output : joint weights and user channel weights (write arrays only if all are not 1)    
    bool allOnes = true;
    for (std::vector<JointAnimation>::const_iterator iteJointAnims = animation.m_jointAnimations.begin();
          iteJointAnims != animation.m_jointAnimations.end() && allOnes;
          iteJointAnims++) {
        if (iteJointAnims->m_jointWeight <= 1.0f  - (0.5f / 255.0f)) {
            allOnes = false;
        }
    }   
    for (std::vector<UserChannelAnimation>::const_iterator iteUserChannelAnims = animation.m_userChannelAnimations.begin();
          iteUserChannelAnims != animation.m_userChannelAnimations.end() && allOnes;
          iteUserChannelAnims++) {
        if (iteUserChannelAnims->m_weight <= 1.0f  - (0.5f / 255.0f)) {
            allOnes = false;
        }
    }   

    if (!allOnes) {
        // joint weights
        fileWriter.Align(16);
        fileWriter.Label("WeightsStart"); 

        for (std::vector<JointAnimation>::const_iterator iteJointAnims = animation.m_jointAnimations.begin();
            iteJointAnims != animation.m_jointAnimations.end();
            iteJointAnims++) {
            unsigned char weight;
            if (iteJointAnims->m_jointWeight <= 0.0f) {
                weight = 0x00;
            }
            else if (iteJointAnims->m_jointWeight >= 1.0f) {
                weight = 0xFF;
            }
            else {
                weight = (unsigned char)(iteJointAnims->m_jointWeight * 255.0f);
            }
            fileWriter.Write(weight);
        }

        // user channel weights
        fileWriter.Align(4);
        for (std::vector<UserChannelAnimation>::const_iterator iteUserChannelAnims = animation.m_userChannelAnimations.begin();
            iteUserChannelAnims != animation.m_userChannelAnimations.end();
            iteUserChannelAnims++) {
            unsigned char weight;
            if (iteUserChannelAnims->m_weight <= 0.0f) {
                weight = 0x00;
            }
            else if (iteUserChannelAnims->m_weight >= 1.0f) {
                weight = 0xFF;
            }
            else {
                weight = (unsigned char)(iteUserChannelAnims->m_weight * 255.0f);
            }
            fileWriter.Write(weight);
        }

        fileWriter.Align(16);
        fileWriter.Label("WeightsEnd"); 
    }

    // binary output : add frameset data
    for(unsigned int i = 0; i < numFrameSets; i++) {
        const FrameSet& frameSet = frameSets[i];
        unsigned int numIntraFrames = frameSet.m_numIntraFrames;

        fileWriter.Align(16);

        std::string labelStart("FrameSetStart");
        std::string labelInitialPoseEnd("FrameSetInitialPoseEnd");
        std::string labelEnd("FrameSetEnd");
        std::ostringstream oss; oss << i;
        labelStart = labelStart + oss.str();
        labelInitialPoseEnd = labelInitialPoseEnd + oss.str();
        labelEnd = labelEnd + oss.str();
        fileWriter.Label(labelStart);

		// data sizes
		std::string labelInitialRData("InitialRData");
		std::string labelInitialTData("InitialTData");
		std::string labelInitialSData("InitialSData");
		std::string labelInitialUData("InitialUData");
		std::string labelIntraBitStream("IntraBitStream");
		std::string labelIntraRData("IntraRData");
		std::string labelIntraTData("IntraTData");
		std::string labelIntraSData("IntraSData");
		std::string labelIntraUData("IntraUData");
		labelInitialRData = labelInitialRData + oss.str();
		labelInitialTData = labelInitialTData + oss.str();
		labelInitialSData = labelInitialSData + oss.str();
		labelInitialUData = labelInitialUData + oss.str();
		labelIntraBitStream = labelIntraBitStream + oss.str();
		labelIntraRData = labelIntraRData + oss.str();
		labelIntraTData = labelIntraTData + oss.str();
		labelIntraSData = labelIntraSData + oss.str();
		labelIntraUData = labelIntraUData + oss.str();
		fileWriter.WriteOffset16(labelInitialTData, labelInitialRData);
		fileWriter.WriteOffset16(labelInitialSData, labelInitialTData);
		fileWriter.WriteOffset16(labelInitialUData, labelInitialSData);
		fileWriter.WriteOffset16(labelIntraBitStream, labelInitialUData);
		fileWriter.WriteOffset16(labelIntraTData, labelIntraRData);
		fileWriter.WriteOffset16(labelIntraSData, labelIntraTData);
		fileWriter.WriteOffset16(labelIntraUData, labelIntraSData);
		fileWriter.WriteOffset16(labelEnd, labelIntraUData);

		long filePos = fileWriter.GetPos();

        // initial joint R data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelInitialRData);
        BitStream initialRData;
        for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
            const std::vector<AnimationKeyframe>& rotations = animation.m_jointAnimations[jointLoop].m_rotationAnimation;

            // write initial rotation if channel is non-constant
            if(rotations.size() > 1) {
                const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];
				const float* quat = jointFrameSet.m_initialRData.m_data;
				if(compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED)
				{
					// bit packed
					BitCompressQuat(initialRData, quat, RSpecs[jointLoop]);
				}
				else
				{
					// smallest 3 compressed
	                uint64_t q = CompressQuat(quat);
					initialRData.Write(q, 48);
				}
            }
        }

        WriteBitStreamToFile(initialRData, fileWriter);
		stats.animRSize += fileWriter.GetPos() - filePos;

		// initial joint T data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelInitialTData);
		if(compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_NONE)
		{
			// uncompressed
			for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++)
			{
				const std::vector<AnimationKeyframe>& translations = animation.m_jointAnimations[jointLoop].m_translationAnimation;

				// write initial translation if channel is non-constant
				if(translations.size() > 1) {
					const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];

					for(int eltLoop = 0; eltLoop < 3; eltLoop++) {
						fileWriter.Write(jointFrameSet.m_initialTData[eltLoop]);
					}
				}
			}
		}
		else
		{
			// bit packed
			BitStream initialTData;
			for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++)
			{
				const std::vector<AnimationKeyframe>& translations = animation.m_jointAnimations[jointLoop].m_translationAnimation;

				// write initial translation if channel is non-constant
				if(translations.size() > 1) {
					const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];
					BitCompressVec3(initialTData, jointFrameSet.m_initialTData.m_data, TSpecs[jointLoop]);
				}
			}
			WriteBitStreamToFile(initialTData, fileWriter);
		}
		stats.animTSize += fileWriter.GetPos() - filePos;

        // initial joint S data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelInitialSData);
		if(compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_NONE)
		{
			// uncompressed
			for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++)
			{
				const std::vector<AnimationKeyframe>& scales = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

				// write initial scale if channel is non-constant
				if(scales.size() > 1) {
					const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];

					for(int eltLoop = 0; eltLoop < 3; eltLoop++) {
						fileWriter.Write(jointFrameSet.m_initialSData[eltLoop]);
					}
				}
			}
		}
		else
		{
			// bit packed
			BitStream initialSData;
			for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++)
			{
				const std::vector<AnimationKeyframe>& scales = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

				// write initial scale if channel is non-constant
				if(scales.size() > 1) {
					const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];
					BitCompressVec3(initialSData, jointFrameSet.m_initialSData.m_data, SSpecs[jointLoop]);
				}
			}
			WriteBitStreamToFile(initialSData, fileWriter);
		}
		stats.animSSize += fileWriter.GetPos() - filePos;

        // initial user channel data
		fileWriter.Align(4);
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelInitialUData);
		if(compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_NONE)
		{
			// uncompressed
			for(unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++)
			{
				const std::vector<AnimationKeyframe>& userData = animation.m_userChannelAnimations[userChannelLoop].m_animation;

				// write initial data if channel is non-constant
				if(userData.size() > 1) {
					const UserChannelFrameSet& userChannelFrameSet = frameSet.m_userChannelFrameSets[userChannelLoop];
					fileWriter.Write(userChannelFrameSet.m_initialData);
				}
			}
		}
		else
		{
			// bit packed
			BitStream initialUData;
			for(unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++)
			{
				const std::vector<AnimationKeyframe>& userData = animation.m_userChannelAnimations[userChannelLoop].m_animation;

				// write initial data if channel is non-constant
				if(userData.size() > 1) {
					const UserChannelFrameSet& userChannelFrameSet = frameSet.m_userChannelFrameSets[userChannelLoop];
					BitCompressFloat(initialUData, userChannelFrameSet.m_initialData, USpecs[userChannelLoop]);
				}
			}
			WriteBitStreamToFile(initialUData, fileWriter);
		}
		stats.animUSize += fileWriter.GetPos() - filePos;

        // this is the end of the previous frameset's dma
        // align the label to 16 bytes since it is used to calculate dma size
        fileWriter.Label(labelInitialPoseEnd, 16);

        // intra frame bit stream
        BitStream intraBits;

        filePos = fileWriter.GetPos();

        // intra frame rotations bit stream
        for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
            const std::vector<AnimationKeyframe>& rotations = animation.m_jointAnimations[jointLoop].m_rotationAnimation;

            if(rotations.size() > 1) {
                const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];

                for(unsigned int intraFrameLoop = 0; intraFrameLoop < numIntraFrames; intraFrameLoop++) {
                    if(jointFrameSet.m_hasRFrame[intraFrameLoop]) {
                        intraBits.Write(1, 1);
                    }
                    else {
                        intraBits.Write(0, 1);
                    }
                }
            }
        }

        // intra frame translation bit stream
        for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
            const std::vector<AnimationKeyframe>& translations = animation.m_jointAnimations[jointLoop].m_translationAnimation;

            if(translations.size() > 1) {
                const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];

                for(unsigned int intraFrameLoop = 0; intraFrameLoop < numIntraFrames; intraFrameLoop++) {
                    if(jointFrameSet.m_hasTFrame[intraFrameLoop]) {
                        intraBits.Write(1, 1);
                    }
                    else {
                        intraBits.Write(0, 1);
                    }
                }
            }
        }

        // intra frame scale bit stream
        for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
            const std::vector<AnimationKeyframe>& scales = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

            if(scales.size() > 1) {
                const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];

                for(unsigned int intraFrameLoop = 0; intraFrameLoop < numIntraFrames; intraFrameLoop++) {
                    if(jointFrameSet.m_hasSFrame[intraFrameLoop]) {
                        intraBits.Write(1, 1);
                    }
                    else {
                        intraBits.Write(0, 1);
                    }
                }
            }
        }

        // intra frame user channel bit stream
        for(unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++) {
            const std::vector<AnimationKeyframe>& userData = animation.m_userChannelAnimations[userChannelLoop].m_animation;

            if(userData.size() > 1) {
                const UserChannelFrameSet& userChannelFrameSet = frameSet.m_userChannelFrameSets[userChannelLoop];

                for(unsigned int intraFrameLoop = 0; intraFrameLoop < numIntraFrames; intraFrameLoop++) {
                    if(userChannelFrameSet.m_hasFrame[intraFrameLoop]) {
                        intraBits.Write(1, 1);
                    }
                    else {
                        intraBits.Write(0, 1);
                    }
                }
            }
        }

		fileWriter.Label(labelIntraBitStream);
		WriteBitStreamToFile(intraBits, fileWriter);

        stats.bitStreamSize += fileWriter.GetPos() - filePos;

        // intra frame joint R data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelIntraRData);
        BitStream intraRData;
        for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
            const std::vector<AnimationKeyframe>& rotations = animation.m_jointAnimations[jointLoop].m_rotationAnimation;

            if(rotations.size() > 1) {
                const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];
                unsigned int intraRKeyIdx = jointFrameSet.m_intraRFrame0;

                for(unsigned int intraFrameLoop = 0; intraFrameLoop < numIntraFrames; intraFrameLoop++) {
                    if(jointFrameSet.m_hasRFrame[intraFrameLoop]) {
                        const float* quat = rotations[intraRKeyIdx].m_keyData.m_data;
						if(compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED)
						{
							// bit packed
							BitCompressQuat(intraRData, quat, RSpecs[jointLoop]);
						}
						else
						{
							// smallest 3 compressed
							uint64_t q = CompressQuat(quat);
							intraRData.Write(q, 48);
						}
                        intraRKeyIdx++;
                    }
                }
            }
        }
        WriteBitStreamToFile(intraRData, fileWriter);
		stats.animRSize += fileWriter.GetPos() - filePos;

        // intra frame joint T data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelIntraTData);
		if(compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_NONE)
		{
			// uncompressed
			for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
				const std::vector<AnimationKeyframe>& translations = animation.m_jointAnimations[jointLoop].m_translationAnimation;

				if(translations.size() > 1) {
					const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];
					unsigned int intraTKeyIdx = jointFrameSet.m_intraTFrame0;

					for(unsigned int intraFrameLoop = 0; intraFrameLoop < numIntraFrames; intraFrameLoop++) {

						if(jointFrameSet.m_hasTFrame[intraFrameLoop]) {

							for(int eltLoop = 0; eltLoop < 3; eltLoop++) {
								const float* t = &translations[intraTKeyIdx].m_keyData[eltLoop];
								fileWriter.Write(*t);
							}

							intraTKeyIdx++;
						}
					}
				}
			}
		}
		else
		{
			// bit packed
			BitStream intraTData;
			for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
				const std::vector<AnimationKeyframe>& translations = animation.m_jointAnimations[jointLoop].m_translationAnimation;

				if(translations.size() > 1) {
					const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];
					unsigned int intraTKeyIdx = jointFrameSet.m_intraTFrame0;

					for(unsigned int intraFrameLoop = 0; intraFrameLoop < numIntraFrames; intraFrameLoop++) {

						if(jointFrameSet.m_hasTFrame[intraFrameLoop]) {

							const float* t = translations[intraTKeyIdx].m_keyData.m_data;
							BitCompressVec3(intraTData, t, TSpecs[jointLoop]);

							intraTKeyIdx++;
						}
					}
				}
			}
			WriteBitStreamToFile(intraTData, fileWriter);
		}
		stats.animTSize += fileWriter.GetPos() - filePos;

        // intra frame joint S data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelIntraSData);
		if(compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_NONE)
		{
			// uncompressed
			for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
				const std::vector<AnimationKeyframe>& scales = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

				if(scales.size() > 1) {
					const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];
					unsigned int intraSKeyIdx = jointFrameSet.m_intraSFrame0;

					for(unsigned int intraFrameLoop = 0; intraFrameLoop < numIntraFrames; intraFrameLoop++) {

						if(jointFrameSet.m_hasSFrame[intraFrameLoop]) {

							for(int eltLoop = 0; eltLoop < 3; eltLoop++) {
								const float* s = &scales[intraSKeyIdx].m_keyData[eltLoop];
								fileWriter.Write(*s);
							}

							intraSKeyIdx++;
						}
					}
				}
			}
		}
		else
		{
			// bit packed
			BitStream intraSData;
			for(unsigned int jointLoop = 0; jointLoop < numJoints; jointLoop++) {
				const std::vector<AnimationKeyframe>& scales = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

				if(scales.size() > 1) {
					const JointFrameSet& jointFrameSet = frameSet.m_jointFrameSets[jointLoop];
					unsigned int intraSKeyIdx = jointFrameSet.m_intraSFrame0;

					for(unsigned int intraFrameLoop = 0; intraFrameLoop < numIntraFrames; intraFrameLoop++) {

						if(jointFrameSet.m_hasSFrame[intraFrameLoop]) {

							const float* s = scales[intraSKeyIdx].m_keyData.m_data;
							BitCompressVec3(intraSData, s, SSpecs[jointLoop]);

							intraSKeyIdx++;
						}
					}
				}
			}
			WriteBitStreamToFile(intraSData, fileWriter);
		}
		stats.animSSize += fileWriter.GetPos() - filePos;

        // intra frame user channel data
        fileWriter.Align(4);
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelIntraUData);
		if(compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_NONE)
		{
			// uncompressed
			for(unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++) {
				const std::vector<AnimationKeyframe>& userData = animation.m_userChannelAnimations[userChannelLoop].m_animation;
	    
				if(userData.size() > 1) {
					const UserChannelFrameSet& userChannelFrameSet = frameSet.m_userChannelFrameSets[userChannelLoop];
					unsigned int intraKeyIdx = userChannelFrameSet.m_intraFrame0;

					for(unsigned int intraFrameLoop = 0; intraFrameLoop < numIntraFrames; intraFrameLoop++) {

						if(userChannelFrameSet.m_hasFrame[intraFrameLoop]) {

							fileWriter.Write(userData[intraKeyIdx].m_keyData[0]);

							intraKeyIdx++;
						}
					}
				}
			}
		}
		else
		{
			// bit packed
			BitStream intraUData;
			for(unsigned int userChannelLoop = 0; userChannelLoop < numUserChannels; userChannelLoop++) {
				const std::vector<AnimationKeyframe>& userData = animation.m_userChannelAnimations[userChannelLoop].m_animation;

				if(userData.size() > 1) {
					const UserChannelFrameSet& userChannelFrameSet = frameSet.m_userChannelFrameSets[userChannelLoop];
					unsigned int intraKeyIdx = userChannelFrameSet.m_intraFrame0;

					for(unsigned int intraFrameLoop = 0; intraFrameLoop < numIntraFrames; intraFrameLoop++) {

						if(userChannelFrameSet.m_hasFrame[intraFrameLoop]) {

							BitCompressFloat(intraUData, userData[intraKeyIdx].m_keyData[0], USpecs[userChannelLoop]);

							intraKeyIdx++;
						}
					}
				}
			}
			WriteBitStreamToFile(intraUData, fileWriter);
		}
		stats.animUSize += fileWriter.GetPos() - filePos;

        fileWriter.Align(16);
        fileWriter.Label(labelEnd);
    }

	// binary output : custom data supplied by client code
	filePos = fileWriter.GetPos();
	if ( customDataCallback )
	{
		fileWriter.Align(16);
		fileWriter.Label("StartCustomData");

		fileWriter.Label("CustomData");
		customDataCallback( fileWriter, customData );

		fileWriter.Align(16);
		fileWriter.Label("EndCustomData");
	}
	stats.customDataSize += fileWriter.GetPos() - filePos;

    long fileSize = fileWriter.GetPos();

    // binary output : patch offsets & close the file 
    fileWriter.PatchOffsets();
    fileWriter.Close();

	if(writeStats)
	{
		// write statistics
		std::string statsFilename = outputAnimFilename + ".txt";
		FILE* pStatsFile = fopen(statsFilename.c_str(), "w");
		if (!pStatsFile) {
			std::cerr << "Error: can't open " << statsFilename << "." << std::endl;
			EDGEERROR_F();
		}

		float f = 100.0f / (float)fileSize;
		long paddingSize = fileSize - stats.headerSize - stats.tablesSize - 
						   stats.framesetDmaSize - stats.framesetInfoSize - 
						   stats.constRSize - stats.animRSize - 
						   stats.constTSize - stats.animTSize - 
						   stats.constSSize - stats.animSSize - 
						   stats.constUSize - stats.animUSize -
						   stats.bitStreamSize - stats.specsSize -
						   stats.locomotionSize - stats.customDataSize;

		fprintf(pStatsFile, "File statistics: (total = %d bytes)\n\n", fileSize);
		fprintf(pStatsFile, "Header                               %8d\t       %3.2f%%\n", stats.headerSize, stats.headerSize * f);
		fprintf(pStatsFile, "Channel tables                       %8d\t       %3.2f%%\n", stats.tablesSize, stats.tablesSize * f);
		fprintf(pStatsFile, "Frameset dma array                   %8d\t       %3.2f%%\n", stats.framesetDmaSize, stats.framesetDmaSize * f);
		fprintf(pStatsFile, "Frameset info array                  %8d\t       %3.2f%%\n", stats.framesetInfoSize, stats.framesetInfoSize * f);
		fprintf(pStatsFile, "Rotation const data                  %8d\t       %3.2f%%\n", stats.constRSize, stats.constRSize * f);
		fprintf(pStatsFile, "Rotation anim data                   %8d\t       %3.2f%%\n", stats.animRSize, stats.animRSize * f);
		fprintf(pStatsFile, "Translation const data               %8d\t       %3.2f%%\n", stats.constTSize, stats.constTSize * f);
		fprintf(pStatsFile, "Translation anim data                %8d\t       %3.2f%%\n", stats.animTSize, stats.animTSize * f);
		fprintf(pStatsFile, "Scale const data                     %8d\t       %3.2f%%\n", stats.constSSize, stats.constSSize * f);
		fprintf(pStatsFile, "Scale anim data                      %8d\t       %3.2f%%\n", stats.animSSize, stats.animSSize * f);
		fprintf(pStatsFile, "User channel const data              %8d\t       %3.2f%%\n", stats.constUSize, stats.constUSize * f);
		fprintf(pStatsFile, "User channel anim data               %8d\t       %3.2f%%\n", stats.animUSize, stats.animUSize * f);
		fprintf(pStatsFile, "Intra frame bitstreams               %8d\t       %3.2f%%\n", stats.bitStreamSize, stats.bitStreamSize * f);
		fprintf(pStatsFile, "Packing specs                        %8d\t       %3.2f%%\n", stats.specsSize, stats.specsSize * f);
		fprintf(pStatsFile, "Locomotion Delta                     %8d\t       %3.2f%%\n", stats.locomotionSize, stats.locomotionSize * f);
		fprintf(pStatsFile, "Custom Data                          %8d\t       %3.2f%%\n", stats.customDataSize, stats.customDataSize * f);
		fprintf(pStatsFile, "padding                              %8d\t       %3.2f%%\n", paddingSize, paddingSize * f);

		fprintf(pStatsFile, "\nAnim info:\n\n");
		fprintf(pStatsFile, "Duration                             %3.2fs\n", duration);
		fprintf(pStatsFile, "Sample frequency                     %3.2f\n", frequency);
		fprintf(pStatsFile, "Num frames                           %8d\n", numFrames);
		fprintf(pStatsFile, "Num framesets                        %8d\n", numFrameSets);
		fprintf(pStatsFile, "Num joints                           %8d\n", numJoints);
		fprintf(pStatsFile, "Num constant rotations               %8d\n", numConstRChannels);
		fprintf(pStatsFile, "Num constant translations            %8d\n", numConstTChannels);
		fprintf(pStatsFile, "Num constant scales                  %8d\n", numConstSChannels);
		fprintf(pStatsFile, "Num constant user channels           %8d\n", numConstUserChannels);
		fprintf(pStatsFile, "Num animated rotations               %8d\n", numAnimRChannels);
		fprintf(pStatsFile, "Num animated translations            %8d\n", numAnimTChannels);
		fprintf(pStatsFile, "Num animated scales                  %8d\n", numAnimSChannels);
		fprintf(pStatsFile, "Num animated user channels           %8d\n", numAnimUserChannels);

		fclose(pStatsFile);
	}
}

//--------------------------------------------------------------------------------------------------
/**
    Export binary animation file from compressed animation data
**/
//--------------------------------------------------------------------------------------------------

void    ExportAnimation(const std::string outputAnimFilename, const CompressedAnimation& animation, bool bigEndian, bool writeStats, CustomDataCallback customDataCallback, void* customData )
{
	std::ofstream outStream(outputAnimFilename.c_str(), std::ios_base::out|std::ios_base::binary);
	ExportAnimation( outputAnimFilename, &outStream, animation, bigEndian, writeStats, customDataCallback, customData );
}

//--------------------------------------------------------------------------------------------------

void    ExportAnimation(const std::string outputAnimFilename, std::ostream* pStream, const CompressedAnimation& animation, bool bigEndian, bool writeStats, CustomDataCallback customDataCallback, void* customData )
{
	uint16_t flags = 0;
	if(animation.m_compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED)
		flags |= EDGE_ANIM_FLAG_BIT_PACKED_R;
	if(animation.m_compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_BITPACKED)
		flags |= EDGE_ANIM_FLAG_BIT_PACKED_T;
	if(animation.m_compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_BITPACKED)
		flags |= EDGE_ANIM_FLAG_BIT_PACKED_S;
	if(animation.m_compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_BITPACKED)
		flags |= EDGE_ANIM_FLAG_BIT_PACKED_U;

	AnimStats stats;
	memset(&stats, 0, sizeof(AnimStats));

	long filePos;

	// binary output : header
	FileWriter fileWriter(pStream, bigEndian);
	filePos = fileWriter.GetPos();
	fileWriter.Label("StartHeader");

	fileWriter.Write(edgeAnimGetAnimTag());
	fileWriter.Write(animation.m_duration);
	fileWriter.Write(animation.m_sampleFrequency);
	fileWriter.WriteOffset16("EndHeader", "StartHeader");
	fileWriter.Write((unsigned short)animation.m_numJoints);
	fileWriter.Write((unsigned short)animation.m_numFrames);
	fileWriter.Write((unsigned short)animation.m_numFrameSets);
	fileWriter.Write((unsigned short)animation.m_evalBufferSizeRequired);
	fileWriter.Write((unsigned short)animation.m_numConstRChannels);
	fileWriter.Write((unsigned short)animation.m_numConstTChannels);
	fileWriter.Write((unsigned short)animation.m_numConstSChannels);
	fileWriter.Write((unsigned short)animation.m_numConstUChannels);
	fileWriter.Write((unsigned short)animation.m_numAnimRChannels);
	fileWriter.Write((unsigned short)animation.m_numAnimTChannels);
	fileWriter.Write((unsigned short)animation.m_numAnimSChannels);
	fileWriter.Write((unsigned short)animation.m_numAnimUChannels);
	fileWriter.Write((unsigned short)flags);
	fileWriter.WriteOffset32("WeightsEnd", "WeightsStart");
	fileWriter.Write((unsigned int)0); /* eaUserJointWeightArray */
	fileWriter.Write((unsigned int)0); /* pad1 */
	fileWriter.WriteOffset32("WeightsStart", "StartHeader");
	fileWriter.WriteOffset32("FrameSetDmaEntries");
	fileWriter.WriteOffset32("FrameSetInfo");
	fileWriter.WriteOffset32("ConstantRData");
	fileWriter.WriteOffset32("ConstantTData");
	fileWriter.WriteOffset32("ConstantSData");
	fileWriter.WriteOffset32("ConstantUserData");
	fileWriter.WriteOffset32("PackingSpecs");
	fileWriter.WriteOffset32("CustomData");
	fileWriter.WriteOffset32("EndCustomData", "StartCustomData");
	fileWriter.WriteOffset32("LocomotionDelta");

	stats.headerSize += fileWriter.GetPos() - filePos;

	// binary output : channel tables
	// Note: due to simd processing, the padding entries at the end of each table are filled with dummy indexes
	// ('numJoints' and 'numUserChannels' for joint and user channels respectively).
	// Currently up to 7 dummy entries are required for constant rotation channels, and 3 for all other channels.
	fileWriter.Align(16);
	filePos = fileWriter.GetPos();
	fileWriter.Label("ChannelIndexTables");
	unsigned int padding;

	// const R table
	for(unsigned int loop = 0; loop < animation.m_numConstRChannels; loop++) {
		fileWriter.Write((unsigned short)animation.m_constRChannels[loop]);
	}
	padding = 7 - ((animation.m_numConstRChannels + 7) & 7);
	for(unsigned int loop = 0; loop < padding; loop++) {
		fileWriter.Write((unsigned short)animation.m_numJoints);
	}

	// const T table
	for(unsigned int loop = 0; loop < animation.m_numConstTChannels; loop++) {
		fileWriter.Write((unsigned short)animation.m_constTChannels[loop]);
	}
	padding = 3 - ((animation.m_numConstTChannels + 3) & 3);
	for(unsigned int loop = 0; loop < padding; loop++) {
		fileWriter.Write((unsigned short)animation.m_numJoints);
	}

	// const S table
	for(unsigned int loop = 0; loop < animation.m_numConstSChannels; loop++) {
		fileWriter.Write((unsigned short)animation.m_constSChannels[loop]);
	}
	padding = 3 - ((animation.m_numConstSChannels + 3) & 3);
	for(unsigned int loop = 0; loop < padding; loop++) {
		fileWriter.Write((unsigned short)animation.m_numJoints);
	}

	// const user channel table
	for(unsigned int loop = 0; loop < animation.m_numConstUChannels; loop++) {
		fileWriter.Write((unsigned short)animation.m_constUChannels[loop]);
	}
	padding = 3 - ((animation.m_numConstUChannels + 3) & 3);
	for(unsigned int loop = 0; loop < padding; loop++) {
		fileWriter.Write((unsigned short)animation.m_numJoints);
	}

	// anim R table
	for(unsigned int loop = 0; loop < animation.m_numAnimRChannels; loop++) {
		fileWriter.Write((unsigned short)animation.m_animRChannels[loop]);
	}
	padding = 3 - ((animation.m_numAnimRChannels + 3) & 3);
	for(unsigned int loop = 0; loop < padding; loop++) {
		fileWriter.Write((unsigned short)animation.m_numJoints);
	}

	// anim T table
	for(unsigned int loop = 0; loop < animation.m_numAnimTChannels; loop++) {
		fileWriter.Write((unsigned short)animation.m_animTChannels[loop]);
	}
	padding = 3 - ((animation.m_numAnimTChannels + 3) & 3);
	for(unsigned int loop = 0; loop < padding; loop++) {
		fileWriter.Write((unsigned short)animation.m_numJoints);
	}

	// anim S table
	for(unsigned int loop = 0; loop < animation.m_numAnimSChannels; loop++) {
		fileWriter.Write((unsigned short)animation.m_animSChannels[loop]);
	}
	padding = 3 - ((animation.m_numAnimSChannels + 3) & 3);
	for(unsigned int loop = 0; loop < padding; loop++) {
		fileWriter.Write((unsigned short)animation.m_numJoints);
	}

	// anim user channel table
	for(unsigned int loop = 0; loop < animation.m_numAnimUChannels; loop++) {
		fileWriter.Write((unsigned short)animation.m_animUChannels[loop]);
	}
	padding = 3 - ((animation.m_numAnimUChannels + 3) & 3);
	for(unsigned int loop = 0; loop < padding; loop++) {
		fileWriter.Write((unsigned short)animation.m_numJoints);
	}

	stats.tablesSize += fileWriter.GetPos() - filePos;

	// binary output : constant joint rotation data
	if(animation.m_compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED)
	{
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantRData" ); 
		for( size_t i = 0; i < animation.m_constRBitpacked.size(); i++ ) {
			// bitpacked
			uint8_t packed = animation.m_constRBitpacked[i];
			fileWriter.Write((unsigned char)packed);
		}
	}
	else
	{
		fileWriter.Align(16);
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantRData" ); 
		BitStream constRData;
		for( size_t i = 0; i < animation.m_constRSmallest3.size(); i++ ) {
			// smallest 3 compressed
			uint64_t packed = animation.m_constRSmallest3[i];
			constRData.Write(packed, 48);
		}
		WriteBitStreamToFile(constRData, fileWriter);
	}
	stats.constRSize += fileWriter.GetPos() - filePos;

	// binary output : constant joint translation data
	if(animation.m_compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_BITPACKED)
	{
		// bit packed
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantTData"); 
		for( size_t i = 0; i < animation.m_constTBitpacked.size(); i++ ) {
			// bitpacked
			uint8_t packed = animation.m_constTBitpacked[i];
			fileWriter.Write((unsigned char)packed);
		}
	}
	else
	{
		// uncompressed
		fileWriter.Align(16);
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantTData"); 
		for( size_t i = 0; i < animation.m_constTRaw.size(); i++ ) {
			Float4 t = animation.m_constTRaw[i];
			fileWriter.Write( t.m_data[0] );
			fileWriter.Write( t.m_data[1] );
			fileWriter.Write( t.m_data[2] );
		}
	}
	stats.constTSize += fileWriter.GetPos() - filePos;

	// binary output : constant joint scale data
	if(animation.m_compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_BITPACKED)
	{
		// bit packed
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantSData"); 
		for( size_t i = 0; i < animation.m_constSBitpacked.size(); i++ ) {
			// bitpacked
			uint8_t packed = animation.m_constSBitpacked[i];
			fileWriter.Write((unsigned char)packed);
		}
	}
	else
	{
		// uncompressed
		fileWriter.Align(16);
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantSData"); 
		for( size_t i = 0; i < animation.m_constSRaw.size(); i++ ) {
			Float4 s = animation.m_constSRaw[i];
			fileWriter.Write( s.m_data[0] );
			fileWriter.Write( s.m_data[1] );
			fileWriter.Write( s.m_data[2] );
		}
	}
	stats.constSSize += fileWriter.GetPos() - filePos;

	// binary output : constant user data
	if(animation.m_compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_BITPACKED)
	{
		// bit packed
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantUserData"); 
		for( size_t i = 0; i < animation.m_constUBitpacked.size(); i++ ) {
			// bitpacked
			uint8_t packed = animation.m_constUBitpacked[i];
			fileWriter.Write((unsigned char)packed);
		}
	}
	else
	{
		// uncompressed
		fileWriter.Align(4);
		filePos = fileWriter.GetPos();
		fileWriter.Label("ConstantUserData"); 
		for( size_t i = 0; i < animation.m_constURaw.size(); i++ ) {
			float u = animation.m_constURaw[i];
			fileWriter.Write( u );
		}
	}
	stats.constUSize += fileWriter.GetPos() - filePos;

	// binary output : locomotion delta
	filePos = fileWriter.GetPos();
	if( animation.m_enableLocoDelta ) {
		fileWriter.Align(16);
		fileWriter.Label("LocomotionDelta");
		fileWriter.Write(animation.m_locoDeltaQuat[0]);
		fileWriter.Write(animation.m_locoDeltaQuat[1]);
		fileWriter.Write(animation.m_locoDeltaQuat[2]);
		fileWriter.Write(animation.m_locoDeltaQuat[3]);
		fileWriter.Write(animation.m_locoDeltaTrans[0]);
		fileWriter.Write(animation.m_locoDeltaTrans[1]);
		fileWriter.Write(animation.m_locoDeltaTrans[2]);
		fileWriter.Write(animation.m_locoDeltaTrans[3]);
	}
	stats.locomotionSize += fileWriter.GetPos() - filePos;

	filePos = fileWriter.GetPos();

	// binary output : packing specs
	if( animation.m_compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED ||
		animation.m_compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_BITPACKED ||
		animation.m_compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_BITPACKED ||
		animation.m_compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_BITPACKED)
	{
		fileWriter.Align(4);
		filePos = fileWriter.GetPos();
		fileWriter.Label("PackingSpecs"); 

		if(animation.m_compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED)
		{
			// R specs
			for( size_t i = 0; i < animation.m_packingSpecsR.size(); i++ )
			{
				uint32_t spec = animation.m_packingSpecsR[i].GetPackedSpec();
				fileWriter.Write( spec );
			}
		}

		if(animation.m_compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_BITPACKED)
		{
			// T specs
			for( size_t i = 0; i < animation.m_packingSpecsT.size(); i++ )
			{
				uint32_t spec = animation.m_packingSpecsT[i].GetPackedSpec();
				fileWriter.Write( spec );
			}
		}

		if(animation.m_compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_BITPACKED)
		{
			// S specs
			for( size_t i = 0; i < animation.m_packingSpecsS.size(); i++ )
			{
				uint32_t spec = animation.m_packingSpecsS[i].GetPackedSpec();
				fileWriter.Write( spec );
			}
		}

		if(animation.m_compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_BITPACKED)
		{
			// U specs
			for( size_t i = 0; i < animation.m_packingSpecsU.size(); i++ )
			{
				uint32_t spec = animation.m_packingSpecsU[i].GetPackedSpec();
				fileWriter.Write( spec );
			}
		}
	}
	stats.specsSize += fileWriter.GetPos() - filePos;

	// binary output : dma entries
	// Each dma encompasses a frameset and the initial pose of the frameset that follows it.
	fileWriter.Align(16);
	filePos = fileWriter.GetPos();
	fileWriter.Label("FrameSetDmaEntries");
	for(unsigned int i = 0; i < animation.m_numFrameSets; i++) {
		std::string labelStart("FrameSetStart");
		std::string labelEnd("FrameSetInitialPoseEnd");
		std::ostringstream oss1; oss1 << i;
		labelStart = labelStart + oss1.str();
		std::ostringstream oss2; oss2 << ( i + 1 );
		labelEnd = labelEnd + oss2.str();
		fileWriter.Write((unsigned short)0);
		fileWriter.WriteOffset16(labelEnd, labelStart);
		fileWriter.WriteOffset32(labelStart, "StartHeader");      // requires runtime relocation      
	}
	stats.framesetDmaSize += fileWriter.GetPos() - filePos;

	// binary output : frameset info
	fileWriter.Align(4);
	filePos = fileWriter.GetPos();
	fileWriter.Label("FrameSetInfo");
	for(unsigned int i = 0; i < animation.m_numFrameSets; i++) {
		fileWriter.Write((unsigned short) animation.m_frameSets[i].m_baseFrame);        
		fileWriter.Write((unsigned short) animation.m_frameSets[i].m_numIntraFrames);                
	}
	stats.framesetInfoSize += fileWriter.GetPos() - filePos;

	// binary output : end label (to compute total size)
	fileWriter.Align(16);
	fileWriter.Label("EndHeader");

	// binary output : joint weights and user channel weights (write arrays only if all are not 1)    
	if( animation.m_enableWeights ) {
		// joint weights
		fileWriter.Align(16);
		fileWriter.Label("WeightsStart"); 

		for( size_t i = 0; i < animation.m_jointWeights.size(); i++ ) {
			unsigned char weight = animation.m_jointWeights[i];
			fileWriter.Write(weight);
		}

		// user channel weights
		fileWriter.Align(4);
		for( size_t i = 0; i < animation.m_userChannelWeights.size(); i++ ) {
			unsigned char weight = animation.m_userChannelWeights[i];
			fileWriter.Write(weight);
		}

		fileWriter.Align(16);
		fileWriter.Label("WeightsEnd"); 
	}

	// binary output : add frameset data
	for(unsigned int i = 0; i < animation.m_numFrameSets; i++) {
		const CompressedFrameSet& frameSet = animation.m_frameSets[i];

		fileWriter.Align(16);

		std::string labelStart("FrameSetStart");
		std::string labelInitialPoseEnd("FrameSetInitialPoseEnd");
		std::string labelEnd("FrameSetEnd");
		std::ostringstream oss; oss << i;
		labelStart = labelStart + oss.str();
		labelInitialPoseEnd = labelInitialPoseEnd + oss.str();
		labelEnd = labelEnd + oss.str();
		fileWriter.Label(labelStart);

		// data sizes
		std::string labelInitialRData("InitialRData");
		std::string labelInitialTData("InitialTData");
		std::string labelInitialSData("InitialSData");
		std::string labelInitialUData("InitialUData");
		std::string labelIntraBitStream("IntraBitStream");
		std::string labelIntraRData("IntraRData");
		std::string labelIntraTData("IntraTData");
		std::string labelIntraSData("IntraSData");
		std::string labelIntraUData("IntraUData");
		labelInitialRData = labelInitialRData + oss.str();
		labelInitialTData = labelInitialTData + oss.str();
		labelInitialSData = labelInitialSData + oss.str();
		labelInitialUData = labelInitialUData + oss.str();
		labelIntraBitStream = labelIntraBitStream + oss.str();
		labelIntraRData = labelIntraRData + oss.str();
		labelIntraTData = labelIntraTData + oss.str();
		labelIntraSData = labelIntraSData + oss.str();
		labelIntraUData = labelIntraUData + oss.str();
		fileWriter.WriteOffset16(labelInitialTData, labelInitialRData);
		fileWriter.WriteOffset16(labelInitialSData, labelInitialTData);
		fileWriter.WriteOffset16(labelInitialUData, labelInitialSData);
		fileWriter.WriteOffset16(labelIntraBitStream, labelInitialUData);
		fileWriter.WriteOffset16(labelIntraTData, labelIntraRData);
		fileWriter.WriteOffset16(labelIntraSData, labelIntraTData);
		fileWriter.WriteOffset16(labelIntraUData, labelIntraSData);
		fileWriter.WriteOffset16(labelEnd, labelIntraUData);

		long filePos = fileWriter.GetPos();

		// initial joint R data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelInitialRData);
		if( animation.m_compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED )
		{
			// bit packed
			for( size_t i = 0; i < frameSet.m_initialRBitpacked.size(); i++ ) {
				uint8_t packed = frameSet.m_initialRBitpacked[i];
				fileWriter.Write((unsigned char)packed);
			}
		}
		else
		{
			// smallest 3 compressed
			BitStream initialRData;
			for( size_t i = 0; i < frameSet.m_initialRSmallest3.size(); i++ ) {
				uint64_t packed = frameSet.m_initialRSmallest3[i];
				initialRData.Write(packed, 48);
			}
			WriteBitStreamToFile(initialRData, fileWriter);
		}
		stats.animRSize += fileWriter.GetPos() - filePos;

		// initial joint T data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelInitialTData);
		if( animation.m_compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_BITPACKED )
		{
			// bit packed
			for( size_t i = 0; i < frameSet.m_initialTBitpacked.size(); i++ ) {
				uint8_t packed = frameSet.m_initialTBitpacked[i];
				fileWriter.Write((unsigned char)packed);
			}
		}
		else
		{
			// uncompressed
			for( size_t i = 0; i < frameSet.m_initialTRaw.size(); i++ ) {
				Float4 t = frameSet.m_initialTRaw[i];
				fileWriter.Write( t.m_data[0] );
				fileWriter.Write( t.m_data[1] );
				fileWriter.Write( t.m_data[2] );
			}
		}
		stats.animTSize += fileWriter.GetPos() - filePos;

		// initial joint S data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelInitialSData);
		if( animation.m_compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_BITPACKED )
		{
			// bit packed
			for( size_t i = 0; i < frameSet.m_initialSBitpacked.size(); i++ ) {
				uint8_t packed = frameSet.m_initialSBitpacked[i];
				fileWriter.Write((unsigned char)packed);
			}
		}
		else
		{
			// uncompressed
			for( size_t i = 0; i < frameSet.m_initialSRaw.size(); i++ ) {
				Float4 s = frameSet.m_initialSRaw[i];
				fileWriter.Write( s.m_data[0] );
				fileWriter.Write( s.m_data[1] );
				fileWriter.Write( s.m_data[2] );
			}
		}
		stats.animSSize += fileWriter.GetPos() - filePos;

		// initial user channel data
		fileWriter.Align(4);
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelInitialUData);
		if( animation.m_compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_BITPACKED )
		{
			// bit packed
			for( size_t i = 0; i < frameSet.m_initialUBitpacked.size(); i++ ) {
				uint8_t packed = frameSet.m_initialUBitpacked[i];
				fileWriter.Write((unsigned char)packed);
			}
		}
		else
		{
			// uncompressed
			for( size_t i = 0; i < frameSet.m_initialURaw.size(); i++ ) {
				float u = frameSet.m_initialURaw[i];
				fileWriter.Write( u );
			}
		}
		stats.animUSize += fileWriter.GetPos() - filePos;

		// this is the end of the previous frameset's dma
		// align the label to 16 bytes since it is used to calculate dma size
		fileWriter.Label(labelInitialPoseEnd, 16);

		// intra frame bit stream
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelIntraBitStream);
		BitStream intraBits = frameSet.m_intraBits;
		WriteBitStreamToFile( intraBits, fileWriter );
		stats.bitStreamSize += fileWriter.GetPos() - filePos;

		// intra frame joint R data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelIntraRData);
		if( animation.m_compressionInfo.m_compressionTypeRotation == COMPRESSION_TYPE_BITPACKED )
		{
			for( size_t i = 0; i < frameSet.m_intraRBitpacked.size(); i++ )
			{
				// bit packed
				uint8_t packed = frameSet.m_intraRBitpacked[i];
				fileWriter.Write( packed );
			}
		}
		else
		{
			BitStream intraRData;
			for( size_t i = 0; i < frameSet.m_intraRSmallest3.size(); i++ )
			{
				// smallest 3 compressed
				uint64_t packed = frameSet.m_intraRSmallest3[i];
				intraRData.Write( packed, 48 );
			}
			WriteBitStreamToFile(intraRData, fileWriter);
		}
		stats.animRSize += fileWriter.GetPos() - filePos;

		// intra frame joint T data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelIntraTData);
		if( animation.m_compressionInfo.m_compressionTypeTranslation == COMPRESSION_TYPE_BITPACKED )
		{
			for( size_t i = 0; i < frameSet.m_intraTBitpacked.size(); i++ )
			{
				// bit packed
				uint8_t packed = frameSet.m_intraTBitpacked[i];
				fileWriter.Write( packed );
			}
		}
		else
		{
			for( size_t i = 0; i < frameSet.m_intraTRaw.size(); i++ )
			{
				// uncompressed
				Float4 t = frameSet.m_intraTRaw[i];
				fileWriter.Write( t.m_data[0] );
				fileWriter.Write( t.m_data[1] );
				fileWriter.Write( t.m_data[2] );
			}
		}
		stats.animTSize += fileWriter.GetPos() - filePos;

		// intra frame joint S data
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelIntraSData);
		if( animation.m_compressionInfo.m_compressionTypeScale == COMPRESSION_TYPE_BITPACKED )
		{
			for( size_t i = 0; i < frameSet.m_intraSBitpacked.size(); i++ )
			{
				// bit packed
				uint8_t packed = frameSet.m_intraSBitpacked[i];
				fileWriter.Write( packed );
			}
		}
		else
		{
			for( size_t i = 0; i < frameSet.m_intraSRaw.size(); i++ )
			{
				// uncompressed
				Float4 s = frameSet.m_intraSRaw[i];
				fileWriter.Write( s.m_data[0] );
				fileWriter.Write( s.m_data[1] );
				fileWriter.Write( s.m_data[2] );
			}
		}
		stats.animSSize += fileWriter.GetPos() - filePos;

		// intra frame user channel data
		fileWriter.Align(4);
		filePos = fileWriter.GetPos();
		fileWriter.Label(labelIntraUData);
		if( animation.m_compressionInfo.m_compressionTypeUser == COMPRESSION_TYPE_BITPACKED )
		{
			for( size_t i = 0; i < frameSet.m_intraUBitpacked.size(); i++ )
			{
				// bit packed
				uint8_t packed = frameSet.m_intraUBitpacked[i];
				fileWriter.Write( packed );
			}
		}
		else
		{
			for( size_t i = 0; i < frameSet.m_intraURaw.size(); i++ )
			{
				// uncompressed
				float u = frameSet.m_intraURaw[i];
				fileWriter.Write( u );
			}
		}
		stats.animUSize += fileWriter.GetPos() - filePos;

		fileWriter.Align(16);
		fileWriter.Label(labelEnd);
	}

	// binary output : custom data supplied by client code
	filePos = fileWriter.GetPos();
	if ( customDataCallback )
	{
		fileWriter.Align(16);
		fileWriter.Label("StartCustomData");

		fileWriter.Label("CustomData");
		customDataCallback( fileWriter, customData );

		fileWriter.Align(16);
		fileWriter.Label("EndCustomData");
	}
	stats.customDataSize += fileWriter.GetPos() - filePos;

	long fileSize = fileWriter.GetPos();

	// binary output : patch offsets & close the file 
	fileWriter.PatchOffsets();
	fileWriter.Close();

	if(writeStats)
	{
		// write statistics
		std::string statsFilename = outputAnimFilename + ".txt";
		FILE* pStatsFile = fopen(statsFilename.c_str(), "w");
		if (!pStatsFile) {
			std::cerr << "Error: can't open " << statsFilename << "." << std::endl;
			EDGEERROR_F();
		}

		float f = 100.0f / (float)fileSize;
		long paddingSize = fileSize - stats.headerSize - stats.tablesSize - 
			stats.framesetDmaSize - stats.framesetInfoSize - 
			stats.constRSize - stats.animRSize - 
			stats.constTSize - stats.animTSize - 
			stats.constSSize - stats.animSSize - 
			stats.constUSize - stats.animUSize -
			stats.bitStreamSize - stats.specsSize -
			stats.locomotionSize - stats.customDataSize;

		fprintf(pStatsFile, "File statistics: (total = %d bytes)\n\n", fileSize);
		fprintf(pStatsFile, "Header                               %8d\t       %3.2f%%\n", stats.headerSize, stats.headerSize * f);
		fprintf(pStatsFile, "Channel tables                       %8d\t       %3.2f%%\n", stats.tablesSize, stats.tablesSize * f);
		fprintf(pStatsFile, "Frameset dma array                   %8d\t       %3.2f%%\n", stats.framesetDmaSize, stats.framesetDmaSize * f);
		fprintf(pStatsFile, "Frameset info array                  %8d\t       %3.2f%%\n", stats.framesetInfoSize, stats.framesetInfoSize * f);
		fprintf(pStatsFile, "Rotation const data                  %8d\t       %3.2f%%\n", stats.constRSize, stats.constRSize * f);
		fprintf(pStatsFile, "Rotation anim data                   %8d\t       %3.2f%%\n", stats.animRSize, stats.animRSize * f);
		fprintf(pStatsFile, "Translation const data               %8d\t       %3.2f%%\n", stats.constTSize, stats.constTSize * f);
		fprintf(pStatsFile, "Translation anim data                %8d\t       %3.2f%%\n", stats.animTSize, stats.animTSize * f);
		fprintf(pStatsFile, "Scale const data                     %8d\t       %3.2f%%\n", stats.constSSize, stats.constSSize * f);
		fprintf(pStatsFile, "Scale anim data                      %8d\t       %3.2f%%\n", stats.animSSize, stats.animSSize * f);
		fprintf(pStatsFile, "User channel const data              %8d\t       %3.2f%%\n", stats.constUSize, stats.constUSize * f);
		fprintf(pStatsFile, "User channel anim data               %8d\t       %3.2f%%\n", stats.animUSize, stats.animUSize * f);
		fprintf(pStatsFile, "Intra frame bitstreams               %8d\t       %3.2f%%\n", stats.bitStreamSize, stats.bitStreamSize * f);
		fprintf(pStatsFile, "Packing specs                        %8d\t       %3.2f%%\n", stats.specsSize, stats.specsSize * f);
		fprintf(pStatsFile, "Locomotion Delta                     %8d\t       %3.2f%%\n", stats.locomotionSize, stats.locomotionSize * f);
		fprintf(pStatsFile, "Custom Data                          %8d\t       %3.2f%%\n", stats.customDataSize, stats.customDataSize * f);
		fprintf(pStatsFile, "padding                              %8d\t       %3.2f%%\n", paddingSize, paddingSize * f);

		fprintf(pStatsFile, "\nAnim info:\n\n");
		fprintf(pStatsFile, "Duration                             %3.2fs\n", animation.m_duration);
		fprintf(pStatsFile, "Sample frequency                     %3.2f\n", animation.m_sampleFrequency);
		fprintf(pStatsFile, "Num frames                           %8d\n", animation.m_numFrames);
		fprintf(pStatsFile, "Num framesets                        %8d\n", animation.m_numFrameSets);
		fprintf(pStatsFile, "Num joints                           %8d\n", animation.m_numJoints);
		fprintf(pStatsFile, "Num constant rotations               %8d\n", animation.m_numConstRChannels);
		fprintf(pStatsFile, "Num constant translations            %8d\n", animation.m_numConstTChannels);
		fprintf(pStatsFile, "Num constant scales                  %8d\n", animation.m_numConstSChannels);
		fprintf(pStatsFile, "Num constant user channels           %8d\n", animation.m_numConstUChannels);
		fprintf(pStatsFile, "Num animated rotations               %8d\n", animation.m_numAnimRChannels);
		fprintf(pStatsFile, "Num animated translations            %8d\n", animation.m_numAnimTChannels);
		fprintf(pStatsFile, "Num animated scales                  %8d\n", animation.m_numAnimSChannels);
		fprintf(pStatsFile, "Num animated user channels           %8d\n", animation.m_numAnimUChannels);

		fclose(pStatsFile);
	}
}

//--------------------------------------------------------------------------------------------------

}   // namespace Tools
}   // namespace Edge
