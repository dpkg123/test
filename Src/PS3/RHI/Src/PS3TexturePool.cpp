/*=============================================================================
	PS3TexturePool.cpp: PS3 texture pool definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3RHIPrivate.h"

#if USE_PS3_RHI


/*-----------------------------------------------------------------------------
	GPU Memory Move
-----------------------------------------------------------------------------*/

/**
 * Performs a memcpy within local memory using the RSX.
 * Does not handle overlapping memory, but all transfers are properly ordered so the caller
 * can split up an overlapping transfer into a sequence of non-overlapping memcpys.
 * Taken from PS3 SDK sample code: ...\target\ppu\src\PSGL\jetstream\src\JSGCM\GmmAlloc.cpp
 *
 * @param DstOffset		RSX IO offset of the destination start address
 * @param SrcOffset		RSX IO offset of the source start address
 * @param NumBytes		Number of bytes to transfer
 */
static void RSXLocalMemCpy( const DWORD DstOffset, const DWORD SrcOffset, const DWORD NumBytes )
{
	int32_t Offset = 0;
	int32_t SizeLeft = NumBytes;
	int32_t Dimension = 4096;

	while (SizeLeft > 0)
	{
		while(SizeLeft >= Dimension * Dimension * 4)
		{
			check((DstOffset + Offset) % 64 == 0);
			check((SrcOffset + Offset) % 64 == 0);

			cellGcmSetTransferImage(
				CELL_GCM_TRANSFER_LOCAL_TO_LOCAL,
				DstOffset + Offset,
				Dimension * 4,
				0,
				0,
				SrcOffset + Offset,
				Dimension * 4,
				0,
				0,
				Dimension,
				Dimension,
				4);

			Offset += Dimension * Dimension * 4;
			SizeLeft -= Dimension * Dimension * 4;
		}

		Dimension >>= 1;

		// through lots of trial and error, found that
		// for remain portions smaller than 32x32x4, we
		// need break and handle rest with 1 line transfer
		if (Dimension == 32)
		{
			break;
		}
	}

	if (SizeLeft > 0)
	{
		check((DstOffset + Offset) % 64 == 0);
		check((SrcOffset + Offset) % 64 == 0);
		check(SizeLeft % 4 == 0);

		cellGcmSetTransferImage(
			CELL_GCM_TRANSFER_LOCAL_TO_LOCAL,
			DstOffset + Offset,
			SizeLeft,
			0,
			0,
			SrcOffset + Offset,
			SizeLeft,
			0,
			0,
			SizeLeft / 4,
			1,
			4);
	}
}

/**
 * Performs a memmove within local memory using the RSX.
 * Handles overlapping memory, and all transfers are properly ordered.
 *
 * @param DstOffset		RSX IO offset of the destination start address
 * @param SrcOffset		RSX IO offset of the source start address
 * @param NumBytes		Number of bytes to transfer
 */
static void RSXLocalMemMove( const DWORD DstOffset, const DWORD SrcOffset, const DWORD NumBytes )
{
	INT MemDistance = (INT)(DstOffset) - (INT)(SrcOffset);
	INT BytesLeftToMove = NumBytes;

	if (MemDistance > 0)
	{
		// Copying up (dst > src), copy non-overlapping chunks to higher addresses starting at the end
		INT Offset = NumBytes;
		while ( BytesLeftToMove > 0 )
		{
			const INT NumNonOverlappingBytes = Min( BytesLeftToMove, MemDistance );
			Offset -= NumNonOverlappingBytes;
			BytesLeftToMove -= NumNonOverlappingBytes;
			RSXLocalMemCpy( DstOffset + Offset, SrcOffset + Offset, NumNonOverlappingBytes );
		}
	}
	else
	{
		// Copying down (dst < src), copy non-overlapping chunks to lower addresses starting at the front
		MemDistance = -MemDistance;
		INT Offset = 0;
		while ( BytesLeftToMove > 0 )
		{
			const INT NumNonOverlappingBytes = Min( BytesLeftToMove, MemDistance );
			RSXLocalMemCpy( DstOffset + Offset, SrcOffset + Offset, NumNonOverlappingBytes );
			Offset += NumNonOverlappingBytes;
			BytesLeftToMove -= NumNonOverlappingBytes;
		}
	}
}


/*-----------------------------------------------------------------------------
	FPS3TexturePool implementation.
-----------------------------------------------------------------------------*/

/** Enables/disables the texture memory defragmentation feature. */
UBOOL GEnableTextureMemoryDefragmentation = TRUE;

