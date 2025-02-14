/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2010 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#include <iostream>
#include "edge/anim/edgeanim_structs.h"
#include "edge/anim/edgeanim_common.h"
#include "edge/libedgeanimtool/libedgeanimtool_bitpacking.h"
#include "edge/libedgeanimtool/libedgeanimtool_bitstream.h"
#include "edge/libedgeanimtool/libedgeanimtool_animation.h"
#include "edge/libedgeanimtool/libedgeanimtool_skeleton.h"

namespace Edge
{
namespace Tools
{

//--------------------------------------------------------------------------------------------------
/**
	Encodes an arbitrary precision bitpacked float 
**/
//--------------------------------------------------------------------------------------------------

uint32_t ComponentPackingSpec::Encode(float val) const
{
	uint32_t res = 0;

	// Mask tables ( to avoid microcoded shifts where we can! )
	static const uint32_t maskTable[] = 
	{
		0x00000000, 
		0x00000001, 0x00000003, 0x00000007, 0x0000000F,	
		0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
		0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,	
		0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
		0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,	
		0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
		0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,	
		0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
	};
	// Exponent bias table ( to avoid microcoded shifts where we can! )
	// 128 - ( 1U << ( m_numExponentBits - 1 ) )
	static const int expBiasTable[] =
	{
		128, 127, 126, 124, 120,
		112, 96,  64,  0, -128, -384
	};

	// Floating point ?
	if ( m_numExponentBits ) 
	{
		union
		{
			unsigned int	m_floatBits;
			float			m_floatVal;
		} floatConv;

		floatConv.m_floatVal = val;

		if ( floatConv.m_floatBits == 0 )
			return 0;

		int	         e = m_numExponentBits ? ( ( floatConv.m_floatBits & 0x7f800000 ) >> 23 ) - expBiasTable[ m_numExponentBits ] : 0;
		unsigned int s = m_numSignBits ? ( floatConv.m_floatBits & 0x80000000 ) >> 31 : 0;
		unsigned int m = 0;
		
		if ( m_numMantissaBits > 23 )
		{
			m = ( floatConv.m_floatBits & 0x007fffff ) << ( m_numMantissaBits - 23 );
		}
		else if ( m_numMantissaBits )
		{
			m = ( floatConv.m_floatBits & 0x007fffff ) >> ( 23 - m_numMantissaBits );
		}

		if ( e < 0 )
		{
			e = 0;
			m = 0;
			s = 0;
		}
		else if ( m_numExponentBits && ( e >= ( 1 << m_numExponentBits ) ) )
		{
			e = ( 1 << m_numExponentBits ) - 1;
			m = 0xffffffffU >> ( 32 - m_numMantissaBits);
		}

		s &= m_numSignBits?		maskTable[ m_numSignBits ]		: 0;
		e &= m_numExponentBits?	maskTable[ m_numExponentBits ]	: 0;
		m &= m_numMantissaBits?	maskTable[ m_numMantissaBits ]	: 0;

		res |= s << ( m_numMantissaBits + m_numExponentBits );
		res |= e << m_numMantissaBits;
		res |= m;
	}
	// Fixed point ( normalised )
	else if ( m_numMantissaBits )
	{				
		uint32_t lim = maskTable[ m_numMantissaBits ];
		float flim = ( float ) lim;

		// Signed fixed point
		if ( m_numSignBits )
		{
			float fres = val * flim;
			if ( fres > flim )
				fres = flim;
			else if ( fres < -flim )
				fres = -flim;
			res = ( int32_t ) fres;
		}
		// Unsigned fixed point
		else 
		{
			float fres = val * flim;
			if ( fres > flim )
				fres = flim;
			else if ( fres < 0 )
				fres = 0;
			res = ( uint32_t ) fres;
		}
	}

	return res;
}

//--------------------------------------------------------------------------------------------------
/**
	Decodes an arbitrary precision float 
**/
//--------------------------------------------------------------------------------------------------

float  ComponentPackingSpec::Decode(uint32_t bits) const
{
	float res = 0.0f;

	// Mask tables ( to avoid microcoded shifts where we can! )
	static const uint32_t maskTable[] = 
	{
		0x00000000, 
		0x00000001, 0x00000003, 0x00000007, 0x0000000F,	
		0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
		0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,	
		0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
		0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,	
		0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
		0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,	
		0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
	};
	static const uint32_t signMaskTable[] = 
	{
		0x00000001, 0x00000002, 0x00000004,	0x00000008,
		0x00000010,	0x00000020,	0x00000040,	0x00000080,
		0x00000100,	0x00000200,	0x00000400,	0x00000800,
		0x00001000,	0x00002000,	0x00004000,	0x00008000,
		0x00010000,	0x00020000,	0x00040000,	0x00080000,
		0x00100000,	0x00200000,	0x00400000,	0x00800000,
		0x01000000,	0x02000000,	0x04000000,	0x08000000,
		0x10000000,	0x20000000,	0x40000000,	0x80000000
	};
	static const uint32_t signExtendTable[] = 
	{
		0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFC, 0xFFFFFFF8,
		0xFFFFFFF0, 0xFFFFFFE0, 0xFFFFFFC0, 0xFFFFFF80,
		0xFFFFFF00, 0xFFFFFE00, 0xFFFFFC00, 0xFFFFF800,
		0xFFFFF000, 0xFFFFE000, 0xFFFFC000, 0xFFFF8000,
		0xFFFF0000, 0xFFFE0000, 0xFFFC0000, 0xFFF80000,
		0xFFF00000, 0xFFE00000, 0xFFC00000, 0xFF800000,
		0xFF000000, 0xFE000000, 0xFC000000, 0xF8000000,
		0xF0000000, 0xE0000000, 0xC0000000, 0x80000000,
		0x00000000
	};


	// Exponent bias table ( to avoid microcoded shifts where we can! )
	// 128 - ( 1U << ( m_numExponentBits - 1 ) )
	static const int expBiasTable[] =
	{
		128, 127, 126, 124, 120,
		112, 96,  64,  0, -128, -384
	};

	// Floating point mode 
	if ( m_numExponentBits )
	{
		union
		{
			unsigned int	m_floatBits;
			float			m_floatVal;
		} floatConv;

		unsigned int m = ( bits ) & maskTable[ m_numMantissaBits ];
		int			 e = ( bits >> m_numMantissaBits ) & maskTable[ m_numExponentBits ];
		unsigned int s = ( bits >> ( m_numMantissaBits + m_numExponentBits ) ) & maskTable[ m_numSignBits ];

		if ( m_numExponentBits )
			e += expBiasTable[ m_numExponentBits ];

		if ( m_numMantissaBits <= 23 )
			m <<= ( 23 - m_numMantissaBits );
		else
			m >>= m_numMantissaBits - 23;

		// Clamp exponent - is it really necessary? - warning: SPU side doesn't do this
		if ( e < 0 )
		{
			e = 0;
			m = 0;
			s = 0;
		}
		else if ( e >= ( 1 << 8 ) )
		{
			e = ( 1 << 8 ) - 1;
			m = 0xffffffffU >> ( 32 - 23 );
		}

		floatConv.m_floatBits  = 0;
		floatConv.m_floatBits |= s << 31U;
		floatConv.m_floatBits |= e << 23U;
		floatConv.m_floatBits |= m;

		res = floatConv.m_floatVal;
	} // Fixed point ( normalised )
	else if ( m_numMantissaBits )
	{				
		uint32_t lim = maskTable[ m_numMantissaBits ];
		float flim = ( float ) lim;

		// Signed fixed point
		if ( m_numSignBits )
		{
			int32_t val = ( int32_t ) bits;
			// sign extend?
			if ( val & signMaskTable [  m_numMantissaBits ] )
				val |= signExtendTable[  m_numMantissaBits ];			
			res = val / flim;
		}
		// Unsigned fixed point
		else 
		{
			res = ( ( uint32_t ) bits ) / flim;
		}
	}

	return res;
}

//--------------------------------------------------------------------------------------------------
/**
	Decode packing spec packed to 32 bits
	10 bits per component (1 sign bit, 4 exponent bits, 5 mantissa bits)
	2 lowest bits used for reconstructed component index (quaternions only)
**/
//--------------------------------------------------------------------------------------------------

KeyframePackingSpec::KeyframePackingSpec( unsigned packedSpec )
{
	m_componentSpecs[0] = ComponentPackingSpec( (packedSpec >> 31) & 1, (packedSpec >> 27) & 15, (packedSpec >> 22) & 31 );
	m_componentSpecs[1] = ComponentPackingSpec( (packedSpec >> 21) & 1, (packedSpec >> 17) & 15, (packedSpec >> 12) & 31 );
	m_componentSpecs[2] = ComponentPackingSpec( (packedSpec >> 11) & 1, (packedSpec >>  7) & 15, (packedSpec >>  2) & 31 );
	m_recomputeComponentIdx = packedSpec & 3;
}

//--------------------------------------------------------------------------------------------------
/**
	Get packing spec packed to 32 bits
	10 bits per component (1 sign bit, 4 exponent bits, 5 mantissa bits)
	2 lowest bits used for reconstructed component index (quaternions only)
**/
//--------------------------------------------------------------------------------------------------

unsigned KeyframePackingSpec::GetPackedSpec() const
{
	uint32_t packed = 0;

	for(unsigned i=0; i<3; i++)
	{
		unsigned shift = 2 + (2-i)*10;
		packed |= (m_componentSpecs[i].m_numSignBits << (shift + 9));
		packed |= (m_componentSpecs[i].m_numExponentBits << (shift + 5));
		packed |= (m_componentSpecs[i].m_numMantissaBits << (shift));
	}

	packed |= (m_recomputeComponentIdx & 3);

	return packed;
}

void BitCompressQuat(unsigned* pRes, const float* q, const KeyframePackingSpec& spec)
{
	float qn[4];
	if(q[spec.m_recomputeComponentIdx] >= 0)
	{
		qn[0] = q[0];
		qn[1] = q[1];
		qn[2] = q[2];
		qn[3] = q[3];
	}
	else
	{
		qn[0] = -q[0];
		qn[1] = -q[1];
		qn[2] = -q[2];
		qn[3] = -q[3];
	}
	unsigned c = 0;
	for(unsigned i=0; i<3; i++, c++)
	{
		if(i == spec.m_recomputeComponentIdx)
			c++;
		pRes[i] = spec.m_componentSpecs[i].Encode(qn[c]);
	}
}

void BitCompressQuat(BitStream& bitStream, const float* q, const KeyframePackingSpec& spec)
{
	uint32_t packed[3];
	BitCompressQuat(packed, q, spec);

	for(unsigned i=0; i<3; i++)
		bitStream.Write(packed[i], spec.m_componentSpecs[i].GetNumBits());
}

void BitCompressFloat(BitStream& bitStream, float v, const KeyframePackingSpec& spec)
{
	uint32_t packed = spec.m_componentSpecs[0].Encode(v);
	bitStream.Write(packed, spec.m_componentSpecs[0].GetNumBits());
}

void BitCompressVec3(BitStream& bitStream, const float* v, const KeyframePackingSpec& spec)
{
	uint32_t packed[3];

	for(unsigned i=0; i<3; i++)
		packed[i] = spec.m_componentSpecs[i].Encode(v[i]);

	for(unsigned i=0; i<3; i++)
		bitStream.Write(packed[i], spec.m_componentSpecs[i].GetNumBits());
}

void BitDecompressQuat(float* pRes, unsigned* packed, const KeyframePackingSpec& spec)
{
	float sumSq = 0;
	unsigned c = 0;
	for(unsigned i=0; i<3; i++, c++)
	{
		if(i == spec.m_recomputeComponentIdx)
			c++;
		pRes[c] = spec.m_componentSpecs[i].Decode(packed[i]);
		sumSq += pRes[c] * pRes[c];
	}

	if(sumSq > 1)
		sumSq = 1;

	pRes[spec.m_recomputeComponentIdx] = sqrtf(1 - sumSq);
}

float CheckQuatError(float* q, const KeyframePackingSpec& spec)
{
	unsigned packed[4];
	BitCompressQuat(packed, q, spec);
	Float4 qd;
	BitDecompressQuat(qd.m_data, packed, spec);
	return QuatAbsError(q, qd.m_data);
}

float CheckVec3Error(float* v, const KeyframePackingSpec& spec)
{
	unsigned packed[3];
	Float4 unpacked;

	for(unsigned i=0; i<3; i++)
		packed[i] = spec.m_componentSpecs[i].Encode(v[i]);

	for(unsigned i=0; i<3; i++)
		unpacked[i] = spec.m_componentSpecs[i].Decode(packed[i]);

	unpacked[3] = v[3];

	return VecAbsError(v, unpacked.m_data);
}

bool CheckError(const std::vector<AnimationKeyframe>& channel, const KeyframePackingSpec& spec, float tolerance, bool rotation)
{
	for(unsigned keyLoop=0; keyLoop<channel.size(); keyLoop++)
	{
		Float4 key = channel[keyLoop].m_keyData;
		float err;
		if(rotation)
			err = CheckQuatError(key.m_data, spec);
		else
			err = CheckVec3Error(key.m_data, spec);
		if(err > tolerance)
			return false;
	}
	return true;
}

void FindBestPackingSpec(const std::vector<AnimationKeyframe>& channel, KeyframePackingSpec& spec, float tolerance, bool isRotation)
{
	KeyframePackingSpec bestSpec = spec;

	unsigned maxRecompute = isRotation ? 4 : 1;
	for(unsigned recomputeLoop=0; recomputeLoop<maxRecompute; recomputeLoop++)
	{
		KeyframePackingSpec curSpec = spec;
		curSpec.m_recomputeComponentIdx = isRotation ? recomputeLoop : 3;

		for(int cptLoop=0; cptLoop<3; cptLoop++)
		{
			ComponentPackingSpec& cptSpec = curSpec.m_componentSpecs[cptLoop];

			// remove exponent bits
			while(cptSpec.m_numExponentBits > 0)
			{
				cptSpec.m_numExponentBits--;
				if(!CheckError(channel, curSpec, tolerance, isRotation))
				{
					cptSpec.m_numExponentBits++;
					break;
				}
			}

			// remove mantissa bits
			while(cptSpec.m_numMantissaBits > 0)
			{
				cptSpec.m_numMantissaBits--;
				if(!CheckError(channel, curSpec, tolerance, isRotation))
				{
					cptSpec.m_numMantissaBits++;
					break;
				}
			}

			// remove sign bits
			while(cptSpec.m_numSignBits > 0)
			{
				cptSpec.m_numSignBits--;
				if(!CheckError(channel, curSpec, tolerance, isRotation))
				{
					cptSpec.m_numSignBits++;
					break;
				}
			}

			// try fixed point
			ComponentPackingSpec specBackup = cptSpec;
			unsigned s = cptSpec.m_numSignBits;
			unsigned m = cptSpec.m_numExponentBits + cptSpec.m_numMantissaBits;
			cptSpec = ComponentPackingSpec(s, 0, m);
			while(cptSpec.m_numMantissaBits > 0)
			{
				cptSpec.m_numMantissaBits--;
				if(!CheckError(channel, curSpec, tolerance, isRotation))
				{
					cptSpec.m_numMantissaBits++;
					break;
				}
			}
			if(cptSpec.GetNumBits() >= specBackup.GetNumBits())
				cptSpec = specBackup;

		}

		if(curSpec.GetNumBits() < bestSpec.GetNumBits())
			bestSpec = curSpec;
	}

	// double check
	if(!CheckError(channel, bestSpec, tolerance, isRotation))
	{
		std::cerr << "Error: unable to compress channel, tolerance is too low" << std::endl;
		EDGEERROR_F();
	}

	spec = bestSpec;
}

void FindBestPackingSpecConstRotations(KeyframePackingSpec& spec, Animation& animation, float tolerance)
{
	std::vector<AnimationKeyframe> constRotations;
	for(unsigned jointLoop=0; jointLoop<animation.m_jointAnimations.size(); jointLoop++)
	{
		std::vector<AnimationKeyframe>& channel = animation.m_jointAnimations[jointLoop].m_rotationAnimation;

		if(channel.size() == 1)
			constRotations.push_back(channel[0]);
	}

	FindBestPackingSpec(constRotations, spec, tolerance, true);
}

void FindBestPackingSpecConstTranslations(KeyframePackingSpec& spec, Animation& animation, float tolerance)
{
	std::vector<AnimationKeyframe> constTranslations;
	for(unsigned jointLoop=0; jointLoop<animation.m_jointAnimations.size(); jointLoop++)
	{
		std::vector<AnimationKeyframe>& channel = animation.m_jointAnimations[jointLoop].m_translationAnimation;

		if(channel.size() == 1)
			constTranslations.push_back(channel[0]);
	}

	FindBestPackingSpec(constTranslations, spec, tolerance, false);
}

void FindBestPackingSpecConstScales(KeyframePackingSpec& spec, Animation& animation, float tolerance)
{
	std::vector<AnimationKeyframe> constScales;
	for(unsigned jointLoop=0; jointLoop<animation.m_jointAnimations.size(); jointLoop++)
	{
		std::vector<AnimationKeyframe>& channel = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

		if(channel.size() == 1)
			constScales.push_back(channel[0]);
	}

	FindBestPackingSpec(constScales, spec, tolerance, false);
}

void FindBestPackingSpecConstUserChannels(KeyframePackingSpec& spec, Animation& animation, float tolerance)
{
	std::vector<AnimationKeyframe> constUserChannel;
	for(unsigned jointLoop=0; jointLoop<animation.m_userChannelAnimations.size(); jointLoop++)
	{
		std::vector<AnimationKeyframe>& channel = animation.m_userChannelAnimations[jointLoop].m_animation;

		if(channel.size() == 1)
			constUserChannel.push_back(channel[0]);
	}

	spec.m_componentSpecs[1] = ComponentPackingSpec(0,0,0);
	spec.m_componentSpecs[2] = ComponentPackingSpec(0,0,0);
	FindBestPackingSpec(constUserChannel, spec, tolerance, false);
}

void FindBestPackingSpecRotations(std::vector<KeyframePackingSpec>& animSpecs, Animation& animation, std::vector<float>& tolerances)
{
	// animated rotations
	for(unsigned jointLoop=0; jointLoop<animation.m_jointAnimations.size(); jointLoop++)
	{
		std::vector<AnimationKeyframe>& channel = animation.m_jointAnimations[jointLoop].m_rotationAnimation;

		KeyframePackingSpec spec;
		FindBestPackingSpec(channel, spec, tolerances[jointLoop], true);
		animSpecs.push_back(spec);
	}
}

void FindBestPackingSpecTranslations(std::vector<KeyframePackingSpec>& animSpecs, Animation& animation, std::vector<float>& tolerances)
{
	// animated translations
	for(unsigned jointLoop=0; jointLoop<animation.m_jointAnimations.size(); jointLoop++)
	{
		std::vector<AnimationKeyframe>& channel = animation.m_jointAnimations[jointLoop].m_translationAnimation;

		KeyframePackingSpec spec;
		FindBestPackingSpec(channel, spec, tolerances[jointLoop], false);
		animSpecs.push_back(spec);
	}
}

void FindBestPackingSpecScales(std::vector<KeyframePackingSpec>& animSpecs, Animation& animation, std::vector<float>& tolerances)
{
	// animated translations
	for(unsigned jointLoop=0; jointLoop<animation.m_jointAnimations.size(); jointLoop++)
	{
		std::vector<AnimationKeyframe>& channel = animation.m_jointAnimations[jointLoop].m_scaleAnimation;

		KeyframePackingSpec spec;
		FindBestPackingSpec(channel, spec, tolerances[jointLoop], false);
		animSpecs.push_back(spec);
	}
}

void FindBestPackingSpecUserChannels(std::vector<KeyframePackingSpec>& animSpecs, Animation& animation, std::vector<float>& tolerances)
{
	// animated user channels
	for(unsigned chanLoop=0; chanLoop<animation.m_userChannelAnimations.size(); chanLoop++)
	{
		std::vector<AnimationKeyframe>& channel = animation.m_userChannelAnimations[chanLoop].m_animation;

		KeyframePackingSpec spec;
		spec.m_componentSpecs[1] = ComponentPackingSpec(0,0,0);
		spec.m_componentSpecs[2] = ComponentPackingSpec(0,0,0);
		FindBestPackingSpec(channel, spec, tolerances[chanLoop], false);
		animSpecs.push_back(spec);
	}
}

}
}
