/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#include <spu_intrinsics.h>
#include <string.h>
#include "edge/edge_stdint.h"
#include "edge/edge_assert.h"
#include "edge/post/edgePost.h"
#include "edge/post/edgePost_spu.h"
#include "edge/post/edgePost_framework_spu.h"
#include "PostParams.h"

extern "C"
void edgePostMain( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;
	PostParams* pParams = (PostParams*)tileInfo->parameters;

	EDGE_ASSERT( stage.sources[0].width == stage.sources[1].width);
	EDGE_ASSERT( stage.sources[0].width == stage.sources[2].width);

	const uint16_t numscaline = stage.sources[0].height;
	const uint16_t numpixels = stage.sources[0].width;
	register uint8_t* src_address0 = tileInfo->tiles[1];
	register uint8_t* src_address1 = tileInfo->tiles[2];
	register uint8_t* dst_address = tileInfo->tiles[0];

	// bloom parameter
	float bloomstrenght = pParams->m_bloomStrength * ( pParams->m_maxLuminance - pParams->m_minLuminance ) * pParams->m_exposureLevel;
	vec_float4 bloomstrenghtS = { 0, bloomstrenght, bloomstrenght, bloomstrenght };

	// convert inputs to FX16
	edgePostLuvToFX16( src_address0, src_address0, numpixels * numscaline);
	edgePostLuvToFX16( src_address1, src_address1, numpixels * numscaline);

	switch ( stage.user0 )
	{
	case POST_ROP_ADDSAT:
		
		// add and saturate
		edgePostAddSatFX16( dst_address, src_address0, src_address1, spu_splats( 2048U), numpixels * numscaline);
		break;
	
	case POST_ROP_PREMULTIPLY_ADDSAT:
		// this is used when combining dof/motionblur with bloom

		// first apply the blend mask previously calculated ( premultiply dof/motionblur )
		edgePostApplyMaskFX16( dst_address, src_address0, tileInfo->tiles[3], numscaline * numpixels );

		// add bloom to accumulator
		edgePostAddSatFX16( dst_address, dst_address, src_address1, spu_convtu( bloomstrenghtS, 11), numscaline * numpixels );

		break;

	default:
		EDGE_ASSERT(0);
	}

	// convert back to Luv
	edgePostFX16ToLuv( tileInfo->tiles[0], tileInfo->tiles[0], numpixels * numscaline);

}
