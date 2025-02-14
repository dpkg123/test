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
#include <sys/prx.h>
#include <ppu_intrinsics.h>
#include <spu_printf.h>

#include "gcm/FWCellGCMWindow.h"
#include "FWDebugFont.h"
#include "FWTime.h"
#include "gcmutil.h"
#include "../common/profile_bars.h"

#include "main.h"

#define SPURS_SPU_NUM				1
#define SPU_THREAD_GROUP_PRIORITY	250
#define EVENT_FLAG_BIT				(0U)					// bit used by job_send_event (between 0 and 15)
#define EVENT_FLAG_MASK				(1U<<(EVENT_FLAG_BIT))	// bit mask for job_send_event

#define NOTIFY_LABEL_INDEX			128
#define DEST_WAIT_LABEL_INDEX		129
#define DEBUG_WAIT_LABEL_INDEX		130

using namespace cell;
using namespace cell::Gcm;

void* LoadFile( const char* filename, uint32_t* pFileSize );
void FreeFile( void* pBuffer );

void SetVertexProgramConstant(uint32_t index, float x, float y = 0.0f, float z = 0.0f, float w = 0.0f)
{
	float values[4] = { x, y, z, w };
	cellGcmSetVertexProgramConstants(index, 4, values);
}

// instantiate the class
SampleApp app;

SampleApp::SampleApp() 
 	: mLastUpdate(FWTime::sZero)
	, mIsRotating(false)
	, mAngle(0.0f)
	, mScale(1.0f)
	, mDxtMode(1)
	, mShowDebug(false)
	, mShowOffset(0.0f)
{
}

SampleApp::~SampleApp()
{
}

