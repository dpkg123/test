/*=============================================================================
	PS3Viewport.cpp: FPS3Viewport code.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3Drv.h"

void FPS3Viewport::StaticInit()
{

}

//
//	FPS3Viewport::FPS3Viewport
//
FPS3Viewport::FPS3Viewport(UPS3Client* InClient, FViewportClient* InViewportClient, UINT InSizeX, UINT InSizeY)
: FViewport(InViewportClient)
{
	Client					= InClient;

	for (INT ControllerIndex = 0; ControllerIndex < ARRAY_COUNT(Controllers); ControllerIndex++)
	{
		Controllers[ControllerIndex] = new FPS3Controller(Client, this, ViewportClient, ControllerIndex);
	}

	// Creates the viewport window.
	Resize(InSizeX, InSizeY, TRUE);

	ProcessInput(0.0f);
}

//
//	FPS3Viewport::Destroy
//

void FPS3Viewport::Destroy()
{
	ViewportClient = NULL;

	// Release the viewport RHI.
	UpdateViewportRHI(TRUE, SizeX, SizeY, TRUE);

	for (INT ControllerIndex = 0; ControllerIndex < ARRAY_COUNT(Controllers); ControllerIndex++)
	{
		delete Controllers[ControllerIndex];
	}
}

//
//	FPS3Viewport::Resize
//
void FPS3Viewport::Resize(UINT NewSizeX, UINT NewSizeY, UBOOL NewFullscreen, INT InPosX, INT InPosY)
{
	SizeX					= NewSizeX;
	SizeY					= NewSizeY;

	// if we have a size, update the viewport RHI, otherwise destroy it
	if (NewSizeX && NewSizeY)
	{
		UpdateViewportRHI(FALSE, SizeX, SizeY, TRUE);
	}
	else
	{
		UpdateViewportRHI(TRUE, SizeX, SizeY, TRUE);
	}
}

FViewportFrame* FPS3Viewport::GetViewportFrame()
{
	return this;
}

//
//	FPS3Viewport::KeyState
//
UBOOL FPS3Viewport::KeyState(FName Key) const
{
	// let the controller handle its own input
	for (INT ControllerIndex = 0; ControllerIndex < ARRAY_COUNT(Controllers); ControllerIndex++)
	{
		if ( Controllers[ControllerIndex]->IsKeyPressed(Key) )
		{
			return TRUE;
		}
	}

	return FALSE;
}

//
//	FPS3Viewport::ProcessInput
//
void FPS3Viewport::ProcessInput(FLOAT DeltaTime)
{
	if( !ViewportClient )
	{
		return;
	}

	// let the controller handle its own input
	for (INT ControllerIndex = 0; ControllerIndex < ARRAY_COUNT(Controllers); ControllerIndex++)
	{
		Controllers[ControllerIndex]->Tick(DeltaTime);

		if (IsInGameThread())
		{
			// update vibration if we are running
			if (GWorld && GWorld->HasBegunPlay())
			{
				if( GWorld->IsPaused() )
				{
					// Stop vibration when game is paused
					for(INT i=0; i<GEngine->GamePlayers.Num(); i++)
					{
						ULocalPlayer* LP = GEngine->GamePlayers(i);
						if(LP && LP->Actor && LP->Actor->ForceFeedbackManager)
						{
							LP->Actor->ForceFeedbackManager->bIsPaused = TRUE;
							if( LP->Actor->ForceFeedbackManager )
							{
								LP->Actor->ForceFeedbackManager->ForceClearWaveformData(ControllerIndex);
							}
						}
					}
				}
				else
				{
					// Now update any waveform data
					UForceFeedbackManager* Manager = ViewportClient->GetForceFeedbackManager(ControllerIndex);
					if (Manager != NULL)
					{
						Manager->ApplyForceFeedback(ControllerIndex,DeltaTime);
					}
				}
			}
		}
	}
}



UBOOL FPS3Viewport::IsControllerTiltActive( INT ControllerID ) const 
{ 
	//warnf( TEXT("FPS3Viewport::IsControllerTiltActive CALLED!!! %i" ), Controller->IsControllerTiltActive() );
	return Controllers[ControllerID]->IsControllerTiltActive(); 
}

void FPS3Viewport::SetControllerTiltDesiredIfAvailable( INT ControllerID, UBOOL bActive ) 
{
	//warnf( TEXT("FPS3Viewport::SetControllerTiltActive CALLED!!! %i" ), bActive );
	Controllers[ControllerID]->SetControllerTiltDesiredIfAvailable( bActive ); 
}

void FPS3Viewport::SetControllerTiltActive( INT ControllerID, UBOOL bActive ) 
{
	//warnf( TEXT("FPS3Viewport::SetControllerTiltActive CALLED!!! %i" ), bActive );
	Controllers[ControllerID]->SetControllerTiltActive( bActive ); 
}

void FPS3Viewport::SetOnlyUseControllerTiltInput( INT ControllerID, UBOOL bActive ) 
{
	//warnf( TEXT("FPS3Viewport::SetOnlyUseControllerTiltInput CALLED!!! %i" ), bActive );
	Controllers[ControllerID]->SetOnlyUseControllerTiltInput( bActive ); 
}


/**
* sets whether or not to use the tilt forward and back input controls
**/
void FPS3Viewport::SetUseTiltForwardAndBack( INT ControllerID, UBOOL bActive )
{
	Controllers[ControllerID]->SetUseTiltForwardAndBack( bActive ); 
}	


UBOOL FPS3Viewport::IsKeyboardAvailable( INT ControllerID ) const 
{ 
	//warnf( TEXT("FPS3Viewport::IsKeyboardAvailable CALLED!!! %i" ), Controller->IsKeyboardAvailable() );
	return Controllers[ControllerID]->IsKeyboardAvailable(); 
}

UBOOL FPS3Viewport::IsMouseAvailable( INT ControllerID ) const 
{ 
	//warnf( TEXT("FPS3Viewport::IsMouseAvailable CALLED!!! %i" ), Controller->IsMouseAvailable() );
	return Controllers[ControllerID]->IsMouseAvailable(); 
}

INT FPS3Viewport::GetMouseX()
{
	for ( INT ControllerID = 0; ControllerID < ARRAY_COUNT(Controllers); ControllerID++ )
	{
		if ( IsMouseAvailable(ControllerID) )
		{
			return appTrunc(Controllers[ControllerID]->GetCursorPos().X);
		}
	}

	return 0;
}

INT FPS3Viewport::GetMouseY()
{
	for ( INT ControllerID = 0; ControllerID < ARRAY_COUNT(Controllers); ControllerID++ )
	{
		if ( IsMouseAvailable(ControllerID) )
		{
			return appTrunc(Controllers[ControllerID]->GetCursorPos().Y);
		}
	}

	return 0;
}

void FPS3Viewport::GetMousePos( FIntPoint& MousePosition )
{
	for ( INT ControllerID = 0; ControllerID < ARRAY_COUNT(Controllers); ControllerID++ )
	{
		if ( IsMouseAvailable(ControllerID) )
		{
			MousePosition.X = appTrunc(Controllers[ControllerID]->GetCursorPos().X);
			MousePosition.Y = appTrunc(Controllers[ControllerID]->GetCursorPos().Y);
			break;
		}
	}
}

void FPS3Viewport::SetMouse(INT x, INT y)
{
	for ( INT ControllerID = 0; ControllerID < ARRAY_COUNT(Controllers); ControllerID++ )
	{
		if ( IsMouseAvailable(ControllerID) )
		{
			Controllers[ControllerID]->SetCursorPos(x, y);
			break;
		}
	}
}


