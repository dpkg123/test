/*=============================================================================
	FMallocGcm.h: Gcm memory allocator that keeps bookeeping outside of heap
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FMALLOCGCM_H__
#define __FMALLOCGCM_H__

// NOTE: This enum must match STAT_LocalCommandBufferSize and up (Core.h)!
enum EAllocationType
{
	AT_CommandBuffer,
	AT_FrameBuffer,
	AT_ZBuffer,
	AT_RenderTarget,
	AT_Texture,
	AT_VertexShader,
	AT_PixelShader,
	AT_VertexBuffer,
	AT_IndexBuffer,
	AT_RingBuffer,
	AT_CompressionTag,
	AT_ResourceArray,
	AT_OcclusionQueries,
	AT_MAXValue,
};

/**
* Base class for all GCM memory allocators
*/
class FMallocGcmBase
{
public:
	virtual void* Malloc(DWORD Size, EAllocationType AllocationType, DWORD Alignment=DEFAULT_ALIGNMENT)=0;
	virtual void Free(void* Ptr)=0;
	virtual void* Realloc(void* Ptr, DWORD NewSize, EAllocationType AllocationType, DWORD Alignment=DEFAULT_ALIGNMENT)=0;
	virtual void UpdateMemStats(EMemoryStats FirstStat)=0;
	virtual void Dump(const TCHAR* Desc, UBOOL bShowIndividual, UBOOL bForcePrint=FALSE)=0;
	/**
	 * Print out memory information
	 * @param Desc				Descriptive name for the heap
	 * @param bShowIndividual	If TRUE, each allocation will be printed, if FALSE, a summary of each type will be printed
	 * @param Ar				The archive to log to...
	 */
	virtual void DumpToArchive(const TCHAR* Desc, UBOOL bShowIndividual, FOutputDevice& Ar)=0;
	virtual void GetAllocationInfo(SIZE_T& Virtual, SIZE_T& Physical)=0;
	virtual SIZE_T GetHeapSize()=0;
	virtual void MapMemoryToGPU()=0;
	virtual void DepugDraw(class FCanvas* Canvas, FLOAT& X,FLOAT& Y,FLOAT SizeX,FLOAT SizeY, const TCHAR* Desc,UBOOL bDrawKey=FALSE){}
};

/**
* Allocator for a region of of pre-allocated memory
*/
class FMallocGcm : public FMallocGcmBase
{
public:
	/**
	 * Constructor
	 * @param InSize The size of the total heap. If Base is NULL, the heap will be allocated to this size
	 * @param InBase Optional location of the preallocated buffer to use as the heap (must be at least Size big)
	 */
	FMallocGcm(SIZE_T InSize, void* InBase);

	/**
	 * Destructor 
	 */
	virtual ~FMallocGcm();

	// FMallocGcmBase interface

	virtual void*	Malloc(DWORD Size, EAllocationType AllocationType, DWORD Alignment=DEFAULT_ALIGNMENT);
	virtual void	Free(void* Ptr);
	virtual void*	Realloc(void* Ptr, DWORD NewSize, EAllocationType AllocationType, DWORD Alignment=DEFAULT_ALIGNMENT);

	/**
	 * Gathers memory allocations for both virtual and physical allocations.
	 *
	 * @param Virtual	[out] size of virtual allocations
	 * @param Physical	[out] size of physical allocations	
	 */
	virtual void	GetAllocationInfo(SIZE_T& Virtual, SIZE_T& Physical);

	/** 
	* Get the total size of the heap 
	*/
	virtual SIZE_T GetHeapSize()
	{
		return HeapSize;
	}

	/**
	 * Updates memory stats using this heap. 
	 * @param FirstStat ID of the first stat that matches up to the first EAllocationType 
	 */
	void UpdateMemStats(EMemoryStats FirstStat);