void SampleApp::createJobs()
{
    extern char _binary_job_job_send_event_spu_bin_start[]; 
    extern char _binary_job_job_send_event_spu_bin_size[];
    extern char _binary_job_job_dxt_spu_bin_start[]; 
    extern char _binary_job_job_dxt_spu_bin_size[];

	// DXT jobs
	mJobCompress = (CellSpursJob256*)memalign(128, sizeof(CellSpursJob256));
	__builtin_memset(mJobCompress, 0, sizeof(CellSpursJob256));
	mJobCompress->header.eaBinary = (uintptr_t)_binary_job_job_dxt_spu_bin_start;
	mJobCompress->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_dxt_spu_bin_size);

	mJobDecompress = (CellSpursJob256*)memalign(128, sizeof(CellSpursJob256));
	__builtin_memset(mJobDecompress, 0, sizeof(CellSpursJob256));
	mJobDecompress->header.eaBinary = (uintptr_t)_binary_job_job_dxt_spu_bin_start;
	mJobDecompress->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_dxt_spu_bin_size);

	uint32_t pitch = 4*mTexDim;
	uint32_t blocksPerBatch = mTexDim/4;
	uint32_t numBatches = mTexDim/4;
	uint32_t rawBatchSize = 4*pitch;				// batch of 4 rows
	uint32_t dxtBatchSize = 16*blocksPerBatch;		// worst case DXT3/5

	// DMA the first batch using SPURS job
    int ret = cellSpursJobGetInputList(&mJobCompress->workArea.dmaList[0], rawBatchSize, (uintptr_t)mSourceTextureData);
    assert(ret == CELL_OK);

	mJobCompress->header.sizeDmaList = 1*sizeof(mJobCompress->workArea.dmaList[0]);
	mJobCompress->header.sizeInOrInOut = 4*rawBatchSize;	// need space for 4 batches for pipelining
	mJobCompress->header.useInOutBuffer = 1;
	
	mJobCompress->workArea.userData[1] = (uintptr_t)mSourceTextureData;
	mJobCompress->workArea.userData[2] = (uintptr_t)mDestTextureData;
	mJobCompress->workArea.userData[3] = blocksPerBatch;
	mJobCompress->workArea.userData[4] = numBatches;
	mJobCompress->workArea.userData[5] = (uintptr_t)cellGcmGetLabelAddress(DEST_WAIT_LABEL_INDEX);
	mJobCompress->workArea.userData[6] = mDxtMode;
	
    ret = cellSpursJobGetInputList(&mJobDecompress->workArea.dmaList[0], dxtBatchSize, (uintptr_t)mDestTextureData);
    assert(ret == CELL_OK);

	mJobDecompress->header.sizeDmaList = 1*sizeof(mJobDecompress->workArea.dmaList[0]);
	mJobDecompress->header.sizeInOrInOut = 4*rawBatchSize;	// need space for 4 batches for pipelining
	mJobDecompress->header.useInOutBuffer = 1;
	
	mJobDecompress->workArea.userData[1] = (uintptr_t)mDestTextureData;
	mJobDecompress->workArea.userData[2] = (uintptr_t)mDebugTextureData;
	mJobDecompress->workArea.userData[3] = blocksPerBatch;
	mJobDecompress->workArea.userData[4] = numBatches;
	mJobDecompress->workArea.userData[5] = (uintptr_t)cellGcmGetLabelAddress(DEBUG_WAIT_LABEL_INDEX);
	mJobDecompress->workArea.userData[6] = mDxtMode + 1;

    // enable memory checking in jobs.  SPURS will place a memory check marker in 16 byte buffers added after IO buffer, read only data, and stack, and check the integrity 
    // of the marker at the end of the job.  Enabling this flag will add 3-4% to SPURS overhead of running jobs.  Linking with -mspurs-job instead of -mspurs-job-initialize
    // will cause this flag to be ignored.
    mJobCompress->header.jobType = CELL_SPURS_JOB_TYPE_MEMORY_CHECK;
    mJobDecompress->header.jobType = CELL_SPURS_JOB_TYPE_MEMORY_CHECK;
    

	ret = cellSpursCheckJob(mJobCompress, sizeof(CellSpursJob256),
		sizeof(CellSpursJob256));
	if ( ret != CELL_OK){
		printf("cellSpursCheckJob failed : 0x%x\n", ret);
	}
	ret = cellSpursCheckJob(mJobDecompress, sizeof(CellSpursJob256),
		sizeof(CellSpursJob256));
	if ( ret != CELL_OK){
		printf("cellSpursCheckJob failed : 0x%x\n", ret);
	}


	// end event job
	mJobEnd = (CellSpursJob256*)memalign(128, sizeof(CellSpursJob256));
	__builtin_memset(mJobEnd,0,sizeof(CellSpursJob256));
	mJobEnd->header.eaBinary = (uintptr_t)_binary_job_job_send_event_spu_bin_start;
	mJobEnd->header.sizeBinary = CELL_SPURS_GET_SIZE_BINARY(_binary_job_job_send_event_spu_bin_size);
	mJobEnd->workArea.userData[0] = (uintptr_t)&mSpursEventFlag;
	mJobEnd->workArea.userData[1] = (uint64_t)EVENT_FLAG_MASK;
}

void SampleApp::createJobChain()
{
	// allocate memory
	mGuard = (CellSpursJobGuard*)memalign(128, sizeof(CellSpursJobGuard));
	mCommandList = (uint64_t*)memalign(8, 7*sizeof(uint64_t));
	mJobChain = (CellSpursJobChain*)memalign(128, sizeof(CellSpursJobChain));

	// create the command list
	mCommandList[0] = CELL_SPURS_JOB_COMMAND_GUARD(mGuard);
	mCommandList[1] = CELL_SPURS_JOB_COMMAND_JOB(mJobCompress);
	mCommandList[2] = CELL_SPURS_JOB_COMMAND_SYNC;
	mCommandList[3] = CELL_SPURS_JOB_COMMAND_JOB(mJobDecompress);
	mCommandList[4] = CELL_SPURS_JOB_COMMAND_SYNC;
	mCommandList[5] = CELL_SPURS_JOB_COMMAND_JOB(mJobEnd);
	mCommandList[6] = CELL_SPURS_JOB_COMMAND_NEXT(&mCommandList[0]);

    // create the job chain
	CellSpursJobChainAttribute jobChainAttributes;
	uint8_t chainPriorities[8] = {1, 1, 1, 1, 1, 1, 1, 1};
	int ret = cellSpursJobChainAttributeInitialize(
		&jobChainAttributes, mCommandList, sizeof(CellSpursJob256), 
		1, chainPriorities, SPURS_SPU_NUM,
		true, 0, 1, false, sizeof(CellSpursJob256), SPURS_SPU_NUM);
	if (ret != CELL_OK){
		printf("cellSpursJobChainAttributeInitialize failed : 0x%x\n", ret);
	}
	//When the job manager detects an error, halt on SPU to help with debugging
	ret = cellSpursJobChainAttributeSetHaltOnError(&jobChainAttributes);
	if (ret != CELL_OK){
		printf("cellSpursJobChainAttributeSetHaltOnError failed : 0x%x\n", ret);
	}
	ret = cellSpursCreateJobChainWithAttribute(&mSpurs, mJobChain, &jobChainAttributes);
	if (ret != CELL_OK){
		printf("cellSpursCreateJobChainWithAttribute failed : 0x%x\n", ret);
	}

	// create the guard
	ret = cellSpursJobGuardInitialize(mJobChain, mGuard, 1, SPURS_SPU_NUM, 1);
	if (ret != CELL_OK){
		printf("cellSpursJobGuardInitialize failed : 0x%x\n", ret);
	}

	// we can safely start the chain
	ret = cellSpursRunJobChain(mJobChain);
	if (ret != CELL_OK){
		printf("cellSpursRunJobChain failed : 0x%x\n", ret);
	}
}

