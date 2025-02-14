/*=============================================================================
	PS3RHICommands.cpp: PS3 RHI commands implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3RHIPrivate.h"
#include "FMallocPS3.h"
#include "FFileManagerPS3.h"

// for flip helper thread to also check for sysutil messages
#include <sysutil/sysutil_common.h>

// For loading SPURS job PRX
#include <sys/paths.h>
#include <sys/prx.h>
#include <np/drm.h>
#include <np/lookup.h>
#include <cell/sysmodule.h>

#include <sdk_version.h>


// For memcpy16
extern "C" void *memcpy16(void *_Restrict, const void *_Restrict, size_t);

#if USE_PS3_RHI
#include "BatchedElements.h"
#include "ChartCreation.h"
#include "PS3PixelShaderPatching.h"
#include "PS3Landscape.h"

#include <cell/spurs/job_chain.h>
#include <cell/spurs/job_descriptor.h>
#include <cell/spurs/event_flag.h>


#if RHI_SUPPORTS_PREVERTEXSHADER_CULLING
#include "edge/geom/edgegeom_structs.h"
#endif

static const UINT PS3_TOTAL_VERTEX_CONSTANTS = 468;

#define CT_ASSERT(X) extern char CompileTimeAssert[ (X) ? 1 : -1 ]

struct FPS3Stream
{
	FPS3Stream()
	:	Stride(0)
	{
	}
	FVertexBufferRHIRef VertexBuffer;
	UINT	Stride;
	UBOOL	bUseInstanceIndex;
	UINT	NumVerticesPerInstance;
};

#define MAX_VERTEX_STREAMS 16

// These are flags for GPS3RHIDirty.
enum PS3RHIDirtyFlags
{
	PS3RHIDIRTY_VERTEXSTREAMS	= 1ull,															// Need to set the vertex shader non-instance inputs?
	PS3RHIDIRTY_INSTANCESTREAMS	= 2ull,															// Need to set the vertex shader instance inputs?
	PS3RHIDIRTY_ALLSTREAMS		= PS3RHIDIRTY_VERTEXSTREAMS | PS3RHIDIRTY_INSTANCESTREAMS,		// Need to set any of the vertex shader inputs?
    PS3RHIDIRTY_ALPHATEST		= 4ull,															// Need to update alpha test settings?
	PS3RHIDIRTY_PIXELSHADER		= 8ull,															// Need to set/update pixel shader program?
};

// Current state
FPS3RHIPixelShader *		GCurrentPixelShader = NULL;
FPS3RHIVertexShader *		GCurrentVertexShader = NULL;
INT							GVertexInputsUsed[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
DWORD						GEnabledTextures = 0;
UINT						GViewportX = 0;
UINT						GViewportY = 0;
UINT						GViewportWidth = 0;
UINT						GViewportHeight = 0;
FLOAT						GViewportMinZ = 0.0f;
FLOAT						GViewportMaxZ = 1.0f;
UINT						GScissorX = 0;
UINT						GScissorY = 0;
UINT						GScissorWidth = 0;
UINT						GScissorHeight = 0;

// Whether we should throttle the texture quad limits to 452, 226 or 113 instead of 468, 234 or 117.
// According to Sony, the latter (official) numbers are wrong and can randomly crash the GPU.
// NOTE: The official numbers should have been fixed in later SDKs.
UBOOL						GThrottleTextureQuads = FALSE;

FPS3RHITexture *			GCurrentTexture[16];
INT							GNumFatTexturesUsed = 0;
INT							GNumSemiFatTexturesUsed = 0;


// Pending states
FMatrix						GPendingViewProjectionMatrix;
FVector4                    GPendingViewTranslation;
FPixelShaderRHIRef			GPendingPixelShader;
FVertexDeclarationRHIRef	GPendingVertexDeclaration;
FPS3Stream					GPendingStreams[MAX_VERTEX_STREAMS];
#if RHI_SUPPORTS_PREVERTEXSHADER_CULLING
INT							GPendingPositionStreamIndex = INDEX_NONE;
#endif
UINT						GPendingNumInstances = 1;
UINT						GPendingInstanceIndex = 0;
UINT						GPendingBaseVertexIndex = 0;
UBOOL						GPendingAlphaTestEnable = FALSE;
WORD						GPendingAlphaTestFunc = CELL_GCM_ALWAYS;
BYTE						GPendingAlphaTestRef = 0;
UBOOL						GPendingBackfaceCullingEnable = FALSE;
WORD						GPendingBackfaceCullingFacing = 0;
QWORD						GPS3RHIDirty = 0ull;
UBOOL						GUseEdgeGeometry = TRUE;

// Embedded SPURS job programs. These are loaded from a PRX file
char* GPixelshaderPatchingJobStart = 0;
char* GPixelshaderPatchingJobSize = 0;
char* GWriteFenceJobStart = 0;
char* GWriteFenceJobSize = 0;
char* GLandscapeSPUJobStart = 0;
char* GLandscapeSPUJobSize = 0;

// Ring Buffer variables.
DWORD GNumDrawCallsPerAutoFence = 100;

// How many drawcalls until we periodically insert a fence.
static DWORD	GAutoFenceCountdown = GNumDrawCallsPerAutoFence;

// Whether we should run the PPU and GPU in lockstep (exec command: "PS3SingleStep")
UBOOL GSingleStepGPU = FALSE;

// Causes the GPU to crash with a crash dump (exec command: "PS3CrashRSX").
UBOOL GCrashRSX = FALSE;

UBOOL GEnableDrawCounter = FALSE;
UBOOL GTriggerDrawCounter = FALSE;
DWORD GCurrentDrawCounter = 0;
volatile DWORD* GCurrentDrawCounterLabel = NULL;


const FLOAT GBiasScale = FLOAT((1<<24)-1);
extern FLOAT GDepthBiasOffset;

/*=============================================================================
	Pixelshader patching
=============================================================================*/

template <typename InElementType, DWORD InNumElements, DWORD InNumPartitions>
struct FSpursData
{
	typedef InElementType ElementType;
	enum
	{
		NumElements		= InNumElements,
		NumPartitions	= InNumPartitions,
		PartitionSize	= InNumElements/InNumPartitions,
	};

	DWORD			BeginData( DWORD NumElementsToReserve );
	void			EndData( DWORD ElementIndex, DWORD NumElementsUsed );

	ElementType		Buffer[ NumElements ] __attribute__((__aligned__(128)));
	DWORD			Fences[ NumPartitions ];
	DWORD			CurrentIndex;
};

typedef CellSpursJob256		FSpursJobType;

class FSpursManager
{
public:

	/** Encapsulates the setup of a SPURS job. */
	class FSpursJobSetupContext
	{
	public:

		/** Initialization constructor. */
		FSpursJobSetupContext(const char* JobProgram,const char* JobProgramSize,UINT NumUserQWORDs,UINT NumScratchBytes,UINT NumOutputBytes);

		/** Destructor. */
		~FSpursJobSetupContext();

		/** Called to finish the setup. */
		void Finish();

		/**
		 * Adds to the list of DMA transfers into SPU local memory to complete before starting the job.
		 * @param Source - The base address to read the memory from.
		 * @param NumBytes - The number of bytes to transfer.
		 * @return The index of the transfer's first entry in the DMA list.
		 */
		UINT AddInputMemory(void* SourceBase,UINT NumBytes);

		/** @return A pointer to the job's user data. */
		QWORD* GetUserData() const;

		/**
		 * Retrieve the job under construction.
		 * @return The job being constructed
		 */
		FSpursJobType* GetJob() const { return Job; }

	private:

		/** Whether the job set up has finished. */
		UBOOL bFinished;

		/** The job which is being set up. */
		FSpursJobType* Job;

		/** The index of the job being set up. */
		DWORD JobIndex;

		/** The current DMA list pointer. */
		QWORD* NextDmaList;

		/** The last available DMA list pointer. */
		QWORD* LastDmaList;
	};

	enum
	{
		MaxJobs = 512,
		NumPartitions =8,
		PartitionSize = MaxJobs/NumPartitions,
	};

	void			Startup();
	void			Shutdown();

	DWORD			InsertFence();
	void			BlockOnFence( DWORD Fence );
	DWORD			GetCurrentFence();
	UBOOL			IsFencePending( DWORD Fence );
	void			KickCommandBuffer();

protected:
	DWORD			AllocateJobs( DWORD NumJobs );

	typedef FSpursData<FSpursJobType, MaxJobs, NumPartitions>	FJobBuffer;
	QWORD			CommandBuffer[MaxJobs + 1];
	FJobBuffer		Jobs;

	DWORD			PendingFence;
	DWORD			CurrentPPUFence;
	UBOOL			bIsAddingJob;

	volatile BYTE		CurrentSPUFence[128]	__attribute__((__aligned__(128)));
	CellSpursJobChain	JobChain				__attribute__((__aligned__(128)));
};

FSpursManager	GSpursManager;

void PS3KickSPUCommandBuffer()
{
	GSpursManager.KickCommandBuffer();
}

template <typename ElementType, DWORD NumElements, DWORD NumPartitions>
DWORD FSpursData<ElementType,NumElements,NumPartitions>::BeginData( DWORD NumElementsToReserve )
{
	// Do we fit into the current partition?
	DWORD PartitionOffset = CurrentIndex % PartitionSize;
	if ( (PartitionOffset + NumElementsToReserve) <= PartitionSize )
	{
		// Is this the first time we use this partition?
		if ( PartitionOffset == 0 )
		{
			// Make sure the SPUs are finished with it.
			DWORD Partition = CurrentIndex / PartitionSize;
			GSpursManager.BlockOnFence( Fences[Partition] );
		}
		return CurrentIndex;
	}

	// A partition should be big enough to fit any reservation.
	check( NumElementsToReserve <= PartitionSize );

	// Block until the elements in the next partition are done processing.
	DWORD Partition = (CurrentIndex + NumElementsToReserve) / PartitionSize;
	Partition = Partition % NumPartitions;
	GSpursManager.BlockOnFence( Fences[Partition] );

	// Did we wrap around?
	if ( Partition == 0 )
	{
		return 0;
	}
	else
	{
		return CurrentIndex;
	}
}

template <typename ElementType, DWORD NumElements, DWORD NumPartitions>
void FSpursData<ElementType,NumElements,NumPartitions>::EndData( DWORD ElementIndex, DWORD NumElementsUsed )
{
	check( ElementIndex == 0 || ElementIndex == CurrentIndex );
	check( ElementIndex + NumElementsUsed <= NumElements );
	DWORD PrevPartition = CurrentIndex / PartitionSize;
	CurrentIndex = (ElementIndex + NumElementsUsed) % NumElements;
	DWORD Partition = CurrentIndex / PartitionSize;

	// Did we actually cross into a new partition?
	if ( PrevPartition != Partition )
	{
		// Insert a fence.
		Fences[ PrevPartition ] = GSpursManager.InsertFence();
	}
}



void FSpursManager::Startup()
{
	CurrentPPUFence = 0;
	appMemzero((void*)&CurrentSPUFence, sizeof(CurrentSPUFence));

	// Initialize the command buffer as an empty ring buffer.
	for ( DWORD CommandIndex=0; CommandIndex < MaxJobs; ++CommandIndex )
	{
		CommandBuffer[CommandIndex] = CELL_SPURS_JOB_COMMAND_END;
	}
	CommandBuffer[MaxJobs] = CELL_SPURS_JOB_COMMAND_NEXT( &CommandBuffer[0] );

	// Initialize the jobs.
	appMemzero( Jobs.Buffer, MaxJobs*sizeof(FSpursJobType) );
	for ( DWORD JobIndex=0; JobIndex < MaxJobs; ++JobIndex )
	{
		Jobs.Buffer[JobIndex].header.eaBinary		= (uintptr_t) GPixelshaderPatchingJobStart;
		Jobs.Buffer[JobIndex].header.sizeBinary		= CELL_SPURS_GET_SIZE_BINARY( GPixelshaderPatchingJobSize );
		Jobs.Buffer[JobIndex].header.useInOutBuffer	= TRUE;
	}

	// Initialize the job chain.
	CellSpursJobChainAttribute JobChainAttributes;
	const BYTE WORKLOAD_PRIORITIES[8] = { 1,1,1,1,1,0,0,0 };
	const DWORD MAX_CONTENTION = SPU_NUM_SPURS;
	cellSpursJobChainAttributeInitialize(
					&JobChainAttributes,
					CommandBuffer,
					sizeof(FSpursJobType),
					16, WORKLOAD_PRIORITIES,
					MAX_CONTENTION,
					TRUE,0,1,FALSE,sizeof(FSpursJobType),0 );
	cellSpursJobChainAttributeSetName( &JobChainAttributes, "ShaderPatching" );
	cellSpursCreateJobChainWithAttribute( GSPURS, &JobChain, &JobChainAttributes );
}

void FSpursManager::Shutdown()
{
}

void FSpursManager::KickCommandBuffer()
{
	// NOTE: We should NOT try to add a new job if we're already inside BeginJob/EndJob!
	if ( PendingFence && !bIsAddingJob )
	{
		// Add a specific SPURS job for writing out the fence.
		// Note: BeginJob automatically sets it up for PendingFence.
		FSpursJobSetupContext(GWriteFenceJobStart, GWriteFenceJobSize, 0, 0, 1*1024);
	}
	INT ErrorCode = cellSpursRunJobChain( &JobChain );
	checkf( ErrorCode == CELL_OK, TEXT("SPU crash detected (0x%08x)!"), ErrorCode );
}

inline DWORD FSpursManager::InsertFence()
{
	// Only bump the fence number if we need to.
	if ( PendingFence == 0 )
	{
		// Make sure the new fence isn't 0.
		const DWORD NewFence1 = CurrentPPUFence + 1;
		const DWORD NewFence2 = CurrentPPUFence + 2;
		PendingFence = NewFence1 ? NewFence1 : NewFence2;
		CurrentPPUFence = PendingFence;
	}
	return PendingFence;
}

UBOOL FSpursManager::IsFencePending( DWORD Fence )
{
	SQWORD SPUFence = *((DWORD*) &CurrentSPUFence);
	SQWORD PPUFence = Fence;

	// Detect if either fence number has wrapped around
	UBOOL bWrapped = (Abs(PPUFence - SPUFence) > (1<<30)) ? TRUE : FALSE;

	UBOOL bIsPending = (SPUFence < PPUFence) || (bWrapped && SPUFence > PPUFence);
	return bIsPending;
}

void FSpursManager::BlockOnFence( DWORD Fence )
{
	if ( IsFencePending(Fence) )
	{
		SCOPE_CYCLE_COUNTER(STAT_BlockedBySPU);
		KickCommandBuffer();
		DOUBLE StartTime = appSeconds();
		do
		{
			appSleep( 0.0f );	// Yield for a tiny time
			// double-check to minimize chance of missing fence during breakpoint and such
			if ( !GPS3Gcm->ShouldIgnoreLongStalls() && IsFencePending(Fence))
			{
				DOUBLE CurrentTime = appSeconds();
				if( (CurrentTime - StartTime) > 5.0 )
				{
					warnf(TEXT("FSpursManager::BlockOnFence still waiting..."));
					StartTime = CurrentTime;
				}
			}
		} while ( IsFencePending(Fence) );
	}
}

