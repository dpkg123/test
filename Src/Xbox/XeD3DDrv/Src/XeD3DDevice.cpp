/*=============================================================================
	D3DViewport.cpp: D3D viewport RHI implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "XeD3DDrvPrivate.h"

#if USE_XeD3D_RHI
#include "ChartCreation.h"

/*-----------------------------------------------------------------------------
	Defines.
-----------------------------------------------------------------------------*/

#define PERF_LOG_CPU_BLOCKING_ON_GPU 0

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

/** The global D3D device. */
IDirect3DDevice9* GDirect3DDevice;

/** The global D3D device's back buffer. */
IDirect3DSurface9* GD3DBackBuffer;

/** The global D3D device's 4X resolve source back buffer (overlapped with GD3DBackBuffer). */
IDirect3DSurface9* GD3DBackBufferResolveSource;

/** The global D3D device's front buffer texture */
IDirect3DTexture9* GD3DFrontBuffer;

/** The viewport RHI which is currently fullscreen. */
FD3DViewport* GD3DFullscreenViewport = NULL;

/** True if the currently set depth surface invertes the z-range to go from (1,0) */
UBOOL GInvertZ = FALSE;

/** The global device video mode. */
XVIDEO_MODE GVideoMode;

/** The width of the D3D device's back buffer. */
INT GScreenWidth = 1280;

/** The height of the D3D device's back buffer. */
INT GScreenHeight= 720;

/** True if the D3D device is in fullscreen mode. */
UBOOL GD3DIsFullscreenDevice = FALSE;

/** True if the application has lost D3D device access. */
UBOOL GD3DDeviceLost = FALSE;

/** Whether we're allowed to swap/ resolve */
UBOOL GAllowBufferSwap = TRUE;

/** Value between 0-100 that determines the percentage of the vertical scan that is allowed to pass while still allowing us to swap when VSYNC'ed.
    This is used to get the same behavior as the old *_OR_IMMEDIATE present modes. */
DWORD GPresentImmediateThreshold = 100;

/** Value between 0-100 that determines the percentage of the vertical scan that is allowed to pass.  This used when we are locking fully to the VSYNC. */
DWORD GPresentLockThreshold = 10;

/** Amount of time the CPU is stalled waiting on the GPU */
FLOAT GCPUWaitingOnGPUTime = 0.0f;
/** Amount of time the CPU was stalled waiting on the GPU the previous frame. */
FLOAT GCPUWaitingOnGPUTimeLastFrame = 0.0f;

/** When TRUE, a gamma ramp will be set that compensates for the Xenon's default gamma ramp making darks too dark. */
UBOOL GUseCorrectiveGammaRamp = FALSE;

/**
 *	RingBuffer Information
 *
 *	These are the default values, unless they are found in the Engine.ini file.
 *	NOTE: If the application calls IDirect3DDevice9::SetRingBufferParameters
 *		  anywhere in the application, these need to be updated to the values
 *		  set.
 *
 /
/**	Primary ring buffer size. D3D Default value is 32kB.				*/
INT GRingBufferPrimarySize = 32 * 1024;
/** Secondary ring buffer size. D3D Default value is 2MB.				*/
INT GRingBufferSecondarySize = 3 * 1024 * 1024;
/**
 *	The number of segments the secondary ring buffer is partitioned
 *	into. When enough commands are added to the ring buffer to fill
 *	a segment, D3D kicks the segment to the device for processing.
 *	D3D Default value is 32.
 */
INT GRingBufferSegmentCount = 48;

/**
 * Callback function called whenever D3D blocks CPU till GPU is ready.
 */
void XeBlockCallback( DWORD Flags, D3DBLOCKTYPE BlockType, FLOAT ClockTime, DWORD ThreadTime )
{
	GCPUWaitingOnGPUTime += ClockTime;
#if LOOKING_FOR_PERF_ISSUES || PERF_LOG_CPU_BLOCKING_ON_GPU
	if( BlockType != D3DBLOCKTYPE_SWAP_THROTTLE )
	{
		debugf(NAME_PerfWarning,TEXT("CPU waiting for GPU: Blocked=%5.2f ms, Spun=%i ms, Event=%i"), ClockTime, ThreadTime, (INT)BlockType );
	}
#endif
}

#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
#include "DwTrioviz/DwTriovizImpl.h"
#endif

