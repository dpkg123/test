/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
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
#include <cell/dbgfont.h>

#include "gcm/FWCellGCMWindow.h"
#include "FWDebugFont.h"
#include "FWTime.h"
#include "gcmutil.h"
#include "gtfloader.h"
#include "../common/profile_bars.h"
#include "../common/vsync.h"
#include "edge/geom/edgegeom_structs.h"
#include "spu/job_geom_interface.h"
#include "edge/post/edgepost.h"
#include "edge/post/edgepost_ppu.h"
#include "main.h"
#include "assets/elephant.edge.h"



/////
#define SPURS_SPU_NUM					5
#define SPU_THREAD_GROUP_PRIORITY		250
#define EVENT_FLAG_BIT					(0U)					// bit used by job_send_event (between 0 and 15)
#define EVENT_FLAG_BITS					(1U<<(EVENT_FLAG_BIT))	// bit mask for job_send_event

const uint32_t EDGE_SHARED_OUTPUT_BUFFER_SIZE = 1*1024*1024;
const uint32_t EDGE_RING_OUTPUT_BUFFER_SIZE = 2*1024*1024; // per SPU
const uint32_t COMMAND_BUFFER_SIZE = 4*1024*1024;

#define MAX_NUM_BLENDSHAPES 2 // arbitrary limit
#define MAX_SEGMENTS 0x100 * 5

// This must be the index of the first of a contiguous block of RSX labels,
// one for each SPU.  Labels 0-63 are reserved by the OS, and your application
// and its libraries may have reserved others.  128 seems safe for this sample app.
const uint32_t kFirstRsxLabelIndex = 128;

using namespace cell;
using namespace cell::Gcm;

// Skinning matrix data
static float *s_skinningMatrices = 0;
static const uint32_t kNumBones = 32;
static const uint32_t kNumFrames = 120;

// edgeGeom jobs
extern char _binary_job_job_geom_spu_bin_start[];
extern char _binary_job_job_geom_spu_bin_size[];
extern char _binary_job_job_send_event_spu_bin_start[];	
extern char _binary_job_job_send_event_spu_bin_size[];

// instantiate the class
SampleApp app;

// edge geom data
static CellSpursJob256 jobs[MAX_SEGMENTS];
static EdgeGeomBlendShapeInfo shapeInfos[MAX_SEGMENTS*MAX_NUM_BLENDSHAPES] __attribute__((__aligned__(128)));
static uint32_t numShapeInfos = 0;
static float shapeAlphas[MAX_NUM_BLENDSHAPES] = {0};
static const unsigned int textureDimension = 1024;
static const uint32_t numElephants = 32;
static EdgeGeomLocalToWorldMatrix localToWorldMatrix[numElephants] __attribute__((__aligned__(128)));

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

