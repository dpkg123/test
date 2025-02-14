/*=============================================================================
FMallocXenonPooled.h: Xenon support for Pooled allocator
Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

WARNING: this code is not 64 bit clean as it assumes sizeof(void*)==4
WARNING: and also uses DWORD (== 4 bytes) for size instead of size_t

@todo : synchronization with GPU for physical alloc/ free
=============================================================================*/

#include "CorePrivate.h"
#include "FMallocXenon.h"
#include "PerfMem.h"
#include "Database.h"

#if USE_FMALLOC_POOLED || USE_FMALLOC_DL

/** Tracking of memory allocated by the OS for the OS */
extern SIZE_T XMemVirtualAllocationSize;
extern SIZE_T XMemPhysicalAllocationSize;
extern SIZE_T XMemAllocsById[eXALLOCAllocatorId_MsReservedMax - eXALLOCAllocatorId_MsReservedMin + 1];
extern TCHAR* XMemAllocIdDescription[eXALLOCAllocatorId_MsReservedMax - eXALLOCAllocatorId_MsReservedMin + 1];

/**
 * Determines how many bytes we are willing to waste per allocation in order to get large pages. If this is set 
 * to zero then large pages will only be used if it doesn't consume any additional memory. Positive numbers mean 
 * that more memory will be used.
 */
#define WASTAGE_THRESHOLD			0
// CAUTION: QuantizeSize is a special case and is NOT guarded by a thread lock, so IS_SMALL_OS_ALLOC must be intrinsically thread safe!
#define IS_SMALL_OS_ALLOC( Size )	( ( ( Size + WASTAGE_THRESHOLD + 0xFFF ) & ~0x0FFF ) < ( ( Size + 0xFFFF ) & ~0xFFFF ) )

// Macros to determine whether an allocation is using a 4k page or not.
#define IS_VIRTUAL_4K(Ptr)	((((DWORD)(Ptr)) & 0xC0000000) == 0x00000000)
#define IS_VIRTUAL_64K(Ptr)	((((DWORD)(Ptr)) & 0xC0000000) == 0x40000000)
#define IS_PHYSICAL(Ptr)	((((DWORD)(Ptr)) & 0x80000000) == 0x80000000)
#define IS_PHYSICAL_4K(Ptr)	((((DWORD)(Ptr)) & 0xE0000000) == 0xE0000000)
#define IS_PHYSICAL_64K(Ptr)((((DWORD)(Ptr)) & 0xE0000000) == 0xA0000000)

enum { MEM_Cookie_FreeMemory = 0xdeadbeef };
enum { MEM_Cookie_AllocatedMemory = 0xa110ca73 };

#define INVALID_FREEMEM 0xffff

#if STATS_FAST
void FMallocXenonPooled::IncreaseMemoryStats( DWORD Allocated, DWORD Waste, DWORD Used )
{
	check( Allocated < 512 * 1024 * 1024 );
	check( Waste < 512 * 1024 * 1024 );
	check( Used < 512 * 1024 * 1024 );

	if( bIsCurrentAllocationPhysical )
	{
		PhysicalTotalPeak = Max( PhysicalTotalPeak, PhysicalTotalCurrent += Allocated );
		PhysicalWastePeak = Max( PhysicalWastePeak, PhysicalWasteCurrent += Waste );
		PhysicalUsedPeak = Max( PhysicalUsedPeak, PhysicalUsedCurrent += Used );
	}
	else
	{
		VirtualTotalPeak = Max( VirtualTotalPeak, VirtualTotalCurrent += Allocated );
		VirtualWastePeak = Max( VirtualWastePeak, VirtualWasteCurrent += Waste );
		VirtualUsedPeak = Max( VirtualUsedPeak, VirtualUsedCurrent += Used );
	}
}

void FMallocXenonPooled::DecreaseMemoryStats( DWORD Allocated, DWORD Waste, DWORD Used )
{
	check( Allocated < 512 * 1024 * 1024 );
	check( Waste < 512 * 1024 * 1024 );
	check( Used < 512 * 1024 * 1024 );

	if( bIsCurrentAllocationPhysical )
	{
		PhysicalTotalCurrent -= Allocated;
		PhysicalWasteCurrent -= Waste;
		PhysicalUsedCurrent -= Used;
	}
	else
	{
		VirtualTotalCurrent -= Allocated;
		VirtualWasteCurrent -= Waste;
		VirtualUsedCurrent -= Used;
	}
}
#endif

/*-----------------------------------------------------------------------------
	FMallocXenonPooled
-----------------------------------------------------------------------------*/

// Implementation.
void FMallocXenonPooled::OutOfMemory( INT RequestedSize )
{
	// do not end the world if we are testing for fragmentation
	if( !bTestingForFragmentation )
	{
		// Avoid re-entrancy if we don't have enough memory left to print out error message.
		static UBOOL bHasAlreadyBeenCalled = FALSE;
		if( !bHasAlreadyBeenCalled )
		{
			bHasAlreadyBeenCalled = TRUE;

			if( ( FString(appCmdLine()).InStr( TEXT( "DoingASentinelRun=1" ), FALSE, TRUE ) != INDEX_NONE ) 
				|| ( FString(appCmdLine()).InStr( TEXT( "gDASR=1" ), FALSE, TRUE ) != INDEX_NONE ) )
			{
				const FString EndRun = FString::Printf(TEXT("EXEC EndRun @RunID=%i, @ResultDescription='%s'"), GSentinelRunID, *PerfMemRunResultStrings[ARR_OOM] );

				//warnf( TEXT("%s"), *EndRun );
				GTaskPerfMemDatabase->SendExecCommand( *EndRun );
			}

			// Stop the logging thread, so we don't end up with circular calls to malloc
			extern void appStopLoggingThread(void);
			appStopLoggingThread();

			DumpAllocations( *GLog );

			appErrorf( TEXT("OOM: Ran out of memory trying to allocate %i bytes"), RequestedSize );
			// Log OOM value if we allow linking with non approved libs like we do in Test.
			appOutputDebugString( *FString::Printf(TEXT("OOM: Ran out of memory trying to allocate %i bytes") LINE_TERMINATOR, RequestedSize ) );

			// Force a consistent crash even in final release configurations.
			INT* ForceCrash = NULL;
			(*ForceCrash) = 3;
		}
	}
}

/** 
 * Create a 64k page of FPoolInfo structures for tracking allocations
 */
FMallocXenonPooled::FPoolInfo* FMallocXenonPooled::CreateIndirect()
{
	// We need a normal virtual allocation. Cache and change state before allocating.
	ECacheBehaviour OldCurrentAllocationCacheBehaviour = CurrentAllocationCacheBehaviour;
	UBOOL OldbIsCurrentAllocationPhysical = bIsCurrentAllocationPhysical;
	CurrentAllocationCacheBehaviour = CACHE_Normal;
	bIsCurrentAllocationPhysical = FALSE;

	// Allocate the memory - never freed so we don't need to be able to find/ associate.
	DWORD AllocationSize = (POOL_MASK + 1) * sizeof(FPoolInfo);
	FPoolInfo* Indirect = (FPoolInfo*) AllocateOS( AllocationSize, FALSE );
	if( Indirect == NULL )
	{
		OutOfMemory( AllocationSize );
	}

	STAT(VirtualWastePeak = Max(VirtualWastePeak, VirtualWasteCurrent += AllocationSize));

	// Reset allocation/ memory state.
	CurrentAllocationCacheBehaviour = OldCurrentAllocationCacheBehaviour;
	bIsCurrentAllocationPhysical = OldbIsCurrentAllocationPhysical;

	return Indirect;
}

#if USE_MALLOC_PROFILER
/** 
 * Create a 64k page of FPoolOSInfo structures for tracking allocations between 16k and 64k
 */
