/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#include "PS3Tools.h"

#include "edge/libedgegeomtool/libedgegeomtool.h"
#include "edge/libedgegeomtool/libedgegeomtool_wrap.h"


static uint32_t EdgeExtraPartitionDataCallback(const EdgeGeomPartitionElementCounts& PartitionContents)
{
	(void)PartitionContents;
	// We require an additional 128 bytes for WriteFence()
	return 128;
}

/**
 * Prepares a mesh for use by Playstation(R)EDGE geometry processing at runtime. This involves partitioning
 * the mesh into smaller SPU-sized chunks
 */
static void SharedGeomCooking(
	FMeshFragmentCookInfo& ElementInfo,
	FMeshFragmentCookOutputInfo& OutInfo,
	EdgeGeomSpuVertexFormat& SpuInputVertexFormat,
	EdgeGeomSpuVertexFormat& SpuSecondaryInputVertexFormat,
	EdgeGeomRsxVertexFormat& SpuOutputVertexFormat,
	EdgeGeomRsxVertexFormat& RsxOnlyVertexFormat)
{
	// If we don't need full Edge Geometry support, just run the k-cache optimizer. The caller
	// will manually reorder the entire vertex buffer to improve pre-transform cache performance.
	if (!ElementInfo.bEnableEdgeGeometrySupport)
	{
		// The k-cache optimizer expects the index lists to be 32-bit instead of 16-bit, so we have to
		// create new index lists. Copy the 16-bit index array to the 32-bit index array
		UINT* InputIndices32 = new UINT[ElementInfo.NumTriangles * 3];
		memset(InputIndices32, 0, sizeof(UINT) * ElementInfo.NumTriangles * 3);
		for(UINT Index = 0; Index < ElementInfo.NumTriangles * 3; Index++)
		{
			*(InputIndices32 + Index) = (UINT)*(ElementInfo.Indices + Index);
		}

		UINT* OutputIndices32 = new UINT[ElementInfo.NumTriangles * 3];
		memset(OutputIndices32, 0, sizeof(UINT) * ElementInfo.NumTriangles * 3);

		// Invoke the k-cache optimizer
		EdgeGeomKCacheOptimizerUserData OptimizerParameters;
		memset( &OptimizerParameters, 0, sizeof(EdgeGeomKCacheOptimizerUserData) );
		// Edge 4.0: The m_fifoMissCost:m_lruMissCost ratio that has given the most consistent results is 4:1.
		// Edge 3.5: The m_fifoMissCost:m_lruMissCost ratio that has given the most consistent results is 3:1.
		OptimizerParameters.m_fifoMissCost = 3.0f;	// Cost factor for a single post-transform cache miss. Recommended to be the number of input attributes to the vertex program.
		OptimizerParameters.m_lruMissCost = 1.0f;	// Cost factor for a single mini-cache miss. Recommended to be four times the number of output attributes from the vertex program.
		OptimizerParameters.m_k1 = 1.0f;			// Reasonable default: 1.0
		OptimizerParameters.m_k2 = 0.25f;			// Reasonable default: 0.25
		OptimizerParameters.m_k3 = 0.8f;			// Reasonable default: 0.8
		edgeGeomKCacheOptimizer( InputIndices32, ElementInfo.NumTriangles, &OptimizerParameters, OutputIndices32 );

		// Convert the reordered, partitioned index buffer to 16-bit indices and write to the output buffer.
		for (UINT Index = 0; Index < ElementInfo.NumTriangles * 3; Index++)
		{
			OutInfo.NewIndices[Index] = (WORD)OutputIndices32[Index];
		}

		// Initialize some output data counts to reasonable values, so that the caller knows we
		// succeeded.
		OutInfo.NewNumTriangles = ElementInfo.NumTriangles;
		OutInfo.NewNumVertices  = ElementInfo.MaxVertexIndex - ElementInfo.MinVertexIndex + 1;
		OutInfo.NumPartitions = 1;

		delete [] InputIndices32;
		delete [] OutputIndices32;
		return;
	}

	// Edge functions expect the index lists to be 32-bit instead of 16-bit, so we have to create new index lists.
	// We also need to bias the input indices (which are in the range [MinVertexIndex..MaxVertexIndex] down to the range
	// [0..MaxVertexIndex-MinVertexIndex].
	// copy the 16-bit index array to the 32-bit index array
	UINT* InputIndices32 = new UINT[ElementInfo.NumTriangles * 3];
	for(UINT Index = 0; Index < ElementInfo.NumTriangles * 3; Index++)
	{
		InputIndices32[Index] = (UINT)*(ElementInfo.Indices + Index) - ElementInfo.MinVertexIndex;
	}

	// Partition the triangles into partitions
	EdgeGeomPartitionerInput PartitionerInput;
	memset(&PartitionerInput, 0, sizeof(EdgeGeomPartitionerInput));
	PartitionerInput.m_inputVertexStride[0] = SpuInputVertexFormat.m_vertexStride;
	PartitionerInput.m_inputVertexStride[1] = SpuSecondaryInputVertexFormat.m_vertexStride;
	PartitionerInput.m_numInputAttributes = SpuInputVertexFormat.m_numAttributes + SpuSecondaryInputVertexFormat.m_numAttributes;
	PartitionerInput.m_indexListFlavor = kIndexesU16TriangleListCCW;
	PartitionerInput.m_numOutputAttributes = SpuOutputVertexFormat.m_numAttributes;
	PartitionerInput.m_outputVertexStride = SpuOutputVertexFormat.m_vertexStride;
	PartitionerInput.m_cacheOptimizerCallback = edgeGeomKCacheOptimizer;
	EdgeGeomKCacheOptimizerUserData OptimizerParameters;
	memset(&OptimizerParameters, 0, sizeof(EdgeGeomKCacheOptimizerUserData));
	// Edge 0.4.x: The m_fifoMissCost:m_lruMissCost ratio that has given the most consistent results is 4:1.
	// Edge 0.3.x: The m_fifoMissCost:m_lruMissCost ratio that has given the most consistent results is 3:1.
	OptimizerParameters.m_fifoMissCost = 4.0f;	// Cost factor for a single post-transform cache miss. Recommended to be the number of input attributes to the vertex program.
	OptimizerParameters.m_lruMissCost = 1.0f;	// Cost factor for a single mini-cache miss. Recommended to be four times the number of output attributes from the vertex program.
	OptimizerParameters.m_k1 = 1.0f;			// Reasonable default: 1.0
	OptimizerParameters.m_k2 = 0.25f;			// Reasonable default: 0.25
	OptimizerParameters.m_k3 = 0.8f;			// Reasonable default: 0.8
	PartitionerInput.m_cacheOptimizerUserData = &OptimizerParameters;
	PartitionerInput.m_customDataSizeCallback = EdgeExtraPartitionDataCallback; 
	//PartitionerInput.m_customCommandBufferHoleSizeCallback = 0; // Only necessary if inserting additional GCM calls into each job's command buffer hole (beyond Edge's standard calls)

	PartitionerInput.m_numTriangles = ElementInfo.NumTriangles;
	PartitionerInput.m_triangleList = InputIndices32;
	// Skinning?
	PartitionerInput.m_skinningFlavor = kSkinNone;
	PartitionerInput.m_skinningMatrixIndexesPerVertex = NULL;
	// Blend shapes?
	PartitionerInput.m_deltaStreamVertexStride = 0;
	PartitionerInput.m_blendedVertexIndexes = 0;
	PartitionerInput.m_numBlendedVertexes = 0;
	// generate triangle centroid array for the input triangle list.  This step is optional,
	// but leads to better spatial coherency in the output partitions.
	WORD NumFloatsPerVertex = 3; // only counting the position vertices
	WORD PositionAttributeIndex = 0; // index of position within the SpuInputVertexFormat's array of attributes

	// if we don't have positions, then skip computing the centroids
	if (ElementInfo.PositionVertices)
	{
		edgeGeomComputeTriangleCentroids(ElementInfo.PositionVertices + ElementInfo.MinVertexIndex*NumFloatsPerVertex,
			NumFloatsPerVertex, PositionAttributeIndex,	PartitionerInput.m_triangleList, PartitionerInput.m_numTriangles,
			&PartitionerInput.m_triangleCentroids);
	}
	else
	{
		// ignore centroids
		PartitionerInput.m_triangleCentroids = NULL;
	}

	// Invoke the Edge partitioner, splitting the batch into SPU-sized geometry partitions
	EdgeGeomPartitionerOutput PartitionerOutput;
	memset(&PartitionerOutput, 0, sizeof(EdgeGeomPartitionerOutput));
	edgeGeomPartitioner(PartitionerInput, &PartitionerOutput);
	// Cleanup
	delete [] InputIndices32;
	InputIndices32 = NULL;
	edgeGeomFree(PartitionerInput.m_triangleCentroids);
	PartitionerInput.m_triangleCentroids = NULL;

	// We'll be writing all the new vertex and index data into one large buffer apiece, with padding vertices/triangles
	// inserted such that each partition's data starts at a 16-byte boundary.  First we need to count how many padding elements
	// are required.
	UINT NumPartitions = PartitionerOutput.m_numPartitions;
	UINT VertsSize1 = 0; // position stream
	UINT VertsSize2 = 0; // non-position Edge stream
	UINT VertsSize3 = 0; // RSX-only stream
	UINT Stride1 = SpuInputVertexFormat.m_vertexStride; // position-only: F32x3
	UINT Stride2 = SpuSecondaryInputVertexFormat.m_vertexStride; // secondary SPU input stream (not needed for position-only, static meshes)
	UINT Stride3 = RsxOnlyVertexFormat.m_vertexStride;
	UINT* NumPaddingVerts = new UINT[NumPartitions];
	WORD* IndexBias = new WORD[NumPartitions];
	UINT TotalNumPaddingVerts = 0;
	UINT TrisSize = 0;
	UINT* NumPaddingTris = new UINT[NumPartitions];
	UINT TotalNumPaddingTris = 0;
	for(UINT PartitionIndex=0; PartitionIndex<NumPartitions; ++PartitionIndex)
	{
		// how many verts are already in the stream (including the ones output by previously-cooked MeshElements)?
		// We'll have to bias this partition's triangle list by this amount.
		IndexBias[PartitionIndex] = (WORD)(ElementInfo.NewMinVertexIndex + VertsSize1 / Stride1);

		// Add the size of the actual vertex data from each partition
		VertsSize1 += PartitionerOutput.m_numUniqueVertexesPerPartition[PartitionIndex] * Stride1;
		VertsSize2 += PartitionerOutput.m_numUniqueVertexesPerPartition[PartitionIndex] * Stride2;
		VertsSize3 += PartitionerOutput.m_numUniqueVertexesPerPartition[PartitionIndex] * Stride3;
		// Determine the amount of padding verts necessary to make
		// each stream's size a multiple of 16 bytes.
		NumPaddingVerts[PartitionIndex] = 0;
		while( (VertsSize1 & 0xF) || (VertsSize2 & 0xF) || (VertsSize3 & 0xF) )
		{
			VertsSize1 += Stride1;
			VertsSize2 += Stride2;
			VertsSize3 += Stride3;
			NumPaddingVerts[PartitionIndex] += 1;
		}

		TotalNumPaddingVerts += NumPaddingVerts[PartitionIndex];
		
		NumPaddingTris[PartitionIndex] = 0;
		switch(PartitionerInput.m_indexListFlavor)
		{
		case kIndexesU16TriangleListCW:
		case kIndexesU16TriangleListCCW:
			// uncompressed triangle lists must be padded up to a 16-byte boundary
			// by adding additional triangles.
			TrisSize += PartitionerOutput.m_numTrianglesPerPartition[PartitionIndex] * 3*sizeof(WORD);
			while (TrisSize & 0xF)
			{
				TrisSize += 3*sizeof(WORD);
				NumPaddingTris[PartitionIndex] += 1;
			}
			TotalNumPaddingTris += NumPaddingTris[PartitionIndex];
			break;
		case kIndexesCompressedTriangleListCW:
		case kIndexesCompressedTriangleListCCW:
			// compressed indexes will only be used by Edge, so there's no need to pad them.
			// TODO: ???
			break;
		}

	}

	///////////////////////
	OutInfo.NewNumVertices = (VertsSize1 / Stride1);
	OutInfo.NewNumTriangles = TrisSize / (3*sizeof(WORD));
	OutInfo.NumPartitions = NumPartitions;
	// Make sure we don't have too many vertices, since we're still using 16-bit indices.
	if (OutInfo.NewNumVertices > 65536)
	{
		printf("ERROR! EdgeGeom's padded vertex buffer is too large for 16-bit indices!\n");
		OutInfo.NewNumVertices = 0;
		OutInfo.NewNumTriangles = 0;
		OutInfo.NumPartitions = 0;

		// Clean up the partitioner output
		edgeGeomFree(PartitionerOutput.m_numTrianglesPerPartition);
		edgeGeomFree(PartitionerOutput.m_triangleListOut);
		edgeGeomFree(PartitionerOutput.m_originalVertexIndexesPerPartition);
		edgeGeomFree(PartitionerOutput.m_numUniqueVertexesPerPartition);
		edgeGeomFree(PartitionerOutput.m_ioBufferSizePerPartition);
		memset(&PartitionerOutput, 0, sizeof(EdgeGeomPartitionerOutput));
		return;
	}
	// This is the earliest point we could check to see whether the output arrays are large enough.
	if (OutInfo.NumVerticesReserved < OutInfo.NewNumVertices || OutInfo.NumTrianglesReserved < OutInfo.NewNumTriangles ||
		OutInfo.NumPartitionsReserved < OutInfo.NumPartitions)
	{
		// Clean up the partitioner output
		edgeGeomFree(PartitionerOutput.m_numTrianglesPerPartition);
		edgeGeomFree(PartitionerOutput.m_triangleListOut);
		edgeGeomFree(PartitionerOutput.m_originalVertexIndexesPerPartition);
		edgeGeomFree(PartitionerOutput.m_numUniqueVertexesPerPartition);
		edgeGeomFree(PartitionerOutput.m_ioBufferSizePerPartition);
		memset(&PartitionerOutput, 0, sizeof(EdgeGeomPartitionerOutput));
		return;
	}
	///////////////////////

	// Copy each partition's data into the new streams, inserting padding verts/tris as necessary
	// so that each partition's data starts on a 16-byte boundary.  The padding verts are
	// copied from the first vert of the preceding partition, so that they won't cause unexpected
	// behavior if they're accidentally referenced by code that blindly iterates over all verts.
	// The padding triangles are copied from the first index of the first triangle, so that they're
	// treated as degenerate triangles (but don't reference vertices outside the rest of the partition)

	WORD* NextTri  = OutInfo.NewIndices;
	UINT TriangleOffset = 0;
	UINT UniqueVertexOffset = 0;
	INT* NextRemapEntry = OutInfo.VertexRemapTable;
	WORD FirstPartitionVertex = 0;
	UINT FirstPartitionTriangle = 0;
	for(UINT PartitionIndex=0; PartitionIndex<NumPartitions; ++PartitionIndex)
	{
		OutInfo.PartitionFirstVertex[PartitionIndex] = FirstPartitionVertex;
		OutInfo.PartitionFirstTriangle[PartitionIndex] = FirstPartitionTriangle;
		OutInfo.PartitionNumVertices[PartitionIndex] = (WORD)(PartitionerOutput.m_numUniqueVertexesPerPartition[PartitionIndex]
			+ NumPaddingVerts[PartitionIndex]);
		OutInfo.PartitionNumTriangles[PartitionIndex] = (WORD)(PartitionerOutput.m_numTrianglesPerPartition[PartitionIndex]
			+ NumPaddingTris[PartitionIndex]);
		FirstPartitionVertex   = FirstPartitionVertex + OutInfo.PartitionNumVertices[PartitionIndex];
		FirstPartitionTriangle = FirstPartitionTriangle + OutInfo.PartitionNumTriangles[PartitionIndex];

		// Get pointers to this partition's triangles and unique vertex indexes.
		const UINT* PartitionTriangleList = PartitionerOutput.m_triangleListOut + TriangleOffset*3;
		const UINT  NumTriangles = PartitionerOutput.m_numTrianglesPerPartition[PartitionIndex];
		const UINT* PartitionOriginalVertexIndexes = PartitionerOutput.m_originalVertexIndexesPerPartition + UniqueVertexOffset;
		const UINT NumUniqueVertexes = PartitionerOutput.m_numUniqueVertexesPerPartition[PartitionIndex];
		TriangleOffset += NumTriangles;
		UniqueVertexOffset += NumUniqueVertexes;

		// Store maximum SPU buffer sizes.
		UINT NumSpuInputAttributes = SpuInputVertexFormat.m_numAttributes + SpuSecondaryInputVertexFormat.m_numAttributes;
		OutInfo.PartitionScratchBufferSize[PartitionIndex] = edgeGeomGetScratchBufferSizeInQwords(NumSpuInputAttributes, NumUniqueVertexes);
		OutInfo.PartitionIoBufferSize[PartitionIndex] = PartitionerOutput.m_ioBufferSizePerPartition[PartitionIndex];
		OutInfo.PartitionCommandBufferHoleSize[PartitionIndex] =
			(WORD)edgeGeomGetCommandBufferHoleSize(SpuOutputVertexFormat.m_numAttributes, NumTriangles*3);

		// Create the vertex remap table for this partition.  Each entry must be biased by ElementInfo.MinVertexIndex,
		// since they refer to the old vertex buffer ordering.
		
		// First, copy the entries from the partitioner output.
		for(UINT Vertex=0; Vertex<NumUniqueVertexes; ++Vertex)
		{
			NextRemapEntry[Vertex] = PartitionOriginalVertexIndexes[Vertex] + ElementInfo.MinVertexIndex;
		}
		NextRemapEntry += NumUniqueVertexes;
		// Now add the padding verts, copied from the first vertex in the partition
		for(UINT PaddingVertex=0; PaddingVertex<NumPaddingVerts[PartitionIndex]; ++PaddingVertex)
		{
			*NextRemapEntry = PartitionOriginalVertexIndexes[0] + ElementInfo.MinVertexIndex;
			NextRemapEntry++;
		}
		
		// Make index buffer.  Its entries will need to be biased by ElementInfo.NewMinVertexIndex, since they refer
		// to the new vertex buffer ordering.
		{
			const UINT* NextSourceTriangle = PartitionTriangleList;
			
			// Uncompressed indexes must be biased and padded.  Compressed indexes can simply be copied into
			// place.
			WORD PartitionIndexBias = IndexBias[PartitionIndex];
			OutInfo.PartitionIndexBias[PartitionIndex] = -IndexBias[PartitionIndex]; // Edge runtime requires a negative bias
			WORD paddingIndex = (WORD)NextSourceTriangle[0] + PartitionIndexBias; // used for padding triangles
			switch(PartitionerInput.m_indexListFlavor)
			{
			case kIndexesU16TriangleListCW:
			case kIndexesU16TriangleListCCW:
				// Copy the triangles into position, adding a bias as we go (so that the indexes
				// correctly refer to each vertex relative to the entire element's vertex buffer.
				// Note that when rendering each job, we will have to subtract this bias off 
				for(UINT TriangleIndex=0; TriangleIndex<NumTriangles; ++TriangleIndex)
				{
					NextTri[0] = (WORD)(NextSourceTriangle[0]) + PartitionIndexBias;
					NextTri[1] = (WORD)(NextSourceTriangle[1]) + PartitionIndexBias;
					NextTri[2] = (WORD)(NextSourceTriangle[2]) + PartitionIndexBias;
					NextTri += 3;
					NextSourceTriangle += 3;
				}
				// Padding triangles are copied from the partition's first index
				for(UINT PaddingVertex=0; PaddingVertex<NumPaddingTris[PartitionIndex]; ++PaddingVertex)
				{
					NextTri[0] = NextTri[1] = NextTri[2] = paddingIndex;
					NextTri += 3;
				}
				break;
			case kIndexesCompressedTriangleListCW:
			case kIndexesCompressedTriangleListCCW:
				// compressed indexes will only be used by Edge, so there's no need to pad them.
				// TODO: ???
				break;
			}
		}

		// Make skinning buffers
		// Not needed for static meshes

		// Make blend shape buffers
		// Not needed for static meshes

#if 0 // not building these yet
		// Make EdgeGeomSpuConfigInfo
		const UINT commandBufferHoleSize = edgeGeomGetCommandBufferHoleSize(SpuOutputVertexFormat.m_numAttributes, NumTriangles*3);
		BYTE PartitionFlags = 0;
		// Several Edge operations (e.g. blend shapes, culling, occluders, skinning) require an extra uniform table to be allocated
		// at runtime for the storage of temporary per-vertex data.  We set a flag here if any of these operations are used.
		if ( PartitionerInput.m_cullingFlavor != kCullNone || PartitionerInput.m_canBeOccluded
			 || PartitionerInput.m_skinningFlavor != kSkinNone)
		{
			PartitionFlags |= EDGE_GEOM_FLAG_INCLUDES_EXTRA_UNIFORM_TABLE;
		}
		BYTE* spuConfigInfo = NULL;
		BYTE inputVertexFormatId = edgeGeomGetSpuVertexFormatId(SpuInputVertexFormat);
		BYTE secondaryInputVertexFormatId = 0; // no secondary input stream for static meshes
		BYTE deltaFormatId = 0; // unused
		BYTE outputVertexFormatId = edgeGeomGetRsxVertexFormatId(SpuOutputVertexFormat);
		edgeGeomMakeSpuConfigInfo(NumUniqueVertexes, NumTriangles, PartitionerInput.m_numInputAttributes,
			PartitionFlags, PartitionerInput.m_indexListFlavor, PartitionerInput.m_skinningFlavor, 
			inputVertexFormatId, secondaryInputVertexFormatId, outputVertexFormatId, deltaFormatId,
			commandBufferHoleSize, &spuConfigInfo);
#endif
	}

	delete [] IndexBias;
	delete [] NumPaddingVerts;
	delete [] NumPaddingTris;

	// Clean up the partitioner output
	edgeGeomFree(PartitionerOutput.m_numTrianglesPerPartition);
	edgeGeomFree(PartitionerOutput.m_triangleListOut);
	edgeGeomFree(PartitionerOutput.m_originalVertexIndexesPerPartition);
	edgeGeomFree(PartitionerOutput.m_numUniqueVertexesPerPartition);
	edgeGeomFree(PartitionerOutput.m_ioBufferSizePerPartition);
	memset(&PartitionerOutput, 0, sizeof(EdgeGeomPartitionerOutput));
}


