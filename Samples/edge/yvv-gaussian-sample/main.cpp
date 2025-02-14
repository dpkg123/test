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
#include "../common/vsync.h"
#include "main.h"



/*
 * Sample application for Young-van-Vliet recursive gaussian blur.
 * First of all, there is nothing really special happening in this part of the sample, so feel free to skip this file.
 * We just set up spurs, load a bunch of textures and then for every frame, have RSX copy the 
 * selected texture down to XDR, have the SPU task process it and then have RSX copy it into the back buffer.
 *
 * USAGE:
 * -The program will go through the APP_HOME/assets directory and load all 720p textures. Other texture will be ignored.
 * -You can switch between the active textures using the left/right buttons on the d-pad.
 *
 * Have fun!
 */


/////
#define SPURS_SPU_NUM					5
#define SPU_THREAD_GROUP_PRIORITY		250
#define EVENT_FLAG_BIT					(0U)					// bit used by job_send_event (between 0 and 15)
#define EVENT_FLAG_BITS					(1U<<(EVENT_FLAG_BIT))	// bit mask for job_send_event


using namespace cell;
using namespace cell::Gcm;

// instantiate the class
SampleApp app;


SampleApp::SampleApp() :
	mUseBlur(true),
	mCurrentTexture(0)
{
}

SampleApp::~SampleApp()
{
}


bool SampleApp::initTexture( CellGcmTexture& texture, const char* textureName)
{
	CellGtfTextureAttribute attribute;
	uint32_t size;
	int ret;

	ret = cellGtfReadTextureAttribute( textureName, 0, &attribute );
	assert( ret == 0 );

	// check if textures are usable
	if( (0 != ret) || 
		(1280 != attribute.tex.width) ||
		(720 != attribute.tex.height) ||
		(CELL_GCM_TEXTURE_A8R8G8B8 != CELL_GCM_TEXTURE_A8R8G8B8&attribute.tex.format)) 
		return false;

	ret = cellGtfCalculateMemorySize( &attribute, &size);
	assert( ret == 0 );
	if(0 != ret) return false;

	void* pTextureData = (void*)cellGcmUtilAllocateLocalMemory( size, 128);
	ret = cellGtfLoad( textureName, 0, &texture, CELL_GCM_LOCATION_LOCAL, pTextureData );
	assert( ret == 0 );
	if(0 != ret) return false;

	return true;
}


