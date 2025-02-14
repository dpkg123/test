/*=============================================================================
	DynamicRHI.cpp: Dynamically bound Render Hardware Interface implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if USE_DYNAMIC_RHI

// External dynamic RHI factory functions.
extern FDynamicRHI* NullCreateRHI();
#if PLATFORM_DESKTOP && !USE_NULL_RHI
	#if _WINDOWS
		extern FDynamicRHI* D3D9CreateRHI();
		extern FDynamicRHI* D3D11CreateRHI();
		extern UBOOL IsDirect3D11Supported(UBOOL& OutSupportsD3D11Features);
		#if USE_DYNAMIC_ES2_RHI
			extern FDynamicRHI* ES2CreateRHI();
		#endif
	#endif
	extern FDynamicRHI* OpenGLCreateRHI();
#endif

#if PLATFORM_DESKTOP
/** In bytes. */
INT GCurrentTextureMemorySize = 0;
/** In bytes. 0 means unlimited. */
INT GTexturePoolSize = 0 * 1024 * 1024;
/** Whether to read the texture pool size from engine.ini on PC. Can be turned on with -UseTexturePool on the command line. */
UBOOL GReadTexturePoolSizeFromIni = FALSE;
#endif

// Globals.
FDynamicRHI* GDynamicRHI = NULL;

void RHIInit()
{
	if(!GDynamicRHI)
	{		
#if USE_NULL_RHI
		// Use the null RHI explicitly.
		GDynamicRHI = NullCreateRHI();
		GUsingNullRHI = TRUE;
#else
		const TCHAR* CmdLine = appCmdLine();
		FString Token = ParseToken(CmdLine, FALSE);

		if ( ParseParam(appCmdLine(),TEXT("UseTexturePool")) )
		{
			GReadTexturePoolSizeFromIni = TRUE;
		}

		if(ParseParam(appCmdLine(),TEXT("nullrhi")) || GIsUCC || Token == TEXT("SERVER"))
		{
			// Use the null RHI if it was specified on the command line, or if a commandlet is running.
			GDynamicRHI = NullCreateRHI();
			GUsingNullRHI = TRUE;
		}
#if _WINDOWS
		else
		{
#if USE_DYNAMIC_ES2_RHI
			if( ParseParam( appCmdLine(), TEXT("es2") ) ||
				ParseParam( appCmdLine(), TEXT("simmobile") ) )
			{
				GDynamicRHI = ES2CreateRHI();
				// we don't allow texture streaming with ES2
				GUseTextureStreaming = FALSE;
			}
			else
#endif
			{
				// command line overrides
				const UBOOL bForceD3D9 = ParseParam(appCmdLine(),TEXT("d3d9")) || ParseParam(appCmdLine(),TEXT("sm3")) || ParseParam(appCmdLine(),TEXT("dx9"));
				const UBOOL bForceD3D11 = ParseParam(appCmdLine(),TEXT("d3d11")) || ParseParam(appCmdLine(),TEXT("sm5")) || ParseParam(appCmdLine(),TEXT("dx11"));
				const UBOOL bForceOpenGL = ParseParam(appCmdLine(),TEXT("opengl"));
				
				if ((bForceD3D11 && bForceD3D9) || (bForceD3D11 && bForceOpenGL) || (bForceD3D9 && bForceOpenGL))
				{
					appErrorf(TEXT("-d3d9, -d3d11 and -opengl are mutually exclusive options, but more than one was specified on the command-line."));
				}

				// ini file enables
				const UBOOL bAllowD3D11 = GSystemSettings.bAllowD3D11;
				const UBOOL bAllowOpenGL = GSystemSettings.bAllowOpenGL;
				UBOOL bEditorUsesD3D11 = FALSE;

				if (GSystemSettings.GetIsEditor())
				{
					GConfig->GetBool( TEXT("EditorFrame"), TEXT("EditorUsesD3D11"), bEditorUsesD3D11, GEditorUserSettingsIni );
				}

				// machine spec enables
				// Only check for supported if the RHI is allowed or forced so we don't have a dependency on the dll unless we need it
				UBOOL bSupportsD3D11Features = FALSE;
				UBOOL bSupportsD3D11RHI = (bForceD3D11 || bAllowD3D11 || bEditorUsesD3D11) && IsDirect3D11Supported(bSupportsD3D11Features);

				// These are handy for QA and others to know why they aren't getting the proper API.
				if(bForceD3D11 && !bSupportsD3D11RHI)
				{
					warnf(NAME_Warning,TEXT("Command line -d3d11/-sm5 set, but D3D11 is not supported on this machine.  Will fallback to older API."));
				}

				// if we are forcing d3d11 or not forcing d3d9 and we can run the d3d11 path
				if(!bForceD3D9 && bSupportsD3D11RHI && bSupportsD3D11Features)
				{
					GDynamicRHI = D3D11CreateRHI();
				}
				// if we are forcing opengl or not forcing d3d9 and we can run the opengl path
				else if(bForceOpenGL || (!bForceD3D9 && bAllowOpenGL))
				{
					GDynamicRHI = OpenGLCreateRHI();
				}
				else
				{
					// If neither D3D11 nor OpenGL were desired, allowed, or supported, try the D3D9 RHI.
					GDynamicRHI = D3D9CreateRHI();
				}
			}
		}
#elif PLATFORM_MACOSX
		GDynamicRHI = OpenGLCreateRHI();
#endif // _WINDOWS
#endif // USE_NULL_RHI
		check(GDynamicRHI);
	}
}

void RHIExit()
{
	// Destruct the dynamic RHI.
	delete GDynamicRHI;
	GDynamicRHI = NULL;
}


#else

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
INT DynamicRHILinkerHelper;

#endif // USE_DYNAMIC_RHI


#if !CONSOLE || USE_NULL_RHI

/**
 * Defragment the texture pool.
 */
void appDefragmentTexturePool()
{
}

/**
 * Checks if the texture data is allocated within the texture pool or not.
 */
UBOOL appIsPoolTexture( FTextureRHIParamRef TextureRHI )
{
	return FALSE;
}

/**
 * Log the current texture memory stats.
 *
 * @param Message	This text will be included in the log
 */
void appDumpTextureMemoryStats(const TCHAR* /*Message*/)
{
}

#endif	//#if !CONSOLE
