/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "gz.h"

//////////////////////////////////////////////////////////////////////////

#define MAKE_U32( a, b, c, d )		(((a) << 24) | ((b) << 16) | ((c) << 8) | ((d) << 0) )
#define MAKE_U16( a, b )			(((a) << 8) | ((b) << 0) )

//////////////////////////////////////////////////////////////////////////

const void* GzHeaderFindRawData( const void* pData )
{
	const unsigned char* pBytes = (const unsigned char*) pData;

	uint32_t magic				= MAKE_U16( pBytes[1], pBytes[0] );
	uint8_t compMethod			= pBytes[2];
	uint8_t flags				= pBytes[3];
	//uint32_t modTime			= MAKE_U32( pBytes[7], pBytes[6], pBytes[5], pBytes[4] );
	//uint8_t extraFlags			= pBytes[8];
	//uint8_t osType				= pBytes[9];

	const unsigned char* pEndHeader	= &pBytes[10];	//Increase this pointer past the optional fields as well

	if ( 0x8B1F != magic )
	{
		printf( "ERROR: Incorrect gz header signature (0x%04X)\n", magic );
		return NULL;
	}

	if ( 8 != compMethod )
	{
		printf( "ERROR: only \"deflate\" compressed data is supported\n" );
		return NULL;
	}

	//printf( "flags = 0x%02X\n", flags );
	if ( flags & 1 )
	{
		//printf( "\tFile probably ascii text\n" );
	}

	bool partNumberPresent = false;
	if ( flags & 2 )
	{
		//printf( "\tcontinuation of multi-part gzip file, part number present\n" );
		partNumberPresent = true;
	}
	bool extraFieldPresent = false;
	if ( flags & 4 )
	{
		//printf( "\textra field present\n" );
		extraFieldPresent = true;
	}
	bool origFileNamePresent = false;
	if ( flags & 8 )
	{
		//printf( "\toriginal file name present\n" );
		origFileNamePresent = true;
	}
	bool fileCommentPresent = false;
	if ( flags & 0x10 )
	{
		//printf( "\tfile comment present\n" );
		fileCommentPresent = true;
	}
	bool encyptionHeaderPresent = false;
	if ( flags & 0x20 )
	{
		//printf( "\tfile is encrypted, encryption header present\n" );
		encyptionHeaderPresent = true;
	}
	if ( flags & 0x40 )
	{
		//printf( "\treserved\n" );
		printf( "WARNING: Reserved flag is set - Don't know how to handle - Ignoring\n" );
	}
	if ( flags & 0x80 )
	{
		//printf( "\treserved\n" );
		printf( "WARNING: Reserved flag is set - Don't know how to handle - Ignoring\n" );
	}

	//printf( "modTime = 0x%08X\n", modTime );
	//printf( "extraFlags = 0x%02X\n", extraFlags );
	//printf( "osType = %d\n", osType );

	if ( partNumberPresent )
	{
		//2 bytes
		pEndHeader += 2;

		printf( "ERROR: multi-part gzip file not supported\n" );
		return false;
	}

	if ( extraFieldPresent )
	{
		//2 bytes give length "e"
		//then e bytes long
		uint16_t extraFieldLength = MAKE_U16( pEndHeader[1], pEndHeader[0] );
		pEndHeader += extraFieldLength;

		printf( "WARNING: extra field is present - untested code\n" );	//Maybe we can handle this one, assuming all the rest of the header looks fine?
	}

	if ( origFileNamePresent )
	{
		//null terminated string
		uint32_t origFileNameLen = strlen( (const char*) pEndHeader );
		pEndHeader += origFileNameLen + 1;
	}

	if ( fileCommentPresent )
	{
		//null terminated string
		uint32_t fileCommentLen = strlen( (const char*) pEndHeader );
		pEndHeader += fileCommentLen + 1;

		printf( "WARNING: file comment is present - untested code\n" );
	}

	if ( encyptionHeaderPresent )
	{
		//12 bytes
		pEndHeader += 12;

		printf( "ERROR: encryption header is present - encrypted files are not supported\n" );
		return NULL;
	}

	return pEndHeader;
}

//////////////////////////////////////////////////////////////////////////

uint32_t GzHeaderGetRawDataSize( const void* pData, uint32_t dataSize )
{
	const void* pRawData = GzHeaderFindRawData( pData );
	assert( pRawData );
	uint32_t headerSize = ((uint32_t)pRawData) - ((uint32_t)pData);
	uint32_t footerSize = 8;
	uint32_t rawDataSize = dataSize - headerSize - footerSize;
	return rawDataSize;
}

//////////////////////////////////////////////////////////////////////////

uint32_t GzHeaderGetUncompressedSize( const void* pData, uint32_t dataSize )
{
	const unsigned char* pBytes = (const unsigned char*) pData;
	//expectedUncompressedSize is stored in the footer of the file in the last 8 bits
	const unsigned char* pFooter = &pBytes[dataSize - 8];

	//uint32_t crc32 = MAKE_U32( pFooter[3], pFooter[2], pFooter[1], pFooter[0] );
	uint32_t expectedUncompressedSize = MAKE_U32( pFooter[7], pFooter[6], pFooter[5], pFooter[4] );

	return expectedUncompressedSize;
}

//////////////////////////////////////////////////////////////////////////
