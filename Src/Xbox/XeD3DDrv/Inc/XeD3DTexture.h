/*=============================================================================
	XeD3DTexture.h: XeD3D texture RHI definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if USE_XeD3D_RHI


/** base texture resource */
class FXeTextureBase : public FXeGPUResource
{
public:
	/** default constructor */
	FXeTextureBase(DWORD InFlags=0)
	:	FXeGPUResource(RUF_Static)
	,	ResourceSize(0)
	,	Fence(0)
	,	ReallocationRequest(NULL)
	,	CreationFlags(InFlags)
	,	LockedMipLevels(0)
	,	bReallocationOwner(FALSE)
	,	ReallocatedTexture(NULL)
	,	Format( PF_Unknown )
	{
		bIsTexture = TRUE;
	}

	/** constructor initialized w/ existing D3D resource */
	FXeTextureBase(IDirect3DBaseTexture9* InResource,EPixelFormat InFormat,DWORD InFlags,DWORD InResourceSize,FAsyncReallocationRequest* InReallocationRequest=NULL)
	:	FXeGPUResource(InResource, RUF_Static)
	,	ResourceSize(InResourceSize)
	,	Fence(0)
	,	ReallocationRequest(InReallocationRequest)
	,	CreationFlags(InFlags)
	,	LockedMipLevels(0)
	,	bReallocationOwner(FALSE)
	,	ReallocatedTexture(NULL)
	,	Format( InFormat )
	{
		bIsTexture = TRUE;
	}

	/** Destructor */
	virtual ~FXeTextureBase();

	/** Returns the ETextureCreateFlags used when this texture was created. */
	FORCEINLINE DWORD GetCreationFlags()
	{
		return CreationFlags;
	}

	/** Returns the number of bytes used by the texture (including all mips), for textures that reside in the texture pool. 0 otherwise. */
	DWORD	GetResourceSize() const
	{
		return ResourceSize;
	}

	/**
	 *	Returns whether this texture is locked or not.
	 *	@return		TRUE if this texture is locked
	 */
	UBOOL IsLocked() const
	{
		return LockedMipLevels != 0;
	}

	/**
	 * Locks the resource, blocks if texture memory is currently being defragmented.
	 * @param MipIndex	Mip-level to lock
	 */
	void Lock( INT MipIndex )
	{
		if ( LockedMipLevels == 0 && BaseAddress )
		{
			GTexturePool.Lock( BaseAddress );
		}
		LockedMipLevels |= GBitFlag[MipIndex];
	}

	/**
	 * Unlocks the resource.
	 * @param MipIndex	Mip-level to unlock
	 */
	void Unlock( DWORD MipIndex )
	{
		LockedMipLevels &= ~GBitFlag[MipIndex];
		if ( LockedMipLevels == 0 && BaseAddress )
		{
			GTexturePool.Unlock( BaseAddress );
		}
	}

	/** Checks whether this texture can be relocated or not by the defragmentation process. */
	UBOOL	CanRelocate() const;

	/** Returns the Unreal EPixelFormat of the texture */
	EPixelFormat GetUnrealFormat() const
	{
		return Format;
	}

	/** Returns the current GPU base address for this resource. */
	void*	GetBaseAddress() const
	{
		return BaseAddress;
	}

	/** Sets a new GPU base address for this resource. */
	void	SetBaseAddress( void* NewBaseAddress )
	{
		BaseAddress = NewBaseAddress;
	}

	/**
	 *	Notifies that the texture data is being reallocated and is shared for a brief moment,
	 *	so that this texture can do the right thing upon destruction.
	 *
	 *	@param ReallocatedTexture	- The other texture that briefly shares the texture data when it's reallocated
	 *	@param bReallocationOwner	- TRUE if 'ReallocatedTexture' refers to the old texture
	 **/
	void SetReallocatedTexture( FXeTextureBase* ReallocatedTexture, UBOOL bReallocationOwner );