void RHIInit()
{
	if( !GIsRHIInitialized )
	{
		// Set RHI capabilities.
		GSupportsVertexTextureFetch = TRUE;
		GSupportsEmulatedVertexInstancing = TRUE;
		GSupportsDepthTextures = TRUE;
		GUsesInvertedZ = TRUE;
		// MSAA at 720p is disabled by default, as the quality improvements from allowing normal shadows on the dominant light in the base pass
		// Are better than the quality improvements of 2xMSAA.  The current implementation of normal shadows on the dominant light in the base pass 
		// Is mutually exclusive with MSAA, due to 2xMSAA making the scene depth buffer 2x bigger in EDRAM.
		GUseTilingCode = FALSE;
		extern const UBOOL GOnePassDominantLight;
		check(!(GOnePassDominantLight && GUseTilingCode));
		GDrawUPVertexCheckCount = 400 * 1024;
		GDrawUPIndexCheckCount = 400 * 1024;
		GRHIShaderPlatform = SP_XBOXD3D;
		
		// General purpose shadow buffer size, which has to co-exist with scene depth and light attenuation
		// 2 x 1280x720 x 4byte z-buffer and lighting targets = 1440 EDRAM tiles, with 608 remaining
		// We want an aspect ratio of about 1.5, with the x size the largest, so that character shadows will have larger effective resolutions
		// After taking EDRAM tile size (80x16) and this aspect ratio into account, the below sizes maximize effective resolution
		GMaxPerObjectShadowDepthBufferSizeX = 1120;
		GMaxPerObjectShadowDepthBufferSizeY = 688;
		// Whole scene dominant shadow depth buffer size
		// This is rendered before anything else in the frame so it is only limited by HiZ (since we want HiZ while rendering shadow depths)
		// HiZ has 3600 tiles and each HiZ tile is 512 pixels, so the largest square depth buffer we can create is 1344 after meeting the resolve alignment restrictions
		GMaxWholeSceneDominantShadowDepthBufferSize = 1344;
		// On 360 always use the unused alpha bit for storing emissive mask 
		GModShadowsWithAlphaEmissiveBit = TRUE;
	}
}

void RHIAcquireThreadOwnership()
{
	if( GIsRHIInitialized )
	{
		GDirect3DDevice->AcquireThreadOwnership();
	}
}

void RHIReleaseThreadOwnership()
{
	if( GIsRHIInitialized )
	{
		GDirect3DDevice->ReleaseThreadOwnership();
	}
}

/**
 * Initializes the D3D device
 */
