/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

/* Standard C Libraries */
#include <stdio.h>                      /* printf */
#include <stdlib.h>                     /* exit */
#include <assert.h>

#ifdef  __PPU__
/* PS3 version */
#include <sys/process.h>
#include <cell/spurs.h>
#include <cell/sysmodule.h>
#include <cell/cell_fs.h>
#include <spu_printf.h>
#include "edge/anim/edgeanim_ppu.h"
#else 
/* PC version */
#include <malloc.h>
#include "edge/anim/edgeanim_windows.h"
#include "spu/process_blend_tree.h"
#endif

#include "edge/edge_stdint.h"
#include "main.h"
#include "FWDebugFont.h"

#define SPURS_SPU_NUM				2
#define SPU_THREAD_GROUP_PRIORITY	250
#define EVENT_FLAG_BIT				(0U)							// bit used by job_send_event (between 0 and 15)
#define EVENT_FLAG_BITS				(1U<<(EVENT_FLAG_BIT))			// bit mask for job_send_event

#define NUM_CHARACTERS						1024
#define EDGE_ANIM_EXTERNAL_POSE_CACHE_MASK	-1						// all SPUs need external pose cache space in main memory
#define EDGE_ANIM_EXTERNAL_POSE_CACHE		(128*1024)				// arbitrarily allocate 128KB for each SPUs pose stack extension in main memory
#define VERTEX_BUFFER_SIZE					(16*1024*1024)			// double buffered vertex buffer size (for wireframe rendering)

#ifdef  _MSC_VER
#define MALLOC_ALIGNED(a, s)    _aligned_malloc(s, a)
#define FREE_ALIGNED(a)         _aligned_free(a)
#else
#define MALLOC_ALIGNED(a, s)    memalign(a, s)
#define FREE_ALIGNED(a)         free(a)
#endif

// instantiate the class
SampleApp app;

/* Creation of a (very simple test) blend tree:
 *
 *     leaf 0             leaf 1             leaf 2             leaf 3
 *  animation[0]       animation[1]       animation[2]       animation[3]
 *        \                /                    \                /
 *         +-->branch 2<--+                      +-->branch 1<--+
 *                    \                                /     
 *                     +---------->branch 0<----------+          
 *                                    |
 *                                 result
 * 
 * this is a very simple blend tree and in a real-life situation this code should be in the SPU job, but this 
 * example is showing simplest use from the PPU.
 */
void SampleApp::createTestBlendTree(EdgeAnimBlendBranch* blendBranches, uint32_t& numBlendBranches, EdgeAnimBlendLeaf* blendLeaves, uint32_t& numBlendLeaves, float* evalTimes, float blendIdle, float blendJog, float blendFinal)
{
    numBlendBranches = 0;
    numBlendLeaves = 0;

    // note: animations are not synchronised. it is not a realistic example, just a simple test case
    float evalTime0 = fmodf(evalTimes[0], mAnimation[0]->duration);
    float evalTime1 = fmodf(evalTimes[1], mAnimation[1]->duration);
    float evalTime2 = fmodf(evalTimes[2], mAnimation[2]->duration);
    float evalTime3 = fmodf(evalTimes[3], mAnimation[3]->duration);
    
    // tree leaves
    blendLeaves[numBlendLeaves++] = EdgeAnimBlendLeaf(mAnimation[0], mAnimation[0]->sizeHeader, evalTime0);
    blendLeaves[numBlendLeaves++] = EdgeAnimBlendLeaf(mAnimation[1], mAnimation[1]->sizeHeader, evalTime1);
    blendLeaves[numBlendLeaves++] = EdgeAnimBlendLeaf(mAnimation[2], mAnimation[2]->sizeHeader, evalTime2);
    blendLeaves[numBlendLeaves++] = EdgeAnimBlendLeaf(mAnimation[3], mAnimation[3]->sizeHeader, evalTime3);

    // tree branches
    blendBranches[numBlendBranches++] = EdgeAnimBlendBranch(EDGE_ANIM_BLENDOP_BLEND_LINEAR, 2 | EDGE_ANIM_BLEND_TREE_INDEX_BRANCH, 1 | EDGE_ANIM_BLEND_TREE_INDEX_BRANCH, blendFinal);
    blendBranches[numBlendBranches++] = EdgeAnimBlendBranch(EDGE_ANIM_BLENDOP_BLEND_LINEAR, 2 | EDGE_ANIM_BLEND_TREE_INDEX_LEAF,   3 | EDGE_ANIM_BLEND_TREE_INDEX_LEAF,   blendJog);
    blendBranches[numBlendBranches++] = EdgeAnimBlendBranch(EDGE_ANIM_BLENDOP_BLEND_LINEAR, 0 | EDGE_ANIM_BLEND_TREE_INDEX_LEAF,   1 | EDGE_ANIM_BLEND_TREE_INDEX_LEAF,   blendIdle);
}

