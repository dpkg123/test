/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "edge/libedgegeomtool/libedgegeomtool_internal.h"

// helper function that walks through the finalIndexes (which are now actually deltas) and writes 
// them out to the deltaBlock as the specified number of bits.
static void FillDeltaBlockBuffer(const int32_t *finalIndexes, uint32_t numFinalIndexes, uint8_t *deltaBlock, uint32_t bitsPerIndex, uint32_t deltaOffset)
{
	// bit compress the rest, using a larger type than 8 bits to make it easier
	uint32_t currentByte = 0;
	uint32_t numCurrentBits = 0;
	uint32_t andMask = (1 << bitsPerIndex) - 1;
	for (uint32_t iIndex=0; iIndex<numFinalIndexes; iIndex++)
	{
		const int32_t delta = finalIndexes[iIndex] + deltaOffset;
		uint32_t compressedBits = (delta & andMask);
		EDGEASSERT(delta>=0);  // if this fails, the deltaOffset is incorrect

		// start filling bits in the high order first
		currentByte |= compressedBits << (32 - numCurrentBits - bitsPerIndex);
		numCurrentBits += bitsPerIndex;

		// if we have a full byte, dump the byte into the buffer and keep going
		while (numCurrentBits >= 8)
		{
			numCurrentBits -= 8;
			*deltaBlock++ = (uint8_t)((currentByte >> 24) & 0xff);
			currentByte <<= 8;  // slide the bits up to the top range again
		}
	}

	// write out any partial byte
	if (numCurrentBits!=0)
	{
		*deltaBlock++ = (uint8_t)((currentByte >> 24) & 0xff);
	}
}

//-------------------
// this does a block-by-block subtraction and computes the number bits required to express the largest delta
static void ConvertIndexesToDeltas(int32_t *finalIndexes, uint32_t numFinalIndexes, uint32_t &bitsPerIndex, uint32_t &deltaOffset)
{
	int32_t previousIndexes[8];

	// preload the previous indexes array
	int32_t minDelta = 0;
	int32_t maxDelta = 0;
	for (uint32_t iIndex=0; iIndex<numFinalIndexes && iIndex<8; iIndex++)
	{
		previousIndexes[iIndex] = finalIndexes[iIndex];

		// remember the biggest delta (even though this is not a delta, they are guaranteed 
		// to be pretty small because they come at the start of the index list)
		if (finalIndexes[iIndex]<minDelta)
			minDelta = finalIndexes[iIndex];
		if (finalIndexes[iIndex]>maxDelta)
			maxDelta = finalIndexes[iIndex];
	}

	// convert each chunk of 8 to a delta of the 8 before it
	for (uint32_t iIndex=8; iIndex<numFinalIndexes; iIndex++)
	{
		const int32_t sourceIndex = finalIndexes[iIndex];
		const int32_t delta = sourceIndex - previousIndexes[iIndex % 8];  // convert to a delta from the previous value

		// remember the biggest delta
		if (delta<minDelta)
			minDelta = delta;
		if (delta>maxDelta)
			maxDelta = delta;

		previousIndexes[iIndex % 8] = sourceIndex;  // store the original value here, not the delta
		finalIndexes[iIndex] = delta;
	}

	// handle the oddball case where there are no deltas
	const uint32_t deltaRange = maxDelta - minDelta;
	if (deltaRange==0)
	{
		// This is very rare, but is possible.  Q: Should it be 1 anyway?
		bitsPerIndex = 0;
	}
	else  // figure out the number of bits required to store the maxDelta
	{
		// log2(value)
		bitsPerIndex  = 0;
		while(((unsigned)1<<bitsPerIndex)<=deltaRange)
			bitsPerIndex++;
	}
	
	// store the offset that will adjust the deltas back to their original values
	deltaOffset = -minDelta;
}

