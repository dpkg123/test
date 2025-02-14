/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <string.h>

#include <cell/spurs.h>

#include "../common/segcompfile.h"
#include "lzma_segs_ppu.h"

#include "edge/edge_printf.h"
#include "edge/edge_assert.h"

//////////////////////////////////////////////////////////////////////////

void SplitLzmaDataIntoComponents
(	const void*				pLzmaData,
	uint32_t				lzmaDataSize,
	const unsigned char**	ppProperties,
	uint32_t*				pPropertiesSize,
	uint64_t*				pUncompSize,
	const unsigned char**	ppCompressedStream,
	uint32_t*				pCompressedStreamSize
)
{
	const unsigned char* pProperties = (const unsigned char*) pLzmaData;
	uint32_t propertiesSize = 5;
	EDGE_ASSERT( lzmaDataSize > (propertiesSize+8));


	//First 5 bytes give LZMA properties
	*ppProperties		= pProperties;
	*pPropertiesSize	= propertiesSize;


	//Next 8 bytes give uncompressed size (unaligned, little endian)
	const unsigned char* pUncompSizeUnaligned = &pProperties[propertiesSize];

	uint64_t uncompressedSize64 = 0;
	for ( int sizeByte = 7 ; sizeByte >= 0 ; --sizeByte )
	{
		uncompressedSize64 = (uncompressedSize64 << 8) | pUncompSizeUnaligned[sizeByte];
	}

	*pUncompSize = uncompressedSize64;


	//Then it's the raw compressed data
	const unsigned char* pCompressedStream = &pUncompSizeUnaligned[8];
	uint32_t compressedStreamSize = lzmaDataSize - (propertiesSize+8);

	*ppCompressedStream = pCompressedStream;
	*pCompressedStreamSize = compressedStreamSize;
}

//////////////////////////////////////////////////////////////////////////

uint32_t AddLzmaSegsFileItemsToInflateQueue(
			EdgeSegCompSampleFileDataPtr	pSegsFileData,				// Effective Address of segs file data, 16 aligned
			uint32_t						segsFileSize,				// size of the input segs file
			EdgeLzmaInflateQHandle			inflateQueue,				// Effective Address of the Inflate Queue
			unsigned char*					pOutputUncompBuff,			// Effective Address of output buffer,
			uint32_t						outputBuffMaxSize,			// maximum size for the data sent to the output buffer
			uint32_t*						pNumElementsToDecompress,	// Effective Address of counter of #elements to decompress
			CellSpursEventFlag*				pEventFlag,					// Effective Address of eventFlag to trigger when all uncompressed data is stored
			uint16_t						eventFlagBits )
{
	const EdgeSegCompSampleFileHeader* pSegsHeader	= (const EdgeSegCompSampleFileHeader*) pSegsFileData;
	const EdgeSegCompSampleTocElement* pSegsToc		= (const EdgeSegCompSampleTocElement*) &pSegsHeader[1];

	TestValidLzmaSegsFile( pSegsHeader, NULL, segsFileSize );


	//////////////////////////////////////////////////////////////////////////
	//
	//	This function only handles the cases where the input data buffer
	//	(which contains the segs file) and the output uncompressed buffer
	//	don't overlap.
	//
	//////////////////////////////////////////////////////////////////////////

	unsigned char* pOutputUncompBuffEnd	= pOutputUncompBuff + outputBuffMaxSize;
	unsigned char* pSegsFileDataEnd		= (unsigned char*)pSegsFileData + segsFileSize;
	(void) pOutputUncompBuffEnd;
	(void) pSegsFileDataEnd;
	EDGE_ASSERT( ( pSegsFileDataEnd <= pOutputUncompBuffEnd ) || ( pSegsFileDataEnd >= pOutputUncompBuffEnd ) );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Iterate over the TOC and add to the Inflate Queue for decompressing
	//	the compressed segments
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t numSegments					= pSegsHeader->m_numSegments;
	uint32_t totalUncompressedSize			= pSegsHeader->m_totalUncompressedSize;
	uint32_t numCompressedSegs				= 0;	//Count how many segments are queued up for decompression by the SPU
	uint32_t numNonCompressedSegs			= 0;	//Some segments might have been stored raw because compression actually made them larger
	uint32_t nonCompressedSegmentsSize		= 0;	//This is the total size of all the non-compressed segments
	uint32_t accumUncompSize				= 0;	//This is the total size after decompression of all the compressed segments
	uint32_t rawDataSize					= 0;	//This is the total size of all the raw data (compressed or not) in the file

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

		const unsigned char* pInputSegBuff			= ((const unsigned char*)pSegsFileData) + segOffset;
		EDGE_ASSERT( (((uint32_t)pInputSegBuff) & 0xF) == 0 );
		unsigned char* pOutputUncompSeg				= &pOutputUncompBuff[outputOffset];

		outputOffset += segUncompSize;
		EDGE_ASSERT( outputOffset <= totalUncompressedSize );
		(void) totalUncompressedSize;

		const unsigned char* pProperties;
		uint32_t propertiesSize;
		const unsigned char* pCompressedBuff;
		uint32_t compressedBuffSize;

		if ( isStoredCompressed )
		{
			//Data was stored compressed
			//Parse the data and split it into its components for us to pass into Edge Lzma

			uint64_t uncompressedSize64;
			SplitLzmaDataIntoComponents( pInputSegBuff, segCompSize,
										&pProperties, &propertiesSize,
										&uncompressedSize64,
										&pCompressedBuff, &compressedBuffSize );

			if ( uncompressedSize64 != segUncompSize )
			{
				EDGE_PRINTF(	"**************************************************************************************************************\n"
								"*edgeLzmaInflateRawData error: uncompressedSize in file header (0x%08x_%08x) not same as user indicated (0x%x)\n"
								"**************************************************************************************************************\n",
								(uint32_t)(uncompressedSize64 >> 32), (uint32_t)(uncompressedSize64 & 0xFFFFFFFF), segUncompSize );
				EDGE_ASSERT( false );
			}

			++numCompressedSegs;
			accumUncompSize				+= segUncompSize;
		}
		else
		{
			//If the data was stored uncompressed, then rather than calling decompression on it,
			//all we have to do is move the data to its final location.
			//Memcpy via LS rather than using the PPU to do this.
			EDGE_ASSERT( segCompSize == segUncompSize );

			pProperties					= NULL;
			propertiesSize				= 0;

			pCompressedBuff				= pInputSegBuff;
			compressedBuffSize			= segCompSize;

			++numNonCompressedSegs;
			nonCompressedSegmentsSize	+= segUncompSize;
		}

		rawDataSize						+= segCompSize;

		edgeLzmaAddInflateQueueElement(	inflateQueue,
										pProperties, propertiesSize,
										pCompressedBuff, compressedBuffSize,
										pOutputUncompSeg, segUncompSize,
										pNumElementsToDecompress,	//This will be decremented by 1 when this element is decompressed
										pEventFlag,					//When *pNumElementsToDecompress decrements to zero, this event will be triggered
										eventFlagBits,
										(EdgeLzmaInflateTaskProcessing)isStoredCompressed
									);
	}
	EDGE_ASSERT( numCompressedSegs <= numSegments );
	EDGE_ASSERT( (numNonCompressedSegs + numCompressedSegs) == numSegments );
	EDGE_ASSERT( (accumUncompSize + nonCompressedSegmentsSize) == totalUncompressedSize );

	return numNonCompressedSegs;
}

