/*=============================================================================
	PS3RHIPrivate.h: Private PS3 RHI definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PS3RHIPRIVATE_H__
#define __PS3RHIPRIVATE_H__

// Definitions.

// Dependencies

#include "Engine.h"
#include "PS3RHI.h"

//
// Globals.
//
#if USE_PS3_RHI

extern FPS3RHIPixelShader*	GCurrentPixelShader;
extern FPS3RHIVertexShader* GCurrentVertexShader;
extern INT					GVertexInputsUsed[16];
extern DWORD				GEnabledTextures;
extern UBOOL				GThrottleTextureQuads;
extern FPS3RHITexture *		GCurrentTexture[16];
extern INT					GNumFatTexturesUsed;
extern INT					GNumSemiFatTexturesUsed;

#endif

#endif // USE_PS3_RHI
