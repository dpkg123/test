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

	switch ( stage.user0 )
	{
	case POST_ROP_ADDSAT:	// used to add bloom layers
		// add and saturate
		edgePostAddSat8( dst_address, src_address0, src_address1, spu_splats( 0xffU), numpixels * numscaline);
		break;

	case POST_ROP_PREMULTIPLY_ADDSAT:	// used to premultiply the accumulator and add bloom
		{
			// premultiply src_address0
			vec_uchar16 shuffle0 = { 0xC0, 1, 2, 3,  0xC0, 5, 6, 7,  0xC0, 9, 10, 11,  0xC0, 13, 14, 15};
			vec_uchar16 shuffle1 = { 0, 0, 0, 0,  4, 4, 4, 4,  8, 8, 8, 8,  12, 12, 12, 12};
			edgePostModulate8( src_address0, src_address0, (qword)shuffle0, src_address0, (qword)shuffle1, numpixels * numscaline );

			// bloom integration
			uint32_t bs = uint32_t( pParams->m_bloomStrength * ( pParams->m_maxLuminance - pParams->m_minLuminance ) * pParams->m_exposureLevel * 255.f );
			vec_uint4 vbs = { 0, bs, bs, bs };
			edgePostAddSat8( dst_address, src_address0, src_address1, vbs, numpixels * numscaline);
		}
		break;

	default:
		EDGE_ASSERT( 0 && "Unreacheable" );
	}

}
