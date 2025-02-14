/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2007 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#ifndef VSYNCH_H
#define VSYNCH_H


class VSync
{
public:

	static const uint32_t kVBlankLockLabelIndex = 200;

	enum DisplaySyncMethod
	{
		kDisplaySyncMethodVSync,				//	Always sync to vertical retrace
		kDisplaySyncMethodHSync,				//	Always sync to horizontal retrace
		kDisplaySyncMethodLocked30hz,			//	Try to be as close as possible to 30hz
		kDisplaySyncMethodLocked60hz,			//	Try to be as close as possible to 60hz
	};

	static void							Initialize();
	static void							EndFrame();
	static void							SetDisplaySyncMethod( DisplaySyncMethod method );
	static DisplaySyncMethod			GetDisplaySyncMethod();
	uint64_t							GetVSyncTime();

private:

	static DisplaySyncMethod			mSyncMethod;
	static volatile uint32_t			mVBlankCounter;
	static volatile uint32_t			mVBlankLockCount;
	static uint64_t						mVBlankTime;

	static void VBlankHandler( uint32_t );
};

#endif

