/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "edge/libedgegeomtool/libedgegeomtool_internal.h"

#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

int16_t _ConvertF32toI16(float val)
{
	if (val<-32768.0f || val>32767.0f)
	{
		printf("Value (%5.3f) should be in range [-32768..32767].  Conversion will clamp to I16.\n", val);
	}

	const int16_t i16 = (int16_t)(EDGEMAX(-32768.0f,EDGEMIN(32767.0f,val)));
	return i16;
}

//-------------------

int16_t _ConvertF32toI16N(float val)
{
	if (val<-1.0f || val>1.0f)
	{
		printf("Value (%5.3f) should be in range [-1..1].  Conversion will clamp to I16N [-32768..32767].\n", val);
	}

	// all of this rigamarole is to retain as much precision as possible near 1.0f and -1.0f, without wrapping
	const int16_t i16n = (int16_t)((((EDGEMAX(-1.0f,EDGEMIN(1.0f,val))+1.0f)/2.0f)*65535.0f)-32768.0f);
	return i16n;
}

//-------------------

uint8_t _ConvertF32toU8N(float val)
{
	if (val<0.0f || val>1.0f)
	{
		printf("Value (%5.3f) should be in range [0..1].  Conversion will clamp to U8N [0..255].\n", val);
	}

	const uint8_t u8n = (uint8_t)(EDGEMAX(0.0f,EDGEMIN(1.0f,val))*255.0f);
	return u8n;
}

//-------------------

uint8_t _ConvertF32toU8(float val)
{
	if (val<0.0f || val>255.0f)
	{
		printf("Value (%5.3f) should be in range [0..255].  Conversion will clamp to U8.\n", val);
	}

	// add a half to prevent always rounding down (must happen after the range-check above)
	val += 0.5f;

	const uint8_t u8 = (uint8_t)(EDGEMAX(0.0f,EDGEMIN(255.0f,val)));
	return u8;
}

//-------------------

uint16_t _ConvertF32toF16(float val)
{
	union
	{
		uint32_t asInt;
		float asFloat;
	} valUnion;
	valUnion.asFloat = val;
	const uint32_t f = valUnion.asInt;
	const uint32_t signbit  = (f & 0x80000000) >> 16;
	const int32_t  exponent = ((f & 0x7F800000) >> 23) - (127 - 15);
	const uint32_t mantissa = (f & 0x007FFFFF);

	if (exponent <= 0)
		return 0;
	if (exponent > 30)
		return (uint16_t)(signbit | 0x7BFF);

	return (uint16_t)(signbit | (exponent << 10) | (mantissa >> 13));
}

//-------------------

uint32_t _ConvertF32toX11Y11Z10N(float x, float y, float z)
{
	//                      11111111111 X 0x000007FF
	//           1111111111100000000000 Y 0x003FF800
	// 11111111110000000000000000000000 Z 0xFFC00000
	// ZZZZZZZZZZYYYYYYYYYYYXXXXXXXXXXX
	// #defines for X11Y11Z10N format
	#define X11Y11Z10N_X_MASK 0x000007FF
	#define X11Y11Z10N_X_BITS 11
	#define X11Y11Z10N_X_SHIFT 0

	#define X11Y11Z10N_Y_MASK 0x003FF800
	#define X11Y11Z10N_Y_BITS 11
	#define X11Y11Z10N_Y_SHIFT 11

	#define X11Y11Z10N_Z_MASK 0xFFC00000
	#define X11Y11Z10N_Z_BITS 10
	#define X11Y11Z10N_Z_SHIFT 22

	if (x<-1.0f || x>1.0f)
	{
		printf("Value (%5.3f) should be in range [-1..1].  Conversion will clamp to X11Y11Z10N.\n", x);
	}
	if (y<-1.0f || y>1.0f)
	{
		printf("Value (%5.3f) should be in range [-1..1].  Conversion will clamp to X11Y11Z10N.\n", y);
	}
	if (z<-1.0f || z>1.0f)
	{
		printf("Value (%5.3f) should be in range [-1..1].  Conversion will clamp to X11Y11Z10N.\n", z);
	}

	const uint32_t uX = ((int32_t(EDGEMAX(EDGEMIN(((x)*2047.f - 1.f)*0.5f, 1023.f), -1024.f)) & (X11Y11Z10N_X_MASK >> X11Y11Z10N_X_SHIFT)) << X11Y11Z10N_X_SHIFT);
	const uint32_t uY = ((int32_t(EDGEMAX(EDGEMIN(((y)*2047.f - 1.f)*0.5f, 1023.f), -1024.f)) & (X11Y11Z10N_Y_MASK >> X11Y11Z10N_Y_SHIFT)) << X11Y11Z10N_Y_SHIFT);
	const uint32_t uZ = ((int32_t(EDGEMAX(EDGEMIN(((z)*1023.f - 1.f)*0.5f,  511.f), -512.f )) & (X11Y11Z10N_Z_MASK >> X11Y11Z10N_Z_SHIFT)) << X11Y11Z10N_Z_SHIFT);
	const uint32_t xyz = uX | uY | uZ;
	return xyz;
}

