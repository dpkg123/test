/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <cell/cell_fs.h>
#include <cell/gcm.h>
#include <cell/spurs.h>
#include <cell/sysmodule.h>
#include <sys/spu_initialize.h>
#include <ppu_intrinsics.h>
#include <spu_printf.h>

#include "gcm/FWCellGCMWindow.h"
#include "FWDebugFont.h"
#include "FWTime.h"
#include "gcmutil.h"
#include "../common/profile_bars.h"

#include "edge/geom/edgegeom_structs.h"
#include "spu/job_geom_interface.h"

#include "main.h"

#define SPURS_SPU_NUM					1
#define SPU_THREAD_GROUP_PRIORITY		250
#define EVENT_FLAG_BIT					(0U)					// bit used by job_send_event (between 0 and 15)
#define EVENT_FLAG_BITS					(1U<<(EVENT_FLAG_BIT))	// bit mask for job_send_event

const uint32_t EDGE_SHARED_OUTPUT_BUFFER_SIZE = 1*1024*1024;
const uint32_t EDGE_RING_OUTPUT_BUFFER_SIZE = 512*1024; // per SPU
const uint32_t COMMAND_BUFFER_SIZE = 4*1024*1024;

#define MAX_NUM_BLENDSHAPES 2 // arbitrary limit
#define MAX_SEGMENTS 0x100

// This must be the index of the first of a contiguous block of RSX labels,
// one for each SPU.  Labels 0-63 are reserved by the OS, and your application
// and its libraries may have reserved others.  128 seems safe for this sample app.
const uint32_t kFirstRsxLabelIndex = 128;

using namespace cell;
using namespace cell::Gcm;

// Skinning matrix data
static float __attribute__((aligned(16))) s_skinningMatrices[1*12];
static const uint32_t kNumBones = 1;

// Shader (embedded)
extern uint32_t _binary_vpshader_vpo_start;
extern uint32_t _binary_vpshader_vpo_end;
extern uint32_t _binary_fpshader_fpo_start;
extern uint32_t _binary_fpshader_fpo_end;

// Embedded SPU ELF symbols
extern char _binary_job_job_geom_spu_bin_start[];
extern char _binary_job_job_geom_spu_bin_size[];
extern char _binary_job_job_send_event_spu_bin_start[];	
extern char _binary_job_job_send_event_spu_bin_size[];

// instantiate the class
SampleApp app;

static CellSpursJob256 jobs[MAX_SEGMENTS];

static EdgeGeomBlendShapeInfo shapeInfos[MAX_SEGMENTS*MAX_NUM_BLENDSHAPES] __attribute__((__aligned__(128)));
static uint32_t numShapeInfos = 0;
static float shapeAlphas[MAX_NUM_BLENDSHAPES] = {0};

EdgeGeomScene edgeScene;
EdgeGeomSegmentFormat edgeFormat;
EdgeGeomSegment *edgeSegments = NULL;
uint32_t numEdgeSegments = 0;

static const unsigned int textureDimension = 512;

struct SphereVert
{
	float m_pos[3];
	float m_norm[3];
	float m_tan[4];
	float m_tex[2];
};

static inline void SetVec(float *vec, float x, float y) { vec[0] = x; vec[1] = y; }
static inline void SetVec(float *vec, float x, float y, float z) { vec[0] = x; vec[1] = y; vec[2] = z; }
static inline void SetVec(float *vec, float x, float y, float z, float w) { vec[0] = x; vec[1] = y; vec[2] = z; vec[3] = w; }
static inline void SetVec2(float *vec, float *from) { vec[0] = from[0]; vec[1] = from[1]; }
static inline void SetVec3(float *vec, float *from) { vec[0] = from[0]; vec[1] = from[1]; vec[2] = from[2]; }
static inline void AddVec3(float *vec, float *from) { vec[0] += from[0]; vec[1] += from[1]; vec[2] += from[2]; }

