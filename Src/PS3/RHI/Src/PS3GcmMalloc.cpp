/*=============================================================================
	FMallocGcm.cpp: Gcm memory allocator that keeps bookeeping outside of heap
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3RHIPrivate.h"
#include "PS3Gcm.h"
#include "PS3GcmMalloc.h"
#include "FMallocPS3.h"
#include "AllocatorFixedSizeFreeList.h"

#if FINAL_RELEASE_DEBUGCONSOLE
#ifdef printf
#undef printf
#endif
#ifdef checkf
#undef checkf
#endif
#define checkf(expr,...) { if( expr ) debugf(...); }
#endif

#if !USE_NULL_RHI
void PrintGPUHostMemoryMap()
{
#if !FINAL_RELEASE
	CellGcmOffsetTable Table;
	cellGcmGetOffsetTable( &Table );
	DWORD NumEntries = 256;
	printf( "PS3GCMMALLOC: GPU Host Memory Map:\n" );
	for ( DWORD Entry=0; Entry < NumEntries; ++Entry )
	{
		if ( Table.eaAddress[Entry] < 0x0fff )
		{
			printf( "PS3GCMMALLOC:  Offset 0x%08x -> Address 0x%08x (1MB)\n", Entry*1024*1024, DWORD(Table.eaAddress[Entry])*1024*1024 );
		}
	}
#endif
}
#endif	//#if !USE_NULL_RHI

static void VARARGS ImmediateDebugf( UBOOL bForcePrint, const TCHAR *Format, ... )
{
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Format, Format );

	// Note: We can't use debugf because it can cause a thread dead-lock if the Logf is locked
	// to another thread who's calling debugf, and the other thread is blocked by this allocator.
	printf( TCHAR_TO_ANSI(TempStr) );
	printf( "\n" );
}

/**
* Map effective address (ea) system memory to io memory offset for use by GPU
*
* @param InBase - system memory base ptr to map
* @param InSize - size of system memory to map
* @param OutGPUHostMemoryOffset - [out] io address offset used by GPU
* @param bAllowSortIoTable - if TRUE then cellGcmSortRemapEaIoAddress is called when cellGcmMapMainMemory returns CELL_GCM_ERROR_NO_IO_PAGE_TABLE
* @return TRUE if success
*/
static FORCEINLINE UBOOL MapBaseMemoryToGPU( void* InBase, SIZE_T InSize, DWORD& OutGPUHostMemmoryOffset, UBOOL bAllowSortIoTable )
{
#if !USE_NULL_RHI
	INT Ret = cellGcmMapMainMemory( InBase, InSize, (UINT*)&OutGPUHostMemmoryOffset );
	if( bAllowSortIoTable && Ret == CELL_GCM_ERROR_NO_IO_PAGE_TABLE )
	{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		printf( "\n---- MapBaseMemoryToGPU begin ---\n" );
		PrintGPUHostMemoryMap();
#endif	

		Ret = cellGcmSortRemapEaIoAddress();
		Ret = cellGcmMapMainMemory( InBase, InSize, (UINT*)&OutGPUHostMemmoryOffset );

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		PrintGPUHostMemoryMap();
		printf( "\n---- MapBaseMemoryToGPU end ---\n\n" );
#endif
	}
	checkf( Ret == CELL_OK, TEXT("Failed to map host memory to GPU. sys error=[%d]"), Ret );
	return( Ret == CELL_OK );
#else	//#if !USE_NULL_RHI
	return FALSE;
#endif	//#if !USE_NULL_RHI
}

/**
* Unmap effective address (ea) system memory which has already been mapped to io memory offset for use by GPU
*
* @param GPUHostMemoryOffset - io address offset used by GPU to unmap
* @return TRUE if success
*/
static FORCEINLINE UBOOL UnMapBaseMemoryFromGPU( DWORD GPUHostMemmoryOffset )
{
#if !USE_NULL_RHI
	INT Ret = cellGcmUnmapIoAddress( GPUHostMemmoryOffset );
	checkf( Ret == CELL_OK, TEXT("Failed to unmap host memory from GPU. sys error=[%d]"), Ret );
	return( Ret == CELL_OK );
#else
	return FALSE;
#endif
}


/*-----------------------------------------------------------------------------
FMallocGcm
-----------------------------------------------------------------------------*/

#define DETAILED_GPU_MEM_TRACKING !FINAL_RELEASE

#if DETAILED_GPU_MEM_TRACKING
struct FAllocationInfo
{
	/**
	* Constructor
	*/
	FAllocationInfo()
	{

	}

	FAllocationInfo(DWORD InSize, DWORD InPadBytes, EAllocationType InAllocationType)
	: Size(InSize)
	, PadBytes(InPadBytes)
	, AllocationType(InAllocationType)
	, AllocationTime(appSeconds())
	{

	}

	/** Size of the allocation */
	DWORD Size;

	/** Number of pad bytes before the allocation */
	DWORD PadBytes;

	/** The type of allocation */
	EAllocationType AllocationType;

	/** What time the allocation happened */
	DOUBLE AllocationTime; 
};

#define TRACK_ALLOCATION(Ptr, Size, Padding, AllocationType) \
	AllocatedPointers.Set(Ptr, FAllocationInfo(Size, Padding, AllocationType));

#define GET_ALLOCATION_INFO(Ptr, Size, Padding) \
	{ \
		FAllocationInfo* Info = AllocatedPointers.Find(Ptr); \
		if (Info == NULL) \
		{ \
			printf("Tried to free an unknown pointer from GPU memory: %x\n", Ptr); \
			return; \
		} \
		check(Info); \
		Size = Info->Size; \
		Padding = Info->PadBytes; \
	}

// Each FMallocGcm should probably have their own map, to allow for different address spaces.
TMap<void*, FAllocationInfo> AllocatedPointers;

#else

// if we aren't storing detailed info, then we only store a QWORD into the AllocatedPointers map
typedef QWORD FAllocationInfo;

#define TRACK_ALLOCATION(Ptr, Size, PadBytes, AllocationType) \
	AllocatedPointers.Set(Ptr, QWORD(Size) | (PadBytes << 32));

#define GET_ALLOCATION_INFO(Ptr, Size, PadBytes) \
	{ \
		QWORD* Info = AllocatedPointers.Find(Ptr); \
		if (Info == NULL) \
		{ \
			printf("Tried to free an unknown pointer from GPU memory: %x\n", Ptr); \
			return; \
		} \
		check(Info); \
		Size = DWORD((*Info) & 0xffffffffull); \
		PadBytes = DWORD((*Info) >> 32); \
	}

// Each FMallocGcm should probably have their own map, to allow for different address spaces.
// High 32-bits:	Number of pad bytes before the returned pointer
// Low 32-bits:		Number of bytes used
TMap<void*, QWORD> AllocatedPointers;

#endif

/** Shared critical section object to protect the heap array from use on multiple threads */
FCriticalSection GMallocGcmCriticalSection;

/**
 * Constructor
 * @param InSize	The size of the total heap. If Base is NULL, the heap will be allocated to this size
 * @param InBase	Optional location of the preallocated buffer to use as the heap (must be at least Size big)
 */
FMallocGcm::FMallocGcm(SIZE_T InSize, void* InBase)
: HeapBase((BYTE*)InBase)
, HeapSize(InSize)
{
	// always has a region allocated for it already
	check(HeapBase != NULL);

	// whole heap is free!
	FreeList = new FFreeEntry(NULL, HeapBase, HeapSize);

	UsedMemorySize = 0;
}

/**
 * Destructor 
 */
FMallocGcm::~FMallocGcm()
{
	// assuming any memory allocated externally gets freed
	check(UsedMemorySize==0);
}

/** 
 * Constructor
 */
FMallocGcm::FFreeEntry::FFreeEntry(FFreeEntry *NextEntry, BYTE* InLocation, SIZE_T InSize)
:	Location(InLocation)
,	BlockSize(InSize)
,	Next(NextEntry)
{
}

