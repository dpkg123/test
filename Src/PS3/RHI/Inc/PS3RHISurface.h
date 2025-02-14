/*=============================================================================
	PS3RHISurface.h: PS3 surface/render target RHI definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if USE_PS3_RHI

#ifndef __PS3RHISURFACE_H__
#define __PS3RHISURFACE_H__

////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////

class FAsyncReallocationRequest;

////////////////////////////////////////////////////
// RHI texture references
////////////////////////////////////////////////////
typedef TPS3ResourceRef<class FPS3RHISurface> FSurfaceRHIRef;
typedef FPS3RHISurface* FSurfaceRHIParamRef;

class FTextureRHIRef : public TPS3ResourceRef<class FPS3RHITexture>
{
public:
	/** Default constructor. */
	FTextureRHIRef() {}

	/** Initialization constructor. */
	FTextureRHIRef(class FPS3RHITexture* InResource): TPS3ResourceRef<class FPS3RHITexture>(InResource)
	{}
};

typedef FPS3RHITexture* FTextureRHIParamRef;

class FTexture2DRHIRef : public FTextureRHIRef
{
public:
	/** Default constructor. */
	FTexture2DRHIRef() {}

	/** Initialization constructor. */
	FTexture2DRHIRef(class FPS3RHITexture2D* InResource) : FTextureRHIRef((class FPS3RHITexture*)InResource)
	{}

	class FPS3RHITexture2D* operator->() const
	{
		return (class FPS3RHITexture2D*) Reference;
	}

	operator class FPS3RHITexture2D*() const
	{
		return (class FPS3RHITexture2D*) Reference;
	}

	class FPS3RHITexture2D* GetReferenced() const
	{
		return (class FPS3RHITexture2D*) Reference;
	}
};

typedef FPS3RHITexture2D* FTexture2DRHIParamRef;
// GEMINI_TODO - implement shared PS3 resource
typedef FTexture2DRHIRef FSharedTexture2DRHIRef;
typedef FTexture2DRHIParamRef FSharedTexture2DRHIParamRef;

class FTextureCubeRHIRef : public FTextureRHIRef
{
public:
	
	/** Default constructor. */
	FTextureCubeRHIRef() {}

	/** Initialization constructor. */
	FTextureCubeRHIRef(class FPS3RHITextureCube* InResource) : FTextureRHIRef((class FPS3RHITexture*)InResource)
	{}

	class FPS3RHITextureCube* operator->() const
	{
		return (class FPS3RHITextureCube*) Reference;
	}

	operator class FPS3RHITextureCube*() const
	{
		return (class FPS3RHITextureCube*) Reference;
	}
};

typedef FPS3RHITextureCube* FTextureCubeRHIParamRef;


////////////////////////////////////////////////////
// FPS3RHITexture: base class for all textures
////////////////////////////////////////////////////
class FPS3RHITexture : public FPS3RHIGPUResource
{
public:
	/** Constructor */
	FPS3RHITexture();
	/** Constructor initialized from another texture but with a new Gcm texture header */
	FPS3RHITexture(FPS3RHITexture* OtherTexture, const CellGcmTexture& NewGcmHeader,void* NewBaseAddress, DWORD NewTextureSize,FAsyncReallocationRequest* InReallocationRequest=NULL);
	/** Destructor */
	virtual ~FPS3RHITexture();

	// accessors
	inline const CellGcmTexture& GetTexture() const	{ return Texture; }
	inline EPixelFormat GetUnrealFormat() const	{ return PixelFormat; }
	inline DWORD GetFormat() const				{ return Texture.format; }
	inline DWORD GetSizeX() const				{ return Texture.width; }
	inline DWORD GetSizeY() const				{ return Texture.height; }
	inline DWORD GetPitch() const				{ return Texture.pitch; }
	inline DWORD GetNumMipmaps() const			{ return Texture.mipmap; }
	inline UBOOL IsLinear() const				{ return (Texture.format & CELL_GCM_TEXTURE_LN) ? TRUE : FALSE; }
	inline UBOOL IsSRGB() const					{ return bIsSRGB; }
	inline DWORD GetCreationFlags() const		{ return CreationFlags; }
	inline UBOOL IsLocked() const				{ return LockedMipLevels != 0; }
	inline UBOOL IsCubemap() const				{ return (Texture.cubemap == CELL_GCM_TRUE) ? TRUE : FALSE; }
	inline DWORD GetUnsignedRemap() const		{ return UnsignedRemap; }