FMallocXenonPooled::FPoolOSInfo* FMallocXenonPooled::CreateMediumIndirect()
{
	// We need a normal virtual allocation. Cache and change state before allocating.
	ECacheBehaviour OldCurrentAllocationCacheBehaviour = CurrentAllocationCacheBehaviour;
	UBOOL OldbIsCurrentAllocationPhysical = bIsCurrentAllocationPhysical;
	CurrentAllocationCacheBehaviour = CACHE_Normal;
	bIsCurrentAllocationPhysical = FALSE;

	// Allocate the memory - never freed so we don't need to be able to find/ associate.
	DWORD AllocationSize = (POOL_SIMPLE_MASK + 1) * sizeof(FPoolOSInfo);
	FPoolOSInfo* Indirect = (FPoolOSInfo*) AllocateOS( AllocationSize, FALSE );
	if( Indirect == NULL )
	{
		OutOfMemory( AllocationSize );
	}

	STAT(VirtualWastePeak = Max(VirtualWastePeak, VirtualWasteCurrent += AllocationSize));

	// Reset allocation/ memory state.
	CurrentAllocationCacheBehaviour = OldCurrentAllocationCacheBehaviour;
	bIsCurrentAllocationPhysical = OldbIsCurrentAllocationPhysical;

	return Indirect;
}
#endif

/**
 * Returns a pool table fitting the passed in parameters.
 *
 * @param	Size					size of allocation
 * @param	bIsPhysicalAllocation	whether allocation is physical or virtual
 * @param	CacheBehaviour			cache type for allocation
 * @return	matching pool table
 */
FMallocXenonPooled::FPoolTable* FMallocXenonPooled::GetPoolTable( INT Size, UBOOL bIsPhysicalAllocation, ECacheBehaviour CacheBehaviour )
{
	// CAUTION: This logic is partially mirrored in FMallocXenonPooled::QuantizeSize because that code must be intrinsically thread safe
	// if this code is changed, QuantizeSize must also change
	if( Size > MAX_POOLED_ALLOCATION_SIZE )
	{
		return &LargeOsTable;
	}
	else
	{
		Size = Align(Size, 1 << MIN_ALLOC_GRANULARITY_SHIFT) >> MIN_ALLOC_GRANULARITY_SHIFT;

		if( bIsPhysicalAllocation )
		{
			check(CacheBehaviour != CACHE_None);
			return &PoolTable[CacheBehaviour][MemSizeToIndexInPoolTable[CacheBehaviour][Size]];
		}
		else
		{
			return &PoolTable[CACHE_Virtual][MemSizeToIndexInPoolTable[CACHE_Virtual][Size]];
		}
	}
}

FMallocXenonPooled::FMallocXenonPooled()
:	bIsCurrentAllocationPhysical( FALSE )
,	CurrentAllocationCacheBehaviour( CACHE_Normal )
,   bTestingForFragmentation( FALSE )
#if STATS_FAST
,	VirtualTotalCurrent( 0 )
,	VirtualTotalPeak( 0 )
,	VirtualWasteCurrent( 0 )
,	VirtualWastePeak( 0 )
,	VirtualUsedCurrent( 0 )
,	VirtualUsedPeak( 0 )

,	PhysicalTotalCurrent( 0 )
,	PhysicalTotalPeak( 0 )
,	PhysicalWasteCurrent( 0 )
,	PhysicalWastePeak( 0 )
,	PhysicalUsedCurrent( 0 )
,	PhysicalUsedPeak( 0 )

,	CurrentAllocs( 0 )
,	TotalAllocs( 0 )
#endif
#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
,	MemoryLowestTimeRecently( 0.0 )
,	LowestAvailablePageCountRecently( INT_MAX )
,	LowestAvailablePageCountEver( INT_MAX )
,	bCheckMemoryEveryFrame( FALSE )
#endif
{
	bIsCurrentAllocationPhysical	= FALSE;
	CurrentAllocationCacheBehaviour	= CACHE_Normal;

#if STATS_FAST
	appMemset( &MediumOsTable, 0, sizeof( FPoolTable ) );
	appMemset( &LargeOsTable, 0, sizeof( FPoolTable ) );
	appMemset( PoolTable, 0, sizeof( FPoolTable ) * POOL_COUNT * CACHE_MAX );
#endif

	// Init OS tables and fill in unused variables with invalid entries.
	LargeOsTable.bIsPhysicalAllocation	= FALSE;
	LargeOsTable.CacheBehaviour			= CACHE_MAX;
	LargeOsTable.FirstPool				= NULL;
	LargeOsTable.ExhaustedPool			= NULL;
	LargeOsTable.BlockSize				= 0;
#if STATS_FAST
	LargeOsTable.MinRequest				= 512 * 1024 * 1024;

	MediumOsTable.bIsPhysicalAllocation	= FALSE;
	MediumOsTable.CacheBehaviour		= CACHE_MAX;
	MediumOsTable.FirstPool				= NULL;
	MediumOsTable.ExhaustedPool			= NULL;
	MediumOsTable.BlockSize				= 0;

	MediumOsTable.MinRequest			= 512 * 1024 * 1024;
#endif

	// Every allocation is at least 8 byte aligned, every allocation >= 16 bytes is at least 16 byte aligned and
	// every allocation that is a power of two is aligned by the same power of two.
	static const WORD VirtualBlockSizes[POOL_COUNT] =
	{
		8,		16,		32,		48,		64,		80,		96,		112,
		128,	160,	192,	208,	224,	256,	272,	288,
		336,	384,	512,	592,	640,	768,	896,	1024,
		1232,	1488,	1632,	1872,	2048,	2336,	2608,	2976,
		3264,	3632,	3840,	4096,	4368,	4672,	5040,	5456,
		5952,	6544,	7280,	8192,	9360,	10912,	13104,	16384
	};

	static const WORD PhysicalBlockSizes[POOL_COUNT] =
	{
		256,	512,   	1024,	1424,	1808,	2048,	2336,	2608,
		2976,	3264,	3632,	3840,	4096,	4368,	4672,	5040,
		5456,	5952,	6144,	6544,	7280,	8192,	9360,	10240,
		12288,	13104,	14336,	16384,	0,		0,		0,		0,
		0,		0,		0,		0,		0,		0,		0,		0,
		0,		0,		0,		0,		0,		0,		0,		0
	};

	for( INT MemoryType = 0; MemoryType < CACHE_MAX; MemoryType++ )
	{
		// Create the pools for this memory type
		const WORD* BlockSizes = (MemoryType == CACHE_Virtual) ? VirtualBlockSizes : PhysicalBlockSizes;
		for (DWORD i = 0; i < POOL_COUNT; ++i)
		{
			DWORD BlockSize = BlockSizes[i];

			PoolTable[MemoryType][i].bIsPhysicalAllocation	= MemoryType != CACHE_Virtual ? TRUE : FALSE;
			PoolTable[MemoryType][i].CacheBehaviour			= (ECacheBehaviour) MemoryType;
			PoolTable[MemoryType][i].FirstPool				= NULL;
			PoolTable[MemoryType][i].ExhaustedPool			= NULL;
			PoolTable[MemoryType][i].BlockSize				= BlockSize;

#if STATS_FAST
			PoolTable[MemoryType][i].MinRequest = PoolTable[MemoryType][i].BlockSize;
#endif
		}

		// Populate the size->pool index mapping table
		DWORD CurrentSize = 0;
		for (DWORD PoolIndex = 0; PoolIndex < POOL_COUNT; ++PoolIndex)
		{
			DWORD BlockSize = PoolTable[MemoryType][PoolIndex].BlockSize;
			if (BlockSize == 0)
			{
				break;
			}

			// Verify that the pool sizes were in strictly ascending order and is a multiple of the granularity
			check((CurrentSize <= BlockSize) && (Align(BlockSize, 1 << MIN_ALLOC_GRANULARITY_SHIFT) == BlockSize));

			// Fill out the size->pool mapping table for this pool
			while (CurrentSize <= BlockSize)
			{
				MemSizeToIndexInPoolTable[MemoryType][CurrentSize >> MIN_ALLOC_GRANULARITY_SHIFT] = PoolIndex;
				CurrentSize += (1 << MIN_ALLOC_GRANULARITY_SHIFT);
			}
		}
		check(CurrentSize == MAX_POOLED_ALLOCATION_SIZE + (1 << MIN_ALLOC_GRANULARITY_SHIFT));
	}

	// No indirection tables yet
	for( DWORD i = 0; i < INDIRECT_TABLE_SIZE; i++ )
	{
		PoolIndirect[i] = NULL;
#if USE_MALLOC_PROFILER
		PoolOSIndirect[i] = NULL;
#endif
	}
}