	/**
	 * Print out memory information
	 * @param Desc				Descriptive name for the heap
	 * @param bShowIndividual	If TRUE, each allocation will be printed, if FALSE, a summary of each type will be printed
	 * @param bForcePrint		If TRUE, outputs with printf instead of debugf
	 */
	void Dump(const TCHAR* Desc, UBOOL bShowIndividual, UBOOL bForcePrint=FALSE );
	/**
	 * Print out memory information
	 * @param Desc				Descriptive name for the heap
	 * @param bShowIndividual	If TRUE, each allocation will be printed, if FALSE, a summary of each type will be printed
	 * @param Ar				The archive to log to...
	 */
	virtual void DumpToArchive(const TCHAR* Desc, UBOOL bShowIndividual, FOutputDevice& Ar);

	/** not used */
	virtual void MapMemoryToGPU() {}

	/** render free/used blocks from this malloc gcm region */
	virtual void DepugDraw(class FCanvas* Canvas, FLOAT& X,FLOAT& Y,FLOAT SizeX,FLOAT SizeY, const TCHAR* Desc,UBOOL bDrawKey=FALSE);

	class FFreeEntry
	{
	public:
		/** Constructor */
		FFreeEntry(FFreeEntry *NextEntry, BYTE* InLocation, SIZE_T InSize);

		/**
		* Determine if the given allocation with this alignment and size will fit
		* @param AllocationSize	Already aligned size of an allocation
		* @param Alignment			Alignment of the allocation (location may need to increase to match alignment)
		* @return TRUE if the allocation will fit
		*/
		UBOOL CanFit(SIZE_T AlignedSize, DWORD Alignment);

		/**
		* Take a free chunk, and split it into a used chunk and a free chunk
		* @param UsedSize	The size of the used amount (anything left over becomes free chunk)
		* @param Alignment	The alignment of the allocation (location and size)
		* @param AllocationType The type of allocation
		* @param bDelete Whether or not to delete this FreeEntry (ie no more is left over after splitting)
		* 
		* @return The pointer to the free data
		*/
		BYTE* Split(SIZE_T UsedSize, DWORD Alignment, EAllocationType AllocationType, UBOOL &bDelete);

#if USE_ALLOCATORFIXEDSIZEFREELIST
		/** Custom new/delete */
		void* operator new(size_t Size);
		void operator delete(void *RawMemory);
#endif

		/** Address in the heap */
		BYTE*		Location;

		/** Size of the free block */
		DWORD		BlockSize;

		/** Next block of free memory. */
		FFreeEntry*	Next;
	};	

protected:
	/** Start of the heap */
	BYTE*			HeapBase;

	/** Size of the heap */
	SIZE_T			HeapSize;

	/** Size of used memory */
	SIZE_T			UsedMemorySize;

	/** List of free blocks */
	FFreeEntry*		FreeList;
};

/**
* Host memory allocator which uses pre-allocated memory 
*/
class FMallocGcmHostStatic : public FMallocGcm
{	
public:
	/**
	* Constructor
	*
	* @param InSize - total size of the heap
	* @param InBase - start address of memory block for heap (1MB align required)
	*/
	FMallocGcmHostStatic(SIZE_T InSize, void* InBase);

	/**
	* Destructor
	*/
	virtual ~FMallocGcmHostStatic();

	// FMallocGcmBase interface

	/**
	* Map all memory (Base+Size) for use by GPU
	*/
	virtual void MapMemoryToGPU();

	/** io offset on GPU corresponding to this sys memory allocation */
	DWORD GPUHostMemmoryOffset;	
};

/**
* Host memory allocator which uses the default malloc heap to allocate memory
*/
class FMallocGcmHostHeap : public FMallocGcm
{
public:

	/**
	* Constructor
	*
	* @param InSize - total size of the heap
	* @param InBase - start address of memory block for heap (1MB align required)
	*/
	FMallocGcmHostHeap(SIZE_T InSize, void* InBase);

	/**
	* Destructor
	*/
	virtual ~FMallocGcmHostHeap();

	// FMallocGcmBase interface
	
