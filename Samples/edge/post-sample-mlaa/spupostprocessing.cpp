/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <cell/cell_fs.h>
#include <cell/gcm.h>
#include <cell/spurs.h>
#include <cell/spurs/task.h>
#include <cell/sysmodule.h>
#include <sys/spu_initialize.h>
#include <ppu_intrinsics.h>
#include <spu_printf.h>
#include <cell/dbgfont.h>
#include <sys/sys_time.h>
#include <sys/time_util.h>
#include <cell/sync/barrier.h>
#include <edge/post/edgepost_mlaa_handler_ppu.h>
#include "main.h"
#include "gcm/FWCellGCMWindow.h"
#include "gcmutil.h"

// MLAA integration in 150 lines of code...

using namespace cell;
using namespace cell::Gcm;

//////////////////////////////////////////////////////
/// NUMBER OF SPUS EDGEPOST CAN USE
#define SPU_NUM					5
#define SPURS_PRIORITY			12

extern uint8_t g_DebugFlags;
EdgePostMlaaContext g_MlaaContext;					// This structure contains all the information needed by MLAA.
static uint32_t g_InterruptRequestedValue = 0;		// Count number of requested interrupts
static uint32_t g_InterruptAcknowledgedValue = 0;	// Count number of acknowledged interrupts
static const uint32_t kRsxInterruptLabel = 252;		// The label we wait on for rsx interrupt synchronization

static inline uint32_t align( const uint32_t value, uint32_t alignment)
{
	return ( value + (alignment - 1)) & ~(alignment - 1);
}

void SampleApp::StartSpuPostprocessing()
{

	// we write the parameter structs of the MLAA tasks,
	// thus we need to make sure they are no longer needed.
	edgePostMlaaWait(&g_MlaaContext);

	uint8_t mode = 0;	// Enable MLAA and debug features
	mode |= mMlaaEnabled?EDGE_POST_MLAA_MODE_ENABLED:0;
	mode |= mMlaaShowEdges?EDGE_POST_MLAA_MODE_SHOW_EDGES:0;

	// now write the structs. This does not yet start the SPUs.
	edgePostMlaaPrepareWithRelativeThreshold(
		&g_MlaaContext,
		mPostprocessArena,
		mPostprocessArena,
		mTextureWidth,
		mTextureHeight,
		sizeof(uint32_t) * mTextureWidth,
		mMlaaRelativeEdgeDetection?mMlaaThresholdBase:mMlaaAbsoluteThreshold,
		mMlaaRelativeEdgeDetection?mMlaaThresholdFactor:0,
		mode,
		++mTaskSyncLabelValue);

	// We do not want to trigger an interrupt while a previous interrupt is still 
	// pending/executing as this will likely confuse the RSX making it call the
	// interrupt function with the wrong cause word.
	// Note that this is not strictly needed within this sample as we do not use
	// the interrupt function more than once within a frame.
	cellGcmSetWaitLabel(kRsxInterruptLabel, g_InterruptRequestedValue);
	g_InterruptRequestedValue++;

	// we want to start the tasks once RSX has successfully copied the
	// texture to XDR. We do this by using the user command callback
	// and then kicking the tasks form there.
	cellGcmSetUserCommand(EDGE_POST_MLAA_USER_COMMAND);
	
	// kick rsx, so we do not stall.
	cellGcmFlush();

}

// This is the function called by the RSX user callback.
void RsxDispatcherCallback(uint32_t cause)
{
	switch(cause)
	{
	// We only use this for MLAA now. See the post-sample for a 
	// combination of MLAA and regular EDGE Post.
	case EDGE_POST_MLAA_USER_COMMAND:
		// Note: You may want to use the RSX notification API to
		// make sure the 2D transfer is finished.
		edgePostMlaaKickTasks(&g_MlaaContext);
		break;
	default:
		assert(!"Unknown rsx interrupt parameter.");
	}

	// release the RSX interrupt label
	g_InterruptAcknowledgedValue++;
	*cellGcmGetLabelAddress(kRsxInterruptLabel) = g_InterruptAcknowledgedValue;
}

