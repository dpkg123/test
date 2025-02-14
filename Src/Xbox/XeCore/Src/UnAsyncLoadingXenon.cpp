/*=============================================================================
	UnAsyncLoading.cpp: Unreal async loading code, Xbox 360 implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "FTableOfContents.h"
#include "FFileManagerGeneric.h"


/*-----------------------------------------------------------------------------
	Async loading stats.
-----------------------------------------------------------------------------*/

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Platform read time"),STAT_AsyncIO_PlatformReadTime,STATGROUP_AsyncIO);


/*-----------------------------------------------------------------------------
	FAsyncIOSystemXenon implementation.
-----------------------------------------------------------------------------*/

/** Constructor, initializing member variables and allocating buffer */
FAsyncIOSystemXenon::FAsyncIOSystemXenon()
:	LastSortKey( INDEX_NONE )
,	LastOffset( INDEX_NONE )
{	
}

/** Virtual destructor, freeing allocated memory. */
FAsyncIOSystemXenon::~FAsyncIOSystemXenon()
{
}

/** 
 * Reads passed in number of bytes from passed in file handle.
 *
 * @param	FileHandle	Handle of file to read from. 
 * @param	Offset		Offset in bytes from start, INDEX_NONE if file pointer shouldn't be changed
 * @param	Size		Size in bytes to read at current position from passed in file handle
 * @param	Dest		Pointer to data to read into
 *
 * @return	TRUE if read was successful, FALSE otherwise
 */
UBOOL FAsyncIOSystemXenon::PlatformReadDoNotCallDirectly( FAsyncIOHandle FileHandle, INT Offset, INT Size, void* Dest )
{
	DWORD BytesRead		= 0;
	UBOOL bSeekFailed	= FALSE;
	UBOOL bReadFailed	= FALSE;
	{
		SCOPED_FILE_IO_ASYNC_READ_STATS(FileHandle.StatsHandle,Size,Offset);
		if( Offset != INDEX_NONE )
		{
			bSeekFailed = SetFilePointer( FileHandle.Handle, Offset, NULL, FILE_BEGIN ) == INVALID_SET_FILE_POINTER;
			if( bSeekFailed )
			{
				appHandleIOFailure( NULL );
			}
		}
		if( !bSeekFailed )
		{
			bReadFailed = ReadFile( FileHandle.Handle, Dest, Size, &BytesRead, NULL ) == 0;
			if( bReadFailed )
			{
				appHandleIOFailure( NULL );
			}
		}
	}
	return BytesRead == Size;
}

/** 
 * Creates a file handle for the passed in file name
 *
 * @param	FileName	Pathname to file
 *
 * @return	INVALID_HANDLE if failure, handle on success
 */
FAsyncIOHandle FAsyncIOSystemXenon::PlatformCreateHandle( const TCHAR* FileName )
{
	FAsyncIOHandle FileHandle;

	FFilename CookedFileName	= FileName;
	CookedFileName				= CookedFileName.GetPath() + TEXT("\\") + CookedFileName.GetBaseFilename() + TEXT(".xxx");

	FileHandle.StatsHandle	= FILE_IO_STATS_GET_HANDLE( *CookedFileName );
	SCOPED_FILE_IO_READ_OPEN_STATS( FileHandle.StatsHandle );

	UBOOL bSuccess = TRUE;

	FileHandle.Handle	= CreateFileA( 
								TCHAR_TO_ANSI(*GFileManager->GetPlatformFilepath(*CookedFileName)), 
								GENERIC_READ, 
								FILE_SHARE_READ, 
								NULL, 
								OPEN_EXISTING, 
								FILE_ATTRIBUTE_NORMAL,
								NULL );

	// if that failed to be found, attempt the direct filename instead
	if( FileHandle.Handle == INVALID_HANDLE_VALUE )
	{
		FileHandle.Handle = CreateFileA( 
								TCHAR_TO_ANSI(*GFileManager->GetPlatformFilepath(FileName)), 
								GENERIC_READ, 
								FILE_SHARE_READ, 
								NULL, 
								OPEN_EXISTING, 
								FILE_ATTRIBUTE_NORMAL,
								NULL );
	}
	bSuccess = (FileHandle.Handle != INVALID_HANDLE_VALUE);

	if( !bSuccess )
	{
		appErrorf(TEXT("Failed to open file '%s' for async IO operation."), FileName);
		appHandleIOFailure( FileName );
	}

	if (bSuccess)
	{
		FileHandle.PlatformSortKey = XGetFilePhysicalSortKey( FileHandle.Handle );
	}
	else
	{
		FileHandle.PlatformSortKey = INDEX_NONE;
	}

	return FileHandle;
}

/**
 * Closes passed in file handle.
 */
void FAsyncIOSystemXenon::PlatformDestroyHandle( FAsyncIOHandle FileHandle )
{
	FILE_IO_STATS_CLOSE_HANDLE( FileHandle.StatsHandle );
	CloseHandle( FileHandle.Handle );
}

/**
 * Returns whether the passed in handle is valid or not.
 *
 * @param	FileHandle	File hande to check validity
 *
 * @return	TRUE if file handle is valid, FALSE otherwise
 */