uint32_t edgeGeomGetScratchBufferSizeInQwords(uint32_t numInputAttributes, uint32_t numUniqueVertexes)
{
	// Basically, the scratch buffer will contain one uniform table for each input vertex attribute, plus
	// an extra table for temporary per-vertex storage.  Each uniform table is 16 bytes per vertex, where the vertex count is rounded
	// up to a multiple of 8.
	// The scratch size is stored as a qword count, *not* a byte count!
	return (16 * (numInputAttributes + 1) * ((numUniqueVertexes + 7) & ~7)) >> 4;
}

//-------------------
// This function generates a final EdgeGeomSpuConfigInfo structure
void edgeGeomMakeSpuConfigInfo(uint32_t numUniqueVertexes, uint32_t numTriangles, uint32_t numInputAttributes, uint8_t segmentFlags,
							   EdgeGeomIndexesFlavor indexListType, EdgeGeomSkinningFlavor skinType, EdgeGeomMatrixFormat skinMatrixFormat,
							   uint8_t inputVertexFormatId, uint8_t secondaryInputVertexFormatId, uint8_t outputVertexFormatId, uint8_t /*deltaFormatId*/,
							   uint32_t commandBufferHoleSize, uint8_t **outSpuConfigInfo)
{
	EdgeGeomSpuConfigInfo *out = (EdgeGeomSpuConfigInfo*)edgeGeomAlloc(sizeof(EdgeGeomSpuConfigInfo));

	bool includesExtraUniformTable = (segmentFlags & EDGE_GEOM_FLAG_INCLUDES_EXTRA_UNIFORM_TABLE) != 0;
	EDGEASSERT(numInputAttributes + (includesExtraUniformTable ? 1 : 0) <= 16);
	out->flagsAndUniformTableCount = (uint8_t)( (numInputAttributes-1) + (includesExtraUniformTable ? 1 : 0) ) & 0xF;
	out->flagsAndUniformTableCount |= segmentFlags;

	EDGEASSERT(commandBufferHoleSize % 16 == 0); // must be an even number of quadwords
	EDGEASSERT(commandBufferHoleSize / 16 < 255); // quadword count must fit in a byte
	out->commandBufferHoleSize = (uint8_t)(commandBufferHoleSize / 16); // hole size is stored as a quadword count

	out->inputVertexFormatId = inputVertexFormatId;
	out->secondaryInputVertexFormatId = secondaryInputVertexFormatId;
	out->outputVertexFormatId = outputVertexFormatId;
	out->vertexDeltaFormatId = 0xFF; // all deltas now use a custom vertex format.  This field should be deprecated.
	out->indexesFlavorAndSkinningFlavor = ((uint8_t)indexListType << 4) | (uint8_t)skinType;
	out->skinningMatrixFormat = (uint8_t)skinMatrixFormat;
	_edgeGeomWriteShort( (uint16_t)(numUniqueVertexes), &(out->numVertexes), _EDGE_GEOM_WRITE_BIG_ENDIAN );
	_edgeGeomWriteShort( (uint16_t)(numTriangles*3), &(out->numIndexes), _EDGE_GEOM_WRITE_BIG_ENDIAN );
	out->indexesOffset = 0xFFFFFFFF;

	*outSpuConfigInfo = (uint8_t*)out;
}

//-------------------
// This function generates a final big-endian triangle list (possibly compressed) and returns it in indexList, setting indexesSizes in the process.
void edgeGeomMakeIndexBuffer(const uint32_t *triangles, uint32_t numTriangles, EdgeGeomIndexesFlavor indexListType, uint8_t **outIndexBuffer, uint16_t outIndexDmaSizes[2])
{
	EdgeGeomIndexesSizes measuredSizes;
	memset( &measuredSizes, 0, sizeof(measuredSizes) );
	
	switch (indexListType)
	{
		case kIndexesU16TriangleListCW:
		case kIndexesU16TriangleListCCW:
		{
			// write out the index list as 16-bit big-endian indexes.
			// Note: 32 extra bytes pad in the array (because of EnableStaticOptimizations()) are not needed anymore
			// aligned allocation + padding set to zero
			uint32_t indexListSize = (numTriangles*3*sizeof(uint16_t) + 15) & ~15;
			uint16_t *finalIndexList = (uint16_t*)edgeGeomAlloc( indexListSize );  // shorts are easier to iterate
			memset(finalIndexList, 0, indexListSize);
			*outIndexBuffer = (uint8_t *)finalIndexList;  // return the memory for the index list
			for (uint32_t iIndex=0; iIndex<numTriangles*3; iIndex++)
			{
				EDGEASSERT(triangles[iIndex] < 0xFFFF);
				uint16_t index16 = (uint16_t)(triangles[iIndex] & 0xFFFF);
				_edgeGeomWriteShort(index16, finalIndexList+iIndex, _EDGE_GEOM_WRITE_BIG_ENDIAN);
			}

			measuredSizes.m_inputSize = indexListSize;
			measuredSizes.m_maxTempSize = indexListSize;
			break;
		}
			
		case kIndexesCompressedTriangleListCW:
		case kIndexesCompressedTriangleListCCW:
		{
			// use index compression on this index list
			_edgeGeomCompressIndexes(triangles, numTriangles, outIndexBuffer, &measuredSizes);

			break;
		}
		
		default:
			EDGEERROR_F();
	}

	const uint32_t firstDma = EDGEMIN(16384u, measuredSizes.m_inputSize);
	const uint32_t secondDma = measuredSizes.m_inputSize - firstDma;
	EDGEASSERT(secondDma<=16384u);  // too big
	outIndexDmaSizes[0] = (uint16_t)firstDma;   // size of the index list after compression, capped at 16k
	outIndexDmaSizes[1] = (uint16_t)secondDma;  // overflow to the second DMA as needed

}

