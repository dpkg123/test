/*=============================================================================
	PS3GCM.h: Functionality for dealing with libgcm directly
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3RHIPrivate.h"
#include "PS3Gcm.h"
#include "FMallocPS3.h"
#include "BestFitAllocator.h"
#include "AllocatorFixedSizeFreeList.h"
#include "ChartCreation.h"

#include <sysutil/sysutil_sysparam.h>

#include <sys/sys_time.h>
#include <sys/time_util.h>
#include <sdk_version.h>

#if !FINAL_RELEASE && CELL_GCM_DEBUG
#include <cell/gcm/gcm_gpad.h>
#endif

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
// For cellRSXFifoFlattenToFile(), etc
#include <librsxdebug.h>
#endif

DOUBLE GLastSwapTime = 0.0;

// @todo: Temp define for testing patches (makes the loading screen upside down to verify its running patched exe)
#define PATCH_VERSION 0

#if USE_STATIC_HOST_MODEL
// the size of preallocated main memory buffer that the GPU can access
// you may need to increase this depending on your vertex data - CPU-accessible
// vertex and index buffers are allocated from here
#define RSXHOST_SIZE		(60 * 1024 * 1024)
#endif

#if USE_GROW_DYNAMIC_HOST_MODEL
// don't let host size dynamically grow beyond this limit
#define MAX_RSXHOST_SIZE	(100 * 1024 * 1024)
// minimum memory initially used by host allocator
#define MIN_RSXHOST_SIZE	(10 * 1024 * 1024)
//#define MIN_RSXHOST_SIZE	(0 * 1024 * 1024)
#endif

// Used for patching pixelshader constants
#define BUFFERSIZE_PATCHEDPIXELSHADERS		(1*1024*1024)
#define PATCHBUFFER_PARTITIONS				8

#define BUFFERSIZE_DYNAMICDATA				(1 * 1024 * 1024)
#define DYNAMICDATA_PARTITIONS				4

#define BUFFERSIZE_EDGEGEOMETRYOUTPUT		(768 * 1024)

#if USE_PS3_RHI

// ps3 specific GPU stats
#if PS3
DECLARE_MEMORY_STAT2(TEXT("GPU Memory Used"),STAT_GPUAllocSize,STATGROUP_Memory, MCR_GPU, TRUE);
DECLARE_MEMORY_STAT2(TEXT("  Local CommandBuffer Size"),STAT_LocalCommandBufferSize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local FrameBuffer Size"),STAT_LocalFrameBufferSize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local ZBuffer Size"),STAT_LocalZBufferSize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local RenderTarget Size"),STAT_LocalRenderTargetSize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local Texture Size"),STAT_LocalTextureSize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local VertexShader Size"),STAT_LocalVertexShaderSize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local PixelShader Size"),STAT_LocalPixelShaderSize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local VertexBuffer Size"),STAT_LocalVertexBufferSize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local IndexBuffer Size"),STAT_LocalIndexBufferSize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local RingBuffer Size"),STAT_LocalRingBufferSize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local CompressionTag Size"),STAT_LocalCompressionTagSize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local ResourceArray Size"),STAT_LocalResourceArraySize,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Local OcclusionQuery Size"),STAT_LocalOcclusionQueries,STATGROUP_Memory, MCR_GPU, FALSE);
DECLARE_MEMORY_STAT2(TEXT("Host Memory Used"),STAT_HostAllocSize,STATGROUP_Memory, MCR_GPUSystem, TRUE);
DECLARE_MEMORY_STAT2(TEXT("  Host CommandBuffer Size"),STAT_HostCommandBufferSize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host FrameBuffer Size"),STAT_HostFrameBufferSize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host ZBuffer Size"),STAT_HostZBufferSize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host RenderTarget Size"),STAT_HostRenderTargetSize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host Texture Size"),STAT_HostTextureSize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host VertexShader Size"),STAT_HostVertexShaderSize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host PixelShader Size"),STAT_HostPixelShaderSize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host VertexBuffer Size"),STAT_HostVertexBufferSize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host IndexBuffer Size"),STAT_HostIndexBufferSize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host RingBuffer Size"),STAT_HostRingBufferSize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host CompressionTag Size"),STAT_HostCompressionTagSize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host ResourceArray Size"),STAT_HostResourceArraySize,STATGROUP_Memory, MCR_GPUSystem, FALSE);
DECLARE_MEMORY_STAT2(TEXT("  Host OcclusionQuery Size"),STAT_HostOcclusionQueries,STATGROUP_Memory, MCR_GPUSystem, FALSE);
#endif

/** Toggles between 0 and 1 each frame. */
static INT GFrameSelector = 0;

/** The GPU Report indices to use for the GPU frame timestamps. */
static INT GFrameTimeStamps[4] = { GPUREPORT_BEGINFRAME, GPUREPORT_ENDFRAME, GPUREPORT_BEGINFRAME2, GPUREPORT_ENDFRAME2 };

/** Resets the counter for when to periodically insert new fences. */
void PS3ResetAutoFence();


/** global definitions */
FPS3Gcm* GPS3Gcm = NULL;

/** Clock frequency of the GPU. */
DWORD GGPUFrequency;

/** Maximum level of anisotropy. */
DWORD GPS3MaxAnisotropy = CELL_GCM_TEXTURE_MAX_ANISO_4;

DWORD UnrealUsageToSemantic[] = 
{
	0,	//	VEU_Position,
	8,	//	VEU_TextureCoordinate,
	1,	//	VEU_BlendWeight,
	7,	//	VEU_BlendIndices,
	2,	//	VEU_Normal,
	5,	//	VEU_Tangent,
	6,	//	VEU_Binormal,
	3,	//	VEU_Color
};

DWORD UnrealMaxUsageIndex[] =
{
	0,	//	VEU_Position,
	8,	//	VEU_TextureCoordinate,
	0,	//	VEU_BlendWeight,
	0,	//	VEU_BlendIndices,
	0,	//	VEU_Normal,
	0,	//	VEU_Tangent,
	0,	//	VEU_Binormal,
	1,	//	VEU_Color
};

DWORD UnrealTypeToCount[] = 
{
	0,	//	VET_None,
	1,	//	VET_Float1,
	2,	//	VET_Float2,
	3,	//	VET_Float3,
	4,	//	VET_Float4,
	4,	//	VET_PackedNormal,	// FPackedNormal
	4,	//	VET_UByte4,
	4,	//	VET_UByte4N,
	4,	//	VET_Color,
	2,	//	VET_Short2
	2,	//	VET_Short2N
	2,	//	VET_Half2
	1,	//  VET_Pos3N
};

DWORD UnrealTypeToType[] = 
{
	0,						//	VET_None,
	CELL_GCM_VERTEX_F,		//	VET_Float1,
	CELL_GCM_VERTEX_F,		//	VET_Float2,
	CELL_GCM_VERTEX_F,		//	VET_Float3,
	CELL_GCM_VERTEX_F,		//	VET_Float4,
	CELL_GCM_VERTEX_UB256,	//	VET_PackedNormal,	// FPackedNormal
	CELL_GCM_VERTEX_UB256,	//	VET_UByte4,
	CELL_GCM_VERTEX_UB,		//	VET_UByte4N,
	CELL_GCM_VERTEX_UB,		//	VET_Color,
	CELL_GCM_VERTEX_S32K,	//	VET_Short2
	CELL_GCM_VERTEX_S1,		//	VET_Short2N
	CELL_GCM_VERTEX_SF,		//	VET_Half2
	CELL_GCM_VERTEX_CMP,	//  VET_Pos3N
};

DWORD UnrealPrimitiveTypeToType[] = 
{
	CELL_GCM_PRIMITIVE_TRIANGLES,		//	PT_TriangleList = 0,
	CELL_GCM_PRIMITIVE_TRIANGLE_STRIP,	//	PT_TriangleStrip = 2,
	CELL_GCM_PRIMITIVE_LINES,			//	PT_LineList = 3,
	CELL_GCM_PRIMITIVE_QUADS,			//	PT_QuadList = 4,
};

// Converts from num-of-primitives to num-of-vertices or num-of-indices.
DWORD UnrealPrimitiveTypeToElementCount[][2] = 
{
	{3,0},	//	PT_TriangleList = 0,
	{1,2},	//	PT_TriangleStrip = 1,
	{2,0},	//	PT_LineList = 2,
	{4,0}	//	PT_QuadList = 3,
};

DWORD TranslateUnrealMinFilterMode[] = 
{
	CELL_GCM_TEXTURE_NEAREST_NEAREST,	// SF_Point
	CELL_GCM_TEXTURE_LINEAR_NEAREST,	// SF_Bilinear
	CELL_GCM_TEXTURE_LINEAR_LINEAR,		// SF_Trilinear
	CELL_GCM_TEXTURE_LINEAR_NEAREST,	// SF_AnisotropicPoint
	CELL_GCM_TEXTURE_LINEAR_LINEAR,		// SF_AnisotropicLinear
};

DWORD TranslateUnrealMagFilterMode[] = 
{
	CELL_GCM_TEXTURE_NEAREST,			// SF_Point
	CELL_GCM_TEXTURE_LINEAR,			// SF_Bilinear
	CELL_GCM_TEXTURE_LINEAR,			// SF_Trilinear
	CELL_GCM_TEXTURE_LINEAR,			// SF_AnisotropicPoint
	CELL_GCM_TEXTURE_LINEAR,			// SF_AnisotropicLinear
};

DWORD TranslateUnrealAddressMode[] = 
{
	CELL_GCM_TEXTURE_WRAP,				// AM_Wrap
	CELL_GCM_TEXTURE_CLAMP_TO_EDGE,		// AM_Clamp
	CELL_GCM_TEXTURE_MIRROR,			// AM_Mirror
};

DWORD TranslateUnrealBlendFactor[] = 
{
	CELL_GCM_ZERO,					//	BF_Zero,
	CELL_GCM_ONE,					//	BF_One,
	CELL_GCM_SRC_COLOR,				//	BF_SourceColor,
	CELL_GCM_ONE_MINUS_SRC_COLOR,	//	BF_InverseSourceColor,
	CELL_GCM_SRC_ALPHA,				//	BF_SourceAlpha,
	CELL_GCM_ONE_MINUS_SRC_ALPHA,	//	BF_InverseSourceAlpha,
	CELL_GCM_DST_ALPHA,				//	BF_DestAlpha,
	CELL_GCM_ONE_MINUS_DST_ALPHA,	//	BF_InverseDestAlpha,
	CELL_GCM_DST_COLOR,				//	BF_DestColor,
	CELL_GCM_ONE_MINUS_DST_COLOR,	//	BF_InverseDestColor
};

