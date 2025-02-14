/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#ifndef POST_PARAMS_H
#define POST_PARAMS_H

#include <math.h>

//	A set of enum used to identify sub-operation inside the same EdgePost module
#define POST_DOWNSAMPLE_F_MAX		0		// down-sample a single channel floating point image, take max of each sample
#define POST_DOWNSAMPLE_FX16		1		// down-sample an FX16 image
#define POST_DOWNSAMPLE_LUV			2		// down-sample a Luv image and output an FX16 image
#define POST_UPSAMPLE_FX16			3		// up-sample an FX16 image
#define POST_UPSAMPLE_F				4		// up-sample a single channel floating point image
#define POST_CALC_NEAR_FUZZINESS	5		// do near fuzziness calculation
#define POST_CALC_FAR_FUZZINESS		6		// do far fuzziness calculation
#define POST_GAUSS_FX16				7		// filter an FX16 image
#define POST_GAUSS_F				8		// filter a single channel floating point image
#define POST_ROP_ADDSAT				9		// add two FX16 images together
#define POST_ROP_PREMULTIPLY_ADDSAT	10		// PreMultiply a source FX16 image and add a second FX16 image on top

struct PostParams
{
	vec_float4  m_avgAndMaxLuminance;
	vec_float4  m_prevAvgAndMaxLuminance;
	float		m_bloomStrength;
	float		m_exposureLevel;
	float		m_minLuminance;
	float		m_maxLuminance;
	float		m_nearFuzzy;
	float		m_nearSharp;
	float		m_farSharp;
	float		m_farFuzzy;
	float		m_maxFuzziness;
	float		m_nearPlane;	
	float		m_farPlane;
	float		m_motionScaler;
	float		m_middleGrey;
	float		m_whitePoint;
	uint32_t	m_numAvgLuminanceTiles;
	float		m_ilrStrenght;
} __attribute__((__aligned__(16)));


#endif