inline DWORD FSpursManager::GetCurrentFence()
{
	return CurrentPPUFence;
}

inline DWORD FSpursManager::AllocateJobs( DWORD NumJobs )
{
	DWORD JobIndex = Jobs.BeginData( NumJobs );
	Jobs.EndData( JobIndex, NumJobs );
	return JobIndex;
}

FSpursManager::FSpursJobSetupContext::FSpursJobSetupContext(const char* JobProgram,const char* JobProgramSize,UINT NumUserQWORDs,UINT NumScratchBytes,UINT NumOutputBytes):
	bFinished(FALSE)
{
	// There's an additional QWORD of user data reserved for the job's fence.
	++NumUserQWORDs;

	// Prevent anyone else from adding jobs (e.g. KickCommandBuffer)
	GSpursManager.bIsAddingJob = TRUE;

	DWORD NumJobs = GSpursManager.PendingFence ? 2 : 1;

	// Are we going to cross into a new partition?
	DWORD PrevJobIndex		= GSpursManager.Jobs.CurrentIndex;
	DWORD PartitionOffset	= PrevJobIndex % PartitionSize;
	UBOOL bGotNewPartition	= (PartitionOffset + NumJobs >= PartitionSize);
	if ( bGotNewPartition )
	{
		NumJobs = 2;
	}

	// Allocate 1 or 2 jobs.
	JobIndex  = GSpursManager.AllocateJobs( NumJobs );
	Job = &GSpursManager.Jobs.Buffer[ JobIndex + NumJobs - 1 ];

	if ( bGotNewPartition )
	{
		// We should have a pending fence now.
		check( GSpursManager.PendingFence );

		// Initialize the new partition.
		const DWORD Partition		= ((JobIndex + NumJobs) / PartitionSize) % NumPartitions;
		const DWORD PartitionStart	= Partition * PartitionSize;
		const DWORD PartitionEnd	= PartitionStart + PartitionSize;
		GSpursManager.BlockOnFence( GSpursManager.Jobs.Fences[Partition] );	// Make sure the SPUs are done with this partition.
		for ( DWORD CommandIndex=PartitionStart; CommandIndex < PartitionEnd; ++CommandIndex )
		{
			GSpursManager.CommandBuffer[CommandIndex] = CELL_SPURS_JOB_COMMAND_END;
		}

		// Make sure previous memory writes has completed.
		appMemoryBarrier();

		// Did we wrap around with a gap?
		if ( JobIndex != PrevJobIndex )
		{
			// Pad out previous partition with NOPs.
			const DWORD PartitionStart = PrevJobIndex;
			const DWORD PartitionEnd   = Align( PrevJobIndex, PartitionSize );
			for ( DWORD CommandIndex=PartitionStart; CommandIndex < PartitionEnd; ++CommandIndex )
			{
				GSpursManager.CommandBuffer[CommandIndex] = CELL_SPURS_JOB_COMMAND_NOP;
			}
		}
	}

	// Do we need to SYNC before a fence update?
	if ( GSpursManager.PendingFence )
	{
		check( NumJobs == 2 );
		Job->header.eaBinary	= (uintptr_t) JobProgram;
		Job->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY( JobProgramSize );
		GSpursManager.CommandBuffer[ JobIndex++ ] = CELL_SPURS_JOB_COMMAND_LWSYNC;
	}
	else
	{
		check( NumJobs == 1 );
	}
	
	// Set the job's binary pointer and size.
	Job->header.eaBinary		= (uintptr_t) JobProgram;
	Job->header.sizeBinary	= CELL_SPURS_GET_SIZE_BINARY( JobProgramSize );

	// Initialize the in-out buffer to contain nothing.
	Job->header.sizeInOrInOut = 0;

	// Initialize the scratch and output buffers to the appropriate size.
	Job->header.sizeScratch = NumScratchBytes / 16;
	Job->header.sizeOut = NumOutputBytes;

	// Let the job write out the fence if we have one (non-zero).
	Job->workArea.userData[ARRAY_COUNT(Job->workArea.userData) - 1] = QWORD( GSpursManager.PendingFence ) | (QWORD(&GSpursManager.CurrentSPUFence) << 32);
	GSpursManager.PendingFence = 0;

	// Allow everyone else to add jobs (e.g. KickCommandBuffer)
	GSpursManager.bIsAddingJob = FALSE;

	// Compute the DMA list pointers.
	NextDmaList = &Job->workArea.dmaList[0];
	LastDmaList = &Job->workArea.dmaList[ARRAY_COUNT(Job->workArea.dmaList) - NumUserQWORDs - 1];
}

FSpursManager::FSpursJobSetupContext::~FSpursJobSetupContext()
{
	// If it's not finished yet, finish setting up the job.
	if(!bFinished)
	{
		Finish();
	}
}

void FSpursManager::FSpursJobSetupContext::Finish()
{
	check(!bFinished);
	bFinished = TRUE;

	// Compute the DMA list size.
	Job->header.sizeDmaList = (NextDmaList - &Job->workArea.dmaList[0]) * sizeof(QWORD);	

	// Ensure that the job's memory is visible to the SPU before a reference to it is written to the SPURS command buffer.
	appMemoryBarrier();

	GSpursManager.CommandBuffer[ JobIndex ] = CELL_SPURS_JOB_COMMAND_JOB( Job );
	GSpursManager.KickCommandBuffer();
}

UINT FSpursManager::FSpursJobSetupContext::AddInputMemory(void* SourceBase,UINT NumBytes)
{
	check(!bFinished);
	check(Align(SourceBase,16) == SourceBase);
	check(Align(NumBytes,16) == NumBytes);

	const UINT DMAIndex = (UINT)(NextDmaList - &Job->workArea.dmaList[0]);

	// Split the memory into 16KB transfers.
	static const UINT MaxTransferBytes = 16*1024;
	UINT TransferredBytes = 0;
	while(TransferredBytes < NumBytes)
	{
		// Add each 16KB or less transfer to the job's DMA list.
		const WORD TransferBytes = (WORD)Min(NumBytes - TransferredBytes,MaxTransferBytes);
		check(NextDmaList <= LastDmaList);
		verify(cellSpursJobGetInputList(NextDmaList++,TransferBytes,UINT(SourceBase) + TransferredBytes) == CELL_OK);
		TransferredBytes += TransferBytes;
	};

	// Add space to the in-out buffer for the data.
	Job->header.sizeInOrInOut += NumBytes;

	return DMAIndex;
}

QWORD* FSpursManager::FSpursJobSetupContext::GetUserData() const
{
	check(!bFinished);

	// The user data begins after the DMA list.
	return LastDmaList + 1;
}


// Pixelshader constants (ring buffer)
typedef FSpursData<FVector4, 2048, 4>	FPixelShaderConstantBuffer;
FPixelShaderConstantBuffer		GPixelShaderConstantBuffer;
FVector4						GPixelShaderConstants[256];



/** Initialize SPURS jobs for pixelshader patching and other things */
void PS3LoadSPUJobs()
{
	// get the job info from the linked .obj files (see top of UE3BuildPS3.cs)
	extern char _binary_PixelshaderPatching_start[], _binary_PixelshaderPatching_size[];
	GPixelshaderPatchingJobStart = _binary_PixelshaderPatching_start;
	GPixelshaderPatchingJobSize = _binary_PixelshaderPatching_size;

	extern char _binary_WriteFence_start[], _binary_WriteFence_size[];
	GWriteFenceJobStart = _binary_WriteFence_start;
	GWriteFenceJobSize = _binary_WriteFence_size;

	// get the job info from the linked .obj files (see top of UE3BuildPS3.cs)
	extern char _binary_LandscapeSPU_start[], _binary_LandscapeSPU_size[];
	GLandscapeSPUJobStart = _binary_LandscapeSPU_start;
	GLandscapeSPUJobSize = _binary_LandscapeSPU_size;

	GSpursManager.Startup();
}


/*=============================================================================
	Helper functions
=============================================================================*/

// Calculates number of elements (vertices or indices) based on primitive type and number of primitives.
inline DWORD CalcNumElements( UINT PrimitiveType, DWORD NumPrimitives )
{
	return NumPrimitives * UnrealPrimitiveTypeToElementCount[PrimitiveType][0] + UnrealPrimitiveTypeToElementCount[PrimitiveType][1];
}

/*=============================================================================
	State management
=============================================================================*/

inline void ClearUnusedVertexInputs()
{
	// Clear unused attributes.
	for ( INT AttributeIndex=0; AttributeIndex < 16; ++AttributeIndex )
	{
		if ( GVertexInputsUsed[AttributeIndex] == 1 )	// "Old" attribute?
		{
			cellGcmSetVertexDataArray(AttributeIndex,0,0,0,CELL_GCM_VERTEX_F,MR_Local,0);
		}
		GVertexInputsUsed[AttributeIndex] = Max( GVertexInputsUsed[AttributeIndex] - 1, 0 );
	}
}

/** Changes the instance index and base vertex index to use from the source vertex streams. */
inline void PS3SetVertexOffsets(UINT BaseVertexIndex,UINT InstanceIndex)
{
	// If the new base vertex index is different from the last set state, trigger a deferred state update.
	if(GPendingBaseVertexIndex != BaseVertexIndex)
	{
		GPendingBaseVertexIndex = BaseVertexIndex;
		GPS3RHIDirty |= PS3RHIDIRTY_VERTEXSTREAMS;
	}

	// If the new instance index is different from the last set state, trigger a deferred state update.
	if(GPendingInstanceIndex != InstanceIndex)
	{
		GPendingInstanceIndex = InstanceIndex;
		GPS3RHIDirty |= PS3RHIDIRTY_INSTANCESTREAMS;
	}
}

static const DWORD GFrequencyFlags[2][16] =
{
	{
		CELL_GCM_FREQUENCY_MODULO << 0,
		CELL_GCM_FREQUENCY_MODULO << 1,
		CELL_GCM_FREQUENCY_MODULO << 2,
		CELL_GCM_FREQUENCY_MODULO << 3,
		CELL_GCM_FREQUENCY_MODULO << 4,
		CELL_GCM_FREQUENCY_MODULO << 5,
		CELL_GCM_FREQUENCY_MODULO << 6,
		CELL_GCM_FREQUENCY_MODULO << 7,
		CELL_GCM_FREQUENCY_MODULO << 8,
		CELL_GCM_FREQUENCY_MODULO << 9,
		CELL_GCM_FREQUENCY_MODULO << 10,
		CELL_GCM_FREQUENCY_MODULO << 11,
		CELL_GCM_FREQUENCY_MODULO << 12,
		CELL_GCM_FREQUENCY_MODULO << 13,
		CELL_GCM_FREQUENCY_MODULO << 14,
		CELL_GCM_FREQUENCY_MODULO << 15,
	},
	{
		CELL_GCM_FREQUENCY_DIVIDE << 0,
		CELL_GCM_FREQUENCY_DIVIDE << 1,
		CELL_GCM_FREQUENCY_DIVIDE << 2,
		CELL_GCM_FREQUENCY_DIVIDE << 3,
		CELL_GCM_FREQUENCY_DIVIDE << 4,
		CELL_GCM_FREQUENCY_DIVIDE << 5,
		CELL_GCM_FREQUENCY_DIVIDE << 6,
		CELL_GCM_FREQUENCY_DIVIDE << 7,
		CELL_GCM_FREQUENCY_DIVIDE << 8,
		CELL_GCM_FREQUENCY_DIVIDE << 9,
		CELL_GCM_FREQUENCY_DIVIDE << 10,
		CELL_GCM_FREQUENCY_DIVIDE << 11,
		CELL_GCM_FREQUENCY_DIVIDE << 12,
		CELL_GCM_FREQUENCY_DIVIDE << 13,
		CELL_GCM_FREQUENCY_DIVIDE << 14,
		CELL_GCM_FREQUENCY_DIVIDE << 15,
	}
};

/** Set the vertex stream data. */
inline void PS3UpdateStreams()
{
	const FVertexDeclarationElementList& VertexElements = GPendingVertexDeclaration->VertexElements;
	const UINT NumElements = VertexElements.Num();

	// Determine what types of streams need to be updated.
	const UBOOL bUpdateVertexStreams = (GPS3RHIDirty & PS3RHIDIRTY_VERTEXSTREAMS);
	const UBOOL bUpdateInstanceStreams = (GPS3RHIDirty & PS3RHIDIRTY_INSTANCESTREAMS);

	DWORD FrequencyDividerOperations = 0;

	for ( UINT ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex )
	{
		const FVertexElement& Element = VertexElements(ElementIndex);
		const DWORD AttributeIndex = UnrealUsageToSemantic[Element.Usage] + Element.UsageIndex;

		// Setup the vertex attribute
		if ( GCurrentVertexShader->UsedAttributes[AttributeIndex] )
		{
			FPS3Stream& Stream = GPendingStreams[ Element.StreamIndex ];
			if(	(!Stream.bUseInstanceIndex && bUpdateVertexStreams) ||
				(Stream.bUseInstanceIndex && bUpdateInstanceStreams))
			{
				const UINT VertexOffset = Stream.bUseInstanceIndex ?
					GPendingBaseVertexIndex + GPendingInstanceIndex :
					GPendingBaseVertexIndex;
				cellGcmSetVertexDataArray(
					AttributeIndex,						// Attr index
					Stream.NumVerticesPerInstance,		// Frequency
					Stream.Stride,						// Stride
					UnrealTypeToCount[Element.Type],	// Num "coordinates" per vertex
					UnrealTypeToType[Element.Type],		// Data type of "coordinate"
					Stream.VertexBuffer->MemoryRegion,	// Location of memory
					Stream.VertexBuffer->MemoryOffset	// Offset of verts into that memory
						+ VertexOffset*Stream.Stride
						+ Element.Offset
					);
			}
			
			// Mark this attribute as in use.
			GVertexInputsUsed[AttributeIndex] = 2;

			// Set the bits for this attribute's frequency operation.
//			FrequencyDividerOperations |= (Stream.bUseInstanceIndex ? CELL_GCM_FREQUENCY_DIVIDE : CELL_GCM_FREQUENCY_MODULO) << AttributeIndex;
			FrequencyDividerOperations |= GFrequencyFlags[Stream.bUseInstanceIndex][AttributeIndex];
		}
	}

	// Set the combined frequency operations for all attributes.
	cellGcmSetFrequencyDividerOperation(FrequencyDividerOperations);

	ClearUnusedVertexInputs();
	GPS3RHIDirty &= ~PS3RHIDIRTY_ALLSTREAMS;
}

