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

//Set this to 5 if you want compression to run in parallel on 5 SPUs.
//However, since we're only adding one work item to the queue in this
//sample it won't make a difference.
const uint32_t kNumDeflateTasks			= 1;

//////////////////////////////////////////////////////////////////////////

void lzo1x_basic_deflate_sample_main( CellSpurs* pSpurs );
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

	lzo1x_basic_deflate_sample_main( &s_spurs );

	ret = cellSpursFinalize( &s_spurs );
	assert( CELL_OK == ret );

	ret = spu_printf_finalize();
	assert( CELL_OK == ret );

	return 0;
}

//////////////////////////////////////////////////////////////////////////

void lzo1x_basic_deflate_sample_main( CellSpurs* pSpurs )
{
	static CellSpursEventFlag s_eventFlag;		//Cannot be on stack
	static CellSpursTaskset s_taskSet;			//Cannot be on stack
	static uint32_t s_numElementsToCompress;	//Cannot be on stack

	const char masterFilename[]			= "/app_home/assets/bs_cube.dae";
	const char compressedFilename[]		= "/app_home/assets/bs_cube.dae.lzo1x";


	//////////////////////////////////////////////////////////////////////////
	//
	//	Load uncompressed file
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t uncompressedSize;
	const void* pUncompressedBuf = LoadFile( masterFilename, &uncompressedSize );
	assert( uncompressedSize <= kEdgeLzo1xDeflateInputBufferMaxSize );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Allocate a buffer that the compressed data will be written to
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t compressedAllocation	= kEdgeLzo1xDeflateOutputBufferMaxSize;
	unsigned char* pCompressedBuf	= (unsigned char*) memalign( 128, compressedAllocation );


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
	//	Initialize Deflate Queue.
	//	This will hold the queue of work that the Deflate Task(s) on SPU will
	//	pull work from.
	//
	//////////////////////////////////////////////////////////////////////////

	const uint32_t kMaxNumDeflateQueueEntries = 16;
	uint32_t deflateQueueBuffSize = edgeLzo1xGetDeflateQueueSize( kMaxNumDeflateQueueEntries );
	void* pDeflateQueueBuffer = memalign( EDGE_LZO1X_DEFLATE_QUEUE_ALIGN, deflateQueueBuffSize );
	EdgeLzo1xDeflateQHandle deflateQueue = edgeLzo1xCreateDeflateQueue(
														pSpurs,
														kMaxNumDeflateQueueEntries,
														pDeflateQueueBuffer,
														deflateQueueBuffSize );
	printf( "PPU: Deflate Queue created\n" );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Build Deflate Tasks.
	//	We want the compression to be able to run in parallel on multiple
	//	SPUs so we create as many tasks as SPUs that we want it to run
	//	on (kNumDeflateTasks).
	//
	//////////////////////////////////////////////////////////////////////////

	void* pTaskContext[kNumDeflateTasks];
	for( uint32_t taskNum = 0 ; taskNum < kNumDeflateTasks ; taskNum++ )
	{
		uint32_t contextSaveSize = edgeLzo1xGetDeflateTaskContextSaveSize();
		pTaskContext[taskNum] = memalign( CELL_SPURS_TASK_CONTEXT_ALIGN, contextSaveSize );
		edgeLzo1xCreateDeflateTask( &s_taskSet, pTaskContext[taskNum], deflateQueue );
	}
	printf( "PPU: %d Deflate Task(s) started\n", kNumDeflateTasks );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Add one item to the Deflate Queue.
	//	The Deflate Task will wake up and process this work.
	//
	//////////////////////////////////////////////////////////////////////////

	s_numElementsToCompress = 1;	//Must be set correctly before any elements are added

	uint16_t eventFlagBits = 1;

	// Deflate task will return size here (hi bit says whether it was compressed)
	static uint32_t compressedSize;	//Cannot be on stack

	// create one task queue entry (max 64K deflate per entry)
	edgeLzo1xAddDeflateQueueElement(		deflateQueue,
											pUncompressedBuf, uncompressedSize,
											pCompressedBuf, compressedAllocation,
											&compressedSize,
											&s_numElementsToCompress,	//This will be decremented by 1 when this element is compressed
											&s_eventFlag,				//When s_numElementsToCompress decrements to zero, this event will be triggered
											eventFlagBits,
											kEdgeLzo1xDeflateTask_DeflateStoreCompressed // store compressed even if it's bigger than uncompressed
										);	

	printf( "PPU: Deflate element(s) added to queue\n" );

	printf( "PPU: Wait for event flag acknowledgment\n" );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Wait for the event flag to be acknowledged in order to tell us the
	//	work is done.
	//
	//////////////////////////////////////////////////////////////////////////

	ret = cellSpursEventFlagWait( &s_eventFlag, &eventFlagBits, CELL_SPURS_EVENT_FLAG_AND );
	assert( CELL_OK == ret );
	assert( s_numElementsToCompress == 0 );	//Should have reached zero by now

	printf( "PPU: Edge LZO event acknowledged\n" );

	// ensure Deflate task told us it stored the compressed data
	assert( (compressedSize & kEdgeLzo1xDeflateTask_HowStoredMask) == kEdgeLzo1xDeflateTask_CompressedWasStored );
	// remove bit from compressedSize
	compressedSize &= ~kEdgeLzo1xDeflateTask_HowStoredMask;

	// release uncompressedbuf
	FreeFile( (void*)pUncompressedBuf );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Shutdown Deflate Queue, event flag and taskset.
	//
	//////////////////////////////////////////////////////////////////////////

	printf( "PPU: Shutdown Deflate Queue...\n" );
	edgeLzo1xShutdownDeflateQueue( deflateQueue );

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
	for( uint32_t taskNum = 0 ; taskNum < kNumDeflateTasks ; taskNum++ )
	{
		free( pTaskContext[taskNum] );
	}
	free( pDeflateQueueBuffer );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Write out compressed data
	//
	//////////////////////////////////////////////////////////////////////////

	FILE* fp = fopen( compressedFilename, "wb" );
	if( fp == NULL )
	{
		printf("Error: can't open output compressed file (%s)\n", compressedFilename );
		assert ( false );
	}

	// write out #uncompressed bytes in first word
	size_t numWritten = fwrite( &uncompressedSize, 1, 4, fp );
	assert ( numWritten == 4 );

	// write out compressed data
	numWritten = fwrite( pCompressedBuf, 1, compressedSize, fp );
	assert ( numWritten == compressedSize );

	// free compressed buf
	free( pCompressedBuf );

	// close file
	ret = fclose( fp );
	assert( ret == 0 );

	// since we output 4 bytes at the start, show that in the compressedSize
	compressedSize += 4;


	//////////////////////////////////////////////////////////////////////////
	//
	//	Print stats
	//
	//////////////////////////////////////////////////////////////////////////

	printf( "\n uncompSiz    compSiz    compress%% numTasks\n" );
	printf( "___________________________________________\n" );

	printf( "%s\n", masterFilename );

	float compressionPercent = (1.f - (float)compressedSize / (float)uncompressedSize) * 100.f;

	printf( "   0x%05x    0x%05x   %8.2f%%         %d\n\n", 
			(int)uncompressedSize, (int)compressedSize,	compressionPercent, kNumDeflateTasks );
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