#if USE_ALLOCATORFIXEDSIZEFREELIST
TAllocatorFixedSizeFreeList<sizeof(FMallocGcm::FFreeEntry), 20> GGcmFreeEntryFixedSizePool(200);
void* FMallocGcm::FFreeEntry::operator new(size_t Size)
{
	checkSlow(Size == sizeof(FMallocGcm::FFreeEntry));
	return GGcmFreeEntryFixedSizePool.Allocate();
}
void FMallocGcm::FFreeEntry::operator delete(void *RawMemory)
{
	GGcmFreeEntryFixedSizePool.Free(RawMemory);
}
#endif

/**
 * Determine if the given allocation with this alignment and size will fit
 * @param AllocationSize	Already aligned size of an allocation
 * @param Alignment			Alignment of the allocation (location may need to increase to match alignment)
 * @return TRUE if the allocation will fit
 */
UBOOL FMallocGcm::FFreeEntry::CanFit(SIZE_T AllocationSize, DWORD Alignment)
{
	// location of the aligned allocation
	BYTE* AlignedLocation = Align<BYTE*>(Location, Alignment);

	// if we fit even after moving up the location for alignment, then we are good
	return (AllocationSize + (AlignedLocation - Location)) <= BlockSize;
}

/**
 * Take a free chunk, and split it into a used chunk and a free chunk
 * @param UsedSize	The size of the used amount (anything left over becomes free chunk)
 * @param Alignment	The alignment of the free block
 * @param AllocationType The type of allocation
 * @param bDelete Whether or not to delete this FreeEntry (ie no more is left over after splitting)
 * 
 * @return The pointer to the free data
 */
BYTE* FMallocGcm::FFreeEntry::Split( SIZE_T UsedSize, DWORD Alignment, EAllocationType AllocationType, UBOOL &bDelete )
{
	// make sure we are already aligned
	check((UsedSize & (Alignment - 1)) == 0);

	// this is the pointer to the free data
	BYTE* FreePtr = Align<BYTE*>( Location, Alignment );

	// Adjust the allocated size for any alignment padding
	QWORD Padding = QWORD(FreePtr - Location);
	SIZE_T AllocationSize = UsedSize + SIZE_T(Padding);

	// see if there's enough space left over for a new free chunk (of at least default alignment size)
	if (BlockSize - AllocationSize >= DEFAULT_ALIGNMENT)
	{
		// update this free entry to just point to what's left after using the UsedSize
		Location += AllocationSize;
		BlockSize -= AllocationSize;
		bDelete = FALSE;
	}
	// if no more room, then just remove this entry from the list of free items
	else
	{
		bDelete = TRUE;
	}

	// Check to see that it's not already in the map (all FMallocGcm heaps share the same address space)
	check( AllocatedPointers.Find(FreePtr) == NULL );

	// remember this allocation
	TRACK_ALLOCATION(FreePtr, UsedSize, Padding, AllocationType)

	// return a usable pointer!
	return FreePtr;
}

void* FMallocGcm::Malloc(DWORD Size, EAllocationType AllocationType, DWORD Alignment)
{
	// multi-thread protection
	FScopeLock ScopeLock(&GMallocGcmCriticalSection);

	check(AllocationType >= 0 && AllocationType < AT_MAXValue);

	// Alignment here is assumed to be for location and size
	Size = Align<DWORD>(Size, Alignment);

	// look for a good free chunk
	FFreeEntry* Prev = NULL;
	for (FFreeEntry *Entry = FreeList; Entry; Entry=Entry->Next)
	{
		if (Entry->CanFit(Size, Alignment))
		{
			// Use it, leaving over any unused space
			UsedMemorySize += Size;
			UBOOL bDelete;
			BYTE* Ptr = Entry->Split(Size, Alignment, AllocationType, bDelete);
			if ( bDelete )
			{
				FFreeEntry*& PrevRef = Prev ? Prev->Next : FreeList;
				PrevRef = Entry->Next;
				delete Entry;
			}
			return Ptr;
		}
		Prev = Entry;
	}

	// if no suitable blocks were found, we must fail
	ImmediateDebugf(TRUE, TEXT("Failed to allocate GPU memory (Size: %d, Allocation Type: %d)"), Size, AllocationType);
	return NULL;
}

void FMallocGcm::Free(void* Ptr)
{
	if ( Ptr == NULL )
	{
		return;
	}

	// multi-thread protection
	FScopeLock ScopeLock(&GMallocGcmCriticalSection);

	check(Ptr >= HeapBase && Ptr < HeapBase + HeapSize);

	// Bleh, this causes 2 lookups. Remove() should be changed so that it can also return the value. :(
	DWORD Size, Padding;
	GET_ALLOCATION_INFO(Ptr, Size, Padding);
	DWORD AllocationSize = Size + Padding;
	AllocatedPointers.Remove(Ptr);

	UsedMemorySize -= Size;

	// Search for where a place to insert a new free entry.
	FFreeEntry* Prev = NULL;
	FFreeEntry* Entry = FreeList;
	while ( Entry && Ptr > Entry->Location )
	{
		Prev = Entry;
		Entry = Entry->Next;
	}

	// Are we right before this free entry?
	if ( Entry && ((BYTE*)Ptr + Size) == Entry->Location )
	{
		// Join with chunk
		Entry->Location -= AllocationSize;
		Entry->BlockSize += AllocationSize;

		// Can we join the two entries?
		if ( Prev && (Prev->Location + Prev->BlockSize) == Entry->Location )
		{
			Prev->BlockSize += Entry->BlockSize;
			Prev->Next = Entry->Next;
			delete Entry;
		}
		return;
	}

	// Are we right after the previous free entry?
	if ( Prev && (Prev->Location + Prev->BlockSize + Padding) == (BYTE*)Ptr )
	{
		// Join with chunk
		Prev->BlockSize += AllocationSize;

		// Can we join the two entries?
		if ( Entry && (Prev->Location + Prev->BlockSize) == Entry->Location )
		{
			Prev->BlockSize += Entry->BlockSize;
			Prev->Next = Entry->Next;
			delete Entry;
		}
		return;
	}

	// Insert a new entry.
	FFreeEntry* NewFree = new FFreeEntry(Entry, ((BYTE*)Ptr) - Padding, AllocationSize);
	FFreeEntry*& PrevRef = Prev ? Prev->Next : FreeList;
	PrevRef = NewFree;
}

void* FMallocGcm::Realloc(void* Ptr, DWORD NewSize, EAllocationType AllocationType, DWORD Alignment)
{
	// multi-thread protection
	FScopeLock ScopeLock(&GMallocGcmCriticalSection);

	check(AllocationType >= 0 && AllocationType < AT_MAXValue);

	//@TODO: Implement this
	appErrorf(TEXT("FMallocGcm::Realloc is unsupported, as it's not expected to be called. It is now being called, so support will be needed."));
	return NULL;
}

/**
 * Gathers memory allocations for both virtual and physical allocations.
 *
 * @param Virtual	[out] size of virtual allocations
 * @param Physical	[out] size of physical allocations	
 */
void FMallocGcm::GetAllocationInfo( SIZE_T& Virtual, SIZE_T& Physical )
{
	Virtual		= 0;
	Physical	= UsedMemorySize;
}

/**
 * Retrieves the amount used per allocation type in the array
 * @param Usage Array of infos that will have the amount allocated, MUST be AT_MAXValue big!
 */
void GetAllocationInfoByType(FAllocationInfo* Usage, BYTE* Base, DWORD HeapSize)
{
	appMemset(Usage, 0, sizeof(FAllocationInfo) * AT_MAXValue);

#if DETAILED_GPU_MEM_TRACKING
	// go voer all the allocations
	for (TMap<void*, FAllocationInfo>::TIterator It(AllocatedPointers); It; ++It)
	{
		// make sure the allocation is in this heap
		if ((DWORD)It.Key() >= (DWORD)Base && (DWORD)It.Key() < (DWORD)Base + HeapSize)
		{
			// cache it
			FAllocationInfo& Info = It.Value();

			// add em up
			Usage[Info.AllocationType].Size += Info.Size;
			Usage[Info.AllocationType].PadBytes += Info.PadBytes;
			Usage[Info.AllocationType].AllocationTime += (appSeconds() - Info.AllocationTime);
			// hijack the type for the count
			Usage[Info.AllocationType].AllocationType = (EAllocationType)(Usage[Info.AllocationType].AllocationType + 1);
		}
	}
#endif
}

