/*=============================================================================
	PS3GCM.h: Functionality for dealing with libgcm directly.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if USE_PS3_RHI

#ifndef __PS3GCM_H__
#define __PS3GCM_H__

#include <cell/gcm.h>

using namespace cell::Gcm;

#include "PS3GcmMalloc.h"
#include "PS3TexturePool.h"
#include "PS3RHIUtil.h"
#include "PS3RHISurface.h"

#define PS3TEXTURE_ALIGNMENT 128

class FPS3TexturePool;

/** Maximum level of anisotropy. */
extern DWORD GPS3MaxAnisotropy;

// maps an EVertexElementUsage to GCM define
extern DWORD UnrealUsageToSemantic[];

// maps an EVertexElementUsage to the maximum usage index supported on PS3
extern DWORD UnrealMaxUsageIndex[];

// maps an EVertexElementType to how many elements it holds
extern DWORD UnrealTypeToCount[];

// maps an EVertexElementType to GCM data type define
extern DWORD UnrealTypeToType[];

// maps an EPrimitiveType to GCM define
extern DWORD UnrealPrimitiveTypeToType[];

// Converts from num-of-primitives to num-of-vertices or num-of-indices
extern DWORD UnrealPrimitiveTypeToElementCount[][2];

// maps an ESampleAddressMode to GCM define
extern DWORD TranslateUnrealAddressMode[];

// maps an ESamplerFilter to GCM define
extern DWORD TranslateUnrealMinFilterMode[];
extern DWORD TranslateUnrealMagFilterMode[];

// maps an EBlendOperation to GCM define
extern DWORD TranslateUnrealBlendOp[];

// maps an EBlendFactor to GCM define
extern DWORD TranslateUnrealBlendFactor[];

// maps an ECompareFunction to GCM define
extern DWORD TranslateUnrealCompareFunction[];

// maps an EStencilOp to GCM define
extern DWORD TranslateUnrealStencilOp[];

// maps an ERasterizerFillMode to GCM define
extern DWORD TranslateUnrealFillMode[];

// maps an ERasterizerCullMode to GCM define
extern DWORD TranslateUnrealCullMode[];

// Clock frequency of the GPU (in Hz, so 500000000 or 550000000).
extern DWORD GGPUFrequency;

// The following is a list of all Labels we use. Valid numbers are 64-255.
#define LABEL_FENCE					255	// Used by FPS3Gcm fence functions
#define LABEL_BUFFERFLIPPED			254	// The specified display buffer has been flipped
#define LABEL_PERF					253	// Used by the GPU performance measuremeants
#define LABEL_DRAWCALL				252	// Used for marking drawcalls for easier FIFO buffer debugging.
#define LABEL_LANDSCAPE_SPU_RSX		251	// Used to make the SPU wait for RSX when generating landscape vertex buffer data.
#define LABEL_LANDSCAPE_RSX_FLUSH	250	// Used to flush and wait for rendering to finish RSX


#define LABEL_EDGE_SPU0             240 // Used by Edge Geometry for SPU<->RSX synchronization.  One per SPU allocated to SPURS.
#define LABEL_EDGE_SPU1             241
#define LABEL_EDGE_SPU2             242
#define LABEL_EDGE_SPU3             243
#define LABEL_EDGE_SPU4             244
#define LABEL_EDGE_SPU5             245

#if !FINAL_RELEASE
	// Maximum number of supported GPU reports (timestamps and occlusion queries).
	#define MAX_GPUREPORTS				1024
#else
	// Maximum number of supported GPU reports (timestamps and occlusion queries).
	#define MAX_GPUREPORTS				2048
#endif

#define GPUREPORT_NUMUSEDBYSTATS	4
#define GPUREPORT_BEGINFRAME		(MAX_GPUREPORTS-1)
#define GPUREPORT_ENDFRAME			(MAX_GPUREPORTS-2)
#define GPUREPORT_BEGINFRAME2		(MAX_GPUREPORTS-3)
#define GPUREPORT_ENDFRAME2			(MAX_GPUREPORTS-4)