#ifdef  __PPU__

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
void SampleApp::createAnimJob(CellSpursJob256* jobAnim, Transform3* outputMatrices, const struct EdgeAnimBlendBranch* blendBranches, uint32_t numBlendBranches, const struct EdgeAnimBlendLeaf* blendLeaves, uint32_t numBlendLeaves, const EdgeAnimJointTransform* rootJoint)
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
    jobAnim->header.sizeDmaList = (userDataCount - startInputDmaList) * sizeof(jobAnim->workArea.dmaList[0]);
    for(unsigned int listLoop = startInputDmaList; listLoop < userDataCount; listLoop++) {
        jobAnim->header.sizeInOrInOut += (jobAnim->workArea.dmaList[listLoop] >> 32);
    }
    
    // read-only cached buffer contains the global context (pose cache per spu etc) and the skeleton (since we assume it's shared by many characters)
    uint32_t startCacheDmaList = userDataCount;
    ret = cellSpursJobGetCacheList(&jobAnim->workArea.dmaList[userDataCount++], EDGE_ALIGN(sizeof(*mEdgeAnimPpuContext), 16), reinterpret_cast<uintptr_t>(mEdgeAnimPpuContext));
    assert(ret == CELL_OK);
    ret = cellSpursJobGetCacheList(&jobAnim->workArea.dmaList[userDataCount++], EDGE_ALIGN(mSkeleton->sizeTotal, 16), reinterpret_cast<uintptr_t>(mSkeleton));
    assert(ret == CELL_OK);
    jobAnim->header.sizeCacheDmaList = (userDataCount - startCacheDmaList) * sizeof(jobAnim->workArea.dmaList[0]);

    // output buffer contains world matrices (warning: for the internal buffer, numMatrices must aligned to a multiple of 4)
    unsigned int numJointsAligned = EDGE_ALIGN(mSkeleton->numJoints, 4U);
    jobAnim->header.sizeOut = EDGE_ALIGN(numJointsAligned * sizeof(Vectormath::Aos::Transform3), 16U);

    // scratch buffer
    jobAnim->header.sizeScratch = 128000U >> 4U;    // Scratch size (TODO: provide interfaces to query how much scratch is needed? not sure how to expose this) 

    // add root joint (must be aligned to 16 bytes / match code in cellSpursJobMain on the SPU side)
    assert(rootJoint);
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
 * - This job is required to be notified of the completion of the job list
 * - Similar to all SPURS job samples; pelase refer to them for more detail.
 * - Make sure you always end your command list this way :
 *     CELL_SPURS_JOB_COMMAND_SYNC              // important - enforces job ordering
 *     CELL_SPURS_JOB_COMMAND_JOB(&jobSendEvent)// the job itself
 *     CELL_SPURS_JOB_COMMAND_END               // end of command list
 */
void SampleApp::createSendEventJob(CellSpursJob256* jobSendEvent, uint16_t bitsEventFlag)
{
    // embedded SPU ELF symbols
    extern char _binary_job_job_send_event_spu_bin_start[]; 
    extern char _binary_job_job_send_event_spu_bin_size[];

    // job setup
    __builtin_memset(jobSendEvent, 0, sizeof(*jobSendEvent));
    jobSendEvent->header.eaBinary   = (uintptr_t)_binary_job_job_send_event_spu_bin_start;
    jobSendEvent->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_send_event_spu_bin_size);
    jobSendEvent->workArea.userData[0] = (uintptr_t)&mSpursEventFlag;
	jobSendEvent->workArea.userData[1] = (uint64_t)bitsEventFlag;
}


#endif

