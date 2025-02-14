/*=============================================================================
	PS3RHI.cpp: Null RHI implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "BestFitAllocator.h"
#include "../../Engine/Src/ScenePrivate.h"

#if USE_PS3_RHI

// Global definitions
FSurfaceRHIRef				GPS3RHIRenderTarget;
FSurfaceRHIRef				GPS3RHIDepthTarget;

// Stat declarations.
DECLARE_STATS_GROUP(TEXT("PS3RHI"), STATGROUP_PS3RHI);
DECLARE_CYCLE_STAT(TEXT("Set render target"),	STAT_SetRenderTarget,	STATGROUP_PS3RHI);
DECLARE_CYCLE_STAT(TEXT("Draw primitive"),		STAT_DrawPrimitive,		STATGROUP_PS3RHI);
DECLARE_CYCLE_STAT(TEXT("Draw primitive UP"),	STAT_DrawPrimitiveUP,	STATGROUP_PS3RHI);
DECLARE_CYCLE_STAT(TEXT("Clear"),				STAT_Clear,				STATGROUP_PS3RHI);
DECLARE_CYCLE_STAT(TEXT("Swap buffers"),		STAT_SwapBuffers,		STATGROUP_PS3RHI);
DECLARE_CYCLE_STAT(TEXT("Get Query Results"),	STAT_GetQueryResults,	STATGROUP_PS3RHI);
DECLARE_CYCLE_STAT(TEXT("CopyToResolveTarget GPU"),	STAT_CopyToResolveTarget_GPU, STATGROUP_PS3RHI);
DECLARE_CYCLE_STAT(TEXT("Stalled by GPU"),		STAT_BlockedByGPU,		STATGROUP_PS3RHI);
DECLARE_CYCLE_STAT(TEXT("Stalled by SPU"),		STAT_BlockedBySPU,		STATGROUP_PS3RHI);
DECLARE_CYCLE_STAT(TEXT("Landscape SPU"),		STAT_LandscapeSPU,		STATGROUP_PS3RHI);

void RHIInit()
{
	if( !GIsRHIInitialized )
	{
		// Set the RHI capabilities.
		GSupportsHardwarePCF = TRUE;
		GSupportsQuads = TRUE;
		GPixelCenterOffset = 0.0f;
		GMaxPerObjectShadowDepthBufferSizeX = 512;
		GMaxPerObjectShadowDepthBufferSizeY = 512;
		GMaxWholeSceneDominantShadowDepthBufferSize = 512;
		GSupportsEmulatedVertexInstancing = TRUE;
		GRHIShaderPlatform = SP_PS3;

		// The hardware would support it but for now deactivated - as there is a bug that needs deeper investigation
		// and the used method (vertex texture fetch) is expected to be too slow on Playstation 3.
		GSupportsVertexTextureFetch = FALSE;

		// @todo - just use RGBA8 for G8 instead for now
		GSupportsRenderTargetFormat_PF_G8 = FALSE;

		// RHI can now startup
		GIsRHIInitialized = TRUE;
	}
}

void RHIAcquireThreadOwnership()
{
	// Nothing to do
}

void RHIReleaseThreadOwnership()
{
	// Nothing to do
}

/**
 * Perform PS3 GCM/RHI initialization that needs to wait until after appInit has happened
 */
void PS3PostAppInitRHIInitialization()
{
	// allocate memory for memory pools (may use GConfig, so we needed to delay this)
	GPS3Gcm->InitializeMemoryPools();

	// fixup resources that couldn't be initialized before now
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitDynamicRHI();
	}
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitRHI();
	}

	// allocate the render targets early, as they can't go into fallback (host) gpu memory if local gpu mem fills up (unlike meshes)
	GSceneRenderTargets.Allocate(GScreenWidth, GScreenHeight);

	// RHI can now startup
	GIsRHIInitialized = TRUE;
}

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

/**
 * Retrieves texture memory stats.
 *
 * @return FALSE, indicating that out variables were left unchanged.
 */
