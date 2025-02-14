/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_SEGCOMP_SAMPLE_FILE_H__
#define __EDGE_SEGCOMP_SAMPLE_FILE_H__

#include "../../../../target/common/include/edge/edge_stdint.h"

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

//////////////////////////////////////////////////////////////////////////

extern void* LoadFile( const char* filename, uint32_t* pFileSize );
extern void FreeFile( void* pData );

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __EDGE_SEGCOMP_SAMPLE_FILE_H__ */
