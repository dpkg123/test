/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#ifndef POST_PARAMS_H
#define POST_PARAMS_H

#include <math.h>

struct PostParams
{
	vec_float4  m_avgAndMaxLuminance;
	vec_float4  m_prevAvgAndMaxLuminance;
	float		m_bloomStrength;
	float		m_exposureLevel;
	float		m_minLuminance;
	float		m_maxLuminance;
	float		m_middleGrey;
	float		m_whitePoint;
	uint32_t	m_numAvgLuminanceTiles;
	float		m_pad;
} __attribute__((__aligned__(16)));


#endif
