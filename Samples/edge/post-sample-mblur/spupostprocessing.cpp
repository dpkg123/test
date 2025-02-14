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
#include <sys/sys_time.h>
#include <sys/time_util.h>

#include "edge/post/edgepost.h"
#include "edge/post/edgepost_ppu.h"
#include "main.h"
#include "spu/edgepostjobheader.h"
#include "../common/profile_bars.h"
#include "gcm/FWCellGCMWindow.h"
#include "gcmutil.h"

using namespace cell;
using namespace cell::Gcm;

//////////////////////////////////////////////////////
/// NUMBER OF SPU EDGEPOST CAN USE
#define EDGEPOST_SPU_NUM				5
#define EDGEPOST_SPURS_PRIORITY			12


#ifdef SN_TARGET_PS3
#	define EDGEPOSTMODULE( name ) _binary_spu_##name##_bin_start
#else
#	ifdef __SNC__
#		ifdef _DEBUG
#			define EDGEPOSTMODULE( name ) _binary_spu_objs_debug_snc_##name##_spu_bin_start
#		else
#			define EDGEPOSTMODULE( name ) _binary_spu_objs_release_snc_##name##_spu_bin_start
#		endif
#	else
#		ifdef _DEBUG
#			define EDGEPOSTMODULE( name ) _binary_spu_objs_debug_##name##_spu_bin_start
#		else
#			define EDGEPOSTMODULE( name ) _binary_spu_objs_release_##name##_spu_bin_start
#		endif
#	endif
#endif

// edgepost embedded jobs
extern EdgePostJobHeader EDGEPOSTMODULE( motionblur );

// edgepost spurs task
extern char _binary_task_posttask_spu_elf_start[];				// edgepost taskset embedded image start
extern char _binary_task_posttask_spu_elf_size[];				// edgepost taskset embedded image size
CellSpursTaskset* g_pTaskset = 0;								// edgepost taskset
CellSpursTaskId g_tid[EDGEPOST_SPU_NUM];
CellSpursTaskAttribute g_attributeTask[EDGEPOST_SPU_NUM];
CellSpursTaskArgument g_argument[EDGEPOST_SPU_NUM];
CellSpursTaskSaveConfig g_saveConfig[EDGEPOST_SPU_NUM];
static const CellSpursTaskLsPattern s_postTaskLsPattern = { { 0x00000000, 0x00000000, 0x00000000, 0x00000001 } };	// last 2K for stack
volatile uint32_t g_completionCounter  __attribute__(( aligned(16))) = EDGEPOST_SPU_NUM;
static uint8_t* g_notify_area = 0;								// allocated notification area for RSX synchronization

static uint32_t g_InterruptRequestedValue = 0;		// Count number of requested interrupts
static uint32_t g_InterruptAcknowledgedValue = 0;	// Count number of acknowledged interrupts
static const uint32_t kRsxInterruptLabel = 252;		// The label we wait on for rsx interrupt synchronization

static inline uint32_t align( const uint32_t value, uint32_t alignment)
{
	return ( value + (alignment - 1)) & ~(alignment - 1);
}

void SampleApp::SetupStage( EdgePostProcessStage* pStage, const EdgePostImage* pImages, uint32_t numImages, EdgePostJobHeader* pJobCode	)
{
	
	edgePostSetupStage( pStage, pImages, numImages, pJobCode, pJobCode->bssOffset + pJobCode->bssSize, pJobCode->bssOffset, 0, 0 );
}

void SampleApp::SetupMotionBlur( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* motion, void* dstEa )
{
	EdgePostImage images[] =
	{
		EdgePostImage( kEdgePostOutputTile, dstEa, width, height, 4),
		EdgePostImage( kEdgePostInputTile, srcEa, width, height, 4, 16, 16, 4),
		EdgePostImage( kEdgePostInputTile, motion, width, height, 4),
	};
	SetupStage( pStage, images, 3, &EDGEPOSTMODULE( motionblur ));
}