void SampleApp::runAnimJobs()
{
#ifdef __PPU__
    // jobs
    static CellSpursJob256 jobsAnim[NUM_CHARACTERS];
    static CellSpursJob256 jobEnd;

    // create send event job
    createSendEventJob(&jobEnd, EVENT_FLAG_BITS);
#endif

    // one blend tree per character
    uint32_t numBlendBranches[NUM_CHARACTERS];
    uint32_t numBlendLeaves[NUM_CHARACTERS];
    static EdgeAnimBlendBranch  blendTreeBranches[NUM_CHARACTERS][8]   EDGE_ALIGNED_PS3(16);
    static EdgeAnimBlendLeaf    blendTreeLeaves[NUM_CHARACTERS][8]     EDGE_ALIGNED_PS3(16);

    // create animation jobs - or evaluate directly on the PC
    for(unsigned int characterLoop = 0; characterLoop < NUM_CHARACTERS; characterLoop++) {  
        EdgeAnimJointTransform rootJoint;

        unsigned int d = (unsigned int) sqrtf((float) NUM_CHARACTERS);
        if ( d < 1 ) {
            d = 1;
        }
        float x = (float) (characterLoop % d) - 0.5f * (float) d;
        float z = (float) (characterLoop / d) - 0.5f * (float) d;
        float animJitter = (characterLoop % 31) / 31.0f;
        float blendJitter = (characterLoop % 17) / 17.0f;

        // create test blend tree 
        float evalTimes[4];
        evalTimes[0] = mEvalTime + animJitter * mAnimation[0]->duration;
        evalTimes[1] = mEvalTime + animJitter * mAnimation[1]->duration;
        evalTimes[2] = mEvalTime + animJitter * mAnimation[2]->duration;
        evalTimes[3] = mEvalTime + animJitter * mAnimation[3]->duration;
        float whichAnim = fmodf(mWhichAnim + blendJitter, 1.0f);
        createTestBlendTree(blendTreeBranches[characterLoop], numBlendBranches[characterLoop], 
            blendTreeLeaves[characterLoop], numBlendLeaves[characterLoop], 
            evalTimes,
            (characterLoop % d) / (float) d, 
            (characterLoop / d) / (float) d,
            (whichAnim<0.5f)? whichAnim * 2.0f: 2.0f - whichAnim * 2.0f);

        // root joint (character position / orientation)
        rootJoint.rotation = Vectormath::Aos::Quat(0, 0, 0, 1);
        rootJoint.translation = Vectormath::Aos::Point3(x*2.0f, 0, z*2.0f);
        rootJoint.scale = Vectormath::Aos::Vector4(1, 1, 1, 1);
#ifdef __PPU__
        // create job
        createAnimJob(&jobsAnim[characterLoop], mWorldMatrices[characterLoop], 
            blendTreeBranches[characterLoop], numBlendBranches[characterLoop], 
            blendTreeLeaves[characterLoop], numBlendLeaves[characterLoop], 
            &rootJoint);

		// Test the job descriptor for validity -- if this fails, the
		// job will crash when it's executed.
		int ret = cellSpursCheckJob(&jobsAnim[characterLoop], sizeof(CellSpursJob256),
			sizeof(CellSpursJob256));
		if ( ret != CELL_OK){
			printf("cellSpursCheckJob failed : 0x%x\n", ret);
		}
		
#else
        // direct evaluation (calls code that would be SPU code on PS3)
        processBlendTree(mWorldMatrices[characterLoop], mEdgeAnimPpuContext, 0, mSkeleton, 
            blendTreeBranches[characterLoop], numBlendBranches[characterLoop], 
            blendTreeLeaves[characterLoop], numBlendLeaves[characterLoop], 
            &rootJoint, mScratchBufferPC, EDGE_ANIM_EXTERNAL_POSE_CACHE);
#endif
    }

#ifdef __PPU__
    // create a job list of numJobs jobs (instead of adding each job individually)
    static CellSpursJobList jobList;
    jobList.eaJobList = (uint64_t)jobsAnim;
    jobList.numJobs = NUM_CHARACTERS;
    jobList.sizeOfJob = sizeof(jobsAnim[0]);

    // command list
    static uint64_t command_list_anim[4];
    unsigned int idxCmdList = 0;
    command_list_anim[idxCmdList++] = CELL_SPURS_JOB_COMMAND_JOBLIST(&jobList);
    command_list_anim[idxCmdList++] = CELL_SPURS_JOB_COMMAND_SYNC;
    command_list_anim[idxCmdList++] = CELL_SPURS_JOB_COMMAND_JOB(&jobEnd);
    command_list_anim[idxCmdList++] = CELL_SPURS_JOB_COMMAND_END;

    // init job chain
    static CellSpursJobChain jobChainAnim;
    static CellSpursJobChainAttribute jobChainAttributesAnim;

    const unsigned int MAX_CONTENTION = SPURS_SPU_NUM;          /* up to SPURS_SPU_NUM can work on this workload */
    const unsigned int MAX_GRAB = 8;                            /* 8 characters grabbed at a time */
    const uint8_t WORKLOAD_PRIORITIES[8] = {8,8,8,8,8,8,8,8};   /* all SPUs with average priorities */
    int ret = cellSpursJobChainAttributeInitialize(
        &jobChainAttributesAnim, command_list_anim, sizeof(jobsAnim[0]),
        MAX_GRAB, WORKLOAD_PRIORITIES, MAX_CONTENTION,
        true, 0, 1, false, sizeof(CellSpursJob256), MAX_CONTENTION);
    if (ret != CELL_OK){
        printf("cellSpursJobChainAttributeInitialize failed : 0x%x\n", ret);
    }
	//When the job manager detects an error, halt on SPU to help with debugging
	ret = cellSpursJobChainAttributeSetHaltOnError(&jobChainAttributesAnim);
	if (ret != CELL_OK){
		printf("cellSpursJobChainAttributeSetHaltOnError failed : 0x%x\n", ret);
	}
    ret = cellSpursCreateJobChainWithAttribute(mSpurs, &jobChainAnim, &jobChainAttributesAnim);
    if (ret != CELL_OK){
        printf("cellSpursCreateJobChainWithAttribute failed : 0x%x\n", ret);
    }
    
    // run the job chain
    ret = cellSpursRunJobChain(&jobChainAnim);
    if (ret != CELL_OK){
        printf("cellSpursRunJobChain failed : 0x%x\n", ret);
    }

    // wait for completion (SendEvent job) & shutdown
    uint16_t ev_mask = EVENT_FLAG_BITS;
    cellSpursEventFlagWait(&mSpursEventFlag,&ev_mask,CELL_SPURS_EVENT_FLAG_AND);
    ret = cellSpursShutdownJobChain(&jobChainAnim);
    if (ret != CELL_OK){
        printf("cellSpursShutdownJobChain failed : 0x%x\n", ret);
    }    
    ret = cellSpursJoinJobChain(&jobChainAnim);
    if (ret != CELL_OK){
        printf("cellSpursJoinJobChain failed : 0x%x\n", ret);
    }
#endif
}

