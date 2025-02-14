/*=============================================================================
	FMallocPS3.cpp: PS3 memory allocator implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "FMallocPS3.h"

#if PS3_SNC
#pragma diag_suppress=112,175
#endif

#include "UnMallocDL.inl" 

#if PS3_SNC
#pragma diag_default=112,552
#endif

#if USE_FMALLOC_DL

#if !(USE_STATIC_HOST_MODEL || USE_GROW_DYNAMIC_HOST_MODEL)
#error Unsupported host memory model used with USE_FMALLOC_DL
#endif

#if TRACK_MEM_USING_STAT_SECTIONS
// stat sections relies on unique ptrs for each realloc(0,0) call
#define USE_GLOBAL_REALLOC_ZERO_PTR 0
#else
#define USE_GLOBAL_REALLOC_ZERO_PTR 1
#endif

void* FMallocPS3DL::ReallocZeroPtr = NULL;
FMallocPS3DL::FMemSpace FMallocPS3DL::MemSpaces[];

// our externs
extern void dlfree(void* mem);
extern void* dlrealloc(void*, size_t);
extern void* dlmalloc(size_t bytes);
extern void* dlmemalign(size_t alignment, size_t bytes);
extern struct mallinfo dlmallinfo(void);

static void VARARGS ImmediateDebugf( FOutputDevice* Ar, const ANSICHAR *Format, ... )
{
	static ANSICHAR TempStr[4096];
	GET_VARARGS_ANSI( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Format, Format );

	// Note: We can't use Ar because it can cause a thread dead-lock if the Logf is locked
	// to another thread who's calling debugf, and the other thread is blocked by this allocator.
	printf( TempStr );
	printf( "\n" );
}

void FMallocPS3DL::StaticInit()
{
	// query for how much free memory we have before any allocations happen
	sys_memory_info MemInfo;
	sys_memory_get_user_memory_size(&MemInfo);
	GPS3MemoryAvailableAtBoot = MemInfo.available_user_memory;

	// Allocate RSX command buffer (this is nicer than memalign, since there is no header info,
	// so we won't leave a "hole" between the container and commandbuffer memory blocks).
	// note: this could use the host allocator but keeping the manual os allocation here to mirror functionality in FMallocPS3Builtin 
	sys_addr_t CommandBufferAddr;
	sys_memory_allocate(GPS3CommandBufferSizeMegs * 1024 * 1024, SYS_MEMORY_PAGE_SIZE_1M, &CommandBufferAddr);
	GPS3CommandBuffer = (void*) CommandBufferAddr;

	// Reserve space for PRX modules
	// Not needed for dl malloc since it mmaps memory directly from os

	// get how much room malloc has
	sys_memory_get_user_memory_size(&MemInfo);
	GPS3MemoryAvailableToMalloc = MemInfo.available_user_memory;
}

FMallocPS3DL::FMallocPS3DL()
#if STATS
:	OsCurrent( 0 )
,	OsPeak( 0 )
,	CurrentAllocs( 0 )
,	TotalAllocs( 0 )
,	TotalWaste( 0 )
,	MemTime( 0.0 )
#endif
{
 	appMemzero(MemSpaces,sizeof(MemSpaces)); 

	const SIZE_T MaxAllocSizeHeap0 = 32;
	// create a heap for all allocs <= 32 bytes
 	MemSpaces[MemSpace_Heap0].MemSpacePtr = create_mspace(0,0);
 	MemSpaces[MemSpace_Heap0].MinAllocSize = 0;
 	MemSpaces[MemSpace_Heap0].MaxAllocSize = pad_request(MaxAllocSizeHeap0) - CHUNK_OVERHEAD;

	ReallocZeroPtr = NULL;
}

FMallocPS3DL::~FMallocPS3DL()
{
	// do nothing. can leak, etc, no big deal. But, since Free may be called after this function during shutdown
	// it needs to still work to be able to call dlfree
#if 0
	if( ReallocZeroPtr )
	{
		dlfree(ReallocZeroPtr);
		ReallocZeroPtr=NULL;
	}
	// destroy any custom heaps that were created
	for( INT MemSpaceIdx=0; MemSpaceIdx < ARRAY_COUNT(MemSpaces); MemSpaceIdx++ )
	{
		if( MemSpaces[MemSpace_Heap0].MemSpacePtr )
		{
			destroy_mspace(MemSpaces[MemSpace_Heap0].MemSpacePtr);
		}
	}
#endif
}

/** 
 * Malloc
 */
