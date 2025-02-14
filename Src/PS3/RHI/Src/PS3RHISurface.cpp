/*=============================================================================
	PS3RHISurface.cpp: PS3 surface/render target RHI definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3RHIPrivate.h"

#if USE_PS3_RHI


UBOOL RequireLinearTexture(INT Format, DWORD SizeX, DWORD SizeY)
{
	return (GPixelFormats[Format].Flags & PF_LINEAR) || !appIsPowerOfTwo(SizeX) || !appIsPowerOfTwo(SizeY);
}

/**
 * Calculate the pitch of a mip level
 * @param Format Unreal format for the texture
 * @param SizeX Width of base level of texture
 * @param MipIndex Mip level
 * @param bIsLinear Is the texture linear format?
 */
DWORD GetTexturePitch(INT Format, DWORD SizeX, DWORD MipIndex, UBOOL bIsLinear)
{
	// Mip maps for linear textures have the same pitch as mip 0.
	if ( bIsLinear )
	{
		MipIndex = 0;
	}

	DWORD MipSizeX = Max<DWORD>(SizeX >> MipIndex, 1);
	DWORD NumBlocksX = (MipSizeX + GPixelFormats[Format].BlockSizeX - 1) / GPixelFormats[Format].BlockSizeX;
	DWORD Pitch = NumBlocksX * GPixelFormats[Format].BlockBytes;
	return Pitch;
}

/**
 * Gets the mip's number of rows (in blocks), taking texture format into account
 * @param Format Unreal format for the texture
 * @param SizeY Height of base level of texture
 * @param MipIndex Mip level
 */
DWORD GetMipNumRows(INT Format, DWORD SizeY, DWORD MipIndex)
{
	DWORD MipSizeY = Max<DWORD>(SizeY >> MipIndex, 1);
	DWORD NumBlocksY = (MipSizeY + GPixelFormats[Format].BlockSizeY - 1) / GPixelFormats[Format].BlockSizeY;
	return NumBlocksY;
}

/**
 * Calculate the size of a mip
 * @param Format Unreal format for the texture
 * @param SizeX Width of base level of texture
 * @param SizeY Height of base level of texture
 * @param MipIndex Mip level
 * @param bIsLinear Is the texture linear format?
 */
DWORD GetTextureMipSize(INT Format, DWORD SizeX, DWORD SizeY, DWORD MipIndex, UBOOL bIsLinear)
{
	return GetTexturePitch(Format, SizeX, MipIndex, bIsLinear) * GetMipNumRows(Format, SizeY, MipIndex);
}

/**
 * Calculate the offset into the texture memory for the given mip level
 * @param Format Unreal format for the texture
 * @param SizeX Width of base level of texture
 * @param SizeY Height of base level of texture
 * @param MipIndex Mip level
 * @param bIsLinear Is the texture linear format?
 */
DWORD GetTextureMipOffset(INT Format, DWORD SizeX, DWORD SizeY, DWORD MipIndex, UBOOL bIsLinear)
{
	// add up size of mips up to this mip level
	DWORD MipOffset = 0;
	for (DWORD Index = 0; Index < MipIndex; Index++)
	{
		MipOffset += GetTextureMipSize(Format, SizeX, SizeY, Index, bIsLinear);
	}

	return MipOffset;
}

/** Mask to get the Gcm Format from CellGcmTexture::format. */
enum { GcmFormatMask = ~(CELL_GCM_TEXTURE_SZ | CELL_GCM_TEXTURE_LN | CELL_GCM_TEXTURE_UN | CELL_GCM_TEXTURE_NR) };

DWORD TextureToSurfaceFormat(EPixelFormat PixelFormat)
{
	switch (PixelFormat)
	{
		case PF_FloatRGB:
		case PF_FloatRGBA:
		case PF_G16R16F:
		case PF_G16R16F_FILTER:
		case PF_G32R32F:
		case PF_A16B16G16R16:
			return CELL_GCM_SURFACE_F_W16Z16Y16X16;

		case PF_A8R8G8B8:
			return CELL_GCM_SURFACE_A8R8G8B8;

		case PF_DepthStencil:
		case PF_ShadowDepth:
		case PF_FilteredShadowDepth:
			return CELL_GCM_SURFACE_Z24S8;

		case PF_R32F:
			return CELL_GCM_SURFACE_F_X32;

		case PF_G16R16:
			return CELL_GCM_SURFACE_A8R8G8B8;
	}

	appErrorf(TEXT("Using a bad texture format %d as a surface"), PixelFormat);
	return 0;
}


/*-----------------------------------------------------------------------------
	FPS3RHISurface
-----------------------------------------------------------------------------*/

/**
 * Constructor
 */
FPS3RHISurface::FPS3RHISurface(DWORD InSizeX, DWORD InSizeY, EPixelFormat InFormat, FTexture2DRHIRef InResolveTarget, DWORD Flags, DWORD InMultisampleType)
: SizeX(InSizeX)
, SizeY(InSizeY)
// pixel formats are texture formats, not surface formats, must convert
, Format(TextureToSurfaceFormat(InFormat))
, MultisampleType(InMultisampleType)
, ResolveTargetMemoryOffset(IsValidRef(InResolveTarget) ? InResolveTarget->MemoryOffset : 0)
, ResolveTarget(InResolveTarget)
{
	// surfaces can't be compressed
	check(InFormat != PF_DXT1 && InFormat != PF_DXT3 && InFormat != PF_DXT5);

	// Adjust to a valid tiled pitch (multiple of 256 bytes).
	Pitch = SizeX * GPixelFormats[InFormat].BlockBytes;

	// Try to allocate tiled memory first (unless we write once, then don't waste a tiled register)
	if ((Flags & TargetSurfCreate_WriteOnce) || !GPS3Gcm->AllocateTiledMemory( *this, Pitch, InFormat, MR_Local, SizeX, SizeY, MultisampleType ) )
	{
		// @todo: support host memory surfaces?
		// Start address must be a multiple of 64 bytes (128 bytes if swizzled).
		AllocateMemory(Pitch * SizeY, MR_Local, AT_RenderTarget, PS3TEXTURE_ALIGNMENT);
	}
}

/**
 * Construct from a CellGcmSurface with surface info
 * 
 * @param AttachSurface		Surface descriptor to copy params from 
 * @param bIsColorBuffer	If TRUE, copies the color buffer fom the Surface, otherwise copies depth buffer
 */
FPS3RHISurface::FPS3RHISurface(const CellGcmSurface* AttachSurface, UBOOL bIsColorBuffer)
: FPS3RHIGPUResource(bIsColorBuffer ? AttachSurface->colorOffset[0] : AttachSurface->depthOffset, (EMemoryRegion)(bIsColorBuffer ? AttachSurface->colorLocation[0] : AttachSurface->depthLocation))
, SizeX(AttachSurface->width)
, SizeY(AttachSurface->height)
, MultisampleType(AttachSurface->antialias)
, ResolveTargetMemoryOffset(0)
, ResolveTarget(NULL)
{
	if (bIsColorBuffer)
	{
		Format = AttachSurface->colorFormat;
		Pitch = AttachSurface->colorPitch[0];
	}
	else
	{
		Format = AttachSurface->depthFormat;
		Pitch = AttachSurface->depthPitch;
	}

	// don't set tiled, as it should already be tiled
}

/**
 * Construct from a CellGcmTexture
 * 
 * @param AttachTexture		Texture descriptor to copy params from 
 */
FPS3RHISurface::FPS3RHISurface(FTexture2DRHIRef AttachTexture)
: FPS3RHIGPUResource(AttachTexture->Texture.offset, (EMemoryRegion)AttachTexture->MemoryRegion)
, SizeX(AttachTexture->Texture.width)
, SizeY(AttachTexture->Texture.height)
, Format(TextureToSurfaceFormat(AttachTexture->PixelFormat)) // convert from texture format to surface format
, MultisampleType(CELL_GCM_SURFACE_CENTER_1)
, Pitch(AttachTexture->Texture.pitch)
, ResolveTargetMemoryOffset(0)
, ResolveTarget(NULL)
{
}

/** Cubemap constructor*/
FPS3RHISurface::FPS3RHISurface(DWORD InSizeX, EPixelFormat InFormat, FTextureCubeRHIRef InResolveTarget, ECubeFace CubeFace, DWORD Flags, DWORD InMultisampleType)
: SizeX(InSizeX)
, SizeY(InSizeX)
// pixel formats are texture formats, not surface formats, must convert
, Format(TextureToSurfaceFormat(InFormat))
, MultisampleType(InMultisampleType)
, ResolveTargetMemoryOffset(0)
, ResolveTargetCube(InResolveTarget)
, ResolveTargetCubeFace(CubeFace)
{
	// surfaces can't be compressed
	check(InFormat != PF_DXT1 && InFormat != PF_DXT3 && InFormat != PF_DXT5);

	// Adjust to a valid tiled pitch (multiple of 256 bytes).
	Pitch = SizeX * GPixelFormats[InFormat].BlockBytes;

	// Try to allocate tiled memory first.
	// @todo: maybe syupport tiled cubemaps
//	if ((Flags & TargetSurfCreate_WriteOnce) || !GPS3Gcm->AllocateTiledMemory( *this, Pitch, InFormat, MR_Local, SizeX, SizeY, MultisampleType ) )
	{
		// Start address must be a multiple of 64 bytes (128 bytes if swizzled).
		// use base class to allocate memory in GPU mem
		// @todo: support host memory surfaces?
		AllocateMemory(Pitch * SizeY, MR_Local, AT_RenderTarget, PS3TEXTURE_ALIGNMENT);
	}
}

