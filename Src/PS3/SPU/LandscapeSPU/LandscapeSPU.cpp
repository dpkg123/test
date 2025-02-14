/*=============================================================================
	LandscapeSPU.cpp: SPU replacement for Landscape VTF rendering
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include <math.h>
#include "SPU.h"
#include "..\..\Inc\PS3Landscape.h"

#if 1
#define debug_printf(...) spu_printf(__VA_ARGS__);
#else
#define debug_printf(...) nop_printf(__VA_ARGS__);
static void nop_printf(char* Format,...)
{
}
#endif

vector float Broadcast(float value)
{
	vector float result = spu_promote(value,0);
	const vector unsigned char mask = {0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3};
	return spu_shuffle(result, result, mask);
}

inline int appRound(float f)
{
	return (int)(truncf(f + 0.5f));
}



// Unpack vertex heights data packed in words, into float vectors one height per component
inline void UnpackInputVerts(vector unsigned char* SrcData, vector float* OutputData, int NumVerts)
{
	// Patterns for shuffle
	const vector unsigned char Pattern1 = { 0x80, 0x80, 0, 1, 0x80, 0x80, 2, 3, 0x80, 0x80, 4, 5, 0x80, 0x80, 6, 7 };
	const vector unsigned char Pattern2 = { 0x80, 0x80, 8, 9, 0x80, 0x80, 10, 11, 0x80, 0x80, 12, 13, 0x80, 0x80, 14, 15 };

	for( INT x=0;x<NumVerts;x+=8)
	{
		*OutputData++ = spu_convtf((vector unsigned int)spu_shuffle(*SrcData, *SrcData, Pattern1), 0);
		*OutputData++ = spu_convtf((vector unsigned int)spu_shuffle(*SrcData, *SrcData, Pattern2), 0);
		SrcData++;
	}
}

#define MAX_COMPONENT_SIZE 256
#define OUTPUTTAG 3
#define COMMAND_BUFFER_HOLE_SIZE 16

// Place to load the current value of RSX fence
DWORD SpuRsxFence  __attribute__((aligned(128)));

// input data
vector unsigned char CurrLODInputData[2][MAX_COMPONENT_SIZE/8]  __attribute__((aligned(128)));
vector unsigned char NextLODInputData[2][MAX_COMPONENT_SIZE/16]  __attribute__((aligned(128)));

// output row
vector unsigned int OutputRowData[2][MAX_COMPONENT_SIZE/4]  __attribute__((aligned(128)));

// temporary data for current row
vector float CurrLODRow[MAX_COMPONENT_SIZE/4] __attribute__((aligned(128)));
vector float NextLODRow[MAX_COMPONENT_SIZE/8] __attribute__((aligned(128)));
vector float PackedMorphAlphas[2][MAX_COMPONENT_SIZE/4] __attribute__((aligned(128)));


struct FLandscapeSPUEnvironment
{
	FLandscapeSPUEnvironment(FSpursJobType* Job)
	:	Parameters(GetUserData<FLandscapeJobParameters>(Job))
	,	CurrLODSubsectionSizeVerts( Parameters.SubsectionSizeVerts >> (Parameters.CurrentLOD) )
	,	NextLODSubsectionSizeVerts( Parameters.SubsectionSizeVerts >> (Parameters.CurrentLOD+1) )
	,	NextLODFactor( (float)(NextLODSubsectionSizeVerts-1) / (float)(CurrLODSubsectionSizeVerts-1) )
	,	BaseLODInvFactor( (float)(Parameters.SubsectionSizeVerts-1) / (float)(CurrLODSubsectionSizeVerts-1) )
	,	vBaseLODInvFactor( Broadcast(BaseLODInvFactor) )
	,	vLodDistancesValuesX( Broadcast(Parameters.LodDistancesValues[0]) )
	,	vLodDistancesValuesZ( Broadcast(Parameters.LodDistancesValues[2]) )
	,	vLodDistancesValuesW( Broadcast(Parameters.LodDistancesValues[3]) )
	{}

	const FLandscapeJobParameters& Parameters;
	int CurrLODSubsectionSizeVerts;
	int NextLODSubsectionSizeVerts;
	float NextLODFactor;
	float BaseLODInvFactor;
	vector float vBaseLODInvFactor;
	vector float vLodDistancesValuesX;
	vector float vLodDistancesValuesZ;
	vector float vLodDistancesValuesW;
};

/* Calculate the morph values for a row of data. Returns true if any morph factor was nonzero */
inline bool CalcMorphAlphas( const FLandscapeSPUEnvironment& Env, int y, vector float* PackedMorphAlphaOutput)
{
	float fBaseLODy = (float)y * Env.BaseLODInvFactor;

	if( Env.Parameters.HeightDataNextLodBase == 0 )
	{
		return false;
	}

	// Y component of distance squared
	float fDistSqy = Env.Parameters.LodDistancesValues[1] - fBaseLODy;
	fDistSqy *= fDistSqy;
	vector float vDistSqy = Broadcast(fDistSqy);

	vector unsigned int AnyGreaterThanZeroMask = {0,0,0,0};

	for( int x=0;x<Env.CurrLODSubsectionSizeVerts;x+=4 )
	{
		const vector float VertOffsets = { 0.f, 1.f, 2.f, 3.f };
		vector float vBaseLODx = spu_mul( spu_add( VertOffsets, Broadcast((float)x) ), Env.vBaseLODInvFactor );

		// X components of distance squared for 4 verts
		vector float vDistSqx = spu_sub( Env.vLodDistancesValuesX, vBaseLODx );
		vDistSqx = spu_mul(vDistSqx, vDistSqx);

		// distance squared for 4 verts
		vector float vLodDistanceSq = spu_add( vDistSqx, vDistSqy );

		// calculate amount of morphing based on LOD.
		vector float vLodDistance = spu_re( spu_rsqrte(vLodDistanceSq) );

		// calculate morph alpha
		vector float MorphAlpha = spu_mul( spu_sub(vLodDistance, Env.vLodDistancesValuesZ), Env.vLodDistancesValuesW );
		// if greater than 0, all bits are 1, otherwise all bits are 0.
		const vector float AllZeros = {0.f,0.f,0.f,0.f};
		const vector float AllOnes = {1.f,1.f,1.f,1.f};

		vector unsigned int GreaterThanZeroMask = spu_cmpgt( MorphAlpha, AllZeros );

		// remember if any element was greater than 0
		AnyGreaterThanZeroMask = spu_or( AnyGreaterThanZeroMask, GreaterThanZeroMask );

		// MorphAlpha = Max(MorphAlpha, 0)
		MorphAlpha = (vector float)spu_and((vector unsigned int)MorphAlpha, GreaterThanZeroMask);

		// if greater than 1, all bits are 0, otherwise all bits are 1.
		vector unsigned int LessThanOneMask = spu_cmpgt( AllOnes, MorphAlpha );
		vector unsigned int GreaterThanOneMask = spu_cmpgt( MorphAlpha, AllOnes );
		MorphAlpha = (vector float)spu_and((vector unsigned int)MorphAlpha, LessThanOneMask);		// zero out any values > 1.
		vector unsigned int GreaterThanOneOnes = spu_and((vector unsigned int)AllOnes, GreaterThanOneMask);

		MorphAlpha = (vector float)spu_or((vector unsigned int)MorphAlpha, GreaterThanOneOnes);

		// set the values > 1.0 to 1.0
		*PackedMorphAlphaOutput++ = MorphAlpha;
	}

	return (spu_extract((vector unsigned long long)AnyGreaterThanZeroMask, 0) | spu_extract((vector unsigned long long)AnyGreaterThanZeroMask, 1)) != 0L;
}