// Maximum numer of occlusion queries per frame (reports 0 to MAX_OCCLUSIONQUERIES).
#define MAX_OCCLUSIONQUERIES		200

#define MAX_TILESLOTS				15			// Maximum number of tiled memory slots
#define MAX_ZCULLSLOTS				8			// Maximum number of zcull slots
#define MAX_ZCULLMEMORY				(2048*1536)	// Maximum number of pixels in the zcull memory (shared for all regions)
#define MAX_COMPRESSIONTAGMEMORY	0x800		// Maximum size of compression tag memory (on GPU)

struct FTiledEntry
{
	UBOOL			bUsed;				// Whether this entry is used
	UBOOL			bUseZcull;			// Whether this entry is using zcull
	UBOOL			bUseCompression;	// Whether this entry is using compression
	DWORD			MemoryOffset;		// RSX IO offset to buffer in local memory (4KB aligned)
	DWORD			ZCullIndex;			// Index to a FZCullEntry
	DWORD			CompressionAddress;	// Compression address
};

struct FZCullEntry
{
	UBOOL			bUsed;				// Whether this entry is used
	DWORD			Width;				// Width in pixels (multiple of 64)
	DWORD			Height;				// Height in pixels (multiple of 64)
	DWORD			MultisampleType;	// Multisampling format of the depth buffer
};

/** types for individual host mem heaps */
enum EHostMemoryHeapType
{
	/** default host mem heap for all resource allocs */
	HostMem_Default=0,
	/** used only for movie textures */
	HostMem_Movies,
	HostMem_MAX
};

/**
 * Encapsulates RSX management through libGcm, such as RSX synchronization and backbuffer management.
 */
class FPS3Gcm
{
	// allow the RHI viewport access to my protected members
	friend class FPS3RHIViewport;

public:

	/**
	 * Initialize the PS3 graphics system as early in the boot up process as possible.
	 * This will make it possible to display loading screens, etc, before a
	 * Viewport is created.
	 */
	FPS3Gcm();

	/**
	 * Destructor
	 */
	~FPS3Gcm();

	/**
	 * Draw a full screen loading screen, without actually using the RHI
	 * Located in the RHI code because all rendering occurs here
	 *
	 * @param Filename Path to the screen to load
	 * @return TRUE if successful
	 */
	UBOOL	DrawLoadingScreen(const TCHAR* Filename);

	/**
	 * Initialize the memory (texture, etc) pools. Will use GConfig at this time,
	 * so appInit must be called before calling this
	 */
	void	InitializeMemoryPools();

	/** Called before rendering a scene. */
	void	BeginScene();

	/** Called when finished rendering a scene. */
	void	EndScene();

	/** Resets the state of currently set GPU resources, so that they can be freed from memory. */
	void	ResetGPUState();

	/** Queue a frame buffer swap. May block. */
	void	SwapBuffers();

	/** Blocks the PPU until the GPU is idle. */
	void	BlockUntilGPUIdle();

	/** Flushes the pending commands to the GPU. */
	void	Flush();

	/** Inserts a fence into the command buffer and flush. The inserted fence is returned. */
	DWORD	InsertFence( UBOOL bKickBuffer=TRUE );

	/** Returns TRUE if the specified fence has not yet been processed by the GPU, otherwise FALSE. */
	UBOOL	IsFencePending( DWORD Fence );

	/** Blocks the CPU until the specified fence has been processed by the GPU. Asserts if blocked more than 1 second. */
	void	BlockOnFence( DWORD Fence );

	/** Returns the last fence inserted in the command buffer. */
	DWORD	GetCurrentFence();

	/**
	 * Return screen resolution
	 */
	DWORD	GetScreenSizeX() { return ScreenSizeX; }
	DWORD	GetScreenSizeY() { return ScreenSizeY; }

	/**
	 * Returns the index of the frame buffer that is the current back buffer
	 */
	DWORD	GetBackBufferIndex()
	{
		return BackBufferIndex;
	}

