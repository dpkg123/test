/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <string.h>
#include <math.h>

#include "edge/libedgegeomtool/libedgegeomtool_internal.h"

/// This function determines if a specific blend shape has any data that overlaps this particular section of a geometry unit.
bool edgeGeomBlendShapeAffectsSegment(const float *shapeDeltas, uint32_t numFloatsPerDelta, const EdgeGeomSpuVertexFormat& deltaFormat,
									  const uint16_t *blendedAttributeIndexes, const EdgeGeomAttributeId *blendedAttributeIds, uint8_t numBlendedAttributes,
									  const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes)
{
	// The correct way to do this is to check the post-conversion values of the blend shape data attributes
	// and if any are not zero, return true.  Rather, I'm just going the easy way out and checking the input floats
	// for zero.  Not strictly correct, but a much easier piece of code to write in the short term.
	for (uint32_t iVertex=0; iVertex<numUniqueVertexes; iVertex++)
	{
		const float *delta = shapeDeltas + originalVertexIndexes[iVertex]*numFloatsPerDelta;
		for (uint32_t iAttr=0; iAttr<deltaFormat.m_numAttributes; iAttr++)
		{
			// Find the corresponding attribute in the source stream
			bool foundSourceAttribute = false;
			uint16_t srcAttrIndex = 0;
			for(uint32_t iSourceAttr=0; iSourceAttr<numBlendedAttributes; ++iSourceAttr)
			{
				if (blendedAttributeIds[iSourceAttr] != deltaFormat.m_attributeDefinition[iAttr].m_attributeId)
					continue;
				srcAttrIndex = blendedAttributeIndexes[iSourceAttr];
				foundSourceAttribute = true;
				break;
			}
			EDGEASSERT(foundSourceAttribute);

			const float *deltaAttr = &delta[srcAttrIndex];
			for (uint32_t iComponent=0; iComponent<deltaFormat.m_attributeDefinition[iAttr].m_count; iComponent++)
			{
				// check for non-zero data
				const float bsValue = deltaAttr[iComponent];
				if (bsValue != 0.0f)
				{
					return true;
				}
			}
		}
	}

	return false;
}