/**
 * Construct from a CubeTexture
 * 
 * @param AttachTexture		Texture descriptor to copy params from 
 * @param CubeFace			Which face int he attached cubemap to update
 */
FPS3RHISurface::FPS3RHISurface(FTextureCubeRHIRef AttachTexture, ECubeFace CubeFace)
// @todo: Is it guaranteed to be linear?
: FPS3RHIGPUResource(AttachTexture->GetTexture().offset, (EMemoryRegion)AttachTexture->MemoryRegion)
, SizeX(AttachTexture->GetTexture().width)
, SizeY(AttachTexture->GetTexture().width)
, Format(TextureToSurfaceFormat(AttachTexture->GetUnrealFormat())) // convert from texture format to surface format
, MultisampleType(CELL_GCM_SURFACE_CENTER_1)
, Pitch(AttachTexture->GetTexture().pitch)
, ResolveTargetMemoryOffset(0)
, ResolveTarget(NULL)
, ResolveTargetCube(NULL)
{
	// fixup the memory offset to point to the particular cube face
	MemoryOffset += Align(GetTextureMipSize(AttachTexture->GetUnrealFormat(), SizeX, SizeX, 0, TRUE), PS3TEXTURE_ALIGNMENT) * (INT)CubeFace;
}


/** Destructor */
FPS3RHISurface::~FPS3RHISurface()
{
	// Restore the memory offsets, so that the resolvetarget can unbind its tiled memory.
	if ( IsValidRef(ResolveTarget) )
	{
		ResolveTarget->SetMemoryOffset(ResolveTargetMemoryOffset);
	}
	GPS3Gcm->UnbindTiledMemory( *this );
}

/**
 * Swaps memory pointer with the resolve target.
 **/
void FPS3RHISurface::SwapWithResolveTarget()
{
	if ( IsValidRef(ResolveTarget) )
	{
		UINT TargetMemoryOffset = ResolveTargetMemoryOffset;
		ResolveTarget->SetMemoryOffset(MemoryOffset);
		ResolveTargetMemoryOffset = MemoryOffset;
		MemoryOffset = TargetMemoryOffset;
	}
}

/**
 * Makes the surface and the resolve target point to the same memory.
 **/
void FPS3RHISurface::UnifyWithResolveTarget()
{
	if ( IsValidRef(ResolveTarget) )
	{
		ResolveTarget->SetMemoryOffset( MemoryOffset );
	}
}

/**
 * Makes the resolve target point its own memory again, different from the surface.
 **/
void FPS3RHISurface::DualBufferWithResolveTarget()
{
	if ( IsValidRef(ResolveTarget) )
	{
		ResolveTarget->SetMemoryOffset( ResolveTargetMemoryOffset );
	}
}

/**
 * Retrieves the current buffer state (i.e. which memory buffer is used for the surface and which is used for the resolve texture).
 */
FPS3RHISurface::FBufferState FPS3RHISurface::GetBufferState()
{
	FBufferState BufferState;
	if ( IsValidRef(ResolveTarget) )
	{
		BufferState.SurfaceOffset = MemoryOffset;
		BufferState.ResolveTargetOffset = ResolveTargetMemoryOffset;
		BufferState.TextureOffset = ResolveTarget->MemoryOffset;
	}
	else
	{
		BufferState.SurfaceOffset = 0;
		BufferState.ResolveTargetOffset = 0;
		BufferState.TextureOffset = 0;
	}
	return BufferState;
}

/**
 * Sets the current dual-buffer state.
 */
void FPS3RHISurface::SetBufferState( FPS3RHISurface::FBufferState BufferState )
{
	if ( IsValidRef(ResolveTarget) )
	{
		MemoryOffset = BufferState.SurfaceOffset;
		ResolveTargetMemoryOffset = BufferState.ResolveTargetOffset;
		ResolveTarget->SetMemoryOffset(BufferState.TextureOffset);
	}
}

/**
 *	Returns the resolve target of a surface.
 *	@param SurfaceRHI	- Surface from which to get the resolve target
 *	@return				- Resolve target texture associated with the surface
 */
FTexture2DRHIRef RHIGetResolveTarget( FSurfaceRHIParamRef SurfaceRHI )
{
	return SurfaceRHI->ResolveTarget;
}

/**
 * Creates a RHI surface that can be bound as a render target.
 * Note that a surface cannot be created which is both resolvable AND readable.
 * @param SizeX - The width of the surface to create.
 * @param SizeY - The height of the surface to create.
 * @param Format - The surface format to create.
 * @param ResolveTargetTexture - The texture which the surface will be resolved to.  It must have been allocated with bResolveTargetable=TRUE
 * @param Flags - Surface creation flags
 * @param UsageStr - Text describing usage for this surface
 * @return The surface that was created.
 */
FSurfaceRHIRef RHICreateTargetableSurface(
	UINT SizeX,
	UINT SizeY,
	BYTE Format,
	FTexture2DRHIParamRef ResolveTargetTexture,
	DWORD Flags,
	const TCHAR* UsageStr
	)
{
	if (!(Flags&TargetSurfCreate_Dedicated) && IsValidRef(ResolveTargetTexture))
	{
		return new FPS3RHISurface(ResolveTargetTexture);
	}
	// create a tiled surface unless the flags say it will be written to only once
	return new FPS3RHISurface(SizeX, SizeY, (EPixelFormat)Format, ResolveTargetTexture, Flags);
}

/**
 * Creates a RHI surface that can be bound as a render target and can resolve w/ a cube texture
 * Note that a surface cannot be created which is both resolvable AND readable.
 * @param SizeX - The width of the surface to create.
 * @param Format - The surface format to create.
 * @param ResolveTargetTexture - The cube texture which the surface will be resolved to.  It must have been allocated with bResolveTargetable=TRUE
 * @param CubeFace - face from resolve texture to use as surface
 * @param Flags - Surface creation flags
 * @param UsageStr - Text describing usage for this surface
 * @return The surface that was created.
 */
FSurfaceRHIRef RHICreateTargetableCubeSurface(
	UINT SizeX,
	BYTE Format,
	FTextureCubeRHIParamRef ResolveTargetTexture,
	ECubeFace CubeFace,
	DWORD Flags,
	const TCHAR* UsageStr
	)
{
	if (!(Flags&TargetSurfCreate_Dedicated) && IsValidRef(ResolveTargetTexture))
	{
		return new FPS3RHISurface(ResolveTargetTexture, CubeFace);
	}
	return new FPS3RHISurface(SizeX, (EPixelFormat)Format, ResolveTargetTexture, CubeFace, Flags);
}

