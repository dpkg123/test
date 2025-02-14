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
#include "edge/lzma/edgelzma_spu.h"
#include "../common/segcompfile.h"
#include "edge/lzma/edgelzma_inflate_queue_element.h"
#include "lzma_segs_spu.h"


/////////////////////////////////////////////////////////////////////////

enum
{
	kEdgeSegCompSampleOverAllocateBufferSize = 128,		//Need to reserve more than maximum buffer size due to mis-alignment of incoming data
};
static uint8_t s_segmentTransferBuffer[kEdgeLzmaInflateInputBufferMaxSize + kEdgeSegCompSampleOverAllocateBufferSize] __attribute__((aligned(128)));

/////////////////////////////////////////////////////////////////////////

static void moveBuffer( uint32_t eaDest, uint32_t eaSource, uint32_t size, uint32_t dmaTagId );

//////////////////////////////////////////////////////////////////////////

//Given the EA of some LZMA format data, this helper function:
//	- Fetches the first 5 bytes (which contains the "properties") and
//		copies the data into the desired location
//	- Fetches the next 8 bytes so that it can give the compressed
//		data size
//	- Indicates the EA and size of the raw compressed data
static void FetchPropertiesFetchUncompSizeFindCompressedDataInLzmaData(
				uint32_t eaLzmaData, uint32_t lzmaDataSize,
				unsigned char* pOutputProperties, uint32_t maxPropertiesSize, uint32_t* pReturnedPropertiesSize,
				uint64_t* pUncompSize,
				uint32_t* pEaCompressedStream, uint32_t* pCompressedStreamSize,
				uint32_t dmaTagId )
{
	const uint32_t propertiesSize = 5;
	EDGE_ASSERT( lzmaDataSize > (propertiesSize+8));

	//We want to fetch 5+8 bytes, but we don't know what alignment it might be on
	//So round down to start of this qword and fetch 2 qwords to guarantee receiving data we need
	static unsigned char s_tempDataBuffer[32] __attribute__((aligned(16)));
	uint32_t eaFetch = eaLzmaData & ~0xF;
	bool interruptsEnabled = spu_readch(SPU_RdMachStat) & 1;
	spu_idisable();
	EDGE_DMA_GET( s_tempDataBuffer, eaFetch, sizeof(s_tempDataBuffer), dmaTagId, 0, 0 );
	EDGE_DMA_WAIT_TAG_STATUS_ALL( 1 << dmaTagId );
	if ( interruptsEnabled )
		spu_ienable();

	const unsigned char* pData = &s_tempDataBuffer[eaLzmaData&0xF];

	//First 5 bytes give LZMA properties
	const unsigned char* pInputProperties = pData;

	EDGE_ASSERT( maxPropertiesSize >= propertiesSize );
	//Copy out first 5 bytes give LZMA properties
	pOutputProperties[0]				= pInputProperties[0];
	pOutputProperties[1]				= pInputProperties[1];
	pOutputProperties[2]				= pInputProperties[2];
	pOutputProperties[3]				= pInputProperties[3];
	pOutputProperties[4]				= pInputProperties[4];
	*pReturnedPropertiesSize			= propertiesSize;


	//Next 8 bytes give uncompressed size (unaligned, little endian)
	const unsigned char* pUncompSizeUnaligned = &pData[propertiesSize];

	uint64_t uncompressedSize64 = 0;
	for ( int sizeByte = 7 ; sizeByte >= 0 ; --sizeByte )
	{
		uncompressedSize64 = (uncompressedSize64 << 8) | pUncompSizeUnaligned[sizeByte];
	}

	*pUncompSize = uncompressedSize64;


	//Then it's the raw compressed data
	const uint32_t eaCompressedStream	= eaLzmaData + (propertiesSize+8);
	uint32_t compressedStreamSize		= lzmaDataSize - (propertiesSize+8);

	*pEaCompressedStream				= eaCompressedStream;
	*pCompressedStreamSize				= compressedStreamSize;
}

