/*=============================================================================
FMallocXenonPooled.h: Xenon support for Pooled allocator
Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

WARNING: this code is not 64 bit clean as it assumes sizeof(void*)==4
WARNING: and also uses DWORD (== 4 bytes) for size instead of size_t

@todo : synchronization with GPU for physical alloc/ free
=============================================================================*/

#ifndef _F_MALLOC_XENON_POOLED_H_
#define _F_MALLOC_XENON_POOLED_H_

// Memory flags.
enum EMemoryFlags
{
	MEMORY_Free			= 0,
	MEMORY_Used			= 1,
	MEMORY_Pool			= 2,
	MEMORY_Physical		= 4,
	MEMORY_NoCache		= 8,
	MEMORY_WriteCombine	= 16
};

/**
 * Optimized Xbox 360 memory allocator based on Windows implementation. The main difference
 * is using 4K pages for large OS allocations and pointer tricks to determine allocation
 * page size. This saves ~10 MByte for large levels.
 */
class FMallocXenonPooled : public FMalloc
{
private:
	/** Counts (note: POOL_COUNT should be less than 256, or the BYTE type in MemSizeToIndexInPoolTable should be changed). */
	enum { POOL_COUNT = 48 };

	/** Maximum allocation for the pooled allocator */
	enum { MAX_POOLED_ALLOCATION_SIZE = 16384 };

	/** All allocation bucket sizes (must be a multiple of 8) */
	enum { MIN_ALLOC_GRANULARITY_SHIFT = 3 }; 

	/** Size of indirect pools */
	enum { INDIRECT_TABLE_BIT_SIZE = 5 };
	enum { INDIRECT_TABLE_SIZE = ( 1 << INDIRECT_TABLE_BIT_SIZE ) };
	enum { INDIRECT_TABLE_SHIFT = ( 32 - INDIRECT_TABLE_BIT_SIZE ) };

	/** Shift to get the 64k reference from the indirect tables */
	enum { POOL_BIT_SHIFT = 16 };
	/** Used to mask off the bits that have been used to lookup the indirect table */
	enum { POOL_MASK = ( ( 1 << ( INDIRECT_TABLE_SHIFT - POOL_BIT_SHIFT ) ) - 1 ) };

	/** Shift for the simple tracker */
	enum { POOL_SIMPLE_BIT_SHIFT = 12 };
	/** Used to mask off the bits that have been used to lookup the indirect table */
	enum { POOL_SIMPLE_MASK = ( ( 1 << ( INDIRECT_TABLE_SHIFT - POOL_SIMPLE_BIT_SHIFT ) ) - 1 ) };

	enum { OS_SMALL_PAGE_SIZE = 0x1000 };
	enum { OS_LARGE_PAGE_SIZE = 0x10000 };

	// Forward declares.
	struct FFreeMem;
	struct FPoolTable;

	// Memory pool info for 64k blocks. 32 bytes.
	struct FPoolInfo
	{
		DWORD				Bytes;		// Bytes allocated for pool.
		DWORD				OsBytes;	// Bytes aligned to page size.
		DWORD				Taken;      // Number of allocated elements in this pool, when counts down to zero can free the entire pool.
		FPoolTable*			Table;		// Index of pool.
		FFreeMem*			FirstMem;   // Pointer to first free memory in this pool.
		BYTE*				OsPointer;	// Pointer to OS allocation if used.
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

	// Memory pool info for 16k-64k blocks (4k aligned). 4 bytes.
	struct FPoolOSInfo
	{
		DWORD				Bytes;		// Exact bytes of allocation
	};

	// Information about a piece of free memory. 8 bytes.
	struct FFreeMem
	{
		DWORD			Cookie;			// Cookie to detect whether we're freeing something twice.
		WORD			NextFreeMem;	// Byte-index to the next FFreeMem within the pool.
		WORD			NumFreeBlocks;	// Number of consecutive free blocks here, at least 1.
	};

