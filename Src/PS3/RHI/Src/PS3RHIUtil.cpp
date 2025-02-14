/*=============================================================================
	PS3RHIUtil.cpp: PS3 RHI utility implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3RHIPrivate.h"
#include "BestFitAllocator.h"

#if USE_PS3_RHI


/*-----------------------------------------------------------------------------
	Misc GPU resource management.
-----------------------------------------------------------------------------*/

/**
 * Container class for PS3 GPU resources that are to be deleted the next frame.
 */
struct FPS3UnusedResource
{
	/** address of manually allocated resource */
	void* BaseAddress;

	/** Which block of memory (GPU/CPU) this resource was allocated in */
	EMemoryRegion MemoryRegion;

	/** TRUE if resource is a texture */
	BITFIELD bIsTexture:1;

	/** TRUE if BaseAddress of this resource comes from a TResourceArray */ 
	BITFIELD bUsesResourceArray:1;
};


/*
 * Two buffers of orphaned resources that will be deleted the next frame.
 * One buffer is for the current frame, the other is for the previous frame.
 */
static TArray<FPS3UnusedResource> GUnusedPS3Resources[2];
	
/* Index into GDeferredPS3Resources that specifies which buffer is for the current frame. */
static UINT GUnusedPS3ResourcesCurrentFrame = 0;


/*
 * Adds an unused PS3 GPU resource to a buffer so that it can be deleted properly at a later time.
 *
 * @param BaseAddress	- Memory to be deleted.
 * @param MemoryRegion	- Where the memory was allocated
 * @param bIsTexture	- TRUE if resource is a texture.
 * @param bUsesResourceArray - TRUE if BaseAddress of this resource comes from a TResourceArray [UNUSED for now]
 */
void AddUnusedPS3Resource(void* BaseAddress, EMemoryRegion MemoryRegion, UBOOL bIsTexture, UBOOL bUsesResourceArray)
{
	check(IsInRenderingThread());

	TArray<FPS3UnusedResource>& ResourceArray = GUnusedPS3Resources[GUnusedPS3ResourcesCurrentFrame];
	INT Index = ResourceArray.Add(1);
	ResourceArray(Index).BaseAddress	= BaseAddress;
	ResourceArray(Index).MemoryRegion	= MemoryRegion;
	ResourceArray(Index).bIsTexture		= bIsTexture;
	ResourceArray(Index).bUsesResourceArray = bUsesResourceArray;
}

/*
 * Deletes all resources that became unused during the previous frame. It also swaps the two buffers.
 * This function should be called once at the end of each frame.
 */
void DeleteUnusedPS3Resources()
{
	TArray<FPS3UnusedResource>& ResourceArray = GUnusedPS3Resources[1 - GUnusedPS3ResourcesCurrentFrame];

	for ( INT Index=0; Index < ResourceArray.Num(); ++Index )
	{
		FPS3UnusedResource& Resource = ResourceArray( Index );

		// free from texture pool if its a texture
		if (Resource.bIsTexture)
		{
			GPS3Gcm->GetTexturePool()->Free(Resource.BaseAddress);
		}
		// resource arrays free themselves
		else if (!Resource.bUsesResourceArray)
		{
			// pick an allocator to free the memory
			(Resource.MemoryRegion == MR_Local ? GPS3Gcm->GetLocalAllocator() : GPS3Gcm->GetHostAllocator())->Free(Resource.BaseAddress);
		}
	}

	// empty the array
	// @todo: Mark FPS3UnusedREsource as not needing a constructor
	ResourceArray.Reset();

	// Swap the current frame.
	GUnusedPS3ResourcesCurrentFrame = 1 - GUnusedPS3ResourcesCurrentFrame;
}


/**
 * Constructor - basic initialization
 */
FPS3RHIGPUResource::FPS3RHIGPUResource()
: Data(NULL)
, MemoryRegion(MR_Bad)
, bFreeMemory(FALSE)
, bUsesResourceArray(FALSE)
, bUsesTexturePool(FALSE)
, bUsesTiledRegister(FALSE)
, AllocatedSize(0)
{
}

/**
 * Constructor - attaches to existing memory
 */
FPS3RHIGPUResource::FPS3RHIGPUResource(DWORD Offset, EMemoryRegion Region)
: Data(NULL)
, MemoryOffset(Offset)
, MemoryRegion(Region)
, bFreeMemory(FALSE)
, bUsesResourceArray(FALSE)
, bUsesTexturePool(FALSE)
, bUsesTiledRegister(FALSE)
, AllocatedSize(0)
{
}