//-------------------
// This function is used by primary and secondary vertex stream writers.  It primarily exists so as not to unnecessarily replicate
// the conversion and writing process between the functions.
void _edgeGeomConvertAndWriteDataToSpuStream(const EdgeGeomSpuVertexAttributeDefinition &attrDef, const float *srcAttr, uint8_t *destStream, float const **currentMins, bool writeBigEndian)
{
	switch (attrDef.m_type)
	{
		case kSpuAttr_U8:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				uint8_t u8 = _ConvertF32toU8(*(srcAttr+iComponent));
				destStream[iComponent] = u8;
			}
			break;
		}
		case kSpuAttr_U8N:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				uint8_t u8n = _ConvertF32toU8N(*(srcAttr+iComponent));
				destStream[iComponent] = u8n;
			}
			break;
		}
		case kSpuAttr_I16:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				uint16_t i16 = _ConvertF32toI16(*(srcAttr+iComponent));
				_edgeGeomWriteShort(i16, destStream+sizeof(uint16_t)*iComponent, writeBigEndian);
			}
			break;
		}
		case kSpuAttr_I16N:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				uint16_t i16n = _ConvertF32toI16N(*(srcAttr+iComponent));
				_edgeGeomWriteShort(i16n, destStream+sizeof(uint16_t)*iComponent, writeBigEndian);
			}
			break;
		}
		case kSpuAttr_F16:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				uint16_t f16 = _ConvertF32toF16(*(srcAttr+iComponent));
				_edgeGeomWriteShort(f16, destStream+sizeof(uint16_t)*iComponent, writeBigEndian);
			}
			break;
		}
		case kSpuAttr_F32:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				float f32 = *(srcAttr+iComponent);
				_edgeGeomWriteFloat(f32, destStream+sizeof(float)*iComponent, writeBigEndian);
			}
			break;
		}
		case kSpuAttr_X11Y11Z10N:  // this consumes 3 float32's, not one!
		{
			float f32x = srcAttr[0];
			float f32y = srcAttr[1];
			float f32z = srcAttr[2];
			uint32_t x11y11z10 = _ConvertF32toX11Y11Z10N(f32x, f32y, f32z);
			_edgeGeomWriteInt(x11y11z10, destStream, writeBigEndian);
			break;
		}
		case kSpuAttr_UnitVector:
		{
			EDGEASSERT(attrDef.m_count==3 || attrDef.m_count==4);
				
			static const float minRange = -sqrtf(2.0f)/2.0f;
			static const float maxRange = sqrtf(2.0f)/2.0f;
				
			// find the two smallest components in the first three elements of the attribute
			float f32x = srcAttr[0];
			float f32y = srcAttr[1];
			float f32z = srcAttr[2];
			float f32w = (attrDef.m_count==4)? srcAttr[3] : 1.0f;  // fetch the 4th component if present

			float minA;
			float minB;
			float maxC;
			uint32_t twoBits;
			if (fabsf(f32x)>fabsf(f32y) && fabsf(f32x)>fabsf(f32z))
			{
				// x will be computed
				minA = f32y;
				minB = f32z;
				maxC = f32x;
				twoBits = 0;  // tell runtime that X needs to be generated
			}
			else if (fabsf(f32y)>fabsf(f32x) && fabsf(f32y)>fabsf(f32z))
			{
				// y will be computed
				minA = f32x;
				minB = f32z;
				maxC = f32y;
				twoBits = 1;  // tell runtime that Y needs to be generated
			}
			else
			{
				// z will be computed
				minA = f32x;
				minB = f32y;
				maxC = f32z;
				twoBits = 2;  // tell runtime that Z needs to be generated
			}

			// generate the smallestA, smallestB, shuf, W, and S data
			const uint32_t smallestA = 
				(minA<=minRange)? 0 :
				(minA>=maxRange)? (1<<10)-1 :
				(uint32_t)((minA-minRange)/(maxRange-minRange)*(1<<10));
			const uint32_t smallestB = 
				(minB<=minRange)? 0 :
				(minB>=maxRange)? (1<<10)-1 :
				(uint32_t)((minB-minRange)/(maxRange-minRange)*(1<<10));
			const uint32_t W = (f32w>0.0f ? 1 : 0);  // capture the left- or right-handedness of the binormal, if present
			const uint32_t S = (maxC>0.0f ? 1 : 0);  // capture the sign of the largest component

			// pack into 24 bits and write it out in the appropriate byte order.
			uint32_t packedData = (smallestA << 14) | (smallestB << 4) | (twoBits << 2) | (W << 1) | S;
			if (writeBigEndian)
			{
				destStream[0] = (uint8_t) ((packedData>>16)&0xFF);
				destStream[1] = (uint8_t) ((packedData>>8)&0xFF);
				destStream[2] = (uint8_t) ((packedData>>0)&0xFF);
			}
			else
			{
				destStream[0] = (uint8_t) ((packedData>>0)&0xFF);
				destStream[1] = (uint8_t) ((packedData>>8)&0xFF);
				destStream[2] = (uint8_t) ((packedData>>16)&0xFF);
			}
			break;
		}
		case kSpuAttr_FixedPoint:  // This requires the fixed point bitfield to specify how many bits per component.
		{
			_edgeGeomConvertToFixedPoint(attrDef, srcAttr, *currentMins, destStream);
			*currentMins += 4; // always 4 per attribute... 
			break;
		}
	}
}
// This variation is used to generate vertex streams that will go directly to the RSX
void _edgeGeomConvertAndWriteDataToRsxStream(const EdgeGeomRsxVertexAttributeDefinition &attrDef, const float *srcAttr, uint8_t *destStream, bool writeBigEndian)
{
	switch (attrDef.m_type)
	{
		case kRsxAttr_U8:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				uint8_t u8 = _ConvertF32toU8(*(srcAttr+iComponent));
				destStream[iComponent] = u8;
			}
			break;
		}
		case kRsxAttr_U8N:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				uint8_t u8n = _ConvertF32toU8N(*(srcAttr+iComponent));
				destStream[iComponent] = u8n;
			}
			break;
		}
		case kRsxAttr_I16:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				uint16_t i16 = _ConvertF32toI16(*(srcAttr+iComponent));
				_edgeGeomWriteShort(i16, destStream+sizeof(uint16_t)*iComponent, writeBigEndian);
			}
			break;
		}
		case kRsxAttr_I16N:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				uint16_t i16n = _ConvertF32toI16N(*(srcAttr+iComponent));
				_edgeGeomWriteShort(i16n, destStream+sizeof(uint16_t)*iComponent, writeBigEndian);
			}
			break;
		}
		case kRsxAttr_F16:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				uint16_t f16 = _ConvertF32toF16(*(srcAttr+iComponent));
				_edgeGeomWriteShort(f16, destStream+sizeof(uint16_t)*iComponent, writeBigEndian);
			}
			break;
		}
		case kRsxAttr_F32:
		{
			for(uint32_t iComponent=0; iComponent<attrDef.m_count; ++iComponent)
			{
				float f32 = *(srcAttr+iComponent);
				_edgeGeomWriteFloat(f32, destStream+sizeof(float)*iComponent, writeBigEndian);
			}
			break;
		}
		case kRsxAttr_X11Y11Z10N:  // this consumes 3 float32's, not one!
		{
			float f32x = srcAttr[0];
			float f32y = srcAttr[1];
			float f32z = srcAttr[2];
			uint32_t x11y11z10 = _ConvertF32toX11Y11Z10N(f32x, f32y, f32z);
			_edgeGeomWriteInt(x11y11z10, destStream, writeBigEndian);
			break;
		}
	}
}

