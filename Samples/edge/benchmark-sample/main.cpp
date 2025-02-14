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
#include "edge/anim/edgeanim_ppu.h"

#include "edge/geom/edgegeom_structs.h"
#include "spu/job_geom_interface.h"

#include "main.h"

#include "msaaResolveDriver.h"
static uint32_t g_msaaMode = CELL_GCM_SURFACE_CENTER_1;
CellGcmSurface				 mMSAASurface;
CellGcmTexture				 mMSAATexture;
MSAAResolveDriver			mMSAAResolveShader;

#define SPURS_SPU_NUM					2
#define SPU_THREAD_GROUP_PRIORITY		250
#define EVENT_FLAG_BIT					(0U)					// bit used by job_send_event (between 0 and 15)
#define EVENT_FLAG_BITS					(1U<<(EVENT_FLAG_BIT))	// bit mask for job_send_event

const uint32_t EDGE_SHARED_OUTPUT_BUFFER_SIZE = 0*1024*1024;
const uint32_t EDGE_RING_OUTPUT_BUFFER_SIZE = 3*1024*1024; // per SPU
const uint32_t COMMAND_BUFFER_SIZE = 4*1024*1024;

#define NUM_CHARACTERS					128*2
#define MAX_SEGMENTS					(NUM_CHARACTERS * 10)

static JobGeomPerformanceCounters g_spuTimingTable[MAX_SEGMENTS]  __attribute__((__aligned__(128)));;
static JobGeomPerformanceCounters g_spuTotalTicks;
static uint32_t g_ppuElapsedCycles = 0;

// This must be the index of the first of a contiguous block of RSX labels,
// one for each SPU.  Labels 0-63 are reserved by the OS, and your application
// and its libraries may have reserved others.  128 seems safe for this sample app.
const uint32_t kFirstRsxLabelIndex = 128;

using namespace cell;
using namespace cell::Gcm;

// instantiate the class
SampleApp app;

static const unsigned int textureDimension = 1024;

/* Animation job setup (spurs job 2.0):
 * 
 * Setup:
 * - input buffer = the blend tree (per character):
 *   - one buffer 
 *   - input DMA list gathers the array of blend tree branches and the array of blend tree leaves in single buffer
 * - read only cached buffers (assumed to be shared between multiple characters)
 *   - global configuration EdgeAnimPpuContext (potentially shared between all animation jobs)
 *   - skeleton (potentially shared between multiple characters)
 *   - despite the appearance, cached-read-only buffers are not handled through a conventional DMA list and all these
 *     elements represent independent buffers. 
 * - output buffer
 *   - output matrices (this buffer is alive after the end of the job - we must use the supplied DMA tag to "put" it)
 * - scratch buffer
 *   - temporary data allocated during the lifetime of our job
 *   - that's where the pose stack and temporary structures are allocated by edgeAnimSpuInitialize()
 * - user parameters
 *   - 3 x 16 bytes = root joint (unique per character, not part of the blend tree)
 *   - 1 x 2 bytes = number of blend tree branches
 *   - 1 x 2 bytes = number of blend tree leaves
 *   - 1 x 4 bytes = main memory address of wher to DMA the output matrices
 *
 * Notes: 
 *   Buffer and parameter ordering must strictly match the SPU side -see spu/job_anim.cpp 
 */
void SampleApp::CreateAnimJob(CellSpursJob256* jobAnim, void* outputMatrices, const struct EdgeAnimBlendBranch* blendBranches, uint32_t numBlendBranches, const struct EdgeAnimBlendLeaf* blendLeaves, uint32_t numBlendLeaves, const EdgeAnimJointTransform* rootJoint)
{
    int ret;

    // embedded SPU ELF symbols
    extern char _binary_job_job_anim_spu_bin_start[];   
    extern char _binary_job_job_anim_spu_bin_size[];

    // job setup
    __builtin_memset(jobAnim, 0, sizeof(*jobAnim));
    jobAnim->header.eaBinary = (uintptr_t)_binary_job_job_anim_spu_bin_start;
    jobAnim->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_anim_spu_bin_size);
    unsigned int userDataCount = 0;

    // input buffer - DMA list (must match code in cellSpursJobMain on the SPU side)
    unsigned int startInputDmaList = userDataCount;
    ret = cellSpursJobGetInputList(&jobAnim->workArea.dmaList[userDataCount++], EDGE_ALIGN(numBlendBranches * sizeof(EdgeAnimBlendBranch), 16U), reinterpret_cast<uintptr_t>(blendBranches));
    assert(ret == CELL_OK);
    ret = cellSpursJobGetInputList(&jobAnim->workArea.dmaList[userDataCount++], EDGE_ALIGN(numBlendLeaves * sizeof(EdgeAnimBlendLeaf), 16U), reinterpret_cast<uintptr_t>(blendLeaves));
    assert(ret == CELL_OK);
    ret = cellSpursJobGetInputList(&jobAnim->workArea.dmaList[userDataCount++], EDGE_ALIGN(mSkeleton->numJoints * 48, 16U), reinterpret_cast<uintptr_t>(mInvBindMatrices));
    assert(ret == CELL_OK);
    jobAnim->header.sizeDmaList = (userDataCount - startInputDmaList) * sizeof(jobAnim->workArea.dmaList[0]);
    for(unsigned int listLoop = startInputDmaList; listLoop < userDataCount; listLoop++) {
        jobAnim->header.sizeInOrInOut += (jobAnim->workArea.dmaList[listLoop] >> 32);
    }
    
    // read-only cached buffer contains the global context (pose cache per spu etc) and the skeleton (since we assume it's shared by many characters)
    uint32_t startCacheDmaList = userDataCount;
    ret = cellSpursJobGetCacheList(&jobAnim->workArea.dmaList[userDataCount++], EDGE_ALIGN(sizeof(mEdgeAnimPpuContext), 16), reinterpret_cast<uintptr_t>(&mEdgeAnimPpuContext));
    assert(ret == CELL_OK);
    ret = cellSpursJobGetCacheList(&jobAnim->workArea.dmaList[userDataCount++], EDGE_ALIGN(mSkeleton->sizeTotal, 16), reinterpret_cast<uintptr_t>(mSkeleton));
    assert(ret == CELL_OK);
    jobAnim->header.sizeCacheDmaList = (userDataCount - startCacheDmaList) * sizeof(jobAnim->workArea.dmaList[0]);

    // output buffer contains transposed 3x4 skinning matrices (warning: for the internal buffer, numMatrices must aligned to a multiple of 4)
    unsigned int numJointsAligned = EDGE_ALIGN(mSkeleton->numJoints, 4U);
    jobAnim->header.sizeOut = EDGE_ALIGN(numJointsAligned * 48, 16U);

    // scratch buffer
    jobAnim->header.sizeScratch = 128000U >> 4U;    // Scratch size (TODO: provide interfaces to query how much scratch is needed? not sure how to expose this) 

    // add root joint (must be aligned to 16 bytes / match code in cellSpursJobMain on the SPU side)
    assert(rootJoint);
    userDataCount++;
    assert(EDGE_IS_ALIGNED(&jobAnim->workArea.userData[userDataCount], 16));
    __builtin_memcpy(&jobAnim->workArea.userData[userDataCount], rootJoint, sizeof(*rootJoint));    
    userDataCount += EDGE_ALIGN(sizeof(*rootJoint), sizeof(jobAnim->workArea.userData[0])) / sizeof(jobAnim->workArea.userData[0]);

    // additional parameters (must match code in cellSpursJobMain on the SPU side)
    assert(EDGE_IS_ALIGNED(outputMatrices, 16));
    jobAnim->workArea.userData[userDataCount++] = (static_cast<uint64_t>(numBlendBranches) << (32+16)) 
                                                | (static_cast<uint64_t>(numBlendLeaves) << 32)
                                                | (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(outputMatrices))); 


    // check for overflow - if it does, you want to convert this job to a larger cellSpursJobN
    assert(userDataCount <= (sizeof(jobAnim->workArea.userData) / sizeof(jobAnim->workArea.userData[0])));

    // enable memory checking in jobs.  SPURS will place a memory check marker in 16 byte buffers added after IO buffer, read only data, and stack, and check the integrity 
    // of the marker at the end of the job.  Enabling this flag will add 3-4% to SPURS overhead of running jobs.  Linking with -mspurs-job instead of -mspurs-job-initialize
    // will cause this flag to be ignored.
    jobAnim->header.jobType = CELL_SPURS_JOB_TYPE_MEMORY_CHECK;
}

