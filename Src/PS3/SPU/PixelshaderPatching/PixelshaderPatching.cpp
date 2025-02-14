/*=============================================================================
	PixelshaderPatching.cpp: SPU pixel shader patching
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "SPU.h"
#include "..\..\Inc\PS3PixelShaderPatching.h"
#include "edge/geom/edgegeom.h"

#if 0
#define debug_printf(...) spu_printf(__VA_ARGS__);
#else
#define debug_printf(...) nop_printf(__VA_ARGS__);
#endif

// This struct must match the corresponding structs in PS3Tools.cpp and PS3RHIResources.h!
struct FPixelShaderInfo
{
	// These are from CellCgbFragmentProgramConfiguration, to be used with cellGcmSetFragmentProgramLoad() at run-time.
	uint32_t	AttributeInputMask;	/** Bitfield for the interpolators (I guess including FACE register, etc) */
	uint32_t	FragmentControl;	/** Control flags for the pixel shaders */
	uint16_t	TexCoordsInputMask;	/** Bitfield for the texcoord interpolators */
	uint16_t	TexCoords2D;		/** Bitfield for each tex2D/tex3D (set to 0xffff) */
	uint16_t	TexCoordsCentroid;	/** Set to 0 */
	uint16_t	RegisterCount;		/** Number of temp registers used */

	uint16_t	MicrocodeSize;		/** Size of the MicroCode[], in bytes */
	uint16_t	TextureMask;		/** Bit-mask of used texture units. Bit #0 represents texture unit #0, etc. */
	uint16_t	NumSamplers;		/** Number of used samplers (size of TextureUnits[]) */
	uint16_t	NumParameters;		/** Number of pixel shader parameters (size of NumOccurrances[]) */
	uint16_t	NumParamOffsets;	/** Number of offset in total (each parameter may have multiple offsets into the microcode) */

	uint8_t		Padding[6];			/** Make sure the struct is 32 bytes, so that the microcode is 16-byte aligned. */

//	uint8_t		Microcode[MicrocodeSize];		/** 16-byte aligned */
//	uint8_t		TextureUnits[NumSamplers];
//	uint8_t		NumOccurrances[NumParameters];
//	uint16_t	Offsets[NumParamOffsets];		/** 16-byte aligned */
};

static void nop_printf(char* Format,...)
{
}

static void MyDMATransferSmall(UINT DmaTag,void* LocalAddress,void* MainAddress,UINT NumBytes)
{
	UINT Offset = 0;
	while(Offset < NumBytes)
	{
		const UINT NumRemainingBytes = NumBytes - Offset;
		const UINT OffsetMainAddress = UINT(MainAddress) + Offset;
		UINT AlignedTransferSize;
		if(NumRemainingBytes >= 8 && !(OffsetMainAddress & 0x7))
		{
			AlignedTransferSize = 8;
		}
		else if(NumRemainingBytes >= 4 && !(OffsetMainAddress & 0x3))
		{
			AlignedTransferSize = 4;
		}
		else if(NumRemainingBytes >= 2 && !(OffsetMainAddress & 0x1))
		{
			AlignedTransferSize = 2;
		}
		else
		{
			AlignedTransferSize = 1;
		}
		cellDmaSmallPutf((BYTE*)LocalAddress + Offset,OffsetMainAddress,AlignedTransferSize,DmaTag,0,0);
		Offset += AlignedTransferSize;
	};
}