/**
 * Constructor, initializes the FPresizedMemoryPool with already allocated memory
 * 
 * @param PoolMemory				Pointer to the pre-allocated memory pool
 * @param FailedAllocationMemory	Dummy pointer which be returned upon OutOfMemory
 * @param PoolSize					Size of the memory pool
 * @param Alignment					Default alignment for each allocation
 */
FPS3TexturePool::FPS3TexturePool( BYTE* PoolMemory, BYTE* FailedAllocationMemory, INT PoolSize, INT Alignment )
:	FPresizedMemoryPool( PoolMemory, FailedAllocationMemory, PoolSize, Alignment )
,	MemoryLossPointer(NULL)
{
	INT MemoryLoss = 0;
	GConfig->GetInt( TEXT("TextureStreaming"), TEXT("MemoryLoss"), MemoryLoss, GEngineIni );
	if ( MemoryLoss > 0 )
	{
		MemoryLossPointer = Allocate( MemoryLoss*1024*1024, TRUE );
	}

	// Allocator settings
	{
		FPS3TexturePool::FSettings AllocatorSettings;
		GetSettings( AllocatorSettings );
		GConfig->GetBool(TEXT("TextureStreaming"), TEXT("bEnableAsyncDefrag"), AllocatorSettings.bEnableAsyncDefrag, GEngineIni);
		GConfig->GetBool(TEXT("TextureStreaming"), TEXT("bEnableAsyncReallocation"), AllocatorSettings.bEnableAsyncReallocation, GEngineIni);
		if ( GConfig->GetInt(TEXT("TextureStreaming"), TEXT("MaxDefragRelocations"), AllocatorSettings.MaxDefragRelocations, GEngineIni) )
		{
			// Convert from KB to bytes.
			AllocatorSettings.MaxDefragRelocations *= 1024;
		}
		if ( GConfig->GetInt(TEXT("TextureStreaming"), TEXT("MaxDefragDownShift"), AllocatorSettings.MaxDefragDownShift, GEngineIni) )
		{
			// Convert from KB to bytes.
			AllocatorSettings.MaxDefragDownShift *= 1024;
		}

		SetSettings( AllocatorSettings );
	}
}

/**
 * Destructor
 */
FPS3TexturePool::~FPS3TexturePool()
{
}

/**
 * Allocates texture memory.
 *
 * @param	Size			Size of allocation
 * @param	bAllowFailure	Whether to allow OOM silently, because the caller checks for it instead
 * @returns	Pointer to allocated memory
 */
void* FPS3TexturePool::Allocate( DWORD Size, UBOOL bAllowFailure )
{
	void* Pointer = FPresizedMemoryPool::Allocate( Size, GEnableTextureMemoryDefragmentation || bAllowFailure );
	if ( !bAllowFailure && !IsValidTextureData(Pointer) && GEnableTextureMemoryDefragmentation )
	{
		DefragmentTextureMemory();
		Pointer = FPresizedMemoryPool::Allocate( Size, FALSE );
	}

	if ( !bAllowFailure && !IsValidTextureData(Pointer) )
	{
		// Mark texture memory as having been corrupted or not
		extern UBOOL GIsTextureMemoryCorrupted;
		if ( !GIsTextureMemoryCorrupted )
		{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
			INT AllocatedSize, AvailableSize, PendingMemoryAdjustment, LargestAvailableAllocation, NumFreeChunks;
			GetMemoryStats( AllocatedSize, AvailableSize, PendingMemoryAdjustment, LargestAvailableAllocation, NumFreeChunks );
			GLog->Logf( TEXT("RAN OUT OF TEXTURE MEMORY, EXPECT CORRUPTION AND GPU HANGS!") );
			GLog->Logf( TEXT("Tried to allocate %d bytes"), Size );
			GLog->Logf( TEXT("Texture memory available: %.3f MB"), FLOAT(AvailableSize)/1024.0f/1024.0f );
			GLog->Logf( TEXT("Largest available allocation: %.3f MB (%d holes)"), FLOAT(LargestAvailableAllocation)/1024.0f/1024.0f, NumFreeChunks );
#endif
			GIsTextureMemoryCorrupted = TRUE;
		}
	}
	return Pointer;
}

/**
 * Frees texture memory allocated via Allocate
 *
 * @param	Pointer		Allocation to free
 */
void FPS3TexturePool::Free( void* Pointer )
{
	FPresizedMemoryPool::Free( Pointer );
}