	// Pool table.
	struct FPoolTable
	{
		/** Indicates whether the pool maps to physical or virtual memory allocations */
		UBOOL				bIsPhysicalAllocation;
		/** Defines the cache behaviour for physical allocations */
		ECacheBehaviour		CacheBehaviour;
		FPoolInfo*			FirstPool;
		FPoolInfo*			ExhaustedPool;
		DWORD				BlockSize;

#if STATS_FAST
		/** Number of currently active pools */
		DWORD				NumActivePools;

		/** Largest number of pools simultaneously active */
		DWORD				MaxActivePools;

		/** Number of requests currently active */
		DWORD				ActiveRequests;

		/** High watermark of requests simultaneously active */
		DWORD				MaxActiveRequests;

		/** Minimum request size (in bytes) */
		DWORD				MinRequest;

		/** Maximum request size (in bytes) */
		DWORD				MaxRequest;

		/** Total number of requests ever */
		QWORD				TotalRequests;

		/** Total currently allocated memory in this table, including waste */
		QWORD				ActiveMemory;

		/** Total waste from all allocs in this table */
		QWORD				TotalWaste;
#endif
	};

	// Tracking of all the pools for small allocations
	FPoolTable			PoolTable[CACHE_MAX][POOL_COUNT];
#if STATS_FAST
	// Tracking of all the pools for allocations larger than MAX_POOLED_ALLOCATION_SIZE and less than OS_LARGE_PAGE_SIZE
	FPoolTable			MediumOsTable;
#endif
	// Tracking of all the pools for allocations larger than OS_LARGE_PAGE_SIZE
	FPoolTable			LargeOsTable;
	// Indirection table to quickly access the correct pool table
	FPoolInfo*			PoolIndirect[INDIRECT_TABLE_SIZE];
#if USE_MALLOC_PROFILER
	// Indirection table to quickly access the size of the OS alloc
	FPoolOSInfo*		PoolOSIndirect[INDIRECT_TABLE_SIZE];
#endif

	BYTE				MemSizeToIndexInPoolTable[CACHE_MAX][1 + (MAX_POOLED_ALLOCATION_SIZE >> MIN_ALLOC_GRANULARITY_SHIFT)];

#if STATS_FAST
	DWORD				VirtualTotalCurrent;
	DWORD				VirtualTotalPeak;
	DWORD				VirtualWasteCurrent;
	DWORD				VirtualWastePeak;
	DWORD				VirtualUsedCurrent;
	DWORD				VirtualUsedPeak;

	DWORD				PhysicalTotalCurrent;
	DWORD				PhysicalTotalPeak;
	DWORD				PhysicalWasteCurrent;
	DWORD				PhysicalWastePeak;
	DWORD				PhysicalUsedCurrent;
	DWORD				PhysicalUsedPeak;

	DWORD				CurrentAllocs;
	DWORD				TotalAllocs;

	void				IncreaseMemoryStats( DWORD Allocated, DWORD Waste, DWORD Used );
	void				DecreaseMemoryStats( DWORD Allocated, DWORD Waste, DWORD Used );
	void				DumpAllocationPoolHeader( class FOutputDevice& Ar );
	void				DumpTableAllocations( class FOutputDevice& Ar, FPoolTable* Table, DWORD MemUsed, DWORD MemWaste, DWORD PoolWaste, DWORD MemSlack );
#endif

#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	/** The time when the memory was lowest, since the last time DumpAllocations was called (updated if bCheckMemoryEveryFrame) */
	DOUBLE				MemoryLowestTimeRecently;

	/** The lowest that the allocator reached, since the last time DumpAllocations was called (updated if bCheckMemoryEveryFrame) */
	DWORD				LowestAvailablePageCountRecently;

	/** The lowest that the allocator reached, since the last time DumpAllocations was called (updated if bCheckMemoryEveryFrame) */
	DWORD				LowestAvailablePageCountEver;
	
	/** Should the Tick() for the allocator check the number of free pages each frame */
	UBOOL				bCheckMemoryEveryFrame;
#endif

