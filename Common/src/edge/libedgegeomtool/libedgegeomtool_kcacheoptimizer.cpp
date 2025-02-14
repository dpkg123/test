/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <limits.h>
#include <float.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "edge/libedgegeomtool/libedgegeomtool_internal.h"

//-------------------

const uint32_t kFifoSize = 23;
const uint32_t kLruSize  = 4;

//-------------------
/// This function figures out how big of an array we need to set aside for vertex-related data.
static uint32_t FindHighestVertexIndex(const uint32_t *triangles, uint32_t numTriangles)
{
	uint32_t highest = 0;
	const uint32_t numIndexes = numTriangles*3;
	for (uint32_t i=0; i<numIndexes; i++)
	{
		if (triangles[i] > highest)
		{
			highest = triangles[i];
		}
	}
	return highest;
}

//-------------------
/// This function walks all the triangles and constructs a quick lookup that goes from vertex to faces it is contained within.
static void FindAdjacentTriangles(const uint32_t *triangles, const uint32_t numTriangles, uint32_t *adjacentFacesPerVertex, 
									uint32_t *adjacencyDataStarts, const uint32_t numVertexes)
{
	// first, clear the data structures to zero or invalid
	memset(adjacencyDataStarts, 0, sizeof(uint32_t)*numVertexes);
	memset(adjacentFacesPerVertex, 0xff, sizeof(uint32_t)*numTriangles*3);

	// handle degenerate triangles in a special way... we NEED them to retain the same structure so we know what triangle
	// they came from, so we can't remove degenerate triangles, nor can we just remove the redundant index from the list (it 
	// throws off the triangle indexing (ie vertexIndex/3==triangleIndex)), so let's detect degenerates and substitute 0xFFFFFFFF
	// for the redundant index in a triangle, and handle it with care below, so we don't double count indices or adjacencies.
	const uint32_t numIndexes = numTriangles * 3;
	uint32_t *treatedTriangleList = (uint32_t*)edgeGeomAlloc(numIndexes*sizeof(uint32_t));
	for (uint32_t i=0; i<numTriangles; i++)
	{
		const uint32_t v0 = triangles[i*3+0];  // always accept the first index		
		uint32_t       v1 = triangles[i*3+1];
		uint32_t       v2 = triangles[i*3+2];

		// change any index that is obviously equal to a previous one in the same triangle
		if (v1==v0)
			v1 = 0xFFFFFFFF;
		if (v2==v0 || v2==v1)
			v2 = 0xFFFFFFFF;

		treatedTriangleList[i*3+0] = v0;  // just store whatever the results are
		treatedTriangleList[i*3+1] = v1;
		treatedTriangleList[i*3+2] = v2;
	}

	// now, walk each index in the triangle list and add up how many times we see each one.  This first pass is quick.
	for (uint32_t i=0; i<numIndexes; i++)
	{
		// this is off by one, so that [0]==0, and [1] is the # of times 0 was seen.  This makes summing them simpler.
		const uint32_t vertIndex = treatedTriangleList[i];
		if (vertIndex != 0xFFFFFFFF)
			adjacencyDataStarts[vertIndex+1]++;
	}

	// Rewrite the adjacencyDataStarts so it is an offset into the adjacentFacesPerVertex array, such that 
	// adjacencyDataStarts[N] is the index where vertex N's triangle indexes begin, and adjacencyDataStarts[N+1] are 
	// where the next one begins.
	for (uint32_t i=1; i<numVertexes; i++)
	{
		adjacencyDataStarts[i] += adjacencyDataStarts[i-1];
	}

	// Finally, walk the triangle list one last time and insert the triangle indexes into their respective sub-arrays.
	for (uint32_t i=0; i<numIndexes; i++)
	{
		const uint32_t vertexIndex = treatedTriangleList[i];
		if (vertexIndex==0xFFFFFFFF)
			continue;

		uint32_t unusedSlot = adjacencyDataStarts[vertexIndex];
		for (; unusedSlot<adjacencyDataStarts[vertexIndex+1]; unusedSlot++)
		{
			if (adjacentFacesPerVertex[unusedSlot]==0xFFFFFFFF)  // use this empty slot
				break;
		}
		EDGEASSERT(unusedSlot<adjacencyDataStarts[vertexIndex+1]);
		EDGEASSERT(unusedSlot<numIndexes);

		// store the triangle number in this vertex's adjacent face list
		adjacentFacesPerVertex[unusedSlot] = i/3;
	}
	
	edgeGeomFree(treatedTriangleList);
}

