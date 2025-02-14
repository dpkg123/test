/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdint.h>
#include <stdbool.h>
#include <spu_intrinsics.h>
#include <cell/spurs/common.h>
#include <cell/spurs/job_chain.h>
#include <cell/spurs/job_context.h>
#include <cell/atomic.h>
#include <cell/dma.h>

#include "edge/edge_printf.h"
#include "edge/edge_assert.h"
#include "edge/geom/edgegeom.h"
#include "edge/geom/edgegeom_decompress.h"

uint32_t userDecompress_F32c3_F32c3_f32c4_F32c2(EdgeGeomSpuContext *ctx,
	const void *vertexes, uint32_t numVertexes, const void *fixedOffsets,
	uint32_t inputFormatId)
{
	switch(inputFormatId)
	{
	default:
	case 15:
		{
			EDGE_GEOM_DECOMPRESS_INIT_GLOBAL_COMMON(ctx, vertexes, numVertexes, fixedOffsets);

			EDGE_GEOM_DECOMPRESS_INIT_GLOBAL(48);
			EDGE_GEOM_DECOMPRESS_INIT_F32(EDGE_GEOM_ATTRIBUTE_ID_POSITION, 0, 3);
			EDGE_GEOM_DECOMPRESS_INIT_F32(EDGE_GEOM_ATTRIBUTE_ID_NORMAL, 12, 3);
			EDGE_GEOM_DECOMPRESS_INIT_F32(EDGE_GEOM_ATTRIBUTE_ID_TANGENT, 24, 4);
			EDGE_GEOM_DECOMPRESS_INIT_F32(EDGE_GEOM_ATTRIBUTE_ID_UV0, 40, 2);

			EDGE_GEOM_DECOMPRESS_LOAD_COMMON();
			do
			{
				EDGE_GEOM_DECOMPRESS_LOOP_START();

				EDGE_GEOM_DECOMPRESS_LOOP_F32(EDGE_GEOM_ATTRIBUTE_ID_POSITION);
				EDGE_GEOM_DECOMPRESS_LOOP_F32(EDGE_GEOM_ATTRIBUTE_ID_NORMAL);
				EDGE_GEOM_DECOMPRESS_LOOP_F32(EDGE_GEOM_ATTRIBUTE_ID_TANGENT);
				EDGE_GEOM_DECOMPRESS_LOOP_F32(EDGE_GEOM_ATTRIBUTE_ID_UV0);

				EDGE_GEOM_DECOMPRESS_LOOP_END();
			} while (!EDGE_GEOM_DECOMPRESS_LOOP_DONE());

			EDGE_GEOM_DECOMPRESS_FINALIZE_F32(EDGE_GEOM_ATTRIBUTE_ID_POSITION);
			EDGE_GEOM_DECOMPRESS_FINALIZE_F32(EDGE_GEOM_ATTRIBUTE_ID_NORMAL);
			EDGE_GEOM_DECOMPRESS_FINALIZE_F32(EDGE_GEOM_ATTRIBUTE_ID_TANGENT);
			EDGE_GEOM_DECOMPRESS_FINALIZE_F32(EDGE_GEOM_ATTRIBUTE_ID_UV0);
		}
		break;
	}
	return 0;
}


extern "C" 
{
	void cellSpursJobMain2(CellSpursJobContext2* stInfo, CellSpursJob256 *job)
	{
		// Input buffers
		// Note that these are pointers into the input buffer, and
		// their lifetime is limited by the current stage of
		// processing.  For example, you can not assume that the
		// vertexesA pointer will actually point to input vertex data
		// after calling DecompressVertexes()!
		struct __attribute__(aligned(16))
		{
			EdgeGeomVertexStreamDescription *outputStreamDescription; // 0
			void *vertexesA; // 1
			void *pad4; // 2
			void *pad5; // 3
		} inputs;
		cellSpursJobGetPointerList((void**)&inputs, &job->header, stInfo);

		// Generate the EdgeSpuConfigInfo
		EdgeGeomSpuConfigInfo spuInfo;
		spuInfo.flagsAndUniformTableCount = job->workArea.userData[8];
		spuInfo.commandBufferHoleSize = 0;
		spuInfo.inputVertexFormatId = job->workArea.userData[5];
		spuInfo.outputVertexFormatId = job->workArea.userData[6];
		spuInfo.vertexDeltaFormatId = 0;
		spuInfo.indexesFlavorAndSkinningFlavor = EDGE_GEOM_SKIN_NONE;
		spuInfo.numVertexes = job->workArea.userData[7];
		spuInfo.numIndexes = 0;
		spuInfo.indexesOffset = 0;

		// Validate buffer order
		//NOTE: This will occasionally spit out a warning about the relative positions of spuInfo and inputs.vertexesA.
		//      Since we're creating the EdgeGeomSpuConfigInfo locally, this doesn't matter, so we'll mask it out.  In the 
		//      cases where it would be part of the data buffer sent across, however, it would be an issue.  
		uint32_t errCode = edgeGeomValidateBufferOrder(0, 0, 0, 0, inputs.vertexesA, 0, 0, 0, &spuInfo, 0, 0, 0, 0);
		errCode &= ~(EDGE_GEOM_VALIDATE_WARNING_SPU_CONFIG_INFO);
		EDGE_ASSERT_MSG( errCode == 0, ("Buffer order incorrect; errCode = 0x%08X\n", errCode) );

		// Edge processing begins here
		EdgeGeomCustomVertexFormatInfo customFormatInfo =
			{
				0,0,
				inputs.outputStreamDescription,
				0,
				userDecompress_F32c3_F32c3_f32c4_F32c2,
				0,
				0,
				0,
				0,
				0 };

		EdgeGeomSpuContext ctx;

		edgeGeomInitialize(&ctx, &spuInfo, 
			stInfo->sBuffer, job->header.sizeScratch << 4, 
			stInfo->ioBuffer, job->header.sizeInOrInOut,                        
            stInfo->dmaTag, 0, 0, &customFormatInfo);
		edgeGeomDecompressVertexes(&ctx, inputs.vertexesA, 0, 0, 0);

		EdgeGeomAllocationInfo info;
		uint32_t outputEa = job->workArea.userData[4];
		uint32_t holeEa = job->workArea.userData[9] & 0xFFFFFFFF;
		uint32_t allocSize = edgeGeomCalculateDefaultOutputSize(&ctx, 0);
		if(!edgeGeomAllocateOutputSpace(&ctx, outputEa, allocSize, &info, cellSpursGetCurrentSpuId()))
		{
			CellGcmContextData gcmCtx;
			edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
			edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
			return;
		}

		edgeGeomCompressVertexes(&ctx);
		EdgeGeomLocation vtx;
		edgeGeomOutputVertexes(&ctx, &info, &vtx);
		CellGcmContextData gcmCtx;
		edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
		edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
	}
}

