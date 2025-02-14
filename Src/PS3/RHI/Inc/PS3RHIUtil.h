/*=============================================================================
	PS3RHIUtil.h: PS3 RHI utility definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if USE_PS3_RHI

#ifndef __PS3RHIUTIL_H__
#define __PS3RHIUTIL_H__


//
// Globals
//

/**
 * Initialize the PS3 graphics system as early in the boot up process as possible.
 * This will make it possible to display loading screens, etc, before a
 * Viewport is created.
 *
 * @return TRUE if successful
 */
extern UBOOL PS3RenderStaticInit();

/**
 * Draw a full screen loading screen, without actually using the RHI
 * Located in the RHI code because all rendering occurs here
 *
 * @param Filename Path to the screen to load
 * @return TRUE if successful
 */
extern UBOOL PS3DrawLoadingScreen(const TCHAR* Filename);

/**
 * Log the current texture memory stats.
 *
 * @param Message	This text will be included in the log
 */
void appDumpTextureMemoryStats(const TCHAR* Message);


/**
 * The base class of PS3RHI resources.
 * Currently just implements a FRefCountedObject
 */
//class FPS3RHIResource : public virtual FRefCountedObject
//{
//};
typedef FRefCountedObject FPS3RHIResource;

/**
 * Memory regions that the GPU can access
 */
enum EMemoryRegion
{
	MR_Bad		= -1,
	MR_Local	= CELL_GCM_LOCATION_LOCAL,
	MR_Host		= CELL_GCM_LOCATION_MAIN,
};



/*
 * Adds an unused Xenon GPU resource to a buffer so that it can be deleted properly at a later time.
 *
 * @param BaseAddress	- Memory to be deleted.
 * @param MemoryRegion	- Where the memory was allocated
 * @param bIsTexture	- TRUE if resource is a texture.
 * @param bUsesResourceArray - TRUE if BaseAddress of this resource comes from a TResourceArray [UNUSED for now]
 */
void AddUnusedPS3Resource(void* BaseAddress, EMemoryRegion MemoryRegion, UBOOL bIsTexture, UBOOL bUsesResourceArray);

/*
 * Deletes all resources that became unused during the previous frame. It also swaps the two buffers.
 * This function should be called once at the end of each frame.
 */
void DeleteUnusedPS3Resources();



/**
 * A resource that has GPU-accessible memory (in host or local mem)
 */
class FPS3RHIGPUResource : public FPS3RHIResource
{
public:
	/**
	 * Constructor - basic initialization
	 */
	FPS3RHIGPUResource();

	/**
	 * Constructor - attaches to existing memory
	 */
	FPS3RHIGPUResource(DWORD Offset, EMemoryRegion Region);

	/**
	 * Constructor - attaches to a resource array
	 */
	FPS3RHIGPUResource(FResourceArrayInterface* ResourceArray);

	/**
	 * Allocate RSX-usable memory in one of the two memory regions
	 *
	 * @param Size			How much to allocation
	 * @param Region		Which memory region to allocate in
	 * @param AllocationType The type of allocation (texture, vertex buffer, etc)
	 * @param Alignment		The alignment for the allocation (default to 128)
	 */
	UBOOL AllocateMemory(DWORD Size, EMemoryRegion Region, EAllocationType AllocationType, DWORD Alignment=128, FResourceBulkDataInterface* BulkData=NULL);

	/**
	 * Destructor - deallocates memory if necessary 
	 */
	virtual ~FPS3RHIGPUResource();

	/**
	 * Set our data to the given pointer
	 * @param Data	A pointer to vertex data, assumed to be the proper size set in the constructor
	 */
	void SetData(const void* InData);

	/** Returns the base address to this resource (FPS3RHIGPUResource::Data, in CPU address space). */
	const void* GetBaseAddress() const
	{
		return Data;
	}

	/** Returns the base address to this resource (FPS3RHIGPUResource::Data, in CPU address space). */
	void* GetBaseAddress()
	{
		return Data;
	}

	/** The EA the data (usable by allocator) */
	void*		Data;
	/** The offset of the data pointer into the MemoryRegion */
	UINT		MemoryOffset;
	/** The region the data lives in, either local or host */
	INT			MemoryRegion;
	/** Does the destructor need to clean up memory? */
	BITFIELD	bFreeMemory : 1;
	/** Does the memory point into a resource array? (freed specially) */
	BITFIELD	bUsesResourceArray : 1;
	/** Whether the memory was allocated in the texture pool. */
	BITFIELD	bUsesTexturePool : 1;
	/** Whether the resource uses a tiled register. */
	BITFIELD	bUsesTiledRegister : 1;
	/** The type of resource (used to know what pool/heap to free from) */
	EAllocationType AllocationType;
	/** The size allocated by AllocateMemory. Used for DMAing data */
	DWORD		AllocatedSize;
};

/**
 * A resource that POSSIBLY has two GPU-accessible memory buffers
 */
class FPS3RHIDoubleBufferedGPUResource : public FPS3RHIGPUResource
{
public:
	/**
	* Constructor - allocates one or two buffers of the given size in the given memory region
	*/
	FPS3RHIDoubleBufferedGPUResource(DWORD Size, EMemoryRegion Region, EAllocationType InAllocationType, UBOOL bNeedsTwoBuffers, DWORD Alignment = DEFAULT_ALIGNMENT);