	virtual void*	Malloc(DWORD Size, EAllocationType AllocationType, DWORD Alignment=DEFAULT_ALIGNMENT);
	virtual void	Free(void* Ptr);
	virtual void*	Realloc(void* Ptr, DWORD NewSize, EAllocationType AllocationType, DWORD Alignment=DEFAULT_ALIGNMENT);

	/**
	* Gathers memory allocations for both virtual and physical allocations.
	*
	* @param Virtual	[out] size of virtual allocations
	* @param Physical	[out] size of physical allocations	
	*/
	virtual void	GetAllocationInfo(SIZE_T& Virtual, SIZE_T& Physical);

	/**
	* Map all memory (Base+Size) for use by GPU
	*/
	virtual void MapMemoryToGPU();

	/** io offset on GPU corresponding to this sys memory allocation */
	DWORD GPUHostMemmoryOffset;

	/** total memory allocated from appmalloc (including padding) */
	INT TotalAllocated;
};

/**
* Host memory allocator which grows as needed. Uses a list of FMallocGcm entries for currently allocated blocks
*/
class FMallocGcmHostGrowable : public FMallocGcmBase
{
public:

	/**
	* Types of physical OS page sizes supported
	*/
	enum EPhysicalMemPageSize
	{
		// 1 MByte page size for physical allocs
		PHYS_PAGE_1MB	=0,
		// 64 KByte page size for physical allocs
		PHYS_PAGE_64KB
	};

	/** Minimum alignment required for all chunk regions allocated */
	enum 
	{ 
		// must be a multiple of 1 MB
		MIN_CHUNK_ALIGNMENT				= 1 * 1024 * 1024,
		// address space must be at least 256 MB and power of 2
		ADDRESS_SPACE_SIZE				= 256 * 1024 * 1024,
		// max number of chunk entries that can be allocated
		MAX_ADDRESS_SPACE_ENTRIES		= ADDRESS_SPACE_SIZE/MIN_CHUNK_ALIGNMENT
	};

	/**
	* Constructor
	* Internally allocates address space for use only by this allocator
	*
	* @param InMinSizeAllowed - min reserved space by this allocator
	* @param InMaxSizeAllowed - total size of allocations won't exceed this limit
	* @param InPageSize - physical memory allocs are done in increments of this page size
	*/
	FMallocGcmHostGrowable(SIZE_T InMinSizeAllowed=0,SIZE_T InMaxSizeAllowed=ADDRESS_SPACE_SIZE,EPhysicalMemPageSize InPageSize=PHYS_PAGE_1MB);
	
	/**
	* Destructor
	*/
	virtual ~FMallocGcmHostGrowable();

	// FMallocGcmBase interface

	virtual void* Malloc(DWORD Size, EAllocationType AllocationType, DWORD Alignment=DEFAULT_ALIGNMENT);
	virtual void Free(void* Ptr);
	virtual void* Realloc(void* Ptr, DWORD NewSize, EAllocationType AllocationType, DWORD Alignment=DEFAULT_ALIGNMENT);	
	virtual void UpdateMemStats(EMemoryStats FirstStat);
	virtual void Dump(const TCHAR* Desc, UBOOL bShowIndividual, UBOOL bForcePrint=FALSE);
	/**
	 * Print out memory information
	 * @param Desc				Descriptive name for the heap
	 * @param bShowIndividual	If TRUE, each allocation will be printed, if FALSE, a summary of each type will be printed
	 * @param Ar				The archive to log to...
	 */
	virtual void DumpToArchive(const TCHAR* Desc, UBOOL bShowIndividual, FOutputDevice& Ar);
	virtual void GetAllocationInfo(SIZE_T& Virtual, SIZE_T& Physical);
	virtual SIZE_T GetHeapSize();
	virtual void MapMemoryToGPU() {}

	/** render free/used blocks from this malloc gcm region */
	virtual void DepugDraw(class FCanvas* Canvas, FLOAT& X,FLOAT& Y,FLOAT SizeX,FLOAT SizeY, const TCHAR* Desc,UBOOL bDrawKey=FALSE);

private:

