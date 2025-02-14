/*=============================================================================
	PS3RHIViewport.cpp: PS3 viewport RHI implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3RHIPrivate.h"

#if USE_PS3_RHI


FPS3RHIViewport::FPS3RHIViewport(UINT InSizeX, UINT InSizeY)
: SizeX(InSizeX)
, SizeY(InSizeY)
{
	FrameBuffers[0] = GPS3Gcm->GetColorBuffer(0);
	FrameBuffers[1] = GPS3Gcm->GetColorBuffer(1);
}

FPS3RHIViewport::~FPS3RHIViewport()
{
}

FViewportRHIRef RHICreateViewport(void* WindowHandle,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen)
{
	check( IsInGameThread() );
	return new FPS3RHIViewport(SizeX, SizeY);
}

void RHIResizeViewport(FViewportRHIParamRef Viewport,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen)
{
	check( IsInGameThread() );
	check(0);
}

void RHITick( FLOAT DeltaTime )
{
	check( IsInGameThread() );
}

/** Determine if currently drawing a viewport. Render thread only */
static UBOOL GPS3IsDrawingViewport=FALSE;

void RHIBeginDrawingViewport(FViewportRHIParamRef Viewport)
{
	GPS3Gcm->BeginScene();

	GPS3IsDrawingViewport = TRUE;

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources();

	// set the default render target and viewport
	RHISetRenderTarget(RHIGetViewportBackBuffer(Viewport),NULL);	
}

void RHIEndDrawingViewport(FViewportRHIParamRef Viewport, UBOOL bPresent,UBOOL bLockToVsync)
{
	GPS3Gcm->EndScene();

	GPS3IsDrawingViewport = FALSE;

	if (bPresent)
	{
		SCOPE_CYCLE_COUNTER(STAT_SwapBuffers);
		GPS3Gcm->SwapBuffers();

		// Delete unused resources.
		DeleteUnusedPS3Resources();

		// If the input latency timer has been triggered, block until the GPU is completely
		// finished displaying this frame and calculate the delta time.
		if ( GInputLatencyTimer.RenderThreadTrigger )
		{
			GPS3Gcm->BlockUntilGPUIdle();
			DWORD EndTime = appCycles();
			GInputLatencyTimer.DeltaTime = EndTime - GInputLatencyTimer.StartTime;
			GInputLatencyTimer.RenderThreadTrigger = FALSE;
		}
	}
}

/**
 * Determine if currently drawing the viewport
 *
 * @return TRUE if currently within a BeginDrawingViewport/EndDrawingViewport block
 */
UBOOL RHIIsDrawingViewport()
{
	return GPS3IsDrawingViewport;
}

void RHIBeginScene()
{
}

void RHIEndScene()
{
}

FSurfaceRHIRef RHIGetViewportBackBuffer(FViewportRHIParamRef Viewport)
{
	// return a reference to the surface that is current back buffer 
	return Viewport->GetSurface(GPS3Gcm->GetBackBufferIndex());
}

FSurfaceRHIRef RHIGetViewportDepthStencil(FViewportRHIParamRef Viewport)
{
	// the surface contains the depth info, so just use the back buffer surface
	return RHIGetViewportBackBuffer(Viewport);
}

#endif // USE_PS3_RHI
