/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/


#ifndef __SAMPLEAPP_H__
#define __SAMPLEAPP_H__

#include "spu/job_geom_interface.h"
#include "spu/postParams.h"
#include "spu/edgepostjobheader.h"
#include "edge/geom/edgegeom_structs.h"
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
	
	struct Shader
	{
		CGprogram	mCgProgram;
		void*		mUCode;
		uint32_t	mUCodeSize;
		uint32_t	mUCodeOffset;
	};
	
	void	initShader( Shader& shader, void* pData );
	void	initTexture( CellGcmTexture& texture, const char* textureName);
	void	initShaders();
	void	initTextures();
	void	initRenderTargets();
	void	createQuad();
	void	CreateGeomJob(JobGeom256* job, CellGcmContextData *ctx, EdgeGeomPpuConfigInfo *info, void *skinningMatrices, uint32_t elephantIndex) const;
	void	drawSkybox();
	void	renderGround();
	void	RenderTweakables();
	void	SetTweakColor( int inx );

	// spu postprocessing
	void	InitSpuPostprocessing( uint32_t width, uint32_t height );
	void	StartSpuPostprocessing();
	void	SwapSpuPostprocessBuffers();
	void	CleanSpuPostprocessing();
	void	InitVRamBuffers( uint32_t width, uint32_t height );

	// spu stages
	void	SetupStage( EdgePostProcessStage* pStage, const EdgePostImage* pImages, uint32_t numImages, EdgePostJobHeader* pJobCode	);
	void	SetupBloomStart( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa );
	void	SetupBloomBlur( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa, float* weights, bool horizontal, void* downsampleEa = 0);
	void	SetupBloomUpsampleCombine( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* src0Ea, void* src1Ea, void* dstEa, bool bConvertToFp16 = false);

	// gpu stages
	void	Tonemap( const CellGcmTexture& original, const CellGcmTexture& accumulator );
	void	LuvDownsample( const CellGcmSurface& src, const CellGcmSurface& dst );
	
	// utils
	void	SetViewport( uint32_t w, uint32_t h );
	void	SetTarget( const CellGcmSurface& dst );

private:

	FWTimeVal					mLastTime;
	FWInputFilter				*mpSquare, *mpCross, *mpStart, *mpTriangle, *mpCircle;
	FWInputFilter				*mpUp, *mpDown, *mpLeft, *mpRight;
	FWInputFilter				*mpInputX0, *mpInputY0, *mpInputX1, *mpInputY1;
	FWTimeVal					mLastUpdate;
	float						mTurnRate;
	float						mMoveRate;

	void						*mTextures[2];
	void						*mCommandBuffer;
	uint32_t					mCommandBufferOffset;
	bool						mAnimPause;

	// Current render target
	CellGcmSurface				mFrameTarget;

	// offscreen mrt targets
	CellGcmSurface				mRenderTarget;
	uint32_t					mColorOffsets[2];

	//	scene shaders
	Shader						mSkyVp;
	Shader						mSkyFp;
	Shader						mSceneVp;
	Shader						mSceneFp;

	// shaders for postprocess work done on RSX
	Shader						mResampleVp;
	Shader						mTonemapCompositeFp;
	Shader						mLuvDownsampleFp;

	// textures
	CellGcmTexture				mSkyCubeTexture;
	CellGcmTexture				mDesertDiffuse;
	CellGcmTexture				mDesertNormal;

	// mixed render targets
	CellGcmSurface				mHalfResVram;
	CellGcmSurface				mColorSurface[2];

	// Texture used for the final composition done on RSX
	CellGcmTexture				mAccumulatorTexture;
	CellGcmTexture				mOriginalTexture;

	// edgegeom stuff
	uint32_t					mEdgeJobSize;
	void						*mEdgeJobStart;
	uint32_t					mSendEventJobSize;
	void						*mSendEventJobStart;
	uint8_t						*mSharedOutputBuffer;
	uint8_t						*mRingOutputBuffer;
	EdgeGeomOutputBufferInfo	mOutputBufferInfo __attribute__((__aligned__(128)));
	EdgeGeomViewportInfo		mViewportInfo __attribute__((__aligned__(128)));
	EdgeGeomLocalToWorldMatrix	mLocalToWorldMatrix;
	
	// full screen quad vertices
	float*						mQuadVertexData;
	uint32_t					mQuadVertexDataOffset;

	// spurs
	CellSpurs					mSpurs;
	CellSpursEventFlag			mSpursEventFlag;

	// spu postprocessing data
	static const uint32_t		kEdgePostLabelIndex = 254; // RSX label used for synchronization between spu and rsx
	static const uint32_t		kTransferLabelIndex = 253;
	static const uint32_t		kMaxStages = 256;
	static const int			kNumParameters = 6; // number of tweakable
	uint8_t*					mpPostprocessArena;	// postprocessing work area ( pixel buffers ... )
	uint8_t*					mLocalToMainImage; // RSX writes source image here
	uint32_t					mLocalToMainOffset;	// RSX writes source image here (RSX offset )
	uint32_t					mPostWidth; 
	uint32_t					mPostHeight;
	uint32_t					mPostLabelValue;
	uint32_t					mNumStages;	// number of postprocessing stages
	EdgePostProcessStage		mStages[ kMaxStages]; // array of stages
	EdgePostWorkload			mWorkload; // workload descriptor
	PostParams					mPostParams; // postprocessing parameters
	int							mCurrentTweak; // currently edited parameter
	float						mAvgLuminance;
	float						mMaxLuminance;

	// gauss weights
	static float				mBloomWeights[4];	// = { 0.3125f, 0.234375f, 0.09375f, 0.015625f};

	// GPU interrupt handler
	friend void RsxUserHandler( const uint32_t cause);
};

#endif // __SAMPLEAPP_H__