void XeInitD3DDevice()
{
	// Ensure the capabilities are set.
	RHIInit();

	// Read the selected video mode and adjust the backbuffer size/aspect ratio
	// accordingly (TCRs 018 & 019)
	XGetVideoMode(&GVideoMode);

	FLOAT AspectRatio = GVideoMode.fIsWideScreen || GVideoMode.dwDisplayHeight > 720 ? AspectRatio16x9 :
		((FLOAT)GVideoMode.dwDisplayWidth / (FLOAT)GVideoMode.dwDisplayHeight);
	GScreenHeight = 720;
	GScreenWidth = Clamp(
		// Width is height times aspect ratio (900 = 5x4, 960 = 4x3, 1280 = 16x9)
		appTrunc(720.f * AspectRatio),
		// Min is 5x4 aspect ration
		900,
		// Max is 16x9 resolution
		1280);

	// Do not use tiling when the backbuffer isn't 1280
	if (GScreenWidth != 1280)
	{
		GUseTilingCode = FALSE;

		// We are overlapping the shadow depth texture with light attenuation,
		// So we must limit the size to 800 since light attenuation is at a minimum: 900 * 720 * 4
		GMaxPerObjectShadowDepthBufferSizeX = 800;
		GMaxPerObjectShadowDepthBufferSizeY = 800;
	}

	// Always assume vsync is enabled on console and ignore config option loaded by GSystemSettings
	// Disable VSYNC if -novsync is on the command line.  
	GSystemSettings.bUseVSync = !ParseParam(appCmdLine(), TEXT("novsync"));

	D3DPRESENT_PARAMETERS PresentParameters = { 0 };
	PresentParameters.BackBufferWidth			= GScreenWidth;
	PresentParameters.BackBufferHeight			= GScreenHeight;
	PresentParameters.SwapEffect				= D3DSWAPEFFECT_DISCARD; 
	PresentParameters.EnableAutoDepthStencil	= FALSE;
	PresentParameters.DisableAutoBackBuffer		= TRUE;
	PresentParameters.DisableAutoFrontBuffer	= TRUE;	
	PresentParameters.PresentationInterval		= GSystemSettings.bUseVSync ? D3DPRESENT_INTERVAL_TWO : D3DPRESENT_INTERVAL_IMMEDIATE;
	PresentParameters.hDeviceWindow				= NULL;
	
	// Ring buffer
	PresentParameters.RingBufferParameters.Flags = 0;
	PresentParameters.RingBufferParameters.PrimarySize = GRingBufferPrimarySize;
	PresentParameters.RingBufferParameters.SecondarySize = GRingBufferSecondarySize;
	PresentParameters.RingBufferParameters.SegmentCount = GRingBufferSegmentCount;

	// Create D3D device.
	VERIFYD3DRESULT( Direct3DCreate9(D3D_SDK_VERSION)->CreateDevice( 
		D3DADAPTER_DEFAULT, 
		D3DDEVTYPE_HAL, 
		NULL, 
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_BUFFER_2_FRAMES, 
		&PresentParameters, 
		&GDirect3DDevice ));

#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
	//Test frame packing availability on Xbox 360
	// Get the capabilities of the display.  This determines if we can switch to S3D mode.
	DWORD VideoCaps = XGetVideoCapabilities();

	if( (VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_ENABLED)  &&    // S3D is enabled in the dashboard (default is enabled)
		(VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_720P_60HZ) )    // Display device supports 720p60Hz S3D
	{
		// Get the stereo parameters based on the desired frame buffer size		
		XGGetStereoParameters(GScreenWidth, GScreenHeight, D3DMULTISAMPLE_NONE, 0, &g_DwStereoParams);

		g_Dw720p3DAvailable = TRUE;

		UINT StereoFrontbufferSize = XGSetTextureHeaderEx(g_DwStereoParams.FrontBufferWidth,g_DwStereoParams.FrontBufferHeight,1, 0,D3DFMT_LE_X8R8G8B8,0,0,0,XGHEADER_CONTIGUOUS_MIP_OFFSET,0,NULL,NULL,NULL); 
		StereoFrontbufferSize = Align(StereoFrontbufferSize,D3DTEXTURE_ALIGNMENT);
		g_DwMemoryFrontBuffer = RHICreateSharedMemory(StereoFrontbufferSize);
		g_DwStereoFrontBuffer = DwCreateSharedTexture2D(g_DwStereoParams.FrontBufferWidth,g_DwStereoParams.FrontBufferHeight,D3DFMT_LE_X8R8G8B8,1,g_DwMemoryFrontBuffer,0);
		g_DwStereoFrontBufferDx9 = ((IDirect3DTexture9*)g_DwStereoFrontBuffer);		
		//To avoid tearing, allocate another buffer	
#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_VSYNC
		VERIFYD3DRESULT( GDirect3DDevice->CreateTexture( g_DwStereoParams.FrontBufferWidth,g_DwStereoParams.FrontBufferHeight, 1, 0, D3DFMT_LE_X8R8G8B8, 0, &g_DwStereoSecondFrontBufferDx9, NULL ) );  
#endif

		g_DwPresentParameters = PresentParameters;
		
		D3DSURFACE_PARAMETERS BackBufferParameters	= { 0 };
		D3DFORMAT BackBufferFormat = D3DFMT_A8R8G8B8;  
		BackBufferParameters.Base = XeEDRAMOffset( TEXT("DwStereoBackbuffer"), XGSurfaceSize(g_DwStereoParams.EyeBufferWidth, g_DwStereoParams.EyeBufferHeight, BackBufferFormat, D3DMULTISAMPLE_NONE) );
		VERIFYD3DRESULT( GDirect3DDevice->CreateRenderTarget(g_DwStereoParams.EyeBufferWidth, g_DwStereoParams.EyeBufferHeight, BackBufferFormat, D3DMULTISAMPLE_NONE, 0, 0, &g_DwStereoBackBuffer, &BackBufferParameters ) );
	}
#endif

	// Create backbuffer.
	D3DSURFACE_PARAMETERS BackBufferParameters	= { 0 };
	D3DFORMAT BackBufferFormat = D3DFMT_A8R8G8B8;  
	BackBufferParameters.Base = XeEDRAMOffset( TEXT("DefaultBB"), XGSurfaceSize(GScreenWidth, GScreenHeight, BackBufferFormat, D3DMULTISAMPLE_NONE) );
	VERIFYD3DRESULT( GDirect3DDevice->CreateRenderTarget( GScreenWidth, GScreenHeight, BackBufferFormat, D3DMULTISAMPLE_NONE, 0, 0, &GD3DBackBuffer, &BackBufferParameters ) );

	// Create overlaid resolve source texture
	D3DSURFACE_PARAMETERS ResolveBufferParameters = BackBufferParameters;
	ResolveBufferParameters.Base = XeEDRAMOffset( TEXT("DefaultColor"), XGSurfaceSize(GScreenWidth, GScreenHeight, D3DFMT_A2B10G10R10, D3DMULTISAMPLE_NONE) );
	ResolveBufferParameters.ColorExpBias = 0;

	D3DMULTISAMPLE_TYPE ResolveSourceMultisampleType = D3DMULTISAMPLE_NONE;
#if !_DEBUG
	// Only use MSAA in non-debug, because the debug runtime will throw an assert when resolving from this MSAA surface to a non-MSAA texture
	// As a result the resolve will not work correctly in debug
	ResolveSourceMultisampleType = D3DMULTISAMPLE_4_SAMPLES;
#endif
	VERIFYD3DRESULT( GDirect3DDevice->CreateRenderTarget( GScreenWidth/2, GScreenHeight/2, D3DFMT_A2B10G10R10F_EDRAM, ResolveSourceMultisampleType, 0, 0, &GD3DBackBufferResolveSource, &ResolveBufferParameters ) );

#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
	//Allocate the front buffer from the shared memory space to switch to 3D in realtime
	if (g_Dw720p3DAvailable)
	{
		g_DwFrontBuffer = DwCreateSharedTexture2D(GScreenWidth,GScreenHeight,D3DFMT_LE_X8R8G8B8,1,g_DwMemoryFrontBuffer,0);
		GD3DFrontBuffer = ((IDirect3DTexture9*)g_DwFrontBuffer);
	}
	else
#endif
	{
		// Create front buffer texture.
		D3DFORMAT FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
		VERIFYD3DRESULT( GDirect3DDevice->CreateTexture( GScreenWidth, GScreenHeight, 1, 0, FrontBufferFormat, 0, &GD3DFrontBuffer, NULL ) );  
	}

	// Set back and depth buffer.
	GDirect3DDevice->SetRenderTarget( 0, GD3DBackBuffer );
	GDirect3DDevice->SetDepthStencilSurface( NULL );

	// Default D3D half pixel offset
	GDirect3DDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, FALSE );

	// Initialize the platform pixel format map.
	GPixelFormats[ PF_Unknown		].PlatformFormat	= D3DFMT_UNKNOWN;
	GPixelFormats[ PF_A32B32G32R32F	].PlatformFormat	= D3DFMT_A32B32G32R32F;
	GPixelFormats[ PF_A8R8G8B8		].PlatformFormat	= D3DFMT_A8R8G8B8;
	GPixelFormats[ PF_G8			].PlatformFormat	= D3DFMT_L8;
	GPixelFormats[ PF_G16			].PlatformFormat	= D3DFMT_UNKNOWN;	// Not supported for rendering.
	GPixelFormats[ PF_DXT1			].PlatformFormat	= D3DFMT_DXT1;
	GPixelFormats[ PF_DXT3			].PlatformFormat	= D3DFMT_DXT3;
	GPixelFormats[ PF_DXT5			].PlatformFormat	= D3DFMT_DXT5;
	GPixelFormats[ PF_UYVY			].PlatformFormat	= D3DFMT_UYVY;	
	GPixelFormats[ PF_FloatRGB		].PlatformFormat	= D3DFMT_A16B16G16R16F;
	GPixelFormats[ PF_FloatRGB		].Supported			= 1;
	GPixelFormats[ PF_FloatRGB		].BlockBytes		= 8;
	GPixelFormats[ PF_FloatRGBA		].PlatformFormat	= D3DFMT_A16B16G16R16F_EXPAND;
	GPixelFormats[ PF_FloatRGBA		].Supported			= 1;
	GPixelFormats[ PF_FloatRGBA		].BlockBytes		= 8;
	GPixelFormats[ PF_DepthStencil	].PlatformFormat	= GUsesInvertedZ ? D3DFMT_D24FS8 : D3DFMT_D24S8;
	GPixelFormats[ PF_DepthStencil	].BlockBytes		= 4;
	GPixelFormats[ PF_ShadowDepth	].PlatformFormat	= D3DFMT_D24X8;
	GPixelFormats[ PF_ShadowDepth	].BlockBytes		= 4;
	GPixelFormats[ PF_R32F			].PlatformFormat	= D3DFMT_R32F;
	GPixelFormats[ PF_G16R16		].PlatformFormat	= D3DFMT_G16R16;
	GPixelFormats[ PF_G16R16F		].PlatformFormat	= D3DFMT_G16R16F;
	GPixelFormats[ PF_G16R16F_FILTER].PlatformFormat	= D3DFMT_G16R16F_EXPAND;
	GPixelFormats[ PF_G32R32F		].PlatformFormat	= D3DFMT_G32R32F;
	GPixelFormats[ PF_A2B10G10R10   ].PlatformFormat    = D3DFMT_A2B10G10R10;
	GPixelFormats[ PF_A16B16G16R16  ].PlatformFormat	= D3DFMT_A16B16G16R16;
	GPixelFormats[ PF_R16F			].PlatformFormat	= D3DFMT_R16F;
	GPixelFormats[ PF_R16F_FILTER	].PlatformFormat	= D3DFMT_R16F_EXPAND;
	GPixelFormats[ PF_V8U8			].PlatformFormat	= D3DFMT_V8U8;
	GPixelFormats[ PF_BC5			].PlatformFormat	= D3DFMT_DXN;

	// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitDynamicRHI();
	}
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitRHI();
	}

	// Register callback function to look into CPU blocking operations.
	GDirect3DDevice->SetBlockCallback( 0, XeBlockCallback );

	// Ensure that the buffer is clear
	GDirect3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
	XePerformSwap(TRUE);

	// RHI device is now valid
	GIsRHIInitialized = TRUE;
}

