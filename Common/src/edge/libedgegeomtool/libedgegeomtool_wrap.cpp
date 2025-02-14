/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "edge/libedgegeomtool/libedgegeomtool_wrap.h"

#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/// Helper structure to represent a "batch" -- a collection of triangles from a scene which all use the
/// same material, and which are all either skinned or unskinned (in other words, which could all be
/// rendered using a single GPU draw call). Each EdgeGeomScene object may be referenced by several
/// EdgeGeomBatch objects, and each EdgeGeomBatch object may be partitioned into several SPU-sized
/// EdgeGeomSegment objects.
struct EdgeGeomBatch
{
	uint32_t m_trianglesStart; // triangle index of this batch's first triangle in the scene's sorted triangle list
	uint32_t m_numTriangles;
	int32_t m_materialId;
	bool m_isSkinned;
};

//-----------------

static bool TriangleHasSkinningInfo(const int32_t *matrixIndexes, const float *skinningWeights, uint32_t v0, uint32_t v1, uint32_t v2)
{
	// If the scene has no skinning info, then neither does the triangle :)
	if (matrixIndexes == NULL || skinningWeights == NULL)
		return false;

	const float kEpsilonWeight = 0.0001f;
	bool hasSkinningInfo = false;

	// check all three vertexes of each triangle for any skinning info.  We determine this SOLELY by the skin weights.
	// If they are all zero, there is no skinning info.  Otherwise, there is.
	for (uint32_t weightIndex=0; weightIndex<kEdgeGeomNumInputBonesPerVertex; weightIndex++)
	{
		hasSkinningInfo |= (matrixIndexes[v0*kEdgeGeomNumInputBonesPerVertex+weightIndex]!=-1)
			&& (skinningWeights[v0*kEdgeGeomNumInputBonesPerVertex+weightIndex]>kEpsilonWeight);
		hasSkinningInfo |= (matrixIndexes[v1*kEdgeGeomNumInputBonesPerVertex+weightIndex]!=-1)
			&& (skinningWeights[v1*kEdgeGeomNumInputBonesPerVertex+weightIndex]>kEpsilonWeight);
		hasSkinningInfo |= (matrixIndexes[v2*kEdgeGeomNumInputBonesPerVertex+weightIndex]!=-1)
			&& (skinningWeights[v2*kEdgeGeomNumInputBonesPerVertex+weightIndex]>kEpsilonWeight);

		if (hasSkinningInfo)
			return true;
	}

	return false;
}


/// Used to sort a triangle list, first by material ID (in ascending order),
/// then by skinning (skinned before unskinned).
struct TriangleInfo
{
	uint32_t triangleIndex;
	int32_t materialId;
	bool isSkinned;
};
static int CompareTriangles(const void *t1, const void *t2)
{
	TriangleInfo *tri1 = (TriangleInfo*)t1;
	TriangleInfo *tri2 = (TriangleInfo*)t2;

	if (tri1->materialId != tri2->materialId)
	{
		return (tri1->materialId < tri2->materialId) ? -1 : 1;
	}
	if (tri1->isSkinned != tri2->isSkinned)
	{
		return tri1->isSkinned ? -1 : 1;
	}
	return 0;
}


//-------------------

