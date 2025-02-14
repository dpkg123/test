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
#include "edge/geom/edgegeom_compress.h"
#include "edge/geom/edgegeom_decompress.h"
#include "simulate.h"

// Anything over 0x10 should be safe, so long as it doesn't conflict with edgegeom_attributes.h
enum {CUSTOM_ATTRIBUTE_ID_VELOCITY_MASS = 0x20};

uint32_t userDecompress_F32c4_F32c4(EdgeGeomSpuContext *ctx, 
	const void *vertexes, uint32_t numVertexes, const void *fixedOffsets,
	uint32_t inputFormatId)
{
	switch(inputFormatId)
	{
	default:
	case 0xFF:
		{
			EDGE_GEOM_DECOMPRESS_INIT_GLOBAL_COMMON(ctx, vertexes, numVertexes, fixedOffsets);

			EDGE_GEOM_DECOMPRESS_INIT_GLOBAL(32);
			EDGE_GEOM_DECOMPRESS_INIT_F32(EDGE_GEOM_ATTRIBUTE_ID_POSITION /* Position */, 0, 4);
			EDGE_GEOM_DECOMPRESS_INIT_F32(CUSTOM_ATTRIBUTE_ID_VELOCITY_MASS /* Velocity xyz, 1/Mass w */, 16, 4);

			EDGE_GEOM_DECOMPRESS_LOAD_COMMON();
			do
			{
				EDGE_GEOM_DECOMPRESS_LOOP_START();

				EDGE_GEOM_DECOMPRESS_LOOP_F32(EDGE_GEOM_ATTRIBUTE_ID_POSITION);
				EDGE_GEOM_DECOMPRESS_LOOP_F32(CUSTOM_ATTRIBUTE_ID_VELOCITY_MASS);

				EDGE_GEOM_DECOMPRESS_LOOP_END();
			} while (!EDGE_GEOM_DECOMPRESS_LOOP_DONE());

			EDGE_GEOM_DECOMPRESS_FINALIZE_F32(EDGE_GEOM_ATTRIBUTE_ID_POSITION);
			EDGE_GEOM_DECOMPRESS_FINALIZE_F32(CUSTOM_ATTRIBUTE_ID_VELOCITY_MASS);
		}
		break;
	}
	return 0;
}

uint32_t userOutputVertexStride(EdgeGeomSpuContext *ctx, uint32_t outputFormatId)
{
	(void)ctx;
	switch(outputFormatId)
	{
	default:
	case 0xFF:
		{
			return 32;
		}
		break;
	}
	return 0;

}