	/** base for the address space used by this alloc */
	PTRINT HostAddressBase;
	/** largest total allocation allowed */
	const SIZE_T MaxSizeAllowed;
	/** minimum reserved space initially allocated */
	const SIZE_T MinSizeAllowed;
	/** total currently allocated from OS */
	SIZE_T CurSizeAllocated;
	/** configured size of pages that are committed to OS */
	const EPhysicalMemPageSize PhysicalPageSize;
	/** list of currently allocated chunks */
	TArray<class FMallocGcmAllocChunk*> AllocChunks;
	/** Shared critical section object to protect from use on multiple threads */
	static FCriticalSection CriticalSection;	

	/** represents an entry of size MIN_CHUNK_ALIGNMENT */
	class FAddressSpaceEntry
	{
	public:
		FAddressSpaceEntry() 
			: AllocChunk(NULL) 
		{}
		/** chunk currently occupying this address space entry */
		class FMallocGcmAllocChunk* AllocChunk;
	};
	/** currently used entries within the ADDRESS_SPACE_SIZE limit in blocks of MIN_CHUNK_ALIGNMENT */
	FAddressSpaceEntry AddressSpaceEntries[MAX_ADDRESS_SPACE_ENTRIES];		

	/** 
	* Find a contiguous space in the address space for this allocator
	*
	* @param SizeNeeded - total contiguous memory needed 
	* @param OutAddressBase - [out] pointer to available space in address space
	* @param OutAddressSize - [out] avaialbe space in address space >= SizeNeeded
	* @return TRUE if success
	*/
	UBOOL GetAvailableAddressSpace(PTRINT& OutAddressBase, SIZE_T& OutAddressSize, SIZE_T SizeNeeded);

	/**
	* Create a new allocation chunk to fit the requested size. All chunks are aligned to MIN_CHUNK_ALIGNMENT
	*
	* @param Size - size of chunk
	*/
	class FMallocGcmAllocChunk* CreateAllocChunk(SIZE_T Size);

	/**
	* Removes an existing allocated chunk. Unmaps its memory, decommits physical memory back to OS,
	* flushes address entries associated with it, and deletes it
	*
	* @param Chunk - existing chunk to remove
	*/
	void RemoveAllocChunk(class FMallocGcmAllocChunk* Chunk);

	/**
	* Shrink an existing chunk allocation (from front,back) based on used entries
	* and unmap,decommit its memory
	*
	* @param Chunk - existing chunk to shrink
	*/
	void ShrinkAllocChunk(class FMallocGcmAllocChunk* Chunk);

	/**
	* Allocate physical memory from OS and map it to the address space given
	*
	* @param AddressBase - address space ptr to map physical memory to
	* @param SizeNeeded - total memory needed in increments of PhysicalPageSize
	* @return TRUE if success
	*/
	UBOOL CommitPhysicalMemory(PTRINT AddressBase, SIZE_T SizeNeeded);

	/**
	* Unmaps physical memory from the address space and frees it back to the OS 
	*
	* @param AddressBase - address space ptr to unmap physical memory 
	* @return TRUE if success
	*/
	UBOOL DecommitPhysicalMemory(PTRINT AddressBase);

	/**
	* Update address space entries with a newly allocated chunk by marking entries within its range as used
	*
	* @param AddressBase - base address used by the heap of the chunk
	* @param AddressSize - size used by the heap of the chunk
	* @param Chunk - chunk with base/size used wtihin the address space
	*/
	void UpdateAddressSpaceEntries(PTRINT AddressBase, SIZE_T AddressSize, class FMallocGcmAllocChunk* Chunk);

	/** triggered during out of memory failure for this allocator */
	FORCENOINLINE void OutOfMemory( DWORD Size );
};

/**
 * @return the amount of memory the bookkeeping information is taking for 
 * FMallocGcm
 */
DWORD appPS3GetGcmMallocOverheadSize();

#endif //__FMALLOCGCM_H__