/// Partition an Edge scene into an array of batches, each of which contains all the triangles from the scene
/// that use a particular material.
///
/// @param edgeScene The input scene
/// @param outNumBatches The number of batches in the outBatches array will be stored here
/// @param outBatches A pointer to the array of output batches will be stored here.  The caller is responsible
///                   for freeing this memory using edgeGeomFree().
/// @param outBatchTriangles A pointer to a sorted list of the scene's triangles will be written here.  The
///                          batches in outBatches refer to subranges of this array.  The caller is responsible
///                          for freeing this memory using edgeGeomFree() when the batches are no longer needed.
static void PartitionSceneIntoBatches(const EdgeGeomScene &edgeScene,
									  uint32_t *outNumBatches, EdgeGeomBatch **outBatches, uint32_t **outBatchTriangles)
{
	const uint32_t *allTriangles = edgeScene.m_triangles;
	uint32_t numAllTriangles = edgeScene.m_numTriangles;
	const int32_t *materialIdPerTriangle = edgeScene.m_materialIdPerTriangle;
	const int32_t *matrixIndexesPerVertex = edgeScene.m_matrixIndexesPerVertex;
	const float *skinningWeightsPerVertex = edgeScene.m_skinningWeightsPerVertex;

	if (numAllTriangles == 0)
	{
		*outNumBatches = 0;
		*outBatches = NULL;
		*outBatchTriangles = NULL;
		return;
	}

	// Prepare an array of structures containing all relevant triangle information, suitable for qsort
	TriangleInfo *infos = (TriangleInfo*)edgeGeomAlloc(numAllTriangles*sizeof(TriangleInfo));
	for(uint32_t iTri=0; iTri<numAllTriangles; ++iTri)
	{
		infos[iTri].triangleIndex = iTri;
		infos[iTri].materialId = materialIdPerTriangle[iTri];
		infos[iTri].isSkinned = TriangleHasSkinningInfo(matrixIndexesPerVertex, skinningWeightsPerVertex,
			allTriangles[3*iTri+0], allTriangles[3*iTri+1], allTriangles[3*iTri+2]);
	}

	// Sort the triangles, by skinning and then by material ID.
	qsort(infos, numAllTriangles, sizeof(TriangleInfo), CompareTriangles);

	// Copy the sorted triangles in to the batch triangles array
	uint32_t *batchTriangles = (uint32_t*)edgeGeomAlloc(numAllTriangles * 3 * sizeof(uint32_t));
	for(uint32_t iTri=0; iTri<numAllTriangles; ++iTri)
	{
		const uint32_t srcTriIndex = infos[iTri].triangleIndex;
		batchTriangles[3*iTri+0] = allTriangles[3*srcTriIndex+0];
		batchTriangles[3*iTri+1] = allTriangles[3*srcTriIndex+1];
		batchTriangles[3*iTri+2] = allTriangles[3*srcTriIndex+2];
	}

	// Count the number of batches
	uint32_t numBatches = 1;
	int32_t batchMaterialId = infos[0].materialId;
	bool isBatchSkinned = infos[0].isSkinned;
	for(uint32_t iTri=1; iTri<numAllTriangles; ++iTri)
	{
		if (infos[iTri].materialId != batchMaterialId)
		{
			batchMaterialId = infos[iTri].materialId;
			numBatches++;
			continue; // avoid double-counting
		}
		else if (infos[iTri].isSkinned != isBatchSkinned)
		{
			isBatchSkinned = !isBatchSkinned;
			numBatches++;
		}
	}

	// Allocate the batch array and iterate over the sorted triangle infos, filling out
	// the the batch data
	EdgeGeomBatch *batches = (EdgeGeomBatch*)edgeGeomAlloc(numBatches*sizeof(EdgeGeomBatch));
	memset(batches, 0, numBatches*sizeof(EdgeGeomBatch));
	batches[0].m_numTriangles = 1;
	batches[0].m_trianglesStart = 0;
	batches[0].m_materialId = infos[0].materialId;
	batches[0].m_isSkinned = infos[0].isSkinned;
	uint32_t currentBatchIndex = 0;
	for(uint32_t iTri=1; iTri<numAllTriangles; ++iTri)
	{
		// Check if this is the beginning of a new batch
		if (infos[iTri].materialId != batches[currentBatchIndex].m_materialId || infos[iTri].isSkinned != batches[currentBatchIndex].m_isSkinned)
		{
			currentBatchIndex++;
			batches[currentBatchIndex].m_trianglesStart = iTri;
			batches[currentBatchIndex].m_materialId = infos[iTri].materialId;
			batches[currentBatchIndex].m_isSkinned = infos[iTri].isSkinned;
		}
		batches[currentBatchIndex].m_numTriangles++;
	}

	// Subtle improvement: partitioning batches by skinning can occasionally result in very small partitions (if only
	// a few triangles in a scene aren't skinning, for example).  So, it's helpful to go back over the batches
	// looking for small unskinned batches, and merging them with the corresponding skinned batch if it exists.
	// Note that we can only do this because the CompareTriangleInfo function used by qsort specifically
	// orders the unskinned batches to come directly after their skinned counterparts!
	uint32_t finalNumBatches = 1;
	const uint32_t MIN_UNSKINNED_BATCH_SIZE = 100; // if an unskinned batch has fewer than this many triangles, it is merged with its corresponding skinned batch.
	for(uint32_t iBatch=1; iBatch<numBatches; ++iBatch) // start at the second batch, because the first one will always be copied in-place
	{
		bool isEligibleToMerge = (!batches[iBatch].m_isSkinned
			&& batches[iBatch].m_numTriangles < MIN_UNSKINNED_BATCH_SIZE);
		bool skinnedBatchExists = (batches[iBatch].m_materialId == batches[finalNumBatches-1].m_materialId
			&& batches[finalNumBatches-1].m_isSkinned);
		if (isEligibleToMerge && skinnedBatchExists)
		{
			// This batch is eligible for merging.  Add its triangles to the corresponding skinned batch,
			// which was the last one we copied.
			batches[finalNumBatches-1].m_numTriangles += batches[iBatch].m_numTriangles;
		}
		else
		{
			// Copy this batch directly into the next available slot.
			batches[finalNumBatches].m_materialId = batches[iBatch].m_materialId;
			batches[finalNumBatches].m_isSkinned = batches[iBatch].m_isSkinned;
			batches[finalNumBatches].m_numTriangles = batches[iBatch].m_numTriangles;
			finalNumBatches++;
		}
	}

	*outNumBatches = finalNumBatches;
	*outBatches = batches;
	*outBatchTriangles = batchTriangles;

	// Cleanup
	edgeGeomFree(infos);
}