	/** 
	* Get the allocator for system memory. Default one should always be valid
	*
	* @param HostMemType - EHostMemoryHeapType for the host mem heap we want
	* @return pointer to host allocator
	*/
	FMallocGcmBase* GetHostAllocator(EHostMemoryHeapType HostMemType=HostMem_Default)
	{
		if( HostMem[HostMemType] != NULL )
		{
			return HostMem[HostMemType];
		}
		else
		{
			check(HostMem[0] != NULL);
			return HostMem[0];
		}		
	}

	/** 
	* Determine if the host mem heap is valid
	*
	* @param HostMemType - EHostMemoryHeapType for the host mem heap we want
	* @return TRUE if the host allocator for the give heap is valid 
	*/
	UBOOL HasHostAllocator(EHostMemoryHeapType HostMemType)
	{
		return HostMem[HostMemType] ? TRUE : FALSE;
	}

	/** 
	* Friendly name of host allocator type
	*
	* @param HostMemType - EHostMemoryHeapType for the host mem heap we want
	* @return text description of the allocator 
	*/
	const TCHAR* GetHostAllocatorDesc(EHostMemoryHeapType HostMemType);

	/** Returns the allocator for video memory. */
	FMallocGcmBase* GetLocalAllocator()
	{
		return LocalMem;
	}

	/**
	 * @return the PatchedPixelShaders ring buffer 
	 */
	FFencedRingBuffer* GetPatchedPixelShadersRingBuffer()
	{
		return PatchedPixelShadersRingBuffer;
	}

	/**
	 * @return the dynamic (DrawPrimUP, etc) ring buffer 
	 */
	FFencedRingBuffer* GetDynamicRingBuffer()
	{
		return DynamicRingBuffer;
	}

#if RHI_SUPPORTS_PREVERTEXSHADER_CULLING
	/** @return The EDGE geometry output ring buffer. */
	FEdgeOutputBuffer* GetEdgeGeometryOutputBuffer()
	{
		return EdgeGeometryOutputBuffer;
	}
#endif

	/**
	 *	Checks whether the texture pool has been initialized yet or not.
	 *	@return	- TRUE if the texture pool is initialized, otherwise FALSE.
	 */
	UBOOL IsTexturePoolInitialized() const
	{
		return TexturePool ? TRUE : FALSE;
	}

	FPS3TexturePool* GetTexturePool()
	{
		check(TexturePool);
		return TexturePool;
	}

	/** Access to the frame buffers */
	const FSurfaceRHIRef& GetColorBuffer(DWORD Index)
	{
		return ColorBuffers[Index];
	}

	/**
	 * Set the render target, sets up the viewport as the size in the surface
	 * 
	 * @param Surface Structure describing the surface/render target
	 */
	void SetRenderTarget(const CellGcmSurface* Surface);

	/**
	 * Return whether or not the most recently set render target is floating point format
	 */
	UBOOL IsCurrentRenderTargetFloatingPoint()
	{
		return bIsCurrentRenderTargetFloatingPoint;
	}

	void GetCurrentRenderTargetSize(DWORD& SizeX, DWORD& SizeY)
	{
		SizeX = CurrentRenderTargetSizeX;
		SizeY = CurrentRenderTargetSizeY;
	}

	/**
	 * Allocates memory and binds it to a tiled region (and in the Zcull region if it's a depth surface)
	 * @param Resource		Resource to allocate and bind to a tiled region
	 * @param Format		Texture/surface format
	 * @param MemoryRegion	Location of memory to tile (local or host)
	 * @param Height		Height in pixels
	 * @param Pitch			Pitch of the buffer (width * bytes/pixel)
	 * @return				Returns the MemoryOffset to the new memory
	 */
	UBOOL	AllocateTiledMemory( class FPS3RHIGPUResource &Resource, UINT &Pitch, EPixelFormat Format, EMemoryRegion MemoryRegion, DWORD Width, DWORD Height, DWORD MultisampleType );

	/**
	 * Unbinds the tiled region used by the resource. Does NOT free the memory.
	 * Silently fails if the resource isn't bind to a tiled region.
	 * @param Resource		Resource which is bound to a tiled region
	 */
	void	UnbindTiledMemory( class FPS3RHIGPUResource &Resource );