/**
* Updates memory stats using this heap. 
* @param FirstStat ID of the first stat that matches up to the first EAllocationType 
*/
void FMallocGcm::UpdateMemStats(EMemoryStats FirstStat)
{
#if DETAILED_GPU_MEM_TRACKING
	// multi-thread protection
	FScopeLock ScopeLock(&GMallocGcmCriticalSection);

	// get memory usage
	FAllocationInfo Totals[AT_MAXValue];
	GetAllocationInfoByType(Totals, HeapBase, HeapSize);

	// update stats
	for (INT StatIndex = 0; StatIndex < AT_MAXValue; StatIndex++)
	{
		INC_DWORD_STAT_BY(FirstStat + StatIndex, Totals[StatIndex].Size);
	}
#endif
}

static const FLOAT Alpha=0.8f;
#if DETAILED_GPU_MEM_TRACKING
static const FLinearColor DisplayColors[] =
{
	FLinearColor(0,0,1.f,Alpha),		//TEXT("Command Buffer"), 
	FLinearColor(0,1.f,0,Alpha),		//TEXT("Frame Buffer"), 
	FLinearColor(0,1.f,1.f,Alpha),		//TEXT("Depth Buffer"), 
	FLinearColor(1.f,0,0,Alpha),		//TEXT("Render Target"), 
	FLinearColor(1.f,0,1.f,Alpha),		//TEXT("Texture"), 
	FLinearColor(1.f,1.f,0,Alpha),		//TEXT("Vertex Shader"), 
	FLinearColor(1.f,1.f,1.f,Alpha),	//TEXT("Pixel Shader"), 
	FLinearColor(0,0,0.4f,Alpha),		//TEXT("Vertex Buffer"), 
	FLinearColor(0,0.4f,0,Alpha),		//TEXT("Index Buffer"), 
	FLinearColor(0,0.4f,0.4f,Alpha),	//TEXT("Ring Buffer"), 
	FLinearColor(0.4f,0,0,Alpha),		//TEXT("Compression Tag"), 
	FLinearColor(0.4f,0,0.4f,Alpha),	//TEXT("Resource Array"), 
	FLinearColor(0.4f,0.4f,0,Alpha)		//TEXT("Occlusion Queries")
};
static TCHAR* DisplayNames[] =
{
	TEXT("Command Buffer"), 
	TEXT("Frame Buffer"), 
	TEXT("Depth Buffer"), 
	TEXT("Render Target"), 
	TEXT("Texture"), 
	TEXT("Vertex Shader"), 
	TEXT("Pixel Shader"), 
	TEXT("Vertex Buffer"), 
	TEXT("Index Buffer"), 
	TEXT("Ring Buffer"), 
	TEXT("Compression Tag"), 
	TEXT("Resource Array"), 
	TEXT("Occlusion Queries")
};
#endif

static void DebugDrawKey(class FCanvas* Canvas,FLOAT& X,FLOAT& Y)
{
#if DETAILED_GPU_MEM_TRACKING
	if( GEngine && GEngine->TinyFont )
	{
		// draw color key text
		const FLOAT TextHeight = GEngine->TinyFont->GetMaxCharHeight()*0.75f;
		for( INT Type=0; Type < AT_MAXValue; Type++ )
		{
			DrawTile(Canvas,X,Y,TextHeight,TextHeight,0,0,1,1,DisplayColors[Type]);
			DrawString(Canvas,X+TextHeight,Y,DisplayNames[Type],GEngine ? GEngine->TinyFont : NULL,DisplayColors[Type]);
			Y += TextHeight;
		}
	}	
#endif
}

/**
 * Print out memory information
 * @param Desc				Descriptive name for the heap
 * @param bShowIndividual	If TRUE, each allocation will be printed, if FALSE, a summary of each type will be printed
 */
void FMallocGcm::Dump(const TCHAR* Desc, UBOOL bShowIndividual, UBOOL bForcePrint/*=FALSE*/)
{
	// multi-thread protection
	FScopeLock ScopeLock(&GMallocGcmCriticalSection);

	ImmediateDebugf(bForcePrint, TEXT("Dumping GCM Heap: %s"), Desc);
	ImmediateDebugf(bForcePrint, TEXT("Used %.2fM / %.2fM"), (FLOAT)UsedMemorySize / 1024.0f / 1024.0f, (FLOAT)HeapSize / 1024.0f / 1024.f);

#if DETAILED_GPU_MEM_TRACKING
	ImmediateDebugf(bForcePrint, TEXT("Allocations:"));

	// buffer to track each type
	FAllocationInfo Totals[AT_MAXValue];
	if (bShowIndividual)
	{
		// go voer all the allocations
		for (TMap<void*, FAllocationInfo>::TIterator It(AllocatedPointers); It; ++It)
		{
			// make sure the allocation is in this heap
			if ((DWORD)It.Key() >= (DWORD)HeapBase && (DWORD)It.Key() < (DWORD)HeapBase + HeapSize)
			{
				// cache it
				FAllocationInfo& Info = It.Value();

				ImmediateDebugf(bForcePrint, TEXT("   %s: 0x%08x %.2fKB Used, %dB Padding, %.2f seconds old"), 
					Info.AllocationType < AT_MAXValue ? DisplayNames[Info.AllocationType] : TEXT("Unknown"),
					DWORD(It.Key()),
					(FLOAT)Info.Size / 1024.0f,
					Info.PadBytes,
					appSeconds() - Info.AllocationTime);
			}
		}
	}
	else
	{
		GetAllocationInfoByType(Totals, HeapBase, HeapSize);

		// print summary if desired
		for (INT Type = 0; Type < AT_MAXValue; Type++)
		{
			FAllocationInfo& Info = Totals[Type];
			// show types that have some
			if (Info.Size > 0)
			{
				ImmediateDebugf(bForcePrint, TEXT("   %s: %.3fM, %.2fK Padding, %.2f Average age"),
					Type < AT_MAXValue ? DisplayNames[Type] : TEXT("Unknown"),
					(FLOAT)Info.Size / 1024.0f / 1024.0f,
					(FLOAT)Info.PadBytes / 1024.0f,
					Info.AllocationTime / Info.AllocationType // average out the age
					);
			}
		}
	}
	ImmediateDebugf(bForcePrint, TEXT(""));
#endif
}

/**
 * Print out memory information
 * @param Desc				Descriptive name for the heap
 * @param bShowIndividual	If TRUE, each allocation will be printed, if FALSE, a summary of each type will be printed
 * @param Ar				The archive to log to...
 */
