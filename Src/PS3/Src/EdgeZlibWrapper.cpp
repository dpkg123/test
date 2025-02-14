/* SCE CONFIDENTIAL
 * PLAYSTATION(R)Edge 0.3.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */


#include "Core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "edge/zlib/edgezlib_ppu.h"
#include "EdgeZlibWrapper.h"

#include <ppu_intrinsics.h>

#define EXPECT_TRUE(a) __builtin_expect((a),1)
#define EXPECT_FALSE(a) __builtin_expect((a),0)


//////////////////////////////////////////////////////////////////////////

static void edge_zlib_launch_tasks(int numTasks);

//////////////////////////////////////////////////////////////////////////
//
// BEGIN EDGE ZLIB IMPL
//
//////////////////////////////////////////////////////////////////////////


static int gNumTasks = 1;
static void *gTaskContext[5];
static void* gInflateQueueBuffer = 0;
static CellSpursEventFlag gEventFlag;
static CellSpurs* gpSpurs = 0;
static CellSpursTaskset* gpTaskset = 0;
static bool gOwnTaskset = false;
static EdgeZlibInflateQHandle gInflateQueue;
static uint32_t gEventFlagAllocatedBitMask = 0;

/// Set SPURS instance for Zlib task(s)
void edge_zlib_set_spurs( CellSpurs* pSpurs )
{
	gpSpurs = pSpurs;
}

/// Set Taskset for Zlib task(s) -- optionally, call sample_zlib_create_taskset instead.
void edge_zlib_set_taskset_and_start(CellSpursTaskset* pTaskset, int numTasks)
{
	gpTaskset = pTaskset;
	edge_zlib_launch_tasks(numTasks);
}

/// Create dedicated Taskset for Zlib tasks.
CellSpursTaskset* edge_zlib_create_taskset_and_start(int numTasks)
{
	gpTaskset = (CellSpursTaskset*)appMalloc(sizeof(CellSpursTaskset), 128);
	gOwnTaskset = true;

	// register taskSet
	CellSpursTasksetAttribute taskSetAttribute;
	uint8_t prios[8] = {15, 15, 15, 15, 15, 15, 0, 0};
	int ret = cellSpursTasksetAttributeInitialize( &taskSetAttribute, 0, prios, 8 );
	assert( CELL_OK == ret );
	ret = cellSpursTasksetAttributeSetName( &taskSetAttribute, "edgeZlibTaskSet" );
	assert( CELL_OK == ret );
	ret = cellSpursCreateTasksetWithAttribute( gpSpurs, gpTaskset, &taskSetAttribute );
	assert( CELL_OK == ret );

	edge_zlib_launch_tasks(numTasks);
	
	return gpTaskset;
}

static void edge_zlib_launch_tasks(int numTasks)
{
	assert(numTasks <= 5);
	gNumTasks = numTasks;

	//
	// Setup EventFlag
	//

	int ret = cellSpursEventFlagInitializeIWL(	gpSpurs,
											&gEventFlag,
											CELL_SPURS_EVENT_FLAG_CLEAR_AUTO,
											CELL_SPURS_EVENT_FLAG_SPU2PPU );
	assert( CELL_OK == ret );

	ret = cellSpursEventFlagAttachLv2EventQueue( &gEventFlag );
	assert( CELL_OK == ret );

	//////////////////////////////////////////////////////////////////////////
	//
    //	Initialize Inflate Queue
	//	This will hold the queue of work that the Inflate Task(s) on SPU will
	//	work through
	//
	//////////////////////////////////////////////////////////////////////////

    const uint32_t kMaxNumInflateQueueEntries = 16;
    //const uint32_t kMaxNumInflateQueueEntries = numSegments;
	uint32_t inflateQueueBuffSize = edgeZlibGetInflateQueueSize( kMaxNumInflateQueueEntries );
	gInflateQueueBuffer = appMalloc( inflateQueueBuffSize, kEdgeZlibInflateQueueAlign );
	gInflateQueue = edgeZlibCreateInflateQueue(	gpSpurs,
												kMaxNumInflateQueueEntries,
												gInflateQueueBuffer,
												inflateQueueBuffSize );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Build Inflate Tasks
	//	We want the decompression to be able to run in parallel on multiple
	//	SPUs so we create as many tasks as SPUs that we want it to run
	//	on (ZLIB_SPURS_NUM_TASK).
	//
	//////////////////////////////////////////////////////////////////////////

	assert(numTasks <= 5);
    for( int i = 0 ; i < numTasks ; i++ )
    {
		uint32_t contextSaveSize = edgeZlibGetTaskContextSaveSize();
	    gTaskContext[i] = appMalloc( contextSaveSize, CELL_SPURS_TASK_CONTEXT_ALIGN );
	    edgeZlibCreateInflateTask( gpTaskset, gTaskContext[i], gInflateQueue );
    }
}

