/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#include <spu_intrinsics.h>
#include <string.h>
#include <vmx2spu.h>
#include <cell/atomic.h>
#include "edge/edge_stdint.h"
#include "edge/edge_assert.h"
#include "edge/post/edgePost.h"
#include "edge/post/edgePost_spu.h"
#include "edge/post/edgePost_framework_spu.h"
#include "PostParams.h"

// used for luminance atomic update
static uint64_t s_atomicLine[16] __attribute__(( aligned(128)));

// blurs, generated with edgepostfiltergen
extern "C" void Filter7x1_fx16_fx16( void* output, const void* input, uint32_t stride, const vec_float4* weights, uint32_t count );
extern "C" void Filter1x7_fx16_fx16( void* output, const void* input, uint32_t stride, const vec_float4* weights, uint32_t count );


void BloomStart( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;
	PostParams* pParams = (PostParams*)tileInfo->parameters;

	// metrics
	const uint16_t numdstscaline = stage.sources[0].height;
	const uint16_t numdstpixels = stage.sources[0].width;
	const uint16_t numsrcscaline = stage.sources[1].height;
	const uint16_t numsrcpixels = stage.sources[1].width;
	const uint16_t istride = stage.sources[1].width * stage.sources[1].bytesPerPixel;
	const uint16_t ostride = stage.sources[0].width * stage.sources[0].bytesPerPixel;
	register uint8_t* src_address = tileInfo->tiles[1];
	
	// convert from Fp16Luv to FX16 internal format
	// NOTE: this step might be different depending on input format
	// NOTE: conversion is done in-place
	edgePostFP16LuvToFX16( src_address, src_address, numsrcpixels * numsrcscaline );

	// down-sample into the output tile
	register uint8_t* dst_address = tileInfo->tiles[0];
	for ( uint32_t j = 0; j < numdstscaline; ++j, dst_address += ostride, src_address += istride * 4 )
		edgePostDownsample16( dst_address, src_address, istride * 2, numdstpixels);
	
	// calculate average luminance now
	vec_float4 avgLuminance;
	vec_float4 maxLuminance;
	dst_address = tileInfo->tiles[0];
	edgePostAvgLuminanceFX16( dst_address, numdstscaline * numdstpixels, &avgLuminance, &maxLuminance );

	// avg luminance atomic update loop
	union 
	{	
		uint64_t packed;
		vec_float4 avgAndMaxLuminance;
	};

	do
	{
		packed = cellAtomicLockLine64( (uint64_t*)s_atomicLine, stage.stageParametersEa);
		vec_float4 maxlum = vec_max( avgAndMaxLuminance, maxLuminance);

		// accumulate luminance
		avgAndMaxLuminance = spu_add( avgAndMaxLuminance, avgLuminance);

		// insert max lumianance
		avgAndMaxLuminance = spu_insert( spu_extract( maxlum, 0), avgAndMaxLuminance, 1);
	} while( cellAtomicStoreConditional64( (uint64_t*)s_atomicLine, stage.stageParametersEa, packed ) );

	// setup bloom capture parameters
	const vec_float4 exposureLevelS = spu_splats( pParams->m_exposureLevel );
	const vec_float4 minLuminanceS = spu_splats( pParams->m_minLuminance );
	const vec_float4 luminanceRangeRcpS = spu_splats( 1.0f / ( pParams->m_maxLuminance - pParams->m_minLuminance ) );

	// bloom color capture (in place)
	dst_address = tileInfo->tiles[0];
	edgePostBloomCaptureFX16( dst_address, dst_address, numdstscaline * numdstpixels, exposureLevelS, minLuminanceS, luminanceRangeRcpS );
}