	/**
	 * This is used when swapping a texture and a surface, we need to update
	 * the offset stored in the texture structure 
	 *
	 * @param MemoryOffset Memory that was previously a surface memory location
	 */
	void	SetMemoryOffset( DWORD InMemoryOffset );

	/** Locks the resource, blocks if texture memory is currently being defragmented. */
	void	Lock( INT MipIndex );

	/** Unlocks the resource. */
	void	Unlock( INT MipIndex );

	/** Checks whether this texture can be relocated or not by the defragmentation process. */
	UBOOL	CanRelocate() const;

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
	static void OnReallocationFinished( FPS3RHITexture* OldTexture, FAsyncReallocationRequest* FinishedRequest );

	/**
	 * Finalizes an async reallocation for the specified texture.
	 * This should be called for the new texture, not the original.
	 *
	 * @param Texture				Texture to finalize
	 * @param bBlockUntilCompleted	If TRUE, blocks until the finalization is fully completed
	 * @return						Reallocation status
	 */
	static ETextureReallocationStatus FinalizeAsyncReallocateTexture2D( FPS3RHITexture* Texture, UBOOL bBlockUntilCompleted );

	/**
	 * Cancels an async or immediate reallocation for the specified texture.
	 * This should be called for the new texture, not the original.
	 *
	 * @param Texture				Texture to cancel
	 * @param bBlockUntilCompleted	If TRUE, blocks until the cancellation is fully completed
	 * @return						Reallocation status
	 */
	static ETextureReallocationStatus CancelAsyncReallocateTexture2D( FPS3RHITexture* Texture, UBOOL bBlockUntilCompleted );

	/**
	 *	Notifies that the texture data is being reallocated and is shared for a brief moment,
	 *	so that this texture can do the right thing upon destruction.
	 *
	 *	@param ReallocatedTexture	- The other texture that briefly shares the texture data when it's reallocated
	 *	@param bReallocationOwner	- TRUE if 'ReallocatedTexture' refers to the old texture
	 **/
	void	SetReallocatedTexture( FPS3RHITexture* ReallocatedTexture, UBOOL bReallocationOwner );

protected:
	/**
	 * Initialize internal structures of the texture
	 * @param SizeX - width of texture
	 * @param SizeY - height of texture
	 * @param Format - Unreal texture format
	 * @param NumMips - number of mips to allocate room for
	 * @param bIsLinearTexture - force linear texture, independent of format or size
	 * @param bIsCubemap - is the texture a cubemap
	 * @param bInIsSRGB - is the texture in sRGB space
	 * @param BulkData - preallocated memory for all mips of a texture resource
	 */
	void InitializeInternals(DWORD SizeX, DWORD SizeY, EPixelFormat Format, DWORD NumMips, UBOOL bIsLinearTexture, DWORD Flags, UBOOL bIsCubemap, UBOOL bInIsSRGB, UBOOL bBiasNormalMap, FResourceBulkDataInterface* BulkData);

	/** Texture descriptor */
	CellGcmTexture		Texture;

	/** Unreal format */
	EPixelFormat		PixelFormat;

	/** Unsigned remap (range 0 to 1 or -1 to 1): CELL_GCM_TEXTURE_UNSIGNED_REMAP_BIASED or CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL. */
	DWORD				UnsignedRemap;

	/** Bitwise combination of flags from ETextureCreateFlags. */
	DWORD				CreationFlags;

	/** Another texture that briefly shares the texture data when it's reallocated. */
	FPS3RHITexture*		ReallocatedTexture;

	/** Currently pending async reallocation. Must not be deleted until the texture pool manager is finished with it. */
	FAsyncReallocationRequest* ReallocationRequest;

	/** Whether the resource is locked or not. */
	DWORD				LockedMipLevels : 20;

	/** sRGB space */
	DWORD				bIsSRGB : 1;

	/** TRUE if this texture is being reallocated and is the new owner of the texture memory. 'ReallocatedTexture' points to old texture. */
	DWORD				bReallocationOwner : 1;
};


////////////////////////////////////////////////////
// FPS3RHITexture2D: 2d texture class
////////////////////////////////////////////////////
class FPS3RHITexture2D : public FPS3RHITexture
{
	friend class FPS3RHISurface;
public:
	/**
	 * Constructor
	 */
	FPS3RHITexture2D(DWORD InSizeX, DWORD InSizeY, EPixelFormat InFormat, DWORD NumMips, UBOOL bIsLinearTexture, DWORD Flags, FResourceBulkDataInterface* BulkData);

