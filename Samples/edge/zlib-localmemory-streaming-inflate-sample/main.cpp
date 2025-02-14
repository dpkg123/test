/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <cell/gcm.h>
#include <cell/spurs.h>
#include <sys/spu_initialize.h>
#include <spu_printf.h>

#include "edge/zlib/edgezlib_ppu.h"
#include "streaming.h"


//////////////////////////////////////////////////////////////////////////
//
// Local defines & structures
//
//////////////////////////////////////////////////////////////////////////

const uint32_t				kMaxNumInflateTasks = 6;

//////////////////////////////////////////////////////////////////////////

struct InflateTasksData
{
	CellSpursTaskset/*NOT ON STACK*/*	m_pTaskSet;
	CellSpurs*							m_pSpurs;
	EdgeZlibInflateQHandle				m_inflateQueueHandle;
	void*								m_pInflateQueueBuffer;
	uint32_t							m_numInflateTasks;
	void*								m_apTaskContext[kMaxNumInflateTasks];
};

//////////////////////////////////////////////////////////////////////////

// additional data passed to mainCallBack
struct MainCallBackData
{
	const char* m_pMasterFileName;
	uint32_t	m_numInflateTasks;
};


//////////////////////////////////////////////////////////////////////////
//
// Local prototypes
//
//////////////////////////////////////////////////////////////////////////

void zlib_localmemory_streaming_inflate_sample_main( CellSpurs* pSpurs );
void zlib_localmemory_streaming_inflate_sample_callback(	const StreamingQueueEntry* pStreamingQueueEntry,
															StreamingReturnData *pStreamingReturnData );
void initInflateTasks(	InflateTasksData*					pInflateTasksData,
						CellSpursTaskset/*NOT ON STACK*/*	pTaskSet,
						CellSpurs*							pSpurs,
						uint32_t							numInflateTasks,
						uint32_t							numInflateQueueEntries );
void shutDownInflateTasks( InflateTasksData* pInflateTasksData );
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

	zlib_localmemory_streaming_inflate_sample_main( &s_spurs );

	ret = cellSpursFinalize( &s_spurs );
	assert( CELL_OK == ret );

	ret = spu_printf_finalize();
	assert( CELL_OK == ret );

	return 0;
}

//////////////////////////////////////////////////////////////////////////