void SampleApp::StartSpuPostprocessing()
{
	// wait notification data
	volatile CellGcmNotifyData* pNotifyData = cellGcmGetNotifyDataAddress( 0);
	while( pNotifyData->zero != 0 )
	{
		sys_timer_usleep(30);
	}

	// reset notification data
	pNotifyData->zero = 0xdeadbeef;

	// make sure every edgepost task has ended
	while ( g_completionCounter != EDGEPOST_SPU_NUM )
	{
		sys_timer_usleep(30);
	}

	// reset completion counter
	g_completionCounter = 0;

	// reset sync values for current frame
	int i = mFullScreenMotionBlur ? 0 : 1;
	mStages[i].flags |= EDGE_POST_WRITE_RSX_LABEL;
	mStages[i].rsxLabelAddress = (uint32_t)cellGcmGetLabelAddress( kEdgePostLabelIndex);
	mStages[i].rsxLabelValue = ++mPostLabelValue;

	// start SPU work
	edgePostInitializeWorkload( &mWorkload, &mStages[i], 1 );

	// Send task signals
	for ( int t = 0; t < EDGEPOST_SPU_NUM; ++t )
		cellSpursSendSignal( g_pTaskset, g_tid[t] );

	// Record method
	mLastFrameFullScreenMotionBlur = mFullScreenMotionBlur;
}

void RsxUserHandler( const uint32_t cause)
{
	// kick Edge Post
	SampleApp* pApp = (SampleApp*)cause;
	pApp->StartSpuPostprocessing();

	// release the RSX interrupt label
	g_InterruptAcknowledgedValue++;
	*cellGcmGetLabelAddress(kRsxInterruptLabel) = g_InterruptAcknowledgedValue;
}