DWORD TranslateUnrealCompareFunction[] = 
{
	CELL_GCM_LESS,			//	CF_Less,
	CELL_GCM_LEQUAL,		//	CF_LessEqual,
	CELL_GCM_GREATER,		//	CF_Greater,
	CELL_GCM_GEQUAL,		//	CF_GreaterEqual,
	CELL_GCM_EQUAL,			//	CF_Equal,
	CELL_GCM_NOTEQUAL,		//	CF_NotEqual,
	CELL_GCM_NEVER,			//	CF_Never,
	CELL_GCM_ALWAYS,		//	CF_Always
};

DWORD TranslateUnrealBlendOp[] =
{
	CELL_GCM_FUNC_ADD,		//	BO_Add,
	CELL_GCM_FUNC_SUBTRACT,	//	BO_Subtract
	CELL_GCM_MIN,			//	BO_Min
	CELL_GCM_MAX,			//	BO_Max
    CELL_GCM_FUNC_REVERSE_SUBTRACT, // BO_ReverseSubtract
};

DWORD TranslateUnrealStencilOp[] =
{
	CELL_GCM_KEEP,			//	SO_Keep,
	CELL_GCM_ZERO,			//	SO_Zero,
	CELL_GCM_REPLACE,		//	SO_Replace,
	CELL_GCM_INCR,			//	SO_SaturatedIncrement,
	CELL_GCM_DECR,			// SO_SaturatedDecrement,
	CELL_GCM_INVERT,		//	SO_Invert,
	CELL_GCM_INCR_WRAP,		//	SO_Increment,
	CELL_GCM_DECR_WRAP,		//	SO_Decrement
};

DWORD TranslateUnrealFillMode[] = 
{
	CELL_GCM_POLYGON_MODE_POINT,	//	FM_Point,
	CELL_GCM_POLYGON_MODE_LINE,		//	FM_Wireframe,
	CELL_GCM_POLYGON_MODE_FILL,		//	FM_Solid
};

DWORD TranslateUnrealCullMode[] = 
{
	CELL_GCM_CCW,	//	CM_None,
	// the next two are swapped because gcm actually wants what is the front face, NOT what to cull
	CELL_GCM_CCW,	//	CM_CW,
	CELL_GCM_CW,	//	CM_CCW
};

const DWORD GTextureRemapARGB	= CELL_GCM_REMAP_MODE(CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
										 CELL_GCM_TEXTURE_REMAP_FROM_A,
										 CELL_GCM_TEXTURE_REMAP_FROM_R,
										 CELL_GCM_TEXTURE_REMAP_FROM_G,
										 CELL_GCM_TEXTURE_REMAP_FROM_B,
										 CELL_GCM_TEXTURE_REMAP_REMAP,
										 CELL_GCM_TEXTURE_REMAP_REMAP,
										 CELL_GCM_TEXTURE_REMAP_REMAP,
										 CELL_GCM_TEXTURE_REMAP_REMAP);

//=============================================================================
// The following variables are maintained by GPU callbacks only.

// Contains the number of VSYNC signals since the last flip.
DWORD GVBlankCounter = 0;

// When flip is requested, GBufferToFlip contains the FlipID for the buffer. 255 if no flip is requested.
DWORD GBufferToFlip = 255;

// Contains the FlipID of the last buffer that was flipped.
DWORD GBufferFlipped = 0;
DWORD GBufferFlipFinished = 0;
volatile DWORD* GFlipLabel = NULL;

// DEBUG variables:
QWORD GLastVBlankTick = 0;
QWORD GLastVSyncTick = 0;
QWORD GLastFlipTick = 0;
DWORD GFlipCounter = 0;
DWORD GVSyncCounter = 0;

CellGcmControl* GPS3GcmControlRegisters = NULL;

//=============================================================================

/*
 * Called by GPU interrupt for the "Secondary VSYNC". Not sure what this means.
 * When tried with 60 Hz progressive scan LCD, it's called once every 6.66 seconds...
 */
void VSyncCallback( DWORD Param )
{
	QWORD CurrentTick;
	SYS_TIMEBASE_GET( CurrentTick );
	QWORD TicksPerMillisec = GTicksPerSeconds / 1000;
	QWORD TimeSinceLastVBlank = (CurrentTick - GLastVSyncTick) / TicksPerMillisec;
//	printf( "2nd VSYNC: %d ms\n", TimeSinceLastVBlank );
	GLastVSyncTick = CurrentTick;
	GVSyncCounter++;
}



/*
 * Called by GPU interrupt when a flip is finished. Triggered by cellGcmSetFlipImmediate.
 *
 * @param Param		Always set to 1.
 */
void FlipCallback( UINT Param )
{
	QWORD CurrentTick;
	SYS_TIMEBASE_GET( CurrentTick );
	QWORD TicksPerMillisec = GTicksPerSeconds / 1000;
	QWORD TimeSinceLastVBlank = (CurrentTick - GLastFlipTick) / TicksPerMillisec;
	GLastFlipTick = CurrentTick;
	GFlipCounter++;

//	printf( "FLIP %d: FINISHED\n",GBufferFlipped );

	GBufferFlipFinished = GBufferFlipped;

	// Flip is finished, release the WaitLabel.
	*GFlipLabel = GBufferFlipped;
//	*(volatile DWORD *)cellGcmGetLabelAddress( LABEL_BUFFERFLIPPED ) = GBufferFlipped;
}

/*
 * Called by the GPU user callback or VBlank callback to perform a flip if necessary.
 * It will only do a flip if there has been 2+ VSYNCs since the last flip. (30 Hz)
 */
void ConditionallyPerformFlip()
{
	INT Ret;
	if ( GVBlankCounter >= 2 && GBufferToFlip != 255 )
	{
		DWORD FlipID = GBufferFlipped = GBufferToFlip;
		GBufferToFlip = 255;
		GVBlankCounter = 0;
		Ret = cellGcmSetFlipImmediate( FlipID );
//		printf( "FLIP %d\n", FlipID );
		if ( Ret == CELL_GCM_ERROR_FAILURE )
		{
			// Flip didn't work, release the WaitLabel.
			*GFlipLabel = FlipID;
//			*(volatile DWORD *)cellGcmGetLabelAddress( LABEL_BUFFERFLIPPED ) = FlipID;
		}
	}
}

/*
 * Called by GPU interrupt during VBLANK and after VSYNC.
 * Increments the VBlank counter and performs a flip if it's been requested, and
 * at least 2 VBlanks have passed since the last flip.
 *
 * @param Param		Always set to 1.
 */
void VBlankCallback( UINT Param )
{
	QWORD CurrentTick;
	SYS_TIMEBASE_GET( CurrentTick );
	QWORD TicksPerMillisec = GTicksPerSeconds / 1000;
	QWORD TimeSinceLastVBlank = (CurrentTick - GLastVBlankTick) / TicksPerMillisec;
	GLastVBlankTick = CurrentTick;
	GVBlankCounter++;

//	printf( "VBLANK %d: VBlanks=%d\n", GBufferToFlip, GVBlankCounter );

	// Conditionally starts the flip, if GBufferToFlip is set and enough vblanks have occurred.
	ConditionallyPerformFlip();
}

/*
 * Called by GPU interrupt when the USER callback command is processed.
 * This requests a FLIP. In addition, it will also perform it immediately
 * (not waiting for VBLANK) if at least 2 VBlanks have passed since the last flip.
 *
 * @param Param		Specifies which FlipID to use for flipping.
 */
void GPUUserCallback( UINT Param )
{
	DWORD FrameSelector = Param >> 16;
	Param &= 0xffff;

	{
		// At this point, both GPUREPORT_BEGINFRAME and GPUREPORT_ENDFRAME has been processed,
		// and it's now safe to read them.
		QWORD BeginFrame = cellGcmGetTimeStamp( GFrameTimeStamps[FrameSelector*2] );
		QWORD EndFrame = cellGcmGetTimeStamp( GFrameTimeStamps[FrameSelector*2 + 1] );

		// Convert from nanoseconds to appCycles.
		GGPUFrameTime = DWORD( (EndFrame - BeginFrame) * GTicksPerSeconds / 1000000000ull );
	}

	check( GBufferToFlip == 255 );
	GBufferToFlip = Param;

//	printf( "USER %d: VBlanks=%d\n", GBufferToFlip, GVBlankCounter );
	if ( !GSystemSettings.bUseVSync )
	{
		GVBlankCounter = 2;	// Force ConditionallyPerformFlip() to actually do the flip.
	}
	ConditionallyPerformFlip();
}

void GPUExceptionHandler( UINT Value )
{
	printf( "RSX exception detected (0x%08x)!\n", Value );
}



/**
 * Initialize the PS3 graphics system as early in the boot up process as possible.
 * This will make it possible to display loading screens, etc, before a
 * Viewport is created.
 */
FPS3Gcm::FPS3Gcm()
:	TexturePool(NULL)
,	bNeedsScreenCleared(0)
{
	Initialize();
}

/**
 * Destructor
 */
FPS3Gcm::~FPS3Gcm()
{
	Cleanup();
}

/**
 * Initialize the PS3 graphics system (libgcm). Called from constructor
 */
