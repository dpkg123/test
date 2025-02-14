/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef LIBEDGEGEOMTOOL_INTERNAL_H
#define LIBEDGEGEOMTOOL_INTERNAL_H

//-------------------

#include "libedgegeomtool.h"

//-------------------

#define EDGESTRINGIFY_(x) #x
#define EDGESTRINGIFY(x) EDGESTRINGIFY_(x)
#if defined(_MSC_VER)
#include <exception>
#define EDGEERROR_F() { throw std::exception(__FILE__ "(" EDGESTRINGIFY(__LINE__) ")"); }
#define EDGEASSERT(x) { if (!(x)) EDGEERROR_F() }
#elif defined(__PPU__)
#include <assert.h> // TODO: use edge/edge_assert.h
#define EDGEERROR_F() { assert(0); }
#define EDGEASSERT(x) { if (!(x)) assert(0); }
#endif
namespace Edge
{
	template<typename T>
	T min(const T &x, const T &y) { return x<y ? x : y; }
	template<typename T>
	T max(const T &x, const T &y) { return x>y ? x : y; }
}
#define EDGEMIN(x,y) Edge::min((x),(y))
#define EDGEMAX(x,y) Edge::max((x),(y))

//-------------------

// These functions convert full-precision F32 values to various GPU-native formats
int16_t _ConvertF32toI16(float val);
int16_t _ConvertF32toI16N(float val);
uint8_t _ConvertF32toU8N(float val);
uint8_t _ConvertF32toU8(float val);
uint16_t _ConvertF32toF16(float val);
uint32_t _ConvertF32toX11Y11Z10N(float x, float y, float z);
//-------------------

// These functions write their inputs to the specified address in the specified byte order.
// Gentle reminder, because I'm always mixing this up:
// - x86 and friends are little-endian, and store the least-significant byte in the smallest address
// - PPC/PPU/SPU are big-endian, and store the most-significant byte in the smallest address.
//
// It is safe to use these functions to perform in-place conversions, such as
// "_edgeGeomWriteInt(x, &x, true);"
void _edgeGeomWriteShort(uint16_t input, void *output, bool writeBigEndian);
void _edgeGeomWriteInt(uint32_t input, void *output, bool writeBigEndian);
void _edgeGeomWriteFloat(float input, void *output, bool writeBigEndian); // Note that a byte-swapped float is not necessarily a valid floating-point value!
// Some convenient definitions to make code using the above functions clearer
#define _EDGE_GEOM_WRITE_BIG_ENDIAN true
#define _EDGE_GEOM_WRITE_LITTLE_ENDIAN false

//-------------------

int32_t _edgeGeomIsEqualInt     (const void *a, const void *b);
int32_t _edgeGeomIsEqualUnsigned(const void *a, const void *b);
int32_t _edgeGeomIsEqualShort   (const void *a, const void *b);

uint32_t _edgeGeomSortUniqueArrayInt     (int32_t  *array, uint32_t totalElements);
uint32_t _edgeGeomSortUniqueArrayUnsigned(uint32_t *array, uint32_t totalElements);

//-------------------

uint32_t _edgeGeomConvertToFixedPoint(const EdgeGeomSpuVertexAttributeDefinition &attrDef, const float *sourceData, const float *offsets, uint8_t *outputBuffer);

void _edgeGeomConvertAndWriteDataToSpuStream(const EdgeGeomSpuVertexAttributeDefinition &attrDef, const float *srcAttr, uint8_t *destStream, float const **currentMins, bool writeBigEndian);
void _edgeGeomConvertAndWriteDataToRsxStream(const EdgeGeomRsxVertexAttributeDefinition &attrDef, const float *srcAttr, uint8_t *destStream, bool writeBigEndian);

//-------------------

void _edgeGeomFindAttributeMins(const float *sourceVertexes, uint32_t numFloatsPerSourceVertex, const EdgeGeomSpuVertexFormat& vertexFormat, 
					   const uint16_t *sourceAttributeIndexes, const EdgeGeomAttributeId *sourceAttributeIds, uint8_t numSourceAttributes, 
					   const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes,
					   float **outMins, uint32_t **outFixedPointOffsets, uint32_t *outFixedPointOffsetsSize);

//------------------------------------------------------ from edgegeom_compressindexes.cpp

struct EdgeGeomIndexesSizes
{
	uint32_t m_inputSize;
	uint32_t m_maxTempSize;
};

//-------------------
/// This function rotates triangles in-place so that they are in ideal ordering for index compression.
/// It is exposed primarily so that the results of the KCacheOptimizer can be tested for LRU/FIFO misses
/// as if it were going through the index compression routine, without having to unpack the binary table.
void _edgeGeomRotateTrianglesForAdjacency(uint32_t *triangles, uint32_t numTriangles);

//-------------------
// Remaps a triangle list in-place so that vertexes are sorted in order of appearance within the triangle
// list.  This significantly improves the degree to which the triangle list can be compressed.
//
// largestUniqueIndex is the value of the largest index in the array.
// If the outReverseVertexMapping array is non-zero, it must point to an array with numUniqueIndexes entries.
// This array will be filled with the original indexes of each index in the remapped list.
void _edgeGeomRemapTriangleList(uint32_t *triangles, uint32_t numTriangles, uint32_t largestUniqueIndex,
					   uint32_t *outReverseMapping);

//-------------------
/// This function is useful when partitioning needs an accurate measurement of the compressed 
/// index table size.  For best results, you should call the triangle reordering code for 
/// maximum post-transform cache coherency.
void _edgeGeomMeasureCompressedIndexTable(const uint32_t *triangles, uint32_t numTriangles, EdgeGeomIndexesFlavor indexFlavor,
								 EdgeGeomIndexesSizes *measuredSizes);

//-------------------
/// This takes in an index list and an index flavor, and creates a compressed index buffer.
/// The caller is responsible for deleting the buffer.  The triangle list should be renumbered
/// into final order before calling (ie. the first tri should be 0,1,2, typically).
void _edgeGeomCompressIndexes(const uint32_t *triangles, uint32_t numTriangles, uint8_t **indexBuffer, EdgeGeomIndexesSizes *indexesSizes);

//-----------------------------------------------------

#endif // !defined(LIBEDGEGEOMTOOL_INTERNAL_H)