static void* loadFile(const char* fileName)
{
    FILE* fp = fopen(fileName, "rb");
    if (!fp) {
        printf("cannot open file %s\n", fileName);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    unsigned int l = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    void* data = MALLOC_ALIGNED(128, l);
    if (!data) {
        printf("can't allocate memory\n");
        return NULL;
    }
    fread(data, 1, l, fp);
    fclose(fp);

    return data;
}

SampleApp::SampleApp()
{
    mDispInfo.mAntiAlias = true; 
    mTurnRate = 800.0f; 
    mMoveRate = 200.0f;
    mStartupInfo.mNumRawSPUs = 1; // required by PSGL? 
}

bool SampleApp::onInit(int argc, char **ppArgv)
{
    FWGLCamControlApplication::onInit(argc, ppArgv);	// note: calls sys_spu_initialize() internally

#ifdef __PPU__
    int ret;
    ret = cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
    if (ret != CELL_OK) {
        printf("cellSysmoduleLoadModule() error 0x%x !\n", ret);
        sys_process_exit(1);
    }

	// Get PPU thread priority
	int ppu_thr_prio;
	sys_ppu_thread_t my_ppu_thread_id;
	ret = sys_ppu_thread_get_id(&my_ppu_thread_id);
    if (ret != CELL_OK) {
        printf("sys_ppu_thread_get_id failed : 0x%x\n", ret);
        return false;
    }
	ret = sys_ppu_thread_get_priority(my_ppu_thread_id, &ppu_thr_prio);
    if (ret != CELL_OK) {
        printf("sys_ppu_thread_get_priority failed : 0x%x\n", ret);
        return false;
    }

	// Initialize spu_printf handling
    ret = spu_printf_initialize(ppu_thr_prio - 1, NULL);
    if (ret != CELL_OK) {
        printf("spu_printf_initialize failed : 0x%x\n", ret);
        return false;
	}

    mSpurs = (CellSpurs*)MALLOC_ALIGNED(CELL_SPURS_ALIGN, sizeof(CellSpurs));

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
	ret = cellSpursInitializeWithAttribute(mSpurs, &spursAttributes);
	if (ret != CELL_OK){
		printf("cellSpursInitializeWithAttribute failed : 0x%x\n", ret);
		return false;
	}

    // Initialize Spurs event flag (used for job chain completion)
    ret = cellSpursEventFlagInitializeIWL(mSpurs, &mSpursEventFlag,
                                    CELL_SPURS_EVENT_FLAG_CLEAR_AUTO,
                                    CELL_SPURS_EVENT_FLAG_SPU2PPU);
    if (ret != CELL_OK) {
        printf("cellSpursEventFlagInitializeIWL failed : 0x%x\n", ret);
        return false;
    }
    ret = cellSpursEventFlagAttachLv2EventQueue(&mSpursEventFlag);
    if (ret != CELL_OK) {
        printf("cellSpursEventFlagAttachLv2EventQueue failed : 0x%x\n", ret);
        return false;
    }
#endif

    // set default camera position
    mCamera.setPosition(Point3(0, 0, 5));

    // allocate vertex buffer
    mVertexBufferBase = MALLOC_ALIGNED(1024*1024, VERTEX_BUFFER_SIZE*2);
    mVertexBufferCur = mVertexBufferBase;

    // intialise edge animation 
    mEdgeAnimPpuContext = (EdgeAnimPpuContext*) MALLOC_ALIGNED(16, sizeof(*mEdgeAnimPpuContext));
    mExternalSpuStorage = MALLOC_ALIGNED(16, edgeAnimComputeExternalStorageSize( SPURS_SPU_NUM, EDGE_ANIM_EXTERNAL_POSE_CACHE_MASK, EDGE_ANIM_EXTERNAL_POSE_CACHE));
    edgeAnimPpuInitialize(mEdgeAnimPpuContext, SPURS_SPU_NUM, EDGE_ANIM_EXTERNAL_POSE_CACHE_MASK, EDGE_ANIM_EXTERNAL_POSE_CACHE, mExternalSpuStorage);

    // load animation data
#ifdef __PPU__    
#   define FILE_PREFIX SYS_APP_HOME"/assets/"
#   define FILE_SUFFIX
#else
#   define FILE_PREFIX "assets/"
#   define FILE_SUFFIX "pc"
#endif    
    mSkeleton = (EdgeAnimSkeleton*) loadFile(FILE_PREFIX"I_STA_M_13.skel"FILE_SUFFIX);
    mAnimation[0] = (EdgeAnimAnimation*) loadFile(FILE_PREFIX"I_STA_M_14.anim"FILE_SUFFIX);
    mAnimation[1] = (EdgeAnimAnimation*) loadFile(FILE_PREFIX"I_STA_M_15.anim"FILE_SUFFIX);
    mAnimation[2] = (EdgeAnimAnimation*) loadFile(FILE_PREFIX"M_STA_M_03_P2.anim"FILE_SUFFIX);
    mAnimation[3] = (EdgeAnimAnimation*) loadFile(FILE_PREFIX"M_STA_M_04_P2.anim"FILE_SUFFIX);

    // allocate world matrix array
    mWorldMatrices = (Transform3**)malloc(sizeof(Transform3*) * NUM_CHARACTERS);
    int numJointsAligned = (mSkeleton->numJoints + 3) & (~3);
    for (unsigned int characterLoop = 0; characterLoop < NUM_CHARACTERS; characterLoop++) {
        mWorldMatrices[characterLoop] = (Transform3*)MALLOC_ALIGNED(16, sizeof(Transform3) * numJointsAligned);  
    }

#ifndef  __PPU__
    // animation scratch buffer when running on PC
    mScratchBufferPC = (uint8_t*)MALLOC_ALIGNED(16, EDGE_ANIM_EXTERNAL_POSE_CACHE);
#endif

    mEvalTime = 0.0f;
    mWhichAnim = 0.0f;
    mFrameCount = 0;

    return true;
}

void SampleApp::drawSkeleton(const Transform3* pWorldMats, const short* jointParents, int count)
{
    Vector3* v = (Vector3*)mVertexBufferCur;

    int numLines = 0;

    for(int i=0; i<count; i++) {
        int parent = jointParents[i];
        if (parent != -1) {
            *v++ = pWorldMats[i].getTranslation();
            *v++ = pWorldMats[parent].getTranslation();
            numLines++;
        }
    }

    glColor4f(1,0,0,1);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 16, mVertexBufferCur);
    glDrawArrays(GL_LINES, 0, numLines*2);
    glDisableClientState(GL_VERTEX_ARRAY);

    mVertexBufferCur = v;

    mFrameCount = 0;
}