void FMallocGcm::DumpToArchive(const TCHAR* Desc, UBOOL bShowIndividual, FOutputDevice& Ar)
{
	// multi-thread protection
	FScopeLock ScopeLock(&GMallocGcmCriticalSection);

	Ar.Logf(TEXT("Dumping GCM Heap: %s"), Desc);
	Ar.Logf(TEXT("Used %.2fM / %.2fM"), (FLOAT)UsedMemorySize / 1024.0f / 1024.0f, (FLOAT)HeapSize / 1024.0f / 1024.f);

#if DETAILED_GPU_MEM_TRACKING
	Ar.Logf(TEXT("Allocations:"));

	// buffer to track each type
	FAllocationInfo Totals[AT_MAXValue];
	if (bShowIndividual)
	{
		// go voer all the allocations
		for (TMap<void*, FAllocationInfo>::TIterator It(AllocatedPointers); It; ++It)
		{
			// make sure the allocation is in this heap
			if ((DWORD)It.Key() >= (DWORD)HeapBase && (DWORD)It.Key() < (DWORD)HeapBase + HeapSize)
			{
				// cache it
				FAllocationInfo& Info = It.Value();

				Ar.Logf(TEXT("   %s: 0x%08x %.2fKB Used, %dB Padding, %.2f seconds old"), 
					Info.AllocationType < AT_MAXValue ? DisplayNames[Info.AllocationType] : TEXT("Unknown"),
					DWORD(It.Key()),
					(FLOAT)Info.Size / 1024.0f,
					Info.PadBytes,
					appSeconds() - Info.AllocationTime);
			}
		}
	}
	else
	{
		GetAllocationInfoByType(Totals, HeapBase, HeapSize);

		// print summary if desired
		for (INT Type = 0; Type < AT_MAXValue; Type++)
		{
			FAllocationInfo& Info = Totals[Type];
			// show types that have some
			if (Info.Size > 0)
			{
				Ar.Logf(TEXT("   %s: %.3fM, %.2fK Padding, %.2f Average age"),
					Type < AT_MAXValue ? DisplayNames[Type] : TEXT("Unknown"),
					(FLOAT)Info.Size / 1024.0f / 1024.0f,
					(FLOAT)Info.PadBytes / 1024.0f,
					Info.AllocationTime / Info.AllocationType // average out the age
					);
			}
		}
	}
	Ar.Logf(TEXT(""));
#endif
}

/** render free/used blocks from this malloc gcm region */
void FMallocGcm::DepugDraw(class FCanvas* Canvas, FLOAT& X,FLOAT& Y,FLOAT SizeX,FLOAT SizeY, const TCHAR* Desc,UBOOL bDrawKey)
{	
	const FLinearColor BkgColor(1.0f,1.0f,1.0f,Alpha);
	const FLinearColor OutlineColor(0.f,0.f,0.f,1.f);

#if !DETAILED_GPU_MEM_TRACKING
	// draw background
	DrawTile(Canvas,
		X,Y,SizeX,SizeY,
		0,0,1,1,
		BkgColor);
#endif
	// iterate over entries and draw free regions
	FFreeEntry* Entry = FreeList;
	while( Entry )
	{
		FLOAT EntryOffset = Max<FLOAT>((FLOAT)(Entry->Location - HeapBase),0);
		FLOAT EntrySize = (FLOAT)Entry->BlockSize;		
		FLOAT EntryX = X + SizeX*(EntryOffset/HeapSize);
		FLOAT EntryY = Y;
		FLOAT EntrySizeX = SizeX*(EntrySize/HeapSize);
		FLOAT EntrySizeY = SizeY;
		// free region
		DrawTile(Canvas,
			EntryX, EntryY, EntrySizeX, EntrySizeY,
			0,0,1,1,
			BkgColor*.2f);
		// cap free region with lines
		DrawLine2D(Canvas,FVector2D(EntryX,EntryY),FVector2D(EntryX,EntryY+EntrySizeY),BkgColor*.05f);
		DrawLine2D(Canvas,FVector2D(EntryX+EntrySizeX,EntryY),FVector2D(EntryX+EntrySizeX,EntryY+EntrySizeY),BkgColor*.05f);
		Entry = Entry->Next;
	}
#if DETAILED_GPU_MEM_TRACKING
	// show allocated entries
	for (TMap<void*, FAllocationInfo>::TIterator It(AllocatedPointers); It; ++It)
	{
		// make sure the allocation is in this heap
		if ((DWORD)It.Key() >= (DWORD)HeapBase && (DWORD)It.Key() < (DWORD)HeapBase + HeapSize)
		{
			FAllocationInfo& Info = It.Value();
			FLOAT EntryOffset = Max<FLOAT>((FLOAT)((BYTE*)It.Key() - HeapBase),0);
			FLOAT EntrySize = (FLOAT)Info.Size;		
			FLOAT EntryX = X + SizeX*(EntryOffset/HeapSize);
			FLOAT EntryY = Y;
			FLOAT EntrySizeX = SizeX*(EntrySize/HeapSize);
			FLOAT EntrySizeY = SizeY;
			DrawTile(Canvas,
				EntryX, EntryY, EntrySizeX, EntrySizeY,
				0,0,1,1,
				DisplayColors[Info.AllocationType]);
			DrawLine2D(Canvas,FVector2D(EntryX,EntryY),FVector2D(EntryX,EntryY+EntrySizeY),DisplayColors[Info.AllocationType]*.5f);
			DrawLine2D(Canvas,FVector2D(EntryX+EntrySizeX,EntryY),FVector2D(EntryX+EntrySizeX,EntryY+EntrySizeY),DisplayColors[Info.AllocationType]*.5f);
		}
	}
#endif
	// draw border outline	
	DrawLine2D(Canvas,FVector2D(X,Y),FVector2D(X+SizeX,Y),OutlineColor);				//top
	DrawLine2D(Canvas,FVector2D(X,Y),FVector2D(X,Y+SizeY),OutlineColor);				//left
	DrawLine2D(Canvas,FVector2D(X+SizeX,Y),FVector2D(X+SizeX,Y+SizeY),OutlineColor);	//right
	DrawLine2D(Canvas,FVector2D(X,Y+SizeY),FVector2D(X+SizeX,Y+SizeY),OutlineColor);	//bottom
	// overlay text
	DrawString(Canvas,
		X,Y,
		*FString::Printf(TEXT("%s Used %.2f KB [%.2f KB]"),Desc ? Desc : TEXT(""),UsedMemorySize/1024.f,HeapSize/1024.f),
		GEngine ? GEngine->TinyFont : NULL,
		OutlineColor);	
	// draw key
	if( bDrawKey )
	{
		Y += SizeY;
		DebugDrawKey(Canvas,X,Y);
	}
}

/**
* @return the amount of memory the bookkeeping information is taking for 
* FMallocGcm
*/
DWORD appPS3GetGcmMallocOverheadSize()
{
	return AllocatedPointers.GetAllocatedSize();
}

/*-----------------------------------------------------------------------------
FMallocGcmHostStatic
-----------------------------------------------------------------------------*/

/**
* Constructor
*
* @param InSize - total size of the heap
* @param InBase - start address of memory block for heap (1MB align required)
*/
FMallocGcmHostStatic::FMallocGcmHostStatic(SIZE_T InSize, void* InBase)
: FMallocGcm(InSize, InBase)
{
}

/**
* Destructor
*/
FMallocGcmHostStatic::~FMallocGcmHostStatic()
{
	appErrorf(TEXT("FMallocGcmHostStatic: Destroying a host heap is not supported yet."));

	// need to make sure externally allocated memory gets freed
}

/**
* Map all memory (Base+Size) for use by GPU
*/
void FMallocGcmHostStatic::MapMemoryToGPU()
{
	MapBaseMemoryToGPU(HeapBase,HeapSize,GPUHostMemmoryOffset,TRUE);	
}

/*-----------------------------------------------------------------------------
FMallocGcmHostHeap
-----------------------------------------------------------------------------*/

/**
* Constructor
*
* @param InSize - total size of the heap
* @param InBase - start address of memory block for heap (1MB align required)
*/
FMallocGcmHostHeap::FMallocGcmHostHeap(SIZE_T InSize, void* InBase)
: FMallocGcm(InSize, InBase)
, TotalAllocated(0)
{
	// allocate from default malloc
	if( HeapBase == NULL )
	{
		// allocate in 1 meg blocks
		HeapSize = Align<SIZE_T>(HeapSize, 1024 * 1024);

		// align the base to 1 meg
		HeapBase = (BYTE*)appMalloc(HeapSize, 1024 * 1024);
	}
}

/**
* Destructor
*/
FMallocGcmHostHeap::~FMallocGcmHostHeap()
{
	appErrorf(TEXT("FMallocGcmHostHeap: Destroying a host heap is not supported yet."));

	// need to make sure all appMalloc allocations have been freed
}