/** 
 * Malloc
 */
void* FMallocXenonPooled::Malloc( DWORD Size, DWORD Alignment )
{
	check(Alignment == DEFAULT_ALIGNMENT && "Alignment is not supported on Xenon, yet");
	checkSlow(Size >= 0);
	STAT_FAST(CurrentAllocs++);
	STAT_FAST(TotalAllocs++);
	FFreeMem* Free;
	if( Size <= MAX_POOLED_ALLOCATION_SIZE )
	{
		// Allocate from pool.
		FPoolTable* Table = GetPoolTable( Size, bIsCurrentAllocationPhysical, CurrentAllocationCacheBehaviour );
		FPoolInfo*&	FirstPool = Table->FirstPool;
		FPoolInfo*& ExhaustedPool = Table->ExhaustedPool;
		checkSlow(Size <= Table->BlockSize);
		
#if STATS_FAST
		Table->TotalWaste += Table->BlockSize - Size;
		Table->TotalRequests++;
		Table->ActiveRequests++;
		Table->MaxActiveRequests = Max(Table->MaxActiveRequests, Table->ActiveRequests);
		Table->MaxRequest = Size > Table->MaxRequest ? Size : Table->MaxRequest;
		Table->MinRequest = Size < Table->MinRequest ? Size : Table->MinRequest;
#endif
		FPoolInfo* Pool = FirstPool;
		if( !Pool )
		{
			// Must create a new pool.
			DWORD Blocks = OS_LARGE_PAGE_SIZE / Table->BlockSize;
			DWORD Bytes = Blocks * Table->BlockSize;
			checkSlow(Blocks >= 1);
			checkSlow(Blocks * Table->BlockSize <= Bytes);

			// Allocate memory.
			BYTE* OsPointer = (BYTE*) AllocateOS( Bytes, FALSE );
			Free = (FFreeMem*) OsPointer;
			if( Free )
			{
				// Create pool in the indirect table.
				FPoolInfo*& Indirect = PoolIndirect[((DWORD)Free >> INDIRECT_TABLE_SHIFT)];
				if( !Indirect )
				{
					Indirect = CreateIndirect();
				}
				Pool = &Indirect[((DWORD)Free >> POOL_BIT_SHIFT) & POOL_MASK];

				// Init pool.
				Pool->Link( FirstPool );
				Pool->Bytes			= Bytes;
				Pool->OsBytes		= Align(Bytes, OS_LARGE_PAGE_SIZE);
				Pool->Table			= Table;
				Pool->Taken			= 0;
				Pool->FirstMem		= Free;
				Pool->OsPointer		= OsPointer;

#if STATS_FAST
				IncreaseMemoryStats( Pool->OsBytes, Pool->OsBytes - Pool->Bytes, 0 );
				Table->NumActivePools++;
				Table->MaxActivePools = Max(Table->MaxActivePools, Table->NumActivePools);
#endif
				// Create first free item.
				Free->NumFreeBlocks	= Blocks;
				Free->NextFreeMem	= INVALID_FREEMEM;
				Free->Cookie		= MEM_Cookie_FreeMemory;

				//@TODO: Note, we're not marking every FFreeMem block here (there are "Free->NumFreeBlocks" of them).
				// That would require a for-loop, which would slow down the allocator a little bit. So we can't fully detect
				// if someone is freeing memory that was never allocated, just that it wasn't freed more than once.
			}
		}

		// Pick first available block and unlink it.
		Pool->Taken++;
		checkSlow(Pool->FirstMem);
		checkSlow(Pool->FirstMem->NumFreeBlocks > 0);
		Free = (FFreeMem*)((BYTE*)Pool->FirstMem + ( --Pool->FirstMem->NumFreeBlocks * Table->BlockSize ));

		// Check that no one has overwritten the first 4 bytes in this free memory block.
		check( Pool->FirstMem->Cookie == MEM_Cookie_FreeMemory );
		// Write a different cookie, so we know it's now allocated even if it's never filled in before it's freed.
		Free->Cookie = MEM_Cookie_AllocatedMemory;

		// Did we use up all free blocks in this FFreeMem?
		if( Pool->FirstMem->NumFreeBlocks == 0 )
		{
			// Are there more FFreeMem:s?
			if ( Pool->FirstMem->NextFreeMem != INVALID_FREEMEM )
			{
				Pool->FirstMem = (FFreeMem*) (Pool->OsPointer + Pool->FirstMem->NextFreeMem);
			}
			else
			{
				// Move to exhausted list.
				Pool->FirstMem = NULL;
				Pool->Unlink();
				Pool->Link( ExhaustedPool );
			}
		}
#if STATS_FAST
		IncreaseMemoryStats( 0, 0, Table->BlockSize );
#endif
	}
	// Use 4 KByte pages if and only if it will save some memory. 64 KByte pages are preferred for performance
	// as they result in fewer TLB misses. WASTAGE_THRESHOLD determines how to trade off performance for memory
	// and a value of 0 means we're only going to use 64 KByte pages if the Size was going to be 64 KByte 
	// aligned anyways.	
	else if( IS_SMALL_OS_ALLOC( Size ) )
	{
		BYTE* OsPointer = (BYTE*) AllocateOS( Size, TRUE );			
		Free = (FFreeMem*) OsPointer;
		if( Free )
		{
			checkSlow(!((DWORD)Free & (OS_SMALL_PAGE_SIZE - 1)));
#if USE_MALLOC_PROFILER
			// Create indirect.
			FPoolOSInfo*& Indirect = PoolOSIndirect[((DWORD)Free >> INDIRECT_TABLE_SHIFT)];
			if( !Indirect )
			{
				Indirect = CreateMediumIndirect();
			}

			// Init pool.
			FPoolOSInfo* Pool = &Indirect[((DWORD)Free >> POOL_SIMPLE_BIT_SHIFT) & POOL_SIMPLE_MASK];
			Pool->Bytes	= Size;
#else
			Size = Align(Size, OS_SMALL_PAGE_SIZE);
#endif
#if STATS_FAST
			INT AlignedSize = Align(Size, OS_SMALL_PAGE_SIZE);
			IncreaseMemoryStats( AlignedSize, AlignedSize - Size, Size );

			MediumOsTable.NumActivePools++;
			MediumOsTable.MaxActivePools = Max( MediumOsTable.MaxActivePools, MediumOsTable.NumActivePools );
			MediumOsTable.ActiveRequests++;
			MediumOsTable.TotalRequests++;
			MediumOsTable.ActiveMemory += AlignedSize;
			MediumOsTable.TotalWaste += AlignedSize - Size;
			MediumOsTable.MaxRequest = Size > MediumOsTable.MaxRequest ? Size : MediumOsTable.MaxRequest;
			MediumOsTable.MinRequest = Size < MediumOsTable.MinRequest ? Size : MediumOsTable.MinRequest;
#endif
		}
	}
	else
	{
		// Use 64k pages for large allocs tracked via indirection tables.
		INT AlignedSize = Align(Size, OS_LARGE_PAGE_SIZE);
		BYTE* OsPointer = (BYTE*) AllocateOS( Size, FALSE );			
		Free = (FFreeMem*) OsPointer;
		if( Free )
		{
			checkSlow(!((DWORD)Free & (OS_LARGE_PAGE_SIZE - 1)));
			// Create indirect.
			FPoolInfo*& Indirect = PoolIndirect[((DWORD)Free >> INDIRECT_TABLE_SHIFT)];
			if( !Indirect )
			{
				Indirect = CreateIndirect();
			}

			// Init pool.
			FPoolInfo* Pool = &Indirect[((DWORD)Free >> POOL_BIT_SHIFT) & POOL_MASK];
			Pool->Bytes		= Size;
			Pool->OsBytes	= AlignedSize;
			Pool->Table		= &LargeOsTable;
			Pool->OsPointer	= OsPointer;
#if STATS_FAST
			IncreaseMemoryStats( AlignedSize, AlignedSize - Size, Size );

			LargeOsTable.NumActivePools++;
			LargeOsTable.MaxActivePools = Max( LargeOsTable.MaxActivePools, LargeOsTable.NumActivePools );
			LargeOsTable.ActiveRequests++;
			LargeOsTable.TotalRequests++;
			LargeOsTable.ActiveMemory += AlignedSize;
			LargeOsTable.TotalWaste += AlignedSize - Size;
			LargeOsTable.MaxRequest = Size > LargeOsTable.MaxRequest ? Size : LargeOsTable.MaxRequest;
			LargeOsTable.MinRequest = Size < LargeOsTable.MinRequest ? Size : LargeOsTable.MinRequest;
#endif
		}
	}
	return Free;
}

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
DWORD FMallocXenonPooled::QuantizeSize( DWORD Size, DWORD Alignment )
{
	check(Alignment == DEFAULT_ALIGNMENT && "Alignment is not supported on Xenon, yet");
	checkSlow(Size >= 0);
	DWORD QuantizedSize;
	if( Size <= MAX_POOLED_ALLOCATION_SIZE )
	{
		Size = Align(Size, 1 << MIN_ALLOC_GRANULARITY_SHIFT) >> MIN_ALLOC_GRANULARITY_SHIFT;
		FPoolTable* Table = &PoolTable[CACHE_Virtual][MemSizeToIndexInPoolTable[CACHE_Virtual][Size]];
		QuantizedSize = Table->BlockSize;
	}
	else if( IS_SMALL_OS_ALLOC( Size ) )
	{
		QuantizedSize = Align(Size, OS_SMALL_PAGE_SIZE);
	}
	else
	{
		// Use 64k pages for large allocs 
		QuantizedSize = Align(Size, OS_LARGE_PAGE_SIZE);
	}
	check(Size <= QuantizedSize);
	return QuantizedSize;
}


