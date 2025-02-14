/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <cell/spurs.h>
#include <cell/atomic.h>
#include <stdio.h>
#include <pthread.h>

#include "../zlib-segcomp-sample/ppu/zlib_segs_ppu.h"
#include "../zlib-segcomp-sample/common/segcompfile.h"
#include "streaming.h"
#include "ppu2ppu_queue.h"

//////////////////////////////////////////////////////////////////////////

enum
{
    kStreamingQueueCommand_shutDown,
    kStreamingQueueCommand_decompressInPlace,   // NOTE: this command supported in zlib-streaming-sample
    kStreamingQueueCommand_decompressBufferInput// NOTE: this command supported in zlib-localstore-streaming-sample
};

//////////////////////////////////////////////////////////////////////////

void* streamingThread( void* pVoid );
void decompressInPlace( StreamingQueueEntry* pStreamingQueueEntry );

//////////////////////////////////////////////////////////////////////////

// Set streamingData data.
// Initialize streamingQueue and the streaming thread.
// Return streaming thread ID.
void initStreaming(	StreamingData/*NOT ON STACK!*/*		pStreamingData,
					CellSpurs*							pSpurs,
					EdgeZlibInflateQHandle				inflateQueueHandle,
					uint32_t							numStreamingQueueEntries )
{
	pStreamingData->m_pSpurs				= pSpurs;
	pStreamingData->m_inflateQueueHandle	= inflateQueueHandle;


	//////////////////////////////////////////////////////////////////////////
	//
	// Initialize the streamingQueue which buffers requests for multiple files
	// to decompress.
	//
	//////////////////////////////////////////////////////////////////////////

    // Allocate a queue object
	_Ppu2PpuQueue* pStreamingQueue = (_Ppu2PpuQueue*)memalign( _PPU_2_PPU_QUEUE_ALIGN, sizeof(_Ppu2PpuQueue) );
    assert( pStreamingQueue != NULL );
	pStreamingData->m_pStreamingQueue = pStreamingQueue;

    // Init queue
	int ret = _ppu2PpuQueueInitialize(	pStreamingQueue,
										sizeof(StreamingQueueEntry),
										numStreamingQueueEntries );
    assert( CELL_OK == ret );


	//////////////////////////////////////////////////////////////////////////
	//
	// Create the streaming thread to decompress multiple files.
	//
	//////////////////////////////////////////////////////////////////////////

	// Create the thread, which starts executing
	ret = pthread_create(	&pStreamingData->m_streamingThreadId,
							NULL/*default attributes*/,
							streamingThread,
							pStreamingData );
	assert( CELL_OK == ret );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Initialize event flag(s)
	//
	//////////////////////////////////////////////////////////////////////////

    CellSpursEventFlag/*NOT ON STACK!*/*pEventFlag = &pStreamingData->m_eventFlag;

	ret = cellSpursEventFlagInitializeIWL(	pSpurs,
	    								    pEventFlag,
										    CELL_SPURS_EVENT_FLAG_CLEAR_AUTO,
										    CELL_SPURS_EVENT_FLAG_SPU2PPU );
	assert( CELL_OK == ret );
	ret = cellSpursEventFlagAttachLv2EventQueue( pEventFlag );
	assert( CELL_OK == ret );
}

//////////////////////////////////////////////////////////////////////////

