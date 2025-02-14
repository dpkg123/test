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

#include "edge/edge_assert.h"
#include "edge/edge_printf.h"
#include "edge/geom/edgegeom.h"
#include "job_geom_interface.h"

extern "C" 
{
	void cellSpursJobMain2(CellSpursJobContext2* stInfo,  CellSpursJob256 *job256)
	{
		const JobGeom256 *job = (const JobGeom256 *) job256;

		// Input buffers
		// Note that these are pointers into the input buffer, and
		// their lifetime is limited by the current stage of
		// processing.  For example, you can not assume that the
		// vertexesA pointer will actually point to input vertex data
		// after calling DecompressVertexes()!
		void* inputsPointers[kJobGeomInputDmaEnd] __attribute__((aligned(16)));
		cellSpursJobGetPointerList(inputsPointers, &job->header, stInfo);

		// Extract matrix count from dma size
		uint32_t totalMatrixCount  = (job->inputDmaList[kJobGeomDmaSkinMatrices_0].size
			+ job->inputDmaList[kJobGeomDmaSkinMatrices_1].size) / 48;

		// Extract additional values from the userData area.
		uint32_t outputBufferInfoEa = job->userData.eaOutputBufferInfo;
		uint32_t holeEa            	= job->userData.eaCommandBufferHole;
		uint32_t numBlendShapes    	= job->userData.blendShapeInfoCount;
		uint32_t blendShapeInfosEa 	= job->userData.eaBlendShapeInfo;

		// If only one stream was passed (i.e. the second stream size is 0), we need to explicitly zero
		// out the secondary vertex stream pointers.
		bool hasSecondStream = (job->inputDmaList[kJobGeomDmaVertexes2_0].size
			+ job->inputDmaList[kJobGeomDmaVertexes2_1].size
			+ job->inputDmaList[kJobGeomDmaVertexes2_2].size) > 0;
		if (!hasSecondStream)
		{
			inputsPointers[kJobGeomDmaVertexes2] = 0;
			inputsPointers[kJobGeomDmaInputStreamDesc2] = 0;
			inputsPointers[kJobGeomDmaFixedPointOffsets2] = 0;
		}
		
		// Validate buffer order
		uint32_t errCode = edgeGeomValidateBufferOrder(
			inputsPointers[kJobGeomDmaOutputStreamDesc], 
			inputsPointers[kJobGeomDmaIndexes], 
			inputsPointers[kJobGeomDmaSkinMatrices], 
			inputsPointers[kJobGeomDmaSkinIndexesAndWeights], 
			inputsPointers[kJobGeomDmaVertexes1],
			inputsPointers[kJobGeomDmaVertexes2], 
			inputsPointers[kJobGeomDmaViewportInfo], 
			inputsPointers[kJobGeomDmaLocalToWorldMatrix], 
			inputsPointers[kJobGeomDmaSpuConfigInfo],
			inputsPointers[kJobGeomDmaFixedPointOffsets1], 
			inputsPointers[kJobGeomDmaFixedPointOffsets2], 
			inputsPointers[kJobGeomDmaInputStreamDesc1],
			inputsPointers[kJobGeomDmaInputStreamDesc2]);
		EDGE_ASSERT_MSG( errCode == 0, ("Buffer order incorrect; errCode = 0x%08X\n", errCode) );
		(void) errCode;

		EdgeGeomCustomVertexFormatInfo customFormatInfo = 
		{
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobGeomDmaInputStreamDesc1],
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobGeomDmaInputStreamDesc2],
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobGeomDmaOutputStreamDesc],
			0,
			0,0,0,0,0,0
		};

		EdgeGeomSpuContext ctx;

		// Edge processing begins here
		edgeGeomInitialize(&ctx, 
			(const EdgeGeomSpuConfigInfo*) inputsPointers[kJobGeomDmaSpuConfigInfo], 
			stInfo->sBuffer, job->header.sizeScratch << 4, 
			stInfo->ioBuffer, job->header.sizeInOrInOut,            
            stInfo->dmaTag, 
			(const EdgeGeomViewportInfo*) inputsPointers[kJobGeomDmaViewportInfo], 
			(const EdgeGeomLocalToWorldMatrix*) inputsPointers[kJobGeomDmaLocalToWorldMatrix], 
            &customFormatInfo);

		// Initialize Edge's free pointer to the end of the input
		// data. Not strictly necessary, but it can make certain
		// problems easier to debug.
		uint32_t lastInputListIndex = (job256->header.sizeDmaList / sizeof(uint64_t)) - 1;
		while(inputsPointers[lastInputListIndex] == 0)
			--lastInputListIndex;
		uint32_t inputDataEnd = (intptr_t)inputsPointers[lastInputListIndex]
			+ (job256->workArea.dmaList[lastInputListIndex]>>32); // LS start + size
		edgeGeomSetFreePtr(&ctx, (uint8_t*)inputDataEnd);

		edgeGeomDecompressVertexes(&ctx, inputsPointers[kJobGeomDmaVertexes1], 
			inputsPointers[kJobGeomDmaFixedPointOffsets1],
			inputsPointers[kJobGeomDmaVertexes2], 
			inputsPointers[kJobGeomDmaFixedPointOffsets2]);
		edgeGeomProcessBlendShapes(&ctx, numBlendShapes, blendShapeInfosEa);
		edgeGeomSkinVertexes(&ctx, inputsPointers[kJobGeomDmaSkinMatrices], totalMatrixCount, inputsPointers[kJobGeomDmaSkinIndexesAndWeights]);
		edgeGeomDecompressIndexes(&ctx, inputsPointers[kJobGeomDmaIndexes]);

        uint32_t numVisibleIdxs = edgeGeomCullTriangles(&ctx, EDGE_GEOM_CULL_BACKFACES_AND_FRUSTUM);
		// If all triangles were culled, we can avoid further
		// processing with an early return. But we still have to 
		// write the command buffer hole!
		if (numVisibleIdxs == 0)
		{
			CellGcmContextData gcmCtx;
			edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
			edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
			return;
		}

		EdgeGeomAllocationInfo info;
		uint32_t allocSize = edgeGeomCalculateDefaultOutputSize(&ctx, numVisibleIdxs);
		if(!edgeGeomAllocateOutputSpace(&ctx, outputBufferInfoEa, allocSize, &info, cellSpursGetCurrentSpuId()))
		{
			CellGcmContextData gcmCtx;
			edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
			edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
			return;
		}
		
		EdgeGeomLocation idx;
		edgeGeomOutputIndexes(&ctx, numVisibleIdxs, &info, &idx);
		edgeGeomCompressVertexes(&ctx);
		EdgeGeomLocation vtx;
		edgeGeomOutputVertexes(&ctx, &info, &vtx);
		
		CellGcmContextData gcmCtx;
		edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, &info, 1);
		// Note: If any of the push buffer contents are changed from this sequence, the
		// spuConfigInfo.holeSize must be updated to reflect those changes.
		edgeGeomSetVertexDataArrays(&ctx, &gcmCtx, &vtx);
		cellGcmSetDrawIndexArrayUnsafeInline(&gcmCtx, CELL_GCM_PRIMITIVE_TRIANGLES,
			numVisibleIdxs, CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16, 
			idx.location, idx.offset);
		edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, &info, 1);
	}
}