struct FLinkList
{
	void*		MemPtr;
	FLinkList*	Next;
};

void* FMallocGcmHostHeap::Malloc(DWORD Size, EAllocationType AllocationType, DWORD Alignment)
{
	void* Ptr = appMalloc(Size, Alignment);
	void* PtrEnd = ((BYTE*)Ptr) + Size;
	void* MemEnd = ((BYTE*)HeapBase) + HeapSize;

	// If not within RSX-addressable space, try again until we get a good pointer.
	FLinkList* Head = NULL;
	while ( Ptr < HeapBase || PtrEnd > MemEnd )
	{
		FLinkList* NewLink = (FLinkList*) appMalloc( sizeof(FLinkList) );
		NewLink->MemPtr = Ptr;
		NewLink->Next = Head;
		Head = NewLink;
		Ptr = appMalloc(Size, Alignment);
		PtrEnd = ((BYTE*)Ptr) + Size;
		checkf( Ptr, TEXT("OUT OF MEMORY") );	// This checkf may be unnecessary (already done in GMalloc?)
	}

	// Now free the memory we couldn't use.
	while ( Head )
	{
		appFree( Head->MemPtr );
		FLinkList* NewLink = Head->Next;
		appFree( Head );
		Head = NewLink;
	}

#if DETAILED_GPU_MEM_TRACKING
	{
		// multi-thread protection
		FScopeLock ScopeLock(&GMallocGcmCriticalSection);

		// remember this allocation
		INT AllocationSize = malloc_usable_size(Ptr);
		TRACK_ALLOCATION(Ptr, AllocationSize, AllocationSize - Size, AllocationType);
	}
#endif

	INT UsedSize = malloc_usable_size(Ptr);
	appInterlockedAdd(&TotalAllocated,UsedSize);

	return Ptr;
}

void FMallocGcmHostHeap::Free(void* Ptr)
{
#if DETAILED_GPU_MEM_TRACKING
	// remove this from list of allocated pointers
	AllocatedPointers.Remove(Ptr);
#endif

	INT UsedSize = malloc_usable_size(Ptr);
	appInterlockedAdd(&TotalAllocated,-UsedSize);

	appFree(Ptr);
}

void* FMallocGcmHostHeap::Realloc(void* Ptr, DWORD NewSize, EAllocationType AllocationType, DWORD Alignment)
{
	check(0);
	return NULL;
}

/**
* Map all memory (Base+Size) for use by GPU
*/
void FMallocGcmHostHeap::MapMemoryToGPU()
{
	MapBaseMemoryToGPU(HeapBase,HeapSize,GPUHostMemmoryOffset,TRUE);
}

/**
* Gathers memory allocations for both virtual and physical allocations.
*
* @param Virtual	[out] size of virtual allocations
* @param Physical	[out] size of physical allocations	
*/
void FMallocGcmHostHeap::GetAllocationInfo(SIZE_T& Virtual, SIZE_T& Physical)
{
	Virtual = 0;
	Physical = TotalAllocated;
}

/*-----------------------------------------------------------------------------
FMallocGcmHostGrowable
-----------------------------------------------------------------------------*/

#define DEBUG_LOGS 0

// global shared instance
FCriticalSection FMallocGcmHostGrowable::CriticalSection;

/**
* Chunk used for allocations within a region of the FMallocGcmHostGrowable address space
*/
class FMallocGcmAllocChunk : public FMallocGcm
{
public:
	/**
	* Constructor
	* @param InSize - size of this chunk
	* @param InBase - base address for this chunk
	* @param InGPUHostMemoryOffset - io address needed when the chunk's memory has to be unmapped
	*/
	FMallocGcmAllocChunk(SIZE_T InSize, void* InBase, DWORD InGPUHostMemoryOffset)
		:	FMallocGcm(InSize,InBase)
		,	GPUHostMemoryOffset(InGPUHostMemoryOffset)
	{		
	}
	/**
	* Destructor
	*/
	virtual ~FMallocGcmAllocChunk()
	{
		checkf(IsEmpty(),TEXT("Chunk memory not freed!"));
	}
	/**
	* Check free list for an entry big enough to fit the requested Size with Alignment
	* @param Size - allocation size
	* @param Alignment - allocation alignment
	* @return TRUE if available entry was found
	*/
	UBOOL CanFitEntry(DWORD Size,DWORD Alignment)
	{
		UBOOL bResult=FALSE;
		// look for a good free chunk
		for( FFreeEntry *Entry = FreeList; Entry; Entry=Entry->Next )
		{
			if( Entry->CanFit(Size, Alignment) )
			{
				bResult = TRUE;
				break;
			}
		}
		return bResult;
	}
	/**
	* @return TRUE if this chunk has no used memory
	*/
	UBOOL IsEmpty()
	{
		return UsedMemorySize == 0;
	}

	/** io offsets on GPU corresponding to each mapped sys memory allocation for this chunk */
	DWORD GPUHostMemoryOffset;

	// needed to access FMallocGcm members from FMallocGcmHostGrowable
	friend class FMallocGcmHostGrowable;
};

/**
* @return convert EPhysicalMemPageSize to system page size value
*/
static FORCEINLINE DWORD GetSysPageSize(FMallocGcmHostGrowable::EPhysicalMemPageSize PageSize)
{
	switch(PageSize)
	{
	case FMallocGcmHostGrowable::PHYS_PAGE_1MB:
		return SYS_MEMORY_PAGE_SIZE_1M;
		break;
	case FMallocGcmHostGrowable::PHYS_PAGE_64KB:
		return SYS_MEMORY_PAGE_SIZE_64K;
		break;
	default:
		checkf(0,TEXT("Unsupported page size"));
		return 0;
	}
}

/**
* @return convert EPhysicalMemPageSize to system granularity size value
*/
static FORCEINLINE DWORD GetSysGranularitySize(FMallocGcmHostGrowable::EPhysicalMemPageSize PageSize)
{
	switch(PageSize)
	{
	case FMallocGcmHostGrowable::PHYS_PAGE_1MB:
		return SYS_MEMORY_GRANULARITY_1M;
		break;
	case FMallocGcmHostGrowable::PHYS_PAGE_64KB:
		return SYS_MEMORY_GRANULARITY_64K;
		break;
	default:
		checkf(0,TEXT("Unsupported granularity size"));
		return 0;
	}
}

/**
* Constructor
* Internally allocates address space for use only by this allocator
*
* @param InMinSizeAllowed - min reserved space by this allocator
* @param InMaxSizeAllowed - total size of allocations won't exceed this limit
* @param InPageSize - physical memory allocs are done in increments of this page size
*/
FMallocGcmHostGrowable::FMallocGcmHostGrowable(SIZE_T InMinSizeAllowed,SIZE_T InMaxSizeAllowed,EPhysicalMemPageSize InPageSize)
:	MaxSizeAllowed(Align<SIZE_T>(InMaxSizeAllowed,MIN_CHUNK_ALIGNMENT))
,	MinSizeAllowed(Align<SIZE_T>(InMinSizeAllowed,MIN_CHUNK_ALIGNMENT))
,	CurSizeAllocated(0)
,	PhysicalPageSize(InPageSize)
{
	// using 1MB pages for this address space
	const DWORD AddrFlags = SYS_MEMORY_ACCESS_RIGHT_ANY|GetSysPageSize(PhysicalPageSize);
	// address space must be >= 256 MB and power of 2
	const SIZE_T AddrSize = ADDRESS_SPACE_SIZE;
	// reserve a 256 MB portion of the process address space for host memory allocations
	INT Ret = sys_mmapper_allocate_address(AddrSize, AddrFlags, AddrSize, (UINT*)&HostAddressBase);
	checkf( Ret == CELL_OK, TEXT("Failed to allocate address space. sys error=[%d]"), Ret );
}