#if STATS
#define PERFBUFFERSIZE	2
static D3DPerfCounters *GPerfCounterBeginFrame[PERFBUFFERSIZE];
static D3DPerfCounters *GPerfCounterEndFrame[PERFBUFFERSIZE];
static UINT GSwapFrameCounter = 0;
#endif

/**
 * Helper function performing tasks involved in advancing the presented frame by swapping buffers.
 *
 * @param bSyncToPresentationInterval	whether we should sync to the presentation interval defined when creating the device
 */
void XePerformSwap( UBOOL bSyncToPresentationInterval, UBOOL bLockToVsync )
{
	INT Result = GTexturePool.Tick();

#if LINK_AGAINST_PROFILING_D3D_LIBRARIES && STATS && !PERF_LOG_CPU_BLOCKING_ON_GPU
	if ( GSwapFrameCounter == 0 )
	{
		D3DPERFCOUNTER_EVENTS GPerfCounterEvents = { { GPUPE_CP_COUNT } };
		GPerfCounterEvents.CP[0] = GPUPE_CSF_RB_I1_I2_FETCHING;
		GDirect3DDevice->EnablePerfCounters( TRUE );
		GDirect3DDevice->SetPerfCounterEvents( &GPerfCounterEvents, 0 );
		for ( INT PerfCounterIndex=0; PerfCounterIndex < PERFBUFFERSIZE; ++PerfCounterIndex )
		{
			GDirect3DDevice->CreatePerfCounters( &GPerfCounterBeginFrame[PerfCounterIndex], 1 );
			GDirect3DDevice->CreatePerfCounters( &GPerfCounterEndFrame[PerfCounterIndex], 1 );
		}
	}

	INT PerfIndex = GSwapFrameCounter % PERFBUFFERSIZE;
	GDirect3DDevice->QueryPerfCounters( GPerfCounterEndFrame[PerfIndex], 0 );

	PerfIndex = ++GSwapFrameCounter % PERFBUFFERSIZE;
	if ( GSwapFrameCounter > PERFBUFFERSIZE )
	{
		D3DPERFCOUNTER_VALUES PerfValuesBeginFrame, PerfValuesEndFrame;
		GPerfCounterBeginFrame[PerfIndex]->GetValues( &PerfValuesBeginFrame, 0, NULL );
		GPerfCounterEndFrame[PerfIndex]->GetValues( &PerfValuesEndFrame, 0, NULL );

 		// Calculate time using RBBM; GPU utilization using Ring data availability
 		DWORD TotalGPUFrameTime = DWORD( (PerfValuesEndFrame.RBBM[0].QuadPart - PerfValuesBeginFrame.RBBM[0].QuadPart)*GCyclesPerSecond/500000000ull );
 		GGPUFrameTime = DWORD( (PerfValuesEndFrame.CP[0].QuadPart - PerfValuesBeginFrame.CP[0].QuadPart)*GCyclesPerSecond/500000000ull );
 
 		SET_FLOAT_STAT(STAT_GPUWaitingOnCPU, ((float)(TotalGPUFrameTime - GGPUFrameTime)) / GCyclesPerSecond * 1000.0f);
 		SET_FLOAT_STAT(STAT_CPUWaitingOnGPU, GCPUWaitingOnGPUTime); 
 		GCPUWaitingOnGPUTimeLastFrame = GCPUWaitingOnGPUTime;
		GCPUWaitingOnGPUTime = 0.0f;
	}
#else
	GCPUWaitingOnGPUTimeLastFrame = GCPUWaitingOnGPUTime;
	GCPUWaitingOnGPUTime = 0.0f;
#endif

	// This may not be the right place for this.  Scaling should be applied at the beginning
	// of a frame, and we need only detect that the scale is different before resolve.
	D3DVIDEO_SCALER_PARAMETERS *VideoScalerParams = NULL;
	D3DVIDEO_SCALER_PARAMETERS VideoScalerAdjust;
	static FSystemSettingsDataScreenPercentage PrevScreenPercentage;
	if (PrevScreenPercentage.bUpscaleScreenPercentage != GSystemSettings.bUpscaleScreenPercentage ||
		PrevScreenPercentage.ScreenPercentage != GSystemSettings.ScreenPercentage )
	{
		PrevScreenPercentage.bUpscaleScreenPercentage = GSystemSettings.bUpscaleScreenPercentage;
		PrevScreenPercentage.ScreenPercentage = GSystemSettings.ScreenPercentage;

		FLOAT Scale = 1.0f;
		if ( GSystemSettings.bUpscaleScreenPercentage == FALSE ) 
		{
			Scale = Clamp( GSystemSettings.ScreenPercentage * 0.01f, 0.0f, 1.f );
		}
		INT ScaledWidth = ( INT( GScreenWidth * Scale + 0.5f ) ) & ~0xf;
		INT ScaledHeight = ( INT( GScreenHeight * Scale + 0.5f ) ) & ~0xf;
		INT XOff = ( GScreenWidth - ScaledWidth ) >> 1;
		INT YOff = ( GScreenHeight - ScaledHeight ) >> 1;
		VideoScalerAdjust.ScalerSourceRect.x1 = XOff;
		VideoScalerAdjust.ScalerSourceRect.y1 = YOff;
		VideoScalerAdjust.ScalerSourceRect.x2 = XOff + ScaledWidth;
		VideoScalerAdjust.ScalerSourceRect.y2 = YOff + ScaledHeight;
		VideoScalerAdjust.ScaledOutputWidth = GVideoMode.dwDisplayWidth;
		VideoScalerAdjust.ScaledOutputHeight = GVideoMode.dwDisplayHeight;
		VideoScalerAdjust.FilterProfile = 1;
		VideoScalerParams = & VideoScalerAdjust;
		if( Scale < 1.0f )
		{
			debugf( TEXT("VideoScalerParams: x1=%d y1=%d x2=%d y2=%d ScaledOutputWidth=%d ScaledOutputHeight=%d"),
				VideoScalerAdjust.ScalerSourceRect.x1,
				VideoScalerAdjust.ScalerSourceRect.y1,
				VideoScalerAdjust.ScalerSourceRect.x2,
				VideoScalerAdjust.ScalerSourceRect.y2,
				VideoScalerAdjust.ScaledOutputWidth,
				VideoScalerAdjust.ScaledOutputHeight
				);
		}
	}

	extern UBOOL GD3DRenderingIsSuspended;
	check(GD3DRenderingIsSuspended == FALSE);

	// Stop CPU trace if, and only if, one was kicked off below.
	static FName NameRender( TEXT("Render"), FNAME_Add );
	appStopCPUTrace( NameRender );

	if( GAllowBufferSwap )
	{
		if( bSyncToPresentationInterval )
		{
			// Sync to vertical blank.
			GDirect3DDevice->SynchronizeToPresentationInterval();
		}
#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
		if (g_DwUseFramePacking)
		{
			GDirect3DDevice->SetRenderTarget(0, g_DwStereoBackBuffer);
			GDirect3DDevice->SetDepthStencilSurface(NULL);

			//Clear the blank region
			XGSTEREOREGION * pRegion = &g_DwStereoParams.RightEye;
			if( pRegion->BlankRectTop.x2 != pRegion->BlankRectTop.x1 )
			{
				D3DVIEWPORT9 blankViewport = { 0, pRegion->BlankRectTop.y1, GScreenWidth, pRegion->BlankRectTop.y2 - pRegion->BlankRectTop.y1 };
				GDirect3DDevice->SetViewport(&blankViewport);
				GDirect3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 0, 0);
			}

			//Resolve only right eye
			//Left eye already taken care of in DwTriovizImpl_XboxFramePackingCopyBackbuffer_RenderThread()			
			D3DVIEWPORT9 RenderViewport = { 0, g_DwStereoParams.RightEye.ViewportYOffset, GScreenWidth, GScreenHeight, 0, 1 };
			GDirect3DDevice->SetViewport(&RenderViewport);
			GDirect3DDevice->Resolve(0, &g_DwStereoParams.RightEye.ResolveSourceRect, g_DwStereoFrontBufferDx9, &g_DwStereoParams.RightEye.ResolveDestPoint, 0, 0, NULL, 0.0f, 0, NULL );
			
			GDirect3DDevice->Swap(((IDirect3DTexture9*)g_DwStereoFrontBufferDx9), VideoScalerParams );
			
			//Swap between the buffers
			#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_VSYNC
			IDirect3DTexture9* pTempStereo = g_DwStereoSecondFrontBufferDx9;
			g_DwStereoSecondFrontBufferDx9 = g_DwStereoFrontBufferDx9;
			g_DwStereoFrontBufferDx9 = pTempStereo;
			#endif
			
			//Reset texture states
			for(UINT Index = 0;Index < 16;Index++)
			{
				GDirect3DDevice->SetTexture(Index,NULL);
			}
		}
		else
#endif
		{
			GDirect3DDevice->Resolve( 0, NULL, GD3DFrontBuffer, NULL, 0, 0, 0, 0, 0, NULL );
			GDirect3DDevice->Swap( GD3DFrontBuffer, VideoScalerParams );
		}
	}