void* GRSXBaseAddress;
void FPS3Gcm::Initialize()
{
	BackBufferIndex = 0;
	for( INT HostMemIdx=0; HostMemIdx < HostMem_MAX; HostMemIdx++ )
	{
		HostMem[HostMemIdx]=NULL;
	}

	for( INT SamplerIndex=0; SamplerIndex < 16; ++SamplerIndex )
	{
		GCurrentTexture[SamplerIndex] = NULL;
	}

#if USE_STATIC_HOST_MODEL
	// total host memory that needs to be allocated
	SIZE_T HostSizeAligned = Align<SIZE_T>(RSXHOST_SIZE,1024*1024);
	// allocate system memory in 1 MB pages
	sys_addr_t HostMemAddr;
	INT Ret = sys_memory_allocate(HostSizeAligned, SYS_MEMORY_PAGE_SIZE_1M, &HostMemAddr);
	checkf( Ret == CELL_OK, TEXT("Failed to allocate %.2f MB host memory from system!"), HostSizeAligned/1024.f/1024.f );
	// set up a host memory allocator that uses the preallocated system memory
	HostMem[HostMem_Default] = new FMallocGcmHostStatic(HostSizeAligned, (void*)HostMemAddr);
#elif USE_GROW_DYNAMIC_HOST_MODEL
	// set up a host memory allocator that uses dynamically allocated system memory
	HostMem[HostMem_Default] = new FMallocGcmHostGrowable(MIN_RSXHOST_SIZE,MAX_RSXHOST_SIZE);
	// create a separate host heap for the movies
	HostMem[HostMem_Movies] = new FMallocGcmHostGrowable(0,MAX_RSXHOST_SIZE);
#else
#error Invalid Host memory model
#endif

	// Set up compression tag memory allocator (with dummy 0x1000 base address).
	CompressionTagMem = new FMallocGcm(MAX_COMPRESSIONTAGMEMORY, (void*) 0x1000);

	// Initialize the ZCullMemory management.
	appMemzero( ZCullMemory, sizeof(ZCullMemory) );

	// Initialize the Tiled Region management.
	appMemzero( TileMemory, sizeof(TileMemory) );

	// Setup RSX command buffer
	INT GcmInitMemSize = GPS3CommandBufferSizeMegs*1024*1024;
	DefaultCommandBufferStart = (BYTE*)GPS3CommandBuffer;
	DefaultCommandBufferSize = GcmInitMemSize - 4*1024;

	// initialize gcm with size of command buffer, size of this gpu accessible memory, and base address of the memory
	if (cellGcmInit(DefaultCommandBufferSize, GcmInitMemSize, DefaultCommandBufferStart) != CELL_OK)
	{
		appErrorf(TEXT("Failed to initialize libgcm"));
	}

	CellGcmConfig GcmConfig;
	cellGcmGetConfiguration(&GcmConfig);
	printf("RSX running with %d core clock, %d mem clock, %d MB of memory\n", GcmConfig.coreFrequency, GcmConfig.memoryFrequency, GcmConfig.localSize/1024/1024);
	printf("RSX base address is %x\n", GcmConfig.ioAddress);
	GRSXBaseAddress = GcmConfig.localAddress;
	GGPUFrequency = GcmConfig.coreFrequency;

	// Check local memory for bad hardware.
	if ( ParseParam(appCmdLine(),TEXT("memtest")) )
	{
		appOutputDebugString(TEXT("Running memory test on RSX memory...\n"));
		if ( appMemoryTest( GRSXBaseAddress, GcmConfig.localSize ) == FALSE )
		{
			appOutputDebugString(TEXT("Memory test failed!\n\n"));
		}
		else
		{
			appOutputDebugString(TEXT("Memory test passed.\n\n"));
		}
	}

	// map all of host memory for use on GPU
	for( INT HostMemIdx=0; HostMemIdx < HostMem_MAX; HostMemIdx++ )
	{
		const EHostMemoryHeapType HostMemType = (EHostMemoryHeapType)HostMemIdx; 
		if( HasHostAllocator(HostMemType) )
		{
			GetHostAllocator(HostMemType)->MapMemoryToGPU();
		}
	}

	// set up a local memory allocator to handle the memory returned in the GcmConfig
	LocalMem = new FMallocGcm(GcmConfig.localSize, GcmConfig.localAddress);

	// get correct video info from the system and set up our buffers accordingly
	INT NumDevices = cellVideoOutGetNumberOfDevice(CELL_VIDEO_OUT_PRIMARY);
	if (NumDevices < 0)
	{
		printf("FPS3Gcm::Initialize() ERROR - cellVideoOutGetNumberOfDevice returned 0x%x\n", NumDevices);
	}

	CellVideoOutDeviceInfo DeviceInfo;
	INT Error = cellVideoOutGetDeviceInfo(CELL_VIDEO_OUT_PRIMARY, 0, &DeviceInfo);
	if (Error != CELL_VIDEO_OUT_SUCCEEDED)
	{
		printf("FPS3Gcm::Initialize() ERROR - cellVideoOutGetDeviceInfo returned 0x%x\n", Error);
	}

	// prioritized list of resolutions
	struct FResolutionEntry
	{
		CellVideoOutResolutionId	FoundResolution;
		CellVideoOutResolutionId	OutputResolution;
		CellVideoOutDisplayAspect	Aspect;
	};

	// translation table for system resolutions vs. game resolutions
	struct FResolutionEntry ResolutionMapping[] =
	{
		{ CELL_VIDEO_OUT_RESOLUTION_1080,		CELL_VIDEO_OUT_RESOLUTION_720,		CELL_VIDEO_OUT_ASPECT_16_9 },
		{ CELL_VIDEO_OUT_RESOLUTION_1600x1080,	CELL_VIDEO_OUT_RESOLUTION_720,		CELL_VIDEO_OUT_ASPECT_16_9 },
		{ CELL_VIDEO_OUT_RESOLUTION_1440x1080,	CELL_VIDEO_OUT_RESOLUTION_720,		CELL_VIDEO_OUT_ASPECT_16_9 },
		{ CELL_VIDEO_OUT_RESOLUTION_1280x1080,	CELL_VIDEO_OUT_RESOLUTION_720,		CELL_VIDEO_OUT_ASPECT_16_9 },
		{ CELL_VIDEO_OUT_RESOLUTION_960x1080,	CELL_VIDEO_OUT_RESOLUTION_720,		CELL_VIDEO_OUT_ASPECT_16_9 },
		{ CELL_VIDEO_OUT_RESOLUTION_720,		CELL_VIDEO_OUT_RESOLUTION_720,		CELL_VIDEO_OUT_ASPECT_16_9 },
		{ CELL_VIDEO_OUT_RESOLUTION_576,		CELL_VIDEO_OUT_RESOLUTION_576,		CELL_VIDEO_OUT_ASPECT_16_9 },
		{ CELL_VIDEO_OUT_RESOLUTION_576,		CELL_VIDEO_OUT_RESOLUTION_576,		CELL_VIDEO_OUT_ASPECT_4_3 },
		{ CELL_VIDEO_OUT_RESOLUTION_480,		CELL_VIDEO_OUT_RESOLUTION_480,		CELL_VIDEO_OUT_ASPECT_16_9 },
		{ CELL_VIDEO_OUT_RESOLUTION_480,		CELL_VIDEO_OUT_RESOLUTION_480,		CELL_VIDEO_OUT_ASPECT_4_3 },
		{ (CellVideoOutResolutionId)-1,			(CellVideoOutResolutionId)-1,		(CellVideoOutDisplayAspect)-1 },
	};

	// search for the highest resolution that is supported and use that
	INT BestResolutionIndex = 999;
	INT Res;
	for (INT Mode = 0; Mode < DeviceInfo.availableModeCount; Mode++)
	{
		Res = 0;
		do
		{
			if ((ResolutionMapping[Res].FoundResolution == DeviceInfo.availableModes[Mode].resolutionId) &&
				(ResolutionMapping[Res].Aspect == DeviceInfo.availableModes[Mode].aspect))
			{
				if (Res < BestResolutionIndex)
				{
					BestResolutionIndex = Res;
				}
			}
		}
		while (ResolutionMapping[++Res].FoundResolution != -1);
	}

	// make sure it's available
	Error = cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, ResolutionMapping[BestResolutionIndex].OutputResolution, ResolutionMapping[BestResolutionIndex].Aspect, 0);
	if (Error == 0)
	{
		printf("FPS3Gcm::Initialize() ERROR - cellVideoOutGetResolutionAvailability failed!\n");
	}

	// CNN - set the best resolution
	CellVideoOutResolution ResolutionInfo;
	Error = cellVideoOutGetResolution(ResolutionMapping[BestResolutionIndex].OutputResolution, &ResolutionInfo);
	if (Error != CELL_VIDEO_OUT_SUCCEEDED)
	{
		printf("FPS3Gcm::Initialize() ERROR - cellVideoOutGetResolution returned 0x%x\n", Error);
	}

	// remember 4x3 screen
	bIsScreen4x3 = ResolutionMapping[BestResolutionIndex].Aspect == CELL_VIDEO_OUT_ASPECT_4_3;

	printf("FPS3Gcm::Initialize() - Setting %dx%d resolution (%s aspect ratio)\n", ResolutionInfo.width, ResolutionInfo.height, bIsScreen4x3 ? "4x3" : "16x9");

	// set it!!
	GScreenWidth = ScreenSizeX = ResolutionInfo.width;
	GScreenHeight = ScreenSizeY = ResolutionInfo.height;

	// buffer width MUST be a multiple of 64, so round up if necessary
	INT BufferWidth = ((ScreenSizeX + 0x3f) & ~0x3f) * 4;
	DWORD ColorBufferPitch = cellGcmGetTiledPitchSize(BufferWidth);

	CellVideoOutConfiguration VideoCfg;
	memset(&VideoCfg, 0, sizeof(VideoCfg));
	VideoCfg.resolutionId = ResolutionMapping[BestResolutionIndex].OutputResolution;
	VideoCfg.aspect = ResolutionMapping[BestResolutionIndex].Aspect;
	VideoCfg.format = CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8;
	VideoCfg.pitch = ColorBufferPitch;

	INT Return = cellVideoOutConfigure(CELL_VIDEO_OUT_PRIMARY, &VideoCfg, NULL, 0);
	if (Return != CELL_OK)
	{
		printf("cellVideoOutConfigure() failed. (0x%x)\n", Return);
	}

	// NOTE: Our VBlank callback is handling vsync (according to the GSystemSettings.bUseVSync variable).
	cellGcmSetFlipMode( CELL_GCM_DISPLAY_HSYNC );

	DWORD AAType = CELL_GCM_SURFACE_CENTER_1;

/*
	// @todo: If MSAA is desired, this is a start:
	// 		enable 2X MSAA for 480 or 576 modes - BROKEN! - needs a final blit since PS3 hardware doesn't do it for you
	if ((ResolutionMapping[BestResolutionIndex].OutputResolution == CELL_VIDEO_OUT_RESOLUTION_480) ||
		(ResolutionMapping[BestResolutionIndex].OutputResolution == CELL_VIDEO_OUT_RESOLUTION_576))
	{
		AAType = CELL_GCM_SURFACE_DIAGONAL_CENTERED_2;

		// pitch is doubled for MSAA
		ColorBufferPitch = cellGcmGetTiledPitchSize(BufferWidth*2);
	}
*/

	GFlipLabel = (volatile DWORD *)cellGcmGetLabelAddress( LABEL_BUFFERFLIPPED );
	*GFlipLabel = 255;
	cellGcmSetVBlankHandler( VBlankCallback );
	cellGcmSetFlipHandler( FlipCallback );
	cellGcmSetUserHandler( GPUUserCallback );
	cellGcmSetGraphicsHandler( GPUExceptionHandler );