	/**
	 * Constructor - attaches to a resource array
	 */
	FPS3RHIDoubleBufferedGPUResource(FResourceArrayInterface* ResourceArray);

	/**
	 * Destructor - deallocates memory if necessary 
	 */
	virtual ~FPS3RHIDoubleBufferedGPUResource();
	
	/**
	 * Swap the current buffer, and update the FPS3RHIGPUResource's Data and MemoryOffset accordingly
	 */
	void SwapBuffers();

protected:

	/** Which of the current buffers is active */
	DWORD CurrentBuffer;

	/** Pointers to one or two buffers (depending on dynamic or not) */
	void* Buffers[2];

	/** Offsets of one or two buffers */
	UINT Offsets[2];
};	

/**
 * Encapsulates a reference to a FPS3RHIResource
 */
template<typename ReferencedType>
class TPS3ResourceRef
{
	typedef ReferencedType* ReferenceType;
public:

	TPS3ResourceRef():
		Reference(NULL)
	{}

	TPS3ResourceRef(ReferencedType* InReference, UBOOL bAddRef = TRUE)
	{
		Reference = InReference;
		if(Reference && bAddRef)
		{
			Reference->AddRef();
		}
	}

	TPS3ResourceRef(const TPS3ResourceRef& Copy)
	{
		Reference = Copy.Reference;
		if(Reference)
		{
			Reference->AddRef();
		}
	}

	~TPS3ResourceRef()
	{
		if(Reference)
		{
			Reference->Release();
		}
	}

	TPS3ResourceRef& operator=(ReferencedType* InReference)
	{
		ReferencedType* OldReference = Reference;
		Reference = InReference;
		if(Reference)
		{
			Reference->AddRef();
		}
		if(OldReference)
		{
			OldReference->Release();
		}
		return *this;
	}

	TPS3ResourceRef& operator=(const TPS3ResourceRef& InPtr)
	{
		return *this = InPtr.Reference;
	}

	UBOOL operator==(const TPS3ResourceRef& Other) const
	{
		return Reference == Other.Reference;
	}

	ReferencedType* operator->() const
	{
		return Reference;
	}

	operator ReferenceType() const
	{
		return Reference;
	}

	ReferencedType** GetInitReference()
	{
		*this = NULL;
		return &Reference;
	}

	// RHI reference interface.
	template<typename T>
	friend UBOOL IsValidRef(const TPS3ResourceRef<T>& Ref);

	void SafeRelease()
	{
		*this = NULL;
	}

	DWORD GetRefCount()
	{
		if(Reference)
		{
			return Reference->GetRefCount();
		}
		else
		{
			return 0;
		}
	}

protected:
	ReferencedType* Reference;
};

// global decl for this friend function (for gcc 4.1.1)
template<typename ReferencedType>
UBOOL IsValidRef(const TPS3ResourceRef<ReferencedType>& Ref)
{
	return Ref.Reference != NULL;
}

FORCEINLINE UBOOL IsValidRef(const FPS3RHIResource* Ref)
{
	return Ref != NULL;
}

/**
 * Class to handle a fenced, partitioned ring buffer
 */
struct FFencedRingBuffer
{
	/**
	 * Constructor
	 *
	 * @param InBufferSize Size of total buffer (must be a multiple of InNumPartitions)
	 * @param InNumPartitions NUmber of partitions in the buffer 
	 * @param InAlignment Amount to align each allocation
	 * @param InMemoryRegion Location to allocate the buffer
	 */
	FFencedRingBuffer(INT InBufferSize, INT InNumPartitions, INT InAlignment, EMemoryRegion InMemoryRegion);

	/**
	 * Allocate space in the ring buffer, using fences to make sure GPU is not using the data
	 *
	 * @param Size Amount to allocate
	 * 
	 * @return Allocated data
	 */
	void* Malloc(INT Size);

	/** Convert an allocated pointer to a GPU offset */
	inline DWORD PointerToOffset(void* Pointer)
	{
		return RingBufferOffset + ((BYTE*)Pointer - (BYTE*)RingBufferData);
	}

protected:
	/** Memory for the ring buffer */
	void* RingBufferData;

	/** Gcm memory offset into the memory region for the buffer */
	UINT RingBufferOffset;

	/** Current offset into the ring buffer */
	INT CurrentOffset;

	/** Array of fences for each partition */
	DWORD* Fences;

	/** Which partition we are in */
	INT CurrentPartition;

	/** How many partitions the data is broken into */
	INT NumPartitions;

	/** Size of each partition */
	INT PartitionSize;

	/** Alignment for each allocation */
	INT Alignment;
};

#if RHI_SUPPORTS_PREVERTEXSHADER_CULLING
#include "edge/geom/edgegeom_structs.h"
struct FEdgeOutputBuffer
{
	FEdgeOutputBuffer(INT InRingBufferSizePerSpu, EMemoryRegion InMemoryRegion);
	~FEdgeOutputBuffer();

	EdgeGeomOutputBufferInfo *GetOutputBufferInfo(void) { return &BufferInfo; }

protected:
	BYTE*	RingBuffers;
	EdgeGeomOutputBufferInfo BufferInfo;
};
#endif


#endif // __PS3RHIUTIL_H__

#endif // USE_PS3_RHI