//-------------------

// Splits a scene into several segments, each of which will fit in the SPU's local store.
void edgeGeomPartitionSceneIntoSegments(const EdgeGeomScene &edgeScene, const EdgeGeomSegmentFormat &edgeFormat,
										EdgeGeomSegment **outSegments, uint32_t *outNumSegments,
										EdgeGeomCustomPartitionDataSizeFunc partitionDataSizeFunc,
										EdgeGeomCustomCommandBufferHoleSizeFunc commandBufferHoleSizeFunc)
{
	// Partition the scene into batches (one per unique skinning/material pair).  The batches refer
	// to a sorted copy of the scene's triangle list.
	EdgeGeomBatch *edgeBatches = NULL;
	uint32_t numEdgeBatches = 0;
	uint32_t *batchTriangles = NULL;
	PartitionSceneIntoBatches(edgeScene, &numEdgeBatches, &edgeBatches, &batchTriangles);

	//
	// Call the Edge partitioner on each batch
	//
	EdgeGeomPartitionerOutput *partitionsPerBatch = (EdgeGeomPartitionerOutput*)edgeGeomAlloc(numEdgeBatches * sizeof(EdgeGeomPartitionerOutput));
	memset(partitionsPerBatch, 0, numEdgeBatches*sizeof(EdgeGeomPartitionerOutput));
	// Most of the partitionerDataIn settings are identical across all batches
	EdgeGeomPartitionerInput partitionDataIn;
	memset(&partitionDataIn, 0, sizeof(EdgeGeomPartitionerInput));
	partitionDataIn.m_inputVertexStride[0] = edgeFormat.m_spuInputVertexFormats[0]->m_vertexStride;
	partitionDataIn.m_inputVertexStride[1] = (edgeFormat.m_spuInputVertexFormats[1] != NULL) ? edgeFormat.m_spuInputVertexFormats[1]->m_vertexStride : 0;
	partitionDataIn.m_numInputAttributes = edgeFormat.m_spuInputVertexFormats[0]->m_numAttributes;
	if (edgeFormat.m_spuInputVertexFormats[1] != NULL)
		partitionDataIn.m_numInputAttributes += edgeFormat.m_spuInputVertexFormats[1]->m_numAttributes;
	partitionDataIn.m_indexListFlavor = edgeFormat.m_indexesType;
	partitionDataIn.m_skinningMatrixFormat = edgeFormat.m_skinMatrixFormat;
	partitionDataIn.m_numOutputAttributes = (edgeFormat.m_spuOutputVertexFormat != NULL) ? edgeFormat.m_spuOutputVertexFormat->m_numAttributes : 0;
	partitionDataIn.m_outputVertexStride  = (edgeFormat.m_spuOutputVertexFormat != NULL) ? edgeFormat.m_spuOutputVertexFormat->m_vertexStride  : 0;
	partitionDataIn.m_cacheOptimizerCallback = edgeGeomKCacheOptimizerHillclimber;
	EdgeGeomKCacheOptimizerHillclimberUserData cacheOptimizerData =
	{
		0, // with m_numIterations of 0, this is essentially the same as using the non-hillclimber version of kcache
		edgeFormat.m_indexesType,
		partitionDataIn.m_numOutputAttributes, // Note that the "numOutputAttributes" in the partitioner data are *SPU* outputs, not (necessarily) GPU outputs.
		partitionDataIn.m_numOutputAttributes  // The hillclimber function wants GPU inputs and outputs, which we assume are both the same as the SPU outputs.
	};
	partitionDataIn.m_cacheOptimizerUserData = &cacheOptimizerData;
	partitionDataIn.m_customDataSizeCallback = partitionDataSizeFunc; // only necessary if our segments will include extra data beyond Edge standard buffers
	partitionDataIn.m_customCommandBufferHoleSizeCallback = commandBufferHoleSizeFunc; // only necessary if our segments will include extra GCM commands beyond the ones expected by Edge
	// Determine the attribute index of the vertex positions within the scene's vertex stream
	uint16_t positionAttributeIndex = 0;
	for(uint32_t iSourceAttr=0; iSourceAttr<edgeScene.m_numVertexAttributes; ++iSourceAttr)
	{
		if (edgeScene.m_vertexAttributeIds[iSourceAttr] == EDGE_GEOM_ATTRIBUTE_ID_POSITION)
		{
			positionAttributeIndex = edgeScene.m_vertexAttributeIndexes[iSourceAttr];
			break;
		}
	}
	*outNumSegments = 0; // we'll count the total number of output segments along the way
	for(uint32_t iBatch=0; iBatch<numEdgeBatches; ++iBatch)
	{
		// These fields can vary per batch
		partitionDataIn.m_skinningFlavor = edgeBatches[iBatch].m_isSkinned ? edgeFormat.m_skinType : kSkinNone;
		partitionDataIn.m_numTriangles = edgeBatches[iBatch].m_numTriangles;
		partitionDataIn.m_triangleList = batchTriangles + 3*edgeBatches[iBatch].m_trianglesStart;
		partitionDataIn.m_skinningMatrixIndexesPerVertex = (partitionDataIn.m_skinningFlavor != kSkinNone)
			? edgeScene.m_matrixIndexesPerVertex : NULL;
		if (edgeScene.m_numBlendShapes > 0 && edgeFormat.m_spuInputVertexDeltaFormat != NULL)
		{
			partitionDataIn.m_deltaStreamVertexStride = edgeFormat.m_spuInputVertexDeltaFormat->m_vertexStride;
			edgeGeomGetBlendedVertexes(edgeScene.m_vertexDeltas, edgeScene.m_numVertexes, edgeScene.m_numFloatsPerDelta, *edgeFormat.m_spuInputVertexDeltaFormat, 
				edgeScene.m_blendedAttributeIndexes, edgeScene.m_blendedAttributeIds, edgeScene.m_numBlendedAttributes,
				edgeScene.m_numBlendShapes, edgeScene.m_triangles, edgeScene.m_numTriangles,
				&partitionDataIn.m_blendedVertexIndexes, &partitionDataIn.m_numBlendedVertexes);
		}		
		// generate triangle centroid array for the input triangle list.  This step is optional,
		// but leads to better spatial coherency in the output segments.
		edgeGeomComputeTriangleCentroids(edgeScene.m_vertexes, edgeScene.m_numFloatsPerVertex, positionAttributeIndex,
			partitionDataIn.m_triangleList, partitionDataIn.m_numTriangles, &partitionDataIn.m_triangleCentroids);

		// Invoke the Edge partitioner, splitting the batch into SPU-sized geometry segments
		edgeGeomPartitioner(partitionDataIn, &partitionsPerBatch[iBatch]);

		*outNumSegments += partitionsPerBatch[iBatch].m_numPartitions;

		// Cleanup
		edgeGeomFree(partitionDataIn.m_blendedVertexIndexes);
		edgeGeomFree(partitionDataIn.m_triangleCentroids);
		partitionDataIn.m_blendedVertexIndexes = NULL;
		partitionDataIn.m_triangleCentroids = NULL;
	}

	// The batch triangle data is no longer necessary
	edgeGeomFree(batchTriangles);
	batchTriangles = NULL;
	// allocate the final geometry segment array
	EdgeGeomSegment *segments = (EdgeGeomSegment*)edgeGeomAlloc(*outNumSegments * sizeof(EdgeGeomSegment));
	memset(segments, 0, *outNumSegments * sizeof(EdgeGeomSegment));
	*outSegments = segments;

	//
	// Convert the partitions to geometry segments, suitable for output or processing.
	//
	uint32_t nextSegmentIndex = 0; // index of the next segment to write to
	// Get pointers to various scene data buffers, to save typing below
	const float *sourceVertexes = edgeScene.m_vertexes;
	uint32_t numFloatsPerSourceVertex = edgeScene.m_numFloatsPerVertex;
	uint8_t numSourceAttributes = edgeScene.m_numVertexAttributes;
	const uint16_t *sourceAttributeIndexes = edgeScene.m_vertexAttributeIndexes;
	const EdgeGeomAttributeId *sourceAttributeIds = edgeScene.m_vertexAttributeIds;
	const int32_t *matrixIndexesPerVertex = edgeScene.m_matrixIndexesPerVertex;
	const float *boneWeightsPerVertex = edgeScene.m_skinningWeightsPerVertex;
	// Get IDs for the flavors used in this scene
	uint8_t inputVertexFormatId = edgeGeomGetSpuVertexFormatId(*edgeFormat.m_spuInputVertexFormats[0]);
	uint8_t secondaryInputVertexFormatId = (edgeFormat.m_spuInputVertexFormats[1] == NULL) ? EDGE_GEOM_SPU_VERTEX_FORMAT_EMPTY : edgeGeomGetSpuVertexFormatId(*edgeFormat.m_spuInputVertexFormats[1]);
	uint8_t outputVertexFormatId = (edgeFormat.m_spuOutputVertexFormat == NULL) ? EDGE_GEOM_RSX_VERTEX_FORMAT_EMPTY :
		edgeGeomGetRsxVertexFormatId(*edgeFormat.m_spuOutputVertexFormat);
	uint8_t deltaFormatId = (edgeFormat.m_spuInputVertexDeltaFormat == NULL) ? EDGE_GEOM_SPU_VERTEX_FORMAT_EMPTY : edgeGeomGetSpuVertexFormatId(*edgeFormat.m_spuInputVertexDeltaFormat);

	for(uint32_t iBatch=0; iBatch<numEdgeBatches; ++iBatch)
	{
		EdgeGeomPartitionerOutput &batchPartitions = partitionsPerBatch[iBatch];
		// Handle trivial case
		if (batchPartitions.m_numPartitions == 0)
		{
			continue;
		}

		// Override the scene's skinning flavor if this batch isn't skinned
		EdgeGeomSkinningFlavor batchSkinType = edgeBatches[iBatch].m_isSkinned ? edgeFormat.m_skinType : kSkinNone;

		// Populate each segment from the corresponding partition
		uint32_t totalTriangleCount = 0;
		uint32_t totalUniqueVertexCount = 0;
		for(uint32_t iPartition=0; iPartition<batchPartitions.m_numPartitions; ++iPartition)
		{
			EdgeGeomSegment &segment = segments[nextSegmentIndex++];
			memset(&segment, 0, sizeof(EdgeGeomSegment));

			// Get pointers to this partitions triangles and unique vertex indexes.
			const uint32_t *partitionTriangleList = batchPartitions.m_triangleListOut + totalTriangleCount*3;
			const uint32_t  numTriangles = batchPartitions.m_numTrianglesPerPartition[iPartition];
			const uint32_t *partitionOriginalVertexIndexes = batchPartitions.m_originalVertexIndexesPerPartition + totalUniqueVertexCount;
			const uint32_t numUniqueVertexes = batchPartitions.m_numUniqueVertexesPerPartition[iPartition];
			totalTriangleCount += numTriangles;
			totalUniqueVertexCount += numUniqueVertexes;

			// Store material ID (for bookkeeping purposes)
			segment.m_materialId = edgeBatches[iBatch].m_materialId;

			// Store maximum SPU buffer sizes.
			uint32_t numSpuInputAttributes = edgeFormat.m_spuInputVertexFormats[0]->m_numAttributes;
			if (edgeFormat.m_spuInputVertexFormats[1] != NULL)
				numSpuInputAttributes += edgeFormat.m_spuInputVertexFormats[1]->m_numAttributes;
			uint32_t numSpuOutputAttributes = 0;
			if (edgeFormat.m_spuOutputVertexFormat != NULL)
				numSpuOutputAttributes = edgeFormat.m_spuOutputVertexFormat->m_numAttributes;
			uint32_t maxNumSpuAttributes = std::max(numSpuInputAttributes, numSpuOutputAttributes);
			segment.m_scratchSize = edgeGeomGetScratchBufferSizeInQwords(maxNumSpuAttributes, numUniqueVertexes);
			segment.m_ioBufferSize = batchPartitions.m_ioBufferSizePerPartition[iPartition];

			// Make primary vertex buffer
			edgeGeomMakeSpuVertexBuffer(sourceVertexes, numFloatsPerSourceVertex, sourceAttributeIndexes, sourceAttributeIds, numSourceAttributes,
				partitionOriginalVertexIndexes, numUniqueVertexes, *edgeFormat.m_spuInputVertexFormats[0],
				&segment.m_spuVertexes[0], &segment.m_spuVertexesSizes[0], &segment.m_fixedOffsetPtrs[0], &segment.m_fixedOffsetsSize[0]);
			// Make secondary vertex buffer, if present
			if (edgeFormat.m_spuInputVertexFormats[1] != NULL)
			{
				edgeGeomMakeSpuVertexBuffer(sourceVertexes, numFloatsPerSourceVertex, sourceAttributeIndexes, sourceAttributeIds, numSourceAttributes,
					partitionOriginalVertexIndexes, numUniqueVertexes, *edgeFormat.m_spuInputVertexFormats[1],
					&segment.m_spuVertexes[1], &segment.m_spuVertexesSizes[3], &segment.m_fixedOffsetPtrs[1], &segment.m_fixedOffsetsSize[1]);
			}
			// Make GPU-only vertex buffer, if present
			if (edgeFormat.m_rsxOnlyVertexFormat != NULL)
			{
				edgeGeomMakeRsxVertexBuffer(sourceVertexes, numFloatsPerSourceVertex, sourceAttributeIndexes, sourceAttributeIds, numSourceAttributes,
					partitionOriginalVertexIndexes, numUniqueVertexes, *edgeFormat.m_rsxOnlyVertexFormat, true, // the wrapper always writes GPU streams as big-endian
					&segment.m_rsxOnlyVertexes, &segment.m_rsxOnlyVertexesSize);
			}

			// Make index buffer
			edgeGeomMakeIndexBuffer(partitionTriangleList, numTriangles, edgeFormat.m_indexesType,
				&segment.m_indexes, &segment.m_indexesSizes[0]);

			// Make skinning buffers
			edgeGeomMakeSkinningBuffer(matrixIndexesPerVertex, boneWeightsPerVertex, batchSkinType, edgeFormat.m_skinMatrixFormat, partitionOriginalVertexIndexes, numUniqueVertexes, 
				&segment.m_skinIndexesAndWeights, &segment.m_skinIndexesAndWeightsSizes[0], &segment.m_skinMatricesByteOffsets[0], &segment.m_skinMatricesSizes[0]);

			// Make blend shape buffers
			// First, determine which blend shapes affect this partition.
			bool *includeShape = (bool*)edgeGeomAlloc(edgeScene.m_numBlendShapes * sizeof(bool)); // indicates which shapes should be included in the output for this segment
			uint32_t numAffectingShapes = 0;
			for(uint32_t iShape=0; iShape<edgeScene.m_numBlendShapes; ++iShape)
			{
				const float *shapeDeltas = edgeScene.m_vertexDeltas + iShape*edgeScene.m_numVertexes*edgeScene.m_numFloatsPerDelta;
				includeShape[iShape] = edgeGeomBlendShapeAffectsSegment(shapeDeltas, edgeScene.m_numFloatsPerDelta, *edgeFormat.m_spuInputVertexDeltaFormat,
					edgeScene.m_blendedAttributeIndexes, edgeScene.m_blendedAttributeIds, edgeScene.m_numBlendedAttributes,
					partitionOriginalVertexIndexes, numUniqueVertexes);
				numAffectingShapes += includeShape[iShape] ? 1 : 0;
			}

			// If any blend shapes affect this partition, generate their output buffers.
			if (numAffectingShapes > 0)
			{
				segment.m_numBlendShapes = edgeScene.m_numBlendShapes;
				segment.m_blendShapes = (uint8_t**)edgeGeomAlloc(segment.m_numBlendShapes * sizeof(uint8_t*));
				segment.m_blendShapeSizes = (uint16_t*)edgeGeomAlloc(segment.m_numBlendShapes * sizeof(uint16_t));
				memset(segment.m_blendShapes, 0, segment.m_numBlendShapes * sizeof(uint8_t*));
				memset(segment.m_blendShapeSizes, 0, segment.m_numBlendShapes * sizeof(uint16_t));
				for(uint32_t iShape=0; iShape<edgeScene.m_numBlendShapes; ++iShape)
				{
					if (!includeShape[iShape])
						continue;
					const float *shapeDeltas = edgeScene.m_vertexDeltas + iShape*edgeScene.m_numVertexes*edgeScene.m_numFloatsPerDelta;
					edgeGeomMakeBlendShapeBuffer(shapeDeltas, edgeScene.m_numFloatsPerDelta, *edgeFormat.m_spuInputVertexDeltaFormat,
						edgeScene.m_blendedAttributeIndexes, edgeScene.m_blendedAttributeIds, edgeScene.m_numBlendedAttributes,
						partitionOriginalVertexIndexes, numUniqueVertexes,
						&segment.m_blendShapes[iShape], &segment.m_blendShapeSizes[iShape]);
				}
			}
			edgeGeomFree(includeShape);

			// Make EdgeGeomSpuConfigInfo
			uint32_t numOutputAttributes = (edgeFormat.m_spuOutputVertexFormat != NULL) ? edgeFormat.m_spuOutputVertexFormat->m_numAttributes : 0;
			const uint32_t commandBufferHoleSize = edgeGeomGetCommandBufferHoleSize(numOutputAttributes, numTriangles*3, commandBufferHoleSizeFunc);
			uint8_t segmentFlags = 0;
			// Several Edge operations (e.g. blend shapes, culling, occluders, skinning) require an extra uniform table to be allocated
			// at runtime for the storage of temporary per-vertex data.  We set a flag here if any of these operations are used.
			// UPDATED: as of Edge 1.1.0, the extra uniform table is always present.
			segmentFlags |= EDGE_GEOM_FLAG_INCLUDES_EXTRA_UNIFORM_TABLE;
			edgeGeomMakeSpuConfigInfo(numUniqueVertexes, numTriangles, maxNumSpuAttributes,
				segmentFlags, edgeFormat.m_indexesType, batchSkinType, edgeFormat.m_skinMatrixFormat,
				inputVertexFormatId, secondaryInputVertexFormatId, outputVertexFormatId, deltaFormatId,
				commandBufferHoleSize, &segment.m_spuConfigInfo);

			// Make stream descriptions
			if (inputVertexFormatId == 0xFF) // primary input stream -- only needs a stream description if it's a custom stream format
			{
				edgeGeomMakeSpuStreamDescription(*edgeFormat.m_spuInputVertexFormats[0], &segment.m_spuInputStreamDescriptions[0], &segment.m_spuInputStreamDescriptionSizes[0]);
			}
			if (secondaryInputVertexFormatId == 0xFF)
			{
				// NULL/empty streams should match the appropriate *_EMPTY built-in stream format, and thus should not have a custom format
				assert(edgeFormat.m_spuInputVertexFormats[1] != NULL);

				edgeGeomMakeSpuStreamDescription(*edgeFormat.m_spuInputVertexFormats[1], &segment.m_spuInputStreamDescriptions[1], &segment.m_spuInputStreamDescriptionSizes[1]);
			}
			if (outputVertexFormatId == 0xFF) // output stream -- only needs a stream description if it's a custom stream format
			{
				// NULL/empty streams should match the appropriate *_EMPTY built-in stream format, and thus should not have a custom format
				assert(edgeFormat.m_spuOutputVertexFormat != NULL); // NULL/empty 
				edgeGeomMakeRsxStreamDescription(*edgeFormat.m_spuOutputVertexFormat, &segment.m_spuOutputStreamDescription, &segment.m_spuOutputStreamDescriptionSize);
			}
			if (edgeFormat.m_rsxOnlyVertexFormat != NULL) // RSX-only stream will always need a stream description
			{
				edgeGeomMakeRsxStreamDescription(*edgeFormat.m_rsxOnlyVertexFormat, &segment.m_rsxOnlyStreamDescription, &segment.m_rsxOnlyStreamDescriptionSize);
			}
		}

		// We're finished with the data in this batch's partitioner output, so delete it now
		edgeGeomFree(batchPartitions.m_numTrianglesPerPartition);
		edgeGeomFree(batchPartitions.m_triangleListOut);
		edgeGeomFree(batchPartitions.m_originalVertexIndexesPerPartition);
		edgeGeomFree(batchPartitions.m_numUniqueVertexesPerPartition);
		edgeGeomFree(batchPartitions.m_ioBufferSizePerPartition);
		memset(&batchPartitions, 0, sizeof(EdgeGeomPartitionerOutput));
	}

	// Cleanup
	edgeGeomFree(partitionsPerBatch);
	edgeGeomFree(edgeBatches);
	edgeBatches = NULL;
}


