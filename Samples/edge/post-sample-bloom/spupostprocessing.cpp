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
#include "spu/postParams.h"
#include "spu/edgepostjobheader.h"
#include "../common/profile_bars.h"
#include "gcm/FWCellGCMWindow.h"
#include "gcmutil.h"

using namespace cell;
using namespace cell::Gcm;

//////////////////////////////////////////////////////
/// NUMBER OF SPU EDGEPOST CAN USE
#define EDGEPOST_SPU_NUM				2
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
extern EdgePostJobHeader EDGEPOSTMODULE( bloom );

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

//////////////////////////////////////////////////////////////////////////
// Stage setups


void SampleApp::SetupStage( EdgePostProcessStage* pStage, const EdgePostImage* pImages, uint32_t numImages, EdgePostJobHeader* pJobCode	)
{
	edgePostSetupStage( pStage, pImages, numImages, pJobCode, pJobCode->bssOffset + pJobCode->bssSize, 
		pJobCode->bssOffset, &mPostParams, sizeof(mPostParams) );
}

void SampleApp::SetupBloomStart( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa )
{
	EdgePostImage images[] =
	{
		EdgePostImage( kEdgePostOutputTile, dstEa, width/2, height/2, 8),
		EdgePostImage( kEdgePostInputTile, srcEa, width, height, 4, 0, 0, 2),
	};
	SetupStage( pStage, images, 2, &EDGEPOSTMODULE( bloom ));
	pStage->user0 = 1;
}

void SampleApp::SetupBloomBlur( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa, float* weights, bool horizontal, void* downsampleEa)
{
	uint32_t numImages = downsampleEa == 0 ? 2 : 3;

	if ( horizontal )
	{
		EdgePostImage images[] =
		{
			EdgePostImage( kEdgePostOutputTile, dstEa, width, height, 8),
			EdgePostImage( kEdgePostInputTile, srcEa, width, height, 8, 4),
			EdgePostImage( kEdgePostOutputTile, downsampleEa, width/2, height/2, 8),
		};
		SetupStage( pStage, images, numImages, &EDGEPOSTMODULE( bloom ));
	} else {
		EdgePostImage images[] =
		{
			EdgePostImage( kEdgePostOutputTile, dstEa, width, height, 8),
			EdgePostImage( kEdgePostInputTile, srcEa, width, height, 8, 0, 3),
			EdgePostImage( kEdgePostOutputTile, downsampleEa, width/2, height/2, 8),
		};
		SetupStage( pStage, images, numImages, &EDGEPOSTMODULE( bloom ));
	}

	pStage->user0 = horizontal ? 2 : 3;
	pStage->userDataF[0] = weights[0];
	pStage->userDataF[1] = weights[1];
	pStage->userDataF[2] = weights[2];
	pStage->userDataF[3] = weights[3];
}