//-------------------
// This function is essentially the same as push index, except that it pushed 3 indexes at a time,
// which makes the operation *much* simpler and faster, rather than doing so many searches.  
// Also, the LRU is not overwritten, instead a modified version is written
// into the destLru, so calls to PushTriangle are non-destructive.  The return value is the total cost
// of pushing the triangle, in terms of LRU misses and FIFO misses.  This function counts LRU and FIFO misses
// exactly and independently.  So, a FIFO miss should be the cost of the vertex program, LRU miss should
// only be the cost of populating the mini-cache from the FIFO.  Final note: This routine does NOT reorder
// vertexes in a triangle, because it is used not only to place triangle and score them, but in post-processing
// to determine the number of cache hits and misses, so it must reflect exactly what the hardware does.
static float PushTriangle(uint32_t **fifoStart, uint32_t **fifoEnd, const uint32_t *srcLru, uint32_t *destLru, const uint32_t *triIndex, float kFifoMiss, float kLruMiss)
{
	uint32_t *current = *fifoStart;
	uint32_t *end     = *fifoEnd;

	// count LRU and FIFO hits/misses
	uint32_t lruMisses = 0;
	uint32_t fifoMisses = 0;

	// we want to inspect the end as well, so the loop goes until it passes the end
	uint32_t position[3] = { 0,0,0 };
	uint32_t counter = 1;
	for (; current<=end; current++, counter++)
	{
		const uint32_t curIndex = *current;
		if (curIndex == triIndex[0])
			position[0] = counter;
		if (curIndex == triIndex[1])
			position[1] = counter;
		if (curIndex == triIndex[2])
			position[2] = counter;
	}

	// LRU will trump anything the fifo says, so check in there.  Update LRU as we go, too.
	memcpy(destLru, srcLru, sizeof(uint32_t)*kLruSize);
	for (uint32_t iVert=0; iVert<3; iVert++)
	{
		// check to see if it's already in the LRU.  If so, move it to the front
		for (uint32_t i=0; i<kLruSize; i++)
		{
			if (destLru[i]==triIndex[iVert])  // yes, move to front!
			{
				// push everything back 1 step
				for (uint32_t iOuter=i; iOuter<kLruSize-1; iOuter++)
				{
					destLru[iOuter] = destLru[iOuter+1];
				}
				destLru[kLruSize-1] = triIndex[iVert];  // put it at the top
				position[iVert] = kFifoSize;  // prevent this getting a FIFO miss
				break;
			}
		}
		
		// It wasn't in the LRU, so add it and count the miss
		if (destLru[kLruSize-1]!=triIndex[iVert])
		{
			// push everything back 1 step
			for (uint32_t iOuter=0; iOuter<kLruSize-1; iOuter++)
			{
				destLru[iOuter] = destLru[iOuter+1];
			}
			destLru[kLruSize-1] = triIndex[iVert];  // put it at the top
			lruMisses++;
		}
	}

	// push any vertexes that need it
	if (position[0]<3 || position[1]<3 || position[2]<3)
	{
		bool notDone = true;
		while (notDone)
		{
			notDone = false;

			// loop over the vertexes and make sure they haven't fallen off the FIFO
			for (uint32_t iVertex=0; iVertex<3; iVertex++)
			{
				if (position[iVertex]<=fifoMisses)
				{
					(*fifoStart)++;
					(*fifoEnd)++;
					**fifoEnd = triIndex[iVertex];
					fifoMisses++;
					notDone = true;
					position[iVertex] = kFifoSize;  // do not allow this to count more than once
				}
			}
		}
	}

	EDGEASSERT(fifoMisses<=3 && lruMisses<=3);  // this can NEVER be higher than 3.
	return fifoMisses*kFifoMiss + lruMisses*kLruMiss;
}

//-------------------
/// This function exists because the need to rotate a triangle to best match the LRU is important code
/// that would otherwise be repeated endlessly.  Note, it MODIFIES the triIndex so the best rotation
/// is accessible to the caller afterwards.
static float RotateAndPushTriangle(uint32_t **fifoStart, uint32_t **fifoEnd, const uint32_t *srcLru, uint32_t *destLru, uint32_t *triIndex, float kFifoMiss, float kLruMiss)
{
	// this forms a complete rotation of the triangle (01201)
	uint32_t newTri[5] = { triIndex[0], triIndex[1], triIndex[2], triIndex[0], triIndex[1] };
	
	uint32_t *bestTri = newTri;
	uint32_t bestScore = 0;

	// the easiest way to find the best rotation is to try all three and score them
	// based on the age of the LRU hits, where the first vertex hitting an old LRU value is rated highest,
	// and the third vertex hitting the newest LRU value is rated lowest
	uint32_t *tri = newTri;
	for (uint32_t iRotation=0; iRotation<3; iRotation++)
	{
		// count up the score per vertex as we find it in the LRU
		uint32_t score = 0;
		for (uint32_t iVertex=0; iVertex<3; iVertex++)
		{
			// lru[0] is the oldest entry, and lru[kLruSize-1] is the newest
			for (uint32_t iLru=0; iLru<kLruSize; iLru++)
			{
				if (srcLru[iLru]==tri[iVertex])
				{
					// point allotment table
					score += (kLruSize-iLru) * (3-iVertex);  
					break;
				}
			}
		}
		
		// remember the best triangle and its score
		if (score>bestScore)
		{
			bestTri = tri;
			bestScore = score;
		}
	
		tri++;  // rotate the triangle
	}

	// remember this order
	triIndex[0] = bestTri[0];
	triIndex[1] = bestTri[1];
	triIndex[2] = bestTri[2];

	return PushTriangle(fifoStart, fifoEnd, srcLru, destLru, triIndex, kFifoMiss, kLruMiss);
}

