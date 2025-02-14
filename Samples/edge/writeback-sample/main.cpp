/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <cell/gcm.h>
#include <sys/spu_initialize.h>
#include <sys/spu_thread.h>
#include <sys/ppu_thread.h>
#include <sys/event.h>
#include <sys/timer.h>

#define PI								3.141592653
#define SPURS_SPU_NUM					1
#define SPU_THREAD_GROUP_PRIORITY		250
#define EVENT_FLAG_BIT					(0U)					// bit used by job_send_event (between 0 and 15)
#define EVENT_FLAG_BITS					(1U<<(EVENT_FLAG_BIT))	// bit mask for job_send_event

#include <cell/spurs.h>
#include <spu_printf.h>

#include "main.h"
#include "gcm/FWCellGCMWindow.h"
#include "FWDebugFont.h"
#include "FWTime.h"
#include "cellutil.h"
#include "gcmutil.h"
#include "../common/profile_bars.h"

#include "edge/geom/edgegeom_structs.h"

using namespace cell;
using namespace cell::Gcm;

static bool simulate = true;

struct Particle
{
	float position[3];
	float time;
	float velocity[3];
    float rMass;
};

static const uint32_t numParticles = 32 * 1024;
static Particle particles[numParticles] __attribute__((__aligned__(16)));

//static inline void SetVec(float *vec, float x, float y) { vec[0] = x; vec[1] = y; }
//static inline void SetVec(float *vec, float x, float y, float z) { vec[0] = x; vec[1] = y; vec[2] = z; }
//static inline void SetVec(float *vec, float x, float y, float z, float w) { vec[0] = x; vec[1] = y; vec[2] = z; vec[3] = w; }
//static inline void SetVec2(float *vec, float *from) { vec[0] = from[0]; vec[1] = from[1]; }
//static inline void SetVec3(float *vec, float *from) { vec[0] = from[0]; vec[1] = from[1]; vec[2] = from[2]; }
//static inline void AddVec3(float *vec, float *from) { vec[0] += from[0]; vec[1] += from[1]; vec[2] += from[2]; }

// instantiate the class
SampleApp app;

// shader
extern unsigned long _binary_vpshader_vpo_start;
extern unsigned long _binary_vpshader_vpo_end;
extern unsigned long _binary_fpshader_fpo_start;
extern unsigned long _binary_fpshader_fpo_end;

// embedded SPU ELF symbols
extern char _binary_job_job_geom_spu_bin_start[];
extern char _binary_job_job_geom_spu_bin_size[];
extern char _binary_job_job_send_event_spu_bin_start[];	
extern char _binary_job_job_send_event_spu_bin_size[];

#define MAX_SEGMENTS 0x100
#define AVAILABLE_SPU_SPACE 64*1024 //(235*1024 - 96*1024 - 384 - 112*3 - 64*3)
static CellSpursJob256 jobs[MAX_SEGMENTS];
static CellSpursJob256 simJobs[MAX_SEGMENTS];
static CellSpursEventFlag spursEventFlag;
static uint32_t numSegments = 0;
static void *decompressedVertexData = NULL;
static uint32_t outputAddr = 0;