/* Create a simple "send_event" job
 *
 * - This job is required to be notifed of the completion of the job list
 * - Similar to all SPURS job samples; please refer to them for more detail.
 * - Make sure you always end your command list this way :
 *     CELL_SPURS_JOB_COMMAND_SYNC              // important - enforces job ordering
 *     CELL_SPURS_JOB_COMMAND_JOB(&jobSendEvent)// the job itself
 *     CELL_SPURS_JOB_COMMAND_END               // end of command list
 */
static void CreateSendEventJob(CellSpursJob256* jobSendEvent, CellSpursEventFlag* spursEventFlag, uint16_t bitsEventFlag)
{
    // embedded SPU ELF symbols
    extern char _binary_job_job_send_event_spu_bin_start[]; 
    extern char _binary_job_job_send_event_spu_bin_size[];

    // Job setup
    __builtin_memset(jobSendEvent, 0, sizeof(*jobSendEvent));
    jobSendEvent->header.eaBinary = (uintptr_t)_binary_job_job_send_event_spu_bin_start;
    jobSendEvent->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_send_event_spu_bin_size);
    jobSendEvent->workArea.userData[0] = (uintptr_t)spursEventFlag;
	jobSendEvent->workArea.userData[1] = (uint64_t)bitsEventFlag;
}

void SampleApp::runAnimJobs()
{
    // jobs
    static CellSpursJob256 jobsAnim[NUM_CHARACTERS];
    static CellSpursJob256 jobEnd;
    CreateSendEventJob(&jobEnd, &mSpursEventFlag, EVENT_FLAG_BITS);

    // create anim blend tree - array of 4 leaves per character since they must be aligned (DMA)
    static EdgeAnimBlendLeaf leaves[NUM_CHARACTERS][4] __attribute__((__aligned__(16)));

    // create anim jobs
    int numJobs = 0;
    for(int i=0; i<NUM_CHARACTERS; i++) {
        leaves[i][0] = EdgeAnimBlendLeaf(mCharacterStates[i].animation, mCharacterStates[i].animation->sizeHeader, mCharacterStates[i].evalTime);
        CreateAnimJob(&jobsAnim[i], mCharacterStates[i].skinningMatrices, NULL, 0, &leaves[i][0], 1, &mCharacterStates[i].rootJoint);

		int ret = cellSpursCheckJob((CellSpursJob256*)&jobsAnim[i], sizeof(JobGeom256), sizeof(CellSpursJob256));
		if ( ret != CELL_OK){
			printf("cellSpursCheckJob failed : 0x%x\n", ret);
		}

        numJobs++;
    }

    // create a job list of numJobs jobs (instead of adding each job individually)
    static CellSpursJobList jobList;
    jobList.eaJobList = (uint64_t)jobsAnim;
    jobList.numJobs = numJobs;
    jobList.sizeOfJob = sizeof(jobsAnim[0]);

    // init anim command list
    static uint64_t command_list_anim[4];
    unsigned int idxCmdList = 0;
    command_list_anim[idxCmdList++] = CELL_SPURS_JOB_COMMAND_JOBLIST(&jobList);
    command_list_anim[idxCmdList++] = CELL_SPURS_JOB_COMMAND_SYNC;
    command_list_anim[idxCmdList++] = CELL_SPURS_JOB_COMMAND_JOB(&jobEnd);
    command_list_anim[idxCmdList++] = CELL_SPURS_JOB_COMMAND_END;

    // init job chain
    static CellSpursJobChain jobChain;
    static CellSpursJobChainAttribute jobChainAttributes;

    const unsigned int MAX_CONTENTION = SPURS_SPU_NUM;          /* up to SPURS_SPU_NUM can work on this workload */
    const unsigned int MAX_GRAB = 8;                            /* 8 characters grabbed at a time */
    const uint8_t WORKLOAD_PRIORITIES[8] = {8,8,8,8,8,8,8,8};
    int ret = cellSpursJobChainAttributeInitialize(&jobChainAttributes, 
        command_list_anim, sizeof(jobsAnim[0]),
        MAX_GRAB, WORKLOAD_PRIORITIES, MAX_CONTENTION,
        true, 0, 1, false, sizeof(CellSpursJob256), MAX_CONTENTION);
    if (ret != CELL_OK){
        printf("cellSpursJobChainAttributeInitialize failed : 0x%x\n", ret);
    }
	ret = cellSpursJobChainAttributeSetName(&jobChainAttributes, "anim-jobchain");
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
    
    // run the job chain
    ret = cellSpursRunJobChain(&jobChain);
    if (ret != CELL_OK){
        printf("cellSpursRunJobChain failed : 0x%x\n", ret);
    }

    // wait for completion (SendEvent job) & shutdown
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
}