void SampleApp::onRender()
{
    FWGLCamControlApplication::onRender();

    // run animation jobs
    runAnimJobs();

    mVertexBufferCur = (void*)((size_t)mVertexBufferBase + VERTEX_BUFFER_SIZE * (mFrameCount&1));

    // init render state
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    // draw skeleton
    const int16_t* parentIndices = EDGE_OFFSET_GET_POINTER(int16_t, mSkeleton->offsetParentIndicesArray);
    for(unsigned int characterLoop = 0; characterLoop < NUM_CHARACTERS; characterLoop++) {
        drawSkeleton(mWorldMatrices[characterLoop], parentIndices, mSkeleton->numJoints);
    }

	// calc fps
	FWTimeVal time = FWTime::getCurrentTime();
	float fFPS = 1.f / (float)(time - mLastTime);
	mLastTime = time;

	// print text
	FWDebugFont::setPosition(0, 0);

	FWDebugFont::setColor(1.f, 1.f, 1.f, 1.0f);
	FWDebugFont::printf("  Edge Animation Sample Application\n\n\n");

	FWDebugFont::setColor(1.f, 1.f, 0.f, 1.0f);
	FWDebugFont::printf("  FPS  %3.4f\n", fFPS);
	FWDebugFont::printf("  Eval %3.4f\n", mEvalTime);

	mFrameCount++;
}

