/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "edgegeomcompiler.h"

#include "edge/libedgegeomtool/libedgegeomtool_wrap.h"

#include <sdk_version.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

///////////////////////////////////////////////////////////////////////
// Everything below this point is related to file output of the EdgeGeomSegments.
///////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------
/**
	Class to assist in the creation of a binary file in memory.
**/
//--------------------------------------------------------------------------------------------------

class BinaryFileHelper
{
private:
	void*		m_container;				// the allocated memory containing the file data
	uint32_t	m_size;						// (used) size of the data
	uint32_t	m_allocatedSize;			// size of the allocation

public:
	// constructor
	BinaryFileHelper()
	{
		m_size = 0;
		m_allocatedSize = 0x10000;
		m_container = malloc( m_allocatedSize );
	}

	// destructor
	~BinaryFileHelper()
	{
		free( m_container );
	}

	// allocate a block
	// NB the returned pointer is only valid until the next call to Allocate()
	void*	Allocate( uint32_t allocationSize, uint32_t alignment, uint32_t& offset )
	{
		// must be power of 2
		assert((alignment & (alignment-1)) == 0);

		// position of new object
		uint32_t pos = m_size;
		if ( alignment )
			pos = ( pos + alignment - 1 ) & ~( alignment - 1 );

		// do we have space?
		if ( pos + allocationSize >= m_allocatedSize )
		{
			uint32_t extra = allocationSize + 0x10000;
			m_allocatedSize += extra;
			m_container = realloc( m_container, m_allocatedSize );
		}

		// zero any pad bytes
		if ( pos > m_size )
			memset( GetPointer( m_size ), 0, pos-m_size );

		// increment the used size
		m_size = pos + allocationSize;
		
		offset = pos;
		return (void*)((uint32_t)m_container + pos);
	}

	// return a pointer to data within the container
	// NB the returned pointer is only valid until the next call to Allocate()
	void*	GetPointer( uint32_t offset )
	{
		return (void*)((uint32_t)m_container + offset);
	}

	// get the data size
	uint32_t GetSize() const
	{
		return m_size;
	}
};

//--------------------------------------------------------------------------------------------------
/**
	The file header structure.
**/
//--------------------------------------------------------------------------------------------------

struct FileHeader
{
	uint32_t	m_numSegments;
	uint32_t	m_offsetToSegments;
	uint32_t	m_numPatches;
	uint32_t	m_offsetToPatches;
	int32_t	m_edgeSdkVersion;
	int32_t	m_cellSdkVersion;
};

//--------------------------------------------------------------------------------------------------
/**
	Reverse methods. All output words are routed through these.
	Replace with straight-through methods if tool and target endiannesses are the same.
**/
//--------------------------------------------------------------------------------------------------

static inline uint8_t		Reverse( uint8_t v )
{
   return v;
}

static inline uint16_t	Reverse( uint16_t v )
{
   return ( ( v & 0xFF00 ) >> 8 ) | ( ( v & 0x00FF ) << 8 );
}

static inline uint32_t			Reverse( uint32_t v )
{
   return ( ( v & 0xFF000000 ) >> 24 ) | ( ( v & 0x00FF0000 ) >> 8 ) | ( ( v & 0x0000FF00 ) << 8 ) | ( ( v & 0x000000FF ) << 24 );
}

//--------------------------------------------------------------------------------------------------
/**
	Utility class to make stdio FILE usage exception safe.
**/
//--------------------------------------------------------------------------------------------------

class FileCloser
{
	FILE*	m_F;
public:
	FileCloser( FILE* F ) : m_F( F ) {}
	~FileCloser()
	{
		if ( m_F ) fclose( m_F );
	}
	void	Close()
	{
		fclose( m_F );
		m_F = 0;
	}
};

//--------------------------------------------------------------------------------------------------

// used by DumpToFile() to output a single binary block
static void DumpBlock(FILE *f, unsigned char const *data, unsigned size)
{
	for (unsigned i=0; i<size; i++)
	{
		fprintf(f, "0x%02X, ", data[i]);
		if ((i % 16) == 15)
			fprintf(f, "\n\t");
	}
}

//--------------------------------------------------------------------------------------------------

