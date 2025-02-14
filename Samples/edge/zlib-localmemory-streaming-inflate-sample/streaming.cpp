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
void decompressBufferInput( StreamingQueueEntry* pStreamingQueueEntry );

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

    CellSpursEventFlag/*NOT ON STACK!*/*pEventFlag = &pStreamingData->m_eventFlag[0];
	for ( uint32_t eventFlagIndex = 0 ; eventFlagIndex < kNumEventFlags ; eventFlagIndex++ )
	{
		ret = cellSpursEventFlagInitializeIWL(	pSpurs,
												&pEventFlag[eventFlagIndex],
												CELL_SPURS_EVENT_FLAG_CLEAR_AUTO,
												CELL_SPURS_EVENT_FLAG_SPU2PPU );
		assert( CELL_OK == ret );

		ret = cellSpursEventFlagAttachLv2EventQueue( &pEventFlag[eventFlagIndex] );
		assert( CELL_OK == ret );
	}
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

    CellSpursEventFlag/*NOT ON STACK!*/*pEventFlag = &pStreamingData->m_eventFlag[0];
	for ( uint32_t eventFlagIndex = 0 ; eventFlagIndex < kNumEventFlags ; eventFlagIndex++ )
	{
		ret = cellSpursEventFlagDetachLv2EventQueue( &pEventFlag[eventFlagIndex] );
		assert( CELL_OK == ret );
	}

    // For safety in future, clear structure
	memset( pStreamingData, 0, sizeof(StreamingData) );
}

//////////////////////////////////////////////////////////////////////////

