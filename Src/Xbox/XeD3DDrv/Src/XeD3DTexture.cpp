/*=============================================================================
	D3DVertexBuffer.cpp: D3D texture RHI implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "XeD3DDrvPrivate.h"

#if USE_XeD3D_RHI

/** Whether we allow growing in-place reallocations. */
UBOOL GEnableGrowingReallocations = TRUE;

/** Whether we allow shrinking in-place reallocations. */
UBOOL GEnableShrinkingReallocations = TRUE;

/**
 * Uses the XDK to calculate the amount of texture memory needed for the mip data.
 *
 * @param SizeX				Width of the texture
 * @param SizeY				Height of the texture
 * @param Format			Unreal texture format
 * @param NumMips			Number of mips, including the top mip
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 * @return					amount of texture memory needed for the mip data, in bytes
 */
INT XeGetTextureSize( UINT SizeX, UINT SizeY, EPixelFormat Format, UINT NumMips, DWORD TexCreateFlags );

/**
 * Initializes a D3D texture struct.
 *
 * @param SizeX				Width of the texture
 * @param SizeY				Height of the texture
 * @param Format			Unreal texture format
 * @param NumMips			Number of mips, including the top mip
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 * @param D3DTexture		[out] D3D texture struct to initialize
 * @return					amount of texture memory needed for the mip data, in bytes
 */
INT XeSetTextureHeader( UINT SizeX, UINT SizeY, EPixelFormat Format, UINT NumMips, DWORD TexCreateFlags, IDirect3DTexture9* D3DTexture );

/**
 * Initializes a new DirectX texture header, based on an existing header and new settings.
 *
 * @param OldTexture2D		Texture to base the new D3D texture header on
 * @param NewMipCount		New number of mip-levels (including base mip)
 * @param NewSizeX			Number of pixels (along X) in the new texture header
 * @param NewSizeY			Number of pixels (along Y) in the new texture header
 * @param OutNewTexture		[out] Texture header to initialize
 * @return					Number of bytes used by all mips for the new texture
 */
INT XeCreateNewTextureFrom( FXeTexture2D* OldTexture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, IDirect3DTexture9*& OutNewTexture );

/** 
* @param	Format - unreal format type
* @param	Flags - creation flags
* @return	D3D texture format for the unreal texture format 
*/
D3DFORMAT XeGetTextureFormat(BYTE Format,DWORD Flags)
{
	// convert to platform specific format
	D3DFORMAT D3DFormat = (D3DFORMAT)GPixelFormats[Format].PlatformFormat;
	// sRGB is handled as a surface format on Xe
	if( Flags&TexCreate_SRGB )
	{
		D3DFormat = (D3DFORMAT)MAKESRGBFMT(D3DFormat);
	}	
	// handle un-tiled formats
	if( (Flags&TexCreate_NoTiling) || (Flags & TexCreate_Uncooked) )
	{
		D3DFormat = (D3DFORMAT)MAKELINFMT(D3DFormat);
	}
	return D3DFormat;
}

/**
 *	Calculates the offset to the specified mip-level, in bytes from the baseaddress.
 *	@param D3DTexture	- D3D9 texture struct
 *	@param MipIndex		- Mip-level to calculate the offset for
 *	@return				- Mip-level offset, in bytes from the baseaddress
 */
INT XeGetMipOffset( IDirect3DTexture9* D3DTexture, INT MipIndex )
{
	UINT BaseAddress = 0;
	UINT MipAddress = 0;
	XGGetTextureLayout(D3DTexture,&BaseAddress,NULL,NULL,NULL,0,&MipAddress,NULL,NULL,NULL,0);
	INT MipChainOffset = INT(MipAddress - BaseAddress);

	XGTEXTURE_DESC Desc;
	XGGetTextureDesc(D3DTexture, 0, &Desc);
	INT MipOffset = XGGetMipLevelOffset(D3DTexture, 0, MipIndex);
	DWORD MipTailIndex = XGGetMipTailBaseLevel(Desc.Width, Desc.Height, FALSE);
	UBOOL bHasPackedMipTail = XGIsPackedTexture(D3DTexture);

	// Is this the base level?
	if ( MipIndex == 0 || MipAddress == 0 )
	{
		// Is the base level a packed mip-map tail?
		if ( bHasPackedMipTail && MipIndex == MipTailIndex )
		{
			MipOffset = 0;
		}
	}
	// Is this a packed mip tail?
	else if ( bHasPackedMipTail && MipIndex == MipTailIndex )
	{
		// Get the address of the previous mip-level.
		INT PrevMipOffset;
		if ( MipIndex == 1 )
		{
			PrevMipOffset = 0;
		}
		else
		{
			PrevMipOffset = MipChainOffset + XGGetMipLevelOffset(D3DTexture, 0, MipIndex - 1);
		}

		// Add its mip-size to reach the requested mip-level.
		XGTEXTURE_DESC Desc;
		XGGetTextureDesc(D3DTexture, MipIndex - 1, &Desc);
		MipOffset = PrevMipOffset + Desc.SlicePitch;
	}
	else
	{
		MipOffset += MipChainOffset;
	}

	return MipOffset;
}


/*-----------------------------------------------------------------------------
	FXeTextureBase
-----------------------------------------------------------------------------*/

/** Destructor */
FXeTextureBase::~FXeTextureBase()
{
	// Did the user delete a locked texture?
	if ( IsLocked() )
	{
		LockedMipLevels = 0;
		if ( BaseAddress )
		{
			// Make sure to unlock it.
			GTexturePool.Unlock( BaseAddress );
		}
	}

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
		BaseAddress = NULL;
		Resource = NULL;
	}
	else if ( BaseAddress )
	{
		GTexturePool.UnregisterTexture( this );
	}
}

/**
 *	Notifies that the texture data is being reallocated and is shared for a brief moment,
 *	so that this texture can do the right thing upon destruction.
 *
 *	@param InReallocatedTexture	- The other texture that briefly shares the texture data when it's reallocated
 *	@param bReallocationOwner	- TRUE if 'ReallocatedTexture' refers to the old texture
 **/
void FXeTextureBase::SetReallocatedTexture( FXeTextureBase* InReallocatedTexture, UBOOL bInReallocationOwner )
{
	ReallocatedTexture = InReallocatedTexture;
	bReallocationOwner = bInReallocationOwner;
}

/** Checks whether this texture can be relocated or not by the defragmentation process. */
UBOOL FXeTextureBase::CanRelocate() const
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
ETextureReallocationStatus FXeTextureBase::GetReallocationStatus() const
{
	if ( ReallocationRequest )
	{
		if ( ReallocationRequest->HasCompleted() )
		{
			return TexRealloc_Succeeded;
		}
		return TexRealloc_InProgress;
	}
	return (BaseAddress && ResourceSize) ? TexRealloc_Succeeded : TexRealloc_Failed;
}

/**
 * Called by the texture allocator when an async reallocation request has been completed.
 *
 * @param OldTexture		Original texture. OldTexture->ReallocatedTexture refers to the new texture.
 * @param FinishedRequest	The request that got completed
 */
