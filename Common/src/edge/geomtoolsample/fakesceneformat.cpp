/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "fakesceneformat.h"

#include "edge/libedgegeomtool/libedgegeomtool_wrap.h"

// We use the STL here for convenience; Edge itself does not use any templates.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>

void FakeSceneFormat::ConvertToEdgeScene(EdgeGeomScene *outScene) const
{
	memset(outScene, 0, sizeof(EdgeGeomScene));

	// Create the input index buffer.
	// Edge Geometry expects its index data as a flat list with 3 indexes per triangle, with
	// the a counterclockwise winding order.  If your scene format uses triangle strips,
	// N-sided polygons, or some other index format, you'll have to convert it here.
	// See the edgeGeomTriangulatePolygons() function for more information.
	//
	// For the FakeSceneFormat, we have to merge its per-material index buffers into one unified
	// buffer, along with a separate buffer that stores the material index of each triangle.
	uint32_t totalTriangleCount = 0;
	for(uint32_t iMaterial=0; iMaterial<m_materialCount; ++iMaterial)
	{
		totalTriangleCount += m_triangleCountPerMaterial[iMaterial];
	}

	outScene->m_numTriangles = totalTriangleCount;
	outScene->m_triangles = (uint32_t*)edgeGeomAlloc(totalTriangleCount*3*sizeof(uint32_t)); // 3 indexes per triangle
	outScene->m_materialIdPerTriangle = (int32_t*)edgeGeomAlloc(totalTriangleCount*sizeof(int32_t));
	uint32_t destTriangleIndex = 0;
	for(uint32_t iMaterial=0; iMaterial<m_materialCount; ++iMaterial)
	{
		const uint32_t *srcIndexes = m_indexesPerMaterial[iMaterial];
		const int32_t materialId = m_materialIdPerMaterial[iMaterial];
		for(uint32_t iTriangle=0; iTriangle<m_triangleCountPerMaterial[iMaterial]; ++iTriangle)
		{
			outScene->m_triangles[3*destTriangleIndex+0] = srcIndexes[3*iTriangle+0];
			outScene->m_triangles[3*destTriangleIndex+1] = srcIndexes[3*iTriangle+1];
			outScene->m_triangles[3*destTriangleIndex+2] = srcIndexes[3*iTriangle+2];
			outScene->m_materialIdPerTriangle[destTriangleIndex] = materialId;
			destTriangleIndex++;
		}
	}

	// Create the input vertex data.  The EdgeGeomScene stores its vertex data as an interleaved
	// array of floats. The input flavor you choose later will determine which of these attributes
	// will actually get processed and exported.
	std::vector<float> vertexes;
	for (uint32_t iVertex=0; iVertex<m_vertexCount; iVertex++)
	{
		vertexes.push_back(m_positions[3*iVertex+0]);	// attribute index 0
		vertexes.push_back(m_positions[3*iVertex+1]);
		vertexes.push_back(m_positions[3*iVertex+2]);
		vertexes.push_back(m_normals[3*iVertex+0]);		// attribute index 3
		vertexes.push_back(m_normals[3*iVertex+1]);
		vertexes.push_back(m_normals[3*iVertex+2]);
		vertexes.push_back(m_uvs[2*iVertex+0]);			// attribute index 6
		vertexes.push_back(m_uvs[2*iVertex+1]);
		vertexes.push_back(m_tangents[4*iVertex+0]);	// attribute index 8
		vertexes.push_back(m_tangents[4*iVertex+1]);
		vertexes.push_back(m_tangents[4*iVertex+2]);
		vertexes.push_back(m_tangents[4*iVertex+3]);
		//vertexes.push_back(m_binormals[3*iVertex+0]);	// attribute index 12
		//vertexes.push_back(m_binormals[3*iVertex+1]);
		//vertexes.push_back(m_binormals[3*iVertex+2]);
		//vertexes.push_back(m_colors[3*iVertex+0]);	// attribute index 15
		//vertexes.push_back(m_colors[3*iVertex+1]);
		//vertexes.push_back(m_colors[3*iVertex+2]);
	}

	// Create the vertex attribute description tables, which describe the identities and locations
	// the attributes in the vertex buffer created above.
	std::vector<EdgeGeomAttributeId> vertexAttributeIds;
	vertexAttributeIds.push_back(EDGE_GEOM_ATTRIBUTE_ID_POSITION);
	vertexAttributeIds.push_back(EDGE_GEOM_ATTRIBUTE_ID_NORMAL);
	vertexAttributeIds.push_back(EDGE_GEOM_ATTRIBUTE_ID_UV0);
	vertexAttributeIds.push_back(EDGE_GEOM_ATTRIBUTE_ID_TANGENT);

	std::vector<uint16_t> vertexAttributeIndexes;
	vertexAttributeIndexes.push_back(0); // position
	vertexAttributeIndexes.push_back(3); // normal
	vertexAttributeIndexes.push_back(6); // UV
	vertexAttributeIndexes.push_back(8); // tangent

	// Allocate the final buffers, and fill them with the data we've generated above
	outScene->m_numFloatsPerVertex = (uint32_t)vertexes.size() / m_vertexCount;
	outScene->m_numVertexes = m_vertexCount;
	outScene->m_numVertexAttributes = 4; // position, normal, uv, tangent
	outScene->m_vertexes = (float*)edgeGeomAlloc(vertexes.size() * sizeof(float));
	outScene->m_vertexAttributeIds = (EdgeGeomAttributeId*)edgeGeomAlloc(outScene->m_numVertexAttributes * sizeof(EdgeGeomAttributeId));
	outScene->m_vertexAttributeIndexes = (uint16_t*)edgeGeomAlloc(outScene->m_numVertexAttributes*sizeof(uint16_t));
	memcpy(outScene->m_vertexes, &vertexes[0], sizeof(float) * vertexes.size());
	memcpy(outScene->m_vertexAttributeIds, &vertexAttributeIds[0], outScene->m_numVertexAttributes * sizeof(EdgeGeomAttributeId));
	memcpy(outScene->m_vertexAttributeIndexes, &vertexAttributeIndexes[0], outScene->m_numVertexAttributes * sizeof(uint16_t));

	// We don't have any blend shape data in our test scene, but if we did, this
	// is roughly how we'd process it:
#if 0
	// Determine the number of blend shapes by scraping the blendshape nodes looking for this mesh's name
	outScene->m_numBlendShapes = <YOUR COUNT HERE>
	
	// Set up blend shape attribute indices, so the scene knows what attributes are in the
	// blended attribute table
	if (outScene->m_numBlendShapes)
	{
		// For now, we assume all attributes are blended.  In reality, blend shapes will probably
		// only affect a subset of the scene's attributes (for instance, texture coordinates are rarely
		// blended).  The important point here is that blended attribute tables have the exact same
		// format as the main vertex attribute tables.
		outScene->m_numBlendedAttributes = outScene->m_numVertexAttributes;
		outScene->m_numFloatsPerDelta = outScene->m_numFloatsPerVertex;
		memcpy(outScene->m_blendedAttributeIds, outScene->m_vertexAttributeIds,
			outScene->m_numBlendedAttributes*sizeof(EdgeGeomAttributeId));
		memcpy(outScene->m_blendedAttributeIndexes, outScene->m_vertexAttributeIndexes,
			outScene->m_numBlendedAttributes*sizeof(uint16_t));

		// now, collect all the blended attributes.  Each blend shape is stored analagously to the main vertex data array,
		// and each shape's data starts immediately after the previous one ends.
		outScene->m_vertexDeltas = (float*)edgeGeomAlloc(outScene->m_numFloatsPerDelta * outScene->m_numVertexes * outScene->m_numBlendShapes * sizeof(float));
		uint32_t currentBlendShape = 0;
		for (uint32_t iBlendShape=0; iBlendShape<outScene->m_numBlendShapes; iBlendShape++)
		{
			// Collect the deltas for each vertex in the current shape, and write them to the end of the array.
			// Since we chose to copy the main vertex attribute ids/indexes arrays above, we must store the
			// attributes in the same order as the main vertex data.
			std::vector<float> shapeDeltas;
			for (uint32_t iVertex=0; iVertex<bsVertexIds.size(); iVertex++)
			{
				// We assume here that the FakeSceneFormat stores each attribute's delta data in a different array, rather
				// than the interleaved format that the EdgeGeomScene format expects.
				shapeDeltas.push_back(m_bsPositions[iBlendShape][3*iVertex+0] - m_positions[3*iVertex+0]);   // attribute index 0
				shapeDeltas.push_back(m_bsPositions[iBlendShape][3*iVertex+1] - m_positions[3*iVertex+1]);	 
				shapeDeltas.push_back(m_bsPositions[iBlendShape][3*iVertex+2] - m_positions[3*iVertex+2]);	 
				shapeDeltas.push_back(m_bsNormals[iBlendShape][3*iVertex+0] - m_normals[3*iVertex+0]);		 // attribute index 3
				shapeDeltas.push_back(m_bsNormals[iBlendShape][3*iVertex+1] - m_normals[3*iVertex+1]);		 
				shapeDeltas.push_back(m_bsNormals[iBlendShape][3*iVertex+2] - m_normals[3*iVertex+2]);		 
				shapeDeltas.push_back(m_bsUvs[iBlendShape][2*iVertex+0] - m_uvs[3*iVertex+0]);				 // attribute index 6
				shapeDeltas.push_back(m_bsUvs[iBlendShape][2*iVertex+1] - m_uvs[3*iVertex+1]);				 
				shapeDeltas.push_back(m_bsTangents[iBlendShape][4*iVertex+0] - m_tangents[4*iVertex+0]);	 // attribute index 8
				shapeDeltas.push_back(m_bsTangents[iBlendShape][4*iVertex+1] - m_tangents[4*iVertex+1]);	 
				shapeDeltas.push_back(m_bsTangents[iBlendShape][4*iVertex+2] - m_tangents[4*iVertex+2]);	 
				shapeDeltas.push_back(m_bsTangents[iBlendShape][4*iVertex+3] - m_tangents[4*iVertex+3]);	 
				//shapeDeltas.push_back(m_bsBinormals[iBlendShape][3*iVertex+0] - m_binormals[3*iVertex+0]); // attribute index 12
				//shapeDeltas.push_back(m_bsBinormals[iBlendShape][3*iVertex+1] - m_binormals[3*iVertex+1]); 
				//shapeDeltas.push_back(m_bsBinormals[iBlendShape][3*iVertex+2] - m_binormals[3*iVertex+2]); 
				//shapeDeltas.push_back(m_bsColors[iBlendShape][3*iVertex+0] - m_colors[3*iVertex+0]);		 // attribute index 15
				//shapeDeltas.push_back(m_bsColors[iBlendShape][3*iVertex+1] - m_colors[3*iVertex+1]);
				//shapeDeltas.push_back(m_bsColors[iBlendShape][3*iVertex+2] - m_colors[3*iVertex+2]);
			}

			// copy this shape's deltas to the end of the scene's deltas array
			memcpy(outScene->m_vertexDeltas + iBlendShape*outScene->m_numFloatsPerDelta*outScene->m_numVertexes, 
						&shapeDeltas[0], sizeof(float)*shapeDeltas.size());
		}
	}
#endif

	// Set up skinning weights per vertex. 	Edge Geometry supports up to four bones per vertex;
	// if your source data has more bones, you'll have to pick the four with the
	// strongest influence and rescale their weights to add up to 1.0f.
	outScene->m_matrixIndexesPerVertex   = (int32_t*)edgeGeomAlloc(outScene->m_numVertexes*4*sizeof(int32_t));
	outScene->m_skinningWeightsPerVertex = (float*)edgeGeomAlloc(outScene->m_numVertexes*4*sizeof(float));

	// This code demonstrates how to choose the four most significant bones for each vertexes
	// and rescale their weights to sum to 1.0f.  It's not necessary to run on our sample data
	// (which already has four properly weighted bones per vertex), but it doesn't hurt.
	std::vector<std::pair<float, int32_t> > jointWeights;  // this is a pair of a weight and its associated bone index
	for (uint32_t iVertex=0; iVertex<outScene->m_numVertexes; iVertex++)
	{
		// Extract all bone weights and indexes for this vertex into the jointWeights array
		jointWeights.clear();
		jointWeights.push_back(std::make_pair(m_boneWeights[4*iVertex+0], m_boneIndexes[4*iVertex+0]));
		jointWeights.push_back(std::make_pair(m_boneWeights[4*iVertex+1], m_boneIndexes[4*iVertex+1]));
		jointWeights.push_back(std::make_pair(m_boneWeights[4*iVertex+2], m_boneIndexes[4*iVertex+2]));
		jointWeights.push_back(std::make_pair(m_boneWeights[4*iVertex+3], m_boneIndexes[4*iVertex+3]));
		
		// now, take the top four influences
		std::sort(jointWeights.begin(), jointWeights.end(), std::greater<std::pair<float,int> >());

		// pad to 4 entries, to simplify the logic below
		while (jointWeights.size()<4)
			jointWeights.push_back(std::make_pair(0.0f, -1));

		// normalize the top four
		float weightSum = 0;
		for (uint32_t iWeight=0; iWeight<4; iWeight++)
		{
			// if there is NO WAY that a weight can ever be rounded up to a whole value, it's better to mark 
			// the matrix as unused, so it does not pull in unnecessary matrices during partitioning
			if (jointWeights[iWeight].first < (1.0f/255.0f)*0.75f)
			{
				jointWeights[iWeight] = std::make_pair(0.0f, -1);
			}
			else
			{
				weightSum += jointWeights[iWeight].first;
			}
		}

		// dump out the weights, renormalizing in the process
		float scaledSum = 0.0f;  // accumulate the scaled sums, so we can set the highest weight precisely to make them equal 1.0
		for (uint32_t iWeight=0; iWeight<4; iWeight++)
		{
			outScene->m_matrixIndexesPerVertex[iVertex*4 + iWeight] = jointWeights[iWeight].second;
			if (iWeight!=0)  // skip the first weight, so we know what the remainder should be
			{
				const float scaledWeight = (weightSum!=0?jointWeights[iWeight].first/weightSum:0.0f);
				scaledSum += scaledWeight;
				outScene->m_skinningWeightsPerVertex[iVertex*4 + iWeight] = scaledWeight;
			}
		}
		
		// finally, rewrite the top weight to be exactly the difference between the sum of 1,2,3's weights and 1.0
		outScene->m_skinningWeightsPerVertex[iVertex*4 + 0] = 1.0f - scaledSum;
	}
}