void* FMallocPS3DL::Malloc( DWORD Size, DWORD Alignment )
{
	void* Ptr = NULL;

	// use a different heap based on the requested size
	DWORD UsableSize = pad_request( Size ) - CHUNK_OVERHEAD;
	if( MemSpaces[MemSpace_Heap0].MemSpacePtr &&
		UsableSize >= MemSpaces[MemSpace_Heap0].MinAllocSize && 
		UsableSize <= MemSpaces[MemSpace_Heap0].MaxAllocSize )
	{
		if( Alignment != DEFAULT_ALIGNMENT )
		{
			Ptr = mspace_memalign(MemSpaces[MemSpace_Heap0].MemSpacePtr, Alignment, Size);
		}
		else
		{
			Ptr = mspace_malloc(MemSpaces[MemSpace_Heap0].MemSpacePtr, Size);
		}
	}
	// default heap
	else
	{
		if( Alignment != DEFAULT_ALIGNMENT )
		{
			Ptr = dlmemalign( Alignment, Size );
		}
		else
		{
			Ptr = dlmalloc( Size );
		}
	}

	if( !Ptr )
	{
		OutOfMemory( Size );
	}

#if STATS
	// get how much malloc is marked as usable
	DWORD AlignedSize = dlmalloc_usable_size( Ptr );
	OsPeak = Max( OsPeak, OsCurrent += AlignedSize );

	// update the waste
	TotalWaste += AlignedSize - Size;

	// update count of allocations
	TotalAllocs++;
	CurrentAllocs++;
#endif

	return Ptr;
}

/** 
 * Free
 */
void FMallocPS3DL::Free( void* Ptr )
{
#if STATS
	if( Ptr )
	{
		// get how much malloc is marked as usable
		INT UsableSize = dlmalloc_usable_size( Ptr );

		// update total bytes
		OsCurrent -= UsableSize;
		CurrentAllocs--;
	}
#endif

	if( Ptr != ReallocZeroPtr )
	{
		// find the heap the ptr was allocated from based on its size
		SIZE_T Size = dlmalloc_usable_size(Ptr);		
		if( MemSpaces[MemSpace_Heap0].MemSpacePtr &&
			Size >= MemSpaces[MemSpace_Heap0].MinAllocSize && 
			Size <= MemSpaces[MemSpace_Heap0].MaxAllocSize )
		{
#if FOOTERS
			mstate m = get_mstate_for(mem2chunk(Ptr));
			mstate MemSpacePtr = (mstate)MemSpaces[MemSpace_Heap0].MemSpacePtr;
			check(m == MemSpacePtr);
#endif
			mspace_free(MemSpaces[MemSpace_Heap0].MemSpacePtr,Ptr);
		}
		else
		{
#if FOOTERS
			mstate m = get_mstate_for(mem2chunk(Ptr));			
			check(m == gm);
#endif
			dlfree( Ptr );
		}
	}
}

/** 
 * Realloc
 */
void* FMallocPS3DL::Realloc( void* Ptr, DWORD NewSize, DWORD Alignment )
{
	checkf(Alignment == DEFAULT_ALIGNMENT, TEXT("Alignment with realloc is not supported with FMallocPS3DL"));

	// Realloc(NULL,0) will always return the same valid ptr and won't be freed
#if USE_GLOBAL_REALLOC_ZERO_PTR
	if( !Ptr && !NewSize )
	{
		if( !ReallocZeroPtr )
		{
			ReallocZeroPtr = dlmalloc( NewSize );
		}
		else
		{
			return ReallocZeroPtr;
		}		
	}
	if( Ptr == ReallocZeroPtr )
	{
		if( NewSize )
		{
			Ptr = NULL;			
		}
		else
		{
			return NULL;
		}
	}
#endif

	void* NewPtr = NULL;
	if( Ptr && NewSize > 0 )
	{
		NewPtr = Malloc( NewSize, Alignment );
		DWORD PtrSize = dlmalloc_usable_size(Ptr);
		appMemcpy( NewPtr, Ptr, Min<DWORD>(PtrSize, NewSize));
		Free( Ptr );
	}
	else if( !Ptr )
	{
		NewPtr = Malloc( NewSize, Alignment );
	}
	else
	{
		Free( Ptr );
	}

	return NewPtr;
}