static uint32_t CreateSimulationJobs(uint32_t count,
									 uint32_t inputVertexFormatId, uint32_t outputVertexFormatId, 
									 uint32_t numAttributes, 
									 uint32_t inputStride, uint32_t outputStride)
{
	count = (count + 7) & ~0x7;
	uint32_t maxStride = (inputStride > outputStride) ? inputStride : outputStride;

	uint32_t numParticlesPerSegment = AVAILABLE_SPU_SPACE / (maxStride + 16*numAttributes);
	numParticlesPerSegment = (numParticlesPerSegment > (0xC000 / maxStride)) ? (0xC000 / maxStride) : numParticlesPerSegment;
	numParticlesPerSegment &= ~0xF;
	uint32_t particlesEaA = (uint32_t)particles;

	outputAddr = (uint32_t)particles;
	uint32_t startingAddr = outputAddr;
    
	while (count > 0) {
		uint32_t numParts = (count > numParticlesPerSegment) ? numParticlesPerSegment : count;
		uint32_t particlesSize = (numParts*inputStride + 15) & ~0xF;
		uint32_t particlesSizeA = (particlesSize >= 0x4000) ? 0x4000 : particlesSize;
		uint32_t particlesSizeB = ((particlesSize-particlesSizeA) >= 0x4000) ? 0x4000 : (particlesSize-particlesSizeA);
		uint32_t particlesSizeC = (particlesSize >= 0x8000) ? (particlesSize-0x8000) : 0;
		uint32_t particlesEaB = particlesEaA + particlesSizeA;
		uint32_t particlesEaC = particlesEaB + particlesSizeB;

		CellSpursJob256 *job = &simJobs[numSegments];
		job->workArea.dmaList[0] = particlesEaA | ((uint64_t)particlesSizeA << 32);
		job->workArea.dmaList[1] = particlesEaB | ((uint64_t)particlesSizeB << 32);
		job->workArea.dmaList[2] = particlesEaC | ((uint64_t)particlesSizeC << 32);
		job->workArea.userData[3] = outputAddr | EDGE_GEOM_DIRECT_OUTPUT_TO_MAIN_MEMORY; // high bit specifies that it is to output to a specific address
		job->workArea.userData[4] = inputVertexFormatId;
		job->workArea.userData[5] = outputVertexFormatId;
		job->workArea.userData[6] = numParts;
		job->workArea.userData[7] = numAttributes - 1;
		job->workArea.userData[8] = 0xFFFFFFFF;
		*(float *)(void*)&(job->workArea.userData[9]) = simulate ? (1.f/60.f) : 0.f;
		job->header.eaBinary = (uintptr_t)_binary_job_job_geom_spu_bin_start;
		job->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_geom_spu_bin_size);
		job->header.sizeDmaList = 3 * 8;
		job->header.eaHighInput = 0;
		job->header.useInOutBuffer = 1;
		job->header.sizeInOrInOut = (numParts * maxStride + 16 + 127) & ~0x7F;
		job->header.sizeStack = 0;
		job->header.sizeScratch = (numParts * numAttributes + 7) & ~0x7;
		job->header.sizeCacheDmaList = 0;
		count -= numParts;
		particlesEaA += numParts * inputStride;
		outputAddr += numParts * outputStride;
		++numSegments;

        // enable memory checking in jobs.  SPURS will place a memory check marker in 16 byte buffers added after IO buffer, read only data, and stack, and check the integrity 
        // of the marker at the end of the job.  Enabling this flag will add 3-4% to SPURS overhead of running jobs.  Linking with -mspurs-job instead of -mspurs-job-initialize
        // will cause this flag to be ignored.
        job->header.jobType = CELL_SPURS_JOB_TYPE_MEMORY_CHECK;

		int ret = cellSpursCheckJob(job, sizeof(CellSpursJob256),
			sizeof(CellSpursJob256));
		if ( ret != CELL_OK){
			printf("cellSpursCheckJob failed : 0x%x\n", ret);
		}
	}
	outputAddr = (outputAddr + 0x7F) & ~0x7F;
	return startingAddr;
}

