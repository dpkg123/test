/*=============================================================================
	PS3TexturePool.h: PS3 texture pool definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if USE_PS3_RHI

#include "BestFitAllocator.h"

#ifndef __PS3TEXTUREPOOL_H__
#define __PS3TEXTUREPOOL_H__

class FPS3RHITexture;


class FPS3TexturePool : protected FPresizedMemoryPool
{
public:
	/**
	 * Constructor, initializes the FPresizedMemoryPool with already allocated memory
	 * 
	 * @param PoolMemory				Pointer to the pre-allocated memory pool
	 * @param FailedAllocationMemory	Dummy pointer which be returned upon OutOfMemory
	 * @param PoolSize					Size of the memory pool
	 * @param Alignment					Default alignment for each allocation
	 */
	FPS3TexturePool( BYTE* PoolMemory, BYTE* FailedAllocationMemory, INT PoolSize, INT Alignment );

	/**
	 * Destructor
	 */
	~FPS3TexturePool( );

	/**
	 * Allocates texture memory.
	 *
	 * @param	Size			Size of allocation
	 * @param	bAllowFailure	Whether to allow OOM silently, because the caller checks for it instead
	 * @returns	Pointer to allocated memory
	 */
	void*	Allocate( DWORD Size, UBOOL bAllowFailure );

	/**
	 * Frees texture memory allocated via Allocate
	 *
	 * @param	Pointer		Allocation to free
	 */
	void	Free( void* Pointer );

	/**
	 * Tries to reallocate texture memory in-place (without relocating),
	 * by adjusting the base address of the allocation but keeping the end address the same.
	 *
	 * @param	OldBaseAddress	Pointer to the original allocation
	 * @returns	New base address if it succeeded, otherwise NULL
	 **/
	void*	Reallocate( void* OldBaseAddress, INT NewSize );

	/**
	 * Requests an async allocation or reallocation.
	 * The caller must hold on to the request until it has been completed or canceled.
	 *
	 * @param ReallocationRequest	The request
	 * @param bForceRequest			If TRUE, the request will be accepted even if there's currently not enough free space
	 * @return						TRUE if the request was accepted
	 */
	UBOOL	AsyncReallocate( FAsyncReallocationRequest* ReallocationRequest, UBOOL bForceRequest )
	{
		return FPresizedMemoryPool::AsyncReallocate( ReallocationRequest, bForceRequest );
	}

	/**
	 * Blocks the calling thread until all relocations and reallocations that were initiated by Tick() have completed.
	 *
	 * @return		TRUE if there were any relocations in progress before this call
	 */
	void	FinishAllRelocations()
	{
		FPresizedMemoryPool::FinishAllRelocations();
	}

	/**
	 * Blocks the calling thread until the specified request has been completed.
	 *
	 * @param Request	Request to wait for. Must be a valid request.
	 */
	void	BlockOnAsyncReallocation( FAsyncReallocationRequest* Request )
	{
		FPresizedMemoryPool::BlockOnAsyncReallocation( Request );
	}

	/**
	 * Cancels the specified reallocation request.
	 * Note that the allocator doesn't keep track of requests after it's been completed,
	 * so the user must provide the current base address. This may not match any of the
	 * addresses in the (old) request since the memory may have been relocated since then.
	 *
	 * @param Request				Request to cancel. Must be a valid request.
	 * @param CurrentBaseAddress	Current baseaddress used by the allocation.
	 */
	void	CancelAsyncReallocation( FAsyncReallocationRequest* Request, const void* CurrentPointer )
	{
		FPresizedMemoryPool::CancelAsyncReallocation( Request, CurrentPointer );
	}

	/**
	 * Returns the current allocator settings.
	 *
	 * @param OutSettings	[out] Current allocator settings
	 */
	void	GetSettings( FSettings& OutSettings )
	{
		FPresizedMemoryPool::GetSettings( OutSettings );
	}

	/**
	 * Sets new allocator settings.
	 *
	 * @param InSettings	New allocator settings to replace the old ones.
	 */
	void	SetSettings( const FSettings& InSettings )
	{
		FPresizedMemoryPool::SetSettings( InSettings );
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
	void	GetMemoryStats( INT& OutAllocatedMemorySize, INT& OutAvailableMemorySize, INT& OutPendingMemoryAdjustment, INT& OutLargestAvailableAllocation, INT& OutNumFreeChunks );

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
	UBOOL	GetTextureMemoryVisualizeData( FColor* TextureData, INT SizeX, INT SizeY, INT Pitch, INT PixelSize )
	{
		return FPresizedMemoryPool::GetTextureMemoryVisualizeData( TextureData, SizeX, SizeY, Pitch, PixelSize );
	}

	/**
	 * Scans the free chunks and returns the largest size you can allocate.
	 *
	 * @param OutNumFreeChunks	Upon return, contains the total number of free chunks. May be NULL.
	 * @return					The largest size of all free chunks.
	 */
	INT		GetLargestAvailableAllocation( INT* OutNumFreeChunks=NULL );

	/**
	 * Locks the memory chunk in the texture pool to prevent any changes to it (i.e. preventing defragmentation)
	 */
	void	Lock( void* BaseAddress )
	{
		FPresizedMemoryPool::Lock( BaseAddress );
	}

	/**
	 * Unlocks the memory chunk in the texture pool to allow changes to it again (i.e. allowing defragmentation)
	 */
	void	Unlock( void* BaseAddress )
	{
		FPresizedMemoryPool::Unlock( BaseAddress );
	}

	/**
	 * Determines whether this pointer resides within the texture memory pool.
	 *
	 * @param	Pointer		Pointer to check
	 * @return				TRUE if the pointer resides within the texture memory pool, otherwise FALSE.
	 */
	UBOOL	IsTextureMemory( const void* Pointer )
	{
		return FPresizedMemoryPool::IsTextureMemory( Pointer );
	}

	/**
	 * Checks whether the pointer represents valid texture data.
	 *
	 * @param	Pointer		Baseaddress of the texture data to check
	 * @return				TRUE if it points to valid texture data
	 */
	UBOOL	IsValidTextureData( void* Pointer )
	{
		return FPresizedMemoryPool::IsValidTextureData( Pointer );
	}

	/**
	 * Notify the texture pool that a new texture has been created.
	 * This allows its memory to be relocated during defragmentation.
	 *
	 * @param Texture	The new texture object that has been created
	 */
	void	RegisterTexture( FPS3RHITexture* Texture );

	/**
	 * Notify the texture pool that a texture has been deleted.
	 *
	 * @param Texture	The texture object that is being deleted
	 */
	void	UnregisterTexture( FPS3RHITexture* Texture );

	/**
	 * Defragment the texture memory. This function can be called from both gamethread and renderthread.
	 * Texture memory is shuffled around primarily using GPU transfers. Texture memory that can't be associated
	 * with a tracked texture object will not be relocated. Locked textures will not be relocated either.
	 */
	void	DefragmentTextureMemory( );

	/**
	 * Partially defragments the texture memory and tries to process all async reallocation requests at the same time.
	 * Call this once per frame.
	 */
	INT		Tick();

protected:
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
	virtual void	PlatformRelocate( void* Dest, const void* Source, INT Size, PTRINT UserPayload );

	/**
	 * Inserts a fence to synchronize relocations.
	 * The fence can be blocked on at a later time to ensure that all relocations initiated
	 * so far has been fully completed.
	 *
	 * @return		New fence value
	 */
	virtual DWORD	PlatformInsertFence();

	/**
	 * Blocks the calling thread until all relocations initiated before the fence
	 * was added has been fully completed.
	 *
	 * @param Fence		Fence to block on
	 */
	virtual void	PlatformBlockOnFence( DWORD Fence );

	/**
	 * Allows each platform to decide whether an allocation can be relocated at this time.
	 *
	 * @param Source		Base address of the allocation
	 * @param UserPayload	User payload for the allocation
	 * @return				TRUE if the allocation can be relocated at this time
	 */
	virtual UBOOL	PlatformCanRelocate( const void* Source, PTRINT UserPayload ) const;

	/**
	 * Notifies the platform that an async reallocation request has been completed.
	 *
	 * @param FinishedRequest	The request that got completed
	 * @param UserPayload		User payload for the allocation
	 */
	virtual void	PlatformNotifyReallocationFinished( FAsyncReallocationRequest* FinishedRequest, PTRINT UserPayload );

	/** Memory loss for optimization purposes. */
	void* MemoryLossPointer;
};


/**
 * Defragment the texture memory. This function can be called from both gamethread and renderthread.
 * Texture memory is shuffled around primarily using GPU transfers. Texture memory that can't be associated
 * with a tracked texture object will not be relocated. Locked textures will not be relocated either.
 */
void appDefragmentTexturePool();


#endif //__PS3TEXTUREPOOL_H__

#endif //USE_PS3_RHI