// This function walks the triangle list and whenever a new vertex is encountered, 
// dumps the compressed version of its data into the vertex buffer.
void edgeGeomMakeSpuVertexBuffer(const float *sourceVertexes, uint32_t numFloatsPerSourceVertex,
							  const uint16_t *sourceAttributeIndexes, const EdgeGeomAttributeId *sourceAttributeIds, uint8_t numSourceAttributes,
							  const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes, const EdgeGeomSpuVertexFormat& vertexFormat, 
							  uint8_t **outVertexBuffer, uint16_t outVertexDmaSizes[3], uint32_t **outFixedPointOffsets, uint32_t *outFixedOffsetsSize)
{
	if (vertexFormat.m_vertexStride == 0)
	{
		*outVertexBuffer = 0;
		outVertexDmaSizes[0] = 0;
		outVertexDmaSizes[1] = 0;
		outVertexDmaSizes[2] = 0;
		*outFixedPointOffsets = 0;
		*outFixedOffsetsSize = 0;
		return;
	}

	// allocate space for the output vertex buffer
	const uint32_t totalVertexBufferSize = (vertexFormat.m_vertexStride * numUniqueVertexes  + 15) & ~15;  // round up to be divisible by 16	
	*outVertexBuffer = (uint8_t*)edgeGeomAlloc(totalVertexBufferSize);
	memset(*outVertexBuffer, 0, totalVertexBufferSize); // for determinism

	// For each fixed point attribute, we determine the minimum value taken by each attribute component in this
	// segment.  The value of each component is then shifted up by this amount, so that all fixed-point
	// compression is done on non-negative values.  The offset is stored in the fixed point offsets array,
	// which will be passed to the runtime to adjust the final values back into the correct range before they're
	// decompressed.
	float *fixedPointMins = NULL;
	_edgeGeomFindAttributeMins(sourceVertexes, numFloatsPerSourceVertex, vertexFormat,
		sourceAttributeIndexes, sourceAttributeIds, numSourceAttributes,
		originalVertexIndexes, numUniqueVertexes,
		&fixedPointMins, outFixedPointOffsets, outFixedOffsetsSize);
	
	// Convert all the unique vertexes to their final formats.  Order matters, naturally!
	for(uint32_t iVertex=0; iVertex<numUniqueVertexes; ++iVertex)
	{
		const uint32_t srcVertexIndex = originalVertexIndexes[iVertex];
		const float *currentMins = fixedPointMins;  // remembers which offsets have been used by fixed point attributes while writing them out
		const float *srcVertex = &sourceVertexes[numFloatsPerSourceVertex*srcVertexIndex];
		uint8_t *destVertex = ((uint8_t *)*outVertexBuffer) + iVertex*vertexFormat.m_vertexStride;	

		for (uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; iAttr++)
		{
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

			// walking off the end of the per-vertex data is bad
			EDGEASSERT(srcAttrIndex + vertexFormat.m_attributeDefinition[iAttr].m_count <= numFloatsPerSourceVertex);  
			const float *srcAttr = &srcVertex[srcAttrIndex];
			uint8_t *destAttr = destVertex + vertexFormat.m_attributeDefinition[iAttr].m_byteOffset;
			// write out data.
			// Since this function is writing an SPU stream, it always writes big-endian data.  For an endian-neutral
			// version, see edgeGeomMakeRsxVertexBuffer().
			_edgeGeomConvertAndWriteDataToSpuStream(vertexFormat.m_attributeDefinition[iAttr], srcAttr, destAttr,
				&currentMins, _EDGE_GEOM_WRITE_BIG_ENDIAN);
		}
	}
	edgeGeomFree(fixedPointMins);

	// fixup the vertexesSizes so no more than 16k is sent in the first DMA.
	const uint32_t firstDma = EDGEMIN(16384u, totalVertexBufferSize);
	const uint32_t secondDma = EDGEMIN(16384u, totalVertexBufferSize - firstDma);
	const uint32_t thirdDma = totalVertexBufferSize - firstDma - secondDma;
	EDGEASSERT(thirdDma<16384u);  // must not exceed three full DMA kicks
	outVertexDmaSizes[0] = (uint16_t)firstDma;   // size of the index list after compression, capped at 16k
	outVertexDmaSizes[1] = (uint16_t)secondDma;  // overflow to the second DMA as needed, capped at 16k
	outVertexDmaSizes[2] = (uint16_t)thirdDma;  // overflow to the third DMA as needed
}