static void* loadFile(const char* fileName, uint32_t& fileSize)
{
	FILE* fp = fopen(fileName, "rb");
	if (!fp) {
		printf("cannot open file %s\n", fileName);
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	void* data = memalign(128, fileSize);
	if (!data) {
		printf("can't allocate memory\n");
		return NULL;
	}
	fread(data, 1, fileSize, fp);
	fclose(fp);

	return data;
}

void SampleApp::CreateGeomJob(JobGeom256* job, CellGcmContextData *ctx, EdgeGeomPpuConfigInfo *info, 
			void *prevSkinningMatrices, void *curSkinningMatrices, uint32_t elephantIndex) const
{
	// Steam descriptor
	job->inputDmaList[kJobGeomDmaOutputStreamDesc].eal  = (uintptr_t)info->spuOutputStreamDesc;
	job->inputDmaList[kJobGeomDmaOutputStreamDesc].size = info->spuOutputStreamDescSize;

	// Indexes
	job->inputDmaList[kJobGeomDmaIndexes_0].eal  = (uintptr_t)info->indexes;
	job->inputDmaList[kJobGeomDmaIndexes_0].size = info->indexesSizes[0];
	job->inputDmaList[kJobGeomDmaIndexes_1].eal  = job->inputDmaList[kJobGeomDmaIndexes_0].eal + job->inputDmaList[kJobGeomDmaIndexes_0].size;
	job->inputDmaList[kJobGeomDmaIndexes_1].size = info->indexesSizes[1];
	
	// Skinning Matrices
	job->inputDmaList[kJobGeomDmaSkinMatrices_0].eal  = info->skinMatricesByteOffsets[0] + (uintptr_t)prevSkinningMatrices;
	job->inputDmaList[kJobGeomDmaSkinMatrices_0].size = info->skinMatricesSizes[0];
	job->inputDmaList[kJobGeomDmaSkinMatrices_1].eal  = info->skinMatricesByteOffsets[1] + (uintptr_t)prevSkinningMatrices;
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
	job->inputDmaList[kJobGeomDmaLocalToWorldMatrix].eal  = (uintptr_t)(&localToWorldMatrix[elephantIndex]);
	job->inputDmaList[kJobGeomDmaLocalToWorldMatrix].size = sizeof(EdgeGeomLocalToWorldMatrix);
	
	// SpuConfigInfo
	job->inputDmaList[kJobGeomDmaSpuConfigInfo].eal	 = (uintptr_t)info;
	job->inputDmaList[kJobGeomDmaSpuConfigInfo].size = sizeof(EdgeGeomSpuConfigInfo);
	
	// Fixed Point Attribute Offsets
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
	job->userData.eaCurrentFrameMatrix0 = info->skinMatricesByteOffsets[0] + (uintptr_t)curSkinningMatrices;
	job->userData.eaCurrentFrameMatrix1 = info->skinMatricesByteOffsets[1] + (uintptr_t)curSkinningMatrices;
	job->userData.currentFrameMatrixSize0 = info->skinMatricesSizes[0];
	job->userData.currentFrameMatrixSize1 = info->skinMatricesSizes[1];

	// Command Buffer Hole
	uint32_t holeSize = info->spuConfigInfo.commandBufferHoleSize << 4;

	// Account for an additional output ( previous frame skinned position )
	holeSize += 16;

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
	if(info->blendShapes) {
		job->userData.eaBlendShapeInfo = (uintptr_t)&shapeInfos[numShapeInfos];
		uint64_t numShapes = 0;
		for(uint32_t i = 0; i < info->numBlendShapes; ++i) {
			if(!info->blendShapes[i] || !shapeAlphas[i])
				continue;
			uint64_t shapeSize = (uint64_t)(info->blendShapeSizes[i]);
			shapeInfos[numShapeInfos].dmaTag = info->blendShapes[i] | (shapeSize << 32);
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
	job->header.sizeInOrInOut = info->ioBufferSize;
	job->header.sizeStack = 0;
	job->header.sizeScratch = info->scratchSizeInQwords;
	job->header.sizeCacheDmaList = 0;

    // enable memory checking in jobs.  SPURS will place a memory check marker in 16 byte buffers added after IO buffer, read only data, and stack, and check the integrity 
    // of the marker at the end of the job.  Enabling this flag will add 3-4% to SPURS overhead of running jobs.  Linking with -mspurs-job instead of -mspurs-job-initialize
    // will cause this flag to be ignored.
    job->header.jobType = CELL_SPURS_JOB_TYPE_MEMORY_CHECK;
}

SampleApp::SampleApp() :
mLastUpdate(FWTime::sZero),
mTurnRate(500.f),
mMoveRate(5.f)
{
	mNear = 0.2f;
	mFar = 500.f;
	mAnimPause = false;
	mDrawOnScreenInfo = true;
	mFullScreenMotionBlur = false;
	mLastFrameFullScreenMotionBlur = false;
}

SampleApp::~SampleApp()
{
}

void SampleApp::initShader( Shader& shader, void* pData )
{
	// Init shader
	shader.mCgProgram = (CGprogram)pData;
	cellGcmCgInitProgram( shader.mCgProgram);

	// Get uCode
	cellGcmCgGetUCode( shader.mCgProgram, &shader.mUCode, &shader.mUCodeSize);

	
	if ( CG_PROFILE_FPRSX == cellGcmCgGetProgramProfile( shader.mCgProgram	))
	{
		// move ucode to video memory
		void* ucode = (void*)cellGcmUtilAllocateLocalMemory( shader.mUCodeSize, 128);
		int ret = cellGcmAddressToOffset( ucode, &shader.mUCodeOffset);
		assert( ret == CELL_OK);
		(void)ret;
		memcpy( ucode, shader.mUCode, shader.mUCodeSize );
		shader.mUCode = ucode;
	}
}

void SampleApp::initShaders()
{
	extern char _binary_vpshader_vpo_start[];
	extern char _binary_fpshader_fpo_start[];
	extern char _binary_vpsky_vpo_start[];
	extern char _binary_fpsky_fpo_start[];
	extern char _binary_postvp_vpo_start[];
	extern char _binary_postfp_fpo_start[];

	// scene shaders
	initShader( mSkyVp, _binary_vpsky_vpo_start );
	initShader( mSkyFp, _binary_fpsky_fpo_start );
	initShader( mSceneVp, _binary_vpshader_vpo_start );
	initShader( mSceneFp, _binary_fpshader_fpo_start );
	initShader( mPostVp, _binary_postvp_vpo_start );
	initShader( mPostFp, _binary_postfp_fpo_start );
}

void SampleApp::initTexture( CellGcmTexture& texture, const char* textureName)
{
	CellGtfTextureAttribute attribute;
	uint32_t size;
	int ret;

	ret = cellGtfReadTextureAttribute( textureName, 0, &attribute );
	assert( ret == 0 );

	ret = cellGtfCalculateMemorySize( &attribute, &size);
	assert( ret == 0 );

	void* pTextureData = (void*)cellGcmUtilAllocateLocalMemory( size, 128);
	ret = cellGtfLoad( textureName, 0, &texture, CELL_GCM_LOCATION_LOCAL, pTextureData );
	assert( ret == 0 );
}

void SampleApp::initTextures()
{
	initTexture( mSkyCubeTexture, "/app_home/assets/sky.gtf" );
	initTexture( mDesertDiffuse, "/app_home/assets/M01_desertGrit01_DM.gtf" );
	initTexture( mDesertNormal, "/app_home/assets/M01_desertGrit01_NM.gtf" );
}

void SampleApp::initMRTs()
{
	uint32_t width = 1280;
	uint32_t height = 720;

	// get pitch for a tiled surface
	uint32_t pitch = cellGcmGetTiledPitchSize( width * 4);

	// calculate MRT memory requirements
	//	- 2 color buffers to simulate triple buffer
	//	- 1 motion vector buffer + linear depth
	const uint32_t kNumMrtFramebuffers = 3;
	uint32_t frameSize = pitch * (( height + 31) & ~31);
	uint32_t memSize = ((frameSize * kNumMrtFramebuffers) + 0xffff) & ~0xffff;
	printf( "Allocating MRT buffers in VRAM: %d bytes\n", memSize );

	// allocate memory
	uint8_t* tiledMrtArea = (uint8_t*)cellGcmUtilAllocateLocalMemory( memSize, 0x10000);

	// setup gcm surface structure
	mMrtTarget.type = CELL_GCM_SURFACE_PITCH;
	mMrtTarget.antialias = CELL_GCM_SURFACE_CENTER_1;
	mMrtTarget.colorFormat	= CELL_GCM_SURFACE_A8R8G8B8;
	mMrtTarget.colorTarget = CELL_GCM_SURFACE_TARGET_MRT1;
	mMrtTarget.width = width;
	mMrtTarget.height = height;
	mMrtTarget.x = 0;
	mMrtTarget.y = 0;

	// 0 is (doublebuffered) color
	cellGcmAddressToOffset( tiledMrtArea, &mMrtColorOffsets[0]);
	cellGcmAddressToOffset( tiledMrtArea + frameSize, &mMrtColorOffsets[1]);
	mMrtTarget.colorPitch[0] = pitch;
	mMrtTarget.colorLocation[0] = CELL_GCM_LOCATION_LOCAL;
	mMrtTarget.colorOffset[0] = 0xffffffff;		// will be double-buffered

	// 1 is motion vector and linear depth
	mMrtTarget.colorPitch[1] = pitch;
	mMrtTarget.colorLocation[1] = CELL_GCM_LOCATION_LOCAL;
	cellGcmAddressToOffset( tiledMrtArea + (frameSize*2), &mMrtTarget.colorOffset[1]);

	// Last 2 MRTs initialized but not used.
	for ( int j = 2; j < 4; ++j )
	{
		// Last MRT initialized but not used.
		mMrtTarget.colorLocation[j] = CELL_GCM_LOCATION_LOCAL;
		mMrtTarget.colorOffset[j] = mMrtTarget.colorOffset[1];
		mMrtTarget.colorPitch[j] = mMrtTarget.colorPitch[1];
	}

	// Depth buffer
	FWCellGCMWindow* pWindow = (FWCellGCMWindow*)FWWindow::getWindow();
	mMrtTarget.depthFormat = CELL_GCM_SURFACE_Z24S8;
	mMrtTarget.depthLocation = CELL_GCM_LOCATION_LOCAL;
	mMrtTarget.depthOffset	= pWindow->getFrameOffset(2);
	mMrtTarget.depthPitch = pWindow->getFramePitch(2);

	// set Tile info
	cellGcmSetTileInfo( 4, CELL_GCM_LOCATION_LOCAL, mMrtColorOffsets[0], memSize, pitch, CELL_GCM_COMPMODE_DISABLED, 0, 0);
	cellGcmBindTile( 4);
}

void SampleApp::createQuad()
{
	mQuadVertexData = (float*)cellGcmUtilAllocateLocalMemory(8*sizeof(float), 128);
	cellGcmAddressToOffset( mQuadVertexData, &mQuadVertexDataOffset);

	mQuadVertexData[0] = -1.0f;
	mQuadVertexData[1] =  1.0f;
	mQuadVertexData[2] = -1.0f;
	mQuadVertexData[3] = -1.0f;
	mQuadVertexData[4] =  1.0f;
	mQuadVertexData[5] = -1.0f;
	mQuadVertexData[6] =  1.0f;
	mQuadVertexData[7] =  1.0f;
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
		mpLeft = pPad->bindFilter();
		mpLeft->setChannel(FWInput::Channel_Button_Left);
		mpRight = pPad->bindFilter();
		mpRight->setChannel(FWInput::Channel_Button_Right);
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
	const uint8_t* colorTex = (uint8_t*)loadFile(SYS_APP_HOME"/assets/elephant-color.bin", colorTexSize);
	
	uint32_t normalTexSize = 0;
	const uint8_t* normalTex = (uint8_t*)loadFile(SYS_APP_HOME"/assets/elephant-normal.bin", normalTexSize);

	mTextures[0] = memalign(1024*1024, 2*colorTexSize); // double texture buffer size to leave room for mipmaps
	mTextures[1] = memalign(1024*1024, 2*normalTexSize);	
	memcpy(mTextures[0], colorTex, colorTexSize);
	memcpy(mTextures[1], normalTex, normalTexSize);
	buildMipmaps(mTextures[0], (uint8_t *)(mTextures[0]) + colorTexSize, textureDimension, textureDimension);
	buildMipmaps(mTextures[1], (uint8_t *)(mTextures[1]) + normalTexSize, textureDimension, textureDimension);
	cellGcmMapMainMemory(mTextures[0], 2*colorTexSize, &dummy);
	cellGcmMapMainMemory(mTextures[1], 2*normalTexSize, &dummy);

	// Skinning matrices
	// Skinning matrix
	uint32_t matrixFileSize = 0;
	s_skinningMatrices = (float*)loadFile(SYS_APP_HOME"/assets/elephant-matrices.bin", matrixFileSize);
	assert(matrixFileSize / (kNumBones * 12 * sizeof(float)) == kNumFrames); 

	// Init textures and shaders
	initTextures();
	initShaders();

	// Init MRT surface
	initMRTs();

	// Copy the Edge segment's RSX-only buffers into local memory, so that the RSX can see them.
	// This is something of a hack, as ideally you'd want to load this data directly into VRAM,
	// but it suffices for the purposes of this demo.
	uint8_t *geomBuffer = (uint8_t*)cellGcmUtilAllocateLocalMemory(1024*1024, 16);
	uint8_t *nextSegmentGeom = geomBuffer;
	for(uint32_t iSegment = 0; iSegment < sizeof(allSegments) / sizeof(allSegments[0]); ++iSegment)
	{
		// Skip this segment if it doesn't have a RSX-only stream
		if (allSegments[iSegment]->rsxOnlyVertexesSize == 0)
			continue;
		memcpy(nextSegmentGeom, allSegments[iSegment]->rsxOnlyVertexes,
			allSegments[iSegment]->rsxOnlyVertexesSize);
		// Update the segment's rsxOnlyVertexes pointer to reference the VRAM copy of the data.
		// NOTE: This leaks a pointer to the original rsxOnlyVertexes in main memory, but due
		// to the implementation of this sample it would be impossible to free it.
		allSegments[iSegment]->rsxOnlyVertexes = nextSegmentGeom;
		nextSegmentGeom += allSegments[iSegment]->rsxOnlyVertexesSize;
	}
	(void)s_skinningMatrices;
	(void)jobs;

	// set default camera position
	mCamera.setPosition( Point3( -28.5f, .0f, .0f));
	mCamera.setPan( 1.57 );

	// Initialize output buffer info.
	// Basic rules:
	//  - If the ring buffer size (end-start) is zero, use the shared buffer exclusively.
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

	// init vsync lock
	VSync::Initialize();
	VSync::SetDisplaySyncMethod( VSync::kDisplaySyncMethodLocked30hz );

	// Setup backbuffer structure
	memset( &mFrameTarget, 0, sizeof(mFrameTarget));
	mFrameTarget.colorFormat = mConfig.mColorFormat;
	mFrameTarget.colorTarget = CELL_GCM_SURFACE_TARGET_0;
	mFrameTarget.colorLocation[0] = CELL_GCM_LOCATION_LOCAL;
	mFrameTarget.colorOffset[0] = 0;
	mFrameTarget.colorPitch[0] = 0;
	mFrameTarget.colorLocation[1] = CELL_GCM_LOCATION_LOCAL;
	mFrameTarget.colorOffset[1]	= 0;
	mFrameTarget.colorPitch[1] = 64;
	mFrameTarget.colorLocation[2] = CELL_GCM_LOCATION_LOCAL;
	mFrameTarget.colorOffset[2]	= 0;
	mFrameTarget.colorPitch[2] = 64;
	mFrameTarget.colorLocation[3] = CELL_GCM_LOCATION_LOCAL;
	mFrameTarget.colorOffset[3] = 0;
	mFrameTarget.colorPitch[3] = 64;
	mFrameTarget.depthFormat = CELL_GCM_SURFACE_Z24S8;
	mFrameTarget.depthLocation = CELL_GCM_LOCATION_LOCAL;
	mFrameTarget.depthOffset = 0;
	mFrameTarget.depthPitch = 0;
	mFrameTarget.type = CELL_GCM_SURFACE_PITCH;
	mFrameTarget.antialias = CELL_GCM_SURFACE_CENTER_1;
	mFrameTarget.width = mDispInfo.mWidth;
	mFrameTarget.height = mDispInfo.mHeight;
	mFrameTarget.x = 0;
	mFrameTarget.y = 0;

	// create quad vertices
	createQuad();
	
	// Init post-processing stuff
	InitSpuPostprocessing( mDispInfo.mWidth, mDispInfo.mHeight);
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

	// mode...
	static int delay = 6;
	if ( delay == 0)
	{
		if ( mpSquare->getBoolTrue() )
			mAnimPause = !mAnimPause;

		if ( mpStart->getBoolTrue() )
			mDrawOnScreenInfo = !mDrawOnScreenInfo;

		if ( mpTriangle->getBoolTrue() )
			mFullScreenMotionBlur = !mFullScreenMotionBlur;

	} else --delay;

	return FWGCMCamApplication::onUpdate();
}

float g_skySize = 1000.f;

void SampleApp::drawSkybox( const Matrix4& mvp, const Matrix4& pmvp, const Matrix4& mv )
{
	cellGcmSetZMinMaxControl( CELL_GCM_FALSE, CELL_GCM_TRUE, CELL_GCM_FALSE );
	cellGcmSetBlendEnable(CELL_GCM_FALSE);
	cellGcmSetDepthTestEnable(CELL_GCM_FALSE);
	cellGcmSetDepthFunc(CELL_GCM_LESS);
	cellGcmSetShadeMode(CELL_GCM_SMOOTH);
	cellGcmSetCullFaceEnable(CELL_GCM_FALSE);

	// set shaders
	cellGcmSetVertexProgram( mSkyVp.mCgProgram, mSkyVp.mUCode );
	cellGcmSetFragmentProgram( mSkyFp.mCgProgram, mSkyFp.mUCodeOffset );

	// set parameters
	float size[4] = { g_skySize, g_skySize, g_skySize, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&mvp);
	cellGcmSetVertexProgramConstants(4, 16, (const float*)&pmvp);
	cellGcmSetVertexProgramConstants(8, 16, (const float*)&mv);
	cellGcmSetVertexProgramConstants(17, 4, size );
	
	// set sky texture
	cellGcmSetTextureControl(0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress(0,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	cellGcmSetTextureFilter(0, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	cellGcmSetTexture( 0, &mSkyCubeTexture );

	// disable attributes
	for ( int i = 1; i < 16; ++i )
		cellGcmSetVertexDataArray( i, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL , 0);
	
	static bool bInit = true;
	static uint32_t vertex_offset = 0;
	if ( bInit )
	{
		float X[] = {-1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  1.0f};
		float Y[] = {-1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f};
		float Z[] = {-1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f};

		float* pData = (float*)cellGcmUtilAllocateLocalMemory( 36 * sizeof( float ) * 3, 128);
		float* V = pData;

		*V++ = X[0]; *V++ = Y[0]; *V++ = Z[0];
		*V++ = X[1]; *V++ = Y[1]; *V++ = Z[1];
		*V++ = X[2]; *V++ = Y[2]; *V++ = Z[2];
		*V++ = X[1]; *V++ = Y[1]; *V++ = Z[1];
		*V++ = X[2]; *V++ = Y[2]; *V++ = Z[2];
		*V++ = X[3]; *V++ = Y[3]; *V++ = Z[3];
		*V++ = X[0]; *V++ = Y[0]; *V++ = Z[0];
		*V++ = X[1]; *V++ = Y[1]; *V++ = Z[1];
		*V++ = X[4]; *V++ = Y[4]; *V++ = Z[4];
		*V++ = X[1]; *V++ = Y[1]; *V++ = Z[1];
		*V++ = X[4]; *V++ = Y[4]; *V++ = Z[4];
		*V++ = X[5]; *V++ = Y[5]; *V++ = Z[5];
		*V++ = X[2]; *V++ = Y[2]; *V++ = Z[2];
		*V++ = X[3]; *V++ = Y[3]; *V++ = Z[3];
		*V++ = X[6]; *V++ = Y[6]; *V++ = Z[6];
		*V++ = X[3]; *V++ = Y[3]; *V++ = Z[3];
		*V++ = X[6]; *V++ = Y[6]; *V++ = Z[6];
		*V++ = X[7]; *V++ = Y[7]; *V++ = Z[7];
		*V++ = X[6]; *V++ = Y[6]; *V++ = Z[6];
		*V++ = X[7]; *V++ = Y[7]; *V++ = Z[7];
		*V++ = X[4]; *V++ = Y[4]; *V++ = Z[4];
		*V++ = X[7]; *V++ = Y[7]; *V++ = Z[7];
		*V++ = X[4]; *V++ = Y[4]; *V++ = Z[4];
		*V++ = X[5]; *V++ = Y[5]; *V++ = Z[5];
		*V++ = X[3]; *V++ = Y[3]; *V++ = Z[3];
		*V++ = X[1]; *V++ = Y[1]; *V++ = Z[1];
		*V++ = X[5]; *V++ = Y[5]; *V++ = Z[5];
		*V++ = X[3]; *V++ = Y[3]; *V++ = Z[3];
		*V++ = X[5]; *V++ = Y[5]; *V++ = Z[5];
		*V++ = X[7]; *V++ = Y[7]; *V++ = Z[7];
		*V++ = X[2]; *V++ = Y[2]; *V++ = Z[2];
		*V++ = X[0]; *V++ = Y[0]; *V++ = Z[0];
		*V++ = X[4]; *V++ = Y[4]; *V++ = Z[4];
		*V++ = X[2]; *V++ = Y[2]; *V++ = Z[2];
		*V++ = X[4]; *V++ = Y[4]; *V++ = Z[4];
		*V++ = X[6]; *V++ = Y[6]; *V++ = Z[6];

		cellGcmAddressToOffset( pData, &vertex_offset);
		bInit = false;
	}
	
	cellGcmSetVertexDataArray( 0 , 0, sizeof(float) * 3, 3, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, vertex_offset);
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_TRIANGLES, 0, 36);
	cellGcmSetDepthTestEnable(CELL_GCM_TRUE);
	cellGcmSetZMinMaxControl( CELL_GCM_TRUE, CELL_GCM_FALSE, CELL_GCM_FALSE );
}


void SampleApp::renderGround( const Matrix4& oldViewProj )
{
	// set ground texture
	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0<<8, 12<<8, CELL_GCM_TEXTURE_MAX_ANISO_1);
	cellGcmSetTextureAddress( 0, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	cellGcmSetTexture( 0, &mDesertDiffuse );

	cellGcmSetTextureAddress( 1, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	cellGcmSetTextureControl( 1, CELL_GCM_TRUE, 0<<8, 12<<8, CELL_GCM_TEXTURE_MAX_ANISO_1);
	cellGcmSetTextureFilter( 1, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	cellGcmSetTexture( 1, &mDesertNormal );
	
	// shaders
	cellGcmSetVertexProgram( mSceneVp.mCgProgram, mSceneVp.mUCode );
	cellGcmSetFragmentProgram( mSceneFp.mCgProgram, mSceneFp.mUCodeOffset );

	// vertex shader constants
	Matrix4 mv = transpose( getViewMatrix());
	Matrix4 mvp = transpose( getProjectionMatrix() * getViewMatrix());
	Matrix4 pmvp = transpose( oldViewProj);
	float lp[4] = {0,25,100,1};
	Point3 objSpaceEyePos = mCamera.getPosition();
	float ep[4] = {objSpaceEyePos[0], objSpaceEyePos[1], objSpaceEyePos[2], 1};
	cellGcmSetVertexProgramConstants( 0, 16, (float*)&mvp);
	cellGcmSetVertexProgramConstants( 4, 16, (float*)&pmvp);
	cellGcmSetVertexProgramConstants( 8, 16, (float*)&mv);
	cellGcmSetVertexProgramConstants( 17, 4, lp);
	cellGcmSetVertexProgramConstants( 18, 4, ep);

	// init vertex data
	static bool bInitVertexData = true;
	static uint32_t ground_vertex_offset = 0;
	static const float kGroundSize = 300.f;
	static const float kGroundY = -2.0f;
	static const float kTexScale = .1f;
	if ( bInitVertexData )
	{
		float* pData = (float*)cellGcmUtilAllocateLocalMemory( 48 * 4, 128);
		float* V = pData;
		
		static const float vtx[] =
		{
			-kGroundSize, -kGroundSize,
			-kGroundSize, kGroundSize,
			kGroundSize, kGroundSize,
			kGroundSize, -kGroundSize,
		};

		for ( int i = 0; i < 4; ++i )
		{
			float x = vtx[i*2];
			float z = vtx[i*2+1];
			*V++ = x; *V++ = kGroundY; *V++ = z;
			*V++ = 0; *V++ = 1; *V++ = 0;
			*V++ = 1; *V++ = 0; *V++ = 0; *V++ = 1;
			*V++ = x * kTexScale; *V++ = z * kTexScale;
		}

		cellGcmAddressToOffset( pData, &ground_vertex_offset);
		bInitVertexData = false;
	}

	// setup attributes
	for ( int i = 1; i < 16; ++i )
		cellGcmSetVertexDataArray( i, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL , 0);

	uint8_t stride = 48;	//	sizeof(float) * 3 + sizeof(float) * 3 + sizeof(float) * 4 + + sizeof(float) * 2;
	cellGcmSetVertexDataArray( 0, 0, stride, 3, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, ground_vertex_offset);
	cellGcmSetVertexDataArray( 1, 0, stride, 3, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, ground_vertex_offset);
	cellGcmSetVertexDataArray( 2, 0, stride, 3, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, ground_vertex_offset + 12);
	cellGcmSetVertexDataArray( 14, 0, stride, 4, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, ground_vertex_offset + 24);
	cellGcmSetVertexDataArray( 8, 0, stride, 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, ground_vertex_offset + 40);
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_QUADS, 0, 4);
}

void SampleApp::onRender()
{
	// wait for vsync and write the timestamp
	static uint32_t finishReference = 0;
	cellGcmFinish(finishReference++);

	ProfileBars::StartFrame();
	cellGcmSetTimeStamp(0);
	cellGcmFlush();
	cellGcmSetDitherEnable( CELL_GCM_FALSE );


	// Get current target configuration
	FWCellGCMWindow* pWindow = (FWCellGCMWindow*)FWWindow::getWindow();
	uint32_t frameIndex = pWindow->getFrameIndex();
	mFrameTarget.colorOffset[0] = pWindow->getFrameOffset(frameIndex );
	mFrameTarget.colorPitch[0]	= pWindow->getFramePitch(frameIndex );
	mFrameTarget.depthOffset = pWindow->getFrameOffset(2 );
	mFrameTarget.depthPitch = pWindow->getFramePitch(2 );

	// Select current color offset
	mMrtTarget.colorOffset[0] = mMrtColorOffsets[frameIndex];

	// setup MRT
	cellGcmSetSurfaceWindow( &mMrtTarget, CELL_GCM_WINDOW_ORIGIN_BOTTOM, CELL_GCM_WINDOW_PIXEL_CENTER_HALF );
	cellGcmSetColorMask( CELL_GCM_COLOR_MASK_R | CELL_GCM_COLOR_MASK_G | CELL_GCM_COLOR_MASK_B | CELL_GCM_COLOR_MASK_A );
	cellGcmSetColorMaskMrt( CELL_GCM_COLOR_MASK_MRT1_A | CELL_GCM_COLOR_MASK_MRT1_R | CELL_GCM_COLOR_MASK_MRT1_G | CELL_GCM_COLOR_MASK_MRT1_B |
		CELL_GCM_COLOR_MASK_MRT2_A | CELL_GCM_COLOR_MASK_MRT2_R | CELL_GCM_COLOR_MASK_MRT2_G | CELL_GCM_COLOR_MASK_MRT2_B );
	cellGcmSetBlendEnableMrt( CELL_GCM_FALSE, CELL_GCM_FALSE, CELL_GCM_FALSE);

	// base implementation clears screen and sets up camera
	FWGCMCamApplication::onRender();

	// used for motion blur
	static Matrix4 oldProjection = getProjectionMatrix();
	static Matrix4 oldView = getViewMatrix();

	// calculate matrix for sky rendering
	Matrix4 skyMat = getViewMatrix();
	Matrix4 oldSkyMat = oldView;
	skyMat.setTranslation( Vector3(0.0f, 0.0f, 0.0f));
	oldSkyMat.setTranslation( Vector3(0.0f, 0.0f, 0.0f));
	Matrix4 skyMatProj = transpose( getProjectionMatrix() * skyMat);
	Matrix4 oldSkyMatProj = transpose( oldProjection * oldSkyMat);

	// Set shader parameters
	float ms[4] = { 2.f, getNear(), 1.f / ( getFar() - getNear()), 0}; 
	
	CGparameter depthAndMotionScale0 = cellGcmCgGetNamedParameter( mSceneFp.mCgProgram, "depthAndMotionScale" );
	CGparameter depthAndMotionScale1 = cellGcmCgGetNamedParameter( mSkyFp.mCgProgram, "depthAndMotionScale" );
	assert( depthAndMotionScale0 );
	assert( depthAndMotionScale1 );
	cellGcmSetFragmentProgramParameter( mSceneFp.mCgProgram, depthAndMotionScale0, ms, mSceneFp.mUCodeOffset );
	cellGcmSetFragmentProgramParameter( mSkyFp.mCgProgram, depthAndMotionScale1, ms, mSkyFp.mUCodeOffset );

	// skybox
	drawSkybox( skyMatProj, oldSkyMatProj, transpose(skyMat) );

	// setup states
	cellGcmSetBlendEnable(CELL_GCM_FALSE);
	cellGcmSetDepthTestEnable(CELL_GCM_TRUE);
	cellGcmSetDepthFunc(CELL_GCM_LESS);
	cellGcmSetShadeMode(CELL_GCM_SMOOTH);

	// render the ground
	renderGround( oldProjection * oldView);

	// draw scene
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
	cellGcmSetVertexProgram( mSceneVp.mCgProgram, mSceneVp.mUCode );
	cellGcmSetFragmentProgram( mSceneFp.mCgProgram, mSceneFp.mUCodeOffset );
	
	cellGcmSetBlendEnable(CELL_GCM_FALSE);
	cellGcmSetDepthTestEnable(CELL_GCM_TRUE);
	cellGcmSetDepthFunc(CELL_GCM_LESS);
	cellGcmSetShadeMode(CELL_GCM_SMOOTH);

	// edgegeom viewport structure
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
    mViewportInfo.viewportOffsets[1] = mViewportInfo.scissorArea[3] - mViewportInfo.scissorArea[1] + mViewportInfo.viewportScales[1];
    mViewportInfo.viewportOffsets[2] = (mViewportInfo.depthRange[1] + mViewportInfo.depthRange[0]) * .5f;
    mViewportInfo.viewportOffsets[3] = 0.0f;
    mViewportInfo.sampleFlavor = CELL_GCM_SURFACE_CENTER_1;

	Matrix4 vp = transpose(getProjectionMatrix() * getViewMatrix());
	memcpy(mViewportInfo.viewProjectionMatrix, &vp, 64);

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
	
	if ( !mAnimPause )
		++frame;

	srand( 0xbabe );
	for (uint32_t iElephant = 0; iElephant < numElephants; iElephant++) 
	{
		Matrix4 mat = Matrix4::rotationZYX( Vector3(0, -1.5f, 0));
		
		// randomize an elephant position
		float ofz = rand()/(((float)RAND_MAX + 1) / 50.f) - 25.f;
		mat.setTranslation(Vector3( -10.0f + 10.0f * (float)iElephant, -2.0f, ofz));

		Matrix4 mv = transpose( getViewMatrix() * mat);
		Matrix4 mvp = transpose( getProjectionMatrix() * getViewMatrix() * mat);
		Matrix4 pmvp = transpose( oldProjection * oldView * mat);
		
		float lp[4] = {0,25,100,1};
		Vector4 objSpaceEyePos = inverse(mat) * mCamera.getPosition();
		float ep[4] = {objSpaceEyePos[0], objSpaceEyePos[1], objSpaceEyePos[2], 1};


		cellGcmSetVertexProgramConstants(&pbContext, 0, 16, (float*)&mvp);
		cellGcmSetVertexProgramConstants(&pbContext, 4, 16, (float*)&pmvp);
		cellGcmSetVertexProgramConstants(&pbContext, 8, 16, (float*)&mv);
		
		cellGcmSetVertexProgramConstants(&pbContext, 17, 4, lp);
		cellGcmSetVertexProgramConstants(&pbContext, 18, 4, ep);

		Matrix4 m = transpose(mat);
		memcpy(&localToWorldMatrix[iElephant].matrixData, &m, 48); 

		for(uint32_t i = 0; i < sizeof(allSegments) / sizeof(allSegments[0]); ++i)
		{
			// Bind the RSX-only vertex attributes for this job before it's created
			if (allSegments[i]->rsxOnlyVertexesSize > 0)
			{
				EdgeGeomVertexStreamDescription *rsxOnlyStreamDesc = (EdgeGeomVertexStreamDescription*)(allSegments[i]->rsxOnlyStreamDesc);
				uint32_t rsxVertexesOffset = 0;
				cellGcmAddressToOffset(allSegments[i]->rsxOnlyVertexes, &rsxVertexesOffset);
				for(uint32_t iRsxAttr=0; iRsxAttr < rsxOnlyStreamDesc->numAttributes; ++iRsxAttr)
				{
					EdgeGeomAttributeBlock &attr = rsxOnlyStreamDesc->blocks[iRsxAttr].attributeBlock;
					cellGcmSetVertexDataArray(&pbContext, attr.vertexProgramSlotIndex, 0, rsxOnlyStreamDesc->stride,
						attr.componentCount, attr.format, CELL_GCM_LOCATION_LOCAL,
						rsxVertexesOffset + attr.offset);
				}
			}

			JobGeom256 *job = (JobGeom256*) &jobs[numSegments++];
			
			uint32_t prevFrame = frame == 0 ? 0 : frame - 1;
			void* pPrevMatrixPalette = &s_skinningMatrices[kNumBones*12*(prevFrame%kNumFrames)];
			void* pCurMatrixPalette = &s_skinningMatrices[kNumBones*12*(frame%kNumFrames)];
			CreateGeomJob(job, &pbContext, allSegments[i], pPrevMatrixPalette, pCurMatrixPalette, iElephant);

			int ret = cellSpursCheckJob((CellSpursJob256*)job, sizeof(JobGeom256),
				sizeof(CellSpursJob256));
			if ( ret != CELL_OK){
				printf("cellSpursCheckJob failed : 0x%x\n", ret);
			}
		}
	}

	oldProjection = getProjectionMatrix();
	oldView = getViewMatrix();

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
	static uint8_t prios[8] = {0, 0, 1, 1, 1, 1, 0, 0};

	int ret = cellSpursJobChainAttributeInitialize(
		&jobChainAttributes, command_list, sizeof(CellSpursJob256),
		1, prios, 4, true, 0, 1, false, sizeof(CellSpursJob256), 6);
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

	// add return cmd
	cellGcmSetReturnCommand( &pbContext);

	// call elephant rendering
	ProfileBars::AddRsxMarker(0x20202000);		// grey rsx bar until that point (clear etc)
	cellGcmSetCallCommand(mCommandBufferOffset);
	ProfileBars::AddRsxMarker(0xff000000);		// red rsx bar indicates draw time	

	// disable attributes
	for ( int i = 1; i < 16; ++i )
		cellGcmSetVertexDataArray( i, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL , 0);

	// post-processing
	SwapSpuPostprocessBuffers();
	
	// Reset render target
	cellGcmSetSurface( &mFrameTarget );

	// restore MRTs
	cellGcmSetColorMask( CELL_GCM_COLOR_MASK_R | CELL_GCM_COLOR_MASK_G | CELL_GCM_COLOR_MASK_B | CELL_GCM_COLOR_MASK_A );
	cellGcmSetColorMaskMrt( 0 );
	cellGcmSetBlendEnableMrt( CELL_GCM_FALSE, CELL_GCM_FALSE, CELL_GCM_FALSE);
	
	cellGcmSetBlendEnable( CELL_GCM_FALSE);
	cellGcmSetDepthTestEnable( CELL_GCM_FALSE);
	cellGcmSetShadeMode( CELL_GCM_SMOOTH);

	// kick rsx
    cellGcmFlush();

	// wait for edgegeom 
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

	if ( mDrawOnScreenInfo )
	{
		// render profiler
		ProfileBars::Render();

		{
			// calc fps
			FWTimeVal	time = FWTime::getCurrentTime();
			float fFPS = 1.f / (float)(time - mLastTime);
			mLastTime = time;

			// print some messages
			FWDebugFont::setPosition( 0, 0);
			FWDebugFont::setColor( .5f, .5f, .5f, 1.0f);

			FWDebugFont::print("Edge Post Sample Application\n\n");
			FWDebugFont::printf("FPS: %.2f\n", fFPS);

			FWDebugFont::printf("\n\n%s motion blur (use triangle to toggle)", 
				mFullScreenMotionBlur ? "Fullscreen" : "Quarter resolution");
		}
	}

	VSync::EndFrame();
}

void SampleApp::onSize(const FWDisplayInfo& rDispInfo)
{
	FWGCMCamApplication::onSize(rDispInfo);
}

void SampleApp::onShutdown()
{
	CleanSpuPostprocessing();

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
		pPad->unbindFilter(mpLeft);
		pPad->unbindFilter(mpRight);
		pPad->unbindFilter(mpStart);
		pPad->unbindFilter(mpInputX0);
		pPad->unbindFilter(mpInputY0);
		pPad->unbindFilter(mpInputX1);
		pPad->unbindFilter(mpInputY1);
	}
}
