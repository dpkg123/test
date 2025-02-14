/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <cell/spurs.h>
#include <sys/spu_initialize.h>
#include <spu_printf.h>

#include "edge/zlib/edgezlib_ppu.h"

#include "gz.h"

//////////////////////////////////////////////////////////////////////////

//Set this to 1 if you want decompression to only run on 1 SPU
const uint32_t kNumInflateTasks			= 5;

//////////////////////////////////////////////////////////////////////////

void zlib_large_unsegmented_inflate_sample_main( CellSpurs* pSpurs );
void* LoadFile( const char* filename, uint32_t* pFileSize );
void FreeFile( void* pBuffer );

//////////////////////////////////////////////////////////////////////////

int main()
{
	static CellSpurs s_spurs;
	const uint32_t kNumSpusInSpursInstance = 5;
	const uint32_t kSpuThreadGroupPriority = 250;

	int ret = sys_spu_initialize( 6, 0 );
	assert( CELL_OK == ret );

	sys_ppu_thread_t threadId;
	ret = sys_ppu_thread_get_id( &threadId );
	assert( CELL_OK == ret );

	int ppuThrPrio;
	ret = sys_ppu_thread_get_priority( threadId, &ppuThrPrio );
	assert( CELL_OK == ret );

	int ppuSecondaryThrPrio = ppuThrPrio - 1;

	ret = spu_printf_initialize( ppuSecondaryThrPrio, NULL );
	assert( CELL_OK == ret );

	CellSpursAttribute	spursAttrib;
	ret = cellSpursAttributeInitialize( &spursAttrib, kNumSpusInSpursInstance, kSpuThreadGroupPriority, ppuSecondaryThrPrio, false );
	assert( CELL_OK == ret );

	ret = cellSpursAttributeEnableSpuPrintfIfAvailable( &spursAttrib );
	assert( CELL_OK == ret );

	ret = cellSpursInitializeWithAttribute( &s_spurs, &spursAttrib );
	assert( CELL_OK == ret );

	zlib_large_unsegmented_inflate_sample_main( &s_spurs );

	ret = cellSpursFinalize( &s_spurs );
	assert( CELL_OK == ret );

	ret = spu_printf_finalize();
	assert( CELL_OK == ret );

	return 0;
}

//////////////////////////////////////////////////////////////////////////