void SampleApp::InitSpuPostprocessing( uint32_t width, uint32_t height )
{
	// remember these
	mPostWidth = width;
	mPostHeight = height;

	printf("Postprocessing window dimensions : %d x %d\n", width, height);

	// calculate host memory requirements
	uint32_t imageSize = align( width * height * 4, 128 );

	// calculate arena size
	uint32_t arenaSize = 
		imageSize +			// color buffer
		imageSize;			// motion buffer

	printf( "Allocating SPU Postprocessing arena size in host memory : %d ( ~ %dMB)\n", arenaSize, align( arenaSize, 1024 * 1024) );

	// align to 1MB
	arenaSize = align( arenaSize, 1024 * 1024);

	// Allocate and map XDR memory
	mpPostprocessArena = (uint8_t*)memalign( 1024*1024, arenaSize );
	int ret = cellGcmMapMainMemory( mpPostprocessArena, arenaSize, &mLocalToMainOffset);
	assert( ret == CELL_OK );

	// Allocate images inside the arena
	uint8_t* pData = mpPostprocessArena;
	
	// color buffer
	mLocalToMainImage = pData; pData += imageSize;

	// depth buffer + motion
	mDepthImage = pData; pData += imageSize;

	// Get RSX offsets
	ret = cellGcmAddressToOffset( mLocalToMainImage, &mLocalToMainOffset);
	assert( ret == CELL_OK );

	ret = cellGcmAddressToOffset( mDepthImage, &mDepthOffset);
	assert( ret == CELL_OK );

	// Setup stages
	memset( &mStages[0], 0, sizeof(mStages) );

	// we are assuming 1280 x 720
	assert( width == 1280 );
	assert( height == 720 );

	// Setup motion blur stage
	SetupMotionBlur( &mStages[0], 1280, 720, mLocalToMainImage, mDepthImage, mDepthImage );
	SetupMotionBlur( &mStages[1], 640, 360, mLocalToMainImage, mDepthImage, mDepthImage );

	// Set onscreen profiler target
	ProfileBars::SetTargetFps( 30.f );

	// Spurs priority
	uint8_t prios[8] = { 1, 0, 0, 0, 0, 0, 0, 0};
	for ( int j = 0; j < EDGEPOST_SPU_NUM; ++j)
		prios[j] = EDGEPOST_SPURS_PRIORITY;

	// Create a taskset
	g_pTaskset = (CellSpursTaskset*)memalign( CELL_SPURS_TASKSET_ALIGN, CELL_SPURS_TASKSET_CLASS1_SIZE);

	// Initialize it
	CellSpursTasksetAttribute attributeTaskset;
	ret = cellSpursTasksetAttributeInitialize ( &attributeTaskset, (uint64_t)0, prios, EDGEPOST_SPU_NUM );
	assert( ret == CELL_OK );

	ret = cellSpursTasksetAttributeSetName( &attributeTaskset, "Edgepost");
	assert( ret == CELL_OK );

	ret = cellSpursTasksetAttributeSetTasksetSize( &attributeTaskset, CELL_SPURS_TASKSET_CLASS1_SIZE);
	assert( ret == CELL_OK );

	ret = cellSpursCreateTasksetWithAttribute( &mSpurs, g_pTaskset, &attributeTaskset );
	assert( ret == CELL_OK );

	// Create spurs tasks
	for ( int t = 0; t < EDGEPOST_SPU_NUM; ++t )
	{
		// context save area
		cellSpursTaskGetContextSaveAreaSize( &g_saveConfig[t].sizeContext, &s_postTaskLsPattern);
		void *context_save_buffer = memalign(128, g_saveConfig[t].sizeContext);
		g_saveConfig[t].eaContext = context_save_buffer;
		g_saveConfig[t].lsPattern = &s_postTaskLsPattern;

		// argument
		g_argument[t].u32[0] = (uint32_t)&mWorkload;
		g_argument[t].u32[1] = (uint32_t)&g_completionCounter;

		// task
		ret = cellSpursTaskAttributeInitialize( &g_attributeTask[t], _binary_task_posttask_spu_elf_start, &g_saveConfig[t], &g_argument[t]);
		assert( ret == CELL_OK );
		ret = cellSpursCreateTaskWithAttribute( g_pTaskset, &g_tid[t], &g_attributeTask[t] );
		assert( ret == CELL_OK );
	}

	// initialize rsx label value
	volatile uint32_t *label = cellGcmGetLabelAddress( kEdgePostLabelIndex);
	*label = mPostLabelValue = 0;
	*cellGcmGetLabelAddress(kRsxInterruptLabel) = 0;

	// set RSX user handler
	cellGcmSetUserHandler( RsxUserHandler );

	// setup notify area
	g_notify_area = (uint8_t*)memalign( 1024*1024, 1024 );
	ret = cellGcmMapEaIoAddressWithFlags( (const void*)g_notify_area, CELL_GCM_NOTIFY_IO_ADDRESS_BASE, 1024*1024, CELL_GCM_IOMAP_FLAG_STRICT_ORDERING);
	assert( ret == CELL_OK );

	// set a non zero value for the first time
	CellGcmNotifyData* pNotifyData = cellGcmGetNotifyDataAddress( 0);
	pNotifyData->zero = 0xdeadbeef;
}

void SampleApp::CleanSpuPostprocessing()
{
	int ret = cellSpursShutdownTaskset(g_pTaskset);
	assert( CELL_OK == ret );

	ret = cellSpursJoinTaskset(g_pTaskset);
	assert( CELL_OK == ret );

	free(mpPostprocessArena);
	free(g_notify_area);
	free(g_pTaskset);
	for ( int t = 0; t < EDGEPOST_SPU_NUM; ++t )
		free(g_saveConfig[t].eaContext);
}