void edgeGeomFreeSegmentData(EdgeGeomSegment &segment)
{
	edgeGeomFree(segment.m_spuConfigInfo);
	edgeGeomFree(segment.m_indexes);
	edgeGeomFree(segment.m_spuVertexes[0]);
	edgeGeomFree(segment.m_spuVertexes[1]);
	edgeGeomFree(segment.m_fixedOffsetPtrs[0]);
	edgeGeomFree(segment.m_fixedOffsetPtrs[1]);
	edgeGeomFree(segment.m_rsxOnlyVertexes);
	edgeGeomFree(segment.m_spuInputStreamDescriptions[0]);
	edgeGeomFree(segment.m_spuInputStreamDescriptions[1]);
	edgeGeomFree(segment.m_spuOutputStreamDescription);
	edgeGeomFree(segment.m_rsxOnlyStreamDescription);
	edgeGeomFree(segment.m_skinIndexesAndWeights);
	edgeGeomFree(segment.m_blendShapeSizes);
	for(uint32_t iShape=0; iShape<segment.m_numBlendShapes; ++iShape)
		edgeGeomFree(segment.m_blendShapes[iShape]);
	edgeGeomFree(segment.m_blendShapes);
	memset(&segment, 0, sizeof(EdgeGeomSegment));
}