void SampleApp::createShaders()
{
	extern unsigned long _binary_vpshader_vpo_start;
	extern unsigned long _binary_fpshader_fpo_start;
	extern unsigned long _binary_fpclear_fpo_start;

	mQuadVertexProgram		= (CGprogram)(void*)&_binary_vpshader_vpo_start;
	mQuadFragmentProgram	= (CGprogram)(void*)&_binary_fpshader_fpo_start;
	mClearFragmentProgram	= (CGprogram)(void*)&_binary_fpclear_fpo_start;

	// init
	cellGcmCgInitProgram(mQuadVertexProgram);
	cellGcmCgInitProgram(mQuadFragmentProgram);
	cellGcmCgInitProgram(mClearFragmentProgram);

	// allocate video memory for fragment programs
	unsigned int ucodeSize;
	void *ucode;

	cellGcmCgGetUCode(mQuadFragmentProgram, &ucode, &ucodeSize);
	mQuadFragmentProgramUCode = cellGcmUtilAllocateLocalMemory(ucodeSize, 128);
	memcpy(mQuadFragmentProgramUCode, ucode, ucodeSize); 
	cellGcmAddressToOffset(mQuadFragmentProgramUCode, &mQuadFragmentProgramOffset);

	cellGcmCgGetUCode(mClearFragmentProgram, &ucode, &ucodeSize);
	mClearFragmentProgramUCode = cellGcmUtilAllocateLocalMemory(ucodeSize, 128);
	memcpy(mClearFragmentProgramUCode, ucode, ucodeSize);
	cellGcmAddressToOffset(mClearFragmentProgramUCode, &mClearFragmentProgramOffset);

	// get the vertex program
	cellGcmCgGetUCode(mQuadVertexProgram, &ucode, &ucodeSize);
	mQuadVertexProgramUCode = ucode;
}

