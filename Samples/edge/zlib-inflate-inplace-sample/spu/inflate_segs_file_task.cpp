/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "edge/edge_stdint.h"
#include <spu_intrinsics.h>
#include <stddef.h>

#include <cell/spurs/types.h>
#include <cell/spurs/task.h>
#include <cell/spurs/lfqueue.h>
#include <cell/dma.h>

#include "edge/edge_assert.h"
#include "edge/zlib/edgezlib_spu.h"
#include "inflate_segs_file_queue_element.h"
#include "../../zlib-segcomp-sample/spu/zlib_segs_spu.h"

//////////////////////////////////////////////////////////////////////////

//This buffer is 2K aligned so that we can have the minimum number of pages
//to store for this task
static uint8_t s_headerAndTocs[kInflateSegsFileTaskMaxHeaderAndTocSize] __attribute__((aligned(2048)));

//////////////////////////////////////////////////////////////////////////

void cellSpursMain( qword argTask, uint64_t )
{
	//////////////////////////////////////////////////////////////////////////
	//
	//	We need to set the pattern for what LS needs to be stored into our
	//	context area if we yield.  This area will involved the s_headerAndTocs
	//	array, and also the stack.
	//
	//	WARNING: If you edit this task in any manner which requires different
	//	data to be saved on a task yield, then remember to edit this
	//	appropriately.
	//
	//////////////////////////////////////////////////////////////////////////

	register vec_uint4 headerAndTocLsPattern = cellSpursContextGenerateLsPattern(
												(uintptr_t)s_headerAndTocs,
												kInflateSegsFileTaskMaxHeaderAndTocSize	);
	register const vec_uint4 stackLsPattern = { 0x00000000, 0x00000000, 0x00000000, 0x00000001 };	// last 2K for stack
	register vec_uint4 lsPatternToBeSaved = spu_or( stackLsPattern, headerAndTocLsPattern );
	register int ret = cellSpursContextSetLsPattern( lsPatternToBeSaved );
	EDGE_ASSERT( CELL_OK == ret );

	//////////////////////////////////////////////////////////////////////////
	//
	// We use an Inflate Segs File queue, popContainer, and queueEntry to get
	// input to the Inflate Segs File code.
	//
	//////////////////////////////////////////////////////////////////////////

	// get adrs of Inflate Segs File Queue
	uint64_t eaInflateSegsFileQueue = spu_extract( (vec_uint4)argTask, 0 );

	// initialize Inflate Segs File Queue popContainer to queueEntry
	CellSpursLFQueuePopContainer inflateSegsFilePopContainer;
	InflateSegsFileQueueElement inflateSegsFileQueueEntry;
	register const uint32_t kDmaTagId = 0;
	cellSpursLFQueuePopContainerInitialize(	&inflateSegsFilePopContainer, &inflateSegsFileQueueEntry, kDmaTagId );

	while ( 1 )
	{
		// read Inflate Segs File Queue data into queueEntry
		// If there is none, this will yield to another workload
		ret = cellSpursLFQueuePopBegin( eaInflateSegsFileQueue, &inflateSegsFilePopContainer ); //This will sleep if there's no work in the queue
		EDGE_ASSERT( CELL_OK == ret );
		ret = cellSpursLFQueuePopEnd( eaInflateSegsFileQueue, &inflateSegsFilePopContainer );
		EDGE_ASSERT( CELL_OK == ret );

		// get parameters from queue
		register uint32_t eaSegsFileData				= inflateSegsFileQueueEntry.m_eaSegsFileData;
		register uint32_t segsFileSize					= inflateSegsFileQueueEntry.m_segsFileSize;
		register uint32_t eaOutputUncompBuff			= inflateSegsFileQueueEntry.m_eaOutputUncompBuff;
		register uint32_t outputBuffMaxSize				= inflateSegsFileQueueEntry.m_outputBuffMaxSize;
		register uint32_t eaInflateQueue				= inflateSegsFileQueueEntry.m_eaInflateQueue;
		register uint32_t eaNumElementsToDecompress		= inflateSegsFileQueueEntry.m_eaNumElementsToDecompress;
		register uint32_t eaEventFlag					= inflateSegsFileQueueEntry.m_eaEventFlag;
		register uint16_t eventFlagBits					= inflateSegsFileQueueEntry.m_eventFlagBits;
		register uint16_t segsHeaderAndTocSize			= inflateSegsFileQueueEntry.m_segsHeaderAndTocSize;
		EDGE_ASSERT( segsHeaderAndTocSize <= kInflateSegsFileTaskMaxHeaderAndTocSize );

		EDGE_ASSERT( eaInflateQueue );
		EDGE_ASSERT( eaSegsFileData );
		EDGE_ASSERT( (eaSegsFileData & 0xF) == 0 );


		//////////////////////////////////////////////////////////////////////////
		//
		//	Load the header and TOC into LS.
		//
		//////////////////////////////////////////////////////////////////////////

		// input file header
		EdgeSegCompSampleFileHeader* pSegsHeader	= (EdgeSegCompSampleFileHeader*)(void*)&s_headerAndTocs[0];

		register uint32_t sizeofHeaderAndTocsRoundUp16		= (segsHeaderAndTocSize + 0xF) & ~0xF;
		EDGE_ASSERT( sizeofHeaderAndTocsRoundUp16 <= kInflateSegsFileTaskMaxHeaderAndTocSize );
		cellDmaGet( pSegsHeader, eaSegsFileData, sizeofHeaderAndTocsRoundUp16, kDmaTagId, 0, 0 );
		cellDmaWaitTagStatusAll( 1 << kDmaTagId );

		TestValidZlibSegsFile( pSegsHeader, NULL, segsFileSize );


		//////////////////////////////////////////////////////////////////////////
		//
		// Call the routine to iterate over the elements, move them as necessary
		// and add them to the Inflate Queue.
		// Note this routine will regularly check to potentially yield to a higher
		// priority workload.
		//
		//////////////////////////////////////////////////////////////////////////

		MoveBuffersAndAddZlibSegsFileItemsToInflateQueue(	pSegsHeader,
														eaSegsFileData,
														segsFileSize,
														eaInflateQueue,
														eaOutputUncompBuff,
														outputBuffMaxSize,
														eaNumElementsToDecompress,
														eaEventFlag,
														eventFlagBits,
														kDmaTagId );
	}

	// never reach here
}

//////////////////////////////////////////////////////////////////////////
