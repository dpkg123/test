/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdint.h>
#include <stdbool.h>
#include <spu_intrinsics.h>
#include <cell/spurs/common.h>
#include <cell/spurs/job_chain.h>
#include <cell/spurs/job_context.h>
#include <cell/atomic.h>
#include <cell/dma.h>

#include "edge/edge_assert.h"
#include "edge/edge_printf.h"
#include "edge/geom/edgegeom.h"
#include "job_intersect_interface.h"
#include "intersect.h"

static uint8_t atomicArea[0x80] __attribute__((__aligned__(128)));
static float ray[8] __attribute__((__aligned__(128)));
static float transform[16] __attribute__((__aligned__(16)));

void computeTransform()
{
    float *rayPosition = (float *)ray;
    float *rayDirection = ((float *)ray) + 4;
    //  from RealTimeRendering
    const float zAxis[4] = {0.f, 0.f, -1.f, 0.f}; //T
    float e = rayDirection[0] * zAxis[0] + rayDirection[1] * zAxis[1] + rayDirection[2] * zAxis[2];
    float cross[4] = {  rayDirection[1] * zAxis[2] - rayDirection[2] * zAxis[1],
                        rayDirection[2] * zAxis[0] - rayDirection[0] * zAxis[2],
                        rayDirection[0] * zAxis[1] - rayDirection[1] * zAxis[0],
                        0.f};

    float v[4] = {cross[0], cross[1], cross[2], 0.f};
    float h = (1.f - e) / (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    float rotation[16];
    rotation[0]    = e + h * v[0] * v[0];   
    rotation[1]    = h * v[0] * v[1] - v[2]; 
    rotation[2]    = h * v[0] * v[2] + v[1]; 
    rotation[3]    = 0.f;

    rotation[4]    = h * v[0] * v[1] + v[2];              
    rotation[5]    = e + h * v[1] * v[1]; 
    rotation[6]    = h * v[1] * v[2] - v[0]; 
    rotation[7]    = 0.f;

    rotation[8]    = h * v[0] * v[2] - v[1];   
    rotation[9]    = h * v[1] * v[2] + v[0]; 
    rotation[10]   = e + h * v[2] * v[2]; 
    rotation[11]   = 0.f;

    rotation[12]   = 0.f;   
    rotation[13]   = 0.f; 
    rotation[14]   = 0.f; 
    rotation[15]   = 1.f;

    float translation[16];
    translation[0] = 1.f;
    translation[1] = 0.f;
    translation[2] = 0.f;
    translation[3] = -rayPosition[0];

    translation[4] = 0.f;
    translation[5] = 1.f;
    translation[6] = 0.f;
    translation[7] = -rayPosition[1];

    translation[8] = 0.f;
    translation[9] = 0.f;
    translation[10] = 1.f;
    translation[11] = -rayPosition[2];

    translation[12] = 0.f;
    translation[13] = 0.f;
    translation[14] = 0.f;
    translation[15] = 1.f;

    transform[0]    = rotation[0]   * translation[0] + rotation[1]  * translation[4] + rotation[2]  * translation[8]    + rotation[3]  * translation[12];
    transform[1]    = rotation[0]   * translation[1] + rotation[1]  * translation[5] + rotation[2]  * translation[9]    + rotation[3]  * translation[13];
    transform[2]    = rotation[0]   * translation[2] + rotation[1]  * translation[6] + rotation[2]  * translation[10]   + rotation[3]  * translation[14];
    transform[3]    = rotation[0]   * translation[3] + rotation[1]  * translation[7] + rotation[2]  * translation[11]   + rotation[3]  * translation[15];

    transform[4]    = rotation[4]   * translation[0] + rotation[5]  * translation[4] + rotation[6]  * translation[8]    + rotation[7]  * translation[12];
    transform[5]    = rotation[4]   * translation[1] + rotation[5]  * translation[5] + rotation[6]  * translation[9]    + rotation[7]  * translation[13];
    transform[6]    = rotation[4]   * translation[2] + rotation[5]  * translation[6] + rotation[6]  * translation[10]   + rotation[7]  * translation[14];
    transform[7]    = rotation[4]   * translation[3] + rotation[5]  * translation[7] + rotation[6]  * translation[11]   + rotation[7]  * translation[15];

    transform[8]    = rotation[8]   * translation[0] + rotation[9]  * translation[4] + rotation[10] * translation[8]    + rotation[11] * translation[12];
    transform[9]    = rotation[8]   * translation[1] + rotation[9]  * translation[5] + rotation[10] * translation[9]    + rotation[11] * translation[13];
    transform[10]   = rotation[8]   * translation[2] + rotation[9]  * translation[6] + rotation[10] * translation[10]   + rotation[11] * translation[14];
    transform[11]   = rotation[8]   * translation[3] + rotation[9]  * translation[7] + rotation[10] * translation[11]   + rotation[11] * translation[15];

    transform[12]   = rotation[12]  * translation[0] + rotation[13] * translation[4] + rotation[14] * translation[8]    + rotation[15] * translation[12];
    transform[13]   = rotation[12]  * translation[1] + rotation[13] * translation[5] + rotation[14] * translation[9]    + rotation[15] * translation[13];
    transform[14]   = rotation[12]  * translation[2] + rotation[13] * translation[6] + rotation[14] * translation[10]   + rotation[15] * translation[14];
    transform[15]   = rotation[12]  * translation[3] + rotation[13] * translation[7] + rotation[14] * translation[11]   + rotation[15] * translation[15];
}


extern "C" 
{
	void cellSpursJobMain2(CellSpursJobContext2* stInfo, CellSpursJob256 *job256)
	{
		const JobIntersect256 *job = (const JobIntersect256 *) job256;

		// Input buffers
		// Note that these are pointers into the input buffer, and
		// their lifetime is limited by the current stage of
		// processing.  For example, you can not assume that the
		// vertexesA pointer will actually point to input vertex data
		// after calling DecompressVertexes()!
		void* inputsPointers[kJobIntersectInputDmaEnd] __attribute__((aligned(16)));
		cellSpursJobGetPointerList(inputsPointers, &job->header, stInfo);

        ray[0] = ((float *)inputsPointers[kJobIntersectDmaRay])[0];
        ray[1] = ((float *)inputsPointers[kJobIntersectDmaRay])[1];
        ray[2] = ((float *)inputsPointers[kJobIntersectDmaRay])[2];
        ray[3] = 1.f;

        ray[4] = ((float *)inputsPointers[kJobIntersectDmaRay])[4];
        ray[5] = ((float *)inputsPointers[kJobIntersectDmaRay])[5];
        ray[6] = ((float *)inputsPointers[kJobIntersectDmaRay])[6];
        ray[7] = 0.f;

		// Extract matrix count from dma size
		uint32_t totalMatrixCount  = (job->inputDmaList[kJobIntersectDmaSkinMatrices_0].size
			+ job->inputDmaList[kJobIntersectDmaSkinMatrices_1].size) / 48;

		// Extract additional values from the userData area.
		uint32_t numBlendShapes    	= job->userData.blendShapeInfoCount;
		uint32_t blendShapeInfosEa 	= job->userData.eaBlendShapeInfo;

        uint32_t rayResults                 = job->userData.eaRayResults;
        uint32_t segmentId                  = job->userData.segmentIndex;

		// If only one stream was passed (i.e. the second stream size is 0), we need to explicitly zero
		// out the secondary vertex stream pointers.
		bool hasSecondStream = (job->inputDmaList[kJobIntersectDmaVertexes2_0].size
			+ job->inputDmaList[kJobIntersectDmaVertexes2_1].size
			+ job->inputDmaList[kJobIntersectDmaVertexes2_2].size) > 0;
		if (!hasSecondStream)
		{
			inputsPointers[kJobIntersectDmaVertexes2] = 0;
			inputsPointers[kJobIntersectDmaInputStreamDesc2] = 0;
			inputsPointers[kJobIntersectDmaFixedPointOffsets2] = 0;
		}
		
		// Validate buffer order
		uint32_t errCode = edgeGeomValidateBufferOrder(
			inputsPointers[kJobIntersectDmaOutputStreamDesc], 
			inputsPointers[kJobIntersectDmaIndexes], 
			inputsPointers[kJobIntersectDmaSkinMatrices], 
			inputsPointers[kJobIntersectDmaSkinIndexesAndWeights], 
			inputsPointers[kJobIntersectDmaVertexes1],
			inputsPointers[kJobIntersectDmaVertexes2], 
			inputsPointers[kJobIntersectDmaViewportInfo], 
			inputsPointers[kJobIntersectDmaLocalToWorldMatrix], 
			inputsPointers[kJobIntersectDmaSpuConfigInfo],
			inputsPointers[kJobIntersectDmaFixedPointOffsets1], 
			inputsPointers[kJobIntersectDmaFixedPointOffsets2], 
			inputsPointers[kJobIntersectDmaInputStreamDesc1],
			inputsPointers[kJobIntersectDmaInputStreamDesc2]);
		EDGE_ASSERT_MSG( errCode == 0, ("Buffer order incorrect; errCode = 0x%08X\n", errCode) );
		(void)errCode;

		EdgeGeomCustomVertexFormatInfo customFormatInfo = 
		{
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobIntersectDmaInputStreamDesc1],
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobIntersectDmaInputStreamDesc2],
			(EdgeGeomVertexStreamDescription*) inputsPointers[kJobIntersectDmaOutputStreamDesc],
			0,
			0,0,0,0,0,0
		};

		EdgeGeomSpuContext ctx;

		// Edge processing begins here
		edgeGeomInitialize(&ctx, 
			(const EdgeGeomSpuConfigInfo*) inputsPointers[kJobIntersectDmaSpuConfigInfo], 
			stInfo->sBuffer, job->header.sizeScratch << 4, 
			stInfo->ioBuffer, job->header.sizeInOrInOut,            
            stInfo->dmaTag, 
			(const EdgeGeomViewportInfo*) inputsPointers[kJobIntersectDmaViewportInfo], 
			(const EdgeGeomLocalToWorldMatrix*) inputsPointers[kJobIntersectDmaLocalToWorldMatrix], 
            &customFormatInfo);

		// Initialize Edge's free pointer to the end of the input
		// data. Not strictly necessary, but it can make certain
		// problems easier to debug.
		uint32_t lastInputListIndex = (job256->header.sizeDmaList / sizeof(uint64_t)) - 1;
		while(inputsPointers[lastInputListIndex] == 0)
			--lastInputListIndex;
		uint32_t inputDataEnd = (intptr_t)inputsPointers[lastInputListIndex]
			+ (job256->workArea.dmaList[lastInputListIndex]>>32); // LS start + size
		edgeGeomSetFreePtr(&ctx, (uint8_t*)inputDataEnd);

		edgeGeomDecompressVertexes(&ctx, inputsPointers[kJobIntersectDmaVertexes1], 
			inputsPointers[kJobIntersectDmaFixedPointOffsets1],
			inputsPointers[kJobIntersectDmaVertexes2], 
			inputsPointers[kJobIntersectDmaFixedPointOffsets2]);
		edgeGeomProcessBlendShapes(&ctx, numBlendShapes, blendShapeInfosEa);
		edgeGeomSkinVertexes(&ctx, inputsPointers[kJobIntersectDmaSkinMatrices], totalMatrixCount, inputsPointers[kJobIntersectDmaSkinIndexesAndWeights]);
		edgeGeomDecompressIndexes(&ctx, inputsPointers[kJobIntersectDmaIndexes]);

		// Cast the ray into the skinned geometry
		const EdgeGeomSpuConfigInfo *spuConfigInfo = edgeGeomGetSpuConfigInfo(&ctx);
        computeTransform();
        edgeGeomTransformVertexes(spuConfigInfo->numVertexes, edgeGeomGetPositionUniformTable(&ctx), edgeGeomGetPositionUniformTable(&ctx), transform);
        RayResult rayResult0;
        intersectRayTriangles(spuConfigInfo->numIndexes/3, edgeGeomGetPositionUniformTable(&ctx), inputsPointers[kJobIntersectDmaIndexes], ray, &rayResult0, transform);

		// Test the closest hit against the global results.
		// If our intersection point is closer, update the copy in main memory.
        bool isEnabled = spu_readch(SPU_RdMachStat) & 1;
        uint32_t result;
        do 
        {
            spu_idisable();

            // Get the mutex into LS.
            RayResult volatile *mutex = (RayResult *)atomicArea;
            spu_writech(MFC_LSA, (uint32_t)mutex);
            spu_writech(MFC_EAL, rayResults);
            spu_writech(MFC_Cmd, MFC_GETLLAR_CMD);
            spu_readch(MFC_RdAtomicStat);

            //rayResults contains the atomically retrieved min
            // test here store new(if necesarry) result back into rayResults

            RayResult *curr = (RayResult *)mutex;
            if (curr->t <= rayResult0.t) 
            {
                //EDGE_PRINTF("curr: %.3f\tresult: %.3f\n", curr->t, rayResult0.t);
                break;
            }

            curr->t = rayResult0.t;
            curr->uid = segmentId;
            curr->intersection[0] = rayResult0.intersection[0];
            curr->intersection[1] = rayResult0.intersection[1];
            curr->intersection[2] = rayResult0.intersection[2];
            curr->normal[0] = rayResult0.normal[0];
            curr->normal[1] = rayResult0.normal[1];
            curr->normal[2] = rayResult0.normal[2];

            spu_writech(MFC_LSA, (uint32_t)mutex);
            spu_writech(MFC_EAL, rayResults);
            spu_writech(MFC_Cmd, MFC_PUTLLC_CMD);
            result = spu_readch(MFC_RdAtomicStat);
            if(isEnabled)
                spu_ienable();
        } while (result & 1);

	}
}

