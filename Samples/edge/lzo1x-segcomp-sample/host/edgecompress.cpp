/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <assert.h>
#include <stdio.h>

#include "edgecompress.h"
#include "lzo/edgelzo1x_lzoconf.h"
#include "lzo/lzopro/edgelzo1x_lzo1x.h"



extern "C" 
{

/* ========================================================================= */

//////////////////////////////////////////////////////////////////////////

/* Work-memory needed for compression. Allocate memory in units
 * of `lzo_align_t' (instead of `char') to make sure it is properly aligned.
 */

#define HEAP_ALLOC(var,size) \
	lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

//static HEAP_ALLOC(wrkmem,LZOPRO1X_1_MEM_COMPRESS);
static HEAP_ALLOC(wrkmem,LZOPRO_LZO1X_1_14_MEM_COMPRESS);

/* ========================================================================= */

/** 
 * @brief	Compress the data using Lzopro on the PC
 *
 * @param	pCompr				ptr to output compressed data
 * @param	pComprSize			ptr to compressed data allocation.  Returned as compressed size used.
 * @param	pUncomp				ptr to uncompressed data
 * @param	uncomprSize			size of uncompressed data
 * @param	level				compression level (0:9)
 *
 * @return						0 if compression OK
 */
int segcompCompress( unsigned char* pCompr, uint32_t* pComprSize, const unsigned char* pUncomp, uint32_t uncomprSize, int level )
{
	// get compressed allocation, and ensure it is big enough
	uint32_t comprAllocation = *pComprSize;
	assert( comprAllocation >= (uncomprSize + uncomprSize / 16 + 64 + 3) );

	// compress data
	//int ret = lzopro1x_1_compress( pUncomp, uncomprSize, pCompr, (lzo_uint*)pComprSize, wrkmem );
	int ret;
	switch( level )
	{
		case 0:
		case 1:
			ret = lzopro_lzo1x_1_06_compress( pUncomp, uncomprSize, pCompr, (lzo_uint*)pComprSize, wrkmem );
			break;
		case 2:
			ret = lzopro_lzo1x_1_07_compress( pUncomp, uncomprSize, pCompr, (lzo_uint*)pComprSize, wrkmem );
			break;
		case 3:
			ret = lzopro_lzo1x_1_08_compress( pUncomp, uncomprSize, pCompr, (lzo_uint*)pComprSize, wrkmem );
			break;
		case 4:
			ret = lzopro_lzo1x_1_09_compress( pUncomp, uncomprSize, pCompr, (lzo_uint*)pComprSize, wrkmem );
			break;
		case 5:
			ret = lzopro_lzo1x_1_10_compress( pUncomp, uncomprSize, pCompr, (lzo_uint*)pComprSize, wrkmem );
			break;
		case 6:
			ret = lzopro_lzo1x_1_11_compress( pUncomp, uncomprSize, pCompr, (lzo_uint*)pComprSize, wrkmem );
			break;
		case 7:
			ret = lzopro_lzo1x_1_12_compress( pUncomp, uncomprSize, pCompr, (lzo_uint*)pComprSize, wrkmem );
			break;
		case 8:
			ret = lzopro_lzo1x_1_13_compress( pUncomp, uncomprSize, pCompr, (lzo_uint*)pComprSize, wrkmem );
			break;
		case 9:
			ret = lzopro_lzo1x_1_14_compress( pUncomp, uncomprSize, pCompr, (lzo_uint*)pComprSize, wrkmem );
			break;
		default:
			printf("Unknown compression level %d\n", level);
			assert(0);
	}

	if ( ret == LZO_E_OK  &&  *pComprSize <= comprAllocation )
	{
		return 0;	// OK
	}
	else
	{
		/* this should NEVER happen */
		printf("LZOPRO Inflate: internal error - decompression failed: %d\n", ret);
		assert( false );
		return 1;
	}
}

} // extern "C"