void SampleApp::createTextures()
{
	uint32_t colorTexSize;
	uint8_t *colorTex = (uint8_t*)LoadFile( "/app_home/assets/logo-color.bin", &colorTexSize );

	mTexDim = 512;
	
	// HACK: modify the texture to create a binary alpha channel
	for ( uint32_t i = 0; i < (mTexDim*mTexDim); ++i )
		colorTex[i*4+3] = colorTex[i*4+3]>0 ? 0xff : 0;

	// allocate some VRAM for it and copy the data
	mOrigTextureData = cellGcmUtilAllocateLocalMemory(colorTexSize, 128);
	memcpy(mOrigTextureData, colorTex, colorTexSize);

	FreeFile( colorTex );

	// set up the texture object
	uint32_t format, remap;
	cellGcmUtilGetTextureAttribute(CELL_GCM_UTIL_RGBA8, &format, &remap, 0, 1);

	mOrigTexture.format = format;
	mOrigTexture.mipmap = 1;
	mOrigTexture.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
	mOrigTexture.cubemap = CELL_GCM_FALSE;
	mOrigTexture.remap = remap;
	mOrigTexture.width = mTexDim;
	mOrigTexture.height = mTexDim;
	mOrigTexture.depth = 1;
	mOrigTexture.location = CELL_GCM_LOCATION_LOCAL;
	mOrigTexture.pitch = 4*mTexDim;
	cellGcmAddressToOffset(mOrigTextureData, &mOrigTexture.offset);

	// total amount of texture data (aligned to 1MB for RSX mapping)
	uint32_t compressedTexSize = colorTexSize/4;	// DXT3/5 worst case
	uint32_t texDataSize = ( 2*colorTexSize + compressedTexSize + 0xfffff ) & ~0xfffff;

	mSourceTextureData = memalign(1024*1024, texDataSize);
	mDestTextureData = (uint8_t*)mSourceTextureData + colorTexSize;
	mDebugTextureData = (uint8_t*)mDestTextureData + compressedTexSize;

	uint32_t sourceTexOffset = 0;
	int ret = cellGcmMapMainMemory(mSourceTextureData, texDataSize, &sourceTexOffset);
	if (ret != CELL_OK){
		printf("cellGcmMapMainMemory failed : 0x%x\n", ret);
	}
	uint32_t destTexOffset = sourceTexOffset + colorTexSize;
	uint32_t debugTexOffset = destTexOffset + compressedTexSize;

	// source texture in main memory
	cellGcmUtilGetTextureAttribute(CELL_GCM_UTIL_ARGB8, &format, &remap, 0, 1);

	mSourceTexture.format = format;
	mSourceTexture.mipmap = 1;
	mSourceTexture.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
	mSourceTexture.cubemap = CELL_GCM_FALSE;
	mSourceTexture.remap = remap;
	mSourceTexture.width = mTexDim;
	mSourceTexture.height = mTexDim;
	mSourceTexture.depth = 1;
	mSourceTexture.location = CELL_GCM_LOCATION_MAIN;
	mSourceTexture.pitch = 4*mTexDim;
	mSourceTexture.offset = sourceTexOffset;

	mSourceSurface.type = CELL_GCM_SURFACE_PITCH;
	mSourceSurface.antialias = CELL_GCM_SURFACE_CENTER_1;
	mSourceSurface.colorFormat = CELL_GCM_SURFACE_A8R8G8B8;
	mSourceSurface.colorTarget = CELL_GCM_SURFACE_TARGET_0;
	mSourceSurface.colorLocation[0] = CELL_GCM_LOCATION_MAIN;
	mSourceSurface.colorOffset[0] = mSourceTexture.offset;
	mSourceSurface.colorPitch[0] = mSourceTexture.pitch;
	mSourceSurface.colorLocation[1] = CELL_GCM_LOCATION_LOCAL;
	mSourceSurface.colorOffset[1] = 0;
	mSourceSurface.colorPitch[1] = 64;
	mSourceSurface.colorLocation[2] = CELL_GCM_LOCATION_LOCAL;
	mSourceSurface.colorOffset[2] = 0;
	mSourceSurface.colorPitch[2] = 64;
	mSourceSurface.colorLocation[3] = CELL_GCM_LOCATION_LOCAL;
	mSourceSurface.colorOffset[3] = 0;
	mSourceSurface.colorPitch[3] = 64;
	mSourceSurface.depthFormat = CELL_GCM_SURFACE_Z24S8;
	mSourceSurface.depthLocation = 0;
	mSourceSurface.depthOffset 	= 0;
	mSourceSurface.depthPitch = 64;
	mSourceSurface.width = mTexDim;
	mSourceSurface.height = mTexDim;
	mSourceSurface.x = 0;
	mSourceSurface.y = 0;

	// dest texture in main memory
	mDestTexture.format = CELL_GCM_TEXTURE_COMPRESSED_DXT1;
	mDestTexture.mipmap = 1;
	mDestTexture.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
	mDestTexture.cubemap = CELL_GCM_FALSE;
	mDestTexture.remap = remap;
	mDestTexture.width = mTexDim;
	mDestTexture.height = mTexDim;
	mDestTexture.depth = 1;
	mDestTexture.location = CELL_GCM_LOCATION_MAIN;
	mDestTexture.pitch = 0;
	mDestTexture.offset = destTexOffset;

	// debug texture in main memory
	mDebugTexture.format = format;
	mDebugTexture.mipmap = 1;
	mDebugTexture.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
	mDebugTexture.cubemap = CELL_GCM_FALSE;
	mDebugTexture.remap = remap;
	mDebugTexture.width = mTexDim;
	mDebugTexture.height = mTexDim;
	mDebugTexture.depth = 1;
	mDebugTexture.location = CELL_GCM_LOCATION_MAIN;
	mDebugTexture.pitch = 4*mTexDim;
	mDebugTexture.offset = debugTexOffset;
}