/** Update the alpha test settings. */
inline void PS3UpdateAlphaTest()
{
	DWORD AlphaRef;

	// 16-bit floating point render target uses 0..65535 for alpha ref :( and so we can't put it into the command list without a patch
	if ( GPS3Gcm->IsCurrentRenderTargetFloatingPoint() )
	{
		FFloat16 Fp16( FLOAT(GPendingAlphaTestRef) / 255.0f );
		AlphaRef = Fp16.Encoded;
	}
	else
	{
		AlphaRef = GPendingAlphaTestRef;
	}
	cellGcmSetAlphaFunc( GPendingAlphaTestFunc, AlphaRef );
	cellGcmSetAlphaTestEnable( GPendingAlphaTestEnable );
	GPS3RHIDirty &= ~PS3RHIDIRTY_ALPHATEST;	
}

/**
 * Writes halting commands to the command buffer to reserve space for sequenced commands at the current location.
 * The GPU won't execute past the current location in the command buffer until the reserved space has been filled in
 * with the desired commands.
 * @param NumReserveBytes - The number of bytes to reserve.
 * @param bAlign - TRUE if the reserved space should be aligned to a 16 byte boundary.
 * @return The address of the reserved space.
 */
static UINT* ReserveCommandBufferSpace(UINT NumReserveBytes,UBOOL bAlign = FALSE)
{
	const UINT NumReserveDWORDs = NumReserveBytes / sizeof(DWORD);

	if(bAlign)
	{
		// Write NOPs until the start of the reserved space is 16-byte aligned.
		while(UINT(gCellGcmCurrentContext->current) & 0xf)
		{
			cellGcmSetNopCommand(gCellGcmCurrentContext,1);
		};
	}

	// Make sure we have room for the commands contiguously at the current pointer.
	if ( (gCellGcmCurrentContext->current + NumReserveDWORDs) > gCellGcmCurrentContext->end )
	{
#if CELL_SDK_VERSION >= 0x250000 && defined(__SNC__)
		// see docs for why cellGcmCallbackForSnc is needed
		DWORD ErrorCode = cellGcmCallbackForSnc(gCellGcmCurrentContext, NumReserveDWORDs);
#else
		DWORD ErrorCode = (*gCellGcmCurrentContext->callback)(gCellGcmCurrentContext, NumReserveDWORDs);
#endif
	
		check( ErrorCode == CELL_OK );
	}

	// Save the address of the start of the buffer.
	UINT* const CommandBufferHole = gCellGcmCurrentContext->current;
	// Insert a jump-to-self at the beginning of the hole...
	UINT* const CommandBufferHoleEnd = CommandBufferHole + NumReserveDWORDs;
	UINT JumpOffset;
	cellGcmAddressToOffset( gCellGcmCurrentContext->current, &JumpOffset );
	cellGcmSetJumpCommandUnsafeInline(gCellGcmCurrentContext, JumpOffset);
	// ...and at every 128-byte aligned boundary thereafter, until the end of the hole.
	UINT* NextJumpToSelf = (UINT*)( ((uintptr_t)gCellGcmCurrentContext->current + 0x80) & ~0x7F );
	while(NextJumpToSelf < CommandBufferHoleEnd)
	{
		__dcbz(NextJumpToSelf); // prefetching here improves the loop's performance significantly
		gCellGcmCurrentContext->current = NextJumpToSelf;
		cellGcmAddressToOffset( gCellGcmCurrentContext->current, &JumpOffset );
		cellGcmSetJumpCommandUnsafeInline(gCellGcmCurrentContext, JumpOffset);
		NextJumpToSelf = (UINT*)( ((uintptr_t)gCellGcmCurrentContext->current + 0x80) & ~0x7F );
	}
	gCellGcmCurrentContext->current = CommandBufferHoleEnd;

	// Verify that the jump commands fill exactly the reserve size requested.
	const SIZE_T CommandBufferHoleSize = gCellGcmCurrentContext->current - CommandBufferHole;
	checkSlow( CommandBufferHoleSize == NumReserveDWORDs );

	return CommandBufferHole;
}

/** Update the pixel shader, either by complete upload or just an i-cache flush */
UBOOL bGUseSPUPatching = TRUE;
inline void PS3UpdatePixelShader()
{
	GCurrentPixelShader						= GPendingPixelShader;
	const FPS3RHIPixelShader *PixelShader	= GCurrentPixelShader;
	const FPixelShaderInfo& Info			= *PixelShader->Info;

	// Get an offset to where we can place our patched microcode.
	BYTE* MicrocodeCopy	= (BYTE*)GPS3Gcm->GetPatchedPixelShadersRingBuffer()->Malloc(Info.MicrocodeSize);

	if ( bGUseSPUPatching )
	{
		// Setup a SPURS job for patching the shader.
		static const UINT NumUserQWORDs = (sizeof(FPixelShaderPatchingParameters) + sizeof(QWORD) - 1) / sizeof(QWORD);
		static const UINT NumScratchBytes = 0;
		static const UINT NumOutputBytes = 1*1024;
		FSpursManager::FSpursJobSetupContext SpursJobSetup( GPixelshaderPatchingJobStart, GPixelshaderPatchingJobSize, NumUserQWORDs, NumScratchBytes, NumOutputBytes );
		FPixelShaderPatchingParameters& JobParameters = *(FPixelShaderPatchingParameters*)SpursJobSetup.GetUserData();
		JobParameters.BaseParameters.TaskId = 0;
		JobParameters.MicrocodeOut = (DWORD)MicrocodeCopy;

		// Reserve space at the current command buffer location to block the GPU from executing subsequent commands until the patching program has finished.
		JobParameters.CommandBufferSize = sizeof(DWORD);
		JobParameters.CommandBufferHole = ReserveCommandBufferSpace(JobParameters.CommandBufferSize);

		const WORD BufferSize = Align(PixelShader->BufferSize, 16);
		checkSlow(BufferSize > 0);
		SpursJobSetup.AddInputMemory(PixelShader->Buffer,BufferSize);

		// Copy the constant values for the SPU patching (using memcpy16 since the data is formed nicely).
		DWORD PixelShaderConstantStart = GPixelShaderConstantBuffer.BeginData( Info.NumParameters );
		const UINT ConstantSize = Info.NumParameters * sizeof(FVector4);
		memcpy16( &GPixelShaderConstantBuffer.Buffer[PixelShaderConstantStart], GPixelShaderConstants, ConstantSize );
		GPixelShaderConstantBuffer.EndData( PixelShaderConstantStart, Info.NumParameters );
		SpursJobSetup.AddInputMemory(&GPixelShaderConstantBuffer.Buffer[PixelShaderConstantStart],ConstantSize);

		SpursJobSetup.Finish();
	}
	else
	{
		const BYTE* NumOccurrances	= PixelShader->NumOccurrances;
		const WORD* Offsets			= PixelShader->Offsets;
		DWORD* PixelShaderConstants	= (DWORD*)GPixelShaderConstants;
		appMemcpy( MicrocodeCopy, PixelShader->Microcode, Info.MicrocodeSize );

		// Patch the microcode on the PPU.
		for ( DWORD ParameterIndex=0; ParameterIndex < Info.NumParameters; ++ParameterIndex )
		{
			DWORD Value[4];
			Value[0] = ((PixelShaderConstants[0] & 0xffff0000) >> 16) | ((PixelShaderConstants[0] & 0x0000ffff) << 16);	// Vector.X
			Value[1] = ((PixelShaderConstants[1] & 0xffff0000) >> 16) | ((PixelShaderConstants[1] & 0x0000ffff) << 16);	// Vector.Y
			Value[2] = ((PixelShaderConstants[2] & 0xffff0000) >> 16) | ((PixelShaderConstants[2] & 0x0000ffff) << 16);	// Vector.Z
			Value[3] = ((PixelShaderConstants[3] & 0xffff0000) >> 16) | ((PixelShaderConstants[3] & 0x0000ffff) << 16);	// Vector.W
			DWORD NumOffsets = *NumOccurrances++;
			while ( NumOffsets-- )
			{
				DWORD* Patch = (DWORD*) (MicrocodeCopy + *Offsets++);
				Patch[0] = Value[0];
				Patch[1] = Value[1];
				Patch[2] = Value[2];
				Patch[3] = Value[3];
			}
			PixelShaderConstants += 4;
		}
	}

	// Throttle the texture quad limits to 452, 226 or 113 instead of 468, 234 or 117.
	// According to Sony, the latter (official) numbers are wrong and can randomly crash the GPU.
	// NOTE: The official numbers should have been fixed in later SDKs.
	BYTE NumTempRegisters		= Info.RegisterCount;
	if ( GThrottleTextureQuads )
	{
		BYTE MinTempRegisters	= 4;
		MinTempRegisters		= GNumSemiFatTexturesUsed ? 7 : MinTempRegisters;
		MinTempRegisters		= GNumFatTexturesUsed ? 14 : MinTempRegisters;
		NumTempRegisters		= Max<BYTE>(NumTempRegisters, MinTempRegisters);
	}

	// Tell RSX to use the copy.
	CellCgbFragmentProgramConfiguration Config;
	Config.registerCount		= NumTempRegisters;
	Config.attributeInputMask	= Info.AttributeInputMask;
	Config.fragmentControl		= Info.FragmentControl;
	Config.offset				= GPS3Gcm->GetPatchedPixelShadersRingBuffer()->PointerToOffset(MicrocodeCopy);
	Config.texCoords2D			= Info.TexCoords2D;
	Config.texCoordsCentroid	= Info.TexCoordsCentroid;
	Config.texCoordsInputMask	= Info.TexCoordsInputMask;
	cellGcmSetFragmentProgramLoad( &Config );

	// Refresh the texture cache, since we're re-using the same memory for pixelshaders,
	// and the texture cache dirty-flags are tied to the address of the pixelshader.
	cellGcmSetInvalidateTextureCache( CELL_GCM_INVALIDATE_TEXTURE );

	// Check that all textures that the shader uses have been set.
	checkSlow( (GEnabledTextures & Info.TextureMask) == Info.TextureMask );

	// Disable unused texture units
	DWORD TexturesToDisable = GEnabledTextures & ~Info.TextureMask;
	while ( TexturesToDisable )
	{
		DWORD TextureUnit = 31 - appCountLeadingZeros( TexturesToDisable );
		DWORD TextureBitFlag = GBitFlag[TextureUnit];
		cellGcmSetTextureControl( TextureUnit, CELL_GCM_FALSE, (0 << 8) | 0, (12 << 8) | 0, CELL_GCM_TEXTURE_MAX_ANISO_1 );

		DWORD OldFormat = GCurrentTexture[ TextureUnit ]->GetFormat() & ~(CELL_GCM_TEXTURE_SZ|CELL_GCM_TEXTURE_LN|CELL_GCM_TEXTURE_NR|CELL_GCM_TEXTURE_UN);
		GNumFatTexturesUsed -= (OldFormat == CELL_GCM_TEXTURE_W32_Z32_Y32_X32_FLOAT || OldFormat == CELL_GCM_SURFACE_F_X32) ? 1 : 0;
		GNumSemiFatTexturesUsed -= (OldFormat == CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT) ? 1 : 0;
		GCurrentTexture[ TextureUnit ] = NULL;
		GEnabledTextures &= ~TextureBitFlag;
		TexturesToDisable &= ~TextureBitFlag;
	}

	GPS3RHIDirty &= ~PS3RHIDIRTY_PIXELSHADER;
}

static INT CurrentLandscapeHeightVB = -1;
static FPS3RHIVertexBuffer* LandscapeHeightVertexBuffers[LANDSCAPE_NUM_BUFFERS];
static DWORD GNextLandscapeSpuRsxFence = 1;

FVertexBufferRHIRef PS3GenerateLandscapeHeightStream( const FVector4& LodDistancesValues, INT CurrentLOD, INT SubsectionSizeQuads, INT NumSubsections, INT SubsectionX, INT SubsectionY, void* PS3Data )
{
	if( CurrentLandscapeHeightVB == -1 )
	{
		for( INT i=0;i<LANDSCAPE_NUM_BUFFERS;i++ )
		{
			LandscapeHeightVertexBuffers[i] = new FPS3RHIVertexBuffer( 256*256 * 4, RUF_Static );
			LandscapeHeightVertexBuffers[i]->AddRef();
		}
		CurrentLandscapeHeightVB = 0;
		*((volatile DWORD *)cellGcmGetLabelAddress(LABEL_LANDSCAPE_SPU_RSX)) = 0;
	}

	static const UINT NumUserQWORDs = (sizeof(FLandscapeJobParameters) + sizeof(QWORD) - 1) / sizeof(QWORD);
	FSpursManager::FSpursJobSetupContext SpursJobSetup( GLandscapeSPUJobStart, GLandscapeSPUJobSize, NumUserQWORDs, 0, 0 );

	// Fill in the job data
	FLandscapeJobParameters& JobParameters = *(FLandscapeJobParameters*)SpursJobSetup.GetUserData();
	JobParameters.CurrentLOD = CurrentLOD;
	JobParameters.SubsectionSizeVerts = SubsectionSizeQuads+1;
	JobParameters.LodDistancesValues[0] = LodDistancesValues.X;
	JobParameters.LodDistancesValues[1] = LodDistancesValues.Y;
	JobParameters.LodDistancesValues[2] = LodDistancesValues.Z;
	JobParameters.LodDistancesValues[3] = 1.f / (LodDistancesValues.W - LodDistancesValues.Z);
	JobParameters.MySpuRsxFence = GNextLandscapeSpuRsxFence;
	JobParameters.SpuRsxFencePtr = (DWORD)cellGcmGetLabelAddress(LABEL_LANDSCAPE_SPU_RSX);
	check( (JobParameters.SpuRsxFencePtr & 0xF) == 0 );

	struct FLodInfo
	{
		DWORD Offset;
		DWORD Stride;
	};

	FLodInfo* LodInfo = (FLodInfo*)PS3Data;
	
	// Find subsection
	JobParameters.HeightDataCurrentLodPitch = LodInfo[CurrentLOD].Stride;
	JobParameters.HeightDataCurrentLodBase = (DWORD)PS3Data + LodInfo[CurrentLOD].Offset + (SubsectionX + SubsectionY * NumSubsections) * ((SubsectionSizeQuads+1) >> CurrentLOD) * LodInfo[CurrentLOD].Stride;

	if( ((SubsectionSizeQuads+1) >> CurrentLOD) <= 2 )
	{
		JobParameters.HeightDataNextLodPitch = 0;
		JobParameters.HeightDataNextLodBase = 0;
	}
	else
	{
		JobParameters.HeightDataNextLodPitch = LodInfo[CurrentLOD+1].Stride;
		JobParameters.HeightDataNextLodBase = (DWORD)PS3Data + LodInfo[CurrentLOD+1].Offset + (SubsectionX + SubsectionY * NumSubsections) * ((SubsectionSizeQuads+1) >> (CurrentLOD+1)) * LodInfo[CurrentLOD+1].Stride;
	}

	FPS3RHIVertexBuffer* VertexBuffer = LandscapeHeightVertexBuffers[CurrentLandscapeHeightVB];
	JobParameters.OutputBase = (DWORD)VertexBuffer->GetBaseAddress();

	// Create command buffer hole that will jump-to-self, blocking RSX.
	// We will NOP it out from SPU code when we've finished generating the vertices.
	JobParameters.CommandBufferHole = (DWORD)ReserveCommandBufferSpace(LANDSCAPE_SPU_CMD_BUFFER_HOLE_SIZE, TRUE);

	// Ensure RSX pipeline is flushed
	cellGcmSetWriteBackEndLabel(LABEL_LANDSCAPE_RSX_FLUSH,  0);

	// Make sure we invalidate vertex cache before rendering as there could be stale data from a previous component
	cellGcmSetInvalidateVertexCache();

	{
		SCOPE_CYCLE_COUNTER(STAT_LandscapeSPU);
		SpursJobSetup.Finish(); 

		// Uncomment this to get accurate Landscape SPU stats
		// GSpursManager.BlockOnFence(GSpursManager.InsertFence());
	}

	return VertexBuffer;
}

