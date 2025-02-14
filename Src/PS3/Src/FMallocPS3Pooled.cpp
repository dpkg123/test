/*=============================================================================
	FMallocPS3Pooled.cpp: Shared pooled allocator 

	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

	WARNING: this code is not 64 bit clean as it assumes sizeof(void*)==4
	WARNING: and also uses DWORD (== 4 bytes) for size instead of size_t
=============================================================================*/

#include "CorePrivate.h"
#include "FMallocPS3.h"

#if USE_FMALLOC_POOLED

#if !(USE_STATIC_HOST_MODEL || USE_GROW_DYNAMIC_HOST_MODEL)
#error Unsupported host memory model used with USE_FMALLOC_POOLED
#endif

#define MEM_TIME(st)

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

#if 1
// use 64k chunks (can be 65536 or 1024 * 1024)
#define OS_PAGE_SIZE 65536
// use 64k page size flag (can be _64K or _1M, to match above)
#define PAGE_SIZE_FLAG SYS_MEMORY_PAGE_SIZE_64K
#else
#error
// use 64k chunks (can be 65536 or 1024 * 1024)
#define OS_PAGE_SIZE 1024 * 1024
// use 64k page size flag (can be _64K or _1M, to match above)
#define PAGE_SIZE_FLAG SYS_MEMORY_PAGE_SIZE_1M
#endif


FORCEINLINE void PlatformLargeMalloc(FLargeAllocInfo* AllocInfo, DWORD Size, UBOOL /*bIsPhysicalAlloc*/, ECacheBehaviour /*CacheBehaviour*/, DWORD Alignment)
{
//	printf( "Alignment: %i \n", Alignment );

	// allocated address
	sys_addr_t Address;

	// result from allocation
	INT Result;

	// pool's may be a little smaller than a block size, so align back up
	Size = Align(Size, OS_PAGE_SIZE);

	// allocate from OS
	Result = sys_memory_allocate(Size, PAGE_SIZE_FLAG, &Address);

	// make sure nothing wonky happened (running out of memory is not wonky)
	checkf(Result == CELL_OK || Result == ENOMEM, TEXT("Memory allocation failed with error %d"), Result);

	// return the allocated block in the AllocInfo
	AllocInfo->BaseAddress = (void*)Address;
}

FORCEINLINE void PlatformLargeFree(FLargeAllocInfo* AllocInfo, UBOOL /*bIsPhysicalAlloc*/)
{
	//printf( "  PlatformLargeFree %x\n", AllocInfo->BaseAddress );
	// free back to OS
	sys_memory_free((sys_addr_t)AllocInfo->BaseAddress);
}



// @todo: Once we make final decision on using FMallocPS3Pooled for PS3, remove this check
//#if !PS3 || !USE_BUILTIN_MALLOC

//@obsolete: the below could be rolled into FPoolTable
class FLargeAlloc
{
private:
	// Variables.

	// 32768 (== minimum allocation size for non indirect allocations [of which there are a maximum of 256]) * 8192 == 256 MByte
	//@todo pool: this could be significantly less to save more memory.
	enum {LARGEALLOC_COUNT	= 8192 };

	FLargeAllocInfo			LargeAllocInfo[LARGEALLOC_COUNT];
	FLargeAllocInfo*		AvailablePool;

public:
	// Functions.

	FLargeAllocInfo* Malloc( DWORD Size, UBOOL IsPhysicalAlloc, UBOOL IsPool, ECacheBehaviour CacheBehaviour, DWORD Alignment )
	{
		// Obtain available allocation info and unlink it from pool.
		check( AvailablePool );
		FLargeAllocInfo* AllocInfo = AvailablePool;
		AvailablePool->Unlink();
		AllocInfo->Flags = 0;

#if PS3
		//printf( "FLargeAllocInfo* Malloc %x  %i b %5.2f kb %5.2f mb\n", AllocInfo, Size, Size/1024.0f, Size/1024.0f/1024.0f );
		// Let platform do the actual allocation
		PlatformLargeMalloc(AllocInfo, Size, IsPhysicalAlloc, CacheBehaviour, Alignment);
#endif

		AllocInfo->Size			= Size;
		AllocInfo->Flags		|= (IsPool ? MEMORY_Pool : 0) | MEMORY_Used | (IsPhysicalAlloc ? MEMORY_Physical : 0);

		// address must be aligned to 64 KB
		check( ((DWORD)AllocInfo->BaseAddress & 0xFFFF) == 0 );
		return AllocInfo;
	}

