/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef LIBEDGEANIMTOOL_BITPACKING_H
#define LIBEDGEANIMTOOL_BITPACKING_H

#include <assert.h>
#include <math.h>
#include <vector>

#include "edge/libedgeanimtool/libedgeanimtool_common.h"

namespace Edge
{
namespace Tools
{

struct Animation;
struct Skeleton;
struct FrameSet;
class BitStream;

const unsigned kKeyframePackingSpecSizeBits = 32;

//--------------------------------------------------------------------------------------------------

struct ComponentPackingSpec
{
	ComponentPackingSpec() : m_numSignBits(1), m_numExponentBits(8), m_numMantissaBits(23) {};
	ComponentPackingSpec(unsigned s, unsigned e, unsigned m) : m_numSignBits(s), m_numExponentBits(e), m_numMantissaBits(m) {};

	unsigned	m_numSignBits;
	unsigned	m_numExponentBits;
	unsigned	m_numMantissaBits;

	unsigned	GetNumBits() const {return m_numSignBits + m_numExponentBits + m_numMantissaBits;}
	unsigned	Encode(float flt) const;
	float		Decode(unsigned bits) const;
};

struct KeyframePackingSpec
{
	KeyframePackingSpec() : m_recomputeComponentIdx(0) {};
	KeyframePackingSpec( unsigned packedSpec );

	unsigned				m_recomputeComponentIdx;
	ComponentPackingSpec	m_componentSpecs[3];

	unsigned				GetNumBits() const {return m_componentSpecs[0].GetNumBits() + m_componentSpecs[1].GetNumBits() + m_componentSpecs[2].GetNumBits();}
	unsigned				GetPackedSpec() const;
};

//--------------------------------------------------------------------------------------------------

void FindBestPackingSpecRotations(std::vector<KeyframePackingSpec>& animSpecs, Animation& animation, std::vector<float>& tolerances);
void FindBestPackingSpecTranslations(std::vector<KeyframePackingSpec>& animSpecs, Animation& animation, std::vector<float>& tolerances);
void FindBestPackingSpecScales(std::vector<KeyframePackingSpec>& animSpecs, Animation& animation, std::vector<float>& tolerances);
void FindBestPackingSpecUserChannels(std::vector<KeyframePackingSpec>& animSpecs, Animation& animation, std::vector<float>& tolerances);

void FindBestPackingSpecConstRotations(KeyframePackingSpec& spec, Animation& animation, float tolerance);
void FindBestPackingSpecConstTranslations(KeyframePackingSpec& spec, Animation& animation, float tolerance);
void FindBestPackingSpecConstScales(KeyframePackingSpec& spec, Animation& animation, float tolerance);
void FindBestPackingSpecConstUserChannels(KeyframePackingSpec& spec, Animation& animation, float tolerance);

void BitCompressQuat(BitStream& bitStream, const float* q, const KeyframePackingSpec& spec);
void BitCompressVec3(BitStream& bitStream, const float* v, const KeyframePackingSpec& spec);
void BitCompressFloat(BitStream& bitStream, float v, const KeyframePackingSpec& spec);

}
}

#endif // LIBEDGEANIMTOOL_BITPACKING_H
