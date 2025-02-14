/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <cell/spurs.h>
#include <sys/spu_initialize.h>
#include <spu_printf.h>

#include "edge/lzo/edgelzo_ppu.h"

//////////////////////////////////////////////////////////////////////////

//Set this to 5 if you want decompression to run in parallel on 5 SPUs.
//However, since we're only adding one work item to the queue in this
//sample it won't make a difference.
const uint32_t kNumInflateTasks			= 1;

//////////////////////////////////////////////////////////////////////////

void lzo1x_basic_inflate_sample_main( CellSpurs* pSpurs );
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

	lzo1x_basic_inflate_sample_main( &s_spurs );

	ret = cellSpursFinalize( &s_spurs );
	assert( CELL_OK == ret );

	ret = spu_printf_finalize();
	assert( CELL_OK == ret );

	return 0;
}

//////////////////////////////////////////////////////////////////////////

void lzo1x_basic_inflate_sample_main( CellSpurs* pSpurs )
{
	static CellSpursEventFlag s_eventFlag;		//Cannot be on stack
	static CellSpursTaskset s_taskSet;			//Cannot be on stack
	static uint32_t s_numElementsToDecompress;	//Cannot be on stack

	// Note: these files were copied from lzo1x-basic-deflate-sample after it was run
	const char masterFilename[]		= "/app_home/assets/bs_cube.dae";
	const char compressedFilename[]	= "/app_home/assets/bs_cube.dae.lzo1x";


	//////////////////////////////////////////////////////////////////////////
	//
	//	Load compressed file
	//  First 4 bytes is uncompressed size, rest is compressed data
	//
	//////////////////////////////////////////////////////////////////////////

	// open compressed file
	uint32_t compressedFileSize;
	void* pUncompressedSizeAndCompressedBuf = LoadFile( compressedFilename, &compressedFileSize );

	// get #uncompressed bytes
	uint32_t expectedUncompressedSize = *(uint32_t*)pUncompressedSizeAndCompressedBuf;
	// get ptr to compressed bytes
	const void* pCompressedBuf = (const void*)((uint32_t)pUncompressedSizeAndCompressedBuf + 4);
	// get compressed size
	uint32_t compressedDataSize = compressedFileSize - 4;

	assert( compressedDataSize			<= kEdgeLzo1xInflateInputBufferMaxSize );
	assert( expectedUncompressedSize	<= kEdgeLzo1xInflateOutputBufferMaxSize );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Allocate a buffer that the uncompressed data will be written to
	//
	//////////////////////////////////////////////////////////////////////////

	unsigned char* pOutputUncompBuff	= (unsigned char*) memalign( 128, expectedUncompressedSize );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Initialize taskset and event flag
	//
	//////////////////////////////////////////////////////////////////////////

	// register taskSet
	CellSpursTasksetAttribute taskSetAttribute;
	uint8_t prios[8] = {15, 15, 15, 15, 15, 15, 0, 0};
	int ret = cellSpursTasksetAttributeInitialize( &taskSetAttribute, 0, prios, 8 );
	assert( CELL_OK == ret );
	ret = cellSpursTasksetAttributeSetName(&taskSetAttribute, "edgeLzo1xTaskSet");
	assert( CELL_OK == ret );
	ret = cellSpursCreateTasksetWithAttribute( pSpurs, &s_taskSet, &taskSetAttribute );
	assert( CELL_OK == ret );
	printf( "PPU: Taskset created\n" );

	ret = cellSpursEventFlagInitializeIWL(	pSpurs,
											&s_eventFlag,
											CELL_SPURS_EVENT_FLAG_CLEAR_AUTO,
											CELL_SPURS_EVENT_FLAG_SPU2PPU );
	assert( CELL_OK == ret );

	ret = cellSpursEventFlagAttachLv2EventQueue( &s_eventFlag );
	assert( CELL_OK == ret );
	printf( "PPU: Event flag created\n" );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Initialize Inflate Queue.
	//	This will hold the queue of work that the Inflate Task(s) on SPU will
	//	pull work from.
	//
	//////////////////////////////////////////////////////////////////////////

	const uint32_t kMaxNumInflateQueueEntries = 16;
	uint32_t inflateQueueBuffSize = edgeLzo1xGetInflateQueueSize( kMaxNumInflateQueueEntries );
	void* pInflateQueueBuffer = memalign( EDGE_LZO1X_INFLATE_QUEUE_ALIGN, inflateQueueBuffSize );
	EdgeLzo1xInflateQHandle inflateQueue = edgeLzo1xCreateInflateQueue(
														pSpurs,
														kMaxNumInflateQueueEntries,
														pInflateQueueBuffer,
														inflateQueueBuffSize );
	printf( "PPU: Inflate Queue created\n" );


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
		uint32_t contextSaveSize = edgeLzo1xGetInflateTaskContextSaveSize();
		pTaskContext[taskNum] = memalign( CELL_SPURS_TASK_CONTEXT_ALIGN, contextSaveSize );
		edgeLzo1xCreateInflateTask( &s_taskSet, pTaskContext[taskNum], inflateQueue );
	}
	printf( "PPU: %d Inflate Task(s) started\n", kNumInflateTasks );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Add one item to the Inflate Queue.
	//	The Inflate Task will wake up and process this work.
	//
	//////////////////////////////////////////////////////////////////////////

	s_numElementsToDecompress = 1;	//Must be set correctly before any elements are added

	uint16_t eventFlagBits = 1;
	// create one task queue entry (max 64K inflate per entry)
	edgeLzo1xAddInflateQueueElement(		inflateQueue,
											pCompressedBuf, compressedDataSize,
											pOutputUncompBuff, expectedUncompressedSize,
											&s_numElementsToDecompress,	//This will be decremented by 1 when this element is decompressed
											&s_eventFlag,				//When s_numElementsToDecompress decrements to zero, this event will be triggered
											eventFlagBits,
											kEdgeLzo1xInflateTask_Inflate
										);	

	printf( "PPU: Inflate element(s) added to queue\n" );

	printf( "PPU: Wait for event flag acknowledgment\n" );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Wait for the event flag to be acknowledged in order to tell us the
	//	work is done.
	//
	//////////////////////////////////////////////////////////////////////////

	ret = cellSpursEventFlagWait( &s_eventFlag, &eventFlagBits, CELL_SPURS_EVENT_FLAG_AND );
	assert( CELL_OK == ret );
	assert( s_numElementsToDecompress == 0 );	//Should have reached zero by now

	printf( "PPU: Edge LZO event acknowledged\n" );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Shutdown Inflate Queue, event flag and taskset.
	//
	//////////////////////////////////////////////////////////////////////////

	printf( "PPU: Shutdown Inflate Queue...\n" );
	edgeLzo1xShutdownInflateQueue( inflateQueue );

	ret = cellSpursEventFlagDetachLv2EventQueue( &s_eventFlag );
	assert( CELL_OK == ret );

	// shutdown taskSet
	printf( "PPU: Wait for taskset shutdown...\n" );
	ret = cellSpursShutdownTaskset( &s_taskSet );
	assert( CELL_OK == ret );
	// wait for all tasks to finish
	ret = cellSpursJoinTaskset( &s_taskSet );
	assert( CELL_OK == ret );
	printf( "PPU: Shutdown taskset completed.\n" );

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

	assert( masterFileSize == expectedUncompressedSize );

	printf( "PPU: Test decompressed data against original master data...\n" );
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
	printf( "PPU: Decompressed data matches original master data\n" );

	//clean up
	FreeFile( pMasterBuff );
	free( pOutputUncompBuff );
	FreeFile( pUncompressedSizeAndCompressedBuf );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Print stats
	//
	//////////////////////////////////////////////////////////////////////////

	printf( "\n uncompSiz    compSiz    compress%% numTasks\n" );
	printf( "___________________________________________\n" );

	printf( "%s\n", masterFilename );

	float compressionPercent = (1.f - (float)compressedFileSize / (float)masterFileSize) * 100.f;

	printf( "   0x%05x    0x%05x   %8.2f%%         %d\n\n", 
			(int)masterFileSize, (int)compressedFileSize,	compressionPercent, kNumInflateTasks );
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