	/**
	 * Returns the status of an ongoing or completed texture reallocation:
	 *	TexRealloc_Succeeded	- The texture is ok, reallocation is not in progress.
	 *	TexRealloc_Failed		- The texture is bad, reallocation is not in progress.
	 *	TexRealloc_InProgress	- The texture is currently being reallocated async.
	 *
	 * @param Texture2D		- Texture to check the reallocation status for
	 * @return				- Current reallocation status
	 */
	ETextureReallocationStatus GetReallocationStatus() const;

	/**
	 * Called by the texture allocator when an async reallocation request has been completed.
	 *
	 * @param OldTexture		Original texture. OldTexture->ReallocatedTexture refers to the new texture.
	 * @param FinishedRequest	The request that got completed
	 */
	static void OnReallocationFinished( FXeTextureBase* OldTexture, FAsyncReallocationRequest* FinishedRequest );

	/**
	 * Finalizes an async reallocation for the specified texture.
	 * This should be called for the new texture, not the original.
	 *
	 * @param Texture				Texture to finalize
	 * @param bBlockUntilCompleted	If TRUE, blocks until the finalization is fully completed
	 * @return						Reallocation status
	 */
	static ETextureReallocationStatus FinalizeAsyncReallocateTexture2D( FXeTextureBase* Texture, UBOOL bBlockUntilCompleted );

	/**
	 * Cancels an async reallocation for the specified texture.
	 * This should be called for the new texture, not the original.
	 *
	 * @param Texture				Texture to cancel
	 * @param bBlockUntilCompleted	If TRUE, blocks until the cancellation is fully completed
	 * @return						Reallocation status
	 */
	static ETextureReallocationStatus CancelAsyncReallocateTexture2D( FXeTextureBase* Texture, UBOOL bBlockUntilCompleted );

	/**
	 * Associates the texture with the current GPU fence.
	 */
	void	LockGPUFence();

	/**
	 * Blocks until the associated GPU fence has been passed.
	 */
	void	UnlockGPUFence();

protected:
	/** Number of bytes used by the texture (including all mips), for textures that reside in the texture pool. 0 otherwise. */
	DWORD	ResourceSize;

	/** Used for synchronization with the GPU. */
	DWORD	Fence;

	/** Currently pending async reallocation. Must not be deleted until the texture pool manager is finished with it. */
	FAsyncReallocationRequest* ReallocationRequest;

private:
	/** Bitwise combination of flags from ETextureCreateFlags. */
	DWORD	CreationFlags;

	/** One bit for each mip-level. A "one" means that mip-level is locked. */
	DWORD	LockedMipLevels : 20;

	/** TRUE if this texture is being reallocated and is the new owner of the texture memory. 'ReallocatedTexture' points to old texture. */
	DWORD	bReallocationOwner : 1;

	/** Another texture that briefly shares the texture data when it's reallocated. */
	FXeTextureBase*	ReallocatedTexture;

	/** Unreal texture format */
	EPixelFormat Format;
};

/** 2D texture resource */
class FXeTexture2D : public FXeTextureBase
{
public:
	/** default constructor */
	FXeTexture2D(DWORD InFlags=0)
	:	FXeTextureBase(new IDirect3DTexture9,PF_Unknown,InFlags,0)
	{
	}

	/** constructor initialized w/ existing D3D resource */
	FXeTexture2D(IDirect3DTexture9* InResource,EPixelFormat InFormat,DWORD InFlags,DWORD InResourceSize,FAsyncReallocationRequest* InReallocationRequest=NULL)
	:	FXeTextureBase(InResource,InFormat,InFlags,InResourceSize,InReallocationRequest)
	{
	}

	/**
	 * Sets D3D header and allocates memory for a D3D resource 
	 *
	 * @param SizeX			- width of texture
	 * @param SizeY			- height of texture
	 * @param Format		- format of texture
	 * @param NumMips		- number of mips
	 * @param Flags			- creation flags
	 * @param Texture		- [in/out optional] IDirect3DTexture9 header to initialize
	 * @param OutMemorySize	- [out] Memory size of the allocation in bytes
	 * @param BulkData		- [optional] pre-allocated memory containing the bulk data
	 * @return				- Base address of the allocation
	 */
	static void* AllocTexture2D(UINT SizeX,UINT SizeY,EPixelFormat Format,UINT NumMips,DWORD Flags,IDirect3DTexture9* Texture,DWORD& OutMemorySize,FResourceBulkDataInterface* BulkData);

