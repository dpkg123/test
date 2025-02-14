/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/


#ifndef __SAMPLEAPP_H__
#define __SAMPLEAPP_H__

#include "spu/job_geom_interface.h"
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

	// spu postprocessing
	void	InitSpuPostprocessing( uint32_t width, uint32_t height );
	void	StartSpuPostprocessing();
	void	SwapSpuPostprocessBuffers();
	void	CleanSpuPostprocessing();

	// spu stages
	void	SetupStage(EdgePostProcessStage* pStage, const EdgePostImage* pImages, uint32_t numImages, EdgePostJobHeader* pJobCode	);
	void	SetupMotionBlur(EdgePostProcessStage* pStage, uint32_t width, uint32_t height, void* srcEa, void* motion, void* dstEa );

	// gpu composition
	void	Composite(const CellGcmSurface& dst, uint32_t texOffset, uint32_t w, uint32_t h);

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
	bool						mDrawOnScreenInfo;

	// motionblur method
	bool						mFullScreenMotionBlur;
	bool						mLastFrameFullScreenMotionBlur;

	// Current render target
	CellGcmSurface				mFrameTarget;

	// offscreen mrt targets
	CellGcmSurface				mMrtTarget;
	uint32_t					mMrtColorOffsets[2];

	//	Shaders
	Shader						mSkyVp;
	Shader						mSkyFp;
	Shader						mSceneVp;
	Shader						mSceneFp;
	Shader						mPostFp;
	Shader						mPostVp;

	//	textures
	CellGcmTexture				mSkyCubeTexture;
	CellGcmTexture				mDesertDiffuse;
	CellGcmTexture				mDesertNormal;

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
	static const int			kNumParameters = 11; // number of tweakable
	uint8_t*					mpPostprocessArena;	// postprocessing work area ( pixel buffers ... )
	uint8_t*					mLocalToMainImage; // RSX writes source image here
	uint8_t*					mDepthImage; // RSX writes depth here
	uint32_t					mLocalToMainOffset;	// RSX writes source image here (RSX offset )
	uint32_t					mDepthOffset; // RSX writes depth here (RSX offset )
	uint32_t					mPostWidth; 
	uint32_t					mPostHeight;
	uint32_t					mPostLabelValue;
	EdgePostProcessStage		mStages[2]; // array of stages
	EdgePostWorkload			mWorkload; // workload descriptor

	friend void RsxUserHandler( const uint32_t cause);
};

#endif // __SAMPLEAPP_H__