void SampleApp::InitSpuPostprocessing( uint32_t width, uint32_t height )
{
	// Allocate XDR memory
	const uint32_t imageSize = align( width * height * 4, 1024*1024 );
	printf( "Allocating SPU Postprocessing space in host memory : %d\n", imageSize);

	// Allocate and map XDR memory
	mPostprocessArena = memalign( 1024*1024, imageSize );
	int ret = cellGcmMapMainMemory( mPostprocessArena, imageSize, &mLocalToMainOffset);
	(void)ret;
	assert(ret == CELL_OK);

	// if you have a texture larger then 720p, you can still use TargetManager to read it, by using the address printed here.
	printf("SPU buffer: 0x%08X\n",(uint32_t)mPostprocessArena);
	
	uint8_t priorities[8] = {SPURS_PRIORITY,SPURS_PRIORITY,SPURS_PRIORITY,SPURS_PRIORITY,SPURS_PRIORITY,SPURS_PRIORITY,SPURS_PRIORITY,SPURS_PRIORITY};


	const uint32_t mlaaHandlerMemBlockSize = EDGE_POST_MLAA_HANDLER_BASE_BUFFER_SIZE + EDGE_POST_MLAA_HANDLER_SPU_BUFFER_SIZE(SPU_NUM);
	mMlaaHandlerMemBlock = memalign(EDGE_POST_MLAA_HANDLER_BUFFER_ALIGN, mlaaHandlerMemBlockSize);

	int handler_ok=edgePostMlaaInitializeContext(
		&g_MlaaContext,
		SPU_NUM,
		mSpurs,
		&priorities[0],
		kTaskSyncLabel,
		mMlaaHandlerMemBlock,
		mlaaHandlerMemBlockSize);
	(void)handler_ok;
	assert(CELL_OK == handler_ok);

	// We use an RSX->PPU interrupt to wake up the tasks.
	// This is a valid way of doing it if you are using MLAA only a few
	// times per frame. Otherwise, you may have to do something more sophisticated,
	// like using the SPURS workload flags to wake up a workload that wakes up your
	// tasks.
	cellGcmSetUserHandler(RsxDispatcherCallback);

	*cellGcmGetLabelAddress(kTaskSyncLabel) = mTaskSyncLabelValue = 0;
	*cellGcmGetLabelAddress(kRsxInterruptLabel) = 0;
}

void SampleApp::ShutDownSpuProcessing()
{
	edgePostMlaaDestroyContext(&g_MlaaContext);
	free(mMlaaHandlerMemBlock);
	free(mPostprocessArena);
}

#define MIN(a,b)    (((a) < (b)) ? (a) : (b))

void SampleApp::SwapSpuPostprocessBuffers()
{
	uint32_t kWidth = 1280;
	uint32_t kHeight = 720;

	//// RSX wait for spu post processing to end
	cellGcmSetWaitLabel( kTaskSyncLabel, mTaskSyncLabelValue);

	//////////////////////////////////////////////////////////////////////////
	/// Move SPU results into VRAM
	cellGcmSetTransferImage( CELL_GCM_TRANSFER_MAIN_TO_LOCAL,
		mFrameTarget.colorOffset[0], mFrameTarget.colorPitch[0], 0, 0,
		mLocalToMainOffset, mTextureWidth * 4, 0, 0,
		MIN(kWidth,mTextureWidth), MIN(kHeight,mTextureHeight), 4 );

	//////////////////////////////////////////////////////////////////////////
	/// Move current texture into host memory
	cellGcmSetTransferImage( CELL_GCM_TRANSFER_LOCAL_TO_MAIN,
		mLocalToMainOffset, mTextureWidth * 4, 0, 0,
		mTextures[mCurrentTexture].offset, mTextures[mCurrentTexture].pitch,
		0, 0,
		mTextures[mCurrentTexture].width, mTextures[mCurrentTexture].height, 4 );
	
	StartSpuPostprocessing();
}