bool SampleApp::onUpdate()
{
    // advance anim eval time
    const float deltaTime = 1.0f/60.0f;
    mEvalTime += deltaTime;

    // advance blend factor
    mWhichAnim += 0.005f;
    if (mWhichAnim >= 1.0f) 
        mWhichAnim = 0.0f;

    return FWGLCamControlApplication::onUpdate();
}

void SampleApp::onSize(const FWDisplayInfo& rDispInfo)
{
    FWGLCamControlApplication::onSize(rDispInfo);
}

void SampleApp::onShutdown()
{
#ifdef __PPU__
	int ret;
	ret = cellSpursEventFlagDetachLv2EventQueue(&mSpursEventFlag);
	if (ret != CELL_OK) {
		printf("cellSpursEventFlagDetachLv2EventQueue failed : 0x%x\n", ret);
	}
    ret = cellSpursFinalize(mSpurs);
    if (ret != CELL_OK) {
        printf("cellSpursFinalize failed : 0x%x\n", ret);
    }
    ret = spu_printf_finalize();
    if (ret != CELL_OK) {
		printf("spu_printf_finalize failed : 0x%x\n", ret);
    }
#endif

    edgeAnimPpuFinalize(mEdgeAnimPpuContext);
    FREE_ALIGNED(mExternalSpuStorage);
    FREE_ALIGNED(mEdgeAnimPpuContext);

    for(unsigned int i=0; i<sizeof(mAnimation)/sizeof(mAnimation[0]); i++){
        FREE_ALIGNED(mAnimation[i]);
    }
    FREE_ALIGNED(mSkeleton);
#ifdef __PPU__
    FREE_ALIGNED(mSpurs);
#else
    FREE_ALIGNED(mScratchBufferPC);
#endif
    FREE_ALIGNED(mVertexBufferBase);

    for(unsigned int characterLoop = 0; characterLoop < NUM_CHARACTERS; characterLoop++) {
        FREE_ALIGNED(mWorldMatrices[characterLoop]);
    }

    free(mWorldMatrices);

    FWGLCamControlApplication::onShutdown();
}