//-------------------
// this helper function walks the remaining index list and emits a 1 for the first 
// time a sequential index is seen, and a 0 otherwise.  It should be noted that all
// 0's should be emitted for indexes that are LESS than the current sequential index, or
// the index list is invalid.
static void FillOneBitBuffer(const uint32_t *indexList, uint32_t numInputIndexes, uint8_t *oneBitBuffer, int32_t *outIndexes, uint32_t &numOutIndexes)
{
	uint32_t currentMax = 0;
	uint8_t currentByte = 0;
	uint32_t currentByteBitCount = 0;
	numOutIndexes = 0;
	for (uint32_t iIndex=0; iIndex<numInputIndexes; iIndex++)
	{
		currentByte <<= 1;  // shift up to make room for the next bit
		currentByteBitCount++;

		// fill from high order bit downward
		uint32_t currentIndex = indexList[iIndex];
		if (currentIndex==currentMax)
		{
//			currentByte |= 0;   // nop
			currentMax++;
		}
		else if (currentIndex<currentMax)
		{
			// emit this as an unpredicted index
			currentByte |= 1;
			outIndexes[numOutIndexes++] = currentIndex;
		}
		else
		{
			// input index stream is malformed: you must reindex the triangle list in pre-transform-cache friendly order:
			// when an index is seen for the first time, it gets the next highest sequential position.
			EDGEERROR_F();  
		}

		// dump out the byte if it's full
		if (currentByteBitCount == 8)
		{
			oneBitBuffer[iIndex/8] = currentByte;
			currentByte = 0;
			currentByteBitCount = 0;
		}
	}

	// write out any partial bytes
	if (currentByteBitCount!=0)
	{
		currentByte <<= 8-currentByteBitCount;  // slide up to high order bits
		oneBitBuffer[numInputIndexes/8] = currentByte;
	}
}

