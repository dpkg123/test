/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#include <spu_intrinsics.h>
#include "edge/edge_stdint.h"
#include "edge/edge_assert.h"
#include "edge/post/edgePost.h"
#include "edge/post/edgePost_spu.h"
#include "edge/post/edgePost_framework_spu.h"
#include "postparams.h"

extern "C"
void edgePostMain( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;

	EDGE_ASSERT( ( stage.sources[0].width * 2 ) == stage.sources[1].width);
	EDGE_ASSERT( ( stage.sources[1].height & 1) == 0 );
	EDGE_ASSERT( ( stage.sources[1].width % 8) == 0 );

	const uint16_t numscaline = stage.sources[0].height;
	const uint16_t numpixels = stage.sources[0].width;
	uint32_t numsrcpixels = stage.sources[1].width * stage.sources[1].height;
	uint16_t istride = stage.sources[1].width * stage.sources[1].bytesPerPixel;
	const uint16_t ostride = numpixels * stage.sources[0].bytesPerPixel;
	register uint8_t* src_address = tileInfo->tiles[1];
	register uint8_t* dst_address = tileInfo->tiles[0];

	// Switch on downsample stage type
	switch ( stage.user0 )
	{
	
	// Floating point downsample ( used by near fuzziness calculation )
	case POST_DOWNSAMPLE_F_MAX:		
		for ( uint32_t j = 0; j < numscaline; ++j, dst_address += ostride, src_address += istride * 2 )
			edgePostDownsampleFloatMax( dst_address, src_address, istride, numpixels );

		break;

	// FX16 downsample
	case POST_DOWNSAMPLE_FX16:
		edgePostLuvToFX16( src_address, src_address, numsrcpixels);
		for ( uint32_t j = 0; j < numscaline; ++j, dst_address += ostride * 2, src_address += istride * 4 )
			edgePostDownsample16( dst_address, src_address, istride * 2, numpixels );
		edgePostFX16ToLuv( tileInfo->tiles[0], tileInfo->tiles[0], numpixels * numscaline);
		break;

	// FX16 downsample, but input is in RSX format ( this is the first stage )
	case POST_DOWNSAMPLE_LUV:
		edgePostLogLuvToFX16( src_address, src_address, numsrcpixels);
		for ( uint32_t j = 0; j < numscaline; ++j, dst_address += ostride * 2, src_address += istride * 4 )
			edgePostDownsample16( dst_address, src_address, istride * 2, numpixels );
		edgePostFX16ToLuv( tileInfo->tiles[0], tileInfo->tiles[0], numpixels * numscaline);
		break;

	default:
		EDGE_ASSERT( 0 && "Unreacheable" );
	}

}
