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
#include "edge/post/edgePost_refc.h"
#include "edge/post/edgePost_framework_spu.h"
#include "PostParams.h"


extern "C"
void edgePostMain( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;
	PostParams* pParams = (PostParams*)tileInfo->parameters;

	EDGE_ASSERT( stage.sources[0].width == stage.sources[1].width);
	EDGE_ASSERT( stage.sources[0].bytesPerPixel == 4);

	const uint16_t numscaline = stage.sources[0].height;
	const uint16_t numpixels = stage.sources[0].width;
	
	// src_address0 is depth buffer
	register uint8_t* src_address0 = tileInfo->tiles[1];
	uint8_t* pOutput = tileInfo->tiles[0];

	// dof parameters
	const float nearFuzzy = pParams->m_nearFuzzy;
	const float nearSharp = pParams->m_nearSharp;
	const float farSharp = pParams->m_farSharp;
	const float farFuzzy = pParams->m_farFuzzy;
	const float maxFuzziness = pParams->m_maxFuzziness;

	// expand linear depth from fp16 to floats
	edgePostFP16LoToFloats( src_address0, src_address0, numpixels * numscaline);

	switch ( stage.user0 )
	{
	case POST_CALC_NEAR_FUZZINESS: // calculate near fuzziness
		edgePostExtractNearFuzziness( pOutput, src_address0, numpixels * numscaline, nearFuzzy, nearSharp, maxFuzziness);
		break;

	case POST_CALC_FAR_FUZZINESS: // calculate far fuzziness
		edgePostExtractFarFuzziness( pOutput, src_address0, tileInfo->tiles[2], numpixels * numscaline, farSharp, farFuzzy, maxFuzziness);
		break;

	default:
		EDGE_ASSERT( 0 && "Unreacheable" );
	}
}