//////////////////////////////////////////////////////////////////////////

void MoveBuffersAndAddLzmaSegsFileItemsToInflateQueue(
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
	TestValidLzmaSegsFile( pSegsHeader, NULL, segsFileSize );	//Check this is a legitimate file header before we do anything else

	uint32_t numSegments					= pSegsHeader->m_numSegments;
	uint32_t totalUncompressedSize			= pSegsHeader->m_totalUncompressedSize;
	if ( totalUncompressedSize > outputBuffMaxSize )
	{
		EDGE_PRINTF( "EDGE LZMA ERROR: Segs file will decompress to bigger than output buffer size (%d > %d)\n", totalUncompressedSize, outputBuffMaxSize );
		EDGE_ASSERT( false );
	}

	// pointer to the segs file's table of contents in LS
	const EdgeSegCompSampleTocElement* pSegsToc	= (const EdgeSegCompSampleTocElement*) &pSegsHeader[1];

	//////////////////////////////////////////////////////////////////////////
	//
	//	If the input data buffer (which contains the segs file) and the
	//	output uncompressed buffer don't overlap, then call
	//	AddLzmaSegsFileItemsToInflateQueue instead.
	//
	//////////////////////////////////////////////////////////////////////////

	register uint32_t eaOutputUncompBuffEnd	= eaOutputUncompBuff + outputBuffMaxSize;
	register uint32_t eaSegsFileDataEnd		= eaSegsFileData + segsFileSize;
	if ( ( eaSegsFileDataEnd <= eaOutputUncompBuff ) || ( eaSegsFileData >= eaOutputUncompBuffEnd ) )
	{
		//If the input data and the output data do not overlap, then we can just schedule the work
		//straight away and there is no need to move the data buffers around.
		//So just call AddLzmaSegsFileItemsToInflateQueue to do this.
		AddLzmaSegsFileItemsToInflateQueue(	pSegsHeader,
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

	register const uint32_t kYieldFrequency = 32;	//call poll and yield after processing every 32 segments
	int yieldCount = 0;

	uint32_t numCompressedSegs				= 0;	//Count how many segments are queued up for decompression by the SPU
	uint32_t numNonCompressedSegs			= 0;	//Some segments might have been stored raw because compression actually made them larger

	//Work backwards through the segments, spreading them out by moving them towards the end of the buffer.
	//These have to be done in order working backwards so that the copy of later segments is done before
	//moving the earlier segments overwrites that location.

	uint32_t outputOffset = totalUncompressedSize;

	for ( int segNo = numSegments-1; segNo >= 0; --segNo )
	{
		register const EdgeSegCompSampleTocElement* pSegElt	= &pSegsToc[segNo];    
		register uint32_t segUncompSize						= pSegElt->m_uncompressedSize;
		if ( segUncompSize == 0 )
			segUncompSize += 64*1024;
		register uint32_t segCompSize						= pSegElt->m_compressedSize;
		if ( segCompSize == 0 )
			segCompSize += 64*1024;
		register uint32_t segOffset							= pSegElt->m_fileOffset & ~1;
		register uint32_t isStoredCompressed				= pSegElt->m_fileOffset & 1;

		EDGE_ASSERT( outputOffset >= segUncompSize );
		outputOffset = outputOffset - segUncompSize;

		register uint32_t eaInputSegBuff					= eaSegsFileData + segOffset;
		EDGE_ASSERT( (eaInputSegBuff & 0xF) == 0 );
		register uint32_t eaOutputUncompSeg					= eaOutputUncompBuff + outputOffset;

		if ( eaOutputUncompSeg <= eaInputSegBuff )
		{
			//Don't do the copy if it's going to be moving backwards because we could potentially
			//be overwriting valid data that is still to be moved.
			//This copy will be handled in the second loop.
			break;
		}

		//Move the buffer from its current location in main memory to its new location
		moveBuffer( eaOutputUncompSeg, eaInputSegBuff, segCompSize, dmaTagId );

		if ( isStoredCompressed )
		{
			//	This segments needs decompressing, so add an entry to the Inflate Queue for it.

			//Parse the data so we know what to add to the Inflate Queue
			uint8_t properties[5];
			uint32_t propertiesSize;
			uint64_t uncompressedSize64;
			uint32_t eaCompressedBuff;
			uint32_t compressedBuffSize;
			FetchPropertiesFetchUncompSizeFindCompressedDataInLzmaData(
											eaOutputUncompSeg, segCompSize,
											properties, sizeof(properties), &propertiesSize,
											&uncompressedSize64,
											&eaCompressedBuff, &compressedBuffSize,
											dmaTagId );
			EDGE_ASSERT( uncompressedSize64 == segUncompSize );

			// If there is not enough room in the queue, this will yield
			edgeLzmaAddInflateQueueElement(	eaInflateQueue,
											properties, propertiesSize,
											eaCompressedBuff, compressedBuffSize,
											eaOutputUncompSeg, segUncompSize,
											eaNumElementsToDecompress,	//This will be decremented by 1 when this element is decompressed
											eaEventFlag,				//When *eaNumElementsToDecompress decrements to zero, this event will be triggered
											eventFlagBits,
											kEdgeLzmaInflateTask_Inflate,
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
				int ret = cellSpursYield();
				EDGE_ASSERT( CELL_OK == ret );
				(void) ret;
			}
		}
	}

	//Some segments at the start may need copying forwards, so now work forwards through them,
	//spreading them out by moving them towards the front of the buffer.
	outputOffset = 0;

	for ( uint32_t segNo = 0; segNo < numSegments; ++segNo )
	{
		register const EdgeSegCompSampleTocElement* pSegElt	= &pSegsToc[segNo];
		register uint32_t segUncompSize						= pSegElt->m_uncompressedSize;
		if ( segUncompSize == 0 )
			segUncompSize += 64*1024;
		register uint32_t segCompSize						= pSegElt->m_compressedSize;
		if ( segCompSize == 0 )
			segCompSize += 64*1024;
		register uint32_t segOffset							= pSegElt->m_fileOffset & ~1;
		register uint32_t isStoredCompressed				= pSegElt->m_fileOffset & 1;

		register uint32_t eaInputSegBuff					= eaSegsFileData + segOffset;
		EDGE_ASSERT( (eaInputSegBuff & 0xF) == 0 );
		register uint32_t eaOutputUncompSeg					= eaOutputUncompBuff + outputOffset;

		outputOffset += segUncompSize;
		EDGE_ASSERT( outputOffset <= totalUncompressedSize );

		if ( eaInputSegBuff <= eaOutputUncompSeg)
		{
			//Once we reach the point of copying forwards we can stop because these
			//will already have been handled by the first loop.
			break;
		}

		//Move the buffer from its current location in main memory to its new location
		moveBuffer( eaOutputUncompSeg, eaInputSegBuff, segCompSize, dmaTagId );

		if ( isStoredCompressed )
		{
			//	This segments needs decompressing, so add an entry to the Inflate Queue for it.

			//Parse the data so we know what to add to the Inflate Queue
			uint8_t properties[5];
			uint32_t propertiesSize;
			uint64_t uncompressedSize64;
			uint32_t eaCompressedBuff;
			uint32_t compressedBuffSize;
			FetchPropertiesFetchUncompSizeFindCompressedDataInLzmaData(
											eaOutputUncompSeg, segCompSize,
											properties, sizeof(properties), &propertiesSize,
											&uncompressedSize64,
											&eaCompressedBuff, &compressedBuffSize,
											dmaTagId );
			EDGE_ASSERT( uncompressedSize64 == segUncompSize );

			// If there is not enough room in the queue, this will yield
			edgeLzmaAddInflateQueueElement(	eaInflateQueue,
											properties, propertiesSize,
											eaCompressedBuff, compressedBuffSize,
											eaOutputUncompSeg, segUncompSize,
											eaNumElementsToDecompress,	//This will be decremented by 1 when this element is decompressed
											eaEventFlag,				//When *eaNumElementsToDecompress decrements to zero, this event will be triggered
											eventFlagBits,
											kEdgeLzmaInflateTask_Inflate,
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
				int ret = cellSpursYield();
				EDGE_ASSERT( CELL_OK == ret );
				(void) ret;
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
				EDGE_PRINTF( "EDGE LZMA ERROR: Attempted to decrement a work-to-do counter by more than counters current value (%d > %d)\n", numNonCompressedSegs, old );
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
				EDGE_ASSERT( CELL_OK == ret );
				(void) ret;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////

void AddLzmaSegsFileItemsToInflateQueue(
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
	TestValidLzmaSegsFile( pSegsHeader, NULL, segsFileSize );	//Check this is a legitimate file header before we do anything else

	uint32_t numSegments					= pSegsHeader->m_numSegments;
	uint32_t totalUncompressedSize			= pSegsHeader->m_totalUncompressedSize;
	if ( totalUncompressedSize > outputBuffMaxSize )
	{
		EDGE_PRINTF( "EDGE LZMA ERROR: Segs file will decompress to bigger than output buffer size (%d > %d)\n", totalUncompressedSize, outputBuffMaxSize );
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

	register uint32_t eaOutputUncompBuffEnd	= eaOutputUncompBuff + outputBuffMaxSize;
	register uint32_t eaSegsFileDataEnd		= eaSegsFileData + segsFileSize;
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
		register const EdgeSegCompSampleTocElement* pSegElt	= &pSegsToc[segNo];
		register uint32_t segUncompSize						= pSegElt->m_uncompressedSize;
		if ( segUncompSize == 0 )
			segUncompSize += 64*1024;
		register uint32_t segCompSize						= pSegElt->m_compressedSize;
		if ( segCompSize == 0 )
			segCompSize += 64*1024;
		register uint32_t segOffset							= pSegElt->m_fileOffset & ~1;
		register uint32_t isStoredCompressed				= pSegElt->m_fileOffset & 1;

		register uint32_t eaInputSegBuff					= eaSegsFileData + segOffset;
		EDGE_ASSERT( (eaInputSegBuff & 0xF) == 0 );
		register uint32_t eaOutputUncompSeg					= eaOutputUncompBuff + outputOffset;

		outputOffset += segUncompSize;
		EDGE_ASSERT( outputOffset <= totalUncompressedSize );

		if ( isStoredCompressed )
		{
			//	This segments needs decompressing, so add an entry to the Inflate Queue for it.

			//Parse the data so we know what to add to the Inflate Queue
			uint8_t properties[5];
			uint32_t propertiesSize;
			uint64_t uncompressedSize64;
			uint32_t eaCompressedBuff;
			uint32_t compressedBuffSize;
			FetchPropertiesFetchUncompSizeFindCompressedDataInLzmaData(
											eaInputSegBuff, segCompSize,
											properties, sizeof(properties), &propertiesSize,
											&uncompressedSize64,
											&eaCompressedBuff, &compressedBuffSize,
											dmaTagId );
			EDGE_ASSERT( uncompressedSize64 == segUncompSize );

			// If there is not enough room in the queue, this will yield
			edgeLzmaAddInflateQueueElement(	eaInflateQueue,
											properties, propertiesSize,
											eaCompressedBuff, compressedBuffSize,
											eaOutputUncompSeg, segUncompSize,
											eaNumElementsToDecompress,	//This will be decremented by 1 when this element is decompressed
											eaEventFlag,				//When *eaNumElementsToDecompress decrements to zero, this event will be triggered
											eventFlagBits,
											kEdgeLzmaInflateTask_Inflate,
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
			moveBuffer( eaOutputUncompSeg, eaInputSegBuff, segCompSize, dmaTagId );
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
			register uint32_t old = EDGE_ATOMIC_SUB_32( (uint32_t*)&s_atomicBuffer[0], eaNumElementsToDecompress, numNonCompressedSegs );
			if ( numNonCompressedSegs > old )
			{
				EDGE_PRINTF( "EDGE LZMA ERROR: Attempted to decrement a work-to-do counter by more than counters current value (%d > %d)\n", numNonCompressedSegs, old );
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
				EDGE_ASSERT( CELL_OK == ret );
				(void) ret;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////

void TestValidLzmaSegsFile( const EdgeSegCompSampleFileHeader* pSegsHeader, const char* segFileName, uint32_t segsFileSize )
{
	EDGE_ASSERT( (((uint32_t)pSegsHeader) & 0xF) == 0 );

	if ( EdgeSegCompSampleFileHeader::kMagic != pSegsHeader->m_magic )
	{
		EDGE_PRINTF( "EDGE LZMA ERROR: Invalid segs file\n" );
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

	if ( EdgeSegCompSampleFileHeader::kDecompressionType_EdgeLzma != pSegsHeader->m_decompressionType )
	{
		if ( segFileName )
		{
			EDGE_PRINTF( "\"%s\" decompressionType is %d\n\tExpected %d (%s)\n",
				segFileName,
				pSegsHeader->m_decompressionType,
				EdgeSegCompSampleFileHeader::kDecompressionType_EdgeLzma,
				"kDecompressionType_EdgeLzma" );
		}
		else
		{
			EDGE_PRINTF( "segs file decompressionType is %d\n\tExpected %d (%s)\n",
				pSegsHeader->m_decompressionType,
				EdgeSegCompSampleFileHeader::kDecompressionType_EdgeLzma,
				"kDecompressionType_EdgeLzma" );
		}
		EDGE_ASSERT( false );
	}

	EDGE_ASSERT( pSegsHeader->m_segsFileSize == segsFileSize );
}

//////////////////////////////////////////////////////////////////////////

static void moveBuffer(	register uint32_t eaDest,
						register uint32_t eaSource,
						register uint32_t size,
						register uint32_t dmaTagId )
{
	//////////////////////////////////////////////////////////////////////////
	//
	//	This function moves a buffer by DMAing it in from eaSource to a buffer
	//	in LS, then DMAing it back out to eaDest.
	//	The buffers can be overlapping if necessary.  Also, there are no
	//	alignment restrictions on the buffers.  The size however must be not
	//	more than kEdgeLzmaInflateInputBufferMaxSize.
	//
	//////////////////////////////////////////////////////////////////////////

	EDGE_ASSERT( size <= kEdgeLzmaInflateInputBufferMaxSize );

	//Fetch the data for this segment into LS.
	//If the data is not-qword aligned in main memory, then fetch it into LS at the same mis-alignment
	//Round up the DMA size to fetch slightly more than we need.
	register uint32_t eaSourceRoundDown				= eaSource & ~0x7F;
	register uint32_t inputAlignOffset				= eaSource & 0x7F;
	register uint32_t sizeRoundUp					= ((size + inputAlignOffset) + 0x7F) & ~0x7F;
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
	register uint32_t outputAlignOffset				= eaDest & 0x7F;
	register void* pLsBufferDest;
	if ( (eaSource & 0xF) != (eaDest & 0xF) )
	{
		register const void* pLsBufferSource		= &s_segmentTransferBuffer[inputAlignOffset];
		pLsBufferDest								= &s_segmentTransferBuffer[outputAlignOffset];
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
	_edgeLzmaUnalignedLargeDmaPut( pLsBufferDest, eaDest, size, dmaTagId );
	spu_idisable();
	EDGE_DMA_WAIT_TAG_STATUS_ALL( 1 << dmaTagId );
	if ( interruptsEnabled )
		spu_ienable();
}

//////////////////////////////////////////////////////////////////////////