	/** Constructor initialized from another texture but with a new Gcm texture header */
	FPS3RHITexture2D(FPS3RHITexture* OtherTexture, const CellGcmTexture& NewGcmHeader,void* NewBaseAddress, DWORD NewTextureSize,FAsyncReallocationRequest* InReallocationRequest)
	:	FPS3RHITexture(OtherTexture,NewGcmHeader,NewBaseAddress,NewTextureSize,InReallocationRequest)
	{
	}

	/** 
	 * Tries to add or remove mip-levels of the texture without relocation.
	 *
	 * @param Texture2D		Source texture
	 * @param NewMipCount	Number of desired mip-levels
	 * @param NewSizeX		New width, in pixels
	 * @param NewSizeY		New height, in pixels
	 * @return				A newly allocated FPS3RHITexture2D if successful, or NULL if it failed.
	 */
	static FPS3RHITexture2D* ReallocateTexture2D( FPS3RHITexture2D* Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY );

	/**
	 * Requests an async reallocation of the specified texture.
	 * Returns a new texture that represents if the request was accepted.
	 *
	 * @param Texture2D		Texture to reallocate
	 * @param NewMipCount	New number of mip-levels (including the base mip)
	 * @param NewSizeX		New width, in pixels
	 * @param NewSizeY		New height, in pixels
	 * @param RequestStatus	Thread-safe counter to monitor the status of the request. Decremented by one when the request is completed.
	 * @return				Newly created FPS3RHITexture2D if the request was accepted, or NULL
	 */
	static FPS3RHITexture2D* AsyncReallocateTexture2D( FPS3RHITexture2D* Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, FThreadSafeCounter* RequestStatus );

	/**
	 * Initializes a new Gcm texture header, based on an existing header and new settings.
	 *
	 * @param OldGcmHeader		Gcm texture header to base the new header on
	 * @param NewMipCount		New number of mip-levels (including base mip)
	 * @param NewSizeX			New width, in pixels
	 * @param NewSizeY			New height, in pixels
	 * @param NewGcmHeader		[out] Texture header to initialize
	 * @return					Number of bytes used by all mips for the new texture
	 */
	static INT SetTextureHeaderFrom( FPS3RHITexture2D* Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, CellGcmTexture& NewGcmHeader );

	// RHI Interface
	friend void* RHILockTexture2D(FTexture2DRHIParamRef Texture,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail);
	friend void RHIUnlockTexture2D(FTexture2DRHIParamRef Texture,UINT MipIndex,UBOOL bLockWithinMiptail);
};


////////////////////////////////////////////////////
// FPS3RHITextureCube: cubemap texture class
//////////////////////////////////////////////////// 
class FPS3RHITextureCube : public FPS3RHITexture
{
public:
	/**
	 * Constructor
	 */
	FPS3RHITextureCube(DWORD InSizeX, DWORD InSizeY, EPixelFormat InFormat, DWORD NumMips, UBOOL bIsLinearTexture, DWORD Flags, FResourceBulkDataInterface* BulkData);

	// RHI Interface
	friend void* RHILockTextureCube(FTextureCubeRHIRef Texture,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail);
	friend void RHIUnlockTextureCube(FTextureCubeRHIRef Texture,UINT MipIndex,UBOOL bLockWithinMiptail);
};





////////////////////////////////////////////////////
// FPS3RHISurface: Surface, either a color buffer or a depth buffer
////////////////////////////////////////////////////
class FPS3RHISurface : public FPS3RHIGPUResource
{
public:
	/** Constructor */
	FPS3RHISurface(DWORD InSizeX, DWORD InSizeY, EPixelFormat InFormat, FTexture2DRHIRef InResolveTarget, DWORD Flags, DWORD InMultisampleType=CELL_GCM_SURFACE_CENTER_1);

	/**
	 * Buffer state for a dual-buffered surface.
	 */
	struct FBufferState
	{
		/** GPU memory offset to the surface buffer. */
		UINT		SurfaceOffset;
		/** GPU memory offset to the resolve target texture buffer. */
		UINT		ResolveTargetOffset;
		/** GPU memory offset to the buffer currently used as texture. */
		UINT		TextureOffset;
	};

	/**
	 * Construct from a CellGcmSurface with surface info
	 * 
	 * @param AttachSurface		Surface descriptor to copy params from 
	 * @param bIsColorBuffer	If TRUE, copies the color buffer fom the Surface, otherwise copies depth buffer
	 */
	FPS3RHISurface(const CellGcmSurface* AttachSurface, UBOOL bIsColorBuffer);