void BloomBlur( EdgePostTileInfo* tileInfo, bool bHorizontal )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;

	// metrics
	const uint16_t numsrcscaline = stage.sources[1].height;
	const uint16_t numsrcpixels = stage.sources[1].width;
	uint16_t istride = ( stage.sources[1].width + stage.sources[1].borderWidth * 2 ) * stage.sources[1].bytesPerPixel;
	uint16_t ostride = stage.sources[0].width * stage.sources[0].bytesPerPixel;

	// get filter weights
	static vec_float4 weights_splats[7];
	weights_splats[0] = spu_splats( stage.userDataF[3] );
	weights_splats[1] = spu_splats( stage.userDataF[2] );
	weights_splats[2] = spu_splats( stage.userDataF[1] );
	weights_splats[3] = spu_splats( stage.userDataF[0] );
	weights_splats[4] = spu_splats( stage.userDataF[1] );
	weights_splats[5] = spu_splats( stage.userDataF[2] );
	weights_splats[6] = spu_splats( stage.userDataF[3] );

	// vertical blur
	register uint8_t* src_address = tileInfo->tiles[1];
	register uint8_t* dst_address = tileInfo->tiles[0];

	if (bHorizontal)
	{
		for ( uint32_t j = 0; j < numsrcscaline; ++j, dst_address += ostride, src_address += istride )
			Filter7x1_fx16_fx16( dst_address, src_address, istride, weights_splats, numsrcpixels );
	} else {
		for ( uint32_t j = 0; j < numsrcscaline; ++j, dst_address += ostride, src_address += istride )
			Filter1x7_fx16_fx16( dst_address, src_address, istride, weights_splats, numsrcpixels );
	}

	// check for a second output
	if (tileInfo->tiles[2] != 0)
	{
		// recalculate image stats
		istride = stage.sources[0].width * stage.sources[0].bytesPerPixel;
		ostride = stage.sources[2].width * stage.sources[2].bytesPerPixel;
		const uint16_t numscaline = stage.sources[2].height;
		const uint16_t numpixels = stage.sources[2].width;

		// down-sample
		src_address = tileInfo->tiles[0];
		dst_address = tileInfo->tiles[2];
		for ( uint32_t j = 0; j < numscaline; ++j, dst_address += ostride, src_address += istride * 2 )
			edgePostDownsample16( dst_address, src_address, istride, numpixels);
	}
}

void BloomUpsampleAndAdd( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;

	const uint16_t numsrcpixels = stage.sources[1].width + stage.sources[1].borderWidth * 2;
	const uint16_t numdstscaline = stage.sources[0].height;
	const uint16_t numdstpixels = stage.sources[0].width;
	const uint16_t istride = numsrcpixels * stage.sources[1].bytesPerPixel;
	const uint16_t ostride = numdstpixels * stage.sources[0].bytesPerPixel;

	// up-sample the first input tile
	register uint8_t* src_address = tileInfo->tiles[1];
	register uint8_t* dst_address = tileInfo->tiles[0];
	for ( uint32_t j = 0; j < (numdstscaline>>1); ++j, dst_address += ostride * 2, src_address += istride)
		edgePostUpsample16( dst_address, src_address, istride, ostride, numdstpixels );

	// if there is a second input than add it to the up-sampled result
	if ( tileInfo->tiles[2] != 0 )
		edgePostAddSatFX16( tileInfo->tiles[0], tileInfo->tiles[0], tileInfo->tiles[2], spu_splats(2048U), numdstpixels * numdstscaline);

	// if requested convert output to FP16 (in place)
	if ( stage.user1 == 1 )
		edgePostFX16ToFP16( tileInfo->tiles[0], tileInfo->tiles[0], numdstscaline * numdstpixels );
}

extern "C"
void edgePostMain( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;
	
	switch ( stage.user0)
	{
	case 1:
		BloomStart(tileInfo);
		break;

	case 2:
		BloomBlur(tileInfo, true);
		break;

	case 3:
		BloomBlur(tileInfo, false);
		break;

	case 4:
		BloomUpsampleAndAdd(tileInfo);
		break;
	}
}