	void Free( FLargeAllocInfo* AllocInfo, UBOOL IsPhysicalAlloc )
	{
		check( AllocInfo );
		check( (AllocInfo->Flags & MEMORY_Physical) == (IsPhysicalAlloc ? MEMORY_Physical : 0) );

#if PS3
		// free back to OS
		//printf( "FLargeAllocInfo* Free: %x\n", AllocInfo->BaseAddress );
		PlatformLargeFree(AllocInfo, IsPhysicalAlloc);
#endif

		// Reset status information and add allocation info to available pool.
		AllocInfo->BaseAddress	= NULL;
		AllocInfo->Size			= 0;
		AllocInfo->Flags		= MEMORY_Free;

		AllocInfo->Link( AvailablePool );
	}

	/**
	 * Gathers memory allocations for both virtual and physical allocations.
	 *
	 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
	 */
	void GetAllocationInfo( SIZE_T& Virtual, SIZE_T& Physical )
	{
		Virtual		= 0;
		Physical	= 0;
		for( DWORD i=0; i<LARGEALLOC_COUNT; i++ )
		{
			if( LargeAllocInfo[i].Flags & MEMORY_Used )
			{
				Physical += LargeAllocInfo[i].Size;
			}
		}
	}

	/**
	 *
	 */
	void Init()
	{
		// Initialize allocation info and pointer data.
		AvailablePool = NULL;
		for( DWORD i=0; i<LARGEALLOC_COUNT; i++ )
		{
			LargeAllocInfo[i].BaseAddress	= 0;
			LargeAllocInfo[i].Size			= 0;
			LargeAllocInfo[i].Flags			= MEMORY_Free;
			LargeAllocInfo[i].Link( AvailablePool );
		}
	}
};

// global large allocator object
FLargeAlloc GLargeAlloc;


///////////////////////////////////////////////////
//	FMalloc interface.
///////////////////////////////////////////////////

FMallocPS3Pooled::FMallocPS3Pooled()
:	MemInit( 0 )
,	OsCurrent( 0 )
,	OsPeak( 0 )
,	UsedCurrent( 0 )
,	UsedPeak( 0 )
,	CurrentAllocs( 0 )
,	TotalAllocs( 0 )
,	MemTime( 0.0 )
,	bIsCurrentAllocationPhysical( FALSE )
,	CurrentAllocationCacheBehaviour( CACHE_Normal )
,   bTestingForFragmentation( FALSE )
{
	Init();
}

