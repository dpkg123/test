/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <string.h>
#include <assert.h>
#include <cell/spurs.h>
#include <stdio.h>
#include <ppu_intrinsics.h>

#include "edge/zlib/edgezlib_ppu.h"
#include "spu/inflate_segs_file_queue_element.h"
#include "inflate_segs_file.h"
#include "../zlib-segcomp-sample/ppu/zlib_segs_ppu.h"


//======================================================================================================

extern char _binary_task_inflate_segs_file_task_spu_elf_start[];   

//======================================================================================================

/** 
 * @brief	Create an Inflate Segs Queue.  This will hold the list of work that is queued up for the
 *			Inflate Segs Task to work on.
 *
 * @note    The queue is a FIFO and will stall as necessary if work is pushed onto a full queue,
 *			so the queue size can safely be lower than the actual maximum number of elements needed.
 *
 * @param	pTaskSet			The taskset that this decompression queue will be associated with.
 * @param	maxNumQueueEntries	The maximum number of entries this queue will hold.
 *								(maximum: 32767)
 * @param	pBuffer				The buffer in main memory to be used for this decompression queue.
 *								Must be aligned to 128 bytes.
 * @param	bufferSize			The size of the provided buffer in main memory.
 *								The required size of this buffer for a given number of queue elements
 *								can be queried by calling "edgeZlibGetDecompressionQueueSize".
 *
 * @return						The handle of the created decompression queue.
 */

InflateSegsFileQHandle CreateInflateSegsFileQueue(
			CellSpursTaskset*					pTaskSet,
			uint32_t							maxNumQueueEntries,
			void*								pBuffer,
			uint32_t							bufferSize )
{
	assert( bufferSize == GetInflateSegsFileQueueSize( maxNumQueueEntries ) );
	assert( (((uint32_t)pBuffer) & 0x7F) == 0 );

	memset( pBuffer, 0, bufferSize );

	//Passed in buffer is used for the CellSpursLFQueue first, followed by the LFQueue buffer
	CellSpursLFQueue* pLfQueue = (CellSpursLFQueue*) pBuffer;
	void* pLfQueueBuffer = &pLfQueue[1];

	int ret = cellSpursLFQueueInitialize(	pTaskSet,
											pLfQueue,
											pLfQueueBuffer,
											sizeof(InflateSegsFileQueueElement),
											maxNumQueueEntries,
											CELL_SPURS_LFQUEUE_ANY2ANY );
    assert( CELL_OK == ret );

	ret = cellSpursLFQueueAttachLv2EventQueue( pLfQueue );
	assert( CELL_OK == ret );

	return pLfQueue;
}

//======================================================================================================

/** 
 * @brief	Returns the required buffer size needed for an Inflate Segs Queue which can hold at most
 *			the specified number of elements at one time.
 *
 * @note	The allocated buffer must be 128 byte aligned.
 *
 * @param	maxNumQueueEntries	The maximum number of entries the queue will hold at one time.
 *								(maximum: 32767)
 *
 * @return						The size of the buffer needed for storing the decompression queue.
 */

uint32_t GetInflateSegsFileQueueSize( uint32_t maxNumQueueEntries )
{
	assert( maxNumQueueEntries > 0 );
	uint32_t size = sizeof(CellSpursLFQueue) + (maxNumQueueEntries * sizeof(InflateSegsFileQueueElement));
	return size;
}

//======================================================================================================

/** 
 * @brief	Returns the required buffer size needed by one one Inflate Segs Task for storing its
 *			context into.
 *
 * @note	The allocated buffer must be 16 byte aligned.
 *
 * @return						The size of the buffer needed for storing the context data of one
 *								Inflate Segs Task.
 */

uint32_t GetInflateSegsFileTaskContextSaveSize( void )
{
	uint32_t kStackSaveSize = 1*2048;
	uint32_t lsSaveSize = kInflateSegsFileTaskMaxHeaderAndTocSize + kStackSaveSize;
	assert( (lsSaveSize & 2047) == 0 );
	return CELL_SPURS_TASK_CONTEXT_SIZE( lsSaveSize );
}

//======================================================================================================