inline vector float CalcNextLODValues( const FLandscapeSPUEnvironment& Env, int x )
{
	// Calculate four X values for next LOD.
	const vector float VertOffsets = { 0, 1, 2, 3 };
	const vector float HalfRound = { 0.5f, 0.5f, 0.5f, 0.5f }; // 0.5 is for round
	vector unsigned int NextLodXs = spu_convtu( spu_add( spu_mul( spu_add( VertOffsets, Broadcast((float)x) ), Broadcast(Env.NextLODFactor) ), HalfRound ), 0);
	// now NextLodXs = per component round((float)x * NextLODFactor);

	// Get vector NextLodX0, the first NextLodX
	const vector unsigned char FirstElementMask = {0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3};
	const vector unsigned int AlignmentMask = {~0x3,~0x3,~0x3,~0x3};
	vector unsigned int NextLodX0s = spu_and( spu_shuffle(NextLodXs, NextLodXs, FirstElementMask), AlignmentMask);

	unsigned int NextLodX0 = spu_extract(NextLodX0s, 0);

	// find offset for each vert from NextLodX0 
	NextLodXs = spu_sub( NextLodXs, NextLodX0s );
	// eg now NextLodsXs = { 3, 3, 4, 4 }
	// As nextLODFactor < 0.5, it should not be possible to have values other than 0 and 1, 2 or 3 for four neighboring verts.

	vector unsigned int NextLodXs0 = spu_rl( NextLodXs, 2 );	// Multiply by 4 - this gets the index of the first byte
	vector unsigned int NextLodXs1 = spu_add( NextLodXs0, 1 );	// 2nd byte
	vector unsigned int NextLodXs2 = spu_add( NextLodXs0, 2 );  // 3rd byte
	vector unsigned int NextLodXs3 = spu_add( NextLodXs0, 3 );  // 4th byte

	// These masks move the index bytes into the appropriate places to make another mask that will select the 4 NextLODRow values.
	const vector unsigned char ConvertMask0 = { 3,0x80,0x80,0x80,7,0x80,0x80,0x80,11,0x80,0x80,0x80,15,0x80,0x80,0x80 }; 
	const vector unsigned char ConvertMask1 = { 16,3,18,19,20,7,22,23,24,11,26,27,28,15,30,31 }; 
	const vector unsigned char ConvertMask2 = { 16,17,3,19,20,21,7,23,24,25,11,27,28,29,15,31 }; 
	const vector unsigned char ConvertMask3 = { 16,17,18,3,20,21,22,7,24,25,26,11,28,29,30,15 }; 

	// Combine generate the final mask that selects the 4 values from NextLODRow.
	vector unsigned char SelectMask = (vector unsigned char)spu_shuffle( NextLodXs0, NextLodXs0, ConvertMask0 );
	SelectMask = (vector unsigned char)spu_shuffle( (vector unsigned char)NextLodXs1, SelectMask, ConvertMask1 );
	SelectMask = (vector unsigned char)spu_shuffle( (vector unsigned char)NextLodXs2, SelectMask, ConvertMask2 );
	SelectMask = (vector unsigned char)spu_shuffle( (vector unsigned char)NextLodXs3, SelectMask, ConvertMask3 );

#if 0
	debug_printf("x=%d, NextLodX's={%d,%d,%d,%d} ",
		x, spu_extract(NextLodXs, 0),spu_extract(NextLodXs, 1),spu_extract(NextLodXs, 2),spu_extract(NextLodXs, 3));
	debug_printf("  SelectMask is %d,%d,%d,%d, %d,%d,%d,%d, ",
		spu_extract(SelectMask, 0),spu_extract(SelectMask, 1),spu_extract(SelectMask, 2),spu_extract(SelectMask, 3),
		spu_extract(SelectMask, 4),spu_extract(SelectMask, 5),spu_extract(SelectMask, 6),spu_extract(SelectMask, 7)
		);
	debug_printf("%d,%d,%d,%d, %d,%d,%d,%d\n", 
		spu_extract(SelectMask, 8),spu_extract(SelectMask, 9),spu_extract(SelectMask, 10),spu_extract(SelectMask, 11),
		spu_extract(SelectMask, 12),spu_extract(SelectMask, 13),spu_extract(SelectMask, 14),spu_extract(SelectMask, 15)
		);
#endif

	// Mask should be something like { 12,13,14,15,12,13,14,15,16,17,18,19,16,17,18,19 };
	return (vector float)spu_shuffle( NextLODRow[NextLodX0>>2], NextLODRow[(NextLodX0>>2) + 1], SelectMask );
}