void SampleApp::createQuad()
{
	mVertexData = (float*)cellGcmUtilAllocateLocalMemory(8*sizeof(float), 128);
	cellGcmAddressToOffset(mVertexData, &mVertexDataOffset);
	
	mVertexData[0] = -1.0f;
	mVertexData[1] =  1.0f;

	mVertexData[2] = -1.0f;
	mVertexData[3] = -1.0f;

	mVertexData[4] =  1.0f;
	mVertexData[5] = -1.0f;

	mVertexData[6] =  1.0f;
	mVertexData[7] =  1.0f;
}

bool SampleApp::onInit(int argc, char **ppArgv)
{
	FWGCMCamApplication::onInit(argc, ppArgv);

	FWInputDevice	*pPad = FWInput::getDevice(FWInput::DeviceType_Pad, 0);
	if(pPad) {
		mpLeft = pPad->bindFilter();
		mpLeft->setChannel(FWInput::Channel_Button_Left);
		mpRight = pPad->bindFilter();
		mpRight->setChannel(FWInput::Channel_Button_Right);
		mpSquare = pPad->bindFilter();
		mpSquare->setChannel(FWInput::Channel_Button_Square);
		mpTriangle = pPad->bindFilter();
		mpTriangle->setChannel(FWInput::Channel_Button_Triangle);
		mpCross = pPad->bindFilter();
		mpCross->setChannel(FWInput::Channel_Button_Cross);
	}

	// Get PPU thread priority
	sys_ppu_thread_t my_ppu_thread_id;
	int ret = sys_ppu_thread_get_id(&my_ppu_thread_id);
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

	// Create graphics objects
	createShaders();
	createTextures();
	createQuad();

	// Create jobs
	createJobs();
	createJobChain();

	// set our gcm user interrupt handler
	cellGcmSetUserHandler( &SampleApp::userHandler );
	
	// reset the labels
	*cellGcmGetLabelAddress(NOTIFY_LABEL_INDEX) = 0;
	*cellGcmGetLabelAddress(DEST_WAIT_LABEL_INDEX) = 0;
	*cellGcmGetLabelAddress(DEBUG_WAIT_LABEL_INDEX) = 0;

	// Profile bars
	ProfileBars::Init(&mSpurs, SPURS_SPU_NUM);

	return true;
}

void SampleApp::userHandler( uint32_t )
{
	app.onInterrupt();
}

void SampleApp::onInterrupt()
{
	// wait for the label before running the job chain
	volatile uint32_t *notifyLabel = cellGcmGetLabelAddress(NOTIFY_LABEL_INDEX);
	uint32_t syncValue = *notifyLabel;
	while(syncValue == 0)
	{
		sys_timer_usleep(30);
		syncValue = *notifyLabel;
	}

	// reset the value
	*notifyLabel = 0;

	// run the job chain by notifying the guard
	int ret = cellSpursJobGuardNotify(mGuard);
	if (ret != CELL_OK){
        printf("cellSpursJobGuardNotify failed : 0x%x\n", ret);
	}
}