void FXeTextureBase::OnReallocationFinished( FXeTextureBase* OldTexture, FAsyncReallocationRequest* FinishedRequest )
{
	if ( FinishedRequest->IsReallocation() )
	{
		check( OldTexture && OldTexture->ReallocatedTexture && !OldTexture->ReallocationRequest );
		FXeTextureBase* NewTexture = OldTexture->ReallocatedTexture;
		check( NewTexture && NewTexture->ReallocatedTexture && NewTexture->ReallocationRequest == FinishedRequest );
		check( FinishedRequest->HasCompleted() );
		check( NewTexture->BaseAddress == NULL );

		NewTexture->BaseAddress = FinishedRequest->GetNewBaseAddress();
		NewTexture->ResourceSize = FinishedRequest->GetNewSize();
		XGOffsetResourceAddress( (IDirect3DTexture9*)NewTexture->Resource, NewTexture->BaseAddress );
		GTexturePool.RegisterTexture( NewTexture );
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
ETextureReallocationStatus FXeTextureBase::FinalizeAsyncReallocateTexture2D( FXeTextureBase* Texture, UBOOL bBlockUntilCompleted )
{
	if ( Texture->ReallocationRequest == NULL )
	{
		return TexRealloc_Failed;
	}
	if ( bBlockUntilCompleted && !Texture->ReallocationRequest->HasCompleted() )
	{
		GTexturePool.BlockOnAsyncReallocation( Texture->ReallocationRequest );
	}
	ETextureReallocationStatus Status = Texture->GetReallocationStatus();
	return Status;
}

/**
 * Cancels an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If TRUE, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus FXeTextureBase::CancelAsyncReallocateTexture2D( FXeTextureBase* Texture, UBOOL bBlockUntilCompleted )
{
	if ( Texture->IsLocked() )
	{
		Texture->LockedMipLevels = 0;
		if ( Texture->BaseAddress )
		{
			GTexturePool.Unlock( Texture->BaseAddress );
		}
	}

	FXeTextureBase* OldTexture = Texture->ReallocatedTexture;
	check( OldTexture );

	ETextureReallocationStatus Status = Texture->GetReallocationStatus();
	if ( Texture->ReallocationRequest )
	{
		GTexturePool.CancelAsyncReallocation( Texture->ReallocationRequest, Texture->BaseAddress );
		if ( bBlockUntilCompleted )
		{
			GTexturePool.BlockOnAsyncReallocation( Texture->ReallocationRequest );
		}
	}
	// Are we canceling an immediate reallocation?
	else if ( Status == TexRealloc_Succeeded )
	{
		// Can't cancel a Shrink, only a Grow.
		check( OldTexture->ResourceSize < Texture->ResourceSize );

		// Shrink it back to the original size.
		void* NewBaseAddress = GTexturePool.Reallocate( Texture->BaseAddress, OldTexture->ResourceSize );
		check( NewBaseAddress && NewBaseAddress == OldTexture->BaseAddress );
	}

	// Remove any remnants of reallocation from our side. (The allocator will keep a copy of the request if necessary.)
	OldTexture->SetReallocatedTexture( NULL, TRUE );
	Texture->SetReallocatedTexture( NULL, FALSE );
	Texture->BaseAddress = 0;
	Texture->ResourceSize = 0;
	delete Texture->ReallocationRequest;
	Texture->ReallocationRequest = NULL;

	// Delete the D3D resource struct.
	delete Texture->Resource;
	Texture->Resource = NULL;

	// Shouldn't be really necessary re-register but won't hurt...
	GTexturePool.RegisterTexture( OldTexture );

	return Status;
}

/**
 * Associates the texture with the current GPU fence.
 */
void FXeTextureBase::LockGPUFence()
{
	Fence = GDirect3DDevice->GetCurrentFence();
}

/**
 * Blocks until the associated GPU fence has been passed.
 */
void FXeTextureBase::UnlockGPUFence()
{
	GDirect3DDevice->BlockOnFence( Fence );
	Fence = 0;
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

/** 
 * Sets D3D header and allocates memory for a D3D resource 
 *
 * @param SizeX			- width of texture
 * @param SizeY			- height of texture
 * @param Format		- format of texture
 * @param NumMips		- number of mips
 * @param Flags			- creation flags
 * @param Texture		- [in/out optional] IDirect3DTexture9 header to initialize
 * @param OutMemorySize	- [out] Memory size of the allocation in bytes
 * @param BulkData		- [optional] pre-allocated memory containing the bulk data
 * @return				- Base address of the allocation
 */
void* FXeTexture2D::AllocTexture2D(UINT SizeX,UINT SizeY,EPixelFormat Format,UINT NumMips,DWORD TexCreateFlags,IDirect3DTexture9* Texture,DWORD& OutMemorySize,FResourceBulkDataInterface* BulkData)
{
	// Set the texture header
	OutMemorySize = XeSetTextureHeader( SizeX, SizeY, Format, NumMips, TexCreateFlags, Texture );
	void* BaseAddress;
	if ( BulkData )
	{
		// Get preallocated memory for this resource and hold onto it 
		BaseAddress = BulkData->GetResourceBulkData();
		DWORD BulkDataSize = BulkData->GetResourceBulkDataSize();
		checkf(OutMemorySize == BulkDataSize, TEXT("Resource bulkdata size mismatch Size=%d BulkDataSize=%d"),OutMemorySize,BulkDataSize);
		// Discard will just clear data pointer since it will be freed by when this resource is released 
		BulkData->Discard();
	}
	else
	{
		BaseAddress = GTexturePool.Allocate( OutMemorySize, (TexCreateFlags & TexCreate_AllowFailure) ? TRUE : FALSE, (TexCreateFlags & TexCreate_DisableAutoDefrag) ? TRUE : FALSE );
	}

	// Set data address for this resource
	XGOffsetResourceAddress( Texture, BaseAddress );

	return BaseAddress;
}

/**
 * Uses the XDK to calculate the amount of texture memory needed for the mip data.
 *
 * @param SizeX				Width of the texture
 * @param SizeY				Height of the texture
 * @param Format			Unreal texture format
 * @param NumMips			Number of mips, including the top mip
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 * @return					amount of texture memory needed for the mip data, in bytes
 */
INT XeGetTextureSize( UINT SizeX, UINT SizeY, EPixelFormat Format, UINT NumMips, DWORD TexCreateFlags )
{
	IDirect3DTexture9 DummyTexture;
	return XeSetTextureHeader( SizeX, SizeY, Format, NumMips, TexCreateFlags, &DummyTexture );
}

/**
 * Initializes a D3D texture struct.
 *
 * @param SizeX				Width of the texture
 * @param SizeY				Height of the texture
 * @param Format			Unreal texture format
 * @param NumMips			Number of mips, including the top mip
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 * @param D3DTexture		[out] D3D texture struct to initialize
 * @return					amount of texture memory needed for the mip data, in bytes
 */
INT XeSetTextureHeader( UINT SizeX, UINT SizeY, EPixelFormat Format, UINT NumMips, DWORD TexCreateFlags, IDirect3DTexture9* D3DTexture )
{
	// set the texture header
	UINT TextureSize = XGSetTextureHeaderEx( 
		SizeX,
		SizeY,
		NumMips, 
		0,
		XeGetTextureFormat(Format,TexCreateFlags),
		0,
		(TexCreateFlags&TexCreate_NoMipTail) ? XGHEADEREX_NONPACKED : 0,
		0,
		XGHEADER_CONTIGUOUS_MIP_OFFSET,
		0,
		D3DTexture,
		NULL,
		NULL
		);

	UINT UnusedMipTailSize = XeCalcUnusedMipTailSize( SizeX, SizeY, Format, NumMips, (TexCreateFlags & TexCreate_NoMipTail) ? FALSE : TRUE );
	TextureSize -= UnusedMipTailSize;

	return Align(TextureSize, D3DTEXTURE_ALIGNMENT);
}

/**
 * Initializes a new DirectX texture header, based on an existing header and new settings.
 *
 * @param OldTexture2D		Texture to base the new D3D texture header on
 * @param NewMipCount		New number of mip-levels (including base mip)
 * @param NewSizeX			Number of pixels (along X) in the new texture header
 * @param NewSizeY			Number of pixels (along Y) in the new texture header
 * @param OutNewTexture		[out] Texture header to initialize
 * @return					Number of bytes used by all mips for the new texture
 */
INT XeCreateNewTextureFrom( FXeTexture2D* OldTexture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, IDirect3DTexture9*& OutNewTexture )
{
	IDirect3DTexture9* OldD3DTexture = (IDirect3DTexture9*) OldTexture2D->Resource;

	XGTEXTURE_DESC Desc;
	XGGetTextureDesc( OldD3DTexture, 0, &Desc);
	DWORD TextureFlags = XGIsPackedTexture( OldD3DTexture) ? 0 : XGHEADEREX_NONPACKED;
	DWORD TextureSize = XGSetTextureHeaderEx( NewSizeX, NewSizeY, NewMipCount, 0, Desc.Format, 0, TextureFlags, 0, XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, OutNewTexture, NULL, NULL ); 
	UINT UnusedMipTailSize = XeCalcUnusedMipTailSize( NewSizeX, NewSizeY, OldTexture2D->GetUnrealFormat(), NewMipCount, (TextureFlags & XGHEADEREX_NONPACKED) ? FALSE : TRUE );
	TextureSize -= UnusedMipTailSize;
	return TextureSize;
}

/** 
 * Tries to add or remove mip-levels of the texture without relocation.
 *
 * @param Texture2D		- Source texture
 * @param NewMipCount	- Number of desired mip-levels
 * @param NewSizeX		- New width, in pixels
 * @param NewSizeY		- New height, in pixels
 * @return				- A newly allocated FXeTexture2D if successful, or NULL if it failed.
 */
FXeTexture2D* FXeTexture2D::ReallocateTexture2D(FXeTexture2D* Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY )
{
	// Create and setup a new D3D struct.
	IDirect3DTexture9* NewD3DTexture = new IDirect3DTexture9;
	DWORD NewTextureSize = XeCreateNewTextureFrom( Texture2D, NewMipCount, NewSizeX, NewSizeY, NewD3DTexture );

	// Try to resize the texture in-place, by moving the baseaddress to encompass the new mip-levels.
	// Note: Newly freed memory due to shrinking won't be available for allocation right away (need GPU sync).
	void* NewBaseAddress = GTexturePool.Reallocate( Texture2D->BaseAddress, NewTextureSize );

	// Did it succeed?
	if ( NewBaseAddress )
	{
		XGOffsetResourceAddress( NewD3DTexture, NewBaseAddress );

		// Create a 2D texture resource.
		FXeTexture2D* NewTexture2D = new FXeTexture2D( NewD3DTexture, Texture2D->GetUnrealFormat(), Texture2D->GetCreationFlags(), NewTextureSize );
		NewTexture2D->BaseAddress = NewBaseAddress;
		check(NewTexture2D->BaseAddress == Align(NewTexture2D->BaseAddress, D3DTEXTURE_ALIGNMENT));

#if TRACK_GPU_RESOURCES
	    NewTextureSize = Align(NewTextureSize,D3DTEXTURE_ALIGNMENT);
		NewTexture2D->PhysicalSize = NewTextureSize;
		NewTexture2D->VirtualSize = sizeof(IDirect3DTexture9);
#endif

		// Let the resources know about the reallocation, so it knows what to do with the memory upon destruction.
		Texture2D->SetReallocatedTexture( NewTexture2D, FALSE );
		NewTexture2D->SetReallocatedTexture( Texture2D, TRUE );

		GTexturePool.RegisterTexture( NewTexture2D );

		// Return the new resource.
		return NewTexture2D;
	}
	else
	{
		// Failure, cleanup the D3D struct.
		delete NewD3DTexture;
	}

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
 * @return				Newly created FXeTexture2D if the request was accepted, or NULL
 */
FXeTexture2D* FXeTexture2D::AsyncReallocateTexture2D( FXeTexture2D* Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, FThreadSafeCounter* RequestStatus )
{
	FXeTexture2D* NewTexture = NULL;

	// Only try reallocation if continuous defragmentation is enabled.
	FBestFitAllocator::FSettings AllocatorSettings;
	GTexturePool.GetSettings( AllocatorSettings );
	if ( AllocatorSettings.bEnableAsyncDefrag )
	{
		IDirect3DTexture9* NewD3DTexture = new IDirect3DTexture9;
		DWORD NewTextureSize = XeCreateNewTextureFrom( Texture2D, NewMipCount, NewSizeX, NewSizeY, NewD3DTexture );
		FAsyncReallocationRequest* ReallocationRequest = new FAsyncReallocationRequest( Texture2D->BaseAddress, NewTextureSize, RequestStatus );
		if ( GTexturePool.AsyncReallocate( ReallocationRequest, FALSE ) )
		{
			// Keep ResourceSize == 0 until the reallocation request has been completed.
			NewTexture = new FXeTexture2D( NewD3DTexture, Texture2D->GetUnrealFormat(), Texture2D->GetCreationFlags(), 0, ReallocationRequest );

			// Let the resources know about the reallocation, so it knows what to do with the memory upon destruction.
			Texture2D->SetReallocatedTexture( NewTexture, FALSE );
			NewTexture->SetReallocatedTexture( Texture2D, TRUE );

			if ( ReallocationRequest->HasCompleted() )
			{
				FXeTexture2D::OnReallocationFinished( Texture2D, ReallocationRequest );
			}
		}
		else
		{
			delete ReallocationRequest;
			delete NewD3DTexture;
		}
	}
	return NewTexture;
}

/** 
 * Writes out the actual memory layout of a textures bottom mip levels to a .bmp 
 * it is assumed this texture is using a packed mip tail
 *
 * @param	Texture - the texture to write out
 * @param	FileName - name of file (.bmp extension will be added)
 */
void XeWriteOutTextureMipTail(FTexture2DRHIRef Texture, TCHAR *FileName)
{
	int MipIdx = RHIGetMipTailIdx(Texture) - 2;

	if(MipIdx >= 0)
	{
		UINT DestStride = 0;
		VOID *BaseAddress = RHILockTexture2D(Texture,MipIdx,FALSE,DestStride,FALSE);

		IDirect3DTexture9* ResourceTexture = (IDirect3DTexture9*)Texture->Resource;
		D3DSURFACE_DESC Desc;
		ResourceTexture->GetLevelDesc(0, &Desc);

		IDirect3DTexture9 *TempD3DTexture = new IDirect3DTexture9();
		XGSetTextureHeaderEx(128,384,1,0,(D3DFORMAT)(MAKELINFMT(Desc.Format)),0,0,0,0,0,TempD3DTexture,NULL,NULL);
		XGOffsetResourceAddress( TempD3DTexture, BaseAddress );

		FString UniqueFilePath = FString("GAME:") * FileName + TEXT(".bmp");

		D3DXSaveTextureToFileA(TCHAR_TO_ANSI(*UniqueFilePath), D3DXIFF_BMP, TempD3DTexture, NULL);

		delete TempD3DTexture;
		RHIUnlockTexture2D(Texture,MipIdx,FALSE);

	}
}

/**
 * Creates a 2D RHI texture resource
 * @param SizeX		- width of the texture to create
 * @param SizeY		- height of the texture to create
 * @param Format	- EPixelFormat texture format
 * @param NumMips	- number of mips to generate or 0 for full mip pyramid
 * @param Flags		- ETextureCreateFlags creation flags
 */
FTexture2DRHIRef RHICreateTexture2D(UINT SizeX,UINT SizeY,BYTE InFormat,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData)
{
	FXeTexture2D* XeTexture = NULL;
	EPixelFormat Format = (EPixelFormat) InFormat;

	if( (Flags&TexCreate_ResolveTargetable) || (Flags & TexCreate_Uncooked) )
	{
		check(!BulkData);
		check( !((Flags&TexCreate_ResolveTargetable) && NumMips > 1) );

		IDirect3DTexture9* pD3DTexture2D;
		// create texture in memory for resolving surface
		HRESULT D3DResult = GDirect3DDevice->CreateTexture( 
			SizeX, 
			SizeY, 
			NumMips, 
			0, 
			XeGetTextureFormat(Format,Flags), 
			D3DPOOL_DEFAULT, 
			&pD3DTexture2D, 
			NULL );

		if ( Flags & TexCreate_AllowFailure )
		{
			XeTexture = (D3DResult == S_OK) ? new FXeTexture2D(pD3DTexture2D,Format,Flags,0) : NULL;
		}
		else
		{
			VERIFYD3DRESULT( D3DResult );
			XeTexture = new FXeTexture2D(pD3DTexture2D,Format,Flags,0);
		}
#if TRACK_GPU_RESOURCES
		if (XeTexture)
		{
			XeTexture->VirtualSize = sizeof(D3DTexture);
			XeTexture->PhysicalSize = XeGetTextureSize( SizeX, SizeY, Format, NumMips, Flags );
		}
#endif
	}
	else
	{
		// create the 2d texture resource
		IDirect3DTexture9* D3DResource = new IDirect3DTexture9;
		DWORD MemorySize = 0;
		void* BaseAddress = FXeTexture2D::AllocTexture2D(SizeX, SizeY, Format, NumMips, Flags, D3DResource, MemorySize, BulkData);
		if ( GTexturePool.IsValidTextureData(BaseAddress) || (Flags & TexCreate_AllowFailure) == 0 )
		{
			XeTexture = new FXeTexture2D(D3DResource, Format, Flags, MemorySize);
			XeTexture->BaseAddress = BaseAddress;
#if TRACK_GPU_RESOURCES
			XeTexture->PhysicalSize = MemorySize;
			XeTexture->VirtualSize = sizeof(IDirect3DTexture9);
#endif
			GTexturePool.RegisterTexture( XeTexture );
		}
		else
		{
			delete D3DResource;
			XeTexture = NULL;
		}
	}
	return FTexture2DRHIRef(XeTexture);
}


/*-----------------------------------------------------------------------------
	Array texture support.
-----------------------------------------------------------------------------*/

/**
* Creates a Array RHI texture resource
* @param SizeX		- width of the texture to create
* @param SizeY		- height of the texture to create
* @param SizeZ		- depth of the texture to create
* @param Format	- EPixelFormat texture format
* @param NumMips	- number of mips to generate or 0 for full mip pyramid
* @param Flags		- ETextureCreateFlags creation flags
*/
FTexture2DArrayRHIRef RHICreateTexture2DArray(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE InFormat,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData)
{
	EPixelFormat Format = (EPixelFormat) InFormat;
	FXeTextureArray* XeTexture = NULL;

	if( (Flags&TexCreate_ResolveTargetable) || (Flags & TexCreate_Uncooked) )
	{
		check(!BulkData);
		check( !((Flags&TexCreate_ResolveTargetable) && NumMips > 1) );

		IDirect3DArrayTexture9* pD3DTextureArray;
		// create texture in memory for resolving surface
		HRESULT D3DResult = GDirect3DDevice->CreateArrayTexture( 
			SizeX, 
			SizeY, 
			SizeZ, 
			NumMips, 
			0, 
			XeGetTextureFormat(Format,Flags), 
			0, 
			&pD3DTextureArray,
			NULL );

		if ( Flags & TexCreate_AllowFailure )
		{
			XeTexture = (D3DResult == S_OK) ? new FXeTextureArray(pD3DTextureArray,Format,Flags) : NULL;
		}
		else
		{
			VERIFYD3DRESULT( D3DResult );
			XeTexture = new FXeTextureArray(pD3DTextureArray,Format,Flags);
		}
#if TRACK_GPU_RESOURCES
		if (XeTexture)
		{
			XeTexture->VirtualSize = sizeof(D3DTexture);
			// Create a dummy texture we can query the physical size from
			XeTexture->PhysicalSize = XGSetArrayTextureHeaderEx( 
				SizeX,							// Width
				SizeY,							// Height
				SizeZ,							// Depth
				NumMips,						// Levels
				0,								// Usage
				XeGetTextureFormat(Format,Flags),		// Format
				0,								// ExpBias
				0,								// Flags
				0,								// BaseOffset
				XGHEADER_CONTIGUOUS_MIP_OFFSET,	// MipOffset
				0,								// Pitch
				0,								// D3D texture
				NULL,							// unused
				NULL							// unused
				);
		}
#endif
	}
	else
	{
		// not supported for Array textures
		check(0);
	}
	return FTexture2DArrayRHIRef(XeTexture);
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
	FTexture2DRHIRef Texture2DRef( FXeTexture2D::ReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY) );
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
	if(!TextureRHI)
	{
		return 0;
	}

	FXeTexture2D* Texture = TextureRHI;

	// for textures that reside in the texture pool. 0 otherwise.
	return Texture->GetResourceSize();
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
	FTexture2DRHIRef Texture2DRef( FXeTexture2D::AsyncReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus) );
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
	return FXeTexture2D::FinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

/**
 * Cancels an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If TRUE, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus RHICancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted )
{
	return FXeTexture2D::CancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
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
	Texture->Lock( MipIndex );
	void* Result=NULL;
	IDirect3DTexture9* D3DTexture2D = (IDirect3DTexture9*) Texture->Resource;
	DWORD LockFlags = 0;

//@todo xenon - triggers D3D error due to 64k page allocs
#ifndef _DEBUG
	if( !bIsDataBeingWrittenTo )
	{
		LockFlags |= D3DLOCK_READONLY;
	}
#endif

	UBOOL bLockMipTail = FALSE;
	if( !(Texture->GetCreationFlags()&TexCreate_ResolveTargetable) &&
		!(Texture->GetCreationFlags()&TexCreate_Uncooked) &&
		!(Texture->GetCreationFlags()&TexCreate_NoMipTail) &&
		!bLockWithinMiptail )
	{
		XGMIPTAIL_DESC MipTailDesc;
		XGGetMipTailDesc(D3DTexture2D, &MipTailDesc);
		if( MipIndex == MipTailDesc.BaseLevel )
		{
			bLockMipTail = TRUE;
		}
	}	

	if( bLockMipTail )
	{
		// Lock and fill the tail
		D3DLOCKED_TAIL LockedTail;
		D3DTexture2D->LockTail(&LockedTail, LockFlags);
		DestStride = LockedTail.RowPitch;
		Result = LockedTail.pBits;
	}
	else
	{
		// Lock and fill the mip level
		D3DLOCKED_RECT LockedRect;
		D3DTexture2D->LockRect(MipIndex, &LockedRect, NULL, LockFlags);
		DestStride = LockedRect.Pitch;
		Result = LockedRect.pBits;
	}

	return Result;
}

/**
* Unlocks a previously locked RHI texture resource
* @param Texture - the RHI texture resource to unlock
* @param MipIndex - mip level index for the surface to unlock
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
*/
void RHIUnlockTexture2D(FTexture2DRHIParamRef Texture,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	IDirect3DTexture9* D3DTexture2D = (IDirect3DTexture9*) Texture->Resource;
	if ( D3DTexture2D )
	{
		UBOOL bLockMipTail = FALSE;
		if( !(Texture->GetCreationFlags()&TexCreate_ResolveTargetable) &&
			!(Texture->GetCreationFlags()&TexCreate_Uncooked) &&
			!(Texture->GetCreationFlags()&TexCreate_NoMipTail) &&
			!bLockWithinMiptail )
		{
			XGMIPTAIL_DESC MipTailDesc;
			XGGetMipTailDesc(D3DTexture2D, &MipTailDesc);
			if( MipIndex == MipTailDesc.BaseLevel )
			{
				bLockMipTail = TRUE;
			}
		}

		if( bLockMipTail )
		{
			// Unlock the tail
			D3DTexture2D->UnlockTail();
		}
		else
		{
			// Unlock the mip level
			D3DTexture2D->UnlockRect(MipIndex);		
		}
	}
	Texture->Unlock( MipIndex );
}

/**
 * Checks if a texture is still in use by the GPU.
 * @param Texture - the RHI texture resource to check
 * @param MipIndex - Which mipmap we're interested in
 * @return TRUE if the texture is still in use by the GPU, otherwise FALSE
 */
UBOOL RHIIsBusyTexture2D(FTexture2DRHIParamRef Texture, UINT MipIndex)
{
	check( Texture );
	IDirect3DTexture9* D3DTexture2D = (IDirect3DTexture9*) Texture->Resource;
	return D3DTexture2D && (D3DTexture2D->IsSet(GDirect3DDevice) || D3DTexture2D->IsBusy());
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
	IDirect3DTexture9* D3DTexture2D = (IDirect3DTexture9*) Texture->Resource;

    const DWORD LockFlags = D3DLOCK_NOSYSLOCK;

	UnsetPSTextures();
	UnsetVSTextures();

    for (UINT i = 0; i < n; i++)
    {
        const FUpdateTextureRegion2D& rect = rects[i];
        D3DLOCKED_RECT	LockedRect;
        RECT            destr;

        destr.left	 = rect.DestX;
        destr.bottom = rect.DestY + rect.Height;
        destr.right  = rect.DestX + rect.Width;
        destr.top    = rect.DestY;

        VERIFYD3DRESULT(D3DTexture2D->LockRect(MipIndex,&LockedRect,&destr,LockFlags));

        for (int j = 0; j < rect.Height; j++)
		{
            appMemcpy(((BYTE*)LockedRect.pBits) + j * LockedRect.Pitch,
            psrc + pitch * (j + rect.SrcY) + sbpp * rect.SrcX,
            rect.Width * sbpp);
		}

        VERIFYD3DRESULT(D3DTexture2D->UnlockRect(MipIndex));
    }

    return TRUE;
}

/**
 * For platforms that support packed miptails return the first mip level which is packed in the mip tail
 * @return mip level for mip tail or -1 if mip tails are not used
 */
INT RHIGetMipTailIdx(FTexture2DRHIParamRef Texture)
{
	check( Texture->Resource );
	XGMIPTAIL_DESC MipTailDesc;
	XGGetMipTailDesc( (IDirect3DTexture9*)Texture->Resource, &MipTailDesc );
	return MipTailDesc.BaseLevel;
}

/**
* Copies a region within the same mip levels of one texture to another.  An optional region can be speci
* Note that the textures must be the same size and of the same format.
*
* @param DstTexture - destination texture resource to copy to
* @param MipIdx - mip level for the surface to copy from/to. This mip level should be valid for both source/destination textures
* @param BaseSizeX - width of the texture (base level). Same for both source/destination textures
* @param BaseSizeY - height of the texture (base level). Same for both source/destination textures 
* @param Format - format of the texture. Same for both source/destination textures
* @param Region - list of regions to specify rects and source textures for the copy
*/
void RHICopyTexture2D(FTexture2DRHIParamRef DstTexture, UINT MipIdx, INT BaseSizeX, INT BaseSizeY, INT Format, const TArray<FCopyTextureRegion2D>& Regions)
{
	check( DstTexture->Resource );
	check( !(DstTexture->GetCreationFlags()&TexCreate_ResolveTargetable) );	
	UBOOL bPackedMipTail = (DstTexture->GetCreationFlags()&TexCreate_NoMipTail) ? FALSE : TRUE;

	// scale the base SizeX,SizeY for the current mip level
	INT MipSizeX = Max((INT)GPixelFormats[Format].BlockSizeX,BaseSizeX >> MipIdx);
	INT MipSizeY = Max((INT)GPixelFormats[Format].BlockSizeY,BaseSizeY >> MipIdx);

	// lock the destination texture
	UINT DstStride;
	BYTE* DstData = (BYTE*)RHILockTexture2D( DstTexture, MipIdx, TRUE, DstStride, FALSE );	

	// get description of destination texture
	XGTEXTURE_DESC DstDesc;
	XGGetTextureDesc( (IDirect3DTexture9*)DstTexture->Resource, MipIdx, &DstDesc );

	// get the offset of the current mip into the miptail for the destination texture. 0 if no miptail
	UINT DstMipTailOffset = XGGetMipTailLevelOffset( 
		BaseSizeX, 
		BaseSizeY, 
		1, 
		MipIdx, 
		XGGetGpuFormat((D3DFORMAT)MAKELINFMT(DstDesc.Format)),
		XGIsTiledFormat(DstDesc.Format),
		false
		);
	// default the non tiled version to the same offset
	INT DstNonTiledMipTailOffset = DstMipTailOffset;

	// ptr to untiled result data
	BYTE* DstDataNonTiled = DstData;
	// temp texture to store untiled result
	FTexture2DRHIRef TempTextureNonTiled;
	// if the destination texture is tiled then need to use a temp texture to store the untiled result
	if( XGIsTiledFormat(DstDesc.Format) )
	{
		DWORD TempFlags = DstTexture->GetCreationFlags()|TexCreate_NoTiling;
		if( !bPackedMipTail )
		{
			TempFlags |= TexCreate_NoMipTail;
		}
		// create a temp texture to store the untiled result
		TempTextureNonTiled = RHICreateTexture2D(BaseSizeX,BaseSizeY,Format,0,TempFlags,NULL);
		// lock this texture and use its data ptr as the untiled result
		UINT TempStride=0;
		DstDataNonTiled = (BYTE*)RHILockTexture2D(
			TempTextureNonTiled,
			MipIdx,
			TRUE,
			TempStride,
			FALSE
			);
		// get temp texture description
		XGTEXTURE_DESC TempDesc;
		XGGetTextureDesc( (IDirect3DTexture9*)TempTextureNonTiled->Resource, MipIdx, &TempDesc );
		// get the offset of the current mip into the miptail for this temp texture. 0 if no miptail
		DstNonTiledMipTailOffset = XGGetMipTailLevelOffset( 
			BaseSizeX, 
			BaseSizeY, 
			1, 
			MipIdx, 
			XGGetGpuFormat((D3DFORMAT)MAKELINFMT(TempDesc.Format)),
			false,
			false
			);	
	}

	for( INT RegionIdx=0; RegionIdx < Regions.Num(); RegionIdx++ )		
	{
		const FCopyTextureRegion2D& Region = Regions(RegionIdx);
		check( Region.SrcTexture->Resource );		

		// lock source RHI texture
		UINT SrcStride=0;
		BYTE* SrcData = (BYTE*)RHILockTexture2D( 
			Region.SrcTexture,
			// it is possible for the source texture to have > mips than the dest so start at the FirstMipIdx
			Region.FirstMipIdx + MipIdx,
			FALSE,
			SrcStride,
			FALSE
			);
		
		// get description of source texture
		XGTEXTURE_DESC SrcDesc;
		XGGetTextureDesc( (IDirect3DTexture9*)Region.SrcTexture->Resource, Region.FirstMipIdx + MipIdx, &SrcDesc );

		// calc the offset from the miptail base level for this mip level. If no miptail then it is just 0
		UINT SrcMipTailOffset = XGGetMipTailLevelOffset( 
			BaseSizeX, 
			BaseSizeY, 
			1, 
			MipIdx, 
			XGGetGpuFormat((D3DFORMAT)MAKELINFMT(SrcDesc.Format)),
			true,
			false
			);

		// align/truncate the region offset to block size
		INT RegionOffsetX = (Clamp( Region.OffsetX, 0, MipSizeX - GPixelFormats[Format].BlockSizeX ) / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockSizeX;
		INT RegionOffsetY = (Clamp( Region.OffsetY, 0, MipSizeY - GPixelFormats[Format].BlockSizeY ) / GPixelFormats[Format].BlockSizeY) * GPixelFormats[Format].BlockSizeY;
		// scale region size to the current mip level. Size is aligned to the block size
		check(Region.SizeX != 0 && Region.SizeY != 0);
		INT RegionSizeX = Clamp( Align( Region.SizeX, GPixelFormats[Format].BlockSizeX), 0, MipSizeX );
		INT RegionSizeY = Clamp( Align( Region.SizeY, GPixelFormats[Format].BlockSizeY), 0, MipSizeY );
		// handle special case for full copy
		if( Region.SizeX == -1 || Region.SizeY == -1 )
		{
			RegionSizeX = MipSizeX;
			RegionSizeY = MipSizeY;
		}

		// copying from tiled to untiled
		if( XGIsTiledFormat(SrcDesc.Format) )
		{
			// destination point to copy to
			POINT Point = 
			{
				RegionOffsetX / GPixelFormats[Format].BlockSizeX,
				RegionOffsetY / GPixelFormats[Format].BlockSizeY
			};
			// source rect to copy from			
			RECT Rect;
			SetRect(
				&Rect,
				RegionOffsetX / GPixelFormats[Format].BlockSizeX,
				RegionOffsetY / GPixelFormats[Format].BlockSizeY,
				RegionOffsetX / GPixelFormats[Format].BlockSizeX + RegionSizeX / GPixelFormats[Format].BlockSizeX,
				RegionOffsetY / GPixelFormats[Format].BlockSizeY + RegionSizeY / GPixelFormats[Format].BlockSizeY 
				);

			// untile from source texture into the destination data
			XGUntileSurface(
				DstDataNonTiled + DstNonTiledMipTailOffset,
				DstDesc.RowPitch,
				&Point,
				SrcData + SrcMipTailOffset,
				SrcDesc.WidthInBlocks,
				SrcDesc.HeightInBlocks,
				&Rect,
				SrcDesc.BytesPerBlock
				);
		}
		// copying from untiled to untiled
		else
		{
			// size in bytes of an entire row for this mip
			DWORD PitchBytes = (MipSizeX / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes;
			// size in bytes of the offset to the starting part of the row to copy for this mip
			DWORD RowOffsetBytes = (RegionOffsetX / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes;
			// size in bytes of the amount to copy within each row
			DWORD RowSizeBytes = (RegionSizeX / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes;

			// copy each region row in increments of the block size
			for( INT CurOffsetY=RegionOffsetY; CurOffsetY < (RegionOffsetY+RegionSizeY); CurOffsetY += GPixelFormats[Format].BlockSizeY )
			{
				INT CurBlockOffsetY = CurOffsetY / GPixelFormats[Format].BlockSizeY;

				BYTE* SrcOffset = SrcData + SrcMipTailOffset + (CurBlockOffsetY * PitchBytes) + RowOffsetBytes;
				BYTE* DstOffset = DstDataNonTiled + DstNonTiledMipTailOffset + (CurBlockOffsetY * PitchBytes) + RowOffsetBytes;
				appMemcpy( DstOffset, SrcOffset, RowSizeBytes );
			}
		}

		// done reading from source mip so unlock it
		RHIUnlockTexture2D( Region.SrcTexture, Region.FirstMipIdx + MipIdx, FALSE );
	}

	// copy from the untiled temp result to the tiled result
	if( DstDataNonTiled != DstData )
	{
		check( IsValidRef(TempTextureNonTiled) );
		check( XGIsTiledFormat(DstDesc.Format) );

		XGTileSurface(
			DstData + DstMipTailOffset,
			DstDesc.WidthInBlocks,
			DstDesc.HeightInBlocks,
			NULL,
			DstDataNonTiled + DstNonTiledMipTailOffset,
			DstDesc.RowPitch,
			NULL,
			DstDesc.BytesPerBlock
			);
		
		RHIUnlockTexture2D( TempTextureNonTiled, MipIdx, FALSE );
	}

	// unlock the destination texture
	RHIUnlockTexture2D( DstTexture, MipIdx, FALSE );	
}

/**
 * Copies mip data from one location to another, selectively copying only used memory based on
 * the texture tiling memory layout of the given mip level
 * Note that the mips must be the same size and of the same format.
 * @param Texture - texture to base memory layout on
 * @param Src - source memory base address to copy from
 * @param Dst - destination memory base address to copy to
 * @param MemSize - total size of mip memory
 * @param MipIdx - mip index to base memory layout on
 */
void RHISelectiveCopyMipData(FTexture2DRHIParamRef Texture, BYTE *Src, BYTE *Dst, UINT MemSize, UINT MipIdx)
{
	// default: just copy the whole memory section
	appMemcpy(Dst, Src, MemSize);
}

/**
 * Copies texture data from one mip to another
 * Note that the mips must be the same size and of the same format.
 * @param SrcText Source texture to copy from
 * @param SrcMipIndex Mip index into the source texture to copy data from
 * @param DestText Destination texture to copy to
 * @param DestMipIndex Mip index in the destination texture to copy to - note this is probably different from source mip index if the base widths/heights are different
 * @param Size Size of mip data
 * @param Counter Thread safe counter used to flag end of transfer
 */
void RHICopyMipToMipAsync(FTexture2DRHIParamRef SrcTexture, INT SrcMipIndex, FTexture2DRHIParamRef DestTexture, INT DestMipIndex, INT Size, FThreadSafeCounter& Counter)
{
	// Lock old and new texture.
	UINT SrcStride;
	UINT DestStride;
	BYTE* Src = (BYTE*) RHILockTexture2D( SrcTexture, SrcMipIndex, FALSE, SrcStride, FALSE );
	BYTE* Dst = (BYTE*) RHILockTexture2D( DestTexture, DestMipIndex, TRUE, DestStride, FALSE );
	GGPUMemMove.BeginMemMove();
	GGPUMemMove.MemMove( Dst, Src, Size );
	GGPUMemMove.EndMemMove();
	SrcTexture->LockGPUFence();
	DestTexture->LockGPUFence();
}

void RHIFinalizeAsyncMipCopy(FTexture2DRHIParamRef SrcTexture, INT SrcMipIndex, FTexture2DRHIParamRef DestTexture, INT DestMipIndex)
{
	SrcTexture->UnlockGPUFence();
	DestTexture->UnlockGPUFence();
	RHIUnlockTexture2D( SrcTexture, SrcMipIndex, FALSE );
	RHIUnlockTexture2D( DestTexture, DestMipIndex, FALSE );
}

/*-----------------------------------------------------------------------------
Cubemap texture support.
-----------------------------------------------------------------------------*/

/** 
* Sets D3D header and allocates memory for the resource 
*
* @param	Size - width of texture
* @param	Format - format of texture
* @param	NumMips - number of mips
* @param	Flags - creation flags
*/
void FXeTextureCube::AllocTextureCube(UINT Size,EPixelFormat Format,UINT NumMips,DWORD Flags)
{
	// set the texture header for the driver	
	DWORD TextureSize = XGSetCubeTextureHeaderEx( 
		Size,
		NumMips, 
		0,
		XeGetTextureFormat(Format,Flags),
		0,
		(Flags&TexCreate_NoMipTail) ? XGHEADEREX_NONPACKED : 0,
		0,
		XGHEADER_CONTIGUOUS_MIP_OFFSET,
		(IDirect3DCubeTexture9*)Resource,
		NULL,
		NULL 
		);
	DWORD AlignedSize = Align(TextureSize,D3DTEXTURE_ALIGNMENT);
#if TRACK_GPU_RESOURCES
	PhysicalSize = AlignedSize;
	VirtualSize	 = sizeof(IDirect3DCubeTexture9);
#endif
	// allocate memory for texture data
	BaseAddress	= GTexturePool.Allocate( AlignedSize, (Flags & TexCreate_AllowFailure) ? TRUE : FALSE, (Flags & TexCreate_DisableAutoDefrag) ? TRUE : FALSE );
	check(BaseAddress == Align(BaseAddress, D3DTEXTURE_ALIGNMENT));

	GTexturePool.RegisterTexture( this );

	// set data address for this resource
	XGOffsetResourceAddress( Resource, BaseAddress );
	ResourceSize = AlignedSize;
}

/**
* Creates a Cube RHI texture resource
* @param Size - width/height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
FTextureCubeRHIRef RHICreateTextureCube( UINT Size, BYTE InFormat, UINT NumMips, DWORD Flags, FResourceBulkDataInterface* BulkData )
{
	EPixelFormat Format = (EPixelFormat) InFormat;
	if( (Flags&TexCreate_ResolveTargetable) || (Flags & TexCreate_Uncooked) )
	{
		check( !((Flags&TexCreate_ResolveTargetable) && NumMips > 1) );

		IDirect3DCubeTexture9* pD3DTextureCube;		
		// create texture in memory for resolving surface
		VERIFYD3DRESULT( GDirect3DDevice->CreateCubeTexture( 
			Size,
			NumMips, 
			0, 
			XeGetTextureFormat(Format,Flags), 
			D3DPOOL_DEFAULT, 
			&pD3DTextureCube, 
			NULL ));

		FTextureCubeRHIRef TextureCube(new FXeTextureCube(pD3DTextureCube,Format,Flags));
		return TextureCube;
	}
	else
	{
		// create the new cube texture resource
		FTextureCubeRHIRef TextureCube(new FXeTextureCube(Format,Flags));
		// manually allocate resource and set header
		TextureCube->AllocTextureCube(Size,Format,NumMips,Flags);

		return TextureCube;
	}
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
void* RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCube,UINT FaceIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail)
{
	TextureCube->Lock( MipIndex );
	void* Result=NULL;
	IDirect3DCubeTexture9* D3DTextureCube = (IDirect3DCubeTexture9*) TextureCube->Resource;

	DWORD LockFlags = 0;

//@todo xenon - triggers D3D error due to 64k page allocs
#ifndef _DEBUG
	if( !bIsDataBeingWrittenTo )
	{
		LockFlags |= D3DLOCK_READONLY;
	}
#endif

	UBOOL bLockMipTail = FALSE;
	if( !(TextureCube->GetCreationFlags()&TexCreate_ResolveTargetable) &&
		!(TextureCube->GetCreationFlags()&TexCreate_Uncooked) &&
		!(TextureCube->GetCreationFlags()&TexCreate_NoMipTail) &&
		!bLockWithinMiptail )
	{
		XGMIPTAIL_DESC MipTailDesc;
		XGGetMipTailDesc(D3DTextureCube, &MipTailDesc);
		if( MipIndex == MipTailDesc.BaseLevel )
		{
			bLockMipTail = TRUE;
		}
	}

	if( bLockMipTail )
	{
		// Lock and fill the tail
		D3DLOCKED_TAIL LockedTail;
		D3DTextureCube->LockTail((D3DCUBEMAP_FACES) FaceIndex, &LockedTail, LockFlags);
		DestStride = LockedTail.RowPitch;
		Result = LockedTail.pBits;
	}
	else
	{
		// Lock and fill the mip level
		D3DLOCKED_RECT LockedRect;
		D3DTextureCube->LockRect((D3DCUBEMAP_FACES) FaceIndex, MipIndex, &LockedRect, NULL, LockFlags);
		DestStride = LockedRect.Pitch;
		Result = LockedRect.pBits;
	}

	return Result;
}

/**
* Unlocks a previously locked RHI texture resource
* @param Texture - the RHI texture resource to unlock
* @param MipIndex - mip level index for the surface to unlock
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
*/
void RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCube,UINT FaceIndex,UINT MipIndex,UBOOL bLockWithinMiptail)
{
	IDirect3DCubeTexture9* D3DTextureCube = (IDirect3DCubeTexture9*) TextureCube->Resource;

	UBOOL bLockMipTail = FALSE;
	if( !(TextureCube->GetCreationFlags()&TexCreate_ResolveTargetable) &&
		!(TextureCube->GetCreationFlags()&TexCreate_Uncooked) &&
		!(TextureCube->GetCreationFlags()&TexCreate_NoMipTail) &&
		!bLockWithinMiptail )
	{
		XGMIPTAIL_DESC MipTailDesc;
		XGGetMipTailDesc(D3DTextureCube, &MipTailDesc);
		if( MipIndex == MipTailDesc.BaseLevel )
		{
			bLockMipTail = TRUE;
		}
	}

	if( bLockMipTail )
	{
		// Unlock the tail
		D3DTextureCube->UnlockTail((D3DCUBEMAP_FACES) FaceIndex);
	}
	else
	{
		// Unlock the mip level
		D3DTextureCube->UnlockRect((D3DCUBEMAP_FACES) FaceIndex, MipIndex);		
	}
	TextureCube->Unlock( MipIndex );
}

/*-----------------------------------------------------------------------------
Shared texture support.
-----------------------------------------------------------------------------*/

/**
* Computes the device-dependent amount of memory required by a texture.  This size may be passed to RHICreateSharedMemory.
* @param SizeX - The width of the texture.
* @param SizeY - The height of the texture.
* @param SizeZ - The depth of the texture.
* @param Format - The format of the texture.
* @return The amount of memory in bytes required by a texture with the given properties.
*/
SIZE_T XeCalculateTextureBytes(DWORD SizeX,DWORD SizeY,DWORD SizeZ,BYTE Format)
{
	const DWORD TiledSizeX = Align(SizeX,32);
	const DWORD TiledSizeY = Align(SizeY,32);
	const UBOOL bIsTiled = (XeGetTextureFormat(Format,0) & D3DFORMAT_TILED_MASK) != 0;
	return Align(
			CalculateImageBytes(
				bIsTiled ? TiledSizeX : SizeX,
				bIsTiled ? TiledSizeY : SizeY,
				SizeZ,
				Format
				),
			D3DTEXTURE_ALIGNMENT
			);
}

/**
* Create resource memory to be shared by multiple RHI resources
* @param Size - aligned size of allocation
* @return shared memory resource RHI ref
*/
FSharedMemoryResourceRHIRef RHICreateSharedMemory(SIZE_T Size)
{
	// create the shared memory resource
	FSharedMemoryResourceRHIRef SharedMemory(new FXeSharedMemoryResource(Size));
	return SharedMemory;
}

/**
 * Creates a RHI texture and if the platform supports it overlaps it in memory with another texture
 * Note that modifying this texture will modify the memory of the overlapped texture as well
 * @param SizeX - The width of the surface to create.
 * @param SizeY - The height of the surface to create.
 * @param Format - The surface format to create.
 * @param ResolveTargetTexture - The 2d texture to use the memory from if the platform allows
 * @param Flags - Surface creation flags
 * @return The surface that was created.
 */
FSharedTexture2DRHIRef RHICreateSharedTexture2D(UINT SizeX,UINT SizeY,BYTE InFormat,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemory,DWORD Flags)
{
	EPixelFormat Format = (EPixelFormat) InFormat;
	// create the D3D resource
	IDirect3DTexture9* Resource = new IDirect3DTexture9;

	// set the texture header
	const DWORD AlignedSize = XeSetTextureHeader( SizeX, SizeY, Format, NumMips, Flags & ~TexCreate_NoMipTail, Resource );
	check( AlignedSize <= SharedMemory->Size );

	if ( AlignedSize > SharedMemory->Size )
	{
		DebugBreak();
	}

	// setup the shared memory
	XGOffsetResourceAddress( Resource, SharedMemory->BaseAddress );

	// keep track of the fact that the D3D resource is using the shared memory
	SharedMemory->AddResource(Resource);

	// create the 2d texture resource
	FSharedTexture2DRHIRef SharedTexture2D(new FXeSharedTexture2D(Resource, Format, SharedMemory));

#if TRACK_GPU_RESOURCES
	SharedTexture2D->VirtualSize = sizeof(D3DTexture);
	SharedTexture2D->PhysicalSize = SharedMemory->Size;
#endif

	return SharedTexture2D;
}


/*-----------------------------------------------------------------------------
	FXeTexture2DResourceMem
-----------------------------------------------------------------------------*/

/** 
 * Init Constructor 
 * Allocates texture memory during construction
 *
 * @param InSizeX			- width of texture
 * @param InSizeY			- height of texture
 * @param InNumMips			- total number of mips to allocate memory for
 * @param InFormat			- EPixelFormat texture format
 * @param InTexCreateFlags	- ETextureCreateFlags bit flags
 * @param AsyncCounter		- If provided, starts an async allocation. If NULL, allocates memory immediately.
 */
FXeTexture2DResourceMem::FXeTexture2DResourceMem(INT InSizeX, INT InSizeY, INT InNumMips, EPixelFormat InFormat, DWORD InTexCreateFlags, FThreadSafeCounter* AsyncCounter)
:	SizeX(InSizeX)
,	SizeY(InSizeY)
,	NumMips(InNumMips)
,	Format(InFormat)
,	BaseAddress(NULL)
,	TextureSize(0)
,	TexCreateFlags(InTexCreateFlags)
,	ReallocationRequest(NULL)
{
	// Allow failure the first attempt.
	TexCreateFlags |= TexCreate_AllowFailure;

	TextureSize = XeSetTextureHeader( SizeX, SizeY, Format, NumMips, TexCreateFlags, &TextureHeader );

	// First try to allocate immediately.
	AllocateTextureMemory( FALSE );

	// Did the allocation fail and the user requested async allocation?
	if ( AsyncCounter && !IsValid() )
	{
		ReallocationRequest = new FAsyncReallocationRequest(NULL, TextureSize, AsyncCounter);
		UBOOL bSuccess = GTexturePool.AsyncReallocate( ReallocationRequest, TRUE );
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
FXeTexture2DResourceMem::~FXeTexture2DResourceMem()
{
	CancelAsyncAllocation();

	// mip data gets deleted by a texture resource destructor once this memory is used for RHI resource creation
	// but, if it was never used then free the memory here
	if( BaseAddress != NULL )
	{
		FreeTextureMemory();
	}
}

/** 
 * @return ptr to the resource memory which has been preallocated
 */
void* FXeTexture2DResourceMem::GetResourceBulkData() const
{
	return BaseAddress;
}

/** 
 * @return size of resource memory
 */
DWORD FXeTexture2DResourceMem::GetResourceBulkDataSize() const
{
	return TextureSize;
}

/**
 * Free memory after it has been used to initialize RHI resource 
 */
void FXeTexture2DResourceMem::Discard()
{
	CancelAsyncAllocation();

	// no longer maintain a pointer to mip data since it is owned by RHI resource after creation	
	BaseAddress = NULL;
	TextureSize = 0;
}

/**
 * @param MipIndex index for mip to retrieve
 * @return ptr to the offset in bulk memory for the given mip
 */
void* FXeTexture2DResourceMem::GetMipData(INT MipIndex)
{
	UINT MipAddress;
	XGGetTextureLayout(&TextureHeader,NULL,NULL,NULL,NULL,0,&MipAddress,NULL,NULL,NULL,0);
	DWORD Offset = XGGetMipLevelOffset(&TextureHeader, 0, MipIndex);
	DWORD MipTailIndex = XGGetMipTailBaseLevel(SizeX, SizeY, FALSE);
	void* MipData;

	// Is this the base level?
	if ( MipIndex == 0 || MipAddress == 0 )
	{
		// Is the base level a packed mipmap tail?
		if ( !(TexCreateFlags&TexCreate_NoMipTail) && MipIndex == MipTailIndex )
		{
			Offset = 0;
		}
		MipData = (BYTE*)BaseAddress + Offset;
	}
	// Is this a packed mip tail?
	else if ( !(TexCreateFlags&TexCreate_NoMipTail) && MipIndex == MipTailIndex )
	{
		// Get the address of the previous mip-level.
		BYTE* PrevMipAddress;
		if ( MipIndex == 1 )
		{
			PrevMipAddress = (BYTE*)BaseAddress;
		}
		else
		{
			Offset = XGGetMipLevelOffset(&TextureHeader, 0, MipIndex - 1);
			PrevMipAddress = (BYTE*)MipAddress + Offset;
		}

		// Add its mip-size to reach the requested mip-level.
		XGTEXTURE_DESC Desc;
		XGGetTextureDesc(&TextureHeader, MipIndex - 1, &Desc);
		MipData = PrevMipAddress + Desc.SlicePitch;
	}
	// Is this a regular mip-level?
	else
	{
		MipData = (BYTE*)MipAddress + Offset;
	}

	return MipData;
}

/**
 * @return total number of mips stored in this resource
 */
INT	FXeTexture2DResourceMem::GetNumMips()
{
	return NumMips;
}

/** 
 * @return width of texture stored in this resource
 */
INT FXeTexture2DResourceMem::GetSizeX()
{
	return SizeX;
}

/** 
 * @return height of texture stored in this resource
 */
INT FXeTexture2DResourceMem::GetSizeY()
{
	return SizeY;
}

/**
 * Blocks the calling thread until the allocation has been completed.
 */
void FXeTexture2DResourceMem::FinishAsyncAllocation()
{
	if ( ReallocationRequest )
	{
		if ( ReallocationRequest->HasStarted() )
		{
			check( BaseAddress == NULL );
			GTexturePool.BlockOnAsyncReallocation( ReallocationRequest );
			check( ReallocationRequest->HasCompleted() );
			BaseAddress = ReallocationRequest->GetNewBaseAddress();
			XGOffsetResourceAddress( &TextureHeader, BaseAddress );

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
void FXeTexture2DResourceMem::CancelAsyncAllocation()
{
	if ( ReallocationRequest )
	{
		// Note: This also updates the external async counter.
		GTexturePool.CancelAsyncReallocation( ReallocationRequest, NULL );
		delete ReallocationRequest;
		ReallocationRequest = NULL;
	}
}

/** 
 * Calculate size needed to store this resource and allocate texture memory for it
 *
 * @param bAllowRetry	If TRUE, retry the allocation after panic-stream-out/panic-defrag upon failure
 */
void FXeTexture2DResourceMem::AllocateTextureMemory( UBOOL bAllowRetry )
{
	check( IsValid() == FALSE );

	// Try one time.
	BaseAddress = FXeTexture2D::AllocTexture2D(SizeX, SizeY, Format, NumMips, TexCreateFlags | TexCreate_AllowFailure, &TextureHeader, TextureSize, NULL);

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
			TexCreateFlags &= ~TexCreate_AllowFailure;
		}

		BaseAddress = FXeTexture2D::AllocTexture2D(SizeX, SizeY, Format, NumMips, TexCreateFlags, &TextureHeader, TextureSize, NULL);

		NumRetries--;
	}
}

/**
 * Free the memory from the texture pool
 */
void FXeTexture2DResourceMem::FreeTextureMemory()
{
	if ( GTexturePool.IsValidTextureData(BaseAddress) )
	{
		// add this texture resource to the list of resources to be deleted later
		// this will free the memory from that was allocated from the texture pool
		AddUnusedXeResource(NULL, BaseAddress, TRUE, FALSE);
	}
	BaseAddress = NULL;
}


#endif