void SampleApp::CreateGeomJob(JobGeom256* job, CellGcmContextData *ctx, EdgeGeomPpuConfigInfo *info, void *skinningMatrices, uint32_t jobId) const
{
    // embedded SPU ELF symbols
    extern char _binary_job_job_geom_spu_bin_start[];   
    extern char _binary_job_job_geom_spu_bin_size[];

	// Steam descriptor
	job->inputDmaList[kJobGeomDmaOutputStreamDesc].eal  = (uintptr_t)info->spuOutputStreamDesc;
	job->inputDmaList[kJobGeomDmaOutputStreamDesc].size = info->spuOutputStreamDescSize;

	// Indexes
	job->inputDmaList[kJobGeomDmaIndexes_0].eal  = (uintptr_t)info->indexes;
	job->inputDmaList[kJobGeomDmaIndexes_0].size = info->indexesSizes[0];
	job->inputDmaList[kJobGeomDmaIndexes_1].eal  = job->inputDmaList[kJobGeomDmaIndexes_0].eal + job->inputDmaList[kJobGeomDmaIndexes_0].size;
	job->inputDmaList[kJobGeomDmaIndexes_1].size = info->indexesSizes[1];
	
	// Skinning Matrices
	job->inputDmaList[kJobGeomDmaSkinMatrices_0].eal  = info->skinMatricesByteOffsets[0] + (uintptr_t)skinningMatrices;
	job->inputDmaList[kJobGeomDmaSkinMatrices_0].size = info->skinMatricesSizes[0];
	job->inputDmaList[kJobGeomDmaSkinMatrices_1].eal  = info->skinMatricesByteOffsets[1] + (uintptr_t)skinningMatrices;
	job->inputDmaList[kJobGeomDmaSkinMatrices_1].size = info->skinMatricesSizes[1];

	// Skinning Indexes & Weights
	job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_0].eal  = (uintptr_t)info->skinIndexesAndWeights;
	job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_0].size = info->skinIndexesAndWeightsSizes[0]; 
	job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_1].eal  = job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_0].eal + job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_0].size;
	job->inputDmaList[kJobGeomDmaSkinIndexesAndWeights_1].size = info->skinIndexesAndWeightsSizes[1];

	// Vertexes (stream 1)
	job->inputDmaList[kJobGeomDmaVertexes1_0].eal  = (uintptr_t)info->spuVertexes[0];
	job->inputDmaList[kJobGeomDmaVertexes1_0].size = info->spuVertexesSizes[0];
	job->inputDmaList[kJobGeomDmaVertexes1_1].eal  = job->inputDmaList[kJobGeomDmaVertexes1_0].eal + job->inputDmaList[kJobGeomDmaVertexes1_0].size;
	job->inputDmaList[kJobGeomDmaVertexes1_1].size = info->spuVertexesSizes[1];
	job->inputDmaList[kJobGeomDmaVertexes1_2].eal  = job->inputDmaList[kJobGeomDmaVertexes1_1].eal + job->inputDmaList[kJobGeomDmaVertexes1_1].size;
	job->inputDmaList[kJobGeomDmaVertexes1_2].size = info->spuVertexesSizes[2];

	// Vertexes (stream 2)
	job->inputDmaList[kJobGeomDmaVertexes2_0].eal  = (uintptr_t)info->spuVertexes[1];
	job->inputDmaList[kJobGeomDmaVertexes2_0].size = info->spuVertexesSizes[3];
	job->inputDmaList[kJobGeomDmaVertexes2_1].eal  = job->inputDmaList[kJobGeomDmaVertexes2_0].eal + job->inputDmaList[kJobGeomDmaVertexes2_0].size;
	job->inputDmaList[kJobGeomDmaVertexes2_1].size = info->spuVertexesSizes[4];
	job->inputDmaList[kJobGeomDmaVertexes2_2].eal  = job->inputDmaList[kJobGeomDmaVertexes2_1].eal + job->inputDmaList[kJobGeomDmaVertexes2_1].size;
	job->inputDmaList[kJobGeomDmaVertexes2_2].size = info->spuVertexesSizes[5];

	// Triangle Culling Data
	job->inputDmaList[kJobGeomDmaViewportInfo].eal  = (uintptr_t)&mViewportInfo;
	job->inputDmaList[kJobGeomDmaViewportInfo].size = sizeof(EdgeGeomViewportInfo);
	job->inputDmaList[kJobGeomDmaLocalToWorldMatrix].eal  = (uintptr_t)&mLocalToWorldMatrix;
	job->inputDmaList[kJobGeomDmaLocalToWorldMatrix].size = sizeof(EdgeGeomLocalToWorldMatrix);
	
	// SpuConfigInfo
	job->inputDmaList[kJobGeomDmaSpuConfigInfo].eal	 = (uintptr_t)info;
	job->inputDmaList[kJobGeomDmaSpuConfigInfo].size = sizeof(EdgeGeomSpuConfigInfo);
	
	// Software Fixed Point Format Offsets
	job->inputDmaList[kJobGeomDmaFixedPointOffsets1].eal  = (uintptr_t)info->fixedOffsets[0];	 
	job->inputDmaList[kJobGeomDmaFixedPointOffsets1].size = info->fixedOffsetsSize[0];
	job->inputDmaList[kJobGeomDmaFixedPointOffsets2].eal  =	(uintptr_t)info->fixedOffsets[1];
	job->inputDmaList[kJobGeomDmaFixedPointOffsets2].size = info->fixedOffsetsSize[1];

	// Input Streams Descriptors
	job->inputDmaList[kJobGeomDmaInputStreamDesc1].eal  = (uintptr_t)info->spuInputStreamDescs[0];
	job->inputDmaList[kJobGeomDmaInputStreamDesc1].size = info->spuInputStreamDescSizes[0];
	job->inputDmaList[kJobGeomDmaInputStreamDesc2].eal  = (uintptr_t)info->spuInputStreamDescs[1];
	job->inputDmaList[kJobGeomDmaInputStreamDesc2].size = info->spuInputStreamDescSizes[1];

	// --- User Data ---

	job->userData.eaOutputBufferInfo = (uintptr_t)&mOutputBufferInfo;

	// Command Buffer Hole
	uint32_t holeSize = info->spuConfigInfo.commandBufferHoleSize << 4;
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
	job->userData.eaBlendShapeInfo = 0; // no blend shapes in this scene
	job->userData.blendShapeInfoCount = 0;

	job->userData.eaPerformanceCounters = (uintptr_t)(g_spuTimingTable + jobId);

	job->header.eaBinary = (uintptr_t)_binary_job_job_geom_spu_bin_start;
	job->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_geom_spu_bin_size);
	job->header.sizeDmaList = kJobGeomInputDmaEnd*8;
	job->header.eaHighInput = 0;
	job->header.useInOutBuffer = 1;
	job->header.sizeInOrInOut = info->ioBufferSize;
	//job->header.sizeOut = 0;
	job->header.sizeStack = 0;
	job->header.sizeScratch = info->scratchSizeInQwords;
	//job->header.eaHighCache = 0;
	job->header.sizeCacheDmaList = 0;

    // enable memory checking in jobs.  SPURS will place a memory check marker in 16 byte buffers added after IO buffer, read only data, and stack, and check the integrity 
    // of the marker at the end of the job.  Enabling this flag will add 3-4% to SPURS overhead of running jobs.  Linking with -mspurs-job instead of -mspurs-job-initialize
    // will cause this flag to be ignored.
    job->header.jobType = CELL_SPURS_JOB_TYPE_MEMORY_CHECK;
}

