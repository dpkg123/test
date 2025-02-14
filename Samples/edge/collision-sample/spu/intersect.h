/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __INTERSECT_H__
#define __INTERSECT_H__

#include <stdint.h>

struct RayResult{
    float intersection[4];
    float normal[4];
    float t;
    uint32_t padding0[3];
    uint32_t uid;
    uint32_t padding1[3];
};

extern "C" 
{
    void intersectRayTriangles(uint32_t numTriangles, void *pVertexes, void *pIndexes, void *ray, RayResult *pOut, float *transform);
#if 0
    void intersectRayTrianglesUnrolled(uint32_t numTriangles, void *pVertexes, void *pIndexes, void *ray, RayResult *pOut, float *transform);
    void intersectRayTrianglesC(uint32_t numTriangles, void *pVertexes, void *pIndexes, void *ray, RayResult *pOut, float *transform);
#endif
}

#endif