/**
 * Tries to reallocate texture memory in-place (without relocating),
 * by adjusting the base address of the allocation but keeping the end address the same.
 *
 * @param	OldBaseAddress	Pointer to the original allocation
 * @returns	New base address if it succeeded, otherwise NULL
 **/
void* FPS3TexturePool::Reallocate( void* OldBaseAddress, INT NewSize )
{
	return FPresizedMemoryPool::Reallocate( OldBaseAddress, NewSize );
}

/**
 * Retrieves allocation stats.
 *
 * @param OutAllocatedMemorySize		[out] Size of allocated memory
 * @param OutAvailableMemorySize		[out] Size of available memory
 * @param OutPendingMemoryAdjustment	[out] Size of pending allocation change (due to async reallocation)
 * @param OutLargestAvailableAllocation	[out] Size of the largest allocation possible
 * @param OutNumFreeChunks				[out] Total number of free chunks (fragmentation holes)
 */
void FPS3TexturePool::GetMemoryStats( INT& OutAllocatedMemorySize, INT& OutAvailableMemorySize, INT& OutPendingMemoryAdjustment, INT& OutLargestAvailableAllocation, INT& OutNumFreeChunks )
{
	FPresizedMemoryPool::GetMemoryStats( OutAllocatedMemorySize, OutAvailableMemorySize, OutPendingMemoryAdjustment );
	OutLargestAvailableAllocation = GPS3Gcm->GetTexturePool()->GetLargestAvailableAllocation( &OutNumFreeChunks );
}

/**
 * Scans the free chunks and returns the largest size you can allocate.
 *
 * @param OutNumFreeChunks	Upon return, contains the total number of free chunks. May be NULL.
 * @return					The largest size of all free chunks.
 */
INT FPS3TexturePool::GetLargestAvailableAllocation( INT* OutNumFreeChunks/*=NULL*/ )
{
	return FPresizedMemoryPool::GetLargestAvailableAllocation( OutNumFreeChunks );
}

/**
 * Notify the texture pool that a new texture has been created.
 * This allows its memory to be relocated during defragmention.
 *
 * @param Texture	The new texture object that has been created
 */
void FPS3TexturePool::RegisterTexture( FPS3RHITexture* Texture )
{
	FScopeLock ScopeLock( &SynchronizationObject );
	SetUserPayload( Texture->Data, PTRINT(Texture) );
}

/**
 * Notify the texture pool that a texture has been deleted.
 *
 * @param Texture	The texture object that is being deleted
 */
void FPS3TexturePool::UnregisterTexture( FPS3RHITexture* Texture )
{
	FScopeLock ScopeLock( &SynchronizationObject );
	SetUserPayload( Texture->Data, NULL );
}

/**
 * Defragment the texture memory. This function can be called from both gamethread and renderthread.
 * Texture memory is shuffled around primarily using GPU transfers. Texture memory that can't be associated
 * with a tracked texture object will not be relocated. Locked textures will not be relocated either.
 */
void FPS3TexturePool::DefragmentTextureMemory()
{
	if ( IsInRenderingThread() )
	{
		FPS3TexturePool::FRelocationStats Stats;
//		GPS3Gcm->BlockUntilGPUIdle();
		DeleteUnusedPS3Resources();
		DeleteUnusedPS3Resources();
		FPresizedMemoryPool::DefragmentMemory( Stats );
//		GPS3Gcm->BlockUntilGPUIdle();
	}
	else
	{
		// Flush and delete all gamethread-deferred-deletion objects (like textures).
//		FlushRenderingCommands();

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			DefragmentTextureMemoryCommand,
		{
			FPS3TexturePool::FRelocationStats Stats;
//			GPS3Gcm->BlockUntilGPUIdle();
			DeleteUnusedPS3Resources();
			DeleteUnusedPS3Resources();
			GPS3Gcm->GetTexturePool()->DefragmentMemory( Stats );
//			GPS3Gcm->BlockUntilGPUIdle();
		});

		// Flush and blocks until defragmentation is completed.
		FlushRenderingCommands();
	}
}

/**
 * Partially defragments the texture memory and tries to process all async reallocation requests at the same time.
 * Call this once per frame.
 */
INT FPS3TexturePool::Tick()
{
	FPS3TexturePool::FRelocationStats Stats;
	INT Result = FPresizedMemoryPool::Tick( Stats );
	return Result;
}