void RHISetRenderTarget(FSurfaceRHIParamRef NewRenderTarget, FSurfaceRHIParamRef NewDepthStencilBuffer)
{
	// remember the most recently set render and depth/stencil targets
	GPS3RHIRenderTarget = NewRenderTarget;
	GPS3RHIDepthTarget = NewDepthStencilBuffer;

	if (!IsValidRef(NewRenderTarget) && !IsValidRef(NewDepthStencilBuffer))
	{
		NewRenderTarget = GPS3Gcm->GetColorBuffer(GPS3Gcm->GetBackBufferIndex());
		NewDepthStencilBuffer = NULL;
	}

	SCOPE_CYCLE_COUNTER(STAT_SetRenderTarget);
	static CellGcmSurface Surface = {0};
	static UBOOL bSurfaceInitialized = FALSE;

	// initialized properties that never change
	if (!bSurfaceInitialized)
	{
		Surface.colorLocation[0]	= CELL_GCM_LOCATION_LOCAL;
		Surface.colorOffset[0]		= 0;
		Surface.colorPitch[0]		= 64;
		Surface.colorLocation[1]	= CELL_GCM_LOCATION_LOCAL;
		Surface.colorOffset[1]		= 0;
		Surface.colorPitch[1]		= 64;
		Surface.colorLocation[2]	= CELL_GCM_LOCATION_LOCAL;
		Surface.colorOffset[2]		= 0;
		Surface.colorPitch[2]		= 64;
		Surface.colorLocation[3]	= CELL_GCM_LOCATION_LOCAL;
		Surface.colorOffset[3]		= 0;
		Surface.colorPitch[3]		= 64;

		Surface.depthLocation		= CELL_GCM_LOCATION_LOCAL;
		Surface.depthOffset			= 0;
		Surface.depthPitch			= 64;

		Surface.type				= CELL_GCM_SURFACE_PITCH; // not swizzled
		Surface.antialias			= CELL_GCM_SURFACE_CENTER_1;
		Surface.x					= 0;
		Surface.y					= 0;

		bSurfaceInitialized = TRUE;
	}


	DWORD SizeX = 0;
	DWORD SizeY = 0;
	// make sure the targets match
	check(!IsValidRef(NewRenderTarget) || !IsValidRef(NewDepthStencilBuffer) ||
		  (NewRenderTarget->SizeX <= NewDepthStencilBuffer->SizeX && NewRenderTarget->SizeY <= NewDepthStencilBuffer->SizeY));

	if (IsValidRef(NewRenderTarget))
	{
		// set up the render target color buffer info for the single RT
		Surface.colorTarget			= CELL_GCM_SURFACE_TARGET_0; // not using MRTs
		Surface.colorFormat			= NewRenderTarget->Format;
		Surface.colorLocation[0]	= NewRenderTarget->MemoryRegion;
		Surface.colorOffset[0]		= NewRenderTarget->MemoryOffset;
		Surface.colorPitch[0]		= NewRenderTarget->Pitch;
		SizeX						= NewRenderTarget->SizeX;
		SizeY						= NewRenderTarget->SizeY;
	}
	else
	{
		Surface.colorTarget			= CELL_GCM_SURFACE_TARGET_NONE;
		Surface.colorFormat			= CELL_GCM_SURFACE_A8R8G8B8;
		Surface.colorLocation[0]	= CELL_GCM_LOCATION_LOCAL;
		Surface.colorOffset[0]		= 0;
		Surface.colorPitch[0]		= 64;
		SizeX						= NewDepthStencilBuffer->SizeX;
		SizeY						= NewDepthStencilBuffer->SizeY;
	}

	// set up z buffer
	if (IsValidRef(NewDepthStencilBuffer))
	{
		Surface.depthFormat			= NewDepthStencilBuffer->Format;
		Surface.depthLocation		= NewDepthStencilBuffer->MemoryRegion;
		Surface.depthOffset			= NewDepthStencilBuffer->MemoryOffset;
		Surface.depthPitch			= NewDepthStencilBuffer->Pitch;
	}
	else
	{
		Surface.depthFormat			= CELL_GCM_SURFACE_Z24S8;
		Surface.depthLocation		= CELL_GCM_LOCATION_LOCAL;
		Surface.depthOffset			= 0;
		Surface.depthPitch			= 64;
	}
	Surface.width	= SizeX;
	Surface.height	= SizeY;

	GPS3Gcm->SetRenderTarget(&Surface);
}

void RHISetMRTRenderTarget(FSurfaceRHIParamRef NewRenderTargetRHI, UINT TargetIndex)
{
	//@todo: MRT support for PS3
}

void RHIReadSurfaceData(FSurfaceRHIParamRef Surface,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<BYTE>& OutData, FReadSurfaceDataFlags InFlags)
{
	// currently only support full surface reading
	checkf(MinX == 0 && MinY == 0 && MaxX == Surface->SizeX - 1 && MaxY == Surface->SizeY - 1, TEXT("Non full surface Read Surface is not supported yet"));

	// @todo: we only support 4 bytes
	INT BytesPerTexel = 4;

	GPS3Gcm->BlockUntilGPUIdle();

	UINT SurfaceSize = Surface->Pitch * Surface->SizeY;
	void* Temp = GPS3Gcm->GetHostAllocator()->Malloc(SurfaceSize, AT_RenderTarget);
	check( Temp );
	UINT TempOffset;
	cellGcmAddressToOffset(Temp, &TempOffset);

	cellGcmSetTransferData(
		CELL_GCM_TRANSFER_LOCAL_TO_MAIN, 
		TempOffset,
		Surface->Pitch,
		Surface->MemoryOffset,
		Surface->Pitch,
		Surface->Pitch,
		Surface->SizeY);

	GPS3Gcm->BlockUntilGPUIdle();

	// allocate OutData
	UINT SizeX = MaxX - MinX + 1;
	UINT SizeY = MaxY - MinY + 1;
	UINT OutDataSize = SizeY * SizeX * BytesPerTexel;
	OutData.Empty(OutDataSize);
	OutData.Add(OutDataSize);

	// copy from temp host buffer to bytes
	BYTE* Source = (BYTE*)Temp;
	BYTE* Dest = (BYTE*)OutData.GetData();
	for (INT Y = MinY; Y <= MaxY; Y++)
	{
		appMemcpy(Dest + (Y - MinY) * SizeX * BytesPerTexel, Source + Y * Surface->Pitch, SizeX * BytesPerTexel);
	}

	GPS3Gcm->GetHostAllocator()->Free(Temp);
}

void RHIReadSurfaceFloatData(FSurfaceRHIParamRef Surface,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<FFloat16Color>& OutData,ECubeFace CubeFace)
{
}


/*-----------------------------------------------------------------------------
	FPS3RHITexture
-----------------------------------------------------------------------------*/

FPS3RHITexture::FPS3RHITexture()
:	CreationFlags( 0 )
,	ReallocatedTexture( NULL )
,	ReallocationRequest( NULL )
,	LockedMipLevels( 0 )
,	bIsSRGB( FALSE )
,	bReallocationOwner( FALSE )
{
}

/** Constructor initialized from another texture but with a new Gcm texture header */
FPS3RHITexture::FPS3RHITexture(FPS3RHITexture* OtherTexture, const CellGcmTexture& NewGcmHeader,void* NewBaseAddress, DWORD NewTextureSize,FAsyncReallocationRequest* InReallocationRequest/*=NULL*/)
:	Texture( NewGcmHeader )
,	PixelFormat( OtherTexture->GetUnrealFormat() )
,	UnsignedRemap( OtherTexture->GetUnsignedRemap() )
,	CreationFlags( OtherTexture->GetCreationFlags() )
,	ReallocatedTexture( NULL )
,	ReallocationRequest( InReallocationRequest )
,	LockedMipLevels( 0 )
,	bIsSRGB( OtherTexture->IsSRGB() )
,	bReallocationOwner( FALSE )
{
	Data = NewBaseAddress;
	bUsesTexturePool = OtherTexture->bUsesTexturePool;
	MemoryRegion = OtherTexture->MemoryRegion;
	AllocatedSize = NewTextureSize;
	SetMemoryOffset( NewGcmHeader.offset );
//	bFreeMemory = TRUE;	// Set by SetReallocatedTexture().

	// Let the resources know about the reallocation, so it knows what to do with the memory upon destruction.
	OtherTexture->SetReallocatedTexture( this, FALSE );
	SetReallocatedTexture( OtherTexture, TRUE );

	// Only register the new texture if we have the new location (async reallocations don't have it yet).
	if ( NewBaseAddress )
	{
		GPS3Gcm->GetTexturePool()->RegisterTexture( this );
	}
}

FPS3RHITexture::~FPS3RHITexture()
{
	// Did the user delete a locked texture?
	if ( IsLocked() )
	{
		LockedMipLevels = 0;
		if ( GetBaseAddress() )
		{
			// Make sure to unlock it.
			GPS3Gcm->GetTexturePool()->Unlock( GetBaseAddress() );
		}
	}

	//debugf(TEXT("Destructing texture %d"), MemoryOffset);
	GPS3Gcm->UnbindTiledMemory( *this );

	// Check if we've been reallocated (only for 2D textures).
	if ( ReallocatedTexture )
	{
		// A texture must be canceled or finalized before destroying it.
		check( !bReallocationOwner );

		// Finally delete the request (if async).
		delete ReallocationRequest;
		ReallocationRequest = NULL;
		delete ReallocatedTexture->ReallocationRequest;
		ReallocatedTexture->ReallocationRequest = NULL;

		// The other texture is now the only one holding on to texture memory.
		ReallocatedTexture->SetReallocatedTexture( NULL, TRUE );

		// The other resource (the new texture) is solely responsible for the texture memory.
		bFreeMemory = FALSE;
	}
	else if ( bUsesTexturePool && GetBaseAddress() )
	{
		GPS3Gcm->GetTexturePool()->UnregisterTexture( this );
	}
}

/**
 * This is used when swapping a texture and a surface, we need to update
 * the offset stored in the texture structure 
 *
 * @param MemoryOffset Memory that was previously a surface memory location
 */
void FPS3RHITexture::SetMemoryOffset(DWORD InMemoryOffset)
{
	MemoryOffset = InMemoryOffset;
	Texture.offset = MemoryOffset;
}