bool SampleApp::onUpdate()
{
	// compute the time delta for this frame
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

	// handle pad input
	if(mpLeft && mpLeft->getBoolTrue())
		mShowDebug = false;
	if(mpRight && mpRight->getBoolTrue())
		mShowDebug = true;

	if(mpSquare && mpSquare->getBoolTrue())
		mIsRotating = !mIsRotating;
	if(mpTriangle && mpTriangle->getBoolTrue())
	{
		mAngle = 0.0f;
		mScale = 1.0f;
		mIsRotating = false;
	}
	if(mpCross && mpCross->getBoolTrue())
	{
		mDxtMode += 2;
		if( mDxtMode > 7 )
			mDxtMode = 1;
	}

	// update the rotation of the quad
	if(mIsRotating)
	{
		mAngle += 0.5f*deltaTime;

		float twoPi = 2.0f*3.1415926535f;
		while(mAngle > twoPi)
			mAngle -= twoPi;

		mScale = 1.0f + 0.7f*sinf(3.0f*mAngle);
	}

	// compute the texture location to show the debug texture or not
	float maxOffset = 2.0f*(float)mTexDim/1280.0f;
	float deltaOffset = 6.0f*deltaTime*maxOffset;
	if( mShowDebug && mShowOffset < maxOffset )
	{
		float newOffset = mShowOffset + deltaOffset;
		mShowOffset = ( newOffset < maxOffset ) ? newOffset : maxOffset;
	}
	else if( !mShowDebug && mShowOffset > 0.0f )
	{
		float newOffset = mShowOffset - deltaOffset;
		mShowOffset = ( newOffset > 0.0f ) ? newOffset : 0.0f;
	}

	// update the job descriptions and texture objects with the currently used DXT mode
	mJobCompress->workArea.userData[6] = mDxtMode;
	mJobDecompress->workArea.userData[6] = mDxtMode + 1;
	switch( mDxtMode )
	{
	case 1:
	case 7:
	default:
		mDestTexture.format = CELL_GCM_TEXTURE_COMPRESSED_DXT1;
		break;

	case 3:
		mDestTexture.format = CELL_GCM_TEXTURE_COMPRESSED_DXT23;
		break;

	case 5:
		mDestTexture.format = CELL_GCM_TEXTURE_COMPRESSED_DXT45;
		break;
	}

	// call base class
	return FWGCMCamApplication::onUpdate();
}