void FMallocPS3Pooled::Init()
{
	check(!MemInit);
	MemInit							= 1;
	bIsCurrentAllocationPhysical	= FALSE;
	CurrentAllocationCacheBehaviour	= CACHE_Normal;

	// Init OS tables and fill in unused variables with invalid entries.
	OsTable.bIsPhysicalAllocation	= -1;
	OsTable.CacheBehaviour			= CACHE_MAX;
	OsTable.FirstPool				= NULL;
	OsTable.ExhaustedPool			= NULL;
	OsTable.BlockSize				= 0;

	for( INT MemoryType=0; MemoryType<CACHE_MAX; MemoryType++ )
	{
		// Every allocation is at least 8 byte aligned, every allocation >= 16 bytes is at least 16 byte aligned and
		// every allocation that is a power of two is aligned by the same power of two.
		PoolTable[MemoryType][0].bIsPhysicalAllocation		= MemoryType != CACHE_Virtual ? TRUE : FALSE;
		PoolTable[MemoryType][0].CacheBehaviour				= (ECacheBehaviour) MemoryType;
		PoolTable[MemoryType][0].FirstPool					= NULL;
		PoolTable[MemoryType][0].ExhaustedPool				= NULL;
		PoolTable[MemoryType][0].BlockSize					= 8;
		for( DWORD i=1; i<5; i++ )
		{
			PoolTable[MemoryType][i].bIsPhysicalAllocation	= MemoryType != CACHE_Virtual ? TRUE : FALSE;
			PoolTable[MemoryType][i].CacheBehaviour			= (ECacheBehaviour) MemoryType;
			PoolTable[MemoryType][i].FirstPool				= NULL;
			PoolTable[MemoryType][i].ExhaustedPool			= NULL;
			PoolTable[MemoryType][i].BlockSize				= (8<<((i+1)>>2)) + (2<<i);
		}
		for( DWORD i=5; i<POOL_COUNT; i++ )
		{
			PoolTable[MemoryType][i].bIsPhysicalAllocation	= MemoryType != CACHE_Virtual ? TRUE : FALSE;
			PoolTable[MemoryType][i].CacheBehaviour			= (ECacheBehaviour) MemoryType;
			PoolTable[MemoryType][i].FirstPool				= NULL;
			PoolTable[MemoryType][i].ExhaustedPool			= NULL;
			PoolTable[MemoryType][i].BlockSize				= (4+((i+7)&3)) << (1+((i+7)>>2));
		}

		for( DWORD i=0; i<POOL_MAX; i++ )
		{
			DWORD Index;
			for( Index=0; PoolTable[MemoryType][Index].BlockSize<i; Index++ );
			checkSlow(Index<POOL_COUNT);
			MemSizeToIndexInPoolTable[i] = Index;
		}
		check(POOL_MAX-1==PoolTable[MemoryType][POOL_COUNT-1].BlockSize);
	}

	for( DWORD i=0; i<ARRAY_COUNT(PoolIndirect); i++ )
	{
		PoolIndirect[i] = NULL;
	}

	GLargeAlloc.Init();
}

