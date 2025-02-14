/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_SEGCOMP_SAMPLE_SEG_COMP_FILE_H__
#define __EDGE_SEGCOMP_SAMPLE_SEG_COMP_FILE_H__

#include "endian.h"
#include "edge/lzo/edgelzo_aligned.h"

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

//////////////////////////////////////////////////////////////////////////

//All data is stored in big endian format

struct EDGELZO_ALIGNED(8) EdgeSegCompSampleTocElement
{
	EdgeSegCompSampleBigEndianU16		m_compressedSize;	//'0x0000' means 64K (m_compressedSize need not be a qword multiple)
	EdgeSegCompSampleBigEndianU16		m_uncompressedSize;	//'0x0000' means 64K
	EdgeSegCompSampleBigEndianU32		m_fileOffset;		//segments necessarily start at qword offsets
															//LSB is a flag to say if segment was stored compressed (1) or uncompressed (0)
};

//////////////////////////////////////////////////////////////////////////

struct EDGELZO_ALIGNED(16) EdgeSegCompSampleFileHeader
{
	enum EdgeSegCompDecompressionType
	{
		kDecompressionType_EdgeZlib		= 0,
		kDecompressionType_EdgeLzma		= 1,
		kDecompressionType_EdgeLzo1x	= 2,
	};

	static const uint32_t				kMagic				= 0x73656773;	//"segs"
	static const uint16_t				kFileVersionNumber	= 5;

	EdgeSegCompSampleBigEndianU32		m_magic;
	EdgeSegCompSampleBigEndianU8		m_decompressionType;
	EdgeSegCompSampleBigEndianU8		m_versionNumber;
	EdgeSegCompSampleBigEndianU16		m_numSegments;
	EdgeSegCompSampleBigEndianU32		m_totalUncompressedSize;
	EdgeSegCompSampleBigEndianU32		m_segsFileSize;
//Header is immediately followed by TOC array:
	EdgeSegCompSampleTocElement			m_elements[0 /*m_numSegments*/];
};

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __EDGE_SEGCOMP_SAMPLE_SEG_COMP_FILE_H__ */
