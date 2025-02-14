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

	// double-check tiles have same size
	EDGE_ASSERT( stage.sources[1].width == stage.sources[0].width);
	EDGE_ASSERT( stage.sources[1].height == stage.sources[0].height);

	// get pointers
	const uint16_t numscaline = stage.sources[1].height;
	const uint16_t numpixels = stage.sources[1].width;
	register uint8_t* src_address = tileInfo->tiles[1];
	register uint8_t* dst_address = tileInfo->tiles[0];

	// setup params
	const vec_float4 exposureLevelS = spu_splats( pParams->m_exposureLevel );
	const vec_float4 minLuminanceS = spu_splats( pParams->m_minLuminance );
	const vec_float4 luminanceRangeRcpS = spu_splats( 1.0f / ( pParams->m_maxLuminance - pParams->m_minLuminance ) );

	// convert to FX16
	edgePostLuvToFX16( src_address, src_address, numscaline * numpixels);

	// run bloom capture
	edgePostBloomCaptureFX16( dst_address, src_address, numscaline * numpixels, exposureLevelS, minLuminanceS, luminanceRangeRcpS );

	// convert back to Luv
	edgePostFX16ToLuv( dst_address, dst_address, numscaline * numpixels);
}