void edgeGeomMakeRsxVertexBuffer(const float *sourceVertexes, uint32_t numFloatsPerSourceVertex,
							  const uint16_t *sourceAttributeIndexes, const EdgeGeomAttributeId *sourceAttributeIds, uint8_t numSourceAttributes,
							  const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes, const EdgeGeomRsxVertexFormat& vertexFormat, bool writeBigEndian,
							  uint8_t **outVertexBuffer, uint32_t *outVertexBufferSize)
{
	if (vertexFormat.m_vertexStride == 0)
	{
		*outVertexBuffer = 0;
		*outVertexBufferSize = 0;
		return;
	}

	// allocate space for the output vertex buffer
	const uint32_t totalVertexBufferSize = vertexFormat.m_vertexStride * numUniqueVertexes; // no rounding necessary for RSX-only vertex streams
	*outVertexBuffer = (uint8_t*)edgeGeomAlloc(totalVertexBufferSize);
	memset(*outVertexBuffer, 0, totalVertexBufferSize); // for determinism

	// Convert all the unique vertexes to their final formats.  Order matters, naturally!
	for(uint32_t iVertex=0; iVertex<numUniqueVertexes; ++iVertex)
	{
		const uint32_t srcVertexIndex = originalVertexIndexes[iVertex];
		const float *srcVertex = &sourceVertexes[numFloatsPerSourceVertex*srcVertexIndex];
		uint8_t *destVertex = ((uint8_t *)*outVertexBuffer) + iVertex*vertexFormat.m_vertexStride;	
		for (uint32_t iAttr=0; iAttr<vertexFormat.m_numAttributes; iAttr++)
		{
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

			// walking off the end of the per-vertex data is bad
			EDGEASSERT(srcAttrIndex + vertexFormat.m_attributeDefinition[iAttr].m_count <= numFloatsPerSourceVertex);  
			const float *srcAttr = &srcVertex[srcAttrIndex];
			uint8_t *destAttr = destVertex + vertexFormat.m_attributeDefinition[iAttr].m_byteOffset;
			// write out data, incrementing currentVertexData.  Use the byte order passed in by the caller.
			_edgeGeomConvertAndWriteDataToRsxStream(vertexFormat.m_attributeDefinition[iAttr], srcAttr, destAttr,
				writeBigEndian);
		}
	}
	*outVertexBufferSize = totalVertexBufferSize;
}