void* FMallocPS3Pooled::Malloc( DWORD Size, DWORD Alignment )
{
	checkf(Alignment < OS_PAGE_SIZE, TEXT("Alignment must be < %d"), OS_PAGE_SIZE);
	checkSlow(Size>=0);
	checkSlow(MemInit);
	MEM_TIME(MemTime -= appSeconds());
	STAT(CurrentAllocs++);
	STAT(TotalAllocs++);
	FFreeMem* Free;

	// use the pool for small allocations, as long as they have default alignment
	if( Size<POOL_MAX && Alignment == DEFAULT_ALIGNMENT )
	{
		// Allocate from pool.
		FPoolTable* Table			= GetPoolTable( Size, bIsCurrentAllocationPhysical, CurrentAllocationCacheBehaviour );
		FPoolInfo*&	FirstPool		= Table->FirstPool;
		FPoolInfo*& ExhaustedPool	= Table->ExhaustedPool;
		checkSlow(Size<=Table->BlockSize);
		FPoolInfo* Pool = FirstPool;
		if( !Pool )
		{
			// this is using up a 64KB block for a 1byte allocation!?  But it gets reused! So all good!
			// Must create a new pool.
			DWORD Blocks  = 65536 / Table->BlockSize; // @hardcoded
			DWORD Bytes   = Blocks * Table->BlockSize;
			checkSlow(Blocks>=1);
			checkSlow(Blocks*Table->BlockSize<=Bytes);

			// Allocate memory.
			//printf( "we are going into OS land Size<POOL_MAX!  %i b %5.2f kb %5.2f mb  %i\n", Size, Size/1024.0f, Size/1024.0f/1024.0f, Alignment );
			FLargeAllocInfo* AllocInfo = GLargeAlloc.Malloc( Bytes, bIsCurrentAllocationPhysical, TRUE, CurrentAllocationCacheBehaviour, Alignment );
			Free = (FFreeMem*) AllocInfo->BaseAddress;
			if( !Free )
			{
				if( bTestingForFragmentation == TRUE )
				{
					return NULL;
				}
				OutOfMemory();
				return NULL;
			}

			// Create pool in the indirect table.
			FPoolInfo*& Indirect = PoolIndirect[((DWORD)Free>>27)];
			if( !Indirect )
			{
				Indirect = CreateIndirect();
			}
			Pool = &Indirect[((DWORD)Free>>16)&2047];

			// Init pool.
			Pool->Link( FirstPool );
			Pool->Bytes			= Bytes;
			Pool->OsBytes		= Align(Bytes,OS_PAGE_SIZE);
			STAT(OsPeak			= Max(OsPeak,OsCurrent+=Pool->OsBytes));
			Pool->Table			= Table;
			Pool->Taken			= 0;
			Pool->FirstMem		= Free;
			Pool->AllocInfo		= AllocInfo;

			// Create first free item.
			Free->Blocks		= Blocks;
			Free->Next			= NULL;
		}

		// Pick first available block and unlink it.
		Pool->Taken++;
		checkSlow(Pool->FirstMem);
		checkSlow(Pool->FirstMem->Blocks>0);
		Free = (FFreeMem*)((BYTE*)Pool->FirstMem + --Pool->FirstMem->Blocks * Table->BlockSize);
		if( Pool->FirstMem->Blocks==0 )
		{
			Pool->FirstMem = Pool->FirstMem->Next;
			if( !Pool->FirstMem )
			{
				// Move to exhausted list.
				Pool->Unlink();
				Pool->Link( ExhaustedPool );
			}
		}
		STAT(UsedPeak = Max(UsedPeak,UsedCurrent+=Table->BlockSize));
	}
	else
	{
		//printf( "we are going into OS land!  %i b %5.2f kb %5.2f mb  %i\n", Size, Size/1024.0f, Size/1024.0f/1024.0f, Alignment );

		// Use OS for large allocations.
		INT AlignedSize = Align(Size,OS_PAGE_SIZE);
		FLargeAllocInfo* AllocInfo = GLargeAlloc.Malloc( Size, bIsCurrentAllocationPhysical, FALSE, CurrentAllocationCacheBehaviour, Alignment );

		Free = (FFreeMem*) AllocInfo->BaseAddress;
		if( !Free )
		{
			if( bTestingForFragmentation == TRUE )
			{
				return NULL;
			}
			OutOfMemory();
			return NULL;
		}
		checkSlow(!((DWORD)Free&65535));

		// Create indirect.
		FPoolInfo*& Indirect = PoolIndirect[((DWORD)Free>>27)];
		if( !Indirect )
		{
			Indirect = CreateIndirect();
		}

		// Init pool.
		FPoolInfo* Pool = &Indirect[((DWORD)Free>>16)&2047];
		Pool->Bytes		= Size;
		Pool->OsBytes	= AlignedSize;
		Pool->Table		= &OsTable;
		Pool->AllocInfo	= AllocInfo;
		STAT(OsPeak   = Max(OsPeak,  OsCurrent+=AlignedSize));
		STAT(UsedPeak = Max(UsedPeak,UsedCurrent+=Size));
	}
	MEM_TIME(MemTime += appSeconds());
	return Free;
}