static uint32_t CreateJobs(uint32_t vertexCount, 
	uint32_t inputVertexFormatId, uint32_t outputVertexFormatId, uint32_t numAttributes, 
	uint32_t inputStride, uint32_t outputStride, CellGcmContextData *ctx)
{
	// Find number of vertexes per segment
	vertexCount = (vertexCount + 7) & ~0x7;
	uint32_t maxStride = (inputStride > outputStride) ? inputStride : outputStride;
	uint32_t numVertsPerSegment = AVAILABLE_SPU_SPACE / (maxStride + 16*numAttributes);
	numVertsPerSegment = (numVertsPerSegment > (0xC000 / maxStride)) ? (0xC000 / maxStride) : numVertsPerSegment;
	numVertsPerSegment &= ~0xF;

	outputAddr = (uint32_t)decompressedVertexData;	
	uint32_t startingAddr = outputAddr;
	uint32_t vertexesEaA = (uint32_t)particles;
	while(vertexCount > 0)
	{
		uint32_t numVerts = (vertexCount > numVertsPerSegment) ? numVertsPerSegment : vertexCount;
		uint32_t vertexesSize = (numVerts*inputStride + 15) & ~0xF;
		uint32_t vertexesSizeA = (vertexesSize >= 0x4000) ? 0x4000 : vertexesSize;
        uint32_t vertexesSizeB = ((vertexesSize-vertexesSizeA) >= 0x4000) ? 0x4000 : (vertexesSize-vertexesSizeA);
		uint32_t vertexesSizeC = (vertexesSize >= 0x8000) ? (vertexesSize-0x8000) : 0;
		uint32_t vertexesEaB = vertexesEaA + vertexesSizeA;
		uint32_t vertexesEaC = vertexesEaB + vertexesSizeB;

		// Jump to Self Sync
		if(ctx->current >= ctx->end)
		{
			if((*ctx->callback)(ctx, 1) != CELL_OK)
				return 0;
		}

		uint64_t inputStreamCount = 1;
		uint32_t holeEa = (uint32_t)ctx->current;
		uint32_t jumpOffset;
		cellGcmAddressToOffset(ctx->current, &jumpOffset);
		cellGcmSetJumpCommandUnsafeInline(ctx, jumpOffset);

		CellSpursJob256 *job = &jobs[numSegments];
		job->workArea.dmaList[0] = vertexesEaA | ((uint64_t)vertexesSizeA << 32);
        job->workArea.dmaList[1] = vertexesEaB | ((uint64_t)vertexesSizeB << 32);
		job->workArea.dmaList[2] = vertexesEaC | ((uint64_t)vertexesSizeC << 32);
		job->workArea.userData[3] = outputAddr | EDGE_GEOM_DIRECT_OUTPUT_TO_MAIN_MEMORY; // high bit specifies that it is to output to a specific address
        job->workArea.userData[4] = inputVertexFormatId;
        job->workArea.userData[5] = outputVertexFormatId;
        job->workArea.userData[6] = numVerts;
        job->workArea.userData[7] = numAttributes - 1;
		job->workArea.userData[8] = (uint64_t)holeEa | (inputStreamCount << 32); // store input stream count in high word of holeEa
		job->header.eaBinary = (uintptr_t)_binary_job_job_geom_spu_bin_start;
		job->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_geom_spu_bin_size);
		job->header.sizeDmaList = 3 * 8;
		job->header.eaHighInput = 0;
		job->header.useInOutBuffer = 1;
		job->header.sizeInOrInOut = (numVerts * maxStride + 16 + 127) & ~0x7F;
		job->header.sizeStack = 0;
		job->header.sizeScratch = (numVerts * numAttributes + 7) & ~0x7;
		job->header.sizeCacheDmaList = 0;
		vertexCount -= numVerts;
		vertexesEaA += numVerts * inputStride;
		outputAddr += numVerts * outputStride;
		++numSegments;

        // enable memory checking in jobs.  SPURS will place a memory check marker in 16 byte buffers added after IO buffer, read only data, and stack, and check the integrity 
        // of the marker at the end of the job.  Enabling this flag will add 3-4% to SPURS overhead of running jobs.  Linking with -mspurs-job instead of -mspurs-job-initialize
        // will cause this flag to be ignored.
        job->header.jobType = CELL_SPURS_JOB_TYPE_MEMORY_CHECK;

		int ret = cellSpursCheckJob(job, sizeof(CellSpursJob256),
			sizeof(CellSpursJob256));
		if ( ret != CELL_OK){
			printf("cellSpursCheckJob failed : 0x%x\n", ret);
		}
	}
	outputAddr = (outputAddr + 0x7F) & ~0x7F;
	
	return startingAddr;
}

static void ResetParticle(Particle *p)
{
	p->position[0] = -5.f + 10.f * rand() / float( RAND_MAX ); //random position inside a box
	p->position[1] = 5.f + rand() / float( RAND_MAX );
	p->position[2] = -1.f + 2.f * rand() / float( RAND_MAX );

	p->velocity[0] = p->position[0] * .5f * rand()/ float(RAND_MAX);

	float tmp = rand() / float( RAND_MAX );
	if (tmp < .3f) {
		p->velocity[1] = -10.f * rand() / float( RAND_MAX);//0.0f;
	}
	else{
		p->velocity[1] = 0.f;
	}

	p->velocity[2] = 5.f + 3.f * rand() / float( RAND_MAX );

	p->rMass = 1.0f; //mass doesn't matter if we're just doing gravity

	p->time = 5.f * rand() / float( RAND_MAX ); //random life length
}

