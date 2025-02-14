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


#include "main.h"
#include "gcm/FWCellGCMWindow.h"
#include "gcmutil.h"

#include "spu/yvv-gaussian.h"

using namespace cell;
using namespace cell::Gcm;

//////////////////////////////////////////////////////
/// NUMBER OF SPU EDGEPOST CAN USE
#define SPU_NUM					5
#define SPURS_PRIORITY			12
#define EDGE_YVV_USER_COMMAND	0x50F7C0DE					// identifier used in GPU callback

extern char _binary_task_yvv_gaussian_spu_elf_start[];			// YvY task code 
extern char _binary_task_yvv_gaussian_spu_elf_size[];			// YvY task code size

CellSpursTaskset2* g_pTaskset = 0;
CellSpursTaskId g_TaskIds[SPU_NUM];
CellSpursTaskAttribute2 g_TaskAttributes[SPU_NUM];	// used to start the tasks.
CellSpursTaskArgument g_Arguments[SPU_NUM];			// these just contain pointers to the g_TaskParameters
YvvParameters g_TaskParameters[SPU_NUM]; //the actual parameters
static const CellSpursTaskLsPattern s_TaskLsPattern = { { 0x00000000, 0x00000000, 0x00000000, 0x00000001 } };	// last 2K for stack
CellSpursTaskSaveConfig g_SaveConfig[SPU_NUM];
CellSpursBarrier* g_Barrier = NULL;
int32_t g_DirectionLock[32] __attribute__((aligned(128))); //cache line for the direction lock

static inline uint32_t align( const uint32_t value, uint32_t alignment)
{
	return ( value + (alignment - 1)) & ~(alignment - 1);
}

void SampleApp::SetupYvVTask(uint8_t* destEa, uint8_t* srcEa, uint32_t imagePitch, uint32_t imageHeight, uint32_t mode)
{
	const float sigma = 50.f+100.f*mpInputX0->getFloatValue();
	const float sigma_ratio = mpInputX1->getFloatValue();
	const float sigma_h = (sigma_ratio < 0.f) ? ((1.f + sigma_ratio) * sigma) : sigma;
	const float sigma_v = (sigma_ratio > 0.f) ? ((1.f - sigma_ratio) * sigma) : sigma;

	for ( int t = 0; t < SPU_NUM; ++t )
	{
		g_TaskParameters[t].rsxLabelAddress         = 0;
		g_TaskParameters[t].taskCounterAddress      = NULL;
		g_TaskParameters[t].imageAddress            = (uint32_t)srcEa;
		g_TaskParameters[t].destAddress             = (uint32_t)destEa;
		g_TaskParameters[t].imageWidth              = imagePitch;
		g_TaskParameters[t].imageHeight             = imageHeight;		
		g_TaskParameters[t].imagePitch              = imagePitch;
		g_TaskParameters[t].barrierAddress          = (uint32_t)g_Barrier;
		g_TaskParameters[t].directionLockAddress    = (uint32_t)&g_DirectionLock[0];
		g_TaskParameters[t].sigmaH                  = sigma_h;
		g_TaskParameters[t].sigmaV                  = sigma_v;
		g_TaskParameters[t].mode                    = mode;
		g_TaskParameters[t].spuId                   = t;
		g_TaskParameters[t].spuCount                = SPU_NUM;		
	}
}

void SampleApp::StartSpuPostprocessing()
{
	SetupYvVTask(mPostprocessArena, mPostprocessArena, 1280, 720, mUseBlur?1:0);

	g_TaskParameters[0].rsxLabelAddress = (uint32_t)cellGcmGetLabelAddress(kTaskSyncLabel);
	g_TaskParameters[0].rsxLabelValue   = ++mTaskSyncLabelValue;
	
	__lwsync();

	// let RSX kick the tasks.
	cellGcmSetUserCommand(EDGE_YVV_USER_COMMAND);
	
	// kick rsx
	cellGcmFlush();

}

void RsxDispatcherCallback(uint32_t cause)
{
	switch(cause)
	{
	case EDGE_YVV_USER_COMMAND:
		for(uint32_t i = 0; i < SPU_NUM; ++i)
		{
			cellSpursSendSignal(g_pTaskset,g_TaskIds[i]);
		}
		break;
	}
}


