/*=============================================================================
	PS3Viewport.h: Unreal PS3 viewport interface
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PS3VIEWPORT_H__
#define __PS3VIEWPORT_H__

//
//	FPS3Viewport
//
struct FPS3Viewport: public FViewportFrame, public FViewport
{
	// Viewport state.
	UPS3Client*				Client;
	
	UINT					SizeX;
	UINT					SizeY;

	FPS3Controller*			Controllers[PS3_MAX_SUPPORTED_PLAYERS];

	static void StaticInit();
	// Constructor/destructor.
	FPS3Viewport(UPS3Client* InClient, FViewportClient* InViewportClient, UINT InSizeX, UINT InSizeY);

	// FViewportFrame interface.
	virtual void* GetWindow() { return NULL; }

	virtual void LockMouseToWindow(UBOOL bLock) {}
	virtual UBOOL KeyState(FName Key) const;

	virtual UBOOL CaptureJoystickInput(UBOOL Capture) { return TRUE; }

	virtual INT GetMouseX();
	virtual INT GetMouseY();
	virtual void GetMousePos( FIntPoint& MousePosition );
	virtual void SetMouse(INT x, INT y);

	virtual UBOOL MouseHasMoved() { return TRUE; }

	virtual UINT GetSizeX() const { return SizeX; }
	virtual UINT GetSizeY() const { return SizeY; }

	virtual void InvalidateDisplay() {}

	virtual void GetHitProxyMap(UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<HHitProxy*>& OutMap) {}
	virtual void Invalidate() {}

	// FViewport interface.
	/**
	 *	Sets a viewport client if one wasn't provided at construction time.
	 *	For the PS3, we also need to set it on the controllers!
	 *	@param InViewportClient	- The viewport client to set.
	 **/
	virtual void SetViewportClient( FViewportClient* InViewportClient )
	{
		FViewport::SetViewportClient(InViewportClient);
		for (INT ControllerIndex = 0; ControllerIndex < ARRAY_COUNT(Controllers); ControllerIndex++)
		{
			if (Controllers[ControllerIndex])
			{
				Controllers[ControllerIndex]->SetViewportClient(InViewportClient);
			}
		}
	}

	virtual FViewport* GetViewport() { return this; }
	virtual UBOOL IsFullscreen() { return TRUE; }
	virtual void SetName(const TCHAR* NewName) {}
	virtual void Resize(UINT NewSizeX,UINT NewSizeY,UBOOL NewFullscreen,INT InPosX = -1, INT InPosY = -1);
	virtual FViewportFrame* GetViewportFrame();
	void ProcessInput(FLOAT DeltaTime);

	/**
	 * @return whether or not this Controller has Tilt Turned on
	 **/
	virtual UBOOL IsControllerTiltActive( INT ControllerID ) const;

	/**
	 * sets whether or not the the player wants to utilize the Tilt functionality
	 **/
	virtual void SetControllerTiltDesiredIfAvailable( INT ControllerID, UBOOL bActive );

	/**
	 * sets whether or not the Tilt functionality is turned on
	 **/
	virtual void SetControllerTiltActive( INT ControllerID, UBOOL bActive );

	/**
	 * sets whether or not to ONLY use the tilt input controls
	 **/
	virtual void SetOnlyUseControllerTiltInput( INT ControllerID, UBOOL bActive );

	/**
	 * sets whether or not to use the tilt forward and back input controls
	 **/
	virtual void SetUseTiltForwardAndBack( INT ControllerID, UBOOL bActive );

	/**
	 * @return whether or not this Controller has a keyboard available to be used
	 **/
	virtual UBOOL IsKeyboardAvailable( INT ControllerID ) const;

	/**
	 * @return whether or not this Controller has a mouse available to be used
	**/
	virtual UBOOL IsMouseAvailable( INT ControllerID ) const;

	// FPS3Viewport interface.	
	void Destroy();
};

#endif
