	/*=============================================================================
	UnContentStreaming.cpp: Implementation of content streaming classes.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include <sys/types.h>
#include <sys/sys_time.h>
#include <unistd.h>
#include <sys/process.h>
#include <cell/fs/cell_fs_file_api.h>
#include "EnginePrivate.h"
#include "UnAsyncLoadingPS3.h"
#include "FFileManagerPS3.h"
#include "EdgeZlibWrapper.h"


/*-----------------------------------------------------------------------------
	Helpers.
-----------------------------------------------------------------------------*/


extern void* GRSXBaseAddress;
inline UBOOL IsTransferringToGPU(PTRINT Addr)
{
	return (Addr >= (PTRINT)GRSXBaseAddress && Addr < (PTRINT)GRSXBaseAddress + 256 * 1024 * 1024);
}


/*-----------------------------------------------------------------------------
	Async loading stats.
-----------------------------------------------------------------------------*/

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Platform read time"),STAT_AsyncIO_PlatformReadTime,STATGROUP_AsyncIO);


/*-----------------------------------------------------------------------------
	FAsyncIOSystemPS3 implementation.
-----------------------------------------------------------------------------*/

/**
 * Constructor
 */
FAsyncIOSystemPS3::FAsyncIOSystemPS3()
:	BufferedReader(NULL)
,	BufferedOffset( INDEX_NONE )
,	BufferedData( NULL )
,	CurrentOffset( INDEX_NONE )
{
	// @todo ps3 fios: This is 128Kb, and it is per instance (not per file/reader). Do we
	// need this? Read (or written) data is very likely already in the HDD1
	// cache, and if it was recently read, it's probably in the hard drive's
	// memory (on-disk) cache, it should be really fast to read that. But if
	// we have a bunch of small reads, using buffered data is better.
	BufferedData = (BYTE*) appMalloc( DVD_MIN_READ_SIZE );
}


/**
 * Destructor
 */
