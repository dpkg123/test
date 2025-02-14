/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __ZLIB_SAMPLE_GZ_H__
#define __ZLIB_SAMPLE_GZ_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

//////////////////////////////////////////////////////////////////////////

extern const void* GzHeaderFindRawData( const void* pData );
extern uint32_t GzHeaderGetRawDataSize( const void* pData, uint32_t dataSize );
extern uint32_t GzHeaderGetUncompressedSize( const void* pData, uint32_t dataSize );

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __ZLIB_SAMPLE_GZ_H__
