/*=============================================================================
	XeLaunch.cpp: Game launcher.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "LaunchPrivate.h"

/* Include this here to get access to the LINK_AGAINST_PROFILING_D3D_LIBRARIES define */
#include "XeD3DDrv.h"

/*-----------------------------------------------------------------------------
	Linking mojo.
-----------------------------------------------------------------------------*/

#if _DEBUG

#pragma comment(lib, "xapilibd.lib")
#pragma comment(lib, "d3d9d.lib")

#elif FINAL_RELEASE

#pragma comment(lib, "xapilib.lib")
#pragma comment(lib, "d3d9ltcg.lib")

#elif LINK_AGAINST_PROFILING_D3D_LIBRARIES
#pragma comment(lib, "xapilibi.lib")
#pragma comment(lib, "d3d9i.lib")

#else

#pragma comment(lib, "xapilib.lib")
#pragma comment(lib, "d3d9.lib")

#endif

/**
 * This can be overridden by a #define in the XeCR code (Warfare) but otherwise
 * will function normally for games other than WarGame. 
 *
 * @see XeViewport.cpp
 **/
DWORD UnrealXInputGetState( DWORD ControllerIndex, PXINPUT_STATE CurrentInput )
{
	// Try the controller first and then the big button
	DWORD Return = XInputGetStateEx(ControllerIndex,XINPUT_FLAG_GAMEPAD,CurrentInput);
	if (Return != ERROR_SUCCESS)
	{
		// Check big button controller
		if (XInputGetStateEx(ControllerIndex,XINPUT_FLAG_BIGBUTTON,CurrentInput) == ERROR_SUCCESS)
		{
			Return = ERROR_SUCCESS;
		}
	}
	return Return;
}

/** Global engine loop object												*/
FEngineLoop							EngineLoop;

#if USE_XeD3D_RHI
/** Global D3D device, residing in non- clobbered segment					*/
extern IDirect3DDevice9*			GDirect3DDevice;
#endif // USE_XeD3D_RHI

/*-----------------------------------------------------------------------------
	Guarded code.
-----------------------------------------------------------------------------*/

static void GuardedMain( const TCHAR* CmdLine )
{
	CmdLine = RemoveExeName(CmdLine);
	EngineLoop.PreInit( CmdLine );
	EngineLoop.Init( );

	while( !GIsRequestingExit )
	{
		{
			// Start tracing game thread if requested.
			appStartCPUTrace( NAME_Game, FALSE, TRUE, 40, NULL );

			// Main game thread tick.
			{
				SCOPE_CYCLE_COUNTER(STAT_FrameTime);
				EngineLoop.Tick();
			}

			appStopCPUTrace( NAME_Game );
		}
#if STATS
		// Write all stats for this frame out
		GStatManager.AdvanceFrame();

		if(GIsThreadedRendering)
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND(AdvanceFrame,{RenderingThreadAdvanceFrame();});
		}
#endif
	}

	EngineLoop.Exit();
}

/*-----------------------------------------------------------------------------
	Main.
-----------------------------------------------------------------------------*/

void __cdecl main()
{
	// default to no game
	appStrcpy(GGameName, TEXT("None"));

	TCHAR* CommandLine = NULL;

	// see if image was relaunched and was passed any launch data 
	DWORD LaunchDataSize=0;
	const SIZE_T LaunchCmdLineSize = 4096;
	TCHAR* LaunchCmdLine = new TCHAR[LaunchCmdLineSize];
	if( ERROR_SUCCESS == XGetLaunchDataSize( &LaunchDataSize ) )
	{
		// should always be true unless we are coming from the demo launcher
		check( LaunchDataSize == sizeof(FXenonLaunchData) );

		// get the launch data that was passed to this exe
		FXenonLaunchData XeLaunchData;
		appMemzero( &XeLaunchData,sizeof(FXenonLaunchData) );
        XGetLaunchData( &XeLaunchData, LaunchDataSize );

		// tack on dummy exe name to command line 
		appStrcat( LaunchCmdLine, LaunchCmdLineSize, TEXT("dummy ") );
		appStrcat( LaunchCmdLine, LaunchCmdLineSize, ANSI_TO_TCHAR( XeLaunchData.CmdLine ) );

		// set cmd line
		CommandLine = LaunchCmdLine;
	}
	else
	{
		// Command line contains executable name (though not fully qualified)
		appStrcpy( LaunchCmdLine, LaunchCmdLineSize, ANSI_TO_TCHAR( GetCommandLine() ) );
		CommandLine = LaunchCmdLine;
	}

	// Perform initial platform initialization.
	appXenonInit( CommandLine );

#if _TICKET_TRACKER_
	LoadTicketTrackerSettings();
#endif

#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	// Initialize remote debugging tool.
	HRESULT extern __stdcall DebugConsoleCmdProcessor( const CHAR* Command, CHAR* Response, DWORD ResponseLen, PDM_CMDCONT pdmcc );
	verify( SUCCEEDED( DmRegisterCommandProcessor( "UNREAL", DebugConsoleCmdProcessor ) ));
#endif

	// Begin guarded code.
	GIsStarted = 1;
	GIsGuarded = 0; //@todo xenon: "guard" up GuardedMain at some point for builds running outside the debugger
	GuardedMain( CommandLine );
	GIsGuarded = 0;

	// Final shut down.
	appExit();
	GIsStarted = 0;

#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	// We're done - reboot.
	DmReboot( DMBOOT_TITLE );
#endif
}

