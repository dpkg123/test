/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#include <spu_intrinsics.h>
#include <vmx2spu.h>
#include <string.h>
#include <cell/atomic.h>

#include "edge/edge_stdint.h"
#include "edge/edge_assert.h"
#include "edge/post/edgePost.h"
#include "edge/post/edgePost_spu.h"
#include "edge/post/edgePost_framework_spu.h"
#include "PostParams.h"

static uint64_t s_atomicLine[16] __attribute__(( aligned(128)));

extern "C"
void edgePostMain( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;

	// there is only one input buffer
	register uint8_t* src_address = tileInfo->tiles[0];

	// number of tile pixels
	const uint16_t numscaline = stage.sources[0].height;
	const uint16_t numpixels = stage.sources[0].width;
	
	// convert from Luv to FX16
	edgePostLuvToFX16( src_address, src_address, numscaline * numpixels);

	// result goes here
	vec_float4 avgLuminance;
	vec_float4 maxLuminance;
	edgePostAvgLuminanceFX16( src_address, numscaline * numpixels, &avgLuminance, &maxLuminance );
	
	// output address is directly inside the parameter area
	uint32_t avgAndMaxLuminanceEa = stage.stageParametersEa;
	
	union 
	{	
		uint64_t packed;
		vec_float4 avgAndMaxLuminance;
	};

	// avg luminance atomic update loop
	do
	{
		packed = cellAtomicLockLine64( (uint64_t*)s_atomicLine, avgAndMaxLuminanceEa);
		vec_float4 maxlum = vec_max( avgAndMaxLuminance, maxLuminance);
		
		// accumulate luminance
		avgAndMaxLuminance = spu_add( avgAndMaxLuminance, avgLuminance);

		// insert max lumianance
		avgAndMaxLuminance = spu_insert( spu_extract( maxlum, 0), avgAndMaxLuminance, 1);
	} while( cellAtomicStoreConditional64( (uint64_t*)s_atomicLine, avgAndMaxLuminanceEa, packed ) );

}