//-------------------
// This function creates skinning data with remapped matrix set (in two runs)
void edgeGeomMakeSkinningBuffer(const int32_t *matrixIndexes, const float *skinningWeights, EdgeGeomSkinningFlavor skinType,
								EdgeGeomMatrixFormat skinMatrixFormat, const uint32_t *originalVertexIndexes, uint32_t numUniqueVertexes,
								uint8_t **outSkinIndexesAndWeights, uint16_t outSkinIndexesAndWeightsDmaSizes[2],
								uint16_t outSkinMatricesByteOffsets[2], uint16_t outSkinMatricesSizes[2])
{
	// Trivial case: not skinned
	if (skinType==kSkinNone)
	{
		*outSkinIndexesAndWeights = NULL;
		outSkinIndexesAndWeightsDmaSizes[0] = 0;
		outSkinIndexesAndWeightsDmaSizes[1] = 0;
		outSkinMatricesByteOffsets[0] = 0;
		outSkinMatricesByteOffsets[1] = 0;
		outSkinMatricesSizes[0] = 0;
		outSkinMatricesSizes[1] = 0;
		return;
	}

	// Determine how many skinning bones to expect per vertex
	uint32_t numOutputBonesPerVertex = 0;
	switch(skinType)
	{
	case kSkinSingleBoneNoScaling:
	case kSkinSingleBoneUniformScaling:
	case kSkinSingleBoneNonUniformScaling:
		// INTENTIONAL FALL-THROUGH! single-bone skinning does actually output 4 bones; it's fixed up as a postprocess.
	default:
		numOutputBonesPerVertex = 4;
		break;
	}

	// If the skinning flavor expects us to be skinned, then the skinning data
	// arrays had better be non-NULL!
	EDGEASSERT(matrixIndexes != NULL);
	EDGEASSERT(skinningWeights != NULL);

	// first, figure out what unique matrices we use IN THIS PARTITION
	int32_t *tmpMatrixIndexes = (int32_t*)edgeGeomAlloc(numUniqueVertexes*kEdgeGeomNumInputBonesPerVertex*sizeof(int32_t));  // 4 bones per vertex
	for (uint32_t iVertex=0; iVertex<numUniqueVertexes; ++iVertex)
	{
		const uint32_t srcVertexIndex = originalVertexIndexes[iVertex];
		// copy each vertex's four matrices to tmpMatrixIndexes, one after the other
		memcpy(tmpMatrixIndexes + iVertex*kEdgeGeomNumInputBonesPerVertex,
			matrixIndexes + srcVertexIndex*kEdgeGeomNumInputBonesPerVertex,
			sizeof(int32_t)*kEdgeGeomNumInputBonesPerVertex);
	}
	
	uint32_t numUniqueMatrices = _edgeGeomSortUniqueArrayInt(tmpMatrixIndexes, numUniqueVertexes*kEdgeGeomNumInputBonesPerVertex);
	EDGEASSERT(numUniqueMatrices!=0);  // probably a configuration issue... someone tried to process an unskinned geometry unit with skinning enabled

	// remove the -1 if it exists
	if (tmpMatrixIndexes[0]==-1)
	{
		numUniqueMatrices--;
		EDGEASSERT(numUniqueMatrices>0);  // this fails only if skinning is enabled, but NO SKINNING INFO exists at all per vertex.
		memmove(tmpMatrixIndexes, tmpMatrixIndexes+1, numUniqueMatrices*sizeof(int32_t));
	}

	// now, given that we know the unique array of matrices is sorted, all we have to 
	// do is look at the min/max range (first and last entry), then find the biggest gap
	// in the matrix indexes and cut the DMA into two kicks.
	int32_t firstMatrix = tmpMatrixIndexes[0];
	int32_t lastMatrix = tmpMatrixIndexes[numUniqueMatrices-1];
	EDGEASSERT(firstMatrix>=0);  // someone filled the matrix indexes table with more than one negative value, a no-no.

	// walk the unique matrices looking for the biggest gap
	int32_t biggestGap = 0;
	uint32_t indexToGap = 0;  // this is where in tmpMatrixIndexes that the biggest gap was detected, NOT the matrix index itself
	for (uint32_t iMatrix=1; iMatrix<numUniqueMatrices; iMatrix++)
	{
		const int32_t gap = tmpMatrixIndexes[iMatrix] - tmpMatrixIndexes[iMatrix-1] - 1;
		if (gap>biggestGap)
		{
			biggestGap = gap;
			indexToGap = iMatrix;
		}
	}
	// Make sure we're not trying to upload too many bones (this should have been caught by the partitioner;
	// please contact the Edge developers if this assert is firing!)
	if((lastMatrix-firstMatrix+1)-biggestGap >= 256)
	{
		printf("EDGE: Bad skinning range: matrices: %d and gap = %d [first=%d, last=%d]\n", numUniqueMatrices, biggestGap, firstMatrix, lastMatrix);
		EDGEASSERT((lastMatrix-firstMatrix+1)-biggestGap < 256);  // if this fails, you have more than 256 bones and are using most of them in one partition.
	}

	// make a mapping table that converts original matrix indexes to 'local' indexes
	int32_t *matrixToLocalLookup = (int32_t*)edgeGeomAlloc((lastMatrix+1)*sizeof(int32_t));

	// bias all matrix indices first, so that the range [first..last] becomes [0..numUniqueMatrices]
	for (int32_t iMatrix=firstMatrix; iMatrix<=lastMatrix; iMatrix++)
	{
		matrixToLocalLookup[iMatrix] = iMatrix - firstMatrix;
	}

	// write out the byte offsets for each DMA start
	const uint32_t kMatrixSize = (skinMatrixFormat == kMatrix3x4RowMajor) ? 3*4*sizeof(float) : 4*4*sizeof(float);
	outSkinMatricesByteOffsets[0] = (uint16_t)(firstMatrix * kMatrixSize);
	outSkinMatricesByteOffsets[1] = 0;

	// and write out the DMA kick lengths, rounded up to the next 16 bytes
	outSkinMatricesSizes[0] = (uint16_t)((lastMatrix - firstMatrix + 1) * kMatrixSize);
	outSkinMatricesSizes[1] = 0;
	
	// finally, if there appears to be a gap worth optimizing out, rewrite some of the matrices
	// so the ones starting with the second run begin immediately after the end of the first run,
	// skipping the biggest gap worth of matrices.
	if (biggestGap>0)  
	{
		for (int32_t iMatrix=tmpMatrixIndexes[indexToGap]; iMatrix<=lastMatrix; iMatrix++)
		{
			matrixToLocalLookup[iMatrix] = iMatrix - firstMatrix - biggestGap;
		}

		// fix up the dma start for the second DMA and length for both kicks
		outSkinMatricesByteOffsets[1] = (uint16_t)(tmpMatrixIndexes[indexToGap] * kMatrixSize);

		outSkinMatricesSizes[0] = (uint16_t)((tmpMatrixIndexes[indexToGap-1] - firstMatrix + 1) * kMatrixSize);
		outSkinMatricesSizes[1] = (uint16_t)((lastMatrix - tmpMatrixIndexes[indexToGap] + 1) * kMatrixSize);
	}

	// allocate skinning buffer 
	// 2 bytes for each matrix/weight (aligned to 16 to avoid overruns)
	uint32_t skinBufferSize = (numUniqueVertexes * numOutputBonesPerVertex * 2*sizeof(uint8_t)); 
	skinBufferSize = (skinBufferSize + 0x0f) & ~0x0f;
	uint8_t *skinBuffer = (uint8_t*)edgeGeomAlloc(skinBufferSize);

	// make sure padding is set to 0 to keep compression deterministic
	memset(skinBuffer, 0, skinBufferSize);

	// generate the skinning info now
	*outSkinIndexesAndWeights = skinBuffer;

	// walk the unique vertexes and emit their skinning data
	float *srcWeights = (float*)edgeGeomAlloc(kEdgeGeomNumInputBonesPerVertex*sizeof(float));
	uint32_t *weightsAdj = (uint32_t*)edgeGeomAlloc(numOutputBonesPerVertex*sizeof(uint32_t));
	int32_t *srcMatrixIndices = (int32_t*)edgeGeomAlloc(kEdgeGeomNumInputBonesPerVertex*sizeof(int32_t));
	for (uint32_t iVertex=0; iVertex<numUniqueVertexes; iVertex++)
	{
		const uint32_t srcVertexIndex = originalVertexIndexes[iVertex];

		memcpy(srcWeights,
			skinningWeights + srcVertexIndex*kEdgeGeomNumInputBonesPerVertex,
			kEdgeGeomNumInputBonesPerVertex*sizeof(float));
		memcpy(srcMatrixIndices,
			matrixIndexes + srcVertexIndex*kEdgeGeomNumInputBonesPerVertex,
			kEdgeGeomNumInputBonesPerVertex*sizeof(int32_t));

		// Edge requires all input skinning weights to be in the range [0.0-1.0] -- otherwise the normalization
		// below will not work!
		for(uint32_t iWeight=0; iWeight<kEdgeGeomNumInputBonesPerVertex; ++iWeight)
		{
			// Values outside this range by some tiny amount are tolerated and clamped; floating point 
			// rounding error is a regrettable but unavoidable reality.
			if (srcWeights[iWeight] < 0.0f && srcWeights[iWeight] >= -0.00001f)
				srcWeights[iWeight] = 0.0f;
			else if (srcWeights[iWeight] > 1.0f && srcWeights[iWeight] <= 1.00001f)
				srcWeights[iWeight] = 1.0f;
			// If we're still outside the legal range, it's an error.
			EDGEASSERT(srcWeights[iWeight] >= 0.0f && srcWeights[iWeight] <= 1.0f);
		}

		// If the input skinning data has more non-zero weights than the output format can support,
		// discard the smallest weights. Heavy-handed, but serviceable.
		if (kEdgeGeomNumInputBonesPerVertex > numOutputBonesPerVertex)
		{
			float originalWeightSum = 0;
			for(uint32_t iWeight=0; iWeight<kEdgeGeomNumInputBonesPerVertex; ++iWeight)
			{
				originalWeightSum += srcWeights[iWeight];
			}

			// Repeatedly find the largest weight, copy it and its bone index into the temp arrays, and set the
			// original array entries to zero.
			float *tmpWeights = (float*)edgeGeomAlloc(numOutputBonesPerVertex*sizeof(float));
			int32_t *tmpMatrixIndices = (int32_t*)edgeGeomAlloc(numOutputBonesPerVertex*sizeof(int32_t));
			memset(tmpWeights, 0, numOutputBonesPerVertex*sizeof(float));
			memset(tmpMatrixIndices, 0, numOutputBonesPerVertex*sizeof(int32_t));
			for(uint32_t iOutWeight=0; iOutWeight < numOutputBonesPerVertex; ++iOutWeight)
			{
				uint32_t largestWeightIndex = 0;
				for(uint32_t iWeight=1; iWeight<kEdgeGeomNumInputBonesPerVertex; ++iWeight)
				{
					if (srcWeights[iWeight] > srcWeights[largestWeightIndex])
					{
						largestWeightIndex = iWeight;
					}
				}
				tmpWeights[iOutWeight] = srcWeights[largestWeightIndex];
				tmpMatrixIndices[iOutWeight] = srcMatrixIndices[largestWeightIndex];
				srcWeights[largestWeightIndex] = 0;
				srcMatrixIndices[largestWeightIndex] = -1;
			}
			// Copy the largest weights back to the source arrays
			memcpy(srcWeights, tmpWeights, numOutputBonesPerVertex*sizeof(float));
			memcpy(srcMatrixIndices, tmpMatrixIndices, numOutputBonesPerVertex*sizeof(int32_t));
			edgeGeomFree(tmpWeights);
			edgeGeomFree(tmpMatrixIndices);

			// Scale the new weights up so that they sum to 1.0 again
			float newWeightSum = 0;
			for(uint32_t iWeight=0; iWeight<numOutputBonesPerVertex; ++iWeight)
			{
				newWeightSum += srcWeights[iWeight];
			}
			EDGEASSERT(newWeightSum <= originalWeightSum);
			const float weightScale = originalWeightSum / newWeightSum;
			for(uint32_t iWeight=0; iWeight<numOutputBonesPerVertex; ++iWeight)
			{
				srcWeights[iWeight] *= weightScale;
			}
		}

		// Adjust the weights to make sure they add up to exactly 255 when quantized to 8 bits
		memset(weightsAdj, 0, numOutputBonesPerVertex*sizeof(uint32_t));
		uint32_t weightSum = 0;
		for (;;)
		{
			// calculate how many units are unaccounted for
			weightSum = 0;
			for (uint32_t iBone=0; iBone<numOutputBonesPerVertex; ++iBone)
			{
				weightSum += (uint8_t)(srcWeights[iBone]*255.0f) + weightsAdj[iBone];
			}
			
			if (weightSum==255)  // exit only when we've completed our objective of full weighting
				break;
			
			// at least one more unit to distribute, so find the matrix that will 
			// achieve LEAST PROPORTIONAL ERROR as compared to the original data requested by the artist.
			// Many other possible algorithms exist, but in terms of visual artifacting, it is always better
			// to err on the side of the most prominent weight, rather than on the side of the smallest 
			// fractional weight (which may well have been a mistake in the first place).  The proportional
			// error is what we want to reduce, so most extra units are allocated to the largest weights.
			uint32_t bestIndex = 0;
			float minError = FLT_MAX;
			for (uint32_t iBone=0; iBone<numOutputBonesPerVertex; ++iBone)
			{
				float const newValue = ((uint8_t)(srcWeights[iBone]*255.0f) + weightsAdj[iBone] + 1.0f);
				float const error = (newValue - srcWeights[iBone]*255.0f) / (srcWeights[iBone]*255.0f+0.00001f);  // avoid div by zero
				if (error<minError)
				{
					minError = error;
					bestIndex = iBone;
				}
			}
			weightsAdj[bestIndex]++;
		}

		// emit the skinning info for this vertex
		for (uint32_t iWeight=0; iWeight<numOutputBonesPerVertex; iWeight++)
		{
			const int32_t sourceMatrix = srcMatrixIndices[iWeight];
			*skinBuffer++ = (uint8_t)(srcWeights[iWeight]*255.0f) + (uint8_t)weightsAdj[iWeight];
			*skinBuffer++ = (uint8_t)(sourceMatrix>=0 ? matrixToLocalLookup[sourceMatrix] : 0);  // make any unassigned weight use matrix 0
		}
	}
	edgeGeomFree(srcWeights);
	edgeGeomFree(weightsAdj);
	edgeGeomFree(srcMatrixIndices);

	// Check for the case where every vertex is influenced by exactly one matrix.
	// If so, we can use single-bone skinning for significant memory and SPU performance
	// savings.
	bool supportsSingleBone = true;
	uint8_t *finalSkinBuffer = *outSkinIndexesAndWeights;
	for(uint32_t iVertex=0; iVertex<numUniqueVertexes; ++iVertex)
	{
		const uint8_t *skin = &finalSkinBuffer[iVertex * 2*numOutputBonesPerVertex];
		uint32_t influenceCount = 0;
		for(uint32_t iBone=0; iBone<numOutputBonesPerVertex; ++iBone)
		{
			influenceCount += (skin[2*iBone] != 0) ? 1 : 0;
		}
		if (influenceCount != 1)
		{
			supportsSingleBone = false;
			break;
		}
	}
	
	bool useSingleBone = (skinType == kSkinSingleBoneNoScaling) || (skinType == kSkinSingleBoneUniformScaling)
		|| (skinType == kSkinSingleBoneNonUniformScaling);
	EDGEASSERT( !useSingleBone || (useSingleBone && supportsSingleBone) ); // If single-bone skinning was requested, the geometry must support it!

	// If we're using single-bone skinning, rewrite the skinning info in-place to only
	// contain the matrix index for each vertex.
	if (useSingleBone)
	{
		uint8_t *outIndexes = *outSkinIndexesAndWeights;
		for(uint32_t iVertex=0; iVertex<numUniqueVertexes; ++iVertex)
		{
			const uint8_t *skin = &finalSkinBuffer[iVertex * 2*numOutputBonesPerVertex];
			for(uint32_t iBone=0; iBone<numOutputBonesPerVertex; ++iBone)
			{
				if (skin[2*iBone] > 0)
				{
					outIndexes[iVertex] = skin[1];
					break;
				}
			}
		}
	}

	// configure the DMA lengths now, rounded up to the next 16 bytes
	const uint32_t totalSkinBufferSize = useSingleBone ? ((numUniqueVertexes + 15) & ~15)
		: ((numUniqueVertexes * 2*numOutputBonesPerVertex + 15) & ~15);
	const uint32_t firstDma = EDGEMIN(16384u, totalSkinBufferSize);
	const uint32_t secondDma = totalSkinBufferSize - firstDma;
	EDGEASSERT(secondDma<16384);  // must not exceed two full DMA kicks
	outSkinIndexesAndWeightsDmaSizes[0] = (uint16_t)firstDma;  // size of the first DMA kick of skin data
	outSkinIndexesAndWeightsDmaSizes[1] = (uint16_t)secondDma; // overflow into second DMA as needed.

	// clean up
	edgeGeomFree(tmpMatrixIndexes);
	edgeGeomFree(matrixToLocalLookup);
}