void cellSpursJobMain2(CellSpursJobContext2 *JobContext, CellSpursJob256 *InJob)
{
	FSpursJobType* Job	= (FSpursJobType*) InJob;

	// Retrieve input from DMA.
	void* DMAInput[26];
	cellSpursJobGetPointerList( DMAInput, &Job->header, JobContext );

	// Find the pointer for the SPURS fence
	BYTE *IOBuffer = (BYTE*)JobContext->ioBuffer;
	BYTE *GcmBuffer = IOBuffer + 0;
	BYTE *FenceBuffer = IOBuffer + ( (COMMAND_BUFFER_HOLE_SIZE + 16 + 0xF) & ~0xF );

	// Read the job's parameters and precompute some items
	const FLandscapeSPUEnvironment Env(Job);

	// Round up data transfer size to nearest 16 bytes
	DWORD OutputRowDMASize = (Env.CurrLODSubsectionSizeVerts*4 + 15) & (~0xF);
	DWORD OutputAddress = Env.Parameters.OutputBase;


	int OutputDblBuf = 0;
	int InputDblBuf = 0;
	int OldNextLODy1 = -1;
	bool HaveNewNextLODData[2] = {false, false};
	bool ShouldMorphRow[2] = {false, false};

	DWORD PackedInputDataCurrLod = Env.Parameters.HeightDataCurrentLodBase;

	// Start DMAs for the first row of data
	cellDmaGet(CurrLODInputData[0], PackedInputDataCurrLod, Env.Parameters.HeightDataCurrentLodPitch, JobContext->dmaTag, 0, 0 );

	// Calculate morph alphas for first row
	if( CalcMorphAlphas( Env, 0, PackedMorphAlphas[0] ) )
	{
		ShouldMorphRow[0] = true;
		cellDmaGet(NextLODInputData[0], Env.Parameters.HeightDataNextLodBase, Env.Parameters.HeightDataNextLodPitch, JobContext->dmaTag, 0, 0 );
		OldNextLODy1 = 0;
		HaveNewNextLODData[0] = true;
	}
	else
	{
		ShouldMorphRow[0] = false;
	}

	// read the current RSX fence value
	cellDmaSmallGetf( &SpuRsxFence, Env.Parameters.SpuRsxFencePtr, sizeof(DWORD), JobContext->dmaTag, 0, 0 );

	for( int y=0;y<Env.CurrLODSubsectionSizeVerts;y++ )
	{
		// wait for previous row of data
		cellDmaWaitTagStatusAll(1<<JobContext->dmaTag);

		if( y<Env.CurrLODSubsectionSizeVerts-1 )
		{
			// Start transfer of next row of data while we process this one.
			PackedInputDataCurrLod += Env.Parameters.HeightDataCurrentLodPitch;
			cellDmaGet(CurrLODInputData[1-InputDblBuf], PackedInputDataCurrLod, Env.Parameters.HeightDataCurrentLodPitch, JobContext->dmaTag, 0, 0 );

			// Calc morph alphas for next row
			if( CalcMorphAlphas( Env, y+1, PackedMorphAlphas[1-InputDblBuf] ) )
			{
				ShouldMorphRow[1-InputDblBuf] = true;
				// find the index of the following row in the next LOD
				int NextLODy1 = appRound((float)(y+1) * Env.NextLODFactor);

				if( OldNextLODy1 != NextLODy1 )
				{
					cellDmaGet(NextLODInputData[1-InputDblBuf], Env.Parameters.HeightDataNextLodBase + Env.Parameters.HeightDataNextLodPitch * NextLODy1, Env.Parameters.HeightDataNextLodPitch, JobContext->dmaTag, 0, 0 );
					HaveNewNextLODData[1-InputDblBuf] = true;
					OldNextLODy1 = NextLODy1;
				}
			}
			else
			{
				ShouldMorphRow[1-InputDblBuf] = false;
			}
		}

		if( y == 0 )
		{
			// Wait to be sure RSX has caught up before we try to output anything.
			while( Env.Parameters.MySpuRsxFence - SpuRsxFence > LANDSCAPE_NUM_BUFFERS )
			{
				// Read the fence value again
				cellDmaSmallGetf( &SpuRsxFence, Env.Parameters.SpuRsxFencePtr, sizeof(DWORD), JobContext->dmaTag, 0, 0 );
				cellDmaWaitTagStatusAll(1<<JobContext->dmaTag);
			}
		}
		else
		{
			// Save previous output
			cellDmaPut(OutputRowData[1-OutputDblBuf], OutputAddress, OutputRowDMASize, JobContext->dmaTag, 0, 0 );
			// This stride is required because of the way we use a fixed vertex buffer for all landscapes
			OutputAddress += MAX_COMPONENT_SIZE*4;
		}

		UnpackInputVerts( CurrLODInputData[InputDblBuf], CurrLODRow, Env.CurrLODSubsectionSizeVerts );
		
		// If we also received a new row of NextLOD data last loop, unpack that too.
		if( HaveNewNextLODData[InputDblBuf] )
		{
			UnpackInputVerts( NextLODInputData[InputDblBuf], NextLODRow, Env.NextLODSubsectionSizeVerts );
			HaveNewNextLODData[InputDblBuf] = false;
		}

		// CurrLODRow and optionally NextLODRow should now contain valid data for this row.

		// Do interpolation and pack output	
		vector unsigned int* OutputRow = OutputRowData[OutputDblBuf];
		if( ShouldMorphRow[InputDblBuf] )
		{
			for( int x=0;x<Env.CurrLODSubsectionSizeVerts;x+=4 )
			{
				// MorphAlphas = morph values for 4 verts.
				vector float MorphAlphas = PackedMorphAlphas[InputDblBuf][(x>>2)];	
				vector float NextLODValues = CalcNextLODValues( Env, x );

				// Lerp between the current and next LOD
				const vector float AllOnes = {1.f,1.f,1.f,1.f};
				vector float LerpData = spu_add( spu_mul(CurrLODRow[x>>2], spu_sub(AllOnes, MorphAlphas)), spu_mul(NextLODValues, MorphAlphas) );

				// convert to bytes and output:
				*OutputRow++ = spu_convtu(LerpData, 0);
			}
		}
		else
		{
			// No morphing
			for( int x=0;x<Env.CurrLODSubsectionSizeVerts;x+=4 )
			{
				// convert to bytes and output:
				*OutputRow++ = spu_convtu(CurrLODRow[(x>>2)], 0);
			}
		}

		// Swap double buffers
		InputDblBuf = 1-InputDblBuf;
		OutputDblBuf = 1-OutputDblBuf;
	}
	// Save the final row.
	cellDmaPut(OutputRowData[1-OutputDblBuf], OutputAddress, OutputRowDMASize, JobContext->dmaTag, 0, 0 );

	// clear out the JTS's in the command buffer hole
	CellGcmContextData GcmContext;
	SetupGcmContext( GcmBuffer, &GcmContext, Env.Parameters.CommandBufferHole, LANDSCAPE_SPU_CMD_BUFFER_HOLE_SIZE );
	cellGcmSetNopCommand( &GcmContext, LANDSCAPE_SPU_CMD_BUFFER_HOLE_SIZE / sizeof(DWORD) );

	// Send it out, freeing up RSX
	// Uses 'fenced' mode to ensure the previous DMAs are complete.
	cellDmaPutf(GcmContext.begin, Env.Parameters.CommandBufferHole, LANDSCAPE_SPU_CMD_BUFFER_HOLE_SIZE, JobContext->dmaTag, 0, 0 );

//	debug_printf("LandscapeSPU cellSpursJobMain2 %d %d exiting\n", Env.Parameters.CurrentLOD, Env.Parameters.SubsectionSizeVerts);
	// clear out the SPURS job fence.
	WriteJobFence(JobContext, Job, FenceBuffer);
}