/**
* Destructor
*/
FMallocGcmHostGrowable::~FMallocGcmHostGrowable()
{
	// remove any existing chunks
	for( INT Idx=0; Idx < AllocChunks.Num(); Idx++ )
	{
		FMallocGcmAllocChunk* Chunk = AllocChunks(Idx);
		if( Chunk )
		{
			RemoveAllocChunk(Chunk);
		}
	}
	// give back the reserved address space
	INT Ret = sys_mmapper_free_address(HostAddressBase);
	checkf( Ret == CELL_OK, TEXT("Failed to free address space. sys error=[%d]"), Ret );
}

/** 
* Find a contiguous space in the address space for this allocator
*
* @param SizeNeeded - total contiguous memory needed 
* @param OutAddressBase - [out] pointer to available space in address space
* @param OutAddressSize - [out] avaialbe space in address space >= SizeNeeded
* @return TRUE if success
*/
UBOOL FMallocGcmHostGrowable::GetAvailableAddressSpace(PTRINT& OutAddressBase, SIZE_T& OutAddressSize, SIZE_T SizeNeeded)
{
	// align size to min chunk boundary
	SizeNeeded = Align<SIZE_T>(SizeNeeded,MIN_CHUNK_ALIGNMENT);
	// iterate over address space entries and find a contiguous block for >= SizeNeeded
	SIZE_T CurSizeFound = 0;
	INT FoundEntryIdx = 0;
	for( INT Idx=0; Idx < MAX_ADDRESS_SPACE_ENTRIES; Idx++ )
	{
		if( CurSizeFound >= SizeNeeded )
			break;
		
		if( AddressSpaceEntries[Idx].AllocChunk != NULL )
		{
			// if region used then start over
			CurSizeFound = 0;		
			FoundEntryIdx = Idx+1;
		}		
		else
		{
			CurSizeFound += MIN_CHUNK_ALIGNMENT;			
		}
	}
	OutAddressBase = HostAddressBase + FoundEntryIdx*MIN_CHUNK_ALIGNMENT;
	OutAddressSize = CurSizeFound;

	UBOOL Success = CurSizeFound >= SizeNeeded && FoundEntryIdx < MAX_ADDRESS_SPACE_ENTRIES;
	checkf( Success,TEXT("Failed to reserve contiguous address space Needed=%d Found=%d"), SizeNeeded, CurSizeFound );
	return Success;	
}

/**
* @return convert base address to AddresSpaceEntries index 
*/
static FORCEINLINE INT GetAddressSpaceIdx(PTRINT AddressBase, PTRINT HostAddressBase)
{
	// starting address space idx used by this chunk
	return (AddressBase - HostAddressBase) / FMallocGcmHostGrowable::MIN_CHUNK_ALIGNMENT;
}

/**
* Update address space entries with a newly allocated chunk by marking entries within its range as used
*
* @param AddressBase - base address used by the heap of the chunk
* @param AddressSize - size used by the heap of the chunk
* @param Chunk - chunk with base/size used wtihin the address space
*/
void FMallocGcmHostGrowable::UpdateAddressSpaceEntries(PTRINT AddressBase, SIZE_T AddressSize, class FMallocGcmAllocChunk* Chunk)
{
	checkSlow(AddressSize == Align<SIZE_T>(AddressSize,MIN_CHUNK_ALIGNMENT));

	// starting address space idx used by this chunk
	INT AddressSpaceIdx = GetAddressSpaceIdx(AddressBase,HostAddressBase);
	// total # of address space entries used by this chunk
	INT AddressSpaceEntriesUsed = AddressSize / MIN_CHUNK_ALIGNMENT;
	checkSlow((AddressSpaceIdx + AddressSpaceEntriesUsed) < MAX_ADDRESS_SPACE_ENTRIES);
	// tag these entries with the chunk
	for( INT Idx=AddressSpaceIdx; Idx < (AddressSpaceIdx + AddressSpaceEntriesUsed); Idx++ )
	{
		check(Chunk == NULL || AddressSpaceEntries[Idx].AllocChunk == NULL);
		AddressSpaceEntries[Idx].AllocChunk = Chunk;		
	}
}

/**
* Allocate physical memory from OS and map it to the address space given
*
* @param AddressBase - address space ptr to map physical memory to
* @param SizeNeeded - total memory needed in increments of PhysicalPageSize
* @return TRUE if success
*/
UBOOL FMallocGcmHostGrowable::CommitPhysicalMemory(PTRINT AddressBase, SIZE_T SizeNeeded)
{
	checkSlow((AddressBase-HostAddressBase) == Align<SIZE_T>((AddressBase-HostAddressBase),MIN_CHUNK_ALIGNMENT));
	checkSlow(SizeNeeded == Align<SIZE_T>(SizeNeeded,MIN_CHUNK_ALIGNMENT));

	// allocate physical memory
	const DWORD AllocFlags = GetSysGranularitySize(PhysicalPageSize);
	sys_memory_t PhysMemId;
	INT Ret = sys_mmapper_allocate_memory(SizeNeeded, AllocFlags, &PhysMemId);
	if( Ret != CELL_OK )
	{
		// trim memory from default heap back to os and try again
		if( GMalloc->TrimMemory(0,TRUE) )
		{
			Ret = sys_mmapper_allocate_memory(SizeNeeded, AllocFlags, &PhysMemId);
		}
	}
	checkf( Ret == CELL_OK, TEXT("Failed to allocate physical memory. sys error=[%d]"), Ret );	

	// map physical to our virtual address space
	Ret = sys_mmapper_map_memory(AddressBase, PhysMemId, SYS_MEMORY_PROT_READ_WRITE);
	checkf( Ret == CELL_OK, TEXT("Failed to map physical memory. sys error=[%d]"), Ret );
	return( Ret == CELL_OK );
}

/**
* Unmaps physical memory from the address space and frees it back to the OS 
*
* @param AddressBase - address space ptr to unmap physical memory 
* @return TRUE if success
*/
UBOOL FMallocGcmHostGrowable::DecommitPhysicalMemory(PTRINT AddressBase)
{
	checkSlow((AddressBase-HostAddressBase) == Align<SIZE_T>((AddressBase-HostAddressBase),MIN_CHUNK_ALIGNMENT));

	sys_memory_t PhysMemId;
	INT Ret = sys_mmapper_unmap_memory(AddressBase,&PhysMemId);
	checkf( Ret == CELL_OK, TEXT("Failed to unmap physical memory. sys error=[%d]"), Ret );

	Ret = sys_mmapper_free_memory(PhysMemId);
	checkf( Ret == CELL_OK, TEXT("Failed to free physical memory. sys error=[%d]"), Ret );	
	return( Ret == CELL_OK );
}


#if STATS
/**
 * Update the maximum size of the GPUSystem memory region after allocation/deallocation of a chunk
 */
static void UpdateMemoryStatMaxSizes()
{
	GStatManager.SetAvailableMemory(MCR_GPUSystem, GPS3MemoryAvailableToMalloc);
}
#endif

/**
* Create a new allocation chunk to fit the requested size. All chunks are aligned to MIN_CHUNK_ALIGNMENT
*
* @param Size - size of chunk
*/
class FMallocGcmAllocChunk* FMallocGcmHostGrowable::CreateAllocChunk(SIZE_T Size)
{
	FMallocGcmAllocChunk* NewChunk = NULL;

	// initial chunk allocates enough for min reserve size
	if( CurSizeAllocated < MinSizeAllowed )
	{
		Size = MinSizeAllowed;
	}

	PTRINT AddressBase=0;
	SIZE_T AddressSize=0;
	// get the available space in the host allocator address space
	GetAvailableAddressSpace(AddressBase,AddressSize,Size);

