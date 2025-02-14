/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <sys/sys_time.h>
#include <sys/time_util.h>
#include <cell/gcm.h>
#include "vsync.h"

using namespace cell::Gcm;

// static members
VSync::DisplaySyncMethod	VSync::mSyncMethod = kDisplaySyncMethodVSync;
volatile uint32_t			VSync::mVBlankCounter = 0;
volatile uint32_t			VSync::mVBlankLockCount = 2;
uint64_t					VSync::mVBlankTime = 0;


//	VBlank handler
void VSync::VBlankHandler( uint32_t )
{
	if( mSyncMethod == kDisplaySyncMethodVSync )
	{
		cellGcmSetFlipMode( CELL_GCM_DISPLAY_VSYNC );
	} else {
		cellGcmSetFlipMode( CELL_GCM_DISPLAY_HSYNC );
	}

	if ( --mVBlankCounter == 0)
	{
		// Set the vblank lock semaphore to release the RSX!
		*cellGcmGetLabelAddress( kVBlankLockLabelIndex ) = 0x1;
		mVBlankCounter = mVBlankLockCount;
	}

	SYS_TIMEBASE_GET(mVBlankTime);
}


//	Called before every Present
void VSync::EndFrame()
{
	// Make RSX wait on the vblank lock semaphore!
	if(( mSyncMethod != kDisplaySyncMethodHSync ) && ( mSyncMethod != kDisplaySyncMethodVSync ) )
		cellGcmSetWaitLabel( kVBlankLockLabelIndex, 0x1 );

	// RSX clears the vblank lock semaphore itself!
	cellGcmSetWriteCommandLabel( kVBlankLockLabelIndex, 0x0);

	if ( mSyncMethod == kDisplaySyncMethodLocked30hz)
	{
		mVBlankLockCount = 2;
	} 
	else 
	{
		mVBlankLockCount = 1;
	}
}

//	Initialisation dynamic hsync<->vsync
void VSync::Initialize()
{
	cellGcmSetVBlankHandler( VBlankHandler);
	cellGcmSetFlipMode( CELL_GCM_DISPLAY_HSYNC );
	mVBlankCounter = mVBlankLockCount;
}

//	Set current VSync method
void VSync::SetDisplaySyncMethod( DisplaySyncMethod method )
{
	mSyncMethod = method;
}

//	Get current VSync method
VSync::DisplaySyncMethod VSync::GetDisplaySyncMethod()
{
	return mSyncMethod;
}

uint64_t VSync::GetVSyncTime()
{
	return mVBlankTime;
}