void SampleApp::initTextures()
{

	const char* asset_path="/app_home/assets/";

	CellFsErrno ret;
	int dir;
	ret=cellFsOpendir("/app_home/assets/",&dir);
	assert(CELL_FS_SUCCEEDED == ret);

	CellFsDirent entry;
	uint64_t found;
	
	uint32_t textures_found=0;

	ret=cellFsReaddir(dir,&entry,&found);
	assert(CELL_FS_SUCCEEDED == ret);

	while(found /*&& textures_found < 1*/)
	{
		if(
			entry.d_namlen>5 && 
			(entry.d_name[entry.d_namlen-4] == '.') &&
			(entry.d_name[entry.d_namlen-3] == 'g') &&
			(entry.d_name[entry.d_namlen-2] == 't') &&
			(entry.d_name[entry.d_namlen-1] == 'f'))
		{
			++textures_found;
		}
		ret=cellFsReaddir(dir,&entry,&found);
		assert(CELL_FS_SUCCEEDED == ret);
	}

	assert(textures_found);

	mTextures = (CellGcmTexture*) malloc(textures_found*sizeof(CellGcmTexture));

	//not sure what the correct way to reset the descriptor is. Lseek doesn't seem to work.
	cellFsClosedir(dir);

	ret=cellFsOpendir(asset_path,&dir);
	assert(CELL_FS_SUCCEEDED == ret);

	ret=cellFsReaddir(dir,&entry,&found);
	assert(CELL_FS_SUCCEEDED == ret);

	const int prefix_len=strlen(asset_path);

	uint32_t i = 0;
	while(found)
	{
		if(
			i<textures_found &&
			entry.d_namlen>5 && 
			(entry.d_name[entry.d_namlen-4] == '.') &&
			(entry.d_name[entry.d_namlen-3] == 'g') &&
			(entry.d_name[entry.d_namlen-2] == 't') &&
			(entry.d_name[entry.d_namlen-1] == 'f'))
		{
			//need to concatenate the filename and path
			char* tmp=new char[prefix_len+entry.d_namlen+1];
			memcpy(tmp,asset_path,prefix_len);
			memcpy(tmp+prefix_len,entry.d_name,entry.d_namlen);
			tmp[prefix_len+entry.d_namlen]='\0';
			if(initTexture(mTextures[i], tmp))
			{
				++i;
			}
			delete[] tmp;
		}
		ret=cellFsReaddir(dir,&entry,&found);
		assert(CELL_FS_SUCCEEDED == ret);
	}

	cellFsClosedir(dir);
	
	mTextureCount = i;
	mCurrentTexture = 0;
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
		mpInputX0->setDeadzone(0.f);
		mpInputY0 = pPad->bindFilter();
		mpInputY0->setChannel(FWInput::Channel_YAxis_0);
		mpInputY0->setGain(-0.5f);
		mpInputY0->setDeadzone(0.08f);
		mpInputX1 = pPad->bindFilter();
		mpInputX1->setChannel(FWInput::Channel_XAxis_1);
		mpInputX1->setGain(1.f);
		mpInputX1->setDeadzone(0.0008f);
		mpInputY1 = pPad->bindFilter();
		mpInputY1->setChannel(FWInput::Channel_YAxis_1);
		mpInputY1->setGain(-0.5f);
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

	mSpurs = (CellSpurs*)memalign(128,sizeof(CellSpurs));

	// Initialize SPURS using our attributes structure.
	ret = cellSpursInitializeWithAttribute(mSpurs, &spursAttributes);
	if (ret != CELL_OK){
		printf("cellSpursInitializeWithAttribute failed : 0x%x\n", ret);
		return false;
	}

	// Init textures
	initTextures();

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
	
	// Init post-processing stuff
	InitSpuPostprocessing(1280, 768);
	return true;
}

bool SampleApp::onUpdate()
{
	// mode...
	static int delay = 6;
	if ( delay == 0)
	{
		if ( mpTriangle->getBoolTrue() )
		{
			mUseBlur = (mUseBlur+1)%2;
		}

		if ( mpRight->getBoolTrue() )
		{
			mCurrentTexture = (mCurrentTexture+1)%mTextureCount;
		}

		if ( mpLeft->getBoolTrue() )
		{
			if(0 == mCurrentTexture)
			{
				mCurrentTexture = mTextureCount-1;
			}
			else
			{
				mCurrentTexture = (mCurrentTexture-1)%mTextureCount;
			}
		}	
	} else --delay;

	return FWGCMCamApplication::onUpdate();
}


void SampleApp::onRender()
{
	// wait for vsync
	static uint32_t finishReference = 0;
	cellGcmFinish(finishReference++);

	// Get current target configuration
	FWCellGCMWindow* pWindow = (FWCellGCMWindow*)FWWindow::getWindow();
	uint32_t frameIndex = pWindow->getFrameIndex();
	mFrameTarget.colorOffset[0] = pWindow->getFrameOffset(frameIndex );
	mFrameTarget.colorPitch[0]	= pWindow->getFramePitch(frameIndex );
	mFrameTarget.depthOffset = pWindow->getFrameOffset(2 );
	mFrameTarget.depthPitch = pWindow->getFramePitch(2 );

	// render the selected image
	cellGcmSetColorMask( CELL_GCM_COLOR_MASK_R | CELL_GCM_COLOR_MASK_G | CELL_GCM_COLOR_MASK_B | CELL_GCM_COLOR_MASK_A );
	cellGcmSetColorMaskMrt( 0 );
	cellGcmSetBlendEnableMrt( CELL_GCM_FALSE, CELL_GCM_FALSE, CELL_GCM_FALSE);

	// post-processing
	SwapSpuPostprocessBuffers();
	

	VSync::EndFrame();
}

void SampleApp::onShutdown()
{
	int ret;
	ret = cellSpursFinalize(mSpurs);
	free(mSpurs);
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