void SampleApp::runGeomJobs()
{
	// Reset Parameters
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
    CreateSendEventJob(&jobEnd, &mSpursEventFlag, EVENT_FLAG_BITS);

    // init jobs
    int numJobs = 0;
    static JobGeom256 jobsGeom[MAX_SEGMENTS];
    for(uint32_t i=0; i<NUM_CHARACTERS; ++i) {
        for(uint32_t j = 0; j < mSceneData->numPpuConfigInfos; ++j)
		{			
			EdgeGeomPpuConfigInfo *segmentInfo = ((EdgeGeomPpuConfigInfo*)(mSceneData->offsetToPpuConfigInfos)) + j;
			// Bind the RSX-only vertex attributes for this job before it's created
			if (segmentInfo->rsxOnlyVertexesSize > 0)
			{
				EdgeGeomVertexStreamDescription *rsxOnlyStreamDesc = (EdgeGeomVertexStreamDescription*)(segmentInfo->rsxOnlyStreamDesc);
				uint32_t rsxVertexesOffset = 0;
				cellGcmAddressToOffset(segmentInfo->rsxOnlyVertexes, &rsxVertexesOffset);
				for(uint32_t iRsxAttr=0; iRsxAttr < rsxOnlyStreamDesc->numAttributes; ++iRsxAttr)
				{
					EdgeGeomAttributeBlock &attr = rsxOnlyStreamDesc->blocks[iRsxAttr].attributeBlock;
					cellGcmSetVertexDataArray(&pbContext, attr.vertexProgramSlotIndex, 0, rsxOnlyStreamDesc->stride,
						attr.componentCount, attr.format, CELL_GCM_LOCATION_LOCAL,
						rsxVertexesOffset + attr.offset);
				}
			}

			JobGeom256 *job = &jobsGeom[numJobs];
            CreateGeomJob(job, &pbContext, segmentInfo, mCharacterStates[i].skinningMatrices, numJobs);

			int ret = cellSpursCheckJob((CellSpursJob256*)job, sizeof(JobGeom256),
				sizeof(CellSpursJob256));
			if ( ret != CELL_OK){
				printf("cellSpursCheckJob failed : 0x%x\n", ret);
			}

            numJobs++;
        }
    }

    // create a job list of numJobs jobs (instead of adding each job individually)
    static CellSpursJobList jobList;
    jobList.eaJobList = (uint64_t)jobsGeom;
    jobList.numJobs = numJobs;
    jobList.sizeOfJob = sizeof(jobsGeom[0]);

    // init geom command list
    static uint64_t command_list_geom[5];
    unsigned int idxCmdList = 0;
    command_list_geom[idxCmdList++] = CELL_SPURS_JOB_COMMAND_JOBLIST(&jobList);
    command_list_geom[idxCmdList++] = CELL_SPURS_JOB_COMMAND_SYNC;
	command_list_geom[idxCmdList++] = CELL_SPURS_JOB_COMMAND_FLUSH;
    command_list_geom[idxCmdList++] = CELL_SPURS_JOB_COMMAND_JOB(&jobEnd);
    command_list_geom[idxCmdList++] = CELL_SPURS_JOB_COMMAND_END;

    // init job chain
    static CellSpursJobChain jobChain;
    static CellSpursJobChainAttribute jobChainAttributes;

    const unsigned int MAX_CONTENTION = SPURS_SPU_NUM;          /* up to SPURS_SPU_NUM can work on this workload */
    const unsigned int MAX_GRAB = 8;                            /* 8 characters grabbed at a time */
    const uint8_t WORKLOAD_PRIORITIES[8] = {8,8,8,8,8,8,8,8};
    int ret = cellSpursJobChainAttributeInitialize(&jobChainAttributes, 
        command_list_geom, sizeof(jobsGeom[0]),
        MAX_GRAB, WORKLOAD_PRIORITIES, MAX_CONTENTION,
        true, 0, 1, false, sizeof(CellSpursJob256), MAX_CONTENTION);
    if (ret != CELL_OK) {
        printf("cellSpursJobChainAttributeInitialize failed : 0x%x\n", ret);
    }

    // run jobs
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
uint64_t ppuStart;
SYS_TIMEBASE_GET(ppuStart);
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

    // wait for joblist completion
    uint16_t ev_mask = EVENT_FLAG_BITS;
    ret = cellSpursEventFlagWait(&mSpursEventFlag,&ev_mask,CELL_SPURS_EVENT_FLAG_AND);
	if (ret != CELL_OK) {
        printf("cellSpursEventFlagWait failed : 0x%x\n", ret);
    }	
    ret = cellSpursShutdownJobChain(&jobChain);
	if (ret != CELL_OK) {
        printf("cellSpursShutdownJobChain failed : 0x%x\n", ret);
    }	
    ret = cellSpursJoinJobChain(&jobChain);
	if (ret != CELL_OK) {
        printf("cellSpursJoinJobChain failed : 0x%x\n", ret);
    }
uint64_t ppuEnd;
SYS_TIMEBASE_GET(ppuEnd);
 g_ppuElapsedCycles = (uint32_t)( (ppuEnd - ppuStart) * 40 );

	memset(&g_spuTotalTicks, 0, sizeof(JobGeomPerformanceCounters));
	
	for(int32_t iJob=0; iJob<numJobs; ++iJob)
	{
		const JobGeomPerformanceCounters &perfInfo = g_spuTimingTable[iJob];
		
		g_spuTotalTicks.m_numInputVertexes  += perfInfo.m_numInputVertexes;
		g_spuTotalTicks.m_numInputIndexes   += perfInfo.m_numInputIndexes;
		g_spuTotalTicks.m_numOutputVertexes += perfInfo.m_numOutputVertexes;
		g_spuTotalTicks.m_numOutputIndexes  += perfInfo.m_numOutputIndexes;

		g_spuTotalTicks.m_initTicks += -(perfInfo.m_initTicks - perfInfo.m_start) * 40;
		g_spuTotalTicks.m_decompressVertexesTicks += -(perfInfo.m_decompressVertexesTicks - perfInfo.m_initTicks) * 40;
		g_spuTotalTicks.m_blendTicks += -(perfInfo.m_blendTicks - perfInfo.m_decompressVertexesTicks) * 40;
		g_spuTotalTicks.m_skinTicks += -(perfInfo.m_skinTicks - perfInfo.m_blendTicks) * 40;
		g_spuTotalTicks.m_decompressIndexesTicks += -(perfInfo.m_decompressIndexesTicks - perfInfo.m_skinTicks) * 40;
		g_spuTotalTicks.m_cullTicks += -(perfInfo.m_cullTicks - perfInfo.m_decompressIndexesTicks) * 40;

		if (perfInfo.m_outitTicks != 0)
		{
			g_spuTotalTicks.m_allocateTicks += -(perfInfo.m_allocateTicks - perfInfo.m_cullTicks) * 40;
			g_spuTotalTicks.m_outputIndexesTicks += -(perfInfo.m_outputIndexesTicks - perfInfo.m_allocateTicks) * 40;
			g_spuTotalTicks.m_compressVertexesTicks += -(perfInfo.m_compressVertexesTicks - perfInfo.m_outputIndexesTicks) * 40;
			g_spuTotalTicks.m_outputVertexesTicks += -(perfInfo.m_outputVertexesTicks - perfInfo.m_compressVertexesTicks) * 40;
			g_spuTotalTicks.m_outitTicks +=	-(perfInfo.m_outitTicks - perfInfo.m_outputVertexesTicks) * 40;
		}
	}

	memset(g_spuTimingTable, 0, MAX_SEGMENTS * sizeof(JobGeomPerformanceCounters));
}