UBOOL FAsyncIOSystemXenon::PlatformIsHandleValid( FAsyncIOHandle FileHandle )
{
	return FileHandle.Handle != INVALID_HANDLE_VALUE;
}

/**
 * Determines the next request index to be fulfilled by taking into account previous and next read
 * requests and ordering them to avoid seeking.
 *
 * This function is being called while there is a scope lock on the critical section so it
 * needs to be fast in order to not block QueueIORequest and the likes.
 *
 * @return	index of next to be fulfilled request or INDEX_NONE if there is none
 */
INT FAsyncIOSystemXenon::PlatformGetNextRequestIndex()
{
	//
	// Calling code already entered critical section so we can access OutstandingRequests.
	//

	// Keep track of index of FWD seek request with least cost from current position.
	INT		BestFWDSeekRequestIndex		= INDEX_NONE;
	SQWORD	BestFWDSeekRequestCSK		= LLONG_MAX;
	
	// Keep track of index of seek request with least cost from start.
	INT		BestStartSeekRequestIndex	= INDEX_NONE;
	SQWORD	BestStartSeekRequestCSK		= LLONG_MAX;
	
	// Keep track of index of first unknown sort key. Use FIFO for those.
	INT		FirstUnknownRequestIndex	= INDEX_NONE;

	// Cache combined sort key. Negative values are okay as it means we want to start seeking
	// from the very beginning and best start and best FWD seek request should be identical.
	SQWORD	LastCombinedSortKey			= LastSortKey * INT_MAX + LastOffset;

	// Keep track of highest encountered priority.
	EAsyncIOPriority HighestPriority = MinPriority;

	// Iterate over all requests and try to populate sort keys.
	for( INT CurrentRequestIndex=0; CurrentRequestIndex<OutstandingRequests.Num(); CurrentRequestIndex++ )
	{
		// Modifies the array to avoid retrieving sort key multiple times.
		FAsyncIORequest& IORequest = OutstandingRequests(CurrentRequestIndex);

		// Try to retrieve sort key associated with requested file.
		if( IORequest.FileSortKey == INDEX_NONE )
		{				
			// We might not have create a file handle for this request yet. The below is not
			// blocking as it doesn't try to create a handle if one is not found.
			FAsyncIOHandle* HandlePtr = FindCachedFileHandle( IORequest.FileName );
			if( HandlePtr )
			{
				IORequest.FileSortKey = HandlePtr->PlatformSortKey;
			}
		}

		// Highest IO priority always wins.
		if( IORequest.Priority > HighestPriority )
		{
			// Keep track of higher priority.
			HighestPriority				= IORequest.Priority;

			// Reset tracked data as we have a higher priority read request.
			BestFWDSeekRequestIndex		= INDEX_NONE;
			BestFWDSeekRequestCSK		= LLONG_MAX;
			BestStartSeekRequestIndex	= INDEX_NONE;
			BestStartSeekRequestCSK		= LLONG_MAX;
			FirstUnknownRequestIndex	= INDEX_NONE;
		}			
		// Skip over requests that are below highest encountered or min priority.
		else if( IORequest.Priority < HighestPriority )
		{
			continue;
		}

		// Unknown sort key.
		if( IORequest.FileSortKey == INDEX_NONE )
		{
			// Keep track of first unknown request index as we fulfill them in FIFO order.
			if( FirstUnknownRequestIndex == INDEX_NONE )
			{
				FirstUnknownRequestIndex = CurrentRequestIndex;
			}
		}
		// Known sort key.
		else
		{
			SQWORD CombinedSortKey = IORequest.FileSortKey * INT_MAX + IORequest.Offset;

			if( CombinedSortKey < BestStartSeekRequestCSK )
			{
				// Found a better minimal seek from beginning. Note that this is a valid seek
				// from start as we rule out unknown sort keys above.
				BestStartSeekRequestIndex	= CurrentRequestIndex;
				BestStartSeekRequestCSK		= CombinedSortKey;
			}

			if( CombinedSortKey >= LastCombinedSortKey
			&&	CombinedSortKey < BestFWDSeekRequestCSK )
			{
				// Found a better FWD seek.
				BestFWDSeekRequestIndex = CurrentRequestIndex;
				BestFWDSeekRequestCSK	= CombinedSortKey;
			}
		}
	}

	// Prefer requests in FWD seek, start seek, unknown seek order.
	
	// Best FWD seek.
	INT BestRequestIndex = BestFWDSeekRequestIndex;

	// Best seek from beginning of DVD.
	if( BestRequestIndex == INDEX_NONE )
	{
		BestRequestIndex = BestStartSeekRequestIndex;
	}

	// Unknown seek, use FIFO order.
	if( BestRequestIndex == INDEX_NONE )
	{
		BestRequestIndex = FirstUnknownRequestIndex;
	}
	
	// Update info for subsequent requests.
	if( BestRequestIndex != INDEX_NONE )
	{
		LastSortKey = OutstandingRequests(BestRequestIndex).FileSortKey;
		LastOffset	= OutstandingRequests(BestRequestIndex).Offset;
	}

	return BestRequestIndex;
}

/*----------------------------------------------------------------------------
	End.
----------------------------------------------------------------------------*/