static void BuildSphereScene(float radius, uint32_t xdiv, uint32_t ydiv, EdgeGeomScene *outScene)
{
	uint32_t vertexCount = (xdiv + 1) * (ydiv + 1);
	uint32_t triangleCount = xdiv * (ydiv - 1) * 2;

	uint32_t *outI = (uint32_t*)malloc(triangleCount*3*sizeof(uint32_t));
	SphereVert *outV = (SphereVert*)malloc(vertexCount*sizeof(SphereVert));

	float gx = 6.283185307179586f / (float) xdiv;
	float gy = 3.141592653589793f / (float) ydiv;

	for (uint32_t i = 0; i < xdiv; ++i)
	{
		float theta = (float)i * gx;
		float ct = cosf(theta);
		float st = sinf(theta);

		uint32_t k = i * (ydiv + 1);
		for (uint32_t j = 1; j < ydiv; ++j)
		{
			float phi = (float) j * gy;
			float sp = sinf(phi);
			float x = ct * sp;
			float y = st * sp;
			float z = cosf(phi);
			SetVec(outV[k+j].m_pos, x*radius,y*radius,z*radius);
			SetVec(outV[k+j].m_norm, x,y,z);
			SetVec(outV[k+j].m_tex, 
				theta * 0.1591549430918953f,
				1.0F - phi * 0.31830988618379f);
		}
	}

	uint32_t kk = xdiv * (ydiv + 1);
	for (uint32_t j = 1; j < ydiv; ++j)
	{
		float phi = (float)j * gy;
		float x = sinf(phi);
		float z = cosf(phi);
		SetVec(outV[kk+j].m_pos, x*radius,0,z*radius);
		SetVec(outV[kk+j].m_norm, x,0,z);
		SetVec(outV[kk+j].m_tex, 
			1, 
			1.f - phi * 0.31830988618379f);
	}

	for (uint32_t i = 0; i < xdiv; i++)
	{
		uint32_t k1 = i * (ydiv + 1) + 1;
		uint32_t k2 = (i + 1) * (ydiv + 1) + 1;
		float s = (outV[k1].m_tex[0] + outV[k2].m_tex[0]) * 0.5f;
		SetVec(outV[k1-1].m_pos, 0,0,radius);
		SetVec(outV[k1-1].m_norm, 0,0,1);
		SetVec(outV[k1-1].m_tex, s,1);
		SetVec(outV[k1+ydiv-1].m_pos, 0,0,-radius);
		SetVec(outV[k1+ydiv-1].m_norm, 0,0,-1);
		SetVec(outV[k1+ydiv-1].m_tex, s,0);
	}

	SetVec3(outV[xdiv*(ydiv+1)].m_pos, outV[0].m_pos);
	SetVec3(outV[xdiv*(ydiv+1)].m_norm, outV[0].m_norm);
	SetVec2(outV[xdiv*(ydiv+1)].m_tex, outV[0].m_tex);
	SetVec3(outV[xdiv*(ydiv+1)+ydiv].m_pos, outV[ydiv].m_pos);
	SetVec3(outV[xdiv*(ydiv+1)+ydiv].m_norm, outV[ydiv].m_norm);
	SetVec2(outV[xdiv*(ydiv+1)+ydiv].m_tex, outV[ydiv].m_tex);

	uint32_t ii = 0;
	for(uint32_t i = 0; i < xdiv; ++i)
	{
		uint32_t k = i * (ydiv + 1);
		outI[ii+0] = (uint16_t) k;
		outI[ii+1] = (uint16_t) (k + 1);
		outI[ii+2] = (uint16_t) (k + ydiv + 2);
		ii += 3;

		for(uint32_t j = 1; j < ydiv - 1; ++j)
		{
			outI[ii+0] = (uint16_t) (k + j);
			outI[ii+1] = (uint16_t) (k + j + 1);
			outI[ii+2] = (uint16_t) (k + j + ydiv + 2);
			outI[ii+3] = (uint16_t) (k + j);
			outI[ii+4] = (uint16_t) (k + j + ydiv + 2);
			outI[ii+5] = (uint16_t) (k + j + ydiv + 1);
			ii += 6;
		}

		outI[ii+0] = (uint16_t) (k + ydiv - 1);
		outI[ii+1] = (uint16_t) (k + ydiv);
		outI[ii+2] = (uint16_t) (k + ydiv * 2);
		ii += 3;
	}

	// Double texcoords
	for(uint32_t i = 0; i < vertexCount; ++i)
	{
		outV[i].m_tex[0] *= 4.f;
		outV[i].m_tex[1] *= 2.f;
	}

	// Calculate tangents
	float *tan1 = (float*)calloc(vertexCount*3, 4);
	float *tan2 = (float*)calloc(vertexCount*3, 4);
	for(uint32_t i = 0; i < triangleCount; ++i)
	{
		uint32_t i1 = outI[i*3+0];
		uint32_t i2 = outI[i*3+1];
		uint32_t i3 = outI[i*3+2];
		const float *v1 = outV[i1].m_pos;
		const float *v2 = outV[i2].m_pos;
		const float *v3 = outV[i3].m_pos;
		const float *w1 = outV[i1].m_tex;
		const float *w2 = outV[i2].m_tex;
		const float *w3 = outV[i3].m_tex;
		float x1 = v2[0] - v1[0];
		float x2 = v3[0] - v1[0];
		float y1 = v2[1] - v1[1];
		float y2 = v3[1] - v1[1];
		float z1 = v2[2] - v1[2];
		float z2 = v3[2] - v1[2];
		float s1 = w2[0] - w1[0];
		float s2 = w3[0] - w1[0];
		float t1 = w2[1] - w1[1];
		float t2 = w3[1] - w1[1];
		float r = 1.f / (s1*t2-s2*t1);
		float sdir[3] = { (t2*x1-t1*x2)*r, (t2*y1-t1*y2)*r, (t2*z1-t1*z2)*r };
		float tdir[3] = { (s1*x2-s2*x1)*r, (s1*y2-s2*y1)*r, (s1*z2-s2*z1)*r };
		AddVec3(&tan1[i1*3], sdir);
		AddVec3(&tan1[i2*3], sdir);
		AddVec3(&tan1[i3*3], sdir);
		AddVec3(&tan2[i1*3], tdir);
		AddVec3(&tan2[i2*3], tdir);
		AddVec3(&tan2[i3*3], tdir);
	}

	uint32_t count = vertexCount;
	for(uint32_t i = 0; i < count; ++i)
	{
		const float *n = outV[i].m_norm;
		const float *t = &tan1[i*3];
		float nDotT = n[0]*t[0] + n[1]*t[1] + n[2]*t[2];
		float tan_a[3] = { t[0]-n[0]*nDotT, t[1]-n[1]*nDotT, t[2]-n[2]*nDotT };
		float ooLen = 1.f/sqrtf(tan_a[0]*tan_a[0] + tan_a[1]*tan_a[1] + tan_a[2]*tan_a[2]);
		SetVec(outV[i].m_tan, tan_a[0]*ooLen, tan_a[1]*ooLen, tan_a[2]*ooLen, 1);
	}

	free(tan1);
	free(tan2);

	// Fill in the output scene object
	outScene->m_numTriangles = triangleCount;
	outScene->m_triangles = outI;
	outScene->m_materialIdPerTriangle = (int32_t*)malloc(triangleCount*sizeof(int32_t));
	outScene->m_numVertexes = vertexCount;
	outScene->m_numFloatsPerVertex = 3+3+4+2;
	outScene->m_vertexes = (float*)outV;
	outScene->m_numVertexAttributes = 4;
	outScene->m_vertexAttributeIndexes = (uint16_t*)malloc(outScene->m_numVertexAttributes*sizeof(uint16_t));
	outScene->m_vertexAttributeIds = (EdgeGeomAttributeId*)malloc(outScene->m_numVertexAttributes*sizeof(EdgeGeomAttributeId));
	// One blend shape, which elongates the sphere along the Z axis (position only; normals/tangents
	// will be rendered slightly inaccurately, but I'm too lazy to recalculate them correctly.
	outScene->m_numBlendShapes = 1;
	outScene->m_numFloatsPerDelta = 3;
	outScene->m_vertexDeltas = (float*)malloc(outScene->m_numVertexes*3*sizeof(float));
	outScene->m_numBlendedAttributes = 1;
	outScene->m_blendedAttributeIndexes = (uint16_t*)malloc(outScene->m_numBlendedAttributes*sizeof(uint16_t));
	outScene->m_blendedAttributeIds = (EdgeGeomAttributeId*)malloc(outScene->m_numBlendedAttributes*sizeof(EdgeGeomAttributeId));
	// Very simple skinning data
	outScene->m_matrixIndexesPerVertex = (int32_t*)malloc(outScene->m_numVertexes*4*sizeof(int32_t));
	outScene->m_skinningWeightsPerVertex = (float*)malloc(outScene->m_numVertexes*4*sizeof(float));

	memset(outScene->m_materialIdPerTriangle, 0, triangleCount*sizeof(int32_t));

	outScene->m_vertexAttributeIndexes[0] = 0;
	outScene->m_vertexAttributeIndexes[1] = 3;
	outScene->m_vertexAttributeIndexes[2] = 6;
	outScene->m_vertexAttributeIndexes[3] = 10;

	outScene->m_vertexAttributeIds[0] = EDGE_GEOM_ATTRIBUTE_ID_POSITION;
	outScene->m_vertexAttributeIds[1] = EDGE_GEOM_ATTRIBUTE_ID_NORMAL;
	outScene->m_vertexAttributeIds[2] = EDGE_GEOM_ATTRIBUTE_ID_TANGENT;
	outScene->m_vertexAttributeIds[3] = EDGE_GEOM_ATTRIBUTE_ID_UV0;

	for(uint32_t iVertex=0; iVertex<outScene->m_numVertexes; ++iVertex)
	{
		// To create vertex deltas, we offset each vertex's y value by
		// a scale of its current y value. We assume here that
		// position is the first three elements of each vertex; it
		// would be safer to extract the appropriate offset from the
		// m_vertexAttributeIndexes array.
		outScene->m_vertexDeltas[outScene->m_numFloatsPerDelta*iVertex+0] = 0;
		outScene->m_vertexDeltas[outScene->m_numFloatsPerDelta*iVertex+1] = 
			(outScene->m_vertexes[outScene->m_numFloatsPerVertex*iVertex+1] / radius) * 0.25f;
		outScene->m_vertexDeltas[outScene->m_numFloatsPerDelta*iVertex+2] = 0;
		
		// For skinning data, we weight each vertex 100% to bone 0.
		outScene->m_matrixIndexesPerVertex[4*iVertex+0] = 0;
		outScene->m_matrixIndexesPerVertex[4*iVertex+1] = -1; // unused
		outScene->m_matrixIndexesPerVertex[4*iVertex+2] = -1; // unused
		outScene->m_matrixIndexesPerVertex[4*iVertex+3] = -1; // unused

		outScene->m_skinningWeightsPerVertex[4*iVertex+0] = 1.0f;
		outScene->m_skinningWeightsPerVertex[4*iVertex+1] = 0.0f; // unused
		outScene->m_skinningWeightsPerVertex[4*iVertex+2] = 0.0f; // unused
		outScene->m_skinningWeightsPerVertex[4*iVertex+3] = 0.0f; // unused
	}

	outScene->m_blendedAttributeIndexes[0] = 0;

	outScene->m_blendedAttributeIds[0] = EDGE_GEOM_ATTRIBUTE_ID_POSITION;

}