static void MyDMATransfer(UINT DmaTag,void* LocalAddress,void* MainAddress,UINT NumBytes)
{
	const UINT MisalignmentOffset = UINT(MainAddress) & 0xf;

	// Detect cases where the lower 4 bits don't match in the source and destination addresses, which is a fatal error.
	if((UINT(MainAddress) & 0xf) != (UINT(LocalAddress) & 0xf))
	{
		debug_printf("Mismatched lower 4 bits!!!!!");
	}

	UINT TransferredBytes = 0;
	if(NumBytes >= 16)
	{
		// Transfer any data at the start which isn't 16-byte aligned.
		const UINT StartUnalignedBytes = (16 - MisalignmentOffset) & 0xf;
		if(StartUnalignedBytes > 0)
		{
			MyDMATransferSmall(DmaTag,LocalAddress,MainAddress,StartUnalignedBytes);
		}
		TransferredBytes += StartUnalignedBytes;

		// Transfer the 16-byte aligned data.
		const UINT AlignedBytes = (NumBytes - TransferredBytes) & ~0xf;
		if(AlignedBytes > 0)
		{
			cellDmaLargePutf((BYTE*)LocalAddress + TransferredBytes,UINT(MainAddress) + TransferredBytes,AlignedBytes,DmaTag,0,0);
		}
		TransferredBytes += AlignedBytes;
	}

	// Transfer any data at the end which isn't 16-byte aligned.
	const UINT EndUnalignedBytes = NumBytes - TransferredBytes;
	if(EndUnalignedBytes > 0)
	{
		MyDMATransferSmall(DmaTag,(BYTE*)LocalAddress + TransferredBytes,(BYTE*)MainAddress + TransferredBytes,EndUnalignedBytes);
	}
}

