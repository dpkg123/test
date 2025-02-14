/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_LZO1X_SEGCOMP_SAMPLE_SEGS_SPU_H__
#define __EDGE_LZO1X_SEGCOMP_SAMPLE_SEGS_SPU_H__

#include "edge/edge_stdint.h"

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

//////////////////////////////////////////////////////////////////////////

struct EdgeSegCompSampleFileHeader;

//////////////////////////////////////////////////////////////////////////

extern void MoveBuffersAndAddLzo1xSegsFileItemsToInflateQueue(
			const EdgeSegCompSampleFileHeader*	pSegsHeader,
			uint32_t							eaSegsFileData,
			uint32_t							segsFileSize,
			uint32_t							eaInflateQueue,
			uint32_t							eaOutputUncompBuff,
			uint32_t							outputBuffMaxSize,
			uint32_t							eaNumElementsToDecompress,
			uint32_t							eaEventFlag,
			uint16_t							eventFlagBits,
			uint32_t							dmaTagId );

extern void AddLzo1xSegsFileItemsToInflateQueue(
			const EdgeSegCompSampleFileHeader*	pSegsHeader,
			uint32_t							eaSegsFileData,
			uint32_t							segsFileSize,
			uint32_t							eaInflateQueue,
			uint32_t							eaOutputUncompBuff,
			uint32_t							outputBuffMaxSize,
			uint32_t							eaNumElementsToDecompress,
			uint32_t							eaEventFlag,
			uint16_t							eventFlagBits,
			uint32_t							dmaTagId );

extern void TestValidLzo1xSegsFile( const EdgeSegCompSampleFileHeader* pSegsHeader, const char* segFileName, uint32_t segsFileSize );

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

#endif	//__EDGE_LZO1X_SEGCOMP_SAMPLE_SEGS_SPU_H__