	/**
	 * Construct from a CellGcmTexture
	 * 
	 * @param AttachTexture		Texture descriptor to copy params from 
	 */
	FPS3RHISurface(FTexture2DRHIRef AttachTexture);

	/** Cubemap constructor*/
	FPS3RHISurface(DWORD InSizeX, EPixelFormat InFormat, FTextureCubeRHIRef InResolveTarget, ECubeFace CubeFace, DWORD Flags, DWORD InMultisampleType=CELL_GCM_SURFACE_CENTER_1);

	/**
	 * Construct from a CubeTexture
	 * 
	 * @param AttachTexture		Texture descriptor to copy params from 
	 * @param CubeFace			Which face int he attached cubemap to update
	 */
	FPS3RHISurface(FTextureCubeRHIRef AttachTexture, ECubeFace CubeFace);

	/** Destructor */
	virtual ~FPS3RHISurface( );

	/**
	 * Swaps memory pointer with the resolve target.
	 **/
	void SwapWithResolveTarget();

	/**
	 * Makes the resolve target point to the same memory as the surface.
	 **/
	void UnifyWithResolveTarget();

	/**
	 * Makes the resolve target point its own memory again, different from the surface.
	 **/
	void DualBufferWithResolveTarget();

	/**
	 * Retrieves the current buffer state (i.e. which memory buffer is used for the surface and which is used for the resolve texture).
	 */
	FBufferState GetBufferState();

	/**
	 * Sets the current dual-buffer state.
	 */
	void SetBufferState( FBufferState BufferState );

	/** Basic surface parameters */
	// @todo: Can we make these protected by any chance?
	DWORD				SizeX;
	DWORD				SizeY;
	DWORD				Format;
	DWORD				MultisampleType;
	UINT				Pitch;

	/** Original memory offset of the Resolve Target. */
	DWORD				ResolveTargetMemoryOffset;

	/** Texture to resolve to */
	FTexture2DRHIRef	ResolveTarget;
	
	/** Cubemap to resolve to */
	FTextureCubeRHIRef	ResolveTargetCube;

	/** Which face int he ResolveTargetCube to resolve to */
	ECubeFace			ResolveTargetCubeFace;
};

/**
* Allocates memory for directly loading texture mip data 
*/
class FTexture2DResourceMemPS3 : public FTexture2DResourceMem
{
public:	
	/** 
	 * Init Constructor 
	 * Allocates texture memory during construction
	 * 
	 * @param InSizeX		- width of texture
	 * @param InSizeY		- height of texture
	 * @param InNumMips		- total number of mips to allocate memory for
	 * @param InFormat		- EPixelFormat texture format
	 * @param AsyncCounter	- If provided, starts an async allocation. If NULL, allocates memory immediately.
	 */
	FTexture2DResourceMemPS3(INT InSizeX, INT InSizeY, INT InNumMips, EPixelFormat InFormat, FThreadSafeCounter* AsyncCounter);
	/** 
	 * Destructor
	 */
	virtual ~FTexture2DResourceMemPS3();

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
	 * Whether the resource memory is properly allocated or not.
	 **/
	virtual UBOOL IsValid();

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
	FTexture2DResourceMemPS3() {}

	/** 
	 * Calculate size needed to store this resource
	 */
	void Init();

	/** 
	 * Calculate size needed to store this resource and allocate texture memory for it
	 *
	 * @param bAllowRetry	If TRUE, retry the allocation after panic-stream-out/panic-defrag upon failure
	 */
	void AllocateTextureMemory( UBOOL bAllowRetry );

	/**
	 * Free the memory from the texture pool
	 */
	void FreeTextureMemory();

	/** width of texture */
	INT SizeX;
	/** height of texture */
	INT SizeY;
	/** total number of mips of texture */
	INT NumMips;
	/** format of texture */
	EPixelFormat Format;
	/** ptr to the allocated memory for all mips */
	void* BaseAddress;
	/** size of the allocated memory for all mips */
	DWORD TextureSize;
	/** Whether to allow allocation to fail silently */
	UBOOL bAllowFailure;

	/** Currently pending async allocation. Must not be deleted until the texture pool manager is finished with it. */
	FAsyncReallocationRequest* ReallocationRequest;
};


/**
 * Checks if the texture data is allocated within the texture pool or not.
 */
UBOOL appIsPoolTexture( FTextureRHIParamRef TextureRHI );


#endif // __PS3RHISURFACE_H__

#endif // USE_PS3_RHI
