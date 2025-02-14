/*=============================================================================
	PS3RHIQuery.cpp: PS3 query RHI implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3RHIPrivate.h"

#if USE_PS3_RHI

/** Number of occlusion query result buckets to allocate at startup. */
#define MIN_OCCLUSIONBUCKETS (1200/MAX_OCCLUSIONQUERIES)

/** Turning this on will give a log warning whenever an additional bucket is allocated. */
#define DEBUG_OCCLUSIONQUERIES 0


/** This struct must be allocated using GPS3Gcm->GetHostAllocator()->Malloc(). */
struct FPS3OcclusionBucket
{
	FPS3OcclusionBucket()
	{
		Init();
	}

	void Init()
	{
		NumQueries = 0;
		Fence = 0;
		bLocked = FALSE;
		INT ReturnCode = cellGcmAddressToOffset(this, (UINT*)&GPUAddress);
		check( ReturnCode == CELL_OK );
		for ( INT QueryIndex=0; QueryIndex < MAX_OCCLUSIONQUERIES; ++QueryIndex )
		{
			Queries[ QueryIndex ] = NULL;
		}
	}

	/** The GPU will copy the results from local memory to this array (in host memory). */
	CellGcmReportData Results[MAX_OCCLUSIONQUERIES];

	/** Keeping track of which result belongs to which query. Corresponds one-to-one to Results[] array. */
	FPS3RHIOcclusionQuery* Queries[MAX_OCCLUSIONQUERIES];

	/** The number of active queries in this bucket. */
	DWORD NumQueries;

	/** The results are ready for use when the GPU has passed this fence. */
	DWORD Fence;

	/** Whether we can add queries to this bucket. */
	UBOOL bLocked;

	/** The address that the GPU uses to access this struct (aka "offset"). */
	DWORD GPUAddress;
};


class FPS3OcclusionManager
{
public:
	FPS3OcclusionManager()
	:	CurrentBucket( NULL )
	,	CurrentBucketIndex( -1 )
	{
	}

	inline void		Startup();
	inline void		Shutdown();

	inline void		BeginQuery( FOcclusionQueryRHIParamRef Query );
	inline void		EndQuery( FOcclusionQueryRHIParamRef Query );
	inline void		ResetQuery( FOcclusionQueryRHIParamRef Query );
	inline UBOOL	GetQueryResult(FOcclusionQueryRHIParamRef Query, DWORD& OutNumPixels, UBOOL bWait);
	inline void		Flush();

	DWORD			GetNumActiveQueries();

protected:
	void			AllocNewEntry( FOcclusionQueryRHIParamRef );
	void			FreeBucket( FPS3OcclusionBucket* Bucket );
	inline void		FlushBucket( FPS3OcclusionBucket* Bucket );

	TArray<FPS3OcclusionBucket*>	Buckets;
	FPS3OcclusionBucket*			CurrentBucket;
	INT								CurrentBucketIndex;
};

FPS3OcclusionManager GPS3OcclusionManager;

inline void FPS3OcclusionManager::Startup( )
{
	Buckets.Reserve( MIN_OCCLUSIONBUCKETS );
	for ( INT BucketIndex=0; BucketIndex < MIN_OCCLUSIONBUCKETS; ++BucketIndex )
	{
		FPS3OcclusionBucket* Bucket = (FPS3OcclusionBucket*) GPS3Gcm->GetHostAllocator()->Malloc( sizeof(FPS3OcclusionBucket), AT_OcclusionQueries, 16 );
		Bucket->Init();
		Buckets.AddItem( Bucket );
	}
	CurrentBucket = Buckets( 0 );
	CurrentBucketIndex = 0;
}

inline void FPS3OcclusionManager::Shutdown( )
{
	GPS3Gcm->BlockUntilGPUIdle();
	for ( INT BucketIndex=0; BucketIndex < Buckets.Num(); ++BucketIndex )
	{
		FPS3OcclusionBucket* Bucket = Buckets( BucketIndex );
		GPS3Gcm->GetHostAllocator()->Free( Bucket );
	}
	Buckets.Empty();
	CurrentBucket = NULL;
	CurrentBucketIndex = -1;
}

