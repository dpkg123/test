/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __SAMPLEAPP_H__
#define __SAMPLEAPP_H__

#include "FWInput.h"
#include "FWTime.h"

#include "psgl/FWGLCamControlApplication.h"

class SampleApp : public FWGLCamControlApplication
{
public:
                    SampleApp();
                    ~SampleApp() {};
    virtual bool    onInit(int argc, char **ppArgv);
    virtual void    onRender();
    virtual bool    onUpdate();
    virtual void    onShutdown();
    virtual void    onSize(const FWDisplayInfo & dispInfo);
    
    void            runAnimJobs();

private:
    void initShader();
    void drawSkeleton(const Vectormath::Aos::Transform3* pWorldMats, const short* jointParents, int count);
    void createTestBlendTree(struct EdgeAnimBlendBranch* blendBranches, uint32_t& numBlendBranches, struct EdgeAnimBlendLeaf* blendLeaves, uint32_t& numBlendLeaves, float* evalTimes, float blendIdle, float blendJog, float blendFinal);

#ifndef  _MSC_VER
    void createSendEventJob(CellSpursJob256* jobSendEvent, uint16_t bitsEventFlag);
    void createAnimJob(CellSpursJob256* jobAnim, Transform3* outputMatrices, const struct EdgeAnimBlendBranch* blendBranches, uint32_t numBlendBranches, const struct EdgeAnimBlendLeaf* blendLeaves, uint32_t numBlendLeaves, const struct EdgeAnimJointTransform* rootJoint);
#endif

private:
#ifndef  _MSC_VER
    CellSpurs*                      mSpurs;
    CellSpursEventFlag              mSpursEventFlag;
    sys_event_queue_t               mQueue;
    uint8_t                         mQueuePort;
#else
    /* PC only */
    uint8_t*						mScratchBufferPC;
#endif

    struct EdgeAnimPpuContext*		mEdgeAnimPpuContext;

    struct EdgeAnimSkeleton*        mSkeleton;
    struct EdgeAnimAnimation*       mAnimation[4];
    void*							mExternalSpuStorage;

    Transform3**					mWorldMatrices;
    float							mEvalTime;
    float							mWhichAnim;

    void*							mVertexBufferBase;
    void*							mVertexBufferCur;

	FWTimeVal						mLastTime;
    unsigned int					mFrameCount;
};

#endif // __SAMPLEAPP_H__