	/** 
	 * Tries to add or remove mip-levels of the texture without relocation.
	 *
	 * @param Texture2D		- Source texture
	 * @param NewMipCount	- Number of desired mip-levels
	 * @param NewSizeX		- New width, in pixels
	 * @param NewSizeY		- New height, in pixels
	 * @return				- A newly allocated FXeTexture2D if successful, or NULL if it failed.
	 */
	static FXeTexture2D* ReallocateTexture2D( FXeTexture2D* Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY );

	/**
	 * Requests an async reallocation of the specified texture.
	 * Returns a new texture that represents if the request was accepted.
	 *
	 * @param Texture2D		Texture to reallocate
	 * @param NewMipCount	New number of mip-levels (including the base mip)
	 * @param NewSizeX		New width, in pixels
	 * @param NewSizeY		New height, in pixels
	 * @param RequestStatus	Thread-safe counter to monitor the status of the request. Decremented by one when the request is completed.
	 * @return				Newly created FXeTexture2D if the request was accepted, or NULL
	 */
	static FXeTexture2D* AsyncReallocateTexture2D( FXeTexture2D* Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, FThreadSafeCounter* RequestStatus );

	/**
	 * @return Resource type name.
	 */
	virtual TCHAR* GetTypeName() const
	{
		return TEXT("Texture2D");
	}

	/** Dynamic cast to a shared texture */
	virtual const class FXeSharedTexture2D* const GetSharedTexture() const
	{
		return NULL;
	}
};

/** cube texture resource */
class FXeTextureCube : public FXeTextureBase
{
public:
	/** default constructor */
	FXeTextureCube(EPixelFormat InFormat,DWORD InFlags=0)
	:	FXeTextureBase(new IDirect3DCubeTexture9,InFormat,InFlags,0)
	{
	}

	/** constructor initialized w/ existing D3D resource */
	FXeTextureCube(IDirect3DCubeTexture9* InResource,EPixelFormat InFormat,DWORD InFlags=0)
	:	FXeTextureBase(InResource,InFormat,InFlags,0)
	{
	}

	/** 
	 * Sets D3D header and allocates memory for the resource 
	 *
	 * @param	Size - width of texture
	 * @param	Format - format of texture
	 * @param	NumMips - number of mips
	 * @param	Flags - creation flags
	 */
	void AllocTextureCube(UINT Size,EPixelFormat Format,UINT NumMips,DWORD Flags);

	/**
	 * @return Resource type name.
	 */
	virtual TCHAR* GetTypeName() const
	{
		return TEXT("TextureCube");
	}
};

/** Array texture resource */
class FXeTextureArray : public FXeTextureBase
{
public:
	/** default constructor */
	FXeTextureArray(EPixelFormat InFormat, DWORD InFlags=0)
	:	FXeTextureBase(new IDirect3DArrayTexture9,InFormat,InFlags,0)
	{
	}

	/** constructor initialized w/ existing D3D resource */
	FXeTextureArray(IDirect3DArrayTexture9* InResource, EPixelFormat InFormat, DWORD InFlags=0)
	:	FXeTextureBase(InResource,InFormat,InFlags,0)
	{
	}

	/**
	 * @return Resource type name.
	 */
	virtual TCHAR* GetTypeName() const
	{
		return TEXT("TextureArray");
	}

	/** Dynamic cast to a shared texture */
	virtual const class FXeSharedTextureArray* const GetSharedTexture() const
	{
		return NULL;
	}
};

class FXeSharedTexture2D : public FXeTexture2D
{
public:
	/** default constructor */
	FXeSharedTexture2D(void)
	{
	}

