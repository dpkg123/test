/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_LZO1X_SEGCOMP_SAMPLE_COMPRESS_H__
#define __EDGE_LZO1X_SEGCOMP_SAMPLE_COMPRESS_H__

#include "edge/edge_stdint.h"

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

//////////////////////////////////////////////////////////////////////////

int segcompCompress( unsigned char* pCompr,
					uint32_t* pComprSize,
					const unsigned char* pUncomp,
					uint32_t uncomprSize, int level );

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __EDGE_LZO1X_SEGCOMP_SAMPLE_COMPRESS_H__ */