// When all decompressions are done, shut down streaming thread and queue
void shutDownStreaming( StreamingData/*NOT ON STACK!*/* pStreamingData )
{
	//////////////////////////////////////////////////////////////////////////
	//
	// Shut down the streaming thread (after all decompressions are done).
	//
	//////////////////////////////////////////////////////////////////////////

    // Send shutDown command to streaming thread
    // It will only execute this after all previous commands have been completed
	_Ppu2PpuQueue* pStreamingQueue = pStreamingData->m_pStreamingQueue;
    StreamingQueueEntry streamingQueueEntry;
    streamingQueueEntry.m_streamingQueueCommand = kStreamingQueueCommand_shutDown;
    // Note: we do not have to set other streamingQueueEntry data with the shut down command
	int ret = _ppu2PpuQueuePush( pStreamingQueue, &streamingQueueEntry, NULL );
    assert( CELL_OK == ret );

	// Wait for streaming thread to exit (other threads execute while we wait)
	// Note: The streaming thread will not return data
	ret = pthread_join( pStreamingData->m_streamingThreadId, NULL );
	assert( CELL_OK == ret );


	//////////////////////////////////////////////////////////////////////////
	//
	// Shut down the streaming queue.
	//
	//////////////////////////////////////////////////////////////////////////

    // free queue buffer and shut down queue
	ret = _ppu2PpuQueueShutDown( pStreamingQueue );

    // Free queue object
    free( pStreamingQueue );


	//////////////////////////////////////////////////////////////////////////
	//
	// Shutdown event flag(s)
	//
	//////////////////////////////////////////////////////////////////////////

    CellSpursEventFlag/*NOT ON STACK!*/*pEventFlag = &pStreamingData->m_eventFlag;
	ret = cellSpursEventFlagDetachLv2EventQueue( pEventFlag );
	assert( CELL_OK == ret );


    // For safety in future, clear structure
	memset( pStreamingData, 0, sizeof(StreamingData) );
}

//////////////////////////////////////////////////////////////////////////

// Start streaming decompression in place
void startStreamingDecompressInPlace(	StreamingData/*NOT ON STACK!*/*		pStreamingData,
										const char*							pSegsFileName,
										StreamingCallBack					streamingCallBack,
										void*								pCallBackData )
{
	//////////////////////////////////////////////////////////////////////////
	//
	// Send queue entry to streaming thread.
	//
	//////////////////////////////////////////////////////////////////////////

	// Allocate and set queue entry
	StreamingQueueEntry streamingQueueEntry;
	streamingQueueEntry.m_streamingQueueCommand       = kStreamingQueueCommand_decompressInPlace;
	streamingQueueEntry.m_pStreamingData              = pStreamingData;
	streamingQueueEntry.m_pSegsFileName               = pSegsFileName;
	streamingQueueEntry.m_pNumElementsToDecompress    = &pStreamingData->m_numElementsToDecompress;
	streamingQueueEntry.m_pEventFlag                  = &pStreamingData->m_eventFlag;
	streamingQueueEntry.m_streamingCallBack           = streamingCallBack;
	streamingQueueEntry.m_pCallBackData               = pCallBackData;

    // Send queue entry to streaming thread
	_Ppu2PpuQueue* pStreamingQueue = pStreamingData->m_pStreamingQueue;
    // Note that the streamingQueueEntry data is copied into the queue location
	int ret = _ppu2PpuQueuePush( pStreamingQueue, &streamingQueueEntry, NULL );
    assert( CELL_OK == ret );
}

//////////////////////////////////////////////////////////////////////////

// Streaming thread for multiple file decompressions
void* streamingThread( void* pVoid )
{
	//////////////////////////////////////////////////////////////////////////
	//
	// Get input for a file to decompress from streamingQueue.
	//
	//////////////////////////////////////////////////////////////////////////

    StreamingData/*NOT ON STACK!*/* pStreamingData  = (StreamingData*)pVoid;
	_Ppu2PpuQueue* pStreamingQueue					= pStreamingData->m_pStreamingQueue;
    StreamingQueueEntry streamingQueueEntry;
    while( true )
    {
		int ret = _ppu2PpuQueuePop( pStreamingQueue, &streamingQueueEntry, NULL );
	    assert( CELL_OK == ret );


	    //////////////////////////////////////////////////////////////////////////
	    //
		// Execute command.
	    //
	    //////////////////////////////////////////////////////////////////////////

        switch( streamingQueueEntry.m_streamingQueueCommand )
        {
        case kStreamingQueueCommand_shutDown:
            // Caller wants this thread to exit.
            // This does return any data.
            pthread_exit( NULL );
			// Note: no execution here
			break;

        case kStreamingQueueCommand_decompressInPlace:
            // Decompress file in place.
            decompressInPlace( &streamingQueueEntry );
            break;

        case kStreamingQueueCommand_decompressBufferInput:
            // see zlib-localmemory-streaming-sample for use of this command
            assert( 0 );
			break;

        default:
            // unknown command
            assert( 0 );
			break;
        }
    }
    // Note: no execution here

#ifndef __SNC__
	return NULL; // remove warning
#endif
}

//////////////////////////////////////////////////////////////////////////