/** 
 * Realloc
 */
void* FMallocXenonPooled::Realloc( void* Ptr, DWORD NewSize, DWORD Alignment )
{
	check(!bIsCurrentAllocationPhysical && "realloc not supported for physical allocations");
	check(Alignment == DEFAULT_ALIGNMENT && "Alignment is not supported on Xenon");
	void* NewPtr = Ptr;
	if( Ptr && NewSize )
	{
		if( IS_VIRTUAL_64K(Ptr) )
		{
			FPoolInfo* Pool = &PoolIndirect[(DWORD)Ptr >> INDIRECT_TABLE_SHIFT][((DWORD)Ptr >> POOL_BIT_SHIFT) & POOL_MASK];
			if( Pool->Table != &LargeOsTable )
			{			
				// Allocated from pool, so grow or shrink if necessary.
				if( (NewSize > Pool->Table->BlockSize) || (GetPoolTable( NewSize, Pool->Table->bIsPhysicalAllocation, Pool->Table->CacheBehaviour ) != Pool->Table) )
				{
					NewPtr = Malloc( NewSize, Alignment );
					appMemcpy( NewPtr, Ptr, Min(NewSize,Pool->Table->BlockSize) );
					Free( Ptr );
					Ptr = NULL;
				}
			}
			else
			{
				// Large alloc.
				checkSlow(!((DWORD)Ptr & (OS_LARGE_PAGE_SIZE - 1)));
				if( ( NewSize > Pool->OsBytes ) || ( NewSize * 3 < Pool->OsBytes * 2 ) )
				{
					// Grow or shrink.
					NewPtr = Malloc( NewSize, Alignment );
					appMemcpy( NewPtr, Ptr, Min(NewSize, Pool->Bytes) );
					Free( Ptr );
				}
				else
				{
					// Keep as-is, reallocation isn't worth the overhead.
					STAT(VirtualWastePeak = Max(VirtualWastePeak, VirtualWasteCurrent += Pool->Bytes - NewSize));
					STAT(VirtualUsedPeak = Max(VirtualUsedPeak, VirtualUsedCurrent += NewSize - Pool->Bytes));
					Pool->Bytes = NewSize;
				}
			}
		}
		else
		{
#if USE_MALLOC_PROFILER
			FPoolOSInfo* Pool = &PoolOSIndirect[(DWORD)Ptr >> INDIRECT_TABLE_SHIFT][((DWORD)Ptr >> POOL_SIMPLE_BIT_SHIFT) & POOL_SIMPLE_MASK];
			checkSlow(!((INT)Ptr & (OS_SMALL_PAGE_SIZE - 1)));

			DWORD OsBytes = Align(Pool->Bytes,OS_SMALL_PAGE_SIZE);
#else
			MEMORY_BASIC_INFORMATION MemoryInfo = { 0 };
			DWORD OsBytes;
			VirtualQuery(Ptr, &MemoryInfo, sizeof(MemoryInfo));
			OsBytes = MemoryInfo.RegionSize;
#endif

			// Allocated from OS.
			if( ( NewSize > OsBytes ) || ( NewSize * 3 < OsBytes * 2 ) )
			{
				// Grow or shrink.
				NewPtr = Malloc( NewSize, Alignment );
				appMemcpy( NewPtr, Ptr, Min(NewSize,OsBytes) );
				Free( Ptr );
			}
			else
			{
				// Keep as-is, reallocation isn't worth the overhead.
			}
		}
	}
	else if( Ptr == NULL )
	{
		NewPtr = Malloc( NewSize, Alignment );
	}
	else
	{
		if( Ptr )
		{
			Free( Ptr );
		}
		NewPtr = NULL;
	}

	return NewPtr;
}

/**
 * Allocates memory via OS functionality. Context/ state dependent
 * as e.g. bIsCurrentAllocationPhysical determines method used and
 * caching behavior is controlled by state as well.
 *
 * @param	bUse4kPages		Whether to use 4k or 64k pages
 * @return	allocated memory
 */
void* FMallocXenonPooled::AllocateOS( DWORD Size, UBOOL bUse4kPages )
{
	void* Ptr = NULL;
	if( bIsCurrentAllocationPhysical )
	{
		Ptr = XPhysicalAlloc( Size, MAXULONG_PTR, bUse4kPages ? 0x1000 : 0x10000, PAGE_READWRITE | (bUse4kPages ? 0 : MEM_LARGE_PAGES) );
		if( Ptr )
		{
			if( CurrentAllocationCacheBehaviour == CACHE_None )
			{				
				XPhysicalProtect( Ptr, Size, PAGE_READWRITE | PAGE_NOCACHE );
			}
			else if( CurrentAllocationCacheBehaviour == CACHE_WriteCombine )
			{
				XPhysicalProtect( Ptr, Size, PAGE_READWRITE | PAGE_WRITECOMBINE );
			}
		}
	} 
	else
	{
		Ptr = VirtualAlloc( NULL, Size, MEM_NOZERO | MEM_COMMIT | (bUse4kPages ? 0 : MEM_LARGE_PAGES), PAGE_READWRITE );
	}

	if( Ptr == NULL )
	{
		OutOfMemory( Size );
	}
	else if( !bUse4kPages )
	{
		check((((DWORD)Ptr) & 0xFFFF) == 0);
	}

	return Ptr;
}

