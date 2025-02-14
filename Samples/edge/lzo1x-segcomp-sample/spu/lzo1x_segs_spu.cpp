/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "edge/edge_stdint.h"
#include <string.h>
#include <cell/spurs.h>

#include "edge/edge_dma.h"
#include "edge/edge_atomic.h"
#include "edge/edge_printf.h"
#include "edge/edge_assert.h"
#include "edge/lzo/edgelzo_spu.h"
#include "../common/segcompfile.h"
#include "edge/lzo/edgelzo1x_inflate_queue_element.h"
#include "lzo1x_segs_spu.h"


/////////////////////////////////////////////////////////////////////////

enum
{
	kEdgeSegCompSampleOverAllocateBufferSize = 128,		//Need to reserve more than maximum buffer size due to mis-alignment of incoming data
};
static uint8_t s_segmentTransferBuffer[kEdgeLzo1xInflateInputBufferMaxSize + kEdgeSegCompSampleOverAllocateBufferSize] __attribute__((aligned(128)));

/////////////////////////////////////////////////////////////////////////

static void moveBuffer( uint32_t eaDest, uint32_t eaSource, uint32_t size, uint32_t dmaTagId );

/////////////////////////////////////////////////////////////////////////

void MoveBuffersAndAddLzo1xSegsFileItemsToInflateQueue(
			const EdgeSegCompSampleFileHeader* pSegsHeader,	// Pointer to where the header and TOC have been pre-loaded into LS
			uint32_t eaSegsFileData,						// Effective Address of segs file data, 16 aligned
			uint32_t segsFileSize,							// size of the input segs file
			uint32_t eaInflateQueue,						// Effective Address of the Inflate Queue
			uint32_t eaOutputUncompBuff,					// Effective Address of output buffer,
			uint32_t outputBuffMaxSize,						// maximum size for the data sent to the output buffer
			uint32_t eaNumElementsToDecompress,				// Effective Address of counter of #elements to decompress
			uint32_t eaEventFlag,							// Effective Address of eventFlag to trigger when all uncompressed data is stored
			uint16_t eventFlagBits,							// This is the value to set to the event flag
			uint32_t dmaTagId )								// DMA tag to use for transfers
{
	TestValidLzo1xSegsFile( pSegsHeader, NULL, segsFileSize );	//Check this is a legitimate file header before we do anything else

	uint32_t numSegments					= pSegsHeader->m_numSegments;
	uint32_t totalUncompressedSize			= pSegsHeader->m_totalUncompressedSize;
	if ( totalUncompressedSize > outputBuffMaxSize )
	{
		EDGE_PRINTF( "EDGE LZO1X ERROR: Segs file will decompress to bigger than output buffer size (%d > %d)\n", totalUncompressedSize, outputBuffMaxSize );
		EDGE_ASSERT( false );
	}

	// pointer to the segs file's table of contents in LS
	const EdgeSegCompSampleTocElement* pSegsToc	= (const EdgeSegCompSampleTocElement*) &pSegsHeader[1];

	//////////////////////////////////////////////////////////////////////////
	//
	//	If the input data buffer (which contains the segs file) and the
	//	output uncompressed buffer don't overlap, then call
	//	AddLzo1xSegsFileItemsToInflateQueue instead.
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t eaOutputUncompBuffEnd	= eaOutputUncompBuff + outputBuffMaxSize;
	uint32_t eaSegsFileDataEnd		= eaSegsFileData + segsFileSize;
	if ( ( eaSegsFileDataEnd <= eaOutputUncompBuff ) || ( eaSegsFileData >= eaOutputUncompBuffEnd ) )
	{
		//If the input data and the output data do not overlap, then we can just schedule the work
		//straight away and there is no need to move the data buffers around.
		//So just call AddLzo1xSegsFileItemsToInflateQueue to do this.
		AddLzo1xSegsFileItemsToInflateQueue(	pSegsHeader,
												eaSegsFileData,
												segsFileSize,
												eaInflateQueue,
												eaOutputUncompBuff,
												outputBuffMaxSize,
												eaNumElementsToDecompress,
												eaEventFlag,
												eventFlagBits,
												dmaTagId );
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	//
	//	Spread out the segments so that the input data is sat in the buffer
	//	which the output data will be going to.
	//	This is done it two stages.  Firstly the segments at the end are
	//	copied backwards, iterating backwards through the segments.
	//	Then the segments at the start are copied forwards, iterating forwards
	//	through the segments.
	//	Once each segment of data has been moved to the appropriate location,
	//	an item of work is added to the Inflate Queue so that it will be
	//	decompressed.
	//
	//////////////////////////////////////////////////////////////////////////

	const uint32_t kYieldFrequency = 32;	//call poll and yield after processing every 32 segments
	int yieldCount = 0;
	int ret;
	(void) ret;

	uint32_t numCompressedSegs				= 0;	//Count how many segments are queued up for decompression by the SPU
	uint32_t numNonCompressedSegs			= 0;	//Some segments might have been stored raw because compression actually made them larger

	//Work backwards through the segments, spreading them out by moving them towards the end of the buffer.
	//These have to be done in order working backwards so that the copy of later segments is done before
	//moving the earlier segments overwrites that location.

	uint32_t outputOffset = totalUncompressedSize;

	for ( int segNo = numSegments-1; segNo >= 0; --segNo )
	{
		const EdgeSegCompSampleTocElement* pSegElt	= &pSegsToc[segNo];    
		uint32_t segUncompSize						= pSegElt->m_uncompressedSize;
		if ( segUncompSize == 0 )
			segUncompSize += 64*1024;
		uint32_t segCompSize						= pSegElt->m_compressedSize;
		if ( segCompSize == 0 )
			segCompSize += 64*1024;
		uint32_t segOffset							= pSegElt->m_fileOffset & ~1;
		uint32_t isStoredCompressed					= pSegElt->m_fileOffset & 1;

		EDGE_ASSERT( outputOffset >= segUncompSize );
		outputOffset = outputOffset - segUncompSize;

		uint32_t eaInputSegBuff				 = eaSegsFileData + segOffset;
		EDGE_ASSERT( (eaInputSegBuff & 0xF) == 0 );
		uint32_t eaOutputUncompSeg			 = eaOutputUncompBuff + outputOffset;

		if ( eaOutputUncompSeg <= eaInputSegBuff )
		{
			//Don't do the copy if it's going to be moving backwards because we could potentially
			//be overwriting valid data that is still to be moved.
			//This copy will be handled in the second loop.
			break;
		}

		//Move the buffer from its current location in main memory to its new location
		moveBuffer( eaOutputUncompSeg , eaInputSegBuff, segCompSize, dmaTagId );

		if ( isStoredCompressed )
		{
			//	This segments needs decompressing, so add an entry to the Inflate Queue for it.

			// If there is not enough room in the queue, this will yield
			edgeLzo1xAddInflateQueueElement(	eaInflateQueue,
											eaOutputUncompSeg, segCompSize,
											eaOutputUncompSeg, segUncompSize,
											eaNumElementsToDecompress,	//This will be decremented by 1 when this element is decompressed
											eaEventFlag,				//When *eaNumElementsToDecompress decrements to zero, this event will be triggered
											eventFlagBits,
											kEdgeLzo1xInflateTask_Inflate,
											dmaTagId
										);	

			++numCompressedSegs;
		}
		else
		{
			//This segment was stored uncompressed so now that it has been moved to its final location, nothing else need be done.

			++numNonCompressedSegs;
			EDGE_ASSERT( segCompSize == segUncompSize );
		}

		++yieldCount;
		if ( (yieldCount % kYieldFrequency) == 0 )	//call poll and yield after processing every n segments
		{

			uint32_t pollVal = cellSpursTaskPoll();
			if ( pollVal & CELL_SPURS_TASK_POLL_FOUND_WORKLOAD )	//Don't switch to other tasks in this task set, only higher priority workloads
			{
				ret = cellSpursYield();
				EDGE_ASSERT( CELL_OK == ret );
			}
		}
	}

	//Some segments at the start may need copying forwards, so now work forwards through them,
	//spreading them out by moving them towards the front of the buffer.
	outputOffset = 0;

	for ( uint32_t segNo = 0; segNo < numSegments; ++segNo )
	{
		const EdgeSegCompSampleTocElement* pSegElt	= &pSegsToc[segNo];    
		uint32_t segUncompSize						= pSegElt->m_uncompressedSize;
		if ( segUncompSize == 0 )
			segUncompSize += 64*1024;
		uint32_t segCompSize						= pSegElt->m_compressedSize;
		if ( segCompSize == 0 )
			segCompSize += 64*1024;
		uint32_t segOffset							= pSegElt->m_fileOffset & ~1;
		uint32_t isStoredCompressed					= pSegElt->m_fileOffset & 1;

		uint32_t eaInputSegBuff						= eaSegsFileData + segOffset;
		EDGE_ASSERT( (eaInputSegBuff & 0xF) == 0 );
		uint32_t eaOutputUncompSeg					= eaOutputUncompBuff + outputOffset;

		outputOffset += segUncompSize;
		EDGE_ASSERT( outputOffset <= totalUncompressedSize );

		if ( eaInputSegBuff <= eaOutputUncompSeg)
		{
			//Once we reach the point of copying forwards we can stop because these
			//will already have been handled by the first loop.
			break;
		}

		//Move the buffer from its current location in main memory to its new location
		moveBuffer( eaOutputUncompSeg , eaInputSegBuff, segCompSize, dmaTagId );

		if ( isStoredCompressed )
		{
			//	This segments needs decompressing, so add an entry to the Inflate Queue for it.

			// If there is not enough room in the queue, this will yield
			edgeLzo1xAddInflateQueueElement(	eaInflateQueue,
											eaOutputUncompSeg, segCompSize,
											eaOutputUncompSeg, segUncompSize,
											eaNumElementsToDecompress,	//This will be decremented by 1 when this element is decompressed
											eaEventFlag,				//When *eaNumElementsToDecompress decrements to zero, this event will be triggered
											eventFlagBits,
											kEdgeLzo1xInflateTask_Inflate,
											dmaTagId
										);	

			++numCompressedSegs;
		}
		else
		{
			//This segment was stored uncompressed so now that it has been moved to its final location, nothing else need be done.

			++numNonCompressedSegs;
			EDGE_ASSERT( segCompSize == segUncompSize );
		}

		++yieldCount;
		if ( (yieldCount % kYieldFrequency) == 0 )	//call poll and yield after processing every n segments
		{
			uint32_t pollVal = cellSpursTaskPoll();
			if ( pollVal & CELL_SPURS_TASK_POLL_FOUND_WORKLOAD )	//Don't switch to other tasks in this task set, only higher priority workloads
			{
				ret = cellSpursYield();
				EDGE_ASSERT( CELL_OK == ret );
			}
		}
	}
	EDGE_ASSERT( numCompressedSegs <= numSegments );
	EDGE_ASSERT( (numNonCompressedSegs + numCompressedSegs) == numSegments );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Since the Inflate Task is not going to be decompressing some of the
	//	segments we need to do the decrement here to say that they're done.
	//	And if necessary set the event flag.
	//
	//////////////////////////////////////////////////////////////////////////

	if ( numNonCompressedSegs > 0 )
	{
		bool setEventFlag = true;
		if ( eaNumElementsToDecompress )
		{
			static unsigned char s_atomicBuffer[128] __attribute__((aligned(128)));	//Only used temporarily for atomic transactions

			setEventFlag = false;
			uint32_t old = EDGE_ATOMIC_SUB_32( (uint32_t*)&s_atomicBuffer[0], eaNumElementsToDecompress, numNonCompressedSegs );
			if ( numNonCompressedSegs > old )
			{
				EDGE_PRINTF( "EDGE LZO1X ERROR: Attempted to decrement a work-to-do counter by more than counters current value (%d > %d)\n", numNonCompressedSegs, old );
				EDGE_ASSERT( false );
			}

			if ( old == numNonCompressedSegs )	//if just decremented to zero
			{
				setEventFlag = true;
			}
		}

		if ( setEventFlag )
		{
			if ( eaEventFlag )
			{
				ret = cellSpursEventFlagSet( eaEventFlag, eventFlagBits );
				EDGE_ASSERT( CELL_OK == ret );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////

void AddLzo1xSegsFileItemsToInflateQueue(
			const EdgeSegCompSampleFileHeader* pSegsHeader,	//Pointer to where the header and TOC have been pre-loaded into LS
			uint32_t eaSegsFileData,						// Effective Address of segs file data, 16 aligned
			uint32_t segsFileSize,							// size of the input segs file
			uint32_t eaInflateQueue,						// Effective Address of the Inflate Queue
			uint32_t eaOutputUncompBuff,					// Effective Address of output buffer,
			uint32_t outputBuffMaxSize,						// maximum size for the data sent to the output buffer
			uint32_t eaNumElementsToDecompress,				// Effective Address of counter of #elements to decompress
			uint32_t eaEventFlag,							// Effective Address of eventFlag to trigger when all uncompressed data is stored
			uint16_t eventFlagBits,							// This is the value to set to the event flag
			uint32_t dmaTagId )								// DMA tag to use for transfers
{
	TestValidLzo1xSegsFile( pSegsHeader, NULL, segsFileSize );	//Check this is a legitimate file header before we do anything else

	uint32_t numSegments					= pSegsHeader->m_numSegments;
	uint32_t totalUncompressedSize			= pSegsHeader->m_totalUncompressedSize;
	if ( totalUncompressedSize > outputBuffMaxSize )
	{
		EDGE_PRINTF( "EDGE LZO1X ERROR: Segs file will decompress to bigger than output buffer size (%d > %d)\n", totalUncompressedSize, outputBuffMaxSize );
		EDGE_ASSERT( false );
	}

	// pointer to the segs file's table of contents in LS
	const EdgeSegCompSampleTocElement* pSegsToc	= (const EdgeSegCompSampleTocElement*) &pSegsHeader[1];

	//////////////////////////////////////////////////////////////////////////
	//
	//	This function only handles the cases where the input data buffer (which
	//	contains the segs file) and the output uncompressed buffer don't overlap.
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t eaOutputUncompBuffEnd	= eaOutputUncompBuff + outputBuffMaxSize;
	uint32_t eaSegsFileDataEnd		= eaSegsFileData + segsFileSize;
	(void) eaOutputUncompBuffEnd;
	(void) eaSegsFileDataEnd;
	EDGE_ASSERT( ( eaSegsFileDataEnd <= eaOutputUncompBuff ) || ( eaSegsFileData >= eaOutputUncompBuffEnd ) );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Iterate over the TOC and add to the Inflate Queue for decompressing
	//	the compressed segments
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t numCompressedSegs				= 0;	//Count how many segments are queued up for decompression by the SPU
	uint32_t numNonCompressedSegs			= 0;	//Some segments might have been stored raw because compression actually made them larger

	uint32_t outputOffset = 0;

	for ( uint32_t segNo = 0; segNo < numSegments; ++segNo )
	{
		const EdgeSegCompSampleTocElement* pSegElt	= &pSegsToc[segNo];    
		uint32_t segUncompSize						= pSegElt->m_uncompressedSize;
		if ( segUncompSize == 0 )
			segUncompSize += 64*1024;
		uint32_t segCompSize						= pSegElt->m_compressedSize;
		if ( segCompSize == 0 )
			segCompSize += 64*1024;
		uint32_t segOffset							= pSegElt->m_fileOffset & ~1;
		uint32_t isStoredCompressed					= pSegElt->m_fileOffset & 1;

		uint32_t eaInputSegBuff						= eaSegsFileData + segOffset;
		EDGE_ASSERT( (eaInputSegBuff & 0xF) == 0 );
		uint32_t eaOutputUncompSeg					= eaOutputUncompBuff + outputOffset;

		outputOffset += segUncompSize;
		EDGE_ASSERT( outputOffset <= totalUncompressedSize );

		if ( isStoredCompressed )
		{
			//	This segments needs decompressing, so add an entry to the Inflate Queue for it.

			// If there is not enough room in the queue, this will yield
			edgeLzo1xAddInflateQueueElement(	eaInflateQueue,
											eaInputSegBuff, segCompSize,
											eaOutputUncompSeg, segUncompSize,
											eaNumElementsToDecompress,	//This will be decremented by 1 when this element is decompressed
											eaEventFlag,				//When *eaNumElementsToDecompress decrements to zero, this event will be triggered
											eventFlagBits,
											kEdgeLzo1xInflateTask_Inflate,
											dmaTagId
										);	

			++numCompressedSegs;
		}
		else
		{
			//This segment was stored uncompressed so now that it has been moved to its final location, nothing else need be done.
			++numNonCompressedSegs;
			EDGE_ASSERT( segCompSize == segUncompSize );
			//Move the buffer from its current location in main memory to its new location
			moveBuffer( eaOutputUncompSeg , eaInputSegBuff, segCompSize, dmaTagId );
		}
	}
	EDGE_ASSERT( numCompressedSegs <= numSegments );
	EDGE_ASSERT( (numNonCompressedSegs + numCompressedSegs) == numSegments );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Since the Inflate Task is not going to be decompressing some of the
	//	segments we need to do the decrement here to say that they're done.
	//	And if necessary set the event flag.
	//
	//////////////////////////////////////////////////////////////////////////

	if ( numNonCompressedSegs > 0 )
	{
		bool setEventFlag = true;
		if ( eaNumElementsToDecompress )
		{
			static unsigned char s_atomicBuffer[128] __attribute__((aligned(128)));	//Only used temporarily for atomic transactions

			setEventFlag = false;
			uint32_t old = EDGE_ATOMIC_SUB_32( (uint32_t*)&s_atomicBuffer[0], eaNumElementsToDecompress, numNonCompressedSegs );
			if ( numNonCompressedSegs > old )
			{
				EDGE_PRINTF( "EDGE LZO1X ERROR: Attempted to decrement a work-to-do counter by more than counters current value (%d > %d)\n", numNonCompressedSegs, old );
				EDGE_ASSERT( false );
			}

			if ( old == numNonCompressedSegs )	//if just decremented to zero
			{
				setEventFlag = true;
			}
		}

		if ( setEventFlag )
		{
			if ( eaEventFlag )
			{
				int ret = cellSpursEventFlagSet( eaEventFlag, eventFlagBits );
				(void) ret;
				EDGE_ASSERT( CELL_OK == ret );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////

void TestValidLzo1xSegsFile( const EdgeSegCompSampleFileHeader* pSegsHeader, const char* segFileName, uint32_t segsFileSize )
{
	EDGE_ASSERT( (((uint32_t)pSegsHeader) & 0xF) == 0 );

	if ( EdgeSegCompSampleFileHeader::kMagic != pSegsHeader->m_magic )
	{
		EDGE_PRINTF( "EDGE LZO1X ERROR: Invalid segs file\n" );
		EDGE_ASSERT( false );
	}

	if ( EdgeSegCompSampleFileHeader::kFileVersionNumber != pSegsHeader->m_versionNumber )
	{
		if ( segFileName )
		{
			EDGE_PRINTF( "File version number conflict\n\t\"%s\" is version number %d\n\tExpected version number %d\n",
				segFileName,
				pSegsHeader->m_versionNumber,
				EdgeSegCompSampleFileHeader::kFileVersionNumber );
		}
		else
		{
			EDGE_PRINTF( "File version number conflict\n\tSegs file has version number %d\n\tExpected version number %d\n",
				pSegsHeader->m_versionNumber,
				EdgeSegCompSampleFileHeader::kFileVersionNumber );
		}
		EDGE_ASSERT( false );
	}

	if ( EdgeSegCompSampleFileHeader::kDecompressionType_EdgeLzo1x != pSegsHeader->m_decompressionType )
	{
		if ( segFileName )
		{
			EDGE_PRINTF( "\"%s\" decompressionType is %d\n\tExpected %d (%s)\n",
				segFileName,
				pSegsHeader->m_decompressionType,
				EdgeSegCompSampleFileHeader::kDecompressionType_EdgeLzo1x,
				"kDecompressionType_EdgeLzo1x" );
		}
		else
		{
			EDGE_PRINTF( "segs file decompressionType is %d\n\tExpected %d (%s)\n",
				pSegsHeader->m_decompressionType,
				EdgeSegCompSampleFileHeader::kDecompressionType_EdgeLzo1x,
				"kDecompressionType_EdgeLzo1x" );
		}
		EDGE_ASSERT( false );
	}

	EDGE_ASSERT( pSegsHeader->m_segsFileSize == segsFileSize );
}

//////////////////////////////////////////////////////////////////////////

static void moveBuffer( uint32_t eaDest, uint32_t eaSource, uint32_t size, uint32_t dmaTagId )
{
	//////////////////////////////////////////////////////////////////////////
	//
	//	This function moves a buffer by DMAing it in from eaSource to a buffer
	//	in LS, then DMAing it back out to eaDest.
	//	The buffers can be overlapping if necessary.  Also, there are no
	//	alignment restrictions on the buffers.  The size however must be not
	//	more than kEdgeLzo1xInflateInputBufferMaxSize.
	//
	//////////////////////////////////////////////////////////////////////////

	EDGE_ASSERT( size <= kEdgeLzo1xInflateInputBufferMaxSize );

	//Fetch the data for this segment into LS.
	//If the data is not-qword aligned in main memory, then fetch it into LS at the same mis-alignment
	//Round up the DMA size to fetch slightly more than we need.
	uint32_t eaSourceRoundDown				= eaSource & ~0x7F;
	uint32_t inputAlignOffset				= eaSource & 0x7F;
	uint32_t sizeRoundUp					= ((size + inputAlignOffset) + 0x7F) & ~0x7F;
	EDGE_ASSERT( inputAlignOffset <= kEdgeSegCompSampleOverAllocateBufferSize );
	EDGE_ASSERT( (eaSourceRoundDown + inputAlignOffset) == eaSource );
	EDGE_ASSERT( sizeRoundUp <= sizeof(s_segmentTransferBuffer) );
	bool interruptsEnabled = spu_readch(SPU_RdMachStat) & 1;
	spu_idisable();
	EDGE_DMA_LARGE_GET( s_segmentTransferBuffer, eaSourceRoundDown, sizeRoundUp, dmaTagId, 0, 0 );
	EDGE_DMA_WAIT_TAG_STATUS_ALL( 1 << dmaTagId );
	if ( interruptsEnabled )
		spu_ienable();

	//If the output buffer has a different offset within a qword than the input buffer
	//then we should move the data appropriately while in LS before doing the DMA out
	uint32_t outputAlignOffset				= eaDest & 0x7F;
	void* pLsBufferDest;
	if ( (eaSource & 0xF) != (eaDest & 0xF) )
	{
		const void* pLsBufferSource			= &s_segmentTransferBuffer[inputAlignOffset];
		pLsBufferDest						= &s_segmentTransferBuffer[outputAlignOffset];
		//memmove is safe for overlapping buffers
		memmove( pLsBufferDest, pLsBufferSource, size );
	}
	else
	{
		//If they're already both at the same alignment within a qword, then rather than
		//spending time on the memmove, we might as well just do the DMA
		pLsBufferDest						= &s_segmentTransferBuffer[inputAlignOffset];
	}

	//Send out the buffer to eaDest
	//Note that we do *not* round up the output buffer size, or round down the eaDest,
	//for fear of corrupting adjacent memory
	_edgeLzoUnalignedLargeDmaPut( pLsBufferDest, eaDest, size, dmaTagId );
	spu_idisable();
	EDGE_DMA_WAIT_TAG_STATUS_ALL( 1 << dmaTagId );
	if ( interruptsEnabled )
		spu_ienable();
}

//////////////////////////////////////////////////////////////////////////