void PS3FinishedWithLandscapeHeightStream()
{
	// When RSX is finished backend processing, we can release any SPU job waiting to write data to the same vertex buffer.
	// Update the label, wrapping around if necessary.
	cellGcmSetWriteBackEndLabel(LABEL_LANDSCAPE_SPU_RSX,  GNextLandscapeSpuRsxFence++ );

	DWORD CurrentLandscapeSpuRsxFence = *((volatile DWORD *)cellGcmGetLabelAddress(LABEL_LANDSCAPE_SPU_RSX));
	if( GNextLandscapeSpuRsxFence - CurrentLandscapeSpuRsxFence > (LANDSCAPE_NUM_BUFFERS/2) )
	{
		// Kick RSX if we've used more than half our buffer space
		cellGcmFlush();
	}

	// Swap vertex buffer doublebuffers
	CurrentLandscapeHeightVB++;
	if( CurrentLandscapeHeightVB >= LANDSCAPE_NUM_BUFFERS )
	{
		CurrentLandscapeHeightVB = 0;
	}
}

inline void PS3UpdateDirtyStates()
{
	if ( GPS3RHIDirty )
	{
		if ( GPS3RHIDirty & PS3RHIDIRTY_ALLSTREAMS )
		{ 
			PS3UpdateStreams();
		}
		if ( GPS3RHIDirty&PS3RHIDIRTY_ALPHATEST )
		{ 
			PS3UpdateAlphaTest();
		}
		if ( GPS3RHIDirty&PS3RHIDIRTY_PIXELSHADER )
		{ 
			PS3UpdatePixelShader();
		}
	}
}


/** Update vertex declaration and all deferred states for a DrawUP call. */
inline void PS3UpdateUP(DWORD VertexData, DWORD VertexDataStride)
{
	for (INT ElementIndex = 0; ElementIndex < GPendingVertexDeclaration->VertexElements.Num(); ElementIndex++)
	{
		FVertexElement& Element = GPendingVertexDeclaration->VertexElements(ElementIndex);
		DWORD AttributeIndex = UnrealUsageToSemantic[Element.Usage] + Element.UsageIndex;

		if ( GCurrentVertexShader->UsedAttributes[AttributeIndex] )
		{
			GVertexInputsUsed[AttributeIndex] = 2;		// Mark as "new"

			// set the input stream
			cellGcmSetVertexDataArray(
				AttributeIndex,							// Attr index
				0,										// Frequency
				VertexDataStride,						// Stride
				UnrealTypeToCount[Element.Type],		// Num "coordinates" per vertex
				UnrealTypeToType[Element.Type],			// Data type of "coordinate"
				CELL_GCM_LOCATION_MAIN,					// Location of memory
				VertexData + Element.Offset);			// Offset of verts into that memory
		}
	}

	cellGcmSetFrequencyDividerOperation(0xffff);		// Set all inputs to CELL_GCM_FREQUENCY_MODULO

	ClearUnusedVertexInputs();

	GPS3RHIDirty &= ~PS3RHIDIRTY_ALLSTREAMS;
	PS3UpdateDirtyStates();
}

/** Resets the counter for when to periodically insert new fences. */
void PS3ResetAutoFence()
{
	GAutoFenceCountdown = GNumDrawCallsPerAutoFence;
}

/** Periodically inserts a new fence, as determined by GNumDrawCallsPerAutoFence */
void PS3UpdateAutoFence()
{
	GCurrentDrawCounter++;
	if ( GEnableDrawCounter && GTriggerDrawCounter )
	{
		// NOTE: Adding this between each drawcall seems to prevent GPU hang!
 		cellGcmSetWriteBackEndLabel( LABEL_DRAWCALL, GCurrentDrawCounter );
//		GTriggerDrawCounter = FALSE;
	}

 	if ( GCrashRSX )
 	{
 		// Cause the RSX to crash.
 		cellGcmSetJumpCommand( 0xffffffff );
		GCrashRSX = FALSE;
 	}

	if ( GSingleStepGPU )
	{
		DWORD Fence = GPS3Gcm->InsertFence();
		GPS3Gcm->BlockOnFence(Fence);
	}

	if ( GAutoFenceCountdown-- == 0 )
	{
		// NOTE: InsertFence() will reset the counter.
		GPS3Gcm->InsertFence( FALSE );
	}
}


/*=============================================================================
	Helper functions.
=============================================================================*/


void FillRenderTargetWithTexture(FTexture* Texture, FColor Color, FLOAT Depth)
{
	// get render target size
	DWORD SizeX, SizeY;
	GPS3Gcm->GetCurrentRenderTargetSize(SizeX, SizeY);

	FBatchedElements BatchedElements;
	INT V00 = BatchedElements.AddVertex(FVector4(-1.0f, -1.0f, Depth, 1.0f), FVector2D(0.0f, 0.0f), Color, FHitProxyId());
	INT V10 = BatchedElements.AddVertex(FVector4( 1.0f, -1.0f, Depth, 1.0f), FVector2D(1.0f, 0.0f), Color, FHitProxyId());
	INT V01 = BatchedElements.AddVertex(FVector4(-1.0f,  1.0f, Depth, 1.0f), FVector2D(0.0f, 1.0f), Color, FHitProxyId());
	INT V11 = BatchedElements.AddVertex(FVector4( 1.0f,  1.0f, Depth, 1.0f), FVector2D(1.0f, 1.0f), Color, FHitProxyId());

	// Draw a quad using the generated vertices.
	BatchedElements.AddTriangle(V00, V10, V11, Texture, BLEND_Opaque);
	BatchedElements.AddTriangle(V00, V11, V01, Texture, BLEND_Opaque);
	BatchedElements.Draw(FMatrix::Identity, SizeX, SizeY, FALSE);
}

/** Vertex declaration for just one FVector4 position. */
class FVector4VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,0,VET_Float4,VEU_Position,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
FGlobalBoundShaderState GOneColorBoundShaderState;
TGlobalResource<FVector4VertexDeclaration> GVector4VertexDeclaration;

void FillRenderTargetWithColor( const FLinearColor& Color, FLOAT Depth )
{
	// Piggy-back on FFilterVertex, even though we're not using the UVs...
	FVector4 Vertices[4];
	Vertices[0].Set( -1.0f, -1.0f, Depth, 1.0f );
	Vertices[1].Set( -1.0f,  1.0f, Depth, 1.0f );
	Vertices[2].Set(  1.0f,  1.0f, Depth, 1.0f );
	Vertices[3].Set(  1.0f, -1.0f, Depth, 1.0f );

	FOneColorVertexShader* VertexShader = (FOneColorVertexShader*) GetGlobalShaderMap()->GetShader(&FOneColorVertexShader::StaticType);
	FOneColorPixelShader* PixelShader = (FOneColorPixelShader*) GetGlobalShaderMap()->GetShader(&FOneColorPixelShader::StaticType);

	if ( VertexShader && PixelShader )
	{
		SetGlobalBoundShaderState( GOneColorBoundShaderState, GVector4VertexDeclaration.VertexDeclarationRHI, VertexShader, PixelShader, sizeof(FVector4));
		SetPixelShaderValue(PixelShader->GetPixelShader(),PixelShader->ColorParameter,Color);
	}
	else
	{
		appErrorf( TEXT("Trying to clear a floating point color buffer before shaders are loaded!") );
	}

	RHISetRasterizerState( TStaticRasterizerState<FM_Solid,CM_None>::GetRHI() );
	RHISetBlendState( TStaticBlendState<>::GetRHI() );

	RHIDrawPrimitiveUP( PT_QuadList, 1, Vertices, sizeof(Vertices[0]) );
}

/*=============================================================================
	RHI functions.
=============================================================================*/

void RHIClear(UBOOL bClearColor,const FLinearColor& Color,UBOOL bClearDepth,FLOAT Depth,UBOOL bClearStencil,DWORD Stencil)
{
	SCOPE_CYCLE_COUNTER(STAT_Clear);

	DWORD Flags = 0;

	if (bClearDepth || bClearStencil)
	{
		// set the depth/stencil clear value
		cellGcmSetClearDepthStencil(((DWORD)(Depth * 0xFFFFFF) << 8) | Stencil);
		Flags |= bClearStencil ? CELL_GCM_CLEAR_S : 0;
		Flags |= bClearDepth ? CELL_GCM_CLEAR_Z : 0;
	}

	if (bClearColor)
	{
		// Use a drawcall to clear the color buffer if it's a floating point format.
		if ( GPS3Gcm->IsCurrentRenderTargetFloatingPoint() )
		{
			RHISetDepthState(TStaticDepthState<FALSE, CF_Always>::GetRHI());
			RHISetStencilState(TStaticStencilState<>::GetRHI());
			FillRenderTargetWithColor( Color, Depth );
		}
		else
		{
			FColor LDRColor( Color.Quantize() );
			cellGcmSetClearColor( LDRColor.DWColor() );
			Flags |= CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A;
		}
	}

	if ( Flags )
	{
		cellGcmSetClearSurface(Flags);
	}
}

/**
* Copies the contents of the given surface to its resolve target texture.
* @param SourceSurface - surface with a resolve texture to copy to
* @param bKeepOriginalSurface - TRUE if the original surface will still be used after this function so must remain valid
* @param ResolveParams - optional resolve params
*/
void RHICopyToResolveTarget(FSurfaceRHIParamRef SourceSurface, UBOOL bKeepOriginalSurface, const FResolveParams& ResolveParams)
{
	// Try the resolve target in the resolve parameters first, then the resolve target in the source surface, and finally
	// if neither are valid, the cube map in the source surface.
	FTexture2DRHIParamRef ResolveTarget = ResolveParams.ResolveTarget;
	BOOL bResolveTargetBelongsToSurface = FALSE;
	if (!IsValidRef(ResolveTarget))
	{
		bResolveTargetBelongsToSurface = TRUE;
		ResolveTarget = SourceSurface->ResolveTarget;
	}

	if (IsValidRef(ResolveTarget) || IsValidRef(SourceSurface->ResolveTargetCube))
	{
		// get some values from the target (either 2d or cube)
		UINT TargetMemoryOffset;
		UINT TargetMemoryRegion;
		UINT TargetPitch;

		// is it a 2d texture (and not a cubemap)
		UBOOL bIs2D = IsValidRef(ResolveTarget);

		// handle the 2d texture case
		if (bIs2D)
		{
			TargetMemoryOffset = bResolveTargetBelongsToSurface ? SourceSurface->ResolveTargetMemoryOffset : ResolveTarget->MemoryOffset;
			TargetMemoryRegion = ResolveTarget->MemoryRegion;
			TargetPitch = ResolveTarget->GetPitch();
		}
		// cubemap case
		else
		{
			extern DWORD GetTextureMipSize(INT Format, DWORD SizeX, DWORD SizeY, DWORD MipIndex, UBOOL bIsLinear);
			// calculate the size of one face (each one is aligned on 128)
			UINT FaceOffset = Align(GetTextureMipSize(SourceSurface->ResolveTargetCube->GetUnrealFormat(), SourceSurface->SizeX, SourceSurface->SizeX, 0, TRUE), 128);
			// get offset to the face in question
			FaceOffset *= (INT)SourceSurface->ResolveTargetCubeFace;

			TargetMemoryOffset = SourceSurface->ResolveTargetCube->MemoryOffset + FaceOffset;
			TargetMemoryRegion = SourceSurface->ResolveTargetCube->MemoryRegion;
			TargetPitch = SourceSurface->ResolveTargetCube->GetPitch();
		}

		// only copy if the memory offset is different
		if ((SourceSurface->MemoryOffset != TargetMemoryOffset) || (SourceSurface->MemoryRegion != TargetMemoryRegion))
		{
			// Lets do some trickery if surface is texture is in the same memory region and is 2D.
			if (bResolveTargetBelongsToSurface && (SourceSurface->MemoryRegion == TargetMemoryRegion) && bIs2D)
			{
				// validate this is kosher (can use ResolveTarget since we know this isn't a cubemap)
				check(SourceSurface->SizeX == ResolveTarget->GetSizeX());
				check(SourceSurface->SizeY == ResolveTarget->GetSizeY());
				check(SourceSurface->Pitch == ResolveTarget->GetPitch());
				check(ResolveTarget->GetCreationFlags() & TexCreate_ResolveTargetable);
				extern DWORD TextureToSurfaceFormat(EPixelFormat PixelFormat);
				check(SourceSurface->Format == TextureToSurfaceFormat(ResolveTarget->GetUnrealFormat()));

				if ( bKeepOriginalSurface )
				{
					// Simply make the texture point to the same memory as the surface.
					// We'll be reading and writing to the same pixels, but that's fine as long as we don't apply an offset (e.g. distortion).
					SourceSurface->UnifyWithResolveTarget();
				}
				else
				{
					// if we don't need the original surface, we can simply swap the texture and surface pointers because 
					// they are really two identical bits of memory, and since we don't need to use the surface anymore, we
					// don't need to copy from it to preserve it
					// this won't work right for cubemaps, since we do this 6 times for each cubemap

					// swap memory pointers.
					SourceSurface->SwapWithResolveTarget();

					// if we copied from current render or depth targets, then re-set them to make use
					// of the new memory pointer
					if (SourceSurface == GPS3RHIRenderTarget || SourceSurface == GPS3RHIDepthTarget)
					{
						// Save current viewport and scissor settings.
						UINT ViewportX = GViewportX;
						UINT ViewportY = GViewportY;
						UINT ViewportWidth = GViewportWidth;
						UINT ViewportHeight = GViewportHeight;
						FLOAT ViewportMinZ = GViewportMinZ;
						FLOAT ViewportMaxZ = GViewportMaxZ;
						UINT ScissorX = GScissorX;
						UINT ScissorY = GScissorY;
						UINT ScissorWidth = GScissorWidth;
						UINT ScissorHeight = GScissorHeight;
						RHISetRenderTarget( GPS3RHIRenderTarget, GPS3RHIDepthTarget );

						// Restore viewport and scissor settings (note scissoring is always on for PS3).
						RHISetViewport(  ViewportX, ViewportY, ViewportMinZ, ViewportX+ViewportWidth, ViewportY+ViewportHeight, ViewportMaxZ);
						RHISetScissorRect(  TRUE, ScissorX, ScissorY, ScissorX+ScissorWidth, ScissorY+ScissorHeight );
					}
				}

				// no need to continue!
				return;
			}

			// currently, no host copying
			check(SourceSurface->MemoryRegion == MR_Local && TargetMemoryRegion == MR_Local);
			check(TargetPitch == SourceSurface->Pitch);

			// GPU timing (optional as it will slow down the framerate significantly)
			DECLARE_CYCLE_COUNTER(STAT_CopyToResolveTarget_GPU);
	
			// 8 bytes per texel crashes the GPU, so fake the width if we need to
			DWORD BytesPerTexel = SourceSurface->Pitch / SourceSurface->SizeX;
			check(BytesPerTexel >= 4);
			DWORD Multiplier = BytesPerTexel / 4;
			// tell GPU to copy data
			cellGcmSetTransferImage(
				CELL_GCM_TRANSFER_LOCAL_TO_LOCAL, 
				TargetMemoryOffset,
				TargetPitch,
				0, 0,
				SourceSurface->MemoryOffset,
				SourceSurface->Pitch,
				0, 0,
				SourceSurface->SizeX * Multiplier,
				SourceSurface->SizeY,
				4); // hard code to 4 bytes per texel
		}
	}
}

