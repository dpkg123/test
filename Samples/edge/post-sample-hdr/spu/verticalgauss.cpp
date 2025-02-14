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
#include "postparams.h"

extern "C"
void Filter1x7_fx16_fx16( void* output, const void* input, uint32_t stride, const vec_float4* weights, uint32_t count );

extern "C"
void edgePostMain( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;

	EDGE_ASSERT( stage.sources[0].width == stage.sources[1].width);
	EDGE_ASSERT( ( stage.sources[0].width % 4) == 0);
	EDGE_ASSERT( stage.sources[1].borderHeight == 3);
	EDGE_ASSERT( stage.sources[1].borderWidth == 0);

	const uint16_t numscaline = stage.sources[0].height;
	const uint16_t numpixels = stage.sources[0].width;
	const uint16_t numsrcscaline = stage.sources[1].height + stage.sources[1].borderHeight * 2;
	const uint16_t numsrcpixels = stage.sources[1].width + stage.sources[1].borderWidth * 2;
	const uint16_t istride = ( stage.sources[1].width + stage.sources[1].borderWidth * 2 ) * stage.sources[1].bytesPerPixel;
	const uint16_t ostride = stage.sources[0].width * stage.sources[0].bytesPerPixel;
	register uint8_t* src_address = tileInfo->tiles[1];
	register uint8_t* dst_address = tileInfo->tiles[0];

	// get filter weights
	vec_float4 weights = { stage.userDataF[0], stage.userDataF[1], stage.userDataF[2], stage.userDataF[3] };
	static vec_float4 weights_splats[7];
	weights_splats[0] = spu_splats( stage.userDataF[3] );
	weights_splats[1] = spu_splats( stage.userDataF[2] );
	weights_splats[2] = spu_splats( stage.userDataF[1] );
	weights_splats[3] = spu_splats( stage.userDataF[0] );
	weights_splats[4] = spu_splats( stage.userDataF[1] );
	weights_splats[5] = spu_splats( stage.userDataF[2] );
	weights_splats[6] = spu_splats( stage.userDataF[3] );

	// This module is called during bloom and during fuzziness calculation
	// handle the two cases separately

	switch ( stage.user0 )
	{
	case POST_GAUSS_FX16:
		// convert Luv to FX16
		src_address -= istride * stage.sources[1].borderHeight + stage.sources[1].borderWidth * 4;
		edgePostLuvToFX16( src_address, src_address, numsrcpixels * numsrcscaline);
		src_address += istride * 2 * stage.sources[1].borderHeight + stage.sources[1].borderWidth * 8;

		// run the filter
		for ( uint32_t j = 0; j < numscaline; ++j, dst_address += ostride * 2, src_address += istride * 2 )
			Filter1x7_fx16_fx16( dst_address, src_address, istride * 2, weights_splats, numpixels );

		// convert back to Luv
		edgePostFX16ToLuv( tileInfo->tiles[0], tileInfo->tiles[0], numpixels * numscaline);
		break;

	case POST_GAUSS_F:
		for ( uint32_t j = 0; j < numscaline; ++j, dst_address += ostride, src_address += istride )
			edgePostGauss1x7F( dst_address, src_address, istride, numpixels, weights );
		break;

	default:
		EDGE_ASSERT( 0 && "Unreacheable" );
	}
}