/**
 * Constructor - attaches to a resource array
 */
FPS3RHIGPUResource::FPS3RHIGPUResource(FResourceArrayInterface* ResourceArray)
: Data((void*)ResourceArray->GetResourceData())
, MemoryRegion(ResourceArray->GetAllowCPUAccess() ? MR_Host : MR_Local)
, bFreeMemory(FALSE)
, bUsesResourceArray(TRUE)
, bUsesTexturePool(FALSE)
, bUsesTiledRegister(FALSE)
, AllocatedSize(0)
{
	// calculate offset
	cellGcmAddressToOffset(Data, &MemoryOffset);
}

/**
 * Destructor - deallocates memory if necessary 
 */
FPS3RHIGPUResource::~FPS3RHIGPUResource()
{
	// if we need to free memory (because we allocated it) then do so now
	if (bFreeMemory)
	{
		check(MemoryRegion != MR_Bad);
		check(Data);

		// add this resource to the list of resources to be deleted later
		// non-tiled textures are in the texture pool, so are handled differently
		AddUnusedPS3Resource(Data, (EMemoryRegion)MemoryRegion, bUsesTexturePool, bUsesResourceArray);
	}
}
/**
 * Allocate RSX-usable memory in one of the two memory regions
 *
 * @param Size			How much to allocation
 * @param Region		Which memory region to allocate in
 * @param AllocationType The type of allocation (texture, vertex buffer, etc)
 * @param Alignment		The alignment for the allocation (default to 128)
 */
UBOOL FPS3RHIGPUResource::AllocateMemory(DWORD Size, EMemoryRegion Region, EAllocationType InAllocationType, DWORD Alignment, FResourceBulkDataInterface* BulkData)
{
	// remember the memory region and allocation type
	MemoryRegion = Region;

	// note that we need to free the memory
	bFreeMemory = TRUE;

	// remember allocation type
	AllocationType = InAllocationType;

	// allocate the data
	if (AllocationType == AT_Texture)
	{
		// Allocate memory from the texture pool.
		bUsesTexturePool = TRUE;
		if( BulkData )
		{
			checkf(Size == BulkData->GetResourceBulkDataSize(), TEXT("Resource bulkdata size mismatch Size=%d BulkDataSize=%d"),Size,BulkData->GetResourceBulkDataSize());
			// get preallocated memory for this resource and hold onto it 
			Data = BulkData->GetResourceBulkData();
			// discard will just clear data pointer since it will be freed by when this resource is released 
			BulkData->Discard();
		}
		else
		{
			Data = GPS3Gcm->GetTexturePool()->Allocate(Size, FALSE);
		}
		check(Data);
	}
	else
	{
		Data = (MemoryRegion == MR_Local ? GPS3Gcm->GetLocalAllocator() : GPS3Gcm->GetHostAllocator())->Malloc(Size, AllocationType, Alignment);
		if (Data == NULL)
		{
			GPS3Gcm->GetHostAllocator()->Dump(TEXT("Host"), FALSE);
			GPS3Gcm->GetLocalAllocator()->Dump(TEXT("Local"), FALSE);
			checkf(Data, TEXT("\nPS3RHIGPUResource failed to allocate %d bytes of GPU data (type = %d"), Size, InAllocationType);
		}
	}

	// calculate offset
	cellGcmAddressToOffset(Data, &MemoryOffset);

	// remember size
	AllocatedSize = Size;

	return TRUE;
}

/**
 * Set our data to the given pointer
 * @param Data	A pointer to vertex data, assumed to be the proper size set in the constructor
 */
void FPS3RHIGPUResource::SetData(const void* InData)
{
	// there must be a known amount of memory
	check(AllocatedSize != 0);

	// @GEMINI_TODO: DMA this data, not memcpy!
	appMemcpy(Data, InData, AllocatedSize);
}

/**
 * Constructor - allocates one or two buffers of the given size in the given memory region
 */
FPS3RHIDoubleBufferedGPUResource::FPS3RHIDoubleBufferedGPUResource(DWORD Size, EMemoryRegion Region, EAllocationType InAllocationType, UBOOL bNeedsTwoBuffers, DWORD Alignment)
: CurrentBuffer(0)
{
	// always allocate one buffer
	AllocateMemory(Size, Region, InAllocationType, Alignment);

	// allocate a second buffer if needed
	if (bNeedsTwoBuffers)
	{
		Buffers[1] = Data;
		Offsets[1] = MemoryOffset;

		// allocate another block
		AllocateMemory(Size, Region, InAllocationType, Alignment);

		// save it
		Buffers[0] = Data;
		Offsets[0] = MemoryOffset;
	}
	else
	{
		Buffers[0] = Buffers[1] = Data;
		Offsets[0] = Offsets[1] = MemoryOffset;
	}
	
}