//-------------------
/// This simply counts the triangles that a vertex is related to that are not yet rendered.  It may return zero, in which
/// case a vertex should be colored black.
static uint32_t CountAdjacentUnrenderedTriangles(int8_t const *triangleIsAvailable, const uint32_t *relatedTriangles, uint32_t numTriangles)
{
	uint32_t count = 0;
	for (uint32_t iTri=0; iTri<numTriangles; iTri++)
	{
		const uint32_t triangleIndex = relatedTriangles[iTri];
		if (triangleIsAvailable[triangleIndex])
		{
			count++;
		}
	}
	return count;
}

//-------------------
// This function does all the real work.  It takes a focus vertex and 'renders' all of its adjacent triangles in an orderly fashion.
// It determines the order of triangles to render by prioritizing them by the age of their vertexes in the fifo, initially.  As each
// triangle's vertexes are pushed into the fifo, an adjacent triangle (using 2 or more of the same verts) is rendered.  When no triangle matches
// that criteria, the next triangle is selected by age of vertexes in the fifo.  Note, this process may require pushing more than one 
// vertex back into the fifo because any of them may cycle off if the valence of the index is high enough, or if it was old in the
// fifo to begin with.  Returns the number of cache misses/pushes.  If the flag at the end (commitChanges) is false, don't change
// the triangleIsAvailable structure.
static float PushFocusVertex(uint32_t **head, uint32_t **tail, uint32_t *lru, int8_t *triangleIsAvailable, const uint32_t *adjacentTriangles, uint32_t numAdjacentTriangles, const uint32_t *inTriangles, uint32_t **outTriangles, float kFifoMiss, float kLruMiss, bool commitChanges)
{
	uint32_t *triangles = (uint32_t *)edgeGeomAlloc(numAdjacentTriangles*sizeof(uint32_t));  // quickly allocate memory on the stack

	// determine the triangles and vertexes we need to concentrate on
	uint32_t numUsableTriangles = 0;
	for (uint32_t iTri=0; iTri<numAdjacentTriangles; iTri++)
	{
		const uint32_t triangleIndex = adjacentTriangles[iTri];
		if (triangleIsAvailable[triangleIndex])
		{
			// copy out the triangle id
			triangles[numUsableTriangles] = triangleIndex;
			numUsableTriangles++;
		}
	}
	EDGEASSERT(numUsableTriangles != 0);

	// this is the "real" lru as far as this routine is concerned.
	uint32_t simLru[kLruSize];
	uint32_t tmpLru[kLruSize];
	memcpy(simLru, lru, sizeof(uint32_t)*kLruSize);

	// this loop simply finds the cheapest triangle to push into the triangle list and 
	// does it until it runs out of triangles.  The k-cache paper says to go in a specific
	// order, which makes sense in a pure fifo system, but with an LRU, you optimize for that instead.
	float totalCost = 0;
	while (numUsableTriangles > 0)
	{
		// now, pick the triangle that needs to go next, based on minimum cost
		uint32_t bestTriangle  = UINT_MAX;
		float    bestScore = FLT_MAX;
		uint32_t currVerts[3];
		for (uint32_t iTri=0; iTri<numUsableTriangles; iTri++)
		{
			const uint32_t triangleIndex = triangles[iTri];

			// calculate the cost for a single face
			uint32_t *currentHead = *head;
			uint32_t *currentTail = *tail;

			// copy the verts, so they can be reordered
			currVerts[0] = inTriangles[triangleIndex*3+0];
			currVerts[1] = inTriangles[triangleIndex*3+1];
			currVerts[2] = inTriangles[triangleIndex*3+2];
			
			// get the score for pushing a triangle.  It also rotates the vertexes in-place to the best orientation.
			float triangleScore = RotateAndPushTriangle(&currentHead, &currentTail, simLru, tmpLru, currVerts, kFifoMiss, kLruMiss);
			if (triangleScore<bestScore)
			{
				bestScore = triangleScore;
				bestTriangle  = triangleIndex;
			}
		}
		EDGEASSERT(bestTriangle != UINT_MAX);

		// push the best face onto the fifo, modifying it and the tmpLRU
		currVerts[0] = inTriangles[bestTriangle*3+0];
		currVerts[1] = inTriangles[bestTriangle*3+1];
		currVerts[2] = inTriangles[bestTriangle*3+2];
		totalCost += RotateAndPushTriangle(head, tail, simLru, tmpLru, currVerts, kFifoMiss, kLruMiss);
		memcpy(simLru, tmpLru, sizeof(uint32_t)*kLruSize);  // keep this LRU setting
		
		if (commitChanges)  // actually change external data structures
		{
			triangleIsAvailable[bestTriangle] = 0;
			*((*outTriangles)++) = currVerts[0];  // output the rotated version of this triangle which matches the LRU well
			*((*outTriangles)++) = currVerts[1];
			*((*outTriangles)++) = currVerts[2];
		}
		
		// run through the usable triangles and delete the bestTriangle from the array
		bool deletedIt = false;
		for (uint32_t iTri=0; iTri<numUsableTriangles; iTri++)
		{
			if (triangles[iTri]==bestTriangle)
			{
				triangles[iTri] = triangles[numUsableTriangles-1];  // delete by copying the last element over it
				numUsableTriangles--;
				deletedIt = true;
				break;
			}
		}
		EDGEASSERT(deletedIt);
	}

	// commit the LRU at the end, if desired
	if (commitChanges)
	{
		memcpy(lru, simLru, sizeof(uint32_t)*kLruSize);
	}

	edgeGeomFree(triangles);

	return totalCost;
}

