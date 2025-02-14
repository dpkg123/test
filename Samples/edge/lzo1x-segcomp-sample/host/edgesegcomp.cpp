/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "edge/edge_stdint.h"
#include "../common/segcompfile.h"
#include "file.h"
#include "edgecompress.h"

//////////////////////////////////////////////////////////////////////////

//#define SEGCOMP_DEBUG_PRINTF( str )	do { printf str ; } while ( 0 )
#define SEGCOMP_DEBUG_PRINTF( str )	do { } while ( 0 )

//////////////////////////////////////////////////////////////////////////

int ConvertFile( const char* outputFileName, const char* masterFilename, uint32_t level, int maxSegmentSize );

//////////////////////////////////////////////////////////////////////////

int main( int argc, char* argv[] )
{
	char generatedOutputFileName[256];
	const char* pMasterFilename			= argv[1];
	const char* pOutputFileName			= NULL;
	uint32_t level						= 9;
	int maxSegmentSize					= 64*1024;
	int unknownOptions					= 0;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	Parse the command line
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//Print the help message if a filename hasn't been passed, or if "--help" has been passed
	if ( ( argc < 2 ) || ( strcmp(pMasterFilename, "--help" ) == 0 ) )
	{
		printf( "Usage: lzo1x-segcomp-sample <input filename> [-o <output filename>] [-<level>] [-seg-size=<size>]\n" );
		printf( "\t'level' can be 0(none) through 9 (default=9)\n" );
		printf( "\t'size' is in bytes for each segment (default=65536)\n" );
		return 1;
	}

	//Print version information
	if ( strcmp(pMasterFilename, "--version" ) == 0 )
	{
		printf( "edgesegcomp built: %s, %s\n", __TIME__, __DATE__ );
		printf( "(EdgeSegCompSampleFileHeader::kFileVersionNumber = %d)\n", (int)EdgeSegCompSampleFileHeader::kFileVersionNumber );
		return 1;
	}

	//Iterate over the command line
	int argNo = 2;
	while ( argNo < argc )
	{
		const char* thisArg = argv[argNo];
		if ( strncmp( thisArg, "-o", 2 ) == 0 )
		{
			if ( strcmp( thisArg, "-o" ) == 0 )
			{
				//of the form "-o foo.bin"
				++argNo;	//This is the output filename, so take the next arg
				if ( argNo == argc )
				{
					printf( "ERROR: output filename not specified\n" );
					return 1;
				}
				thisArg = argv[argNo];
				if ( pOutputFileName )
				{
					printf( "ERROR: output filename specified more than once\n" );
					return 1;
				}
				pOutputFileName = thisArg;
			}
			else
			{
				//of the form "-ofoo.bin"
				if ( pOutputFileName )
				{
					printf( "ERROR: output filename specified more than once\n" );
					return 1;
				}
				pOutputFileName = &thisArg[2];
			}
		}
		else if ( strcmp( thisArg, "-0" ) == 0 )
		{
			level = 0;
		}
		else if ( strcmp( thisArg, "-1" ) == 0 )
		{
			level = 1;
		}
		else if ( strcmp( thisArg, "-2" ) == 0 )
		{
			level = 2;
		}
		else if ( strcmp( thisArg, "-3" ) == 0 )
		{
			level = 3;
		}
		else if ( strcmp( thisArg, "-4" ) == 0 )
		{
			level = 4;
		}
		else if ( strcmp( thisArg, "-5" ) == 0 )
		{
			level = 5;
		}
		else if ( strcmp( thisArg, "-6" ) == 0 )
		{
			level = 6;
		}
		else if ( strcmp( thisArg, "-7" ) == 0 )
		{
			level = 7;
		}
		else if ( strcmp( thisArg, "-8" ) == 0 )
		{
			level = 8;
		}
		else if ( strcmp( thisArg, "-9" ) == 0 )
		{
			level = 9;
		}
		else if ( strncmp( thisArg, "-seg-size", 9 ) == 0 )
		{
			maxSegmentSize = atoi(&thisArg[10]);
			if ( maxSegmentSize > 64*1024 )
			{
				printf( "ERROR: segment size is greater than 64K\n" );
				return 1;
			}
		}
		else
		{
			printf( "ERROR: Unknown option: \"%s\"\n", thisArg );
			++unknownOptions;
		}
		++argNo;
	}

	//If we had any unknown options, then abort
	if ( unknownOptions )
	{
		return 1;
	}

	//If an output filename wasn't chosen, we can generate a default
	if ( pOutputFileName == NULL)
	{
		sprintf( generatedOutputFileName, /*sizeof(generatedOutputFileName),*/ "%s.segs", pMasterFilename );
		pOutputFileName = generatedOutputFileName;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	//	Convert the file that we're interested in
	//
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	uint32_t convertRet = ConvertFile( pOutputFileName, pMasterFilename, level, maxSegmentSize );

	return convertRet;
}

//////////////////////////////////////////////////////////////////////////

inline uint32_t RoundUp( uint32_t value, uint32_t alignment )
{
	assert( ((alignment & (alignment - 1)) == 0) && alignment );

	return (value + (alignment - 1)) & ~(alignment - 1);
}

//////////////////////////////////////////////////////////////////////////

//return 0 on success
int ConvertFile( const char* outputFileName, const char* masterFilename, uint32_t level, int maxSegmentSize )
{
	printf( "Converting \"%s\" to \"%s\" with level %d\n", masterFilename, outputFileName, (unsigned int)level );

	//////////////////////////////////////////////////////////////////////////
	//
	//	Load the master file.
	//
	//////////////////////////////////////////////////////////////////////////

	uint32_t masterFileSize;
	void* pMasterData = LoadFile( masterFilename, &masterFileSize );
	SEGCOMP_DEBUG_PRINTF( ("Loaded %s\n", masterFilename) );

	if ( NULL == pMasterData )
		return 1;

	assert( maxSegmentSize <= 64*1024 );
	uint32_t numSegments					= (masterFileSize + (maxSegmentSize - 1)) / maxSegmentSize;
	uint32_t headerAndTocSize				= sizeof(EdgeSegCompSampleFileHeader) + numSegments*sizeof(EdgeSegCompSampleTocElement);
	uint32_t headerMallocSize				= (headerAndTocSize + 0xF) & ~0xF;	//Round up the malloc size to a qword multiple
	EdgeSegCompSampleFileHeader* pHeader	= (EdgeSegCompSampleFileHeader*)malloc( headerMallocSize );
	if ( NULL == pHeader )
	{
		FreeFile( pMasterData );
		return 1;
	}
	memset( pHeader, 0, headerMallocSize );

	//Fill in the first part of the header
	pHeader->m_magic						= EdgeSegCompSampleFileHeader::kMagic;
	pHeader->m_versionNumber				= EdgeSegCompSampleFileHeader::kFileVersionNumber;
	pHeader->m_decompressionType			= EdgeSegCompSampleFileHeader::kDecompressionType_EdgeLzo1x;
	pHeader->m_numSegments					= numSegments;
	pHeader->m_totalUncompressedSize		= masterFileSize;
	pHeader->m_segsFileSize					= 0;	//Fill this in later
	
	EdgeSegCompSampleTocElement* pSegmentToc	= (EdgeSegCompSampleTocElement*) (&pHeader[1]);

	uint32_t fileOffset						= RoundUp( headerAndTocSize, 16 );
	uint32_t headerAndTocSizeAlign16		= fileOffset;


	//////////////////////////////////////////////////////////////////////////
	//
	// Allocate buffer to hold compressed data.
	//
	//////////////////////////////////////////////////////////////////////////

	//In case compression makes the file larger, we want to allocate more
	//space than we'll need.
	uint32_t tempCompBufSize = (uint32_t)(masterFileSize + masterFileSize / 16 + 64 + 3   + 6*numSegments);
    tempCompBufSize = RoundUp( tempCompBufSize, 16 );
    uint8_t* pTempCompBuf = (uint8_t*)malloc( tempCompBufSize );
    if( pTempCompBuf == NULL )
    {
		printf( "ERROR: Could not allocate compressed buffer of size %d\n", (unsigned int)tempCompBufSize );

    	FreeFile( pMasterData );
		free( pHeader );
		return 1;
    }
	memset( pTempCompBuf, 0, tempCompBufSize );


	//////////////////////////////////////////////////////////////////////////
	//
	// For each segment, run the compressor on it, then choose whichever is 
	// the smaller of the compressed or the uncompressed version for use in
	// our final file.
	//
	//////////////////////////////////////////////////////////////////////////

    const char* pSegmentData = (const char*)pMasterData;
    int remainingSize = masterFileSize;

    // scan the segments
	for ( uint32_t segNo = 0; segNo < numSegments; ++segNo )
	{
        uint32_t uncompSegSize;
        uint32_t compSegRawSize;

   		assert( (fileOffset & 0xF) == 0 );

		uncompSegSize = (maxSegmentSize < remainingSize) ? maxSegmentSize : remainingSize;

        // initialize compressed size with size of allocated buffer
        uint32_t tempCompBufOffset = fileOffset - headerAndTocSizeAlign16;
        compSegRawSize = tempCompBufSize - tempCompBufOffset;

		// execute lzopro decompression
		uint32_t ret = segcompCompress(	&pTempCompBuf[tempCompBufOffset],
										&compSegRawSize, // returns compressed size here
										(const uint8_t*)pSegmentData,
										uncompSegSize,
										level );
		if( ret != 0 )
		{
			printf( "ERROR: Failed lzopro compression, segment=%d, ret=%d\n", (unsigned int)segNo, (unsigned int)ret );
			free( pHeader );
			free( pTempCompBuf );
			FreeFile( pMasterData );
			return 1;
		}

		if ( compSegRawSize >= uncompSegSize )
		{
			//Compressed is *bigger* than uncompressed data
			//(If there's not much in it, then it may be worth just sticking with the uncompressed version, rather than wasting time decompressing)

            // Overwrite compressed data with original uncompressed data
            memcpy( &pTempCompBuf[tempCompBufOffset], pSegmentData, uncompSegSize );

			pSegmentToc[segNo].m_fileOffset			= fileOffset | 0 ;		//store uncompressed

			fileOffset += uncompSegSize;

			pSegmentToc[segNo].m_compressedSize		= uncompSegSize;		//We're storing the uncompressed version, so set compSz = uncompSz
			pSegmentToc[segNo].m_uncompressedSize	= uncompSegSize;
		}
		else
		{
			pSegmentToc[segNo].m_fileOffset			= fileOffset | 1 ;		//store compressed

			fileOffset += compSegRawSize;

			pSegmentToc[segNo].m_compressedSize		= compSegRawSize;
			pSegmentToc[segNo].m_uncompressedSize	= uncompSegSize;
		}
		fileOffset = RoundUp( fileOffset, 16 );

		remainingSize -= uncompSegSize;
		pSegmentData  += uncompSegSize;
	}
	SEGCOMP_DEBUG_PRINTF( ("Converted all segments\n") );

   	FreeFile( pMasterData );


	//////////////////////////////////////////////////////////////////////////
	//
	//	Finally, write out the segmented file.
	//
	//////////////////////////////////////////////////////////////////////////

	pHeader->m_segsFileSize		= fileOffset;	//Fill in the segsFileSize now that we know it.

	FILE* fp = fopen( outputFileName, "wb" );
	if ( NULL == fp )
	{
		printf( "ERROR: Failed to open \"%s\" for writing\n", outputFileName );
        free( pTempCompBuf );
		free( pHeader );
		return 1;
	}
	size_t numWritten = fwrite( pHeader, 1, headerMallocSize, fp );
	if ( headerMallocSize != numWritten )
	{
        free( pTempCompBuf );
		free( pHeader );
		fclose( fp );
		return 1;
	}

    // write out compressed (or uncompressed) data from pTempCompBuf
    numWritten = fwrite( pTempCompBuf, 1, fileOffset - headerAndTocSizeAlign16, fp );
    if( numWritten != fileOffset - headerAndTocSizeAlign16 )
    {
        free( pTempCompBuf );
		free( pHeader );
		fclose( fp );
		return 1;
    }

	uint32_t segsFileSize = ftell( fp );
	assert( segsFileSize == fileOffset );

    free( pTempCompBuf );

	int ret = fclose( fp );
	if( 0 != ret )
	{
		free( pHeader );
		return 1;
	}
	SEGCOMP_DEBUG_PRINTF( ("Finished writing file %s\n", outputFileName) );

	if ( 0 )	//Print header and TOC
	{
		printf( "magic = 0x%08X\n",					(unsigned int)pHeader->m_magic );
		printf( "versionNumber = %d\n",				(unsigned int)pHeader->m_versionNumber );
		printf( "totalUncompressedSize = 0x%08X\n",	(unsigned int)pHeader->m_totalUncompressedSize );
		printf( "numSegments = %d\n",				(unsigned int)pHeader->m_numSegments );

		for ( uint32_t segNo = 0; segNo < numSegments; ++segNo )
		{
			printf( "Seg[%04d]\n",					(unsigned int)segNo );
			printf( "\tcompressedSize = 0x%X\n",	(unsigned int)pSegmentToc[segNo].m_compressedSize );
			printf( "\tuncompressedSize = 0x%X\n",	(unsigned int)pSegmentToc[segNo].m_uncompressedSize );
			printf( "\tfileOffset = 0x%X | %d\n",	(unsigned int)pSegmentToc[segNo].m_fileOffset & ~1, (unsigned int)pSegmentToc[segNo].m_fileOffset & 1 );
		}
	}

	free( pHeader );

	float saving = 100.0f - 100.0f * ((float) fileOffset) / ((float) masterFileSize);
	printf( "\torig size = %12d\n\tnew size  = %12d\n\tnumSegs   = %12d\n\tsaving    = %12.2f%%\n", (int)masterFileSize, (int)fileOffset, (int)numSegments, saving );

	return 0;
}

//////////////////////////////////////////////////////////////////////////