/**
 * Frees passed in pointer via OS functionality. Context/ state dependent
 * as e.g. bIsCurrentAllocationPhysical determines method used.
 *
 * @param	Ptr		Pointer to free
 */
void FMallocXenonPooled::FreeOS( void* Ptr )
{
	if( bIsCurrentAllocationPhysical )
	{
		XPhysicalFree( Ptr );
	}
	else
	{
		VirtualFree( Ptr, 0, MEM_RELEASE );
	}
}

/** 
 * Free
 */
void FMallocXenonPooled::Free( void* Ptr )
{
	if( Ptr != NULL )
	{
		STAT_FAST(CurrentAllocs--);
		if( IS_VIRTUAL_4K( Ptr ) || IS_PHYSICAL_4K( Ptr ) )
		{
			check( bIsCurrentAllocationPhysical == IS_PHYSICAL_4K( Ptr ) );
#if USE_MALLOC_PROFILER
			FPoolOSInfo* Pool = &PoolOSIndirect[(DWORD)Ptr >> INDIRECT_TABLE_SHIFT][((DWORD)Ptr >> POOL_SIMPLE_BIT_SHIFT) & POOL_SIMPLE_MASK];
			checkSlow(!((INT)Ptr & (OS_SMALL_PAGE_SIZE - 1)));
			DWORD OsBytes = Pool->Bytes;
#elif STATS_FAST
			DWORD OsBytes;
			if( bIsCurrentAllocationPhysical )
			{
				OsBytes = XPhysicalSize( Ptr );				
			}
			else
			{
				MEMORY_BASIC_INFORMATION MemoryInfo = { 0 };
				VirtualQuery(Ptr, &MemoryInfo, sizeof(MemoryInfo));
				OsBytes = MemoryInfo.RegionSize;
			}
#endif
#if STATS_FAST
			INT AlignedSize = Align(OsBytes, OS_SMALL_PAGE_SIZE);
			DecreaseMemoryStats( AlignedSize, AlignedSize - OsBytes, OsBytes );

			MediumOsTable.ActiveRequests--;
			MediumOsTable.NumActivePools--;
			MediumOsTable.ActiveMemory -= AlignedSize;
			MediumOsTable.TotalWaste -= AlignedSize - OsBytes;
#endif
			FreeOS( Ptr );
		}
		else
		{
			FPoolInfo* Pool = &PoolIndirect[(DWORD)Ptr >> INDIRECT_TABLE_SHIFT][((DWORD)Ptr >> POOL_BIT_SHIFT) & POOL_MASK];
			checkSlow(Pool->Bytes != 0);
			// Pooled allocation.
			if( Pool->Table != &LargeOsTable )
			{
#if STATS_FAST
				Pool->Table->ActiveRequests--;
#endif
				// If this pool was exhausted, move to available list.
				if( !Pool->FirstMem )
				{
					Pool->Unlink();
					Pool->Link( Pool->Table->FirstPool );
				}

				FFreeMem* Free		= (FFreeMem *)Ptr;

				// Check that the ptr is a properly aligned block
				check( ((DWORD(Ptr) - DWORD(Pool->OsPointer)) % Pool->Table->BlockSize) == 0 );
#if DO_CHECK
				// Check that the ptr doesn't point to already freed memory.
				if (Free->Cookie == MEM_Cookie_FreeMemory)
				{
					// The cookie indicates that this could be a double-free.  Check to see if
					// the free block is part of it's owner pool freelist.  If so, it's a genuine
					// double-free, otherwise it's just an unlucky coincidence.
					FFreeMem* Node = Pool->FirstMem;
					while (Node != NULL)
					{
						checkf(Node != Free, TEXT("Attempted to free an already freed block"));

						Node = (Node->NextFreeMem != INVALID_FREEMEM) ? (FFreeMem*)(Pool->OsPointer + Node->NextFreeMem) : NULL;
					}
				} 
#endif

				// Add the cookie.
				Free->Cookie		= MEM_Cookie_FreeMemory;

				// Free a pooled allocation.
				Free->NumFreeBlocks	= 1;
				Free->NextFreeMem	= Pool->FirstMem ? (DWORD(Pool->FirstMem) - DWORD(Pool->OsPointer)) : INVALID_FREEMEM;
				Pool->FirstMem		= Free;
#if STATS_FAST
				DecreaseMemoryStats( 0, 0, Pool->Table->BlockSize );
#endif
				// Free this pool.
				checkSlow(Pool->Taken>=1);
				if( --Pool->Taken == 0 )
				{
#if STATS_FAST
					Pool->Table->NumActivePools--;
#endif
					// Free the OS memory.
					Pool->Unlink();
					checkSlow(Pool->Table);
					checkSlow(Pool->Table->bIsPhysicalAllocation == bIsCurrentAllocationPhysical);
					FreeOS(Pool->OsPointer);
#if STATS_FAST
					DecreaseMemoryStats( Pool->OsBytes, Pool->OsBytes - Pool->Bytes, 0 );
#endif
				}
			}
			// Direct OS allocation
			else
			{
				checkSlow(!((INT)Ptr & (OS_LARGE_PAGE_SIZE - 1)));
				checkSlow(Pool->Table);
#if STATS_FAST
				DecreaseMemoryStats( Pool->OsBytes, Pool->OsBytes - Pool->Bytes, Pool->Bytes );
				LargeOsTable.ActiveRequests--;
				LargeOsTable.NumActivePools--;
				LargeOsTable.ActiveMemory -= Pool->OsBytes;
				LargeOsTable.TotalWaste -= Pool->OsBytes - Pool->Bytes;
#endif
				FreeOS( Ptr );
			}
		}
	}
}

/**
* If possible determine the size of the memory allocated at the given address
*
* @param Ptr - Pointer to memory we are checking the size of
* @param SizeOut - If possible, this value is set to the size of the passed in pointer
* @return TRUE if succeeded
*/
UBOOL FMallocXenonPooled::GetAllocationSize(void *Ptr, DWORD &SizeOut)
{
#if STATS_FAST
	if( Ptr != NULL )
	{
		if( IS_VIRTUAL_4K( Ptr ) || IS_PHYSICAL_4K( Ptr ) )
		{
#if USE_MALLOC_PROFILER
			FPoolOSInfo* Pool = &PoolOSIndirect[(DWORD)Ptr >> INDIRECT_TABLE_SHIFT][((DWORD)Ptr >> POOL_SIMPLE_BIT_SHIFT) & POOL_SIMPLE_MASK];
			checkSlow(!((INT)Ptr & (OS_SMALL_PAGE_SIZE - 1)));
			SizeOut = Pool->Bytes;
			return TRUE;
#else
			if( bIsCurrentAllocationPhysical )
			{
				SizeOut = XPhysicalSize( Ptr );				
			}
			else
			{
				MEMORY_BASIC_INFORMATION MemoryInfo = { 0 };
				VirtualQuery(Ptr, &MemoryInfo, sizeof(MemoryInfo));
				SizeOut = MemoryInfo.RegionSize;
			}		
			return TRUE;
#endif
		}
		else
		{
			FPoolInfo* Pool = &PoolIndirect[(DWORD)Ptr >> INDIRECT_TABLE_SHIFT][((DWORD)Ptr >> POOL_BIT_SHIFT) & POOL_MASK];
			// Pooled allocation.
			if( Pool->Table != &LargeOsTable )
			{
				SizeOut = Pool->Table->BlockSize;
				return TRUE;
			}
			// Direct OS allocation
			else
			{
				SizeOut = Pool->Bytes;
				return TRUE;
			}
		}
	}
#endif
	return FALSE;
}

/** 
 * PhysicalAlloc
 */
void* FMallocXenonPooled::PhysicalAlloc( DWORD Count, ECacheBehaviour InCacheBehaviour )
{
	bIsCurrentAllocationPhysical	= TRUE;
	CurrentAllocationCacheBehaviour	= InCacheBehaviour;
	void* Pointer = Malloc( Count, DEFAULT_ALIGNMENT );
	bIsCurrentAllocationPhysical	= FALSE;
	CurrentAllocationCacheBehaviour	= CACHE_Normal;
	return Pointer;
}

