/*=============================================================================
	PreviewScene.cpp: Preview scene implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAudioDeviceClasses.h"

FPreviewScene::FPreviewScene(const FRotator& LightRotation,
							 FLOAT SkyBrightness,
							 FLOAT LightBrightness,
							 UBOOL bAlwaysAllowAudioPlayback,
							 UBOOL bForceMipsResident)
	: bForceAllUsedMipsResident(bForceMipsResident) 
{
	Scene = AllocateScene( NULL, bAlwaysAllowAudioPlayback, FALSE );

	SkyLight = ConstructObject<USkyLightComponent>(USkyLightComponent::StaticClass());
	SkyLight->Brightness = SkyBrightness;
	SkyLight->LightColor = FColor(255,255,255);
	AddComponent(SkyLight, FMatrix::Identity);

	DirectionalLight = ConstructObject<UDirectionalLightComponent>(UDirectionalLightComponent::StaticClass());
	DirectionalLight->Brightness = LightBrightness;
	DirectionalLight->LightColor = FColor(255,255,255);
	DirectionalLight->LightShadowMode = LightShadow_Normal;
	DirectionalLight->bUseImageReflectionSpecular = TRUE;
	AddComponent(DirectionalLight, FRotationMatrix(LightRotation));

	LineBatcher = ConstructObject<ULineBatchComponent>(ULineBatchComponent::StaticClass());
	AddComponent(LineBatcher, FMatrix::Identity);
}

FPreviewScene::~FPreviewScene()
{
	// Stop any audio components playing in this scene
	if( GEngine->Client && GEngine->Client->GetAudioDevice() )
	{
		GEngine->Client->GetAudioDevice()->Flush( GetScene() );
	}

	// Remove all the attached components
	for( INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++ )
	{
		UActorComponent* Component = Components( ComponentIndex );

		if (bForceAllUsedMipsResident)
		{
			// Remove the mip streaming override on the mesh to be removed
			UMeshComponent* pMesh = Cast<UMeshComponent>(Component);
			if (pMesh != NULL)
			{
				pMesh->SetTextureForceResidentFlag(FALSE);
			}
		}

		Component->ConditionalDetach();
	}

	Scene->Release();
}

void FPreviewScene::AddComponent(UActorComponent* Component,const FMatrix& LocalToWorld)
{
	Components.AddUniqueItem(Component);
	Component->ConditionalAttach(Scene,NULL,LocalToWorld);
	
	// APEX object has to reattach to make effective
	if(Component->bNeedsReattach)
	{
		const UBOOL bWillReattach = TRUE;
		Component->ConditionalDetach( bWillReattach );
		Component->ConditionalAttach(Scene,NULL,LocalToWorld);
	}

	if (bForceAllUsedMipsResident)
	{
		// Add a mip streaming override to the new mesh
		UMeshComponent* pMesh = Cast<UMeshComponent>(Component);
		if (pMesh != NULL)
		{
			pMesh->SetTextureForceResidentFlag(TRUE);
		}
	}
}

void FPreviewScene::RemoveComponent(UActorComponent* Component)
{
	Component->ConditionalDetach();
	Components.RemoveItem(Component);

	if (bForceAllUsedMipsResident)
	{
		// Remove the mip streaming override on the old mesh
		UMeshComponent* pMesh = Cast<UMeshComponent>(Component);
		if (pMesh != NULL)
		{
			pMesh->SetTextureForceResidentFlag(FALSE);
		}
	}
}

FArchive& operator<<(FArchive& Ar,FPreviewScene& P)
{
	return Ar << P.Components << P.DirectionalLight << P.SkyLight;
}

void FPreviewScene::ClearLineBatcher()
{
	if (LineBatcher != NULL)
	{
		if (LineBatcher->BatchedPoints.Num())
		{
			LineBatcher->BatchedPoints.Empty();
		}

		if (LineBatcher->BatchedLines.Num())
		{
			// clear any lines rendered the previous frame
			LineBatcher->BatchedLines.Empty();
		}

		//RemoveComponent(LineBatcher);
		LineBatcher->BeginDeferredReattach();
	}
}

/** Accessor for finding the current direction of the preview scene's DirectionalLight. */
FRotator FPreviewScene::GetLightDirection()
{
	FMatrix ParentToWorld = FMatrix(
		FPlane(+0,+0,+1,+0),
		FPlane(+0,+1,+0,+0),
		FPlane(+1,+0,+0,+0),
		FPlane(+0,+0,+0,+1)
		) * DirectionalLight->LightToWorld;
		

	return ParentToWorld.Rotator();
}

/** Function for modifying the current direction of the preview scene's DirectionalLight. */
void FPreviewScene::SetLightDirection(const FRotator& InLightDir)
{
	FRotationTranslationMatrix NewLightTM( InLightDir, DirectionalLight->LightToWorld.GetOrigin() );
	DirectionalLight->ConditionalUpdateTransform(NewLightTM);
}

void FPreviewScene::SetLightBrightness(FLOAT LightBrightness)
{
	DirectionalLight->PreEditChange(NULL);
	DirectionalLight->Brightness = LightBrightness;
	DirectionalLight->PostEditChange();
}

void FPreviewScene::SetSkyBrightness(FLOAT SkyBrightness)
{
	SkyLight->PreEditChange(NULL);
	SkyLight->Brightness = SkyBrightness;
	SkyLight->PostEditChange();
}
