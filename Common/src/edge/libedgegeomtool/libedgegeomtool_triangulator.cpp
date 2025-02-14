/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "edge/libedgegeomtool/libedgegeomtool_wrap.h"

//-------------------

void edgeGeomTriangulatePolygons(const int32_t *polygons, const int32_t *materialIdPerPolygon,
								   uint32_t **outTriangles, int32_t **outMaterialIdPerTriangle, uint32_t *outNumTriangles)
{
	// count the triangles first, so we know how big of an array to allocate.
	uint32_t triCount = 0;
	uint32_t index = 0;
	uint32_t vertexCountInPolygon = 0;

	// stop when we've reached our second -1 in the polygons array
	while (!(vertexCountInPolygon==0 && polygons[index]==-1 && index>0))  
	{
		if (polygons[index]!=-1)  // found a vertex
		{
			vertexCountInPolygon++;
		}
		else  // found the end of a polygon
		{
			vertexCountInPolygon = 0;
		}
		
		// for every vertex beyond 2 in a polygon, we have a triangle
		if (vertexCountInPolygon>=3)
		{
			triCount++;
		}
		
		index++;
	}

	// set up returned data
	*outNumTriangles = triCount;
	*outTriangles = (uint32_t*)edgeGeomAlloc(triCount*3*sizeof(uint32_t));
	*outMaterialIdPerTriangle = (int32_t*)edgeGeomAlloc(triCount*sizeof(int32_t));

	// second pass over triangles to fill out these arrays
	index = 0;
	vertexCountInPolygon = 0;
	int32_t firstVertInTriangle = 0;
	uint32_t currentTri = 0;
	uint32_t currentPolygon = 0;

	// stop when we've reached our second -1 in the polygons array
	while (!(vertexCountInPolygon==0 && polygons[index]==-1 && index>0))  
	{
		if (polygons[index]!=-1)  // found a vertex
		{
			vertexCountInPolygon++;
			if (vertexCountInPolygon==1)  // remember the first vertex in a polygon
			{
				firstVertInTriangle = polygons[index];
			}
		}
		else  // found the end of a polygon
		{
			vertexCountInPolygon = 0;
			currentPolygon++;
		}
		
		// for every vertex beyond 2 in a polygon, we have a triangle
		if (vertexCountInPolygon>=3)
		{
			// take the current vertex, the previous vertex, and the vertex that started this polygon to form a triangle
			(*outTriangles)[currentTri*3+0] = firstVertInTriangle;
			(*outTriangles)[currentTri*3+1] = polygons[index-1];
			(*outTriangles)[currentTri*3+2] = polygons[index];

			// replicate material id to each tri in polygon
			(*outMaterialIdPerTriangle)[currentTri] = materialIdPerPolygon[currentPolygon];  
			currentTri++;
		}
		
		index++;
	}
}

//-------------------
