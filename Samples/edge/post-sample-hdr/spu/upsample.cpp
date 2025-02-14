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
#include "postParams.h"

extern "C"
void edgePostMain( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;
	
	EDGE_ASSERT( stage.sources[0].width == stage.sources[1].width * 2);
	EDGE_ASSERT( ( stage.sources[0].width % 8) == 0 );
	EDGE_ASSERT( stage.sources[1].borderHeight == 1);
	EDGE_ASSERT( stage.sources[1].borderWidth == 4);

	const uint16_t numscaline = stage.sources[0].height;
	const uint16_t numscaline2 = numscaline / 2;
	const uint16_t numsrcscaline = stage.sources[1].height + stage.sources[1].borderHeight * 2;
	const uint16_t numsrcpixels = stage.sources[1].width + stage.sources[1].borderWidth * 2;
	const uint16_t numpixels = stage.sources[0].width;
	const uint16_t istride = ( stage.sources[1].width + stage.sources[1].borderWidth * 2 ) * stage.sources[1].bytesPerPixel;
	const uint16_t ostride = stage.sources[0].width * stage.sources[0].bytesPerPixel;
	
	register uint8_t* src_address = tileInfo->tiles[1];
	register uint8_t* dst_address = tileInfo->tiles[0];

	// Switch on upsample stage type
	switch ( stage.user0 )
	{
	case POST_UPSAMPLE_F:
		for ( uint32_t j = 0; j < numscaline2; ++j, dst_address += ostride * 2, src_address += istride )
			edgePostUpsampleF( dst_address, src_address, istride, ostride, numpixels );
		break;

	case POST_UPSAMPLE_FX16:
		// convert to FX16
		src_address -= istride * stage.sources[1].borderHeight + stage.sources[1].borderWidth * 4;
		edgePostLuvToFX16( src_address, src_address, numsrcpixels * numsrcscaline);
		src_address += istride * 2 * stage.sources[1].borderHeight + stage.sources[1].borderWidth * 8;

		// upsample each scanline
		for ( uint32_t j = 0; j < numscaline2; ++j, dst_address += ostride * 4, src_address += istride * 2)
			edgePostUpsample16( dst_address, src_address, istride * 2, ostride * 2, numpixels );
		
		// convert back to luv
		edgePostFX16ToLuv( tileInfo->tiles[0], tileInfo->tiles[0], numpixels * numscaline);
		break;

	default:
		EDGE_ASSERT( 0 && "Unreacheable" );
	}

	// If we have an odd number of destination rows, we'll have missed one. 
	// HACK: Copy final line from the one before.
	if ( numscaline & 1 )
		edgePostMemCopy( tileInfo->tiles[0] + ( numscaline - 1) * ostride, tileInfo->tiles[0] + ( numscaline - 2) * ostride, ostride );

}