/** 
 * @brief	Create a SPURS Task for running Inflate Segs on one SPU.
 *			The task will pull work work off the Inflate Segs Queue when work exists.
 *			If there is no work to do the task will sleep.  It will store its context to the location
 *			specified by pTaskContext.
 *
 * @param	pTaskSet			The taskset that this Inflate Segs Task should be attached to.
 * @param	pTaskContext		The main memory buffer that the task uses for storing its context to.
 *								The required size of this buffer is provided by calling
 *								"GetInflateSegsFileTaskContextSaveSize".
 *								Buffer must be 16 byte aligned.
 * @param	handle				The handle for the Inflate Segs Queue that this task will be pulling
 *								from.
 *
 * @return						The task id of the created SPURS task.
 */

CellSpursTaskId CreateInflateSegsFileTask(
		CellSpursTaskset*			pTaskSet,
		void*						pTaskContext,
		InflateSegsFileQHandle		handle )
{
	CellSpursTaskId taskId;
    CellSpursTaskArgument taskArgument;

	taskArgument.u32[0] = (uint32_t)handle;

	int ret = cellSpursCreateTask(	pTaskSet,
									&taskId,
									_binary_task_inflate_segs_file_task_spu_elf_start,
									pTaskContext,
									GetInflateSegsFileTaskContextSaveSize(),
									NULL,		//The task will set its own pattern
									&taskArgument );
	assert( CELL_OK == ret );
	(void) ret;

	return taskId;
}

//////////////////////////////////////////////////////////////////////////

/** 
 * @brief	Add a new piece of work to the Inflate Segs Queue.  The work added to this queue will be
 *			taken by one of the Inflate Segs Tasks at the next chance they get.
 */

void AddInflateSegsFileQueueElement(
		InflateSegsFileQHandle		inflateSegsFileQHandle,
		const unsigned char*		pSegsFileData,
		uint32_t					segsFileSize,
		void*						pOutputUncompBuff,
		uint32_t					outputBuffMaxSize,
		EdgeZlibInflateQHandle		inflateQHandle,
		uint32_t*					pWorkToDoCounter,
		CellSpursEventFlag*			pEventFlag,
		uint16_t					eventFlagBits )
{
	InflateSegsFileQueueElement queueEntry __attribute__((aligned(16)));

	assert( (((uint32_t)pSegsFileData) & 0xF) == 0 );
	assert( (pWorkToDoCounter == NULL) || ((*pWorkToDoCounter) > 0) );

	uint32_t segsHeaderAndTocSize	= GetZlibSegsFileHeaderAndTocSize( pSegsFileData );
	assert( segsHeaderAndTocSize <= kInflateSegsFileTaskMaxHeaderAndTocSize );

	queueEntry.m_eaSegsFileData				= (uint32_t) pSegsFileData;
	queueEntry.m_segsFileSize				= segsFileSize;
	queueEntry.m_eaOutputUncompBuff			= (uint32_t) pOutputUncompBuff;
	queueEntry.m_outputBuffMaxSize			= outputBuffMaxSize;
	queueEntry.m_eaInflateQueue				= (uint32_t) inflateQHandle;
	queueEntry.m_eaNumElementsToDecompress	= (uint32_t) pWorkToDoCounter;
	queueEntry.m_eaEventFlag				= (uint32_t) pEventFlag;
	queueEntry.m_eventFlagBits				= eventFlagBits;
	queueEntry.m_segsHeaderAndTocSize		= segsHeaderAndTocSize;

	__lwsync();									//Ensure store to queueEntry is complete before task can run
	CellSpursLFQueue* pLfQueue = (CellSpursLFQueue*) inflateSegsFileQHandle;
	int ret = cellSpursLFQueuePush( pLfQueue, &queueEntry );
	assert( CELL_OK == ret );
	(void) ret;
}

//======================================================================================================

/** 
 * @brief	Shuts down the Inflate Segs Queue.
 *
 * @param	handle						The handle of the Inflate Segs Queue to shutdown.
 */

void ShutdownInflateSegsFileQueue( InflateSegsFileQHandle handle )
{
	CellSpursLFQueue* pLfQueue = (CellSpursLFQueue*) handle;

	CellSpursLFQueueDirection direction;
	int ret = cellSpursLFQueueGetDirection( pLfQueue, &direction );
	assert( CELL_OK == ret );

	ret = cellSpursLFQueueDetachLv2EventQueue( pLfQueue );
	assert( CELL_OK == ret );
}

//======================================================================================================