void zlib_large_unsegmented_inflate_sample_main( CellSpurs* pSpurs )
{
	static CellSpursEventFlag s_eventFlag;		//Cannot be on stack
	static CellSpursTaskset s_taskSet;			//Cannot be on stack
	static uint32_t s_numElementsToDecompress;	//Cannot be on stack

	const char masterFilename[]		= "/app_home/assets/elephant-color.bin";
	const char compressedFilename[]	= "/app_home/assets/elephant-color.bin.gz";

	printf("\n  uncompSz    compSz     comp%%  uncompSecs  spus    inputRate    outputRate   numSegs  numNonCompressedSegs\n");
	printf("___________________________________________________________________________________________________________\n");

	printf( "%s\n", compressedFilename );

	//////////////////////////////////////////////////////////////////////////
	//
	//	Load compressed file
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t compressedFileSize;
	void* pCompressedBuff				= LoadFile( compressedFilename, &compressedFileSize );
	const void* pRawData				= GzHeaderFindRawData( pCompressedBuff );
	assert( pRawData );
	uint32_t rawDataSize				= GzHeaderGetRawDataSize( pCompressedBuff, compressedFileSize );
	uint32_t expectedUncompressedSize	= GzHeaderGetUncompressedSize( pCompressedBuff, compressedFileSize );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Allocate a buffer that the uncompressed data will be written to.
	//
	//////////////////////////////////////////////////////////////////////////

	unsigned char* pOutputUncompBuff	= (unsigned char*) memalign( 128, expectedUncompressedSize );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Initialize taskset and event flag.
	//
	//////////////////////////////////////////////////////////////////////////

	// register taskSet
	CellSpursTasksetAttribute taskSetAttribute;
	uint8_t prios[8] = {15, 15, 15, 15, 15, 15, 0, 0};
	int ret = cellSpursTasksetAttributeInitialize( &taskSetAttribute, 0, prios, 8 );
	assert( CELL_OK == ret );
	ret = cellSpursTasksetAttributeSetName(&taskSetAttribute, "edgeZlibTaskSet");
	assert( CELL_OK == ret );
	ret = cellSpursCreateTasksetWithAttribute( pSpurs, &s_taskSet, &taskSetAttribute );
	assert( CELL_OK == ret );

	ret = cellSpursEventFlagInitializeIWL(	pSpurs,
											&s_eventFlag,
											CELL_SPURS_EVENT_FLAG_CLEAR_AUTO,
											CELL_SPURS_EVENT_FLAG_SPU2PPU );
	assert( CELL_OK == ret );

	ret = cellSpursEventFlagAttachLv2EventQueue( &s_eventFlag );
	assert( CELL_OK == ret );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Initialize Inflate Queue.
	//	This will hold the queue of work that the Inflate Task(s) on SPU will
	//	pull work from.
	//
	//////////////////////////////////////////////////////////////////////////

	const uint32_t kMaxNumInflateQueueEntries = 16;
	uint32_t inflateQueueBuffSize = edgeZlibGetInflateQueueSize( kMaxNumInflateQueueEntries );
	void* pInflateQueueBuffer = memalign( EDGE_ZLIB_INFLATE_QUEUE_ALIGN, inflateQueueBuffSize );
	EdgeZlibInflateQHandle inflateQueue = edgeZlibCreateInflateQueue(
														pSpurs,
														kMaxNumInflateQueueEntries,
														pInflateQueueBuffer,
														inflateQueueBuffSize );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Build Inflate Tasks.
	//	We want the decompression to be able to run in parallel on multiple
	//	SPUs so we create as many tasks as SPUs that we want it to run
	//	on (kNumInflateTasks).
	//
	//////////////////////////////////////////////////////////////////////////

	void* pTaskContext[kNumInflateTasks];
	for( uint32_t taskNum = 0 ; taskNum < kNumInflateTasks ; taskNum++ )
	{
		uint32_t contextSaveSize = edgeZlibGetInflateTaskContextSaveSize();
		pTaskContext[taskNum] = memalign( CELL_SPURS_TASK_CONTEXT_ALIGN, contextSaveSize );
		edgeZlibCreateInflateTask( &s_taskSet, pTaskContext[taskNum], inflateQueue );
	}


	//////////////////////////////////////////////////////////////////////////
	//
	//	Add one item to the Inflate Queue.
	//	The Inflate Task will wake up and process this work.
	//
	//////////////////////////////////////////////////////////////////////////

	clock_t clockBeginDecompress = clock();

	s_numElementsToDecompress = 1;	//Must be set correctly before any elements are added

	uint16_t eventFlagBits = 1;
	// create one task queue entry
	edgeZlibAddInflateQueueElement(			inflateQueue,
											pRawData, rawDataSize,
											pOutputUncompBuff, expectedUncompressedSize,
											&s_numElementsToDecompress,	//This will be decremented by 1 when this element is decompressed
											&s_eventFlag,				//When s_numElementsToDecompress decrements to zero, this event will be triggered
											eventFlagBits,
											kEdgeZlibInflateTask_Inflate
										);	


	//////////////////////////////////////////////////////////////////////////
	//
	//	Wait for the event flag to be acknowledged in order to tell us the
	//	work is done.
	//
	//////////////////////////////////////////////////////////////////////////

	ret = cellSpursEventFlagWait( &s_eventFlag, &eventFlagBits, CELL_SPURS_EVENT_FLAG_AND );
	assert( CELL_OK == ret );
	assert( s_numElementsToDecompress == 0 );	//Should have reached zero by now

	clock_t clockEndDecompress = clock();


	//////////////////////////////////////////////////////////////////////////
	//
	//	Shutdown Inflate Queue, event flag and taskset.
	//
	//////////////////////////////////////////////////////////////////////////

	edgeZlibShutdownInflateQueue( inflateQueue );

	ret = cellSpursEventFlagDetachLv2EventQueue( &s_eventFlag );
	assert( CELL_OK == ret );

	// shutdown taskSet
	ret = cellSpursShutdownTaskset( &s_taskSet );
	assert( CELL_OK == ret );
	// wait for all tasks to finish
	ret = cellSpursJoinTaskset( &s_taskSet );
	assert( CELL_OK == ret );

	// free alloc'd buffers
	for( uint32_t taskNum = 0 ; taskNum < kNumInflateTasks ; taskNum++ )
	{
		free( pTaskContext[taskNum] );
	}
	free( pInflateQueueBuffer );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Load master file and verify decompression generated the correct data.
	//
	//////////////////////////////////////////////////////////////////////////

	// open uncompressed file
	uint32_t masterFileSize;
	unsigned char* pMasterBuff			= (unsigned char*) LoadFile( masterFilename, &masterFileSize );

	// verify output uncompressed data is the same as the original master data
	for ( uint32_t i = 0; i < masterFileSize; i++ )
	{
		if ( pMasterBuff[i] != pOutputUncompBuff[i] )
		{
			printf( "PPU: ERROR in test for file %s\n", masterFilename );
			printf( "PPU:   At offset 0x%07x,\n", i );
			printf( "PPU:   output uncompressed data (0x%02x) does not match master data (0x%02x)\n",
					pOutputUncompBuff[i], pMasterBuff[i] );
			assert( false );
		}
	}

	//Clean up
	FreeFile( pMasterBuff );
	free( pOutputUncompBuff );
	FreeFile( pCompressedBuff );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Print stats
	//
	//////////////////////////////////////////////////////////////////////////

	float uncompressSecs			= (float)(clockEndDecompress - clockBeginDecompress) / (float)CLOCKS_PER_SEC;
	float compressionPercent		= (1.0f - (float)compressedFileSize / (float)masterFileSize) * 100.f;
	float inputDataRate				= ((float)compressedFileSize) / (((float)uncompressSecs) * 1024.0f * 1024.0f );
	float outputDataRate			= ((float)masterFileSize) / (((float)uncompressSecs) * 1024.0f * 1024.0f );

	printf( " %9d %9d    %5.2f%%   %9.5f     %d   %6.2fMB/s    %6.2fMb/s      %4d                  %4d\n\n",
			(int32_t)masterFileSize, (int32_t)compressedFileSize,	compressionPercent,
			uncompressSecs, kNumInflateTasks, inputDataRate, outputDataRate,
			1, 0 );
}

