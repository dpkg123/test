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

	// double-check tiles have expected size
	EDGE_ASSERT( stage.sources[2].width == stage.sources[0].width);
	EDGE_ASSERT( stage.sources[2].width == stage.sources[1].width);

	// calculate strides and get pointers
	const uint16_t numscaline = stage.sources[0].height;
	const uint16_t numpixels = stage.sources[0].width;
	const uint16_t numsrcpixels = stage.sources[1].width + stage.sources[1].borderWidth * 2;
	const uint16_t numsrcscanline = stage.sources[1].height + stage.sources[1].borderHeight * 2;
	const uint16_t istride0 = ( stage.sources[1].width + stage.sources[1].borderWidth * 2 ) * stage.sources[1].bytesPerPixel;
	const uint16_t istride1 = ( stage.sources[2].width + stage.sources[2].borderWidth * 2 ) * stage.sources[2].bytesPerPixel;
		
	// src_address0 is frame buffer
	// src_address1 is fuzziness buffer
	register uint8_t* src_address0 = tileInfo->tiles[1];
	register uint8_t* src_address1 = tileInfo->tiles[2];
	register uint8_t* dst_address = tileInfo->tiles[0];

	// An offset that bring us to real tile start ( considering the border )
	uint32_t back_offset0 = stage.sources[1].borderWidth * stage.sources[1].bytesPerPixel + istride0 * stage.sources[1].borderHeight;
	uint32_t back_offset1 = stage.sources[2].borderWidth * stage.sources[2].bytesPerPixel + istride1 * stage.sources[2].borderHeight;

	// convert from FP16Luv and combine with fuzziness
	edgePostFP16LuvToFloats( src_address0 - back_offset0, src_address0 - back_offset0, numsrcpixels * numsrcscanline );
	edgePostInitializeDofInputBufferF4( src_address0 - back_offset0, src_address0 - back_offset0, src_address1 - back_offset1, numsrcpixels * numsrcscanline );

	// pixel size is now 16 bytes after conversion
	uint32_t istride4 = ( stage.sources[1].width + stage.sources[1].borderWidth * 2 ) * 16;

	// rearrange input pointer after conversion
	uint32_t fwd_offset = stage.sources[1].borderWidth * 16 + istride4 * stage.sources[1].borderHeight;
	src_address0 = ( src_address0 - back_offset0 ) + fwd_offset;

	// we generate output as FX16, then convert to Luv for output
	const uint16_t ostride = numpixels * 8;

	// DOF loop
	for ( uint32_t j = 0; j < numscaline; ++j, dst_address += ostride, src_address0 += istride4 )
		edgePostDofVsc_FX16( dst_address, src_address0, istride4, numpixels, g_edgePostDefaultDof36Taps, 36 );

	// convert to Luv
	edgePostFX16ToLuv( tileInfo->tiles[0], tileInfo->tiles[0], numpixels * numscaline);
}