void SampleApp::renderWithoutEdge(void)
{
	CellGcmContextData pbContext;
	pbContext.current = (uint32_t*)mCommandBuffer;
	pbContext.end = (uint32_t*)((uint32_t)mCommandBuffer + COMMAND_BUFFER_SIZE - 4);
	pbContext.begin = (uint32_t*)mCommandBuffer;
	pbContext.callback = 0;

	cellGcmSetCullFaceEnable(&pbContext, CELL_GCM_TRUE);

	uint32_t boneArrayStart = 6; // constant offset of the "boneArray" parameter
	(void)boneArrayStart;

    uint32_t i=0;
	for(i=0; i<NUM_CHARACTERS; ++i)
	{
		uint32_t j=0;
		for(j = 0; j < mSceneData->numPpuConfigInfos; ++j)
		{			
			const EdgeGeomPpuConfigInfo *segmentInfo = ((const EdgeGeomPpuConfigInfo*)(mSceneData->offsetToPpuConfigInfos)) + j;
			
			// Set up vertex attribute pointers for this segment
			uint32_t vertexStreamOffset = 0;
			cellGcmAddressToOffset(mSpuVertexesInVram[j], &vertexStreamOffset);
			const uint32_t stride1 = 24;
			cellGcmSetVertexDataArray(&pbContext, 0, 0, stride1, 3, CELL_GCM_VERTEX_F, // position
				CELL_GCM_LOCATION_LOCAL, vertexStreamOffset + 0);
			cellGcmSetVertexDataArray(&pbContext, 2, 0, stride1, 1, CELL_GCM_VERTEX_CMP, // normal
				CELL_GCM_LOCATION_LOCAL, vertexStreamOffset + 12);
			cellGcmSetVertexDataArray(&pbContext, 14, 0, stride1, 4, CELL_GCM_VERTEX_S1, // tangent
				CELL_GCM_LOCATION_LOCAL, vertexStreamOffset + 16);
			// UVs are in a separate stream
			uint32_t rsxVertexesOffset = 0;
			cellGcmAddressToOffset(segmentInfo->rsxOnlyVertexes, &rsxVertexesOffset);
			cellGcmSetVertexDataArray(&pbContext, 8, 0, 4, 2, CELL_GCM_VERTEX_SF, // UV
				CELL_GCM_LOCATION_LOCAL, rsxVertexesOffset + 0);

			// Don't forget the bone indices and weights (CG_ATTR3 and CG_ATTR4).
			// Note that these values are interleaved component-wise in the Edge streams;
			// we'll de-interleave them in the vertex program.
			uint32_t skinningStreamOffset = 0;
			cellGcmAddressToOffset(mSpuSkinningInVram[j], &skinningStreamOffset);
			const uint32_t stride2 = 8;
			cellGcmSetVertexDataArray(&pbContext, 3, 0, stride2, 4, CELL_GCM_VERTEX_UB256,
				CELL_GCM_LOCATION_LOCAL, skinningStreamOffset + 0);
			cellGcmSetVertexDataArray(&pbContext, 4, 0, stride2, 4, CELL_GCM_VERTEX_UB256,
				CELL_GCM_LOCATION_LOCAL, skinningStreamOffset + 4);

			// Send the appropriate ranges of skinning matrices to the vertex program
			const float *firstBatch = mCharacterStates[i].skinningMatrices
				+ segmentInfo->skinMatricesByteOffsets[0]/4;
			const float *secondBatch = mCharacterStates[i].skinningMatrices
				+ segmentInfo->skinMatricesByteOffsets[1]/4;
			const uint32_t numConstantsInFirstBatch  = segmentInfo->skinMatricesSizes[0]/4;
			const uint32_t numConstantsInSecondBatch = segmentInfo->skinMatricesSizes[1]/4;
			cellGcmSetVertexProgramConstants(&pbContext, boneArrayStart + 0,
				numConstantsInFirstBatch, firstBatch);
			cellGcmSetVertexProgramConstants(&pbContext, boneArrayStart + numConstantsInFirstBatch/4,
				numConstantsInSecondBatch, secondBatch);

			// If the indexes are compressed, this is going to go horribly wrong...
			assert( (segmentInfo->spuConfigInfo.indexesFlavorAndSkinningFlavor >> 4) <= 1 );

			// Draw!
			uint32_t indexStreamOffset = 0;
			cellGcmAddressToOffset(mSpuIndexesInVram[j], &indexStreamOffset);
			cellGcmSetDrawIndexArray(&pbContext, CELL_GCM_PRIMITIVE_TRIANGLES,
				segmentInfo->spuConfigInfo.numIndexes, CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16, 
				CELL_GCM_LOCATION_LOCAL, indexStreamOffset);
		}
	}

	// cleanup -- disable backface culling and skinning vertex attributes
	cellGcmSetVertexDataArray(&pbContext, 3, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
	cellGcmSetVertexDataArray(&pbContext, 4, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
	cellGcmSetCullFaceEnable(&pbContext, CELL_GCM_FALSE);


    cellGcmSetReturnCommand(&pbContext);	

	ProfileBars::AddRsxMarker(0x20202000);		// grey rsx bar until that point (clear etc)
	cellGcmSetCallCommand(mCommandBufferOffset);
	ProfileBars::AddRsxMarker(0xff000000);		// red rsx bar indicates draw time	

	// kick rsx
    cellGcmFlush();
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

SampleApp::SampleApp() :
mLastUpdate(FWTime::sZero),
mTurnRate(500.f),
mMoveRate(25.f)
{
	mNear = 0.1f;
	mFar = 1000.f;
}

SampleApp::~SampleApp()
{
}

void SampleApp::initShader()
{
    // embedded shader symbols
    extern unsigned long _binary_vpshader_vpo_start;
	extern unsigned long _binary_vpshader_skinning_vpo_start;
    extern unsigned long _binary_fpshader_fpo_start;

	mCGVertexProgram           = (CGprogram)(void*)&_binary_vpshader_vpo_start;
	mCGVertexProgramSkinning   = (CGprogram)(void*)&_binary_vpshader_skinning_vpo_start;
	mCGFragmentProgram         = (CGprogram)(void*)&_binary_fpshader_fpo_start;

	// init
	cellGcmCgInitProgram(mCGVertexProgram);
	cellGcmCgInitProgram(mCGVertexProgramSkinning);
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
	cellGcmCgGetUCode(mCGVertexProgramSkinning, &ucode, &ucodeSize);
	mVertexProgramSkinningUCode = ucode;
}

static void* loadFile(const char* fileName, unsigned int* size = NULL)
{
    FILE* fp = fopen(fileName, "rb");
    if (!fp) {
        printf("cannot open file %s\n", fileName);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    unsigned int l = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    void* data = memalign(128, l);
    if (!data) {
        printf("can't allocate memory\n");
        return NULL;
    }
    fread(data, 1, l, fp);
    fclose(fp);

    if (size)
        *size = l;

    return data;
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

    mSceneData = (FileHeader*)loadFile(SYS_APP_HOME"/assets/elephant.edge");
    mColorTex = (uint8_t*)loadFile(SYS_APP_HOME"/assets/elephant-color.bin", &mColorTexSize);
    mNormalTex = (uint8_t*)loadFile(SYS_APP_HOME"/assets/elephant-normal.bin", &mNormalTexSize);

	// Make sure code and data were built with the same version of the Edge and Cell SDKs.
	// In these samples, any mismatch is considered a fatal error; in practice a warning
	// might be sufficient.
	if (mSceneData->edgeSdkVersion != EDGE_SDK_VERSION)
	{
		printf("ERROR: Input scene was built with an older version of libedgegeomtool (0x%08X vs 0x%08X)\n", mSceneData->edgeSdkVersion, EDGE_SDK_VERSION);
		sys_process_exit(1);
	}
	if (mSceneData->cellSdkVersion != CELL_SDK_VERSION)
	{
		printf("ERROR: Input scene was built with an older version of the Cell SDK (0x%08X vs 0x%08X)\n", mSceneData->cellSdkVersion, CELL_SDK_VERSION);
		sys_process_exit(1);
	}

    // resolve the scene's patches
    uint32_t* patchTable = (uint32_t*)(((char*)mSceneData) + mSceneData->offsetToPatches);
    for ( uint32_t i=0; i<mSceneData->numPatches; ++i )
    {
        uint32_t* patch = (uint32_t*)(((char*)mSceneData) + patchTable[i]);
        *patch = (uint32_t)(((char*)mSceneData) + *patch);
    }

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
	mTextures[0] = memalign(1024*1024, 2*mColorTexSize); // double texture buffer size to leave room for mipmaps
	mTextures[1] = memalign(1024*1024, 2*mNormalTexSize);	
	memcpy(mTextures[0], mColorTex, mColorTexSize);
	memcpy(mTextures[1], mNormalTex, mNormalTexSize);
	buildMipmaps(mTextures[0], (uint8_t *)(mTextures[0]) + mColorTexSize, textureDimension, textureDimension);
	buildMipmaps(mTextures[1], (uint8_t *)(mTextures[1]) + mNormalTexSize, textureDimension, textureDimension);
	cellGcmMapMainMemory(mTextures[0], 2*mColorTexSize, &dummy);
	cellGcmMapMainMemory(mTextures[1], 2*mNormalTexSize, &dummy);
    
	initShader();

	// Copy the Edge segment's RSX-only buffers into local memory, so that the RSX can see them.
	// This is something of a hack, as ideally you'd want to load this data directly into VRAM,
	// but it suffices for the purposes of this demo.
	uint8_t *geomBuffer = (uint8_t*)cellGcmUtilAllocateLocalMemory(1024*1024, 16);
	uint8_t *nextSegmentGeom = geomBuffer;
	mSpuVertexesInVram = (uint8_t**)malloc(mSceneData->numPpuConfigInfos * sizeof(uint8_t*));
	mSpuIndexesInVram  = (uint8_t**)malloc(mSceneData->numPpuConfigInfos * sizeof(uint8_t*));
	mSpuSkinningInVram = (uint8_t**)malloc(mSceneData->numPpuConfigInfos * sizeof(uint8_t*));
	for(uint32_t iSegment = 0; iSegment < mSceneData->numPpuConfigInfos; ++iSegment)
	{
		EdgeGeomPpuConfigInfo *segmentInfo = ((EdgeGeomPpuConfigInfo*)(mSceneData->offsetToPpuConfigInfos)) + iSegment;

		// Copy SPU vertexes and indexes into VRAM, so they can be rendered without Edge
		memcpy(nextSegmentGeom, segmentInfo->spuVertexes[0],
			segmentInfo->spuVertexesSizes[0] + segmentInfo->spuVertexesSizes[1] + segmentInfo->spuVertexesSizes[2]);
		mSpuVertexesInVram[iSegment] = nextSegmentGeom;
		nextSegmentGeom += 64*1024;
		memcpy(nextSegmentGeom, segmentInfo->skinIndexesAndWeights,
			segmentInfo->skinIndexesAndWeightsSizes[0] + segmentInfo->skinIndexesAndWeightsSizes[1]);
		mSpuSkinningInVram[iSegment] = nextSegmentGeom;
		nextSegmentGeom += 32*1024;
		memcpy(nextSegmentGeom, segmentInfo->indexes,
			segmentInfo->indexesSizes[0] + segmentInfo->indexesSizes[1]);
		mSpuIndexesInVram[iSegment] = nextSegmentGeom;
		nextSegmentGeom += 32*1024;

		// Skip this segment if it doesn't have a RSX-only stream
		if (segmentInfo->rsxOnlyVertexesSize == 0)
			continue;
		memcpy(nextSegmentGeom, segmentInfo->rsxOnlyVertexes,
			segmentInfo->rsxOnlyVertexesSize);
		// Update the segment's rsxOnlyVertexes pointer to reference the VRAM copy of the data.
		// NOTE: This leaks a pointer to the original rsxOnlyVertexes in main memory, but due
		// to the implementation of this sample it would be impossible to free it.
		segmentInfo->rsxOnlyVertexes = nextSegmentGeom;
		nextSegmentGeom += 32*1024;//segmentInfo->rsxOnlyVertexesSize;
	}

	// set default camera position
	mCamera.setPosition(Point3(0.0f, 10.0f, 20.0f));

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

	// intialise edge animation 
	edgeAnimPpuInitialize(&mEdgeAnimPpuContext, SPURS_SPU_NUM);

    // load skelton and animation
    mSkeleton = (EdgeAnimSkeleton*)loadFile(SYS_APP_HOME"/assets/elephant.skel");
    mAnimations[0] = (EdgeAnimAnimation*)loadFile(SYS_APP_HOME"/assets/idle.anim");
    mAnimations[1] = (EdgeAnimAnimation*)loadFile(SYS_APP_HOME"/assets/trunk.anim");
    mAnimations[2] = (EdgeAnimAnimation*)loadFile(SYS_APP_HOME"/assets/rear.anim");

    // load inverse bind matrices
    mInvBindMatrices = loadFile(SYS_APP_HOME"/assets/elephant.edge.invbind");

    // init characters
    mCharacterStates = (CharacterState*)memalign(16, NUM_CHARACTERS * sizeof(CharacterState));

    // allocate skinning matrices (which are transposed, 3x4)
    for(int i=0; i<NUM_CHARACTERS; ++i)
    {
        mCharacterStates[i].skinningMatrices = (float*)memalign(16, 48 * mSkeleton->numJoints);

        // init all matrices to identity
        for(int j = 0; j < mSkeleton->numJoints; ++j) {
            Matrix4 m( Matrix4::identity() );
            m = transpose(m);
            memcpy(&mCharacterStates[i].skinningMatrices[j*12], &m, 48);
        }
    }

    // init root joints (position characters in the scene)
    for(int i=0; i<NUM_CHARACTERS; ++i) {
        unsigned int d = (unsigned int) sqrtf((float) NUM_CHARACTERS);
        if ( d < 1 ) 
            d = 1;
        float x = (float) (i % d) - 0.5f * (float) d;
        float z = (float) (i / d) - 0.5f * (float) d;

        mCharacterStates[i].rootJoint.rotation = Quat(0,0,0,1);
        mCharacterStates[i].rootJoint.translation = Point3(x*5, 0, z*10);
        mCharacterStates[i].rootJoint.scale = Vector4(1, 1, 1, 1);

        float random = rand() / float( RAND_MAX );
        mCharacterStates[i].animation = mAnimations[ int(random*3) % MAX_ANIMATIONS ];
        mCharacterStates[i].evalTime = fmodf(10.0f * random, mCharacterStates[i].animation->duration);
    }

	// run the anim jobs once, to populate the skinning matrices with
	// valid data (even if animation isn't enabled by default)
	runAnimJobs();

	// init profile bars
	ProfileBars::Init(&mSpurs, SPURS_SPU_NUM);

	// Don't use Edge Geom by default
	mUseEdgeGeom = true;
	mEnableAnimation = true;
	mShowSpuDetails = true;
	
	{
		EdgeGeomPpuConfigInfo *segmentInfo = ((EdgeGeomPpuConfigInfo*)(mSceneData->offsetToPpuConfigInfos));
		mSkinningFlavor = segmentInfo->spuConfigInfo.indexesFlavorAndSkinningFlavor & 0x0F;
	}

	cellGcmSetFlipMode(CELL_GCM_DISPLAY_HSYNC);

	memset(g_spuTimingTable, 0, MAX_SEGMENTS * sizeof(JobGeomPerformanceCounters));

	// Create multisampled render target
	{
		mMSAAResolveShader.init();
		FWDisplayInfo &dispInfo = FWCellGCMWindow::getInstance()->getDisplayInfo();
		uint32_t colorPitch = cellGcmGetTiledPitchSize(dispInfo.mWidth * 4 * 2);	// RGBA8888, 4xMSAA 
		uint32_t depthPitch = cellGcmGetTiledPitchSize(dispInfo.mWidth * 4 * 2);	// D24S8, 4xMSAA 
		uint32_t colorBufferSize = colorPitch*((dispInfo.mHeight*2 + 31) & ~31);		// For tiles, height has to be aligned to 32 lines
		colorBufferSize = (colorBufferSize + 0xffff) & ~0xffff;						// For tiles, size has to be aligned to 0x10000
		uint32_t depthBufferSize = depthPitch*((dispInfo.mHeight*2 + 31) & ~31);
		depthBufferSize = (depthBufferSize + 0xffff) & ~0xffff;				
		void *colorBufferMem = cellGcmUtilAllocateLocalMemory(colorBufferSize, 0x10000);
		void *depthBufferMem = cellGcmUtilAllocateLocalMemory(depthBufferSize, 0x10000);
		uint32_t colorBufferOffset, depthBufferOffset;
		cellGcmAddressToOffset(colorBufferMem, &colorBufferOffset );
		cellGcmAddressToOffset(depthBufferMem, &depthBufferOffset );

		mMSAASurface.colorTarget		= CELL_GCM_SURFACE_TARGET_0;
		mMSAASurface.colorLocation[0]	= CELL_GCM_LOCATION_LOCAL;
		mMSAASurface.colorOffset[0]		= colorBufferOffset;
		mMSAASurface.colorPitch[0] 		= colorPitch;
		mMSAASurface.colorLocation[1]	= CELL_GCM_LOCATION_LOCAL;
		mMSAASurface.colorOffset[1]		= 0;
		mMSAASurface.colorPitch[1] 		= 64;
		mMSAASurface.colorLocation[2]	= CELL_GCM_LOCATION_LOCAL;
		mMSAASurface.colorOffset[2]		= 0;
		mMSAASurface.colorPitch[2] 		= 64;
		mMSAASurface.colorLocation[3]	= CELL_GCM_LOCATION_LOCAL;
		mMSAASurface.colorOffset[3]		= 0;
		mMSAASurface.colorPitch[3] 		= 64;
		mMSAASurface.depthFormat 	= CELL_GCM_SURFACE_Z24S8;
		mMSAASurface.depthLocation 	= CELL_GCM_LOCATION_LOCAL;
		mMSAASurface.depthOffset 	= depthBufferOffset;
		mMSAASurface.depthPitch 	= depthPitch;
		mMSAASurface.colorFormat	= CELL_GCM_SURFACE_A8R8G8B8;
		mMSAASurface.type			= CELL_GCM_SURFACE_PITCH;
		mMSAASurface.antialias 		= g_msaaMode;
		mMSAASurface.width 			= dispInfo.mWidth;
		mMSAASurface.height 		= dispInfo.mHeight;
		mMSAASurface.x 		 		= 0;
		mMSAASurface.y 		 		= 0;

		// Setup MSAA texture that aliases the MSAA surface
		mMSAATexture.format		= CELL_GCM_TEXTURE_A8R8G8B8 |
			CELL_GCM_TEXTURE_LN |
			CELL_GCM_TEXTURE_NR;
		mMSAATexture.mipmap		= 1;
		mMSAATexture.dimension	= CELL_GCM_TEXTURE_DIMENSION_2;
		mMSAATexture.cubemap		= CELL_GCM_FALSE;
		mMSAATexture.remap		= (CELL_GCM_TEXTURE_REMAP_REMAP << 14) |
			(CELL_GCM_TEXTURE_REMAP_REMAP << 12) |
			(CELL_GCM_TEXTURE_REMAP_REMAP << 10) |
			(CELL_GCM_TEXTURE_REMAP_REMAP << 8) |
			(CELL_GCM_TEXTURE_REMAP_FROM_B << 6) |
			(CELL_GCM_TEXTURE_REMAP_FROM_G << 4) |
			(CELL_GCM_TEXTURE_REMAP_FROM_R << 2) |
			(CELL_GCM_TEXTURE_REMAP_FROM_A);
		mMSAATexture.width		= dispInfo.mWidth;
		mMSAATexture.height		= dispInfo.mHeight;
		mMSAATexture.depth		= 1;
		mMSAATexture.location	= CELL_GCM_LOCATION_LOCAL;
		mMSAATexture.pitch		= colorPitch;
		mMSAATexture.offset		= colorBufferOffset;
	}

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

    // advance anim eval time

    for (int i=0; i<NUM_CHARACTERS; ++i) {
        mCharacterStates[ i ].evalTime += deltaTime;
        if ( mCharacterStates[ i ].evalTime >= mCharacterStates[ i ].animation->duration )
            mCharacterStates[ i ].evalTime = 0.0f;
    }

	// Edge Geom must always be enabled, so long as the scene is using compressed indexes
	//if (mpCircle->getBoolTrue())
	//	mUseEdgeGeom = !mUseEdgeGeom;
	if (mpTriangle->getBoolTrue())
		mEnableAnimation = !mEnableAnimation;
	if (mpSquare->getBoolTrue())
	{
		//mShowSpuDetails = !mShowSpuDetails;
		FWDisplayInfo &dispInfo = FWCellGCMWindow::getInstance()->getDisplayInfo();
		switch(g_msaaMode)
		{
			case CELL_GCM_SURFACE_CENTER_1:
				g_msaaMode = CELL_GCM_SURFACE_DIAGONAL_CENTERED_2;
				mMSAATexture.width		= dispInfo.mWidth * 2;
				mMSAATexture.height		= dispInfo.mHeight;
				break;
			case CELL_GCM_SURFACE_DIAGONAL_CENTERED_2:
				g_msaaMode = CELL_GCM_SURFACE_SQUARE_CENTERED_4;
				mMSAATexture.width		= dispInfo.mWidth * 2;
				mMSAATexture.height		= dispInfo.mHeight * 2;
				break;
			case CELL_GCM_SURFACE_SQUARE_CENTERED_4:
				g_msaaMode = CELL_GCM_SURFACE_SQUARE_ROTATED_4;
				mMSAATexture.width		= dispInfo.mWidth * 2;
				mMSAATexture.height		= dispInfo.mHeight * 2;
				break;
			case CELL_GCM_SURFACE_SQUARE_ROTATED_4:
				g_msaaMode = CELL_GCM_SURFACE_CENTER_1;
				mMSAATexture.width		= dispInfo.mWidth;
				mMSAATexture.height		= dispInfo.mHeight;
				break;
		}
		mMSAASurface.antialias = g_msaaMode;
	}
	if (mpCross->getBoolTrue())
	{
		// cycle through skinning flavors 1-3
		mSkinningFlavor++;
		if (mSkinningFlavor > 3)
			mSkinningFlavor = 1;
		for(uint32_t iSegment = 0; iSegment < mSceneData->numPpuConfigInfos; ++iSegment)
		{
			EdgeGeomPpuConfigInfo *segmentInfo = ((EdgeGeomPpuConfigInfo*)(mSceneData->offsetToPpuConfigInfos)) + iSegment;
			segmentInfo->spuConfigInfo.indexesFlavorAndSkinningFlavor &= 0xF0;
			segmentInfo->spuConfigInfo.indexesFlavorAndSkinningFlavor |= mSkinningFlavor;
		}

	}

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

	if (mEnableAnimation)
		runAnimJobs();

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
   	if (mUseEdgeGeom)
		cellGcmSetVertexProgram(mCGVertexProgram, mVertexProgramUCode);
	else
		cellGcmSetVertexProgram(mCGVertexProgramSkinning, mVertexProgramSkinningUCode);		
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

	uint32_t fbWidth = 1280;
	uint32_t fbHeight = 720;
	if (g_msaaMode != CELL_GCM_SURFACE_CENTER_1)
	{
		uint32_t clearMask = CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A;
		clearMask |= (CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_S);
		cellGcmSetPerfMonMarker("MSAA: Clear surface");
		cellGcmSetSurface(&mMSAASurface);
		cellGcmSetClearSurface(clearMask);
		cellGcmSetAntiAliasingControl(gCellGcmCurrentContext, 1, 0, 0, 0xffff);
	}

    mViewportInfo.scissorArea[0] = 0;
    mViewportInfo.scissorArea[1] = 0;
    mViewportInfo.scissorArea[2] = fbWidth;
    mViewportInfo.scissorArea[3] = fbHeight;

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

    mViewportInfo.sampleFlavor = g_msaaMode;

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

    // run character jobs and wait for RSX
	if (mUseEdgeGeom)
		runGeomJobs();
	else
		renderWithoutEdge();

	// cellGcmFinish is only there because of profile bars displayed for the current frame.
	ProfileBars::AddPpuMarker(0x0000ff00);
	static uint32_t finishReference = 0;
	cellGcmFinish(finishReference++);		
	//ProfileBars::Render();

	if (g_msaaMode != CELL_GCM_SURFACE_CENTER_1)
	{
		// MSAA resolve
		cellGcmSetPerfMonPushMarker("MSAA Resolve");
		cellGcmSetAntiAliasingControl(gCellGcmCurrentContext, 0, 0, 0, 0xffff);
		mMSAAResolveShader.apply(&mMSAATexture);
		cellGcmSetDepthTestEnable(false);
		FWCellGCMWindow::getInstance()->resetRenderTarget();
		cellGcmSetDrawBegin(CELL_GCM_PRIMITIVE_TRIANGLE_STRIP);
	
		float pTopLeftTex[] = {0, 0, 0, 0};
		cellGcmSetVertexData4f(1, pTopLeftTex);
		float pTopLeftPos[] = {-1, +1, +1, +1};
		cellGcmSetVertexData4f(0, pTopLeftPos);
	
		float pBotLeftTex[] = {0, 1, 0, 0};
		cellGcmSetVertexData4f(1, pBotLeftTex);
		float pBotLeftPos[] = {-1, -1, +1, +1};
		cellGcmSetVertexData4f(0, pBotLeftPos);
	
		float pTopRightTex[] = {1, 0, 0, 0};
		cellGcmSetVertexData4f(1, pTopRightTex);
		float pTopRightPos[] = {+1, +1, +1, +1};
		cellGcmSetVertexData4f(0, pTopRightPos);
	
		float pBotRightTex[] = {1, 1, 0, 0};
		cellGcmSetVertexData4f(1, pBotRightTex);
		float pBotRightPos[] = {+1, -1, +1, +1};
		cellGcmSetVertexData4f(0, pBotRightPos);
	
		cellGcmSetDrawEnd();
		cellGcmSetPerfMonPopMarker();
	}

	{
		// calc fps
		FWTimeVal	time = FWTime::getCurrentTime();
		float fFPS = 1.f / (float)(time - mLastTime);
		mLastTime = time;

		// print some messages
		FWDebugFont::setPosition(0, 0);
		FWDebugFont::setColor(1.f, 1.f, 1.f, 1.0f);

		FWDebugFont::printf("Edge Geom Benchmark Sample Application\n\n");
		FWDebugFont::printf("FPS: %.2f [%4.1f ms]\n", fFPS, 1000.0f / fFPS);
		//FWDebugFont::printf("EdgeGeom: %sabled\n", mUseEdgeGeom ? "en" : "dis");
		const char* flavorNames[4] = {"no-skinning", "no", "uniform", "non-uniform"};
		FWDebugFont::printf("skinning flavor (X): %s scale\n", flavorNames[mSkinningFlavor]);
		const char *msaaModeName = NULL;
		switch(g_msaaMode)
		{
			case CELL_GCM_SURFACE_CENTER_1:
				msaaModeName = "1x (None)";
				break;
			case CELL_GCM_SURFACE_DIAGONAL_CENTERED_2:
				msaaModeName = "2x grid";
				break;
			case CELL_GCM_SURFACE_SQUARE_CENTERED_4:
				msaaModeName = "4x grid";
				break;
			case CELL_GCM_SURFACE_SQUARE_ROTATED_4:
				msaaModeName = "4x rotated grid";
				break;
		}
		FWDebugFont::printf("MSAA mode (Square): %s\n", msaaModeName);

		if (mShowSpuDetails)
		{
			FWDebugFont::printf(" \n");

			const JobGeomPerformanceCounters &stats = g_spuTotalTicks;
			uint32_t inVerts  = stats.m_numInputVertexes;
			uint32_t outVerts = stats.m_numOutputVertexes;
			uint32_t inTris   = stats.m_numInputIndexes / 3;
			uint32_t outTris  = stats.m_numOutputIndexes / 3;
			
			FWDebugFont::printf("%8d input verts, %8d output (%5.1f%% culled)\n", inVerts, outVerts,
				100.0f - (float)outVerts*100.0f / (float)inVerts);
			FWDebugFont::printf("%8d input tris, %8d output (%5.1f%% culled)\n", inTris, outTris,
				100.0f - (float)outTris*100.0f / (float)inTris);

			uint32_t totalCycles = (1 + // to prevent a divide-by-zero
				stats.m_initTicks +
				stats.m_decompressVertexesTicks +
				stats.m_blendTicks +
				stats.m_skinTicks +
				stats.m_decompressIndexesTicks +
				stats.m_cullTicks +
				stats.m_allocateTicks +
				stats.m_outputIndexesTicks +
				stats.m_compressVertexesTicks +
				stats.m_outputVertexesTicks +
				stats.m_outitTicks);
			float toPercent = 100.0f / (float)totalCycles;
			(void)toPercent;

#define PRINT_STAT1(statName, cycles)									\
			FWDebugFont::printf("%12s: %8d cycles | %5.2f%% |\n",		\
				(statName), (cycles),									\
				(float)(cycles) * toPercent)
			
#define PRINT_STAT2(statName, cycles, numElements, elementName)			\
			FWDebugFont::printf("%12s: %8d cycles | %5.2f%% | %5.1f cycles/%s\n", \
				(statName), (cycles),									\
				(float)(cycles) * toPercent,							\
				(float)(cycles) / ((float)(numElements)+0.000001f),	(elementName))
			
			FWDebugFont::printf(" \n");

			PRINT_STAT1("Initialize",   stats.m_initTicks);
			PRINT_STAT2("Dec. Verts",   stats.m_decompressVertexesTicks, inVerts,  "vertex");
			PRINT_STAT2("Blendshapes",  stats.m_blendTicks,              inVerts,  "vertex");
			PRINT_STAT2("Skinning",     stats.m_skinTicks,               inVerts,  "vertex");
			PRINT_STAT2("Dec. Indexes", stats.m_decompressIndexesTicks,  inTris*3, "index");
			PRINT_STAT2("Tri Culling",  stats.m_cullTicks,               inTris,   "triangle");
			PRINT_STAT1("Allocate",     stats.m_allocateTicks);
			PRINT_STAT2("Output Tris",  stats.m_outputIndexesTicks,      outTris,  "triangle");
			PRINT_STAT2("Comp. Verts",  stats.m_compressVertexesTicks,   outVerts, "vertex");
			PRINT_STAT2("Output Verts", stats.m_outputVertexesTicks,     outVerts, "vertex");
			PRINT_STAT1("Cleanup",      stats.m_outitTicks);

			FWDebugFont::printf(" \n");
			FWDebugFont::printf("%8d total SPU cycles\n", totalCycles/SPURS_SPU_NUM);
			FWDebugFont::printf("%8d total PPU cycles (%4.1f%% job overhead)\n\n", g_ppuElapsedCycles,
				100.0f - (float)totalCycles*100.0f / ((float)SPURS_SPU_NUM*(float)g_ppuElapsedCycles));

			for(uint32_t iSpu=0; iSpu<SPURS_SPU_NUM; ++iSpu)
			{
				FWDebugFont::printf("SPU %d ring buffer stalls: %10d\n", iSpu, mOutputBufferInfo.ringInfo[iSpu].pad[0]);
				mOutputBufferInfo.ringInfo[iSpu].pad[0] = 0;
			}
		}
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
	free( mTextures[0] );
	free( mTextures[1] );
    for(int i=0; i<NUM_CHARACTERS; ++i) {
        free(mCharacterStates[i].skinningMatrices);
    }
    free(mCharacterStates);
    free(mSceneData);
    free(mColorTex);
    free(mNormalTex);
    free(mSkeleton);
    free(mAnimations[0]);
    free(mAnimations[1]);
    free(mAnimations[2]);

	ProfileBars::Shutdown();

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