//	cellGcmSetSecondVHandler( VSyncCallback );
//	cellGcmSetSecondVFrequency( CELL_GCM_DISPLAY_FREQUENCY_SCANOUT );

	// Initialize the fence functions.
	CurrentFence = 0;
	FenceLabel = (volatile DWORD *)cellGcmGetLabelAddress( LABEL_FENCE );
	*FenceLabel = 0;
	FlipFence = 0;

	extern volatile DWORD* GCurrentDrawCounterLabel;
	GCurrentDrawCounterLabel = (volatile DWORD *)cellGcmGetLabelAddress( LABEL_DRAWCALL );
	*GCurrentDrawCounterLabel = 0;

	// set clear color back to black
	cellGcmSetClearColor(0x0);

	// some standard state
	cellGcmSetCullFace(CELL_GCM_BACK);
	cellGcmSetPolygonOffsetFillEnable(TRUE);

	// Initialize the platform pixel format map.
	GPixelFormats[PF_Unknown		].PlatformFormat	= 0;//CELL_GCM_NONE;
	GPixelFormats[PF_A32B32G32R32F	].PlatformFormat	= CELL_GCM_TEXTURE_W32_Z32_Y32_X32_FLOAT;
	GPixelFormats[PF_A8R8G8B8		].PlatformFormat	= CELL_GCM_TEXTURE_A8R8G8B8;
	GPixelFormats[PF_G8				].PlatformFormat	= CELL_GCM_TEXTURE_B8;
	GPixelFormats[PF_G16			].PlatformFormat	= 0;//CELL_GCM_NONE;	// Not supported for rendering.
	GPixelFormats[PF_DXT1			].PlatformFormat	= CELL_GCM_TEXTURE_COMPRESSED_DXT1;
	GPixelFormats[PF_DXT3			].PlatformFormat	= CELL_GCM_TEXTURE_COMPRESSED_DXT23;
	GPixelFormats[PF_DXT5			].PlatformFormat	= CELL_GCM_TEXTURE_COMPRESSED_DXT45;
	GPixelFormats[PF_UYVY			].PlatformFormat	= 0;//CELL_GCM_NONE;CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT
	GPixelFormats[PF_FloatRGB		].PlatformFormat	= CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT;
	GPixelFormats[PF_FloatRGB		].BlockBytes		= 8;
	GPixelFormats[PF_FloatRGBA		].PlatformFormat	= CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT;
	GPixelFormats[PF_FloatRGBA		].BlockBytes		= 8;
	GPixelFormats[PF_DepthStencil	].PlatformFormat	= CELL_GCM_TEXTURE_A8R8G8B8;	//CELL_GCM_TEXTURE_DEPTH24_D8;
	GPixelFormats[PF_DepthStencil	].BlockBytes		= 4;
	GPixelFormats[PF_ShadowDepth	].PlatformFormat	= CELL_GCM_TEXTURE_A8R8G8B8;	//CELL_GCM_TEXTURE_DEPTH24_D8;
	GPixelFormats[PF_ShadowDepth	].BlockBytes		= 4;
	GPixelFormats[PF_FilteredShadowDepth ].PlatformFormat	= CELL_GCM_TEXTURE_DEPTH24_D8;	
	GPixelFormats[PF_FilteredShadowDepth ].BlockBytes		= 4;
	GPixelFormats[PF_R32F			].PlatformFormat	= CELL_GCM_TEXTURE_X32_FLOAT;
	GPixelFormats[PF_R32F			].BlockBytes		= 4;
	GPixelFormats[PF_G16R16			].PlatformFormat	= CELL_GCM_TEXTURE_A8R8G8B8;
	GPixelFormats[PF_G16R16			].BlockBytes		= 4;
	GPixelFormats[PF_G16R16F		].PlatformFormat	= CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT;
	GPixelFormats[PF_G16R16F		].BlockBytes		= 8;
	GPixelFormats[PF_G16R16F_FILTER ].PlatformFormat	= CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT;
	GPixelFormats[PF_G16R16F_FILTER ].BlockBytes		= 8;
	GPixelFormats[PF_G32R32F		].PlatformFormat	= CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT;
	GPixelFormats[PF_G32R32F		].BlockBytes		= 8;
	GPixelFormats[PF_A16B16G16R16	].PlatformFormat	= CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT;
	GPixelFormats[PF_A16B16G16R16	].BlockBytes		= 8;
	GPixelFormats[PF_R32F			].Flags				= PF_LINEAR;
	GPixelFormats[PF_DepthStencil	].Flags				= PF_LINEAR;
	GPixelFormats[PF_ShadowDepth	].Flags				= PF_LINEAR;
	GPixelFormats[PF_FilteredShadowDepth].Flags			= PF_LINEAR;
	GPixelFormats[PF_R16F			].PlatformFormat	= CELL_GCM_TEXTURE_X16;
	GPixelFormats[PF_R16F_FILTER	].PlatformFormat	= CELL_GCM_TEXTURE_X16;
	GPixelFormats[PF_V8U8			].PlatformFormat	= CELL_GCM_TEXTURE_G8B8;

	// set out global dealie to this instance
	GPS3Gcm = this;

	// Initialize the occlusion manager.
	PS3StartupOcclusionManager();

	// Allocate the ring buffers
	PatchedPixelShadersRingBuffer = new FFencedRingBuffer(BUFFERSIZE_PATCHEDPIXELSHADERS, PATCHBUFFER_PARTITIONS, 64, MR_Local);
	DynamicRingBuffer = new FFencedRingBuffer(BUFFERSIZE_DYNAMICDATA, DYNAMICDATA_PARTITIONS, 128, MR_Host);

	// Initialize perfmon
	InitPerfCounters();

#if RHI_SUPPORTS_PREVERTEXSHADER_CULLING
	EdgeGeometryOutputBuffer = new FEdgeOutputBuffer(BUFFERSIZE_EDGEGEOMETRYOUTPUT, MR_Local);
#endif

	GPS3GcmControlRegisters = cellGcmGetControlRegister();

#if CELL_GCM_DEBUG
	// Enable single stepping
	// To disable, SET gCellGcmDebugCallback TO NULL! Don't comment out the line
	gCellGcmDebugCallback = cellGcmDebugFinish;
#endif

	// Create 2 backbuffers. Also create a texture which is used as a resolvetarget for both of the backbuffers.
	FTexture2DRHIRef Texture = RHICreateTexture2D(GScreenWidth,GScreenHeight,PF_A8R8G8B8,1,TexCreate_ResolveTargetable,NULL);
	ColorBuffers[0] = RHICreateTargetableSurface(GScreenWidth,GScreenHeight,PF_A8R8G8B8,Texture,TargetSurfCreate_Dedicated,TEXT("Backbuffer0"));
	ColorBuffers[1] = RHICreateTargetableSurface(GScreenWidth,GScreenHeight,PF_A8R8G8B8,Texture,TargetSurfCreate_Dedicated,TEXT("Backbuffer1"));
	DWORD Pitch = ColorBuffers[0]->Pitch;
	check( ColorBuffers[0]->Pitch == ColorBuffers[1]->Pitch && ColorBuffers[0]->Pitch == Texture->GetPitch() );
	check( ColorBuffers[0]->SizeX == GScreenWidth && ColorBuffers[1]->SizeX == GScreenWidth && Texture->GetSizeX() == GScreenWidth );
	check( ColorBuffers[0]->SizeY == GScreenHeight && ColorBuffers[1]->SizeY == GScreenHeight && Texture->GetSizeY() == GScreenHeight );

	// Remember the original buffer addresses, since they are swapped around by RHICopyToResolveTarget and SwapBuffers.
	DisplayBufferOffsets[0] = ColorBuffers[0]->MemoryOffset;
	DisplayBufferOffsets[1] = ColorBuffers[1]->MemoryOffset;
	DisplayBufferOffsets[2] = Texture->MemoryOffset;

	// Register the three memory buffers as display buffers.
	cellGcmSetDisplayBuffer(0, DisplayBufferOffsets[0], Pitch, GScreenWidth, GScreenHeight);
	cellGcmSetDisplayBuffer(1, DisplayBufferOffsets[1], Pitch, GScreenWidth, GScreenHeight);
	cellGcmSetDisplayBuffer(2, DisplayBufferOffsets[2], Pitch, GScreenWidth, GScreenHeight);

	// start rendering to first buffer
	RHISetRenderTarget( ColorBuffers[0], NULL );
}

/**
 * Cleanup. Called from the destructor.
 */
void FPS3Gcm::Cleanup()
{
	PS3ShutdownOcclusionManager();

	// Free ring buffers used for single-use GPU data.
	delete PatchedPixelShadersRingBuffer;
	delete DynamicRingBuffer;

#if RHI_SUPPORTS_PREVERTEXSHADER_CULLING
	delete EdgeGeometryOutputBuffer;
#endif
}

