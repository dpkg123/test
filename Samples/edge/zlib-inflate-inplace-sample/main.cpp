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

#include "inflate_segs_file.h"
#include "../zlib-segcomp-sample/ppu/zlib_segs_ppu.h"


//////////////////////////////////////////////////////////////////////////

//Set this to 1 if you want decompression to only run on 1 SPU
const uint32_t kNumInflateTasks			= 5;

//////////////////////////////////////////////////////////////////////////

void zlib_inflate_inplace_sample_main( CellSpurs* pSpurs );
void* LoadFile( const char* filename, uint32_t* pFileSize );
void FreeFile( void* pBuffer );
uint32_t LoadFileIntoBuffer( const char* filename, void* pBuffer, uint32_t bufferMaxSize );

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

	zlib_inflate_inplace_sample_main( &s_spurs );

	ret = cellSpursFinalize( &s_spurs );
	assert( CELL_OK == ret );

	ret = spu_printf_finalize();
	assert( CELL_OK == ret );

	return 0;
}

//////////////////////////////////////////////////////////////////////////

void zlib_inflate_inplace_sample_main( CellSpurs* pSpurs )
{
	static CellSpursEventFlag s_eventFlag;		//Cannot be on stack
	static CellSpursTaskset s_taskSet;			//Cannot be on stack
	static uint32_t s_numElementsToDecompress;	//Cannot be on stack

	const char masterFilename[] = "/app_home/assets/elephant-color.bin";
	const char segsFileName[]	= "/app_home/assets/elephant-color.bin.zlib.segs";

	printf("\n  uncompSz    compSz     comp%%  uncompSecs  spus    inputRate    outputRate   numSegs\n");
	printf("_____________________________________________________________________________________\n");

	printf( "%s\n", segsFileName );


	//////////////////////////////////////////////////////////////////////////
	//
	//	This is the buffer that compressed data will be loaded to.
	//	Then it will be decompressed in place within this buffer.
	//
	//////////////////////////////////////////////////////////////////////////

	const uint32_t kMaxInPlaceBufferSize = 6*1024*1024;			//must be big enough to hold max( inputSize, outputSize )
	unsigned char* pInPlaceBuffer = (unsigned char*) memalign( 128, kMaxInPlaceBufferSize );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Load compressed file (".segs")
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t segsFileSize = LoadFileIntoBuffer( segsFileName, pInPlaceBuffer, kMaxInPlaceBufferSize );

	TestValidZlibSegsFile( pInPlaceBuffer, segsFileName, segsFileSize );

	uint32_t numSegments					= GetZlibSegsFileNumSegments( pInPlaceBuffer );
	uint32_t totalUncompressedSize			= GetZlibSegsFileTotalUncompressedSize( pInPlaceBuffer );
	assert( totalUncompressedSize <= kMaxInPlaceBufferSize );

	//PrintSegsHeaderAndToc( pInPlaceBuffer, segsFileName, segsFileSize );


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
	ret = cellSpursTasksetAttributeSetName( &taskSetAttribute, "edgeZlibTaskSet" );
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

	const uint32_t kMaxNumInflateQueueEntries = numSegments;
	uint32_t inflateQueueBuffSize = edgeZlibGetInflateQueueSize( kMaxNumInflateQueueEntries );
	void* pInflateQueueBuffer = memalign( EDGE_ZLIB_INFLATE_QUEUE_ALIGN, inflateQueueBuffSize );
	EdgeZlibInflateQHandle inflateQueue = edgeZlibCreateInflateQueue(
														pSpurs,
														kMaxNumInflateQueueEntries,
														pInflateQueueBuffer,
														inflateQueueBuffSize );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Initialize Inflate Segs File Queue.
	//	Each queue entry is one segs file.  The Inflate Segs File Task on SPU
	//	will wake up and process each Segs File appropriately and add work to
	//	the Inflate Queue in order for the individual segments to be
	//	decompressed.
	//
	//////////////////////////////////////////////////////////////////////////

	const uint32_t kMaxNumInflateSegsFileQueueEntries = 1/*num segs files to decompress, or ring buffer size*/;
	uint32_t inflateSegsFileBuffSize = GetInflateSegsFileQueueSize( kMaxNumInflateSegsFileQueueEntries );
	void* pInflateSegsFileQueueBuffer = memalign( kInflateSegsFileAlign, inflateSegsFileBuffSize );
	InflateSegsFileQHandle inflateSegsFileQueue = CreateInflateSegsFileQueue(
														&s_taskSet,
														kMaxNumInflateSegsFileQueueEntries,
														pInflateSegsFileQueueBuffer,
														inflateSegsFileBuffSize );


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
		uint32_t inflateTaskContextSaveSize = edgeZlibGetInflateTaskContextSaveSize();
		pTaskContext[taskNum] = memalign( CELL_SPURS_TASK_CONTEXT_ALIGN, inflateTaskContextSaveSize );
		edgeZlibCreateInflateTask( &s_taskSet, pTaskContext[taskNum], inflateQueue );
	}


	//////////////////////////////////////////////////////////////////////////
	//
	//	Build an Inflate Segs File Task.
	//	This task will move the memory as necessary, and queue up the
	//	decompression work of the individual segments for the Inflate Tasks
	//	to do.
	//	We could create multiple Inflate Segs File Tasks, but since this is
	//	likely to be bound by bandwidth to main memory we probably wouldn't
	//	get much benefit from having multiple SPUs do it at once.
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t inflateSegsFileTaskContextSaveSize = GetInflateSegsFileTaskContextSaveSize();
	void* pInflateSegsFileTaskContext = memalign( CELL_SPURS_TASK_CONTEXT_ALIGN, inflateSegsFileTaskContextSaveSize );
	CreateInflateSegsFileTask( &s_taskSet, pInflateSegsFileTaskContext, inflateSegsFileQueue );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Add one item to the Inflate Segs File Queue.
	//	The Inflate Segs File Task will wake up and process this work.
	//	After moving the memory appropriately it will queue up the individual
	//	elements onto the Inflate Queue which are to be decompressed by the
	//	Inflate Task(s).
	//
	//////////////////////////////////////////////////////////////////////////

	clock_t clockBeginDecompress = clock();

	s_numElementsToDecompress = numSegments;	//Must be set correctly before any elements are added

	uint16_t eventFlagBits = 1;
	AddInflateSegsFileQueueElement(
			inflateSegsFileQueue,
			pInPlaceBuffer,		//Contains segs file
			segsFileSize,
			pInPlaceBuffer,		//Destination of decompression output
			kMaxInPlaceBufferSize,
			inflateQueue,
			&s_numElementsToDecompress,
			&s_eventFlag,
			eventFlagBits );

	
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
	//	Shutdown Inflate Queue, Inflate Segs File Queue, Inflate Queue,
	//	event flag and taskset.
	//
	//////////////////////////////////////////////////////////////////////////

	ShutdownInflateSegsFileQueue( inflateSegsFileQueue );

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
	free( pInflateSegsFileTaskContext );
	for( uint32_t taskNum = 0 ; taskNum < kNumInflateTasks ; taskNum++ )
	{
		free( pTaskContext[taskNum] );
	}
	free( pInflateSegsFileQueueBuffer );
	free( pInflateQueueBuffer );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Load master file and verify decompression generated the correct data.
	//
	//////////////////////////////////////////////////////////////////////////

	// open uncompressed file
	uint32_t masterFileSize;
	unsigned char* pMasterBuff			= (unsigned char*) LoadFile( masterFilename, &masterFileSize );
	assert( totalUncompressedSize == masterFileSize );

	// verify output uncompressed data is the same as the original master data
	for ( uint32_t i = 0; i < masterFileSize; i++ )
	{
		if ( pMasterBuff[i] != pInPlaceBuffer[i] )
		{
			printf( "Error in test for file %s\n", masterFilename );
			printf( "  At offset 0x%07x,\n", i );
			printf( "  output uncompressed data (0x%02x) does not match master data (0x%02x)\n",
					pInPlaceBuffer[i], pMasterBuff[i] );
			assert( false );
		}
	}

	//Clean up
	FreeFile( pMasterBuff );
	free( pInPlaceBuffer );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Print stats
	//
	//////////////////////////////////////////////////////////////////////////

	float uncompressSecs			= (float)(clockEndDecompress - clockBeginDecompress) / (float)CLOCKS_PER_SEC;
	float compressionPercent		= (1.0f - (float)segsFileSize / (float)masterFileSize) * 100.f;
	float inputDataRate				= ((float)segsFileSize) / (((float)uncompressSecs) * 1024.0f * 1024.0f );
	//float inputDataRatePerSpu		= ((float)segsFileSize) / (((float)kNumInflateTasks) * ((float)uncompressSecs) * 1024.0f * 1024.0f );
	float outputDataRate			= ((float)totalUncompressedSize) / (((float)uncompressSecs) * 1024.0f * 1024.0f );
	//float outputDataRatePerSpu	= ((float)totalUncompressedSize) / (((float)kNumInflateTasks) * ((float)uncompressSecs) * 1024.0f * 1024.0f );

	printf( " %9d %9d    %5.2f%%   %9.5f     %d   %6.2fMB/s    %6.2fMb/s      %4d\n\n",
			(int32_t)masterFileSize, (int32_t)segsFileSize,	compressionPercent,
			uncompressSecs, kNumInflateTasks, inputDataRate, outputDataRate,
			numSegments );
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

uint32_t LoadFileIntoBuffer( const char* filename, void* pBuffer, uint32_t bufferMaxSize )
{
	memset( pBuffer, 0, bufferMaxSize );	//Make sure the buffer's zero-ed.  Do we really need this?

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
	assert( fileSize <= bufferMaxSize );	//file must fit in buffer

	ret = fseek( fp, 0, SEEK_SET );
	assert( ret == 0 );

	size_t numRead = fread( pBuffer, 1, fileSize, fp );
	assert( fileSize == numRead );
	(void) numRead;

	ret = fclose( fp );
	assert( ret == 0 );

	return fileSize;
}

//////////////////////////////////////////////////////////////////////////
