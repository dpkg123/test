/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_LZMA_SEGCOMP_SAMPLE_SEGS_PPU_H__
#define __EDGE_LZMA_SEGCOMP_SAMPLE_SEGS_PPU_H__

#include "edge/edge_stdint.h"

#include "edge/lzma/edgelzma_ppu.h"

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

//////////////////////////////////////////////////////////////////////////

typedef const void*	EdgeSegCompSampleFileDataPtr;

//////////////////////////////////////////////////////////////////////////

extern void SplitLzmaDataIntoComponents
(	const void*				pLzmaData,
	uint32_t				lzmaDataSize,
	const unsigned char**	ppProperties,
	uint32_t*				pPropertiesSize,
	uint64_t*				pUncompSize,
	const unsigned char**	ppCompressedStream,
	uint32_t*				pCompressedStreamSize
);

extern uint32_t AddLzmaSegsFileItemsToInflateQueue(
			EdgeSegCompSampleFileDataPtr	pSegsFileData,
			uint32_t						segsFileSize,
			EdgeLzmaInflateQHandle			inflateQueue,
			unsigned char*					pOutputUncompBuff,
			uint32_t						outputBuffMaxSize,
			uint32_t*						pNumElementsToDecompress,
			CellSpursEventFlag*				pEventFlag,
			uint16_t						eventFlagBits );

extern void PrintLzmaSegsHeaderAndToc(
			EdgeSegCompSampleFileDataPtr	pSegsFileData,
			const char*						segFileName,
			uint32_t						segsFileSize );

extern void TestValidLzmaSegsFile(
			EdgeSegCompSampleFileDataPtr	pSegsFileData,
			const char*						segFileName,
			uint32_t						segsFileSize );

extern uint16_t GetLzmaSegsFileNumSegments( EdgeSegCompSampleFileDataPtr pSegsFileData );

extern uint32_t GetLzmaSegsFileTotalUncompressedSize( EdgeSegCompSampleFileDataPtr pSegsFileData );

extern uint32_t GetLzmaSegsFileHeaderAndTocSize( EdgeSegCompSampleFileDataPtr pSegsFilesData );

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

#endif	//__EDGE_LZMA_SEGCOMP_SAMPLE_SEGS_PPU_H__