/**
 * Gathers memory allocations for both virtual and physical allocations.
 *
 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
 */
void FMallocPS3DL::GetAllocationInfo( FMemoryAllocationStats& MemStats )
{
	sys_memory_info_t MemInfo;
	sys_memory_get_user_memory_size(&MemInfo);

	MemStats.CPUAvailable = GPS3MemoryAvailableAtBoot;
	MemStats.ImageSize = MemInfo.total_user_memory - MemStats.CPUAvailable;
	MemStats.OSReportedFree = MemInfo.available_user_memory;
	MemStats.OSReportedUsed = MemInfo.total_user_memory - MemInfo.available_user_memory - MemStats.ImageSize;

	for( INT MemSpaceIdx = 0; MemSpaceIdx < ARRAY_COUNT(MemSpaces); MemSpaceIdx++ )
	{
		if( MemSpaces[MemSpaceIdx].MemSpacePtr )
		{
			mallinfo MspaceInfo = mspace_mallinfo(MemSpaces[MemSpaceIdx].MemSpacePtr);
			MemStats.CPUSlack += MspaceInfo.fordblks;
		}
	}
#if STATS
	MemStats.CPUUsed = OsCurrent;

	DOUBLE Waste = ( ( DOUBLE )TotalWaste / ( DOUBLE )TotalAllocs ) * CurrentAllocs;
	MemStats.CPUWaste = ( DWORD )Waste;
#endif // STATS

#if !USE_NULL_RHI
	// keep track of host allocations that came from physical memory instead of heap so that stats match USE_HEAP_HOST_MODEL usage
#if (USE_STATIC_HOST_MODEL || USE_GROW_DYNAMIC_HOST_MODEL)
	for( INT HostMemIdx = 0; HostMemIdx < HostMem_MAX; HostMemIdx++ )
	{
		const EHostMemoryHeapType HostMemType = (EHostMemoryHeapType)HostMemIdx; 
		if( GPS3Gcm && GPS3Gcm->HasHostAllocator(HostMemType) )
		{
			SIZE_T HostVirtualAllocationSize = 0;
			SIZE_T HostPhysicalAllocationSize = 0;
			GPS3Gcm->GetHostAllocator(HostMemType)->GetAllocationInfo(HostVirtualAllocationSize,HostPhysicalAllocationSize);
			MemStats.HostUsed += HostVirtualAllocationSize + HostPhysicalAllocationSize;
			MemStats.HostSlack += GPS3Gcm->GetHostAllocator(HostMemType)->GetHeapSize() - (HostVirtualAllocationSize + HostPhysicalAllocationSize);
		}
	}
#endif

	if( GPS3Gcm )
	{
		FMallocGcmBase *GPUMem = GPS3Gcm->GetLocalAllocator();
		SIZE_T UsedPhysical, UsedVirtual;
		GPUMem->GetAllocationInfo( UsedVirtual, UsedPhysical );
		MemStats.GPUUsed = UsedPhysical;
		MemStats.GPUSlack = GPUMem->GetHeapSize() - UsedPhysical;
		MemStats.GPUWaste = appPS3GetGcmMallocOverheadSize();
	}
#endif // USE_PS3_RHI

	MemStats.TotalUsed = MemStats.CPUUsed + MemStats.GPUUsed + MemStats.HostUsed;
	MemStats.TotalAllocated = MemStats.CPUUsed + MemStats.CPUSlack + MemStats.CPUWaste 
							+ MemStats.GPUUsed + MemStats.GPUSlack + MemStats.GPUWaste 
							+ MemStats.HostUsed + MemStats.HostSlack + MemStats.HostWaste;
}


/**
 * If possible give memory back to the os from unused segments
 *
 * @param ReservePad - amount of space to reserve when trimming
 * @param bShowStats - log stats about how much memory was actually trimmed. Disable this for perf
 * @return TRUE if succeeded
 */ 
