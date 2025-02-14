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
#include "edge/geom/edgegeom_decompress.h"
#include "edge/geom/edgegeom_compress.h"
#include "job_geom_interface.h"

inline void insert_bookmark(uint32_t bookmark)
{
	__asm__ volatile ("wrch $69, %0" :: "r" (bookmark));
	__asm__ volatile ("nop;nop;nop;nop;nop;nop;nop;nop;");
	__asm__ volatile ("nop;nop;nop;nop;nop;nop;nop;nop;");
}

static uint32_t userDecompressVertexStream(EdgeGeomSpuContext *ctx,
	const void *vertexes, uint32_t numVertexes,
	const void *fixedOffsets, uint32_t inputFormatId)
{
	switch(inputFormatId)
	{
	default:
	case 0xFF:
		{
			EDGE_GEOM_DECOMPRESS_INIT_GLOBAL_COMMON(ctx, vertexes, numVertexes, fixedOffsets);
			
			EDGE_GEOM_DECOMPRESS_INIT_GLOBAL(12);
			
			EDGE_GEOM_DECOMPRESS_INIT_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_NORMAL, 0, 3);
			EDGE_GEOM_DECOMPRESS_INIT_I16N(EDGE_GEOM_ATTRIBUTE_ID_TANGENT, 4, 4);
			
			EDGE_GEOM_DECOMPRESS_LOAD_COMMON();
			do
			{
				EDGE_GEOM_DECOMPRESS_LOOP_START();
				
				EDGE_GEOM_DECOMPRESS_LOOP_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_NORMAL);
				EDGE_GEOM_DECOMPRESS_LOOP_I16N(EDGE_GEOM_ATTRIBUTE_ID_TANGENT);
				
				EDGE_GEOM_DECOMPRESS_LOOP_END();
			} while (!EDGE_GEOM_DECOMPRESS_LOOP_DONE());
			
			EDGE_GEOM_DECOMPRESS_FINALIZE_X11Y11Z10N(EDGE_GEOM_ATTRIBUTE_ID_NORMAL);
			EDGE_GEOM_DECOMPRESS_FINALIZE_I16N(EDGE_GEOM_ATTRIBUTE_ID_TANGENT);
		}
		break;
	}
	return 0;
}
#define spu_read_decrementer()        spu_readch(SPU_RdDec)

extern "C" 
{
	void ReportTimings(EdgeGeomSpuContext *ctx, const JobGeomPerformanceCounters &perfInfo, uint32_t eaDest, uint32_t dmaTag)
	{
		// Copy the timings in the I/O buffer, where they won't be overwritten by the next job
		JobGeomPerformanceCounters *outPerfInfo = (JobGeomPerformanceCounters*)edgeGeomGetFreePtr(ctx);
		*outPerfInfo = perfInfo;

        bool isEnabled = spu_readch(SPU_RdMachStat) & 1;
	
		spu_idisable();

		spu_writech(MFC_LSA, (uint32_t)outPerfInfo);
		spu_writech(MFC_EAL, (uint32_t)eaDest);
		spu_writech(MFC_Size, sizeof(JobGeomPerformanceCounters));
		spu_writech(MFC_TagID, dmaTag);
		spu_writech(MFC_Cmd, MFC_PUTF_CMD);
		/*		
		// wait for dmas
		spu_writech(MFC_WrTagMask, 1 << 0);
		spu_writech(MFC_WrTagUpdate, 2);
		spu_readch(MFC_RdTagStat);
		*/
		if(isEnabled)
			spu_ienable();		
	}

	void cellSpursJobMain2(CellSpursJobContext2* stInfo,  CellSpursJob256 *job256)
	{
JobGeomPerformanceCounters perfInfo;
perfInfo.m_outitTicks = 0;
perfInfo.m_start = spu_read_decrementer();
insert_bookmark(0xED00);

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
		(void)errCode;

		EdgeGeomCustomVertexFormatInfo customFormatInfo = 
		{
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobGeomDmaInputStreamDesc1],
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobGeomDmaInputStreamDesc2],
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobGeomDmaOutputStreamDesc],
			0, // unused (deprecated blend shape description)

			0,// decompress primary vertex stream,
			userDecompressVertexStream, // decompress secondary vertex stream
			0, // decompress/blend stream (deprecated)
			0,
			0,
			0,
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

