/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#ifndef POST_PARAMS_H
#define POST_PARAMS_H

#include <math.h>

//	A set of enum used to identify sub-operation inside the same EdgePost module
#define POST_DOWNSAMPLE_8			0		// down-sample an ARGB image
#define POST_DOWNSAMPLE_F			1		// down-sample a single channel floating point image
#define POST_UPSAMPLE_8				2		// up-sample an ARGB image
#define POST_UPSAMPLE_F				3		// up-sample a single channel floating point image
#define POST_CALC_NEAR_FUZZINESS	4		// do near fuzziness calculation
#define POST_CALC_FAR_FUZZINESS		5		// do far fuzziness calculation
#define POST_GAUSS_8				6		// filter an ARGB image
#define POST_GAUSS_F				7		// filter a single channel floating point image
#define POST_ROP_ADDSAT				8		// add two ARGB images together
#define POST_ROP_PREMULTIPLY_ADDSAT	9		// PreMultiply a source ARGB image and add a second ARGB image on top


// a struct used to share post-processing parameters
struct PostParams
{
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
	float		m_ilrStrenght;
	uint32_t	m_pad[3];
} __attribute__((__aligned__(16)));


#endif