void FPS3RHITexture::InitializeInternals(DWORD SizeX, DWORD SizeY, EPixelFormat Format, DWORD NumMips, UBOOL bIsLinearTexture, DWORD Flags, UBOOL bIsCubemap, UBOOL bInIsSRGB, UBOOL bBiasNormalMap, FResourceBulkDataInterface* BulkData)
{
	LockedMipLevels	= 0;
	CreationFlags	= Flags;
	PixelFormat		= Format;
	bIsSRGB			= bInIsSRGB;

	Texture.format		= GPixelFormats[PixelFormat].PlatformFormat;
	Texture.mipmap		= NumMips;
	Texture.dimension	= CELL_GCM_TEXTURE_DIMENSION_2;
	Texture.cubemap		= bIsCubemap;
	UnsignedRemap		= bBiasNormalMap ? CELL_GCM_TEXTURE_UNSIGNED_REMAP_BIASED : CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL;

	// this tells RSX what order the components are in
	if ( PixelFormat == PF_DepthStencil || PixelFormat == PF_ShadowDepth )
	{
		Texture.remap	= (CELL_GCM_TEXTURE_REMAP_REMAP << 14) |
						  (CELL_GCM_TEXTURE_REMAP_REMAP << 12) |
						  (CELL_GCM_TEXTURE_REMAP_REMAP << 10) |
						  (CELL_GCM_TEXTURE_REMAP_REMAP <<  8) |	// Memory layout for each texel: Element[0], Element[1], Element[2], Element[3]
						  (CELL_GCM_TEXTURE_REMAP_FROM_A << 2) |	// tex2d: FetchedValue.x = Element[0]
						  (CELL_GCM_TEXTURE_REMAP_FROM_R << 4) |	// tex2d: FetchedValue.y = Element[1]
						  (CELL_GCM_TEXTURE_REMAP_FROM_G << 6) |	// tex2d: FetchedValue.z = Element[2]
						  (CELL_GCM_TEXTURE_REMAP_FROM_B);			// tex2d: FetchedValue.w = Element[3]
	}
	else if ( PixelFormat == PF_A8R8G8B8 && !(Flags & (TexCreate_ResolveTargetable|TexCreate_Uncooked)) )
	{
		Texture.remap	= (CELL_GCM_TEXTURE_REMAP_REMAP << 14) |
						  (CELL_GCM_TEXTURE_REMAP_REMAP << 12) |
						  (CELL_GCM_TEXTURE_REMAP_REMAP << 10) |
						  (CELL_GCM_TEXTURE_REMAP_REMAP <<  8) |	// Memory layout for each texel: Element[0], Element[1], Element[2], Element[3]
						  (CELL_GCM_TEXTURE_REMAP_FROM_G << 2) |	// tex2d: FetchedValue.x = Element[2]
						  (CELL_GCM_TEXTURE_REMAP_FROM_R << 4) |	// tex2d: FetchedValue.y = Element[1]
						  (CELL_GCM_TEXTURE_REMAP_FROM_A << 6) |	// tex2d: FetchedValue.z = Element[0]
						  (CELL_GCM_TEXTURE_REMAP_FROM_B);			// tex2d: FetchedValue.w = Element[3]
	}
	else if ( PixelFormat == PF_V8U8 )
	{
		Texture.remap	= (CELL_GCM_TEXTURE_REMAP_ZERO  << 14) |
						  (CELL_GCM_TEXTURE_REMAP_REMAP << 12) |
						  (CELL_GCM_TEXTURE_REMAP_REMAP << 10) |
						  (CELL_GCM_TEXTURE_REMAP_ONE   <<  8) |	// Memory layout for each texel: Element[0], Element[1]
						  (CELL_GCM_TEXTURE_REMAP_FROM_B << 6) |	// tex2d: FetchedValue.z = 0
						  (CELL_GCM_TEXTURE_REMAP_FROM_R << 4) |	// tex2d: FetchedValue.y = Element[0]
						  (CELL_GCM_TEXTURE_REMAP_FROM_G << 2) |	// tex2d: FetchedValue.x = Element[1]
						  (CELL_GCM_TEXTURE_REMAP_FROM_A);			// tex2d: FetchedValue.w = 1
	}
	else
	{
		Texture.remap	= (CELL_GCM_TEXTURE_REMAP_REMAP << 14) |
						  (CELL_GCM_TEXTURE_REMAP_REMAP << 12) |
						  (CELL_GCM_TEXTURE_REMAP_REMAP << 10) |
						  (CELL_GCM_TEXTURE_REMAP_REMAP <<  8) |	// Memory layout for each texel: Element[0], Element[1], Element[2], Element[3]
						  (CELL_GCM_TEXTURE_REMAP_FROM_R << 2) |	// tex2d: FetchedValue.x = Element[1]
						  (CELL_GCM_TEXTURE_REMAP_FROM_G << 4) |	// tex2d: FetchedValue.y = Element[2]
						  (CELL_GCM_TEXTURE_REMAP_FROM_B << 6) |	// tex2d: FetchedValue.z = Element[3]
						  (CELL_GCM_TEXTURE_REMAP_FROM_A);			// tex2d: FetchedValue.w = Element[0]
	}
	Texture.width		= SizeX;
	Texture.height		= SizeY;
	Texture.depth		= 1;
	// @todo support main memory textures
	Texture.location	= CELL_GCM_LOCATION_LOCAL;

	// calculate texture pitch
	// linear textures are if marked, or if the format requires it, or it's not a power of 2
	if (bIsLinearTexture || (Flags & TexCreate_ResolveTargetable) || RequireLinearTexture(PixelFormat, SizeX, SizeY) )
	{
		Texture.pitch = GetTexturePitch( PixelFormat, SizeX, 0, TRUE );
		Texture.format |= CELL_GCM_TEXTURE_LN;
		bIsLinearTexture = TRUE;
	}
	else
	{
		// 0 for swizzled/DXT textures
		Texture.pitch = 0;
		Texture.format |= CELL_GCM_TEXTURE_SZ;
	}

	// @todo support host memory textures
	// @todo support tiled cubemap render targets
	UBOOL bIsRendertarget = (Flags & (TexCreate_ResolveTargetable|TexCreate_DepthStencil));
	if ( bIsRendertarget )
	{
		check( NumMips == 1 && Texture.pitch > 0 );
		UBOOL bIsTiled = !bIsCubemap && !(Flags & TexCreate_WriteOnce);
		if ( bIsTiled )
		{
			bIsTiled = GPS3Gcm->AllocateTiledMemory( *this, Texture.pitch, PixelFormat, MR_Local, SizeX, SizeY, CELL_GCM_SURFACE_CENTER_1 );
		}
		if ( !bIsTiled )
		{
			DWORD TotalSize = Texture.pitch * SizeY;
			if ( bIsCubemap )
			{
				TotalSize = Align(TotalSize, PS3TEXTURE_ALIGNMENT) * 6;
			}
			AllocateMemory(TotalSize, MR_Local, AT_RenderTarget, PS3TEXTURE_ALIGNMENT);
		}
	}
	else
	{
		// calculate total size for all mips
		DWORD TotalSize = 0;
		for (DWORD MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			TotalSize += GetTextureMipSize(Format, SizeX, SizeY, MipIndex, bIsLinearTexture);
		}
		if ( bIsCubemap )
		{
			TotalSize = Align(TotalSize, PS3TEXTURE_ALIGNMENT) * 6;
		}

		// Allocate memory for the texture. Be safe and always align to 128 bytes (could be 64 in many cases).
		AllocateMemory(TotalSize, MR_Local, AT_Texture, PS3TEXTURE_ALIGNMENT, BulkData);
	}

	// copy the allocated mem offset into the texture object
	Texture.offset = MemoryOffset;

	if ( bUsesTexturePool )
	{
		GPS3Gcm->GetTexturePool()->RegisterTexture( this );
	}
}

/** Locks the resource, blocks if texture memory is currently being defragmented. */
void FPS3RHITexture::Lock( INT MipIndex )
{
	if ( LockedMipLevels == 0 && GetBaseAddress() )
	{
		GPS3Gcm->GetTexturePool()->Lock( GetBaseAddress() );
	}
	LockedMipLevels |= GBitFlag[MipIndex];	
}

/** Unlocks the resource. */
void FPS3RHITexture::Unlock( INT MipIndex )
{
	LockedMipLevels &= ~GBitFlag[MipIndex];
	if ( LockedMipLevels == 0 && GetBaseAddress() )
	{
		GPS3Gcm->GetTexturePool()->Unlock( GetBaseAddress() );
	}
}