//-------------------
/// This function walks through the list of vertex ids (either the LRU array or the FIFO array) and determines 
/// the cost of rendering each non-black vertex's fan of triangles.  The cheapest one is returned.  Be aware, it is
/// entirely possible that a UINT_MAX is returned instead, because no verts are in the pipe yet.
static void ChooseBestVertex(const uint32_t *first, const uint32_t *last, float &bestScore, uint32_t &bestIndex,
				const uint32_t *adjacentTrianglesPerVertex, const uint32_t *adjacencyDataStarts, int8_t *triangleIsAvailable, 
				uint32_t *fifoStart, uint32_t *fifoEnd, uint32_t *lru, const uint32_t *inTriangles,
				float kFifoMiss, float kLruMiss, float k1, float k2, float k3)
{
	for (const uint32_t *current = first; current<=last; current++)
	{
		const uint32_t currentIndex = *current;
		if (currentIndex!=UINT_MAX)  // ignore invalid entries in the LRU (it's initialized to this)
		{
			// find the number of adjacent triangles (C2)
			const uint32_t *currentAdjacentTriangles = adjacentTrianglesPerVertex + adjacencyDataStarts[currentIndex];
			uint32_t numCurrentAdjacentTriangles = adjacencyDataStarts[currentIndex+1]-adjacencyDataStarts[currentIndex];
			const uint32_t numAdjacentUnrenderedTriangles = CountAdjacentUnrenderedTriangles(triangleIsAvailable, currentAdjacentTriangles, numCurrentAdjacentTriangles);
			
			// skip any vertex that is black
			if (numAdjacentUnrenderedTriangles==0)
				continue;
			
			// count up the number of pushes adding this focus vertex takes (C1)
			uint32_t *head = fifoStart;
			uint32_t *tail = fifoEnd;
			float const totalCost = PushFocusVertex(&head, &tail, lru, triangleIsAvailable, currentAdjacentTriangles, numCurrentAdjacentTriangles, inTriangles, NULL, kFifoMiss, kLruMiss, false);

			// finally, since we know this vertex turned black now, let's find out where it is in the buffer (C3)
			uint32_t distance = 0;
			for (; head<=tail; head++, distance++)
			{
				if (*head==currentIndex)
					break;
			}

			// calculate the final score for this vertex
			float const currentScore = k1*totalCost/(kFifoMiss+kLruMiss) - k2*(float)numAdjacentUnrenderedTriangles + k3*distance/(float)kFifoSize;
			if (currentScore < bestScore)
			{
				bestIndex = currentIndex;
				bestScore = currentScore;
			}
		}
	}
}