// Output a header file containing an array of pointers to segments and a segment per entry in the EdgeGeomLibPpuConfigInfo vector.
void Edge::Tools::DumpToFile( const char *filename, const EdgeGeomSegment *segments, uint32_t numSegments)
{
	FILE *f = fopen(filename, "wt");
	assert(f != NULL);

	fprintf(f, "//-------------------\n");
	fprintf(f, "// %s\n", filename);
	fprintf(f, "// Built with Edge SDK %08X\n", EDGE_SDK_VERSION);
	fprintf(f, "// Built with Cell SDK %08X\n", CELL_SDK_VERSION);
	fprintf(f, "//-------------------\n\n");

	// dump out the big binary blocks first, so the bottom of the file only has the config data in it (easier to look at that way)
	for (unsigned iSegment=0; iSegment<numSegments; iSegment++)
	{
		const EdgeGeomSegment &segment = segments[iSegment];

		// write index table
		if (segment.m_indexesSizes[0] + segment.m_indexesSizes[1] > 0)
		{
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dIndexes[] = {\n\t", iSegment);
			DumpBlock(f, segment.m_indexes, segment.m_indexesSizes[0] + segment.m_indexesSizes[1]);
			fprintf(f, "};\n");
		}

		// write primary SPU vertex buffer
		if (segment.m_spuVertexesSizes[0] + segment.m_spuVertexesSizes[1] + segment.m_spuVertexesSizes[2] > 0)
		{
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dSpuVertexes[] = {\n\t", iSegment);
			DumpBlock(f, segment.m_spuVertexes[0], segment.m_spuVertexesSizes[0] + segment.m_spuVertexesSizes[1] + segment.m_spuVertexesSizes[2]);
			fprintf(f, "};\n");
		}

		// write secondary SPU vertex buffer
		if (segment.m_spuVertexesSizes[3] + segment.m_spuVertexesSizes[4] + segment.m_spuVertexesSizes[5] > 0)
		{
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dSpuVertexes2[] = {\n\t", iSegment);
			DumpBlock(f, segment.m_spuVertexes[1], segment.m_spuVertexesSizes[3] + segment.m_spuVertexesSizes[4] + segment.m_spuVertexesSizes[5]);
			fprintf(f, "};\n");
		}

		// write RSX-only vertex buffer
		if (segment.m_rsxOnlyVertexesSize > 0)
		{
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dRsxOnlyVertexes[] = {\n\t", iSegment);
			DumpBlock(f, segment.m_rsxOnlyVertexes, segment.m_rsxOnlyVertexesSize);
			fprintf(f, "};\n");
		}

		// write primary fixed point offsets
		if (segment.m_fixedOffsetPtrs[0] != NULL)
		{
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dFixedOffsets[] = {\n\t", iSegment);
			DumpBlock(f, (const uint8_t*)segment.m_fixedOffsetPtrs[0], segment.m_fixedOffsetsSize[0]);
			fprintf(f, "};\n");
		}
		// write secondary fixed point offsets
		if (segment.m_fixedOffsetPtrs[1] != NULL)
		{
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dFixedOffsets2[] = {\n\t", iSegment);
			DumpBlock(f, (const uint8_t*)segment.m_fixedOffsetPtrs[1], segment.m_fixedOffsetsSize[1]);
			fprintf(f, "};\n");
		}

		// write skin info
		if (segment.m_skinIndexesAndWeightsSizes[0] + segment.m_skinIndexesAndWeightsSizes[1] > 0)
		{
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dSkinInfo[] = {\n\t", iSegment);
			DumpBlock(f, segment.m_skinIndexesAndWeights, segment.m_skinIndexesAndWeightsSizes[0] + segment.m_skinIndexesAndWeightsSizes[1]);
			fprintf(f, "};\n");
		}

		// Write stream descriptions
		if (segment.m_spuInputStreamDescriptionSizes[0] > 0)
		{
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dSpuInputStreamDesc[] = {\n\t", iSegment);
			DumpBlock(f, segment.m_spuInputStreamDescriptions[0], segment.m_spuInputStreamDescriptionSizes[0]);
			fprintf(f, "};\n");
		}
		if (segment.m_spuInputStreamDescriptionSizes[1] > 0)
		{
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dSpuInputStreamDesc2[] = {\n\t", iSegment);
			DumpBlock(f, segment.m_spuInputStreamDescriptions[1], segment.m_spuInputStreamDescriptionSizes[1]);
			fprintf(f, "};\n");
		}
		if (segment.m_rsxOnlyStreamDescriptionSize > 0)
		{
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dRsxOnlyStreamDesc[] = {\n\t", iSegment);
			DumpBlock(f, segment.m_rsxOnlyStreamDescription, segment.m_rsxOnlyStreamDescriptionSize);
			fprintf(f, "};\n");
		}
		if (segment.m_spuOutputStreamDescriptionSize > 0)
		{
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dSpuOutputStreamDesc[] = {\n\t", iSegment);
			DumpBlock(f, segment.m_spuOutputStreamDescription, segment.m_spuOutputStreamDescriptionSize);
			fprintf(f, "};\n");
		}

		// write blend shapes
		if (segment.m_numBlendShapes > 0)
		{
			// write the blend shape sizes array
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned short Segment%dBlendShapeSizes[] = {\n", iSegment);
			for (uint32_t iShape=0; iShape<segment.m_numBlendShapes; ++iShape)
			{
				fprintf(f, "\t%d,\n", segment.m_blendShapeSizes[iShape]);
			}
			fprintf(f, "};\n");

			// Write the blend shapes themselves
			for (uint32_t iShape=0; iShape<segment.m_numBlendShapes; ++iShape)
			{
				if (segment.m_blendShapes[iShape] != NULL)  // if a shape exists, dump out its data
				{
					fprintf(f, "\n//-------------------\n\n");
					fprintf(f, "__attribute__((__aligned__(16))) unsigned char Segment%dBlendShape%d[] = {\n\t", iSegment, iShape);
					DumpBlock(f, segment.m_blendShapes[iShape], segment.m_blendShapeSizes[iShape]);
					fprintf(f, "};\n");
				}
			}

			// write the blend shape array too
			fprintf(f, "\n//-------------------\n\n");
			fprintf(f, "__attribute__((__aligned__(16))) unsigned char * Segment%dBlendShapes[] = {\n", iSegment);
			for (uint32_t iShape=0; iShape<segment.m_numBlendShapes; ++iShape)
			{
				if (segment.m_blendShapes[iShape])
				{
					fprintf(f, "\t&Segment%dBlendShape%d[0],\n", iSegment, iShape);
				}
				else
				{
					fprintf(f, "\tNULL,\n");
				}
			}
			fprintf(f, "};\n");
		}
	}

	fprintf(f, "//-------------------\n\n");

	// write out each segment's EdgeGeomPpuConfigInfo
	for (uint32_t iSegment=0; iSegment<numSegments; iSegment++)
	{
		const EdgeGeomSegment &segment = segments[iSegment];

		// EdgeGeomLibPpuConfigInfo
		fprintf(f, "EdgeGeomPpuConfigInfo Segment%d = \n", iSegment);
		fprintf(f, "{\n");

		// dump out the EdgeGeomSpuConfigInfo.  Note that the buffer we store in the segment
		// is already byte-swapped, so we must byte-swap it back to dump it as text
		const EdgeGeomSpuConfigInfo *spu = (const EdgeGeomSpuConfigInfo*)segment.m_spuConfigInfo;
		fprintf(f, "\t{\n");
		fprintf(f, "\t\t0x%02X, // flagsAndUniformTableCount\n", Reverse(spu->flagsAndUniformTableCount) );
		fprintf(f, "\t\t0x%02X, // commandBufferHoleSize\n", Reverse(spu->commandBufferHoleSize) );
		fprintf(f, "\t\t0x%02X, // inputVertexFormatId\n", Reverse(spu->inputVertexFormatId) );
		fprintf(f, "\t\t0x%02X, // secondaryInputVertexFormatId\n", Reverse(spu->secondaryInputVertexFormatId) );
		fprintf(f, "\t\t0x%02X, // outputVertexFormatId\n", Reverse(spu->outputVertexFormatId) );
		fprintf(f, "\t\t0x%02X, // vertexDeltaFormatId\n", Reverse(spu->vertexDeltaFormatId) );
		fprintf(f, "\t\t0x%02X, // indexesFlavorAndSkinningFlavor\n", Reverse(spu->indexesFlavorAndSkinningFlavor) );

		fprintf(f, "\t\t0x%02X, // skinningMatrixFormat\n", Reverse(spu->skinningMatrixFormat));

		fprintf(f, "\t\t%d, // numVertexes\n", Reverse(spu->numVertexes) );
		fprintf(f, "\t\t%d, // numIndexes\n", Reverse(spu->numIndexes) );

		fprintf(f, "\t\t0x%08X, // indexesOffset\n", Reverse(spu->indexesOffset) );
		fprintf(f, "\t},\n");

		// index buffer
		if (segment.m_indexesSizes[0] + segment.m_indexesSizes[1] > 0)
			fprintf(f, "\t&Segment%dIndexes[0],\n", iSegment);
		else fprintf(f, "\tNULL,\n");
		fprintf(f, "\t{ %d, %d }, // indexesSizes[2]\n", segment.m_indexesSizes[0], segment.m_indexesSizes[1]);
		
		// vertex buffers
		fprintf(f, "\t{ ");
		if (segment.m_spuVertexesSizes[0] + segment.m_spuVertexesSizes[1] + segment.m_spuVertexesSizes[2] > 0)
			fprintf(f, "&Segment%dSpuVertexes[0], ", iSegment);
		else fprintf(f, "NULL, ");
		if (segment.m_spuVertexesSizes[3] + segment.m_spuVertexesSizes[4] + segment.m_spuVertexesSizes[5])
			fprintf(f, "&Segment%dSpuVertexes2[0]", iSegment);
		else fprintf(f, "NULL");
		fprintf(f, " }, // spuVertexes\n");
		fprintf(f, "\t{ %d, %d, %d, %d, %d, %d }, // spuVertexesSizes[6]\n", segment.m_spuVertexesSizes[0], segment.m_spuVertexesSizes[1],
			segment.m_spuVertexesSizes[2], segment.m_spuVertexesSizes[3], segment.m_spuVertexesSizes[4], segment.m_spuVertexesSizes[5]);
		// RSX-only vertex buffer
		if (segment.m_rsxOnlyVertexesSize > 0)
			fprintf(f, "\t&Segment%dRsxOnlyVertexes[0], // rsxOnlyVertexes\n\t%d, // rsxOnlyVertexesSize\n", iSegment, segment.m_rsxOnlyVertexesSize);
		else fprintf(f, "\tNULL,\n\t0,\n");

		// skinning stuff
		fprintf(f, "\t{ %d, %d }, // skinMatricesByteOffsets[2]\n", segment.m_skinMatricesByteOffsets[0], segment.m_skinMatricesByteOffsets[1]);
		fprintf(f, "\t{ %d, %d }, // skinMatricesSizes[2]\n", segment.m_skinMatricesSizes[0], segment.m_skinMatricesSizes[1]);
		fprintf(f, "\t{ %d, %d }, // skinIndexesAndWeightSizes[2]\n", segment.m_skinIndexesAndWeightsSizes[0], segment.m_skinIndexesAndWeightsSizes[1]);
		if (segment.m_skinIndexesAndWeightsSizes[0] + segment.m_skinIndexesAndWeightsSizes[1])
			fprintf(f, "\t&Segment%dSkinInfo[0],\n", iSegment);
		else fprintf(f, "\tNULL,\n");

		fprintf(f, "\t%d, // ioBufferSize\n", segment.m_ioBufferSize);
		fprintf(f, "\t%d, // scratchSize\n", segment.m_scratchSize);

		// Blend shapes
		fprintf(f, "\t%d, // numBlendShapes\n", segment.m_numBlendShapes);
		if (segment.m_numBlendShapes > 0)
			fprintf(f, "\t(uint16_t*)&Segment%dBlendShapeSizes[0],\n", iSegment);
		else fprintf(f, "\tNULL,\n");
		if (segment.m_numBlendShapes > 0)
			fprintf(f, "\t(uint32_t*)(void*)&Segment%dBlendShapes[0],\n", iSegment);
		else fprintf(f, "\tNULL,\n");

		// Fixed point offsets
		fprintf(f, "\t{%d,%d}, // fixed point offsets size\n", segment.m_fixedOffsetsSize[0], segment.m_fixedOffsetsSize[1]);
		fprintf(f, "\t{");
		if (segment.m_fixedOffsetsSize[0] > 0)
			fprintf(f, "&Segment%dFixedOffsets[0], ", iSegment);
		else
			fprintf(f, "NULL, ");
		if (segment.m_fixedOffsetsSize[1] > 0)
			fprintf(f, "&Segment%dFixedOffsets2[0]}, // fixed point offsets ptr\n", iSegment);
		else
			fprintf(f, "NULL}, // fixed point offsets ptr\n");

		// Stream descriptions
		fprintf(f, "\t{ ");
		if (segment.m_spuInputStreamDescriptionSizes[0] > 0)
			fprintf(f, "&Segment%dSpuInputStreamDesc[0], ", iSegment);
		else fprintf(f, "NULL, ");
		if (segment.m_spuInputStreamDescriptionSizes[1] > 0)
			fprintf(f, "&Segment%dSpuInputStreamDesc2[0]", iSegment);
		else fprintf(f, "NULL");
		fprintf(f, "\t}, // spuInputStreamDescs[2]\n");
		if (segment.m_spuOutputStreamDescriptionSize > 0)
			fprintf(f, "\t&Segment%dSpuOutputStreamDesc[0],\n", iSegment);
		else fprintf(f, "\tNULL,\n");
		if (segment.m_rsxOnlyStreamDescriptionSize > 0)
			fprintf(f, "\t&Segment%dRsxOnlyStreamDesc[0],\n", iSegment);
		else fprintf(f, "\tNULL,\n");
		fprintf(f, "\t{%d, %d},\n", segment.m_spuInputStreamDescriptionSizes[0], segment.m_spuInputStreamDescriptionSizes[1]);
		fprintf(f, "\t%d,\n", segment.m_spuOutputStreamDescriptionSize);
		fprintf(f, "\t%d,\n", segment.m_rsxOnlyStreamDescriptionSize);

		fprintf(f, "};\n");
		
		fprintf(f, "\n//-------------------\n\n");
	}

	// finally, write the linkage array that gives the runtime the ability to traverse all segments
	fprintf(f, "EdgeGeomPpuConfigInfo *allSegments[] = {\n");
	for (uint32_t iSegment=0; iSegment<numSegments; iSegment++)
	{
		fprintf(f, "&Segment%d,\n", iSegment);
	}
	fprintf(f, "};\n\n");
	fclose(f);
}

//--------------------------------------------------------------------------------------------------
/**
	Write a geometry file to disk.
**/
//--------------------------------------------------------------------------------------------------

void Edge::Tools::WriteGeomFile( const char *filename, const EdgeGeomSegment *segments, uint32_t numSegments)
{
	// open the output file for write
	FILE *fp = fopen( filename, "wb" );
	FileCloser fileCloser( fp );
	if ( !fp )
	{
		printf( "Error: Cannot open file %s for writing.\n", filename );
		assert(fp != NULL);
	}

	// the list of patches i.e. offsets written to the file to be resolved to pointers at runtime
	std::vector<uint32_t> patches;

	// offsets of segments in the file
	std::vector<uint32_t> segmentOffsets;

	// struct to build up the file in memory
	BinaryFileHelper helper;

	// prepare the file header
	uint32_t headerOffset;
	FileHeader* header = (FileHeader*)helper.Allocate( sizeof(FileHeader), 0, headerOffset );
	header->m_cellSdkVersion = Reverse((uint32_t)CELL_SDK_VERSION);
	header->m_edgeSdkVersion = Reverse((uint32_t)EDGE_SDK_VERSION);
	patches.push_back( Reverse( headerOffset + (uint32_t)offsetof( FileHeader, m_offsetToSegments ) ) );
	patches.push_back( Reverse( headerOffset + (uint32_t)offsetof( FileHeader, m_offsetToPatches ) ) );
	
	// loop over output segments
	for ( uint32_t iSegment=0; iSegment<numSegments; ++iSegment )
	{
		// segment we want to write
		const EdgeGeomSegment *s = &(segments[iSegment]);
		// copy of segment in output file
		uint32_t segmentOffset;
		EdgeGeomPpuConfigInfo* d = (EdgeGeomPpuConfigInfo*)helper.Allocate( sizeof(EdgeGeomPpuConfigInfo), 16, segmentOffset );
		segmentOffsets.push_back( segmentOffset );
		// make byte swapped copy
		memset( d, 0, sizeof(*d) );
		memcpy(&(d->spuConfigInfo), s->m_spuConfigInfo, sizeof(EdgeGeomSpuConfigInfo));
		for ( uint32_t j=0; j<2; ++j )
			d->indexesSizes[j]							= Reverse( s->m_indexesSizes[j] );
		for ( uint32_t j=0; j<6; ++j )
			d->spuVertexesSizes[j]						= Reverse( s->m_spuVertexesSizes[j] );
		d->rsxOnlyVertexesSize							= Reverse( s->m_rsxOnlyVertexesSize );
		for ( uint32_t j=0; j<2; ++j )
			d->skinMatricesByteOffsets[j]				= Reverse( s->m_skinMatricesByteOffsets[j] );
		for ( uint32_t j=0; j<2; ++j )
			d->skinMatricesSizes[j]						= Reverse( s->m_skinMatricesSizes[j] );
		for ( uint32_t j=0; j<2; ++j )
			d->skinIndexesAndWeightsSizes[j]			= Reverse( s->m_skinIndexesAndWeightsSizes[j] );
		d->ioBufferSize									= Reverse( s->m_ioBufferSize );
		d->scratchSizeInQwords							= Reverse( s->m_scratchSize );
		d->numBlendShapes                               = Reverse( s->m_numBlendShapes );
		for ( uint32_t j=0; j<2; ++j)
			d->fixedOffsetsSize[j]						= Reverse( s->m_fixedOffsetsSize[j] );
		for ( uint32_t j=0; j<2; ++j)
			d->spuInputStreamDescSizes[j]				= Reverse( s->m_spuInputStreamDescriptionSizes[j] );
		d->spuOutputStreamDescSize						= Reverse( s->m_spuOutputStreamDescriptionSize );
		d->rsxOnlyStreamDescSize						= Reverse( s->m_rsxOnlyStreamDescriptionSize );
	}

	// link header to segments
	header = (FileHeader*)helper.GetPointer( headerOffset );
	header->m_numSegments			= Reverse( (uint32_t)numSegments );
	header->m_offsetToSegments	= Reverse( segmentOffsets[0] );
	
	// dump binary blocks
	for ( uint32_t iSegment=0; iSegment<numSegments; ++iSegment )
	{
		// segment we want to write
		const EdgeGeomSegment *s = &(segments[iSegment]);

#define ADD_BLOCK( sizeExpression, alignment, smember, dmember, ptrType )												\
		{																										\
			uint32_t size = (sizeExpression);																	\
			if ( size )																							\
			{																									\
				uint32_t offset;																				\
				void* ptr = helper.Allocate( size, alignment, offset );											\
				memcpy( ptr, s->smember, size );																\
				EdgeGeomPpuConfigInfo* d = (EdgeGeomPpuConfigInfo*)helper.GetPointer( segmentOffsets[iSegment] );						\
				uint32_t revOffset = Reverse(offset);															\
				d->dmember = (ptrType)revOffset;																\
				patches.push_back( Reverse(segmentOffsets[iSegment] + (uint32_t)offsetof(EdgeGeomPpuConfigInfo, dmember)) );				\
			}																									\
		}

		// indexes
		ADD_BLOCK( s->m_indexesSizes[0]+s->m_indexesSizes[1], 128, m_indexes, indexes, voidptr32_t );
		// vertexes
		ADD_BLOCK( s->m_spuVertexesSizes[0]+s->m_spuVertexesSizes[1]+s->m_spuVertexesSizes[2], 16, m_spuVertexes[0], spuVertexes[0], voidptr32_t );
		ADD_BLOCK( s->m_spuVertexesSizes[3]+s->m_spuVertexesSizes[4]+s->m_spuVertexesSizes[5], 16, m_spuVertexes[1], spuVertexes[1], voidptr32_t );
		ADD_BLOCK( s->m_rsxOnlyVertexesSize, 16, m_rsxOnlyVertexes, rsxOnlyVertexes, voidptr32_t );
		// skinning
		ADD_BLOCK( s->m_skinIndexesAndWeightsSizes[0]+s->m_skinIndexesAndWeightsSizes[1], 16, m_skinIndexesAndWeights, skinIndexesAndWeights, voidptr32_t );
		// fixed offsets
		ADD_BLOCK( s->m_fixedOffsetsSize[0], 16, m_fixedOffsetPtrs[0], fixedOffsets[0], voidptr32_t );
		ADD_BLOCK( s->m_fixedOffsetsSize[1], 16, m_fixedOffsetPtrs[1], fixedOffsets[1], voidptr32_t );
		// blend shapes
		const uint32_t numBlendShapes = s->m_numBlendShapes;
		if (numBlendShapes > 0 && s->m_blendShapes != NULL)
		{
			// Write the blend shape sizes array, byte-swapping each entry in the output
			{
				uint32_t sizesOffset;
				helper.Allocate( numBlendShapes * sizeof(uint16_t), sizeof(uint16_t), sizesOffset );
				uint16_t *sizes = (uint16_t*)helper.GetPointer(sizesOffset);
				for(uint32_t iShape=0; iShape<numBlendShapes; ++iShape)
					sizes[iShape] = Reverse( s->m_blendShapeSizes[iShape] );
				EdgeGeomPpuConfigInfo* d = (EdgeGeomPpuConfigInfo*)helper.GetPointer( segmentOffsets[iSegment] );
				d->blendShapeSizes = (uint16ptr32_t)Reverse( sizesOffset );
				patches.push_back( Reverse(segmentOffsets[iSegment] + (uint32_t)offsetof(EdgeGeomPpuConfigInfo, blendShapeSizes)) );
			}
			// make space for the array of pointers
			uint32_t arrayOffset;
			helper.Allocate( numBlendShapes * sizeof(uint32_t), sizeof(uint32_t), arrayOffset );
			memset( helper.GetPointer( arrayOffset ), 0, numBlendShapes * sizeof(uint32_t));
			EdgeGeomPpuConfigInfo* d = (EdgeGeomPpuConfigInfo*)helper.GetPointer( segmentOffsets[iSegment] );
			d->blendShapes = (uint32ptr32_t)Reverse( arrayOffset );
			patches.push_back( Reverse(segmentOffsets[iSegment] + (uint32_t)offsetof(EdgeGeomPpuConfigInfo, blendShapes)) );
			// add actual blend shape data
			for ( uint32_t iShape=0; iShape<numBlendShapes; ++iShape )
			{
				if(s->m_blendShapes[iShape])
				{
					uint32_t offset;
					void *ptr = helper.Allocate( s->m_blendShapeSizes[iShape], 16, offset );
					memcpy( ptr, s->m_blendShapes[iShape], s->m_blendShapeSizes[iShape] );
					uint32ptr32_t* d = (uint32ptr32_t*)helper.GetPointer( arrayOffset );
					d[iShape] = (uint32ptr32_t)Reverse( offset );
					patches.push_back( Reverse( arrayOffset + iShape*(uint32_t)sizeof(uint32_t) ) );
				}
			}
		}
		// stream descriptions
		ADD_BLOCK( s->m_spuInputStreamDescriptionSizes[0], 16, m_spuInputStreamDescriptions[0], spuInputStreamDescs[0], voidptr32_t );
		ADD_BLOCK( s->m_spuInputStreamDescriptionSizes[1], 16, m_spuInputStreamDescriptions[1], spuInputStreamDescs[1], voidptr32_t );
		ADD_BLOCK( s->m_spuOutputStreamDescriptionSize, 16, m_spuOutputStreamDescription, spuOutputStreamDesc, voidptr32_t );
		ADD_BLOCK( s->m_rsxOnlyStreamDescriptionSize, 16, m_rsxOnlyStreamDescription, rsxOnlyStreamDesc, voidptr32_t );
	}
	
	// write patch table
	uint32_t patchTableRegion;
	uint32_t* patchTable = (uint32_t*)helper.Allocate( sizeof(uint32_t) * (uint32_t)patches.size(), 4, patchTableRegion );
	memcpy( patchTable, &patches[0], sizeof(uint32_t) * patches.size() );

	// link header to patch table
	header = (FileHeader*)helper.GetPointer( headerOffset );
	header->m_numPatches		= Reverse( (uint32_t)patches.size() );
	header->m_offsetToPatches	= Reverse( patchTableRegion );

	// finally write the whole thing to disk
	fwrite( helper.GetPointer(0), helper.GetSize(), 1, fp );

	// close the file
	fileCloser.Close();
}