void SampleApp::InitSpuPostprocessing( uint32_t width, uint32_t height )
{
	// Allocate XDR memory
	const uint32_t imageSize = align( width * height * 4, 1024*1024 );
	printf( "Allocating SPU Postprocessing space in host memory : %d\n", imageSize);

	// Allocate and map XDR memory
	mPostprocessArena = (uint8_t*)memalign( 1024*1024, imageSize );
	int ret = cellGcmMapMainMemory( mPostprocessArena, imageSize, &mLocalToMainOffset);
	assert( ret == CELL_OK );


	// Spurs priority
	uint8_t prios[8] = { 1, 0, 0, 0, 0, 0, 0, 0};
	for ( int j = 0; j < SPU_NUM; ++j)
		prios[j] = SPURS_PRIORITY;

	g_pTaskset = (CellSpursTaskset2*)memalign( CELL_SPURS_TASKSET2_ALIGN, CELL_SPURS_TASKSET2_SIZE);
	
	CellSpursTasksetAttribute2 attributeTaskset;
	cellSpursTasksetAttribute2Initialize( &attributeTaskset);	
	memcpy(attributeTaskset.priority,prios,sizeof(attributeTaskset.priority));
	attributeTaskset.maxContention = SPU_NUM;
	attributeTaskset.name = "EDGE YvY";

	ret = cellSpursCreateTaskset2( mSpurs, g_pTaskset, &attributeTaskset );
	assert( ret == CELL_OK );

	g_Barrier = (CellSpursBarrier*)memalign(CELL_SPURS_BARRIER_ALIGN, CELL_SPURS_BARRIER_SIZE);
	cellSpursBarrierInitialize(g_pTaskset,g_Barrier,SPU_NUM);

	// Create spurs tasks
	for ( int t = 0; t < SPU_NUM; ++t )
	{
		// context save area
		cellSpursTaskGetContextSaveAreaSize( &g_SaveConfig[t].sizeContext, &s_TaskLsPattern);
		void *context_save_buffer = memalign(128, g_SaveConfig[t].sizeContext);

		g_Arguments[t].u32[0]                  = (uint32_t)&g_TaskParameters[t];
		cellSpursTaskAttribute2Initialize( &g_TaskAttributes[t] );
		g_TaskAttributes[t].lsPattern   = s_TaskLsPattern;
		g_TaskAttributes[t].eaContext   = (uint64_t)context_save_buffer;
		g_TaskAttributes[t].sizeContext = g_SaveConfig[t].sizeContext;

		cellSpursCreateTask2(g_pTaskset, &g_TaskIds[t], _binary_task_yvv_gaussian_spu_elf_start, &g_Arguments[t],&g_TaskAttributes[t]);
		assert( ret == CELL_OK );
	}

	// initialize rsx label value
	volatile uint32_t *label = cellGcmGetLabelAddress( kTaskSyncLabel);
	*label = mTaskSyncLabelValue = 0;

	// We use an RSX->PPU interrupt to wake up the tasks.
	// This is a valid way of doing it if you are using the task only a few
	// times per frame. Otherwise, you may have to do something more sophisticated,
	// like using the SPURS workload flags to wake up a workload that wakes up your
	// tasks.
	cellGcmSetUserHandler(RsxDispatcherCallback);

	// initialize direction lock used during the transpose pass.
	g_DirectionLock[0] = 0;
}


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
		mLocalToMainOffset, kWidth * 4, 0, 0,
		kWidth, kHeight, 4 );

	//////////////////////////////////////////////////////////////////////////
	/// Move current texture into host memory
	cellGcmSetTransferImage( CELL_GCM_TRANSFER_LOCAL_TO_MAIN,
		mLocalToMainOffset, kWidth * 4, 0, 0,
		mTextures[mCurrentTexture].offset, mTextures[mCurrentTexture].pitch,
		0, 0,
		mTextures[mCurrentTexture].width, mTextures[mCurrentTexture].height, 4 );
	
	StartSpuPostprocessing();
}