//////////////////////////////////////////////////////////////////////////

inline uint32_t RoundUp( uint32_t value, uint32_t alignment )
{
	assert( ((alignment & (alignment - 1)) == 0) && alignment );

	return (value + (alignment - 1)) & ~(alignment - 1);
}

//////////////////////////////////////////////////////////////////////////

void* LoadFile( const char* filename, uint32_t* pFileSize )
{
	*pFileSize = 0;	//In case we fail, set size to zero

	FILE* fp = fopen( filename, "rb" );
	if ( fp == NULL )
	{
		printf( "Error: Failed to load \"%s\"\n", filename );
		assert( false );
	}

	int ret = fseek( fp, 0, SEEK_END);
	assert( ret == 0 );

	size_t fileSize = ftell( fp );
	assert( fileSize > 0 );

	ret = fseek( fp, 0, SEEK_SET );
	assert( ret == 0 );

	int mallocSize = RoundUp( fileSize, 16 );	//Round up the malloc size to a qword multiple
	void* pData = memalign( 16, mallocSize );
	assert( pData );
	memset( pData, 0, mallocSize );

	size_t numRead = fread( pData, 1, fileSize, fp );
	assert( fileSize == numRead );
	(void) numRead;

	ret = fclose( fp );
	assert( ret == 0 );

	*pFileSize = fileSize;
	return pData;
}

//////////////////////////////////////////////////////////////////////////

void FreeFile( void* pBuffer )
{
	free( pBuffer );
}

//////////////////////////////////////////////////////////////////////////