void SampleApp::SetupBloomUpsampleCombine( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* src0Ea, void* src1Ea, void* dstEa, bool bConvertToFp16 )
{
	uint32_t numImages = src1Ea == 0 ? 2 : 3;

	EdgePostImage images[] =
	{
		EdgePostImage( kEdgePostOutputTile, dstEa, width, height, 8),
		EdgePostImage( kEdgePostInputTile, src0Ea, width/2, height/2, 8, 4, 1),
		EdgePostImage( kEdgePostInputTile, src1Ea, width, height, 8),
	};
	SetupStage( pStage, images, numImages, &EDGEPOSTMODULE( bloom ));
	pStage->user0 = 4;
	pStage->user1 = bConvertToFp16 ? 1 : 0;
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
	mStages[ mNumStages - 1].flags |= EDGE_POST_WRITE_RSX_LABEL;
	mStages[ mNumStages - 1].rsxLabelAddress = (uint32_t)cellGcmGetLabelAddress( kEdgePostLabelIndex);
	mStages[ mNumStages - 1].rsxLabelValue = ++mPostLabelValue;

	// calculate current frame luminance
	mAvgLuminance = vec_extract( mPostParams.m_avgAndMaxLuminance, 0 ) / mPostParams.m_numAvgLuminanceTiles;
	mMaxLuminance = vec_extract( mPostParams.m_avgAndMaxLuminance, 1 );
	float oldAvgLuminance = vec_extract( mPostParams.m_prevAvgAndMaxLuminance, 0 );
	float oldMaxLuminance = vec_extract( mPostParams.m_prevAvgAndMaxLuminance, 1 );
	mAvgLuminance = mAvgLuminance * .05f + oldAvgLuminance * .95f;
	mMaxLuminance = mMaxLuminance * .05f + oldMaxLuminance * .95f;
	mPostParams.m_prevAvgAndMaxLuminance = vec_splats( mAvgLuminance);
	mPostParams.m_prevAvgAndMaxLuminance = vec_insert( mMaxLuminance, mPostParams.m_prevAvgAndMaxLuminance, 1);

	// reset avg luminance
	mPostParams.m_avgAndMaxLuminance = vec_splats( .0f );

	// start SPU work
	edgePostInitializeWorkload( &mWorkload, &mStages[0], mNumStages );

	// Send task start signal
	for ( int t = 0; t < EDGEPOST_SPU_NUM; ++t )
		cellSpursSendSignal( g_pTaskset, g_tid[t] );
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
	uint32_t halfResImageSize = align( ( width / 2) * ( height / 2) * 4, 128 );

	// calculate arena size
	uint32_t arenaSize = 
		halfResImageSize +			// half resolution color buffer copied by RSX into host ram
		( 320 * 180 * 8) * 2 +		// first bloom buffer + scratch area 
		( 160 * 90 * 8) +			// second bloom buffer
		( 80 * 45 * 8);				// third bloom buffer

	printf( "Allocating SPU Postprocessing arena size in host memory : %d ( ~ %dMB)\n", arenaSize, align( arenaSize, 1024 * 1024) / (1024*1024) );

	// align to 1MB
	arenaSize = align( arenaSize, 1024 * 1024);

	// Allocate and map XDR memory
	mpPostprocessArena = (uint8_t*)memalign( 1024*1024, arenaSize );
	int ret = cellGcmMapMainMemory( mpPostprocessArena, arenaSize, &mLocalToMainOffset);
	assert( ret == CELL_OK );

	// Allocate images inside the arena
	uint8_t* pData = mpPostprocessArena;
	
	// color buffer
	mLocalToMainImage = pData; 
	pData += halfResImageSize;

	// Get RSX offsets
	ret = cellGcmAddressToOffset( mLocalToMainImage, &mLocalToMainOffset);
	assert( ret == CELL_OK );

	// Setup stages
	memset( &mStages[0], 0, sizeof(mStages) );

	// we are assuming 1280 x 720 !
	assert( width == 1280 );
	assert( height == 720 );
	EdgePostProcessStage* pStage = &mStages[0];

	// allocate bloom sub-buffers
	uint8_t* sub0 = pData;				// 320x180x8
	uint8_t* sub1 = sub0 + 320*180*8;	// 160x90x8
	uint8_t* sub2 = sub1 + 160*90*8;	// 80x45x8
	uint8_t* scr = sub2 + 80*45*8;		// 320x180x8 (max) scratch area

	// Setup bloom effect chain
	EdgePostProcessStage* pAvgLuminanceStage = pStage;
	SetupBloomStart( pStage++, 640, 360, mLocalToMainImage, sub0 );					// (640x360) convert to FX16, capture bloom color and down-sample
	SetupBloomBlur( pStage++, 320, 180, sub0, scr, mBloomWeights, true );			// (320x180) horizontal blur
	SetupBloomBlur( pStage++, 320, 180,	scr, sub0, mBloomWeights, false, sub1 );	// (320x180) vertical blur and down-sample
	SetupBloomBlur( pStage++, 160, 90, sub1, scr, mBloomWeights, true );			// (160x90) horizontal blur
	SetupBloomBlur( pStage++, 160, 90, scr, sub1, mBloomWeights, false, sub2 );		// (160x90) vertical blur and down-sample
	//SetupBloomBlur( pStage++, 160, 90, sub1, scr, mBloomWeights, true );			// (160x90) horizontal blur
	//SetupBloomBlur( pStage++, 160, 90, scr, sub1, mBloomWeights, false );			// (160x90) vertical blur
	SetupBloomBlur( pStage++, 80, 45, sub2, scr, mBloomWeights, true );				// (80x45) horizontal blur
	SetupBloomBlur( pStage++, 80, 45, scr, sub2, mBloomWeights, false );			// (80x45) vertical blur
	SetupBloomBlur( pStage++, 80, 45, sub2, scr, mBloomWeights, true );				// (80x45) horizontal blur
	SetupBloomBlur( pStage++, 80, 45, scr, sub2, mBloomWeights, false );			// (80x45) vertical blur
	SetupBloomUpsampleCombine( pStage++, 160, 90, sub2, sub1, sub1);				// (160x90) up-sample and accumulate onto previous layer
	SetupBloomUpsampleCombine( pStage++, 320, 180, sub1, sub0, sub0, true);			// (320x180) up-sample, accumulate onto previous layer and convert to FP16

	// Count number of stages
	mNumStages = pStage - &mStages[0];
	assert( mNumStages <= kMaxStages );

	// Setup result texture
	memset( &mAccumulatorTexture, 0, sizeof(CellGcmTexture));
	mAccumulatorTexture.format = CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT | CELL_GCM_TEXTURE_LN;
	mAccumulatorTexture.mipmap = 1;
	mAccumulatorTexture.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
	mAccumulatorTexture.cubemap = CELL_GCM_FALSE;
	mAccumulatorTexture.remap = 
		CELL_GCM_TEXTURE_REMAP_REMAP << 14 |
		CELL_GCM_TEXTURE_REMAP_REMAP << 12 |
		CELL_GCM_TEXTURE_REMAP_REMAP << 10 |
		CELL_GCM_TEXTURE_REMAP_REMAP << 8 |
		CELL_GCM_TEXTURE_REMAP_FROM_B << 6 |
		CELL_GCM_TEXTURE_REMAP_FROM_G << 4 |
		CELL_GCM_TEXTURE_REMAP_FROM_R << 2 |
		CELL_GCM_TEXTURE_REMAP_FROM_A;
	mAccumulatorTexture.width = 320;
	mAccumulatorTexture.height = 180;
	mAccumulatorTexture.depth = 1;
	mAccumulatorTexture.location = CELL_GCM_LOCATION_MAIN;
	mAccumulatorTexture.pitch = 320 * 8;

	// Get RSX offsets
	ret = cellGcmAddressToOffset( sub0, &mAccumulatorTexture.offset);
	assert( ret == CELL_OK );

	// Set some default parameters values
	mPostParams.m_bloomStrength = 1.1f;
	mPostParams.m_exposureLevel = .8f;
	mPostParams.m_minLuminance = .25f;
	mPostParams.m_maxLuminance = .7f;
	mPostParams.m_middleGrey = .18f;
	mPostParams.m_whitePoint = 3.f;
	mPostParams.m_numAvgLuminanceTiles = pAvgLuminanceStage->numTileX * pAvgLuminanceStage->numTileY;

	// TODO: should init to sensible values ?
	mPostParams.m_avgAndMaxLuminance = vec_splats( .0f );
	mPostParams.m_prevAvgAndMaxLuminance = vec_splats( .0f );

	// Set target frame rate of profiler
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

	// selected editable tweak
	mCurrentTweak = 0;

	// reset workload
	mWorkload.stage = 0xffff;

	// set RSX user handler
	cellGcmSetUserHandler( RsxUserHandler );

	// setup notify area
	g_notify_area = (uint8_t*)memalign( 1024*1024, 1024 );
	ret = cellGcmMapEaIoAddressWithFlags( g_notify_area, CELL_GCM_NOTIFY_IO_ADDRESS_BASE, 1024*1024, CELL_GCM_IOMAP_FLAG_STRICT_ORDERING);
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

void SampleApp::SwapSpuPostprocessBuffers()
{
	uint32_t frameIndex = ((FWCellGCMWindow*)FWWindow::getWindow())->getFrameIndex();

	// RSX wait for spu post processing to end
	cellGcmSetWaitLabel( kEdgePostLabelIndex, mPostLabelValue);
	ProfileBars::AddRsxMarker(0xffffffff);

	// setup default states
	cellGcmSetBlendEnable( CELL_GCM_FALSE);
	cellGcmSetDepthTestEnable( CELL_GCM_FALSE);
	cellGcmSetCullFaceEnable( CELL_GCM_FALSE);
	cellGcmSetDepthMask( CELL_GCM_FALSE);
	cellGcmSetColorMask( CELL_GCM_COLOR_MASK_R | CELL_GCM_COLOR_MASK_G | CELL_GCM_COLOR_MASK_B | CELL_GCM_COLOR_MASK_A);


	//////////////////////////////////////////////////////////////////////////
	/// Move SPU results into VRAM and composite with original frame

	// Setup current offset
	mOriginalTexture.offset = mColorOffsets[ frameIndex ^ 1];

	// final tone-mapping step on RSX
	Tonemap( mOriginalTexture, mAccumulatorTexture );
	ProfileBars::AddRsxMarker(0x00ff0000);
	
	//////////////////////////////////////////////////////////////////////////
	/// Move current frame into Main Memory for SPU input

	// Downsample Luv color buffer ( VRAM -> VRAM )
	LuvDownsample( mRenderTarget, mHalfResVram );
	ProfileBars::AddRsxMarker(0x00ffff00);

	// Move downsampled Luv to Main Memory ( ~ 0.11 ms )
	cellGcmSetTransferImage( CELL_GCM_TRANSFER_LOCAL_TO_MAIN,
		mLocalToMainOffset, mPostWidth * 2, 0, 0,
		mHalfResVram.colorOffset[0], mHalfResVram.colorPitch[0], 0, 0,
		mPostWidth / 2, mPostHeight / 2, 4 );
	ProfileBars::AddRsxMarker(0xff00ff00);

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

	// call rsx callback
	cellGcmSetUserCommand( (const uint32_t)this );
	SetViewport( 1280, 720 );
}