void zlib_localmemory_streaming_inflate_sample_main( CellSpurs* pSpurs )
{
	// Specify how many SPUs can have Inflate Tasks running on them at once (1 Inflate Task per SPU)
	// Note: In this sample it takes so long for each input segment data to be read, that you will get
	//  about the same speed with just 1 SPU running an Inflate Task as you would with more SPUs running them.
	const uint32_t kNumInflateTasks = 1;

	const uint32_t kNumInflateQueueEntries = 8;
	const uint32_t kNumStreamingQueueEntries = 8;

	//////////////////////////////////////////////////////////////////////////
	//
	//	Do a minimal initialization of the RSX so that we can access
	//  its local memory (VRAM)
	//
	//////////////////////////////////////////////////////////////////////////

    // allocate minimum amount of main memory for use by RSX as the default buffer
    uint32_t gcmDefBufSize = 1024*1024;
    uint8_t* pGcmDefBuf = (uint8_t*)memalign( 1024*1024/*alignment*/, gcmDefBufSize );
    assert( pGcmDefBuf != NULL );

    // of this, use minimum size for default command buffer
    uint32_t gcmDefCmdBufSize = 64*1024;

    // init libgcm
    int ret = cellGcmInit( gcmDefCmdBufSize, gcmDefBufSize, pGcmDefBuf );
	assert( CELL_OK == ret );

    // get ptr to RSX usable local memory & size
    // (this sample will use the start of this memory as the destination to decompress into)
    CellGcmConfig gcmConfig;
    cellGcmGetConfiguration( &gcmConfig );
    void* pGcmLocMem                = gcmConfig.localAddress;
    assert( pGcmLocMem != NULL );
    uint32_t gcmUsableLocMemSize    = gcmConfig.localSize;


	//////////////////////////////////////////////////////////////////////////
	//
	// Initialize Inflate Queue and Inflate Tasks.
	//
	//////////////////////////////////////////////////////////////////////////

    InflateTasksData inflateTasksData;
    static CellSpursTaskset/*NOT ON STACK*/ s_taskSet;
	initInflateTasks( &inflateTasksData, &s_taskSet, pSpurs, kNumInflateTasks, kNumInflateQueueEntries );


	//////////////////////////////////////////////////////////////////////////
	//
	// Initialize streaming system.
	//
	//////////////////////////////////////////////////////////////////////////

    static StreamingData/*NOT ON STACK!*/ s_streamingData;
	EdgeZlibInflateQHandle inflateQueueHandle = inflateTasksData.m_inflateQueueHandle;
	initStreaming(	&s_streamingData,
					pSpurs,
					inflateQueueHandle,
					kNumStreamingQueueEntries );


	// print stats header
	printf("\n  uncompSz    compSz     comp%%  uncompSecs  spus    inputRate    outputRate   numSegs  numNonCompressedSegs\n");
	printf("___________________________________________________________________________________________________________\n");


	//////////////////////////////////////////////////////////////////////////
	//
	// Start decompression (buffer input) of multiple segs files to localmemory.
	//
	//////////////////////////////////////////////////////////////////////////

	const uint32_t kNumFilesToDecompress = 2;

	// Filename of compressed data in segmented format
	static const char* segsFileName[kNumFilesToDecompress] =
	        { "/app_home/assets/elephant-color.bin.zlib.segs", "/app_home/assets/elephant-normal.bin.zlib.segs" };
	
	// Filename of original uncompressed data (used for verification)
	static const char* masterFileName[kNumFilesToDecompress] =
	        { "/app_home/assets/elephant-color.bin", "/app_home/assets/elephant-normal.bin" };

	for( uint32_t fileIndex = 0 ; fileIndex < kNumFilesToDecompress ; fileIndex++ )
	{
        // set user data for use by mainCallBack
        // Note: this is made static to avoid the compiler freeing/reusing the memory while the
        //  created thread (that uses it) executes
        static MainCallBackData mainCallBackData[kNumFilesToDecompress];
		mainCallBackData[fileIndex].m_pMasterFileName = masterFileName[fileIndex];
		mainCallBackData[fileIndex].m_numInflateTasks = kNumInflateTasks;

		// Start streaming decompression (buffered input) of file
		startStreamingDecompressBufferInput(	&s_streamingData,
												segsFileName[fileIndex],
												pGcmLocMem,
												gcmUsableLocMemSize,
												zlib_localmemory_streaming_inflate_sample_callback,
												&mainCallBackData[fileIndex] );
    }


	//////////////////////////////////////////////////////////////////////////
	//
	//	The streaming and decompression is being managed by another thread,
	//	so the main thread could do misc. game logic here.
	//
	//////////////////////////////////////////////////////////////////////////



	//////////////////////////////////////////////////////////////////////////
	//
	// Shut down streaming system.
	//
	//////////////////////////////////////////////////////////////////////////

	// When all decompressions are done, shut down streaming thread and queue
	shutDownStreaming( &s_streamingData );


	//////////////////////////////////////////////////////////////////////////
	//
	// Shut down Inflate Tasks.
	//
	//////////////////////////////////////////////////////////////////////////

	shutDownInflateTasks( &inflateTasksData );

	free( pGcmDefBuf );
}

//////////////////////////////////////////////////////////////////////////

