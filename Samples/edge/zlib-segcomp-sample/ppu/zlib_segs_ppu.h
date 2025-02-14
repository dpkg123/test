/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_ZLIB_SEGCOMP_SAMPLE_SEGS_PPU_H__
#define __EDGE_ZLIB_SEGCOMP_SAMPLE_SEGS_PPU_H__

#include "edge/edge_stdint.h"

#include "edge/zlib/edgezlib_ppu.h"

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

//////////////////////////////////////////////////////////////////////////

typedef const void*	EdgeSegCompSampleFileDataPtr;

//////////////////////////////////////////////////////////////////////////

extern uint32_t AddZlibSegsFileItemsToInflateQueue(
			EdgeSegCompSampleFileDataPtr	pSegsFileData,
			uint32_t						segsFileSize,
			EdgeZlibInflateQHandle			inflateQueue,
			unsigned char*					pOutputUncompBuff,
			uint32_t						outputBuffMaxSize,
			uint32_t*						pNumElementsToDecompress,
			CellSpursEventFlag*				pEventFlag,
			uint16_t						eventFlagBits );

extern void PrintZlibSegsHeaderAndToc(
			EdgeSegCompSampleFileDataPtr	pSegsFileData,
			const char*						segFileName,
			uint32_t						segsFileSize );

extern void TestValidZlibSegsFile(
			EdgeSegCompSampleFileDataPtr	pSegsFileData,
			const char*						segFileName,
			uint32_t						segsFileSize );

extern uint16_t GetZlibSegsFileNumSegments( EdgeSegCompSampleFileDataPtr pSegsFileData );

extern uint32_t GetZlibSegsFileTotalUncompressedSize( EdgeSegCompSampleFileDataPtr pSegsFileData );

extern uint32_t GetZlibSegsFileHeaderAndTocSize( EdgeSegCompSampleFileDataPtr pSegsFilesData );

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

#endif	//__EDGE_ZLIB_SEGCOMP_SAMPLE_SEGS_PPU_H__