void FPS3SkeletalMeshCooker::Init()
{
}

void FPS3SkeletalMeshCooker::CookMeshElement(const FSkeletalMeshFragmentCookInfo& ElementInfo, FSkeletalMeshFragmentCookOutputInfo& OutInfo)
{
	if(ElementInfo.Indices && (ElementInfo.NumTriangles > 0) && OutInfo.NewIndices)
	{
		// the optimizer expects the index lists to be 32-bit instead of 16-bit, so we have to create new index lists
		// copy the 16-bit index array to the 32-bit index array
		UINT* InputIndices32 = new UINT[ElementInfo.NumTriangles * 3];
		memset(InputIndices32, 0, sizeof(UINT) * ElementInfo.NumTriangles * 3);
		for(UINT Index = 0; Index < ElementInfo.NumTriangles * 3; Index++)
		{
			*(InputIndices32 + Index) = (UINT)*(ElementInfo.Indices + Index);
		}

		UINT* OutputIndices32 = new UINT[ElementInfo.NumTriangles * 3];
		memset(OutputIndices32, 0, sizeof(UINT) * ElementInfo.NumTriangles * 3);

		EdgeGeomKCacheOptimizerUserData OptimizerParameters;
		memset( &OptimizerParameters, 0, sizeof(EdgeGeomKCacheOptimizerUserData) );
		// Edge 4.0: The m_fifoMissCost:m_lruMissCost ratio that has given the most consistent results is 4:1.
		// Edge 3.5: The m_fifoMissCost:m_lruMissCost ratio that has given the most consistent results is 3:1.
		OptimizerParameters.m_fifoMissCost = 3.0f;	// Cost factor for a single post-transform cache miss. Recommended to be the number of input attributes to the vertex program.
		OptimizerParameters.m_lruMissCost = 1.0f;	// Cost factor for a single mini-cache miss. Recommended to be four times the number of output attributes from the vertex program.
		OptimizerParameters.m_k1 = 1.0f;			// Reasonable default: 1.0
		OptimizerParameters.m_k2 = 0.25f;			// Reasonable default: 0.25
		OptimizerParameters.m_k3 = 0.8f;			// Reasonable default: 0.8

		edgeGeomKCacheOptimizer( InputIndices32, ElementInfo.NumTriangles, &OptimizerParameters, OutputIndices32 );

		// Convert the reordered, partitioned index buffer to 16-bit indices and write to the output buffer.
		for (UINT Index = 0; Index < ElementInfo.NumTriangles * 3; Index++)
		{
			OutInfo.NewIndices[Index] = (DWORD)OutputIndices32[Index];
		}

		delete [] InputIndices32;
		delete [] OutputIndices32;

		// These counts aren't changing yet; give them reasonable values
		OutInfo.NewNumTriangles = ElementInfo.NumTriangles;
		OutInfo.NewNumVertices = ElementInfo.MaxVertexIndex - ElementInfo.MinVertexIndex + 1;
	}
}