void SampleApp::renderBackground()
{
	// render the whole screen using the checkerboard shader
	cellGcmSetDepthTestEnable(CELL_GCM_FALSE);

	cellGcmSetVertexProgram(mQuadVertexProgram, mQuadVertexProgramUCode);
	cellGcmSetFragmentProgram(mClearFragmentProgram, mClearFragmentProgramOffset);
	
	SetVertexProgramConstant(0, 1.0f, 1.0f, 0.0f, 0.0f);
	SetVertexProgramConstant(1, 1280.0f/64.0f, 0.0f, 0.0f, 720.0f/64.0f);

	cellGcmSetVertexDataArray(0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mVertexDataOffset);
	cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray(0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::renderToSourceTexture()
{
	// set the source texture as our render target
	cellGcmSetSurface(&mSourceSurface);

	float halfDim = 0.5f*(float)mTexDim;
	float scale[4] = { halfDim, -halfDim, 0.5f, 0.0f };
	float offset[4] = { halfDim, halfDim, 0.5f, 0.0f };
	cellGcmSetViewport(0, 0, mTexDim, mTexDim, 0.0f, 1.0f, scale, offset);
	cellGcmSetScissor(0, 0, mTexDim, mTexDim);

	cellGcmSetVertexProgram(mQuadVertexProgram, mQuadVertexProgramUCode);
	cellGcmSetFragmentProgram(mQuadFragmentProgram, mQuadFragmentProgramOffset);
	
	// render using rotated uvs
	float st = mScale*sin( mAngle );
	float ct = mScale*cos( mAngle );

	SetVertexProgramConstant(0, 1.0f, 1.0f, 0.0f, 0.0f);
	SetVertexProgramConstant(1, ct, st, -st, ct);
	
	cellGcmSetTextureControl(0, CELL_GCM_TRUE, 0<<8, 0<<8, CELL_GCM_TEXTURE_MAX_ANISO_1);
	cellGcmSetTextureFilter(0, 0, CELL_GCM_TEXTURE_LINEAR_NEAREST, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	cellGcmSetTexture(0, &mOrigTexture);

	cellGcmSetVertexDataArray(0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mVertexDataOffset);
	cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray(0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::renderJobChainTrigger()
{
	// write the label value and trigger our interrupt
	cellGcmSetWriteBackEndLabel(NOTIFY_LABEL_INDEX, 1);
	cellGcmSetUserCommand(1);

	// kick rsx to get it started on the rotated texture ASAP
    cellGcmFlush();
}

void SampleApp::renderStallForResults(uint32_t labelIndex)
{
	cellGcmSetWaitLabel(labelIndex, 1);
	cellGcmSetWriteCommandLabel(labelIndex, 0);
}

void SampleApp::waitForJobChainFinished()
{
	// wait for the final job to send its event
    uint16_t ev_mask = EVENT_FLAG_MASK;
	int ret = cellSpursEventFlagWait(&mSpursEventFlag,&ev_mask,CELL_SPURS_EVENT_FLAG_AND);
    if (ret != CELL_OK){
        printf("cellSpursEventFlagWait failed : 0x%x\n", ret);
    } 
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

	// background
	ProfileBars::AddRsxMarker(0x00ffff00);
	renderBackground();

	// render to the source texture
	ProfileBars::AddRsxMarker(0x0000ff00);
	renderToSourceTexture();

	// have the RSX trigger the job chain to start processing the source texture as soon as possible
	renderJobChainTrigger();

	// set the intial target
	ProfileBars::AddRsxMarker(0x00ff0000);
	FWCellGCMWindow::getInstance()->resetRenderTarget();
	FWGCMCamApplication::setViewport();
	cellGcmSetDepthTestEnable(CELL_GCM_FALSE);

	// set up for rendering our textures
	cellGcmSetVertexProgram(mQuadVertexProgram, mQuadVertexProgramUCode);
	cellGcmSetFragmentProgram(mQuadFragmentProgram, mQuadFragmentProgramOffset);
	
	float xs = (float)mTexDim/1280.0f;
	float ys = (float)mTexDim/720.0f;
	float x1 = -( xs + 8.0f/1280.0f + mShowOffset );
	float x2 = x1 + 2.0f*xs + 16.0f/1280.0f;
	float x3 = x2 + 2.0f*xs + 16.0f/1280.0f;
	
	SetVertexProgramConstant(1, 1.0f, 0.0f, 0.0f, 1.0f);

	cellGcmSetTextureControl(0, CELL_GCM_TRUE, 0<<8, 0<<8, CELL_GCM_TEXTURE_MAX_ANISO_1);
	cellGcmSetTextureFilter(0, 0, CELL_GCM_TEXTURE_NEAREST_NEAREST, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);

	cellGcmSetVertexDataArray(0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mVertexDataOffset);	

	cellGcmSetBlendEnable(mDxtMode != 1 ? CELL_GCM_TRUE : CELL_GCM_FALSE);
	cellGcmSetBlendFunc(CELL_GCM_SRC_ALPHA, CELL_GCM_ONE_MINUS_SRC_ALPHA, CELL_GCM_ZERO, CELL_GCM_ZERO);

	// render source texture view (this easily covers the SPU compression time)
	cellGcmSetInvalidateTextureCache(CELL_GCM_INVALIDATE_TEXTURE);
	cellGcmSetTexture(0, &mSourceTexture);
	SetVertexProgramConstant(0, xs, ys, x1, 0.0f);
	cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_QUADS, 0, 4);

	// wait for the compressed results
	ProfileBars::AddRsxMarker(0x00000000);
	renderStallForResults(DEST_WAIT_LABEL_INDEX);
	ProfileBars::AddRsxMarker(0x00ff0000);

	// render dest texture view
	cellGcmSetInvalidateTextureCache(CELL_GCM_INVALIDATE_TEXTURE);
	cellGcmSetTexture(0, &mDestTexture);
	SetVertexProgramConstant(0, xs, ys, x2, 0.0f);
	cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_QUADS, 0, 4);

	// wait for the debug results
	ProfileBars::AddRsxMarker(0x00000000);
	renderStallForResults(DEBUG_WAIT_LABEL_INDEX);
	ProfileBars::AddRsxMarker(0x00ff0000);

	// render debug texture view
	cellGcmSetInvalidateTextureCache(CELL_GCM_INVALIDATE_TEXTURE);
	cellGcmSetTexture(0, &mDebugTexture);
	SetVertexProgramConstant(0, xs, ys, x3, 0.0f);
	cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_QUADS, 0, 4);

	// clean up
	cellGcmSetBlendEnable(CELL_GCM_FALSE);
	cellGcmSetVertexDataArray(0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);

	// wait until the jobchain is completely done
	waitForJobChainFinished();

	// cellGcmFinish is only there because of profile bars displayed for the current frame.
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
		FWDebugFont::setColor(1.f, 1.f, 1.f, 1.f);

		FWDebugFont::print("Edge DXT Sample Application - [Cross] to cycle DXT mode, [Square] to toggle rotation, [Triangle] to reset\n\n");
		FWDebugFont::printf("FPS: %.2f\n", fFPS);

		// help text
		int textOffset = (int)(0.5f*1280.0f*mShowOffset);

		FWDebugFont::setPosition(100 - textOffset, 50);
		FWDebugFont::print("Uncompressed");

		FWDebugFont::setPosition(620 - textOffset, 50);
		
		switch (mDxtMode)
		{
		case 1:
		case 3:
		case 5:
		default:
			FWDebugFont::printf("DXT%d", mDxtMode);
			break;

		case 7:
			FWDebugFont::printf("DXT1a");
			break;
		}

		FWDebugFont::setPosition(1150 - textOffset, 50);
		FWDebugFont::print("Debug");
	}
}

void SampleApp::onSize(const FWDisplayInfo& rDispInfo)
{
	FWGCMCamApplication::onSize(rDispInfo);
}

void SampleApp::onShutdown()
{
	// shut down the job chain
	int ret = cellSpursShutdownJobChain(mJobChain);
    if (ret != CELL_OK){
        printf("cellSpursShutdownJobChain failed : 0x%x\n", ret);
    } 
	ret = cellSpursJoinJobChain(mJobChain);
    if (ret != CELL_OK){
        printf("cellSpursJoinJobChain failed : 0x%x\n", ret);
    } 

	// kill the event
	ret = cellSpursEventFlagDetachLv2EventQueue(&mSpursEventFlag);
	if (ret != CELL_OK) {
		printf("cellSpursEventFlagDetachLv2EventQueue failed : 0x%x\n", ret);
	}

	// kill spurs
	ret = cellSpursFinalize(&mSpurs);
	if (ret != CELL_OK){
		printf("cellSpursFinalize failed : 0x%x\n", ret);
	}
	ret = spu_printf_finalize();
	if (ret != CELL_OK){
		printf("spu_printf_finalize failed : 0x%x\n", ret);
	}

	// unbind input filters
	FWInputDevice	*pPad = FWInput::getDevice(FWInput::DeviceType_Pad, 0);
	if(pPad)  {	
		pPad->unbindFilter(mpTriangle);
		pPad->unbindFilter(mpSquare);
		pPad->unbindFilter(mpCross);
	}

	FWGCMCamApplication::onShutdown();
}

//////////////////////////////////////////////////////////////////////////

inline uint32_t RoundUp( uint32_t value, uint32_t alignment )
{
	assert( ((alignment & (alignment - 1)) == 0) && alignment );

	return (value + (alignment - 1)) & ~(alignment - 1);
}

//////////////////////////////////////////////////////////////////////////

void* LoadFile( const char* filename, uint32_t* pFileSize )
{
	*pFileSize = 0;	//In case we fail, set size to zero

	FILE* fp = fopen( filename, "rb" );
	if ( fp == NULL )
	{
		printf( "Error: Failed to load \"%s\"\n", filename );
		assert( false );
	}

	int ret = fseek( fp, 0, SEEK_END);
	assert( ret == 0 );

	size_t fileSize = ftell( fp );
	assert( fileSize > 0 );

	ret = fseek( fp, 0, SEEK_SET );
	assert( ret == 0 );

	int mallocSize = RoundUp( fileSize, 16 );	//Round up the malloc size to a qword multiple
	void* pData = memalign( 16, mallocSize );
	assert( pData );
	memset( pData, 0, mallocSize );

	size_t numRead = fread( pData, 1, fileSize, fp );
	assert( fileSize == numRead );
	(void) numRead;

	ret = fclose( fp );
	assert( ret == 0 );

	*pFileSize = fileSize;
	return pData;
}

//////////////////////////////////////////////////////////////////////////

void FreeFile( void* pBuffer )
{
	free( pBuffer );
}

//////////////////////////////////////////////////////////////////////////