	// Temporary status information used to not have to break interface for Malloc.
	UBOOL				bIsCurrentAllocationPhysical;
	ECacheBehaviour		CurrentAllocationCacheBehaviour;

    /** Whether or not we are testing for fragmentation.  This will make it so we do not appErrorf but instead return NULL when OOM **/
	UBOOL				bTestingForFragmentation;

	// Implementation.
	void OutOfMemory( INT RequestedSize );

	/** 
	 * Create a 64k page of FPoolInfo structures for tracking allocations
	 */
	FPoolInfo* CreateIndirect();

#if USE_MALLOC_PROFILER
	/** 
	 * Create a 64k page of FPoolOSInfo structures for tracking allocations between 16k and 64k
	 */
	FPoolOSInfo* CreateMediumIndirect();
#endif

	/**
	 * Returns a pool table fitting the passed in parameters.
	 *
	 * @param	Size					size of allocation
	 * @param	bIsPhysicalAllocation	whether allocation is physical or virtual
	 * @param	CacheBehaviour			cache type for allocation
	 * @return	matching pool table
	 */
	FPoolTable* GetPoolTable( INT Size, UBOOL bIsPhysicalAllocation, ECacheBehaviour CacheBehaviour );

public:
	FMallocXenonPooled();


	/** 
	 * QuantizeSize returns the actual size of allocation request likely to be returned
	 * so for the template containers that use slack, they can more wisely pick
	 * appropriate sizes to grow and shrink to.
	 *
	 * CAUTION: QuantizeSize is a special case and is NOT guarded by a thread lock, so must be intrinsically thread safe!
	 *
	 * @param Size			The size of a hypothetical allocation request
	 * @param Alignment		The alignment of a hypothetical allocation request
	 * @return				Returns the usable size that the allocation request would return. In other words you can ask for this greater amount without using any more actual memory.
	 */
	DWORD QuantizeSize( DWORD Size, DWORD Alignment );

	/** 
	 * Malloc
	 */
	virtual void* Malloc( DWORD Size, DWORD Alignment );

	/** 
	 * Realloc
	 */
	virtual void* Realloc( void* Ptr, DWORD NewSize, DWORD Alignment );

	/** 
	 * Free
	 */
	virtual void Free( void* Ptr );

	/** 
	 * PhysicalAlloc
	 */
	virtual void* PhysicalAlloc( DWORD Count, ECacheBehaviour InCacheBehaviour );

	/** 
	 * PhysicalFree
	 */
	virtual void PhysicalFree( void* Original );

	/** 
	 * Handles any commands passed in on the command line
	 */
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

	/** 
	 * Called every game thread tick 
	 */
	virtual void Tick( FLOAT DeltaTime );

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

	/**
	 * Validates the allocator's heap
	 */
	virtual UBOOL ValidateHeap();

	/**
 	 * Keeps trying to allocate memory until we fail
	 *
	 * @param Ar Device to send output to
	 */
	virtual void CheckMemoryFragmentationLevel( class FOutputDevice& Ar );

	/**
	* If possible determine the size of the memory allocated at the given address
	*
	* @param Original - Pointer to memory we are checking the size of
	* @param SizeOut - If possible, this value is set to the size of the passed in pointer
	* @return TRUE if succeeded
	*/
	virtual UBOOL GetAllocationSize(void *Original, DWORD &SizeOut);

	/**
	 * Allocates memory via OS functionality. Context/ state dependent
	 * as e.g. bIsCurrentAllocationPhysical determines method used and
	 * caching behavior is controlled by state as well.
	 *
	 * @param	bUse4kPages		Whether to use 4k or 64k pages
	 * @return	allocated memory
	 */
	void* AllocateOS( DWORD Size, UBOOL bUse4kPages );

	/**
	 * Frees passed in pointer via OS functionality. Context/ state dependent
	 * as e.g. bIsCurrentAllocationPhysical determines method used.
	 *
	 * @param	Ptr		Pointer to free
	 */
	void FreeOS( void* Ptr );
};

#endif //_F_MALLOC_XENON_POOLED_H_
