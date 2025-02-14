/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __SAMPLEAPP_H__
#define __SAMPLEAPP_H__

#include "gcm/FWGCMCamControlApplication.h"
#include "FWInput.h"
#include "FWTime.h"
#include "cellutil.h"

enum
{
	IN_SQUARE,
	IN_CROSS,
	IN_START,
	IN_TRIANGLE,
	IN_CIRCLE,
	IN_UP,
	IN_DOWN,
	IN_X0,
	IN_Y0,
	IN_X1,
	IN_Y1,
	IN_COUNT
};

static const FWInput::Channel sInputChannels[IN_COUNT] = 
{
	FWInput::Channel_Button_Square,
	FWInput::Channel_Button_Cross,
	FWInput::Channel_Button_Start,
	FWInput::Channel_Button_Triangle,
	FWInput::Channel_Button_Circle,
	FWInput::Channel_Button_Up,
	FWInput::Channel_Button_Down,
	FWInput::Channel_XAxis_0,
	FWInput::Channel_YAxis_0,
	FWInput::Channel_XAxis_1,
	FWInput::Channel_YAxis_1
}; 

class SampleApp : public FWGCMCamApplication
{
public:
					SampleApp();
					~SampleApp();
	virtual bool	onInit(int argc, char **ppArgv);
	virtual void	onRender();
	virtual bool	onUpdate();
	virtual	void	onShutdown();
	virtual void	onSize(const FWDisplayInfo & dispInfo);

private:
	void initShader();

private:
	FWTimeVal		mLastTime;
	FWInputFilter	*mInputs[IN_COUNT];
	FWTimeVal		mLastUpdate;
	float			mTurnRate;
	float			mMoveRate;

	CGparameter mModelViewProj;

	CGprogram mCGVertexProgram;				// CG binary program
	CGprogram mCGFragmentProgram;			// CG binary program

	void *mVertexProgramUCode;				// this is sysmem
	void *mFragmentProgramUCode;			// this is vidmem
	uint32_t mFragmentProgramOffset;

	CellSpurs mSpurs;
};


#endif // __SAMPLEAPP_H__