static void TransferDownsample( 
				uint32_t dstOffset, uint32_t dstPitch, 
				uint32_t srcOffset, uint32_t srcPitch,
				uint32_t srcWidth, uint32_t srcHeight, 
				uint32_t direction )
{
	// setup scale parameters
	CellGcmTransferScale scale;
	CellGcmTransferSurface surface;
	memset( &scale, 0, sizeof(scale));
	memset( &surface, 0, sizeof(surface));
	scale.conversion = CELL_GCM_TRANSFER_CONVERSION_TRUNCATE;
	scale.operation = CELL_GCM_TRANSFER_OPERATION_SRCCOPY;
	scale.format = CELL_GCM_TRANSFER_SCALE_FORMAT_A8R8G8B8;
	scale.outW = scale.clipW = srcWidth >> 1;
	scale.outH = scale.clipH = srcHeight >> 1;
	scale.ratioX = cellGcmGetFixedSint32(2.f);
	scale.ratioY = cellGcmGetFixedSint32(2.f);
	scale.inW = srcWidth;
	scale.inH = srcHeight;
	scale.origin = CELL_GCM_TRANSFER_ORIGIN_CENTER;
	scale.interp = CELL_GCM_TRANSFER_INTERPOLATOR_FOH;
	scale.inX = cellGcmGetFixedUint16(0.5f);
	scale.inY = cellGcmGetFixedUint16(0.5f);
	scale.pitch = srcPitch;
	scale.offset = srcOffset;
	surface.format = CELL_GCM_TRANSFER_SURFACE_FORMAT_A8R8G8B8;
	surface.pitch = dstPitch;
	surface.offset = dstOffset;

	// transfer/scale
	cellGcmSetTransferScaleMode( direction, CELL_GCM_TRANSFER_SURFACE);
	cellGcmSetTransferScaleSurface( &scale, &surface );
	cellGcmSetTransferLocation( CELL_GCM_LOCATION_LOCAL );
}