/**
 * Copy memory from one location to another. If it returns FALSE, the defragmentation
 * process will assume the memory is not relocatable and keep it in place.
 * Note: Source and destination may overlap.
 *
 * @param Dest			Destination memory start address
 * @param Source		Source memory start address
 * @param Size			Number of bytes to copy
 * @param UserPayload	User payload for this allocation
 */
void FPS3TexturePool::PlatformRelocate( void* Dest, const void* Source, INT Size, PTRINT UserPayload )
{
	FPS3RHITexture* Texture = (FPS3RHITexture*) UserPayload;
	check( Texture && Texture->CanRelocate() );
	check( Texture->Data == Source );

	UINT DestIOOffset;
	cellGcmAddressToOffset( Dest, &DestIOOffset );
	const BYTE* Src = (const BYTE*) Source;
	BYTE* Dst = (BYTE*) Dest;
	check( (DWORD(Src)&127) == (DWORD(Dst)&127) );	// Check that the alignment matches

	UINT SourceIOOffset;
	cellGcmAddressToOffset( Source, &SourceIOOffset );
	RSXLocalMemMove( DestIOOffset, SourceIOOffset, Size );

	Texture->Data = Dest;
	Texture->SetMemoryOffset( DestIOOffset );
}

/**
 * Inserts a fence to synchronize relocations.
 * The fence can be blocked on at a later time to ensure that all relocations initiated
 * so far has been fully completed.
 *
 * @return		New fence value
 */
DWORD FPS3TexturePool::PlatformInsertFence()
{
	return GPS3Gcm->InsertFence();
}

/**
 * Blocks the calling thread until all relocations initiated before the fence
 * was added has been fully completed.
 *
 * @param Fence		Fence to block on
 */
void FPS3TexturePool::PlatformBlockOnFence( DWORD Fence )
{
	GPS3Gcm->BlockOnFence( Fence );
}

/**
 * Allows each platform to decide whether an allocation can be relocated at this time.
 *
 * @param Source		Base address of the allocation
 * @param UserPayload	User payload for the allocation
 * @return				TRUE if the allocation can be relocated at this time
 */
UBOOL FPS3TexturePool::PlatformCanRelocate( const void* Source, PTRINT UserPayload ) const
{
	FPS3RHITexture* Texture = (FPS3RHITexture*) UserPayload;
	check( Texture == NULL || Texture->Data == Source );
	if ( Texture && Texture->CanRelocate() )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * Notifies the platform that an async reallocation request has been completed.
 *
 * @param FinishedRequest	The request that got completed
 * @param UserPayload		User payload for the allocation
 */
void FPS3TexturePool::PlatformNotifyReallocationFinished( FAsyncReallocationRequest* FinishedRequest, PTRINT UserPayload )
{
	FPS3RHITexture* OldTexture = (FPS3RHITexture*) UserPayload;
	FPS3RHITexture::OnReallocationFinished( OldTexture, FinishedRequest );
}

/**
 * Defragment the texture memory. This function can be called from both gamethread and renderthread.
 * Texture memory is shuffled around primarily using GPU transfers. Texture memory that can't be associated
 * with a tracked texture object will not be relocated. Locked textures will not be relocated either.
 */
void appDefragmentTexturePool()
{
	if ( GEnableTextureMemoryDefragmentation )
	{
		GPS3Gcm->GetTexturePool()->DefragmentTextureMemory();
	}
}

/**
 * Checks if the texture data is allocated within the texture pool or not.
 */
UBOOL appIsPoolTexture( FTextureRHIParamRef TextureRHI )
{
	return TextureRHI && GPS3Gcm->GetTexturePool()->IsTextureMemory( TextureRHI->GetBaseAddress() );
}


/**
 * Log the current texture memory stats.
 *
 * @param Message	This text will be included in the log
 */
void appDumpTextureMemoryStats(const TCHAR* Message)
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	INT AllocatedSize, AvailableSize, PendingMemoryAdjustment, LargestAvailableAllocation, NumFreeChunks;
	GPS3Gcm->GetTexturePool()->GetMemoryStats( AllocatedSize, AvailableSize, PendingMemoryAdjustment, LargestAvailableAllocation, NumFreeChunks );
	debugf(NAME_DevMemory,TEXT("%s - Texture memory available: %.3f MB. Largest available allocation: %.3f MB (%d holes)"), Message, FLOAT(AvailableSize)/1024.0f/1024.0f, FLOAT(LargestAvailableAllocation)/1024.0f/1024.0f, NumFreeChunks );
#endif
}


#endif //USE_PS3_RHI