//////////////////////////////////////////////////////////////////////////

void TestValidLzmaSegsFile( EdgeSegCompSampleFileDataPtr pSegsFileData, const char* segFileName, uint32_t segsFileSize )
{
	EDGE_ASSERT( (((uint32_t)pSegsFileData) & 0xF) == 0 );

	const EdgeSegCompSampleFileHeader* pSegsHeader	= (const EdgeSegCompSampleFileHeader*) pSegsFileData;

	EDGE_ASSERT( EdgeSegCompSampleFileHeader::kMagic == pSegsHeader->m_magic );

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

void PrintLzmaSegsHeaderAndToc( EdgeSegCompSampleFileDataPtr pSegsFileData, const char* segFileName, uint32_t segsFileSize )
{
	const EdgeSegCompSampleFileHeader* pSegsHeader	= (const EdgeSegCompSampleFileHeader*) pSegsFileData;
	const EdgeSegCompSampleTocElement* pSegsToc		= (const EdgeSegCompSampleTocElement*) &pSegsHeader[1];

	uint32_t totalUncompressedSize					= pSegsHeader->m_totalUncompressedSize;
	uint32_t numSegments							= pSegsHeader->m_numSegments;

	EDGE_PRINTF( "%s\n", segFileName );
	EDGE_PRINTF( "magic               = 0x%08X\n", pSegsHeader->m_magic );
	EDGE_PRINTF( "decompressionType   = %d\n", pSegsHeader->m_decompressionType );
	EDGE_PRINTF( "fileVersionNum      = %d\n", pSegsHeader->m_versionNumber );
	EDGE_PRINTF( "uncompressedSize    = %d\n", totalUncompressedSize );
	EDGE_PRINTF( "numSegs             = %d\n", numSegments );

	TestValidLzmaSegsFile( pSegsFileData, segFileName, segsFileSize );

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

		EDGE_PRINTF( "segment[%d]\n", segNo );
		EDGE_PRINTF( "\tuncompressedSize   = %d\n", segUncompSize );
		EDGE_PRINTF( "\tcompressedSize     = %d\n", segCompSize );
		EDGE_PRINTF( "\tfileOffset         = %d\n", segOffset );
		EDGE_PRINTF( "\tisStoredCompressed = %d\n", isStoredCompressed );
	}
}

//////////////////////////////////////////////////////////////////////////

uint16_t GetLzmaSegsFileNumSegments( EdgeSegCompSampleFileDataPtr pSegsFileData )
{
	const EdgeSegCompSampleFileHeader* pSegsHeader	= (const EdgeSegCompSampleFileHeader*) pSegsFileData;

	return pSegsHeader->m_numSegments;
}

//////////////////////////////////////////////////////////////////////////

uint32_t GetLzmaSegsFileTotalUncompressedSize( EdgeSegCompSampleFileDataPtr pSegsFileData )
{
	const EdgeSegCompSampleFileHeader* pSegsHeader	= (const EdgeSegCompSampleFileHeader*) pSegsFileData;

	return pSegsHeader->m_totalUncompressedSize;
}

//////////////////////////////////////////////////////////////////////////

uint32_t GetLzmaSegsFileHeaderAndTocSize( EdgeSegCompSampleFileDataPtr pSegsFileData )
{
	uint32_t numTocEntries			= GetLzmaSegsFileNumSegments( pSegsFileData );
	uint32_t segsHeaderAndTocSize	= sizeof(EdgeSegCompSampleFileHeader) + (numTocEntries * sizeof(EdgeSegCompSampleTocElement));
	return segsHeaderAndTocSize;
}

//////////////////////////////////////////////////////////////////////////