/**
* Initialize the memory (texture, etc) pools. Will use GConfig at this time,
* so appInit must be called before calling this
*/
void FPS3Gcm::InitializeMemoryPools()
{
	// initialize the texture pool

	// Retrieve texture memory pool size from ini.
	INT MemoryPoolSize = 0;
	check(GConfig);	
	verify(GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSize"), MemoryPoolSize, GEngineIni));

	// Allow command line override of the pool size.
	INT CustomPoolSize = 0;
	if( Parse( appCmdLine(), TEXT("-POOLSIZE="), CustomPoolSize) )
	{
		MemoryPoolSize = CustomPoolSize;
	}

	// Allow command line reduction of pool size. We notify the streaming code to be less aggressive.
	INT AmountToReduceBy = 0;
	if( Parse( appCmdLine(), TEXT("-REDUCEPOOLSIZE="), AmountToReduceBy) )
	{
		if( AmountToReduceBy > 0 )
		{
			extern UBOOL GIsOperatingWithReducedTexturePool;
			GIsOperatingWithReducedTexturePool = TRUE;
		}
		MemoryPoolSize = Max( MemoryPoolSize / 2, MemoryPoolSize - AmountToReduceBy );
	}

	debugf(TEXT("Allocating %d MB for texture pool"), MemoryPoolSize);

	// Convert from MByte to byte and align 
	MemoryPoolSize = Align(MemoryPoolSize * 1024 * 1024, PS3TEXTURE_ALIGNMENT);

	// allocate the texture pool and set up FailedAllocationMem to be a distinguishable pointer.
	BYTE* FailedAllocationMem = (BYTE*)GetLocalAllocator()->Malloc(MemoryPoolSize, AT_Texture, PS3TEXTURE_ALIGNMENT);
	BYTE* TexturePoolMem = FailedAllocationMem + PS3TEXTURE_ALIGNMENT;
	MemoryPoolSize -= PS3TEXTURE_ALIGNMENT;

	// use 128 alignment for all textures in the pool, even tho most can be 64
	TexturePool = new FPS3TexturePool(TexturePoolMem, FailedAllocationMem, MemoryPoolSize, PS3TEXTURE_ALIGNMENT);
}

/**
 * Draw a full screen loading screen, without actually using the RHI
 *
 * @param Filename Path to the screen to load
 * @return TRUE if successful
 */
UBOOL FPS3Gcm::DrawLoadingScreen(const TCHAR* Filename)
{
	struct FBitmapFileHeader
	{
		WORD bfType;
		WORD bfSize[2];
		WORD bfReserved1;
		WORD bfReserved2;
		WORD bfOffBits[2];
	};
	struct FBitmapInfoHeader
	{
		DWORD biSize;
		DWORD biWidth;
		DWORD biHeight;
		WORD biPlanes;
		WORD biBitCount;
		DWORD biCompression;
		DWORD biSizeImage;
		DWORD biXPelsPerMeter;
		DWORD biYPelsPerMeter;
		DWORD biClrUsed;
		DWORD biClrImportant;
	};

	// if we already got a shutdown request, just knock off early
	if (GIsRequestingExit)
	{
		return FALSE;
	}

	// load the bmp file into a buffer
	TArray<BYTE> Data;
	if (!appLoadFileToArray(Data, Filename, GFileManager))
	{
		return FALSE;
	}

	// find the header structs inside the data
	BYTE* Buffer = (BYTE*)Data.GetData();
	const FBitmapInfoHeader* BMHDR = (FBitmapInfoHeader*)(Buffer + sizeof(FBitmapFileHeader));
	const FBitmapFileHeader* BMF   = (FBitmapFileHeader*)(Buffer + 0);

	// get dimensions
	DWORD Width = INTEL_ORDER32(BMHDR->biWidth);
	DWORD Height= INTEL_ORDER32(BMHDR->biHeight);

	// Allocate texture in host memory.
	BYTE* Texture = (BYTE*)GetHostAllocator()->Malloc(Width * Height * 4, AT_Texture, PS3TEXTURE_ALIGNMENT);

	// unpack from 24->32
	const BYTE* Ptr = (BYTE*)Buffer + INTEL_ORDER16(BMF->bfOffBits[1]); // @todo: if bad bitmap, fix this to use [0] and [1]
	// convert the 24bit BMP to 32 bit data
	for (DWORD Y = 0; Y < Height; Y++)
	{
		BYTE* DestPtr = Texture + (Y * Width * 4);
		const BYTE* SrcPtr = &Ptr[Y * Width * 3];
		for (DWORD X = 0; X < Width; X++)
		{
			// swap the R and B
			BYTE R = *SrcPtr++;
			BYTE G = *SrcPtr++;
			BYTE B = *SrcPtr++;
			*DestPtr++ = 0xFF;
			*DestPtr++ = B;
			*DestPtr++ = G;
			*DestPtr++ = R;
		}
	}

	TArray<BYTE> VertexShader;
	TArray<BYTE> FragmentShader;
	appLoadFileToArray(VertexShader, TEXT("..\\Engine\\Splash\\PS3\\LoadScreen.vpo"));
	appLoadFileToArray(FragmentShader, TEXT("..\\Engine\\Splash\\PS3\\LoadScreen.fpo"));

	CGprogram VertexProgram = (CGprogram)VertexShader.GetData();
	CGprogram FragmentProgram = (CGprogram)FragmentShader.GetData();

	// init
	cellGcmCgInitProgram(VertexProgram);
	cellGcmCgInitProgram(FragmentProgram);

	uint32_t MicrocodeSize;
	void *Microcode;

	// get the fragment program microcode
	cellGcmCgGetUCode(FragmentProgram, &Microcode, &MicrocodeSize);
	// 64B alignment required 
	void* FragmentProgramMicrocode = GetLocalAllocator()->Malloc(MicrocodeSize, AT_PixelShader, 64);
	UINT FragmentOffset;
	cellGcmAddressToOffset(FragmentProgramMicrocode, &FragmentOffset);
	memcpy(FragmentProgramMicrocode, Microcode, MicrocodeSize); 

	// get the vertex program microcode
	cellGcmCgGetUCode(VertexProgram, &Microcode, &MicrocodeSize);
	void* VertexProgramMicrocode = Microcode;

	// get input parms
	CGparameter PosParam = cellGcmCgGetNamedParameter(VertexProgram, "Pos");
	CGparameter UVParam = cellGcmCgGetNamedParameter(VertexProgram, "UV");

	// get attrib indices
	INT PosIndex = cellGcmCgGetParameterResource(VertexProgram, PosParam) - CG_ATTR0;
	INT UVIndex = cellGcmCgGetParameterResource(VertexProgram, UVParam) - CG_ATTR0;


	struct FLoadScreenVertex
	{
		FLOAT X, Y, Z, U, V;
	};

	// allocate mem for verts
	FLoadScreenVertex* Verts = (FLoadScreenVertex*)GetHostAllocator()->Malloc(sizeof(FLoadScreenVertex) * 4, AT_VertexBuffer, 128);
	UINT VertsOffset;
	cellGcmAddressToOffset(Verts, &VertsOffset);

	cellGcmSetVertexProgram(VertexProgram, VertexProgramMicrocode);
//	cellGcmSetVertexProgramParameter(model_view_projection, MVP);
	cellGcmSetVertexDataArray(PosIndex,
			0, 
			sizeof(FLoadScreenVertex), 
			3, 
			CELL_GCM_VERTEX_F, 
			CELL_GCM_LOCATION_MAIN, 
			(uint32_t)VertsOffset + STRUCT_OFFSET(FLoadScreenVertex, X));
	cellGcmSetVertexDataArray(UVIndex,
			0, 
			sizeof(FLoadScreenVertex), 
			2, 
			CELL_GCM_VERTEX_F, 
			CELL_GCM_LOCATION_MAIN, 
			(uint32_t)VertsOffset + STRUCT_OFFSET(FLoadScreenVertex, U));


	cellGcmSetFragmentProgram(FragmentProgram, FragmentOffset);

	// set full screen quad, accounting for 4x3 black bars
	FLOAT Ratio = bIsScreen4x3 ? 0.75f : 1.0f;

#if PATCH_VERSION
	Verts[0].X = -1.0f; Verts[0].Y = -Ratio; Verts[0].Z = 1.0f; Verts[0].U = 0.0f; Verts[0].V = 1.0f;
	Verts[1].X = -1.0f; Verts[1].Y =  Ratio; Verts[1].Z = 1.0f; Verts[1].U = 0.0f; Verts[1].V = 0.0f;
	Verts[2].X =  1.0f; Verts[2].Y =  Ratio; Verts[2].Z = 1.0f; Verts[2].U = 1.0f; Verts[2].V = 0.0f;
	Verts[3].X =  1.0f; Verts[3].Y = -Ratio; Verts[3].Z = 1.0f; Verts[3].U = 1.0f; Verts[3].V = 1.0f;
#else
	Verts[0].X = -1.0f; Verts[0].Y = -Ratio; Verts[0].Z = 1.0f; Verts[0].U = 0.0f; Verts[0].V = 0.0f;
	Verts[1].X = -1.0f; Verts[1].Y =  Ratio; Verts[1].Z = 1.0f; Verts[1].U = 0.0f; Verts[1].V = 1.0f;
	Verts[2].X =  1.0f; Verts[2].Y =  Ratio; Verts[2].Z = 1.0f; Verts[2].U = 1.0f; Verts[2].V = 1.0f;
	Verts[3].X =  1.0f; Verts[3].Y = -Ratio; Verts[3].Z = 1.0f; Verts[3].U = 1.0f; Verts[3].V = 0.0f;
#endif

	// set texture
	CellGcmTexture TexInfo;
	TexInfo.format		= CELL_GCM_TEXTURE_A8R8G8B8 | CELL_GCM_TEXTURE_LN;
	TexInfo.mipmap		= 1;
	TexInfo.dimension	= CELL_GCM_TEXTURE_DIMENSION_2;
	TexInfo.cubemap		= CELL_GCM_FALSE;
	TexInfo.remap		= GTextureRemapARGB;
	TexInfo.width		= Width;
	TexInfo.height		= Height;
	TexInfo.depth		= 1;
	TexInfo.location	= CELL_GCM_LOCATION_MAIN;
	TexInfo.pitch		= Width * 4;
	cellGcmAddressToOffset(Texture, &TexInfo.offset);
	cellGcmSetTexture(0, &TexInfo);
	cellGcmSetTextureControl(0, CELL_GCM_TRUE, 0, 0, CELL_GCM_TEXTURE_MAX_ANISO_1); // MIN:0,MAX:0,ANISO:1
	cellGcmSetTextureFilter(0, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	cellGcmSetTextureAddress(0, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	cellGcmSetDepthTestEnable(CELL_GCM_FALSE);

	// clear frame buffer if there are black bars for aspect ratio

	if (Ratio != 1.0f)
	{
		cellGcmSetClearSurface(CELL_GCM_CLEAR_R|CELL_GCM_CLEAR_G|CELL_GCM_CLEAR_B|CELL_GCM_CLEAR_A);
	}
	// set draw command
	cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_QUADS, 0, 4);

	SwapBuffers();

	// free up the memory
	BlockUntilGPUIdle();
	GetLocalAllocator()->Free(FragmentProgramMicrocode);
	GetHostAllocator()->Free(Verts);
	GetHostAllocator()->Free(Texture);


	// make sure the system menu (PS button) will show up during the static loading screen

	// if the rendering thread has been started and we are on the game thread, then send the command over the queue
	// note: this is untested as Epic only calls this function before the rendering thread is created
	if (GUseThreadedRendering && IsInGameThread())
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND( SuspendRendering, { extern UBOOL GGameThreadWantsToSuspendRendering; GGameThreadWantsToSuspendRendering = TRUE; } );
	}
	else
	{
		// otherwise call it directly
		RHISuspendRendering();
	}

	return TRUE;
}


/**
 * Queue a frame buffer swap. May block.
 */
DWORD GFlipIDToWaitFor = 0;
void FPS3Gcm::SwapBuffers()
{
	if ( TexturePool )
	{
		INT Result = TexturePool->Tick();
	}

	// Use a fence to make sure the GPU isn't more than 1 full frame behind
	if ( FlipFence )
	{
		// Wait for previous flip to have been processed before adding another user callback.
		// Otherwise, the user callback could overwrite GBufferToFlip if it's called twice
		// in a row with no flip in between (vsync case only).
		BlockOnFence( FlipFence );
	}

	{
		if (bNeedsScreenCleared)
		{
			cellGcmSetClearColor(0);
			cellGcmSetClearSurface(CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		}

		// End a new STATS GPU frame.
		cellGcmSetTimeStamp( GFrameTimeStamps[GFrameSelector*2 + 1] );

	    // Figure out which displaybuffer contains the final image.
	    // It could the resolve-target or the current backbuffer, since RHICopyToResolveTarget may have swapped the pointers.
	    BYTE BackBufferID;
	    if ( ColorBuffers[BackBufferIndex]->MemoryOffset == DisplayBufferOffsets[0] )
	    {
		    BackBufferID = 0;
	    }
	    else if ( ColorBuffers[BackBufferIndex]->MemoryOffset == DisplayBufferOffsets[1] )
	    {
		    BackBufferID = 1;
	    }
	    else
	    {
		    BackBufferID = 2;
	    }
    
	    // Reset the memory pointers for the back-buffers and their resolve target.
	    BYTE FrontBufferID = (BackBufferID + 1) % 3;
	    BYTE ResolveTargetID = (BackBufferID + 2) % 3;
	    UINT BackBufferOffset = DisplayBufferOffsets[BackBufferID];
	    UINT FrontBufferOffset = DisplayBufferOffsets[FrontBufferID];
	    UINT ResolveTargetOffset = DisplayBufferOffsets[ResolveTargetID];
	    ColorBuffers[0]->ResolveTargetMemoryOffset = ResolveTargetOffset;
	    ColorBuffers[0]->ResolveTarget->SetMemoryOffset( ResolveTargetOffset );
	    ColorBuffers[1]->ResolveTargetMemoryOffset = ResolveTargetOffset;
	    ColorBuffers[1]->ResolveTarget->SetMemoryOffset( ResolveTargetOffset );
	    ColorBuffers[BackBufferIndex]->MemoryOffset = BackBufferOffset;
	    ColorBuffers[1-BackBufferIndex]->MemoryOffset = FrontBufferOffset;

		// Trigger OSD and prepare for flip.
		GFlipIDToWaitFor = cellGcmSetPrepareFlip( BackBufferID );
		check(GFlipIDToWaitFor < 8);

		// Trigger User Callback. Either this or the VBlank callback will perform the flip.
		cellGcmSetUserCommand( GFlipIDToWaitFor | (GFrameSelector<<16) );

		// This fence will let us know if the GPU hang on the wait label, by checking the FIFO dump generated from BlockOnFence().
		InsertFence( FALSE );

		// Tell GPU to wait for flip to be finished.
		cellGcmSetWaitLabel( LABEL_BUFFERFLIPPED, GFlipIDToWaitFor );

		// Reset the status on the wait label.
		cellGcmSetWriteCommandLabel( LABEL_BUFFERFLIPPED, 255 );
	}

	// Insert a fence and kick the command buffer.
	FlipFence = InsertFence( TRUE );

	// go to other render target 
	BackBufferIndex ^= 1;
	GFrameSelector ^= 1;

	/** Start a new STATS GPU frame. */
	cellGcmSetTimeStamp( GFrameTimeStamps[GFrameSelector*2] );

	// use new frame buffer
	// @todo: pointless ingame because we just set immediately set to the FP16 render target ?
	RHISetRenderTarget( ColorBuffers[BackBufferIndex], NULL );

	// remember when we last flipped
	GLastSwapTime = appSeconds();

#if !FINAL_RELEASE && CELL_GCM_DEBUG
	const UBOOL bOldValue = GEmitDrawEvents;

	UINT Status;
	// is GPAD currently capturing?
	cellGcmGpadGetStatus(&Status);
	GEmitDrawEvents = (Status & CELL_GCM_GPAD_STATUS_IS_CAPTURING) != 0;

	if (GEmitDrawEvents != bOldValue)
	{
		debugf(TEXT("Draw events %s\n"), GEmitDrawEvents ? TEXT("ENABLED") : TEXT("DISABLED"));
	}
#endif
}

// @GEMINI_TODO: Debug variable
BYTE* GRTPtr = NULL;
BYTE* GDBPtr = NULL;
DWORD GRTFormat = 0;

/**
 * Set the render target, sets up the viewport as the size in the surface
 * 
 * @param Surface Structure describing the surface/render target
 */
void FPS3Gcm::SetRenderTarget(const CellGcmSurface* Surface)
{
	GRTPtr = (BYTE*)GRSXBaseAddress + Surface->colorOffset[0]; 
	GDBPtr = (BYTE*)GRSXBaseAddress + Surface->depthOffset; 
	GRTFormat = Surface->colorFormat;

	if ( Surface->depthOffset == 0 )
	{
		// Turn off both depth reads and writes.
		cellGcmSetDepthTestEnable( CELL_GCM_FALSE );
		cellGcmSetDepthMask( CELL_GCM_FALSE );
	}

	// set inital target
	cellGcmSetSurface(Surface);

	// remeber if this is a FP RT or not
	DWORD Format = Surface->colorFormat;
	bIsCurrentRenderTargetFloatingPoint = (Format == CELL_GCM_SURFACE_F_W16Z16Y16X16 || Format == CELL_GCM_SURFACE_F_W32Z32Y32X32 || Format == CELL_GCM_SURFACE_F_X32);
	CurrentRenderTargetSizeX = Surface->width;
	CurrentRenderTargetSizeY = Surface->height;

	// Reset viewport (mimic DirectX)
	RHISetViewport(  0, 0, 0.0f, Surface->width, Surface->height, 1.0f);
}

void FPS3Gcm::InitPerfCounters( )
{
	//@todo
}

void FPS3Gcm::SetupPerfCounters( UINT Domain, const UINT Counters[4] )
{
	//@todo
}

void FPS3Gcm::TriggerPerfCounters()
{
	//@todo
}

UINT FPS3Gcm::ReadPerfCounters( UINT Domain, UINT Counters[4] )
{
	//@todo
	return 0;
}

/** Inserts a fence into the command buffer and flush. The inserted fence is returned. */
DWORD FPS3Gcm::InsertFence( UBOOL bKickBuffer/*=TRUE*/ )
{
	// Flush any pending occlusion queries
	PS3FlushOcclusionManager();

	// Reset the counter for when to periodically insert new fences.
	PS3ResetAutoFence();

	++CurrentFence;
	cellGcmSetWriteBackEndLabel( LABEL_FENCE, CurrentFence );

	if ( bKickBuffer )
	{
		Flush();
	}
	return CurrentFence;
}

SQWORD GCachedGPUFence = 0;

/** Returns TRUE if the specified fence has not yet been processed by the GPU, otherwise FALSE. */
UBOOL FPS3Gcm::IsFencePending( DWORD Fence )
{
	SQWORD GPUFence = GCachedGPUFence;
	SQWORD CheckFence = Fence;

	// Detect if either fence number has wrapped around
	UBOOL bWrapped = (Abs(CheckFence - GPUFence) > (1<<30)) ? TRUE : FALSE;

	UBOOL bIsPending = (GPUFence < CheckFence) || (bWrapped && CheckFence < GPUFence);

	if ( bIsPending )
	{
		// Do it again with a refreshed GPU Fence.
		GCachedGPUFence = *FenceLabel;
		SQWORD GPUFence = GCachedGPUFence;
		SQWORD CheckFence = Fence;
		UBOOL bWrapped = (Abs(CheckFence - GPUFence) > (1<<30)) ? TRUE : FALSE;
		bIsPending = (GPUFence < CheckFence) || (bWrapped && CheckFence < GPUFence);
	}

	return bIsPending;
}

/**
 *	Blocks the CPU until the specified fence has been processed by the GPU.
 *	Asserts and dumps the RSX command buffer if blocked more than GMaxBlockOnFenceWaitTime seconds.
 */
DOUBLE GMaxBlockOnFenceWaitTime = 5.0;
CellGcmControl* GCurrentControl = NULL;
void* GCrashGetPtr = NULL;
void* GCrashPutPtr = NULL;
void FPS3Gcm::BlockOnFence( DWORD Fence )
{
	if ( IsFencePending(Fence) )
	{
		DWORD IdleStart = appCycles();
		SQWORD GPUFenceStart = *FenceLabel;
		SCOPE_CYCLE_COUNTER(STAT_BlockedByGPU);
		RHIKickCommandBuffer();

		DOUBLE StartTime = appSeconds();
		do
		{
			Flush();
			appSleep( 0.0002f );	// Sleep 0.2 ms
#if DO_CHECK || FINAL_RELEASE_DEBUGCONSOLE
			// only wait so long before deciding the GPU has crashed/hung.
			if ( !ShouldIgnoreLongStalls() )
			{
				DOUBLE CurrentTime = appSeconds();
				DOUBLE WaitTime = CurrentTime - StartTime;
				if (WaitTime > GMaxBlockOnFenceWaitTime && IsFencePending(Fence))
				{
					// Restart timer with a very high threshold
					GMaxBlockOnFenceWaitTime = 10000.0;
					StartTime = CurrentTime;

					SQWORD GPUFenceEnd = *FenceLabel;
					// We think the GPU has hung, but the OS hasn't dumped. Dump the FIFO around the current get offset
					// to a file to help diagnose the problem. Use RSXFifoDump to disassemble the buffer.
					GCurrentControl = cellGcmGetControlRegister();
					uint32_t GetOffset = GCurrentControl->get;
					uint32_t PutOffset = GCurrentControl->put;
					UINT* GetPtr = NULL;
					UINT* PutPtr = NULL;
					cellGcmIoOffsetToAddress( GetOffset, &(void*&)GetPtr );
					cellGcmIoOffsetToAddress( PutOffset, &(void*&)PutPtr );
					GCrashGetPtr = GetPtr;
					GCrashPutPtr = PutPtr;

					// dump the FIFO to a binary file.
					FString FifoDumpFileName;
					{
						FMemMark MemStackMark(GRenderingThreadMemStack);

						// Start reading 32 KB before the GET for some context.
						UINT DumpStartOffset = (GetOffset > 32*1024) ? (GetOffset - 32*1024) : 0;
						UINT* DumpStartAddress = NULL;
						cellGcmIoOffsetToAddress( DumpStartOffset, &(void*&)DumpStartAddress );

						// Set the filename
						FString BaseFileName = FString::Printf(TEXT("RSXFIFO_%s_0x%08X_G=0x%08X_P=0x%08X.cmb"), *appSystemTimeString(), DumpStartOffset, GetOffset, PutOffset);
						FifoDumpFileName = appGameLogDir() + BaseFileName;
						ANSICHAR PlatformFilename[CELL_FS_MAX_FS_PATH_LENGTH] = "/app_home";
						extern void GetPlatformFilename(const TCHAR* Filename, ANSICHAR* OutputFilename);
						GetPlatformFilename(*FifoDumpFileName, PlatformFilename + strlen(PlatformFilename));
						// Make the extension lower-case again (required for GPAD to be able to read the file).
						strcpy(PlatformFilename + strlen(PlatformFilename) - 3, "cmb");

						// Allocate a buffer and flatten the FIFO into it.
						UINT FIFOSize = cellRSXFifoGetFlattenedSize( DumpStartAddress, PutPtr );	// In bytes
						void* FIFOBaseAddress = GetDefaultCommandBufferStart();
						UINT FIFOBaseOffset = 0;
						cellGcmAddressToOffset(FIFOBaseAddress, &FIFOBaseOffset);
						UINT* FlattenedFIFOBuffer = new (GRenderingThreadMemStack) UINT[FIFOSize/4 + 1];
						UINT FlattenedSize = cellRSXFifoFlattenToMemory( FlattenedFIFOBuffer, FIFOSize, DumpStartAddress, PutPtr );	// In bytes

						// Write it to file.
						FILE* FIFOFile = fopen(PlatformFilename, "wb");
						fwrite(FlattenedFIFOBuffer, 1, FlattenedSize, FIFOFile);
						fclose(FIFOFile);
					}
					extern volatile DWORD* GCurrentDrawCounterLabel;
					printf( "\n============== GPU CRASH ==============\nLast GPU fence passed: %d\nLast Drawcall label passed: %d:\n", DWORD(GPUFenceEnd), *GCurrentDrawCounterLabel );
//					extern void PrintGPUHostMemoryMap();
//					PrintGPUHostMemoryMap();

					// Check if the SPUs have crashed.
					extern void PS3KickSPUCommandBuffer();
					PS3KickSPUCommandBuffer();

#if DO_CHECK
					// crash the app
					appErrorf(TEXT(
						"The GPU has probably crashed/hung!\n"
						"Waited %.1f seconds for fence %d to finish. In that time the GPU went from fence %lld to %lld.\n"
						"The RSX FIFO was dumped to file: %s")
						, WaitTime, Fence, GPUFenceStart, GPUFenceEnd, *FifoDumpFileName);
#endif
				}
			}
#endif
		} while ( IsFencePending(Fence) );

		GRenderThreadIdle += appCycles() - IdleStart;
	}
}

/** Returns the last fence inserted in the command buffer. */
DWORD FPS3Gcm::GetCurrentFence()
{
	return CurrentFence;
}

void FPS3Gcm::Flush()
{
	cellGcmFlush();
}

UBOOL GIsShutdownAlready = FALSE;
/** Blocks the PPU until the GPU is idle. */
void FPS3Gcm::BlockUntilGPUIdle()
{
	if (GIsShutdownAlready)
	{
		return;
	}
	SCOPE_CYCLE_COUNTER(STAT_BlockedByGPU);
	static DWORD GPUIdleCounter = 0xffffffff;

	// Tell the GPU to synchronize itself (flushing the pipeline all the way to the backend)
	DWORD Fence = InsertFence( TRUE );
	cellGcmSetWaitLabel( LABEL_FENCE, Fence );

	// Then we must call cellGcmFinish().
	cellGcmFinish(gCellGcmCurrentContext, ++GPUIdleCounter );
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
UBOOL FPS3Gcm::AllocateTiledMemory( FPS3RHIGPUResource &Resource, UINT &Pitch, EPixelFormat Format, EMemoryRegion MemoryRegion, DWORD Width, DWORD Height, DWORD MultisampleType )
{
	// Check for availability of a Tile register.
	DWORD TileIndex = 0;
    while ( TileIndex < MAX_TILESLOTS && TileMemory[TileIndex].bUsed )
		++TileIndex;
	if (TileIndex == MAX_TILESLOTS)
	{
		debugf(NAME_Warning, TEXT("Attempted to allocate a tiled texture/surface, but we're out of Tile Registers! %s, %u x %u"), UTexture::GetPixelFormatString(Format), Width, Height);

		// Return a tiled pitch. This will make it so that a render target that resolves to a texture, 
		// but if one is not tiled, then the resolve code would mess up. This will allocate more GPU 
		// memory, but if we didn't run out of tiled registers (which we shouldn't by ship), it would
		// use the extra GPU memory anyway
		Pitch = cellGcmGetTiledPitchSize( Pitch );
		return FALSE;
	}

	DWORD NewPitch = cellGcmGetTiledPitchSize( Pitch );

	// The number of rows allocated must be a multiple of 32.
	DWORD Rows = Align( Height, 32 );

	// The total size must be a multiple of 64 KB. (Padding at the end.)
	DWORD TotalSize = Align( NewPitch * Rows, 64 * 1024 );

	// Allocate memory for textures that are also used as render targets
	Resource.AllocateMemory( TotalSize, MemoryRegion, AT_RenderTarget, 64 * 1024 );

	// Mark resource as using tiled register.
	Resource.bUsesTiledRegister = TRUE;

	UBOOL bCompressed	= (Format == PF_DepthStencil) || (Format == PF_FilteredShadowDepth);
	UBOOL bUseZCull		= (Format == PF_DepthStencil) || (Format == PF_FilteredShadowDepth);
	DWORD CompressionTagAddress = 0;

	// Setup compression tag memory if requested.
	if ( bCompressed )
	{
		DWORD CompressionTagSize = Align( TotalSize, 0x10000 ) / 0x10000;
		CompressionTagAddress = (DWORD) CompressionTagMem->Malloc( CompressionTagSize, AT_CompressionTag, 1 );
		if ( CompressionTagAddress )
		{
			CompressionTagAddress -= 0x1000;	// Subtract the dummy base address
		}
		else
		{
			debugf(NAME_Warning, TEXT("Attempted to allocate compression tag memory, but we're out of space!"));
			bCompressed = FALSE;
		}
	}

	// Bind the tiled memory to the buffer.
	if ( bCompressed )
	{
		cellGcmSetTileInfo(TileIndex, MemoryRegion, Resource.MemoryOffset, TotalSize, NewPitch, CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR, CompressionTagAddress, 2);
	}
	else
	{
		cellGcmSetTileInfo(TileIndex, MemoryRegion, Resource.MemoryOffset, TotalSize, NewPitch, CELL_GCM_COMPMODE_DISABLED, 0, 0);
	}
	BlockUntilGPUIdle();
	cellGcmBindTile(TileIndex);

	// Setup zcull memory if requested.
	DWORD ZCullIndex = 0;
	DWORD ZCullAddress = 0;
	if ( bUseZCull )
	{
		// Find the end of the used ZCull memory.
		ZCullAddress = 0;
		for ( DWORD Index=0; Index < MAX_ZCULLSLOTS; ++Index )
		{
			if ( ZCullMemory[Index].bUsed )
			{
				ZCullAddress += Align( Align(ZCullMemory[Index].Width,64) * Align(ZCullMemory[Index].Height,64), 4096 );
			}
		}

		// Find an empty ZCull entry slot
		ZCullIndex = 0;
		while ( ZCullIndex < MAX_ZCULLSLOTS && ZCullMemory[ZCullIndex].bUsed )
		{
			++ZCullIndex;
		}

		// ZCull width and height must be a aligned to 64, and the total must be aligned to 4KB.
		DWORD ZCullSize = Align( Align(Width,64) * Align(Height,64), 4096 );
		if ( (ZCullIndex == MAX_ZCULLSLOTS) || ((ZCullAddress+ZCullSize) > MAX_ZCULLMEMORY) )
		{
			debugf(NAME_Warning, TEXT("Attempted to allocate zcull memory, but we're out of space!"));
			bUseZCull = FALSE;
		}
	}

	if ( bUseZCull )
	{
		// Setup the zcull entry.
		appMemzero( &ZCullMemory[ ZCullIndex ], sizeof(ZCullMemory[ ZCullIndex ]) );
		ZCullMemory[ ZCullIndex ].bUsed				= TRUE;
		ZCullMemory[ ZCullIndex ].Width				= Width;
		ZCullMemory[ ZCullIndex ].Height			= Height;
		ZCullMemory[ ZCullIndex ].MultisampleType	= MultisampleType;

		// Bind the zcull memory to the buffer.
		BlockUntilGPUIdle();
		cellGcmBindZcull(
			ZCullIndex,				// z-cull index
			Resource.MemoryOffset,	// Memory offset of z-buffer (aligned to 4KB)
			Align(Width,64),		// Width of z-buffer (aligned to 64 pixels)
			Align(Height,64),		// Height of z-buffer (aligned to 64 pixels)
			ZCullAddress,			// Offset into zcull memory
			CELL_GCM_ZCULL_Z24S8,	// Format of z-buffer
			MultisampleType,		// Multisample type
			CELL_GCM_ZCULL_LESS,	// Cull direction
			CELL_GCM_ZCULL_LONES,	// Compression mode for the Z-values
			CELL_GCM_SCULL_SFUNC_NOTEQUAL,	// Stencil test direction
			0x0,					// Stencil ref
			0xff);					// Stencil mask
	}

	// Setup the tile entry.
	appMemzero( &TileMemory[ TileIndex ], sizeof(TileMemory[ TileIndex ]) );
	TileMemory[ TileIndex ].bUsed				= TRUE;
	TileMemory[ TileIndex ].bUseZcull			= bUseZCull;
	TileMemory[ TileIndex ].bUseCompression		= bCompressed;
	TileMemory[ TileIndex ].MemoryOffset		= Resource.MemoryOffset;
	TileMemory[ TileIndex ].ZCullIndex			= ZCullIndex;
	TileMemory[ TileIndex ].CompressionAddress	= CompressionTagAddress;

	Pitch = NewPitch;
	return TRUE;
}

/**
 * Unbinds the tiled region used by the resource. Does NOT free the memory.
 * Silently fails if the resource isn't bind to a tiled region.
 * @param Resource		Resource which is bound to a tiled region
 */
void FPS3Gcm::UnbindTiledMemory( FPS3RHIGPUResource &Resource )
{
	DWORD MemoryOffset = Resource.MemoryOffset;

	// Find the FTiledEntry representing the specified MemoryOffset
	DWORD TileIndex = 0;
	while ( TileIndex < MAX_TILESLOTS && (!TileMemory[TileIndex].bUsed || TileMemory[TileIndex].MemoryOffset != MemoryOffset) )
		++TileIndex;
	if ( TileIndex == MAX_TILESLOTS )
		return;

	// Mark the tile entry as free.
	TileMemory[ TileIndex ].bUsed = FALSE;

	// Free the compression tag memory, if used.
	if ( TileMemory[TileIndex].bUseCompression )
	{
		// Add the dummy 0x1000 offset.
		CompressionTagMem->Free( (void*) (TileMemory[TileIndex].CompressionAddress + 0x1000) );
		TileMemory[ TileIndex ].bUseCompression = FALSE;
		TileMemory[ TileIndex ].CompressionAddress = 0;
	}

	// Free the zcull memory, if used.
	if ( TileMemory[TileIndex].bUseZcull )
	{
		// Unbind all z-cull regions.
		for ( DWORD Index=0; Index < MAX_ZCULLSLOTS; ++Index )
		{
			BlockUntilGPUIdle();
			cellGcmUnbindZcull( Index );
		}

		// Free the ZCull entry.
		DWORD ZCullIndex = TileMemory[TileIndex].ZCullIndex;
		appMemzero( &ZCullMemory[ZCullIndex], sizeof(ZCullMemory[ZCullIndex]) );
		TileMemory[ TileIndex ].bUseZcull = FALSE;
		TileMemory[ TileIndex ].ZCullIndex = 0;

		// Setup all z-cull slots again, except the one we're deleting.
		DWORD ZCullAddress = 0;
		for ( DWORD Index=0; Index < MAX_TILESLOTS; ++Index )
		{
			FTiledEntry& TileEntry = TileMemory[Index];
			if ( TileEntry.bUsed && TileEntry.bUseZcull )
			{
				DWORD ZCullIndex = TileEntry.ZCullIndex;
				BlockUntilGPUIdle();
				cellGcmBindZcull(
					ZCullIndex,
					TileEntry.MemoryOffset,
					Align(ZCullMemory[ ZCullIndex ].Width, 64),
					Align(ZCullMemory[ ZCullIndex ].Height, 64),
					ZCullAddress,
					CELL_GCM_ZCULL_Z24S8,
					ZCullMemory[ ZCullIndex ].MultisampleType,
					CELL_GCM_ZCULL_LESS,
					CELL_GCM_ZCULL_LONES,
					CELL_GCM_SCULL_SFUNC_NOTEQUAL,
					0x00,
					0xff );
				ZCullAddress += Align( Align(ZCullMemory[ ZCullIndex ].Width,64) * Align(ZCullMemory[ ZCullIndex ].Height,64), 4096 );
			}
		}
	}

	// Free the tiled memory
	appMemzero( &TileMemory[TileIndex], sizeof(TileMemory[TileIndex]) );
	BlockUntilGPUIdle();
	cellGcmUnbindTile( TileIndex );
}

/**
 * @return TRUE if the memory is a currently tiled region
 */
UBOOL FPS3Gcm::IsMemoryTiled( DWORD MemoryOffset )
{
	// Find the FTiledEntry representing the specified MemoryOffset
	DWORD TileIndex = 0;
	while ( TileIndex < MAX_TILESLOTS && (!TileMemory[TileIndex].bUsed || TileMemory[TileIndex].MemoryOffset != MemoryOffset) )
		++TileIndex;

	// if we found it, it's tiled
	return TileIndex != MAX_TILESLOTS;
}

/** Resets the state of currently set GPU resources, so that they can be freed from memory. */
void FPS3Gcm::ResetGPUState()
{
	// Setup our default state.

	// Disable all texture units. If they are enabled, they may be read from during SwapBuffers (by OS/SDK/RSX, not us)
	// and this can cause RSX crash if the texture has been freed and the memory has been unmapped.
	CellGcmTexture NullTexture;
	NullTexture.format		= CELL_GCM_TEXTURE_A8R8G8B8 | CELL_GCM_TEXTURE_LN;
	NullTexture.mipmap		= 1;
	NullTexture.dimension	= CELL_GCM_TEXTURE_DIMENSION_2;
	NullTexture.cubemap		= CELL_GCM_FALSE;
	NullTexture.remap		= GTextureRemapARGB;
	NullTexture.width		= 1;
	NullTexture.height		= 1;
	NullTexture.depth		= 1;
	NullTexture.location	= CELL_GCM_LOCATION_LOCAL;
	NullTexture.pitch		= 4;
	NullTexture.offset		= 0;
	cellGcmSetInvalidateTextureCache(CELL_GCM_INVALIDATE_TEXTURE);
	cellGcmSetInvalidateTextureCache(CELL_GCM_INVALIDATE_VERTEX_TEXTURE);
	for ( INT TextureUnit=0; TextureUnit < 16; ++TextureUnit )
	{
		cellGcmSetTextureControl( TextureUnit, CELL_GCM_FALSE, (0 << 8) | 0, (12 << 8) | 0, CELL_GCM_TEXTURE_MAX_ANISO_1 );
		cellGcmSetTexture( TextureUnit, &NullTexture );
		GCurrentTexture[TextureUnit] = NULL;
	}
	GEnabledTextures = 0;
	GNumFatTexturesUsed = 0;
	GNumSemiFatTexturesUsed = 0;

	// Do the same for vertex attributes.
	cellGcmSetInvalidateVertexCache();
	uint16_t FrequencyMask = 0;
	for ( INT AttributeIndex=0; AttributeIndex < 16; ++AttributeIndex )
	{
		FrequencyMask |= CELL_GCM_FREQUENCY_MODULO << AttributeIndex;
		cellGcmSetVertexDataArray(AttributeIndex,0,0,0,CELL_GCM_VERTEX_F,MR_Local,0);
		GVertexInputsUsed[AttributeIndex] = 0;
	}
	cellGcmSetVertexAttribInputMask( 0 );
	cellGcmSetFrequencyDividerOperation( FrequencyMask );	// 0xffff, written for clarity :)

	// Convert engine max anisotropy to gcm value.
	if( GSystemSettings.MaxAnisotropy > 12  )
	{
		GPS3MaxAnisotropy = CELL_GCM_TEXTURE_MAX_ANISO_16;
	}
	else if( GSystemSettings.MaxAnisotropy > 10 )
	{
		GPS3MaxAnisotropy = CELL_GCM_TEXTURE_MAX_ANISO_12;
	}
	else if( GSystemSettings.MaxAnisotropy > 8 )
	{
		GPS3MaxAnisotropy = CELL_GCM_TEXTURE_MAX_ANISO_10;
	}
	else if( GSystemSettings.MaxAnisotropy > 6 )
	{
		GPS3MaxAnisotropy = CELL_GCM_TEXTURE_MAX_ANISO_8;
	}
	else if( GSystemSettings.MaxAnisotropy > 4 )
	{
		GPS3MaxAnisotropy = CELL_GCM_TEXTURE_MAX_ANISO_6;
	}
	else if( GSystemSettings.MaxAnisotropy > 2 )
	{
		GPS3MaxAnisotropy = CELL_GCM_TEXTURE_MAX_ANISO_4;
	}
	else if( GSystemSettings.MaxAnisotropy > 1 )
	{
		GPS3MaxAnisotropy = CELL_GCM_TEXTURE_MAX_ANISO_2;
	}
	else
	{
		GPS3MaxAnisotropy = CELL_GCM_TEXTURE_MAX_ANISO_1;
	}

	// Reset our internal shader variables.
	GCurrentPixelShader = NULL;
	GCurrentVertexShader = NULL;

	// Reset some state.
	cellGcmSetZMinMaxControl( CELL_GCM_TRUE, CELL_GCM_FALSE, CELL_GCM_FALSE );
}

/**
 * Called before rendering a scene.
 */
void FPS3Gcm::BeginScene()
{
	ResetGPUState();
}


/**
 * Called when finished rendering a scene.
 */
void FPS3Gcm::EndScene()
{
	ResetGPUState();
}

/**
 * Should we ignore long GPU or SPU stalls? Used for GPAD which can stall for long periods of time
 */
UBOOL FPS3Gcm::ShouldIgnoreLongStalls()
{
	// if we're emitting draw events, then we are capturing, or want to capture, so ignore stalls
	return GEmitDrawEvents;
}

/**
 * Sets various memory stats.
 */
void FPS3Gcm::UpdateMemStats()
{
	for ( INT StatIndex=STAT_FirstGPUStat+1; StatIndex < STAT_LastGPUStat; ++StatIndex )
	{
		SET_DWORD_STAT(StatIndex, 0);
	}
	// let the GPU allocators update particular stats
	GetLocalAllocator()->UpdateMemStats(STAT_LocalCommandBufferSize);
	for( INT HostMemIdx=0; HostMemIdx < HostMem_MAX; HostMemIdx++ )
	{
		const EHostMemoryHeapType HostMemType = (EHostMemoryHeapType)HostMemIdx; 
		if( HasHostAllocator(HostMemType) )
		{
			GetHostAllocator(HostMemType)->UpdateMemStats(STAT_HostCommandBufferSize);
		}
	}	
}

/** description of each of the host mem heaps. coupled with EHostMemoryHeapType */
static const TCHAR* HostMemoryHeapDesc[HostMem_MAX]=
{
	TEXT("Default"),
	TEXT("Movies")	
};

/** 
* Friendly name of host allocator type
*
* @param HostMemType - EHostMemoryHeapType for the host mem heap we want
* @return text description of the allocator 
*/
const TCHAR* FPS3Gcm::GetHostAllocatorDesc(EHostMemoryHeapType HostMemType)
{
	return HostMemoryHeapDesc[HostMemType];
}

/**
 * Sets if the screen needs to be cleared
 */
void FPS3Gcm::SetNeedsScreenClear(UBOOL bNeedsClear)
{
	// set if it needs to be set or not
	appInterlockedExchange(&bNeedsScreenCleared, bNeedsClear ? 1 : 0);
}

/**
 * Adds a Gcm HUD marker
 *
 * @param Color The color to draw the event as
 * @param Text The text displayed with the event
 */
void appBeginDrawEvent(const FColor& Color,const TCHAR* Text)
{
#if !FINAL_RELEASE
	cellGcmSetPerfMonPushMarker(gCellGcmCurrentContext, TCHAR_TO_ANSI(Text));
#endif
}

/**
 * Ends the current Gcm HUD marker
 */
void appEndDrawEvent(void)
{
#if !FINAL_RELEASE
	cellGcmSetPerfMonPopMarker(gCellGcmCurrentContext);
#endif
}

/**
 * Platform specific function for setting the value of a counter that can be
 * viewed in PIX.
 */
void appSetCounterValue(const TCHAR* CounterName, FLOAT Value)
{
}

#endif // USE_PS3_RHI


/**
 * Allocate a block of GPU accessible memory, in local or host memory
 * @param Size Amount to allocate
 * @param Alignment Data alignment
 * @param bAllocateInSystemMemory TRUE to allocate in host memory, FALSE to allocate in local memory
 */
void* PS3GPUAllocate(INT Size, INT Alignment, UBOOL bAllocateInSystemMemory)
{
#if USE_PS3_RHI
	// @todo: Allocation type? Using CompressionTag as a placeholder for now, since nothing tracks host memory now
	return (bAllocateInSystemMemory ? GPS3Gcm->GetHostAllocator() : GPS3Gcm->GetLocalAllocator())->Malloc(Size, AT_ResourceArray, Alignment);
#else
	return NULL;
#endif
}

/**
 * Free a pointer from GPU accessible memory
 */
void PS3GPUFree(void* Ptr, UBOOL bFreeFromSystemMemory)
{
#if USE_PS3_RHI
	(bFreeFromSystemMemory ? GPS3Gcm->GetHostAllocator() : GPS3Gcm->GetLocalAllocator())->Free(Ptr);
#endif
}

#if USE_ALLOCATORFIXEDSIZEFREELIST
TAllocatorFixedSizeFreeList<sizeof(FBestFitAllocator::FMemoryChunk), 20> GMemoryChunkFixedSizePool(1800);
void* FBestFitAllocator::FMemoryChunk::operator new(size_t Size)
{
	checkSlow(Size == sizeof(FBestFitAllocator::FMemoryChunk));
	return GMemoryChunkFixedSizePool.Allocate();
}
void FBestFitAllocator::FMemoryChunk::operator delete(void *RawMemory)
{
	GMemoryChunkFixedSizePool.Free(RawMemory);
}
#endif
