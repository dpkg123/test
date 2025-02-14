/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */


#ifndef __SAMPLEAPP_H__
#define __SAMPLEAPP_H__

#include "spu/job_geom_interface.h"

#include "gcm/FWGCMCamControlApplication.h"
#include "FWInput.h"
#include "FWTime.h"
#include "cellutil.h"
#include "edge/geom/edgegeom_structs.h"
#include "edge/anim/edgeanim_structs_ps3.h"

// .edge binary file header
struct FileHeader
{
	uint32_t    numPpuConfigInfos;
	uint32_t    offsetToPpuConfigInfos;
	uint32_t    numPatches;
	uint32_t    offsetToPatches;
	int32_t     edgeSdkVersion;
	int32_t     cellSdkVersion;
};

// state for a single character
struct CharacterState
{
	EdgeAnimJointTransform	rootJoint;
	EdgeAnimBlendBranch		blendTreeBranches[8];
	EdgeAnimBlendLeaf		blendTreeLeaves[8];
	float*					skinningMatrices;
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

    void            runAnimJobs();
    void            runGeomJobs();
    void            runCharacterJobs();

private:
	void initShader();
	void CreateAnimJob(CellSpursJob256* jobAnim, void* outputMatrices, const struct EdgeAnimBlendBranch* blendBranches, uint32_t numBlendBranches, const struct EdgeAnimBlendLeaf* blendLeaves, uint32_t numBlendLeaves, const EdgeAnimJointTransform* rootJoint);
	void CreateGeomJob(JobGeom256* job, CellGcmContextData *ctx, EdgeGeomPpuConfigInfo *info, void *skinningMatrices) const;
	void CreateBlendTree(EdgeAnimBlendBranch* blendBranches, uint32_t& numBlendBranches, EdgeAnimBlendLeaf* blendLeaves, uint32_t& numBlendLeaves, float* evalTimes, float blend, uint32_t curState);

private:
	FWTimeVal					mLastTime;
	FWInputFilter				*mpSquare, *mpCross, *mpStart, *mpTriangle, *mpCircle;
	FWInputFilter				*mpUp, *mpDown;
	FWInputFilter				*mpInputX0, *mpInputY0, *mpInputX1, *mpInputY1;
	FWTimeVal					mLastUpdate;
	float						mTurnRate;
	float						mMoveRate;

	CellSpurs					mSpurs;
	CellSpursEventFlag			mSpursEventFlag;

    uint8_t*                        mColorTex;
    uint32_t                        mColorTexSize;
    uint8_t*                        mNormalTex;
    uint32_t                        mNormalTexSize;
	void*							mTextures[2];

	CGparameter					mModelViewProj;
	CGprogram					mCGVertexProgram;				// CG binary program
	CGprogram					mCGFragmentProgram;				// CG binary program
	void						*mVertexProgramUCode;			// this is sysmem
	void						*mFragmentProgramUCode;			// this is vidmem
	uint32_t					mFragmentProgramOffset;

	// geom related
	FileHeader*						mSceneData;
	uint8_t						*mSharedOutputBuffer;
	uint8_t						*mRingOutputBuffer;
	EdgeGeomOutputBufferInfo	mOutputBufferInfo;		// FIXME: warning - must be 128-byte aligned for atomic access
	EdgeGeomViewportInfo		mViewportInfo;
	EdgeGeomLocalToWorldMatrix	mLocalToWorldMatrix;
	void						*mCommandBuffer;
	uint32_t					mCommandBufferOffset;

	// anim related
	EdgeAnimSkeleton*				mSkeleton;
	EdgeAnimAnimation*				mAnimations[8];
	EdgeAnimPpuContext				mEdgeAnimPpuContext;
	float							mEvalTime;

	CharacterState*					mCharacterStates;
	void*							mInvBindMatrices;
};

#endif // __SAMPLEAPP_H__