//-------------------
/// This routine is a K-Cache optimizer.  It is not sensitive to the indexes of the input triangles,
/// so remapping them to a low-value doesn't impact performance.
void edgeGeomKCacheOptimizer(const uint32_t *inTriangles, uint32_t numTriangles, void *userData, uint32_t *outTriangles)
{
	EdgeGeomKCacheOptimizerUserData *actualUserData = (EdgeGeomKCacheOptimizerUserData*)userData;
	const float kFifoMiss = actualUserData->m_fifoMissCost;
	const float kLruMiss = actualUserData->m_lruMissCost;
	const float k1 = actualUserData->m_k1;
	const float k2 = actualUserData->m_k2;
	const float k3 = actualUserData->m_k3;

	memset(outTriangles, 0xff, sizeof(uint32_t)*numTriangles*3);

	// first of all, we need to figure out the size of the adjacency list
	const uint32_t numIndexes         = numTriangles * 3;
	const uint32_t highestVertexIndex = FindHighestVertexIndex(inTriangles, numTriangles);

	// Okay, let's make some auxiliary data structures so we can find adjacencies easily.
	uint32_t *adjacencyDataStarts    = (uint32_t*)edgeGeomAlloc((highestVertexIndex+2)*sizeof(uint32_t));  // one extra to get rid of the 'last element' special case.
	uint32_t *adjacentTrianglesPerVertex = (uint32_t*)edgeGeomAlloc(numIndexes*sizeof(uint32_t));

	// dump the data into the array now that it's the right size
	FindAdjacentTriangles(inTriangles, numTriangles, adjacentTrianglesPerVertex, adjacencyDataStarts, highestVertexIndex+2);  

	// The adjacency data structure is not modified as faces are used, because it would be too slow.
	// Instead, each entry in this array is either zero or one, depending on whether the face is 
	// available (1) to be rendered or not (0).
	int8_t *triangleIsAvailable = (int8_t*)edgeGeomAlloc(numTriangles*sizeof(int8_t));
	memset(triangleIsAvailable, 1, sizeof(int8_t)*numTriangles);

	// Here is the list of unique indexes, which shrinks as the verts are turned black (completely used).
	// It is only used in the case when no vertex in the fifo is non-black, so this saves time by allowing
	// us to traverse them directly.  Since it is sorted, removing an entry when it goes black is a binary search
	// to find it, followed by a memmove to delete it.
	uint32_t *nonBlackIndexes = (uint32_t*)edgeGeomAlloc(numIndexes*sizeof(uint32_t));
	memcpy(nonBlackIndexes, inTriangles, sizeof(uint32_t)*numIndexes);
	uint32_t numIndexesLeft = _edgeGeomSortUniqueArrayUnsigned(nonBlackIndexes, numIndexes);

	// Make some scratch pad area so that we can chase down and mark vertexes that should be black
	uint32_t *potentialBlackIndexes = (uint32_t*)edgeGeomAlloc(numIndexes*sizeof(uint32_t));

//	printf("Total unique vertexes (%d)\n", numIndexesLeft);

	// this could be anything, so long as it's definitely bigger than a single focus vertex can push on in a single go.
	// The reason is, the fifo will get indexes pushed onto it, then get reset to its previous state many times before
	// anything is "committed" to the fifo.  So, we leave the intial 24 verts alone and just push more onto the end,
	// and reset some pointers to go back to the initial state when trying the next focus vertex.
	uint32_t fifo[1000];
	uint32_t* const fifoStart = fifo;
	uint32_t* const fifoEnd   = &fifo[kFifoSize-1];
	memset(fifo, 0xff, sizeof(uint32_t)*1000);  // initialize the fifo to unmatchable indexes

	// LRU primitive cache is the first thing tested before FIFO
	uint32_t lru[kLruSize];
	memset(lru, 0xff, sizeof(uint32_t)*kLruSize);

	// This loop continues until all indexes are black (completely used).
	uint32_t *outTris = outTriangles;  // this is the current pointer to the output triangle list
	float totalCost = 0;  // this is the total cost of cache misses in the model
	while (numIndexesLeft)
	{
		// first, we select a best vertex by running through the existing fifo and measure the cost of 
		float    bestScore = FLT_MAX;
		uint32_t bestIndex = UINT_MAX;

		// Look through the LRU for vertexes that are not black and could be rendered.  If they are all 
		// black, look through the FIFO.  Take the vertex with the lowest cost.
		ChooseBestVertex(lru, &lru[kLruSize-1], bestScore, bestIndex, adjacentTrianglesPerVertex, adjacencyDataStarts, triangleIsAvailable, fifoStart, fifoEnd, lru, inTriangles, kFifoMiss, kLruMiss, k1, k2, k3);
		if (bestIndex==UINT_MAX)  // check FIFO next
		{
			ChooseBestVertex(fifoStart, fifoEnd, bestScore, bestIndex, adjacentTrianglesPerVertex, adjacencyDataStarts, triangleIsAvailable, fifoStart, fifoEnd, lru, inTriangles, kFifoMiss, kLruMiss, k1, k2, k3);
		}

		// If no vertex is decidedly best, pick the lowest-degree vertex out of the non-black verts
		if (bestIndex==UINT_MAX)
		{
			// count the faces left on each vertex, and pick the one with the fewest faces
			uint32_t lowestDegree = UINT_MAX;
			for (uint32_t iVertex=0; iVertex<numIndexesLeft; iVertex++)
			{
				const uint32_t currentIndex = nonBlackIndexes[iVertex];
				const uint32_t *currentAdjacentTriangles = adjacentTrianglesPerVertex + adjacencyDataStarts[currentIndex];
				uint32_t numCurrentAdjacentTriangles = adjacencyDataStarts[currentIndex+1]-adjacencyDataStarts[currentIndex];
				const uint32_t numAdjacentUnrenderedTriangles = CountAdjacentUnrenderedTriangles(triangleIsAvailable, currentAdjacentTriangles, numCurrentAdjacentTriangles);
				
				if (numAdjacentUnrenderedTriangles < lowestDegree)
				{
					// this can only fail if there's a bug that does not properly remove black vertexes from the nonBlackIndexes array
					EDGEASSERT(numAdjacentUnrenderedTriangles != 0);  
					lowestDegree = numAdjacentUnrenderedTriangles;
					bestIndex = currentIndex;
				}
			}
		}

		// there must be a vertex to add by now
		EDGEASSERT(bestIndex != UINT_MAX);
		
		// add the vertex and all its related triangles
		uint32_t *head = fifoStart;
		uint32_t *tail = fifoEnd;
		const uint32_t *adjacentTriangles = adjacentTrianglesPerVertex + adjacencyDataStarts[bestIndex];
		uint32_t numAdjacentTriangles = adjacencyDataStarts[bestIndex+1]-adjacencyDataStarts[bestIndex];

		// copy out the vertex indexes that may be going black
		uint32_t numPotentialBlackIndexes = 0;
		for (uint32_t iTri=0; iTri<numAdjacentTriangles; iTri++)
		{
			const uint32_t triangleIndex = adjacentTriangles[iTri];
			if (triangleIsAvailable[triangleIndex])
			{
				potentialBlackIndexes[numPotentialBlackIndexes++] = inTriangles[triangleIndex*3+0];
				potentialBlackIndexes[numPotentialBlackIndexes++] = inTriangles[triangleIndex*3+1];
				potentialBlackIndexes[numPotentialBlackIndexes++] = inTriangles[triangleIndex*3+2];
			}
		}

		// actually push the fan of faces now
		totalCost += PushFocusVertex(&head, &tail, lru, triangleIsAvailable, adjacentTriangles, numAdjacentTriangles, inTriangles, &outTris, kFifoMiss, kLruMiss, true);

		// run through all the vertexes related to the faces related to this bestVertex
		const uint32_t numUniquePotentialBlackIndexes = _edgeGeomSortUniqueArrayUnsigned(potentialBlackIndexes, numPotentialBlackIndexes);
		for (uint32_t iIndex=0; iIndex<numUniquePotentialBlackIndexes; iIndex++)
		{
			const uint32_t currentIndex = potentialBlackIndexes[iIndex];
			const uint32_t *currentAdjacentTriangles = adjacentTrianglesPerVertex + adjacencyDataStarts[currentIndex];
			uint32_t numCurrentAdjacentTriangles = adjacencyDataStarts[currentIndex+1]-adjacencyDataStarts[currentIndex];
			const uint32_t numAdjacentUnrenderedTriangles = CountAdjacentUnrenderedTriangles(triangleIsAvailable, currentAdjacentTriangles, numCurrentAdjacentTriangles);

			// this vertex is black (and it could NOT be black before, because it was just used!)
			if (numAdjacentUnrenderedTriangles == 0)
			{
				// remove it from the nonBlackIndexes array
				uint32_t *ptr = (uint32_t *)bsearch(&currentIndex, nonBlackIndexes, numIndexesLeft, sizeof(uint32_t), _edgeGeomIsEqualUnsigned);
				EDGEASSERT(ptr!=NULL);
				memmove(ptr, ptr+1, sizeof(uint32_t)*(nonBlackIndexes + numIndexesLeft - (ptr+1)));
				numIndexesLeft--;
			}
		}

		// move the fifo back to the start region again, so we don't overrun our static buffer
		uint32_t offsetHead = (uint32_t) (head-fifo);
		EDGEASSERT((offsetHead+kFifoSize)<=(uint32_t)(sizeof(fifo)/sizeof(fifo[0])));
		for(uint32_t i=0; i<kFifoSize; i++)
		{
			fifo[i]=fifo[i+offsetHead];
		}
		for(uint32_t i=kFifoSize; i<(uint32_t)(sizeof(fifo)/sizeof(fifo[0])); i++)
		{
			fifo[i]=UINT_MAX;
		}
	}

	edgeGeomFree(potentialBlackIndexes);
	edgeGeomFree(nonBlackIndexes);
	edgeGeomFree(triangleIsAvailable);
	edgeGeomFree(adjacentTrianglesPerVertex);
	edgeGeomFree(adjacencyDataStarts);

#if defined(_DEBUG) && 0
	// sanity check the remapping code
	uint32_t sumPre = 0;
	uint32_t sumPost = 0;
	for (uint32_t iIndex=0; iIndex<numTriangles*3; iIndex++)
	{
		sumPre += inTriangles[iIndex];
		sumPost += outTriangles[iIndex];
	}
	if (sumPre!=sumPost)
	{
		// search for non-rotated version of each input triangle, since this algorithm doesn't rotate the triangle (yet)
		for (uint32_t iTri=0; iTri<numTriangles; iTri++)
		{
			const uint32_t *triIndexes = inTriangles+iTri*3;
			bool ok = false;
			for (uint32_t iTri2=0; iTri2<numTriangles; iTri2++)
			{
				const uint32_t *outTri = outTriangles+iTri2*3;
				if ((outTri[0]==triIndexes[0] && outTri[1]==triIndexes[1] && outTri[2]==triIndexes[2]) ||
				   (outTri[0]==triIndexes[1] && outTri[1]==triIndexes[2] && outTri[2]==triIndexes[0]) ||				
				   (outTri[0]==triIndexes[2] && outTri[1]==triIndexes[0] && outTri[2]==triIndexes[1]))
				{
					ok = true;
					break;
				}
			}
			if (!ok)
			{
				printf("Missing (%d,%d,%d)\n", triIndexes[0], triIndexes[1], triIndexes[2]);
			}
		}
	}
	EDGEASSERT(sumPre==sumPost);
#endif
}

