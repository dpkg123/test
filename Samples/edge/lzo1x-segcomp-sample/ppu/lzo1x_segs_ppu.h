/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_LZO1X_SEGCOMP_SAMPLE_SEGS_PPU_H__
#define __EDGE_LZO1X_SEGCOMP_SAMPLE_SEGS_PPU_H__

#include "edge/edge_stdint.h"

#include "edge/lzo/edgelzo_ppu.h"

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

//////////////////////////////////////////////////////////////////////////

typedef const void*	EdgeSegCompSampleFileDataPtr;

//////////////////////////////////////////////////////////////////////////

extern uint32_t AddLzo1xSegsFileItemsToInflateQueue(
			EdgeSegCompSampleFileDataPtr	pSegsFileData,
			uint32_t						segsFileSize,
			EdgeLzo1xInflateQHandle			inflateQueue,
			unsigned char*					pOutputUncompBuff,
			uint32_t						outputBuffMaxSize,
			uint32_t*						pNumElementsToDecompress,
			CellSpursEventFlag*				pEventFlag,
			uint16_t						eventFlagBits );

extern void PrintLzo1xSegsHeaderAndToc(
			EdgeSegCompSampleFileDataPtr	pSegsFileData,
			const char*						segFileName,
			uint32_t						segsFileSize );

extern void TestValidLzo1xSegsFile(
			EdgeSegCompSampleFileDataPtr	pSegsFileData,
			const char*						segFileName,
			uint32_t						segsFileSize );

extern uint16_t GetLzo1xSegsFileNumSegments( EdgeSegCompSampleFileDataPtr pSegsFileData );

extern uint32_t GetLzo1xSegsFileTotalUncompressedSize( EdgeSegCompSampleFileDataPtr pSegsFileData );

extern uint32_t GetLzo1xSegsFileHeaderAndTocSize( EdgeSegCompSampleFileDataPtr pSegsFilesData );

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

#endif	//__EDGE_LZO1X_SEGCOMP_SAMPLE_SEGS_PPU_H__