// Start streaming decompression (buffer input)
void startStreamingDecompressBufferInput(	StreamingData/*NOT ON STACK!*/*		pStreamingData,
											const char*							pSegsFileName,
											void*								pLocalMemDestination,
											uint32_t							localMemAvailable,
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
	streamingQueueEntry.m_streamingQueueCommand       = kStreamingQueueCommand_decompressBufferInput;
	streamingQueueEntry.m_pStreamingData              = pStreamingData;
	streamingQueueEntry.m_pSegsFileName               = pSegsFileName;
	streamingQueueEntry.m_pLocalMemDestination        = pLocalMemDestination;
	streamingQueueEntry.m_localMemAvailable           = localMemAvailable;
	streamingQueueEntry.m_pEventFlag                  = &pStreamingData->m_eventFlag[0];
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
            // see zlib-streaming-sample for use of this command
			assert( 0 );
			break;

        case kStreamingQueueCommand_decompressBufferInput:
            // Decompress file (buffer input)
            decompressBufferInput( &streamingQueueEntry );
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

// Decompress a file (buffer input), then return to caller
void decompressBufferInput( StreamingQueueEntry* pStreamingQueueEntry )
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
	// Allocate a buffer that the uncompressed data will be written to.
	// Note: This sample just uses start of available localMemory passed by user.
	// 
	//////////////////////////////////////////////////////////////////////////

	uint32_t totalUncompressedSize      = GetZlibSegsFileTotalUncompressedSize( &segsHeader );
	uint32_t outputMallocSize			= RoundUp( totalUncompressedSize, 128 );
    void* pLocalMemDestination          = pStreamingQueueEntry->m_pLocalMemDestination;
	unsigned char* pOutputUncompBuff    = (unsigned char*)pLocalMemDestination;
    // In this case, we know that the supplied uncompressed file is much smaller than the
    // available space in local memory
    uint32_t localMemAvailable          = pStreamingQueueEntry->m_localMemAvailable;
    assert( outputMallocSize <= localMemAvailable );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Loop through all the segments:
	//		If the input segs buffer is free,
	//		then load the input data from the file into it and issue the
	//		decompression Queue entry
	//
	//////////////////////////////////////////////////////////////////////////

	clock_t clockBeginDecompress = clock();

	// the input segs buffer needs to be read by the SPU
	const uint32_t kMaxInputBufferSize				= 64*1024;
	static unsigned char s_segsInputBuffer[kNumSegsBuffers][kMaxInputBufferSize] __attribute__((__aligned__(128)));
	uint32_t segsBufferNum							= 0;
	bool checkIfBufferFree							= false;	//On the first loop we don't want to call cellSpursEventFlagWait

	const EdgeSegCompSampleTocElement* pTocElement	= pToc;
	uint32_t outputOffset							= 0;
	uint32_t numNonCompressedSegs					= 0;
	const uint16_t eventFlagBits					= 1;

	StreamingData/*NOT ON STACK!*/* pStreamingData	= pStreamingQueueEntry->m_pStreamingData;
	CellSpursEventFlag/*NOT ON STACK!*/*pEventFlag	= pStreamingQueueEntry->m_pEventFlag;

	for ( uint32_t numSegmentsToDo = numSegments; numSegmentsToDo > 0; numSegmentsToDo-- )
	{
		if( checkIfBufferFree )
		{
			// Wait for the event flag to be acknowledged in order to tell us the work is done.
			uint16_t modifiedEventFlagBits = eventFlagBits;
			ret = cellSpursEventFlagWait( &pEventFlag[segsBufferNum], &modifiedEventFlagBits, CELL_SPURS_EVENT_FLAG_AND );
			assert( CELL_OK == ret );
		}

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

		// address to load input data to
		unsigned char* pSegInputBuff = &s_segsInputBuffer[segsBufferNum][0];

		assert( compressedSize <= uncompressedSize );

		// load input data (usually compressed) for 1 segment into buffered input memory
		ret = fseek( fp, fileOffset, SEEK_SET );
		assert( ret == 0 );
		assert( compressedSize <= kMaxInputBufferSize );
		numRead = fread( pSegInputBuff, 1, compressedSize, fp );
		assert( numRead == compressedSize );

		// address to store output data to
		unsigned char* pOutputUncompSeg	= &pOutputUncompBuff[outputOffset];

        // issue Queue entry for SPU Inflate Task to decompress (or move) the segment
		EdgeZlibInflateQHandle inflateQueueHandle = pStreamingData->m_inflateQueueHandle;
		edgeZlibAddInflateQueueElement(	inflateQueueHandle,
										pSegInputBuff, compressedSize,
										pOutputUncompSeg, uncompressedSize,
										NULL,								//Because this is NULL, as soon as this element is decompressed
										&pEventFlag[segsBufferNum],			//	the event will be triggered
										eventFlagBits,
										(EdgeZlibInflateTaskProcessing)isStoredCompressed
									);

        // prepare for next segment
        segsBufferNum++;
        if( segsBufferNum == kNumSegsBuffers )
        {
            segsBufferNum = 0;
			checkIfBufferFree = true;
        }
		pTocElement++;
		outputOffset += uncompressedSize;
		assert( outputOffset <= totalUncompressedSize );
	}
	

	//////////////////////////////////////////////////////////////////////////
	//
	// Since the last Queue Element commands in the segs buffers will have
	// been issued but we have not yet had to wait for their completion, we
	// now have to wait for their completion to ensure that the entire file
	// has been decompressed.
	//
	//////////////////////////////////////////////////////////////////////////

	// Note: We don't need to check for these segments in the order that they
	// were issued, since we have to ensure that they were all completed anyway
	for ( segsBufferNum = 0 ; (segsBufferNum < kNumSegsBuffers) && (segsBufferNum < numSegments) ; segsBufferNum++ )
	{
		// Wait for the event flag to be acknowledged in order to tell us the work is done.
		uint16_t modifiedEventFlagBits = eventFlagBits;
		ret = cellSpursEventFlagWait( &pEventFlag[segsBufferNum], &modifiedEventFlagBits, CELL_SPURS_EVENT_FLAG_AND );
		assert( CELL_OK == ret );
	}

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