//-------------------
// this helper function walks the triangle list and emits two bits into the twoBitBuffer for each triangle,
// either as a compressed triangle or not.  If compressed, it adds the NEW index to the outIndexes array.
// If not compressed, it emits all three indexes.
static void FillTwoBitBuffer(const uint32_t *triangles, uint32_t numTriangles, uint8_t *twoBitBuffer, uint32_t *outIndexes, uint32_t &numIndexes)
{
	// this caches the last triangle's indexes (used separate array to the initial triangle can be unmatchable)
	uint32_t lastIndexes[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

	// walk each triangle and try to match it with the previous, generating two bits per tri, and emitting 1 to 3 indexes/tri.
	uint8_t currentByte = 0;
	uint32_t currentByteBitCount = 0;
	numIndexes = 0;
	for (uint32_t iTri=0; iTri<numTriangles; iTri++)
	{
		const uint32_t *tri = triangles+iTri*3;

		// handle the two cases where we can predict the triangle's first two indexes
		if (lastIndexes[0]==tri[0] && lastIndexes[2]==tri[1])  // case 1
		{
			currentByte <<= 2;
			//currentByte |= 0;  // no need to do this, it's a nop
			outIndexes[numIndexes++] = tri[2];  // output the leftover index
		}
		else if (lastIndexes[2]==tri[0] && lastIndexes[1]==tri[1])  // case 2
		{
			// fill the bits so that they appear at the highest side of a Big Endian quadword (high bits first)
			currentByte <<= 2;  // shift the existing bits up out of the way
			currentByte |= 0x01;  // mark this as case 2
			outIndexes[numIndexes++] = tri[2];  // output the leftover index
		}
		else if (lastIndexes[1]==tri[0] && lastIndexes[0]==tri[1])  // case 3
		{
			// fill the bits so that they appear at the highest side of a Big Endian quadword (high bits first)
			currentByte <<= 2;  // shift the existing bits up out of the way
			currentByte |= 0x02;  // mark this as case 3
			outIndexes[numIndexes++] = tri[2];  // output the leftover index
		}
		else  // not compressed
		{
			currentByte <<= 2;  // shif the existing bits up out of the way
			currentByte |= 0x03;  // mark this as uncompressed
			outIndexes[numIndexes++] = tri[0];
			outIndexes[numIndexes++] = tri[1];
			outIndexes[numIndexes++] = tri[2];
		}

		// when we're done filling up a byte, output it
		currentByteBitCount += 2;
		if (currentByteBitCount==8)
		{
			twoBitBuffer[iTri/4] = currentByte;
			currentByteBitCount = 0;
			currentByte = 0;
		}

		// load up this triangle's resultant indexes 
		lastIndexes[0] = triangles[iTri*3+0];
		lastIndexes[1] = triangles[iTri*3+1];
		lastIndexes[2] = triangles[iTri*3+2];
	}

	// don't forget to output the final set of bits...
	if (currentByteBitCount!=0)
	{
		// padding bits don't apparently matter what they are, so make them 0.
		currentByte <<= 8 - currentByteBitCount;  // shift up the byte so it is in the high order
		twoBitBuffer[numTriangles/4] = currentByte;
	}
}

//-------------------
// helper function that rotates sequential triangles so that they are of the form OLD-OLD-NEW,
// where OLD indicates the index appeared in the previous triangle.
void _edgeGeomRotateTrianglesForAdjacency(uint32_t *triangles, uint32_t numTriangles)
{
	// this caches the final position of the last triangle's indexes
	uint32_t lastIndexes[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

	// this is the current triangle's indexes in this order (01201)
	// it is stored this way so we can iterate through all possible rotations by advancing a pointer
	uint32_t currentIndexes[5];
	uint32_t nextIndexes[5];
	
	for (uint32_t iTri=0; iTri<numTriangles; iTri++)
	{
		// load up the currentIndexes array with this triangle
		currentIndexes[0] = triangles[iTri*3+0];
		currentIndexes[1] = triangles[iTri*3+1];
		currentIndexes[2] = triangles[iTri*3+2];
		currentIndexes[3] = triangles[iTri*3+0];
		currentIndexes[4] = triangles[iTri*3+1];

		// check three rotations to find one that has an appropriate 'compressed' status
		uint32_t *tri = currentIndexes;
		bool matchedPrevious = false;
		for (uint32_t iRotation=0; iRotation<3; iRotation++)
		{
			// if the first two indexes of this triangle's current rotation match (0,2) or (1,2), we can compress it
			if ((lastIndexes[0]==tri[0] && lastIndexes[2]==tri[1]) ||  // good
				(lastIndexes[1]==tri[0] && lastIndexes[0]==tri[1]) ||   // good
				(lastIndexes[2]==tri[0] && lastIndexes[1]==tri[1]))   // good
			{
				triangles[iTri*3+0] = tri[0];  // store the current rotation back
				triangles[iTri*3+1] = tri[1];
				triangles[iTri*3+2] = tri[2];
				matchedPrevious = true;
				break;  // stop checking this triangle--we found a compression that works
			}

			// else... not a predictive compressed triangle.. try other rotations of this triangle
			tri++;
		}

		// if we DID NOT match the previous triangle, see if we can rotate to match the NEXT triangle better
		if ((!matchedPrevious) && ((iTri+1)<numTriangles))
		{
			nextIndexes[0] = triangles[(iTri+1)*3+0];
			nextIndexes[1] = triangles[(iTri+1)*3+1];
			nextIndexes[2] = triangles[(iTri+1)*3+2];
			nextIndexes[3] = triangles[(iTri+1)*3+0];
			nextIndexes[4] = triangles[(iTri+1)*3+1];

			// check for any edge that we can match up to the next triangle in any orientation
			uint32_t *myTri = currentIndexes;
			for (uint32_t iMyRot=0; iMyRot<3; iMyRot++)
			{
				uint32_t *nextTri = nextIndexes;
				for (uint32_t iNextRot=0; iNextRot<3; iNextRot++)
				{
					if ((myTri[0]==nextTri[0] && myTri[2]==nextTri[1]) ||  // good
						(myTri[1]==nextTri[0] && myTri[0]==nextTri[1]) ||  // good
						(myTri[2]==nextTri[0] && myTri[1]==nextTri[1]))    // good
					{
						triangles[iTri*3+0] = myTri[0];  // store the current rotation back
						triangles[iTri*3+1] = myTri[1];
						triangles[iTri*3+2] = myTri[2];
						goto NOMORE;
					}
					nextTri++;
				}
				myTri++;
			}
NOMORE:;  // C++ should have a break(count); so you can skip out of N-nested loops better.
		}
		
		// load up this triangle's resultant indexes 
		lastIndexes[0] = triangles[iTri*3+0];
		lastIndexes[1] = triangles[iTri*3+1];
		lastIndexes[2] = triangles[iTri*3+2];
	}
}

void _edgeGeomRemapTriangleList(uint32_t *triangles, uint32_t numTriangles, uint32_t largestUniqueIndex,
					   uint32_t *outReverseMapping)
{
	// Allocate a temporary lookup table (old indexes to new
	// indexes), ranging from 0 to the largest unique index.
	uint32_t vertexLookupSize = (largestUniqueIndex+1) * sizeof(uint32_t);
	uint32_t *vertexLookup = (uint32_t*)edgeGeomAlloc(vertexLookupSize);
	memset(vertexLookup, 0xFF, vertexLookupSize);

	// Iterate through this partition's triangle list, building the original index lookup table
	// and rewriting the triangle list to use the new indexes.
	uint32_t nextNewIndex = 0;
	for(uint32_t iIndex=0; iIndex<numTriangles*3; ++iIndex)
	{
		const uint32_t vertexIndex = triangles[iIndex];
		// if we haven't seen this vertex yet, assign it the next available index
		// and update both lookup tables
		if (vertexLookup[vertexIndex] == 0xFFFFFFFF)
		{
			vertexLookup[vertexIndex] = nextNewIndex;
			if (outReverseMapping != NULL)
				outReverseMapping[nextNewIndex] = vertexIndex;
			++nextNewIndex;
		}
		triangles[iIndex] = vertexLookup[vertexIndex];
	}

	edgeGeomFree(vertexLookup);
}

//-------------------

void _edgeGeomMeasureCompressedIndexTable(const uint32_t *triangles, uint32_t numTriangles, EdgeGeomIndexesFlavor indexFlavor, EdgeGeomIndexesSizes *indexesSizes)
{
	switch(indexFlavor)
	{
	case kIndexesU16TriangleListCW:
	case kIndexesU16TriangleListCCW:
		{
			const uint32_t totalIndexListSize = (sizeof(uint16_t) * (numTriangles*3) + 15) & ~15;

			indexesSizes->m_inputSize = totalIndexListSize;
			indexesSizes->m_maxTempSize = totalIndexListSize;
			break;
		}
	case kIndexesCompressedTriangleListCW:
	case kIndexesCompressedTriangleListCCW:
		{
			// Sadly, there's no quick way to determine exactly how large a compressed index table will
			// be without actually compressing it.

			// Make a temporary copy of the triangle list, sort it in-place, and strip out the duplicate elements.
			// This gives us a count of the unique indexes and the largest unique index.
			uint32_t *tempTriangles = (uint32_t*)edgeGeomAlloc(numTriangles*3*sizeof(uint32_t));
			memcpy(tempTriangles, triangles, numTriangles*3*sizeof(uint32_t));
			uint32_t numUniqueVertexes = _edgeGeomSortUniqueArrayUnsigned(tempTriangles, numTriangles*3);
			uint32_t largestIndex = tempTriangles[numUniqueVertexes-1];

			// Re-initialize the temporary triangle list to the original list, and remap it
			// in-place.  We don't need the reverse vertex mapping, so the final argument can be NULL.
			memcpy(tempTriangles, triangles, numTriangles*3*sizeof(uint32_t));
			_edgeGeomRemapTriangleList(tempTriangles, numTriangles, largestIndex, NULL);

			// Compress the remapped indexes.
			uint8_t *compressedIndexes = NULL;
			_edgeGeomCompressIndexes(tempTriangles, numTriangles, &compressedIndexes, indexesSizes);

			// We only care about the sizes, so the remapped and compressed index buffers can be
			// released.
			edgeGeomFree(tempTriangles);
			edgeGeomFree(compressedIndexes);
			break;
		}
	
	default:
		EDGEERROR_F();
	}
}

//-------------------

void _edgeGeomCompressIndexes(const uint32_t *triangles, uint32_t numTriangles, uint8_t **indexBuffer, EdgeGeomIndexesSizes *indexesSizes)
{
	// take a temporary copy that we can munge
	uint32_t *tmpTris = (uint32_t*)edgeGeomAlloc(numTriangles*3*sizeof(uint32_t));
	memcpy(tmpTris, triangles, numTriangles*3*sizeof(uint32_t));

	// First pass: rotate the triangles in place so they appear with OLD-OLD-NEW wherever possible.
	_edgeGeomRotateTrianglesForAdjacency(tmpTris, numTriangles);

	// Second pass: generate 2-bit stream and remove any compressed indexes from the temporary triangle list.
	const uint32_t twoBitBufferSize = (numTriangles*2+7)/8;  // bytes to represent all the triangles, 2 bits per tri
	uint8_t *twoBitBuffer = (uint8_t*)edgeGeomAlloc(twoBitBufferSize);
	memset(twoBitBuffer, 0, sizeof(uint8_t) * twoBitBufferSize);
	uint32_t      *tmpIndexes = (uint32_t*)edgeGeomAlloc(numTriangles*3*sizeof(uint32_t));
	uint32_t       numIndexes = 0;
	FillTwoBitBuffer(tmpTris, numTriangles, twoBitBuffer, tmpIndexes, numIndexes);

	// Third pass: generate 1-bit stream for sequential indexes and remove them from the temporary triangle list.
	const uint32_t oneBitBufferSize = (numIndexes+7)/8;  // bytes to represent all triangles, 1 bit per INDEX
	uint8_t *oneBitBuffer = (uint8_t*)edgeGeomAlloc(oneBitBufferSize);
	memset(oneBitBuffer, 0, sizeof(uint8_t) * oneBitBufferSize);
	int32_t           *finalIndexes = (int32_t*)edgeGeomAlloc(numIndexes*sizeof(int32_t));
	uint32_t       numFinalIndexes = 0;
	FillOneBitBuffer(tmpIndexes, numIndexes, oneBitBuffer, finalIndexes, numFinalIndexes);

	// Fourth pass: generate 8-index block sets of delta values by subtracting the same element from the previous block.
	uint32_t bitsPerIndex = 0;
	uint32_t deltaOffset = 0;
	ConvertIndexesToDeltas(finalIndexes, numFinalIndexes, bitsPerIndex, deltaOffset);

	// Fifth pass: actually encode the stream into a buffer
	const uint32_t deltaBlockSize = (bitsPerIndex*numFinalIndexes+7)/8;
	uint8_t *deltaBlock = (uint8_t*)edgeGeomAlloc(deltaBlockSize);
	memset(deltaBlock, 0, sizeof(uint8_t) * deltaBlockSize);
	FillDeltaBlockBuffer(finalIndexes, numFinalIndexes, deltaBlock, bitsPerIndex, deltaOffset);

	// Finally, compose the compressed output index buffer
	{
		const uint32_t baseCost = 2+2+2+1+1;
		indexesSizes->m_inputSize = (baseCost + oneBitBufferSize+twoBitBufferSize+deltaBlockSize + 15) & ~15;  // round up to 16

		//determine the maximum amount of space that will be used up in the IO buffer over the course of decompression
		{
			uint32_t compressedSize = indexesSizes->m_inputSize;
			uint32_t deltaBlockDecompressed = ((baseCost + oneBitBufferSize+twoBitBufferSize + 15) & ~15) + ((numFinalIndexes * 2 + 63) & ~63);
			uint32_t oneBitDecompressed = ((numTriangles * 6 + 15) & ~15) + 2 * ( (numIndexes + 15) & ~15);
			uint32_t copied2Bits = (oneBitDecompressed + twoBitBufferSize + 15) & ~15;

			indexesSizes->m_maxTempSize = compressedSize;

			if (deltaBlockDecompressed > indexesSizes->m_maxTempSize) {
				indexesSizes->m_maxTempSize = deltaBlockDecompressed;
			}

			if (copied2Bits > indexesSizes->m_maxTempSize) {
				indexesSizes->m_maxTempSize = copied2Bits;
			}
		}

		uint8_t *indexBufferPtr = (uint8_t*)edgeGeomAlloc(indexesSizes->m_inputSize);
		memset(indexBufferPtr, 0, indexesSizes->m_inputSize);
		*indexBuffer = indexBufferPtr;  // store the return pointer in the caller's space

		// store the num indexes in the delta buffer, in 2-byte big-endian format
		_edgeGeomWriteShort((uint16_t)numFinalIndexes, indexBufferPtr, _EDGE_GEOM_WRITE_BIG_ENDIAN);
		indexBufferPtr += sizeof(uint16_t);
		
		// then, store the 2-bit buffer size, in 2-byte big-endian format
		_edgeGeomWriteShort((uint16_t)deltaOffset, indexBufferPtr, _EDGE_GEOM_WRITE_BIG_ENDIAN);
		indexBufferPtr += sizeof(uint16_t);

		// store the 1-bit buffer size, in 2-byte big-endian format
		_edgeGeomWriteShort((uint16_t)oneBitBufferSize, indexBufferPtr, _EDGE_GEOM_WRITE_BIG_ENDIAN);
		indexBufferPtr += sizeof(uint16_t);

		// write out the bits per index of the delta buffer
		*indexBufferPtr++ = (uint8_t)(bitsPerIndex & 0xff);
		*indexBufferPtr++ = 0;  // padding byte

		// copy the 1-bit buffer
		memcpy(indexBufferPtr, oneBitBuffer, oneBitBufferSize);
		indexBufferPtr += oneBitBufferSize;

		// copy the 2-bit buffer
		memcpy(indexBufferPtr, twoBitBuffer, twoBitBufferSize);
		indexBufferPtr += twoBitBufferSize;
		
		// lastly, copy the delta buffer
		memcpy(indexBufferPtr, deltaBlock, deltaBlockSize);
	}

	edgeGeomFree(deltaBlock);
	edgeGeomFree(oneBitBuffer);
	edgeGeomFree(twoBitBuffer);
	edgeGeomFree(finalIndexes);
	edgeGeomFree(tmpIndexes);
	edgeGeomFree(tmpTris);
}

//-------------------