// This callback will be called once the file has been loaded and decompressed.
// Verify compressed file is same as original uncompressed file.
// Free compressed file, print stats.
void zlib_localmemory_streaming_inflate_sample_callback(	const StreamingQueueEntry* pStreamingQueueEntry,
															StreamingReturnData *pStreamingReturnData )
{
	const char* pSegsFileName = pStreamingQueueEntry->m_pSegsFileName;
	printf( "%s\n", pSegsFileName );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Load master file and verify decompression generated the correct data.
	//
	//////////////////////////////////////////////////////////////////////////

	// Open uncompressed file
	uint32_t masterFileSize;
    MainCallBackData* pCallBackData = (MainCallBackData*)pStreamingQueueEntry->m_pCallBackData;
	const char* pMasterFileName     = pCallBackData->m_pMasterFileName;
	unsigned char* pMasterBuff = (unsigned char*) LoadFile( pMasterFileName, &masterFileSize );
    uint32_t totalUncompressedSize = pStreamingReturnData->m_totalUncompressedSize;
	assert( totalUncompressedSize == masterFileSize );

	// Verify output uncompressed data is the same as the original master data
  	unsigned char* pOutputUncompBuff = pStreamingReturnData->m_pOutputUncompBuff;
	for ( uint32_t i = 0; i < masterFileSize; i++ )
	{
		// Note: This reads from localMemory (which is *slow*), but this is only for
		//  verification code used in this sample, which wouldn't be in final game code.
		if ( pMasterBuff[i] != pOutputUncompBuff[i] )
		{
			printf( "Error in test for file %s\n", pMasterFileName );
			printf( "  At offset 0x%07x,\n", i );
			printf( "  output uncompressed data (0x%02x) does not match master data (0x%02x)\n",
					pOutputUncompBuff[i], pMasterBuff[i] );
			assert( false );
		}
	}

	// Free master file
	FreeFile( pMasterBuff );
	
	// Note: The decompressed memory (pOutputUncompBuff) is in localMemory
	//  and this sample didn't allocate it so there is nothing to free.


	//////////////////////////////////////////////////////////////////////////
	//
	//	Print stats
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t numInflateTasks		= pCallBackData->m_numInflateTasks;

	clock_t  clockBeginDecompress   = pStreamingReturnData->m_clockBeginDecompress;
	clock_t  clockEndDecompress     = pStreamingReturnData->m_clockEndDecompress;
    uint32_t segsFileSize           = pStreamingReturnData->m_segsFileSize;
	float uncompressSecs			= (float)(clockEndDecompress - clockBeginDecompress) / (float)CLOCKS_PER_SEC;
	float compressionPercent		= (1.0f - (float)segsFileSize / (float)masterFileSize) * 100.f;
	float inputDataRate				= ((float)segsFileSize) / (((float)uncompressSecs) * 1024.0f * 1024.0f );
	//float inputDataRatePerSpu		= ((float)segsFileSize) / (((float)numInflateTasks) * ((float)uncompressSecs) * 1024.0f * 1024.0f );
	float outputDataRate			= ((float)totalUncompressedSize) / (((float)uncompressSecs) * 1024.0f * 1024.0f );
	//float outputDataRatePerSpu	= ((float)totalUncompressedSize) / (((float)numInflateTasks) * ((float)uncompressSecs) * 1024.0f * 1024.0f );

    uint32_t numSegments            = pStreamingReturnData->m_numSegments;
    uint32_t numNonCompressedSegs   = pStreamingReturnData->m_numNonCompressedSegs;
	printf( " %9d %9d    %5.2f%%   %9.5f     %d   %6.2fMB/s    %6.2fMb/s      %4d                  %4d\n\n",
			masterFileSize, segsFileSize, compressionPercent,
			uncompressSecs, numInflateTasks, inputDataRate, outputDataRate,
			numSegments, numNonCompressedSegs );
}

//////////////////////////////////////////////////////////////////////////

