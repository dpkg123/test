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
	void	initMRTs();
	void	createQuad();
	void	CreateGeomJob(JobGeom256* job, CellGcmContextData *ctx, EdgeGeomPpuConfigInfo *info, 
		void *prevSkinningMatrices, void *curSkinningMatrices, uint32_t elephantIndex) const;
	void	drawSkybox( const Matrix4& mvp, const Matrix4& pmvp, const Matrix4& vp );
	void	renderGround( const Matrix4& oldViewProj );
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
	void	SetupDownsampleFloat( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa );
	void	SetupUpsample16( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa );
	void	SetupUpsampleFloat( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa );
	void	SetupHorizontalGauss( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa, float* weights );
	void	SetupHorizontalGaussFloat( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa, float* weights );
	void	SetupVerticalGauss( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa, float* weights );
	void	SetupVerticalGaussFloat( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa, float* weights );
	void	SetupBloomCaptureStage( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa );
	void	SetupNearFuzziness( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa );
	void	SetupFarFuzziness( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* nearEa, void* dstEa, void* maskEa );
	void	SetupDepthOfField( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* depthEa, void* dstEa );
	void	SetupMotionBlur( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* motion, void* dstEa );
	void	SetupAddSat( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* src0, void* src1, void* dstEa );
	void	SetupAddSatModulateBloom( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* src0, void* src1, void* dstEa, void* maskEa );
	void	SetupIlr( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* src, void* dstEa );
	void	SetupDownsample16( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa );
	void	SetupLogLuvDownsample16( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* dstEa );
	void	SetupAvgLuminance( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* src);
	void	SetupFinalCombine( EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* accumulator, void* dstEa );

	// gpu stages
	void	Tonemap( const CellGcmTexture& original, const CellGcmTexture& accumulator, const CellGcmTexture& mask );
	void	LuvDownsample( const CellGcmSurface& src, const CellGcmSurface& dst );
	void	DepthDownsample( const CellGcmSurface& src, const CellGcmSurface& dst );
	
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
	CellGcmSurface				mMrtTarget;
	uint32_t					mMrtColorOffsets[2];

	//	scene shaders
	Shader						mSkyVp;
	Shader						mSkyFp;
	Shader						mSceneVp;
	Shader						mSceneFp;

	// shaders for postprocess work done on RSX
	Shader						mResampleVp;
	Shader						mDepthdownsampleFp;
	Shader						mTonemapCompositeFp;
	Shader						mLuvDownsampleFp;

	// textures
	CellGcmTexture				mSkyCubeTexture;
	CellGcmTexture				mDesertDiffuse;
	CellGcmTexture				mDesertNormal;

	// mixed render targets
	CellGcmSurface				mHalfResVram;
	CellGcmSurface				mMotionAndDepthSurface;
	CellGcmSurface				mColorSurface[2];

	// Texture used for the final composition done on RSX
	CellGcmTexture				mMaskTexture;
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
	static const int			kNumParameters = 13; // number of tweakable
	uint8_t*					mpPostprocessArena;	// postprocessing work area ( pixel buffers ... )
	uint8_t*					mLocalToMainImage; // RSX writes source image here
	uint8_t*					mDepthImage; // RSX writes depth here
	uint32_t					mLocalToMainOffset;	// RSX writes source image here (RSX offset )
	uint32_t					mDepthOffset; // RSX writes depth here (RSX offset )
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
	static float				mDoubleWeights[4];	// = { 0.3125f * 2.f, 0.234375f * 2.f, 0.09375f * 2.f, 0.015625f * 2.f};

	// GPU interrupt handler
	friend void RsxUserHandler( const uint32_t cause);
};

#endif // __SAMPLEAPP_H__