/** Checks whether this texture can be relocated or not by the defragmentation process. */
UBOOL FPS3RHITexture::CanRelocate() const
{
	// Can't be relocated if it's locked.
	if ( IsLocked() )
	{
		return FALSE;
	}

	// During reallocation, the new texture keeps the request around until it's been processed higher up.
	// Can't relocate a completed request until it's been processed higher up.
	if ( ReallocationRequest && ReallocationRequest->HasCompleted() )
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * Returns the status of an ongoing or completed texture reallocation:
 *	TexRealloc_Succeeded	- The texture is ok, reallocation is not in progress.
 *	TexRealloc_Failed		- The texture is bad, reallocation is not in progress.
 *	TexRealloc_InProgress	- The texture is currently being reallocated async.
 *
 * @param Texture2D		- Texture to check the reallocation status for
 * @return				- Current reallocation status
 */
ETextureReallocationStatus FPS3RHITexture::GetReallocationStatus() const
{
	if ( ReallocationRequest )
	{
		if ( ReallocationRequest->HasCompleted() )
		{
			return TexRealloc_Succeeded;
		}
		return TexRealloc_InProgress;
	}
	return (Data && AllocatedSize) ? TexRealloc_Succeeded : TexRealloc_Failed;
}

/**
 * Called by the texture allocator when an async reallocation request has been completed.
 *
 * @param OldTexture		Original texture. OldTexture->ReallocatedTexture refers to the new texture.
 * @param FinishedRequest	The request that got completed
 */
void FPS3RHITexture::OnReallocationFinished( FPS3RHITexture* OldTexture, FAsyncReallocationRequest* FinishedRequest )
{
	if ( FinishedRequest->IsReallocation() )
	{
		check( OldTexture && OldTexture->ReallocatedTexture && !OldTexture->ReallocationRequest );
		FPS3RHITexture* NewTexture = OldTexture->ReallocatedTexture;
		check( NewTexture && NewTexture->ReallocatedTexture && NewTexture->ReallocationRequest == FinishedRequest );
		check( FinishedRequest->HasCompleted() );
		check( NewTexture->GetBaseAddress() == NULL );

		NewTexture->Data = FinishedRequest->GetNewBaseAddress();
		NewTexture->AllocatedSize = FinishedRequest->GetNewSize();

		// Set the IO offset that maps to this address.
		uint32_t GcmOffset = 0;
		cellGcmAddressToOffset(NewTexture->Data, &GcmOffset);
		NewTexture->SetMemoryOffset( GcmOffset );

		GPS3Gcm->GetTexturePool()->RegisterTexture( NewTexture );
	}
}

/**
 * Finalizes an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to finalize
 * @param bBlockUntilCompleted	If TRUE, blocks until the finalization is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus FPS3RHITexture::FinalizeAsyncReallocateTexture2D( FPS3RHITexture* Texture, UBOOL bBlockUntilCompleted )
{
	if ( Texture->ReallocationRequest == NULL )
	{
		return TexRealloc_Failed;
	}
	if ( bBlockUntilCompleted && !Texture->ReallocationRequest->HasCompleted() )
	{
		GPS3Gcm->GetTexturePool()->BlockOnAsyncReallocation( Texture->ReallocationRequest );
	}
	ETextureReallocationStatus Status = Texture->GetReallocationStatus();
	return Status;
}

/**
 * Cancels an async or immediate reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If TRUE, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus FPS3RHITexture::CancelAsyncReallocateTexture2D( FPS3RHITexture* Texture, UBOOL bBlockUntilCompleted )
{
	if ( Texture->IsLocked() )
	{
		Texture->LockedMipLevels = 0;
		if ( Texture->GetBaseAddress() )
		{
			GPS3Gcm->GetTexturePool()->Unlock( Texture->GetBaseAddress() );
		}
	}

	FPS3RHITexture* OldTexture = Texture->ReallocatedTexture;
	check( OldTexture );

	ETextureReallocationStatus Status = Texture->GetReallocationStatus();
	if ( Texture->ReallocationRequest )
	{
		GPS3Gcm->GetTexturePool()->CancelAsyncReallocation( Texture->ReallocationRequest, Texture->GetBaseAddress() );
		if ( bBlockUntilCompleted )
		{
			GPS3Gcm->GetTexturePool()->BlockOnAsyncReallocation( Texture->ReallocationRequest );
		}
	}
	// Are we canceling an immediate reallocation?
	else if ( Status == TexRealloc_Succeeded )
	{
		// Can't cancel a Shrink, only a Grow. This function should not be called for this purpose.
		// (See how UTexture2D::CancelPendingMipChangeRequest() prevents that case.)
		check( OldTexture->AllocatedSize < Texture->AllocatedSize );

		// Shrink it back to the original size. Always succeeds immediately.
		void* NewBaseAddress = GPS3Gcm->GetTexturePool()->Reallocate( Texture->GetBaseAddress(), OldTexture->AllocatedSize );
		check( NewBaseAddress && NewBaseAddress == OldTexture->GetBaseAddress() );
	}

	// Remove any remnants of reallocation from our side. (The allocator will keep a copy of the request if necessary.)
	OldTexture->SetReallocatedTexture( NULL, TRUE );
	Texture->SetReallocatedTexture( NULL, FALSE );
	Texture->SetMemoryOffset( 0 );
	Texture->Data = NULL;
	Texture->AllocatedSize = 0;
	delete Texture->ReallocationRequest;
	Texture->ReallocationRequest = NULL;

	// Shouldn't really be necessary re-register but won't hurt...
	GPS3Gcm->GetTexturePool()->RegisterTexture( OldTexture );

	return Status;
}

/**
 *	Notifies that the texture data is being reallocated and is shared for a brief moment,
 *	so that this texture can do the right thing upon destruction.
 *
 *	@param InReallocatedTexture	- The other texture that briefly shares the texture data when it's reallocated
 *	@param bReallocationOwner	- TRUE if 'ReallocatedTexture' refers to the old texture
 **/
void FPS3RHITexture::SetReallocatedTexture( FPS3RHITexture* InReallocatedTexture, UBOOL bInReallocationOwner )
{
	ReallocatedTexture = InReallocatedTexture;
	bReallocationOwner = bInReallocationOwner;
	bFreeMemory = bReallocationOwner;
}

/**
 * Constructor
 */
FPS3RHITexture2D::FPS3RHITexture2D(DWORD InSizeX, DWORD InSizeY, EPixelFormat InFormat, DWORD NumMips, UBOOL bIsLinearTexture, DWORD Flags, FResourceBulkDataInterface* BulkData)
{
	InitializeInternals(InSizeX, InSizeY, InFormat, NumMips, bIsLinearTexture, Flags, FALSE, Flags & TexCreate_SRGB, Flags & TexCreate_BiasNormalMap, BulkData);
}

/**
 * Returns the number of texels in a texture block, given a native PS3 Gcm texture format.
 */
void PS3GetBlockDimensions( uint8_t GcmFormat, UINT& BlockSizeX, UINT BlockSizeY )
{
	uint8_t Format = GcmFormat & GcmFormatMask;
	if ( Format == CELL_GCM_TEXTURE_COMPRESSED_DXT1 ||
		 Format == CELL_GCM_TEXTURE_COMPRESSED_DXT23 ||
		 Format == CELL_GCM_TEXTURE_COMPRESSED_DXT45 )
	{
		BlockSizeX = 4;
		BlockSizeY = 4;
	}
	else
	{
		BlockSizeX = 1;
		BlockSizeY = 1;
	}
}

/**
 * Initializes a new Gcm texture header, based on an existing header and new settings.
 *
 * @param OldGcmHeader		Gcm texture header to base the new header on
 * @param NewMipCount		New number of mip-levels (including base mip)
 * @param NewGcmHeader		[out] Texture header to initialize
 * @return					Number of bytes used by all mips for the new texture
 */
INT FPS3RHITexture2D::SetTextureHeaderFrom( FPS3RHITexture2D* Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, CellGcmTexture& NewGcmHeader )
{
	UBOOL bIsLinearTexture	= Texture2D->IsLinear();
	EPixelFormat Format		= Texture2D->GetUnrealFormat();
	NewGcmHeader			= Texture2D->Texture;
	NewGcmHeader.width		= NewSizeX;
	NewGcmHeader.height		= NewSizeY;
	NewGcmHeader.mipmap		= NewMipCount;
	if ( bIsLinearTexture )
	{
		NewGcmHeader.pitch = GetTexturePitch( Format, NewGcmHeader.width, 0, TRUE );
	}
	// Set the offset to 0, since the memory for this new texture isn't set up yet.
	NewGcmHeader.offset = 0;

	// calculate total size for all mips
	DWORD TotalSize = 0;
	for (DWORD MipIndex = 0; MipIndex < NewMipCount; MipIndex++)
	{
		TotalSize += GetTextureMipSize(Format, NewGcmHeader.width, NewGcmHeader.height, MipIndex, bIsLinearTexture);
	}
	if ( Texture2D->IsCubemap() )
	{
		TotalSize = Align(TotalSize, PS3TEXTURE_ALIGNMENT) * 6;
	}
	return TotalSize;
}

/** 
 * Tries to add or remove mip-levels of the texture without relocation.
 *
 * @param Texture2D		- Source texture
 * @param NewMipCount	- Number of desired mip-levels
 * @param NewSizeX		- New width, in pixels
 * @param NewSizeY		- New height, in pixels
 * @return				- A newly allocated FPS3RHITexture2D if successful, or NULL if it failed.
 */
FPS3RHITexture2D* FPS3RHITexture2D::ReallocateTexture2D(FPS3RHITexture2D* Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY )
{
	// Create and setup a new D3D struct.
	CellGcmTexture NewGcmHeader;
	INT NewTextureSize = SetTextureHeaderFrom( Texture2D, NewMipCount, NewSizeX, NewSizeY, NewGcmHeader );

	// Note: Can only do an in-place reallocation if the existing mips aren't changed because of alignment restrictions.
	INT OldTextureSize = Texture2D->AllocatedSize;
	INT SizeDifference = Abs( NewTextureSize - OldTextureSize );
	if ( SizeDifference == Align(SizeDifference, PS3TEXTURE_ALIGNMENT) )
	{
		// Try to resize the texture in-place, by moving the baseaddress to encompass the new mip-levels.
		// Note: Newly freed memory due to shrinking won't be available for allocation right away (need GPU sync).
		void* NewBaseAddress = GPS3Gcm->GetTexturePool()->Reallocate( Texture2D->GetBaseAddress(), NewTextureSize );

		// Did it succeed?
		if ( NewBaseAddress )
		{
			// Get the IO offset that maps to this address.
			cellGcmAddressToOffset(NewBaseAddress, &NewGcmHeader.offset);

			// Create a 2D texture resource and return it.
			FPS3RHITexture2D* NewTexture2D = new FPS3RHITexture2D( Texture2D, NewGcmHeader, NewBaseAddress, NewTextureSize, NULL );
			return NewTexture2D;
		}
	}

	// Failure.
	return NULL;
}

/**
 * Requests an async reallocation of the specified texture.
 * Returns a new texture that represents if the request was accepted.
 *
 * @param Texture2D		Texture to reallocate
 * @param NewMipCount	New number of mip-levels (including the base mip)
 * @param NewSizeX		New width, in pixels
 * @param NewSizeY		New height, in pixels
 * @param RequestStatus	Thread-safe counter to monitor the status of the request. Decremented by one when the request is completed.
 * @return				Newly created FPS3RHITexture2D if the request was accepted, or NULL
 */
FPS3RHITexture2D* FPS3RHITexture2D::AsyncReallocateTexture2D( FPS3RHITexture2D* Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, FThreadSafeCounter* RequestStatus )
{
	FPS3RHITexture2D* NewTexture = NULL;

	// Only try reallocation if continuous defragmentation is enabled.
	FBestFitAllocator::FSettings AllocatorSettings;
	GPS3Gcm->GetTexturePool()->GetSettings( AllocatorSettings );
	if ( AllocatorSettings.bEnableAsyncDefrag )
	{
		// Setup a new Gcm texture header.
		CellGcmTexture NewGcmHeader;
		INT NewTextureSize = SetTextureHeaderFrom( Texture2D, NewMipCount, NewSizeX, NewSizeY, NewGcmHeader );

		FAsyncReallocationRequest* ReallocationRequest = new FAsyncReallocationRequest( Texture2D->GetBaseAddress(), NewTextureSize, RequestStatus );
		if ( GPS3Gcm->GetTexturePool()->AsyncReallocate( ReallocationRequest, FALSE ) )
		{
			// Keep ResourceSize == 0 until the reallocation request has been completed.
			NewTexture = new FPS3RHITexture2D( Texture2D, NewGcmHeader, NULL, NewTextureSize, ReallocationRequest );

			if ( ReallocationRequest->HasCompleted() )
			{
				FPS3RHITexture2D::OnReallocationFinished( Texture2D, ReallocationRequest );
			}
		}
		else
		{
			delete ReallocationRequest;
		}
	}
	return NewTexture;
}

/**
 * Creates a 2D RHI texture resource
 * @param SizeX - width of the texture to create
 * @param SizeY - height of the texture to create
 * @param Format - EPixelFormat texture format
 * @param NumMips - number of mips to generate or 0 for full mip pyramid
 * @param Flags - ETextureCreateFlags creation flags
 */
FTexture2DRHIRef RHICreateTexture2D(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips, DWORD Flags, FResourceBulkDataInterface* BulkData)
{
	return new FPS3RHITexture2D(SizeX, SizeY, (EPixelFormat)Format, NumMips, (Flags & (TexCreate_NoTiling|TexCreate_Uncooked)), Flags, BulkData);
}

/**
 * Tries to reallocate the texture without relocation. Returns a new valid reference to the resource if successful.
 * Both the old and new reference refer to the same texture (at least the shared mip-levels) and both can be used or released independently.
 *
 * @param Texture2D		- Texture to reallocate
 * @param NewMipCount	- New number of mip-levels
 * @param NewSizeX		- New width, in pixels
 * @param NewSizeY		- New height, in pixels
 * @return				- New reference to the updated texture, or invalid if the reallocation failed
 */
FTexture2DRHIRef RHIReallocateTexture2D( FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY )
{
	FTexture2DRHIRef Texture2DRef( FPS3RHITexture2D::ReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY) );
	return Texture2DRef;
}