UBOOL FMallocPS3DL::TrimMemory(SIZE_T ReservePad, UBOOL bShowStats)
{
	SCOPE_CYCLE_COUNTER(STAT_TrimMemoryTime);

	UBOOL Result=FALSE;

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	if( !bShowStats )
#endif
	{
		// trim default mem space
		Result = dlmalloc_trim(ReservePad);
		// trim each of the mem spaces
		for( INT MemSpaceIdx=0; MemSpaceIdx < ARRAY_COUNT(MemSpaces); MemSpaceIdx++ )
		{
			if( MemSpaces[MemSpaceIdx].MemSpacePtr )
			{
				Result &= mspace_trim(MemSpaces[MemSpaceIdx].MemSpacePtr,ReservePad);
			}
		}
	}
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	else
	{
		ImmediateDebugf(GLog,"TrimMemory Before:");
		DumpAllocations(*GLog);

		SIZE_T StartFootprint = dlmalloc_footprint();
		for( INT MemSpaceIdx=0; MemSpaceIdx < ARRAY_COUNT(MemSpaces); MemSpaceIdx++ )
		{
			if( MemSpaces[MemSpaceIdx].MemSpacePtr )
			{
				StartFootprint += mspace_footprint(MemSpaces[MemSpaceIdx].MemSpacePtr);
			}
		}
		DOUBLE StartTime = appSeconds();

		Result = dlmalloc_trim(ReservePad);
		// trim each of the mem spaces
		for( INT MemSpaceIdx=0; MemSpaceIdx < ARRAY_COUNT(MemSpaces); MemSpaceIdx++ )
		{
			if( MemSpaces[MemSpaceIdx].MemSpacePtr )
			{
				Result &= mspace_trim(MemSpaces[MemSpaceIdx].MemSpacePtr,ReservePad);
			}
		}

		DOUBLE DiffTime = appSeconds() - StartTime;
		SIZE_T EndFootprint = dlmalloc_footprint();
		for( INT MemSpaceIdx=0; MemSpaceIdx < ARRAY_COUNT(MemSpaces); MemSpaceIdx++ )
		{
			if( MemSpaces[MemSpaceIdx].MemSpacePtr )
			{
				EndFootprint += mspace_footprint(MemSpaces[MemSpaceIdx].MemSpacePtr);
			}
		}
		SIZE_T DiffFootprint = StartFootprint - EndFootprint;

		ImmediateDebugf(GLog,"Trimming time %.2f ms", DiffTime*1000.f );
		ImmediateDebugf(GLog,"Trimming reduced footprint by %i b %5.2f kb %5.2f mb", DiffFootprint, DiffFootprint/1024.f, DiffFootprint/1024.f/1024.f );

		ImmediateDebugf(GLog,"TrimMemory After:");
		DumpAllocations(*GLog);

		if( !Result )
		{
			ImmediateDebugf(GLog,"TrimMemory: failed to reduce footprint!");
		}
	}
#endif
	return Result;
}

UBOOL FMallocPS3DL::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	if( ParseCommand(&Cmd,TEXT("dumper")) )
	{
		ps3dumper(NULL);
		for( INT MemSpaceIdx=0; MemSpaceIdx < ARRAY_COUNT(MemSpaces); MemSpaceIdx++ )
		{
			if( MemSpaces[MemSpaceIdx].MemSpacePtr )
			{
				ps3dumper(MemSpaces[MemSpaceIdx].MemSpacePtr);
			}
		} 
		if( ParseToken( Cmd, 0 ) == TEXT("full") )
		{
			print_segments(NULL);
			for( INT MemSpaceIdx=0; MemSpaceIdx < ARRAY_COUNT(MemSpaces); MemSpaceIdx++ )
			{
				if( MemSpaces[MemSpaceIdx].MemSpacePtr )
				{
					print_segments(MemSpaces[MemSpaceIdx].MemSpacePtr);
				}
			}
		}		
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("dumperfile")) )
	{
		const FString SystemTime = appSystemTimeString();
		const FString Filename = FString::Printf(TEXT("..\\MemLeaks\\DlMemCheck-%s-%s.log"),*ParseToken( Cmd, 0 ),*SystemTime);

		TArray<FColor> AllocData;

		const INT EntryHeight = 4;
		// total number of segments (start with default heap)
		INT NumSegments = get_num_segments(NULL) + 1;
		for( INT MemSpaceIdx=0; MemSpaceIdx < ARRAY_COUNT(MemSpaces); MemSpaceIdx++ )
		{
			if( MemSpaces[MemSpaceIdx].MemSpacePtr )
			{
				// add up segments used by each mspace
				NumSegments += get_num_segments(MemSpaces[MemSpaceIdx].MemSpacePtr);
				// for border line
				NumSegments++;
			}
		}
		const INT Height = EntryHeight * NumSegments;
		const INT Width = 64*1024 / 16;

		AllocData.Empty( Height * Width );
		AllocData.AddZeroed( Height * Width );

		INT CurIdx=0;
		for( INT MemSpaceIdx=0; MemSpaceIdx < ARRAY_COUNT(MemSpaces); MemSpaceIdx++ )
		{
			if( MemSpaces[MemSpaceIdx].MemSpacePtr )
			{
				dump_segments_to_bmp( MemSpaces[MemSpaceIdx].MemSpacePtr, AllocData, CurIdx, Width, EntryHeight );
			}
		}
		dump_segments_to_bmp( NULL, AllocData, CurIdx, Width, EntryHeight );
		
		appCreateBitmap( *Filename, Width, Height, AllocData.GetTypedData() );
		return TRUE;
	}
