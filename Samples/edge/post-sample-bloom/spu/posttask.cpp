/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#include "edge/edge_stdint.h"
#include <spu_intrinsics.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <cell/spurs/types.h>
#include <cell/spurs/task.h>
#include <cell/spurs/trace.h>
#include <cell/dma.h>
#include <cell/atomic.h>

#include "edge/edge_assert.h"
#include "edge/post/edgepost.h"
#include "edge/post/edgepost_framework_spu.h"
#include "edgepostjobheader.h"

#define CONTROL_DMA_TAG	0
#define INPUT_DMA_TAG	1
#define OUTPUT_DMA_TAG	2
#define TRACE_DMA_TAG	3


static EdgePostTileCallback _StageEnterCallback( const EdgePostProcessStage* pStage, void* pJobCodeArea )
{
	(void)pStage;

	// wait for previous trace DMA
	cellDmaWaitTagStatusAll( 1 << TRACE_DMA_TAG );

	// get GUID of current job
	const vec_uchar16 guidShufflePattern = {0,1,4,5,8,9,12,13,0,1,4,5,8,9,12,13};
	qword insn = si_roti( *(const qword*)pJobCodeArea, 7 );
	uint64_t guid = si_to_ullong( si_shufb( insn, insn, (const qword)guidShufflePattern ) );

	// START
	static CellSpursTracePacket s_start_packet __attribute__((__aligned__(16)));
	s_start_packet.header.tag = CELL_SPURS_TRACE_TAG_START;
	s_start_packet.data.start.module[0] = 'P';
	s_start_packet.data.start.module[1] = 'O';
	s_start_packet.data.start.module[2] = 'S';
	s_start_packet.data.start.module[3] = 'T';
	s_start_packet.data.start.ls = (uint16_t)((uint32_t)pJobCodeArea >> 2);
	s_start_packet.data.start.level = 2;
	cellSpursPutTrace( &s_start_packet, TRACE_DMA_TAG);

	// GUID
	static CellSpursTracePacket s_guid_packet __attribute__((__aligned__(16)));
	s_guid_packet.header.tag = CELL_SPURS_TRACE_TAG_GUID;
	s_guid_packet.data.guid = guid;
	cellSpursPutTrace( &s_guid_packet, TRACE_DMA_TAG);

	// cast raw pointer to job header
	EdgePostJobHeader* pHeader = (EdgePostJobHeader*)pJobCodeArea;
	EDGE_ASSERT( 0xda7aba5e == pHeader->magicCode);

	// initialize bss to zero
	uint32_t c = pHeader->bssSize >> 4;
	qword* _bss = (qword*)( (uint8_t*)pHeader + pHeader->bssOffset);
	const qword zero = (qword)spu_splats( 0 );
	for ( uint32_t e = 0; e < c; ++e)
		_bss[e] = zero;

	// call c-tors
	typedef void(*VoidFunc_t)();
	uint32_t* ctorPtr = (uint32_t*)( (uint8_t*)pHeader + pHeader->ctorList);
	while ( *(--ctorPtr) != 0xFFFFFFFF )
		((VoidFunc_t)((uint8_t*)pHeader + *ctorPtr))();

	// return entry point
	return (EdgePostTileCallback)( uint32_t(pJobCodeArea) + pHeader->entryOffset );
}

static void _StageExitCallback( const EdgePostProcessStage* pStage, void* pJobCodeArea )
{
	(void)pStage;

	// cast to job header
	EdgePostJobHeader* pHeader = (EdgePostJobHeader*)pJobCodeArea;

	// call d-tors
	typedef void(*VoidFunc_t)();
	uint32_t* dtorPtr = (uint32_t*)( (uint8_t*)pHeader + pHeader->dtorList);
	while ( *(++dtorPtr) != 0 )
		((VoidFunc_t)((uint8_t*)pHeader + *dtorPtr))();

	// get GUID of current job
	const vec_uchar16 guidShufflePattern = {0,1,4,5,8,9,12,13,0,1,4,5,8,9,12,13};
	qword insn = si_roti( *(const qword*)pJobCodeArea, 7 );
	uint64_t guid = si_to_ullong( si_shufb( insn, insn, (const qword)guidShufflePattern ) );

	// output trace
	static CellSpursTracePacket s_stop_packet __attribute__((__aligned__(16)));
	cellDmaWaitTagStatusAll( 1 << TRACE_DMA_TAG );
	s_stop_packet.header.tag = CELL_SPURS_TRACE_TAG_STOP;
	s_stop_packet.data.guid = guid;
	cellSpursPutTrace( &s_stop_packet, TRACE_DMA_TAG);
}

static unsigned _Poll()
{
	// Poll here if you want to share this SPU with others high priority tasks/jobchains
	uint32_t pollVal = cellSpursTaskPoll();
	return ( pollVal & CELL_SPURS_TASK_POLL_FOUND_WORKLOAD ) ? 1 : 0;
}

//////////////////////////////////////////////////////////////////////////
void cellSpursMain( qword argTask, uint64_t )
{
	// get edgePost workload address
	uint32_t workloadEa = spu_extract( (vec_uint4)argTask, 0 );
	uint32_t completionCounterEa = spu_extract( (vec_uint4)argTask, 1 );

	// a buffer in bss area
	static char buffer[EDGEPOST_DEFAULT_HEAP_SIZE];

	// setup edgePost configuration
	EdgePostSpuConfig config;
	memset(&config, 0, sizeof(config));
	config.heapStart = buffer;
	config.heapSize = EDGEPOST_DEFAULT_HEAP_SIZE;
	config.controlDmaTag = CONTROL_DMA_TAG;
	config.inputDmaTag = INPUT_DMA_TAG;
	config.outputDmaTag = OUTPUT_DMA_TAG;
	config.pollCallback = _Poll;
	config.stageEnterCallback = _StageEnterCallback;
	config.stageExitCallback = _StageExitCallback;

	while (1)
	{
		// Wait for a signal
		cellSpursWaitSignal();

		while (1)
		{
			// (re)set configuration
			edgePostSetSpuConfig( &config );

			// call main execution function
			int ret = edgePostRunWorkload( workloadEa );

			if ( ret == EDGEPOST_WORKLOAD_PREEMPTED)
			{
				// wait for last trace DMA
				cellDmaWaitTagStatusAll( 1 << TRACE_DMA_TAG );
				
				// Yield to higher priority workloads
				cellSpursYield();
			}

			if ( ret == EDGEPOST_WORKLOAD_ENDED)
			{
				// workload is finished, exit inner loop
				break;
			}

			// Another task of the group is finishing off the last tile of current stage.
		}

		// wait for last trace DMA
		cellDmaWaitTagStatusAll( 1 << TRACE_DMA_TAG );

		// signal completion
		static uint32_t buf[32] __attribute__((aligned(128)));
		cellAtomicIncr32( buf, completionCounterEa);
	}

	// never gets here
}