/** 
 * PhysicalFree
 */
void FMallocXenonPooled::PhysicalFree( void* Original )
{
	bIsCurrentAllocationPhysical = TRUE;
	Free( Original );
	bIsCurrentAllocationPhysical = FALSE;
}

/**
 * Gathers memory allocations for both virtual and physical allocations.
 *
 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
 */
void FMallocXenonPooled::GetAllocationInfo( FMemoryAllocationStats& MemStats )
{
#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	DM_MEMORY_STATISTICS DmMemStat = { sizeof( DmMemStat ), 0 };
	DmQueryTitleMemoryStatistics( &DmMemStat );

	MemStats.OSReportedUsed = ( DmMemStat.VirtualMappedPages + DmMemStat.ContiguousPages ) * OS_SMALL_PAGE_SIZE;
	MemStats.OSReportedFree = DmMemStat.AvailablePages * OS_SMALL_PAGE_SIZE;
	MemStats.ImageSize = ( DmMemStat.ImagePages + DmMemStat.StackPages ) * OS_SMALL_PAGE_SIZE;
	MemStats.OSOverhead = ( DmMemStat.VirtualPageTablePages + DmMemStat.SystemPageTablePages + DmMemStat.PoolPages + DmMemStat.FileCachePages + DmMemStat.DebuggerPages ) * OS_SMALL_PAGE_SIZE;

	MemStats.TotalUsed = MemStats.OSReportedUsed + MemStats.ImageSize + MemStats.OSOverhead;
	MemStats.TotalAllocated = MemStats.TotalUsed;
#endif

#if STATS_FAST
	MemStats.CPUUsed = VirtualUsedCurrent + XMemVirtualAllocationSize;
	MemStats.CPUWaste = VirtualWasteCurrent;
	MemStats.CPUSlack = VirtualTotalCurrent - VirtualWasteCurrent - VirtualUsedCurrent;

	MemStats.GPUUsed = PhysicalUsedCurrent + XMemPhysicalAllocationSize;
	MemStats.GPUWaste = PhysicalWasteCurrent;
	MemStats.GPUSlack = PhysicalTotalCurrent - PhysicalWasteCurrent - PhysicalUsedCurrent;

	DOUBLE Waste = 0.0;
	for( INT CacheType = CACHE_WriteCombine; CacheType < CACHE_MAX; CacheType++ )
	{
		for( INT PoolIndex = 0; PoolIndex < POOL_COUNT; PoolIndex++ )
		{
			Waste += ( ( DOUBLE )PoolTable[CacheType][PoolIndex].TotalWaste / ( DOUBLE )PoolTable[CacheType][PoolIndex].TotalRequests ) * ( DOUBLE )PoolTable[CacheType][PoolIndex].ActiveRequests;
		}
	}

	MemStats.CPUWaste += ( DWORD )Waste;
#endif
}

#if STATS_FAST
/** 
 * Dump the header for the pool summary
 */
void FMallocXenonPooled::DumpAllocationPoolHeader( class FOutputDevice& Ar )
{
	Ar.Logf( TEXT( "BlockSize NumPools MaxPools CurAllocs TotalAllocs  MinReq     MaxReq  MemUsed MemSlack PoolWaste MemWaste Efficiency" ) );
	Ar.Logf( TEXT( "--------- -------- -------- --------- -----------  ------     ------  ------- -------- --------- -------- ----------" ) );
}

/** 
 * Dump a line of pool table summary
 */
void FMallocXenonPooled::DumpTableAllocations( class FOutputDevice& Ar, FPoolTable* Table, DWORD MemUsed, DWORD MemWaste, DWORD PoolWaste, DWORD MemSlack )
{
	Ar.Logf( TEXT("% 9i % 8i % 8i % 9i % 11i % 7i % 10i % 7iK % 7iK % 8iK % 7iK % 9.2f%%"),
		Table->BlockSize,
		Table->NumActivePools,
		Table->MaxActivePools,
		Table->ActiveRequests,
		( DWORD )Table->TotalRequests,
		Table->MinRequest,
		Table->MaxRequest,
		MemUsed / 1024,
		MemSlack / 1024,
		PoolWaste / 1024,
		MemWaste / 1024,
		Table->NumActivePools ? 100.0f * ( MemUsed - MemWaste ) / Table->ActiveMemory : 100.0f
		);
}
#endif // STATS_FAST

/**
 * Dumps details about all allocations to an output device
 *
 * @param Ar	[in] Output device
 */
void FMallocXenonPooled::DumpAllocations( class FOutputDevice& Ar )
{
	ValidateHeap();

#if STATS_FAST
	Ar.Logf( TEXT("Memory Allocation Status") );
	Ar.Logf( TEXT("") );
	FLOAT AllocatedMB = ( VirtualUsedCurrent + XMemVirtualAllocationSize ) / ( 1024.0f * 1024.0f );
	FLOAT WasteMB = VirtualWasteCurrent / ( 1024.0f * 1024.0f );
	FLOAT SlackMB = ( VirtualTotalCurrent - VirtualWasteCurrent - VirtualUsedCurrent ) / ( 1024.0f * 1024.0f );
	FLOAT OSMB = XMemVirtualAllocationSize / ( 1024.0f * 1024.0f );
	Ar.Logf( TEXT("Virtual Memory tracked: %.2f MB (with %.2f MB used, %.2f MB slack, %.2f MB waste and %.2f MB from the OS)"), AllocatedMB + SlackMB + WasteMB, AllocatedMB, SlackMB, WasteMB, OSMB );
	FLOAT PeakMB = ( VirtualTotalPeak + XMemVirtualAllocationSize ) / ( 1024.0f * 1024.0f );
	Ar.Logf( TEXT("Peak tracked Virtual Memory %.2f MB used"), PeakMB );
	Ar.Logf( TEXT("") );

	AllocatedMB = ( PhysicalUsedCurrent + XMemPhysicalAllocationSize ) / ( 1024.0f * 1024.0f );
	WasteMB = PhysicalWasteCurrent / ( 1024.0f * 1024.0f );
	SlackMB = ( PhysicalTotalCurrent - PhysicalWasteCurrent - PhysicalUsedCurrent ) / ( 1024.0f * 1024.0f );
	OSMB = XMemPhysicalAllocationSize / ( 1024.0f * 1024.0f );
	Ar.Logf( TEXT("Physical Memory tracked: %.2f MB (with %.2f MB used, %.2f MB slack, %.2f MB waste and %.2f MB from the OS)"), AllocatedMB + SlackMB + WasteMB, AllocatedMB, SlackMB, WasteMB, OSMB );	
	PeakMB = ( PhysicalTotalPeak + XMemPhysicalAllocationSize ) / ( 1024.0f * 1024.0f );
	Ar.Logf( TEXT("Peak tracked Physical Memory %.2f MB used"), PeakMB );
	Ar.Logf( TEXT("") );

	Ar.Logf( TEXT("Allocs      % 6i Current / % 6i Total"), CurrentAllocs, TotalAllocs );
	Ar.Logf( TEXT("") );
#endif

#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	DM_MEMORY_STATISTICS DmMemStat;
	DmMemStat.cbSize = sizeof(DmMemStat);
	DmQueryTitleMemoryStatistics( &DmMemStat );

	Ar.Logf(TEXT("DmQueryTitleMemoryStatistics"));
	Ar.Logf( TEXT("") );
	Ar.Logf(TEXT("TotalPages              %6i (%.2f MB)"), DmMemStat.TotalPages, DmMemStat.TotalPages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ) );
	Ar.Logf(TEXT("AvailablePages          %6i (%.2f MB)"), DmMemStat.AvailablePages, DmMemStat.AvailablePages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ) );	
	Ar.Logf(TEXT("VirtualMappedPages      %6i (%.2f MB)"), DmMemStat.VirtualMappedPages, DmMemStat.VirtualMappedPages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ) );
	Ar.Logf(TEXT("ContiguousPages         %6i (%.2f MB)"), DmMemStat.ContiguousPages, DmMemStat.ContiguousPages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ) );
	Ar.Logf(TEXT("ImagePages              %6i (%.2f MB)"), DmMemStat.ImagePages, DmMemStat.ImagePages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ) );
	Ar.Logf(TEXT("StackPages              %6i (%.2f MB)"), DmMemStat.StackPages, DmMemStat.StackPages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ) );	
																							
	Ar.Logf(TEXT("VirtualPageTablePages   %6i (%.2f MB)"), DmMemStat.VirtualPageTablePages, DmMemStat.VirtualPageTablePages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ) );
	Ar.Logf(TEXT("SystemPageTablePages    %6i (%.2f MB)"), DmMemStat.SystemPageTablePages, DmMemStat.SystemPageTablePages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ) );
	Ar.Logf(TEXT("PoolPages               %6i (%.2f MB)"), DmMemStat.PoolPages, DmMemStat.PoolPages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ) );
	Ar.Logf(TEXT("FileCachePages          %6i (%.2f MB)"), DmMemStat.FileCachePages, DmMemStat.FileCachePages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ) );
	Ar.Logf(TEXT("DebuggerPages           %6i (%.2f MB)"), DmMemStat.DebuggerPages, DmMemStat.DebuggerPages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ) );
	Ar.Logf(TEXT(""));

	DWORD FreePages = DmMemStat.AvailablePages;
	DWORD ImageOverheadPages = DmMemStat.ImagePages + DmMemStat.StackPages;
	DWORD OsOverheadPages = DmMemStat.VirtualPageTablePages + DmMemStat.SystemPageTablePages + DmMemStat.PoolPages + DmMemStat.FileCachePages + DmMemStat.DebuggerPages;
	DWORD StillMissingPages = DmMemStat.TotalPages - DmMemStat.VirtualMappedPages - DmMemStat.ContiguousPages - OsOverheadPages - ImageOverheadPages - FreePages;

	Ar.Logf(TEXT("UsedImageAndStack       %6.2f MB"), ImageOverheadPages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ));
	Ar.Logf(TEXT("UsedVirtualMemory       %6.2f MB"), DmMemStat.VirtualMappedPages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ));
	Ar.Logf(TEXT("UsedPhysicalMemory      %6.2f MB"), DmMemStat.ContiguousPages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ));
	Ar.Logf(TEXT("UsedOverhead            %6.2f MB"), OsOverheadPages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ));
	Ar.Logf(TEXT("FreeMemory              %6.2f MB"), FreePages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ));
	Ar.Logf(TEXT("Still missing           %6.2f MB"), StillMissingPages * OS_SMALL_PAGE_SIZE / ( 1024.0f * 1024.0f ));

	Ar.Logf(TEXT(""));