#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_VSYNC
	if (g_DwUseFramePacking)
	{
		//force vsync to avoid tearing which is really annoying in 3D
		GDirect3DDevice->SetRenderState(D3DRS_PRESENTIMMEDIATETHRESHOLD, 0);
	}
	else
#endif

	// allows for on-the-fly changing of VSYNC setting
	GDirect3DDevice->SetRenderState(D3DRS_PRESENTIMMEDIATETHRESHOLD, (bLockToVsync && GSystemSettings.bUseVSync) ? GPresentLockThreshold : GPresentImmediateThreshold);

	// Start tracing render thread if requested.
	appStartCPUTrace( NameRender, TRUE, FALSE, 40, NULL );

#if LINK_AGAINST_PROFILING_D3D_LIBRARIES && !FINAL_RELEASE && !_DEBUG
	extern UBOOL GShowMaterialDrawEvents;
	GShowMaterialDrawEvents = (PIXGetCaptureState() & PIX_CAPTURE_GPU) != 0;
	GEmitDrawEvents			= (PIXGetCaptureState() & PIX_CAPTURE_GPU) != 0 || (PIXGetCaptureState() & PIX_CAPTURE_TIMING) != 0;
#endif

#if LINK_AGAINST_PROFILING_D3D_LIBRARIES && STATS && !PERF_LOG_CPU_BLOCKING_ON_GPU
	GDirect3DDevice->QueryPerfCounters( GPerfCounterBeginFrame[PerfIndex], 0 );