/**
* Copies the contents of the given surface's resolve target texture back to the surface.
* If the surface isn't currently allocated, the copy may be deferred until the next time it is allocated.
*/
void RHICopyFromResolveTarget(FSurfaceRHIParamRef DestSurface)
{
	// not needed
}
void RHICopyFromResolveTargetFast(FSurfaceRHIParamRef DestSurface)
{
	// not needed
}

void RHICopyFromResolveTargetRectFast(FSurfaceRHIParamRef DestSurface,FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2)
{
	// not needed	
}

/**
* Set bound shader state. This will set the vertex decl/shader, and pixel shader
* @param BoundShaderState - state resource
*/
void RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderState)
{
	check(IsValidRef(BoundShaderState.VertexDeclaration));

	// set vertex decl, deferred to Draw call.
	GPendingVertexDeclaration = BoundShaderState.VertexDeclaration;
	GPS3RHIDirty |= PS3RHIDIRTY_ALLSTREAMS;

#if RHI_SUPPORTS_PREVERTEXSHADER_CULLING
	// Find the vertex stream index which contains the position attribute.
	GPendingPositionStreamIndex = INDEX_NONE;
	for(UINT ElementIndex = 0;ElementIndex < GPendingVertexDeclaration->VertexElements.Num();ElementIndex++)
	{
		const FVertexElement& VertexElement = GPendingVertexDeclaration->VertexElements(ElementIndex);
		if(VertexElement.Usage == VEU_Position)
		{
			GPendingPositionStreamIndex = VertexElement.StreamIndex;
			check(GPendingPositionStreamIndex < MAX_VERTEX_STREAMS);
			break;
		}
	}
#endif

	// Set vertex shader
	if( GCurrentVertexShader != BoundShaderState.VertexShader )
	{
		CellCgbVertexProgramConfiguration Config;
		GCurrentVertexShader		= BoundShaderState.VertexShader;
		Config.instructionSlot		= 0;
		Config.instructionCount		= GCurrentVertexShader->Info.NumInstructions;
		Config.attributeInputMask	= GCurrentVertexShader->Info.AttribMask;
		Config.registerCount		= GCurrentVertexShader->Info.NumRegisters;
		cellGcmSetVertexProgramLoad( &Config, GCurrentVertexShader->Microcode );

		// Set internal constant values.
		FShaderDefaultValue* DefaultValues = GCurrentVertexShader->DefaultValues;
		for ( INT Index=0; Index < GCurrentVertexShader->Info.NumDefaultValues; ++Index )
		{
			DWORD ConstantRegister = DefaultValues[Index].ConstantRegister;
			checkf(ConstantRegister < PS3_TOTAL_VERTEX_CONSTANTS, TEXT("RHISetCountShaderState tried to set an invalid constant register: %u. Max is %u"), ConstantRegister, PS3_TOTAL_VERTEX_CONSTANTS);
			cellGcmSetVertexProgramParameterBlock( ConstantRegister, 1, DefaultValues[Index].DefaultValue );
		}
	}

	// Set pixel shader as pending
	FPixelShaderRHIParamRef PixelShader;
	if ( IsValidRef(BoundShaderState.PixelShader) )
	{
		PixelShader = BoundShaderState.PixelShader;
	}
	else
	{
		// use special null pixel shader when PixelSahder was set to NULL
		TShaderMapRef<FNULLPixelShader> NullPixelShader(GetGlobalShaderMap());
		PixelShader = NullPixelShader->GetPixelShader();
	}
	if ( GPendingPixelShader != PixelShader )
	{
		GPendingPixelShader = PixelShader;
		GPS3RHIDirty |= PS3RHIDIRTY_PIXELSHADER;
	}
}

// globals used by RHI[Begin/End]DrawPrimitiveUP and RHI[Begin/End]DrawIndexedPrimitiveUP
static void *GDrawPrimitiveUPVertexData = NULL;
static UINT GNumVertices = 0;
static UINT GVertexDataStride = 0;

static void *GDrawPrimitiveUPIndexData = NULL;
static UINT GPrimitiveType = 0;
static UINT GNumPrimitives = 0;
static UINT GMinVertexIndex = 0;
static UINT GIndexDataStride = 0;

/**
* Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
* @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
* @param NumPrimitives The number of primitives in the VertexData buffer
* @param NumVertices The number of vertices to be written
* @param VertexDataStride Size of each vertex 
* @param OutVertexData Reference to the allocated vertex memory
*/
void RHIBeginDrawPrimitiveUP(UINT PrimitiveType, UINT NumPrimitives, UINT NumVertices, UINT VertexDataStride, void*& OutVertexData)
{
	check(NULL == GDrawPrimitiveUPVertexData);

	// calc size
	DWORD VertexDataSize = NumVertices * VertexDataStride;
	
	// allocate space to fill out
	GDrawPrimitiveUPVertexData = GPS3Gcm->GetDynamicRingBuffer()->Malloc(VertexDataSize);

	OutVertexData = GDrawPrimitiveUPVertexData;

	// remember the info for the EndDraw
	GPrimitiveType = PrimitiveType;
	GNumPrimitives = NumPrimitives;
	GNumVertices = NumVertices;
	GVertexDataStride = VertexDataStride;
}

/**
* Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
*/
void RHIEndDrawPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER(STAT_DrawPrimitiveUP);

	check(NULL != GDrawPrimitiveUPVertexData);
	checkSlow( GNumPrimitives > 0 );

	// set vertex streams (translating from pointer to offset)
	UINT VertexOffset;
	cellGcmAddressToOffset(GDrawPrimitiveUPVertexData, &VertexOffset);
	PS3UpdateUP(VertexOffset, GVertexDataStride);

	DWORD NumVertices = CalcNumElements( GPrimitiveType, GNumPrimitives );

	if ( GNumPrimitives > 0 )
	{
		// render the primitive
		cellGcmSetDrawArrays(
			UnrealPrimitiveTypeToType[GPrimitiveType],
			0,
			NumVertices);
	}
	PS3UpdateAutoFence();
	
	GDrawPrimitiveUPVertexData = NULL;
}

/**
 * Draw a primitive using the vertices passed in
 * VertexData is NOT created by BeginDrawPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param VertexData A reference to memory preallocate in RHIBeginDrawPrimitiveUP
 * @param VertexDataStride The size of one vertex
 */
void RHIDrawPrimitiveUP(UINT PrimitiveType, UINT NumPrimitives, const void* InVertexData, UINT VertexDataStride)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawPrimitiveUP);
	checkSlow( NumPrimitives > 0 );

	DWORD NumVertices = CalcNumElements( PrimitiveType, NumPrimitives );

	// calc size
	DWORD VertexDataSize = NumVertices * VertexDataStride;

	// allocate memory
	void* VertexData = GPS3Gcm->GetDynamicRingBuffer()->Malloc(VertexDataSize);

	UINT VertexOffset;
	cellGcmAddressToOffset(VertexData, &VertexOffset);

	// copy over the data
	appMemcpy(VertexData, InVertexData, VertexDataSize);

	// set vertex streams and all dirty deferred states
	PS3UpdateUP(VertexOffset, VertexDataStride);

	if ( NumPrimitives > 0 )
	{
		// draw some verts!
		cellGcmSetDrawArrays(
			UnrealPrimitiveTypeToType[PrimitiveType],
			0,
			NumVertices);
	}

	PS3UpdateAutoFence();
}

/**
* Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawIndexedPrimitiveUP
* @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
* @param NumPrimitives The number of primitives in the VertexData buffer
* @param NumVertices The number of vertices to be written
* @param VertexDataStride Size of each vertex
* @param OutVertexData Reference to the allocated vertex memory
* @param MinVertexIndex The lowest vertex index used by the index buffer
* @param NumIndices Number of indices to be written
* @param IndexDataStride Size of each index (either 2 or 4 bytes)
* @param OutIndexData Reference to the allocated index memory
*/
void RHIBeginDrawIndexedPrimitiveUP(UINT PrimitiveType, UINT NumPrimitives, UINT NumVertices, UINT VertexDataStride, void*& OutVertexData, UINT MinVertexIndex, UINT NumIndices, UINT IndexDataStride, void*& OutIndexData)
{
	check((sizeof(WORD) == IndexDataStride) || (sizeof(DWORD) == IndexDataStride));
	check(NULL == GDrawPrimitiveUPVertexData);
	check(NULL == GDrawPrimitiveUPIndexData);

	// allocate space to fill out
	UINT VertexDataSize = NumVertices * VertexDataStride;
	GDrawPrimitiveUPVertexData = GPS3Gcm->GetDynamicRingBuffer()->Malloc(VertexDataSize);
	OutVertexData = GDrawPrimitiveUPVertexData;

	UINT IndexDataSize = NumIndices * IndexDataStride;
	GDrawPrimitiveUPIndexData = GPS3Gcm->GetDynamicRingBuffer()->Malloc(IndexDataSize);
	OutIndexData = GDrawPrimitiveUPIndexData;

	// remember settings
	GPrimitiveType = PrimitiveType;
	GNumPrimitives = NumPrimitives;
	GMinVertexIndex = MinVertexIndex;
	GIndexDataStride = IndexDataStride;

	GNumVertices = NumVertices;
	GVertexDataStride = VertexDataStride;
}

/**
* Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
*/
void RHIEndDrawIndexedPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER(STAT_DrawPrimitiveUP);
	checkSlow( GNumPrimitives > 0 );

	check(NULL != GDrawPrimitiveUPVertexData);
	check(NULL != GDrawPrimitiveUPIndexData);

	// get data in offset, not EA
	UINT VertexOffset, IndexOffset;
	cellGcmAddressToOffset(GDrawPrimitiveUPVertexData, &VertexOffset);
	cellGcmAddressToOffset(GDrawPrimitiveUPIndexData, &IndexOffset);

	// set vertex streams and all dirty deferred states
	PS3UpdateUP(VertexOffset, GVertexDataStride);

	if ( GNumPrimitives > 0 )
	{
		DWORD NumIndices = CalcNumElements( GPrimitiveType, GNumPrimitives );

		// render the primitive
		cellGcmSetDrawIndexArray(
			UnrealPrimitiveTypeToType[GPrimitiveType],
			NumIndices,
			GIndexDataStride == 2 ? CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16 : CELL_GCM_DRAW_INDEX_ARRAY_TYPE_32,
			CELL_GCM_LOCATION_MAIN,
			IndexOffset);
	}

	PS3UpdateAutoFence();
	
	GDrawPrimitiveUPVertexData = NULL;
	GDrawPrimitiveUPIndexData = NULL;
}

/**
 * Draw a primitive using the vertices passed in as described the passed in indices. 
 * IndexData and VertexData are NOT created by BeginDrawIndexedPrimitveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param MinVertexIndex The lowest vertex index used by the index buffer
 * @param NumVertices The number of vertices in the vertex buffer
 * @param NumPrimitives THe number of primitives described by the index buffer
 * @param IndexData The memory preallocated in RHIBeginDrawIndexedPrimitiveUP
 * @param IndexDataStride The size of one index
 * @param VertexData The memory preallocate in RHIBeginDrawIndexedPrimitiveUP
 * @param VertexDataStride The size of one vertex
 */
void RHIDrawIndexedPrimitiveUP(UINT PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT NumPrimitives, const void* InIndexData, UINT IndexDataStride, const void* InVertexData, UINT VertexDataStride)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawPrimitiveUP);
	checkSlow( NumPrimitives > 0 );

	// calculate how many indices we have
	DWORD NumIndices = CalcNumElements( PrimitiveType, NumPrimitives );

	// calculate how much memory we need
	DWORD VertexSize = NumVertices * VertexDataStride;
	DWORD IndexSize = NumIndices * IndexDataStride;

	// allocate space 
	void* VertexData = GPS3Gcm->GetDynamicRingBuffer()->Malloc(VertexSize);
	UINT VertexOffset;
	cellGcmAddressToOffset(VertexData, &VertexOffset);

	void* IndexData = GPS3Gcm->GetDynamicRingBuffer()->Malloc(IndexSize);
	UINT IndexOffset;
	cellGcmAddressToOffset(IndexData, &IndexOffset);

	// copy the immediate data into the ring bufer
	appMemcpy(VertexData, InVertexData, VertexSize);
	appMemcpy(IndexData, InIndexData, IndexSize);

	// set vertex streams and all dirty deferred states
	PS3UpdateUP(VertexOffset, VertexDataStride);

	if ( NumPrimitives > 0 )
	{
		// draw some verts!
		cellGcmSetDrawIndexArray(
			UnrealPrimitiveTypeToType[PrimitiveType],
			NumIndices,
			IndexDataStride == 2 ? CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16 : CELL_GCM_DRAW_INDEX_ARRAY_TYPE_32,
			CELL_GCM_LOCATION_MAIN,
			IndexOffset);
	}

	PS3UpdateAutoFence();
}

/**
 * Draw a sprite particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite particles
 */