void FPS3StaticMeshCooker::Init()
{
}

void FPS3StaticMeshCooker::CookMeshElement(FMeshFragmentCookInfo& ElementInfo, FMeshFragmentCookOutputInfo& OutInfo)
{
	// Early-out if there's nothing to do
	if(ElementInfo.Indices == NULL || ElementInfo.NumTriangles == 0 || OutInfo.NewIndices == NULL)
		return;

	// The primary SPU input vertex stream contains only position for static geometry.
	EdgeGeomSpuVertexFormat SpuInputVertexFormat;
	SpuInputVertexFormat.m_numAttributes = 1;
	SpuInputVertexFormat.m_vertexStride = 3*sizeof(float);
	SpuInputVertexFormat.m_attributeDefinition[0].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_POSITION;
	SpuInputVertexFormat.m_attributeDefinition[0].m_byteOffset = 0;
	SpuInputVertexFormat.m_attributeDefinition[0].m_count = 3;
	SpuInputVertexFormat.m_attributeDefinition[0].m_type = (EdgeGeomSpuVertexAttributeType)EDGE_GEOM_ATTRIBUTE_FORMAT_F32;

	// The secondary SPU input vertex stream is empty for static geometry.
	EdgeGeomSpuVertexFormat SpuSecondaryInputVertexFormat;
	SpuSecondaryInputVertexFormat.m_numAttributes = 0;
	SpuSecondaryInputVertexFormat.m_vertexStride = 0;

	// The SPU output vertex stream contains only position as well.
	EdgeGeomRsxVertexFormat SpuOutputVertexFormat;
	SpuOutputVertexFormat.m_numAttributes = 1;
	SpuOutputVertexFormat.m_vertexStride = 3*sizeof(float);
	SpuOutputVertexFormat.m_attributeDefinition[0].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_POSITION;
	SpuOutputVertexFormat.m_attributeDefinition[0].m_byteOffset = 0;
	SpuOutputVertexFormat.m_attributeDefinition[0].m_count = 3;
	SpuOutputVertexFormat.m_attributeDefinition[0].m_type = (EdgeGeomRsxVertexAttributeType)EDGE_GEOM_ATTRIBUTE_FORMAT_F32;

	// The RSX-only vertex stream contains normals, tangents, vertex color and either full- or half-precision UVs.
	EdgeGeomRsxVertexFormat RsxOnlyVertexFormat;
	RsxOnlyVertexFormat.m_numAttributes = 4;
	// normal
	RsxOnlyVertexFormat.m_attributeDefinition[0].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_NORMAL;
	RsxOnlyVertexFormat.m_attributeDefinition[0].m_byteOffset = 0;
	RsxOnlyVertexFormat.m_attributeDefinition[0].m_count = 3;
	RsxOnlyVertexFormat.m_attributeDefinition[0].m_type = (EdgeGeomRsxVertexAttributeType)EDGE_GEOM_ATTRIBUTE_FORMAT_X11Y11Z10N;
	// tangent
	RsxOnlyVertexFormat.m_attributeDefinition[1].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_TANGENT;
	RsxOnlyVertexFormat.m_attributeDefinition[1].m_byteOffset = 4;
	RsxOnlyVertexFormat.m_attributeDefinition[1].m_count = 3;
	RsxOnlyVertexFormat.m_attributeDefinition[1].m_type = (EdgeGeomRsxVertexAttributeType)EDGE_GEOM_ATTRIBUTE_FORMAT_X11Y11Z10N;
	// vertex color
	RsxOnlyVertexFormat.m_attributeDefinition[2].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_COLOR;
	RsxOnlyVertexFormat.m_attributeDefinition[2].m_byteOffset = 8;
	RsxOnlyVertexFormat.m_attributeDefinition[2].m_count = 4;
	RsxOnlyVertexFormat.m_attributeDefinition[2].m_type = (EdgeGeomRsxVertexAttributeType)EDGE_GEOM_ATTRIBUTE_FORMAT_U8N;
	// texture coordinates
	RsxOnlyVertexFormat.m_attributeDefinition[3].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_UV0;
	RsxOnlyVertexFormat.m_attributeDefinition[3].m_byteOffset = 12;
	RsxOnlyVertexFormat.m_attributeDefinition[3].m_count = 4;
	if (ElementInfo.bUseFullPrecisionUVs)
	{
		RsxOnlyVertexFormat.m_vertexStride = 20;
		RsxOnlyVertexFormat.m_attributeDefinition[3].m_type = (EdgeGeomRsxVertexAttributeType)EDGE_GEOM_ATTRIBUTE_FORMAT_F32;
	}
	else
	{
		RsxOnlyVertexFormat.m_vertexStride = 16;
		RsxOnlyVertexFormat.m_attributeDefinition[3].m_type = (EdgeGeomRsxVertexAttributeType)EDGE_GEOM_ATTRIBUTE_FORMAT_F16;
	}

	SharedGeomCooking(ElementInfo, OutInfo, SpuInputVertexFormat, SpuSecondaryInputVertexFormat,
		SpuOutputVertexFormat, RsxOnlyVertexFormat);
}