#endif

#if STATS_FAST
	Ar.Logf(TEXT("XMem system allocations"));
	Ar.Logf(TEXT(""));
	for( INT Id = 0; Id < eXALLOCAllocatorId_MsReservedMax - eXALLOCAllocatorId_MsReservedMin + 1; Id++ )
	{
		if( XMemAllocsById[Id] != 0 )
		{
			Ar.Logf(TEXT("%20s    %6.2f MB"), XMemAllocIdDescription[Id], XMemAllocsById[Id] / ( 1024.0f * 1024.0f ));
		}
	}

	Ar.Logf(TEXT(""));
#endif

#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	FLOAT VirtualAnomalyMB = ( ( INT )( DmMemStat.VirtualMappedPages * OS_SMALL_PAGE_SIZE ) - ( INT )( VirtualUsedCurrent + XMemVirtualAllocationSize ) ) / ( 1024.0f * 1024.0f );
	Ar.Logf(TEXT("VirtualAnomaly          %6.2f MB"), VirtualAnomalyMB );
	FLOAT PhysicalAnomalyMB = ( ( INT )( DmMemStat.ContiguousPages * OS_SMALL_PAGE_SIZE ) - ( INT )( PhysicalUsedCurrent + XMemPhysicalAllocationSize ) ) / ( 1024.0f * 1024.0f );
	Ar.Logf(TEXT("PhysicalAnomaly         %6.2f MB"), PhysicalAnomalyMB );

	Ar.Logf(TEXT(""));

	MEMORYSTATUS MemStat;
	MemStat.dwLength = sizeof(MemStat);
	GlobalMemoryStatus( &MemStat );

	Ar.Logf(TEXT("GlobalMemoryStatus"));
	Ar.Logf(TEXT("dwTotalPhys             %6.2f MB"), MemStat.dwTotalPhys / ( 1024.0f * 1024.0f ));
	Ar.Logf(TEXT("dwAvailPhys             %6.2f MB"), MemStat.dwAvailPhys / ( 1024.0f * 1024.0f ));
	Ar.Logf(TEXT("dwTotalVirtual          %6.2f MB"), MemStat.dwTotalVirtual / ( 1024.0f * 1024.0f ));
	Ar.Logf(TEXT("dwAvailVirtual          %6.2f MB"), MemStat.dwAvailVirtual / ( 1024.0f * 1024.0f ));

	Ar.Logf(TEXT(""));

	if (bCheckMemoryEveryFrame)
	{
		if (LowestAvailablePageCountRecently < INT_MAX)
		{
			Ar.Logf(TEXT("RecentLowestAvailPages  %i"), LowestAvailablePageCountRecently);
			Ar.Logf(TEXT("RecentLowestOccuredAgo  %f"), appSeconds() - MemoryLowestTimeRecently);
		}

		if (LowestAvailablePageCountEver < INT_MAX)
		{
			Ar.Logf(TEXT("LowestAvailPagesEver    %i"), LowestAvailablePageCountEver);
		}

		if ((LowestAvailablePageCountRecently < INT_MAX) || (LowestAvailablePageCountEver < INT_MAX))
		{
			Ar.Logf(TEXT(""));
		}

		// Reset the recent lowest, so it gets recalculated by the next interval
		LowestAvailablePageCountRecently = INT_MAX;
	}
#endif

