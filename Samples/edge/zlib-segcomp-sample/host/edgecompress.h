/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_ZLIB_SEGCOMP_SAMPLE_COMPRESS_H__
#define __EDGE_ZLIB_SEGCOMP_SAMPLE_COMPRESS_H__

#include "zlib/deflate.h"

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

//////////////////////////////////////////////////////////////////////////

int ZEXPORT segcompCompress
(
	Bytef*			dest,
	uLongf*			destLen,
	const Bytef*	source,
	uLong			sourceLen,
	int				level
);

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __EDGE_ZLIB_SEGCOMP_SAMPLE_COMPRESS_H__ */
