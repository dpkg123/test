/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */


#include "edge/libedgegeomtool/libedgegeomtool_internal.h"

#include <stdlib.h>
#include <float.h>

#if defined(WIN32) // enable verbose memory leak reporting
	#include <crtdbg.h>
	#if defined(_DEBUG)
		#define _CRTDBG_MAP_ALLOC
	#endif
#endif

//-------------------

static void *DefaultAlloc(size_t allocSize, const char *filename, const uint32_t lineNumber)
{
	(void)filename;
	(void)lineNumber;
#if defined(WIN32)
	return _malloc_dbg(allocSize, _NORMAL_BLOCK, filename, lineNumber);
#else
	return malloc(allocSize);
#endif
}
static void DefaultFree(void *ptr, const char *filename, const uint32_t lineNumber)
{
	(void)filename;
	(void)lineNumber;
#if defined(WIN32)
	_free_dbg(ptr, _NORMAL_BLOCK);
#else
	free(ptr);
#endif
}

static EdgeGeomAllocFunc g_edgeGeomAlloc = DefaultAlloc;
static EdgeGeomFreeFunc g_edgeGeomFree = DefaultFree;

void *edgeGeomAllocEx(size_t allocSize, const char *fileName, const uint32_t lineNumber)
{
	return g_edgeGeomAlloc(allocSize, fileName, lineNumber);
}
void edgeGeomFreeEx(void *ptr, const char *fileName, const uint32_t lineNumber)
{
	return g_edgeGeomFree(ptr, fileName, lineNumber);
}

//-------------------

void EdgeGeomSetAllocFunc(EdgeGeomAllocFunc func)
{
	g_edgeGeomAlloc = (func != NULL) ? func : DefaultAlloc;
}
void EdgeGeomSetFreeFunc(EdgeGeomFreeFunc func)
{
	g_edgeGeomFree = (func != NULL) ? func : DefaultFree;
}

//-------------------

// Endian / byte-swapping functions.  
void _edgeGeomWriteShort(uint16_t input, void *output, bool writeBigEndian)
{
	union 
	{
		uint16_t u16;
		uint8_t  u8[4];
	};
	if (writeBigEndian)
	{
		u8[0] = (uint8_t) ((input >>  8) & 0xFF);
		u8[1] = (uint8_t) ((input >>  0) & 0xFF);
	}
	else
	{
		u8[0] = (uint8_t) ((input >>  0) & 0xFF);
		u8[1] = (uint8_t) ((input >>  8) & 0xFF);
	}
	*(uint16_t*)output = u16;
}
void _edgeGeomWriteInt(uint32_t input, void *output, bool writeBigEndian)
{
	union 
	{
		uint32_t u32;
		uint8_t  u8[4];
	};
	if (writeBigEndian)
	{
		u8[0] = (uint8_t) ((input >> 24) & 0xFF);
		u8[1] = (uint8_t) ((input >> 16) & 0xFF);
		u8[2] = (uint8_t) ((input >>  8) & 0xFF);
		u8[3] = (uint8_t) ((input >>  0) & 0xFF);
	}
	else
	{
		u8[0] = (uint8_t) ((input >>  0) & 0xFF);
		u8[1] = (uint8_t) ((input >>  8) & 0xFF);
		u8[2] = (uint8_t) ((input >> 16) & 0xFF);
		u8[3] = (uint8_t) ((input >> 24) & 0xFF);
	}
	*(uint32_t*)output = u32;
}
void _edgeGeomWriteFloat(float input, void *output, bool writeBigEndian)
{
	union
	{
		float f32;
		uint32_t u32;
	};
	f32 = input;
	return _edgeGeomWriteInt(u32, output, writeBigEndian);
}

//-------------------

int32_t _edgeGeomIsEqualInt(const void *a, const void *b)
{
	return (int32_t)(*((const int32_t *)a) - *((const int32_t *)b));  // standard lexical ordering callback
}

//-------------------

int32_t _edgeGeomIsEqualShort(const void *a, const void *b)
{
	return (int32_t)(*((const int16_t *)a) - *((const int16_t *)b));  // standard lexical ordering callback
}
//-------------------

