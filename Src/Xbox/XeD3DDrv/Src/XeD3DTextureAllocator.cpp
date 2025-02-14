/*=============================================================================
	XeD3DTextureAllocator.cpp: Xbox 360 D3D texture allocator.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "XeD3DDrvPrivate.h"


/** Enables/disables the texture memory defragmentation feature. */
UBOOL GEnableTextureMemoryDefragmentation = TRUE;


#if USE_XeD3D_RHI

/*-----------------------------------------------------------------------------
	FXeTexturePool implementation.
-----------------------------------------------------------------------------*/

/** The texture memory pool manager. */
FXeTexturePool GTexturePool;

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
void FXeTexturePool::PlatformRelocate( void* Dest, const void* Source, INT Size, PTRINT UserPayload )
{
	FXeTextureBase* Texture = (FXeTextureBase*) UserPayload;
	check( Texture && Texture->CanRelocate() );
	check( Texture->GetBaseAddress() == Source );

	DWORD Flags = Texture->GetCreationFlags();
	D3DRESOURCETYPE TextureType = Texture->Resource->GetType();
	switch ( TextureType )
	{
		case D3DRTYPE_TEXTURE:
		{
			D3DSURFACE_DESC Desc;
			IDirect3DTexture9* D3D9Texture = (IDirect3DTexture9*) Texture->Resource;
			D3D9Texture->GetLevelDesc(0, &Desc);
			DWORD NumMips = D3D9Texture->GetLevelCount();
			DWORD TextureSize = XGSetTextureHeaderEx( 
				Desc.Width,
				Desc.Height,
				NumMips, 
				0,
				Desc.Format,
				0,
				(Flags&TexCreate_NoMipTail) ? XGHEADEREX_NONPACKED : 0,
				0,
				XGHEADER_CONTIGUOUS_MIP_OFFSET,
				0,
				D3D9Texture,
				NULL,
				NULL
				);
			// Note: Not necessary, we're not using the TextureSize for anything.
//			TextureSize -= XeCalcUnusedMipTailSize( Desc.Width, Desc.Height, Texture->GetUnrealFormat(), NumMips, (Flags&TexCreate_NoMipTail) ? FALSE : TRUE );

			XGOffsetResourceAddress( D3D9Texture, Dest );
			break;
		}
		case D3DRTYPE_CUBETEXTURE:
		{
			D3DSURFACE_DESC Desc;
			IDirect3DCubeTexture9* D3D9Texture = (IDirect3DCubeTexture9*) Texture->Resource;
			D3D9Texture->GetLevelDesc(0, &Desc);
			DWORD NumMips = D3D9Texture->GetLevelCount();
			DWORD TextureSize = XGSetCubeTextureHeaderEx( 
				Desc.Width,
				NumMips, 
				0,
				Desc.Format,
				0,
				(Flags&TexCreate_NoMipTail) ? XGHEADEREX_NONPACKED : 0,
				0,
				XGHEADER_CONTIGUOUS_MIP_OFFSET,
				D3D9Texture,
				NULL,
				NULL 
				);
			XGOffsetResourceAddress( D3D9Texture, Dest );
			break;
		}
		default:
		{
			check( FALSE );
		}
	}

	Texture->SetBaseAddress( Dest );
	GGPUMemMove.MemMove( Dest, Source, Size );
}

/**
 * Default constructor
 */
FXeTexturePool::FXeTexturePool( )
:	MemoryLossPointer(NULL)
{
}

/**
 * Destructor
 */
FXeTexturePool::~FXeTexturePool()
{
}

/**
 * Initializes the FXeTexturePool (will allocate physical memory!)
 */