#endif
	return FALSE;
}

void FMallocPS3DL::OutOfMemory( DWORD Size )
{
	if (GPS3MallocNoCrashOnOOM)
	{
		return;
	}

	ImmediateDebugf(GLog, "OOM %d %f %f", Size, Size/1024.0f, Size/1024.0f/1024.0f );
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	appPS3DumpDetailedMemoryInformation(*GLog);
#endif
	appErrorf(TEXT("OOM %d %f %f"), Size, Size/1024.0f, Size/1024.0f/1024.0f);
}

/**
 * Dumps details about all allocations to an output device
 *
 * @param Ar	[in] Output device
 */
void FMallocPS3DL::DumpAllocations( class FOutputDevice& Ar )
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	mallinfo Info = dlmallinfo();
	SIZE_T Footprint = dlmalloc_footprint();
	SIZE_T MaxFootprint = dlmalloc_max_footprint();

	// add info from each of the mem spaces
	for( INT MemSpaceIdx=0; MemSpaceIdx < ARRAY_COUNT(MemSpaces); MemSpaceIdx++ )
	{
		if( MemSpaces[MemSpaceIdx].MemSpacePtr )
		{
			mallinfo MspaceInfo = mspace_mallinfo(MemSpaces[MemSpaceIdx].MemSpacePtr);
			Info.arena += MspaceInfo.arena;
			Info.ordblks += MspaceInfo.ordblks;
			Info.smblks += MspaceInfo.smblks;
			Info.hblks += MspaceInfo.hblks;
			Info.hblkhd += MspaceInfo.hblkhd;
			Info.usmblks += MspaceInfo.usmblks;
			Info.fsmblks += MspaceInfo.fsmblks;
			Info.uordblks += MspaceInfo.uordblks;
			Info.fordblks += MspaceInfo.fordblks;
			Info.keepcost += MspaceInfo.keepcost;

			Footprint += mspace_footprint(MemSpaces[MemSpaceIdx].MemSpacePtr);
			MaxFootprint += mspace_max_footprint(MemSpaces[MemSpaceIdx].MemSpacePtr);
		}
	}

	// @note
	// Info.uordblks == std::malloc_managed_size.current_inuse_size
	// dlmalloc_max_footprint() == std::malloc_managed_size.size_t max_system_size
	// dlmalloc_footprint() == std::malloc_managed_size.current_system_size;

	ImmediateDebugf(&Ar,"");
	ImmediateDebugf(&Ar,"---- dlmalloc stats begin ----");
	ImmediateDebugf(&Ar,"non-mmapped space allocated from system %.2f KB", Info.arena/1024.f);
	ImmediateDebugf(&Ar,"number of free chunks %d", Info.ordblks);
	ImmediateDebugf(&Ar,"space in mmapped regions %.2f KB", Info.hblkhd/1024.f);
	ImmediateDebugf(&Ar,"maximum total allocated space %.2f KB", Info.usmblks/1024.f);
	ImmediateDebugf(&Ar,"total allocated space %.2f KB", Info.uordblks/1024.f);
	ImmediateDebugf(&Ar,"total free space %.2f KB", Info.fordblks/1024.f);
	ImmediateDebugf(&Ar,"releasable (via malloc_trim) space %.2f KB", Info.keepcost/1024.f);
	ImmediateDebugf(&Ar,"** total footprint %.2f [%.2f max] KB", Footprint/1024.f,MaxFootprint/1024.f);
	ImmediateDebugf(&Ar,"---- dlmalloc stats end ----");
	ImmediateDebugf(&Ar,"");

	std::malloc_managed_size MemInfo;
	malloc_stats(&MemInfo);
	ImmediateDebugf(&Ar,"** std::malloc in use %.2f [%.2f footprint] KB", MemInfo.current_inuse_size/1024.f,MemInfo.current_system_size/1024.f);