/**
 * Computes the size in memory required by a given texture.
 *
 * @param	TextureRHI		- Texture we want to know the size of
 * @return					- Size in Bytes
 */
UINT RHIGetTextureSize(FTexture2DRHIParamRef TextureRHI)
{
	// not supported (yet)
	return 0;
}

/**
 * Requests an async reallocation of the specified texture.
 * Returns a new texture that represents if the request was accepted.
 *
 * @param Texture2D		Texture to reallocate
 * @param NewMipCount	New number of mip-levels (including the base mip)
 * @param NewSizeX		New width, in pixels
 * @param NewSizeY		New height, in pixels
 * @param RequestStatus	Thread-safe counter to monitor the status of the request. Decremented by one when the request is completed.
 * @return				Reference to a newly created Texture2D if the request was accepted, or an invalid ref
 */
FTexture2DRHIRef RHIAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, FThreadSafeCounter* RequestStatus )
{
	FTexture2DRHIRef Texture2DRef( FPS3RHITexture2D::AsyncReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus) );
	return Texture2DRef;
}

/**
 * Finalizes an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to finalize
 * @param bBlockUntilCompleted	If TRUE, blocks until the finalization is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted )
{
	return FPS3RHITexture2D::FinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

/**
 * Cancels an async or immediate reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If TRUE, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus RHICancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted )
{
	return FPS3RHITexture2D::CancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

/**
 * Locks an RHI texture's mip surface for read/write operations on the CPU
 * @param Texture - the RHI texture resource to lock
 * @param MipIndex - mip level index for the surface to retrieve
 * @param bIsDataBeingWrittenTo - used to affect the lock flags 
 * @param DestStride - output to retrieve the textures row stride (pitch)
 * @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
 * @return pointer to the CPU accessible resource data
 */
void* RHILockTexture2D(FTexture2DRHIParamRef Texture,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	Texture->Lock(MipIndex);
	UBOOL bIsLinear = Texture->IsLinear();

	// calculate offset to the given mip level
	DWORD MipOffset = GetTextureMipOffset(Texture->GetUnrealFormat(), Texture->GetSizeX(), Texture->GetSizeY(), MipIndex, Texture->IsLinear());

	DestStride = GetTexturePitch(Texture->GetUnrealFormat(), Texture->GetSizeX(), MipIndex, bIsLinear);

	check(Texture->Data);
	// skip over all previous mips
	return (BYTE*)Texture->Data + MipOffset;
}

/**
 * Unlocks a previously locked RHI texture resource
 * @param Texture - the RHI texture resource to unlock
 * @param MipIndex - mip level index for the surface to unlock
 * @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
 */
void RHIUnlockTexture2D(FTexture2DRHIParamRef Texture,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	Texture->Unlock(MipIndex);
}

/**
 * Checks if a texture is still in use by the GPU.
 * @param Texture - the RHI texture resource to check
 * @param MipIndex - Which mipmap we're interested in
 * @return TRUE if the texture is still in use by the GPU, otherwise FALSE
 */
UBOOL RHIIsBusyTexture2D(FTexture2DRHIParamRef Texture, UINT MipIndex)
{
	//@TODO: Implement GPU resource fence!
	return FALSE;
}

/**
* Updates a region of a 2D texture from system memory
* @param Texture - the RHI texture resource to update
* @param MipIndex - mip level index to be modified
* @param n - number of rectangles to copy
* @param rects - rectangles to copy from source image data
* @param pitch - size in bytes of each line of source image
* @param sbpp - size in bytes of each pixel of source image (must match texture, passed in because some drivers do not maintain it in refs)
* @param psrc - source image data (in same pixel format as texture)
*/
UBOOL RHIUpdateTexture2D(FTexture2DRHIParamRef Texture,UINT MipIndex,UINT n,const FUpdateTextureRegion2D* rects,UINT pitch,UINT sbpp,BYTE* psrc)
{
    // Not needed, handled in GFxUIRenderer
    return 0;
}

/**
 * For platforms that support packed miptails return the first mip level which is packed in the mip tail
 * @return mip level for mip tail or -1 if mip tails are not used
 */
INT RHIGetMipTailIdx(FTexture2DRHIParamRef Texture)
{
	return -1;
}

/**
 * Copies a region within the same mip levels of one texture to another.  An optional region can be speci
 * Note that the textures must be the same size and of the same format.
 * @param DstTexture - destination texture resource to copy to
 * @param MipIdx - mip level for the surface to copy from/to. This mip level should be valid for both source/destination textures
 * @param BaseSizeX - width of the texture (base level). Same for both source/destination textures
 * @param BaseSizeY - height of the texture (base level). Same for both source/destination textures 
 * @param Format - format of the texture. Same for both source/destination textures
 * @param Region - list of regions to specify rects and source textures for the copy
 */