void RHIDrawSpriteParticles(const FMeshElement& Mesh)
{
	check(Mesh.DynamicVertexData);
	FDynamicSpriteEmitterData* SpriteData = (FDynamicSpriteEmitterData*)(Mesh.DynamicVertexData);

	// Sort the particles if required
	INT ParticleCount = SpriteData->Source.ActiveParticleCount;

	// 'clamp' the number of particles actually drawn
	//@todo.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	if ((SpriteData->Source.MaxDrawCount >= 0) && (ParticleCount > SpriteData->Source.MaxDrawCount))
	{
		ParticleCount = SpriteData->Source.MaxDrawCount;
	}

	// Render the particles are indexed tri-lists
	// Make sure we won't choke the buffer
	// NOTE: There is no ValidateDrawUPCall on PS3... may want to implement one though
//	if (ValidateDrawUPCall(ParticleCount * 4, sizeof(FParticleSpriteVertex)))
	{
		// Get the memory from the device for copying the particle vertex/index data to
		void* OutVertexData = NULL;
		RHIBeginDrawPrimitiveUP(PT_QuadList, ParticleCount, ParticleCount * 4, Mesh.DynamicVertexStride, OutVertexData);
		check(OutVertexData);
		// Pack the data
		FParticleSpriteVertex* Vertices = (FParticleSpriteVertex*)OutVertexData;
		SpriteData->GetVertexAndIndexData(Vertices, NULL, (FParticleOrder*)(Mesh.DynamicIndexData));
		// End the draw, which will submit the data for rendering
		RHIEndDrawPrimitiveUP();
	}
//	else
//	{
//#if !FINAL_RELEASE
//		warnf(TEXT("Failed to DrawSpriteParticles - too many vertices (%d)"), ParticleCount * 4);
//#endif	//#if !FINAL_RELEASE
//	}
}

/**
 * Draw a sprite subuv particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite subuv particles
 */
void RHIDrawSubUVParticles(const FMeshElement& Mesh)
{
	check(Mesh.DynamicVertexData);
	FDynamicSubUVEmitterData* SubUVData = (FDynamicSubUVEmitterData*)(Mesh.DynamicVertexData);

	// Sort the particles if required
	INT ParticleCount = SubUVData->Source.ActiveParticleCount;

	// 'clamp' the number of particles actually drawn
	//@todo.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	if ((SubUVData->Source.MaxDrawCount >= 0) && (ParticleCount > SubUVData->Source.MaxDrawCount))
	{
		ParticleCount = SubUVData->Source.MaxDrawCount;
	}

	// Render the particles are indexed tri-lists
	// Make sure we won't choke the buffer
	// NOTE: There is no ValidateDrawUPCall on PS3... may want to implement one though
//	if (ValidateDrawUPCall(ParticleCount * 4, sizeof(FParticleSpriteSubUVVertex)))
	{
		// Get the memory from the device for copying the particle vertex/index data to
		void* OutVertexData = NULL;
		RHIBeginDrawPrimitiveUP( PT_QuadList, ParticleCount, ParticleCount * 4, Mesh.DynamicVertexStride, OutVertexData);
		check(OutVertexData);
		// Pack the data
		FParticleSpriteSubUVVertex* Vertices = (FParticleSpriteSubUVVertex*)OutVertexData;
		SubUVData->GetVertexAndIndexData(Vertices, NULL, (FParticleOrder*)(Mesh.DynamicIndexData));
		// End the draw, which will submit the data for rendering
		RHIEndDrawPrimitiveUP();
	}
//	else
//	{
//#if !FINAL_RELEASE
//		warnf(TEXT("Failed to DrawSpriteParticles - too many vertices (%d)"), ParticleCount * 4);
//#endif	//#if !FINAL_RELEASE
//	}
}

/**
 * Draw a point sprite particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite subuv particles
 */
void RHIDrawPointSpriteParticles(const FMeshElement& Mesh) 
{
	// Not implemented yet!
}

static const UINT GPS3NumBytesPerShaderRegister = sizeof(FLOAT) * 4;

void RHISetShaderParameter(FVertexShaderRHIParamRef VertexShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue,INT ParamIndex)
{
	RHISetVertexShaderParameter(VertexShader, BufferIndex, BaseIndex, NumBytes, NewValue, ParamIndex);
}

void RHISetVertexShaderParameter(FVertexShaderRHIParamRef VertexShader,UINT /*BufferIndex*/,UINT BaseIndex,UINT NumBytes,const void* NewValue,INT ParamIndex)
{
	DWORD BaseRegister = BaseIndex / GPS3NumBytesPerShaderRegister;
	DWORD NumRegisters = (NumBytes + GPS3NumBytesPerShaderRegister - 1) / GPS3NumBytesPerShaderRegister;
	cellGcmSetVertexProgramParameterBlock( BaseRegister, NumRegisters, (FLOAT*)GetPaddedShaderParameterValue(NewValue,NumBytes) );
}

void RHISetVertexShaderBoolParameter(FVertexShaderRHIParamRef VertexShader,UINT /*BufferIndex*/,UINT BaseIndex,UBOOL NewValue)
{
	const FLOAT Value = NewValue ? 1.0f : 0.0f;
	const FLOAT VectorValue[4] GCC_ALIGN(16) = { Value, Value, Value, Value };
	DWORD BaseRegister = BaseIndex / GPS3NumBytesPerShaderRegister;
	cellGcmSetVertexProgramParameterBlock( BaseRegister, 1, VectorValue );
}

/*
 * *** PERFORMANCE WARNING *****
 * This function is to support single float array parameter in shader
 * This pads 3 more floats for each element - at the expensive of CPU/stack memory
 * Do not overuse this. If you can, use float4 in shader. 
 * TODO: Optimize for PS3
 */
void RHISetVertexShaderFloatArray(FVertexShaderRHIParamRef VertexShader,UINT /*BufferIndex*/,UINT BaseIndex,UINT NumValues,const FLOAT* FloatValues,INT ParamIndex)
{
	const UINT MaxSupportedValues = 76;
	checkSlow( ( MaxSupportedValues % 4 ) == 0 );
	checkSlow( NumValues <= MaxSupportedValues );

	FVector4 FloatVertexShaderConstants[MaxSupportedValues];
	for( INT i=0; i<NumValues; i++ )
		FloatVertexShaderConstants[i].X = FloatValues[i];

	DWORD BaseRegister = BaseIndex / GPS3NumBytesPerShaderRegister;
	DWORD NumBytes = NumValues*GPS3NumBytesPerShaderRegister;
	DWORD NumRegisters = (NumBytes + GPS3NumBytesPerShaderRegister - 1) / GPS3NumBytesPerShaderRegister;

	cellGcmSetVertexProgramParameterBlock( BaseRegister, NumRegisters, (FLOAT*)GetPaddedShaderParameterValue(FloatVertexShaderConstants,NumBytes) );
}

void RHISetShaderParameter(FPixelShaderRHIParamRef PixelShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue,INT ParamIndex)
{
	RHISetPixelShaderParameter(PixelShader, BufferIndex, BaseIndex, NumBytes, NewValue, ParamIndex);
}

void RHISetPixelShaderParameter(FPixelShaderRHIParamRef PixelShader,UINT /*BufferIndex*/,UINT BaseIndex,UINT InNumBytes,const void* NewValue,INT ParamIndex)
{
	FLOAT * RESTRICT Dest = (FLOAT*) &GPixelShaderConstants[BaseIndex / GPS3NumBytesPerShaderRegister];
	const FLOAT * RESTRICT Src = (FLOAT*) NewValue;
	INT NumBytes = (INT) InNumBytes;
	while (NumBytes > 0)
	{
		*Dest++ = *Src++;
		NumBytes -= 4;
	}
	checkSlow(NumBytes==0); // shouldn't have any leftovers
	GPS3RHIDirty |= PS3RHIDIRTY_PIXELSHADER;
}

void RHISetPixelShaderBoolParameter(FPixelShaderRHIParamRef PixelShader,UINT /*BufferIndex*/,UINT BaseIndex,UBOOL NewValue)
{
	const FLOAT Value = NewValue ? 1.0f : 0.0f;
	GPixelShaderConstants[BaseIndex / GPS3NumBytesPerShaderRegister][0] = Value;
	GPixelShaderConstants[BaseIndex / GPS3NumBytesPerShaderRegister][1] = Value;
	GPixelShaderConstants[BaseIndex / GPS3NumBytesPerShaderRegister][2] = Value;
	GPixelShaderConstants[BaseIndex / GPS3NumBytesPerShaderRegister][3] = Value;
	GPS3RHIDirty |= PS3RHIDIRTY_PIXELSHADER;
}

/**
 * Set engine shader parameters for the view.
 * @param View					The current view
 */
void RHISetViewParameters(const FSceneView& View)
{
	RHISetViewParametersWithOverrides(View, View.TranslatedViewProjectionMatrix, View.DiffuseOverrideParameter, View.SpecularOverrideParameter);
}

/**
 * Set engine shader parameters for the view.
 * @param View					The current view
 * @param ViewProjectionMatrix	Matrix that transforms from world space to projection space for the view
 * @param DiffuseOverride		Material diffuse input override
 * @param SpecularOverride		Material specular input override
 */
void RHISetViewParametersWithOverrides( const FSceneView& View, const FMatrix& ViewProjectionMatrix, const FVector4& DiffuseOverride, const FVector4& SpecularOverride )
{
	const FVector4 TranslatedViewOrigin = View.ViewOrigin + FVector4(View.PreViewTranslation,0);
	const FVector4 PreViewTranslation = View.PreViewTranslation;
	const FVector4 TemporalAAParameters = View.TemporalAAParameters.GetVector();

	cellGcmSetVertexProgramParameterBlock( VSR_ViewProjMatrix, 4, (const FLOAT*) &ViewProjectionMatrix );
	cellGcmSetVertexProgramParameterBlock( VSR_ViewOrigin, 1, (const FLOAT*) &TranslatedViewOrigin );
	cellGcmSetVertexProgramParameterBlock( VSR_PreViewTranslation, 1, (const FLOAT*) &PreViewTranslation );
	cellGcmSetVertexProgramParameterBlock( VSR_TemporalAAParameters, 1, (const FLOAT*) &TemporalAAParameters );

	// Save the view projection matrix to be used for EDGE geometry culling.
	GPendingViewProjectionMatrix = ViewProjectionMatrix;
	GPendingViewTranslation = View.PreViewTranslation;
}

/**
 * Set engine pixel shader parameters for the view.
 * Some platforms needs to set this for each pixelshader, whereas others can set it once, globally.
 * @param View								The current view
 * @param PixelShader						The pixel shader to set the parameters for
 * @param SceneDepthCalcParameter			Handle for the scene depth calc parameter (PSR_MinZ_MaxZ_Ratio). May be NULL.
 * @param ScreenPositionScaleBiasParameter	Handle for the screen position scale and bias parameter (PSR_ScreenPositionScaleBias). May be NULL.
 */
void RHISetViewPixelParameters( const FSceneView* View, FPixelShaderRHIParamRef PixelShader, const FShaderParameter* SceneDepthCalcParameter, const FShaderParameter* ScreenPositionScaleBiasParameter )
{
	if ( SceneDepthCalcParameter && SceneDepthCalcParameter->IsBound() )
	{
		SetPixelShaderValue(PixelShader,*SceneDepthCalcParameter,View->InvDeviceZToWorldZTransform);
	}
	if ( ScreenPositionScaleBiasParameter && ScreenPositionScaleBiasParameter->IsBound() )
	{
		SetPixelShaderValue(PixelShader,*ScreenPositionScaleBiasParameter,View->ScreenPositionScaleBias);
	}
}

void RHISetSamplerStateOnly(FPixelShaderRHIParamRef PixelShader,UINT /*SamplerIndex*/,FSamplerStateRHIParamRef NewState,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip,UBOOL bForceLinearMinFilter)
{
	// Not yet implemented
	check(0);
}

void RHISetTextureParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	// Not yet implemented
	check(0);
}

void RHISetSurfaceParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewTextureRHI)
{
	// Not yet implemented
	check(0);
}

/**
 * Sets sampler state.
 *
 * @param PixelShader	The pixelshader using the sampler for the next drawcalls.
 * @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
 * @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
 * @param MipBias		Mip bias to use for the texture
 * @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
 * @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
 */
