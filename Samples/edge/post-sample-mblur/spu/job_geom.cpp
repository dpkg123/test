/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2009 Sony Computer Entertainment Inc.
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

	EdgeGeomAllocationInfo skinPreviousFrame( const JobGeom256 *job, EdgeGeomSpuContext* ctx, void *matrices, uint32_t matrixCount, void *indexesAndWeights )
	{	
		// save normal/tangent/binormal
		vec_float4* positions = (vec_float4*)edgeGeomGetPositionUniformTable( ctx);
		vec_float4* normals   = (vec_float4*)edgeGeomGetNormalUniformTable( ctx);
		qword* tangents  = edgeGeomGetTangentUniformTable( ctx);
		qword* binormals = edgeGeomGetBinormalUniformTable( ctx);
	
		// set normal/tangent/binormal to NULL so they don't get skinned
		edgeGeomSetNormalUniformTable( ctx, 0 );
		edgeGeomSetTangentUniformTable( ctx, 0 );
		edgeGeomSetBinormalUniformTable( ctx, 0 );

		// save position by compressing it into normal uniform table
		uint16_t num = edgeGeomGetVertexCount( ctx);
		static const vec_uchar16 pack = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x13, 0x17, 0x1b, 0x1f };

		for ( int v = 0; v < num; ++v )
		{
			vec_float4 n = spu_madd( normals[v], spu_splats( .5f), spu_splats( .5f));
			vec_uint4 nc = spu_convtu( spu_mul( n, spu_splats( 255.0f/256.0f)), 8);
			normals[v] = spu_shuffle( positions[v], (vec_float4)nc, pack );
		}

		// skin ( position only ... )
		void* freePtr = edgeGeomGetFreePtr( ctx);
		edgeGeomSkinVertexes( ctx, matrices, matrixCount, indexesAndWeights);
		edgeGeomSetFreePtr( ctx, freePtr );

		// allocate ea memory to hold previous frame skinned position
		uint32_t memsize = num * 16;
		EdgeGeomAllocationInfo info;

		
		if( !edgeGeomAllocateOutputSpace( ctx, job->userData.eaOutputBufferInfo, memsize, &info, cellSpursGetCurrentSpuId()) )
		{
			EDGE_ASSERT( 0 && "Allocation failed" ) ;
		}
		

		// disable interrupts
		bool interruptsEnabled = spu_readch(SPU_RdMachStat) & 1;
		spu_idisable();

		// output skinned vertices
		uint32_t ea = info.ea;
		uint32_t ls = (uint32_t)positions;
		do
		{
			uint32_t s = (memsize > 0x4000) ? 0x4000 : memsize;
			spu_writech(MFC_LSA, ls);
			spu_writech(MFC_EAL, ea);
			spu_writech(MFC_Size, s);
			spu_writech(MFC_TagID, ctx->inputDmaTag);
			spu_writech(MFC_Cmd, MFC_PUT_CMD);
			ls += s;
			ea += s;
			memsize -= s;
		} while( memsize > 0);

		// input current frame matrix palette
		uint8_t* pMatrices = (uint8_t*) matrices;
		spu_writech(MFC_LSA, (uint32_t)pMatrices);
		spu_writech(MFC_EAL, job->userData.eaCurrentFrameMatrix0 );
		spu_writech(MFC_Size, job->userData.currentFrameMatrixSize0);
		spu_writech(MFC_TagID, ctx->inputDmaTag);
		spu_writech(MFC_Cmd, MFC_GET_CMD);
		spu_writech(MFC_LSA, (uint32_t)( pMatrices + job->userData.currentFrameMatrixSize0));
		spu_writech(MFC_EAL, job->userData.eaCurrentFrameMatrix1 );
		spu_writech(MFC_Size, job->userData.currentFrameMatrixSize1);
		spu_writech(MFC_TagID, ctx->inputDmaTag);
		spu_writech(MFC_Cmd, MFC_GET_CMD);

		// Wait for the DMA to complete
		spu_writech(MFC_WrTagMask, 1 << ctx->inputDmaTag);
		spu_writech(MFC_WrTagUpdate, 2);
		spu_readch(MFC_RdTagStat);

		// reenable interrupts
		if(interruptsEnabled)
			spu_ienable();

		// restore position
		static const vec_uchar16 expandNormal = { 0x80, 0x80, 0x80, 0x0c, 0x80, 0x80, 0x80, 0x0d, 0x80, 0x80, 0x80, 0x0e, 0x80, 0x80, 0x80, 0x0f };
		static const vec_uchar16 expandPos = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x10, 0x11, 0x12, 0x13 };
		for ( int v = 0; v < num; ++v )
		{
			vec_float4 p = normals[v];
			normals[v] = spu_mul( spu_convtf( (vec_uint4)spu_shuffle( p, p, expandNormal ), 8), spu_splats( 256.0f/255.0f));
			positions[v] = spu_shuffle( p, spu_splats( 1.f), expandPos);
		}

		// reset normal/tangent/binormal
		edgeGeomSetNormalUniformTable( ctx, (qword*)normals );
		edgeGeomSetTangentUniformTable( ctx, tangents );
		edgeGeomSetBinormalUniformTable( ctx, binormals );

		// return outputlocation
		return info;
	}

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
		(void)errCode;

		EdgeGeomCustomVertexFormatInfo customFormatInfo = 
		{
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobGeomDmaInputStreamDesc1],
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobGeomDmaInputStreamDesc2],
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobGeomDmaOutputStreamDesc],
			0,
			0,0,0,0,0,0
		};

		EdgeGeomSpuContext ctx;

		// Note: hole size needs to account for an additional output
		EdgeGeomSpuConfigInfo* pEdgeGeomSpuConfigInfo = 
			(EdgeGeomSpuConfigInfo*)inputsPointers[kJobGeomDmaSpuConfigInfo];
		pEdgeGeomSpuConfigInfo->commandBufferHoleSize++;

		// Edge processing begins here
		edgeGeomInitialize(&ctx, 
			pEdgeGeomSpuConfigInfo, 
			stInfo->sBuffer, job->header.sizeScratch << 4, 
			stInfo->ioBuffer, job->header.sizeInOrInOut,            
            stInfo->dmaTag, 
			(const EdgeGeomViewportInfo*) inputsPointers[kJobGeomDmaViewportInfo], 
			(const EdgeGeomLocalToWorldMatrix*) inputsPointers[kJobGeomDmaLocalToWorldMatrix], 
            &customFormatInfo);
		edgeGeomDecompressVertexes(&ctx, inputsPointers[kJobGeomDmaVertexes1], 
			inputsPointers[kJobGeomDmaFixedPointOffsets1],
			inputsPointers[kJobGeomDmaVertexes2], 
			inputsPointers[kJobGeomDmaFixedPointOffsets2]);
		edgeGeomProcessBlendShapes(&ctx, numBlendShapes, blendShapeInfosEa);
		
		// we'll do 2 allocations
		EdgeGeomAllocationInfo allocationInfo[2];

		// Skin previous frame positions
		allocationInfo[0] =	skinPreviousFrame( job, &ctx, inputsPointers[kJobGeomDmaSkinMatrices], totalMatrixCount, inputsPointers[kJobGeomDmaSkinIndexesAndWeights] );

		// Skin current frame position
		edgeGeomSkinVertexes(&ctx, inputsPointers[kJobGeomDmaSkinMatrices], totalMatrixCount, inputsPointers[kJobGeomDmaSkinIndexesAndWeights]);
		
		// decompress indices
		edgeGeomDecompressIndexes(&ctx, inputsPointers[kJobGeomDmaIndexes]);

        uint32_t numVisibleIdxs = edgeGeomCullTriangles(&ctx, EDGE_GEOM_CULL_BACKFACES_AND_FRUSTUM);
		// If all triangles were culled, we can avoid further
		// processing with an early return. But we still have to 
		// write the command buffer hole!
		if (numVisibleIdxs == 0)
		{
			CellGcmContextData gcmCtx;
			edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, allocationInfo, 1);
			edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, allocationInfo, 1);
			return;
		}

		uint32_t allocSize = edgeGeomCalculateDefaultOutputSize(&ctx, numVisibleIdxs);
		if(!edgeGeomAllocateOutputSpace(&ctx, outputBufferInfoEa, allocSize, &allocationInfo[1], cellSpursGetCurrentSpuId()))
		{
			CellGcmContextData gcmCtx;
			edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, allocationInfo, 1);
			edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, allocationInfo, 1);
			return;
		}
		
		EdgeGeomLocation idx;
		edgeGeomOutputIndexes(&ctx, numVisibleIdxs, &allocationInfo[1], &idx);
		edgeGeomCompressVertexes(&ctx);
		EdgeGeomLocation vtx;
		edgeGeomOutputVertexes(&ctx, &allocationInfo[1], &vtx);
		
		CellGcmContextData gcmCtx;
		edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, allocationInfo, 2);
		edgeGeomSetVertexDataArrays(&ctx, &gcmCtx, &vtx);
		cellGcmSetVertexDataArrayUnsafeInline( &gcmCtx, 1, 0, 16, 3, CELL_GCM_VERTEX_F, allocationInfo[0].location, allocationInfo[0].offset);
		cellGcmSetDrawIndexArrayUnsafeInline(&gcmCtx, CELL_GCM_PRIMITIVE_TRIANGLES, numVisibleIdxs, CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16, idx.location, idx.offset);
		edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, allocationInfo, 2);
	}
}