	/** constructor initialized w/ existing D3D resource  */
	FXeSharedTexture2D(IDirect3DTexture9* InResource, EPixelFormat InFormat, FSharedMemoryResourceRHIRef InMemory)
	:	FXeTexture2D(InResource,InFormat,0,0)
	,	Memory(InMemory)
	{
	}

	/** destructor */
	virtual ~FXeSharedTexture2D()
	{
		// This is required because of the shared memory member variable which
		// must have its destructor called.
	}

	/** Dynamic cast to a shared texture */
	virtual const FXeSharedTexture2D* const GetSharedTexture() const
	{
		return this;
	}

	/** Accessor for the shared memory reference */
	const TRefCountPtr<FXeSharedMemoryResource> GetSharedMemory() const
	{
		return Memory;
	}

private:
	FSharedMemoryResourceRHIRef Memory;
};


/*-----------------------------------------------------------------------------
	RHI texture types
-----------------------------------------------------------------------------*/
typedef TXeGPUResourceRef<IDirect3DBaseTexture9,FXeTextureBase> FTextureRHIRef;

typedef FXeTextureBase* FTextureRHIParamRef;

class FTexture2DRHIRef : public TXeGPUResourceRef<IDirect3DTexture9,FXeTexture2D>
{
public:

	/** Default constructor. */
	FTexture2DRHIRef() {}

	/** Initialization constructor. */
	FTexture2DRHIRef(FXeTexture2D* InResource): TXeGPUResourceRef<IDirect3DTexture9,FXeTexture2D>(InResource)
	{}

	/** Conversion to base texture reference operator. */
	operator FTextureRHIRef() const
	{
		return FTextureRHIRef((FXeTextureBase*)*this);
	}
};

typedef FXeTexture2D* FTexture2DRHIParamRef;

class FTextureCubeRHIRef : public TXeGPUResourceRef<IDirect3DCubeTexture9,FXeTextureCube>
{
public:
	
	/** Default constructor. */
	FTextureCubeRHIRef() {}

	/** Initialization constructor. */
	FTextureCubeRHIRef(FXeTextureCube* InResource): TXeGPUResourceRef<IDirect3DCubeTexture9,FXeTextureCube>(InResource)
	{}

	/** Conversion to base texture reference operator. */
	operator FTextureRHIRef() const
	{
		return FTextureRHIRef((FXeTextureBase*)*this);
	}
};

typedef FXeTextureCube* FTextureCubeRHIParamRef;


class FTexture2DArrayRHIRef : public TXeGPUResourceRef<IDirect3DArrayTexture9,FXeTextureArray>
{
public:

	/** Default constructor. */
	FTexture2DArrayRHIRef() {}

	/** Initialization constructor. */
	FTexture2DArrayRHIRef(FXeTextureArray* InResource): TXeGPUResourceRef<IDirect3DArrayTexture9,FXeTextureArray>(InResource)
	{}

	/** Conversion to base texture reference operator. */
	operator FTextureRHIRef() const
	{
		return FTextureRHIRef((FXeTextureBase*)*this);
	}
};

typedef FXeTextureArray* FTexture2DArrayRHIParamRef;


class FSharedTexture2DRHIRef : public TXeGPUResourceRef<IDirect3DTexture9,FXeSharedTexture2D>
{
public:

	/** Default constructor. */
	FSharedTexture2DRHIRef() {}

	/** Initialization constructor. */
	FSharedTexture2DRHIRef(FXeSharedTexture2D* InResource): TXeGPUResourceRef<IDirect3DTexture9,FXeSharedTexture2D>(InResource)
	{}

	/** Conversion to base texture reference operator. */
	operator FTextureRHIRef() const
	{
		return FTextureRHIRef((FXeTextureBase*)*this);
	}

	/** Conversion to base texture reference operator. */
	operator FTexture2DRHIRef() const
	{
		return FTexture2DRHIRef((FXeTexture2D*)*this);
	}
};

typedef FXeTexture2D* FTexture2DRHIParamRef;

/**
 * Allocates memory for directly loading texture mip data 
 */