#endif
}

struct MemOverTimeDatum
{
	DWORD FreeMem;
	DWORD FreeGPU;
	DWORD FreeHost;
	DWORD available_user_memory;
	DWORD MaxContiguousAllocation;
	mallinfo MallocInfo;
	std::malloc_managed_size MemInfo;

	MemOverTimeDatum():
	FreeMem(0)
	, FreeGPU(0)
	, FreeHost(0)
	, available_user_memory(0)
	, MaxContiguousAllocation(0)
	{
	}

	FString ToString()
	{
		FString Retval;

		Retval = FString::Printf( TEXT( "Fragmentation at:  %i b %5.2f kb %5.2f mb  PercentNotFragged: %3.2f  FreeMem: CPU: %5.2f mb  GPU: %5.2f mb  Host: %5.2f mb  AvailUserMem: %5.2f mb" ),  
			MaxContiguousAllocation, MaxContiguousAllocation/1024.0f, MaxContiguousAllocation/1024.0f/1024.0f,
			(MaxContiguousAllocation*1.0f)/(FreeMem*1.0f),
			FreeMem/1024.0f/1024.0f,
			FreeGPU/1024.0f/1024.0f,
			FreeHost/1024.0f/1024.0f,
			available_user_memory/1024.0f/1024.0f
			);

		Retval += FString::Printf( TEXT( "   ") );
		Retval += FString::Printf( TEXT( " non-mmapped space allocated from system: %.2f KB" ), MallocInfo.arena/1024.f);
		Retval += FString::Printf( TEXT( " number of free chunks: %d" ), MallocInfo.ordblks);
		Retval += FString::Printf( TEXT( " space in mmapped regions: %.2f KB" ), MallocInfo.hblkhd/1024.f);
		Retval += FString::Printf( TEXT( " maximum total allocated space: %.2f KB" ), MallocInfo.usmblks/1024.f);
		Retval += FString::Printf( TEXT( " total allocated space: %.2f KB" ), MallocInfo.uordblks/1024.f);
		Retval += FString::Printf( TEXT( " total free space: %.2f KB" ), MallocInfo.fordblks/1024.f);
		Retval += FString::Printf( TEXT( " releasable (via malloc_trim) space: %.2f KB" ), MallocInfo.keepcost/1024.f);
		Retval += FString::Printf( TEXT( " ** total footprint: %.2f [%.2f max] KB" ), dlmalloc_footprint()/1024.f,dlmalloc_max_footprint()/1024.f);

		return Retval;
	}
};

/** 
 * This is used to check our allocator to see if it has issues allocating certain sizes. (e.g. DL malloc had issues when we removed segment merging) *
 * @TODO:  add realloc test to this
 **/
