/*=============================================================================
	FMallocPS3Pooled.h: Shared pooled allocator (needs platform-specific helpers)

	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

	WARNING: this code is not 64 bit clean as it assumes sizeof(void*)==4
	WARNING: and also uses DWORD (== 4 bytes) for size instead of size_t
=============================================================================*/


// This is temp copy of the FWindowsMalloc that has been modified to be used to make Platform generic structure of code
// checking now was we are trying to get the fragmentation fixed on ps3 and we have 3 different allocators  weeee

#ifndef _F_MALLOC_POOLED_H_
#define _F_MALLOC_POOLED_H_

struct FLargeAllocInfo
{
	void*				BaseAddress;		// Virtual base address, required for reallocating memory on load.
	DWORD				Size;				// Size of allocation, required for restoring allocations on load.
	DWORD				Flags;				// Usage flags, useful for gathering statistics (should be removed for final release).
	FLargeAllocInfo*	Next;
	FLargeAllocInfo**	PrevLink;

	void Unlink()
	{
		if( Next )
		{
			Next->PrevLink = PrevLink;
		}
		*PrevLink = Next;
	}

	void Link(FLargeAllocInfo*& Before)
	{
		if(Before)
		{
			Before->PrevLink = &Next;
		}
		Next = Before;
		PrevLink = &Before;
		Before = this;
	}
};

//
// Pooled memory allocator.
//
class FMallocPS3Pooled : public FMalloc
{
private:
	// Counts.
	enum {POOL_COUNT		= 42     };
	enum {POOL_MAX			= 32768+1};

	// Forward declares.
	struct FFreeMem;
	struct FPoolTable;

	// Memory pool info. 32 bytes.
	struct FPoolInfo
	{
		DWORD				Bytes;		// Bytes allocated for pool.
		DWORD				OsBytes;	// Bytes aligned to page size.
		DWORD				Taken;      // Number of allocated elements in this pool, when counts down to zero can free the entire pool.
		FPoolTable*			Table;		// Index of pool.
		FFreeMem*			FirstMem;   // Pointer to first free memory in this pool.
		struct FLargeAllocInfo*	AllocInfo;	// Pointer to large allocation info.
		FPoolInfo*			Next;
		FPoolInfo**			PrevLink;

		void Link( FPoolInfo*& Before )
		{
			if( Before )
			{
				Before->PrevLink = &Next;
			}
			Next     = Before;
			PrevLink = &Before;
			Before   = this;
		}

		void Unlink()
		{
			if( Next )
			{
				Next->PrevLink = PrevLink;
			}
			*PrevLink = Next;
		}
	};

	// Information about a piece of free memory. 8 bytes.
	struct FFreeMem
	{
		FFreeMem*		Next;		// Next or MemLastPool[], always in order by pool.
		DWORD			Blocks;		// Number of consecutive free blocks here, at least 1.
		FPoolInfo* GetPool()
		{
			return (FPoolInfo*)((INT)this & 0xffff0000);
		}
	};

	// Pool table.
	struct FPoolTable
	{
		/** Indicates whether the pool maps to physical or virtual memory allocations */
		UBOOL				bIsPhysicalAllocation;
		/** Defines the cache behavior for physical allocations */
		ECacheBehaviour		CacheBehaviour;
		FPoolInfo*			FirstPool;
		FPoolInfo*			ExhaustedPool;
		DWORD				BlockSize;
	};

	// Variables.
	FPoolTable			PoolTable[CACHE_MAX][POOL_COUNT], OsTable;
	FPoolInfo*			PoolIndirect[32];
	INT					MemSizeToIndexInPoolTable[POOL_MAX];
	INT					MemInit;
	INT					OsCurrent,OsPeak,UsedCurrent,UsedPeak,CurrentAllocs,TotalAllocs;
	DOUBLE				MemTime;

	// Temporary status information used to not have to break interface for Malloc.
	UBOOL				bIsCurrentAllocationPhysical;
	ECacheBehaviour		CurrentAllocationCacheBehaviour;

	/** Whether or not we are testing for fragmentation.  This will make it so we do not appErrorf but instead return NULL when OOM **/
	UBOOL bTestingForFragmentation;


	// Implementation.
	void OutOfMemory();
	FPoolInfo* CreateIndirect();

	/**
	 * Returns a pool table fitting the passed in parameters.
	 *
	 * @param	Size					size of allocation
	 * @param	bIsPhysicalAllocation	whether allocation is physical or virtual
	 * @param	CacheBehaviour			cache type for allocation
	 * @return	matching pool table
	 */
	FPoolTable* GetPoolTable( INT Size, UBOOL bIsPhysicalAllocation, ECacheBehaviour CacheBehaviour )
	{
		if( Size>=POOL_MAX )
		{
			return &OsTable;
		}
		else
		{
			if( bIsPhysicalAllocation )
			{
				return &PoolTable[CacheBehaviour][MemSizeToIndexInPoolTable[Size]];
			}
			else
			{
				return &PoolTable[CACHE_Virtual][MemSizeToIndexInPoolTable[Size]];
			}
		}
	}

public:
	// FMalloc interface.
	FMallocPS3Pooled();

	/**
	 * Pre-initialize memory stuff before we call new on this (which calls 
	 * the constructor, so constructor is too late). 
	 */
	static void StaticInit();


	void* Malloc( DWORD Size, DWORD Alignment );
	void* Realloc( void* Ptr, DWORD NewSize, DWORD Alignment );
	void Free( void* Ptr );
	void* PhysicalAlloc( DWORD Count, ECacheBehaviour InCacheBehaviour );
	void PhysicalFree( void* Original );

	/**
	 * Gathers memory allocations for both virtual and physical allocations.
	 *
	 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
	 */
	virtual void GetAllocationInfo( FMemoryAllocationStats& MemStats );

	/**
	 * Dumps details about all allocations to an output device
	 *
	 * @param Ar	[in] Output device
	 */
	virtual void DumpAllocations( class FOutputDevice& Ar );

	void HeapCheck();

	/**
	 * Keeps trying to allocate memory until we fail
	 *
	 * @param Ar Device to send output to
	 */
	virtual void CheckMemoryFragmentationLevel( class FOutputDevice& Ar );


	void Init();

	/** This is a test function which can be called right after StaticInit().  (e.g. for testing realloc) Also because various GLogs have not been set up so need to use printf **/
	void DoAfterStaticInit();

};

#endif // _F_MALLOC_POOLED_H_