#endif

#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_VSYNC
	if (g_DwUseFramePacking)
	{
		//Setup the stereo backbuffer (1280 * 734)
		GDirect3DDevice->SetRenderTarget(0, g_DwStereoBackBuffer);
		GDirect3DDevice->SetDepthStencilSurface(NULL);

		// Clear the blank area of the front buffer.
		//
		// Note: The left and right eye blank regions must be the same color as the invariant blank area.  D3D will
		// display warning debug text in debug and profile builds if this is not the case.  In release and LTCG
		// builds, this check is not performed, but the title should still conform to this rule, or its stereo
		// 3D output may not work with some TVs.
		GDirect3DDevice->Clear( 1, &g_DwStereoParams.BlankRegion.BlankRectTop, D3DCLEAR_TARGET, 0, 0, 0);

		GDirect3DDevice->Resolve(D3DRESOLVE_ALLFRAGMENTS, &g_DwStereoParams.BlankRegion.ResolveSourceRect, g_DwStereoFrontBufferDx9, &g_DwStereoParams.BlankRegion.ResolveDestPoint, 0, 0, NULL, 0, 0, NULL);
	}
#endif
}

/**
 *	Check the config file for ring buffer parameters. If present, set them on the device.
 */
void XeSetRingBufferParametersFromConfig()
{
	UBOOL bIsSettingRingBufferParameters = FALSE;
	// Retrieve ring buffer settings from ini - if present.
	if (GConfig)
	{
		INT Temporary = 0;
		if (GConfig->GetInt(TEXT("XeD3D"), TEXT("RBPrimarySize"), Temporary, GEngineIni) == TRUE)
		{
			if ((Temporary & (Temporary - 1)) != 0)
			{
				warnf(TEXT("RingBufferPrimarySize must be power of two (%d from engine ini file). Using default of 32kB"), Temporary);
			}
			else
			{
				if (GRingBufferPrimarySize != Temporary)
				{
					GRingBufferPrimarySize = Temporary;
					bIsSettingRingBufferParameters = TRUE;
				}
			}
		}
		if (GConfig->GetInt(TEXT("XeD3D"), TEXT("RBSecondarySize"), Temporary, GEngineIni) == TRUE)
		{
			if ((Temporary > 0) && (Temporary < (500 * 1024)))
			{
				warnf(TEXT("RingBufferSecondarySize of %d from engine ini file. Possibly too small."), Temporary);
			}

			if (GRingBufferSecondarySize != Temporary)
			{
				GRingBufferSecondarySize = Temporary;
				bIsSettingRingBufferParameters = TRUE;
			}
		}
		if (GConfig->GetInt(TEXT("XeD3D"), TEXT("RBSegmentCount"), Temporary, GEngineIni) == TRUE)
		{
			if ((Temporary > 0) && (Temporary < (GRingBufferSegmentCount / 2)))
			{
				warnf(TEXT("RingBufferSegmentCount of %d from engine ini file. Possibly too small."), Temporary);
			}

			if (GRingBufferSegmentCount != Temporary)
			{
				GRingBufferSegmentCount = Temporary;
				bIsSettingRingBufferParameters = TRUE;
			}
		}
	}

	if (bIsSettingRingBufferParameters)
	{
		XeSetRingBufferParameters(GRingBufferPrimarySize, GRingBufferSecondarySize, GRingBufferSegmentCount, TRUE);
	}
}