static bool IsZeroDelta(const uint8_t *delta, const EdgeGeomSpuVertexFormat& deltaFormat, const float *fixedPointMins)
{
	// For some attribute types, an input of 0 results in an output of 0.  This is not true
	// for the three normalized types (X11Y11Z10N, I16N and U8N), or for fixed-point types.  Rather
	// than hard-code the representation of zero in these formats, we calculate it the first time
	// this function is called.
	static uint32_t zeroAsX11Y11Z10N = _ConvertF32toX11Y11Z10N(0,0,0);
	static uint16_t zeroAsI16N = _ConvertF32toI16N(0);
	static uint8_t  zeroAsU8N = _ConvertF32toU8N(0);
	// Since the input delta is stored as big-endian data, the multi-byte zero values must be converted to
	// big-endian as well.
	static bool firstTime = true;
	if (firstTime)
	{
		firstTime = false;
		_edgeGeomWriteInt(zeroAsX11Y11Z10N, &zeroAsX11Y11Z10N, _EDGE_GEOM_WRITE_BIG_ENDIAN);
		_edgeGeomWriteShort(zeroAsI16N, &zeroAsI16N, _EDGE_GEOM_WRITE_BIG_ENDIAN);
	}

	const float *currentMins = fixedPointMins;
	for(uint32_t iAttr=0; iAttr<deltaFormat.m_numAttributes; ++iAttr)
	{
		const uint8_t *rawAttr = delta + deltaFormat.m_attributeDefinition[iAttr].m_byteOffset;
		uint32_t numComponents = deltaFormat.m_attributeDefinition[iAttr].m_count;
		switch(deltaFormat.m_attributeDefinition[iAttr].m_type)
		{
		case kSpuAttr_F32:
			{
				const float *attr = (const float*)rawAttr;
				for(uint32_t iComponent=0; iComponent<numComponents; ++iComponent)
				{
					if (attr[iComponent] != 0)
						return false;
				}
			}
			break;
		case kSpuAttr_X11Y11Z10N:
			{
				const uint32_t *attr = (const uint32_t*)rawAttr;
				if (*attr != zeroAsX11Y11Z10N) // this is the bit pattern of [0,0,0] encoded in X11Y11Z10N.
					return false;
			}
			break;
		case kSpuAttr_I16N:
			{
				const uint16_t *attr = (const uint16_t*)rawAttr;
				for(uint32_t iComponent=0; iComponent<numComponents; ++iComponent)
				{
					if (attr[iComponent] != zeroAsI16N)
						return false;
				}
			}
			break;
		case kSpuAttr_F16:
		case kSpuAttr_I16:
			{
				const uint16_t *attr = (const uint16_t*)rawAttr;
				for(uint32_t iComponent=0; iComponent<numComponents; ++iComponent)
				{
					if (attr[iComponent] != 0)
						return false;
				}
			}
			break;
		case kSpuAttr_U8N:
			{
				const uint8_t *attr = (const uint8_t*)rawAttr;
				for(uint32_t iComponent=0; iComponent<numComponents; ++iComponent)
				{
					if (attr[iComponent] != zeroAsU8N)
						return false;
				}
			}
			break;
		case kSpuAttr_U8:
			{
				const uint8_t *attr = (const uint8_t*)rawAttr;
				for(uint32_t iComponent=0; iComponent<numComponents; ++iComponent)
				{
					if (attr[iComponent] != 0)
						return false;
				}
			}
			break;
		case kSpuAttr_FixedPoint:
			{
				const EdgeGeomSpuVertexAttributeDefinition& attrDef = deltaFormat.m_attributeDefinition[iAttr];
				// Get a representation of zero in the appropriate fixed point format, using this attribute's bit depths and offsets
				const float zeroIn[4] = {0,0,0};
				uint8_t zeroOut[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
				_edgeGeomConvertToFixedPoint(attrDef, zeroIn, currentMins, zeroOut);
				currentMins += 4; // 4 elements per fixed-point attribute
				// Determine the total size (in bytes) of the entire attribute.  Fortunately, fixed-point attributes are required
				// to be a multiple of 8 bits.
				uint32_t attrSize = 0;
				for(uint32_t iComponent=0; iComponent<numComponents; ++iComponent)
				{
					attrSize += attrDef.m_fixedPointBitDepthInteger[iComponent] + attrDef.m_fixedPointBitDepthFractional[iComponent];
				}
				attrSize /= 8;

				const uint8_t *attr = (const uint8_t*)rawAttr;
				if (memcmp(attr, zeroOut, attrSize) != 0)
					return false;
			}
			break;
		case kSpuAttr_UnitVector:
			return false; // unit vectors cannot represent the zero vector. For that reason, they generally shouldn't be used in blend shape deltas...
		}
	}

	// If no attributes compressed to non-zero, then this delta is zero
	return true;
}

//-------------------
// This generates data for a single blend shape in the final format.
void edgeGeomMakeBlendShapeBuffer(const float *shapeDeltas, uint32_t numFloatsPerDelta, const EdgeGeomSpuVertexFormat& deltaFormat,
									 const uint16_t *blendedAttributeIndexes, const EdgeGeomAttributeId *blendedAttributeIds, uint8_t numBlendedAttributes,
									 const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes,
									 uint8_t **outShapeBuffer, uint16_t *outShapeBufferSize){
	// Generate the blend shape stream description. All blend shapes use stream descriptions for
	// decompression.
	uint16_t bsStreamDescriptionSize = 0;
	uint8_t *bsStreamDescription = NULL;
	edgeGeomMakeSpuStreamDescription(deltaFormat, &bsStreamDescription, &bsStreamDescriptionSize);

	// allocate space for the output blend shape vertex buffer.
	// The first quadwords in the buffer are a header, containing the sizes of the various tables.
	// After the sizes, the stream description (if present) is stored.  The size of the structure
	// is padded out to be a multiple of 16 bytes.
	// After the stream description, the fixed point offset table is stored.  This contains
	// one quadword (4 32-bit offsets) per fixed-point attribute in the stream.
	// After the fixed point offsets, the vertex deltas are stored. Only non-zero deltas are present
	// in the stream; the run table (see below) indicates which vertexes these deltas correspond to.
	// After the deltas, the run table is stored.  This table indicates which vertexes in this segment
	// have non-zero delta data.  The table is run-length encoded; each entry is 16 bits.  The high
	// 11 bits indicate the first element of the run, and the low 5 bits give the number of elements
	// in the run, minus one (since empty runs are not allowed, this gives a range of 1-32 elements
	// per run).
	uint32_t bufferSize = 32 + // 2 quads worth of header
		bsStreamDescriptionSize +
		(uint32_t)(sizeof(uint32_t)*4 * deltaFormat.m_numAttributes) + // when allocating space, we assume every attribute is fixed-point
		(sizeof(uint16_t) * ( (numUniqueVertexes+1)/2 )) + // assume worst-case run table size (alternating 1-element runs)
		(deltaFormat.m_vertexStride * numUniqueVertexes) + // assume worst-case delta table size (one delta per vertex)
		16 + 16; // padding so that the last two table sizes can be rounded up
	bufferSize = (bufferSize + 0xF) & ~0xF; // round up to 16 bytes
	*outShapeBuffer = (uint8_t*)edgeGeomAlloc(bufferSize);
	memset(*outShapeBuffer, 0, bufferSize); // for determinism

	// this is our 'write head' in the buffer
	uint8_t *bufferWritePtr = (uint8_t *)*outShapeBuffer;

	uint32_t *tableSizes = (uint32_t*)bufferWritePtr; // Reserve space for eight header intsvalues.  We'll fill these in later
	bufferWritePtr += 8*sizeof(uint32_t);

	// figure out if there are any minimum values we need to remember for fixed point attributes
	float *fixedPointMins = NULL;
	uint32_t *fixedPointOffsets = NULL;
	uint32_t fixedOffsetsSize = 0;
	_edgeGeomFindAttributeMins(shapeDeltas, numFloatsPerDelta, deltaFormat,
		blendedAttributeIndexes, blendedAttributeIds, numBlendedAttributes,
		originalVertexIndexes, numUniqueVertexes,
		&fixedPointMins, &fixedPointOffsets, &fixedOffsetsSize);

	// Copy the stream description to the blend shape vertex buffer
	memcpy(bufferWritePtr, bsStreamDescription, bsStreamDescriptionSize);
	edgeGeomFree(bsStreamDescription);
	bufferWritePtr += bsStreamDescriptionSize;
	tableSizes[0] = bsStreamDescriptionSize;

	// copy the fixed point offsets to the blend shape vertex buffer
	memcpy(bufferWritePtr, fixedPointOffsets, fixedOffsetsSize);
	edgeGeomFree(fixedPointOffsets);
	bufferWritePtr += fixedOffsetsSize;
	tableSizes[1] = fixedOffsetsSize;
	
	// Find all the source attribute indexes
	uint16_t *srcAttrIndexes = (uint16_t*)edgeGeomAlloc(deltaFormat.m_numAttributes * sizeof(uint16_t));
	for (uint32_t iAttr=0; iAttr<deltaFormat.m_numAttributes; ++iAttr)
	{
		// Find the corresponding attribute in the source stream
		bool foundSourceAttribute = false;
		for(uint32_t iSourceAttr=0; iSourceAttr<numBlendedAttributes; ++iSourceAttr)
		{
			if (blendedAttributeIds[iSourceAttr] != deltaFormat.m_attributeDefinition[iAttr].m_attributeId)
				continue;
			srcAttrIndexes[iAttr] = blendedAttributeIndexes[iSourceAttr];
			foundSourceAttribute = true;
			break;
		}
		EDGEASSERT(foundSourceAttribute);

		// walking off the end of the per-vertex blend shape data is bad
		EDGEASSERT(srcAttrIndexes[iAttr] + deltaFormat.m_attributeDefinition[iAttr].m_count <= numFloatsPerDelta);
	}

	// Build the compressed delta table and (optional) non-zero delta index run table.
	// We only use the sparse format if it would be advantageous to do so.
	bool useSparseFormat = true; // assume a sparse format at first
	uint32_t countGenerated = 0;
	uint8_t *deltaTableStart = bufferWritePtr;
	uint16_t *nonZeroDeltaIndexes = (uint16_t*)edgeGeomAlloc(numUniqueVertexes * sizeof(uint16_t));
	do
	{
		countGenerated = 0;
		bufferWritePtr = deltaTableStart;

		// Generate the output deltas for each vertex. Along the way, we determine the number of vertices
		// with non-zero delta data.
		for (uint32_t iVertex=0; iVertex<numUniqueVertexes; ++iVertex)
		{
			const uint32_t srcVertexIndex = originalVertexIndexes[iVertex];
			const float *srcDelta = shapeDeltas + srcVertexIndex*numFloatsPerDelta;
			uint8_t *destDelta = bufferWritePtr + countGenerated*deltaFormat.m_vertexStride;
			const float *currentMins = fixedPointMins;  // remembers which offsets have been used by fixed point attributes while writing them out

			// Generate the final output delta for this vertex.
			for(uint32_t iAttr=0; iAttr<deltaFormat.m_numAttributes; ++iAttr)
			{
				const float *srcDeltaAttr = &srcDelta[ srcAttrIndexes[iAttr] ];
				uint8_t *destDeltaAttr = destDelta + deltaFormat.m_attributeDefinition[iAttr].m_byteOffset;
				_edgeGeomConvertAndWriteDataToSpuStream(deltaFormat.m_attributeDefinition[iAttr], srcDeltaAttr,
					destDeltaAttr, &currentMins, _EDGE_GEOM_WRITE_BIG_ENDIAN);
			}

			// Test each compressed attribute to see if it will decompress to zero. If all attribute values are zero,
			// we don't need to store this delta.
			if (useSparseFormat && IsZeroDelta(destDelta, deltaFormat, fixedPointMins))
			{
				continue;
			}

			// Add this delta's index to the list of non-zero deltas
			nonZeroDeltaIndexes[countGenerated++] = (uint16_t)iVertex;
		}
		// Round the buffer write pointer to the next 16-byte boundary, relative to the start of the array (which is
		// NOT guaranteed to be 16-byte-aligned!)
		tableSizes[2] = (deltaFormat.m_vertexStride * countGenerated + 0xF) & ~0xF;
		bufferWritePtr = deltaTableStart + tableSizes[2];

		// Generate the compressed run table, if necessary
		if (useSparseFormat)
		{
			uint32_t iDelta = 0;
			uint32_t runCount = 0;
			uint8_t *runTableStart = bufferWritePtr;
			while(iDelta < countGenerated)
			{
				// start the run
				uint16_t start = nonZeroDeltaIndexes[iDelta++];
				uint16_t countMinusOne = 0;
				// continue up to 32 elements
				for(countMinusOne=0; countMinusOne<31; ++countMinusOne)
				{
					// Check for the end of the run -- either we've run off the end of the buffer,
					// or the current element is non-consecutive with the previous one
					if (iDelta >= countGenerated || nonZeroDeltaIndexes[iDelta] != nonZeroDeltaIndexes[iDelta-1]+1)
						break;
					++iDelta;
				}
				EDGEASSERT(countMinusOne <= 31);
				// Pack the start and count into 16 bits and append it to the final run table (in big-endian format)
				uint16_t run = (start << 5) | countMinusOne;
				_edgeGeomWriteShort(run, bufferWritePtr, _EDGE_GEOM_WRITE_BIG_ENDIAN);
				runCount++;
				bufferWritePtr += 2;
			}
			tableSizes[3] = runCount * sizeof(uint16_t); // this should *not* be padded!
			tableSizes[4] = countGenerated;
			uint64_t paddedRunTableSize = (bufferWritePtr - runTableStart + 0xF) & ~0xF;
			bufferWritePtr = runTableStart + paddedRunTableSize;
		}
		else
		{
			tableSizes[3] = 0; // no run table
			tableSizes[4] = countGenerated;
		}

		// If we've been using a sparse format, check now to see whether it actually saved us any space.
		// If the sparse table + compressed run table is larger than the non-sparse table would be, then
		// we go back and create the delta table again without using the sparse format.
		if (useSparseFormat)
		{
			// We know how large the data would be if we didn't use the sparse format
			uint32_t uncompressedTableSize = (deltaFormat.m_vertexStride * numUniqueVertexes + 0xF) & ~0xF;

			if (tableSizes[3] + tableSizes[2] > uncompressedTableSize)
			{
				// Try again without the sparse format
				useSparseFormat = false;
				continue;
			}
		}

		// If we make it here, we're done.
		break;
	}
	while(nonZeroDeltaIndexes != NULL); // just need a constant "false" for the conditional, and "true" itself causes a warning

	edgeGeomFree(srcAttrIndexes);
	edgeGeomFree(nonZeroDeltaIndexes);
	edgeGeomFree(fixedPointMins);

	// Convert the table sizes to big-endian
	for(uint32_t iSize=0; iSize<8; ++iSize)
	{
		_edgeGeomWriteInt(tableSizes[iSize], &(tableSizes[iSize]), _EDGE_GEOM_WRITE_BIG_ENDIAN);
	}

	*outShapeBufferSize = (uint16_t)(bufferWritePtr - *outShapeBuffer);
	*outShapeBufferSize = (*outShapeBufferSize + 0xF) & ~0xF; // round up to 16 bytes
	EDGEASSERT(*outShapeBufferSize <= bufferSize); // If this fires, we've somehow overrun the shape buffer!
}