void RHISetSamplerState(FPixelShaderRHIParamRef PixelShader,UINT TextureIndex,UINT /*SamplerIndex*/,FSamplerStateRHIParamRef NewState,FTextureRHIParamRef NewTexture,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip,UBOOL bForceLinearMinFilter)
{
	FPS3RHIPixelShader* Shader = PixelShader;
	FPS3RHITexture* NewTexturePtr = NewTexture;
	check( NewTexturePtr );

	DWORD PS3SamplerIndex;
	if ( TextureIndex < Shader->Info->NumSamplers )
	{
		PS3SamplerIndex = Shader->TextureUnits[TextureIndex];	//@TODO: Once texture arrays adhers to -contalloc, we can remove this helper data.
	}
	else
	{
		// Sampler was optimized out. (Note that Shader->TextureUnits[] may also occasionally contain 255.)
		PS3SamplerIndex = 255;
	}

	// Check if the sampler has been optimized out. (Can happen if there's an unused slot in an array of samplers.)
	if ( PS3SamplerIndex != 255 )
	{
		check( PS3SamplerIndex < 16 );

		if ( GCurrentTexture[PS3SamplerIndex] )
		{
			DWORD OldFormat = GCurrentTexture[PS3SamplerIndex]->GetFormat() & ~(CELL_GCM_TEXTURE_SZ|CELL_GCM_TEXTURE_LN|CELL_GCM_TEXTURE_NR|CELL_GCM_TEXTURE_UN);
			GNumFatTexturesUsed -= (OldFormat == CELL_GCM_TEXTURE_W32_Z32_Y32_X32_FLOAT || OldFormat == CELL_GCM_SURFACE_F_X32) ? 1 : 0;
			GNumSemiFatTexturesUsed -= (OldFormat == CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT) ? 1 : 0;
		}
		DWORD Format = NewTexture->GetFormat() & ~(CELL_GCM_TEXTURE_SZ|CELL_GCM_TEXTURE_LN|CELL_GCM_TEXTURE_NR|CELL_GCM_TEXTURE_UN);
		GNumFatTexturesUsed += (Format == CELL_GCM_TEXTURE_W32_Z32_Y32_X32_FLOAT || Format == CELL_GCM_SURFACE_F_X32) ? 1 : 0;
		GNumSemiFatTexturesUsed += (Format == CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT) ? 1 : 0;

		// set the texture
		cellGcmSetTexture(PS3SamplerIndex, &NewTexture->GetTexture());
		GCurrentTexture[PS3SamplerIndex] = NewTexturePtr;
		GEnabledTextures |= GBitFlag[PS3SamplerIndex];

		// set texture states
		// @GEMINI_TODO: convulution?
		DWORD PS3MipBias = (0 << 8) | 0;		// Signed fixpoint 1:4:8, default bias = 0.0
		DWORD PS3MinLod = (0 << 8) | 0;			// Unsigned fixpoint 4:8, default minLOD = 0.0
		DWORD PS3MaxLod = (12 << 8) | 0;		// Unsigned fixpoint 4:8, default maxLOD = 12.0
		if ( appIsNearlyZero(MipBias) == FALSE )
		{
			INT SignedFixpoint = appTrunc( MipBias * 256.0f );
			PS3MipBias = DWORD( Clamp<INT>( SignedFixpoint, -16*256, 16*256-1 ) ) & 0x1fff;
		}
		if ( LargestMip >= 0.0f )
		{
			INT SignedFixpoint = appTrunc( LargestMip * 256.0f );
			PS3MinLod = DWORD( Clamp<INT>( SignedFixpoint, 0, 16*256-1 ) ) & 0x0fff;
		}
		if ( SmallestMip >= 0.0f )
		{
			INT SignedFixpoint = appTrunc( SmallestMip * 256.0f );
			PS3MaxLod = DWORD( Clamp<INT>( SignedFixpoint, 0, 16*256-1 ) ) & 0x0fff;
		}

		cellGcmSetTextureFilter(PS3SamplerIndex, PS3MipBias, bForceLinearMinFilter ? CELL_GCM_TEXTURE_LINEAR_NEAREST : NewState->MinFilter, NewState->MagFilter, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
		cellGcmSetTextureAddress(
			PS3SamplerIndex, 
			NewState->AddressU, 
			NewState->AddressV, 
			NewState->AddressW, 
			NewTexture->GetUnsignedRemap(),
			CELL_GCM_TEXTURE_ZFUNC_GEQUAL,
			NewTexture->IsSRGB() ? (CELL_GCM_TEXTURE_GAMMA_R | CELL_GCM_TEXTURE_GAMMA_G | CELL_GCM_TEXTURE_GAMMA_B) : 0);
		cellGcmSetTextureControl( PS3SamplerIndex, CELL_GCM_TRUE, PS3MinLod, PS3MaxLod, NewState->Anisotropic );
	}
}

void RHISetSamplerState(FVertexShaderRHIParamRef VertexShader,UINT TextureIndex,UINT /*SamplerIndex*/,FSamplerStateRHIParamRef InNewState,FTextureRHIParamRef NewTexture,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip,UBOOL bForceLinearMinFilter)
{
	// Not yet implemented
	check(0);
}

void RHISetVertexTexture(UINT SamplerIndex,FTextureRHIParamRef NewTextureRHI)
{
	// Not yet implemented
	check(0);
}

INT RHIGetMobileUniformSlotIndexByName(FName ParamName)
{
	// Only used on mobile platforms
	return -1;
}

void RHISetMobileTextureSamplerState( FPixelShaderRHIParamRef PixelShader, const INT MobileTextureUnit, FSamplerStateRHIParamRef NewState, FTextureRHIParamRef NewTextureRHI, FLOAT MipBias, FLOAT LargestMip, FLOAT SmallestMip ) 
{
	// Only used on mobile platforms
} 


void RHISetMobileSimpleParams(EBlendMode InBlendMode)
{
	// Only used on mobile platforms
}

void RHISetMobileMaterialVertexParams(const FMobileMaterialVertexParams& InVertexParams)
{
	// Only used on mobile platforms
}

void RHISetMobileMaterialPixelParams(const FMobileMaterialPixelParams& InPixelParams)
{
	// Only used on mobile platforms
}


void RHISetMobileMeshVertexParams(const FMobileMeshVertexParams& InMeshParams)
{
	// Only used on mobile platforms
}


FLOAT RHIGetMobilePercentColorFade(void)
{
	// Only used on mobile platforms
	return 0.0f;
}


void RHISetMobileFogParams (const UBOOL bInEnabled, const FLOAT InFogStart, const FLOAT InFogEnd, const FColor& InFogColor)
{
	// Only used on mobile platforms
}

void RHISetMobileBumpOffsetParams(const UBOOL bInEnabled, const FLOAT InBumpEnd)
{
	// Only used on mobile platforms
}

void RHISetMobileTextureTransformOverride(TMatrix<3,3>& InOverrideTransform)
{
	// Only used on mobile platforms
}

void RHIResetTrackedPrimitive()
{
	// Not implemented yet
}

void RHIIncrementTrackedPrimitive(const INT InDelta)
{
	// Not implemented yet
}


void RHIClearSamplerBias()
{
	// Not used
}

void RHISetBlendState(FBlendStateRHIParamRef NewState)
{
	cellGcmSetBlendEnable( NewState->bAlphaBlendEnable );
	cellGcmSetBlendFunc( NewState->ColorSourceBlendFactor, NewState->ColorDestBlendFactor, NewState->AlphaSourceBlendFactor, NewState->AlphaDestBlendFactor );
	cellGcmSetBlendEquation( NewState->ColorBlendOperation, NewState->AlphaBlendOperation );
	GPendingAlphaTestEnable	= NewState->bAlphaTestEnable;
	GPendingAlphaTestFunc	= NewState->AlphaFunc;
	GPendingAlphaTestRef	= NewState->AlphaRef;
	GPS3RHIDirty			|= PS3RHIDIRTY_ALPHATEST;	
}

void RHISetColorWriteEnable(UBOOL bEnable)
{
	cellGcmSetColorMask(bEnable ? (CELL_GCM_COLOR_MASK_R | CELL_GCM_COLOR_MASK_G | CELL_GCM_COLOR_MASK_B | CELL_GCM_COLOR_MASK_A) : 0);
}

void RHISetMRTColorWriteEnable(UBOOL bEnable,UINT TargetIndex)
{
	//@todo: MRT support for PS3
}

void RHISetColorWriteMask(UINT ColorWriteMask)
{
	DWORD EnabledStateValue;
	EnabledStateValue  = (ColorWriteMask & CW_RED) ? CELL_GCM_COLOR_MASK_R : 0;
	EnabledStateValue |= (ColorWriteMask & CW_GREEN) ? CELL_GCM_COLOR_MASK_G : 0;
	EnabledStateValue |= (ColorWriteMask & CW_BLUE) ? CELL_GCM_COLOR_MASK_B : 0;
	EnabledStateValue |= (ColorWriteMask & CW_ALPHA) ? CELL_GCM_COLOR_MASK_A : 0;
	cellGcmSetColorMask( EnabledStateValue );
}

void RHISetMRTColorWriteMask(UINT ColorWriteMask,UINT TargetIndex)
{
	//@todo: MRT support for PS3
	check(0);
}

void RHISetDepthState(FDepthStateRHIParamRef NewState)
{
	cellGcmSetDepthTestEnable(NewState->bEnableDepthTest);
	cellGcmSetDepthFunc(NewState->DepthTestFunc);
	cellGcmSetDepthMask(NewState->bEnableDepthWrite);
}

void RHISetStencilState(FStencilStateRHIParamRef NewState)
{
	cellGcmSetStencilTestEnable(NewState->bStencilEnable);
	cellGcmSetTwoSidedStencilTestEnable(NewState->bTwoSidedStencilMode);

	cellGcmSetStencilMask(NewState->StencilWriteMask);
	cellGcmSetStencilFunc(NewState->FrontFaceStencilFunc, NewState->StencilRef, NewState->StencilReadMask);
	cellGcmSetStencilOp(NewState->FrontFaceStencilFailStencilOp, NewState->FrontFaceDepthFailStencilOp, NewState->FrontFacePassStencilOp);

	cellGcmSetBackStencilMask(NewState->StencilWriteMask);
	cellGcmSetBackStencilFunc(NewState->BackFaceStencilFunc, NewState->StencilRef, NewState->StencilReadMask);
	cellGcmSetBackStencilOp(NewState->BackFaceStencilFailStencilOp, NewState->BackFaceDepthFailStencilOp, NewState->BackFacePassStencilOp);
}

void RHISetRasterizerState(FRasterizerStateRHIParamRef NewState)
{
	GPendingBackfaceCullingEnable = NewState->bCullEnable;
	GPendingBackfaceCullingFacing = NewState->FrontFace;

	// process culling
	cellGcmSetCullFaceEnable(NewState->bCullEnable);
	cellGcmSetFrontFace(NewState->FrontFace);
	cellGcmSetFrontPolygonMode(NewState->FillMode);
	cellGcmSetBackPolygonMode(NewState->FillMode);

	if ( Abs(NewState->SlopeScaleDepthBias) > 0.000001f || Abs(NewState->DepthBias) > 0.000001f )
	{
		// bias (slope * NewState->SlopeScaleDepthBias + R * NewState->DepthBias
		cellGcmSetPolygonOffset(NewState->SlopeScaleDepthBias, (NewState->DepthBias + GDepthBiasOffset) * GBiasScale);
		cellGcmSetPolygonOffsetFillEnable( CELL_GCM_TRUE );
	}
	else
	{
		cellGcmSetPolygonOffsetFillEnable( CELL_GCM_FALSE );
	}
}

void RHISetRasterizerStateImmediate(const FRasterizerStateInitializerRHI &ImmediateState)
{
	GPendingBackfaceCullingEnable = ImmediateState.CullMode != CM_None;
	GPendingBackfaceCullingFacing = TranslateUnrealCullMode[ImmediateState.CullMode];

	// process culling
	cellGcmSetCullFaceEnable( ImmediateState.CullMode != CM_None );
	cellGcmSetFrontFace( TranslateUnrealCullMode[ImmediateState.CullMode] );
	cellGcmSetFrontPolygonMode( TranslateUnrealFillMode[ImmediateState.FillMode] );
	cellGcmSetBackPolygonMode( TranslateUnrealFillMode[ImmediateState.FillMode] );

	if ( Abs(ImmediateState.SlopeScaleDepthBias) > 0.000001f || Abs(ImmediateState.DepthBias) > 0.000001f )
	{
		// bias (slope * NewState->SlopeScaleDepthBias + R * NewState->DepthBias
		cellGcmSetPolygonOffset(ImmediateState.SlopeScaleDepthBias, (ImmediateState.DepthBias + GDepthBiasOffset) * GBiasScale);
		cellGcmSetPolygonOffsetFillEnable( CELL_GCM_TRUE );
	}
	else
	{
		cellGcmSetPolygonOffsetFillEnable( CELL_GCM_FALSE );
	}
}

void RHISetViewport(UINT MinX,UINT MinY,FLOAT MinZ,UINT MaxX,UINT MaxY,FLOAT MaxZ)
{
	GScissorX = GViewportX = MinX;
	GScissorY = GViewportY = MinY;
	GScissorWidth = GViewportWidth = MaxX - MinX;
	GScissorHeight = GViewportHeight = MaxY - MinY;
	GViewportMinZ = MinZ;
	GViewportMaxZ = MaxZ;

	// formulas for scale and offset to match OpenGL from libgcm docks
	FLOAT Scale[4]	= { GViewportWidth * 0.5f, GViewportHeight * (-0.5f), GViewportMaxZ - GViewportMinZ, 0.0f };
	FLOAT Offset[4]	= { GViewportX + Scale[0], GViewportY - Scale[1], GViewportMinZ, 0.0f };
	cellGcmSetViewport(GViewportX, GViewportY, GViewportWidth, GViewportHeight, GViewportMinZ, GViewportMaxZ, Scale, Offset);
	cellGcmSetScissor(GScissorX, GScissorY, GScissorWidth, GScissorHeight);
}

void RHISetScissorRect(UBOOL bEnable,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY)
{
	// Defined in UnPlayer.cpp. Used here to disable scissors when doing highres screenshots.
	extern UBOOL GIsTiledScreenshot;

	if (!GIsTiledScreenshot && bEnable)
	{
		GScissorX = MinX;
		GScissorY = MinY;
		GScissorWidth = MaxX - MinX;
		GScissorHeight = MaxY - MinY;
	}
	else
	{
		GScissorX = GViewportX;
		GScissorY = GViewportY;
		GScissorWidth = GViewportWidth;
		GScissorHeight = GViewportHeight;
	}

	cellGcmSetScissor(GScissorX, GScissorY, GScissorWidth, GScissorHeight);
}

/**
 * Set depth bounds test state.  
 * When enabled, incoming fragments are killed if the value already in the depth buffer is outside of [ClipSpaceNearPos, ClipSpaceFarPos]
 */
void RHISetDepthBoundsTest(UBOOL bEnable,const FVector4 &ClipSpaceNearPos,const FVector4 &ClipSpaceFarPos)
{
	cellGcmSetDepthBoundsTestEnable(bEnable);

	if (bEnable)
	{
		// convert to normalized device coordinates, which are the units cellGcmSetDepthBounds expects.
		// clamp to valid ranges
		FLOAT MinZ = Clamp(Max(ClipSpaceNearPos.Z, 0.0f) / ClipSpaceNearPos.W, 0.0f, 1.0f);
		FLOAT MaxZ = Clamp(ClipSpaceFarPos.Z / ClipSpaceFarPos.W, 0.0f, 1.0f);

		// only set depth bounds if ranges are valid
		if (MinZ <= MaxZ)
	{
		cellGcmSetDepthBounds(MinZ, MaxZ);
	}
	}
}


void RHISetStreamSource(UINT StreamIndex, FVertexBufferRHIParamRef VertexBuffer, UINT Stride,UBOOL bUseInstanceIndex,UINT NumVerticesPerInstance,UINT NumInstances)
{
	// Defer the setting to the Draw call.
	GPendingStreams[ StreamIndex ].VertexBuffer = VertexBuffer;
	GPendingStreams[ StreamIndex ].Stride = Stride;
	GPendingStreams[ StreamIndex ].bUseInstanceIndex = bUseInstanceIndex;
	GPendingStreams[ StreamIndex ].NumVerticesPerInstance = NumVerticesPerInstance;
	GPendingNumInstances = NumInstances;
	GPS3RHIDirty |= PS3RHIDIRTY_ALLSTREAMS;
}

void RHIDrawPrimitive(UINT PrimitiveType,UINT BaseVertexIndex,UINT NumPrimitives)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawPrimitive);
	checkSlow( NumPrimitives > 0 );

	DWORD NumVertices = CalcNumElements( PrimitiveType, NumPrimitives );
	PS3SetVertexOffsets(BaseVertexIndex, 0);
	PS3UpdateDirtyStates();

	if ( NumPrimitives > 0 )
	{
		// Draw the instance.
		cellGcmSetDrawArrays(
			UnrealPrimitiveTypeToType[PrimitiveType],
			0,
			NumVertices);
	}

	PS3UpdateAutoFence();

	check(GPendingNumInstances <= 1);
}

void RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawPrimitive);
	checkSlow( NumPrimitives > 0 );

	DWORD NumIndices = CalcNumElements( PrimitiveType, NumPrimitives );
	check(GPendingNumInstances);
	const UINT MaxInstancesPerBatch = IndexBuffer->NumInstances;
	UINT InstancesToDraw = GPendingNumInstances;
	UINT InstanceIndex = 0;
	DWORD IndexSize = (IndexBuffer->DataType == CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16) ? 2 : 4;
	DWORD IndexOffset = IndexBuffer->MemoryOffset + StartIndex * IndexSize;

	do
	{
		PS3SetVertexOffsets(BaseVertexIndex, InstanceIndex);
		PS3UpdateDirtyStates();

		UINT NumInstancesThisBatch = Min<UINT>(InstancesToDraw,MaxInstancesPerBatch);

		if ( NumPrimitives > 0 )
		{
			// Draw the instance.
			cellGcmSetDrawIndexArray(
				UnrealPrimitiveTypeToType[PrimitiveType],
				NumIndices * NumInstancesThisBatch,
				IndexBuffer->DataType,
				IndexBuffer->MemoryRegion,
				IndexOffset);
		}

		PS3UpdateAutoFence();
		InstanceIndex += NumInstancesThisBatch;
		InstancesToDraw -= NumInstancesThisBatch;
	} while ( InstancesToDraw > 0 );
}

void RHIDrawIndexedPrimitive_PreVertexShaderCulling(
	FIndexBufferRHIParamRef IndexBuffer,
	UINT PrimitiveType,
	INT BaseVertexIndex,
	UINT MinIndex, // smallest vertex index in this primitive list
	UINT NumVertices,
	UINT StartIndex, // the first element of the primitive list that should be rendered
	UINT NumPrimitives,
	const FMatrix& LocalToWorld,
	const void* PlatformMeshData
	)
{
#if RHI_SUPPORTS_PREVERTEXSHADER_CULLING
	// Verify that the position vertex stream is set, is in main memory, and the vertex range specified is valid.
	checkSlow(GPendingPositionStreamIndex != INDEX_NONE);
	const FPS3Stream& PositionVertexStream = GPendingStreams[GPendingPositionStreamIndex];
	checkSlow(PositionVertexStream.VertexBuffer);
	checkSlow(PositionVertexStream.VertexBuffer->MemoryRegion == CELL_GCM_LOCATION_MAIN);

	checkSlow(NumVertices > 0);
	checkSlow(NumPrimitives > 0);

	// Verify that the index and vertex buffers are 16-byte aligned
	{
	}
		
	FPS3StaticMeshData *PS3MeshData = (FPS3StaticMeshData*)PlatformMeshData;
	if (GUseEdgeGeometry && PS3MeshData != NULL && GPendingBackfaceCullingEnable && NumPrimitives > 300)
	{
		checkSlow(GPendingNumInstances == 1);
		checkSlow(PrimitiveType == PT_TriangleList);
		// All EdgeGeom input data must be in main memory (bus speeds
		// from VRAM to SPUs are too slow)
		checkSlow(IndexBuffer->MemoryRegion == CELL_GCM_LOCATION_MAIN);

		// This draw call must start ona 16-byte aligned address in
		// the index and vertex buffers (this should be taken care of
		// by the cooker)
		const UINT IndexStride = (IndexBuffer->DataType == CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16) ? 2 : 4;
		checkSlow(IndexStride == 2);
		UINT IndexBufferAlign = (UINT)((BYTE*)IndexBuffer->Data + StartIndex * IndexStride) & 0xF;
		UINT VertexBufferAlign = (UINT)((BYTE*)PositionVertexStream.VertexBuffer->Data
			+ PositionVertexStream.Stride * (BaseVertexIndex + MinIndex)) & 0xF;
		check(IndexBufferAlign == 0);
		check(VertexBufferAlign == 0);

		// Set the dirty rendering states.
		PS3UpdateDirtyStates();

		// Set up some data that's invariant across all jobs
		static const UINT NumUserQWORDS = (sizeof(FPreVertexShaderCullingParameters) + sizeof(QWORD) - 1)
			/ sizeof(QWORD);
		BYTE* const IndexBufferBase = (BYTE*)IndexBuffer->Data + StartIndex * IndexStride;
		BYTE* const PositionVertexStreamBase = (BYTE*)PositionVertexStream.VertexBuffer->Data
			+ PositionVertexStream.Stride * (BaseVertexIndex + MinIndex);
		
		const FMatrix LocalViewProjectionMatrix = LocalToWorld.ConcatTranslation(GPendingViewTranslation) * GPendingViewProjectionMatrix;

		// Each indexed primitive may contain multiple EdgeGeom partitions
		// each of which will produce its own job and draw call.
		UINT NumPartitions = PS3MeshData->IoBufferSize.Num(); // any of these arrays would do
		check(NumPartitions > 0);
		for(UINT PartitionIndex=0; PartitionIndex < NumPartitions; ++PartitionIndex)
		{
			FSpursManager::FSpursJobSetupContext SpursJobSetup( GPixelshaderPatchingJobStart,
				GPixelshaderPatchingJobSize, NumUserQWORDS, PS3MeshData->ScratchBufferSize(PartitionIndex),
				0 /* NumOutputBytes = 0, since we're using the InOut buffer instead */);
			FPreVertexShaderCullingParameters& JobParameters =
				*(FPreVertexShaderCullingParameters*)SpursJobSetup.GetUserData();
			
			// Reserve space in the GCM command buffer for this job's draw call.
			UINT CommandBufferReserveSize = PS3MeshData->CommandBufferHoleSize(PartitionIndex);
			JobParameters.CommandBufferReserveSize = CommandBufferReserveSize;
			JobParameters.CommandBufferReserve = ReserveCommandBufferSpace(CommandBufferReserveSize, TRUE);
			
			// Enqueue the index buffer load into SPU memory.
			const UINT NumTriangles = PS3MeshData->TriangleCount(PartitionIndex);
			const UINT NumIndices = CalcNumElements( PrimitiveType, NumTriangles );
			check(NumIndices % 24 == 0); // the cooker must align buffers thusly, padding the extra space with degenerate triangles
			const UINT IndicesSize = Align(NumIndices * IndexStride, 16);
			BYTE* const PartitionIndices = IndexBufferBase
				+ PS3MeshData->FirstTriangle(PartitionIndex)*3*IndexStride;
			JobParameters.IndexBufferDMAIndex = SpursJobSetup.AddInputMemory(PartitionIndices,
				IndicesSize);
			
			JobParameters.OutputBufferInfo = (UINT*)GPS3Gcm->GetEdgeGeometryOutputBuffer()->GetOutputBufferInfo();
			
			// Enqueue the position vertex stream load into SPU memory.
			const UINT NumVertices = PS3MeshData->VertexCount(PartitionIndex);
			BYTE* const PartitionPositionVertices = PositionVertexStreamBase
				+ PS3MeshData->FirstVertex(PartitionIndex) * PositionVertexStream.Stride;
			UINT VerticesSize = Align(NumVertices * PositionVertexStream.Stride, 16);
			JobParameters.VertexBufferDMAIndex = SpursJobSetup.AddInputMemory(PartitionPositionVertices,
				VerticesSize);
			
			// Set up the job parameters.
			appMemcpy(JobParameters.LocalViewProjectionMatrix,&LocalViewProjectionMatrix,
				sizeof(FMatrix));
			
			JobParameters.VertexBufferStride = PositionVertexStream.Stride;
			JobParameters.NumTriangles = NumTriangles;
			JobParameters.IndexBias = PS3MeshData->IndexBias(PartitionIndex);
			JobParameters.NumVertices = NumVertices;
			JobParameters.bReverseCulling = (GPendingBackfaceCullingFacing == CELL_GCM_CW);
			JobParameters.BaseParameters.TaskId = 1;
			
			FSpursJobType* Job = SpursJobSetup.GetJob();
			Job->header.sizeScratch   = PS3MeshData->ScratchBufferSize(PartitionIndex);
			Job->header.sizeInOrInOut = PS3MeshData->IoBufferSize(PartitionIndex);

			SpursJobSetup.Finish();
			
			PS3UpdateAutoFence();
		}
	}
	else
#endif
	{
		RHIDrawIndexedPrimitive(IndexBuffer,PrimitiveType,BaseVertexIndex,MinIndex,NumVertices,StartIndex,NumPrimitives);
	}
}

// Kick the rendering commands that are currently queued up in the GPU command buffer.
void RHIKickCommandBuffer()
{
	// This will add a new fence and kick the command buffer.
	GPS3Gcm->InsertFence( TRUE );
}

// Blocks the CPU until the GPU catches up and goes idle.
void RHIBlockUntilGPUIdle()
{
	GPS3Gcm->BlockUntilGPUIdle();
}

/*
 * Returns the total GPU time taken to render the last frame. Same metric as appCycles().
 */
DWORD RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

/*
 * Returns an approximation of the available memory that textures can use, which is video + AGP where applicable, rounded to the nearest MB, in MB.
 */
DWORD RHIGetAvailableTextureMemory()
{
	//not currently implemented for ps3
	return 0;
}

/**
 * Optimizes pixel shaders that are heavily texture fetch bound due to many L2 cache misses.
 * @param PixelShader	The pixel shader to optimize texture fetching for
 */
void RHIReduceTextureCachePenalty( FPixelShaderRHIParamRef PixelShader )
{
	PixelShader->Info->FragmentControl &= 0x00ffffff;
	PixelShader->Info->RegisterCount = 48;
}


#define FLIP_PUMP_DELAY (1.0f / 30.0f)

struct FFlipPumper : public FTickableObjectRenderThread
{
	FFlipPumper()
		: DelayCountdown(FLIP_PUMP_DELAY)
	{
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It is
	 * used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
	 */
	virtual UBOOL IsTickable() const
	{
		return TRUE;
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within UnLevTick.cpp after ticking all actors.
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	void Tick(FLOAT DeltaTime)
	{
		// countdown to a flip
		DelayCountdown -= DeltaTime;
		
		// if we reached the flip time, flip
		if (DelayCountdown <= 0.0f)
		{
			// clear to black
			RHIClear(TRUE, FLinearColor::Black, FALSE, 0.0f, FALSE, 0);

			// and flip
			GPS3Gcm->SwapBuffers();

			// reset timer by adding 1 full delay, but don't allow it to be negative
			DelayCountdown = Max(0.0f, DelayCountdown + FLIP_PUMP_DELAY);
		}
	}

	/** Delay until next flip */
	FLOAT DelayCountdown;
};

FFlipPumper* GPumper = NULL;
void RHISuspendRendering()
{
	if (GPumper == NULL)
	{
		// start the flip pumper
		GPumper = new FFlipPumper;
	}
}

void RHIResumeRendering()
{
	delete GPumper;
	GPumper = NULL;
}

UBOOL RHIIsRenderingSuspended()
{
	return GPumper ? TRUE : FALSE;
}

/**
 * Pump the pumper if the we're in the rendering thread when we need to loop while pumping
 */
void PumpFlipPumper(FLOAT DeltaTime)
{
	if (GPumper)
	{
		GPumper->Tick(DeltaTime);
	}
}


volatile UBOOL GStopPumping = FALSE;
sys_ppu_thread_t GPumperThreadID = 0xFFFFFFFF;

/**
 * Thread used to flip the screen during startup sysutl dialogs (gamedata is full, etc)
 */
void FlipPumperThread(uint64_t Param)
{
	// other, ingame pumper shouldn't be used at the same time
	check(GPumper == NULL);

	// go until told to stop
	while (!GStopPumping)
	{
		// clear the back buffer
		cellGcmSetClearSurface(CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
		// flip the buffer
		GPS3Gcm->SwapBuffers();
		// check for sysutil callbacks
		cellSysutilCheckCallback();

		// wait some time (can't constantly flip)
		appSleep(FLIP_PUMP_DELAY);
	}

	// bye!
	sys_ppu_thread_exit(0);
}

/**
 * Start up a thread that will flip the screen and check for sysutil callbacks
 */
void appPS3StartFlipPumperThread()
{
	GStopPumping = FALSE;

	if (GPumperThreadID != 0xFFFFFFFF)
	{
		printf("ALREADY PUMPING!!\n");
		return;
	}

	// start the flipper thread
	INT Err = sys_ppu_thread_create(&GPumperThreadID, FlipPumperThread, NULL, 1000, 8 * 1024, SYS_PPU_THREAD_CREATE_JOINABLE, "FlipPump");
	checkf(Err == CELL_OK, TEXT("Failed to create install thread, ret = %d"), Err);
}

/**
 * Kill the flipper thread above
 */
void appPS3StopFlipPumperThread()
{
	if (GPumperThreadID == 0xFFFFFFFF)
	{
		printf("TRYING TO KILL PUMPER THAT ISN'T RUNNING!\n");
		return;
	}
	// tell the thread to stop
	GStopPumping = TRUE;
	uint64_t Result;

	// wait until it is complete
	sys_ppu_thread_join(GPumperThreadID, &Result);
	GPumperThreadID = 0xFFFFFFFF;
}

/*=============================================================================
	Functions that are not applicable for PS3.
=============================================================================*/

void RHISetRenderTargetBias( FLOAT ColorBias )
{
}
void RHIBeginHiStencilRecord(UBOOL bCompareFunctionEqual, UINT RefValue)
{
}
void RHIBeginHiStencilPlayback(UBOOL bFlush)
{
}
void RHIEndHiStencil()
{
}
void RHISetShaderRegisterAllocation(UINT NumVertexShaderRegisters, UINT NumPixelShaderRegisters)
{
}
void RHIMSAAInitPrepass()
{
}
void RHIMSAAFixViewport()
{
}
void RHIMSAABeginRendering(UBOOL bRequiresClear)
{
}
void RHIMSAAEndRendering(FTexture2DRHIParamRef DepthTexture, FTexture2DRHIParamRef ColorTexture, UINT ViewIndex)
{
}
void RHIRestoreColorDepth(FTexture2DRHIParamRef ColorTexture, FTexture2DRHIParamRef DepthTexture)
{
}
void RHISetTessellationMode( ETessellationMode TessellationMode, FLOAT MinTessellation, FLOAT MaxTessellation )
{
}
void RHISetLargestExpectedViewportSize( UINT NewLargestExpectedViewportWidth, UINT NewLargestExpectedViewportHeight )
{
}


/**
 *	Retrieve available screen resolutions.
 *
 *	@param	Resolutions			TArray<FScreenResolutionRHI> parameter that will be filled in.
 *	@param	bIgnoreRefreshRate	If TRUE, ignore refresh rates.
 *
 *	@return	UBOOL				TRUE if successfully filled the array
 */
UBOOL RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, UBOOL bIgnoreRefreshRate)
{
	//@todo. Implement PS3 version of this function
	return FALSE;
}

#else	// USE_PS3_RHI

/**
 * Start up a thread that will flip the screen and check for sysutil callbacks
 * This should only be used BEFORE the rendering thread is up and going
 */
void appPS3StartFlipPumperThread()
{
}

/**
 * Kill the flipper thread start with appPS3StartFlipPumperThread
 */
void appPS3StopFlipPumperThread()
{
}

void PumpFlipPumper(FLOAT /**DeltaTime**/)
{
}

#endif // USE_PS3_RHI