static void InitializeParticles(uint32_t num, Particle *p)
{
	for (uint32_t i = 0; i < num; i++) {
		ResetParticle(p + i);
	}
}

SampleApp::SampleApp() 
:	mLastUpdate(FWTime::sZero)
,	mTurnRate(500.f)
,	mMoveRate(50.f)
{
	mFar = 10000.f;

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

	mCamera.setPosition(Point3(0.f, -6.f, 86.f));

	FWInputDevice *pPad = FWInput::getDevice(FWInput::DeviceType_Pad, 0);
	if(pPad != NULL) 
	{
		for(unsigned i = 0; i < IN_COUNT; ++i)
		{
			mInputs[i] = pPad->bindFilter();
			mInputs[i]->setChannel(sInputChannels[i]);
		}
		mInputs[IN_X0]->setGain(0.5f);
		mInputs[IN_X0]->setDeadzone(0.08f);
		mInputs[IN_Y0]->setGain(-0.5f);
		mInputs[IN_Y0]->setDeadzone(0.08f);
		mInputs[IN_X1]->setGain(0.002f);
		mInputs[IN_X1]->setDeadzone(0.0008f);
		mInputs[IN_Y1]->setGain(0.002f);
		mInputs[IN_Y1]->setDeadzone(0.0008f);
	}

	// Get PPU thread priority
	int ppu_thr_prio;
	sys_ppu_thread_t my_ppu_thread_id;
	int ret = sys_ppu_thread_get_id(&my_ppu_thread_id);
	if (ret != CELL_OK){
		printf("sys_ppu_thread_get_id failed : 0x%x\n", ret);
	}
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
	ret = cellSpursEventFlagInitializeIWL(&mSpurs, &spursEventFlag,
		CELL_SPURS_EVENT_FLAG_CLEAR_AUTO,
		CELL_SPURS_EVENT_FLAG_SPU2PPU);
    if (ret != CELL_OK){
        printf("cellSpursEventFlagInitializeIWL failed : 0x%x\n", ret);
		return false;
    }
	ret = cellSpursEventFlagAttachLv2EventQueue(&spursEventFlag);
    if (ret != CELL_OK){
        printf("cellSpursEventFlagAttachLv2EventQueue failed : 0x%x\n", ret);
		return false;
    }

	decompressedVertexData = memalign(1024*1024, 1024*1024);
	
	InitializeParticles(numParticles, particles);

	uint32_t dummy;
	cellGcmMapMainMemory(decompressedVertexData, 1024*1024, &dummy);

	initShader();

	// init profile bars
	ProfileBars::Init(&mSpurs, SPURS_SPU_NUM);

	return true;
}