	/**
	 * @return TRUE if the memory is a currently tiled region
	 */
	UBOOL	IsMemoryTiled( DWORD MemoryOffset );

	/**
	 * Should we ignore long GPU or SPU stalls? Used for Replay or other things which can stall for long periods of time
	 */
	UBOOL	ShouldIgnoreLongStalls( void );

	/**
	 * Sets various memory stats.
	 */
	void	UpdateMemStats();

	/** GPU performance counter management */
	void	InitPerfCounters( );
	void	SetupPerfCounters( UINT Domain, const UINT Counters[4] );
	void	TriggerPerfCounters();
	UINT	ReadPerfCounters( UINT Domain, UINT Counters[4] );

	// Accessors.
	BYTE*	GetDefaultCommandBufferStart() const	{ return DefaultCommandBufferStart; }
	UINT	GetDefaultCommandBufferSize() const		{ return DefaultCommandBufferSize; }

	/**
	 * Sets if the screen needs to be cleared
	 */
	void	SetNeedsScreenClear(UBOOL bNeedsClear);

protected:
	/**
	 * Initialize the PS3 graphics system (libgcm). Called from constructor
	 */
	void Initialize();

	/**
	 * Cleanup. Called from the destructor.
	 */
	void Cleanup();


	/** The frame buffer we are currently rendering to */
	BYTE				BackBufferIndex;
	/** The two frame buffers. Really just render targets like anything else, except they are registered as the frame buffers */
	FSurfaceRHIRef		ColorBuffers[2];
	/** The local memory offset to the three registered display buffers. */
	DWORD				DisplayBufferOffsets[3];

	/** Pointer to the default (system) command buffer */
	void*				RSXHostBuffer;

	/** Fence used to block CPU is GPU is lagging behind */
	DWORD				FlipFence;

	/** Size of the total screen */
	DWORD				ScreenSizeX;
	DWORD				ScreenSizeY;

	/** Is screen 4x3? */
	UBOOL				bIsScreen4x3;

	/** Allocators for RSX (system and host) memory */
	FMallocGcmBase*		HostMem[HostMem_MAX];
	FMallocGcmBase*		LocalMem;

	/** Texture pool (memory allocated from LocalMem) */
	FPS3TexturePool*	TexturePool;

	/** Is the most recently set render target is floating point format? */
	UBOOL				bIsCurrentRenderTargetFloatingPoint;

	/** If TRUE, the screen needs to be cleared to black every frame (INT for appInterlocked*) */
	INT					bNeedsScreenCleared;

	/** Saved info about the most recently set render target */
	DWORD				CurrentRenderTargetSizeX;
	DWORD				CurrentRenderTargetSizeY;

	/** Last fence number inserted into the command buffer. */
	DWORD				CurrentFence;

	/** Last fence number processed by the GPU. */
	volatile DWORD *	FenceLabel;

	/** Tiled memory management */
	FTiledEntry			TileMemory[MAX_TILESLOTS];

	/** Compression tag memory management */
	FMallocGcm*			CompressionTagMem;

	/** ZCull memory management */
	FZCullEntry			ZCullMemory[MAX_ZCULLSLOTS];

	/** The default command buffer. */
	BYTE*				DefaultCommandBufferStart;

	/** The default command buffer size in bytes. */
	UINT				DefaultCommandBufferSize;

	/** Ring buffer for patched pixel shaders */
	FFencedRingBuffer*	PatchedPixelShadersRingBuffer;

	/** Ring buffer for dynamic vertex/index buffer */
	FFencedRingBuffer*	DynamicRingBuffer;

#if RHI_SUPPORTS_PREVERTEXSHADER_CULLING
	/** Ring buffer for EDGE geometry output. */
	FEdgeOutputBuffer*	EdgeGeometryOutputBuffer;
#endif
};

extern FPS3Gcm* GPS3Gcm;

#endif // __PS3GCM_H__

#endif // USE_PS3_RHI

