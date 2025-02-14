/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "file.h"

//////////////////////////////////////////////////////////////////////////

void* LoadFile( const char* filename, uint32_t* pFileSize )
{
	*pFileSize = 0;	//In case we fail, set size to zero

	FILE* fp = fopen( filename, "rb" );
	if ( NULL == fp )
	{
		printf( "ERROR: Failed to open \"%s\" for reading\n", filename );
		return NULL;
	}

	uint32_t ret = fseek( fp, 0, SEEK_END);
	if ( ret != 0 )
	{
		printf( "ERROR: failed to SEEK_END (\"%s\")\n", filename );
		fclose( fp );
		return NULL;
	}

	uint32_t fileSize = ftell( fp );
	if ( fileSize <= 0 )
	{
		printf( "ERROR: Bad file size (\"%s\")\n", filename );
		fclose( fp );
		return NULL;
	}

	ret = fseek( fp, 0, SEEK_SET );
	if ( ret != 0 )
	{
		printf( "ERROR: failed to SEEK_SET (\"%s\")\n", filename );
		fclose( fp );
		return NULL;
	}

	uint32_t mallocSize = (fileSize + 0xF) & ~0xF;	//Round up the malloc size to a qword multiple
	void* pData = malloc( mallocSize );
	if ( NULL == pData )
	{
		printf( "ERROR: Failed to allocate buffer of size %d\n", (int)mallocSize );
		fclose( fp );
		return NULL;
	}
	memset( pData, 0, mallocSize );

	size_t numRead = fread( pData, 1, fileSize, fp );
	if ( fileSize != numRead )
	{
		printf( "ERROR: Failed to read all data from \"%s\"\n", filename );
		fclose( fp );
		free( pData );
		return NULL;
	}

	ret = fclose( fp );
	if ( ret != 0 )
	{
		printf( "ERROR: Failed to close file (\"%s\")\n", filename );
		free( pData );
		return NULL;
	}

	*pFileSize = fileSize;
	return pData;
}

//////////////////////////////////////////////////////////////////////////

void FreeFile( void* pData )
{
	free( pData );
}

//////////////////////////////////////////////////////////////////////////
