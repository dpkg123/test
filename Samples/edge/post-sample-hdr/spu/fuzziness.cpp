/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#include <spu_intrinsics.h>
#include <vmx2spu.h>
#include <string.h>
#include "edge/edge_stdint.h"
#include "edge/edge_assert.h"
#include "edge/post/edgePost.h"
#include "edge/post/edgePost_spu.h"
#include "edge/post/edgePost_framework_spu.h"
#include "PostParams.h"


#if 0
/**
	This function prepares the final blend mask to be used when compositing post-processed buffer
	into original high resolution buffer.
	By having a separate image mask we can afford to do subsequent post-processing with a pixel format
	without alpha channel but with more color resolution ( for example Luv ).
 **/
static void MixFuzzinessAndMotionAmount( void* dst, void* fuzz, void* mv, uint32_t count )
{
	// The mix is done by taking the max( fuzziness, motionAmount )
	// motionAmount is the length of the motion vector

	const vec_uchar16 kShuf0 = { 0x80, 0x80, 0x80, 0x00,  0x80, 0x80, 0x80, 0x04,  0x80, 0x80, 0x80, 0x08,  0x80, 0x80, 0x80, 0x0c, };
	const vec_uchar16 kShuf1 = { 0x80, 0x80, 0x80, 0x01,  0x80, 0x80, 0x80, 0x05,  0x80, 0x80, 0x80, 0x09,  0x80, 0x80, 0x80, 0x0d, };

	// process 4 pixels per iteration
	uint32_t count4 = count / 4;
	vec_float4* output = (vec_float4*)dst;
	vec_uint4* fuzziness = (vec_uint4*)fuzz;
	vec_uchar16* motion = (vec_uchar16*)mv;
	for ( uint32_t i = 0; i < count4; ++i )
	{
		// load
		vec_uint4 ifuzz = fuzziness[i];
		vec_uchar16 m = motion[i];

		// expand fuzziness
		vec_float4 f = spu_convtf( spu_rlmask( ifuzz, -16), 16);

		// expand motion vectors
		vec_uint4 imx = (vec_uint4)spu_shuffle( m, m, kShuf0);
		vec_uint4 imy = (vec_uint4)spu_shuffle( m, m, kShuf1);
		vec_float4 fmx = spu_madd( spu_mul( spu_convtf( imx, 8), spu_splats( 256.0f/255.0f)), spu_splats( 2.f), spu_splats( -1.f));
		vec_float4 fmy = spu_madd( spu_mul( spu_convtf( imy, 8), spu_splats( 256.0f/255.0f)), spu_splats( 2.f), spu_splats( -1.f));
		
		// calculate motion amount
		vec_float4 fma = spu_re( spu_rsqrte( spu_madd( fmx, fmx, spu_mul( fmy, fmy))));

		// take the max between motion amount & fuzziness and store into destination buffer
		output[i] = vec_max( f, fma);
	}
}
#else

extern "C" // makeblendmask.spa
void MakeMaskFromFuzzinessAndMotion( void* output, const void* fuzziness, const void* motion, uint32_t count );

#endif

extern "C"
void edgePostMain( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;
	PostParams* pParams = (PostParams*)tileInfo->parameters;
	uint8_t* pOutput = tileInfo->tiles[0];

	EDGE_ASSERT( stage.sources[0].width == stage.sources[1].width);
	EDGE_ASSERT( stage.sources[0].bytesPerPixel == 4);

	const uint16_t numscaline = stage.sources[0].height;
	const uint16_t numpixels = stage.sources[0].width;
	
	// src_address0 is depth buffer
	register uint8_t* src_address0 = tileInfo->tiles[1];

	// dof parameters
	const float nearFuzzy = pParams->m_nearFuzzy;
	const float nearSharp = pParams->m_nearSharp;
	const float farSharp = pParams->m_farSharp;
	const float farFuzzy = pParams->m_farFuzzy;
	const float maxFuzziness = pParams->m_maxFuzziness;

	// expand linear depth from fp16 to floats, place result in the output buffer
	edgePostFP16LoToFloats( pOutput, src_address0, numpixels * numscaline);

	switch ( stage.user0 )
	{
	case POST_CALC_NEAR_FUZZINESS: // calculate near fuzziness ( in place )
		edgePostExtractNearFuzziness( pOutput, pOutput, numpixels * numscaline, nearFuzzy, nearSharp, maxFuzziness);
		break;

	case POST_CALC_FAR_FUZZINESS: // calculate far fuzziness
		edgePostExtractFarFuzziness( pOutput, pOutput, tileInfo->tiles[2], numpixels * numscaline, farSharp, farFuzzy, maxFuzziness, 1.f );

		// also build a blend-mask
#if 1
		MakeMaskFromFuzzinessAndMotion( tileInfo->tiles[3], pOutput, src_address0, numpixels * numscaline );
#else
		MixFuzzinessAndMotionAmount( src_address0, pOutput, src_address0, numpixels * numscaline );
		edgePostMakeMaskFromFloats( tileInfo->tiles[3], src_address0, numpixels * numscaline );
#endif
		break;

	default:
		EDGE_ASSERT( 0 && "Unreacheable" );
	}
}