//-------------------
// This function measures the cost of cache misses when rendering a triangle list.  Lower costs are faster (and thus preferable).
static float GetCacheCost(const uint32_t *triangles, uint32_t numTriangles, float kFifoMiss, float kLruMiss)
{
	// initialize the fifo to unused values
	uint32_t fifo[kFifoSize+3];
	memset(fifo, 0xff, sizeof(fifo));

	// LRU primitive cache is the first thing tested before FIFO
	uint32_t lru[kLruSize];
	uint32_t tmpLru[kLruSize];
	memset(lru, 0xff, sizeof(uint32_t)*kLruSize);
	memset(tmpLru, 0xff, sizeof(uint32_t)*kLruSize);

	float totalCost = 0;
	for (uint32_t iTriangle=0; iTriangle<numTriangles; iTriangle++)
	{
		uint32_t *start = fifo;
		uint32_t *end = &fifo[kFifoSize-1];
		float const cost = PushTriangle(&start, &end, lru, tmpLru, triangles+iTriangle*3, kFifoMiss, kLruMiss);
		totalCost += cost;
		
		memcpy(lru, tmpLru, sizeof(uint32_t)*kLruSize);
		// copy back the fifo now so it starts at the beginning of fifo[]
		uint32_t sizeFifo = (uint32_t) (sizeof(fifo)/sizeof(fifo[0]));
		uint32_t offsetStart = (uint32_t) (start-fifo);
		for(uint32_t iFifoEntry=0; iFifoEntry<sizeFifo-offsetStart; iFifoEntry++)
		{
			fifo[iFifoEntry]=fifo[iFifoEntry+offsetStart];
		}
		for(uint32_t iFifoEntry=sizeFifo-offsetStart; iFifoEntry<sizeFifo; iFifoEntry++)
		{
			fifo[iFifoEntry]=UINT_MAX;
		}
	}
	return totalCost;
}