FAsyncIOSystemPS3::~FAsyncIOSystemPS3()
{
	appFree( BufferedData );
	BufferedData = NULL;
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
UBOOL FAsyncIOSystemPS3::PlatformReadDoNotCallDirectly(FAsyncIOHandle FileHandle, INT Offset, INT Size, void* Dest)
{
	// handle streaming to GPU
	if (IsTransferringToGPU((PTRINT)Dest))
	{
		struct FReadBuffer
		{
			void* Buffer;
			INT Size;
			INT Offset;
			INT DMAIndex;
		};

		// allocate double buffers
		const int READ_SIZE = (32 * 1024);
		FReadBuffer Buffers[2];
		Buffers[0].Buffer = appMalloc(READ_SIZE);
		Buffers[1].Buffer = appMalloc(READ_SIZE);
		INT CurrentBuffer = 0;

		// Initial read request.
		Buffers[CurrentBuffer].Size = Min(READ_SIZE, Size);
		Buffers[CurrentBuffer].Offset = 0;
		// We call PlatformRead directly as it's recursive in this case.
		PlatformReadDoNotCallDirectly(FileHandle, Offset, Buffers[CurrentBuffer].Size, Buffers[CurrentBuffer].Buffer);
		INT DataOffset = Buffers[CurrentBuffer].Size;

		// mark that the other buffer hasn't been used (to skip syncing below)
		Buffers[1 - CurrentBuffer].DMAIndex = -1;

		// Loop till we're done reading all data
		UBOOL bHasProcessedAllData = FALSE;
		while (!bHasProcessedAllData)
		{
			// wait for previous DMA to finish
			INT PrevDMAIndex = Buffers[1 - CurrentBuffer].DMAIndex;
			if (PrevDMAIndex != -1)
			{
				// wait for previous to finish
				edge_zlib_wait(PrevDMAIndex);
			}

			// kick off the task
			Buffers[CurrentBuffer].DMAIndex = 
				edge_zlib_memcpy((BYTE*)Buffers[CurrentBuffer].Buffer, 
					(BYTE*)Dest + Buffers[CurrentBuffer].Offset, 
					Buffers[CurrentBuffer].Size);
			check(Buffers[CurrentBuffer].DMAIndex >= 0);

			// have we sent all data over DMA?
			bHasProcessedAllData = DataOffset >= Size;

			// if not, then read some more in!
			if (!bHasProcessedAllData)
			{
				// swap buffers
				CurrentBuffer = 1 - CurrentBuffer;
				// calculate size for next read
				Buffers[CurrentBuffer].Size = Min(READ_SIZE, Size - DataOffset);
				Buffers[CurrentBuffer].Offset = DataOffset;
				// read next chunk -- we call PlatformRead directly as it's recursive in this case.
				PlatformReadDoNotCallDirectly(FileHandle, Offset + DataOffset, Buffers[CurrentBuffer].Size, Buffers[CurrentBuffer].Buffer);
				// update location in destinatio
				DataOffset += Buffers[CurrentBuffer].Size;
			}
		}

		// sync on final task(which is still in CurrentBuffer, since we didn't wswap buffers on final loop)
		INT PrevDMAIndex = Buffers[CurrentBuffer].DMAIndex;
		if (PrevDMAIndex != -1)
		{
			// wait
			edge_zlib_wait(PrevDMAIndex);
		}

		appFree(Buffers[0].Buffer);
		appFree(Buffers[1].Buffer);
	}
	else
	{
		// cast it appropriately
		PS3Fd Reader = (PS3Fd)FileHandle.Handle;

		// Continue reading from file at current offset.
		if( Offset == INDEX_NONE )
		{
			// Make sure that the file handle hasn't changed if we're not specifying an offset.
			checkf( BufferedReader == Reader, TEXT("BufferedReader = %x Reader=%x"), BufferedReader, Reader );
			check( CurrentOffset != INDEX_NONE );
		}
		// Request a seek.
		else
		{
			// Invalidate cached data if we changed files.
			if ( BufferedReader != Reader )
			{
				BufferedOffset = INDEX_NONE;
				BufferedReader = Reader;
			}
			CurrentOffset	= Offset;
		}

		/** Local version of dest buffer that is being incremented when read into.					*/
		BYTE* DestBuffer = (BYTE*) Dest;
		/**	Remaining size to read into destination buffer.											*/
		INT	RemainingSize = Size;

		// Check whether data is already cached and fulfill read via cache.
		if( BufferedOffset != INDEX_NONE
		&&	(CurrentOffset >= BufferedOffset) 
		&&  (CurrentOffset < BufferedOffset + DVD_MIN_READ_SIZE) )
		{
			// Copy cached data to destination and move offsets.
			INT BytesToCopy = Min( BufferedOffset + DVD_MIN_READ_SIZE - CurrentOffset, Size );
			appMemcpy( DestBuffer, BufferedData + CurrentOffset - BufferedOffset, BytesToCopy );
			RemainingSize	-= BytesToCopy;
			DestBuffer		+= BytesToCopy;

			// Advance 'logical' offset in file.
			CurrentOffset	+= BytesToCopy;
		}
		
		// Fulfill any remaining requests from disk.
		if( RemainingSize > 0 )
		{
			/** Offset in file aligned on ECC boundary. Will be less than Offset in most cases.			*/
			INT		AlignedOffset		= Align( CurrentOffset - DVD_ECC_BLOCK_SIZE + 1, DVD_ECC_BLOCK_SIZE);
			/** Number of bytes padded at beginning for ECC alignment.									*/
			INT		AlignmentPadding	= CurrentOffset - AlignedOffset;

			// Advance 'logical' offset in file.
			CurrentOffset += RemainingSize;

			// Seek to aligned offset.
			PS3CheckedFsLseek(Reader, AlignedOffset, cell::fios::kSEEK_SET, NULL);
			BufferedOffset = AlignedOffset;

			// Read initial DVD_MIN_READ_SIZE bytes.
			{
				SCOPED_FILE_IO_ASYNC_READ_STATS(FileHandle.StatsHandle,DVD_MIN_READ_SIZE,BufferedOffset);
				PS3CheckedFsRead(Reader, BufferedData, DVD_MIN_READ_SIZE, NULL);
			}

			// Copy read data to destination, skipping alignment data at the beginning.
			INT BytesToCopy = Min( RemainingSize, DVD_MIN_READ_SIZE - AlignmentPadding );
			appMemcpy( DestBuffer, BufferedData + AlignmentPadding, BytesToCopy );
			DestBuffer		+= BytesToCopy;
			RemainingSize	-= BytesToCopy;

			// Read and copy remaining data.
			while( RemainingSize > 0 )
			{
				BufferedOffset	+= DVD_MIN_READ_SIZE;
				{
					SCOPED_FILE_IO_ASYNC_READ_STATS(FileHandle.StatsHandle,DVD_MIN_READ_SIZE,BufferedOffset);
					PS3CheckedFsRead(Reader, BufferedData, DVD_MIN_READ_SIZE, NULL);
				}
		
				INT BytesToCopy = Min( RemainingSize, DVD_MIN_READ_SIZE );
				appMemcpy( DestBuffer, BufferedData, BytesToCopy );
				RemainingSize	-= DVD_MIN_READ_SIZE;
				DestBuffer		+= DVD_MIN_READ_SIZE;
			}
		}
	}
	
	// Failure is handled internally so we managed to fulfill the request if we made it here.
	return TRUE;
}

/** 
 * Creates a file handle for the passed in file name
 *
 * @param	FileName	Pathname to file
 *
 * @return	INVALID_HANDLE if failure, handle on success
 */
FAsyncIOHandle FAsyncIOSystemPS3::PlatformCreateHandle(const TCHAR* Filename)
{
	FAsyncIOHandle FileHandle;

	FileHandle.StatsHandle	= FILE_IO_STATS_GET_HANDLE( Filename );
	SCOPED_FILE_IO_READ_OPEN_STATS( FileHandle.StatsHandle );

	// create a wrapper around the file handle and a cache mechanism
	ANSICHAR PlatformFilename[PS3_MAX_PLATFORM_FILENAME];
	GetPlatformFilename(Filename, PlatformFilename);
	PS3Fd Reader = PS3CheckedFsOpen(PlatformFilename, cell::fios::kO_RDONLY);

	// store the handle in the platform independent Handle
	FileHandle.Handle		= Reader;

	return FileHandle;
}

/**
 * Closes passed in file handle.
 */
void FAsyncIOSystemPS3::PlatformDestroyHandle(FAsyncIOHandle FileHandle)
{
	FILE_IO_STATS_CLOSE_HANDLE(FileHandle.StatsHandle);

	PS3Fd Reader = (PS3Fd)FileHandle.Handle;
	if (Reader)
	{
		GScheduler->closeFileSync(NULL, Reader);
	}

	// close the file if present
	FileHandle.Handle = NULL;

	// reset the buffered cache, since we can close and open and get back the same file handle:
	BufferedReader = NULL;
}

/**
 * Returns whether the passed in handle is valid or not.
 *
 * @param	FileHandle	File hande to check validity
 *
 * @return	TRUE if file handle is valid, FALSE otherwise
 */
UBOOL FAsyncIOSystemPS3::PlatformIsHandleValid(FAsyncIOHandle FileHandle)
{
	return FileHandle.Handle != NULL;
}

/**
 * Let the platform handle being done with the file
 *
 * @param Filename File that was being async loaded from, but no longer is
 */
void FAsyncIOSystemPS3::PlatformHandleHintDoneWithFile(const FString& Filename)
{
	// queue up a destroy handle requst
	QueueDestroyHandleRequest(Filename);
}