/**
 * Constructor - attaches to a resource array
 */
FPS3RHIDoubleBufferedGPUResource::FPS3RHIDoubleBufferedGPUResource(FResourceArrayInterface* ResourceArray)
: FPS3RHIGPUResource(ResourceArray)
, CurrentBuffer(0)
{
	// point both buffers to the same data
	Buffers[0] = Buffers[1] = Data;
	Offsets[0] = Offsets[1] = MemoryOffset;
}

/**
 * Destructor - deallocates memory if necessary 
 */
FPS3RHIDoubleBufferedGPUResource::~FPS3RHIDoubleBufferedGPUResource()
{
	// the base destructor will deallocate the buffer pointed to by Data, so don't destroy that one
	for (DWORD BufferIndex = 0; BufferIndex < ARRAY_COUNT(Buffers); BufferIndex++)
	{
		if (Buffers[BufferIndex] != Data)
		{
			// add this resource to the list of resources to be deleted later
			AddUnusedPS3Resource(Buffers[BufferIndex], (EMemoryRegion)MemoryRegion, bUsesTexturePool, FALSE);
		}
	}
}
	
/**
 * Swap the current buffer, and update the FPS3RHIGPUResource's Data and MemoryOffset accordingly
 */
void FPS3RHIDoubleBufferedGPUResource::SwapBuffers()
{
	// handle double buffer if needed
	Data = Buffers[CurrentBuffer];
	MemoryOffset = Offsets[CurrentBuffer];
	CurrentBuffer ^= 1;
}


/**
 * Constructor
 *
 * @param InBufferSize Size of total buffer (must be a multiple of InNumPartitions)
 * @param InNumPartitions NUmber of partitions in the buffer 
 * @param InAlignment Amount to align each allocation
 * @param InMemoryRegion Location to allocate the buffer
 */
FFencedRingBuffer::FFencedRingBuffer(INT InBufferSize, INT InNumPartitions, INT InAlignment, EMemoryRegion InMemoryRegion)
{
	// init some parameters
	NumPartitions = InNumPartitions;
	CurrentOffset = 0;
	CurrentPartition = 0;
	Alignment = InAlignment;

	// initialize fences
	Fences = new DWORD[NumPartitions];

	// make start fence
	GPS3Gcm->InsertFence(FALSE);

	// Setup the partitions fences to the starting fence
	for ( DWORD PartitionIndex=0; PartitionIndex < NumPartitions; ++PartitionIndex )
	{
		Fences[PartitionIndex] = GPS3Gcm->GetCurrentFence();
	}

	// make sure buffersize is a multiple of numpartitions
	check((InBufferSize % NumPartitions) == 0);

	// calculate size of each partition
	PartitionSize = InBufferSize / NumPartitions;

	// allocate the ring buffer memory
	RingBufferData = ((InMemoryRegion == MR_Host) ? GPS3Gcm->GetHostAllocator() : GPS3Gcm->GetLocalAllocator())->
		Malloc(InBufferSize, AT_RingBuffer, Alignment);
	
	// get the gpu offset 
	cellGcmAddressToOffset(RingBufferData, &RingBufferOffset);
}

/**
 * Allocate space in the ring buffer, using fences to make sure GPU is not using the data
 *
 * @param Size Amount to allocate
 * 
 * @return Allocated data
 */
