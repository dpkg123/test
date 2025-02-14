/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __SAMPLEAPP_H__
#define __SAMPLEAPP_H__

#include "FWInput.h"
#include "FWTime.h"

#include "psgl/FWGLCamControlApplication.h"
#ifdef  _MSC_VER
#include "edge/anim/edgeanim_windows.h"
#else
#include "cellutil.h"
#include "edge/anim/edgeanim_ppu.h"
#endif

#include "spu/process_blend_tree.h"

struct LocomotionData;
struct LocomotionState;

struct AnimInstance
{
    EdgeAnimAnimation*          resource;
    LocomotionState             locomotionState;
};

struct Character
{
    Transform3*                 worldMatrices;
    AnimInstance                anims[2];
    EdgeAnimJointTransform      rootJoint;
    float                       evalTime;
    EdgeAnimBlendBranch EDGE_ALIGNED(16) blendBranches[32];       
    EdgeAnimBlendLeaf EDGE_ALIGNED(16) blendLeaves[32];
};

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

#ifdef __PPU__
    /* PS3 only - not available with PC version */
    void createSendEventJob(CellSpursJob256* jobSendEvent, uint16_t bitsEventFlag);
    void createAnimJob(CellSpursJob256* jobAnim, Transform3* outputMatrices, const struct EdgeAnimBlendBranch* blendBranches, uint32_t numBlendBranches, const struct EdgeAnimBlendLeaf* blendLeaves, uint32_t numBlendLeaves, const struct EdgeAnimJointTransform* rootJoint);
#endif

private:
#ifdef __PPU__
    /* PS3 only - not available with PC version */
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
    struct EdgeAnimAnimation*       mAnimation[2];
    void*							mExternalSpuStorage;

    Character*						mCharacters;

    void*							mVertexBufferBase;
    void*							mVertexBufferCur;

	FWTimeVal						mLastTime;
    unsigned int					mFrameCount;
};

#endif // __SAMPLEAPP_H__
