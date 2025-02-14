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
	// number of tile pixels
	const EdgePostProcessStage& stage = *tileInfo->stage;
	const uint16_t numscaline = stage.sources[0].height;
	const uint16_t numpixels = stage.sources[0].width;

	uint8_t* pOutput = tileInfo->tiles[0];
	uint8_t* pAccumulator = tileInfo->tiles[1];

	// we simply convert current Luv buffer to a 4xFP6 that can be read by RSX
	// real composition is done by RSX
	edgePostLuvToFX16( pOutput, pAccumulator, numscaline * numpixels );
	edgePostFX16ToFP16( pOutput, pOutput, numscaline * numpixels );
}