#if STATS_FAST
	// Dump the pool info of the large OS table
	Ar.Logf(TEXT("OS_LARGE_PAGE_SIZE Blocks")); 
	DumpAllocationPoolHeader( Ar );

	DumpTableAllocations( Ar, &LargeOsTable, LargeOsTable.ActiveMemory, LargeOsTable.TotalWaste, 0, 0 );
	Ar.Logf( TEXT( "" ) );

	// Dump the pool info of the large OS table
	Ar.Logf(TEXT("OS_SMALL_PAGE_SIZE Blocks")); 
	DumpAllocationPoolHeader( Ar );

	DumpTableAllocations( Ar, &MediumOsTable, MediumOsTable.ActiveMemory, MediumOsTable.TotalWaste, 0, 0 );
	Ar.Logf( TEXT( "" ) );

	// Dump the pool info of all the tables for small allocations
	for( INT CacheType = CACHE_Normal; CacheType < CACHE_MAX; CacheType++ )
	{
		switch (CacheType)
		{
		case CACHE_Normal: 
			Ar.Logf(TEXT("CacheType Normal")); 
			break;
		case CACHE_WriteCombine: 
			Ar.Logf(TEXT("CacheType WriteCombine")); 
			break;
		// Early out for cachetype none
		case CACHE_None:
			continue;
		case CACHE_Virtual: 
			Ar.Logf(TEXT("CacheType Virtual")); 
			break;
		default: 
			appErrorf(TEXT("Unhandled cache type"));
		}

		DumpAllocationPoolHeader( Ar );

		QWORD TotalMemUsed = 0;
		QWORD TotalMemSlack = 0;
		QWORD TotalPoolWaste = 0;
		QWORD TotalMemWaste = 0;
		QWORD TotalActiveRequests = 0;
		QWORD TotalTotalRequests = 0;
		QWORD TotalPools = 0;

		for( INT i = 0; i < POOL_COUNT; i++ )
		{
			FPoolTable* Table = PoolTable[CacheType] + i;

			if( Table->BlockSize != 0 && Table->ActiveRequests != 0 )
			{
				// MemUsed is the amount of memory in use by the application, including alignment waste
				DWORD MemUsed = Table->BlockSize * Table->ActiveRequests;
				// MemWaste is the estimated alignment waste
				DWORD MemWaste = ( DWORD )( ( ( DOUBLE )Table->TotalWaste / ( DOUBLE )Table->TotalRequests ) * ( DOUBLE )Table->ActiveRequests );
				// PoolWaste is the exact alignment waste per pool which will never be handed to an application
				DWORD PoolWaste = Table->NumActivePools * ( OS_LARGE_PAGE_SIZE - ( ( OS_LARGE_PAGE_SIZE / Table->BlockSize ) * Table->BlockSize ) );
				// MemSlack is the memory allocated by the system, but not yet handed to an application
				DWORD MemSlack = ( Table->NumActivePools * OS_LARGE_PAGE_SIZE ) - PoolWaste - MemUsed;

				Table->ActiveMemory = Table->NumActivePools * OS_LARGE_PAGE_SIZE;
				DumpTableAllocations( Ar, Table, MemUsed, MemWaste, PoolWaste, MemSlack );

				TotalMemUsed += MemUsed;
				TotalMemSlack += MemSlack;
				TotalPoolWaste += PoolWaste;
				TotalMemWaste += MemWaste;
				TotalActiveRequests += Table->ActiveRequests;
				TotalTotalRequests += Table->TotalRequests;
				TotalPools += Table->NumActivePools;
			}
		}

		Ar.Logf( TEXT( "" ) );
		FLOAT TotalMemoryMB = ( TotalPools * OS_LARGE_PAGE_SIZE ) / ( 1024.0f * 1024.0f );
		FLOAT TotalMemUsedMB = TotalMemUsed / ( 1024.0f * 1024.0f );
		FLOAT TotalSlackMB = TotalMemSlack / ( 1024.0f * 1024.0f );
		FLOAT TotalPoolWasteMB = TotalPoolWaste / ( 1024.0f * 1024.0f );
		FLOAT TotalMemWasteMB = TotalMemWaste / ( 1024.0f * 1024.0f );
		Ar.Logf( TEXT( "%.2f MB allocated (containing %.2f MB slack, %.2f MB pool waste and %.2f MB estimated alignment waste). Efficiency %.2f%%" ), TotalMemoryMB, TotalSlackMB, TotalPoolWasteMB, TotalMemWasteMB, TotalPools ? 100.0f * ( TotalMemUsed - TotalMemWaste ) / ( TotalPools * OS_LARGE_PAGE_SIZE ) : 100.0f );
		Ar.Logf( TEXT( "Allocations %i Current / %i Total (in %i pools)"), TotalActiveRequests, TotalTotalRequests, TotalPools );
		Ar.Logf( TEXT( "" ) );
	}
#endif

#if STATS
	Ar.Logf( TEXT( "Xbox 360 Memory Stats:" ) );	
	GStatManager.DumpMemoryStatsForGroup(STATGROUP_XeRHI, Ar);
	Ar.Logf( TEXT( "" ) );
#endif
}

/**
 * Keeps trying to allocate memory until we fail
 *
 * @param Ar Device to send output to
 */
void FMallocXenonPooled::CheckMemoryFragmentationLevel( class FOutputDevice& Ar )
{
	Ar.Logf( TEXT( "Starting CheckMemoryFragmentationLevel" ));
	bTestingForFragmentation = TRUE;
	DWORD Size = 0;
	UBOOL bDidWeAllocate = TRUE;
	while( bDidWeAllocate == TRUE )
	{
		Size += 1024*32; // 32k
		void* Ptr = PhysicalAlloc( Size, CACHE_WriteCombine );
		if( Ptr == NULL )
		{
			Ar.Logf( TEXT( "Fragmentation at: %d b %5.2f kb %5.2f mb" ), Size, Size/1024.0f, Size/1024.0f/1024.0f );
			bDidWeAllocate = FALSE;
		}
		else
		{
			PhysicalFree( Ptr );
		}
	}
	bTestingForFragmentation = FALSE;
	Ar.Logf( TEXT( "Ended CheckMemoryFragmentationLevel" ));
}

/** 
 * Handles any commands passed in on the command line
 */
UBOOL FMallocXenonPooled::Exec( const TCHAR* Cmd, FOutputDevice& Ar ) 
{ 
#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	if( ParseCommand(&Cmd,TEXT("TRACKLOWESTMEMORY")) )
	{
		// Parse the argument
		UBOOL bNewValue = bCheckMemoryEveryFrame;
		FString Parameter0(ParseToken(Cmd, 0));
		if(Parameter0.Len())
		{
			bNewValue = appAtoi(*Parameter0) != 0;
		}

		// Reset the variables if just enabled (as they're currently stale) or disabled (as they'll no longer be valid)
		if (bCheckMemoryEveryFrame != bNewValue)
		{
			bCheckMemoryEveryFrame = bNewValue;

			MemoryLowestTimeRecently = 0.0;
			LowestAvailablePageCountRecently = INT_MAX;
			LowestAvailablePageCountEver = INT_MAX;
		}

		// Remind the user what the current state is
		Ar.Logf(TEXT("TrackLowestMemory: Per-frame tracking is %s."), bCheckMemoryEveryFrame ? TEXT("Enabled") : TEXT("Disabled"));
	}
#endif

	return FALSE;
}

/**
 * Validates the allocator's heap
 */
UBOOL FMallocXenonPooled::ValidateHeap()
{
	for( INT MemoryType=0; MemoryType<CACHE_MAX; MemoryType++ )
	{
		for( INT i=0; i<POOL_COUNT; i++ )
		{
			FPoolTable* Table = &PoolTable[MemoryType][i];
			for( FPoolInfo** PoolPtr=&Table->FirstPool; *PoolPtr; PoolPtr=&(*PoolPtr)->Next )
			{
				FPoolInfo* Pool=*PoolPtr;
				check(Pool->PrevLink==PoolPtr);
				check(Pool->FirstMem);
				for( FFreeMem* Free=Pool->FirstMem; Free; )
				{
					check(Free->NumFreeBlocks > 0);
					Free = (Free->NextFreeMem == INVALID_FREEMEM) ? NULL : (FFreeMem*) (Pool->OsPointer + Free->NextFreeMem);
				}
			}
			for( FPoolInfo** PoolPtr=&Table->ExhaustedPool; *PoolPtr; PoolPtr=&(*PoolPtr)->Next )
			{
				FPoolInfo* Pool=*PoolPtr;
				check(Pool->PrevLink==PoolPtr);
				check(!Pool->FirstMem);
			}
		}
	}

	return( TRUE );
}

/** 
 * Called every game thread tick 
 */
void FMallocXenonPooled::Tick(FLOAT DeltaTime)
{
#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	if (bCheckMemoryEveryFrame)
	{
		// Find out the available title free memory
		DM_MEMORY_STATISTICS DmMemStat;
		DmMemStat.cbSize = sizeof(DmMemStat);
		DmQueryTitleMemoryStatistics(&DmMemStat);

		// Update the minimum tracking
		LowestAvailablePageCountEver = Min(LowestAvailablePageCountEver, DmMemStat.AvailablePages);
		if (DmMemStat.AvailablePages < LowestAvailablePageCountRecently)
		{
			LowestAvailablePageCountRecently = DmMemStat.AvailablePages;
			MemoryLowestTimeRecently = appSeconds();
		}
	}
#endif

	// Call the base implementation
	FMalloc::Tick(DeltaTime);
}

#endif //USE_FMALLOC_POOLED