/**
 *	Set the ring buffer parameters
 *
 *	This function must be used if the ring buffer parameters are altered (rather
 *	than the IDirect3DDevice9 interface directly).
 *
 *	@param		PrimarySize			The size of the primary buffer
 *	@param		SecondarySize		The size of the secondary buffer
 *	@param		SegmentCount		The number of segments to divide the secondary buffer into
 *	@param		bCallDeviceSet		If TRUE, will call the SetRingBufferParameters function
 *									on the device. 
 */
void XeSetRingBufferParameters(INT PrimarySize, INT SecondarySize, INT SegmentCount, UBOOL bCallDeviceSet)
{
	D3DRING_BUFFER_PARAMETERS RBParams;

	appMemset((void*)&RBParams, 0, sizeof(RBParams));

	if ((PrimarySize > 0) && (PrimarySize & (PrimarySize - 1)))
	{
		warnf(TEXT("XeSetRingBufferParameters: Primary must be power of two (%d)!"), PrimarySize);
		return;
	}

	RBParams.PrimarySize = Align(PrimarySize, GPU_COMMAND_BUFFER_ALIGNMENT);
	if (PrimarySize != RBParams.PrimarySize)
	{
		warnf(TEXT("XeSetRingBufferParameters: Primary size adjusted for alignment. From %d to %d"),
			PrimarySize, RBParams.PrimarySize);
	}

	RBParams.SecondarySize = Align(SecondarySize, GPU_COMMAND_BUFFER_ALIGNMENT);
	if (SecondarySize != RBParams.SecondarySize)
	{
		warnf(TEXT("XeSetRingBufferParameters: Secondary size adjusted for alignment. From %d to %d"),
			SecondarySize, RBParams.SecondarySize);
	}
	RBParams.SegmentCount = SegmentCount;

	if (bCallDeviceSet)
	{
		VERIFYD3DRESULT(GDirect3DDevice->SetRingBufferParameters(&RBParams));
	}

	GRingBufferPrimarySize = RBParams.PrimarySize;
	GRingBufferSecondarySize = RBParams.SecondarySize;
	GRingBufferSegmentCount = RBParams.SegmentCount;
}

/**
 *	Retrieve available screen resolutions.
 *
 *	@param	Resolutions			TArray<FScreenResolutionRHI> parameter that will be filled in.
 *	@param	bIgnoreRefreshRate	If TRUE, ignore refresh rates.
 *
 *	@return	UBOOL				TRUE if successfully filled the array
 */
UBOOL RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, UBOOL bIgnoreRefreshRate)
{
	//@todo. Implement 360 version of this function
	return FALSE;
}

/**
 *	Blocks the CPU until the GPU has processed all pending commands and gone idle.
 */
void XeBlockUntilGPUIdle()
{
	RHIKickCommandBuffer();
	GDirect3DDevice->BlockUntilIdle();
}



/**
 * computes a smooth curve x = 0..1 with y(0) = 0, y(1) = 1 and given y'(0), y'(1)
 * y(x) = a * x ^ 3 + b * x ^ 2 + c * x
 * @param x = 0..1
 * @param dy0 y'(0)
 * @param dy1 y'(1)
 */
float PolynomialRamp(float x, float dy0, float dy1)
{
	float a = dy1 + dy0 - 2.0f;
	float b = 3.0f - dy1 - 2.0f * dy0;
	float c = dy0;

	return a * x * x * x + b * x * x + c * x;
}

static float ScaledPolynomialRamp(float t, float dy0, float y1, float dy1)
{
	return PolynomialRamp(t, dy0, dy1 / y1) * y1;
}

/**
 * smooth curve x = 0..1 with y(0) = 0, y(1) = 1 and given y'(1) and y'(ToeWidth..1)
 * smooth non linear adjustment of the lower part (toe) in the x range from 0 to ToeWidth
 * The range from ToeWidth to 1 is linear with y' = y'(1)
 * @param x = 0..1
 * @param ToeWidth >0, <1 (size of the non linear part)
 * @param dy1 y'(1), >0.., 1 results in linear curve
 */