static void buildMipmaps(const void *tex, void *result, uint32_t topDim, uint32_t dim)
{
	if (dim <= 1)
		return;

	const uint8_t *in0 = (const uint8_t*)tex;
	const uint8_t *in1 = (const uint8_t*)tex + 4;
	const uint8_t *in2 = (const uint8_t*)tex + 4 * topDim;
	const uint8_t *in3 = (const uint8_t*)tex + 4 * topDim + 4;
	uint8_t *out = (uint8_t *)result;
	for (uint32_t i = 0; i < dim * topDim / 2; i++) 
	{
		for (int k = 0; k < 4; k++)
		{
			*out = (*in0 + *in1 + *in2 + *in3)/4;
			in0++;
			in1++;
			in2++;
			in3++;
			out++;
		}
		in0 += 4;
		in1 += 4;
		in2 += 4;
		in3 += 4;
	}

	buildMipmaps(result, out, topDim, dim >> 1);
}

// Loads a texture and creates mipmaps. 
// Assumes standard texture dimension (defined by the constant "textureDimension")
// totalSize parameter is set to size of texture including mipmaps.
static void* loadTexture(const char* fileName, uint32_t& totalSize)
{
	totalSize = 0;

	// Open file
	FILE* fp = fopen(fileName, "rb");
	if (!fp) {
		printf("cannot open file %s\n", fileName);
		return NULL;
	}

	// Get the file size
	fseek(fp, 0, SEEK_END);
	uint32_t fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	assert(fileSize == textureDimension*textureDimension*4);

	// Allocate memory for texture and mipmaps
	// (double texture buffer size to leave room for mipmaps)
	void* data = memalign(1024*1024, 2*fileSize);  
	if (!data) {
		printf("can't allocate memory\n");
		return NULL;
	}
	totalSize = 2*fileSize;

	// Load texture
	fread(data, 1, fileSize, fp);
	fclose(fp);

	// Create mipmaps for the loaded texture
	buildMipmaps(data, (uint8_t*)data + fileSize, textureDimension, textureDimension);

	return data;
}


void SampleApp::CreateGeomJob(JobGeom256* job, CellGcmContextData *ctx, EdgeGeomSegment *edgeSegment, void *skinningMatrices) const
{
	// Steam descriptor
	job->inputDmaList[kJobGeomDmaOutputStreamDesc].eal  = (uintptr_t)edgeSegment->m_spuOutputStreamDescription;
	job->inputDmaList[kJobGeomDmaOutputStreamDesc].size = edgeSegment->m_spuOutputStreamDescriptionSize;

	// Indexes
	job->inputDmaList[kJobGeomDmaIndexes_0].eal  = (uintptr_t)edgeSegment->m_indexes;
	job->inputDmaList[kJobGeomDmaIndexes_0].size = edgeSegment->m_indexesSizes[0];
	job->inputDmaList[kJobGeomDmaIndexes_1].eal  = job->inputDmaList[kJobGeomDmaIndexes_0].eal + job->inputDmaList[kJobGeomDmaIndexes_0].size;
	job->inputDmaList[kJobGeomDmaIndexes_1].size = edgeSegment->m_indexesSizes[1];
	
	// Skinning Matrices
	job->inputDmaList[kJobGeomDmaSkinMatrices_0].eal  = edgeSegment->m_skinMatricesByteOffsets[0] + (uintptr_t)skinningMatrices;
	job->inputDmaList[kJobGeomDmaSkinMatrices_0].size = edgeSegment->m_skinMatricesSizes[0];
	job->inputDmaList[kJobGeomDmaSkinMatrices_1].eal  = edgeSegment->m_skinMatricesByteOffsets[1] + (uintptr_t)skinningMatrices;
	job->inputDmaList[kJobGeomDmaSkinMatrices_1].size = edgeSegment->m_skinMatricesSizes[1];

	// Skinning Indexes & Weights
	job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_0].eal  = (uintptr_t)edgeSegment->m_skinIndexesAndWeights;
	job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_0].size = edgeSegment->m_skinIndexesAndWeightsSizes[0];
	job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_1].eal  = job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_0].eal + job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_0].size;
	job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_1].size = edgeSegment->m_skinIndexesAndWeightsSizes[1];

	// Vertexes (stream 1)
	job->inputDmaList[kJobGeomDmaVertexes1_0].eal  = (uintptr_t)edgeSegment->m_spuVertexes[0];
	job->inputDmaList[kJobGeomDmaVertexes1_0].size = edgeSegment->m_spuVertexesSizes[0];
	job->inputDmaList[kJobGeomDmaVertexes1_1].eal  = job->inputDmaList[kJobGeomDmaVertexes1_0].eal + job->inputDmaList[kJobGeomDmaVertexes1_0].size;
	job->inputDmaList[kJobGeomDmaVertexes1_1].size = edgeSegment->m_spuVertexesSizes[1];
	job->inputDmaList[kJobGeomDmaVertexes1_2].eal  = job->inputDmaList[kJobGeomDmaVertexes1_1].eal + job->inputDmaList[kJobGeomDmaVertexes1_1].size;
	job->inputDmaList[kJobGeomDmaVertexes1_2].size = edgeSegment->m_spuVertexesSizes[2];

	// Vertexes (stream 2)
	job->inputDmaList[kJobGeomDmaVertexes2_0].eal  = (uintptr_t)edgeSegment->m_spuVertexes[1];
	job->inputDmaList[kJobGeomDmaVertexes2_0].size = edgeSegment->m_spuVertexesSizes[3];
	job->inputDmaList[kJobGeomDmaVertexes2_1].eal  = job->inputDmaList[kJobGeomDmaVertexes2_0].eal + job->inputDmaList[kJobGeomDmaVertexes2_0].size;
	job->inputDmaList[kJobGeomDmaVertexes2_1].size = edgeSegment->m_spuVertexesSizes[4];
	job->inputDmaList[kJobGeomDmaVertexes2_2].eal  = job->inputDmaList[kJobGeomDmaVertexes2_1].eal + job->inputDmaList[kJobGeomDmaVertexes2_1].size;
	job->inputDmaList[kJobGeomDmaVertexes2_2].size = edgeSegment->m_spuVertexesSizes[5];

	// Triangle Culling Data
	job->inputDmaList[kJobGeomDmaViewportInfo].eal  = (uintptr_t)&mViewportInfo;
	job->inputDmaList[kJobGeomDmaViewportInfo].size = sizeof(EdgeGeomViewportInfo);
	job->inputDmaList[kJobGeomDmaLocalToWorldMatrix].eal  = (uintptr_t)&mLocalToWorldMatrix;
	job->inputDmaList[kJobGeomDmaLocalToWorldMatrix].size = sizeof(EdgeGeomLocalToWorldMatrix);
	
	// SpuConfigInfo
	job->inputDmaList[kJobGeomDmaSpuConfigInfo].eal	 = (uintptr_t)edgeSegment->m_spuConfigInfo;
	job->inputDmaList[kJobGeomDmaSpuConfigInfo].size = sizeof(EdgeGeomSpuConfigInfo);
	
	// Fixed Point Attribute Offsets
	job->inputDmaList[kJobGeomDmaFixedPointOffsets1].eal  = (uintptr_t)edgeSegment->m_fixedOffsetPtrs[0];
	job->inputDmaList[kJobGeomDmaFixedPointOffsets1].size = edgeSegment->m_fixedOffsetsSize[0];
	job->inputDmaList[kJobGeomDmaFixedPointOffsets2].eal  =	(uintptr_t)edgeSegment->m_fixedOffsetPtrs[1];
	job->inputDmaList[kJobGeomDmaFixedPointOffsets2].size = edgeSegment->m_fixedOffsetsSize[1];

	// Input Streams Descriptors
	job->inputDmaList[kJobGeomDmaInputStreamDesc1].eal  = (uintptr_t)edgeSegment->m_spuInputStreamDescriptions[0];;
	job->inputDmaList[kJobGeomDmaInputStreamDesc1].size = edgeSegment->m_spuInputStreamDescriptionSizes[0];
	job->inputDmaList[kJobGeomDmaInputStreamDesc2].eal  = (uintptr_t)edgeSegment->m_spuInputStreamDescriptions[1];
	job->inputDmaList[kJobGeomDmaInputStreamDesc2].size = edgeSegment->m_spuInputStreamDescriptionSizes[1];

	// --- User Data ---

	job->userData.eaOutputBufferInfo = (uintptr_t)&mOutputBufferInfo;

	// Command Buffer Hole
	uint32_t holeSize = ((EdgeGeomSpuConfigInfo*)edgeSegment->m_spuConfigInfo)->commandBufferHoleSize << 4;
	if(ctx->current + holeSize/4 + 3 > ctx->end) {
		if((*ctx->callback)(ctx, holeSize/4 + 3) != CELL_OK) {
			return;
		}
	}
	while(((uint32_t)ctx->current & 0xF) != 0) {
		*ctx->current++ = 0;
	}
	uint32_t holeEa = (uint32_t)ctx->current;
	uint32_t holeEnd = holeEa + holeSize;
	uint32_t jumpOffset;
	cellGcmAddressToOffset(ctx->current, &jumpOffset);
	cellGcmSetJumpCommandUnsafeInline(ctx, jumpOffset);
	uint32_t nextJ2S = ((uint32_t)ctx->current + 0x80) & ~0x7F;
	while(nextJ2S < holeEnd) {
		__dcbz( (void*)nextJ2S ); // prefetching here improves this loop's performance significantly
		ctx->current = (uint32_t*)nextJ2S;
		cellGcmAddressToOffset(ctx->current, &jumpOffset);
		cellGcmSetJumpCommandUnsafeInline(ctx, jumpOffset);
		nextJ2S = ((uint32_t)ctx->current + 0x80) & ~0x7F;
	}
	ctx->current = (uint32_t*)holeEnd;
	job->userData.eaCommandBufferHole = (uintptr_t)holeEa;
	
	// Blend shapes / shape count
	if(edgeSegment->m_numBlendShapes > 0) {
		job->userData.eaBlendShapeInfo = (uintptr_t)&shapeInfos[numShapeInfos];
		uint64_t numShapes = 0;
		for(uint32_t i = 0; i < edgeSegment->m_numBlendShapes; ++i) {
			if(!edgeSegment->m_blendShapes[i] || !shapeAlphas[i])
				continue;
			uint64_t shapeSize = (uint64_t)(edgeSegment->m_blendShapeSizes[i]);
			shapeInfos[numShapeInfos].dmaTag = (uintptr_t)edgeSegment->m_blendShapes[i] | (shapeSize << 32);
			shapeInfos[numShapeInfos].alpha = shapeAlphas[i];
			numShapeInfos++;
			numShapes++;
		}
		job->userData.blendShapeInfoCount = numShapes;
	}

	job->header.eaBinary = (uintptr_t)_binary_job_job_geom_spu_bin_start;
	job->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_geom_spu_bin_size);
	job->header.sizeDmaList = kJobGeomInputDmaEnd*8;
	job->header.eaHighInput = 0;
	job->header.useInOutBuffer = 1;
	job->header.sizeInOrInOut = edgeSegment->m_ioBufferSize;
	//job->header.sizeOut = 0;
	job->header.sizeStack = 0;
	job->header.sizeScratch = edgeSegment->m_scratchSize;
	//job->header.eaHighCache = 0;
	job->header.sizeCacheDmaList = 0;

    // enable memory checking in jobs.  SPURS will place a memory check marker in 16 byte buffers added after IO buffer, read only data, and stack, and check the integrity 
    // of the marker at the end of the job.  Enabling this flag will add 3-4% to SPURS overhead of running jobs.  Linking with -mspurs-job instead of -mspurs-job-initialize
    // will cause this flag to be ignored.
    job->header.jobType = CELL_SPURS_JOB_TYPE_MEMORY_CHECK;
}