void RHICopyTexture2D(FTexture2DRHIParamRef DstTexture, UINT MipIdx, INT BaseSizeX, INT BaseSizeY, INT Format, const TArray<FCopyTextureRegion2D>& Regions)
{
	check( IsValidRef(DstTexture) );

	// The texturepool makes all textures in Local memory
	const BYTE Mode = CELL_GCM_TRANSFER_LOCAL_TO_LOCAL;

	// Alias the destination texture's format information.
	const INT BlockSizeX = GPixelFormats[Format].BlockSizeX;
	const INT BlockSizeY = GPixelFormats[Format].BlockSizeY;
	const INT BlockBytes = GPixelFormats[Format].BlockBytes;

	// scale the base SizeX,SizeY for the current mip level
	const INT MipSizeX = Max(BlockSizeX,BaseSizeX >> MipIdx);
	const INT MipSizeY = Max(BlockSizeY,BaseSizeY >> MipIdx);

	check(DstTexture->GetUnrealFormat() == Format);

	for( INT RegionIdx=0; RegionIdx < Regions.Num(); RegionIdx++ )		
	{
		const FCopyTextureRegion2D& Region = Regions(RegionIdx);
		check( IsValidRef(Region.SrcTexture) );

		// cache some stuff
		const UBOOL bIsLinear = Region.SrcTexture->IsLinear();
		const DWORD SrcSizeX = Region.SrcTexture->GetSizeX();
		const DWORD SrcSizeY = Region.SrcTexture->GetSizeY();
		const DWORD DestSizeX = DstTexture->GetSizeX();
		const DWORD DestSizeY = DstTexture->GetSizeY();
		const INT SrcMipIndex = Region.FirstMipIdx + MipIdx;
		
		check(Region.SrcTexture->GetUnrealFormat() == Format);

		// calculate mip info
		const DWORD SrcMipOffset = GetTextureMipOffset(Format, SrcSizeX, SrcSizeY, SrcMipIndex, bIsLinear);
		const DWORD DestMipOffset = GetTextureMipOffset(Format, DestSizeX, DestSizeY, MipIdx, bIsLinear);

		const DWORD SrcPitch = GetTexturePitch(Format, SrcSizeX, SrcMipIndex, bIsLinear);
		const DWORD DestPitch = GetTexturePitch(Format, DestSizeX, MipIdx, bIsLinear);

		// align/truncate the region offset to block size
		const INT RegionOffsetX = Clamp( Region.OffsetX, 0, MipSizeX - BlockSizeX ) / BlockSizeX * BlockSizeX;
		const INT RegionOffsetY = Clamp( Region.OffsetY, 0, MipSizeY - BlockSizeY ) / BlockSizeY * BlockSizeY;
		// scale region size to the current mip level. Size is aligned to the block size
		check(Region.SizeX != 0 && Region.SizeY != 0);
		INT RegionSizeX = Clamp( Align( Region.SizeX, BlockSizeX), 0, MipSizeX );
		INT RegionSizeY = Clamp( Align( Region.SizeY, BlockSizeY), 0, MipSizeY );
		// handle special case for full copy
		if( Region.SizeX == -1 || Region.SizeY == -1 )
		{
			RegionSizeX = MipSizeX;
			RegionSizeY = MipSizeY;
		}

		// size in bytes of the offset to the starting part of the row to copy for this mip
		const DWORD RowOffsetBytes = (RegionOffsetX / BlockSizeX) * BlockBytes;
		// size in bytes of the amount to copy within each row
		const DWORD RowSizeBytes = (RegionSizeX / BlockSizeX) * BlockBytes;

		cellGcmSetTransferData(
			Mode, 
			DstTexture->MemoryOffset + DestMipOffset + (RegionOffsetY / BlockSizeY) * DestPitch + RowOffsetBytes,
			DestPitch,
			Region.SrcTexture->MemoryOffset + SrcMipOffset + (RegionOffsetY / BlockSizeY) * SrcPitch + RowOffsetBytes,
			SrcPitch,
			(RegionSizeX / BlockSizeX) * BlockBytes,
			RegionSizeY / BlockSizeY
			);
	}
}

/**
 * Copies texture data from one mip to another
 * Note that the mips must be the same size and of the same format.
 * @param SrcText Source texture to copy from
 * @param SrcMipIndex Mip index into the source texture to copy data from
 * @param DestText Destination texture to copy to
 * @param DestMipIndex Mip index in the destination texture to copy to - note this is probably different from source mip index if the base widths/heights are different
 * @param Size Size of mip data (ignored on PS3)
 * @param Counter Thread safe counter used to flag end of transfer (ignored on PS3)
 *
 * The reason we don't need to use the Counter is that the transfer happens with
 * a GPU command in the command buffer. So, the GPU won't use the new texture
 * until this has been copied over, because of normal command buffer ordering.
 * Also, we don't have to worry about the engine destroying the old texture
 * while the GPU is still using it because GPU resource deletion is delayed 
 * already until the next frame. Magic! :)
 */
void RHICopyMipToMipAsync(FTexture2DRHIParamRef SrcTexture, INT SrcMipIndex, FTexture2DRHIParamRef DestTexture, INT DestMipIndex, INT /*Size*/, FThreadSafeCounter& /*Counter*/)
{
	// The texturepool makes all textures in Local memory
	BYTE Mode = CELL_GCM_TRANSFER_LOCAL_TO_LOCAL;

	check(SrcTexture->MemoryRegion == MR_Local&& DestTexture->MemoryRegion == MR_Local);
	check(SrcTexture->GetUnrealFormat() == DestTexture->GetUnrealFormat());
	check(SrcTexture->IsLinear() == DestTexture->IsLinear());

	// cache some stuff
	const EPixelFormat Format = SrcTexture->GetUnrealFormat();
	const UBOOL bIsLinear = SrcTexture->IsLinear();
	const DWORD SrcSizeX = SrcTexture->GetSizeX();
	const DWORD SrcSizeY = SrcTexture->GetSizeY();
	const DWORD DestSizeX = DestTexture->GetSizeX();
	const DWORD DestSizeY = DestTexture->GetSizeY();

	// calculate mip info
	const DWORD SrcMipOffset = GetTextureMipOffset(Format, SrcSizeX, SrcSizeY, SrcMipIndex, bIsLinear);
	const DWORD DestMipOffset = GetTextureMipOffset(Format, DestSizeX, DestSizeY, DestMipIndex, bIsLinear);

	const DWORD SrcPitch = GetTexturePitch(Format, SrcSizeX, SrcMipIndex, bIsLinear);
	const DWORD DestPitch = GetTexturePitch(Format, DestSizeX, DestMipIndex, bIsLinear);
	const DWORD NumBytesPerRow = Min<DWORD>(SrcPitch, DestPitch);

	// @todo: do perf testing vs SetTransferImage (which is less general purpose)
	cellGcmSetTransferData(
		Mode, 
		DestTexture->MemoryOffset + DestMipOffset,
		DestPitch,
		SrcTexture->MemoryOffset + SrcMipOffset,
		SrcPitch,
		NumBytesPerRow,
		GetMipNumRows(Format, DestSizeY, DestMipIndex));
}

void RHIFinalizeAsyncMipCopy(FTexture2DRHIParamRef SrcTexture, INT SrcMipIndex, FTexture2DRHIParamRef DestTexture, INT DestMipIndex)
{
}

/**
 * Constructor
 */
FPS3RHITextureCube::FPS3RHITextureCube(DWORD InSizeX, DWORD InSizeY, EPixelFormat InFormat, DWORD NumMips, UBOOL bIsLinearTexture, DWORD Flags, FResourceBulkDataInterface* BulkData )
{
	InitializeInternals(InSizeX, InSizeY, InFormat, NumMips, bIsLinearTexture, Flags, TRUE, Flags & TexCreate_SRGB, FALSE, BulkData);
}

/**
* Creates a Cube RHI texture resource
* @param Size - width/height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
FTextureCubeRHIRef RHICreateTextureCube( UINT Size, BYTE Format, UINT NumMips, DWORD Flags, FResourceBulkDataInterface* BulkData )
{
	return new FPS3RHITextureCube(Size, Size, (EPixelFormat)Format, NumMips, (Flags & TexCreate_Uncooked), Flags, BulkData);
}

/**
* Locks an RHI texture's mip surface for read/write operations on the CPU
* @param Texture - the RHI texture resource to lock
* @param MipIndex - mip level index for the surface to retrieve
* @param bIsDataBeingWrittenTo - used to affect the lock flags 
* @param DestStride - output to retrieve the textures row stride (pitch)
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
* @return pointer to the CPU accessible resource data
*/
void* RHILockTextureCubeFace(FTextureCubeRHIParamRef Texture,UINT FaceIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	Texture->Lock(MipIndex);
	UBOOL bIsLinear = Texture->IsLinear();
	DWORD NumMipmaps = Texture->GetNumMipmaps();

	// Calculate offset to the given mip level, and the total texture size of each face
	DWORD FaceSize = 0;
	DWORD MipOffset = 0;
	for (DWORD Index = 0; Index < NumMipmaps; Index++)
	{
		if ( Index == MipIndex )
		{
			MipOffset = FaceSize;
		}
		DWORD MipSize = GetTextureMipSize(Texture->GetUnrealFormat(), Texture->GetSizeX(), Texture->GetSizeY(), Index, bIsLinear);
		FaceSize += MipSize;
	}
	FaceSize = Align(FaceSize, PS3TEXTURE_ALIGNMENT);

	DestStride = GetTexturePitch(Texture->GetUnrealFormat(), Texture->GetSizeX(), MipIndex, bIsLinear);

	check(Texture->Data);
	// skip over all previous mips and all previous faces in this mip
	return (BYTE*)Texture->Data + FaceSize * FaceIndex + MipOffset;
}

