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
	void 	createJobs();
	void 	createJobChain();
	void	createShaders();
	void	createTextures();
	void	createQuad();

	static void userHandler(uint32_t cause);

	void 	onInterrupt();

	void	renderBackground();
	void	renderToSourceTexture();
	void	renderJobChainTrigger();
	void	renderStallForResults(uint32_t labelIndex);

	void 	waitForJobChainFinished();

private:
	CellSpurs					mSpurs;
	CellSpursEventFlag			mSpursEventFlag;

	CellSpursJobGuard			*mGuard;
	uint64_t					*mCommandList;
	CellSpursJobChain 			*mJobChain;
	CellSpursJob256				*mJobCompress;
	CellSpursJob256				*mJobDecompress;
	CellSpursJob256				*mJobEnd;

	FWTimeVal					mLastTime;
	FWInputFilter				*mpLeft, *mpRight;
	FWInputFilter				*mpSquare, *mpTriangle, *mpCross;
	FWTimeVal					mLastUpdate;

	CGprogram					mQuadVertexProgram;
	CGprogram					mQuadFragmentProgram;
	CGprogram					mClearFragmentProgram;
	void						*mQuadVertexProgramUCode;
	void						*mQuadFragmentProgramUCode;
	void						*mClearFragmentProgramUCode;
	uint32_t					mQuadFragmentProgramOffset;
	uint32_t					mClearFragmentProgramOffset;

	uint32_t					mTexDim;
	void						*mOrigTextureData;
	CellGcmTexture				mOrigTexture;

	void						*mSourceTextureData;
	CellGcmTexture				mSourceTexture;
	CellGcmSurface				mSourceSurface;
	bool						mIsRotating;
	float						mAngle;
	float 						mScale;
	uint32_t					mDxtMode;
	bool						mShowDebug;
	float						mShowOffset;

	void						*mDestTextureData;
	CellGcmTexture				mDestTexture;

	void						*mDebugTextureData;
	CellGcmTexture				mDebugTexture;

	float 						*mVertexData;
	uint32_t					mVertexDataOffset;
};

#endif // __SAMPLEAPP_H__