void cellSpursJobMain2(CellSpursJobContext2 *JobContext, CellSpursJob256 *InJob)
{
	FSpursJobType* Job	= (FSpursJobType*) InJob;

	// Retrieve input from DMA.
	void* DMAInput[26];
	cellSpursJobGetPointerList( DMAInput, &Job->header, JobContext );

	// Determine which task is requested, and execute the appropriate code.
	const DWORD TaskId = GetUserData<FBaseParameters>(Job).TaskId;
	static const QWORD PixelShaderPatchTask = 0;
	static const QWORD PreVertexShaderCullingTask = 1;

	switch(TaskId)
	{
	case PixelShaderPatchTask:
		{
			BYTE* const Buffer = (BYTE*) DMAInput[0];
			DWORD* GPixelShaderConstants = (DWORD*) DMAInput[1];

			// Read the task's parameters.
			const FPixelShaderPatchingParameters& Parameters = GetUserData<FPixelShaderPatchingParameters>(Job);

			// Setup the helper pointers within the PixelShader Buffer.
			FPixelShaderInfo* Info	= (FPixelShaderInfo*) Buffer;
			BYTE* Microcode			= Buffer + sizeof(FPixelShaderInfo);
			BYTE* InfoBuffer		= Buffer + sizeof(FPixelShaderInfo) + Info->MicrocodeSize;
			BYTE* NumOccurrances	= (BYTE*) (InfoBuffer + Info->NumSamplers*sizeof(BYTE));
			WORD* Offsets			= (WORD*) (InfoBuffer + Info->NumSamplers*sizeof(BYTE) + Info->NumParameters*sizeof(BYTE));
			Offsets					= (WORD*) ALIGN( (DWORD)Offsets, 16 );

			// Patch the microcode.
			for ( DWORD ParameterIndex=0; ParameterIndex < Info->NumParameters; ++ParameterIndex )
			{
				GPixelShaderConstants[0] = ((GPixelShaderConstants[0] & 0xffff0000) >> 16) | ((GPixelShaderConstants[0] & 0x0000ffff) << 16);	// Vector.X
				GPixelShaderConstants[1] = ((GPixelShaderConstants[1] & 0xffff0000) >> 16) | ((GPixelShaderConstants[1] & 0x0000ffff) << 16);	// Vector.Y
				GPixelShaderConstants[2] = ((GPixelShaderConstants[2] & 0xffff0000) >> 16) | ((GPixelShaderConstants[2] & 0x0000ffff) << 16);	// Vector.Z
				GPixelShaderConstants[3] = ((GPixelShaderConstants[3] & 0xffff0000) >> 16) | ((GPixelShaderConstants[3] & 0x0000ffff) << 16);	// Vector.W
				DWORD NumOffsets = *NumOccurrances++;
				while ( NumOffsets-- )
				{
					DWORD* Patch = (DWORD*) (Microcode + *Offsets++);
					Patch[0] = GPixelShaderConstants[0];
					Patch[1] = GPixelShaderConstants[1];
					Patch[2] = GPixelShaderConstants[2];
					Patch[3] = GPixelShaderConstants[3];
				}
				GPixelShaderConstants += 4;
			}

			// Transfer the patched microcode from LS to the proper place in the patchbuffer in local memory.
			MyDMATransfer(JobContext->dmaTag,Microcode,(void*)Parameters.MicrocodeOut,Info->MicrocodeSize);

			BYTE *IOBuffer = (BYTE*)JobContext->ioBuffer;
			BYTE *GcmBuffer = IOBuffer + 0;
			BYTE *FenceBuffer = IOBuffer + ( (Parameters.CommandBufferSize + 16 + 0xF) & ~0xF );

			// Setup an LS command buffer for the hole.
			CellGcmContextData GcmContext;
			SetupGcmContext( GcmBuffer, &GcmContext, (DWORD)Parameters.CommandBufferHole, Parameters.CommandBufferSize );
			cellGcmSetNopCommand( &GcmContext, Parameters.CommandBufferSize / sizeof(DWORD) );

			// Read back the microcode to ensure the write has completed
			// From RSX-Fragment_Program_Constant_Patch_e.pdf:
			//     After SPU patching constants by using put, SPU has to read the last
			//     patched address (*) using getf, and then, modify the JTS to NOP.
			//      (*)Note: To getf from the patched address here is in order to
			//      guarantee that the patching has completed safely before RSX(tm)
			//      starts reading it.
			//      Please note that it is not necessary to confirm the value itself
			//      that is read here. And, the access size of the getf here should be
			//      4B (accesses more than 4B would result in less efficiency.) Also,
			//      even if more than one constant are patched, reading the last patched
			//      address just one time is necessary.
			{
				uint32_t LastOffset = *(--Offsets);
				uint8_t ReadBackBuffer[4] __attribute__((aligned(128)));
				cellDmaSmallGetf(ReadBackBuffer, (uint64_t)(ALIGN((DWORD)((BYTE*)Parameters.MicrocodeOut + LastOffset), 16)), 4, JobContext->dmaTag, 0, 0);
				cellDmaWaitTagStatusAll(1 << JobContext->dmaTag);
			}

			// Transfer the NOPs to overwrite the jump-to-self in the GPU command buffer.
			// Note: This is "fenced", so that all previous DMA transfers is completed before this one starts.
			MyDMATransfer(JobContext->dmaTag,GcmContext.begin,Parameters.CommandBufferHole,Parameters.CommandBufferSize);

			// Write the job's fence.
			if (FenceBuffer == 0) spu_printf("Patch reserved WriteFence() buffer at 0x%p\n", FenceBuffer);
			WriteJobFence(JobContext, Job, FenceBuffer);
		}
		break;


	case PreVertexShaderCullingTask:
		{
			// Read the task's parameters.
			const FPreVertexShaderCullingParameters& Parameters = GetUserData<FPreVertexShaderCullingParameters>(Job);

			debug_printf("SPUID=%u NumTriangles=%u IndexBias=%u NumVertices=%u\n",cellSpursGetCurrentSpuId(), Parameters.NumTriangles,Parameters.IndexBias,Parameters.NumVertices);

			EdgeGeomSpuConfigInfo EdgeGeomConfig;
			EdgeGeomConfig.flagsAndUniformTableCount = 1 | EDGE_GEOM_FLAG_INCLUDES_EXTRA_UNIFORM_TABLE
				| EDGE_GEOM_FLAG_STATIC_GEOMETRY_FAST_PATH; // since we're not outputting vertex data
			EdgeGeomConfig.commandBufferHoleSize = Parameters.CommandBufferReserveSize / sizeof(qword);
			EdgeGeomConfig.inputVertexFormatId = EDGE_GEOM_SPU_VERTEX_FORMAT_F32c3;
			EdgeGeomConfig.secondaryInputVertexFormatId = 0;
			EdgeGeomConfig.outputVertexFormatId = EDGE_GEOM_RSX_VERTEX_FORMAT_F32c3;
			EdgeGeomConfig.vertexDeltaFormatId = 0;
			EdgeGeomConfig.indexesFlavorAndSkinningFlavor = EDGE_GEOM_SKIN_NONE | (EDGE_GEOM_INDEXES_U16_TRIANGLE_LIST_CW << 4);
			EdgeGeomConfig.numVertexes = ALIGN(Parameters.NumVertices,4);
			EdgeGeomConfig.numIndexes = ALIGN(Parameters.NumTriangles,4)*3;
			EdgeGeomConfig.indexesOffset = 0xFFFFFFFF;//(DWORD)Job->workArea.dmaList[Parameters.IndexBufferDMAIndex];

			EdgeGeomViewportInfo ViewportInfo;

			// Edge expects the ViewProjection matrix (in ViewportInfo) to be separate from the LocalToWorld matrix
			// (passed to edgeGeomInitialize).  The matrix we pass up with the job data is the combined LocalToProjection matrix,
			// so we just create a dummy identity matrix for LocalToWorld.
			EdgeGeomLocalToWorldMatrix LocalToWorldMatrix;
			static const FLOAT IdentityMatrix[12] =
			{
				1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0
			};
			for(UINT Index = 0;Index < ARRAY_COUNT(LocalToWorldMatrix.matrixData);Index++)
			{
				LocalToWorldMatrix.matrixData[Index] = IdentityMatrix[Index];
			}

			// Copy and transpose the local view projection matrix.
			for(UINT RowIndex = 0;RowIndex < 4;RowIndex++)
			{
				for(UINT ColumnIndex = 0;ColumnIndex < 4;ColumnIndex++)
				{
					ViewportInfo.viewProjectionMatrix[RowIndex * 4 + ColumnIndex] = Parameters.LocalViewProjectionMatrix[ColumnIndex * 4 + RowIndex];
				}
			}

			ViewportInfo.scissorArea[0] = 0;
			ViewportInfo.scissorArea[1] = 0;
			ViewportInfo.scissorArea[2] = 1280; // TODO: this should be passed in as input;
			ViewportInfo.scissorArea[3] = 720;  // we shouldn't assume 720p resolution!

			ViewportInfo.depthRange[0] = 0.0f;
			ViewportInfo.depthRange[1] = 1.0f;

			ViewportInfo.viewportScales[0] = ViewportInfo.scissorArea[2] * .5f;
			ViewportInfo.viewportScales[1] = ViewportInfo.scissorArea[3] * -.5f;
			ViewportInfo.viewportScales[2] = (ViewportInfo.depthRange[1] - ViewportInfo.depthRange[0]) * .5f;
			ViewportInfo.viewportScales[3] = 0.0f;

			ViewportInfo.viewportOffsets[0] = ViewportInfo.scissorArea[2] * +.5f;
			ViewportInfo.viewportOffsets[1] = ViewportInfo.scissorArea[3] * .5f;
			ViewportInfo.viewportOffsets[2] = (ViewportInfo.depthRange[1] + ViewportInfo.depthRange[0]) * .5f;
			ViewportInfo.viewportOffsets[3] = 0.0f;
			ViewportInfo.sampleFlavor = CELL_GCM_SURFACE_CENTER_1;

			EdgeGeomCustomVertexFormatInfo customFormatInfo =
			{
				0,0,0,0, // stream description pointers; unused
				0,0,0,0,0,0 // custom callback function pointers; unused
			};

			EdgeGeomSpuContext EdgeGeomContext;
			edgeGeomInitialize(
				&EdgeGeomContext,
				&EdgeGeomConfig,
				JobContext->sBuffer, Job->header.sizeScratch << 4,
				JobContext->ioBuffer, Job->header.sizeInOrInOut,
				JobContext->dmaTag,
				&ViewportInfo,
				&LocalToWorldMatrix,
				&customFormatInfo
				);

			void* Vertices = (BYTE*)DMAInput[Parameters.VertexBufferDMAIndex];
			WORD* Indices = (WORD*)DMAInput[Parameters.IndexBufferDMAIndex];

			// Decode the vertex data.
			edgeGeomDecompressVertexes(&EdgeGeomContext,Vertices,0);
			
			// Not needed for static geometry path
			//edgeGeomProcessBlendShapes(&EdgeGeomContext, 0, 0); // unused
			//edgeGeomSkinVertexes();

			// Decode the index data. (we're not using compressed indices, so this will be a no-op)
			edgeGeomDecompressIndexes(&EdgeGeomContext,Indices);

			// Cull triangles which are outside the frustum or backfacing.
			UINT NumVisibleIndices = edgeGeomCullTriangles(
				&EdgeGeomContext,
				Parameters.bReverseCulling ?
					EDGE_GEOM_CULL_BACKFACES_AND_FRUSTUM :
					EDGE_GEOM_CULL_FRONTFACES_AND_FRUSTUM,
					Parameters.IndexBias
				);

			if (NumVisibleIndices == 0) // early out if no triangles are visible
			{
				CellGcmContextData gcmCtx;
				edgeGeomBeginCommandBufferHole(&EdgeGeomContext, &gcmCtx, (DWORD)Parameters.CommandBufferReserve, 0, 0);
				edgeGeomEndCommandBufferHole(&EdgeGeomContext, &gcmCtx, (DWORD)Parameters.CommandBufferReserve, 0, 0);

				// Write the job's fence.
				uint8_t *FenceBuffer = (uint8_t*)edgeGeomGetFreePtr(&EdgeGeomContext);
				edgeGeomSetFreePtr(&EdgeGeomContext, FenceBuffer + 128);
				WriteJobFence(JobContext, Job, FenceBuffer);

				return;
			}

			// Allocate space for the visible triangle vertex indices.
			EdgeGeomAllocationInfo AllocationInfo;
			const UINT AllocationSize = edgeGeomCalculateDefaultOutputSize(&EdgeGeomContext, NumVisibleIndices);
			if (!edgeGeomAllocateOutputSpace(&EdgeGeomContext, (DWORD)Parameters.OutputBufferInfo, AllocationSize, &AllocationInfo,
				cellSpursGetCurrentSpuId()))
			{
				// Allocation failed
				CellGcmContextData gcmCtx;
				edgeGeomBeginCommandBufferHole(&EdgeGeomContext, &gcmCtx, (DWORD)Parameters.CommandBufferReserve, 0, 0);
				edgeGeomEndCommandBufferHole(&EdgeGeomContext, &gcmCtx, (DWORD)Parameters.CommandBufferReserve, 0, 0);

				// Write the job's fence.
				uint8_t *FenceBuffer = (uint8_t*)edgeGeomGetFreePtr(&EdgeGeomContext);
				edgeGeomSetFreePtr(&EdgeGeomContext, FenceBuffer + 128);
				WriteJobFence(JobContext, Job, FenceBuffer);

				return;
			}
			EdgeGeomLocation IndicesOutputLocation;
			edgeGeomOutputIndexes(&EdgeGeomContext, NumVisibleIndices, &AllocationInfo, &IndicesOutputLocation);
			
			// Not needed for static geometry path
			//edgeGeomCompressVertexes(&EdgeGeomContext);
			//EdgeGeomLocation VerticesOutputLocation;
			//edgeGeomOutputVertexes(&EdgeGeomContext, &AllocationInfo, &VerticesOutputLocation);

			CellGcmContextData gcmCtx;
			edgeGeomBeginCommandBufferHole(&EdgeGeomContext, &gcmCtx, (DWORD)Parameters.CommandBufferReserve, &AllocationInfo, 1);
			cellGcmSetDrawIndexArray(&gcmCtx, CELL_GCM_PRIMITIVE_TRIANGLES, NumVisibleIndices, CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16,
				IndicesOutputLocation.location, IndicesOutputLocation.offset);
			edgeGeomEndCommandBufferHole(&EdgeGeomContext, &gcmCtx, (DWORD)Parameters.CommandBufferReserve, &AllocationInfo, 1);

			// Write the job's fence.
			uint8_t *FenceBuffer = (uint8_t*)edgeGeomGetFreePtr(&EdgeGeomContext);
			edgeGeomSetFreePtr(&EdgeGeomContext, FenceBuffer + 128);
			WriteJobFence(JobContext, Job, FenceBuffer);
		}
		break;
	};
}
