/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

// The fakesceneformat.* files contain the definitions of a hypothetical tools-side scene format
// and the functions which convert this format to the EdgeGeomGeometryUnits and EdgeGeomTriangleLists
// which are required by libedgegeomtool.  It is provided mainly for illustrative purposes.

#if !defined(FAKESCENEFORMAT_H)
#define FAKESCENEFORMAT_H

struct EdgeGeomScene;

struct FakeSceneFormat
{
	// Vertex data.  We store each attribute in its own stream.
	// All vertex data is in floating point format, with a fixed
	// number of components per vertex.
	unsigned m_vertexCount;
	float *m_positions; // 3 components/vertex
	float *m_normals; // 3 components/vertex
	float *m_tangents; // 4 components/vertex (tangent XYZ, plus a flip factor of 1.0 or -1.0 in W)
	float *m_uvs; // 2 components/vertex

	// Index data.  For each material, the following arrays contain an indexed
	// triangle list (three 32-bit indexes per triangle) and a triangle count.
	// The triangles are listed with a counterclockwise winding order, which is
	// what Edge expects by default.
	unsigned m_materialCount;
	unsigned **m_indexesPerMaterial;
	unsigned *m_triangleCountPerMaterial;
	// Each material also has a material ID, which is (in our case) an arbitrary integer.
	int *m_materialIdPerMaterial;

	// Skinning data.  For each vertex, we store four bone weights and four bone indexes.
	// The weights for each vertex must sum to 1.0.  A bone index of -1 means not used.  
	float *m_boneWeights;
	int *m_boneIndexes;

	void ConvertToEdgeScene(EdgeGeomScene *outScene) const;
};

#endif // FAKESCENEFORMAT_H