/**
* Unlocks a previously locked RHI texture resource
* @param Texture - the RHI texture resource to unlock
* @param MipIndex - mip level index for the surface to unlock
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
*/
void RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef Texture,UINT FaceIndex,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	Texture->Unlock(MipIndex);
}

/*-----------------------------------------------------------------------------
FTexture2DResourceMemPS3
-----------------------------------------------------------------------------*/

/** 
 * Init Constructor 
 * Allocates texture memory during construction
 * 
 * @param InSizeX		- width of texture
 * @param InSizeY		- height of texture
 * @param InNumMips		- total number of mips to allocate memory for
 * @param InFormat		- EPixelFormat texture format
 * @param AsyncCounter	- If provided, starts an async allocation. If NULL, allocates memory immediately.
 */
FTexture2DResourceMemPS3::FTexture2DResourceMemPS3(INT InSizeX, INT InSizeY, INT InNumMips, EPixelFormat InFormat, FThreadSafeCounter* AsyncCounter)
:	SizeX(InSizeX)
,	SizeY(InSizeY)
,	NumMips(InNumMips)
,	Format(InFormat)
,	BaseAddress(NULL)
,	TextureSize(0)
,	bAllowFailure(TRUE)
,	ReallocationRequest(NULL)
{
	Init();

	// First try to allocate immediately.
	AllocateTextureMemory(FALSE);

	// Did the allocation fail and the user requested async allocation?
	if ( AsyncCounter && !IsValid() )
	{
		ReallocationRequest = new FAsyncReallocationRequest(NULL, TextureSize, AsyncCounter);
		UBOOL bSuccess = GPS3Gcm->GetTexturePool()->AsyncReallocate( ReallocationRequest, TRUE );
		if ( !bSuccess )
		{
			delete ReallocationRequest;
			ReallocationRequest = NULL;
		}
	}

	// Did the immediate allocation fail and we don't have any async reallocation in progress?
	if ( !IsValid() && ReallocationRequest == NULL )
	{
		// Try to allocate again, this time freeing up memory, defragging, and retrying a few times.
		AllocateTextureMemory( TRUE );
	}

	// Did the user request async allocation but we have none in progress?
	if ( AsyncCounter && ReallocationRequest == NULL )
	{
		// Report completion, no matter if we succeeded or failed.
		AsyncCounter->Decrement();
	}
}

/** 
* Destructor
*/
FTexture2DResourceMemPS3::~FTexture2DResourceMemPS3()
{
	// mip data gets deleted by a texture resource destructor once this memory is used for RHI resource creation
	// but, if it was never used then free the memory here
	FreeTextureMemory();
}

/** 
* @return ptr to the resource memory which has been preallocated
*/
void* FTexture2DResourceMemPS3::GetResourceBulkData() const
{
	return BaseAddress;
}

/** 
* @return size of resource memory
*/
DWORD FTexture2DResourceMemPS3::GetResourceBulkDataSize() const
{
	return TextureSize;
}

/**
* Free memory after it has been used to initialize RHI resource 
*/
void FTexture2DResourceMemPS3::Discard()
{
	// no longer maintain a pointer to mip data since it is owned by RHI resource after creation	
	BaseAddress = NULL;
	TextureSize = 0;
}

/**
* @param MipIdx index for mip to retrieve
* @return ptr to the offset in bulk memory for the given mip
*/
void* FTexture2DResourceMemPS3::GetMipData(INT MipIdx)
{
	check(MipIdx >= 0 && MipIdx < NumMips);
	UBOOL bIsLinearTexture = RequireLinearTexture(Format, SizeX, SizeY);
	DWORD Offset = GetTextureMipOffset(Format, SizeX, SizeY, MipIdx, bIsLinearTexture);
	return (BYTE*)BaseAddress + Offset;
}

/**
* @return total number of mips stored in this resource
*/
INT	FTexture2DResourceMemPS3::GetNumMips()
{
	return NumMips;
}

/** 
* @return width of texture stored in this resource
*/
INT FTexture2DResourceMemPS3::GetSizeX()
{
	return SizeX;
}

/** 
* @return height of texture stored in this resource
*/
INT FTexture2DResourceMemPS3::GetSizeY()
{
	return SizeY;
}

/**
 * Whether the resource memory is properly allocated or not.
 **/
UBOOL FTexture2DResourceMemPS3::IsValid()
{
	return GPS3Gcm->GetTexturePool()->IsValidTextureData(BaseAddress);
}

/** 
* Calculate size needed to store this resource
*/
void FTexture2DResourceMemPS3::Init()
{
	check(BaseAddress == NULL);

	UBOOL bIsLinearTexture = RequireLinearTexture(Format, SizeX, SizeY);
	for (DWORD MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		TextureSize += GetTextureMipSize(Format, SizeX, SizeY, MipIndex, bIsLinearTexture);
	}	
}

/** 
 * Calculate size needed to store this resource and allocate texture memory for it
 *
 * @param bAllowRetry	If TRUE, retry the allocation after panic-stream-out/panic-defrag upon failure
 */
void FTexture2DResourceMemPS3::AllocateTextureMemory( UBOOL bAllowRetry )
{
	check( IsValid() == FALSE );

	// Allocate memory for this texture resource from texture pool. Try one time.
	BaseAddress = GPS3Gcm->GetTexturePool()->Allocate( TextureSize, bAllowFailure || bAllowRetry );

	// Did it fail? Retry 3 times
	INT NumRetries = bAllowRetry ? 3 : 0;
	while ( NumRetries && IsValid() == FALSE )
	{
		FreeTextureMemory();

		// Try to stream out texture data to allow for this texture to fit.
		GStreamingManager->StreamOutTextureData( 2*TextureSize );

		// Last attempt?
		if ( NumRetries == 1 )
		{
			// Don't allow failure this time. This will also trigger a panic defrag if necessary.
			bAllowFailure = FALSE;
		}

		BaseAddress = GPS3Gcm->GetTexturePool()->Allocate( TextureSize, bAllowFailure );

		NumRetries--;
	}
}

/**
 * Free the memory from the texture pool
 */
void FTexture2DResourceMemPS3::FreeTextureMemory()
{
	if ( IsValid() )
	{
		// add this texture resource to the list of resources to be deleted later
		// this will free the memory from that was allocated from the texture pool
		AddUnusedPS3Resource(BaseAddress, MR_Local, TRUE, FALSE);
	}
	BaseAddress = NULL;
}

/**
 * Blocks the calling thread until the allocation has been completed.
 */
void FTexture2DResourceMemPS3::FinishAsyncAllocation()
{
	if ( ReallocationRequest )
	{
		if ( ReallocationRequest->HasStarted() )
		{
			check( BaseAddress == NULL );
			GPS3Gcm->GetTexturePool()->BlockOnAsyncReallocation( ReallocationRequest );
			check( ReallocationRequest->HasCompleted() );
			BaseAddress = ReallocationRequest->GetNewBaseAddress();

			delete ReallocationRequest;
			ReallocationRequest = NULL;
		}
		else
		{
			// Async allocation hasn't even started yet. (This also updates the external async counter.)
			CancelAsyncAllocation();

			// Final attempt, which will most likely fail anyway but at least set the BaseAddress to the fail-pointer.
			AllocateTextureMemory( TRUE );
		}
	}
}

/**
 * Cancels any async allocation.
 */
void FTexture2DResourceMemPS3::CancelAsyncAllocation()
{
	if ( ReallocationRequest )
	{
		// Note: This also updates the external async counter.
		GPS3Gcm->GetTexturePool()->CancelAsyncReallocation( ReallocationRequest, NULL );
		delete ReallocationRequest;
		ReallocationRequest = NULL;
	}
}

#else	// USE_PS3_RHI

UBOOL RequireLinearTexture(INT Format, DWORD SizeX, DWORD SizeY)
{
	return (!appIsPowerOfTwo(SizeX) || !appIsPowerOfTwo(SizeY));
}

DWORD GetTexturePitch(INT Format, DWORD SizeX, DWORD MipIndex, UBOOL bIsLinear)
{
	return 0;
}

DWORD GetMipNumRows(INT Format, DWORD SizeY, DWORD MipIndex)
{
	return 0;
}

#endif // USE_PS3_RHI