// Decompress a file in place, then return to caller
void decompressInPlace( StreamingQueueEntry* pStreamingQueueEntry )
{
    int ret;

	//////////////////////////////////////////////////////////////////////////
	//
	//	Load the compressed file (".segs") header.
	//
	//////////////////////////////////////////////////////////////////////////

	EdgeSegCompSampleFileHeader segsHeader;

    const char* pSegsFileName = pStreamingQueueEntry->m_pSegsFileName;
	FILE* fp = fopen( pSegsFileName, "rb" );
    assert( fp != NULL );

	size_t numRead = fread( &segsHeader, 1, sizeof(EdgeSegCompSampleFileHeader), fp );
	assert ( numRead == sizeof(EdgeSegCompSampleFileHeader) );

    uint32_t segsFileSize = segsHeader.m_segsFileSize;
	TestValidZlibSegsFile( &segsHeader, pSegsFileName, segsFileSize );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Load segs file Table Of Contents.
	//
	//////////////////////////////////////////////////////////////////////////

    uint32_t numSegments = GetZlibSegsFileNumSegments( &segsHeader );
    uint32_t sizeofToc = sizeof(EdgeSegCompSampleTocElement) * numSegments;
    uint32_t sizeofTocRoundUp = RoundUp( sizeofToc, 16 );
    EdgeSegCompSampleTocElement* pToc = (EdgeSegCompSampleTocElement*)memalign( 16, sizeofTocRoundUp );
    assert( pToc != NULL);

	numRead = fread( pToc, 1, sizeofToc, fp );
	assert ( numRead == sizeofToc );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Allocate a buffer that the uncompressed data will be written to.
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t totalUncompressedSize			        = GetZlibSegsFileTotalUncompressedSize( &segsHeader );
	uint32_t outputMallocSize						= RoundUp( totalUncompressedSize, 128 );
	unsigned char* pOutputUncompBuff				= (unsigned char*) memalign( 128, outputMallocSize );
    assert( pOutputUncompBuff != NULL );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Loop through all the segments:
	//		Load the input data into the location that the uncompressed data
	//		will be output at.
	//		If the data is compressed, then issue a Queue entry to decompress
	//		the data in place.
	//
	//////////////////////////////////////////////////////////////////////////

	clock_t clockBeginDecompress = clock();

    uint32_t/*NOT ON STACK!*/* pNumElementsToDecompress = pStreamingQueueEntry->m_pNumElementsToDecompress;
    *pNumElementsToDecompress       = numSegments;
	const EdgeSegCompSampleTocElement* pTocElement = pToc;
	uint32_t outputOffset			= 0;
	uint32_t numNonCompressedSegs	= 0;
	uint16_t eventFlagBits	        = 1;

    StreamingData/*NOT ON STACK!*/* pStreamingData = pStreamingQueueEntry->m_pStreamingData;
    CellSpursEventFlag/*NOT ON STACK!*/*pEventFlag = pStreamingQueueEntry->m_pEventFlag;

	for ( uint32_t numSegmentsToDo = numSegments; numSegmentsToDo > 0; numSegmentsToDo-- )
	{
        // read the Table Of Content entry for this segment
		uint32_t compressedSize = pTocElement->m_compressedSize;		//'0x0000' means 64K
        if( compressedSize == 0 )
            compressedSize = 64*1024;
		uint32_t uncompressedSize = pTocElement->m_uncompressedSize;	//'0x0000' means 64K
        if( uncompressedSize == 0 )
            uncompressedSize = 64*1024;
        uint32_t fileOffset = pTocElement->m_fileOffset & ~1; //segments necessarily start at qword offsets
        assert( (fileOffset & 0xF) == 0 );
		uint32_t isStoredCompressed	= pTocElement->m_fileOffset & 1;

        if( !isStoredCompressed )
            numNonCompressedSegs++;

		// address to store output data to
		unsigned char* pOutputUncompSeg	= &pOutputUncompBuff[outputOffset];

		assert( compressedSize <= uncompressedSize );

		// load input data (usually compressed) for 1 segment into start of
		// memory that will contain the output uncompressed data
		ret = fseek( fp, fileOffset, SEEK_SET );
		assert( ret == 0 );
		numRead = fread( pOutputUncompSeg, 1, compressedSize, fp );
		assert( numRead == compressedSize );

        // if the input data is already uncompressed, then that segment is done
        // (since it's already loaded where it should be output)
        if( isStoredCompressed )
        {
            // tell the Inflate Task to start decompression of this segment
			EdgeZlibInflateQHandle inflateQueueHandle	= pStreamingData->m_inflateQueueHandle;
			edgeZlibAddInflateQueueElement(	inflateQueueHandle,
										    pOutputUncompSeg/*input*/, compressedSize,
										    pOutputUncompSeg, uncompressedSize,
											pNumElementsToDecompress,	//This will be decremented by 1 when this element is decompressed
											pEventFlag,					// and when it reaches zero, this event will be triggered
										    eventFlagBits,
										    (EdgeZlibInflateTaskProcessing)isStoredCompressed
									    );
        }

        // prepare for next segment
		pTocElement++;
		outputOffset += uncompressedSize;
		assert( outputOffset <= totalUncompressedSize );
	}
	

	//////////////////////////////////////////////////////////////////////////
	//
	//	Since the Inflate Task is not going to be decompressing some of the
	//	segments (if they were already uncompressed), then we need to do the
	//	decrement here to say that they're done.
	//	And if necessary set the event flag.
	//
	//////////////////////////////////////////////////////////////////////////

	if ( numNonCompressedSegs > 0 )
	{
		bool setEventFlag = true;
		if ( 1 ) // numElementsToDecompress exists!
		{
			setEventFlag = false;
			uint32_t old = cellAtomicSub32( pNumElementsToDecompress, numNonCompressedSegs );
			if ( numNonCompressedSegs > old )
			{
				printf( "EDGE ZLIB ERROR: Attempted to decrement a work-to-do counter by more than counter's current value (%d > %d)\n", numNonCompressedSegs, old );
				assert( false );
			}

			if ( old == numNonCompressedSegs )	//if just decremented to zero
			{
				setEventFlag = true;
			}
		}

		if ( setEventFlag )
		{
			if ( 1 ) // eventFlag exists!
			{
				ret = cellSpursEventFlagSet( pEventFlag, eventFlagBits );
				assert( CELL_OK == ret );
			}
		}
	}


	//////////////////////////////////////////////////////////////////////////
	//
	//	Wait for the event flag to be acknowledged in order to tell us the
	//	work is done.
	//
	//////////////////////////////////////////////////////////////////////////

	ret = cellSpursEventFlagWait( pEventFlag, &eventFlagBits, CELL_SPURS_EVENT_FLAG_AND );
	assert( CELL_OK == ret );
	assert( *pNumElementsToDecompress == 0 );	//Should have reached zero by now

	clock_t clockEndDecompress = clock();


	//////////////////////////////////////////////////////////////////////////
	//
	// Free memory, and close input file.
	//
	//////////////////////////////////////////////////////////////////////////

    free( pToc );
	// Note: We do *not* free the allocated mem for the output uncompressed data
    //  since we return that to the callBack

   	ret = fclose( fp );
   	assert( ret == 0 );


	//////////////////////////////////////////////////////////////////////////
	//
	// Create additional data to send to user callBack.
	//
	//////////////////////////////////////////////////////////////////////////

    StreamingReturnData streamingReturnData;
    streamingReturnData.m_totalUncompressedSize  = totalUncompressedSize;
    streamingReturnData.m_pOutputUncompBuff      = pOutputUncompBuff;
    streamingReturnData.m_clockBeginDecompress   = clockBeginDecompress;
    streamingReturnData.m_clockEndDecompress     = clockEndDecompress;
    streamingReturnData.m_segsFileSize           = segsFileSize;
    streamingReturnData.m_numSegments            = numSegments;
    streamingReturnData.m_numNonCompressedSegs   = numNonCompressedSegs;


	//////////////////////////////////////////////////////////////////////////
	//
	// Execute user callBack, if any.
	//
	//////////////////////////////////////////////////////////////////////////

	StreamingCallBack streamingCallBack = pStreamingQueueEntry->m_streamingCallBack;
    if( streamingCallBack != NULL )
    {
        streamingCallBack( pStreamingQueueEntry, &streamingReturnData );
    }

    // return to streaming thread
}