uint32_t userCompress_F32c4_F32c4(EdgeGeomSpuContext *ctx, uint32_t numVertexes,
								  void *outVertexes, uint32_t outputFormatId)
{
	switch(outputFormatId)
	{
	default:
	case 0xFF:
		{
			EDGE_GEOM_COMPRESS_INIT_GLOBAL_COMMON(ctx, numVertexes, outVertexes);

			EDGE_GEOM_COMPRESS_INIT_GLOBAL(32);
			
			EDGE_GEOM_COMPRESS_INIT_F32(EDGE_GEOM_ATTRIBUTE_ID_POSITION, 0, 16);
			EDGE_GEOM_COMPRESS_INIT_F32(CUSTOM_ATTRIBUTE_ID_VELOCITY_MASS, 16, 16);
			
			do
			{
				EDGE_GEOM_COMPRESS_LOOP_START();
				
				EDGE_GEOM_COMPRESS_LOOP_F32(EDGE_GEOM_ATTRIBUTE_ID_POSITION);
				EDGE_GEOM_COMPRESS_LOOP_F32(CUSTOM_ATTRIBUTE_ID_VELOCITY_MASS);
				
				EDGE_GEOM_COMPRESS_LOOP_END();
			} while(!EDGE_GEOM_COMPRESS_LOOP_DONE());
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
			void *vertexesA; // 0
			void *pad4; // 1
			void *pad5; // 2
		} inputs;
		cellSpursJobGetPointerList((void**)&inputs, &job->header, stInfo);

		// Generate the EdgeSpuConfigInfo
		EdgeGeomSpuConfigInfo spuInfo;
		spuInfo.flagsAndUniformTableCount = job->workArea.userData[7];
		spuInfo.commandBufferHoleSize = 0;
		spuInfo.inputVertexFormatId = job->workArea.userData[4];
		spuInfo.secondaryInputVertexFormatId = 0; // unused
		spuInfo.outputVertexFormatId = job->workArea.userData[5];
		spuInfo.vertexDeltaFormatId = 0;
		spuInfo.indexesFlavorAndSkinningFlavor = EDGE_GEOM_SKIN_NONE;
		spuInfo.numVertexes = job->workArea.userData[6];
		spuInfo.numIndexes = 0;
		spuInfo.indexesOffset = 0;

		qword timestep = si_rotqby(si_lqd(si_from_ptr(&job->workArea.userData[9]), 0x0), si_from_ptr(&job->workArea.userData[9]));

		// Validate buffer orders
		//NOTE: This will occasionally spit out a warning about the
		//relative positions of spuInfo and inputs.vertexesA.  Since
		//we're creating the EdgeGeomSpuConfigInfo locally, this
		//doesn't matter, so we'll mask it out.  In the cases where it
		//would be part of the data buffer sent across, however, it
		//would be an issue.
		uint32_t errCode = edgeGeomValidateBufferOrder(0, 0, 0, 0, inputs.vertexesA, 0, 0, 0, &spuInfo, 0, 0, 0, 0);
		errCode &= ~(EDGE_GEOM_VALIDATE_WARNING_SPU_CONFIG_INFO);
		EDGE_ASSERT_MSG( errCode == 0, ("Buffer order incorrect; errCode = 0x%08X\n", errCode) );

		if ((uint32_t)(job->workArea.userData[8] & 0xFFFFFFFF) == 0xFFFFFFFF) {
			// Edge processing begins here
			EdgeGeomCustomVertexFormatInfo customFormatInfo = {	0,0,0,0,
															// callback functions take precedence
															userDecompress_F32c4_F32c4,
															0,
															0,
															userOutputVertexStride,
															userCompress_F32c4_F32c4,
															0};

			EdgeGeomSpuContext ctx;

			edgeGeomInitialize(&ctx, &spuInfo, 
			    stInfo->sBuffer, job->header.sizeScratch << 4, 
			    stInfo->ioBuffer, job->header.sizeInOrInOut,                            
                stInfo->dmaTag, 0, 0, &customFormatInfo);
			edgeGeomDecompressVertexes(&ctx, inputs.vertexesA, 0, 0, 0);

			// Do the Euler Step here
			{
				void *freePtr = (void *)(((uint32_t)edgeGeomGetFreePtr(&ctx) + 0xf) & ~0xf);

				ParticleDerivatives(spuInfo.numVertexes, edgeGeomGetUniformTable(&ctx, 0), edgeGeomGetUniformTable(&ctx, 1), freePtr);

				ScaleAndAdd(spuInfo.numVertexes, si_to_float(timestep), edgeGeomGetUniformTable(&ctx, 0), freePtr);
				ScaleAndAdd(spuInfo.numVertexes, si_to_float(timestep), edgeGeomGetUniformTable(&ctx, 1), (void *)((uint32_t)freePtr + 16 * spuInfo.numVertexes));
			}

			//We're just outputting back to where this data came from in main memory
			EdgeGeomAllocationInfo info;
			uint32_t outputEa = job->workArea.userData[3];
			uint32_t allocSize = edgeGeomCalculateDefaultOutputSize(&ctx, 0);
			if(!edgeGeomAllocateOutputSpace(&ctx, outputEa, allocSize, &info, cellSpursGetCurrentSpuId()))
			{
				// We don't have space to store the new positions and velocities
				return;
			}

			edgeGeomCompressVertexes(&ctx);

			EdgeGeomLocation vtx;
			// DMA it back to main memory
			edgeGeomOutputVertexes(&ctx, &info, &vtx);
		}
		else{

			//we need to put the draw calls for the generated indices in here
			EdgeGeomCustomVertexFormatInfo customFormatInfo = {	0,0,0,0,
															// callback functions take precedence
															userDecompress_F32c4_F32c4,
															0,0,0,0,0};

			EdgeGeomSpuContext ctx;

			edgeGeomInitialize(&ctx, &spuInfo, 
			    stInfo->sBuffer, job->header.sizeScratch << 4, 
			    stInfo->ioBuffer, job->header.sizeInOrInOut,                                            
                stInfo->dmaTag, 0, 0, &customFormatInfo);
			edgeGeomDecompressVertexes(&ctx, inputs.vertexesA, 0, 0);

			// Compress them as usual
			EdgeGeomAllocationInfo info;
			uint32_t outputEa = job->workArea.userData[3];
			uint32_t holeEa = job->workArea.userData[8];
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
			edgeGeomBeginCommandBufferHole(&ctx, &gcmCtx, holeEa, &info, 1);
			edgeGeomEndCommandBufferHole(&ctx, &gcmCtx, holeEa, &info, 1);
		}
	}
}