inline void FPS3OcclusionManager::Flush()
{
	FlushBucket( CurrentBucket );

	// Free unused additional buckets (while keeping at least MIN_OCCLUSIONBUCKETS in memory)
	for ( INT BucketIndex=MIN_OCCLUSIONBUCKETS; BucketIndex < Buckets.Num(); ++BucketIndex )
	{
		FPS3OcclusionBucket* Bucket = Buckets( BucketIndex );
		if ( !Bucket->bLocked && Bucket->NumQueries == 0 )
		{
			GPS3Gcm->GetHostAllocator()->Free( Bucket );
			Buckets.Remove( BucketIndex-- );
		}
	}

	// Start over from the first bucket (making sure we use the first MIN_OCCLUSIONBUCKETS buckets first)
	CurrentBucket = Buckets( 0 );
	CurrentBucketIndex = 0;
}

DWORD FPS3OcclusionManager::GetNumActiveQueries()
{
	DWORD NumQueries = 0;
	for ( INT BucketIndex=0; BucketIndex < Buckets.Num(); ++BucketIndex )
	{
		FPS3OcclusionBucket* Bucket = Buckets( BucketIndex );
		NumQueries += Bucket->NumQueries;
	}
	return NumQueries;
}

inline void FPS3OcclusionManager::BeginQuery( FOcclusionQueryRHIParamRef Query )
{
	// We obviously don't need the old result anymore, so reset it.
	ResetQuery( Query );

	// Clear the current pixel count
	cellGcmSetClearReport(CELL_GCM_ZPASS_PIXEL_CNT);

	// turn on pixel counting
	cellGcmSetZpassPixelCountEnable(CELL_GCM_TRUE);
}

inline void FPS3OcclusionManager::EndQuery( FOcclusionQueryRHIParamRef Query )
{
	// Get a new bucket entry for this query.
	AllocNewEntry( Query );

	// Turn off pixel counting and write out the pixel count to local memory
	cellGcmSetZpassPixelCountEnable(CELL_GCM_FALSE);
	cellGcmSetReport(CELL_GCM_ZPASS_PIXEL_CNT, Query->Index);

	// Flush the bucket?
	if ( Query->Bucket->NumQueries == MAX_OCCLUSIONQUERIES )
	{
		FlushBucket( Query->Bucket );
	}
}

void FPS3OcclusionManager::FreeBucket( FPS3OcclusionBucket* Bucket )
{
	// Move the results over to the query objects so we can re-use this bucket.
	for ( INT QueryIndex=0; QueryIndex < Bucket->NumQueries; ++QueryIndex )
	{
		FPS3RHIOcclusionQuery* Query = Bucket->Queries[ QueryIndex ];
		if ( Query )
		{
			Query->Bucket		= NULL;
			Query->Index		= 0;
			Query->NumPixels	= Bucket->Results[ QueryIndex ].value;
			Bucket->Queries[ QueryIndex ] = NULL;
		}
	}
	Bucket->NumQueries	= 0;
	Bucket->bLocked		= FALSE;
}

void FPS3OcclusionManager::AllocNewEntry( FOcclusionQueryRHIParamRef Query )
{
	// Find an empty bucket
	INT BucketIndex = CurrentBucketIndex;
	FPS3OcclusionBucket* Bucket = CurrentBucket;
	while ( Bucket->bLocked || Bucket->NumQueries == MAX_OCCLUSIONQUERIES )
	{
		BucketIndex = (BucketIndex + 1) % Buckets.Num();
		Bucket = Buckets(BucketIndex);

		// Is the GPU done with the bucket?
		if ( GPS3Gcm->IsFencePending( Bucket->Fence ) == FALSE )
		{
			FreeBucket( Bucket );
			break;
		}

		// Did we loop around without finding any available bucket?
		if ( BucketIndex == CurrentBucketIndex )
		{
			// Create a new bucket
			Bucket = (FPS3OcclusionBucket*) GPS3Gcm->GetHostAllocator()->Malloc( sizeof(FPS3OcclusionBucket), AT_OcclusionQueries, 16 );
			Bucket->Init();
			BucketIndex = Buckets.AddItem( Bucket );
			break;
		}
	}

	// Set the new working bucket
	CurrentBucket		= Bucket;
	CurrentBucketIndex	= BucketIndex;

	// Setup the query
	Query->Bucket	 = Bucket;
	Query->Index	 = Bucket->NumQueries++;
	Query->NumPixels = 0;

	// Let the bucket keep track of this query
	Bucket->Queries[ Query->Index ] = Query;
}

