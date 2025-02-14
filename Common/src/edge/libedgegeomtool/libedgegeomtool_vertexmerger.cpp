/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "edge/libedgegeomtool/libedgegeomtool_wrap.h"
#include "edge/libedgegeomtool/libedgegeomtool_internal.h" // for _edgeGeomSortUniqueArrayUnsigned()

#include <math.h>
#include <stdio.h>
#include <string.h>

//-------------------

static bool IsVertexEqual(const EdgeGeomScene &edgeScene, uint32_t indexA, uint32_t indexB)
{
	// see if we can find a matching base vertex
	const uint32_t floatsPerBaseVertex = edgeScene.m_numFloatsPerVertex;
	const float *vertexA = edgeScene.m_vertexes + floatsPerBaseVertex*indexA;
	const float *vertexB = edgeScene.m_vertexes + floatsPerBaseVertex*indexB;
	if (memcmp(vertexA, vertexB, sizeof(float)*floatsPerBaseVertex) != 0)
	{
		return false;
	}

	// Check skinning weights and indices, if present
	if (edgeScene.m_skinningWeightsPerVertex != NULL)
	{
		float *skinWeightsA = edgeScene.m_skinningWeightsPerVertex + 4*indexA;
		float *skinWeightsB = edgeScene.m_skinningWeightsPerVertex + 4*indexB;
		if (memcmp(skinWeightsA, skinWeightsB, 4*sizeof(float)) != 0)
		{
			return false;
		}
	}
	if (edgeScene.m_matrixIndexesPerVertex != NULL)
	{
		int32_t *boneIndicesA = edgeScene.m_matrixIndexesPerVertex + 4*indexA;
		int32_t *boneIndicesB = edgeScene.m_matrixIndexesPerVertex + 4*indexB;
		if (memcmp(boneIndicesA, boneIndicesB, 4*sizeof(int32_t)) != 0)
		{
			return false;
		}
	}

	// check the blend shape deltas, if present
	const uint32_t floatsPerBlendShapeDelta = edgeScene.m_numFloatsPerDelta;
	for (uint32_t iBlendShape=0; iBlendShape<edgeScene.m_numBlendShapes; iBlendShape++)
	{
		const float *blendShapeA = edgeScene.m_vertexDeltas + (edgeScene.m_numVertexes*iBlendShape + indexA) * floatsPerBlendShapeDelta;
		const float *blendShapeB = edgeScene.m_vertexDeltas + (edgeScene.m_numVertexes*iBlendShape + indexB) * floatsPerBlendShapeDelta;
		if (memcmp(blendShapeA, blendShapeB, sizeof(float)*floatsPerBlendShapeDelta)!=0)
		{
			return false;
		}
	}

	// Identical!
	return true;
}

//-------------------

uint32_t edgeGeomMergeIdenticalVertexes(EdgeGeomScene &edgeScene)
{
	const uint32_t numIndexes = edgeScene.m_numTriangles*3;

	// run through all of the triangle indexes and come up with a list of vertexes that are actually used.
	// This approach allows this function to scale independently of where in the pipeline the merging is done,
	// whether before or after partitioning the triangle list by shaders or skinning, etc.
	uint32_t *indexesInUse = (uint32_t*)edgeGeomAlloc(numIndexes*sizeof(uint32_t));
	memcpy(indexesInUse, edgeScene.m_triangles, sizeof(uint32_t)*numIndexes);
	
	uint32_t numUniqueIndexes = _edgeGeomSortUniqueArrayUnsigned(indexesInUse, numIndexes);
	if (numUniqueIndexes==0)  // avoid memory corruption when the scene is empty
	{
		edgeGeomFree(indexesInUse);
		return 0;
	}
		
	// make a table that maps from triangleIndex -> sharedTriangleIndex, where the shared index has the same data
	// as the triangleIndex.
	uint32_t *equalityRemapping = (uint32_t*)edgeGeomAlloc( (indexesInUse[numUniqueIndexes-1]+1) * sizeof(uint32_t) );
	memset(equalityRemapping, 0xffffffff, sizeof(uint32_t)*(indexesInUse[numUniqueIndexes-1]+1));

	// creating a closed hash is fine because we know the maximum number of entries (assuming ALL 
	// vertexes are splits) is numUniqueIndexes.  So, what we do is use the combined set of base 
	// floats for a vertex and add them together, then use that as the hash key, modulo the
	// size of the table.  Then, we sequentially search for A) a match, or B) an unused cell in 
	// the hash table.  Degenerate behavior occurs if the table is very full, but I don't anticipate that.
	const uint32_t hashTableSize = numUniqueIndexes*3+1;  // *3 to provide plenty of gaps
	uint32_t *hashTable = (uint32_t*)edgeGeomAlloc(hashTableSize*sizeof(uint32_t));
	memset(hashTable, 0xffffffff, sizeof(uint32_t)*hashTableSize);

	// now, indexesInUse[0..numUniqueIndexes) holds the set of indexes we should consider for merging, 
	// since they are the ones that affect the triangle list.
	uint32_t       equalityCounter = 0;
	const uint32_t floatsPerBaseVertex = edgeScene.m_numFloatsPerVertex;
	for (uint32_t iIndex=0; iIndex<numUniqueIndexes; iIndex++)
	{
		// compute the hash key for this index's data
		uint32_t dataSum = 0;
		for (uint32_t iData=0; iData<floatsPerBaseVertex; iData++)
		{
			dataSum = dataSum + ((dataSum>>16) ^ (dataSum<<8) ^ *(uint32_t *)(edgeScene.m_vertexes + floatsPerBaseVertex*indexesInUse[iIndex] + iData));
		}
		const uint32_t hashKey = (*(uint32_t*)&dataSum) % hashTableSize;  // treat the bit pattern as an uint32_t
		
		// start searching for a match, or stop when an entry is empty
		for (uint32_t iSearch=0; iSearch<hashTableSize; iSearch++)
		{
			// this allows us to search the whole table, and wrap around easily
			const uint32_t hashIndex = (hashKey + iSearch) % hashTableSize;
			
			if (hashTable[hashIndex]==0xffffffff) // unused entry... this is a new vertex
			{
				hashTable[hashIndex] = iIndex;  // store the index into the indexesInUse array.. might come in handy?
				equalityRemapping[indexesInUse[iIndex]] = indexesInUse[iIndex];  // map all 'real' vertexes to themselves
				break;
			}
			else  // check to see if the vertex stored at this point in the hash table is equal to us
			{
				if (IsVertexEqual(edgeScene, indexesInUse[iIndex], indexesInUse[hashTable[hashIndex]]))
				{
					// map this vertex to the one we're equal to
					equalityRemapping[indexesInUse[iIndex]] = indexesInUse[hashTable[hashIndex]];  
					equalityCounter++;
//					printf("Merging %d -> %d\n", indexesInUse[iIndex], indexesInUse[hashTable[hashIndex]]);
					break;  // skip looking for any more equal vertexes... we found it already
				}
			}
		}
	}

	// Rewrite the triangle list once with only shared indexes.
	for (uint32_t iInd=0; iInd<numIndexes; iInd++)
	{
		const uint32_t sharedVertexIndex = equalityRemapping[edgeScene.m_triangles[iInd]];
		EDGEASSERT(sharedVertexIndex!=0xffffffff);  // error in algorithm if this happens.. something didn't get mapped at all
		edgeScene.m_triangles[iInd] = sharedVertexIndex;
	}
	
	// clean up
	edgeGeomFree(indexesInUse);
	edgeGeomFree(hashTable);
	edgeGeomFree(equalityRemapping);

	return equalityCounter;
}