SampleApp::SampleApp() :
mLastUpdate(FWTime::sZero),
mTurnRate(500.f),
mMoveRate(50.f),
mSkinningMatrices(NULL)
{
	mNear = 0.01f;
	mFar = 1000.f;
}

SampleApp::~SampleApp()
{
}

void SampleApp::initShader()
{
	mCGVertexProgram   = (CGprogram)(void*)&_binary_vpshader_vpo_start;
	mCGFragmentProgram = (CGprogram)(void*)&_binary_fpshader_fpo_start;

	// init
	cellGcmCgInitProgram(mCGVertexProgram);
	cellGcmCgInitProgram(mCGFragmentProgram);

	// allocate video memory for fragment program
	unsigned int ucodeSize;
	void *ucode;
	cellGcmCgGetUCode(mCGFragmentProgram, &ucode, &ucodeSize);

	mFragmentProgramUCode = (void*)cellGcmUtilAllocateLocalMemory(ucodeSize, 1024);
	cellGcmAddressToOffset(mFragmentProgramUCode, &mFragmentProgramOffset);
	memcpy(mFragmentProgramUCode, ucode, ucodeSize); 

	// get and copy 
	cellGcmCgGetUCode(mCGVertexProgram, &ucode, &ucodeSize);
	mVertexProgramUCode = ucode;
}