void* FFencedRingBuffer::Malloc(INT Size)
{
	// align size to the alignment for this ring buffer
	Size = Align(Size, Alignment);

	// save off the current location
	DWORD SavedOffset = CurrentOffset;

	// Out of available space in current partition?
	if ((SavedOffset + Size) > (CurrentPartition + 1) * (PartitionSize))
	{
		if (Size > PartitionSize)
		{
			//debugf(TEXT("Ring buffer alloc doesn't fit, using malloc [%d/%d]"), Size, PartitionSize);
			void* Ptr = GPS3Gcm->GetHostAllocator()->Malloc(Size, AT_RingBuffer, Alignment);
			AddUnusedPS3Resource(Ptr, MR_Host, FALSE, FALSE);
			return Ptr;
		}

		// Double-check that the buffer partition is big enough.
		checkf(Size <= PartitionSize, TEXT("Size %d is bigger than partition size %d"), Size, PartitionSize);

		// Insert a fence for the current partition, so that we know when the GPU is done with it next time we need it.
		Fences[CurrentPartition] = GPS3Gcm->InsertFence(FALSE);

		// Move on to the next partition.
		CurrentPartition = (CurrentPartition + 1) % NumPartitions;

		// Align the allocation to the start of the new partition.
		SavedOffset = CurrentPartition * (PartitionSize);

		// Block until the GPU has processed the old data in the new partition (should normally not block).
		GPS3Gcm->BlockOnFence(Fences[CurrentPartition]);
	}

	// Update global patch pointer
	CurrentOffset = SavedOffset + Size;

	return (BYTE*)RingBufferData + SavedOffset;
}

#if RHI_SUPPORTS_PREVERTEXSHADER_CULLING
FEdgeOutputBuffer::FEdgeOutputBuffer(INT InRingBufferSizePerSpu, EMemoryRegion InMemoryRegion)
{
	// Allocate actual buffers
	RingBuffers = NULL;
	InRingBufferSizePerSpu = Align(InRingBufferSizePerSpu, 128);
	if (InRingBufferSizePerSpu > 0)
	{
		RingBuffers = (BYTE*)((InMemoryRegion == MR_Host) ? GPS3Gcm->GetHostAllocator() : GPS3Gcm->GetLocalAllocator())->
			Malloc(InRingBufferSizePerSpu * SPU_NUM_SPURS, AT_RingBuffer, 128);
	}

	// Allocate an area to write the culled triangle list indices to.
	// Initialize the EDGE Geometry output buffer
	// shared buffer info (unused, but must be set to zero size)
	BufferInfo.sharedInfo.startEa = 0;
	BufferInfo.sharedInfo.endEa = 0;
	BufferInfo.sharedInfo.currentEa = 0;
	BufferInfo.sharedInfo.locationId = (InMemoryRegion == MR_Host) ? CELL_GCM_LOCATION_MAIN : CELL_GCM_LOCATION_LOCAL;
	BufferInfo.sharedInfo.failedAllocSize = 0;
	cellGcmAddressToOffset((void*)BufferInfo.sharedInfo.startEa, &BufferInfo.sharedInfo.startOffset);
	// ring buffer info
	for(UINT SPUIndex = 0; SPUIndex < SPU_NUM_SPURS;SPUIndex++)
	{
		// Each SPU must have its own separate ring buffer, with no overlap.
		memset(&BufferInfo.ringInfo[SPUIndex], 0, sizeof(EdgeGeomRingBufferInfo));
		BufferInfo.ringInfo[SPUIndex].startEa = (UINT)RingBuffers + SPUIndex * InRingBufferSizePerSpu;
		BufferInfo.ringInfo[SPUIndex].endEa = BufferInfo.ringInfo[SPUIndex].startEa + InRingBufferSizePerSpu;
		BufferInfo.ringInfo[SPUIndex].currentEa = BufferInfo.ringInfo[SPUIndex].startEa;
		BufferInfo.ringInfo[SPUIndex].locationId = (InMemoryRegion == MR_Host) ? CELL_GCM_LOCATION_MAIN : CELL_GCM_LOCATION_LOCAL;
		BufferInfo.ringInfo[SPUIndex].rsxLabelEa = (UINT)cellGcmGetLabelAddress(LABEL_EDGE_SPU0 + SPUIndex);
		BufferInfo.ringInfo[SPUIndex].cachedFree = 0;
		cellGcmAddressToOffset((void*)BufferInfo.ringInfo[SPUIndex].startEa, &BufferInfo.ringInfo[SPUIndex].startOffset);
		// RSX label value must be initialized to the end of the buffer
		*(UINT*)BufferInfo.ringInfo[SPUIndex].rsxLabelEa = BufferInfo.ringInfo[SPUIndex].endEa;
	}			
}

FEdgeOutputBuffer::~FEdgeOutputBuffer()
{
	if (BufferInfo.sharedInfo.locationId == CELL_GCM_LOCATION_MAIN)
	{
		GPS3Gcm->GetHostAllocator()->Free(RingBuffers);
	}
	else
	{
		GPS3Gcm->GetLocalAllocator()->Free(RingBuffers);
	}
}
#endif // RHI_SUPPORTS_PREVERTEXSHADER_CULLING

#endif // USE_PS3_RHI