static float RampToeAdjustment(float x, float ToeWidth, float dy0, float dy1)
{
	checkSlow(ToeWidth > 0);
	checkSlow(dy1 > 0);

	if(x < ToeWidth)
	{
		// non linear part
		float ToeY = 1.0f + (ToeWidth - 1.0f) * dy1;

		return ScaledPolynomialRamp(x/ToeWidth, dy0, ToeY, dy1*ToeWidth);
	}
	else
	{
		// linear part
		return 1.0f + (x - 1.0f) * dy1;
	}
}

/**
 * Enables or disables a gamma ramp that compensates for Xbox's default gamma ramp, which makes dark too dark.
 */
void XeSetCorrectiveGammaRamp(UBOOL bUseCorrectiveGammaRamp, FVector Custom)
{
	D3DGAMMARAMP GammaRamp;
	if (bUseCorrectiveGammaRamp)
	{
		if(Custom == FVector(0, 0, 0))
		{
			// We measured the response of a monitor on PC, PS3 and Xbox 360, and found that on Xbox 360, 
			// The darks are much too dark compared to the other platforms.  
			// GameDS provided us with the gamma ramp that the 360 applies to output that causes this discrepancy.
			// This gamma ramp compensates for the 360's default gamma ramp.
			static const WORD XenonTVLumaRamp[256]=
			{
				0, 738, 1476, 2214, 2937, 3575, 4141, 4655, 5128, 5567, 5978, 6367, 6735, 7085, 7421, 7742,
				8051, 8350, 8638, 8917, 9187, 9434, 9694, 9955, 10215, 10475, 10734, 10993, 11251, 11509, 11767, 12025,
				12282, 12539, 12795, 13051, 13307, 13563, 13818, 14073, 14327, 14582, 14836, 15090, 15343, 15596, 15849,
				16102, 16354, 16607, 16858, 17110, 17362, 17613, 17864, 18114, 18365, 18615, 18865, 19115, 19365, 19614,
				19863, 20112, 20361, 20609, 20858, 21106, 21354, 21602, 21849, 22096, 22344, 22591, 22837, 23084, 23330,
				23577, 23823, 24069, 24314, 24560, 24805, 25050, 25295, 25540, 25785, 26030, 26274, 26518, 26762, 27006,
				27250, 27493, 27737, 27980, 28223, 28466, 28709, 28952, 29194, 29437, 29679, 29921, 30163, 30405, 30647,
				30888, 31130, 31371, 31612, 31853, 32094, 32335, 32576, 32816, 33056, 33297, 33537, 33777, 34017, 34257,
				34496, 34736, 34975, 35214, 35454, 35693, 35932, 36170, 36409, 36648, 36886, 37125, 37363, 37601, 37839,
				38077, 38315, 38552, 38790, 39028, 39265, 39502, 39739, 39977, 40213, 40450, 40687, 40924, 41160, 41397,
				41633, 41869, 42106, 42342, 42578, 42814, 43049, 43285, 43521, 43756, 43992, 44227, 44462, 44697, 44932,
				45167, 45402, 45637, 45872, 46106, 46341, 46575, 46809, 47044, 47278, 47512, 47746, 47980, 48214, 48447,
				48681, 48914, 49148, 49381, 49615, 49848, 50081, 50314, 50547, 50780, 51013, 51246, 51478, 51711, 51943,
				52176, 52408, 52640, 52873, 53105, 53337, 53569, 53801, 54032, 54264, 54496, 54727, 54959, 55190, 55422,
				55653, 55884, 56116, 56347, 56578, 56809, 57039, 57270, 57501, 57732, 57962, 58193, 58423, 58654, 58884,
				59114, 59344, 59575, 59805, 60035, 60265, 60494, 60724, 60954, 61184, 61413, 61643, 61872, 62102, 62331,
				62560, 62789, 63019, 63248, 63477, 63706, 63934, 64163, 64392, 64621, 64849, 65078, 65307, 65535, 
			};

			// build a corrective ramp from the XenonTVLumaRamp table
			check(sizeof(GammaRamp.red) == sizeof(XenonTVLumaRamp));
			appMemcpy(GammaRamp.red, XenonTVLumaRamp, sizeof(XenonTVLumaRamp));
			appMemcpy(GammaRamp.green, XenonTVLumaRamp, sizeof(XenonTVLumaRamp));
			appMemcpy(GammaRamp.blue, XenonTVLumaRamp, sizeof(XenonTVLumaRamp));
		}
		else
		{
			// build a default linear ramp
			for(INT i = 0; i < 256; ++i)
			{
				INT value = (INT)(0xffff * RampToeAdjustment((i + 1) / 256.0f, Custom.X, Custom.Y, Custom.Z));

				GammaRamp.red[i] = value;
				GammaRamp.green[i] = value;
				GammaRamp.blue[i] = value;
			}
		}
	}
	else
	{
		// build a default linear ramp
		for (INT i=0; i<256; ++i)
		{
			INT value= (i<<8)+i;

			GammaRamp.red[i] = value;
			GammaRamp.green[i] = value;
			GammaRamp.blue[i] = value;
		}
	}

	GDirect3DDevice->SetGammaRamp(0,0,&GammaRamp);
}
#endif