// Set InflateTasksData data.
// Init Inflate task set and queue.
// Build & run inflate task(s) (1 per SPU).
void initInflateTasks(	InflateTasksData*					pInflateTasksData,
						CellSpursTaskset/*NOT ON STACK*/*	pTaskSet,
						CellSpurs*							pSpurs,
						uint32_t							numInflateTasks,
						uint32_t							numInflateQueueEntries )
{
	// copy params to return structure
	pInflateTasksData->m_pTaskSet			= pTaskSet;
	pInflateTasksData->m_pSpurs			= pSpurs;
	pInflateTasksData->m_numInflateTasks	= numInflateTasks;


	//////////////////////////////////////////////////////////////////////////
	//
	//	Initialize taskset.
	//
	//////////////////////////////////////////////////////////////////////////

	// register taskSet
	CellSpursTasksetAttribute taskSetAttribute;
	uint8_t prios[8] = {15, 15, 15, 15, 15, 15, 0, 0};
	int ret = cellSpursTasksetAttributeInitialize( &taskSetAttribute, 0, prios, 8 );
	assert( CELL_OK == ret );
	ret = cellSpursTasksetAttributeSetName( &taskSetAttribute, "edgeZlibTaskSet" );
	assert( CELL_OK == ret );
	ret = cellSpursCreateTasksetWithAttribute( pSpurs, pTaskSet, &taskSetAttribute );
	assert( CELL_OK == ret );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Initialize Inflate Queue.
	//	This will hold the queue of work that the Inflate Task(s) on SPU will
	//	pull work from.
	//
	//////////////////////////////////////////////////////////////////////////

	// allocate inflateQueue buffer
	uint32_t inflateQueueBuffSize = edgeZlibGetInflateQueueSize( numInflateQueueEntries );
	void* pInflateQueueBuffer = memalign( EDGE_ZLIB_INFLATE_QUEUE_ALIGN, inflateQueueBuffSize );
	assert( pInflateQueueBuffer != NULL );
	pInflateTasksData->m_pInflateQueueBuffer = pInflateQueueBuffer;

	// init inflateQueue
	EdgeZlibInflateQHandle inflateQueueHandle = edgeZlibCreateInflateQueue(
														pSpurs,
														numInflateQueueEntries,
														pInflateQueueBuffer,
														inflateQueueBuffSize );
	pInflateTasksData->m_inflateQueueHandle = inflateQueueHandle;


	//////////////////////////////////////////////////////////////////////////
	//
	//	Build Inflate Tasks.
	//	We want the decompression to be able to run in parallel on multiple
	//	SPUs so we create as many tasks as SPUs that we want it to run
	//	on (numInflateTasks).
	//
	//////////////////////////////////////////////////////////////////////////

	assert( numInflateTasks <= kMaxNumInflateTasks );

	uint32_t contextSaveSize = edgeZlibGetInflateTaskContextSaveSize();
	for( uint32_t taskNum = 0 ; taskNum < numInflateTasks ; taskNum++ )
	{
		void *pTaskContext = memalign( CELL_SPURS_TASK_CONTEXT_ALIGN, contextSaveSize );
		pInflateTasksData->m_apTaskContext[taskNum] = pTaskContext;
		edgeZlibCreateInflateTask( pTaskSet, pTaskContext, inflateQueueHandle );
	}
}

//////////////////////////////////////////////////////////////////////////

// Shut down inflate task set & wait for inflate tasks to finish.
// Shut down inflate Queue and free buffer.
// Free context buffers, clear InflateTasksData
void shutDownInflateTasks( InflateTasksData* pInflateTasksData )
{
	uint32_t numInflateTasks = pInflateTasksData->m_numInflateTasks;

	// shutdown taskSet
	CellSpursTaskset/*NOT ON STACK*/* pTaskSet = pInflateTasksData->m_pTaskSet;
	int ret = cellSpursShutdownTaskset( pTaskSet );
	assert( CELL_OK == ret );

	// wait for all tasks to finish
	ret = cellSpursJoinTaskset( pTaskSet );
	assert( CELL_OK == ret );

	// shutdown inflate queue
	EdgeZlibInflateQHandle inflateQueueHandle = pInflateTasksData->m_inflateQueueHandle;
	edgeZlibShutdownInflateQueue( inflateQueueHandle );

	// free inflate queue buffer
	void* pInflateQueueBuffer = pInflateTasksData->m_pInflateQueueBuffer;
	free( pInflateQueueBuffer );

	// free buffers for context
	for( uint32_t taskNum = 0 ; taskNum < numInflateTasks ; taskNum++ )
	{
		void *pTaskContext = pInflateTasksData->m_apTaskContext[taskNum];
		free( pTaskContext );
	}

    // Zero out inflateTasksData for safety in future
    memset( pInflateTasksData, 0, sizeof(InflateTasksData) );
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