int32_t _edgeGeomIsEqualUnsigned(const void *a, const void *b)
{
	return (int32_t)(*((uint32_t const *)a) - *((uint32_t const *)b));  // standard lexical ordering callback
}

//-------------------
/// This function reduces the array to only unique elements, and returns the 
/// count of unique elements in the array, in linear time.
uint32_t _edgeGeomSortUniqueArrayUnsigned(uint32_t *array, uint32_t totalElements)
{
	// sort all elements into order first, so we can only search for value transitions.
	qsort(array, totalElements, sizeof(uint32_t), _edgeGeomIsEqualUnsigned);

	// unique the array and reduce the total to the unique set only
	uint32_t count = 0;
	if (totalElements)
	{
		count = 1;
		for (uint32_t iAttr=0; iAttr<totalElements; iAttr++)
		{
			if (array[count-1]!=array[iAttr])  // found a new index, let's keep it
			{
				array[count] = array[iAttr];
				count++;
			}
		}
	}

	return count;
}

//-------------------
/// This function reduces the array to only unique elements, and returns the 
/// count of unique elements in the array, in linear time.
uint32_t _edgeGeomSortUniqueArrayInt(int32_t *array, uint32_t totalElements)
{
	// sort all elements into order first, so we can only search for value transitions.
	qsort(array, totalElements, sizeof(int32_t), _edgeGeomIsEqualInt);

	// unique the array and reduce the total to the unique set only
	uint32_t count = 0;
	if (totalElements)
	{
		count = 1;
		for (uint32_t iAttr=0; iAttr<totalElements; iAttr++)
		{
			if (array[count-1]!=array[iAttr])  // found a new index, let's keep it
			{
				array[count] = array[iAttr];
				count++;
			}
		}
	}

	return count;
}

//-------------------

// This converts a float to a fixed point value, where it has been adjusted into a positive range by minValue.
static uint32_t ConvertComponentToFixedPoint(float minValue, float value, uint32_t fractionalBits)
{
	/*
	float const range = (value - minValue) * (1 << fractionalBits);  // "shift it left" by the number of fractional bits
	uint32_t const fixedPointValue = (uint32_t)(range+0.5f);  // round the fixed point values to the nearest bit
	return fixedPointValue;
	*/

	//rolling back

    // convert both values to fixed point ("shift left" by number of fractional bits and round to nearest bit)
	uint32_t const fixedPointMinValue = (uint32_t)((minValue * (1 << fractionalBits)) + 0.5f);
	uint32_t const fixedPointValue = (uint32_t)((value * (1 << fractionalBits)) + 0.5f);

	// note: subtraction is done here rather than in float space, so as to match the runtime
	return fixedPointValue - fixedPointMinValue;
}

//-------------------
// Given a bit count, an offset per component, and a scale, convert a single attribute from floats to a set of output bytes.
uint32_t _edgeGeomConvertToFixedPoint(const EdgeGeomSpuVertexAttributeDefinition &attrDef, const float *sourceData, const float *offsets, uint8_t *outputBuffer)
{
	// convert to fixed point representation
	uint32_t bytesAdded = 0;
	uint32_t currentBytes = 0;
	uint32_t currentBytesBitCount = 0;
	for (uint32_t iComponent=0; iComponent<attrDef.m_count; iComponent++)
	{
		uint32_t const numIntBits = attrDef.m_fixedPointBitDepthInteger[iComponent];
		uint32_t const numFracBits = attrDef.m_fixedPointBitDepthFractional[iComponent];
		uint32_t const numBits = numIntBits + numFracBits;
		EDGEASSERT(numBits==0 || (numBits>=numFracBits && numBits<=32));
		if (numBits>=numFracBits)
		{
			uint32_t const convertedData = ConvertComponentToFixedPoint(offsets[iComponent], sourceData[iComponent], numFracBits);
			currentBytes |= convertedData << (32 - numBits - currentBytesBitCount);  // slide up the new data to mesh with the previous data
			currentBytesBitCount += numBits;
			while (currentBytesBitCount>=8)
			{
				*outputBuffer++ = (uint8_t)((currentBytes >> 24) & 0xff);
				currentBytes <<= 8;  // shift up so the next byte is in the high order
				currentBytesBitCount -= 8;
				bytesAdded++;
			}
		}
	}
	EDGEASSERT(currentBytesBitCount==0);  // if this is NOT true, then the number of bits written is not modulo 8, which is illegal
	return bytesAdded;
}

