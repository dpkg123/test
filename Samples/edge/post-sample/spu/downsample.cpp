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
#include "PostParams.h"

extern "C"
void edgePostMain( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;

	EDGE_ASSERT( ( stage.sources[0].width * 2 ) == stage.sources[1].width);
	EDGE_ASSERT( ( stage.sources[1].height & 1) == 0 );
	EDGE_ASSERT( ( stage.sources[1].width % 8) == 0 );

	const uint16_t numscaline = stage.sources[0].height;
	const uint16_t numpixels = stage.sources[0].width;
	const uint16_t istride = stage.sources[1].width * stage.sources[1].bytesPerPixel;
	const uint16_t ostride = stage.sources[0].width * stage.sources[0].bytesPerPixel;
	register uint8_t* src_address = tileInfo->tiles[1];
	register uint8_t* dst_address = tileInfo->tiles[0];

	switch ( stage.user0 )
	{
	case POST_DOWNSAMPLE_8:	// downsample of an argb8 image
		for ( uint32_t j = 0; j < numscaline; ++j, dst_address += ostride, src_address += istride * 2 )
			edgePostDownsample8( dst_address, src_address, istride, numpixels );
		break;

	case POST_DOWNSAMPLE_F:	// downsample a single channel float image
		for ( uint32_t j = 0; j < numscaline; ++j, dst_address += ostride, src_address += istride * 2 )
			edgePostDownsampleF( dst_address, src_address, istride, numpixels );
		break;

	default:
		EDGE_ASSERT( 0 && "Unreacheable" );
	}
}