UBOOL RHIGetTextureMemoryStats( INT& AllocatedMemorySize, INT& AvailableMemorySize, INT& PendingMemoryAdjustment )
{
	if ( GPS3Gcm->IsTexturePoolInitialized() )
	{
		INT LargestAvailableAllocation, NumFreeChunks;
		GPS3Gcm->GetTexturePool()->GetMemoryStats( AllocatedMemorySize, AvailableMemorySize, PendingMemoryAdjustment, LargestAvailableAllocation, NumFreeChunks );
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * Fills a texture with to visualize the texture pool memory.
 *
 * @param	TextureData		Start address
 * @param	SizeX			Number of pixels along X
 * @param	SizeY			Number of pixels along Y
 * @param	Pitch			Number of bytes between each row
 * @param	PixelSize		Number of bytes each pixel represents
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL RHIGetTextureMemoryVisualizeData( FColor* TextureData, INT SizeX, INT SizeY, INT Pitch, INT PixelSize )
{
	if ( GPS3Gcm->IsTexturePoolInitialized() )
	{
		GPS3Gcm->GetTexturePool()->GetTextureMemoryVisualizeData( TextureData, SizeX, SizeY, Pitch, PixelSize );
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


#if !FINAL_RELEASE
/**
 * Mode 0:	Dump color buffer RGB or depth buffer Z
 * Mode 1:	Dump color buffer Alpha or depth buffer stencil
 */
void DumpRenderTarget(FSurfaceRHIRef Surface, DWORD Mode/*=0*/)
{
	GPS3Gcm->BlockUntilGPUIdle();

	extern void* GRSXBaseAddress;

	DWORD SizeX = Surface->SizeX;
	DWORD SizeY = Surface->SizeY;
	DWORD Pitch = Surface->Pitch;
	DWORD Format = Surface->Format;
	BYTE* SrcBuffer = (BYTE*)Surface->MemoryOffset;
	SrcBuffer += DWORD(GRSXBaseAddress);

	TArray<FColor> Data;
	Data.Empty(SizeX * SizeY);
	Data.Add(SizeX * SizeY);
	FColor* Colors = (FColor*)Data.GetData();

	const TCHAR* Filename = NULL;
	BYTE* Src = NULL;
	INT Texel = 0;
	for (INT Y=0; Y < SizeY; ++Y)
	{
		BYTE *RowPtr = SrcBuffer + Y*Pitch;
		for (INT X=0; X < SizeX; ++X, ++Texel)
		{
			switch (Format)
			{
				case CELL_GCM_SURFACE_F_W16Z16Y16X16:
					{
						WORD* F16Src = ((WORD*)RowPtr) + 4 * X;
						FFloat16 F16;
							F16.Encoded = F16Src[Mode == 1 ? 3 : 0];
						Colors[Texel].R = (BYTE)((FLOAT)F16 * 0xFF);
							F16.Encoded = F16Src[Mode == 1 ? 3 : 1];
						Colors[Texel].G = (BYTE)((FLOAT)F16 * 0xFF);
							F16.Encoded = F16Src[Mode == 1 ? 3 : 2];
						Colors[Texel].B = (BYTE)((FLOAT)F16 * 0xFF);
					}
					Filename = Mode == 1 ? TEXT("Alpha_FP16") : TEXT("Surface_FP16");
					break;

				case CELL_GCM_SURFACE_A8R8G8B8:
					Src = RowPtr + X*4;
					Colors[Texel] = Mode == 1 ? FColor(Src[0], Src[0], Src[0], Src[0]) : FColor(Src[1], Src[2], Src[3], Src[0]);
					Filename = Mode == 1 ? TEXT("Alpha_RGBA") : TEXT("Surface_RGBA");
					break;

				case CELL_GCM_SURFACE_Z24S8:
					if ( Mode == 0 )
					{
						Src = RowPtr + X*4;
						FLOAT Depth = FLOAT( DWORD(Src[0])<<16 | DWORD(Src[1])<<8 | DWORD(Src[2]) ) / 16777215.0f;
						Colors[Texel] = FColor( FLinearColor(Depth, Depth, Depth, Depth) );
						Filename = TEXT("Depth_Z24S8");
					}
					else
					{
						Src = RowPtr + X*4;
						Colors[Texel] = FColor(Src[3] * 64, Src[3] * 32, Src[3] * 16, Src[3]);
						Filename = TEXT("Stencil_Z24S8");
					}
					break;

				case CELL_GCM_SURFACE_F_X32:
					Src = RowPtr + X*4;
					Colors[Texel].R = Colors[Texel].G = Colors[Texel].B = (BYTE)(Clamp(((FLOAT*)Src)[0], 0.0f, 1.0f) * 0xFF);
					Filename = TEXT("Surface_X32");
					break;
			}
		}
	}

	appCreateBitmap(*(GSys->ScreenShotPath * Filename), SizeX, SizeY, Colors, GFileManager);
}
#endif

#endif // USE_PS3_RHI
