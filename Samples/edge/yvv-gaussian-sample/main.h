/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */


#ifndef __SAMPLEAPP_H__
#define __SAMPLEAPP_H__

#include "gcm/FWGCMCamControlApplication.h"
#include "FWInput.h"
#include "FWTime.h"
#include "cellutil.h"


class SampleApp : public FWGCMCamApplication
{
public:
					SampleApp();
					~SampleApp();
	virtual bool	onInit(int argc, char **ppArgv);
	virtual void	onRender();
	virtual bool	onUpdate();
	virtual	void	onShutdown();

private:
	
	bool	initTexture( CellGcmTexture& texture, const char* textureName);
	void	initTextures();

	// spu postprocessing
	void	InitSpuPostprocessing( uint32_t width, uint32_t height );
	void	StartSpuPostprocessing();
	void	SwapSpuPostprocessBuffers();

	void	SetupYvVTask(uint8_t* destEa, uint8_t* srcEa, uint32_t imagePitch, uint32_t imageHeight, uint32_t mode);

private:

	FWTimeVal					mLastTime;
	FWInputFilter				*mpSquare, *mpCross, *mpStart, *mpTriangle, *mpCircle;
	FWInputFilter				*mpUp, *mpDown, *mpLeft, *mpRight;
	FWInputFilter				*mpInputX0, *mpInputY0, *mpInputX1, *mpInputY1;

	// Blur target?
	bool						mUseBlur;

	// Current render target
	CellGcmSurface				mFrameTarget;

	// images
	CellGcmTexture*				mTextures;
	uint32_t					mCurrentTexture;
	uint32_t					mTextureCount;
	
	// spurs
	CellSpurs					*mSpurs;	//in RSX mapped memory.

	// spu postprocessing data
	static const uint32_t		kTransferLabelIndex = 253;
	static const uint32_t		kTaskSyncLabel = 254;
	static const uint32_t		kTestLabel = 252;
	uint8_t*					mPostprocessArena;	// postprocessing work area ( pixel buffers ... )
	uint32_t					mLocalToMainOffset;	// RSX writes source image here (RSX offset )
	uint32_t					mPostWidth; 
	uint32_t					mPostHeight;
	uint32_t					mTaskSyncLabelValue;
	friend void RsxUserHandler( const uint32_t cause);
};

#endif // __SAMPLEAPP_H__
