/*=============================================================================
	PS3RHI.h: Null RHI definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if USE_PS3_RHI

#ifndef __PS3RHI_H__
#define __PS3RHI_H__

#define RHI_SUPPORTS_PREVERTEXSHADER_CULLING USE_PS3_PREVERTEXCULLING

#include "PS3Gcm.h"
#include "PS3RHIUtil.h"

#include "PS3RHIResources.h"
#include "PS3RHISurface.h"
#include "PS3RHIViewport.h"

extern FSurfaceRHIRef				GPS3RHIRenderTarget;
extern FSurfaceRHIRef				GPS3RHIDepthTarget;

enum EPS3RHIStats
{
	STAT_SetRenderTarget = STAT_PS3RHIFirstStat,
	STAT_DrawPrimitive,
	STAT_DrawPrimitiveUP,
	STAT_Clear,
	STAT_SwapBuffers,
	STAT_GetQueryResults,
	STAT_CopyToResolveTarget_GPU,
	STAT_BlockedByGPU,
	STAT_BlockedBySPU,
	STAT_LandscapeSPU,
};



/**
* Perform PS3 GCM/RHI initialization that needs to wait until after appInit has happened
*/
void PS3PostAppInitRHIInitialization();


#if !FINAL_RELEASE
/**
 * Mode 0:	Dump color buffer RGB or depth buffer Z
 * Mode 1:	Dump color buffer Alpha or depth buffer stencil
 */
void DumpRenderTarget(FSurfaceRHIRef Surface, DWORD Mode = 0);
#else
inline void DumpRenderTarget(FSurfaceRHIRef, DWORD = 0) {}
#endif

#endif // __PS3RHI_H__

#endif  // USE_PS3_RHI