void SampleApp::Composite( const CellGcmSurface& dst, uint32_t texOffset, uint32_t w, uint32_t h)
{
	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram(mPostVp.mCgProgram, mPostVp.mUCode );
	cellGcmSetFragmentProgram(mPostFp.mCgProgram, mPostFp.mUCodeOffset );

	// setup texture
	CellGcmTexture tex;
	tex.format = CELL_GCM_TEXTURE_A8R8G8B8 | CELL_GCM_TEXTURE_LN | CELL_GCM_TEXTURE_NR;
	tex.mipmap = 1;
	tex.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
	tex.cubemap = CELL_GCM_FALSE;
	tex.remap = 
		CELL_GCM_TEXTURE_REMAP_REMAP << 14 |
		CELL_GCM_TEXTURE_REMAP_REMAP << 12 |
		CELL_GCM_TEXTURE_REMAP_REMAP << 10 |
		CELL_GCM_TEXTURE_REMAP_REMAP << 8 |
		CELL_GCM_TEXTURE_REMAP_FROM_B << 6 |
		CELL_GCM_TEXTURE_REMAP_FROM_G << 4 |
		CELL_GCM_TEXTURE_REMAP_FROM_R << 2 |
		CELL_GCM_TEXTURE_REMAP_FROM_A;
	tex.width = w;
	tex.height = h;
	tex.depth = 1;
	tex.location = CELL_GCM_LOCATION_MAIN;
	tex._padding = 0;
	tex.pitch = w << 2;
	tex.offset = texOffset;

	cellGcmSetTextureFilter(0, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl(0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress(0, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	cellGcmSetTexture(0, &tex );

	// set render target
	cellGcmSetSurfaceWindow(&dst, CELL_GCM_WINDOW_ORIGIN_BOTTOM, CELL_GCM_WINDOW_PIXEL_CENTER_INTEGER );

	// enable blend
	cellGcmSetBlendEnable(CELL_GCM_TRUE);
	cellGcmSetBlendFunc(CELL_GCM_SRC_ALPHA, CELL_GCM_ONE_MINUS_SRC_ALPHA, CELL_GCM_ONE, CELL_GCM_ZERO);

	// draw a fullscreen quad
	cellGcmSetVertexDataArray(0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray(0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);

	// disable blend
	cellGcmSetBlendEnable(CELL_GCM_FALSE);
}

void SampleApp::SwapSpuPostprocessBuffers()
{
	// RSX wait for spu post processing to end
	cellGcmSetWaitLabel( kEdgePostLabelIndex, mPostLabelValue);
	ProfileBars::AddRsxMarker(0xffffffff);

	// setup default states
	cellGcmSetBlendEnable( CELL_GCM_FALSE);
	cellGcmSetBlendEnableMrt( CELL_GCM_FALSE, CELL_GCM_FALSE, CELL_GCM_FALSE);
	cellGcmSetDepthTestEnable( CELL_GCM_FALSE);
	cellGcmSetCullFaceEnable( CELL_GCM_FALSE);
	cellGcmSetDepthMask( CELL_GCM_FALSE);
	cellGcmSetColorMask( CELL_GCM_COLOR_MASK_R | CELL_GCM_COLOR_MASK_G | CELL_GCM_COLOR_MASK_B | CELL_GCM_COLOR_MASK_A);
	cellGcmSetColorMaskMrt( 0 );

	//////////////////////////////////////////////////////////////////////////
	/// Move/Down-sample current color buffer into host memory
	if (mFullScreenMotionBlur)
	{
		cellGcmSetTransferImage( CELL_GCM_TRANSFER_LOCAL_TO_MAIN,
			mLocalToMainOffset, mPostWidth * 4, 0, 0,
			mMrtTarget.colorOffset[0], mMrtTarget.colorPitch[0], 0, 0,
			mPostWidth, mPostHeight, 4 );
	} else {
		TransferDownsample( mLocalToMainOffset, mPostWidth << 1,
			mMrtTarget.colorOffset[0], mMrtTarget.colorPitch[0],
			mPostWidth, mPostHeight, CELL_GCM_TRANSFER_LOCAL_TO_MAIN);
	}

	//////////////////////////////////////////////////////////////////////////
	/// Move SPU results into VRAM
	if (mLastFrameFullScreenMotionBlur)
	{
		cellGcmSetTransferImage( CELL_GCM_TRANSFER_MAIN_TO_LOCAL,
			mFrameTarget.colorOffset[0], mFrameTarget.colorPitch[0], 0, 0,
			mDepthOffset, mPostWidth * 4, 0, 0,
			mPostWidth, mPostHeight, 4 );
	} else {
		FWCellGCMWindow* pWindow = (FWCellGCMWindow*)FWWindow::getWindow();
		uint32_t frameIndex = pWindow->getFrameIndex();
		cellGcmSetTransferImage( CELL_GCM_TRANSFER_LOCAL_TO_LOCAL,
			mFrameTarget.colorOffset[0], mFrameTarget.colorPitch[0], 0, 0,
			mMrtColorOffsets[ frameIndex ^ 1], mPostWidth * 4, 0, 0,
			mPostWidth, mPostHeight, 4 );
		Composite(mFrameTarget, mDepthOffset, mPostWidth>>1, mPostHeight>>1);
	}

	//////////////////////////////////////////////////////////////////////////
	/// Move/Down-sample current motion buffer into host memory
	if (mFullScreenMotionBlur)
	{
		cellGcmSetTransferImage( CELL_GCM_TRANSFER_LOCAL_TO_MAIN,
			mDepthOffset, mPostWidth * 4, 0, 0,
			mMrtTarget.colorOffset[1], mMrtTarget.colorPitch[1], 0, 0,
			mPostWidth, mPostHeight, 4 );
	} else {
		TransferDownsample( mDepthOffset, mPostWidth << 1,
			mMrtTarget.colorOffset[1], mMrtTarget.colorPitch[1],
			mPostWidth, mPostHeight, CELL_GCM_TRANSFER_LOCAL_TO_MAIN);
	}

	// transfer notify data
	cellGcmSetNotifyIndex( 0 );
	cellGcmSetNotify();

	// We do not want to trigger an interrupt while a previous interrupt is still 
	// pending/executing as this will likely confuse the RSX making it call the
	// interrupt function with the wrong cause word.
	// Note that this is not strictly needed within this sample as we do not use
	// the interrupt function more than once within a frame.
	cellGcmSetWaitLabel(kRsxInterruptLabel, g_InterruptRequestedValue);
	g_InterruptRequestedValue++;

	cellGcmSetUserCommand( (const uint32_t)this );
}