void CheckMemoryAllocations( DWORD InCheckSize, class FOutputDevice& Ar )
{
	// determine the fragmentation level
	const DWORD Alignment = DEFAULT_ALIGNMENT;
	const DWORD ChunkSize = 1; // 1b

	DWORD Size = (DWORD)-1;
	UBOOL bDidWeAllocate = TRUE;
	while( ( bDidWeAllocate == TRUE ) && ( Size <= InCheckSize ) )
	{	
		Size += ChunkSize;

		void* Ptr = NULL;
		if (Alignment != DEFAULT_ALIGNMENT)
		{
			Ptr = dlmemalign( Alignment, Size );
		}
		else
		{
			Ar.Logf( TEXT("CheckMemoryAllocations:  %i b %5.2f kb %5.2f mb") , Size, Size/1024.0f, Size/1024.0f/1024.0f );	
			Ptr = dlmalloc( Size );
		}

		if( Ptr == NULL )
		{
			bDidWeAllocate = FALSE; // set exit condition

			Ar.Logf( TEXT("Failed to allocate at:  %i b %5.2f kb %5.2f mb") , Size, Size/1024.0f, Size/1024.0f/1024.0f );			
		}
		else
		{
			dlfree( Ptr );
		}
	}
}


void FMallocPS3DL::CheckMemoryFragmentationLevel( class FOutputDevice& Ar )
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	//CheckMemoryAllocations( 70000, Ar );
	//return;

	const static UINT MAX_FRAG_ENTRIES = 300;
	static MemOverTimeDatum MemOverTime[MAX_FRAG_ENTRIES];
	static INT CurrFragEntry = -1;

	// update the index into the list (circular so we always have the data)
	if( ++CurrFragEntry >= MAX_FRAG_ENTRIES )
	{
		CurrFragEntry = 0;
	}

	// get the current MemOvertimeDatum
	MemOverTimeDatum& NewMemData = MemOverTime[CurrFragEntry];
    
	TrimMemory(0,TRUE);

	// determine how much free memory we currently have
	appPS3DumpDetailedMemoryInformation( Ar );

	// total OS mem available
	sys_memory_info_t MemInfo;
	sys_memory_get_user_memory_size(&MemInfo);
	Ar.Logf(TEXT("MemInfo total_user_memory=%.2f MB \tavailable_user_memory=%.2f MB"),
		MemInfo.total_user_memory/1024.f/1024.f,
		MemInfo.available_user_memory/1024.f/1024.f);

	NewMemData.available_user_memory = MemInfo.total_user_memory;

	FMemoryAllocationStats MemStats;
	GMalloc->GetAllocationInfo(MemStats);

	NewMemData.MallocInfo = dlmallinfo();
	malloc_stats(&NewMemData.MemInfo);

	// freemem is actually just total os pages available plus total free memory internal to dlmalloc
	NewMemData.FreeMem = MemStats.TotalUsed;
	NewMemData.FreeGPU = MemStats.GPUUsed;
	NewMemData.FreeHost = MemStats.CPUUsed;

	// determine the fragmentation level
	const DWORD Alignment = DEFAULT_ALIGNMENT;
	const DWORD ChunkSize = 1024*32; // 32k
	// only allow default heap allocs
	check(ChunkSize > MemSpaces[MemSpace_Heap0].MaxAllocSize);

	DWORD Size = 0;
	UBOOL bDidWeAllocate = TRUE;
	while( bDidWeAllocate == TRUE )
	{	
		Size += ChunkSize;

		void* Ptr = NULL;
		if (Alignment != DEFAULT_ALIGNMENT)
		{
			Ptr = dlmemalign( Alignment, Size );
		}
		else
		{
			Ptr = dlmalloc( Size );
		}

		if( Ptr == NULL )
		{
			bDidWeAllocate = FALSE; // set exit condition

			NewMemData.MaxContiguousAllocation = Size;

			Ar.Logf( TEXT("Fragmentation at:  %i b %5.2f kb %5.2f mb") , Size, Size/1024.0f, Size/1024.0f/1024.0f );

			// print out all the previous fragmentation levels
			Ar.Logf( TEXT("") );
			Ar.Logf( TEXT("----- all fragmentation runs begin -----") );
			for( UINT i = 0; i < MAX_FRAG_ENTRIES; ++i )
			{
				if( MemOverTime[i].FreeMem != 0 )
				{
					const FString ToLog = FString::Printf( TEXT( "(%d) " ), i ) + MemOverTime[i].ToString();
					Ar.Logf( *ToLog );
				}

			}
			Ar.Logf( TEXT("----- all fragmentation runs end -----") );
			Ar.Logf( TEXT("") );			
		}
		else
		{
			dlfree( Ptr );
		}
	}

#endif
}

void FMallocPS3DL::DoAfterStaticInit()
{
}

#endif // USE_FMALLOC_DL