bool SampleApp::onInit(int argc, char **ppArgv)
{
	FWGCMCamApplication::onInit(argc, ppArgv);	// note: calls sys_spu_initialize() internally

    /* 
     * loading prx for libfs
     */ 
    int ret;
    ret = cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
    if (ret != CELL_OK) {
        printf("cellSysmoduleLoadModule() error 0x%x !\n", ret);
        sys_process_exit(1);
    }

	FWInputDevice	*pPad = FWInput::getDevice(FWInput::DeviceType_Pad, 0);
	if(pPad != NULL) {
		mpTriangle = pPad->bindFilter();
		mpTriangle->setChannel(FWInput::Channel_Button_Triangle);
		mpCircle = pPad->bindFilter();
		mpCircle->setChannel(FWInput::Channel_Button_Circle);
		mpCross = pPad->bindFilter();
		mpCross->setChannel(FWInput::Channel_Button_Cross);
		mpSquare = pPad->bindFilter();
		mpSquare->setChannel(FWInput::Channel_Button_Square);
		mpStart = pPad->bindFilter();
		mpStart->setChannel(FWInput::Channel_Button_Start);
		mpUp = pPad->bindFilter();
		mpUp->setChannel(FWInput::Channel_Button_Up);
		mpDown = pPad->bindFilter();
		mpDown->setChannel(FWInput::Channel_Button_Down);
		mpInputX0 = pPad->bindFilter();
		mpInputX0->setChannel(FWInput::Channel_XAxis_0);
		mpInputX0->setGain(0.5f);
		mpInputX0->setDeadzone(0.08f);
		mpInputY0 = pPad->bindFilter();
		mpInputY0->setChannel(FWInput::Channel_YAxis_0);
		mpInputY0->setGain(-0.5f);
		mpInputY0->setDeadzone(0.08f);
		mpInputX1 = pPad->bindFilter();
		mpInputX1->setChannel(FWInput::Channel_XAxis_1);
		mpInputX1->setGain(0.002f);
		mpInputX1->setDeadzone(0.0008f);
		mpInputY1 = pPad->bindFilter();
		mpInputY1->setChannel(FWInput::Channel_YAxis_1);
		mpInputY1->setGain(0.002f);
		mpInputY1->setDeadzone(0.0008f);
	}
	
	// Get PPU thread priority
	sys_ppu_thread_t my_ppu_thread_id;
	ret = sys_ppu_thread_get_id(&my_ppu_thread_id);
	if (ret != CELL_OK){
		printf("sys_ppu_thread_get_id failed : 0x%x\n", ret);
	}
	int ppu_thr_prio;
	ret = sys_ppu_thread_get_priority(my_ppu_thread_id, &ppu_thr_prio);
	if (ret != CELL_OK){
		printf("sys_ppu_thread_get_priority failed : 0x%x\n", ret);
		return false;
	}

	// Initialize spu_printf handling
	ret = spu_printf_initialize(ppu_thr_prio - 1, NULL);
    if (ret != CELL_OK){
        printf("spu_printf_initialize failed : 0x%x\n", ret);
		return false;
    }

	// Initialize a SPURS attribute structure.  This contains all the data that
	// was originally passed directly to cellSpursInitialize(), plus some additional
	// fields.
	CellSpursAttribute spursAttributes;
	ret = cellSpursAttributeInitialize(&spursAttributes, SPURS_SPU_NUM,
	SPU_THREAD_GROUP_PRIORITY, ppu_thr_prio - 1, false);
	if (ret != CELL_OK){
		printf("cellSpursAttributeInitialize failed : 0x%x\n", ret);
		return false;
	}
	// Enable support for spu_printf in the SPURS attributes.
	ret = cellSpursAttributeEnableSpuPrintfIfAvailable(&spursAttributes);
	if (ret != CELL_OK){
		printf("cellSpursAttributeEnableSpuPrintfItAvailable failed : 0x%x\n", ret);
		return false;
	}
	// Initialize SPURS using our attributes structure.
	ret = cellSpursInitializeWithAttribute(&mSpurs, &spursAttributes);
	if (ret != CELL_OK){
		printf("cellSpursInitializeWithAttribute failed : 0x%x\n", ret);
		return false;
	}

	// Initialize Spurs event flag (used for job chain completion)
	ret = cellSpursEventFlagInitializeIWL(&mSpurs, &mSpursEventFlag,
		CELL_SPURS_EVENT_FLAG_CLEAR_AUTO,
		CELL_SPURS_EVENT_FLAG_SPU2PPU);
	if (ret != CELL_OK){
		printf("cellSpursEventFlagInitializeIWL failed : 0x%x\n", ret);
		return false;
	}
	ret = cellSpursEventFlagAttachLv2EventQueue(&mSpursEventFlag);
	if (ret != CELL_OK){
		printf("cellSpursEventFlagAttachLv2EventQueue failed : 0x%x\n", ret);
		return false;
	}

	// Generate an EdgeGeomScene for the sphere geometry.  This will be the input to the partitioner.
	BuildSphereScene(2.0f, 128, 128, &edgeScene);

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
		normalTangentFormat->m_attributeDefinition[0].m_fixedPointBitDepthInteger[iComponent]    = 2;
		normalTangentFormat->m_attributeDefinition[0].m_fixedPointBitDepthFractional[iComponent] = 6;
	}
	normalTangentFormat->m_attributeDefinition[1].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_TANGENT;
	normalTangentFormat->m_attributeDefinition[1].m_byteOffset = 3;
	normalTangentFormat->m_attributeDefinition[1].m_count = 4;
	normalTangentFormat->m_attributeDefinition[1].m_type = kSpuAttr_UnitVector;

	EdgeGeomSpuVertexFormat *vertexDeltaFormat = (EdgeGeomSpuVertexFormat*)edgeGeomAlloc(sizeof(EdgeGeomSpuVertexFormat));
	vertexDeltaFormat->m_numAttributes = 1;
	vertexDeltaFormat->m_vertexStride = 1;
	vertexDeltaFormat->m_attributeDefinition[0].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_POSITION;
	vertexDeltaFormat->m_attributeDefinition[0].m_byteOffset = 0;
	vertexDeltaFormat->m_attributeDefinition[0].m_count = 3;
	vertexDeltaFormat->m_attributeDefinition[0].m_type = kSpuAttr_FixedPoint;
	// In this case, we happen to know that all the deltas are in the
	// positive/negative Y axis, so our vertex delta format can be
	// exceptionally small.
	vertexDeltaFormat->m_attributeDefinition[0].m_fixedPointBitDepthInteger[0]    = 0;
	vertexDeltaFormat->m_attributeDefinition[0].m_fixedPointBitDepthFractional[0] = 0;
	vertexDeltaFormat->m_attributeDefinition[0].m_fixedPointBitDepthInteger[1]    = 1;
	vertexDeltaFormat->m_attributeDefinition[0].m_fixedPointBitDepthFractional[1] = 7;
	vertexDeltaFormat->m_attributeDefinition[0].m_fixedPointBitDepthInteger[2]    = 0;
	vertexDeltaFormat->m_attributeDefinition[0].m_fixedPointBitDepthFractional[2] = 0;

	EdgeGeomRsxVertexFormat *uvFormat = (EdgeGeomRsxVertexFormat*)edgeGeomAlloc(sizeof(EdgeGeomRsxVertexFormat));
	uvFormat->m_numAttributes = 1;
	uvFormat->m_vertexStride = 4;
	uvFormat->m_attributeDefinition[0].m_attributeId = EDGE_GEOM_ATTRIBUTE_ID_UV0;
	uvFormat->m_attributeDefinition[0].m_byteOffset = 0;
	uvFormat->m_attributeDefinition[0].m_count = 2;
	uvFormat->m_attributeDefinition[0].m_type = kRsxAttr_F16;

	memset(&edgeFormat, 0, sizeof(EdgeGeomSegmentFormat));
	edgeFormat.m_spuInputVertexFormats[0] = edgeGeomGetSpuVertexFormat(EDGE_GEOM_SPU_VERTEX_FORMAT_F32c3); // F32 position
	edgeFormat.m_spuInputVertexFormats[1] = normalTangentFormat; // normals and tangents in the secondary stream
	edgeFormat.m_spuInputVertexDeltaFormat = vertexDeltaFormat; 
	edgeFormat.m_spuOutputVertexFormat = edgeGeomGetRsxVertexFormat(EDGE_GEOM_RSX_VERTEX_FORMAT_F32c3_X11Y11Z10N_X11Y11Z10N);
	edgeFormat.m_rsxOnlyVertexFormat = uvFormat; // UVs sent directly to the RSX
	edgeFormat.m_skinType =kSkinSingleBoneNoScaling;
	edgeFormat.m_indexesType = kIndexesCompressedTriangleListCCW;
	edgeFormat.m_skinMatrixFormat = kMatrix3x4RowMajor;

	printf("EdgeGeom: partitioning %d vertices and %d triangles...", edgeScene.m_numVertexes,
		edgeScene.m_numTriangles);
	edgeGeomPartitionSceneIntoSegments(edgeScene, edgeFormat, &edgeSegments, &numEdgeSegments);
	printf("into %d segments\n", numEdgeSegments);

	// Edge output buffers
	// mapped memory regions must be a multiple of 1 MB in size, and aligned to 1 MB
	mSharedOutputBuffer = (uint8_t*)memalign(1024*1024, EDGE_SHARED_OUTPUT_BUFFER_SIZE);
	mRingOutputBuffer = (uint8_t*)memalign(1024*1024, SPURS_SPU_NUM * EDGE_RING_OUTPUT_BUFFER_SIZE);
	uint32_t sharedBufferMapSize = (EDGE_SHARED_OUTPUT_BUFFER_SIZE + 0xFFFFF) & ~0xFFFFF;
	uint32_t ringBufferMapSize = (SPURS_SPU_NUM * EDGE_RING_OUTPUT_BUFFER_SIZE + 0xFFFFF) & ~0xFFFFF;
	uint32_t commandBufferMapSize = (COMMAND_BUFFER_SIZE + 0xFFFFF) & ~0xFFFFF;
	uint32_t dummy;
    if (mSharedOutputBuffer && sharedBufferMapSize) {
	    cellGcmMapMainMemory(mSharedOutputBuffer, sharedBufferMapSize, &dummy);
    }
    if (mRingOutputBuffer && ringBufferMapSize) {
	    cellGcmMapMainMemory(mRingOutputBuffer, ringBufferMapSize, &dummy);
    }
	
	// Command buffer
	mCommandBuffer = memalign(1024*1024, COMMAND_BUFFER_SIZE);
	cellGcmMapMainMemory(mCommandBuffer, commandBufferMapSize, &mCommandBufferOffset);

	// Textures
	uint32_t colorTexSize = 0;
	uint32_t normalTexSize = 0;
	// loadTexture allocates enough space after the texture for mipmaps and creates the mipmaps
	// The second parameter is set to the total texture size including mipmaps
	mTextures[0] = loadTexture(SYS_APP_HOME"/assets/logo-color.bin", colorTexSize); 
	mTextures[1] = loadTexture(SYS_APP_HOME"/assets/logo-normal.bin", normalTexSize);
	cellGcmMapMainMemory(mTextures[0], colorTexSize, &dummy);
	cellGcmMapMainMemory(mTextures[1], normalTexSize, &dummy);

	// populate skinning matrices (which are transposed, 3x4)
	for(uint32_t i = 0; i < kNumBones; ++i) {
		Matrix4 m( Matrix4::identity() );
		m = transpose(m);
		memcpy(&s_skinningMatrices[i*12], &m, 48);
	}

	initShader();

	// Copy the Edge segment's RSX-only buffers into local memory, so that the RSX can see them.
	// This is something of a hack, as ideally you'd want to load this data directly into VRAM,
	// but it suffices for the purposes of this demo.
	uint8_t *geomBuffer = (uint8_t*)cellGcmUtilAllocateLocalMemory(1024*1024, 16);
	uint8_t *nextSegmentGeom = geomBuffer;
	for(uint32_t iSegment = 0; iSegment < numEdgeSegments; ++iSegment)
	{
		// Skip this segment if it doesn't have a RSX-only stream
		if (edgeSegments[iSegment].m_rsxOnlyVertexesSize == 0)
			continue;
		memcpy(nextSegmentGeom, edgeSegments[iSegment].m_rsxOnlyVertexes,
			edgeSegments[iSegment].m_rsxOnlyVertexesSize);
		// Update the segment's rsxOnlyVertexes pointer to reference the VRAM copy of the data.
		// NOTE: This leaks a pointer to the original rsxOnlyVertexes in main memory, but due
		// to the implementation of this sample it would be impossible to free it.
		edgeSegments[iSegment].m_rsxOnlyVertexes = nextSegmentGeom;
		nextSegmentGeom += edgeSegments[iSegment].m_rsxOnlyVertexesSize;
	}

	// set default camera position
	//mCamera.setPosition(Point3(-3.5f, 1.94f, 13.08f));

	// Initialize output buffer info.
	// Basic rules:
	//  - If the ring buffer size (end-start) is zero, use the shared buffer exlusively.
	//  - If the shared buffer size (end-start) is zero, use the ring buffer exclusively.
	//  - If both buffer sizes are nonzero, then use the ring buffer primarily,
	//    falling back on the shared buffer for overflow if the ring buffer is
	//    temporarily full.

	// shared buffer info
	mOutputBufferInfo.sharedInfo.startEa = (uint32_t)mSharedOutputBuffer;
	mOutputBufferInfo.sharedInfo.endEa = mOutputBufferInfo.sharedInfo.startEa
		+ EDGE_SHARED_OUTPUT_BUFFER_SIZE;
	mOutputBufferInfo.sharedInfo.currentEa = mOutputBufferInfo.sharedInfo.startEa;
	mOutputBufferInfo.sharedInfo.locationId = CELL_GCM_LOCATION_MAIN;
	mOutputBufferInfo.sharedInfo.failedAllocSize = 0;
	cellGcmAddressToOffset((void*)mOutputBufferInfo.sharedInfo.startEa,
		&mOutputBufferInfo.sharedInfo.startOffset);
	// ring buffer info
	for(uint32_t iSpu=0; iSpu<SPURS_SPU_NUM; ++iSpu) {
		// Each SPU must have its own separate ring buffer, with no overlap.
		mOutputBufferInfo.ringInfo[iSpu].startEa = (uint32_t)mRingOutputBuffer
			+ iSpu * EDGE_RING_OUTPUT_BUFFER_SIZE;
		mOutputBufferInfo.ringInfo[iSpu].endEa = mOutputBufferInfo.ringInfo[iSpu].startEa
			+ EDGE_RING_OUTPUT_BUFFER_SIZE;
		mOutputBufferInfo.ringInfo[iSpu].currentEa = mOutputBufferInfo.ringInfo[iSpu].startEa;
		mOutputBufferInfo.ringInfo[iSpu].locationId = CELL_GCM_LOCATION_MAIN;
		mOutputBufferInfo.ringInfo[iSpu].rsxLabelEa = (uint32_t)cellGcmGetLabelAddress(kFirstRsxLabelIndex + iSpu);
        mOutputBufferInfo.ringInfo[iSpu].cachedFree = 0;
		cellGcmAddressToOffset((void*)mOutputBufferInfo.ringInfo[iSpu].startEa,
			&mOutputBufferInfo.ringInfo[iSpu].startOffset);
		// RSX label value must be initialized to the end of the buffer
		*(uint32_t *)mOutputBufferInfo.ringInfo[iSpu].rsxLabelEa = mOutputBufferInfo.ringInfo[iSpu].endEa;
	}

	// init profile bars
	ProfileBars::Init(&mSpurs, SPURS_SPU_NUM);

	return true;
}