void* FMallocPS3Pooled::Realloc( void* Ptr, DWORD NewSize, DWORD Alignment )
{
	checkSlow(MemInit);
	MEM_TIME(MemTime -= appSeconds());
	check(NewSize>=0);
	void* NewPtr = Ptr;
	if( Ptr && NewSize )
	{
		checkf(Alignment == DEFAULT_ALIGNMENT, TEXT("Alignment with realloc is not supported with FMallocPS3Pooled, yet"));
		checkSlow(MemInit);
		FPoolInfo* Pool = &PoolIndirect[(DWORD)Ptr>>27][((DWORD)Ptr>>16)&2047];
		if( Pool->Table!=&OsTable )
		{
			// Allocated from pool, so grow or shrink if necessary.
			if( NewSize>Pool->Table->BlockSize || GetPoolTable( NewSize, Pool->Table->bIsPhysicalAllocation, Pool->Table->CacheBehaviour ) !=Pool->Table )
			{
				NewPtr = Malloc( NewSize, Alignment );
				if( !NewPtr )
				{
					return NULL;
				}
				appMemcpy( NewPtr, Ptr, Min(NewSize,Pool->Table->BlockSize) );
				Free( Ptr );
			}
		}
		else
		{
			// Allocated from OS.
			checkSlow(!((INT)Ptr&65535));
			if( NewSize>Pool->OsBytes || NewSize*3<Pool->OsBytes*2 )
			{
				// Grow or shrink.
				NewPtr = Malloc( NewSize, Alignment );
				if( !NewPtr )
				{
					return NULL;
				}
				appMemcpy( NewPtr, Ptr, Min(NewSize,Pool->Bytes) );
				Free( Ptr );
			}
			else
			{
				// Keep as-is, reallocation isn't worth the overhead.
				STAT(UsedCurrent+=NewSize-Pool->Bytes);
				STAT(UsedPeak=Max(UsedPeak,UsedCurrent));
				Pool->Bytes = NewSize;
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
	MEM_TIME(MemTime += appSeconds());
	return NewPtr;
}

void FMallocPS3Pooled::Free( void* Ptr )
{
	if( !Ptr )
	{
		return;
	}
	checkSlow(MemInit);
	MEM_TIME(MemTime -= appSeconds());
	STAT(CurrentAllocs--);

	// Windows version.
	FPoolInfo* Pool = &PoolIndirect[(DWORD)Ptr>>27][((DWORD)Ptr>>16)&2047];
	checkSlow(Pool->Bytes!=0);
	if( Pool->Table!=&OsTable )
	{
		// If this pool was exhausted, move to available list.
		if( !Pool->FirstMem )
		{
			Pool->Unlink();
			Pool->Link( Pool->Table->FirstPool );
		}

		// Free a pooled allocation.
		FFreeMem* Free		= (FFreeMem *)Ptr;
		Free->Blocks		= 1;
		Free->Next			= Pool->FirstMem;
		Pool->FirstMem		= Free;
		STAT(UsedCurrent   -= Pool->Table->BlockSize);

		// Free this pool.
		checkSlow(Pool->Taken>=1);
		if( --Pool->Taken == 0 )
		{
			// Free the OS memory.
			Pool->Unlink();
			checkSlow(Pool->Table);
			checkSlow(Pool->Table->bIsPhysicalAllocation == bIsCurrentAllocationPhysical);
			GLargeAlloc.Free( Pool->AllocInfo, Pool->Table->bIsPhysicalAllocation );
			STAT(OsCurrent -= Pool->OsBytes);
		}
	}
	else
	{
		// Free an OS allocation.
		checkSlow(!((INT)Ptr&65535));
		checkSlow(Pool->Table);
		STAT(UsedCurrent -= Pool->Bytes);
		STAT(OsCurrent   -= Pool->OsBytes);
		GLargeAlloc.Free( Pool->AllocInfo, bIsCurrentAllocationPhysical );	
	}
	MEM_TIME(MemTime += appSeconds());
}

void* FMallocPS3Pooled::PhysicalAlloc( DWORD Count, ECacheBehaviour InCacheBehaviour )
{
	bIsCurrentAllocationPhysical	= TRUE;
	CurrentAllocationCacheBehaviour	= InCacheBehaviour;
	void* Pointer = Malloc( Count, DEFAULT_ALIGNMENT );
	bIsCurrentAllocationPhysical	= FALSE;
	CurrentAllocationCacheBehaviour	= CACHE_Normal;
	return Pointer;
}

void FMallocPS3Pooled::PhysicalFree( void* Original )
{
	bIsCurrentAllocationPhysical = TRUE;
	Free( Original );
	bIsCurrentAllocationPhysical = FALSE;
}

void FMallocPS3Pooled::DumpAllocations( FOutputDevice& Ar )
{
	FMallocPS3Pooled::HeapCheck();

	STAT(Ar.Logf( TEXT("Memory Allocation Status") ));
	STAT(Ar.Logf( TEXT("Curr Memory % 5.3fM / % 5.3fM"), UsedCurrent/1024.0/1024.0, OsCurrent/1024.0/1024.0 ));
	STAT(Ar.Logf( TEXT("Peak Memory % 5.3fM / % 5.3fM"), UsedPeak   /1024.0/1024.0, OsPeak   /1024.0/1024.0 ));
	STAT(Ar.Logf( TEXT("Allocs      % 6i Current / % 6i Total"), CurrentAllocs, TotalAllocs ));
	STAT(Ar.Logf( TEXT("") ));
	MEM_TIME(Ar.Logf( "Seconds     % 5.3f", MemTime ));
	MEM_TIME(Ar.Logf( "MSec/Allc   % 5.5f", 1000.0 * MemTime / MemAllocs ));

	struct FAllocStats
	{
		DWORD	SizePool,
			SizeOther;
		DWORD	AllocsPool,
			AllocsOther;
		DWORD	Waste;
	};

	FAllocStats Virtual		= { 0 },
		Physical	= { 0 },
		*AllocStats	= NULL;

	for( DWORD i=0; i<LARGEALLOC_COUNT; i++ )
	{
		if( LargeAllocInfo[i].Flags & MEMORY_Used )
		{
			AllocStats = LargeAllocInfo[i].Flags & MEMORY_Physical ? &Physical : &Virtual;
			if( LargeAllocInfo[i].Flags & MEMORY_Pool )
			{
				AllocStats->SizePool += LargeAllocInfo[i].Size;
				AllocStats->AllocsPool++;
			}
			else
			{
				AllocStats->SizeOther += LargeAllocInfo[i].Size;
				AllocStats->AllocsOther++;
			}
			AllocStats->Waste += Align( LargeAllocInfo[i].Size, OS_PAGE_SIZE ) - LargeAllocInfo[i].Size;
		}
	}

	Ar.Logf(TEXT("Large Allocation Status:"));
	Ar.Logf(TEXT("Phys Memory (pool/other): % 5.3fM (%i) / % 5.3fM (%i), Waste: % 5.3fM"), Physical.SizePool / 1024.f / 1024.f, Physical.AllocsPool, Physical.SizeOther / 1024.f / 1024.f , Physical.AllocsOther, Physical.Waste / 1024.f / 1024.f );
	Ar.Logf(TEXT("Virt Memory (pool/other): % 5.3fM (%i) / % 5.3fM (%i), Waste: % 5.3fM"), Virtual.SizePool / 1024.f / 1024.f, Virtual.AllocsPool, Virtual.SizeOther / 1024.f / 1024.f , Virtual.AllocsOther, Virtual.Waste / 1024.f / 1024.f );
}

void FMallocPS3Pooled::HeapCheck()
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
				for( FFreeMem* Free=Pool->FirstMem; Free; Free=Free->Next )
				{				
					check(Free->Blocks>0);
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
}

// Implementation.
void FMallocPS3Pooled::OutOfMemory()
{
	if (GPS3MallocNoCrashOnOOM)
	{
		return;
	}

	static UBOOL bHasAlreadyBeenCalled = FALSE;
	// Avoid re-entrancy if we don't have enough memory left to print out error message.
	if( !bHasAlreadyBeenCalled )
	{
		bHasAlreadyBeenCalled = TRUE;
		DumpAllocations( *GLog );
		appErrorf( *LocalizeError("OutOfMemory",TEXT("Core")) );
		// Force a consistent crash even in final release configurations.
		INT* ForceCrash = NULL;
		(*ForceCrash) = 3;
	}
	else
	{
		appDebugBreak();
	}
}

FMallocPS3Pooled::FPoolInfo* FMallocPS3Pooled::CreateIndirect()
{
	FLargeAllocInfo* AllocInfo = GLargeAlloc.Malloc( 256*sizeof(FPoolInfo), FALSE, TRUE, CACHE_Normal, DEFAULT_ALIGNMENT );
	// We're never going to free this memory so we don't have to keep track of this allocation.
	FPoolInfo* Indirect = (FPoolInfo*) AllocInfo->BaseAddress;
	if( !Indirect )
	{
		OutOfMemory();
	}
	return Indirect;
}

void FMallocPS3Pooled::CheckMemoryFragmentationLevel( FOutputDevice& Ar )
{
	bTestingForFragmentation = TRUE;

	// determine how much free memory we currently have
	appPS3DumpDetailedMemoryInformation( Ar );

	// determine the fragmentation level
	DWORD Alignment = DEFAULT_ALIGNMENT;

	DWORD Size = 0;
	UBOOL bDidWeAllocate = TRUE;
	while( bDidWeAllocate == TRUE )
	{
		Size += 1024*32; // 32k

		void* Ptr;
		if (Alignment != DEFAULT_ALIGNMENT)
		{
			Ar.Logf( TEXT( "BADNESS:  %i b %5.2f kb %5.2f mb" ), Size, Size/1024.0f, Size/1024.0f/1024.0f );

			Ptr = this->Malloc(Size,Alignment);
		}
		else
		{
			//Ptr = malloc(Size);
			Ptr = this->Malloc( Size, DEFAULT_ALIGNMENT );
		}

		if( Ptr == NULL )
		{
			Ar.Logf( TEXT( "Fragmentation at:  %i b %5.2f kb %5.2f mb" ), Size, Size/1024.0f, Size/1024.0f/1024.0f );
			bDidWeAllocate = FALSE;
		}
		else
		{
			//Ar.Logf( TEXT( "All Good at: %d %d" ), Size, Size/1024 );
			//free( Ptr );
			this->Free( Ptr );
		}
	}

	bTestingForFragmentation = FALSE;

}





void FMallocPS3Pooled::StaticInit()
{
	// query for how much free memory we have before any allocations happen
	sys_memory_info MemInfo;
	sys_memory_get_user_memory_size(&MemInfo);
	GPS3MemoryAvailableAtBoot = MemInfo.available_user_memory;

	// Allocate RSX command buffer (this is nicer than memalign, since there is no header info,
	// so we won't leave a "hole" between the container and commandbuffer memory blocks).
	sys_addr_t CommandBufferAddr;
	sys_memory_allocate(GPS3CommandBufferSizeMegs * 1024 * 1024, SYS_MEMORY_PAGE_SIZE_1M, &CommandBufferAddr);
	GPS3CommandBuffer = (void*) CommandBufferAddr;

	// get how much room malloc has
	sys_memory_get_user_memory_size(&MemInfo);
	GPS3MemoryAvailableToMalloc = MemInfo.available_user_memory;
}




void FMallocPS3Pooled::DoAfterStaticInit()
{
}

/**
 * Gathers memory allocations for both virtual and physical allocations.
 *
 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
 */
void FMallocPS3Pooled::GetAllocationInfo( FMemoryAllocationStats& MemStats )
{
	sys_memory_info_t MemInfo;
	sys_memory_get_user_memory_size(&MemInfo);
	MemStats.ImageSize = MemInfo.total_user_memory - GPS3MemoryAvailableAtBoot;

	std::malloc_managed_size MemInfo;
	malloc_stats(&MemInfo);

	// Add in GLargeAlloc stats??

	MemStats.TotalUsed = MemInfo.current_inuse_size;
	MemStats.CPUUsed = MemInfo.current_inuse_size;
}

/**
 * Dumps details about all allocations to an output device
 *
 * @param Ar	[in] Output device
 */
void FMallocPS3Pooled::DumpAllocations( class FOutputDevice& Ar )
{
}

#endif // USE_FMALLOC_POOLED