//-------------------
// This computes the min for all the attributes that use fixed point format
void _edgeGeomFindAttributeMins(const float *sourceVertexes, uint32_t numFloatsPerSourceVertex, const EdgeGeomSpuVertexFormat& vertexFormat, 
					   const uint16_t *sourceAttributeIndexes, const EdgeGeomAttributeId *sourceAttributeIds, uint8_t numSourceAttributes,
					   const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes,
					   float **outMins, uint32_t **outFixedPointOffsets, uint32_t *outFixedPointOffsetsSize)
{
	uint32_t numComponents = 0;
	for (uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; iAttr++)
	{
		if (vertexFormat.m_attributeDefinition[iAttr].m_type == kSpuAttr_FixedPoint)
			numComponents += 4;  // always 4 per attribute
	}

	if (numComponents == 0)
	{
		*outMins = NULL;
		*outFixedPointOffsets = NULL;
		*outFixedPointOffsetsSize = 0;
		return;
	}

	// clear the mins
	*outMins = (float*)edgeGeomAlloc(numComponents*sizeof(float));;  // allocate a min for each component
	for (uint32_t iComponent=0; iComponent<numComponents; iComponent++)
	{
		(*outMins)[iComponent] = FLT_MAX;  // start off with widest possible ranges
	}

	// check all the unique vertexes
	for (uint32_t iVertex=0; iVertex<numUniqueVertexes; iVertex++)
	{
		const uint32_t originalIndex = originalVertexIndexes[iVertex];
		float *currentMins = *outMins;

		// simplify the innards of the loop by caching a pointer to the start of the base vertex attributes
		const float *vertexAttr = &sourceVertexes[numFloatsPerSourceVertex*originalIndex];

		for (uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; iAttr++)
		{
			if (vertexFormat.m_attributeDefinition[iAttr].m_type != kSpuAttr_FixedPoint)
				continue;

			// Find the corresponding attribute in the source stream
			bool foundSourceAttribute = false;
			uint16_t srcAttrIndex = 0;
			for(uint32_t iSourceAttr=0; iSourceAttr<numSourceAttributes; ++iSourceAttr)
			{
				if (sourceAttributeIds[iSourceAttr] != vertexFormat.m_attributeDefinition[iAttr].m_attributeId)
					continue;
				srcAttrIndex = sourceAttributeIndexes[iSourceAttr];
				foundSourceAttribute = true;
				break;
			}
			EDGEASSERT(foundSourceAttribute);

			for (uint32_t iComponent=0; iComponent<vertexFormat.m_attributeDefinition[iAttr].m_count; iComponent++)
			{
				currentMins[iComponent] = EDGEMIN(currentMins[iComponent], vertexAttr[srcAttrIndex+iComponent]);
			}
			currentMins += 4;  // always 4 per attribute
		}
	}

	// fixed point offsets are always stored as described in the flavor
	*outFixedPointOffsetsSize = numComponents*sizeof(uint32_t);
	*outFixedPointOffsets = (uint32_t*)edgeGeomAlloc(numComponents*sizeof(uint32_t));
	uint32_t *currentOffset = *outFixedPointOffsets;
	float    *currentMin    = *outMins;

	// generate the fixed point representation for each component's min value
	for (uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; iAttr++)
	{
		if (vertexFormat.m_attributeDefinition[iAttr].m_type != kSpuAttr_FixedPoint)
			continue;
		for (uint32_t iComponent=0; iComponent<4; iComponent++)  // always 4 per attribute
		{
			uint32_t const offsetFracBits = vertexFormat.m_attributeDefinition[iAttr].m_fixedPointBitDepthFractional[iComponent];
			uint32_t const fpOffset = ConvertComponentToFixedPoint(0, *currentMin, offsetFracBits);
			_edgeGeomWriteInt(fpOffset, currentOffset, _EDGE_GEOM_WRITE_BIG_ENDIAN);
			currentOffset++;
			currentMin++;
		}
	}
}