perfInfo.m_numInputVertexes = edgeGeomGetSpuConfigInfo(&ctx)->numVertexes;
perfInfo.m_numInputIndexes = edgeGeomGetSpuConfigInfo(&ctx)->numIndexes;
perfInfo.m_numOutputVertexes = 0;
perfInfo.m_numOutputIndexes = 0;
perfInfo.m_initTicks = spu_read_decrementer();
insert_bookmark(0xED01);
		edgeGeomDecompressVertexes(&ctx, inputsPointers[kJobGeomDmaVertexes1], 
			inputsPointers[kJobGeomDmaFixedPointOffsets1],
			inputsPointers[kJobGeomDmaVertexes2], 
			inputsPointers[kJobGeomDmaFixedPointOffsets2]);
perfInfo.m_decompressVertexesTicks = spu_read_decrementer();
insert_bookmark(0xED02);
        edgeGeomProcessBlendShapes(&ctx, numBlendShapes, blendShapeInfosEa);
perfInfo.m_blendTicks = spu_read_decrementer();
insert_bookmark(0xED03);
        edgeGeomSkinVertexes(&ctx, inputsPointers[kJobGeomDmaSkinMatrices], totalMatrixCount, inputsPointers[kJobGeomDmaSkinIndexesAndWeights]);
perfInfo.m_skinTicks = spu_read_decrementer();
insert_bookmark(0xED04);
        edgeGeomDecompressIndexes(&ctx, inputsPointers[kJobGeomDmaIndexes]);
perfInfo.m_decompressIndexesTicks = spu_read_decrementer();
insert_bookmark(0xED05);
        uint32_t numVisibleIdxs = edgeGeomCullTriangles(&ctx, EDGE_GEOM_CULL_BACKFACES_AND_FRUSTUM);
perfInfo.m_cullTicks = spu_read_decrementer();
insert_bookmark(0xED06);
		// If all triangles were culled, we can avoid further
		// processing with an early return. But we still have to 
		// write the command buffer hole!
		if (numVisibleIdxs == 0)
		{
			CellGcmContextData gcmCtx;
			edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
			edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
			insert_bookmark(0x0000);
			ReportTimings(&ctx, perfInfo, job->userData.eaPerformanceCounters, stInfo->dmaTag);
			return;
		}

		EdgeGeomAllocationInfo info;
		uint32_t allocSize = edgeGeomCalculateDefaultOutputSize(&ctx, numVisibleIdxs);
		if(!edgeGeomAllocateOutputSpace(&ctx, outputBufferInfoEa, allocSize, &info, cellSpursGetCurrentSpuId()))
		{
			CellGcmContextData gcmCtx;
			edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
			edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, 0, 0);
			insert_bookmark(0x0000);
			ReportTimings(&ctx, perfInfo, job->userData.eaPerformanceCounters, stInfo->dmaTag);
			return;
		}

perfInfo.m_numOutputVertexes = perfInfo.m_numInputVertexes;
perfInfo.m_numOutputIndexes = numVisibleIdxs;


perfInfo.m_allocateTicks = spu_read_decrementer();		
insert_bookmark(0xED07);
		EdgeGeomLocation idx;
		edgeGeomOutputIndexes(&ctx, numVisibleIdxs, &info, &idx);
perfInfo.m_outputIndexesTicks = spu_read_decrementer();
insert_bookmark(0xED08);
		edgeGeomCompressVertexes(&ctx);
perfInfo.m_compressVertexesTicks = spu_read_decrementer();
insert_bookmark(0xED09);
		EdgeGeomLocation vtx;
		edgeGeomOutputVertexes(&ctx, &info, &vtx);
perfInfo.m_outputVertexesTicks = spu_read_decrementer();		
insert_bookmark(0xED0A);
		CellGcmContextData gcmCtx;
		edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, &info, 1);
		// Note: If any of the push buffer contents are changed from this sequence, the
		// spuConfigInfo.holeSize must be updated to reflect those changes.
		edgeGeomSetVertexDataArrays(&ctx, &gcmCtx, &vtx);
		cellGcmSetDrawIndexArrayUnsafeInline(&gcmCtx, CELL_GCM_PRIMITIVE_TRIANGLES,
			numVisibleIdxs, CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16, 
			idx.location, idx.offset);
		edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, &info, 1);
perfInfo.m_outitTicks = spu_read_decrementer();
insert_bookmark(0x0000); // end bookmark bars

 ReportTimings(&ctx, perfInfo, job->userData.eaPerformanceCounters, stInfo->dmaTag);
	}

	
}

