/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "fakesceneformat.h" // describes the contrived input scene format
#include "utils.h" // Contains utility functions which are independent of the source scene format

#include "edge/libedgegeomtool/libedgegeomtool_wrap.h"

#if defined(WIN32)
#include	"crtdbg.h"
#endif

#include <cassert>
#include <cstdio>
#include <vector>

void main(void)
{
#if defined(WIN32)
	// Enables Visual Studio's built-in memory leak detection and configures it to
	// run automatically at program termination...
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	// ...and to print its output to stderr in addition to the debugger Output window.
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG );
	_CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDERR );
#endif // defined(WIN32)

	//-------------------
	// This block is loading geometry from some source and putting it into the EdgeGeomScene format.
	// If anything, this bit would depend on the studio- or tool-specific loading format.
	// The result must be a single merged input scene.
	//-------------------
	EdgeGeomScene edgeScene;
	{
		//------------------------------------------------
		// Replace this section with your studio's scene-loading code
		FILE *fakeFile = fopen("elephant.fake", "rb");
		fseek(fakeFile, 0, SEEK_END);
		size_t fakeFileSize = ftell(fakeFile);
		rewind(fakeFile);
		uint8_t *fileBytes = new uint8_t[fakeFileSize];
		uint32_t *fileInts = (uint32_t*)fileBytes;
		uint32_t baseAddress = (uint32_t)fileBytes;
		fread(fileBytes, 1, fakeFileSize, fakeFile);
		fclose(fakeFile);

		// First 4 bytes are a pointer count
		uint32_t patchCount = fileInts[0];
		// A table of file offsets to patch follows
		for(uint32_t iPatch=0; iPatch<patchCount; ++iPatch)
		{
			uint32_t patchOffset = fileInts[1+iPatch];
			uint32_t *ptr = fileInts + (patchOffset/4);
			*ptr += baseAddress;
		}
		// The FakeSceneFormat object is right after the pointer offset table
		FakeSceneFormat *fakeScene = (FakeSceneFormat*)(fileInts + 1 + patchCount);
		//--------------------------------------------------

		// Convert FakeSceneFormat to the EdgeGeomScene format
		fakeScene->ConvertToEdgeScene(&edgeScene);

		delete [] fileBytes;
	}

	// Build the output segment format.  These could be determined based on what
	// attributes are available, or by metadata attached to the scene, etc.
	EdgeGeomSpuVertexFormat *normalTangentFormat = (EdgeGeomSpuVertexFormat*)edgeGeomAlloc(sizeof(EdgeGeomSpuVertexFormat));
	normalTangentFormat->m_numAttributes = 2;
	normalTangentFormat->m_vertexStride = 6;
	normalTangentFormat->m_attributeDefinition[0].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_NORMAL;
	normalTangentFormat->m_attributeDefinition[0].m_byteOffset = 0;
	normalTangentFormat->m_attributeDefinition[0].m_count = 3;
	normalTangentFormat->m_attributeDefinition[0].m_type = kSpuAttr_FixedPoint;
	for(uint32_t iComponent=0; iComponent<3; ++iComponent)
	{
		normalTangentFormat->m_attributeDefinition[0].m_fixedPointBitDepthInteger[iComponent] = 2;
		normalTangentFormat->m_attributeDefinition[0].m_fixedPointBitDepthFractional[iComponent] = 6;
	}
	normalTangentFormat->m_attributeDefinition[1].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_TANGENT;
	normalTangentFormat->m_attributeDefinition[1].m_byteOffset = 3;
	normalTangentFormat->m_attributeDefinition[1].m_count = 4;
	normalTangentFormat->m_attributeDefinition[1].m_type = kSpuAttr_UnitVector;

	EdgeGeomRsxVertexFormat *uvFormat = (EdgeGeomRsxVertexFormat*)edgeGeomAlloc(sizeof(EdgeGeomRsxVertexFormat));
	uvFormat->m_numAttributes = 1;
	uvFormat->m_vertexStride = 4;
	uvFormat->m_attributeDefinition[0].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_UV0;
	uvFormat->m_attributeDefinition[0].m_byteOffset = 0;
	uvFormat->m_attributeDefinition[0].m_count = 2;
	uvFormat->m_attributeDefinition[0].m_type = kRsxAttr_F16;

	EdgeGeomSegmentFormat edgeFormat;
	memset(&edgeFormat, 0, sizeof(EdgeGeomSegmentFormat));
	edgeFormat.m_spuInputVertexFormats[0] = edgeGeomGetSpuVertexFormat(EDGE_GEOM_SPU_VERTEX_FORMAT_F32c3); // F32 position
	edgeFormat.m_spuInputVertexFormats[1] = normalTangentFormat; // normals and tangents in the secondary stream
	edgeFormat.m_spuInputVertexDeltaFormat = NULL; // no blend shapes
	edgeFormat.m_spuOutputVertexFormat = edgeGeomGetRsxVertexFormat(EDGE_GEOM_RSX_VERTEX_FORMAT_F32c3_X11Y11Z10N_X11Y11Z10N);
	edgeFormat.m_rsxOnlyVertexFormat = uvFormat; // UVs sent directly to the RSX
	edgeFormat.m_skinType = kSkinNoScaling;
	edgeFormat.m_indexesType = kIndexesCompressedTriangleListCCW;
	edgeFormat.m_skinMatrixFormat = kMatrix3x4RowMajor;

	// Merge any identical vertices, to save memory and make it possible to determine topology.
	printf("Merging identical vertexes...");
	uint32_t numDuplicates = edgeGeomMergeIdenticalVertexes(edgeScene);
	printf("%d (out of %d) vertexes removed\n", numDuplicates, numDuplicates + edgeScene.m_numVertexes);
	
	// Partition the geometry.  Triangle lists which were determined earlier to be unskinned
	// can be partitioned separately for more efficient partitions.
	printf("Partitioning scene into segments...\n");
	EdgeGeomSegment *segments = NULL;
	uint32_t numSegments = 0;
	edgeGeomPartitionSceneIntoSegments(edgeScene, edgeFormat, &segments, &numSegments);

	// At this point, the scene is no longer needed
	edgeGeomFree(edgeScene.m_vertexes);
	edgeGeomFree(edgeScene.m_vertexDeltas);
	edgeGeomFree(edgeScene.m_triangles);
	edgeGeomFree(edgeScene.m_materialIdPerTriangle);
	edgeGeomFree(edgeScene.m_vertexAttributeIds);
	edgeGeomFree(edgeScene.m_vertexAttributeIndexes);
	edgeGeomFree(edgeScene.m_blendedAttributeIds);
	edgeGeomFree(edgeScene.m_blendedAttributeIndexes);
	edgeGeomFree(edgeScene.m_matrixIndexesPerVertex);
	edgeGeomFree(edgeScene.m_skinningWeightsPerVertex);

	// now, dump all the segments into a big header file
	printf("Dumping scene to elephant.edge.h...\n");
	DumpToFile("elephant.edge.h", segments, numSegments);

	// Delete the vertex formats
	if (edgeFormat.m_spuInputVertexFormats[0] != NULL)
		edgeGeomFree(edgeFormat.m_spuInputVertexFormats[0]);
	if (edgeFormat.m_spuInputVertexFormats[1] != NULL)
		edgeGeomFree(edgeFormat.m_spuInputVertexFormats[1]);
	if (edgeFormat.m_spuInputVertexDeltaFormat != NULL)
		edgeGeomFree(edgeFormat.m_spuInputVertexDeltaFormat);
	if (edgeFormat.m_spuOutputVertexFormat != NULL)
		edgeGeomFree(edgeFormat.m_spuOutputVertexFormat);
	if (edgeFormat.m_rsxOnlyVertexFormat != NULL)
		edgeGeomFree(edgeFormat.m_rsxOnlyVertexFormat);


	// Delete the segments.  Remember to delete each segment's data buffers first!
	for(uint32_t iSegment=0; iSegment<numSegments; ++iSegment)
	{
		edgeGeomFreeSegmentData(segments[iSegment]);
	}
	edgeGeomFree(segments);


#if defined(WIN32) && defined(_DEBUG) // useful for tracking memory leaks
	printf("Printing leaked allocations...\n");
	_CrtMemDumpAllObjectsSince( NULL );
	printf(""); // put a breakpoint here
#endif
}

//-------------------