/**
 * System memory allocator.
 *
 * These are overridden such that all memory allocations from xenon libs are 
 * routed through the UnrealEngine's memory tracking.
 */

#if STATS_FAST
/** Size of allocations routed through XMemAlloc. */
SIZE_T	XMemVirtualAllocationSize = 0;
SIZE_T	XMemPhysicalAllocationSize = 0;

SIZE_T XMemAllocsById[eXALLOCAllocatorId_MsReservedMax - eXALLOCAllocatorId_MsReservedMin + 1] = { 0 };
TCHAR* XMemAllocIdDescription[eXALLOCAllocatorId_MsReservedMax - eXALLOCAllocatorId_MsReservedMin + 1] =
{
	TEXT( "D3D" ),
	TEXT( "D3DX" ),
	TEXT( "XAUDIO" ),
	TEXT( "XAPI" ),
	TEXT( "XACT" ),
	TEXT( "XBOXKERNEL" ),
	TEXT( "XBDM" ),
	TEXT( "XGRAPHICS" ),
	TEXT( "XONLINE" ),
	TEXT( "XVOICE" ),
	TEXT( "XHV" ),
	TEXT( "USB" ),
	TEXT( "XMV" ),
	TEXT( "SHADERCOMPILER" ),
	TEXT( "XUI" ),
	TEXT( "XASYNC" ),
	TEXT( "XCAM" ),
	TEXT( "XVIS" ),
	TEXT( "XIME" ),
	TEXT( "XFILECACHE" ),
	TEXT( "XRN" ),
	TEXT( "XMCORE" ),
	TEXT( "XMASSIVE" ),
	TEXT( "XAUDIO2" ),
	TEXT( "XAVATAR" ),
	TEXT( "XLSP" ),
	TEXT( "D3DAlloc" ),
	TEXT( "NUISPEECH" ),
	TEXT( "NuiApi" ),
	TEXT( "NuiIdentity" ),
	TEXT( "XTweak" ),
	TEXT( "XAMCOMMON" ),
	TEXT( "NUIUI" ),
	TEXT( "LUA" ),
	TEXT( "*Unknown*" ),
};

checkAtCompileTime( eXALLOCAllocatorID_XMASSIVE == 150, PleaseCheckeXALLOCAllocatorIdEnums );
checkAtCompileTime( eXALLOCAllocatorId_LUA == 161, PleaseCheckeXALLOCAllocatorIdEnums );
#endif // STATS_FAST

LPVOID WINAPI XMemAlloc( SIZE_T Size, DWORD Attributes )
{
	void* Address = NULL;
	Address = XMemAllocDefault( Size, Attributes );

#if !FINAL_RELEASE
	if( Address == NULL )
	{
		appErrorf(TEXT("OOM: XMemAlloc failed to allocate %i bytes %5.2f kb %5.2f mb [%x]"), Size, Size/1024.0f, Size/1024.0f/1024.0f, Attributes);
	}
#endif

#if STATS_FAST
	SIZE_T AllocationSize = XMemSize( Address, Attributes );
	if( Attributes & XALLOC_MEMTYPE_HEAP )
	{
		XMemVirtualAllocationSize += AllocationSize;
	}
	else
	{
		XMemPhysicalAllocationSize += AllocationSize;
	}

	INT AllocationId = ( ( Attributes >> 16 ) & 0xff ) - eXALLOCAllocatorId_MsReservedMin;
	check( AllocationId >= 0 && AllocationId < eXALLOCAllocatorId_MsReservedMax - eXALLOCAllocatorId_MsReservedMin );
	XMemAllocsById[AllocationId] += AllocationSize;
#endif
	return Address;
}

VOID WINAPI XMemFree( PVOID Address, DWORD Attributes )
{
#if STATS_FAST
	if( Address )
	{
		SIZE_T AllocationSize = XMemSize( Address, Attributes );
		if( Attributes & XALLOC_MEMTYPE_HEAP )
		{
			XMemVirtualAllocationSize -= AllocationSize;
		}
		else
		{
			XMemPhysicalAllocationSize -= AllocationSize;
		}

		INT AllocationId = ( ( Attributes >> 16 ) & 0xff ) - eXALLOCAllocatorId_MsReservedMin;
		check( AllocationId >= 0 && AllocationId < eXALLOCAllocatorId_MsReservedMax - eXALLOCAllocatorId_MsReservedMin );
		XMemAllocsById[AllocationId] -= AllocationSize;
	}
#endif
	XMemFreeDefault( Address, Attributes );
}

SIZE_T WINAPI XMemSize( PVOID Address, DWORD Attributes )
{
	SIZE_T AllocationSize = 0;
	AllocationSize = XMemSizeDefault( Address, Attributes );
	return AllocationSize;
}