inline UBOOL FPS3OcclusionManager::GetQueryResult(FOcclusionQueryRHIParamRef Query, DWORD& OutNumPixels, UBOOL bWait)
{
	FPS3OcclusionBucket* Bucket = Query->Bucket;
	if ( Bucket == NULL )
	{
		OutNumPixels = Query->NumPixels;
		return TRUE;
	}

	if ( bWait )
	{
		if ( GPS3Gcm->IsFencePending( Bucket->Fence ) )
		{
			SCOPE_CYCLE_COUNTER( STAT_OcclusionResultTime );
			GPS3Gcm->BlockOnFence( Bucket->Fence );			// Block until the GPU is done with the bucket
		}
		FreeBucket( Bucket );								// Free the bucket and update all its queries
		OutNumPixels = Query->NumPixels;
		return TRUE;
	}
	else
	{
		if ( !GPS3Gcm->IsFencePending( Bucket->Fence ) )	// Check if the GPU is finished with the bucket
		{
			FreeBucket( Bucket );							// Free the bucket and update all its queries
			OutNumPixels = Query->NumPixels;
			return TRUE;
		}
	}

	return FALSE;
}

inline void FPS3OcclusionManager::ResetQuery( FOcclusionQueryRHIParamRef Query )
{
	if ( Query && Query->Bucket != NULL )
	{
		FPS3OcclusionBucket* Bucket = Query->Bucket;
		Bucket->Queries[ Query->Index ] = NULL;
		Query->Bucket = NULL;
		Query->Index = 0;
		Query->NumPixels = 0;
	}
}

inline void FPS3OcclusionManager::FlushBucket( FPS3OcclusionBucket* Bucket )
{
	if ( Bucket->NumQueries > 0 && !Bucket->bLocked )
	{
		Bucket->bLocked = TRUE;
		cellGcmSetTransferReportData( Bucket->GPUAddress, 0, Bucket->NumQueries );
		Bucket->Fence = GPS3Gcm->GetCurrentFence() + 1;	// NOTE: Could use InsertFence here also
	}
}


FPS3RHIOcclusionQuery::FPS3RHIOcclusionQuery()
:	Bucket( NULL )
,	Index( 0 )
,	NumPixels( 0 )
{
}

FPS3RHIOcclusionQuery::~FPS3RHIOcclusionQuery()
{
	GPS3OcclusionManager.ResetQuery(this);
}



FOcclusionQueryRHIRef RHICreateOcclusionQuery()
{
	return new FPS3RHIOcclusionQuery;
}
void RHIBeginOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQuery)
{
	GPS3OcclusionManager.BeginQuery(OcclusionQuery);
}
void RHIEndOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQuery)
{
	GPS3OcclusionManager.EndQuery(OcclusionQuery);
}
void RHIResetOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQuery)
{
	GPS3OcclusionManager.ResetQuery(OcclusionQuery);
}
UBOOL RHIGetOcclusionQueryResult(FOcclusionQueryRHIParamRef OcclusionQuery,DWORD& OutNumPixels,UBOOL bWait)
{
	return GPS3OcclusionManager.GetQueryResult(OcclusionQuery, OutNumPixels, bWait );
}
void PS3StartupOcclusionManager()
{
	GPS3OcclusionManager.Startup();
}
void PS3ShutdownOcclusionManager()
{
	GPS3OcclusionManager.Shutdown();
}

void PS3FlushOcclusionManager()
{
	extern UBOOL GIsShutdownAlready;
	if (GIsShutdownAlready)
	{
		return;
	}

	GPS3OcclusionManager.Flush();
//	DWORD NumQueries = GPS3OcclusionManager.GetNumActiveQueries();
}


#endif // USE_PS3_RHI