void FXeTexturePool::Initialize()
{
	FScopeLock ScopeLock( &SynchronizationObject );
	if ( !IsInitialized() )
	{
		// Retrieve texture memory pool size from ini.
		INT MemoryPoolSize = 0;
		check(GConfig);	
		verify(GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSize"), MemoryPoolSize, GEngineIni));

		// Allow command line override of the pool size.
		INT CustomPoolSize = 0;
		if( Parse( appCmdLine(), TEXT("-POOLSIZE="), CustomPoolSize) )
		{
			MemoryPoolSize = CustomPoolSize;
		}

		// Allow command line reduction of pool size. We notify the streaming code to be less aggressive.
		INT AmountToReduceBy = 0;
		if( Parse( appCmdLine(), TEXT("-REDUCEPOOLSIZE="), AmountToReduceBy) )
		{
			if( AmountToReduceBy > 0 )
			{
				extern UBOOL GIsOperatingWithReducedTexturePool;
				GIsOperatingWithReducedTexturePool = TRUE;
			}
			MemoryPoolSize = Max( MemoryPoolSize / 2, MemoryPoolSize - AmountToReduceBy );
		}

		// Convert from MByte to byte and align.
		MemoryPoolSize = Align(MemoryPoolSize * 1024 * 1024, D3DTEXTURE_ALIGNMENT);

		// Initialize the allocator
#if XBOX_OPTIMIZE_TEXTURE_SIZES
		// here we add tail slack to the pool so that a reduced size allocation right at the 
		// end of the pool will not cause the GPU to overfetch into unmapped memory
		FPresizedMemoryPool::Initialize( MemoryPoolSize, D3DTEXTURE_ALIGNMENT, 16*1024 );
#else
		FPresizedMemoryPool::Initialize( MemoryPoolSize, D3DTEXTURE_ALIGNMENT );
#endif

		// Allocator settings
		{
			FBestFitAllocator::FSettings AllocatorSettings;
			GTexturePool.GetSettings( AllocatorSettings );
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

#if !FINAL_RELEASE
		// Benchmark defrag
		if ( ParseParam( appCmdLine(), TEXT("DefragTest") ) )
		{
			static FLOAT FreeRatio = 0.2f;
			static FLOAT LockRatio = 0.1f;
			static UBOOL bFullDefrag = FALSE;
			Benchmark( 4*1024, 128*1024, FreeRatio, LockRatio, bFullDefrag, TRUE, TEXT("TexturePoolHitch-Layout.bin") );
			Benchmark( 4*1024, 128*1024, FreeRatio, LockRatio, bFullDefrag, FALSE, NULL );
		}
#endif

		INT MemoryLoss = 0;
		GConfig->GetInt( TEXT("TextureStreaming"), TEXT("MemoryLoss"), MemoryLoss, GEngineIni );
		if ( MemoryLoss > 0 )
		{
			MemoryLossPointer = Allocate( MemoryLoss*1024*1024, FALSE, FALSE );
		}
	}
}


/**
 * Allocates texture memory.
 *
 * @param	Size			Size of allocation
 * @param	bAllowFailure	Whether to allow allocation failure or not
 * @param	bDisableDefrag	Whether to disable automatic defragmentation if the allocation failed
 * @returns	Pointer to allocated memory
 */
void* FXeTexturePool::Allocate( DWORD Size, UBOOL bAllowFailure, UBOOL bDisableDefrag )
{
	if ( !IsInitialized() )
	{
		Initialize();
	}

	void* Pointer = FPresizedMemoryPool::Allocate( Size, GEnableTextureMemoryDefragmentation || bAllowFailure );

	// Only do a panic defragmentation if we're not allowing failure.
	if ( Pointer == AllocationFailurePointer && GEnableTextureMemoryDefragmentation && !bDisableDefrag )
	{
		INC_DWORD_STAT( STAT_PanicDefragmentations );
		DefragmentTextureMemory();
		Pointer = FPresizedMemoryPool::Allocate( Size, bAllowFailure );
	}

	// Did the allocation fail?
	if ( Pointer == AllocationFailurePointer && !bAllowFailure )
	{
		// Mark texture memory as having been corrupted or not
		extern UBOOL GIsTextureMemoryCorrupted;
		if ( !GIsTextureMemoryCorrupted )
		{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
			INT AllocatedSize, AvailableSize, LargestAvailableAllocation, PendingMemoryAdjustment, NumFreeChunks;
			GetMemoryStats( AllocatedSize, AvailableSize, PendingMemoryAdjustment, &LargestAvailableAllocation, &NumFreeChunks );
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
void FXeTexturePool::Free( void* Pointer )
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
void* FXeTexturePool::Reallocate( void* OldBaseAddress, INT NewSize )
{
	if ( !IsInitialized() )
	{
		Initialize();
	}
	return FPresizedMemoryPool::Reallocate( OldBaseAddress, NewSize );
}

/**
 * Requests an async allocation or reallocation.
 * The caller must hold on to the request until it has been completed or canceled.
 *
 * @param ReallocationRequest	The request
 * @param bForceRequest			If TRUE, the request will be accepted even if there's currently not enough free space
 * @return						TRUE if the request was accepted
 */
UBOOL FXeTexturePool::AsyncReallocate( FAsyncReallocationRequest* ReallocationRequest, UBOOL bForceRequest )
{
	if ( !IsInitialized() )
	{
		Initialize();
	}
	return FPresizedMemoryPool::AsyncReallocate( ReallocationRequest, bForceRequest );
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
void FXeTexturePool::GetMemoryStats( INT& OutAllocatedMemorySize, INT& OutAvailableMemorySize, INT& OutPendingMemoryAdjustment, INT* OutLargestAvailableAllocation/*=NULL*/, INT* OutNumFreeChunks/*=NULL*/ )
{
	FPresizedMemoryPool::GetMemoryStats( OutAllocatedMemorySize, OutAvailableMemorySize, OutPendingMemoryAdjustment );
	INT TempLargestAvailableAllocation = 0;
	if ( OutLargestAvailableAllocation || OutNumFreeChunks )
	{
		TempLargestAvailableAllocation = GTexturePool.GetLargestAvailableAllocation( OutNumFreeChunks );
	}
	if ( OutLargestAvailableAllocation )
	{
		*OutLargestAvailableAllocation = TempLargestAvailableAllocation;
	}
}

/**
 * Scans the free chunks and returns the largest size you can allocate.
 *
 * @param OutNumFreeChunks	Upon return, contains the total number of free chunks. May be NULL.
 * @return					The largest size of all free chunks.
 */
INT FXeTexturePool::GetLargestAvailableAllocation( INT* OutNumFreeChunks/*=NULL*/ )
{
	return FPresizedMemoryPool::GetLargestAvailableAllocation( OutNumFreeChunks );
}

/**
 * Notify the texture pool that a new texture has been created.
 * This allows its memory to be relocated during defragmentation.
 *
 * @param Texture	The new texture object that has been created
 */
void FXeTexturePool::RegisterTexture( FXeTextureBase* Texture )
{
	if ( IsValidTextureData(Texture->BaseAddress) )
	{
		SetUserPayload( Texture->BaseAddress, PTRINT(Texture) );
	}
}

/**
 * Notify the texture pool that a texture has been deleted.
 *
 * @param Texture	The texture object that is being deleted
 */
void FXeTexturePool::UnregisterTexture( FXeTextureBase* Texture )
{
	if ( IsValidTextureData(Texture->BaseAddress) )
	{
		SetUserPayload( Texture->BaseAddress, 0 );
	}
}

/**
 * Finds the texture matching the given base address.
 *
 * @param BaseAddress	The base address to search for
 * @return				Pointer to the found texture, or NULL if not found
 */
FXeTextureBase* FXeTexturePool::FindTexture( const void* BaseAddress )
{
	PTRINT UserPayload = GetUserPayload( BaseAddress );
	if ( UserPayload )
	{
		return (FXeTextureBase*) UserPayload;
	}
	return NULL;
}

/**
 * Defragment the texture memory. This function can be called from both gamethread and renderthread.
 * Texture memory is shuffled around primarily using GPU transfers. Texture memory that can't be associated
 * with a tracked texture object will not be relocated. Locked textures will not be relocated either.
 */
void FXeTexturePool::DefragmentTextureMemory()
{
	extern void DeleteUnusedXeResources();

	if ( IsInRenderingThread() )
	{
		SCOPED_DRAW_EVENT(EventDefrag)(DEC_SCENE_ITEMS,TEXT("DefragmentTextureMemory"));
		FBestFitAllocator::FRelocationStats Stats;
		RHIBeginScene();
//		XeBlockUntilGPUIdle();
		DeleteUnusedXeResources();
		DeleteUnusedXeResources();
		check( PhysicalMemoryBase );
		GGPUMemMove.BeginMemMove();
		FPresizedMemoryPool::DefragmentMemory( Stats );
		GGPUMemMove.EndMemMove();
		GDirect3DDevice->InvalidateGpuCache( PhysicalMemoryBase, PhysicalMemorySize, 0 );
//		XeBlockUntilGPUIdle();
		RHIEndScene();
	}
	else
	{
		// Flush and delete all gamethread-deferred-deletion objects (like textures).
//		FlushRenderingCommands();

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			DefragmentTextureMemoryCommand,
		{
			SCOPED_DRAW_EVENT(EventDefrag)(DEC_SCENE_ITEMS,TEXT("DefragmentTextureMemory"));
			FBestFitAllocator::FRelocationStats Stats;
			RHIBeginScene();
//			XeBlockUntilGPUIdle();
			DeleteUnusedXeResources();
			DeleteUnusedXeResources();
			check( GTexturePool.PhysicalMemoryBase );
			GGPUMemMove.BeginMemMove();
			GTexturePool.DefragmentMemory( Stats );
			GGPUMemMove.EndMemMove();
			GDirect3DDevice->InvalidateGpuCache( GTexturePool.PhysicalMemoryBase, GTexturePool.PhysicalMemorySize, 0 );
//			XeBlockUntilGPUIdle();
			RHIEndScene();
		});

		// Flush and blocks until defragmentation is completed.
		FlushRenderingCommands();
	}
}

/**
 * Blocks the calling thread until all relocations initiated before the fence
 * was added has been fully completed.
 *
 * @param Fence		Fence to block on
 */
void FXeTexturePool::PlatformBlockOnFence( DWORD Fence )
{
	GDirect3DDevice->BlockOnFence( Fence );
}

/**
 * Allows each platform to decide whether an allocation can be relocated at this time.
 *
 * @param Source		Base address of the allocation
 * @param UserPayload	User payload for the allocation
 * @return				TRUE if the allocation can be relocated at this time
 */
UBOOL FXeTexturePool::PlatformCanRelocate( const void* Source, PTRINT UserPayload ) const
{
	FXeTextureBase* Texture = (FXeTextureBase*) UserPayload;
	if ( Texture && Texture->CanRelocate() )
	{
		check( Texture->GetBaseAddress() == Source );
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
void FXeTexturePool::PlatformNotifyReallocationFinished( FAsyncReallocationRequest* FinishedRequest, PTRINT UserPayload )
{
	FXeTextureBase* OldTexture = (FXeTextureBase*) UserPayload;
	FXeTextureBase::OnReallocationFinished( OldTexture, FinishedRequest );
}

/**
 * Inserts a fence to synchronize relocations.
 * The fence can be blocked on at a later time to ensure that all relocations initiated
 * so far has been fully completed.
 *
 * @return		New fence value
 */
DWORD FXeTexturePool::PlatformInsertFence()
{
	return GDirect3DDevice->InsertFence();
}

/**
 * Partially defragments the texture memory and tries to process all async reallocation requests at the same time.
 * Call this once per frame.
 */
INT FXeTexturePool::Tick()
{
#if !FINAL_RELEASE
	// Enable bAllowProfiling to automatically do one profile capture of the first slow (5+ ms) Tick().
	static UBOOL bAllowProfiling = FALSE;
	static UBOOL bCPUTrace = FALSE;
	static UBOOL bCPUTraceDone = FALSE;
	static FLOAT TraceTriggerTime = 5.0f;	// ms
	static FName TraceName( TEXT("TexturePool"), FNAME_Add );
	static FLOAT LastFrameTime = 0.0f;
	static FLOAT LastFrameBlocked = 0.0f;

	static TArray<FMemoryLayoutElement> MemoryLayout;
	if ( !bCPUTraceDone && !InBenchmarkMode() && bAllowProfiling )
	{
		GetMemoryLayout( MemoryLayout );
	}

	INT NumHolesBefore=0, LargestHoleBefore=0, UsedMemBefore=0, FreeMemBefore=0, PendingMemBefore=0;
	INT NumHolesAfter=0, LargestHoleAfter=0, UsedMemAfter=0, FreeMemAfter=0, PendingMemAfter=0;

	if ( bCPUTrace )
	{
		SynchronizationObject.Lock();

		// Save a "before" image
		TArray<FColor> Bitmap;
		Bitmap.Add( 256*256 );
		GetTextureMemoryVisualizeData( Bitmap.GetTypedData(), 256, 256, 256*sizeof(FColor), 4096 );
		appCreateBitmap( TEXT("TexturePoolProfiling-Before.bmp"), 256, 256, Bitmap.GetTypedData() );

		GetMemoryStats( UsedMemBefore, FreeMemBefore, PendingMemBefore, &LargestHoleBefore, &NumHolesBefore );

		// Save a the layout info
		if ( GFileManager )
		{
			TArray<FMemoryLayoutElement> MemoryProfilingLayout;
			GetMemoryLayout( MemoryProfilingLayout );
			FArchive* Ar = GFileManager->CreateFileWriter( TEXT("TexturePoolProfiling-Layout.bin") );
			if ( Ar )
			{
				*Ar << MemoryProfilingLayout;
				delete Ar;
			}
		}

		GCurrentTraceName = TraceName;
		appStartCPUTrace( TraceName, TRUE, FALSE, 40, NULL );
	}
#endif

	INT Result = 0;
	if ( GFileManager )
	{
		FBestFitAllocator::FRelocationStats Stats;
		GGPUMemMove.BeginMemMove();
		Result = FPresizedMemoryPool::Tick( Stats );
		GGPUMemMove.EndMemMove();
		GDirect3DDevice->InvalidateGpuCache( PhysicalMemoryBase, PhysicalMemorySize, 0 );
	}

#if !FINAL_RELEASE
	if ( bCPUTrace )
	{
		appStopCPUTrace( TraceName );
		bCPUTraceDone = TRUE;

		// Save an "after" image
		TArray<FColor> Bitmap;
		Bitmap.Add( 256*256 );
		GetTextureMemoryVisualizeData( Bitmap.GetTypedData(), 256, 256, 256*sizeof(FColor), 4096 );
		appCreateBitmap( TEXT("TexturePoolProfiling-After.bmp"), 256, 256, Bitmap.GetTypedData() );

		GetMemoryStats( UsedMemAfter, FreeMemAfter, PendingMemAfter, &LargestHoleAfter, &NumHolesAfter );
		debugf( TEXT("TexturePoolProfiling: Last frame %.1f ms (blocked %.1f ms)"), LastFrameTime*1000.0f, LastFrameBlocked*1000.0f );
		debugf( TEXT("TexturePoolProfiling-Before.bmp: %d holes, %.3f MB free, %.3f MB largest hole."),
			NumHolesBefore, FreeMemBefore/1024.0f/1024.0f, LargestHoleBefore/1024.0f/1024.0f);
		debugf( TEXT("TexturePoolProfiling-After.bmp: %d holes, %.3f MB free, %.3f MB largest hole."),
			NumHolesAfter, FreeMemAfter/1024.0f/1024.0f, LargestHoleAfter/1024.0f/1024.0f);

		SynchronizationObject.Unlock();
	}
	bCPUTrace = FALSE;

	FLOAT Duration = 0.0f;
	LastFrameTime = GetTickCycles() * GSecondsPerCycle;
	LastFrameBlocked = GetBlockedCycles() * GSecondsPerCycle;
	FLOAT CPUDuration = Max<FLOAT>( LastFrameTime - LastFrameBlocked, 0.0f );

	// If this Tick was slower than the threshold and we haven't done a CPU Trace yet, trigger one next frame.
	if ( CPUDuration*1000.0 > TraceTriggerTime && !bCPUTraceDone && !InBenchmarkMode() && bAllowProfiling )
	{
		bCPUTrace = TRUE;

		// Save a the layout info from this slow Tick().
		if ( GFileManager )
		{
			FArchive* Ar = GFileManager->CreateFileWriter( TEXT("TexturePoolHitch-Layout.bin") );
			if ( Ar )
			{
				*Ar << MemoryLayout;
				delete Ar;
			}
		}
	}
#endif

	return Result;
}

/**
 * Defragment the texture pool.
 */
void appDefragmentTexturePool()
{
	if ( GEnableTextureMemoryDefragmentation )
	{
		GTexturePool.DefragmentTextureMemory();
	}
}

/**
 * Checks if the texture data is allocated within the texture pool or not.
 */
UBOOL appIsPoolTexture( FTextureRHIParamRef TextureRHI )
{
	return TextureRHI && GTexturePool.IsTextureMemory( TextureRHI->GetBaseAddress() );
}


/**
 * Retrieves texture memory stats.
 *
 * @param	OutAllocatedMemorySize	[out]	Size of allocated memory
 * @param	OutAvailableMemorySize	[out]	Size of available memory
 * @param	PendingMemoryAdjustment		[out]	Amount of memory pending deletion (waiting for GPU)
 *
 * @return TRUE, indicating that out variables are being set
 */
UBOOL RHIGetTextureMemoryStats( INT& AllocatedMemorySize, INT& AvailableMemorySize, INT& PendingMemoryAdjustment )
{
	GTexturePool.GetMemoryStats( AllocatedMemorySize, AvailableMemorySize, PendingMemoryAdjustment );
	return TRUE;
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
	GTexturePool.GetTextureMemoryVisualizeData( TextureData, SizeX, SizeY, Pitch, PixelSize );
	return TRUE;
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

	GTexturePool.GetMemoryStats( AllocatedSize, AvailableSize, PendingMemoryAdjustment, &LargestAvailableAllocation, &NumFreeChunks );
	debugf(NAME_DevMemory,TEXT("%s - Texture memory available: %.3f MB. Largest available allocation: %.3f MB (%d holes)"), Message, FLOAT(AvailableSize)/1024.0f/1024.0f, FLOAT(LargestAvailableAllocation)/1024.0f/1024.0f, NumFreeChunks );
#endif
}


#endif	//USE_XeD3D_RHI