bool SampleApp::onUpdate()
{
	FWTimeVal time = FWTime::getCurrentTime();
	float deltaTime = (mLastUpdate == FWTime::sZero) ? 0.f : (float)(time - mLastUpdate);
	mLastUpdate = time;

	mCamera.setPan(mCamera.getPan() + (mInputs[IN_X1] ? mInputs[IN_X1]->getFloatValue() : 0.f) * deltaTime * mTurnRate);
	mCamera.setTilt(mCamera.getTilt() + (mInputs[IN_Y1] ? mInputs[IN_Y1]->getFloatValue() : 0.f) * deltaTime * mTurnRate);

	Matrix3 xRot = Matrix3::rotationX(-mCamera.getTilt());
	Matrix3 yRot = Matrix3::rotationY(-mCamera.getPan());
	Vector3 xVec = (yRot * xRot) * Vector3::xAxis();
	Vector3 yVec = (yRot * xRot) * Vector3::yAxis();
	Vector3 zVec = (yRot * xRot) * -Vector3::zAxis();

	mCamera.setPosition(mCamera.getPosition() + (
		xVec * (mInputs[IN_X0] ? mInputs[IN_X0]->getFloatValue() : 0.f) 
		+ yVec * ((mInputs[IN_UP] && mInputs[IN_UP]->getBoolValue()) ? 1.0f : 0.f)
		+ yVec * ((mInputs[IN_DOWN] && mInputs[IN_DOWN]->getBoolValue()) ? -1.0f : 0.f)
		+ zVec * (mInputs[IN_Y0] ? mInputs[IN_Y0]->getFloatValue() : 0.f)
		) * deltaTime * mMoveRate);

	if (mInputs[IN_SQUARE] && mInputs[IN_SQUARE]->getBoolValue())
		simulate = !simulate;

	//printf("particles[0]: {%f, %f, %f}\n", particles[0].position[0], particles[0].position[1], particles[0].position[2]);
	uint32_t numReset = 0;
	for (uint32_t i = 0; i < numParticles; i++) {
		if (simulate) {
			particles[i].time = particles[i].time - (1.f/60.f);
		}

		if (particles[i].time <= 0.f) {
			ResetParticle(&particles[i]);

			numReset++;
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

	static CellSpursJob256 jobEnd;
	__builtin_memset(&jobEnd,0,sizeof(CellSpursJob256));
	jobEnd.header.eaBinary = (uintptr_t)_binary_job_job_send_event_spu_bin_start;
	jobEnd.header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_send_event_spu_bin_size);
	jobEnd.workArea.userData[0] = (uintptr_t)&spursEventFlag;
	jobEnd.workArea.userData[1] = (uint64_t)EVENT_FLAG_BITS;

	// Set off simulation jobs
	{
		numSegments = 0;
		/*uint32_t outputEa = */
		CreateSimulationJobs(numParticles, 
							 0xFF, 0xFF,
							 2,
							 32, 32);
		
		static CellSpursJobList jobList;
		jobList.eaJobList = (uint64_t)simJobs;
		jobList.numJobs = numSegments;
		jobList.sizeOfJob = 256;
	
		static uint64_t command_list[5];
		command_list[0] = CELL_SPURS_JOB_COMMAND_JOBLIST(&jobList);
		command_list[1] = CELL_SPURS_JOB_COMMAND_SYNC;
		command_list[2] = CELL_SPURS_JOB_COMMAND_FLUSH;
		command_list[3] = CELL_SPURS_JOB_COMMAND_JOB(&jobEnd);
		command_list[4] = CELL_SPURS_JOB_COMMAND_END;
	
		static CellSpursJobChain jobChain __attribute__((aligned(128)));
		static CellSpursJobChainAttribute jobChainAttributes;        
		static uint8_t prios[8] = {1, 1, 1, 1, 1, 1, 1, 1};

		int ret;

		ret = cellSpursJobChainAttributeInitialize(&jobChainAttributes, command_list, 
			sizeof(CellSpursJob256), (SPURS_SPU_NUM == 1) ? 1 : 16, prios, 
			SPURS_SPU_NUM, true, 0, 1, false, sizeof(CellSpursJob256), 6);
		if(ret != CELL_OK){
			printf("cellSpursJobChainAttributeInitialize failed : 0x%x\n", ret);
		}

		//When the job manager detects an error, halt on SPU to help with debugging
		ret = cellSpursJobChainAttributeSetHaltOnError(&jobChainAttributes);
		if (ret != CELL_OK){
			printf("cellSpursJobChainAttributeSetHaltOnError failed : 0x%x\n", ret);
		}

		ret = cellSpursCreateJobChainWithAttribute(&mSpurs, &jobChain, &jobChainAttributes);
		if(ret != CELL_OK){
			printf("cellSpursCreateJobChainWithAttribute failed : 0x%x\n", ret);
		}

		ret = cellSpursRunJobChain(&jobChain);
		if(ret != CELL_OK){
			printf("cellSpursRunJobChain failed : 0x%x\n", ret);
		}

		uint16_t ev_mask = EVENT_FLAG_BITS;
		ret = cellSpursEventFlagWait(&spursEventFlag,&ev_mask,CELL_SPURS_EVENT_FLAG_AND);
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

	// Reset Parameters
	numSegments = 0;

	__builtin_memset(decompressedVertexData, 0xCC, 1024*1024);

	cellGcmSetInvalidateVertexCache();
	
	cellGcmSetVertexProgram(mCGVertexProgram, mVertexProgramUCode);
	cellGcmSetFragmentProgram(mCGFragmentProgram, mFragmentProgramOffset);

	cellGcmSetBlendEnable(CELL_GCM_TRUE);
	cellGcmSetDepthTestEnable(CELL_GCM_FALSE);
	cellGcmSetDepthMask(CELL_GCM_FALSE);
	cellGcmSetDepthFunc(CELL_GCM_LESS);
	cellGcmSetShadeMode(CELL_GCM_SMOOTH);

	cellGcmSetBlendFunc(CELL_GCM_SRC_ALPHA, CELL_GCM_ONE, CELL_GCM_ONE, CELL_GCM_ONE);

	Matrix4 mat = Matrix4::rotationZYX(Vector3(0, 0, 0));
	mat.setTranslation(Vector3(0.0f, 0.0f, -5.0f));
	Matrix4 mvp = transpose(getProjectionMatrix() * getViewMatrix() * mat);
	float lp[4] = {10,10,10,1};
	Point3 ep = mCamera.getPosition();
	cellGcmSetVertexProgramConstants(0, 16, (float*)&mvp);
	cellGcmSetVertexProgramConstants(4, 4, lp);
	cellGcmSetVertexProgramConstants(5, 4, (float*)&ep);

	// Create a job list to decompress the vertexes	
	uint32_t outputEa = CreateJobs(numParticles, 
								   0xFF, 
								   EDGE_GEOM_RSX_VERTEX_FORMAT_F32c3, 
								   2, sizeof(Particle), 12, gCellGcmCurrentContext);

	static CellSpursJobList jobList;
	jobList.eaJobList = (uint64_t)jobs;
	jobList.numJobs = numSegments;
	jobList.sizeOfJob = 256;

	static uint64_t command_list[5];
	command_list[0] = CELL_SPURS_JOB_COMMAND_JOBLIST(&jobList);
	command_list[1] = CELL_SPURS_JOB_COMMAND_SYNC;
	command_list[2] = CELL_SPURS_JOB_COMMAND_FLUSH;
	command_list[3] = CELL_SPURS_JOB_COMMAND_JOB(&jobEnd);
	command_list[4] = CELL_SPURS_JOB_COMMAND_END;

	static CellSpursJobChain jobChain __attribute__((aligned(128)));
	static CellSpursJobChainAttribute jobChainAttributes;        
	static uint8_t prios[8] = {1, 1, 1, 1, 1, 1, 1, 1};

	int ret;

	ret = cellSpursJobChainAttributeInitialize(&jobChainAttributes, command_list, 
		sizeof(CellSpursJob256), (SPURS_SPU_NUM == 1) ? 1 : 16, prios, 
		SPURS_SPU_NUM, true, 0, 1, false, sizeof(CellSpursJob256), 6);

	if (ret != CELL_OK){
		printf("cellSpursJobChainAttributeInitialize failed : 0x%x\n", ret);
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

	uint32_t vertexesOffset;
	cellGcmAddressToOffset((void*)outputEa, &vertexesOffset);

	ProfileBars::AddRsxMarker(0x20202000);	// grey rsx bar until that point (clear etc)

	cellGcmSetPointSize(15.f);
	cellGcmSetPointSpriteControl(CELL_GCM_TRUE, CELL_GCM_POINT_SPRITE_RMODE_ZERO, CELL_GCM_POINT_SPRITE_TEX0);
	cellGcmSetVertexDataArray(0, 0, 12/*stride*/, 3, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_MAIN, vertexesOffset + 0);
	cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_POINTS, 0, numParticles);

	ProfileBars::AddRsxMarker(0xff000000);	// red rsx bar indicates draw time

	// kick rsx
    cellGcmFlush();

    uint16_t ev_mask = EVENT_FLAG_BITS;
	ret = cellSpursEventFlagWait(&spursEventFlag,&ev_mask,CELL_SPURS_EVENT_FLAG_AND);
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

		FWDebugFont::print("Edge Writeback Sample Application\n\n");
		FWDebugFont::printf("FPS: %.2f\n", fFPS);
	}
}

void SampleApp::onSize(const FWDisplayInfo& rDispInfo)
{
	FWGCMCamApplication::onSize(rDispInfo);
}

void SampleApp::onShutdown()
{
	free( decompressedVertexData );

	int ret;
	ret = cellSpursEventFlagDetachLv2EventQueue(&spursEventFlag);
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

	FWGCMCamApplication::onShutdown();

	// unbind input filters
	FWInputDevice	*pPad = FWInput::getDevice(FWInput::DeviceType_Pad, 0);
	if(pPad != NULL) 
		for(unsigned i = 0; i < IN_COUNT; ++i)
			pPad->unbindFilter(mInputs[i]);
}