	// dont go passed max allowed size
	if( (CurSizeAllocated + AddressSize) <= MaxSizeAllowed )
	{
		CurSizeAllocated += AddressSize;

		UBOOL bSuccess=TRUE;
		// commit and map in increments of MIN_CHUNK_ALIGNMENT (assumes Size is aligned to MIN_CHUNK_ALIGNMENT)
		const SIZE_T ChunkSize = MIN_CHUNK_ALIGNMENT;
		INT NumChunksNeeded = AddressSize / ChunkSize;	
		for( INT ChunkIdx=0; ChunkIdx < NumChunksNeeded; ChunkIdx++ )
		{		
			// commit physical memory to this space
			if( !CommitPhysicalMemory(AddressBase + ChunkIdx*ChunkSize,ChunkSize) )
			{	
				bSuccess = FALSE;
				break;
			}
		}

		// map the memory to io space for use by the GPU
		// all chunks allocated are mapped as a single contiguous block
		
		// array of io space offsets generated whenever memory is mapped to GPU
		DWORD GPUOffset = 0;	
		if( !MapBaseMemoryToGPU((void*)(AddressBase),AddressSize,GPUOffset,FALSE) )
		{
			bSuccess = FALSE;
		}

		if( bSuccess )
		{
			// create a new FMallocGcm chunk for this space
			NewChunk = new FMallocGcmAllocChunk(AddressSize,(void*)AddressBase,GPUOffset);
			UpdateAddressSpaceEntries((PTRINT)NewChunk->HeapBase,NewChunk->HeapSize,NewChunk);
			// add a new entry to list
			INT EmptyEntryIdx = AllocChunks.FindItemIndex(NULL);
			if( EmptyEntryIdx == INDEX_NONE )
			{
				EmptyEntryIdx = AllocChunks.Add();
			}
			AllocChunks(EmptyEntryIdx) = NewChunk;
		}
	}
	
#if DEBUG_LOGS
	ImmediateDebugf(TRUE,TEXT("FMallocGcmHostGrowable::CreateAllocChunk  %f KB %d B"),NewChunk->HeapSize/1024.f,NewChunk->HeapSize);
#endif

#if STATS
	UpdateMemoryStatMaxSizes();
#endif

	return NewChunk;
}

/**
* Removes an existing allocated chunk. Unmaps its memory, decommits physical memory back to OS,
* flushes address entries associated with it, and deletes it
*
* @param Chunk - existing chunk to remove
*/
void FMallocGcmHostGrowable::RemoveAllocChunk(class FMallocGcmAllocChunk* Chunk)
{
#if DEBUG_LOGS
	ImmediateDebugf(TRUE,TEXT("FMallocGcmHostGrowable::CreateAllocChunk  %f KB %d B"),Chunk->HeapSize/1024.f,Chunk->HeapSize);
#endif

	checkSlow(Chunk);

	// decommit in increments of MIN_CHUNK_ALIGNMENT (assumes HeapSize is aligned to MIN_CHUNK_ALIGNMENT)
	// unmap memory from GPU
	UnMapBaseMemoryFromGPU(Chunk->GPUHostMemoryOffset);

	const SIZE_T ChunkSize = MIN_CHUNK_ALIGNMENT;
	INT NumChunksNeeded = Chunk->HeapSize / ChunkSize;
	for( INT ChunkIdx=0; ChunkIdx < NumChunksNeeded; ChunkIdx++ )
	{		
		// decommit pages back to OS
		DecommitPhysicalMemory((PTRINT)Chunk->HeapBase + ChunkIdx*ChunkSize);
	}
	CurSizeAllocated -= Chunk->HeapSize;
	
	// clear out address space entries
	UpdateAddressSpaceEntries((PTRINT)Chunk->HeapBase,Chunk->HeapSize,NULL);
	// remove entry
	INT FoundIdx = AllocChunks.FindItemIndex(Chunk);
	check(AllocChunks.IsValidIndex(FoundIdx));
	AllocChunks(FoundIdx) = NULL;			
	delete Chunk;	

#if STATS
	UpdateMemoryStatMaxSizes();
#endif
}

/**
* Shrink an existing chunk allocation (from front,back) based on used entries
* and unmap,decommit its memory
*
* @param Chunk - existing chunk to shrink
*/
void FMallocGcmHostGrowable::ShrinkAllocChunk(class FMallocGcmAllocChunk* Chunk)
{
	// @todo sz - 
	// not implemented since most of our allocations are < 1 MB except for some fixed size host allocs at startup
}

/** triggered during out of memory failure for this allocator */
void FMallocGcmHostGrowable::OutOfMemory( DWORD Size )
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	ImmediateDebugf(FALSE, TEXT("FMallocGcmHostGrowable: OOM %d %f %f"), Size, Size/1024.0f, Size/1024.0f/1024.0f );
	Dump(TEXT("Host (main) memory"),FALSE);
	appPS3DumpDetailedMemoryInformation(*GLog);

	appErrorf(TEXT("FMallocGcmHostGrowable: OOM size=%d %f %f"),Size,Size/1024.f,Size/1024.f/1024.f);
#endif
}

/** 
* Allocate memory with alignment from this allocator
*
* @param Size - size of memory block to allocate
* @param AllocationType - EAllocationType value representing usage of this allocation
* @param Alignment - Size gets padded to min alignment
* @return ptr to base of newly allocated memory
*/
void* FMallocGcmHostGrowable::Malloc(DWORD Size, EAllocationType AllocationType, DWORD Alignment)
{
	// multi-thread protection
	FScopeLock ScopeLock(&CriticalSection);

	checkSlow(Alignment <= MIN_CHUNK_ALIGNMENT);

	void* Result = NULL;
	FMallocGcmAllocChunk* AvailableChunk = NULL;
	
	// align the size to match what Malloc does below
	Size = Align<DWORD>(Size, Alignment);

	// search for an existing alloc chunk with enough space
	for( INT ChunkIdx=0; ChunkIdx < AllocChunks.Num(); ChunkIdx++ )
	{
		FMallocGcmAllocChunk* Chunk = AllocChunks(ChunkIdx);
		if( Chunk && 
			Chunk->CanFitEntry(Size,Alignment) )
		{
			AvailableChunk = Chunk;
			break;			
		}
	}

	// create a new chunk with enough space + alignment to MIN_CHUNK_ALIGNMENT and allocate out of it
	if( AvailableChunk == NULL )
	{	
		AvailableChunk = CreateAllocChunk(Size);
	}

	// allocate from the space in the chunk
	if( AvailableChunk )
	{		
		Result = AvailableChunk->Malloc(Size,AllocationType,Alignment);		
	}

	if( AvailableChunk == NULL || 
		Result == NULL )
	{
		OutOfMemory(Size);	
	}

#if DEBUG_LOGS
	ImmediateDebugf(TRUE,TEXT("FMallocGcmHostGrowable::Malloc  %f KB %d B"),Size/1024.f,Size);
#endif
	
	return Result;
}

/** 
* Free memory previously allocated from this allocator
*
* @param Ptr - memory to free
*/
void FMallocGcmHostGrowable::Free(void* Ptr)
{
	// multi-thread protection
	FScopeLock ScopeLock(&CriticalSection);

	// starting address space idx used by the chunk containing the allocation
	INT AddressSpaceIdx = GetAddressSpaceIdx((PTRINT)Ptr,HostAddressBase);
	if( AddressSpaceIdx < MAX_ADDRESS_SPACE_ENTRIES &&
		AddressSpaceEntries[AddressSpaceIdx].AllocChunk != NULL )
	{
		FMallocGcmAllocChunk* Chunk = AddressSpaceEntries[AddressSpaceIdx].AllocChunk;
		// free space in the chunk
		Chunk->Free(Ptr);
		if( Chunk->IsEmpty() )
		{
			// if empty then unmap and decommit physical memory
			RemoveAllocChunk(Chunk);			
		}		
		else
		{			
			// try to unmap/decommit physical memory from either end of chunk
			ShrinkAllocChunk(Chunk);
		}
	}
	else
	{
		checkf(0,TEXT("Freeing invalid pointer"));
	}
}

/**
* not supported
*/
void* FMallocGcmHostGrowable::Realloc(void* Ptr, DWORD NewSize, EAllocationType AllocationType, DWORD Alignment)
{
	check(0);
	return NULL;
}

