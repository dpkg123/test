/*=============================================================================
	PS3RHIViewport.h: PS3 viewport RHI definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if USE_PS3_RHI

#ifndef __PS3RHIVIEWPORT_H__
#define __PS3RHIVIEWPORT_H__

class FPS3RHIViewport : public FRefCountedObject
{
public:

	FPS3RHIViewport(UINT InSizeX, UINT InSizeY);
	~FPS3RHIViewport();

	// Accessors.

	UINT GetSizeX() const { return SizeX; }
	UINT GetSizeY() const { return SizeY; }

	/**
	 * Retrieve one of the frame buffer objects
	 */
	FSurfaceRHIRef GetSurface(DWORD Index)
	{
		check(Index < ARRAY_COUNT(FrameBuffers));
		return FrameBuffers[Index];
	}

protected:
	/** Viewport size - better be GPS3Gcm->ScreenSize! */
	UINT SizeX;
	UINT SizeY;

	/** Reference to the 2 color buffer surfaces created in PS3Gcm */
	FSurfaceRHIRef		FrameBuffers[2];
};

typedef TPS3ResourceRef<FPS3RHIViewport> FViewportRHIRef;
typedef FPS3RHIViewport* FViewportRHIParamRef;


#endif // __PS3RHIVIEWPORT_H__

#endif // USE_PS3_RHI