/// Create dedicated Taskset for Zlib tasks.
void edge_zlib_shutdown()
{
	//////////////////////////////////////////////////////////////////////////
	//
	//	Shutdown Inflate Queue, event flag and taskset
	//
	//////////////////////////////////////////////////////////////////////////

	edgeZlibShutdownInflateQueue( gInflateQueue );

	int ret = cellSpursEventFlagDetachLv2EventQueue( &gEventFlag );
	assert( CELL_OK == ret );

	if(gOwnTaskset)
	{
		// shutdown taskSet
		ret = cellSpursShutdownTaskset( gpTaskset );
		assert( CELL_OK == ret );
		// wait for all tasks to finish
		ret = cellSpursJoinTaskset( gpTaskset );
		assert( CELL_OK == ret );
	}

    // free alloc'd buffers
    for( int i = 0 ; i < gNumTasks ; i++ )
    {
        appFree( gTaskContext[i] );
    }
	appFree( gInflateQueueBuffer );
}

static inline uint32_t reserve_bit(uint32_t bitIndexEnd = 16)
{
	uint32_t bitIndex;
	uint32_t stackBitMask;

	do {
		stackBitMask = __lwarx(&gEventFlagAllocatedBitMask);
		bitIndex = __cntlzw(~stackBitMask); // count leading bits
		if(EXPECT_TRUE(bitIndex < bitIndexEnd))
			stackBitMask |= (0x80000000u >> bitIndex);
	} while(!__stwcx(&gEventFlagAllocatedBitMask, stackBitMask));

	return bitIndex;
}

static inline void clear_bit(int bitIndex)
{
	uint32_t stackBitMask;

	do {
		stackBitMask = __lwarx(&gEventFlagAllocatedBitMask);
		stackBitMask &= ~(0x80000000u >> bitIndex);
	} while(!__stwcx(&gEventFlagAllocatedBitMask, stackBitMask));
}

int edge_zlib_inflate(const void* CompressedBuffer, int CompressedSize,
						   void* outputUncompressedBuffer, int expectedUncompressedSize)
{
	assert(CompressedSize <= 64*1024);
	assert(expectedUncompressedSize <= 64*1024);

	int bit = (int)reserve_bit(16);
	if(bit < 16)
	{
		uint16_t eventFlagBits = 1 << bit;
		// create one task queue entry (max 64K inflate per entry)
		edgeZlibAddInflateQueueElement(	gInflateQueue,
										CompressedBuffer, CompressedSize,
										(unsigned char*)outputUncompressedBuffer, expectedUncompressedSize,
										0,
										&gEventFlag,
										eventFlagBits,
										kEdgeZlibInflateTask_Inflate
										);	
		return bit;
	}
	return -1;
}

int edge_zlib_memcpy(const void* SrcBuffer, void* DstBuffer, int Size)
{
	assert(Size <= 64*1024);

	int bit = (int)reserve_bit(16);
	if(bit < 16)
	{
		uint16_t eventFlagBits = 1 << bit;
		// create one task queue entry (max 64K inflate per entry)
		edgeZlibAddInflateQueueElement(	gInflateQueue,
										SrcBuffer, Size,
										(unsigned char*)DstBuffer, Size,
										0,
										&gEventFlag,
										eventFlagBits,
										kEdgeZlibInflateTask_Memcpy
										);	
		return bit;
	}
	return -1;
}

void edge_zlib_wait(int handle)
{
	//////////////////////////////////////////////////////////////////////////
	//
	//	Wait for the event flag to be acknowledged in order to tell us the
	//	work is done.
	//
	//////////////////////////////////////////////////////////////////////////

	uint16_t eventFlagBits = 1 << handle;
	int ret = cellSpursEventFlagWait( &gEventFlag, &eventFlagBits, CELL_SPURS_EVENT_FLAG_AND );
	assert( CELL_OK == ret );

	clear_bit(handle);
}


//////////////////////////////////////////////////////////////////////////
//
// END EDGE ZLIB IMPL
//
//////////////////////////////////////////////////////////////////////////