SIZE_T FMallocGcmHostGrowable::GetHeapSize()
{
	// multi-thread protection
	FScopeLock ScopeLock(&CriticalSection);

	SIZE_T TotalHeapSize=0;

	// pass off to individual alloc chunks
	for( INT ChunkIdx=0; ChunkIdx < AllocChunks.Num(); ChunkIdx++ )
	{
		FMallocGcmAllocChunk* Chunk = AllocChunks(ChunkIdx);
		if( Chunk )
		{
			TotalHeapSize += Chunk->GetHeapSize();
		}
	}

	return TotalHeapSize;
}

/**
* Update the stats used by the allocated chunks
*
* @param FirstStat - first stat to update
*/
void FMallocGcmHostGrowable::UpdateMemStats(EMemoryStats FirstStat)
{
	// multi-thread protection
	FScopeLock ScopeLock(&CriticalSection);

	// pass off to individual alloc chunks
	for( INT ChunkIdx=0; ChunkIdx < AllocChunks.Num(); ChunkIdx++ )
	{
		FMallocGcmAllocChunk* Chunk = AllocChunks(ChunkIdx);
		if( Chunk )
		{
			Chunk->UpdateMemStats(FirstStat);
		}
	}
}

/**
* Print out detailed memory stats from each of the allocated chunks
*
* @param Desc - text info describing this dump
* @param bShowIndividual - gets detailed info about each individual allocation
* @param bForcePrint - always print
*/
void FMallocGcmHostGrowable::Dump(const TCHAR* Desc, UBOOL bShowIndividual, UBOOL bForcePrint)
{
	// multi-thread protection
	FScopeLock ScopeLock(&CriticalSection);

	// print summary of totals from all allocated chunks
	SIZE_T TotalAlloced=0;
	SIZE_T TotalUsed=0;
	for( INT ChunkIdx=0; ChunkIdx < AllocChunks.Num(); ChunkIdx++ )
	{
		FMallocGcmAllocChunk* Chunk = AllocChunks(ChunkIdx);
		if( Chunk )
		{
			TotalAlloced += Chunk->HeapSize;
			TotalUsed += Chunk->UsedMemorySize;
		}
	}
	ImmediateDebugf(bForcePrint,TEXT("%s Summary from all allocated chunks:"),Desc);
	ImmediateDebugf(bForcePrint,TEXT("Used %.2fM / %.2fM"),TotalUsed/1024.f/1024.f, TotalAlloced/1024.f/1024.f);
	ImmediateDebugf(bForcePrint,TEXT(""));

	// pass off to individual alloc chunks
	for( INT ChunkIdx=0; ChunkIdx < AllocChunks.Num(); ChunkIdx++ )
	{
		FMallocGcmAllocChunk* Chunk = AllocChunks(ChunkIdx);
		if( Chunk )
		{
			Chunk->Dump(*FString::Printf(TEXT("%s (chunk=%d)"),Desc,ChunkIdx),bShowIndividual,bForcePrint);
		}
	}
}

/**
 * Print out memory information
 * @param Desc				Descriptive name for the heap
 * @param bShowIndividual	If TRUE, each allocation will be printed, if FALSE, a summary of each type will be printed
 * @param Ar				The archive to log to...
 */
void FMallocGcmHostGrowable::DumpToArchive(const TCHAR* Desc, UBOOL bShowIndividual, FOutputDevice& Ar)
{
	// multi-thread protection
	FScopeLock ScopeLock(&CriticalSection);

	// print summary of totals from all allocated chunks
	SIZE_T TotalAlloced=0;
	SIZE_T TotalUsed=0;
	for( INT ChunkIdx=0; ChunkIdx < AllocChunks.Num(); ChunkIdx++ )
	{
		FMallocGcmAllocChunk* Chunk = AllocChunks(ChunkIdx);
		if( Chunk )
		{
			TotalAlloced += Chunk->HeapSize;
			TotalUsed += Chunk->UsedMemorySize;
		}
	}
	Ar.Logf(TEXT("%s Summary from all allocated chunks:"),Desc);
	Ar.Logf(TEXT("Used %.2fM / %.2fM"),TotalUsed/1024.f/1024.f, TotalAlloced/1024.f/1024.f);
	Ar.Logf(TEXT(""));

	// pass off to individual alloc chunks
	for( INT ChunkIdx=0; ChunkIdx < AllocChunks.Num(); ChunkIdx++ )
	{
		FMallocGcmAllocChunk* Chunk = AllocChunks(ChunkIdx);
		if( Chunk )
		{
			Chunk->DumpToArchive(*FString::Printf(TEXT("%s (chunk=%d)"),Desc,ChunkIdx),bShowIndividual,Ar);
		}
	}
}

/** 
* Get total Virtual,Physical memory allocated by this allocator
* 
* @param Virtual - [out] virtual memory allocated
* @param Physical - [out] physical memory allocated
*/
void FMallocGcmHostGrowable::GetAllocationInfo(SIZE_T& Virtual, SIZE_T& Physical)
{
	// multi-thread protection
	FScopeLock ScopeLock(&CriticalSection);

	Virtual = 0;
	Physical = 0;
	// pass off to individual alloc chunks
	for( INT ChunkIdx=0; ChunkIdx < AllocChunks.Num(); ChunkIdx++ )
	{
		FMallocGcmAllocChunk* Chunk = AllocChunks(ChunkIdx);
		if( Chunk )
		{
			SIZE_T ChunkVirtual=0;
			SIZE_T ChunkPhysical=0;
			Chunk->GetAllocationInfo(ChunkVirtual,ChunkPhysical);
			Virtual += ChunkVirtual;
			Physical += ChunkPhysical;
		}
	}
}

/** render free/used blocks from this malloc gcm region */
void FMallocGcmHostGrowable::DepugDraw(class FCanvas* Canvas, FLOAT& X,FLOAT& Y,FLOAT SizeX,FLOAT SizeY, const TCHAR* Desc,UBOOL bDrawKey)
{
	FLOAT ChunkSizeX = SizeX;
	FLOAT ChunkSizeY = SizeY;
	FLOAT ChunkX = X;
	FLOAT ChunkY = Y;
	for( INT ChunkIdx=0; ChunkIdx < AllocChunks.Num(); ChunkIdx++ )
	{
		FMallocGcmAllocChunk* Chunk = AllocChunks(ChunkIdx);
		if( Chunk )
		{
			Chunk->DepugDraw(Canvas,ChunkX,ChunkY,ChunkSizeX,ChunkSizeY,*FString::Printf(TEXT("%s(growable)"),Desc));
			ChunkY += ChunkSizeY;
		}
	}
	// draw empty block
	if( AllocChunks.Num() == 0 )
	{
		// draw border outline	
		DrawLine2D(Canvas,FVector2D(ChunkX,ChunkY),FVector2D(ChunkX+ChunkSizeX,ChunkY),FLinearColor::Black);						//top
		DrawLine2D(Canvas,FVector2D(ChunkX,ChunkY),FVector2D(ChunkX,ChunkY+ChunkSizeY),FLinearColor::Black);						//left
		DrawLine2D(Canvas,FVector2D(ChunkX+ChunkSizeX,ChunkY),FVector2D(ChunkX+ChunkSizeX,ChunkY+ChunkSizeY),FLinearColor::Black);	//right
		DrawLine2D(Canvas,FVector2D(ChunkX,ChunkY+ChunkSizeY),FVector2D(ChunkX+ChunkSizeX,ChunkY+ChunkSizeY),FLinearColor::Black);	//bottom
		// overlay text
		DrawString(Canvas,
			ChunkX,ChunkY,
			*FString::Printf(TEXT("%s(growable) Empty [%.2f KB]"),Desc ? Desc : TEXT(""),0.f),
			GEngine ? GEngine->TinyFont : NULL,
			FLinearColor::Black);	
		ChunkY += ChunkSizeY;
	}
	if( bDrawKey )
	{
		DebugDrawKey(Canvas,ChunkX,ChunkY);
	}
	// next line
	Y = ChunkY;
}