bool SampleApp::onUpdate()
{
	FWTimeVal time = FWTime::getCurrentTime();

	float deltaTime;
	if(mLastUpdate == FWTime::sZero) {
		deltaTime = 0.f;
		mLastUpdate = time;
	}
	else {
		deltaTime = (float)(time - mLastUpdate);
		mLastUpdate = time;
	}

	mCamera.setPan(mCamera.getPan() + (mpInputX1 ? mpInputX1->getFloatValue() : 0.f) * deltaTime * mTurnRate);
	mCamera.setTilt(mCamera.getTilt() + (mpInputY1 ? mpInputY1->getFloatValue() : 0.f) * deltaTime * mTurnRate);

	Matrix3 xRot = Matrix3::rotationX(-mCamera.getTilt());
	Matrix3 yRot = Matrix3::rotationY(-mCamera.getPan());
	Vector3 xVec = (yRot * xRot) * Vector3::xAxis();
	Vector3 yVec = (yRot * xRot) * Vector3::yAxis();
	Vector3 zVec = (yRot * xRot) * -Vector3::zAxis();

	mCamera.setPosition(mCamera.getPosition() + (
		  xVec * (mpInputX0 ? mpInputX0->getFloatValue() : 0.f) 
		+ yVec * ((mpUp && mpUp->getBoolValue()) ? 1.0f : 0.f)
		+ yVec * ((mpDown && mpDown->getBoolValue()) ? -1.0f : 0.f)
		+ zVec * (mpInputY0 ? mpInputY0->getFloatValue() : 0.f)
		) * deltaTime * mMoveRate);

	// noddy animation
	{
		Matrix4 m( Quat::rotation( sinf( time ), Vector3(0,0,1) ), Vector3(0,0,0) );
		m = transpose(m);
		memcpy(&s_skinningMatrices[0*12], &m, 48);
	}

	// periodic blend shape alphas
	static int frame = 0;
	frame++;
	shapeAlphas[0] = sinf(frame*0.05f)*sinf(frame*0.05f);

	return FWGCMCamApplication::onUpdate();
}