class FXeTexture2DResourceMem : public FTexture2DResourceMem
{
public:	
	/** 
	 * Init Constructor 
	 * Allocates texture memory during construction
	 * 
	 * @param InSizeX			- width of texture
	 * @param InSizeY			- height of texture
	 * @param InNumMips			- total number of mips to allocate memory for
	 * @param InFormat			- EPixelFormat texture format
	 * @param InTexCreateFlags	- ETextureCreateFlags bit flags
	 * @param AsyncCounter		- If provided, starts an async allocation. If NULL, allocates memory immediately.
	 */
	FXeTexture2DResourceMem(INT InSizeX, INT InSizeY, INT InNumMips, EPixelFormat InFormat, DWORD InTexCreateFlags, FThreadSafeCounter* AsyncCounter);
	/** 
	 * Destructor
	 */
	virtual ~FXeTexture2DResourceMem();

	// FResourceBulkDataInterface interface	
	
	/** 
	 * @return ptr to the resource memory which has been preallocated
	 */
	virtual void* GetResourceBulkData() const;
	/** 
	 * @return size of resource memory
	 */
	virtual DWORD GetResourceBulkDataSize() const;
	/**
	 * Free memory after it has been used to initialize RHI resource 
	 */
	virtual void Discard();

	// FTexture2DResourceMem interface

	/**
	 * @param MipIdx index for mip to retrieve
	 * @return ptr to the offset in bulk memory for the given mip
	 */
	virtual void* GetMipData(INT MipIdx);
	/**
	 * @return total number of mips stored in this resource
	 */
	virtual INT	GetNumMips();
	/** 
	 * @return width of texture stored in this resource
	 */
	virtual INT GetSizeX();
	/** 
	 * @return height of texture stored in this resource
	 */
	virtual INT GetSizeY();
	/** 
	 * @return Size of the texture in bytes.
	 */
	virtual INT GetTextureSize()
	{
		return TextureSize;
	}
	/**
	 * Whether the resource memory is properly allocated or not.
	 **/
	virtual UBOOL IsValid()
	{
		return GTexturePool.IsValidTextureData(BaseAddress);
	}

	/**
	 * @return Whether the resource memory has an async allocation request and it's been completed.
	 */
	virtual UBOOL HasAsyncAllocationCompleted() const
	{
		if ( ReallocationRequest )
		{
			return ReallocationRequest->HasCompleted();
		}
		return TRUE;
	}

	/**
	 * Blocks the calling thread until the allocation has been completed.
	 */
	virtual void FinishAsyncAllocation();

	/**
	 * Cancels any async allocation.
	 */
	virtual void CancelAsyncAllocation();

private:
	/** Use init constructor */
	FXeTexture2DResourceMem()
	{
	}

	/** 
	 * Calculate size needed to store this resource and allocate texture memory for it
	 *
	 * @param bAllowRetry	If TRUE, retry the allocation after panic-stream-out/panic-defrag upon failure
	 */
	void	AllocateTextureMemory( UBOOL bAllowRetry );

	/**
	 * Free the memory from the texture pool
	 */
	void	FreeTextureMemory();

	/** width of texture */
	INT		SizeX;
	/** height of texture */
	INT		SizeY;
	/** total number of mips of texture */
	INT		NumMips;
	/** format of texture */
	EPixelFormat Format;
	/** ptr to the allocated memory for all mips */
	void*	BaseAddress;
	/** size of the allocated memory for all mips */
	DWORD	TextureSize;
	/** ETextureCreateFlags bit flags */
	DWORD	TexCreateFlags;
	/** D3D texture header to describe the texture memory */
	IDirect3DTexture9 TextureHeader;

	/** Currently pending async allocation. Must not be deleted until the texture pool manager is finished with it. */
	FAsyncReallocationRequest* ReallocationRequest;
};


/**
 * Checks if the texture data is allocated within the texture pool or not.
 */
extern UBOOL appIsPoolTexture( FTextureRHIParamRef TextureRHI );


#endif