//------------------
/// This function is used locally with its seed initialized at each
/// call of edgeGeomKCacheOptimizerHillclimber, to make sure the 
/// optimizer is deterministic.
static uint32_t localRand(uint32_t &seed) 
{
    uint64_t z;
    z = (uint64_t) seed;
    z *= 279470273; 
    z %= 4294967291U;
	seed = (uint32_t) z;

    return seed;
}

//------------------
/// This function will start at a particular place in the 3 dimensional parametric space that controls the k-cache optimizer,
/// and will re-run it a given number of iterations, altering the constants slightly as it goes to find the best configuration
/// for the given triangle list.  This can improve performance by about 1/4% to 1% over about 100 iterations.  Passing in 0 will
/// just run the cache optimizer without trying to randomize the control parameters.
void edgeGeomKCacheOptimizerHillclimber(const uint32_t *inTriangles, const uint32_t numTriangles, void *userData, uint32_t *outTriangleList)
{
	EdgeGeomKCacheOptimizerHillclimberUserData *actualUserData = (EdgeGeomKCacheOptimizerHillclimberUserData*)userData;
	const uint32_t hillClimberIterations = actualUserData->m_numIterations;
	const EdgeGeomIndexesFlavor indexListType = actualUserData->m_indexesType;
	const uint32_t numInputAttr = actualUserData->m_numRsxInputAttributes;
	const uint32_t numOutputAttr = actualUserData->m_numRsxOutputAttributes;

	uint32_t localRandSeed = 0;

	bool const compressIndexes = (indexListType==kIndexesCompressedTriangleListCW || indexListType==kIndexesCompressedTriangleListCCW);

	float const kLruMiss = (float)numInputAttr;
	float const kFifoMiss = 4*(float)numOutputAttr;

	float bestK1 = 1.0f, bestK2 = 0.25f, bestK3 = 0.8f;  // these default starting values tend to work out pretty well
	float bestScore = FLT_MAX;

	// initially run the default constants and measure it as a baseline promise to the partitioner
	if (hillClimberIterations)
	{
		EdgeGeomKCacheOptimizerUserData ud = {kFifoMiss, kLruMiss, bestK1, bestK2, bestK3};
		edgeGeomKCacheOptimizer(inTriangles, numTriangles, &ud, outTriangleList);
		if (compressIndexes)
		{
			_edgeGeomRotateTrianglesForAdjacency(outTriangleList, numTriangles);
		}
		bestScore = GetCacheCost(outTriangleList, numTriangles, kFifoMiss, kLruMiss);

		// conditionally measure the index table if we're doing compressed indexes
		uint32_t indexesSize = 0;
		uint32_t indexesTempSize = 0;

		if (compressIndexes)
		{
			EdgeGeomIndexesSizes measuredSizes;
			_edgeGeomMeasureCompressedIndexTable(outTriangleList, numTriangles, indexListType, &measuredSizes);
			indexesSize = measuredSizes.m_inputSize;
			indexesTempSize = measuredSizes.m_maxTempSize;
		}

		for (uint32_t runs=0; runs<hillClimberIterations; runs++)
		{
			// permute the constants a bit
			float k1 = bestK1 + ((localRand(localRandSeed) % 20)/20.0f - 0.5f);
			float k2 = bestK2 + ((localRand(localRandSeed) % 20)/20.0f - 0.5f);
			float k3 = bestK3 + ((localRand(localRandSeed) % 20)/20.0f - 0.5f);
			
			// re-run the optimizer
			ud.m_k1 = k1;
			ud.m_k2 = k2;
			ud.m_k3 = k3;
			edgeGeomKCacheOptimizer(inTriangles, numTriangles, &ud, outTriangleList);
			if (compressIndexes)
			{
				_edgeGeomRotateTrianglesForAdjacency(outTriangleList, numTriangles);
			}					
			float const currentScore = GetCacheCost(outTriangleList, numTriangles, kFifoMiss, kLruMiss);
			
			// conditionally measure the index table so we can guarantee the partition will fit in the SPU
			uint32_t thisIndexTable = 0;
			uint32_t thisIndexTableTemp = 0;

			if (compressIndexes)
			{
				EdgeGeomIndexesSizes thisIndexTableSizes;
				_edgeGeomMeasureCompressedIndexTable(outTriangleList, numTriangles, indexListType, &thisIndexTableSizes);
				thisIndexTable = thisIndexTableSizes.m_inputSize;
				thisIndexTableTemp = thisIndexTableSizes.m_maxTempSize;
			}
			
			// only consider an ordering that is as good or better than a previous one, and is not LARGER when generating the index table
			if (currentScore <= bestScore && thisIndexTable<=indexesSize && thisIndexTableTemp<=indexesTempSize)
			{
				bestK1 = k1;
				bestK2 = k2;
				bestK3 = k3;
				bestScore = currentScore;
//				printf("Best K's: (%7.5f, %7.5f, %7.5f)  Score: %7.5f\n", bestK1, bestK2, bestK3, bestScore);
			}
		}
	}

	// run the best one through and remember it
	{
		EdgeGeomKCacheOptimizerUserData ud = {kFifoMiss, kLruMiss, bestK1, bestK2, bestK3};
		edgeGeomKCacheOptimizer(inTriangles, numTriangles, &ud, outTriangleList);
		if (compressIndexes)
		{
			_edgeGeomRotateTrianglesForAdjacency(outTriangleList, numTriangles);
		}
	}
}

//-------------------