void SampleApp::onRender()
{
	// time stamp 0 is the base rsx time used by the profile bars for this frame
	// wait for vsync and write the timestamp
	ProfileBars::StartFrame();
	cellGcmSetTimeStamp(0);
	cellGcmFlush();

	// base implementation clears screen and sets up camera
	FWGCMCamApplication::onRender();

	cellGcmSetInvalidateVertexCache();
	
	CellGcmTexture tex;
	uint32_t format, remap;
	cellGcmUtilGetTextureAttribute(CELL_GCM_UTIL_RGBA8, &format, &remap, 0, 1);

	tex.format = format;
	tex.mipmap = 10;
	tex.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
	tex.cubemap = CELL_GCM_FALSE;
	tex.remap = remap;
	tex.width = textureDimension;
	tex.height = textureDimension;
	tex.depth = 1;
	tex.pitch = textureDimension*4;
	tex.location = CELL_GCM_LOCATION_MAIN;
	tex.offset = 0;

	cellGcmSetTextureControl(0, CELL_GCM_TRUE, 0<<8, 12<<8, CELL_GCM_TEXTURE_MAX_ANISO_1); // MIN:0,MAX:12
	cellGcmSetTextureAddress(0,	CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	cellGcmSetTextureFilter(0, 0, CELL_GCM_TEXTURE_NEAREST_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	cellGcmSetTextureControl(1, CELL_GCM_TRUE, 0<<8, 12<<8, CELL_GCM_TEXTURE_MAX_ANISO_1); // MIN:0,MAX:12
	cellGcmSetTextureAddress(1,	CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	cellGcmSetTextureFilter(1, 0, CELL_GCM_TEXTURE_NEAREST_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	cellGcmSetVertexProgram(mCGVertexProgram, mVertexProgramUCode);
	cellGcmSetFragmentProgram(mCGFragmentProgram, mFragmentProgramOffset);
	
	cellGcmSetBlendEnable(CELL_GCM_FALSE);
	cellGcmSetDepthTestEnable(CELL_GCM_TRUE);
	cellGcmSetDepthFunc(CELL_GCM_LESS);
	cellGcmSetShadeMode(CELL_GCM_SMOOTH);

	Matrix4 mat = Matrix4::rotationZYX(Vector3(0, 0, 0));
	mat.setTranslation(Vector3(0.0f, 0.0f, -5.0f));
	Matrix4 mvp = transpose(getProjectionMatrix() * getViewMatrix() * mat);
	float lp[4] = {0,25,100,1};
	Vector4 objSpaceEyePos = inverse(mat) * mCamera.getPosition();
	float ep[4] = {objSpaceEyePos[0], objSpaceEyePos[1], objSpaceEyePos[2], 1};
	cellGcmSetVertexProgramConstants(0, 16, (float*)&mvp);
	cellGcmSetVertexProgramConstants(4, 4, lp);
	cellGcmSetVertexProgramConstants(5, 4, ep);

    mViewportInfo.scissorArea[0] = 0;
    mViewportInfo.scissorArea[1] = 0;
    mViewportInfo.scissorArea[2] = 1280;
    mViewportInfo.scissorArea[3] = 720;

    mViewportInfo.depthRange[0] = 0.0f;
    mViewportInfo.depthRange[1] = 1.0f;

    mViewportInfo.viewportScales[0] = mViewportInfo.scissorArea[2] * .5f;
    mViewportInfo.viewportScales[1] = mViewportInfo.scissorArea[3] * -.5f;
    mViewportInfo.viewportScales[2] = (mViewportInfo.depthRange[1] - mViewportInfo.depthRange[0]) * .5f;
    mViewportInfo.viewportScales[3] = 0.0f;

    mViewportInfo.viewportOffsets[0] = mViewportInfo.scissorArea[0] + mViewportInfo.viewportScales[0];
    mViewportInfo.viewportOffsets[1] = mViewportInfo.scissorArea[1] + mViewportInfo.scissorArea[3] * .5f;
    mViewportInfo.viewportOffsets[2] = (mViewportInfo.depthRange[1] + mViewportInfo.depthRange[0]) * .5f;
    mViewportInfo.viewportOffsets[3] = 0.0f;

    mViewportInfo.sampleFlavor = CELL_GCM_SURFACE_CENTER_1;

	Matrix4 vp = transpose(getProjectionMatrix() * getViewMatrix());
	memcpy(mViewportInfo.viewProjectionMatrix, &vp, 64);
	Matrix4 m = transpose(mat);
	memcpy(&mLocalToWorldMatrix.matrixData, &m, 48); 

	uint32_t texOff[2];
	cellGcmAddressToOffset(mTextures[0], &texOff[0]);
	cellGcmAddressToOffset(mTextures[1], &texOff[1]);
	
	tex.offset = texOff[0];
	cellGcmSetTexture(0, &tex);
	tex.offset = texOff[1];
	cellGcmSetTexture(1, &tex);

	// Reset Parameters
	int numSegments = 0;
	numShapeInfos = 0;
	mOutputBufferInfo.sharedInfo.currentEa = mOutputBufferInfo.sharedInfo.startEa;
	if(mOutputBufferInfo.sharedInfo.failedAllocSize != 0) {
		// When using hybrid buffers, the allocations that "failed" did not actually
		// fail fatally.  What it means is that the shared buffer was full, so the SPUs
		// had to wait for the RSX to consume more data before the allocation could complete.
		//
		// This is an important warning (it means your output buffers should probably be a
		// little larger), but certainly not worth spewing output every frame about failed allocations.
		//
        //printf("Edge: %d bytes of allocations could not be fulfilled\n",
        //    mOutputBufferInfo.sharedInfo.failedAllocSize);
	}
	mOutputBufferInfo.sharedInfo.failedAllocSize = 0;

	CellGcmContextData pbContext;
	pbContext.current = (uint32_t*)mCommandBuffer;
	pbContext.end = (uint32_t*)((uint32_t)mCommandBuffer + COMMAND_BUFFER_SIZE - 4);
	pbContext.begin = (uint32_t*)mCommandBuffer;
	pbContext.callback = 0;

	static CellSpursJob256 jobEnd;
	__builtin_memset(&jobEnd,0,sizeof(CellSpursJob256));
	jobEnd.header.eaBinary     = (uintptr_t)_binary_job_job_send_event_spu_bin_start;
	jobEnd.header.sizeBinary   = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_send_event_spu_bin_size);
	jobEnd.workArea.userData[0] = (uintptr_t)&mSpursEventFlag;
	jobEnd.workArea.userData[1] = (uint64_t)EVENT_FLAG_BITS;

	static int frame = 0;
	++frame;
	for(uint32_t iSegment = 0; iSegment < numEdgeSegments; ++iSegment)
	{
		// Bind the RSX-only vertex attributes for this job before it's created
		if (edgeSegments[iSegment].m_rsxOnlyVertexesSize > 0)
		{
			EdgeGeomVertexStreamDescription *rsxOnlyStreamDesc = (EdgeGeomVertexStreamDescription*)(edgeSegments[iSegment].m_rsxOnlyStreamDescription);
			uint32_t rsxVertexesOffset = 0;
			cellGcmAddressToOffset(edgeSegments[iSegment].m_rsxOnlyVertexes, &rsxVertexesOffset);
			for(uint32_t iRsxAttr=0; iRsxAttr < rsxOnlyStreamDesc->numAttributes; ++iRsxAttr)
			{
				EdgeGeomAttributeBlock &attr = rsxOnlyStreamDesc->blocks[iRsxAttr].attributeBlock;
				cellGcmSetVertexDataArray(&pbContext, attr.vertexProgramSlotIndex, 0, rsxOnlyStreamDesc->stride,
					attr.componentCount, attr.format, CELL_GCM_LOCATION_LOCAL,
					rsxVertexesOffset + attr.offset);
			}
		}

		JobGeom256 *job = (JobGeom256*) &jobs[numSegments++];
		CreateGeomJob(job, &pbContext, &edgeSegments[iSegment], &s_skinningMatrices[0]);

		int ret = cellSpursCheckJob((CellSpursJob256*)job, sizeof(JobGeom256),
			sizeof(CellSpursJob256));
		if ( ret != CELL_OK){
			printf("cellSpursCheckJob failed : 0x%x\n", ret);
		}
	}

	static CellSpursJobList jobList;
	jobList.eaJobList = (uint64_t)jobs;
	jobList.numJobs = numSegments;
	assert(sizeof(JobGeom256)==sizeof(jobs[0]));
	jobList.sizeOfJob = sizeof(JobGeom256);

	static uint64_t command_list[5];
	command_list[0] = CELL_SPURS_JOB_COMMAND_JOBLIST(&jobList);
	command_list[1] = CELL_SPURS_JOB_COMMAND_SYNC;
	command_list[2] = CELL_SPURS_JOB_COMMAND_FLUSH;
	command_list[3] = CELL_SPURS_JOB_COMMAND_JOB(&jobEnd);
	command_list[4] = CELL_SPURS_JOB_COMMAND_END;

	static CellSpursJobChain jobChain;
    static CellSpursJobChainAttribute      jobChainAttributes;
	static uint8_t prios[8] = {1, 1, 1, 1, 1, 1, 1, 1};

	int ret = cellSpursJobChainAttributeInitialize(
		&jobChainAttributes, command_list, sizeof(CellSpursJob256),
		(SPURS_SPU_NUM == 1) ? 1 : 16, prios, SPURS_SPU_NUM,
		true, 0, 1, false, sizeof(CellSpursJob256), 6);
	if (ret != CELL_OK){
		printf("cellSpursJobChainAttributeInitialize failed : 0x%x\n", ret);
	}
	ret = cellSpursJobChainAttributeSetName(&jobChainAttributes, "geom-jobchain");
	if (ret != CELL_OK){
		printf("cellSpursJobChainAttributeSetName failed : 0x%x\n", ret);
	}
	//When the job manager detects an error, halt on SPU to help with debugging
	ret = cellSpursJobChainAttributeSetHaltOnError(&jobChainAttributes);
	if (ret != CELL_OK){
		printf("cellSpursJobChainAttributeSetHaltOnError failed : 0x%x\n", ret);
	}
	ret = cellSpursCreateJobChainWithAttribute(&mSpurs, &jobChain, &jobChainAttributes);
	if (ret != CELL_OK){
		printf("cellSpursCreateJobChainWithAttribute failed : 0x%x\n", ret);
	}
	ret = cellSpursRunJobChain(&jobChain);
	if (ret != CELL_OK){
		printf("cellSpursRunJobChain failed : 0x%x\n", ret);
	}

	cellGcmSetReturnCommand(&pbContext);

	ProfileBars::AddRsxMarker(0x20202000);		// grey rsx bar until that point (clear etc)
	cellGcmSetCallCommand(mCommandBufferOffset);
	ProfileBars::AddRsxMarker(0xff000000);		// red rsx bar indicates draw time	

	// kick rsx
    cellGcmFlush();

    uint16_t ev_mask = EVENT_FLAG_BITS;
	ret = cellSpursEventFlagWait(&mSpursEventFlag,&ev_mask,CELL_SPURS_EVENT_FLAG_AND);
    if (ret != CELL_OK){
        printf("cellSpursEventFlagWait failed : 0x%x\n", ret);
    } 
	ret = cellSpursShutdownJobChain(&jobChain);
    if (ret != CELL_OK){
        printf("cellSpursShutdownJobChain failed : 0x%x\n", ret);
    } 
	ret = cellSpursJoinJobChain(&jobChain);
    if (ret != CELL_OK){
        printf("cellSpursJoinJobChain failed : 0x%x\n", ret);
    } 

	// cellGcmFinish is only there because of profile bars displayed for the current frame.
	ProfileBars::AddPpuMarker(0x0000ff00);
	static uint32_t finishReference = 0;
	cellGcmFinish(finishReference++);		
	ProfileBars::Render();

	{
		// calc fps
		FWTimeVal	time = FWTime::getCurrentTime();
		float fFPS = 1.f / (float)(time - mLastTime);
		mLastTime = time;

		// print some messages
		FWDebugFont::setPosition(0, 0);
		FWDebugFont::setColor(1.f, 1.f, 1.f, 1.0f);

		FWDebugFont::print("Edge PPU Tools Sample Application\n\n");
		FWDebugFont::printf("FPS: %.2f\n", fFPS);
	}
}

void SampleApp::onSize(const FWDisplayInfo& rDispInfo)
{
	FWGCMCamApplication::onSize(rDispInfo);
}

void SampleApp::onShutdown()
{
	int ret;
	ret = cellSpursEventFlagDetachLv2EventQueue(&mSpursEventFlag);
	if (ret != CELL_OK) {
		printf("cellSpursEventFlagDetachLv2EventQueue failed : 0x%x\n", ret);
	}
	ret = cellSpursFinalize(&mSpurs);
	if (ret != CELL_OK){
		printf("cellSpursFinalize failed : 0x%x\n", ret);
	}
	ret = spu_printf_finalize();
	if (ret != CELL_OK){
		printf("spu_printf_finalize failed : 0x%x\n", ret);
	}

	free( mSharedOutputBuffer );
	free( mRingOutputBuffer );
	free( mCommandBuffer );
	free( mSkinningMatrices );
	free( mTextures[0] );
	free( mTextures[1] );

	FWGCMCamApplication::onShutdown();

	// unbind input filters
	FWInputDevice	*pPad = FWInput::getDevice(FWInput::DeviceType_Pad, 0);

	if(pPad != NULL)  {
		pPad->unbindFilter(mpTriangle);
		pPad->unbindFilter(mpCircle);
		pPad->unbindFilter(mpCross);
		pPad->unbindFilter(mpSquare);
		pPad->unbindFilter(mpUp);
		pPad->unbindFilter(mpDown);
		pPad->unbindFilter(mpStart);
		pPad->unbindFilter(mpInputX0);
		pPad->unbindFilter(mpInputY0);
		pPad->unbindFilter(mpInputX1);
		pPad->unbindFilter(mpInputY1);
	}
}
