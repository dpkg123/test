/*=============================================================================
	MaterialShared.cpp: Shared material implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineTextureClasses.h"
#include "TessellationRendering.h"

IMPLEMENT_CLASS(UMaterialInterface);
IMPLEMENT_CLASS(AMaterialInstanceActor);


FArchive& operator<<(FArchive& Ar, FMaterial::FTextureLookup& Ref)
{
	Ref.Serialize( Ar );
	return Ar;
}

FMaterialViewRelevance UMaterialInterface::GetViewRelevance()
{
	// Find the interface's concrete material.
	const UMaterial* Material = GetMaterial();
	if(Material)
	{
		const UBOOL bIsTranslucent = IsTranslucentBlendMode((EBlendMode)Material->BlendMode);
		const UBOOL bIsLit = Material->LightingModel != MLM_Unlit;
		// Determine the material's view relevance.
		FMaterialViewRelevance MaterialViewRelevance;
		MaterialViewRelevance.bOpaque = !bIsTranslucent;
		MaterialViewRelevance.bMasked = Material->bIsMasked;
		MaterialViewRelevance.bTranslucency = bIsTranslucent;
		MaterialViewRelevance.bDistortion = Material->HasDistortion();
		MaterialViewRelevance.bOneLayerDistortionRelevance = bIsTranslucent && Material->bUseOneLayerDistortion;
		MaterialViewRelevance.bInheritDominantShadowsRelevance = bIsTranslucent && Material->bTranslucencyInheritDominantShadowsFromOpaque;
		MaterialViewRelevance.bLit = bIsLit;
		MaterialViewRelevance.bUsesSceneColor = Material->UsesSceneColor();
		MaterialViewRelevance.bSceneTextureRenderBehindTranslucency = Material->bSceneTextureRenderBehindTranslucency && Material->UsesSceneColor();
		MaterialViewRelevance.bDynamicLitTranslucencyPrepass = bIsTranslucent && bIsLit && Material->bUseLitTranslucencyDepthPass;
		MaterialViewRelevance.bDynamicLitTranslucencyPostRenderDepthPass = bIsTranslucent && Material->bUseLitTranslucencyPostRenderDepthPass;
		MaterialViewRelevance.bSoftMasked = Material->BlendMode == BLEND_SoftMasked;
		MaterialViewRelevance.bTranslucencyDoF = bIsTranslucent && Material->bAllowTranslucencyDoF;
		MaterialViewRelevance.bSeparateTranslucency = bIsTranslucent && Material->EnableSeparateTranslucency;
		return MaterialViewRelevance;
	}
	else
	{
		return FMaterialViewRelevance();
	}
}


/**
 * UObject: Performs operations after the object is loaded
 */
void UMaterialInterface::PostLoad ()
{
	// Call parent implementation
	Super::PostLoad();

	if (FlattenedTexture_DEPRECATED != NULL)
	{
		MobileBaseTexture = FlattenedTexture_DEPRECATED;
		FlattenedTexture_DEPRECATED = NULL;
	}

	// Backwards compatibility for deprecated property names
	{
		// bUseMobileVertexSpecular was renamed to bUseMobileSpecular
		if( bUseMobileVertexSpecular_DEPRECATED )
		{
			bUseMobileSpecular = TRUE;
		}
	}
}


INT UMaterialInterface::GetWidth() const
{
	return ME_PREV_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}

INT UMaterialInterface::GetHeight() const
{
	return ME_PREV_THUMBNAIL_SZ+ME_CAPTION_HEIGHT+(ME_STD_BORDER*2);
}


/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UMaterialInterface::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

	// remove the flattened texture reference when stripping for non-ES2 platforms
	if (!(PlatformsToKeep & (UE3::PLATFORM_OpenGLES2 | UE3::PLATFORM_NGP)))
	{
		// auto-flattened textures aren't marked as RF_Standalone, so by NULLing it out here, there will be 
		// no reference to the texture so it won't be saved
		MobileBaseTexture = NULL;
		MobileDetailTexture = NULL;
		MobileEmissiveTexture = NULL;
		MobileMaskTexture = NULL;
		MobileNormalTexture = NULL;
		MobileEnvironmentTexture = NULL;
	}
}


/**
 * @return the flattened texture for the material
 */
UTexture* UMaterialInterface::GetMobileTexture(const INT MobileTextureUnit)
{
	switch( MobileTextureUnit )
	{
		case Base_MobileTexture:
			{
				UTexture* BaseTexture = MobileBaseTexture;

				// If no base texture was assigned, then fall back to the default texture.  Mobile materials
				// are always expecting a valid base texture.
				if( MobileBaseTexture == NULL )
				{
					BaseTexture = GEngine->DefaultTexture;
				}
				return BaseTexture;
			}

		case Detail_MobileTexture:
			return MobileDetailTexture;

		case Environment_MobileTexture:
			return MobileEnvironmentTexture;

		case Normal_MobileTexture:
			return MobileNormalTexture;

		case Mask_MobileTexture:
			return MobileMaskTexture;

		case Emissive_MobileTexture:
			return MobileEmissiveTexture;
	}

	return NULL;
}



/**
 * Forces the streaming system to disregard the normal logic for the specified duration and
 * instead always load all mip-levels for all textures used by this material.
 *
 * @param ForceDuration				- Number of seconds to keep all mip-levels in memory, disregarding the normal priority logic.
 * @param CinematicTextureGroups	- Bitfield indicating which texture groups that use extra high-resolution mips
 */
void UMaterialInterface::SetForceMipLevelsToBeResident( UBOOL OverrideForceMiplevelsToBeResident, UBOOL bForceMiplevelsToBeResidentValue, FLOAT ForceDuration, INT CinematicTextureGroups )
{
	TArray<UTexture*> Textures;
	
	GetUsedTextures(Textures, GCurrentMaterialPlatform);
	for ( INT TextureIndex=0; TextureIndex < Textures.Num(); ++TextureIndex )
	{
		UTexture2D* Texture = Cast<UTexture2D>(Textures(TextureIndex));
		if ( Texture )
		{
			//warnf( TEXT("SetForceMipLevelsToBeResident: %s %d %d %f"), *Texture->GetFullName(), OverrideForceMiplevelsToBeResident, bForceMiplevelsToBeResidentValue, ForceDuration );
			if (ForceDuration >= 0.0f)
			{
				Texture->SetForceMipLevelsToBeResident( ForceDuration, CinematicTextureGroups );
			}
			if (OverrideForceMiplevelsToBeResident)
			{
				Texture->bForceMiplevelsToBeResident = bForceMiplevelsToBeResidentValue;
			}
		}
	}
}

UBOOL UMaterialInterface::IsReadyForFinishDestroy()
{
	UBOOL bIsReady = Super::IsReadyForFinishDestroy();
	bIsReady = bIsReady && ParentRefFence.GetNumPendingFences() == 0; 
	return bIsReady;
}

void UMaterialInterface::BeginDestroy()
{
	ParentRefFence.BeginFence();

	Super::BeginDestroy();
}

void UMaterialInterface::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// fix up guid if it's not been set
	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
	{
		LightingGuid = appCreateGuid();
	}	
}

void UMaterialInterface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// flush the lighting guid on all changes
	LightingGuid = appCreateGuid();

	LightmassSettings.EmissiveBoost = Max(LightmassSettings.EmissiveBoost, 0.0f);
	LightmassSettings.DiffuseBoost = Max(LightmassSettings.DiffuseBoost, 0.0f);
	LightmassSettings.SpecularBoost = Max(LightmassSettings.SpecularBoost, 0.0f);
	LightmassSettings.ExportResolutionScale = Clamp(LightmassSettings.ExportResolutionScale, 0.0f, 16.0f);
	LightmassSettings.DistanceFieldPenumbraScale = Clamp(LightmassSettings.DistanceFieldPenumbraScale, 0.01f, 100.0f);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** Controls discarding of shaders whose source .usf file has changed since the shader was compiled. */
UBOOL ShouldReloadChangedShaders()
{
	static UBOOL bReloadChangedShaders = FALSE;
	static UBOOL bInitialized = FALSE;
	if(!bInitialized)
	{
		bInitialized = TRUE;

#if !CONSOLE && !UE3_LEAN_AND_MEAN
		// Don't allow automatically recompiling changed shaders when using seek-free loading.
		// get the option to skip shaders whose source files have changed since they were compiled
		GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("AutoReloadChangedShaders"), bReloadChangedShaders, GEngineIni );
#endif
	}
	return bReloadChangedShaders;
}

/**
 *	Serialize the given shader map to the given archive
 *
 *	@param	InShaderMap				The shader map to serialize; when loading will be NULL.
 *	@param	Ar						The archvie to serialize it to.
 *
 *	@return	FMaterialShaderMap*		The shader map serialized
 */
FMaterialShaderMap* UMaterialInterface::SerializeShaderMap(FMaterialShaderMap* InShaderMap, FArchive& Ar)
{
	FMaterialShaderMap* ReturnShaderMap = NULL;
	if (Ar.IsSaving())
	{
		INT HasShaderMap = 0;
		if (InShaderMap)
		{
			// Serialize an flag indicating there is a shader map (and the platform flag)
			HasShaderMap = 1;
			Ar << HasShaderMap;

			// Serialize the shaders themselves...
			TMap<FGuid,FShader*> Shaders;
			InShaderMap->GetShaderList(Shaders);

			// Serialize the shaders themselves
			SerializeShaders(Shaders, Ar);

			// Serialize the shader map!
			InShaderMap->Serialize(Ar);
		}
		else
		{
			Ar << HasShaderMap;
		}

		ReturnShaderMap = InShaderMap;
	}
	else
	if (Ar.IsLoading())
	{
		INT HasShaderMap = 0;
		Ar << HasShaderMap;
		if (HasShaderMap == 1)
		{
			TMap<FGuid,FShader*> TempShaders;
			SerializeShaders(TempShaders, Ar);

			// Serialize the shader map!
			FMaterialShaderMap* TempShaderMap = new FMaterialShaderMap();
			check(TempShaderMap);
			TempShaderMap->Serialize(Ar);

			// Register it w/ the shaders...
			FMaterialShaderMap* CheckShaderMap = TempShaderMap->AttemptRegistration();
			if (CheckShaderMap != TempShaderMap)
			{
				delete TempShaderMap;
				TempShaderMap = CheckShaderMap;
			}

			ReturnShaderMap = TempShaderMap;
		}
	}

	return ReturnShaderMap;
}

//
//	UMaterialInterface::execGetMaterial
//
void UMaterialInterface::execGetMaterial(FFrame& Stack,RESULT_DECL)
{
	P_FINISH;
	*(UMaterial**) Result = GetMaterial();
}

void UMaterialInterface::execGetPhysicalMaterial(FFrame& Stack,RESULT_DECL)
{
	P_FINISH;
	*(UPhysicalMaterial**) Result = GetPhysicalMaterial();
}

UBOOL UMaterialInterface::GetVectorParameterValue(FName ParameterName, FLinearColor& OutValue)
{
	return FALSE;
}

UBOOL UMaterialInterface::GetScalarParameterValue(FName ParameterName, FLOAT& OutValue)
{
	return FALSE;
}

UBOOL UMaterialInterface::GetScalarCurveParameterValue(FName ParameterName, FInterpCurveFloat& OutValue)
{
	return FALSE;
}

UBOOL UMaterialInterface::GetVectorCurveParameterValue(FName ParameterName, FInterpCurveVector& OutValue)
{
	return FALSE;
}

UBOOL UMaterialInterface::GetTextureParameterValue(FName ParameterName, UTexture*& OutValue)
{
	return FALSE;
}

UBOOL UMaterialInterface::GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue,INT& OutFontPage)
{
	return FALSE;
}

UBOOL UMaterialInterface::GetParameterDesc(FName ParamaterName,FString& OutDesc)
{
	return FALSE;
}

/** 
 * Returns True if this material has a valid physical material mask setup.
 */
UBOOL UMaterialInterface::HasValidPhysicalMaterialMask() const
{
	const UTexture2D* MaskTexture = GetPhysicalMaterialMaskTexture();
	const INT UVChannel = GetPhysMaterialMaskUVChannel();

	return	UVChannel >= 0 && // UV Channel must be a valid index
			UVChannel < MAX_TEXCOORDS && // UV Channel must be a valid index
			MaskTexture != NULL && // Texture mask cannot be null
			MaskTexture->Format == PF_A1 && // Texture mask must be a 1 bit texture
			MaskTexture->AccessSystemMemoryData().Num() > 0 && // system memory data for the mask texture must exist
			GetBlackPhysicalMaterial() != NULL && // Black physical material cannot be null
			GetWhitePhysicalMaterial() != NULL; // White physical material cannot be null
}

/**
 * Determines the texel on the physical material mask that was hit and returns the physical material corresponding to hit texel's color
 * 
 * @param HitUV the UV that was hit during collision.
 */
UPhysicalMaterial* UMaterialInterface::DetermineMaskedPhysicalMaterialFromUV( const FVector2D& UV ) const
{
	// The return value 
	UPhysicalMaterial* RetMaterial = NULL;
	if( HasValidPhysicalMaterialMask() )
	{	
		UTexture2D* MaskTexture = GetPhysicalMaterialMaskTexture();
		// If we are at this point the mask texture should be valid
		check( MaskTexture );

		// The width and height of the mask texture
		const UINT MaskWidth = (UINT)(MaskTexture->GetSurfaceWidth());
		const UINT MaskHeight = (UINT)(MaskTexture->GetSurfaceHeight());

		// The number of bytes  needed to store all 1 bit pixels in a line of pixels is the width of the image divided by the number of bits in a byte
		const UINT BytesPerLine = MaskWidth / 8;

		// Nearest texel (point sample texture lookup)
		FLOAT BoundX = UV.X - appFloor(UV.X); // convert UVs from large range into 0-1
		const UINT NearestTexelX = Clamp<UINT>(appRound( BoundX * MaskWidth ), 0, MaskWidth - 1);
		FLOAT BoundY = UV.Y - appFloor(UV.Y);
		const UINT NearestTexelY = Clamp<UINT>(appRound( BoundY * MaskHeight ), 0, MaskHeight - 1);	

		// The index to the byte in the image data that contains the nearest texel
		const UINT ByteIndexX = NearestTexelX / 8;
		
		// Get the texel data to determine the color of the hit texel
		const TArray<BYTE>& Texels = MaskTexture->AccessSystemMemoryData();
		
		// The byte that contains the hit texel 
		const BYTE TexelByte = Texels( NearestTexelY * BytesPerLine + ByteIndexX );
		
			
		// Get the actual bit index that was hit.  We will use this index to look at the actual bit in the Byte that contains our hit texel.
		// If the texel index is less than 8, just use that as the bit index, 
		// otherwise the bit that corresponds to the hit texel is: texel index - byte containing the pixel * num bits in a byte).  
		const BYTE TexelBitIndex = NearestTexelX < 8 ? NearestTexelX : (NearestTexelX - ByteIndexX*8);

		// Determine state of the bit, (account for most significant bit as left most texel)
		if( ( TexelByte & ( 1<<(7-TexelBitIndex) ) ) != 0 )
		{
			// The bit is set which means its black
			// Texel is black at this spot so return the black phys mat
			RetMaterial = GetBlackPhysicalMaterial();
		}
		else
		{
			// The bit is not set which means its white
			// Texel is white at this spot so return the white phys mat
			RetMaterial = GetWhitePhysicalMaterial();
		}
	}
	else
	{
		// Invalid physical material mask setup.  Log a warning
		GWarn->Logf(TEXT("Physical material mask is not valid.  The masked texture must be PF_A1 format, and all physical material mask entries on the material must be valid."));
	}

	return RetMaterial;
}

TLinkedList<FMaterialUniformExpressionType*>*& FMaterialUniformExpressionType::GetTypeList()
{
	static TLinkedList<FMaterialUniformExpressionType*>* TypeList = NULL;
	return TypeList;
}

TMap<FName,FMaterialUniformExpressionType*>& FMaterialUniformExpressionType::GetTypeMap()
{
	static TMap<FName,FMaterialUniformExpressionType*> TypeMap;

	// Move types from the type list to the type map.
	TLinkedList<FMaterialUniformExpressionType*>* TypeListLink = GetTypeList();
	while(TypeListLink)
	{
		TLinkedList<FMaterialUniformExpressionType*>* NextLink = TypeListLink->Next();
		FMaterialUniformExpressionType* Type = **TypeListLink;

		TypeMap.Set(FName(Type->Name),Type);
		TypeListLink->Unlink();
		delete TypeListLink;

		TypeListLink = NextLink;
	}

	return TypeMap;
}

FMaterialUniformExpressionType::FMaterialUniformExpressionType(
	const TCHAR* InName,
	SerializationConstructorType InSerializationConstructor
	):
	Name(InName),
	SerializationConstructor(InSerializationConstructor)
{
	// Put the type in the type list until the name subsystem/type map are initialized.
	(new TLinkedList<FMaterialUniformExpressionType*>(this))->Link(GetTypeList());
}

FArchive& operator<<(FArchive& Ar,FMaterialUniformExpression*& Ref)
{
	// Serialize the expression type.
	if(Ar.IsSaving())
	{
		// Write the type name.
		check(Ref);
		FName TypeName(Ref->GetType()->Name);
		Ar << TypeName;
	}
	else if(Ar.IsLoading())
	{
		// Read the type name.
		FName TypeName = NAME_None;
		Ar << TypeName;

		// Find the expression type with a matching name.
		FMaterialUniformExpressionType* Type = FMaterialUniformExpressionType::GetTypeMap().FindRef(TypeName);
		check(Type);

		// Construct a new instance of the expression type.
		Ref = (*Type->SerializationConstructor)();
	}

	// Serialize the expression.
	Ref->Serialize(Ar);

	return Ar;
}

FArchive& operator<<(FArchive& Ar,FMaterialUniformExpressionTexture*& Ref)
{
	Ar << (FMaterialUniformExpression*&)Ref;
	return Ar;
}


UBOOL FShaderFrequencyUniformExpressions::IsEmpty() const
{
	return UniformVectorExpressions.Num() == 0 &&
		UniformScalarExpressions.Num() == 0 &&
		Uniform2DTextureExpressions.Num() == 0;
}

UBOOL FShaderFrequencyUniformExpressions::operator==(const FShaderFrequencyUniformExpressions& ReferenceSet) const
{
	if (UniformVectorExpressions.Num() != ReferenceSet.UniformVectorExpressions.Num() ||
		UniformScalarExpressions.Num() != ReferenceSet.UniformScalarExpressions.Num() ||
		Uniform2DTextureExpressions.Num() != ReferenceSet.Uniform2DTextureExpressions.Num())
	{
		return FALSE;
	}

	for (INT i = 0; i < UniformVectorExpressions.Num(); i++)
	{
		if (!UniformVectorExpressions(i)->IsIdentical(ReferenceSet.UniformVectorExpressions(i)))
		{
			return FALSE;
		}
	}

	for (INT i = 0; i < UniformScalarExpressions.Num(); i++)
	{
		if (!UniformScalarExpressions(i)->IsIdentical(ReferenceSet.UniformScalarExpressions(i)))
		{
			return FALSE;
		}
	}

	for (INT i = 0; i < Uniform2DTextureExpressions.Num(); i++)
	{
		if (!Uniform2DTextureExpressions(i)->IsIdentical(ReferenceSet.Uniform2DTextureExpressions(i)))
		{
			return FALSE;
		}
	}

	return TRUE;
}

void FShaderFrequencyUniformExpressions::ClearDefaultTextureValueReferences()
{
	for (INT i = 0; i < UniformVectorExpressions.Num(); i++)
	{
		if (UniformVectorExpressions(i)->GetTextureUniformExpression())
		{
			UniformVectorExpressions(i)->GetTextureUniformExpression()->ClearDefaultTextureValueReference();
		}
	}
	for (INT i = 0; i < UniformScalarExpressions.Num(); i++)
	{
		if (UniformScalarExpressions(i)->GetTextureUniformExpression())
		{
			UniformScalarExpressions(i)->GetTextureUniformExpression()->ClearDefaultTextureValueReference();
		}
	}
	for (INT i = 0; i < Uniform2DTextureExpressions.Num(); i++)
	{
		if (Uniform2DTextureExpressions(i)->GetTextureUniformExpression())
		{
			Uniform2DTextureExpressions(i)->GetTextureUniformExpression()->ClearDefaultTextureValueReference();
		}
	}
}

void FShaderFrequencyUniformExpressions::GetInputsString(EShaderFrequency Frequency, FString& InputsString) const
{
	const TCHAR* FrequencyName = GetShaderFrequencyName(Frequency);
	for(INT VectorIndex = 0;VectorIndex < UniformVectorExpressions.Num();VectorIndex++)
	{
		InputsString += FString::Printf(TEXT("float4 Uniform%sVector_%i;\r\n"), FrequencyName, VectorIndex);
	}
	for(INT ScalarIndex = 0;ScalarIndex < UniformScalarExpressions.Num();ScalarIndex += 4)
	{
		InputsString += FString::Printf(TEXT("float4 Uniform%sScalars_%i;\r\n"), FrequencyName, ScalarIndex / 4);
	}
	for(INT TextureIndex = 0;TextureIndex < Uniform2DTextureExpressions.Num();TextureIndex++)
	{
		InputsString += FString::Printf(TEXT("sampler2D %sTexture2D_%i;\r\n"), FrequencyName, TextureIndex);
	}
}

void FUniformExpressionSet::Serialize(FArchive& Ar)
{
	Ar << PixelExpressions;
	Ar << UniformCubeTextureExpressions;
	Ar << VertexExpressions;
#if WITH_D3D11_TESSELLATION
	Ar << HullExpressions;
	Ar << DomainExpressions;
#else
	FShaderFrequencyUniformExpressions Dummy0;
	FShaderFrequencyUniformExpressions Dummy1;
	Ar << Dummy0;
	Ar << Dummy1;
#endif
}

UBOOL FUniformExpressionSet::IsEmpty() const
{
	return PixelExpressions.IsEmpty() &&
		UniformCubeTextureExpressions.Num() == 0 &&
#if WITH_D3D11_TESSELLATION
		HullExpressions.IsEmpty() &&
		DomainExpressions.IsEmpty() &&
#endif
		VertexExpressions.IsEmpty();
}

UBOOL FUniformExpressionSet::operator==(const FUniformExpressionSet& ReferenceSet) const
{
	if (UniformCubeTextureExpressions.Num() != ReferenceSet.UniformCubeTextureExpressions.Num())
	{
		return FALSE;
	}

	for (INT i = 0; i < UniformCubeTextureExpressions.Num(); i++)
	{
		if (!UniformCubeTextureExpressions(i)->IsIdentical(ReferenceSet.UniformCubeTextureExpressions(i)))
		{
			return FALSE;
		}
	}

	return PixelExpressions == ReferenceSet.PixelExpressions &&
#if WITH_D3D11_TESSELLATION
		HullExpressions == ReferenceSet.HullExpressions &&
		DomainExpressions == ReferenceSet.DomainExpressions &&
#endif
		VertexExpressions == ReferenceSet.VertexExpressions;
}

void FUniformExpressionSet::ClearDefaultTextureValueReferences()
{
	PixelExpressions.ClearDefaultTextureValueReferences();
	for (INT i = 0; i < UniformCubeTextureExpressions.Num(); i++)
	{
		if (UniformCubeTextureExpressions(i)->GetTextureUniformExpression())
		{
			UniformCubeTextureExpressions(i)->GetTextureUniformExpression()->ClearDefaultTextureValueReference();
		}
	}
#if WITH_D3D11_TESSELLATION
	HullExpressions.ClearDefaultTextureValueReferences();
	DomainExpressions.ClearDefaultTextureValueReferences();
#endif
	VertexExpressions.ClearDefaultTextureValueReferences();
}

FString FUniformExpressionSet::GetSummaryString() const
{
	return FString::Printf(TEXT("(%u pixel vectors, %u pixel scalars, %u 2d tex, %u cube tex, %u vertex vector, %u vertex scalar)"),
		PixelExpressions.UniformVectorExpressions.Num(),
		PixelExpressions.UniformScalarExpressions.Num(),
		PixelExpressions.Uniform2DTextureExpressions.Num(),
		UniformCubeTextureExpressions.Num(),
		VertexExpressions.UniformVectorExpressions.Num(),
		VertexExpressions.UniformScalarExpressions.Num()
		);
}

void FUniformExpressionSet::GetInputsString(FString& InputsString) const
{
	PixelExpressions.GetInputsString(SF_Pixel, InputsString);
	for(INT TextureIndex = 0;TextureIndex < UniformCubeTextureExpressions.Num();TextureIndex++)
	{
		InputsString += FString::Printf(TEXT("samplerCUBE PixelTextureCube_%i;\r\n"),TextureIndex);
	}
	VertexExpressions.GetInputsString(SF_Vertex, InputsString);
#if WITH_D3D11_TESSELLATION
	HullExpressions.GetInputsString(SF_Hull, InputsString);
	DomainExpressions.GetInputsString(SF_Domain, InputsString);
#endif
}

FShaderFrequencyUniformExpressions& FUniformExpressionSet::GetExpresssions(EShaderFrequency Frequency)
{
	checkAtCompileTime(6 == SF_NumFrequencies, Bad_EShaderFrequency);
	switch(Frequency)
	{
	case SF_Vertex: return VertexExpressions;
#if WITH_D3D11_TESSELLATION
	case SF_Hull: return HullExpressions;
	case SF_Domain: return DomainExpressions;
	case SF_Geometry:
	case SF_Compute:
		// not implement yet
#endif
		check(0);
	}
	return PixelExpressions;
}

const FShaderFrequencyUniformExpressions& FUniformExpressionSet::GetExpresssions(EShaderFrequency Frequency) const
{
	checkAtCompileTime(6 == SF_NumFrequencies, Bad_EShaderFrequency);
	switch(Frequency)
	{
	case SF_Vertex: return VertexExpressions;
#if WITH_D3D11_TESSELLATION
	case SF_Hull: return HullExpressions;
	case SF_Domain: return DomainExpressions;
	case SF_Geometry:
	case SF_Compute:
		// not implement yet
#endif
		check(0);
	}
	return PixelExpressions;
}

INT FMaterialCompiler::Errorf(const TCHAR* Format,...)
{
	TCHAR	ErrorText[2048];
	GET_VARARGS( ErrorText, ARRAY_COUNT(ErrorText), ARRAY_COUNT(ErrorText)-1, Format, Format );
	return Error(ErrorText);
}

//
//	FExpressionInput::Compile
//

INT FExpressionInput::Compile(FMaterialCompiler* Compiler)
{
	if(Expression)
	{
		if(Mask)
		{
			INT ExpressionResult = Compiler->CallExpression(Expression,Compiler);
			if(ExpressionResult != INDEX_NONE)
			{
				return Compiler->ComponentMask(
					ExpressionResult,
					MaskR,MaskG,MaskB,MaskA
					);
			}
			else
			{
				return INDEX_NONE;
			}
		}
		else
		{
			return Compiler->CallExpression(Expression,Compiler);
		}
	}
	else
		return INDEX_NONE;
}

//
//	FColorMaterialInput::Compile
//

INT FColorMaterialInput::Compile(FMaterialCompiler* Compiler,const FColor& Default)
{
	if(UseConstant)
	{
		FLinearColor	LinearColor(Constant);
		return Compiler->Constant3(LinearColor.R,LinearColor.G,LinearColor.B);
	}
	else if(Expression)
	{
		INT ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex == INDEX_NONE)
		{
			FLinearColor	LinearColor(Default);
			return Compiler->Constant3(LinearColor.R,LinearColor.G,LinearColor.B);
		}
		else
		{
			return ResultIndex;
		}
	}
	else
	{
		FLinearColor	LinearColor(Default);
		return Compiler->Constant3(LinearColor.R,LinearColor.G,LinearColor.B);
	}
}

//
//	FScalarMaterialInput::Compile
//

INT FScalarMaterialInput::Compile(FMaterialCompiler* Compiler,FLOAT Default)
{
	if(UseConstant)
	{
		return Compiler->Constant(Constant);
	}
	else if(Expression)
	{
		INT ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex == INDEX_NONE)
		{
			return Compiler->Constant(Default);
		}
		else
		{
			return ResultIndex;
		}
	}
	else
	{
		return Compiler->Constant(Default);
	}
}

//
//	FVectorMaterialInput::Compile
//

INT FVectorMaterialInput::Compile(FMaterialCompiler* Compiler,const FVector& Default)
{
	if(UseConstant)
	{
		return Compiler->Constant3(Constant.X,Constant.Y,Constant.Z);
	}
	else if(Expression)
	{
		INT ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex == INDEX_NONE)
		{
			return Compiler->Constant3(Default.X,Default.Y,Default.Z);
		}
		else
		{
			return ResultIndex;
		}
	}
	else
	{
		return Compiler->Constant3(Default.X,Default.Y,Default.Z);
	}
}

//
//	FVector2MaterialInput::Compile
//

INT FVector2MaterialInput::Compile(FMaterialCompiler* Compiler,const FVector2D& Default)
{
	if(UseConstant)
	{
		return Compiler->Constant2(Constant.X,Constant.Y);
	}
	else if(Expression)
	{
		INT ResultIndex = FExpressionInput::Compile(Compiler);
		if (ResultIndex == INDEX_NONE)
		{
			return Compiler->Constant2(Default.X,Default.Y);
		}
		else
		{
			return ResultIndex;
		}
	}
	else
	{
		return Compiler->Constant2(Default.X,Default.Y);
	}
}

EMaterialValueType GetMaterialPropertyType(EMaterialProperty Property)
{
	switch(Property)
	{
	case MP_EmissiveColor: return MCT_Float3;
	case MP_Opacity: return MCT_Float;
	case MP_OpacityMask: return MCT_Float;
	case MP_Distortion: return MCT_Float2;
	case MP_TwoSidedLightingMask: return MCT_Float3;
	case MP_DiffuseColor: return MCT_Float3;
	case MP_DiffusePower: return MCT_Float;
	case MP_SpecularColor: return MCT_Float3;
	case MP_SpecularPower: return MCT_Float;
	case MP_Normal: return MCT_Float3;
	case MP_CustomLighting: return MCT_Float3;
	case MP_CustomLightingDiffuse: return MCT_Float3;
	case MP_AnisotropicDirection: return MCT_Float3;
	case MP_WorldPositionOffset: return MCT_Float3;
	case MP_WorldDisplacement : return MCT_Float3;
	case MP_TessellationFactors: return MCT_Float2;
	case MP_SubsurfaceInscatteringColor: return MCT_Float3;
	case MP_SubsurfaceAbsorptionColor: return MCT_Float3;
	case MP_SubsurfaceScatteringRadius: return MCT_Float;
	};
	return MCT_Unknown;
}

/** Returns the shader frequency corresponding to the given material input. */
EShaderFrequency GetMaterialPropertyShaderFrequency(EMaterialProperty Property)
{
	if (Property == MP_WorldPositionOffset)
	{
		return SF_Vertex;
	}
	else if(Property == MP_WorldDisplacement)
	{
		return SF_Domain;
	}
	else if(Property == MP_TessellationFactors)
	{
		return SF_Hull;
	}
	return SF_Pixel;
}

/**
 * Null any material expression references for this material
 */
void FMaterial::RemoveExpressions()
{
	TextureDependencyLengthMap.Empty();
}

EMaterialTessellationMode FMaterial::GetD3D11TessellationMode() const 
{ 
	return MTM_NoTessellation; 
};

/** 
 * Helper function that fills in the mobile texture transform array based on the Material passed in
 * @param InMaterial - Material with the parameters set
 * @param OutTransform - The Transform to fill in with panner and rotator information
 */
void GetMobileTextureTransformHelper(const UMaterial* InMaterial, TMatrix<3,3>& OutTransform)
{
	FLOAT MaterialTime = GCurrentTime - GStartTime;
	static UBOOL bStaticStopTime = FALSE;
	if (bStaticStopTime)
	{
		MaterialTime = 0;
	}

	FLOAT Cosine = appCos(InMaterial->RotateSpeed*MaterialTime);
	FLOAT Sine = appSin(InMaterial->RotateSpeed*MaterialTime);

	//Scalar Values
	FLOAT ScaleX = InMaterial->FixedScaleX;
	FLOAT ScaleY = InMaterial->FixedScaleY;
	if ((InMaterial->SineScaleX!=0.0f) || (InMaterial->SineScaleY!=0.0f))
	{
		FLOAT SineCurveValue = appSin(InMaterial->SineScaleFrequencyMultipler*MaterialTime);
		ScaleX += InMaterial->SineScaleX*SineCurveValue;
		ScaleY += InMaterial->SineScaleY*SineCurveValue;
	}

	//Cosine  Sine    0
	//-Sine   Cosine  0
	//Tx      Ty      1
	FVector2D Col0(Cosine, -Sine);
	FVector2D Col1(Sine, Cosine);

	//Rotator
	OutTransform.M[0][0] = Cosine*ScaleX;
	OutTransform.M[0][1] =   Sine*ScaleX;
	OutTransform.M[1][0] =  -Sine*ScaleY;
	OutTransform.M[1][1] = Cosine*ScaleY;

	//to ensure that the center offset is respected, we have to move it back to the origin, rotate, and THEN move back to the offset that has been rotated
	FVector2D OriginalOffsetCenter (InMaterial->TransformCenterX, InMaterial->TransformCenterY);
	FVector2D RotatedOffsetCenter  (ScaleX*(OriginalOffsetCenter|Col0), ScaleY*(OriginalOffsetCenter|Col1));
	FVector2D FinalRotationOffset = OriginalOffsetCenter - RotatedOffsetCenter;

	//Translate
	OutTransform.M[2][0] = FinalRotationOffset.X + appFractional(InMaterial->PannerSpeedX*MaterialTime);		//Tx
	OutTransform.M[2][1] = FinalRotationOffset.Y + appFractional(InMaterial->PannerSpeedY*MaterialTime);		//Ty

	//Final Matrix looks like
	//Cosine*ScaleX                          Sine*ScaleX                              0
	//-Sine*ScaleY                           Cosine**ScaleY                           0
	//-ScaleX*(Tx*Cosine + Ty*Sine) + Tx     -ScaleY*(Tx*Cosine + -Ty*Sine) + Ty      1

	//Set the rest to identity
	OutTransform.M[0][2] = 0.0f;
	OutTransform.M[1][2] = 0.0f;
	OutTransform.M[2][2] = 1.0f;
}

/**
 * Internal helper functions to fill in the vertex params struct
 * @param InMaterial - The Material to draw the parameters from
 * @param OutVertexParams - Vertex parameter structure to pass to the shader system
 */
void FMaterial::FillMobileMaterialVertexParams (const UMaterial* InMaterial, FMobileMaterialVertexParams& OutVertexParams) const
{
	OutVertexParams.bUseLighting = ( InMaterial->LightingModel != MLM_Unlit );
	OutVertexParams.bTextureTransformEnabled = InMaterial->bUseMobileTextureTransform;
	OutVertexParams.TextureTransformTarget = (EMobileTextureTransformTarget)InMaterial->MobileTextureTransformTarget;
	if (OutVertexParams.bTextureTransformEnabled)
	{
		GetMobileTextureTransformHelper(InMaterial, OutVertexParams.TextureTransform);
	}
	OutVertexParams.bUseTextureBlend = (InMaterial->MobileDetailTexture != NULL);
	OutVertexParams.bLockTextureBlend = InMaterial->bLockColorBlending;
	OutVertexParams.TextureBlendFactorSource = (EMobileTextureBlendFactorSource)InMaterial->MobileTextureBlendFactorSource;

	OutVertexParams.bUseEmissive =
		InMaterial->MobileEmissiveColorSource == MECS_Constant ||
		( InMaterial->MobileEmissiveColorSource == MECS_EmissiveTexture && InMaterial->MobileEmissiveTexture != NULL ) ||
		( InMaterial->MobileEmissiveColorSource == MECS_BaseTexture && InMaterial->MobileBaseTexture != NULL );
	OutVertexParams.EmissiveColorSource = (EMobileEmissiveColorSource)InMaterial->MobileEmissiveColorSource;
	OutVertexParams.EmissiveMaskSource = (EMobileValueSource)InMaterial->MobileEmissiveMaskSource;
	OutVertexParams.EmissiveColor = InMaterial->MobileEmissiveColor;

	OutVertexParams.bUseNormalMapping = (InMaterial->MobileNormalTexture != NULL);

	OutVertexParams.bUseEnvironmentMapping = (InMaterial->MobileEnvironmentTexture != NULL);

	OutVertexParams.bUseSpecular = InMaterial->bUseMobileSpecular;
	OutVertexParams.bUsePixelSpecular = InMaterial->bUseMobilePixelSpecular;
	OutVertexParams.SpecularColor = InMaterial->MobileSpecularColor;
	OutVertexParams.SpecularPower = InMaterial->MobileSpecularPower;

	OutVertexParams.EnvironmentMaskSource = (EMobileValueSource)InMaterial->MobileEnvironmentMaskSource;
	OutVertexParams.EnvironmentAmount = InMaterial->MobileEnvironmentAmount;
	OutVertexParams.EnvironmentFresnelAmount = InMaterial->MobileEnvironmentFresnelAmount;
	OutVertexParams.EnvironmentFresnelExponent = InMaterial->MobileEnvironmentFresnelExponent;

	OutVertexParams.RimLightingColor = InMaterial->MobileRimLightingColor;
	OutVertexParams.RimLightingStrength = InMaterial->MobileRimLightingStrength;
	OutVertexParams.RimLightingExponent = InMaterial->MobileRimLightingExponent;
	OutVertexParams.RimLightingMaskSource = (EMobileValueSource)InMaterial->MobileRimLightingMaskSource;

	//Wave vertex movement
	OutVertexParams.bWaveVertexMovementEnabled = InMaterial->bUseMobileWaveVertexMovement;
	if (InMaterial->bUseMobileWaveVertexMovement)
	{
		OutVertexParams.VertexMovementTangentFrequencyMultiplier = InMaterial->MobileTangentVertexFrequencyMultiplier;
		OutVertexParams.VertexMovementVerticalFrequencyMultiplier = InMaterial->MobileVerticalFrequencyMultiplier;
		OutVertexParams.MaxVertexMovementAmplitude = InMaterial->MobileMaxVertexMovementAmplitude;
		OutVertexParams.SwayFrequencyMultiplier = InMaterial->MobileSwayFrequencyMultiplier;
		OutVertexParams.SwayMaxAngle = InMaterial->MobileSwayMaxAngle;
	}

	OutVertexParams.bAllowFog = InMaterial->bMobileAllowFog;

	OutVertexParams.MaterialBlendMode = (EBlendMode)InMaterial->BlendMode;

	OutVertexParams.BaseTextureTexCoordsSource = (EMobileTexCoordsSource)InMaterial->MobileBaseTextureTexCoordsSource;
	OutVertexParams.DetailTextureTexCoordsSource = (EMobileTexCoordsSource)InMaterial->MobileDetailTextureTexCoordsSource;
	OutVertexParams.MaskTextureTexCoordsSource = (EMobileTexCoordsSource)InMaterial->MobileMaskTextureTexCoordsSource;

	OutVertexParams.AmbientOcclusionSource = (EMobileAmbientOcclusionSource)InMaterial->MobileAmbientOcclusionSource;

	OutVertexParams.bUseUniformColorMultiply = InMaterial->bUseMobileUniformColorMultiply;
	OutVertexParams.UniformMultiplyColor = InMaterial->DefaultUniformColor;;
	OutVertexParams.bUseVertexColorMultiply = InMaterial->bUseMobileVertexColorMultiply;

	//copy in own name for validation
	InMaterial->GetName(OutVertexParams.MaterialName);
}

/**
 * Internal helper functions to fill in the pixel params struct
 * @param InMaterial - The Material to draw the parameters from
 * @param OutVertexParams - Vertex parameter structure to pass to the shader system
 */
void FMaterial::FillMobileMaterialPixelParams (const UMaterial* InMaterial, FMobileMaterialPixelParams& OutPixelParams) const
{
	OutPixelParams.bBumpOffsetEnabled = InMaterial->bUseMobileBumpOffset;
	if( OutPixelParams.bBumpOffsetEnabled )
	{
		OutPixelParams.BumpReferencePlane = InMaterial->MobileBumpOffsetReferencePlane;
		OutPixelParams.BumpHeightRatio = InMaterial->MobileBumpOffsetHeightRatio;
	}
	OutPixelParams.SpecularMask = static_cast<EMobileSpecularMask>( InMaterial->MobileSpecularMask );

	OutPixelParams.EnvironmentBlendMode = static_cast<EMobileEnvironmentBlendMode>( InMaterial->MobileEnvironmentBlendMode );
	OutPixelParams.EnvironmentColorScale = InMaterial->MobileEnvironmentColor;

}

const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& FMaterial::GetUniform2DTextureExpressions() const 
{ 
	if (ShaderMap)
	{
		return ShaderMap->GetUniform2DTextureExpressions(); 
	}
	static const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> > EmptyExpressions;
	return EmptyExpressions;
}

const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& FMaterial::GetUniformCubeTextureExpressions() const 
{ 
	if (ShaderMap)
	{
		return ShaderMap->GetUniformCubeTextureExpressions(); 
	}
	static const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> > EmptyExpressions;
	return EmptyExpressions;
}

/** This function is intended only for use when cooking in CoderMode! */
void FMaterial::SetShaderMap(FMaterialShaderMap* InMaterialShaderMap)
{
	if (ShaderMap)
	{
		ShaderMap->BeginRelease();
	}
	ShaderMap = InMaterialShaderMap;
}

UBOOL FMaterial::MaterialModifiesMeshPosition() const 
{ 
	return UsesMaterialVertexPositionOffset() || GetD3D11TessellationMode() != MTM_NoTessellation;
}

void FMaterial::AddReferencedObjects(TArray<UObject*>& ObjectArray)
{
	for(TMap<UMaterialExpression*,INT>::TConstIterator DependencyLengthIt(TextureDependencyLengthMap);
		DependencyLengthIt;
		++DependencyLengthIt)
	{
		UObject::AddReferencedObject(ObjectArray,DependencyLengthIt.Key());
	}
	// Add references to all textures referenced by uniform expressions and used for rendering.
	for (INT i = 0; i < UniformExpressionTextures.Num(); i++)
	{
		UObject::AddReferencedObject(ObjectArray,UniformExpressionTextures(i));
	}
}

void FMaterial::Serialize(FArchive& Ar)
{
	Ar << CompileErrors;
	// Cook out editor-only texture dependency map information.
	if( Ar.IsSaving() && (GCookingTarget & UE3::PLATFORM_Stripped) )
	{
		TMap<UMaterialExpression*,INT> DummyMap;
		Ar << DummyMap;
	}
	else
	{
		Ar << TextureDependencyLengthMap;
	}
	Ar << MaxTextureDependencyLength;
	Ar << Id;
	Ar << NumUserTexCoords;
	if (Ar.Ver() < VER_UNIFORM_EXPRESSIONS_IN_SHADER_CACHE)
	{
		// Deserialize the legacy uniform expressions and store them as LegacyUniformExpressions
		LegacyUniformExpressions = new FUniformExpressionSet();
		Ar << LegacyUniformExpressions->PixelExpressions.UniformVectorExpressions;
		Ar << LegacyUniformExpressions->PixelExpressions.UniformScalarExpressions;
		Ar << LegacyUniformExpressions->PixelExpressions.Uniform2DTextureExpressions;
		Ar << LegacyUniformExpressions->UniformCubeTextureExpressions;
		if (Ar.Ver() >= VER_MATERIAL_EDITOR_VERTEX_SHADER)
		{
			Ar << LegacyUniformExpressions->VertexExpressions.UniformVectorExpressions;
			Ar << LegacyUniformExpressions->VertexExpressions.UniformScalarExpressions;
		}
	}
	else
	{
		Ar << UniformExpressionTextures;
	}
	
	UBOOL bUsesSceneColorTemp = bUsesSceneColor;
	Ar << bUsesSceneColorTemp;
	bUsesSceneColor = bUsesSceneColorTemp;

	UBOOL bUsesSceneDepthTemp = bUsesSceneDepth;
	Ar << bUsesSceneDepthTemp;
	bUsesSceneDepth = bUsesSceneDepthTemp;
	if (Ar.Ver() >= VER_DYNAMICPARAMETERS_ADDED)
	{
		UBOOL bUsesDynamicParameterTemp = bUsesDynamicParameter;
		Ar << bUsesDynamicParameterTemp;
		bUsesDynamicParameter = bUsesDynamicParameterTemp;
	}
	if (Ar.Ver() >= VER_MATEXP_LIGHTMAPUVS_ADDED)
	{
		UBOOL bUsesLightmapUVsTemp = bUsesLightmapUVs;
		Ar << bUsesLightmapUVsTemp;
		bUsesLightmapUVs = bUsesLightmapUVsTemp;
	}
	if (Ar.Ver() >= VER_MATERIAL_EDITOR_VERTEX_SHADER)
	{
		UBOOL bUsesMaterialVertexPositionOffsetTemp = bUsesMaterialVertexPositionOffset;
		Ar << bUsesMaterialVertexPositionOffsetTemp;
		bUsesMaterialVertexPositionOffset = bUsesMaterialVertexPositionOffsetTemp;
	}
	Ar << UsingTransforms;
	
	if(Ar.Ver() >= VER_MIN_COMPILEDMATERIAL && Ar.LicenseeVer() >= LICENSEE_VER_MIN_COMPILEDMATERIAL)
	{
		bValidCompilationOutput = TRUE;
	}
	
	Ar << TextureLookups;
	DWORD DummyDroppedFallbackComponents;
	Ar << DummyDroppedFallbackComponents;
}

/** Initializes the material's shader map. */
UBOOL FMaterial::InitShaderMap(EShaderPlatform Platform)
{
	FStaticParameterSet EmptySet(Id);
	return InitShaderMap(&EmptySet, Platform);
}

/** Initializes the material's shader map. */
UBOOL FMaterial::InitShaderMap(FStaticParameterSet* StaticParameters, EShaderPlatform Platform)
{
	UBOOL bSucceeded = FALSE;

	if(!Id.IsValid())
	{
		// If the material doesn't have a valid ID, create a new one.
		// This can happen if it is loaded from an old package or newly created and hasn't had CacheShaders() called yet.
		Id = appCreateGuid();
	}

	//only update the static parameter set's Id if it isn't valid
	if(!StaticParameters->BaseMaterialId.IsValid())
	{
		StaticParameters->BaseMaterialId = Id;
	}

#if !CONSOLE
	// make sure the shader cache is loaded
	GetLocalShaderCache(Platform);
#endif

	if (ShaderMap)
	{
		ShaderMap->BeginRelease();
	}
	// Find the material's cached shader map.
	ShaderMap = FMaterialShaderMap::FindId(*StaticParameters, Platform);
	UBOOL bRequiredRecompile = FALSE;
	if(!bValidCompilationOutput || !ShaderMap || !ShaderMap->IsComplete(this, TRUE))
	{
		if(bValidCompilationOutput)
		{
			const TCHAR* ShaderMapCondition;
			if(ShaderMap)
			{
				ShaderMapCondition = TEXT("Incomplete");
			}
			else
			{
				ShaderMapCondition = TEXT("Missing");
			}
			debugf(TEXT("%s cached shader map for material %s, compiling."),ShaderMapCondition,*GetFriendlyName());
		}
		else
		{
			debugf(TEXT("Material %s has outdated uniform expressions; regenerating."),*GetFriendlyName());
		}

		if (appGetPlatformType() & UE3::PLATFORM_Stripped)
		{
			if (IsSpecialEngineMaterial())
			{
				//assert if the default material's shader map was not found, since it will cause problems later
				appErrorf(TEXT("Failed to find shader map for default material %s!  Please make sure cooking was successful."), *GetFriendlyName());
			}
			else
			{
				debugf(TEXT("Can't compile %s with seekfree loading path on console, will attempt to use default material instead"), *GetFriendlyName());
			}
			// Reset the shader map so the default material will be used.
			ShaderMap = NULL;
		}
		else
		{
			// If there's no cached shader map for this material, compile a new one.
			// This will only happen if the local shader cache happens to get out of sync with a modified material package, which should only
			// happen in exceptional cases; the editor crashed between saving the shader cache and the modified material package, etc.
			bSucceeded = Compile(StaticParameters, Platform, ShaderMap, FALSE);
			bRequiredRecompile = TRUE;
			if(!bSucceeded)
			{
				// If it failed to compile the material, reset the shader map so the material isn't used.
				ShaderMap = NULL;
				if (IsSpecialEngineMaterial())
				{
					// Assert if the default material could not be compiled, since there will be nothing for other failed materials to fall back on.
					appErrorf(TEXT("Failed to compile default material %s!"), *GetFriendlyName());
				}
			}
		}
	}
	else
	{
		check(ShaderMap->IsUniformExpressionSetValid());
		if (LegacyUniformExpressions && ShaderMap->GetUniformExpressionSet().IsEmpty())
		{
			// This material has legacy uniform expressions, so propagate them to the shader map.
			ShaderMap->SetUniformExpressions(*LegacyUniformExpressions);
		}

		// Only initialize the shaders if no recompile was required or if we are not deferring shader compiling
		if (!bRequiredRecompile || !DeferFinishCompiling() && !GShaderCompilingThreadManager->IsDeferringCompilation())
		{
			ShaderMap->BeginInit();
		}
		bSucceeded = TRUE;
	}
	return bSucceeded;
}

/** Flushes this material's shader map from the shader cache if it exists. */
void FMaterial::FlushShaderMap()
{
	if(ShaderMap)
	{
		// If there's a shader map for this material, flush any shaders cached for it.
		UShaderCache::FlushId(ShaderMap->GetMaterialId());
		ShaderMap->BeginRelease();
		ShaderMap = NULL;
	}

}

UBOOL IsTranslucentBlendMode(EBlendMode BlendMode)
{
	return BlendMode != BLEND_Opaque && BlendMode != BLEND_Masked && BlendMode != BLEND_SoftMasked && BlendMode != BLEND_DitheredTranslucent;
}


UBOOL FMaterialResource::IsTwoSided() const { return Material->TwoSided; }
UBOOL FMaterialResource::RenderTwoSidedSeparatePass() const { return Material->TwoSidedSeparatePass && IsTranslucentBlendMode((EBlendMode)Material->BlendMode); }
UBOOL FMaterialResource::RenderLitTranslucencyPrepass() const { return Material->bUseLitTranslucencyDepthPass && IsTranslucentBlendMode((EBlendMode)Material->BlendMode) && Material->LightingModel!=MLM_Unlit; }
UBOOL FMaterialResource::RenderLitTranslucencyDepthPostpass() const { return Material->bUseLitTranslucencyPostRenderDepthPass && IsTranslucentBlendMode((EBlendMode)Material->BlendMode); }
UBOOL FMaterialResource::NeedsDepthTestDisabled() const { return Material->bDisableDepthTest; }
UBOOL FMaterialResource::AllowsFog() const 
{ 
	// Currently not supporting fog on translucent decals WITH skinning as there are not enough vertex shader constants to handle the worst case combinations
	// (GPU skin + morph + translucent + decal + lit)
	return Material->bAllowFog; 
}
UBOOL FMaterialResource::UsesOneLayerDistortion() const { return Material->bUseOneLayerDistortion && IsTranslucentBlendMode((EBlendMode)Material->BlendMode); }
UBOOL FMaterialResource::IsWireframe() const { return Material->Wireframe; }
UBOOL FMaterialResource::IsUsedWithFogVolumes() const { return Material->bUsedWithFogVolumes; }
UBOOL FMaterialResource::IsLightFunction() const { return Material->bUsedAsLightFunction; }
UBOOL FMaterialResource::IsSpecialEngineMaterial() const { return Material->bUsedAsSpecialEngineMaterial; }
UBOOL FMaterialResource::IsTerrainMaterial() const { return FALSE; }
UBOOL FMaterialResource::IsLightmapSpecularAllowed() const { return Material->bAllowLightmapSpecular; }
UBOOL FMaterialResource::HasNormalmapConnected() const { return Material->Normal.Expression != NULL; }
UBOOL FMaterialResource::HasVertexPositionOffsetConnected() const { return Material->WorldPositionOffset.Expression != NULL; }
UBOOL FMaterialResource::AllowTranslucencyDoF() const { return Material->bAllowTranslucencyDoF && IsTranslucentBlendMode((EBlendMode)Material->BlendMode); }
UBOOL FMaterialResource::TranslucencyReceiveDominantShadowsFromStatic() const { return Material->bTranslucencyReceiveDominantShadowsFromStatic && IsTranslucentBlendMode((EBlendMode)Material->BlendMode); }
FString FMaterialResource::GetBaseMaterialPathName() const { return Material->GetPathName(); }

UBOOL FMaterialResource::IsDecalMaterial() const
{
	return FALSE;
}

UBOOL FMaterialResource::IsUsedWithSkeletalMesh() const
{
	return Material->bUsedWithSkeletalMesh;
}

UBOOL FMaterialResource::IsUsedWithTerrain() const
{
	return Material->bUsedWithTerrain;
}

UBOOL FMaterialResource::IsUsedWithLandscape() const
{
	return Material->bUsedWithLandscape;
}

UBOOL FMaterialResource::IsUsedWithFracturedMeshes() const
{
	return Material->bUsedWithFracturedMeshes;
}

UBOOL FMaterialResource::IsUsedWithSpeedTree() const
{
	return Material->bUsedWithSpeedTree;
}

UBOOL FMaterialResource::IsUsedWithParticleSystem() const
{
	return Material->bUsedWithParticleSprites || Material->bUsedWithBeamTrails || Material->bUsedWithParticleSubUV;
}

UBOOL FMaterialResource::IsUsedWithParticleSprites() const
{
	return Material->bUsedWithParticleSprites;
}

UBOOL FMaterialResource::IsUsedWithBeamTrails() const
{
	return Material->bUsedWithBeamTrails;
}

UBOOL FMaterialResource::IsUsedWithParticleSubUV() const
{
	return Material->bUsedWithParticleSubUV;
}

UBOOL FMaterialResource::IsUsedWithStaticLighting() const
{
	return Material->bUsedWithStaticLighting;
}

UBOOL FMaterialResource::IsUsedWithLensFlare() const
{
	return Material->bUsedWithLensFlare;
}

UBOOL FMaterialResource::IsUsedWithGammaCorrection() const
{
	return Material->bUsedWithGammaCorrection;
}

UBOOL FMaterialResource::IsUsedWithInstancedMeshParticles() const
{
	return Material->bUsedWithInstancedMeshParticles;
}

UBOOL FMaterialResource::IsUsedWithFluidSurfaces() const
{
	return Material->bUsedWithFluidSurfaces;
}

UBOOL FMaterialResource::IsUsedWithMaterialEffect() const
{
	return Material->bUsedWithMaterialEffect;
}

UBOOL FMaterialResource::IsUsedWithDecals() const
{
	return Material->bUsedWithDecals;
}

UBOOL FMaterialResource::IsUsedWithMorphTargets() const
{
	return Material->bUsedWithMorphTargets;
}

UBOOL FMaterialResource::IsUsedWithRadialBlur() const
{
	return Material->bUsedWithRadialBlur;
}

UBOOL FMaterialResource::IsUsedWithInstancedMeshes() const
{
	return Material->bUsedWithInstancedMeshes;
}

UBOOL FMaterialResource::IsUsedWithSplineMeshes() const
{
	return Material->bUsedWithSplineMeshes;
}

UBOOL FMaterialResource::IsUsedWithScreenDoorFade() const
{
	return Material->bUsedWithScreenDoorFade;
}

EMaterialTessellationMode FMaterialResource::GetD3D11TessellationMode() const 
{ 
	return (EMaterialTessellationMode)Material->D3D11TessellationMode; 
}

UBOOL FMaterialResource::IsUsedWithAPEXMeshes() const
{
	return Material->bUsedWithAPEXMeshes;
}

/**
 * Should shaders compiled for this material be saved to disk?
 */
UBOOL FMaterialResource::IsPersistent() const { return TRUE; }

EBlendMode FMaterialResource::GetBlendMode() const { return (EBlendMode)Material->BlendMode; }

EMaterialLightingModel FMaterialResource::GetLightingModel() const { return (EMaterialLightingModel)Material->LightingModel; }

UBOOL FMaterialResource::IsMobileTextureCoordinateTransformed () const
{
	return Material->bUseMobileTextureTransform;
}



/** Helper function to accumulate all the material constants */
void GatherMaterialKeyData (ProgramKeyData& MaterialKeyData, const UMaterialInterface* MaterialInterface, const UWorld* InWorld)
{
	BYTE LightingModel = MLM_Unlit;
	BYTE BlendMode = BLEND_Opaque;

	const UMaterial* Material = ConstCast<UMaterial>(MaterialInterface);
	if (Material)
	{
		LightingModel = Material->LightingModel;
		BlendMode = Material->BlendMode;
	}

	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsLightingEnabled, MaterialKeyData, (LightingModel != MLM_Unlit) );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsAlphaTestEnabled, MaterialKeyData, (BlendMode == BLEND_Masked || BlendMode == BLEND_DitheredTranslucent) );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_UsingAdditiveMaterial, MaterialKeyData, (BlendMode == BLEND_Additive) );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_BaseTextureTexCoordsSource, MaterialKeyData, MaterialInterface->MobileBaseTextureTexCoordsSource );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_DetailTextureTexCoordsSource, MaterialKeyData, MaterialInterface->MobileDetailTextureTexCoordsSource );
	if (MaterialInterface->MobileDetailTexture == NULL)
	{
		OVERRIDE_PROGRAM_KEY_VALUE (PKDT_DetailTextureTexCoordsSource, MaterialKeyData, 0);
	}
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_MaskTextureTexCoordsSource, MaterialKeyData, MaterialInterface->MobileMaskTextureTexCoordsSource );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsTextureCoordinateTransformEnabled, MaterialKeyData, MaterialInterface->bUseMobileTextureTransform );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_TextureTransformTarget, MaterialKeyData, MaterialInterface->MobileTextureTransformTarget );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsSpecularEnabled, MaterialKeyData, MaterialInterface->bUseMobileSpecular );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsPixelSpecularEnabled, MaterialKeyData, MaterialInterface->bUseMobilePixelSpecular );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsNormalMappingEnabled, MaterialKeyData, (MaterialInterface->MobileNormalTexture != NULL) );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsEnvironmentMappingEnabled, MaterialKeyData, (MaterialInterface->MobileEnvironmentTexture != NULL) );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_MobileEnvironmentBlendMode, MaterialKeyData, MaterialInterface->MobileEnvironmentBlendMode);
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsMobileEnvironmentFresnelEnabled, MaterialKeyData, (MaterialInterface->MobileEnvironmentFresnelAmount != 0.0f) );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsBumpOffsetEnabled, MaterialKeyData, MaterialInterface->bUseMobileBumpOffset );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsColorTextureBlendingEnabled, MaterialKeyData, (MaterialInterface->MobileDetailTexture != NULL) );
	// if there is a material, like water, that will be broken without this feature, force it to stay as desired and not be overwritten by system settings
	LOCK_PROGRAM_KEY_VALUE(PKDT_IsColorTextureBlendingEnabled, MaterialKeyData, MaterialInterface->bLockColorBlending);

	ASSIGN_PROGRAM_KEY_VALUE( PKDT_TextureBlendFactorSource, MaterialKeyData, MaterialInterface->MobileTextureBlendFactorSource );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsWaveVertexMovementEnabled, MaterialKeyData, MaterialInterface->bUseMobileWaveVertexMovement );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_SpecularMask, MaterialKeyData, MaterialInterface->MobileSpecularMask );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_AmbientOcclusionSource, MaterialKeyData, MaterialInterface->MobileAmbientOcclusionSource );
	//ASSIGN_PROGRAM_KEY_VALUE( PKDT_FixedLightmapScaleFactor, MaterialKeyData, (InWorld ? InWorld->GetWorldInfo()->FixedLightmapScale : 0) );
	//ASSIGN_PROGRAM_KEY_VALUE( PKDT_SimpleLightmapsStoredInLinearSpace, MaterialKeyData, (InWorld ? InWorld->GetWorldInfo()->bSimpleLightmapsStoredInLinearSpace : 0) );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_UseUniformColorMultiply, MaterialKeyData, MaterialInterface->bUseMobileUniformColorMultiply );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_UseVertexColorMultiply, MaterialKeyData, MaterialInterface->bUseMobileVertexColorMultiply );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsRimLightingEnabled, MaterialKeyData, (MaterialInterface->MobileRimLightingStrength != 0.0f) );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_RimLightingMaskSource, MaterialKeyData, MaterialInterface->MobileRimLightingMaskSource );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_EnvironmentMaskSource, MaterialKeyData, MaterialInterface->MobileEnvironmentMaskSource );

	const UBOOL bUseEmissive =
		MaterialInterface->MobileEmissiveColorSource == MECS_Constant ||
		( MaterialInterface->MobileEmissiveColorSource == MECS_EmissiveTexture && MaterialInterface->MobileEmissiveTexture != NULL ) ||
		( MaterialInterface->MobileEmissiveColorSource == MECS_BaseTexture && MaterialInterface->MobileBaseTexture != NULL );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsEmissiveEnabled, MaterialKeyData, bUseEmissive ? 1 : 0 );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_EmissiveColorSource, MaterialKeyData, MaterialInterface->MobileEmissiveColorSource );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_EmissiveMaskSource, MaterialKeyData, MaterialInterface->MobileEmissiveMaskSource );
	if( !bUseEmissive )
	{
		OVERRIDE_PROGRAM_KEY_VALUE( PKDT_EmissiveColorSource, MaterialKeyData, 0 );
		OVERRIDE_PROGRAM_KEY_VALUE( PKDT_EmissiveMaskSource, MaterialKeyData, 0 );
	}
}


void FMaterialResource::FillMobileMaterialVertexParams (FMobileMaterialVertexParams& OutVertexParams) const
{
	//call the base class with an appropriate material
	FMaterial::FillMobileMaterialVertexParams(Material, OutVertexParams);
}

void FMaterialResource::FillMobileMaterialPixelParams (FMobileMaterialPixelParams& OutPixelParams) const
{
	FMaterial::FillMobileMaterialPixelParams(Material, OutPixelParams);
}


QWORD FMaterialResource::GetMobileMaterialSortKey (void) const 
{
	// Fill in a ProgramKeyData structure with the information we have available
	ProgramKeyData MaterialKeyData;
	MaterialKeyData.Start();

	//Reserved for primitive type & platform features
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_PlatformFeatures, MaterialKeyData, 0 );						//IRRELEVANT
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_PrimitiveType, MaterialKeyData, 0 );							//IRRELEVANT

	//World/Misc
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsDepthOnlyRendering, MaterialKeyData, FALSE );						//UNKNOWN
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsGradientFogEnabled, MaterialKeyData, Material->bMobileAllowFog );	//just use this as a sort key IN CASE of using fog
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_ParticleScreenAlignment, MaterialKeyData, 0 );						//UNKNOWN

	//Vertex Factory Flags
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsLightmap, MaterialKeyData, FALSE );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsSkinned, MaterialKeyData, FALSE );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsDecal, MaterialKeyData, FALSE );
	ASSIGN_PROGRAM_KEY_VALUE( PKDT_IsSubUV, MaterialKeyData, FALSE );

	//material provided
	GatherMaterialKeyData(MaterialKeyData, Material, NULL);

	//program type doesn't matter as much here
	MaterialKeyData.Stop();

	// Generate a packed key from the data
	return PackProgramKeyData(MaterialKeyData);
}



FLOAT FMaterialResource::GetOpacityMaskClipValue() const { return Material->OpacityMaskClipValue; }

/**
* Check for distortion use
* @return TRUE if material uses distortion
*/
UBOOL FMaterialResource::IsDistorted() const { return Material->bUsesDistortion && !Material->bUseOneLayerDistortion; }

UBOOL FMaterialResource::HasSubsurfaceScattering() const { return Material->EnableSubsurfaceScattering; }

UBOOL FMaterialResource::HasSeparateTranslucency() const { return Material->EnableSeparateTranslucency; }

/**
 * Check if the material is masked and uses an expression or a constant that's not 1.0f for opacity.
 * @return TRUE if the material uses opacity
 */
UBOOL FMaterialResource::IsMasked() const { return Material->bIsMasked; }

UBOOL FMaterialResource::UsesImageBasedReflections() const { return Material->bUseImageBasedReflections; }

UBOOL FMaterialResource::UsesMaskedAntialiasing() const { return Material->bEnableMaskedAntialiasing; }

FLOAT FMaterialResource::GetImageReflectionNormalDampening() const { return Material->ImageReflectionNormalDampening; }

FLOAT FMaterialResource::GetShadowDepthBias() const { return Material->ShadowDepthBias; }

/** @return TRUE if the author wants the camera vector to be computed per-pixel */
UBOOL FMaterialResource::UsesPerPixelCameraVector() const {	return Material->bPerPixelCameraVector; }

/** @return TRUE if lit translucent objects will cast shadow as if they were masked */
UBOOL FMaterialResource::CastLitTranslucencyShadowAsMasked() const { return IsTranslucentBlendMode((EBlendMode)Material->BlendMode) && Material->bCastLitTranslucencyShadowAsMasked; }

/** Returns TRUE if the material is translucent and wants to inherit dynamic shadows cast by dominant lights onto opaque pixels. */
UBOOL FMaterialResource::TranslucencyInheritDominantShadowsFromOpaque() const 
{ 
	return IsTranslucentBlendMode((EBlendMode)Material->BlendMode) && Material->bTranslucencyInheritDominantShadowsFromOpaque; 
}

FString FMaterialResource::GetFriendlyName() const { return *Material->GetName(); }

/**
 * For UMaterials, this will return the flattened texture for platforms that don't 
 * have full material support
 *
 * @return the FTexture object that represents the flattened texture for this material (can be NULL)
 */
class FTexture* FMaterialResource::GetMobileTexture(const INT MobileTextureUnit) const
{
	UTexture* MobileTexture = Material->GetMobileTexture(MobileTextureUnit);
	return MobileTexture ? MobileTexture->Resource : NULL;
}

/** Allows the resource to do things upon compile. */
UBOOL FMaterialResource::Compile( FStaticParameterSet* StaticParameters, EShaderPlatform Platform, TRefCountPtr<FMaterialShaderMap>& OutShaderMap, UBOOL bForceCompile, UBOOL bDebugDump)
{
	UBOOL bOk;
	STAT(DOUBLE MaterialCompilingTime = 0);
	{
		SCOPE_SECONDS_COUNTER(MaterialCompilingTime);
		check(Material);

		bOk = FMaterial::Compile( StaticParameters, Platform, OutShaderMap, bForceCompile, bDebugDump);

		if ( bOk )
		{
			RebuildTextureLookupInfo( Material );
		}
	}
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_MaterialShaders,(FLOAT)MaterialCompilingTime);
	return bOk;
}

/**
 * Gets instruction counts that best represent the likely usage of this material based on lighting model and other factors.
 * @param Descriptions - an array of descriptions to be populated
 * @param InstructionCounts - an array of instruction counts matching the Descriptions.  
 *		The dimensions of these arrays are guaranteed to be identical and all values are valid.
 */
void FMaterialResource::GetRepresentativeInstructionCounts(TArray<FString> &Descriptions, TArray<INT> &InstructionCounts) const
{
	TArray<FString> ShaderTypeNames;
	TArray<FString> ShaderTypeDescriptions;

	//when adding a shader type here be sure to update FPreviewMaterial::ShouldCache()
	//so the shader type will get compiled with preview materials
	const FMaterialShaderMap* MaterialShaderMap = GetShaderMap();
	if (MaterialShaderMap && MaterialShaderMap->IsCompilationFinalized())
	{
		if (IsUsedWithFogVolumes())
		{
			new (ShaderTypeNames) FString(TEXT("FFogVolumeApplyPixelShader"));
			new (ShaderTypeDescriptions) FString(TEXT("Fog Volume Apply Pixel Shader"));
		}
		else
		{
			if (GetLightingModel() == MLM_Unlit)
			{
				//unlit materials are never lightmapped
				new (ShaderTypeNames) FString(TEXT("TBasePassPixelShaderFNoLightMapPolicyNoSkyLight"));
				new (ShaderTypeDescriptions) FString(TEXT("Base pass shader without light map"));
			}
			else
			{
				if (IsUsedWithParticleSystem())
				{
					// Lit particle materials always use one pass light environments,
					// Which apply a directional and sky light in the base pass.
					new (ShaderTypeNames) FString(TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicySkyLight"));
					new (ShaderTypeDescriptions) FString(TEXT("One pass lit particle shader"));
				}
				else if (IsUsedWithStaticLighting())
				{
					//lit materials are usually lightmapped
					new (ShaderTypeNames) FString(TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicyNoSkyLight"));
					new (ShaderTypeDescriptions) FString(TEXT("Base pass shader with light map"));

					//also show a dynamically lit shader
					new (ShaderTypeNames) FString(TEXT("TLightPixelShaderFPointLightPolicyFNoStaticShadowingPolicy"));
					new (ShaderTypeDescriptions) FString(TEXT("Point light shader"));
				}
				else
				{
					// Not lightmapped, assume lit by a light environment
					new (ShaderTypeNames) FString(TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicySkyLight"));
					new (ShaderTypeDescriptions) FString(TEXT("One pass LightEnv shader"));
				}
			}

			if (IsDistorted())
			{
				//distortion requires an extra pass
				new (ShaderTypeNames) FString(TEXT("TDistortionMeshPixelShader<FDistortMeshAccumulatePolicy>"));
				new (ShaderTypeDescriptions) FString(TEXT("Distortion pixel shader"));
			}

			
			new (ShaderTypeNames) FString(TEXT("TBasePassVertexShaderFNoLightMapPolicyFNoDensityPolicy"));
			new (ShaderTypeDescriptions) FString(TEXT("Vertex shader"));

		}

		const FMeshMaterialShaderMap* MeshShaderMap = MaterialShaderMap->GetMeshShaderMap(&FLocalVertexFactory::StaticType);
		if (MeshShaderMap)
		{
			Descriptions.Empty();
			InstructionCounts.Empty();

			for (INT InstructionIndex = 0; InstructionIndex < ShaderTypeNames.Num(); InstructionIndex++)
			{
				FShaderType* ShaderType = FindShaderTypeByName(*ShaderTypeNames(InstructionIndex));
				if (ShaderType)
				{
					const FShader* Shader = MeshShaderMap->GetShader(ShaderType);
					if (Shader)
					{
						//if the shader was found, add it to the output arrays
						InstructionCounts.Push(Shader->GetNumInstructions());
						Descriptions.Push(ShaderTypeDescriptions(InstructionIndex));
					}
				}
			}
		}
	}

	check(Descriptions.Num() == InstructionCounts.Num());
}

/** @return the number of components in a vector type. */
UINT GetNumComponents(EMaterialValueType Type)
{
	switch(Type)
	{
		case MCT_Float:
		case MCT_Float1: return 1;
		case MCT_Float2: return 2;
		case MCT_Float3: return 3;
		case MCT_Float4: return 4;
		default: return 0;
	}
}

/** @return the vector type containing a given number of components. */
EMaterialValueType GetVectorType(UINT NumComponents)
{
	switch(NumComponents)
	{
		case 1: return MCT_Float;
		case 2: return MCT_Float2;
		case 3: return MCT_Float3;
		case 4: return MCT_Float4;
		default: return MCT_Unknown;
	};
}

/**
 */
class FMaterialUniformExpressionConstant: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionConstant);
public:
	FMaterialUniformExpressionConstant() {}
	FMaterialUniformExpressionConstant(const FLinearColor& InValue,BYTE InValueType):
		Value(InValue),
		ValueType(InValueType)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Value << ValueType;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		OutValue = Value;
	}
	virtual UBOOL IsConstant() const
	{
		return TRUE;
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionConstant* OtherConstant = (FMaterialUniformExpressionConstant*)OtherExpression;
		return OtherConstant->ValueType == ValueType && OtherConstant->Value == Value;
	}

private:
	FLinearColor Value;
	BYTE ValueType;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionConstant);

FMaterialUniformExpressionTexture::FMaterialUniformExpressionTexture() :
	TextureIndex(INDEX_NONE),
	DefaultValueDuringCompile(NULL),
	LegacyTexture(NULL),
	TransientOverrideValue(NULL)
{}

FMaterialUniformExpressionTexture::FMaterialUniformExpressionTexture(UTexture* InDefaultValue) :
	TextureIndex(INDEX_NONE),
	DefaultValueDuringCompile(InDefaultValue),
	LegacyTexture(NULL),
	TransientOverrideValue(NULL)
{
	check(InDefaultValue);
}

void FMaterialUniformExpressionTexture::Serialize(FArchive& Ar)
{
	if (Ar.Ver() >= VER_UNIFORM_EXPRESSIONS_IN_SHADER_CACHE)
	{
		Ar << TextureIndex;
	}
	else
	{
		// This is a legacy uniform expression that was serialized to the UMaterial's package.
		Ar << LegacyTexture;
		if (Ar.IsLoading())
		{
			if (!LegacyTexture)
			{
				LegacyTexture = LoadObject<UTexture2D>(NULL, TEXT("EngineResources.DefaultTexture"), NULL, LOAD_None, NULL);
			}
		}
	}
}

static UTexture* GetIndexedTexture(const FMaterial& Material, INT TextureIndex)
{
	if (TextureIndex >= 0 && TextureIndex < Material.GetTextures().Num())
	{
		UTexture* IndexedTexture = Material.GetTextures()(TextureIndex);
		return IndexedTexture;
	}
	else
	{
		// Uniform expression textures are stripped out on mobile so this warning always triggers
#if WITH_MOBILE_RHI
		if( !GUsingMobileRHI )
#endif
		{
			static UBOOL bWarnedOnce = FALSE;
			if (!bWarnedOnce)
			{
				warnf(TEXT("FMaterialUniformExpressionTexture had invalid TextureIndex! (%u/%u)"), TextureIndex, Material.GetTextures().Num());
				bWarnedOnce = TRUE;
			}
		}
	}
	return NULL;
}

void FMaterialUniformExpressionTexture::GetTextureValue(const FMaterialRenderContext& Context,const FMaterial& Material,const FTexture*& OutValue) const
{
	if( TransientOverrideValue != NULL )
	{
		OutValue = TransientOverrideValue->Resource;
	}
	else
	{
		UTexture* IndexedTexture = GetIndexedTexture(Material, TextureIndex);
		OutValue = IndexedTexture ? IndexedTexture->Resource : NULL;
	}
}

void FMaterialUniformExpressionTexture::GetGameThreadTextureValue(UMaterialInterface* MaterialInterface,const FMaterial& Material,UTexture*& OutValue,UBOOL bAllowOverride) const
{
	if (bAllowOverride && TransientOverrideValue)
	{
		OutValue = TransientOverrideValue;
	}
	else
	{
		OutValue = GetIndexedTexture(Material, TextureIndex);
	}
}

UBOOL FMaterialUniformExpressionTexture::IsIdentical(const FMaterialUniformExpression* OtherExpression) const
{
	if (GetType() != OtherExpression->GetType())
	{
		return FALSE;
	}
	FMaterialUniformExpressionTexture* OtherTextureExpression = (FMaterialUniformExpressionTexture*)OtherExpression;
	if (DefaultValueDuringCompile && OtherTextureExpression->DefaultValueDuringCompile)
	{
		// During compilation we only know the UTexture and not TextureIndex yet, so compare the UTextures
		return DefaultValueDuringCompile == OtherTextureExpression->DefaultValueDuringCompile;
	}
	// Not comparing during compilation, compare TextureIndex
	return TextureIndex == OtherTextureExpression->TextureIndex;
}

IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTexture);

/**
 */
class FMaterialUniformExpressionTime: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTime);
public:

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		OutValue.R = Context.CurrentTime;
	}
	virtual UBOOL IsConstant() const
	{
		return FALSE;
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		return GetType() == OtherExpression->GetType();
	}
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTime);

/**
 */
class FMaterialUniformExpressionRealTime: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionRealTime);
public:

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		OutValue.R = Context.CurrentRealTime;
	}
	virtual UBOOL IsConstant() const
	{
		return FALSE;
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		return GetType() == OtherExpression->GetType();
	}
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionRealTime);

/**
 */
class FMaterialUniformExpressionVectorParameter: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionVectorParameter);
public:

	FMaterialUniformExpressionVectorParameter() {}
	FMaterialUniformExpressionVectorParameter(FName InParameterName,const FLinearColor& InDefaultValue):
		ParameterName(InParameterName),
		DefaultValue(InDefaultValue)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << ParameterName << DefaultValue;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		OutValue.R = OutValue.G = OutValue.B = OutValue.A = 0;

		if(!Context.MaterialRenderProxy->GetVectorValue(ParameterName, &OutValue, Context))
		{
			OutValue = DefaultValue;
		}
	}
	virtual UBOOL IsConstant() const
	{
		return FALSE;
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionVectorParameter* OtherParameter = (FMaterialUniformExpressionVectorParameter*)OtherExpression;
		return ParameterName == OtherParameter->ParameterName && DefaultValue == OtherParameter->DefaultValue;
	}

private:
	FName ParameterName;
	FLinearColor DefaultValue;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionVectorParameter);

/**
 */
class FMaterialUniformExpressionScalarParameter: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionScalarParameter);
public:

	FMaterialUniformExpressionScalarParameter() {}
	FMaterialUniformExpressionScalarParameter(FName InParameterName,FLOAT InDefaultValue):
		ParameterName(InParameterName),
		DefaultValue(InDefaultValue)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << ParameterName << DefaultValue;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		OutValue.R = OutValue.G = OutValue.B = OutValue.A = 0;

		if(!Context.MaterialRenderProxy->GetScalarValue(ParameterName, &OutValue.R, Context))
		{
			OutValue.R = DefaultValue;
		}
	}
	virtual UBOOL IsConstant() const
	{
		return FALSE;
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionScalarParameter* OtherParameter = (FMaterialUniformExpressionScalarParameter*)OtherExpression;
		return ParameterName == OtherParameter->ParameterName && DefaultValue == OtherParameter->DefaultValue;
	}

private:
	FName ParameterName;
	FLOAT DefaultValue;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionScalarParameter);

/**
 * A texture parameter expression.
 */
class FMaterialUniformExpressionTextureParameter: public FMaterialUniformExpressionTexture
{
	typedef FMaterialUniformExpressionTexture Super;
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTextureParameter);
public:

	FMaterialUniformExpressionTextureParameter() {}

	FMaterialUniformExpressionTextureParameter(FName InParameterName, UTexture* InDefaultValue) :
		Super(InDefaultValue),
		ParameterName(InParameterName)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << ParameterName;
		Super::Serialize(Ar);
	}
	virtual void GetTextureValue(const FMaterialRenderContext& Context,const FMaterial& Material,const FTexture*& OutValue) const
	{
		if( TransientOverrideValue != NULL )
		{
			OutValue = TransientOverrideValue->Resource;
		}
		else
		{
			OutValue = NULL;
			if(!Context.MaterialRenderProxy->GetTextureValue(ParameterName,&OutValue,Context))
			{
				UTexture* IndexedTexture = GetIndexedTexture(Material, TextureIndex);
				OutValue = IndexedTexture ? IndexedTexture->Resource : NULL;
			}
		}
	}
	virtual void GetGameThreadTextureValue(UMaterialInterface* MaterialInterface,const FMaterial& Material,UTexture*& OutValue,UBOOL bAllowOverride=TRUE) const
	{
		if( bAllowOverride && TransientOverrideValue != NULL )
		{
			OutValue = TransientOverrideValue;
		}
		else
		{
			OutValue = NULL;
			if(!MaterialInterface->GetTextureParameterValue(ParameterName,OutValue))
			{
				OutValue = GetIndexedTexture(Material, TextureIndex);
			}
		}
	}

	virtual UBOOL IsConstant() const
	{
		return FALSE;
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionTextureParameter* OtherParameter = (FMaterialUniformExpressionTextureParameter*)OtherExpression;
		return ParameterName == OtherParameter->ParameterName && Super::IsIdentical(OtherParameter);
	}

private:
	FName ParameterName;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTextureParameter);

/**
 * A flipbook texture parameter expression.
 */
class FMaterialUniformExpressionFlipBookTextureParameter : public FMaterialUniformExpressionTexture
{
	typedef FMaterialUniformExpressionTexture Super;
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFlipBookTextureParameter);
public:

	FMaterialUniformExpressionFlipBookTextureParameter() {}

	FMaterialUniformExpressionFlipBookTextureParameter(UTextureFlipBook* InDefaultValue) :
		Super(InDefaultValue)
	{}
	
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		OutValue.R = OutValue.G = OutValue.B = OutValue.A = 0;

		const FMaterial* Material = Context.MaterialRenderProxy->GetMaterial();
		UTexture* FlipBookTexture = GetIndexedTexture(*Material, TextureIndex);
		if (FlipBookTexture)
		{
			FlipBookTexture->GetTextureOffset_RenderThread(OutValue);
		}
	}

	virtual UBOOL IsConstant() const
	{
		return FALSE;
	}
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFlipBookTextureParameter);

/**
 */
class FMaterialUniformExpressionSine: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSine);
public:

	FMaterialUniformExpressionSine() {}
	FMaterialUniformExpressionSine(FMaterialUniformExpression* InX,UBOOL bInIsCosine):
		X(InX),
		bIsCosine(bInIsCosine)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X << bIsCosine;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueX = FLinearColor::Black;
		X->GetNumberValue(Context,ValueX);
		OutValue.R = bIsCosine ? appCos(ValueX.R) : appSin(ValueX.R);
	}
	virtual UBOOL IsConstant() const
	{
		return X->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionSine* OtherSine = (FMaterialUniformExpressionSine*)OtherExpression;
		return X->IsIdentical(OtherSine->X) && bIsCosine == OtherSine->bIsCosine;
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
	UBOOL bIsCosine;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSine);

/**
 */
class FMaterialUniformExpressionSquareRoot: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSquareRoot);
public:

	FMaterialUniformExpressionSquareRoot() {}
	FMaterialUniformExpressionSquareRoot(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueX = FLinearColor::Black;
		X->GetNumberValue(Context,ValueX);
		OutValue.R = appSqrt(ValueX.R);
	}
	virtual UBOOL IsConstant() const
	{
		return X->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionSquareRoot* OtherSqrt = (FMaterialUniformExpressionSquareRoot*)OtherExpression;
		return X->IsIdentical(OtherSqrt->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSquareRoot);

/**
 */
class FMaterialUniformExpressionLength: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionLength);
public:

	FMaterialUniformExpressionLength() {}
	FMaterialUniformExpressionLength(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueX = FLinearColor::Black;
		X->GetNumberValue(Context,ValueX);
		OutValue.R = appSqrt(ValueX.R * ValueX.R + ValueX.G * ValueX.G + ValueX.B * ValueX.B);
	}
	virtual UBOOL IsConstant() const
	{
		return X->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionLength* OtherSqrt = (FMaterialUniformExpressionLength*)OtherExpression;
		return X->IsIdentical(OtherSqrt->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionLength);

/**
 */
enum EFoldedMathOperation
{
	FMO_Add,
	FMO_Sub,
	FMO_Mul,
	FMO_Div,
	FMO_Dot
};

/** Converts an arbitrary number into a safe divisor. i.e. Abs(Number) >= DELTA */
static FLOAT GetSafeDivisor(FLOAT Number)
{
	if(Abs(Number) < DELTA)
	{
		if(Number < 0.0f)
		{
			return -DELTA;
		}
		else
		{
			return +DELTA;
		}
	}
	else
	{
		return Number;
	}
}

class FMaterialUniformExpressionFoldedMath: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFoldedMath);
public:

	FMaterialUniformExpressionFoldedMath() {}
	FMaterialUniformExpressionFoldedMath(FMaterialUniformExpression* InA,FMaterialUniformExpression* InB,BYTE InOp):
		A(InA),
		B(InB),
		Op(InOp)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << A << B << Op;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueA = FLinearColor::Black;
		FLinearColor ValueB = FLinearColor::Black;
		A->GetNumberValue(Context, ValueA);
		B->GetNumberValue(Context, ValueB);

		switch(Op)
		{
			case FMO_Add: OutValue = ValueA + ValueB; break;
			case FMO_Sub: OutValue = ValueA - ValueB; break;
			case FMO_Mul: OutValue = ValueA * ValueB; break;
			case FMO_Div: 
				OutValue.R = ValueA.R / GetSafeDivisor(ValueB.R);
				OutValue.G = ValueA.G / GetSafeDivisor(ValueB.G);
				OutValue.B = ValueA.B / GetSafeDivisor(ValueB.B);
				OutValue.A = ValueA.A / GetSafeDivisor(ValueB.A);
				break;
			case FMO_Dot: 
				{
					FLOAT DotProduct = ValueA.R * ValueB.R + ValueA.G * ValueB.G + ValueA.B * ValueB.B + ValueA.A * ValueB.A;
					OutValue.R = OutValue.G = OutValue.B = OutValue.A = DotProduct;
				}
				break;
			default: appErrorf(TEXT("Unknown folded math operation: %08x"),(INT)Op);
		};
	}
	virtual UBOOL IsConstant() const
	{
		return A->IsConstant() && B->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionFoldedMath* OtherMath = (FMaterialUniformExpressionFoldedMath*)OtherExpression;
		return A->IsIdentical(OtherMath->A) && B->IsIdentical(OtherMath->B) && Op == OtherMath->Op;
	}

private:
	TRefCountPtr<FMaterialUniformExpression> A;
	TRefCountPtr<FMaterialUniformExpression> B;
	BYTE Op;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFoldedMath);

/**
 * A hint that only the fractional part of this expession's value matters.
 */
class FMaterialUniformExpressionPeriodic: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionPeriodic);
public:

	FMaterialUniformExpressionPeriodic() {}
	FMaterialUniformExpressionPeriodic(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor TempValue = FLinearColor::Black;
		X->GetNumberValue(Context,TempValue);

		OutValue.R = TempValue.R - appFloor(TempValue.R);
		OutValue.G = TempValue.G - appFloor(TempValue.G);
		OutValue.B = TempValue.B - appFloor(TempValue.B);
		OutValue.A = TempValue.A - appFloor(TempValue.A);
	}
	virtual UBOOL IsConstant() const
	{
		return X->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionPeriodic* OtherPeriodic = (FMaterialUniformExpressionPeriodic*)OtherExpression;
		return X->IsIdentical(OtherPeriodic->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionPeriodic);

/**
 */
class FMaterialUniformExpressionAppendVector: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionAppendVector);
public:

	FMaterialUniformExpressionAppendVector() {}
	FMaterialUniformExpressionAppendVector(FMaterialUniformExpression* InA,FMaterialUniformExpression* InB,UINT InNumComponentsA):
		A(InA),
		B(InB),
		NumComponentsA(InNumComponentsA)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << A << B << NumComponentsA;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueA = FLinearColor::Black;
		FLinearColor ValueB = FLinearColor::Black;
		A->GetNumberValue(Context, ValueA);
		B->GetNumberValue(Context, ValueB);

		OutValue.R = NumComponentsA >= 1 ? ValueA.R : (&ValueB.R)[0 - NumComponentsA];
		OutValue.G = NumComponentsA >= 2 ? ValueA.G : (&ValueB.R)[1 - NumComponentsA];
		OutValue.B = NumComponentsA >= 3 ? ValueA.B : (&ValueB.R)[2 - NumComponentsA];
		OutValue.A = NumComponentsA >= 4 ? ValueA.A : (&ValueB.R)[3 - NumComponentsA];
	}
	virtual UBOOL IsConstant() const
	{
		return A->IsConstant() && B->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionAppendVector* OtherAppend = (FMaterialUniformExpressionAppendVector*)OtherExpression;
		return A->IsIdentical(OtherAppend->A) && B->IsIdentical(OtherAppend->B) && NumComponentsA == OtherAppend->NumComponentsA;
	}

private:
	TRefCountPtr<FMaterialUniformExpression> A;
	TRefCountPtr<FMaterialUniformExpression> B;
	UINT NumComponentsA;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionAppendVector);

/**
 */
class FMaterialUniformExpressionMin: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionMin);
public:

	FMaterialUniformExpressionMin() {}
	FMaterialUniformExpressionMin(FMaterialUniformExpression* InA,FMaterialUniformExpression* InB):
		A(InA),
		B(InB)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << A << B;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueA = FLinearColor::Black;
		FLinearColor ValueB = FLinearColor::Black;
		A->GetNumberValue(Context, ValueA);
		B->GetNumberValue(Context, ValueB);

		OutValue.R = Min(ValueA.R, ValueB.R);
		OutValue.G = Min(ValueA.G, ValueB.G);
		OutValue.B = Min(ValueA.B, ValueB.B);
		OutValue.A = Min(ValueA.A, ValueB.A);
	}
	virtual UBOOL IsConstant() const
	{
		return A->IsConstant() && B->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionMin* OtherMin = (FMaterialUniformExpressionMin*)OtherExpression;
		return A->IsIdentical(OtherMin->A) && B->IsIdentical(OtherMin->B);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> A;
	TRefCountPtr<FMaterialUniformExpression> B;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionMin);

/**
 */
class FMaterialUniformExpressionMax: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionMax);
public:

	FMaterialUniformExpressionMax() {}
	FMaterialUniformExpressionMax(FMaterialUniformExpression* InA,FMaterialUniformExpression* InB):
		A(InA),
		B(InB)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << A << B;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueA = FLinearColor::Black;
		FLinearColor ValueB = FLinearColor::Black;
		A->GetNumberValue(Context, ValueA);
		B->GetNumberValue(Context, ValueB);

		OutValue.R = Max(ValueA.R, ValueB.R);
		OutValue.G = Max(ValueA.G, ValueB.G);
		OutValue.B = Max(ValueA.B, ValueB.B);
		OutValue.A = Max(ValueA.A, ValueB.A);
	}
	virtual UBOOL IsConstant() const
	{
		return A->IsConstant() && B->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionMax* OtherMax = (FMaterialUniformExpressionMax*)OtherExpression;
		return A->IsIdentical(OtherMax->A) && B->IsIdentical(OtherMax->B);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> A;
	TRefCountPtr<FMaterialUniformExpression> B;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionMax);

/**
 */
class FMaterialUniformExpressionClamp: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionClamp);
public:

	FMaterialUniformExpressionClamp() {}
	FMaterialUniformExpressionClamp(FMaterialUniformExpression* InInput,FMaterialUniformExpression* InMin,FMaterialUniformExpression* InMax):
		Input(InInput),
		Min(InMin),
		Max(InMax)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Input << Min << Max;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueMin = FLinearColor::Black;
		FLinearColor ValueMax = FLinearColor::Black;
		FLinearColor ValueInput = FLinearColor::Black;
		Min->GetNumberValue(Context, ValueMin);
		Max->GetNumberValue(Context, ValueMax);
		Input->GetNumberValue(Context, ValueInput);

		OutValue.R = Clamp(ValueInput.R, ValueMin.R, ValueMax.R);
		OutValue.G = Clamp(ValueInput.G, ValueMin.G, ValueMax.G);
		OutValue.B = Clamp(ValueInput.B, ValueMin.B, ValueMax.B);
		OutValue.A = Clamp(ValueInput.A, ValueMin.A, ValueMax.A);
	}
	virtual UBOOL IsConstant() const
	{
		return Input->IsConstant() && Min->IsConstant() && Max->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionClamp* OtherClamp = (FMaterialUniformExpressionClamp*)OtherExpression;
		return Input->IsIdentical(OtherClamp->Input) && Min->IsIdentical(OtherClamp->Min) && Max->IsIdentical(OtherClamp->Max);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> Input;
	TRefCountPtr<FMaterialUniformExpression> Min;
	TRefCountPtr<FMaterialUniformExpression> Max;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionClamp);

/**
 */
class FMaterialUniformExpressionFloor: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFloor);
public:

	FMaterialUniformExpressionFloor() {}
	FMaterialUniformExpressionFloor(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		X->GetNumberValue(Context, OutValue);

		OutValue.R = appFloor(OutValue.R);
		OutValue.G = appFloor(OutValue.G);
		OutValue.B = appFloor(OutValue.B);
		OutValue.A = appFloor(OutValue.A);
	}
	virtual UBOOL IsConstant() const
	{
		return X->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionFloor* OtherFloor = (FMaterialUniformExpressionFloor*)OtherExpression;
		return X->IsIdentical(OtherFloor->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFloor);

/**
 */
class FMaterialUniformExpressionCeil: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionCeil);
public:

	FMaterialUniformExpressionCeil() {}
	FMaterialUniformExpressionCeil(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		X->GetNumberValue(Context, OutValue);

		OutValue.R = appCeil(OutValue.R);
		OutValue.G = appCeil(OutValue.G);
		OutValue.B = appCeil(OutValue.B);
		OutValue.A = appCeil(OutValue.A);
	}
	virtual UBOOL IsConstant() const
	{
		return X->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionCeil* OtherCeil = (FMaterialUniformExpressionCeil*)OtherExpression;
		return X->IsIdentical(OtherCeil->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionCeil);

/**
 */
class FMaterialUniformExpressionFrac: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFrac);
public:

	FMaterialUniformExpressionFrac() {}
	FMaterialUniformExpressionFrac(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		X->GetNumberValue(Context, OutValue);

		OutValue.R = OutValue.R - appFloor(OutValue.R);
		OutValue.G = OutValue.G - appFloor(OutValue.G);
		OutValue.B = OutValue.B - appFloor(OutValue.B);
		OutValue.A = OutValue.A - appFloor(OutValue.A);
	}
	virtual UBOOL IsConstant() const
	{
		return X->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionFrac* OtherFrac = (FMaterialUniformExpressionFrac*)OtherExpression;
		return X->IsIdentical(OtherFrac->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFrac);

/**
 */
class FMaterialUniformExpressionFmod : public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFmod);
public:

	FMaterialUniformExpressionFmod() {}
	FMaterialUniformExpressionFmod(FMaterialUniformExpression* InA,FMaterialUniformExpression* InB):
		A(InA),
		B(InB)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << A << B;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueA = FLinearColor::Black;
		FLinearColor ValueB = FLinearColor::Black;
		A->GetNumberValue(Context, ValueA);
		B->GetNumberValue(Context, ValueB);

		OutValue.R = fmod(ValueA.R, ValueB.R);
		OutValue.G = fmod(ValueA.G, ValueB.G);
		OutValue.B = fmod(ValueA.B, ValueB.B);
		OutValue.A = fmod(ValueA.A, ValueB.A);
	}
	virtual UBOOL IsConstant() const
	{
		return A->IsConstant() && B->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionFmod* OtherMax = (FMaterialUniformExpressionFmod*)OtherExpression;
		return A->IsIdentical(OtherMax->A) && B->IsIdentical(OtherMax->B);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> A;
	TRefCountPtr<FMaterialUniformExpression> B;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFmod);

/**
 * Absolute value evaluator for a given input expression
 */
class FMaterialUniformExpressionAbs: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionAbs);
public:

	FMaterialUniformExpressionAbs() {}
	FMaterialUniformExpressionAbs( FMaterialUniformExpression* InX ):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		X->GetNumberValue(Context, OutValue);
		OutValue.R = Abs<FLOAT>(OutValue.R);
		OutValue.G = Abs<FLOAT>(OutValue.G);
		OutValue.B = Abs<FLOAT>(OutValue.B);
		OutValue.A = Abs<FLOAT>(OutValue.A);
	}
	virtual UBOOL IsConstant() const
	{
		return X->IsConstant();
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return FALSE;
		}
		FMaterialUniformExpressionAbs* OtherAbs = (FMaterialUniformExpressionAbs*)OtherExpression;
		return X->IsIdentical(OtherAbs->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};
IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionAbs);

struct FShaderCodeChunk
{
	/** 
	 * Definition string of the code chunk. 
	 * If !bInline && !UniformExpression || UniformExpression->IsConstant(), this is the definition of a local variable named by SymbolName.
	 * Otherwise if bInline || (UniformExpression && UniformExpression->IsConstant()), this is a code expression that needs to be inlined.
	 */
	FString Definition;
	/** 
	 * Name of the local variable used to reference this code chunk. 
	 * If bInline || UniformExpression, there will be no symbol name and Definition should be used directly instead.
	 */
	FString SymbolName;
	/** Reference to a uniform expression, if this code chunk has one. */
	TRefCountPtr<FMaterialUniformExpression> UniformExpression;
	EMaterialValueType Type;
	DWORD Flags;
	INT TextureDependencyLength;
	/** Whether the code chunk should be inlined or not.  If true, SymbolName is empty and Definition contains the code to inline. */
	UBOOL bInline;

	/** Ctor for creating a new code chunk with no associated uniform expression. */
	FShaderCodeChunk(const TCHAR* InDefinition,const FString& InSymbolName,EMaterialValueType InType,DWORD InFlags,INT InTextureDependencyLength,UBOOL bInInline):
		Definition(InDefinition),
		SymbolName(InSymbolName),
		UniformExpression(NULL),
		Type(InType),
		Flags(InFlags),
		TextureDependencyLength(InTextureDependencyLength),
		bInline(bInInline)
	{}

	/** Ctor for creating a new code chunk with a uniform expression. */
	FShaderCodeChunk(FMaterialUniformExpression* InUniformExpression,const TCHAR* InDefinition,EMaterialValueType InType,DWORD InFlags):
		Definition(InDefinition),
		UniformExpression(InUniformExpression),
		Type(InType),
		Flags(InFlags),
		TextureDependencyLength(0),
		bInline(FALSE)
	{}
};

/**
 * Destructor
 */
FMaterial::~FMaterial()
{
	if (GIsEditor)
	{
		const FSetElementId FoundId = EditorLoadedMaterialResources.FindId(this);
		if (FoundId.IsValidId())
		{
			// Remove the material from EditorLoadedMaterialResources if found
			EditorLoadedMaterialResources.Remove(FoundId);
		}
	}

	FMaterialShaderMap::RemovePendingMaterial(this);

	if (ShaderMap)
	{
		ShaderMap->BeginRelease();
	}

	if (LegacyUniformExpressions)
	{
		delete LegacyUniformExpressions;
	}
}

/** Populates OutEnvironment with defines needed to compile shaders for this material. */
void FMaterial::SetupMaterialEnvironment(
	EShaderPlatform Platform,
	FVertexFactoryType* VertexFactoryType,
	FShaderCompilerEnvironment& OutEnvironment
	) const
{
	if (VertexFactoryType)
	{
		// apply the vertex factory changes to the compile environment
		VertexFactoryType->ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.VFFileName = VertexFactoryType->GetShaderFilename();
	}

#if WITH_D3D11_TESSELLATION
	// It is here that the various D3D11 tessellation policies can set their custom defines to enable/disable shader code
	FTessellationMaterialPolicy::ModifyCompilationEnvironment(Platform,GetD3D11TessellationMode(),OutEnvironment);
#endif

	switch(GetBlendMode())
	{
	case BLEND_Opaque: OutEnvironment.Definitions.Set(TEXT("MATERIALBLENDING_SOLID"),TEXT("1")); break;
	case BLEND_Masked:
		{
			// Only set MATERIALBLENDING_MASKED if the material is truly masked
			//@todo - this may cause mismatches with what the shader compiles and what the renderer thinks the shader needs
			// For example IsTranslucentBlendMode doesn't check IsMasked
			if(IsMasked())
			{
				OutEnvironment.Definitions.Set(TEXT("MATERIALBLENDING_MASKED"),TEXT("1"));
			}
			else
			{
				OutEnvironment.Definitions.Set(TEXT("MATERIALBLENDING_SOLID"),TEXT("1"));
			}
			break;
		}
	case BLEND_SoftMasked:
		{
			// Only set MATERIALBLENDING_SOFTMASKED if the material is truly masked
			if(IsMasked())
			{
				OutEnvironment.Definitions.Set(TEXT("MATERIALBLENDING_SOFTMASKED"),TEXT("1"));
			}
			else
			{
				OutEnvironment.Definitions.Set(TEXT("MATERIALBLENDING_SOLID"),TEXT("1"));
			}
			break;
		}
    case BLEND_AlphaComposite:
	case BLEND_Translucent: OutEnvironment.Definitions.Set(TEXT("MATERIALBLENDING_TRANSLUCENT"),TEXT("1")); break;
	case BLEND_Additive: OutEnvironment.Definitions.Set(TEXT("MATERIALBLENDING_ADDITIVE"),TEXT("1")); break;
	case BLEND_Modulate: OutEnvironment.Definitions.Set(TEXT("MATERIALBLENDING_MODULATE"),TEXT("1")); break;
	case BLEND_DitheredTranslucent: OutEnvironment.Definitions.Set(TEXT("MATERIALBLENDING_DITHEREDTRANSLUCENT"),TEXT("1")); break;
	default: appErrorf(TEXT("Unknown material blend mode: %u"),(INT)GetBlendMode());
	}

	OutEnvironment.Definitions.Set(TEXT("MATERIAL_TWOSIDED"),IsTwoSided() ? TEXT("1") : TEXT("0"));
	OutEnvironment.Definitions.Set(TEXT("MATERIAL_TWOSIDED_SEPARATE_PASS"),RenderTwoSidedSeparatePass() ? TEXT("1") : TEXT("0"));
	OutEnvironment.Definitions.Set(TEXT("MATERIAL_LIT_TRANSLUCENCY_PREPASS"),RenderLitTranslucencyPrepass() ? TEXT("1") : TEXT("0"));
	OutEnvironment.Definitions.Set(TEXT("MATERIAL_LIT_TRANSLUCENCY_DEPTH_POSTPASS"),RenderLitTranslucencyDepthPostpass() ? TEXT("1") : TEXT("0"));
	OutEnvironment.Definitions.Set(TEXT("MATERIAL_CAST_LIT_TRANSLUCENCY_SHADOW_AS_MASKED"),CastLitTranslucencyShadowAsMasked() ? TEXT("1") : TEXT("0"));
	OutEnvironment.Definitions.Set(TEXT("MATERIAL_ENABLE_SUBSURFACE_SCATTERING"),HasSubsurfaceScattering() ? TEXT("1") : TEXT("0"));

	if (IsUsedWithScreenDoorFade())
	{
		OutEnvironment.Definitions.Set(TEXT("MATERIAL_USE_SCREEN_DOOR_FADE"), TEXT( "1" ));
	}

	switch(GetLightingModel())
	{
	case MLM_SHPRT: // For backward compatibility, treat the deprecated SHPRT lighting model as Phong.
	case MLM_Phong: OutEnvironment.Definitions.Set(TEXT("MATERIAL_LIGHTINGMODEL_PHONG"),TEXT("1")); break;
	case MLM_NonDirectional: OutEnvironment.Definitions.Set(TEXT("MATERIAL_LIGHTINGMODEL_NONDIRECTIONAL"),TEXT("1")); break;
	case MLM_Unlit: OutEnvironment.Definitions.Set(TEXT("MATERIAL_LIGHTINGMODEL_UNLIT"),TEXT("1")); break;
	case MLM_Custom: OutEnvironment.Definitions.Set(TEXT("MATERIAL_LIGHTINGMODEL_CUSTOM"),TEXT("1")); break;
	case MLM_Anisotropic: OutEnvironment.Definitions.Set(TEXT("MATERIAL_LIGHTINGMODEL_ANISOTROPIC"),TEXT("1")); break;
	default: appErrorf(TEXT("Unknown material lighting model: %u"),(INT)GetLightingModel());
	};

	// Disable specular if the material is flagged as such...
	if (!IsSpecularAllowed())
	{
		OutEnvironment.Definitions.Set(TEXT("DISABLE_LIGHTMAP_SPECULAR"), TEXT("1"));
		OutEnvironment.Definitions.Set(TEXT("DISABLE_DYNAMIC_SPECULAR"), TEXT("1"));
	}

	if (!IsLightmapSpecularAllowed())
	{
		OutEnvironment.Definitions.Set(TEXT("DISABLE_LIGHTMAP_SPECULAR"), TEXT("1"));
	}

	if (UsesImageBasedReflections())
	{
		OutEnvironment.Definitions.Set(TEXT("IMAGE_BASED_REFLECTIONS"),TEXT("1"));
	}

	if (UsesMaskedAntialiasing())
	{
		OutEnvironment.Definitions.Set(TEXT("MASKED_ANTIALIASING"),TEXT("1"));
	}

	if (GetTransformsUsed() & UsedCoord_World 
		|| GetTransformsUsed() & UsedCoord_View
		|| GetTransformsUsed() & UsedCoord_Local)
	{
		// only use WORLD_COORDS code if a Transform expression was used by the material
		OutEnvironment.Definitions.Set(TEXT("WORLD_COORDS"),TEXT("1"));
	}

	if (GetTransformsUsed() & UsedCoord_WorldPos)
	{
		OutEnvironment.Definitions.Set(TEXT("WORLD_POS"),TEXT("1"));
	}

	if (UsesPerPixelCameraVector())
	{
		// Make sure we actually have all of the requirements for per-pixel camera vector.  Usually these
		// will be set by the expression when per-pixel camera vector is enabled (such as ReflectionVector.)
		const INT PerPixelTransformsRequired = UsedCoord_WorldPos | UsedCoord_World;
		if ((GetTransformsUsed() & PerPixelTransformsRequired) == PerPixelTransformsRequired)
		{
			OutEnvironment.Definitions.Set(TEXT("PER_PIXEL_CAMERA_VECTOR"),TEXT("1"));
		}
	}

	if (IsUsedWithDecals())
	{
		OutEnvironment.Definitions.Set(TEXT("MATERIAL_DECAL"),TEXT("1"));
	}

	if (UsesOneLayerDistortion())
	{
		OutEnvironment.Definitions.Set(TEXT("MATERIAL_ONELAYERDISTORTION"),TEXT("1"));
	}

	if (IsUsedWithGammaCorrection())
	{
		// only use USE_GAMMA_CORRECTION code when enabled
		OutEnvironment.Definitions.Set(TEXT("MATERIAL_USE_GAMMA_CORRECTION"),TEXT("1"));
	}

	if (HasNormalmapConnected())
	{
		// This is used by the Anisotropic shader to determine whether to perform Graham-Schmidt Orthonormalization
		OutEnvironment.Definitions.Set(TEXT("MATERIAL_DEFINED_NORMALMAP"),TEXT("1"));
	}

	if (bUsesLightmapUVs)
	{
		OutEnvironment.Definitions.Set(TEXT("LIGHTMAP_UV_ACCESS"),TEXT("1"));
	}

	if (AllowTranslucencyDoF() && Platform == SP_XBOXD3D)
	{
		OutEnvironment.Definitions.Set(TEXT("ENABLE_TRANSLUCENCY_DOF"),TEXT("1"));	
	}

	if (TranslucencyReceiveDominantShadowsFromStatic())
	{
		OutEnvironment.Definitions.Set(TEXT("TRANSLUCENCY_RECEIVE_DYNAMIC_SHADOWS_FROM_STATIC"),TEXT("1"));	
	}

	if (TranslucencyInheritDominantShadowsFromOpaque())
	{
		OutEnvironment.Definitions.Set(TEXT("TRANSLUCENCY_INHERIT_DOMINANT_SHADOWS_FROM_OPAQUE"),TEXT("1"));		
	}
}

#if !CONSOLE
class FHLSLMaterialTranslator : public FMaterialCompiler
{
protected:
	UBOOL bSuccess;
	/** The shader frequency of the current material property being compiled. */
	EShaderFrequency ShaderFrequency;
	/** The current material property being compiled.  This affects the behavior of all compiler functions except GetFixedParameterCode. */
	EMaterialProperty MaterialProperty;
	FMaterial* Material;
	FUniformExpressionSet& UniformExpressionSet;
	EShaderPlatform Platform;

	/** Code chunks corresponding to each of the material inputs, only initialized after Translate has been called. */
	FString NormalCodeChunk;
	FString EmissiveColorCodeChunk;
	FString DiffuseColorCodeChunk;
	FString DiffusePowerCodeChunk;
	FString SpecularColorCodeChunk;
	FString SpecularPowerCodeChunk;
	FString OpacityCodeChunk;
	FString MaskCodeChunk;
	FString DistortionCodeChunk;
	FString TwoSidedLightingMaskCodeChunk;
	FString CustomLightingCodeChunk;
	FString CustomLightingDiffuseCodeChunk;
	FString AnisotropicDirectionCodeChunk;
	FString WorldPositionOffsetCodeChunk;
	FString WorldDisplacementCodeChunk;
	FString TessellationFactorsCodeChunk;
	FString SubsurfaceInscatteringColorCodeChunk;
	FString SubsurfaceAbsorptionColorCodeChunk;
	FString SubsurfaceScatteringRadiusCodeChunk;

	/** Line number of the #line in MaterialTemplate.usf */
	INT MaterialTemplateLineNumber;

	/** Stores the input declarations */
	FString InputsString;

	/** Contents of the MaterialTemplate.usf file */
	FString MaterialTemplate;

public: 

	FHLSLMaterialTranslator(FMaterial* InMaterial,FUniformExpressionSet& InUniformExpressionSet,EShaderPlatform InPlatform)
	:	bSuccess(FALSE)
	,	ShaderFrequency(SF_Pixel)
	,	MaterialProperty(MP_EmissiveColor)
	,	Material(InMaterial)
	,	UniformExpressionSet(InUniformExpressionSet)
	,	Platform(InPlatform)
	,	MaterialTemplateLineNumber(INDEX_NONE)
	,	NextSymbolIndex(INDEX_NONE)
	{}
 
	UBOOL Translate()
	{
		bSuccess = TRUE;

		Material->CompileErrors.Empty();
		Material->TextureDependencyLengthMap.Empty();
		Material->MaxTextureDependencyLength = 0;

		Material->NumUserTexCoords = 0;
		Material->bUsesDynamicParameter = FALSE;
		Material->bUsesLightmapUVs = FALSE;

		// decals need at least texcoord[0].xy for clipping
		// and texcoord[1].zw for attenuation and distance clipping
		if( Material->IsUsedWithDecals() )
		{
			Material->NumUserTexCoords = 2;				
		}

		Material->UniformExpressionTextures.Empty();
		
		Material->UsingTransforms = UsedCoord_None;
		Material->bUsesSceneDepth = FALSE;
		Material->bUsesSceneColor = FALSE;
		Material->bUsesMaterialVertexPositionOffset = FALSE;

		// Generate code
		INT NormalChunk, EmissiveColorChunk, DiffuseColorChunk, DiffusePowerChunk, SpecularColorChunk, SpecularPowerChunk, OpacityChunk, MaskChunk, DistortionChunk, TwoSidedLightingMaskChunk, CustomLightingChunk, CustomLightingDiffuseChunk, AnisotropicDirectionChunk, WorldPositionOffsetChunk, WorldDisplacementChunk, TessellationFactorsChunk, SubsurfaceInscatteringColorChunk, SubsurfaceAbsorptionColorChunk, SubsurfaceScatteringRadiusChunk;

		STAT(DOUBLE HLSLTranslateTime = 0);
		{
			SCOPE_SECONDS_COUNTER(HLSLTranslateTime);
			const EMaterialShaderPlatform MatPlatform = GetMaterialPlatform(Platform);
			NormalChunk							= ForceCast(Material->CompileProperty(MatPlatform, MP_Normal							,this),MCT_Float3);
			EmissiveColorChunk					= ForceCast(Material->CompileProperty(MatPlatform, MP_EmissiveColor						,this),MCT_Float3);
			DiffuseColorChunk					= ForceCast(Material->CompileProperty(MatPlatform, MP_DiffuseColor						,this),MCT_Float3);
			DiffusePowerChunk					= ForceCast(Material->CompileProperty(MatPlatform, MP_DiffusePower						,this),MCT_Float1);
			SpecularColorChunk					= ForceCast(Material->CompileProperty(MatPlatform, MP_SpecularColor						,this),MCT_Float3);
			SpecularPowerChunk					= ForceCast(Material->CompileProperty(MatPlatform, MP_SpecularPower						,this),MCT_Float1);
			OpacityChunk						= ForceCast(Material->CompileProperty(MatPlatform, MP_Opacity							,this),MCT_Float1);
			MaskChunk							= ForceCast(Material->CompileProperty(MatPlatform, MP_OpacityMask						,this),MCT_Float1);
			DistortionChunk						= ForceCast(Material->CompileProperty(MatPlatform, MP_Distortion						,this),MCT_Float2);
			TwoSidedLightingMaskChunk			= ForceCast(Material->CompileProperty(MatPlatform, MP_TwoSidedLightingMask				,this),MCT_Float3);
			CustomLightingChunk					= ForceCast(Material->CompileProperty(MatPlatform, MP_CustomLighting					,this),MCT_Float3);
			CustomLightingDiffuseChunk			= ForceCast(Material->CompileProperty(MatPlatform, MP_CustomLightingDiffuse				,this),MCT_Float3);
			AnisotropicDirectionChunk			= ForceCast(Material->CompileProperty(MatPlatform, MP_AnisotropicDirection				,this),MCT_Float3);
			WorldPositionOffsetChunk			= ForceCast(Material->CompileProperty(MatPlatform, MP_WorldPositionOffset				,this),MCT_Float3);
			WorldDisplacementChunk				= ForceCast(Material->CompileProperty(MatPlatform, MP_WorldDisplacement					,this),MCT_Float3);
			TessellationFactorsChunk			= ForceCast(Material->CompileProperty(MatPlatform, MP_TessellationFactors				,this),MCT_Float2);

			SubsurfaceInscatteringColorChunk = ForceCast(Material->CompileProperty(MatPlatform,MP_SubsurfaceInscatteringColor,this),MCT_Float3);
			SubsurfaceAbsorptionColorChunk = ForceCast(Material->CompileProperty(MatPlatform,MP_SubsurfaceAbsorptionColor,this),MCT_Float3);
			SubsurfaceScatteringRadiusChunk = ForceCast(Material->CompileProperty(MatPlatform,MP_SubsurfaceScatteringRadius,this),MCT_Float1);
		}
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_HLSLTranslation,(FLOAT)HLSLTranslateTime);

		Material->bUsesMaterialVertexPositionOffset = Material->HasVertexPositionOffsetConnected();

		// Don't allow opaque and masked materials to sample scene color or scene depth as the results are undefined,
		// unless it's for a PP material effect.
		if (!IsTranslucentBlendMode(Material->GetBlendMode()) && !Material->IsUsedWithMaterialEffect())
		{
			if (Material->bUsesSceneDepth)
			{
				Errorf(TEXT("Only transparent materials can read from scene depth."));
			}
			if (Material->bUsesSceneColor)
			{
				Errorf(TEXT("Only transparent materials can read from scene color."));
			}
		}

		if (Material->IsUsedWithDecals())
		{
			// Don't allow decals to sample scene color as we don't currently resolve scene color before decals
			if (Material->bUsesSceneColor)
			{
				Errorf(TEXT("Materials used with decals must not read from scene color."));
			}

			if (Material->NumUserTexCoords > 2)
			{
				Errorf(TEXT("Decal materials can only use UV0 and UV1, currently setup to use %u UV sets."), Material->NumUserTexCoords);
			}
		}

		if (Material->GetBlendMode() == BLEND_Modulate && Material->GetLightingModel() != MLM_Unlit)
		{
			Errorf(TEXT("Dynamically lit translucency is not supported for BLEND_Modulate materials."));
		}

		InputsString = TEXT("");
		UniformExpressionSet.GetInputsString(InputsString);

		// Output the implementation for any custom expressions we will call below.
		for(INT ExpressionIndex = 0;ExpressionIndex < CustomExpressionImplementations.Num();ExpressionIndex++)
		{
			InputsString += CustomExpressionImplementations(ExpressionIndex) + "\r\n\r\n";
		}

		NormalCodeChunk = GetFixedParameterCode(NormalChunk,MP_Normal);
		EmissiveColorCodeChunk = GetFixedParameterCode(EmissiveColorChunk,MP_EmissiveColor);
		DiffuseColorCodeChunk = GetFixedParameterCode(DiffuseColorChunk,MP_DiffuseColor);
		DiffusePowerCodeChunk = GetFixedParameterCode(DiffusePowerChunk,MP_DiffusePower);
		SpecularColorCodeChunk = GetFixedParameterCode(SpecularColorChunk,MP_SpecularColor);
		SpecularPowerCodeChunk = GetFixedParameterCode(SpecularPowerChunk,MP_SpecularPower);
		OpacityCodeChunk = GetFixedParameterCode(OpacityChunk,MP_Opacity);
		MaskCodeChunk = GetFixedParameterCode(MaskChunk,MP_OpacityMask);
		DistortionCodeChunk = GetFixedParameterCode(DistortionChunk,MP_Distortion);
		TwoSidedLightingMaskCodeChunk = GetFixedParameterCode(TwoSidedLightingMaskChunk,MP_TwoSidedLightingMask);
		CustomLightingCodeChunk = GetFixedParameterCode(CustomLightingChunk,MP_CustomLighting);
		CustomLightingDiffuseCodeChunk = GetFixedParameterCode(CustomLightingDiffuseChunk,MP_CustomLightingDiffuse);
		AnisotropicDirectionCodeChunk = GetFixedParameterCode(AnisotropicDirectionChunk,MP_AnisotropicDirection);
		WorldPositionOffsetCodeChunk = GetFixedParameterCode(WorldPositionOffsetChunk,MP_WorldPositionOffset);
		WorldDisplacementCodeChunk = GetFixedParameterCode(WorldDisplacementChunk,MP_WorldDisplacement);
		TessellationFactorsCodeChunk = GetFixedParameterCode(TessellationFactorsChunk,MP_TessellationFactors);
		SubsurfaceInscatteringColorCodeChunk = GetFixedParameterCode(SubsurfaceInscatteringColorChunk,MP_SubsurfaceInscatteringColor);
		SubsurfaceAbsorptionColorCodeChunk = GetFixedParameterCode(SubsurfaceAbsorptionColorChunk,MP_SubsurfaceAbsorptionColor);
		SubsurfaceScatteringRadiusCodeChunk = GetFixedParameterCode(SubsurfaceScatteringRadiusChunk,MP_SubsurfaceScatteringRadius);

		MaterialTemplate = LoadShaderSourceFile(TEXT("MaterialTemplate"));

		// Find the string index of the '#line' statement in MaterialTemplate.usf
		const INT LineIndex = MaterialTemplate.InStr(TEXT("#line"));
		check(LineIndex != INDEX_NONE);

		// Count line endings before the '#line' statement
		MaterialTemplateLineNumber = INDEX_NONE;
		INT StartPosition = LineIndex + 1;
		do 
		{
			MaterialTemplateLineNumber++;
			// Using \n instead of LINE_TERMINATOR as not all of the lines are terminated consistently
			// Subtract one from the last found line ending index to make sure we skip over it
			StartPosition = MaterialTemplate.InStr(TEXT("\n"), TRUE, FALSE, StartPosition - 1);
		} 
		while (StartPosition != INDEX_NONE);
		check(MaterialTemplateLineNumber != INDEX_NONE);
		// At this point MaterialTemplateLineNumber is one less than the line number of the '#line' statement
		// For some reason we have to add 2 more to the #line value to get correct error line numbers from D3DXCompileShader
		MaterialTemplateLineNumber += 3;

		UniformExpressionSet.ClearDefaultTextureValueReferences();

		return bSuccess;
	}

	FString GetMaterialShaderCode()
	{	
		// Note: This Printf maxes out the number of supported variable arguments to a function using VARARG_DECL
		const FString MaterialShaderCode = FString::Printf(
			*MaterialTemplate,
			Material->NumUserTexCoords,
			*InputsString,
			*NormalCodeChunk,
			*EmissiveColorCodeChunk,
			*DiffuseColorCodeChunk,
			*DiffusePowerCodeChunk,	
			*SpecularColorCodeChunk,
			*SpecularPowerCodeChunk,
			*FString::Printf(TEXT("return %.5f"),Material->GetOpacityMaskClipValue()),
			*OpacityCodeChunk,
			*GetDefinitions(MP_OpacityMask),
			*MaskCodeChunk,
			*DistortionCodeChunk,
			*TwoSidedLightingMaskCodeChunk,
			*CustomLightingCodeChunk,
			*CustomLightingDiffuseCodeChunk,
			*AnisotropicDirectionCodeChunk,
			*WorldPositionOffsetCodeChunk,
			*WorldDisplacementCodeChunk,
			*TessellationFactorsCodeChunk,
			*SubsurfaceInscatteringColorCodeChunk,
			*SubsurfaceAbsorptionColorCodeChunk,
			*SubsurfaceScatteringRadiusCodeChunk,
			MaterialTemplateLineNumber
			);

		return MaterialShaderCode;
	}

protected:
	/** A map from material expression to the index into CodeChunks of the code for the material expression. */
	TMap<UMaterialExpression*,INT> ExpressionCodeMap[MP_MAX];

	// GetParameterCode
	virtual const TCHAR* GetParameterCode(INT Index)
	{
		checkf(Index >= 0 && Index < CodeChunks[MaterialProperty].Num(), TEXT("Index %d/%d, Platform=%d"), Index, CodeChunks[MaterialProperty].Num(), Platform);
		const FShaderCodeChunk& CodeChunk = CodeChunks[MaterialProperty](Index);
		if(CodeChunk.UniformExpression && CodeChunk.UniformExpression->IsConstant() || CodeChunk.bInline)
		{
			// Constant uniform expressions and code chunks which are marked to be inlined are accessed via Definition
			return *CodeChunk.Definition;
		}
		else
		{
			if (CodeChunk.UniformExpression)
			{
				// If the code chunk has a uniform expression, create a new code chunk to access it
				const INT AccessedIndex = AccessUniformExpression(Index);
				const FShaderCodeChunk& AccessedCodeChunk = CodeChunks[MaterialProperty](AccessedIndex);
				if(AccessedCodeChunk.bInline)
				{
					// Handle the accessed code chunk being inlined
					return *AccessedCodeChunk.Definition;
				}
				// Return the symbol used to reference this code chunk
				check(AccessedCodeChunk.SymbolName.Len() > 0);
				return *AccessedCodeChunk.SymbolName;
			}
			
			// Return the symbol used to reference this code chunk
			check(CodeChunk.SymbolName.Len() > 0);
			return *CodeChunk.SymbolName;
		}
	}

	/** Creates a string of all definitions needed for the given material input. */
	FString GetDefinitions(EMaterialProperty InProperty) const
	{
		FString Definitions;
		for (INT ChunkIndex = 0; ChunkIndex < CodeChunks[InProperty].Num(); ChunkIndex++)
		{
			const FShaderCodeChunk& CodeChunk = CodeChunks[InProperty](ChunkIndex);
			// Uniform expressions (both constant and variable) and inline expressions don't have definitions.
			if (!CodeChunk.UniformExpression && !CodeChunk.bInline)
			{
				Definitions += CodeChunk.Definition;
			}
		}
		return Definitions;
	}

	// GetFixedParameterCode
	virtual FString GetFixedParameterCode(INT Index, EMaterialProperty InProperty)
	{
		if(Index != INDEX_NONE)
		{
			checkf(Index >= 0 && Index < CodeChunks[InProperty].Num(), TEXT("Index out of range %d/%d [%s]"), Index, CodeChunks[InProperty].Num(), *this->Material->GetFriendlyName());
			check(!CodeChunks[InProperty](Index).UniformExpression || CodeChunks[InProperty](Index).UniformExpression->IsConstant());
			if (CodeChunks[InProperty](Index).UniformExpression && CodeChunks[InProperty](Index).UniformExpression->IsConstant())
			{
				if (InProperty == MP_OpacityMask)
				{
					// Handle OpacityMask differently since it doesn't use the same formatting as the other material properties in MaterialTemplate.usf
					const FShaderCodeChunk& CodeChunk = CodeChunks[InProperty](Index);
					return CodeChunk.Definition;
				}
				else
				{
					// Handle a constant uniform expression being the only code chunk hooked up to a material input
					const FShaderCodeChunk& CodeChunk = CodeChunks[InProperty](Index);
					return FString(TEXT("	return ")) + CodeChunk.Definition;
				}
			}
			else
			{
				if (InProperty == MP_OpacityMask)
				{
					// Handle OpacityMask differently since it doesn't use the same formatting as the other material properties in MaterialTemplate.usf
					const FShaderCodeChunk& CodeChunk = CodeChunks[InProperty](Index);
					if (CodeChunk.bInline)
					{
						return CodeChunk.Definition;
					}
					check(CodeChunk.SymbolName.Len() > 0);
					return CodeChunk.SymbolName;
				}
				else
				{
					const FShaderCodeChunk& CodeChunk = CodeChunks[InProperty](Index);
					// Combine the definition lines and the return statement
					check(CodeChunk.bInline || CodeChunk.SymbolName.Len() > 0);
					return GetDefinitions(InProperty) + TEXT("	return ") + (CodeChunk.bInline ? CodeChunk.Definition : CodeChunk.SymbolName);
				}
			}
		}
		else
		{
			return TEXT("	return 0");
		}
	}

private:
	enum EShaderCodeChunkFlags
	{
		SCF_RGBE_4BIT_EXPONENT			= 1,
		SCF_RGBE_8BIT_EXPONENT			= 2,
		SCF_RGBE						= SCF_RGBE_4BIT_EXPONENT | SCF_RGBE_8BIT_EXPONENT,
		SCF_GREYSCALE_TEXTURE			= 4,
		SCF_A8R8G8B8_TEXTURE			= 8,
	};

	// Array of code chunks per material property
	TArray<FShaderCodeChunk> CodeChunks[MP_MAX];

	// Uniform expressions used across all material properties
	TArray<FShaderCodeChunk> UniformExpressions;

	// Stack used to avoid re-entry
	TArray<UMaterialExpression*> ExpressionStack;

	// Index of the next symbol to create
	INT NextSymbolIndex;

	/** Any custom expression function implementations */
	TArray<FString> CustomExpressionImplementations;

	const TCHAR* DescribeType(EMaterialValueType Type) const
	{
		switch(Type)
		{
		case MCT_Float1:		return TEXT("float");
		case MCT_Float2:		return TEXT("float2");
		case MCT_Float3:		return TEXT("float3");
		case MCT_Float4:		return TEXT("float4");
		case MCT_Float:			return TEXT("float");
		case MCT_Texture2D:		return TEXT("texture2D");
		case MCT_TextureCube:	return TEXT("textureCube");
		default:				return TEXT("unknown");
		};
	}

	INT NonPixelShaderExpressionError()
	{
		return Errorf(TEXT("Invalid node used in vertex/hull/domain shader input!"));
	}

	/** Creates a unique symbol name and adds it to the symbol list. */
	FString CreateSymbolName(const TCHAR* SymbolNameHint)
	{
		NextSymbolIndex++;
		return FString(SymbolNameHint) + appItoa(NextSymbolIndex);
	}

	/** Adds an already formatted inline or referenced code chunk */
	INT AddCodeChunkInner(const TCHAR* FormattedCode,EMaterialValueType Type,DWORD Flags,INT TextureDependencyDepth,UBOOL bInlined)
	{
		if (Type == MCT_Unknown)
		{
			return INDEX_NONE;
		}

		if (bInlined)
		{
			const INT CodeIndex = CodeChunks[MaterialProperty].Num();
			// Adding an inline code chunk, the definition will be the code to inline
			new(CodeChunks[MaterialProperty]) FShaderCodeChunk(FormattedCode,TEXT(""),Type,Flags,TextureDependencyDepth,TRUE);
			return CodeIndex;
		}
		else
		{
			const INT CodeIndex = CodeChunks[MaterialProperty].Num();
			// Allocate a local variable name
			const FString SymbolName = CreateSymbolName(TEXT("Local"));
			// Can only create temporaries for float types
			check(Type & MCT_Float);
			// Construct the definition string which stores the result in a temporary and adds a newline for readability
			const FString LocalVariableDefinition = FString("	") + DescribeType(Type) + TEXT(" ") + SymbolName + TEXT(" = ") + FormattedCode + TEXT(";") + LINE_TERMINATOR;
			// Adding a code chunk that creates a local variable
			new(CodeChunks[MaterialProperty]) FShaderCodeChunk(*LocalVariableDefinition,SymbolName,Type,Flags,TextureDependencyDepth,FALSE);
			return CodeIndex;
		}
	}

	/** 
	 * Constructs the formatted code chunk and creates a new local variable definition from it. 
	 * This should be used over AddInlinedCodeChunk when the code chunk adds actual instructions, and especially when calling a function.
	 * Creating local variables instead of inlining simplifies the generated code and reduces redundant expression chains,
	 * Making compiles faster and enabling the shader optimizer to do a better job.
	 */
	INT AddCodeChunk(EMaterialValueType Type,DWORD Flags,INT TextureDependencyDepth,const TCHAR* Format,...)
	{
		INT		BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		INT		Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) appRealloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( FormattedCode, BufferSize, BufferSize-1, Format, Format, Result );
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;

		const INT CodeIndex = AddCodeChunkInner(FormattedCode,Type,Flags,TextureDependencyDepth,FALSE);
		appFree(FormattedCode);

		return CodeIndex;
	}

	/** 
	 * Constructs the formatted code chunk and creates an inlined code chunk from it. 
	 * This should be used instead of AddCodeChunk when the code chunk does not add any actual shader instructions, for example a component mask.
	 */
	INT AddInlinedCodeChunk(EMaterialValueType Type,DWORD Flags,INT TextureDependencyDepth,const TCHAR* Format,...)
	{
		INT		BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		INT		Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) appRealloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( FormattedCode, BufferSize, BufferSize-1, Format, Format, Result );
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;
		const INT CodeIndex = AddCodeChunkInner(FormattedCode,Type,Flags,TextureDependencyDepth,TRUE);
		appFree(FormattedCode);

		return CodeIndex;
	}

	// AddUniformExpression - Adds an input to the Code array and returns its index.
	INT AddUniformExpression(FMaterialUniformExpression* UniformExpression,EMaterialValueType Type,DWORD Flags,const TCHAR* Format,...)
	{
		if (Type == MCT_Unknown)
		{
			return INDEX_NONE;
		}
		check(UniformExpression);

		UBOOL bFoundExistingExpression = FALSE;
		// Search for an existing code chunk with the same uniform expression in the array of all uniform expressions used by this material.
		for (INT ExpressionIndex = 0; ExpressionIndex < UniformExpressions.Num() && !bFoundExistingExpression; ExpressionIndex++)
		{
			FMaterialUniformExpression* TestExpression = UniformExpressions(ExpressionIndex).UniformExpression;
			check(TestExpression);
			if(TestExpression->IsIdentical(UniformExpression))
			{
				bFoundExistingExpression = TRUE;
				// This code chunk has an identical uniform expression to the new expression, reuse it.
				// This allows multiple material properties to share uniform expressions because AccessUniformExpression uses AddUniqueItem when adding uniform expressions.
				check(Type == UniformExpressions(ExpressionIndex).Type);
				// Search for an existing code chunk with the same uniform expression in the array of code chunks for this material property.
				for(INT ChunkIndex = 0;ChunkIndex < CodeChunks[MaterialProperty].Num();ChunkIndex++)
				{
					FMaterialUniformExpression* OtherExpression = CodeChunks[MaterialProperty](ChunkIndex).UniformExpression;
					if(OtherExpression && OtherExpression->IsIdentical(UniformExpression))
					{
						delete UniformExpression;
						// Reuse the entry in CodeChunks[MaterialProperty]
						return ChunkIndex;
					}
				}
				delete UniformExpression;
				// Use the existing uniform expression from a different material property,
				// And continue so that a code chunk using the uniform expression will be generated for this material property.
				UniformExpression = TestExpression;
			}
		}

		INT		BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		INT		Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) appRealloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( FormattedCode, BufferSize, BufferSize-1, Format, Format, Result );
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;

		const INT ReturnIndex = CodeChunks[MaterialProperty].Num();
		// Create a new code chunk for the uniform expression
		new(CodeChunks[MaterialProperty]) FShaderCodeChunk(UniformExpression,FormattedCode,Type,Flags);

		if (!bFoundExistingExpression)
		{
			// Add an entry to the material-wide list of uniform expressions
			new(UniformExpressions) FShaderCodeChunk(UniformExpression,FormattedCode,Type,Flags);
		}

		appFree(FormattedCode);
		return ReturnIndex;
	}

	// AccessUniformExpression - Adds code to access the value of a uniform expression to the Code array and returns its index.
	INT AccessUniformExpression(INT Index)
	{
		check(Index >= 0 && Index < CodeChunks[MaterialProperty].Num());
		const FShaderCodeChunk&	CodeChunk = CodeChunks[MaterialProperty](Index);
		check(CodeChunk.UniformExpression && !CodeChunk.UniformExpression->IsConstant());

		FMaterialUniformExpressionTexture* TextureUniformExpression = CodeChunk.UniformExpression->GetTextureUniformExpression();
		// Any code chunk can have a texture uniform expression (eg FMaterialUniformExpressionFlipBookTextureParameter),
		// But a texture code chunk must have a texture uniform expression
		check(!(CodeChunk.Type & MCT_Texture) || TextureUniformExpression);
		if (TextureUniformExpression)
		{
			UTexture* DefaultValue = TextureUniformExpression->GetDefaultTextureValue();
			check(DefaultValue);
			// Add the uniform expression's texture to the material so its reference will be maintained
			// Set the index on the uniform expression so that it can get the corresponding 
			TextureUniformExpression->SetTextureIndex(Material->UniformExpressionTextures.AddUniqueItem(DefaultValue));
		}

		const TCHAR* FrequencyName = GetShaderFrequencyName(ShaderFrequency);
		TCHAR FormattedCode[MAX_SPRINTF]=TEXT("");
		if(CodeChunk.Type == MCT_Float)
		{
			const static TCHAR IndexToMask[] = {'x', 'y', 'z', 'w'};
			const INT ScalarInputIndex = UniformExpressionSet.GetExpresssions(ShaderFrequency).UniformScalarExpressions.AddUniqueItem(CodeChunk.UniformExpression);
			// Update the above appMalloc if this appSprintf grows in size, e.g. %s, ...
			appSprintf(FormattedCode, TEXT("Uniform%sScalars_%u.%c"), FrequencyName, ScalarInputIndex / 4, IndexToMask[ScalarInputIndex % 4]);
		}
		else if(CodeChunk.Type & MCT_Float)
		{
			const INT VectorInputIndex = UniformExpressionSet.GetExpresssions(ShaderFrequency).UniformVectorExpressions.AddUniqueItem(CodeChunk.UniformExpression);
			const TCHAR* Mask;
			switch(CodeChunk.Type)
			{
			case MCT_Float:
			case MCT_Float1: Mask = TEXT(".r"); break;
			case MCT_Float2: Mask = TEXT(".rg"); break;
			case MCT_Float3: Mask = TEXT(".rgb"); break;
			default: Mask = TEXT(""); break;
			};

			appSprintf(FormattedCode, TEXT("Uniform%sVector_%u%s"), FrequencyName, VectorInputIndex, Mask);
		}
		else if(CodeChunk.Type & MCT_Texture)
		{
			INT TextureInputIndex = INDEX_NONE;
			const TCHAR* BaseName = TEXT("");
			switch(CodeChunk.Type)
			{
			case MCT_Texture2D:
				TextureInputIndex = UniformExpressionSet.GetExpresssions(ShaderFrequency).Uniform2DTextureExpressions.AddUniqueItem(TextureUniformExpression);
				BaseName = TEXT("Texture2D");
				break;
			case MCT_TextureCube:
				check(ShaderFrequency == SF_Pixel);
				TextureInputIndex = UniformExpressionSet.UniformCubeTextureExpressions.AddUniqueItem(TextureUniformExpression);
				BaseName = TEXT("TextureCube");
				break;
			default: appErrorf(TEXT("Unrecognized texture material value type: %u"),(INT)CodeChunk.Type);
			};
			appSprintf(FormattedCode, TEXT("%s%s_%u"), FrequencyName, BaseName, TextureInputIndex);
		}
		else
		{
			appErrorf(TEXT("User input of unknown type: %s"),DescribeType(CodeChunk.Type));
		}

		return AddInlinedCodeChunk(CodeChunks[MaterialProperty](Index).Type,0,0,FormattedCode);
	}

	// CoerceParameter
	FString CoerceParameter(INT Index,EMaterialValueType DestType)
	{
		check(Index >= 0 && Index < CodeChunks[MaterialProperty].Num());
		const FShaderCodeChunk&	CodeChunk = CodeChunks[MaterialProperty](Index);
		if( CodeChunk.Type == DestType )
		{
			return GetParameterCode(Index);
		}
		else
			if( (CodeChunk.Type & DestType) && (CodeChunk.Type & MCT_Float) )
			{
				switch( DestType )
				{
				case MCT_Float1:
					return FString::Printf( TEXT("float(%s)"), GetParameterCode(Index) );
				case MCT_Float2:
					return FString::Printf( TEXT("float2(%s,%s)"), GetParameterCode(Index), GetParameterCode(Index) );
				case MCT_Float3:
					return FString::Printf( TEXT("float3(%s,%s,%s)"), GetParameterCode(Index), GetParameterCode(Index), GetParameterCode(Index) );
				case MCT_Float4:
					return FString::Printf( TEXT("float4(%s,%s,%s,%s)"), GetParameterCode(Index), GetParameterCode(Index), GetParameterCode(Index), GetParameterCode(Index) );
				default: 
					return FString::Printf( TEXT("%s"), GetParameterCode(Index) );
				}
			}
			else
			{
				Errorf(TEXT("Coercion failed: %s: %s -> %s"),*CodeChunk.Definition,DescribeType(CodeChunk.Type),DescribeType(DestType));
				return TEXT("");
			}
	}

	// GetParameterType
	EMaterialValueType GetParameterType(INT Index) const
	{
		check(Index >= 0 && Index < CodeChunks[MaterialProperty].Num());
		return CodeChunks[MaterialProperty](Index).Type;
	}

	// GetParameterUniformExpression
	FMaterialUniformExpression* GetParameterUniformExpression(INT Index) const
	{
		check(Index >= 0 && Index < CodeChunks[MaterialProperty].Num());
		return CodeChunks[MaterialProperty](Index).UniformExpression;
	}

	// GetParameterFlags
	DWORD GetParameterFlags(INT Index) const
	{
		check(Index >= 0 && Index < CodeChunks[MaterialProperty].Num());
		return CodeChunks[MaterialProperty](Index).Flags;
	}

	/**
	* Finds the texture dependency length of the given code chunk.
	* @param CodeChunkIndex - The index of the code chunk.
	* @return The texture dependency length of the code chunk.
	*/
	INT GetTextureDependencyLength(INT CodeChunkIndex)
	{
		if(CodeChunkIndex != INDEX_NONE)
		{
			check(CodeChunkIndex >= 0 && CodeChunkIndex < CodeChunks[MaterialProperty].Num());
			return CodeChunks[MaterialProperty](CodeChunkIndex).TextureDependencyLength;
		}
		else
		{
			return 0;
		}
	}

	/**
	* Finds the maximum texture dependency length of the given code chunks.
	* @param CodeChunkIndex* - A list of code chunks to find the maximum texture dependency length from.
	* @return The texture dependency length of the code chunk.
	*/
	INT GetTextureDependencyLengths(INT CodeChunkIndex0,INT CodeChunkIndex1 = INDEX_NONE,INT CodeChunkIndex2 = INDEX_NONE)
	{
		return ::Max(
			::Max(
			GetTextureDependencyLength(CodeChunkIndex0),
			GetTextureDependencyLength(CodeChunkIndex1)
			),
			GetTextureDependencyLength(CodeChunkIndex2)
			);
	}

	// GetArithmeticResultType
	EMaterialValueType GetArithmeticResultType(EMaterialValueType TypeA,EMaterialValueType TypeB)
	{
		if(!(TypeA & MCT_Float) || !(TypeB & MCT_Float))
		{
			Errorf(TEXT("Attempting to perform arithmetic on non-numeric types: %s %s"),DescribeType(TypeA),DescribeType(TypeB));
		}

		if(TypeA == TypeB)
		{
			return TypeA;
		}
		else if(TypeA & TypeB)
		{
			if(TypeA == MCT_Float)
			{
				return TypeB;
			}
			else
			{
				check(TypeB == MCT_Float);
				return TypeA;
			}
		}
		else
		{
			Errorf(TEXT("Arithmetic between types %s and %s are undefined"),DescribeType(TypeA),DescribeType(TypeB));
			return MCT_Unknown;
		}
	}

	EMaterialValueType GetArithmeticResultType(INT A,INT B)
	{
		check(A >= 0 && A < CodeChunks[MaterialProperty].Num());
		check(B >= 0 && B < CodeChunks[MaterialProperty].Num());

		EMaterialValueType	TypeA = CodeChunks[MaterialProperty](A).Type,
			TypeB = CodeChunks[MaterialProperty](B).Type;

		return GetArithmeticResultType(TypeA,TypeB);
	}

	// FMaterialCompiler interface.

	/** 
	 * Sets the current material property being compiled.  
	 * This affects the internal state of the compiler and the results of all functions except GetFixedParameterCode.
	 */
	virtual void SetMaterialProperty(EMaterialProperty InProperty)
	{
		MaterialProperty = InProperty;
		ShaderFrequency = GetMaterialPropertyShaderFrequency(InProperty);
	}

	virtual INT Error(const TCHAR* Text)
	{
		//add the error string to the material's CompileErrors array
		new (Material->CompileErrors) FString(Text);
		bSuccess = FALSE;
		return INDEX_NONE;
	}

	virtual INT CallExpression(UMaterialExpression* MaterialExpression,FMaterialCompiler* Compiler)
	{
		// Check if this expression has already been translated.
		INT* ExistingCodeIndex = ExpressionCodeMap[MaterialProperty].Find(MaterialExpression);
		if(ExistingCodeIndex)
		{
			return *ExistingCodeIndex;
		}
		else
		{
			// Disallow reentrance.
			if(ExpressionStack.FindItemIndex(MaterialExpression) != INDEX_NONE)
			{
				return Error(TEXT("Reentrant expression"));
			}

			// The first time this expression is called, translate it.
			ExpressionStack.AddItem(MaterialExpression);
			INT Result = MaterialExpression->Compile(Compiler);

			UMaterialExpression* PoppedExpression = ExpressionStack.Pop();
			check(PoppedExpression == MaterialExpression);

			// Save the texture dependency depth for the expression.
			if (Material->IsTerrainMaterial() == FALSE)
			{
				const INT TextureDependencyLength = GetTextureDependencyLength(Result);
				Material->TextureDependencyLengthMap.Set(MaterialExpression,TextureDependencyLength);
				Material->MaxTextureDependencyLength = ::Max(Material->MaxTextureDependencyLength,TextureDependencyLength);
			}

			// Cache the translation.
			ExpressionCodeMap[MaterialProperty].Set(MaterialExpression,Result);

			return Result;
		}
	}

	virtual EMaterialValueType GetType(INT Code)
	{
		if(Code != INDEX_NONE)
		{
			return GetParameterType(Code);
		}
		else
		{
			return MCT_Unknown;
		}
	}

	virtual INT ForceCast(INT Code,EMaterialValueType DestType,UBOOL bExactMatch=FALSE,UBOOL bReplicateValue=FALSE)
	{
		if(Code == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(Code) && !GetParameterUniformExpression(Code)->IsConstant())
		{
			return ForceCast(AccessUniformExpression(Code),DestType,bExactMatch,bReplicateValue);
		}

		EMaterialValueType	SourceType = GetParameterType(Code);

		if (bExactMatch ? (SourceType == DestType) : (SourceType & DestType))
		{
			return Code;
		}
		else if((SourceType & MCT_Float) && (DestType & MCT_Float))
		{
			const UINT NumSourceComponents = GetNumComponents(SourceType);
			const UINT NumDestComponents = GetNumComponents(DestType);

			if(NumSourceComponents > NumDestComponents) // Use a mask to select the first NumDestComponents components from the source.
			{
				const TCHAR*	Mask;
				switch(NumDestComponents)
				{
				case 1: Mask = TEXT(".r"); break;
				case 2: Mask = TEXT(".rg"); break;
				case 3: Mask = TEXT(".rgb"); break;
				default: appErrorf(TEXT("Should never get here!")); return INDEX_NONE;
				};

				return AddInlinedCodeChunk(DestType,0,GetTextureDependencyLength(Code),TEXT("%s%s"),GetParameterCode(Code),Mask);
			}
			else if(NumSourceComponents < NumDestComponents) // Pad the source vector up to NumDestComponents.
			{
				// Only allow replication when the Source is a Float1
				if (NumSourceComponents != 1)
				{
					bReplicateValue = FALSE;
				}

				const UINT NumPadComponents = NumDestComponents - NumSourceComponents;
				FString CommaParameterCodeString = FString::Printf(TEXT(",%s"), GetParameterCode(Code));

				return AddInlinedCodeChunk(
					DestType,
					0,
					GetTextureDependencyLength(Code),
					TEXT("%s(%s%s%s%s)"),
					DescribeType(DestType),
					GetParameterCode(Code),
					NumPadComponents >= 1 ? (bReplicateValue ? *CommaParameterCodeString : TEXT(",0")) : TEXT(""),
					NumPadComponents >= 2 ? (bReplicateValue ? *CommaParameterCodeString : TEXT(",0")) : TEXT(""),
					NumPadComponents >= 3 ? (bReplicateValue ? *CommaParameterCodeString : TEXT(",0")) : TEXT("")
					);
			}
			else
			{
				return Code;
			}
		}
		else
		{
			return Errorf(TEXT("Cannot force a cast between non-numeric types."));
		}
	}

	virtual INT VectorParameter(FName ParameterName,const FLinearColor& DefaultValue)
	{
		return AddUniformExpression(new FMaterialUniformExpressionVectorParameter(ParameterName,DefaultValue),MCT_Float4,0,TEXT(""));
	}

	virtual INT ScalarParameter(FName ParameterName,FLOAT DefaultValue)
	{
		return AddUniformExpression(new FMaterialUniformExpressionScalarParameter(ParameterName,DefaultValue),MCT_Float,0,TEXT(""));
	}

	virtual INT FlipBookOffset(UTexture* InFlipBook)
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		return AddUniformExpression(new FMaterialUniformExpressionFlipBookTextureParameter(CastChecked<UTextureFlipBook>(InFlipBook)), MCT_Float4, 0, TEXT(""));
	}

	virtual INT Constant(FLOAT X)
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,0,0,0),MCT_Float),MCT_Float,0,TEXT("(%0.8f)"),X);
	}

	virtual INT Constant2(FLOAT X,FLOAT Y)
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,0,0),MCT_Float2),MCT_Float2,0,TEXT("float2(%0.8f,%0.8f)"),X,Y);
	}

	virtual INT Constant3(FLOAT X,FLOAT Y,FLOAT Z)
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,Z,0),MCT_Float3),MCT_Float3,0,TEXT("float3(%0.8f,%0.8f,%0.8f)"),X,Y,Z);
	}

	virtual INT Constant4(FLOAT X,FLOAT Y,FLOAT Z,FLOAT W)
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,Z,W),MCT_Float4),MCT_Float4,0,TEXT("float4(%0.8f,%0.8f,%0.8f,%0.8f)"),X,Y,Z,W);
	}

	virtual INT GameTime()
	{
		return AddUniformExpression(new FMaterialUniformExpressionTime(),MCT_Float,0,TEXT(""));
	}

	virtual INT RealTime()
	{
		return AddUniformExpression(new FMaterialUniformExpressionRealTime(),MCT_Float,0,TEXT(""));
	}

	virtual INT PeriodicHint(INT PeriodicCode)
	{
		if(PeriodicCode == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(PeriodicCode))
		{
			return AddUniformExpression(new FMaterialUniformExpressionPeriodic(GetParameterUniformExpression(PeriodicCode)),GetParameterType(PeriodicCode),0,TEXT("%s"),GetParameterCode(PeriodicCode));
		}
		else
		{
			return PeriodicCode;
		}
	}

	virtual INT Sine(INT X)
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionSine(GetParameterUniformExpression(X),0),MCT_Float,0,TEXT("sin(%s)"),*CoerceParameter(X,MCT_Float));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),0,GetTextureDependencyLength(X),TEXT("sin(%s)"),GetParameterCode(X));
		}
	}

	virtual INT Cosine(INT X)
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionSine(GetParameterUniformExpression(X),1),MCT_Float,0,TEXT("cos(%s)"),*CoerceParameter(X,MCT_Float));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),0,GetTextureDependencyLength(X),TEXT("cos(%s)"),GetParameterCode(X));
		}
	}

	virtual INT Floor(INT X)
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFloor(GetParameterUniformExpression(X)),GetParameterType(X),0,TEXT("floor(%s)"),GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),0,GetTextureDependencyLength(X),TEXT("floor(%s)"),GetParameterCode(X));
		}
	}

	virtual INT Ceil(INT X)
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionCeil(GetParameterUniformExpression(X)),GetParameterType(X),0,TEXT("ceil(%s)"),GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),0,GetTextureDependencyLength(X),TEXT("ceil(%s)"),GetParameterCode(X));
		}
	}

	virtual INT Frac(INT X)
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFrac(GetParameterUniformExpression(X)),GetParameterType(X),0,TEXT("frac(%s)"),GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),0,GetTextureDependencyLength(X),TEXT("frac(%s)"),GetParameterCode(X));
		}
	}

	virtual INT Fmod(INT A, INT B)
	{
		if ((A == INDEX_NONE) || (B == INDEX_NONE))
		{
			return INDEX_NONE;
		}

		if (GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFmod(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),
				GetParameterType(A),0,TEXT("fmod(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(A),0,GetTextureDependencyLengths(A,B),
				TEXT("fmod(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
	}

	/**
	* Creates the new shader code chunk needed for the Abs expression
	*
	* @param	X - Index to the FMaterialCompiler::CodeChunk entry for the input expression
	* @return	Index to the new FMaterialCompiler::CodeChunk entry for this expression
	*/	
	virtual INT Abs( INT X )
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		// get the user input struct for the input expression
		FMaterialUniformExpression* pInputParam = GetParameterUniformExpression(X);
		if( pInputParam )
		{
			FMaterialUniformExpressionAbs* pUniformExpression = new FMaterialUniformExpressionAbs( pInputParam );
			return AddUniformExpression( pUniformExpression, GetParameterType(X), 0, TEXT("abs(%s)"), GetParameterCode(X) );
		}
		else
		{
			return AddCodeChunk( GetParameterType(X), 0, GetTextureDependencyLength(X), TEXT("abs(%s)"), GetParameterCode(X) );
		}
	}

	virtual INT ReflectionVector()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}

		if( Material->UsesPerPixelCameraVector() )
		{
			// We need the world position of the pixel as well as the various world->tangent functions
			// in order to compute the camera vector for each pixel
			Material->UsingTransforms |= UsedCoord_WorldPos;
			Material->UsingTransforms |= UsedCoord_World;
		}

		return AddInlinedCodeChunk(MCT_Float3,0,0,TEXT("Parameters.TangentReflectionVector"));
	}

	virtual INT CameraVector()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		return AddInlinedCodeChunk(MCT_Float3,0,0,TEXT("Parameters.TangentCameraVector"));
	}

	virtual INT CameraWorldPosition()
	{
		return AddInlinedCodeChunk(MCT_Float3,0,0,TEXT("CameraWorldPos"));
	}

	virtual INT LightVector()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		return AddInlinedCodeChunk(MCT_Float3,0,0,TEXT("Parameters.TangentLightVector"));
	}

	virtual INT ScreenPosition(  UBOOL bScreenAlign )
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		if( bScreenAlign )
		{
			return AddCodeChunk(MCT_Float4,0,0,TEXT("ScreenAlignedPosition(Parameters.ScreenPosition)"));		
		}
		else
		{
			return AddInlinedCodeChunk(MCT_Float4,0,0,TEXT("Parameters.ScreenPosition"));		
		}	
	}

	virtual INT ParticleMacroUV(UBOOL bUseViewSpace) 
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		if (bUseViewSpace)
		{
			return AddCodeChunk(MCT_Float2,0,0,TEXT("GetViewSpaceMacroUVs(Parameters)"));
		}
		else
		{
			return AddCodeChunk(MCT_Float2,0,0,TEXT("GetScreenSpaceMacroUVs(Parameters)"));
		}
	}

	virtual INT WorldPosition()
	{
		if (ShaderFrequency == SF_Pixel)
		{
			Material->UsingTransforms |= UsedCoord_WorldPos;
		}
		return AddInlinedCodeChunk(MCT_Float3,0,0,TEXT("Parameters.WorldPosition"));	
	}

	virtual INT ObjectWorldPosition()
	{
		return AddInlinedCodeChunk(MCT_Float3,0,0,TEXT("ObjectWorldPositionAndRadius.xyz"));		
	}

	virtual INT ObjectRadius()
	{
		return AddInlinedCodeChunk(MCT_Float,0,0,TEXT("ObjectWorldPositionAndRadius.w"));		
	}

	virtual INT If(INT A,INT B,INT AGreaterThanB,INT AEqualsB,INT ALessThanB)
	{
		if(A == INDEX_NONE || B == INDEX_NONE || AGreaterThanB == INDEX_NONE || AEqualsB == INDEX_NONE || ALessThanB == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType ResultType = GetArithmeticResultType(GetParameterType(AGreaterThanB),GetArithmeticResultType(AEqualsB,ALessThanB));

		INT CoercedAGreaterThanB = ForceCast(AGreaterThanB,ResultType);
		INT CoercedAEqualsB = ForceCast(AEqualsB,ResultType);
		INT CoercedALessThanB = ForceCast(ALessThanB,ResultType);

		if(CoercedAGreaterThanB == INDEX_NONE || CoercedAEqualsB == INDEX_NONE || CoercedALessThanB == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		return AddCodeChunk(
			ResultType,
			0,
			::Max(GetTextureDependencyLengths(A,B),GetTextureDependencyLengths(AGreaterThanB,AEqualsB,ALessThanB)),
			TEXT("((%s >= %s) ? (%s > %s ? %s : %s) : %s)"),
			GetParameterCode(A),
			GetParameterCode(B),
			GetParameterCode(A),
			GetParameterCode(B),
			GetParameterCode(CoercedAGreaterThanB),
			GetParameterCode(CoercedAEqualsB),
			GetParameterCode(CoercedALessThanB)
			);
	}

	virtual INT TextureCoordinate(UINT CoordinateIndex, UBOOL UnMirrorU, UBOOL UnMirrorV)
	{
		Material->NumUserTexCoords = ::Max(CoordinateIndex + 1,Material->NumUserTexCoords);
		if (Material->NumUserTexCoords > 0)
		{
			FString	SampleCode;
			if (Material->IsUsedWithDecals())
			{
				if ( UnMirrorU && UnMirrorV )
				{
					SampleCode = TEXT("UnMirrorUV(Parameters.TexCoords[0].xy, Parameters)");
				}
				else if ( UnMirrorU )
				{
					SampleCode = TEXT("UnMirrorU(Parameters.TexCoords[0].xy, Parameters)");
				}
				else if ( UnMirrorV )
				{
					SampleCode = TEXT("UnMirrorV(Parameters.TexCoords[0].xy, Parameters)");
				}
				else
				{
					SampleCode = TEXT("Parameters.TexCoords[0].xy");
				}
			}
			else
			{
				if ( UnMirrorU && UnMirrorV )
				{
					SampleCode = TEXT("UnMirrorUV(Parameters.TexCoords[%u].xy, Parameters)");
				}
				else if ( UnMirrorU )
				{
					SampleCode = TEXT("UnMirrorU(Parameters.TexCoords[%u].xy, Parameters)");
				}
				else if ( UnMirrorV )
				{
					SampleCode = TEXT("UnMirrorV(Parameters.TexCoords[%u].xy, Parameters)");
				}
				else
				{
					SampleCode = TEXT("Parameters.TexCoords[%u].xy");
				}
			}

			if ( UnMirrorU || UnMirrorV )
			{
				// to use UnMirroring, need WORLD_COORDS defined
				Material->UsingTransforms |= UsedCoord_World;
			}

			return AddCodeChunk(
				MCT_Float2,0,0,
				*SampleCode,
				CoordinateIndex
				);
		}
		else
		{
			return AddCodeChunk(
				MCT_Float2,0,0,
				TEXT("float2(0,0)"),
				CoordinateIndex
				);
		}
	}

	virtual INT TextureSample(INT TextureIndex,INT CoordinateIndex)
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Domain && ShaderFrequency != SF_Hull)
		{
			return NonPixelShaderExpressionError();
		}
		if(TextureIndex == INDEX_NONE || CoordinateIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType	TextureType = GetParameterType(TextureIndex);
		DWORD				Flags		= GetParameterFlags(TextureIndex);

		FString				SampleCode;

		if (ShaderFrequency == SF_Pixel)
		{
			switch(TextureType)
			{
			case MCT_Texture2D:
				SampleCode = TEXT("tex2Dbias(%s,float4(%s,0,Parameters.MipBias))");
				break;
			case MCT_TextureCube:
				SampleCode = TEXT("texCUBE(%s,%s)");
				break;
			}
		}
		else
		{
			switch(TextureType)
			{
			case MCT_Texture2D:
				SampleCode = TEXT("tex2Dlod(%s,float4(%s,0.f,0.f))");
				break;
			case MCT_TextureCube:
				SampleCode = TEXT("texCUBElod(%s,float4(%s,0.f))");
				break;
			}
		}

		if( Flags & SCF_RGBE_4BIT_EXPONENT )
		{
			SampleCode = FString::Printf( TEXT("ExpandCompressedRGBE(%s)"), *SampleCode );
		}

		if( Flags & SCF_RGBE_8BIT_EXPONENT )
		{
			SampleCode = FString::Printf( TEXT("ExpandRGBE(%s)"), *SampleCode );
		}

		if( Flags & SCF_GREYSCALE_TEXTURE )
		{
			// Sampling a greyscale texture in D3D9 gives: (G,G,G)
			// Sampling a greyscale texture in D3D11 gives: (G,0,0)
			// This replication reproduces the D3D9 behavior in all cases.
			SampleCode = FString::Printf( TEXT("(%s).rrrr"), *SampleCode );
		}

		switch(TextureType)
		{
		case MCT_Texture2D:
			return AddCodeChunk(
				MCT_Float4,
				0,
				GetTextureDependencyLength(CoordinateIndex) + 1,
				*SampleCode,
				*CoerceParameter(TextureIndex,MCT_Texture2D),
				*CoerceParameter(CoordinateIndex,MCT_Float2)
				);
		case MCT_TextureCube:
			return AddCodeChunk(
				MCT_Float4,
				0,
				GetTextureDependencyLength(CoordinateIndex) + 1,
				*SampleCode,
				*CoerceParameter(TextureIndex,MCT_TextureCube),
				*CoerceParameter(CoordinateIndex,MCT_Float3)
				);
		default:
			Errorf(TEXT("Sampling unknown texture type: %s"),DescribeType(TextureType));
			return INDEX_NONE;
		};
	}

	/**
	* Add the shader code for sampling from the scene texture
	* @param	TexType - scene texture type to sample from
	* @param	CoordinateIdx - index of shader code for user specified tex coords
	* @param	ScreenAlign - Whether to map [0,1] UVs to the view within the back buffer
	*/
	virtual INT SceneTextureSample( BYTE TexType, INT CoordinateIdx, UBOOL ScreenAlign )
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}

		// use the scene texture type 
		FString SceneTexCode;
		switch(TexType)
		{
		case SceneTex_Lighting:
			SceneTexCode = FString(TEXT("SceneColorTexture"));
			break;
		default:
			Errorf(TEXT("Scene texture type not supported."));
			return INDEX_NONE;
		}

		TCHAR* FetchFunctionName = TEXT("tex2D");

		if(Material->IsUsedWithMaterialEffect())
		{
			FetchFunctionName = TEXT("GetMaterialEffectSceneColor");
		}

		Material->bUsesSceneColor = TRUE;
		if ( ScreenAlign && CoordinateIdx != INDEX_NONE )
		{
			// sampler
			FString SampleCode( TEXT("%s(%s,ScreenAlignedUV(%s))") );
			// add the code string
			return AddCodeChunk(
				MCT_Float4,
				0,
				GetTextureDependencyLength(CoordinateIdx) + 1,
				*SampleCode,
				FetchFunctionName,
				*SceneTexCode,
				*CoerceParameter(CoordinateIdx,MCT_Float2)
				);
		}
		else
		{
			// sampler
			FString	SampleCode( TEXT("%s(%s,%s)") );
			// replace default tex coords with user specified coords if available
			FString DefaultScreenAligned(TEXT("float2(ScreenAlignedPosition(Parameters.ScreenPosition).xy)"));
			FString TexCoordCode( (CoordinateIdx != INDEX_NONE) ? CoerceParameter(CoordinateIdx,MCT_Float2) : DefaultScreenAligned );
			// add the code string
			return AddCodeChunk(
				MCT_Float4,
				0,
				GetTextureDependencyLength(CoordinateIdx) + 1,
				*SampleCode,
				FetchFunctionName,
				*SceneTexCode,
				*TexCoordCode
				);
		}
	}

	/**
	* Add the shader code for sampling the scene depth
	* @param	bNormalize - @todo implement
	* @param	CoordinateIdx - index of shader code for user specified tex coords
	*/
	virtual INT SceneTextureDepth( UBOOL bNormalize, INT CoordinateIdx)
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		Material->bUsesSceneDepth = TRUE;

		// sampler
		FString	UserDepthCode( TEXT("CalcSceneDepth(%s)") );
		// replace default tex coords with user specified coords if available
		FString DefaultScreenAligned(TEXT("float2(ScreenAlignedPosition(Parameters.ScreenPosition).xy)"));
		FString TexCoordCode( (CoordinateIdx != INDEX_NONE) ? CoerceParameter(CoordinateIdx,MCT_Float2) : DefaultScreenAligned );
		// add the code string
		return AddCodeChunk(
			MCT_Float1,
			0,
			GetTextureDependencyLength(CoordinateIdx) + 1,
			*UserDepthCode,
			*TexCoordCode
			);
	}

	virtual INT PixelDepth(UBOOL bNormalize)
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		return AddInlinedCodeChunk(MCT_Float1, 0, 0, TEXT("Parameters.ScreenPosition.w"));		
	}

	virtual INT DestColor()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		// note: can just call
		// SceneTextureSample(SceneTex_Lighting,INDEX_NONE);

		Material->bUsesSceneColor = TRUE;
		FString	UserColorCode(TEXT("PreviousLighting(%s)"));
		FString	ScreenPosCode(TEXT("Parameters.ScreenPosition"));
		// add the code string
		return AddCodeChunk(
			MCT_Float4,
			0,
			1,
			*UserColorCode,
			*ScreenPosCode
			);
	}

	virtual INT DestDepth(UBOOL bNormalize)
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		// note: can just call
		// SceneTextureDepth(FALSE,INDEX_NONE);

		Material->bUsesSceneDepth = TRUE;
		FString	UserDepthCode(TEXT("PreviousDepth(%s)"));
		FString	ScreenPosCode(TEXT("Parameters.ScreenPosition"));
		// add the code string
		return AddCodeChunk(
			MCT_Float1,
			0,
			1,
			*UserDepthCode,
			*ScreenPosCode
			);
	}

	/**
	* Generates a shader code chunk for the DepthBiasedAlpha expression
	* using the given inputs
	* @param SrcAlphaIdx = index to source alpha input expression code chunk
	* @param BiasIdx = index to bias input expression code chunk
	* @param BiasScaleIdx = index to a scale expression code chunk to apply to the bias
	*/
	virtual INT DepthBiasedAlpha( INT SrcAlphaIdx, INT BiasIdx, INT BiasScaleIdx )
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		INT ResultIdx = INDEX_NONE;

		// all inputs must be valid expressions
		if ((SrcAlphaIdx != INDEX_NONE) &&
			(BiasIdx != INDEX_NONE) &&
			(BiasScaleIdx != INDEX_NONE))
		{
			Material->bUsesSceneDepth = TRUE;
			FString CodeChunk(TEXT("DepthBiasedAlpha(Parameters,%s,%s,%s)"));
			ResultIdx = AddCodeChunk(
				MCT_Float1,
				0,
				::Max(GetTextureDependencyLengths(SrcAlphaIdx,BiasIdx,BiasScaleIdx),1),
				*CodeChunk,
				*CoerceParameter(SrcAlphaIdx,MCT_Float1),
				*CoerceParameter(BiasIdx,MCT_Float1),
				*CoerceParameter(BiasScaleIdx,MCT_Float1)
				);
		}

		return ResultIdx;
	}

	/**
	* Generates a shader code chunk for the DepthBiasedBlend expression
	* using the given inputs
	* @param SrcColorIdx = index to source color input expression code chunk
	* @param BiasIdx = index to bias input expression code chunk
	* @param BiasScaleIdx = index to a scale expression code chunk to apply to the bias
	*/
	virtual INT DepthBiasedBlend( INT SrcColorIdx, INT BiasIdx, INT BiasScaleIdx )
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		INT ResultIdx = INDEX_NONE;

		// all inputs must be valid expressions
		if( SrcColorIdx != INDEX_NONE && 
			BiasIdx != INDEX_NONE &&
			BiasScaleIdx != INDEX_NONE )
		{
			FString CodeChunk( TEXT("DepthBiasedBlend(Parameters,%s,%s,%s)") );
			ResultIdx = AddCodeChunk(
				MCT_Float3,
				0,
				::Max(GetTextureDependencyLengths(SrcColorIdx,BiasIdx,BiasScaleIdx),1),
				*CodeChunk,
				*CoerceParameter(SrcColorIdx,MCT_Float3),
				*CoerceParameter(BiasIdx,MCT_Float1),
				*CoerceParameter(BiasScaleIdx,MCT_Float1)
				);

			Material->bUsesSceneDepth = TRUE;
			Material->bUsesSceneColor = TRUE;
		}

		return ResultIdx;			
	}

	virtual INT FluidDetailNormal(INT TextureCoordinate)
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}

		if (TextureCoordinate == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		INT FluidNormalXY = INDEX_NONE;
		if (Material->IsUsedWithFluidSurfaces())
		{
			// Create the vector parameters which will scale the texture coordinate to align the detail normal with the detail grid
			// These parameters are set by the fluid surface proxy before rendering
			INT DetailCoordOffset = ForceCast(VectorParameter(FName(TEXT("DetailCoordOffset")), FLinearColor::Black), MCT_Float2);
			INT DetailCoordScale = ForceCast(VectorParameter(FName(TEXT("DetailCoordScale")), FLinearColor::Black), MCT_Float2);
			INT ScaledCoord = Mul(Sub(TextureCoordinate, DetailCoordOffset), DetailCoordScale);
			// Lookup the detail fluid normal x and y
			INT TextureCodeIndex = TextureParameter(FName(TEXT("FluidDetailNormal")), GEngine->DefaultTexture);
			FluidNormalXY = ForceCast(TextureSample(TextureCodeIndex,ScaledCoord), MCT_Float2);
		}
		else
		{
			FluidNormalXY = AddCodeChunk(MCT_Float2, 0, 0, TEXT("tex2D(FluidDetailNormalTexture,%s)"), *CoerceParameter(TextureCoordinate, MCT_Float2));
		}

		// Derive the detail fluid normal z, assuming that the tangent space normal z will always be positive
		// z = sqrt(1 - ( x * x + y * y));
		INT DotResult = Dot(FluidNormalXY, FluidNormalXY);
		INT InnerResult = Sub(Constant(1), DotResult);
		INT DerivedZ = SquareRoot(InnerResult);
		INT DerivedFluidNormal = ForceCast(AppendVector(FluidNormalXY, DerivedZ), MCT_Float3);
		return DerivedFluidNormal;
	}	

	static DWORD GetTextureFlags(UTexture* InTexture)
	{
		// Determine the texture's format.
		UTexture2D* Texture2D = Cast<UTexture2D>(InTexture);
		UTextureCube* TextureCube = Cast<UTextureCube>(InTexture);
		BYTE Format = PF_Unknown;
		if(Texture2D)
		{
			Format = Texture2D->Format;
		}
		else if(TextureCube)
		{
			// Make sure the texture has had PostLoad called, because UTextureCube::Format is not serialized but is set based on the cube faces in UTextureCube::PostLoad.
			TextureCube->ConditionalPostLoad();
			Format = TextureCube->Format;
		}

		// Determine the appropriate flags for the texture based on its format.
		DWORD Flags = 0;
		if(InTexture->RGBE)
		{
			Flags |= (Format == PF_A8R8G8B8) ? SCF_RGBE_8BIT_EXPONENT : SCF_RGBE_4BIT_EXPONENT;
		}
		if(Format == PF_G8)
		{
			Flags |= SCF_GREYSCALE_TEXTURE;
		}
		if(Format == PF_A8R8G8B8)
		{
			Flags |= SCF_A8R8G8B8_TEXTURE;
		}
		return Flags;
	}

	virtual INT Texture(UTexture* InTexture)
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Domain && ShaderFrequency != SF_Hull)
		{
			return NonPixelShaderExpressionError();
		}
		EMaterialValueType ShaderType = InTexture->GetMaterialType();
		return AddUniformExpression(new FMaterialUniformExpressionTexture(InTexture),ShaderType,GetTextureFlags(InTexture),TEXT(""));
	}

	virtual INT TextureParameter(FName ParameterName,UTexture* DefaultValue)
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Domain && ShaderFrequency != SF_Hull)
		{
			return NonPixelShaderExpressionError();
		}
		EMaterialValueType ShaderType = DefaultValue->GetMaterialType();
		return AddUniformExpression(new FMaterialUniformExpressionTextureParameter(ParameterName,DefaultValue),ShaderType,GetTextureFlags(DefaultValue),TEXT(""));
	}

	virtual INT BiasNormalizeNormalMap(INT Texture, BYTE CompressionSettings)
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Domain && ShaderFrequency != SF_Hull)
		{
			return NonPixelShaderExpressionError();
		}
		if( Texture != INDEX_NONE )
		{
			switch( CompressionSettings )
			{
			case TC_Normalmap:
				return AddCodeChunk(MCT_Float4,0,GetTextureDependencyLength(Texture),TEXT("BiasNormalizeNormalMap_DXT1(%s)"),GetParameterCode(Texture));
			case TC_NormalmapAlpha:
				return AddCodeChunk(MCT_Float4,0,GetTextureDependencyLength(Texture),TEXT("BiasNormalizeNormalMap_DXT5(%s)"),GetParameterCode(Texture));
			case TC_NormalmapUncompressed:
				return AddCodeChunk(MCT_Float4,0,GetTextureDependencyLength(Texture),TEXT("BiasNormalizeNormalMap_V8U8(%s)"),GetParameterCode(Texture));
			case TC_NormalmapBC5:
				return AddCodeChunk(MCT_Float4,0,GetTextureDependencyLength(Texture),TEXT("BiasNormalizeNormalMap_BC5(%s)"),GetParameterCode(Texture));
			default:
				break;
			}
		}
		return INDEX_NONE;
	}

	virtual INT VertexColor()
	{
		return AddInlinedCodeChunk(MCT_Float4,0,0,TEXT("Parameters.VertexColor"));
	}

	virtual INT Add(INT A,INT B)
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Add),GetArithmeticResultType(A,B),0,TEXT("(%s + %s)"),GetParameterCode(A),GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),0,GetTextureDependencyLengths(A,B),TEXT("(%s + %s)"),GetParameterCode(A),GetParameterCode(B));
		}
	}

	virtual INT Sub(INT A,INT B)
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Sub),GetArithmeticResultType(A,B),0,TEXT("(%s - %s)"),GetParameterCode(A),GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),0,GetTextureDependencyLengths(A,B),TEXT("(%s - %s)"),GetParameterCode(A),GetParameterCode(B));
		}
	}

	virtual INT Mul(INT A,INT B)
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Mul),GetArithmeticResultType(A,B),0,TEXT("(%s * %s)"),GetParameterCode(A),GetParameterCode(B));
		}
		else
		{
			EMaterialValueType TypeA = GetType(A);
			EMaterialValueType TypeB = GetType(B);

			//work around a compiler bug with the 180 PS3 SDK 
			//where UniformVector_0.rgb * UniformVector_0.a is incorrectly optimized as UniformVector_0.rgb
			//but UniformVector_0.a * UniformVector_0.rgb compiles correctly
			if (TypeA != TypeB && TypeB == MCT_Float)
			{
				//swap the multiplication order
				INT SwapTemp = A;
				A = B;
				B = SwapTemp;
			}

			return AddCodeChunk(GetArithmeticResultType(A,B),0,GetTextureDependencyLengths(A,B),TEXT("(%s * %s)"),GetParameterCode(A),GetParameterCode(B));
		}
	}

	virtual INT Div(INT A,INT B)
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Div),GetArithmeticResultType(A,B),0,TEXT("(%s / %s)"),GetParameterCode(A),GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),0,GetTextureDependencyLengths(A,B),TEXT("(%s / %s)"),GetParameterCode(A),GetParameterCode(B));
		}
	}

	virtual INT Dot(INT A,INT B)
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Dot),MCT_Float,0,TEXT("dot(%s,%s)"),GetParameterCode(A),GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(MCT_Float,0,GetTextureDependencyLengths(A,B),TEXT("dot(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
	}

	virtual INT Cross(INT A,INT B)
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		return AddCodeChunk(MCT_Float3,0,GetTextureDependencyLengths(A,B),TEXT("cross(%s,%s)"),*CoerceParameter(A,MCT_Float3),*CoerceParameter(B,MCT_Float3));
	}

	virtual INT Power(INT Base,INT Exponent)
	{
		if(Base == INDEX_NONE || Exponent == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		FString ExponentCode = CoerceParameter(Exponent,MCT_Float);
		if (CodeChunks[MaterialProperty](Exponent).UniformExpression && CodeChunks[MaterialProperty](Exponent).UniformExpression->IsConstant())
		{
			//chop off the parenthesis
			FString NumericPortion = ExponentCode.Mid(1, ExponentCode.Len() - 2);
			FLOAT ExponentValue = appAtof(*NumericPortion); 
			//check if the power was 1.0f to work around a xenon HLSL compiler bug in the Feb XDK
			//which incorrectly optimizes pow(x, 1.0f) as if it were pow(x, 0.0f).
			if (fabs(ExponentValue - 1.0f) < (FLOAT)KINDA_SMALL_NUMBER)
			{
				return Base;
			}
		}

		// use ClampedPow so artist are prevented to cause NAN creeping into the math
		return AddCodeChunk(GetParameterType(Base),0,GetTextureDependencyLengths(Base,Exponent),TEXT("ClampedPow(%s,%s)"),GetParameterCode(Base),*ExponentCode);
	}

	virtual INT SquareRoot(INT X)
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionSquareRoot(GetParameterUniformExpression(X)),GetParameterType(X),0,TEXT("sqrt(%s)"),GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),0,GetTextureDependencyLength(X),TEXT("sqrt(%s)"),GetParameterCode(X));
		}
	}

	virtual INT Length(INT X)
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionLength(GetParameterUniformExpression(X)),MCT_Float,0,TEXT("length(%s)"),GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(MCT_Float,0,GetTextureDependencyLength(X),TEXT("length(%s)"),GetParameterCode(X));
		}
	}

	virtual INT Lerp(INT X,INT Y,INT A)
	{
		if(X == INDEX_NONE || Y == INDEX_NONE || A == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType ResultType = GetArithmeticResultType(X,Y);
		EMaterialValueType AlphaType = ResultType == CodeChunks[MaterialProperty](A).Type ? ResultType : MCT_Float1;
		return AddCodeChunk(ResultType,0,GetTextureDependencyLengths(X,Y,A),TEXT("lerp(%s,%s,%s)"),*CoerceParameter(X,ResultType),*CoerceParameter(Y,ResultType),*CoerceParameter(A,AlphaType));
	}

	virtual INT Min(INT A,INT B)
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionMin(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),GetParameterType(A),0,TEXT("min(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(A),0,GetTextureDependencyLengths(A,B),TEXT("min(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
	}

	virtual INT Max(INT A,INT B)
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionMax(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),GetParameterType(A),0,TEXT("max(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(A),0,GetTextureDependencyLengths(A,B),TEXT("max(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
	}

	virtual INT Clamp(INT X,INT A,INT B)
	{
		if(X == INDEX_NONE || A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X) && GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionClamp(GetParameterUniformExpression(X),GetParameterUniformExpression(A),GetParameterUniformExpression(B)),GetParameterType(X),0,TEXT("min(max(%s,%s),%s)"),GetParameterCode(X),*CoerceParameter(A,GetParameterType(X)),*CoerceParameter(B,GetParameterType(X)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),0,GetTextureDependencyLengths(X,A,B),TEXT("min(max(%s,%s),%s)"),GetParameterCode(X),*CoerceParameter(A,GetParameterType(X)),*CoerceParameter(B,GetParameterType(X)));
		}
	}

	virtual INT ComponentMask(INT Vector,UBOOL R,UBOOL G,UBOOL B,UBOOL A)
	{
		if(Vector == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType	VectorType = GetParameterType(Vector);

		if(	A && (VectorType & MCT_Float) < MCT_Float4 ||
			B && (VectorType & MCT_Float) < MCT_Float3 ||
			G && (VectorType & MCT_Float) < MCT_Float2 ||
			R && (VectorType & MCT_Float) < MCT_Float1)
			Errorf(TEXT("Not enough components in (%s: %s) for component mask %u%u%u%u"),GetParameterCode(Vector),DescribeType(GetParameterType(Vector)),R,G,B,A);

		EMaterialValueType	ResultType;
		switch((R ? 1 : 0) + (G ? 1 : 0) + (B ? 1 : 0) + (A ? 1 : 0))
		{
		case 1: ResultType = MCT_Float; break;
		case 2: ResultType = MCT_Float2; break;
		case 3: ResultType = MCT_Float3; break;
		case 4: ResultType = MCT_Float4; break;
		default: Errorf(TEXT("Couldn't determine result type of component mask %u%u%u%u"),R,G,B,A); return INDEX_NONE;
		};

		return AddInlinedCodeChunk(
			ResultType,
			0,
			GetTextureDependencyLength(Vector),
			TEXT("%s.%s%s%s%s"),
			GetParameterCode(Vector),
			R ? TEXT("r") : TEXT(""),
			// If VectorType is set to MCT_Float which means it could be any of the float types, assume it is a float1
			G ? (VectorType == MCT_Float ? TEXT("r") : TEXT("g")) : TEXT(""),
			B ? (VectorType == MCT_Float ? TEXT("r") : TEXT("b")) : TEXT(""),
			A ? (VectorType == MCT_Float ? TEXT("r") : TEXT("a")) : TEXT("")
			);
	}

	virtual INT AppendVector(INT A,INT B)
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		INT					NumResultComponents = GetNumComponents(GetParameterType(A)) + GetNumComponents(GetParameterType(B));
		EMaterialValueType	ResultType = GetVectorType(NumResultComponents);

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionAppendVector(GetParameterUniformExpression(A),GetParameterUniformExpression(B),GetNumComponents(GetParameterType(A))),ResultType,0,TEXT("float%u(%s,%s)"),NumResultComponents,GetParameterCode(A),GetParameterCode(B));
		}
		else
		{
			return AddInlinedCodeChunk(ResultType,0,GetTextureDependencyLengths(A,B),TEXT("float%u(%s,%s)"),NumResultComponents,GetParameterCode(A),GetParameterCode(B));
		}
	}

	/**
	* Generate shader code for transforming a vector
	*/
	virtual INT TransformVector(BYTE SourceCoordType,BYTE DestCoordType,INT A)
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Domain && ShaderFrequency != SF_Vertex)
		{
			return NonPixelShaderExpressionError();
		}

		const EMaterialVectorCoordTransformSource SourceCoordinateSpace = (EMaterialVectorCoordTransformSource)SourceCoordType;
		const EMaterialVectorCoordTransform DestinationCoordinateSpace = (EMaterialVectorCoordTransform)DestCoordType;

		// Construct float3(0,0,x) out of the input if it is a scalar
		// This way artists can plug in a scalar and it will be treated as height, or a vector displacement
		if(A != INDEX_NONE && (GetType(A) & MCT_Float1) && SourceCoordinateSpace == TRANSFORMSOURCE_Tangent)
		{
			A = AppendVector(Constant2(0, 0), A);
		}

		INT Result = INDEX_NONE;
		if(A != INDEX_NONE)
		{
			INT NumInputComponents = GetNumComponents(GetParameterType(A));
			// only allow float3/float4 transforms
			if( NumInputComponents < 3 )
			{
				Result = Errorf(TEXT("input must be a vector (%s: %s) or a scalar (if source is Tangent)"),GetParameterCode(A),DescribeType(GetParameterType(A)));
			}
			else if (SourceCoordinateSpace == TRANSFORMSOURCE_World && DestinationCoordinateSpace == TRANSFORM_World
				|| SourceCoordinateSpace == TRANSFORMSOURCE_Local && DestinationCoordinateSpace == TRANSFORM_Local
				|| SourceCoordinateSpace == TRANSFORMSOURCE_Tangent && DestinationCoordinateSpace == TRANSFORM_Tangent)
			{
				Result = Errorf(TEXT("Source and Destination coordinate spaces must be different"));
			}
			else 
			{
				// code string to transform the input vector
				FString CodeStr;
				if (SourceCoordinateSpace == TRANSFORMSOURCE_Tangent)
				{
					switch( DestinationCoordinateSpace )
					{
					case TRANSFORM_Local:
						// transform from tangent to local space
						if(ShaderFrequency != SF_Pixel)
						{
							return Errorf(TEXT("Local space in only supported for pixel shader!"));
						}
						CodeStr = FString(TEXT("TransformTangentVectorToLocal(Parameters,%s)"));
						break;
					case TRANSFORM_World:
						// transform from tangent to world space
						CodeStr = FString(TEXT("TransformTangentVectorToWorld(Parameters.TangentToWorld,%s)"));
						break;
					case TRANSFORM_View:
						// transform from tangent to view space
						if(ShaderFrequency != SF_Pixel)
						{
							return Errorf(TEXT("View space in only supported for pixel shader!"));
						}
						CodeStr = FString(TEXT("TransformTangentVectorToView(Parameters,%s)"));
						break;
					default:
						appErrorf( TEXT("Invalid DestCoordType. See EMaterialVectorCoordTransform") );
					}
				}
				else if (SourceCoordinateSpace == TRANSFORMSOURCE_Local)
				{
					if(ShaderFrequency != SF_Pixel)
					{
						return Errorf(TEXT("Local space in only supported for pixel shader!"));
					}

					switch( DestinationCoordinateSpace )
					{
					case TRANSFORM_Tangent:
						CodeStr = FString(TEXT("TransformLocalVectorToTangent(Parameters,%s)"));
						break;
					case TRANSFORM_World:
						CodeStr = FString(TEXT("TransformLocalVectorToWorld(%s)"));
						break;
					case TRANSFORM_View:
						CodeStr = FString(TEXT("TransformLocalVectorToView(%s)"));
						break;
					default:
						appErrorf( TEXT("Invalid DestCoordType. See EMaterialVectorCoordTransform") );
					}
				}
				else if (SourceCoordinateSpace == TRANSFORMSOURCE_World)
				{
					switch( DestinationCoordinateSpace )
					{
					case TRANSFORM_Tangent:
						CodeStr = FString(TEXT("TransformWorldVectorToTangent(Parameters.TangentToWorld,%s)"));
						break;
					case TRANSFORM_Local:
						if(ShaderFrequency != SF_Pixel)
						{
							return Errorf(TEXT("Local space in only supported for pixel shader!"));
						}
						CodeStr = FString(TEXT("TransformWorldVectorToLocal(%s)"));
						break;
					case TRANSFORM_View:
						if(ShaderFrequency != SF_Pixel)
						{
							return Errorf(TEXT("View space in only supported for pixel shader!"));
						}
						CodeStr = FString(TEXT("TransformWorldVectorToView(%s)"));
						break;
					default:
						appErrorf( TEXT("Invalid DestCoordType. See EMaterialVectorCoordTransform") );
					}
				}
				else
				{
					appErrorf( TEXT("Invalid SourceCoordType. See EMaterialVectorCoordTransformSource") );
				}

				Material->UsingTransforms |= UsedCoord_World;
				
				// we are only transforming vectors (not points) so only return a float3
				Result = AddCodeChunk(
					MCT_Float3,
					0,
					GetTextureDependencyLength(A),
					*CodeStr,
					*CoerceParameter(A,MCT_Float3)
					);
			}
		}
		return Result; 
	}

	/**
	* Generate shader code for transforming a position
	*
	* @param	CoordType - type of transform to apply. see EMaterialExpressionTransformPosition 
	* @param	A - index for input vector parameter's code
	*/
	virtual INT TransformPosition(BYTE CoordType,INT A)
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		INT Result = INDEX_NONE;
		if(A != INDEX_NONE)
		{
			INT NumInputComponents = GetNumComponents(GetParameterType(A));
			// only allow float4 transforms
			if( NumInputComponents < 3 )
			{
				Result = Errorf(TEXT("Input must be a float4 (%s: %s)"),GetParameterCode(A),DescribeType(GetParameterType(A)));
			}
			else if( NumInputComponents < 4 )
			{
				Result = Errorf(TEXT("Input must be a float4, append 1 to the vector (%s: %s)"),GetParameterCode(A),DescribeType(GetParameterType(A)));
			}
			else
			{
				// code string to transform the input vector
				FString CodeStr;
				switch( CoordType )
				{
				case TRANSFORMPOS_World:
					// transform from post projection to world space
					CodeStr = FString(TEXT("MulMatrix(InvViewProjectionMatrix, %s)"));
					break;
				default:
					appErrorf( TEXT("Invalid CoordType. See EMaterialExpressionTransformPosition") );
				}

				// we are transforming points, not vectors, so return a float4
				Result = AddCodeChunk(
					MCT_Float4,
					0,
					GetTextureDependencyLength(A),
					*CodeStr,
					*CoerceParameter(A,MCT_Float4)
					);
			}
		}
		return Result; 
	}

	INT LensFlareIntesity()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		INT ResultIdx = INDEX_NONE;
		FString CodeChunk(TEXT("GetLensFlareIntensity(Parameters)"));
		ResultIdx = AddCodeChunk(
			MCT_Float1,
			0,
			0,
			*CodeChunk
			);
		return ResultIdx;
	}

	INT LensFlareOcclusion()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		INT ResultIdx = INDEX_NONE;
		FString CodeChunk(TEXT("GetLensFlareOcclusion(Parameters)"));
		ResultIdx = AddCodeChunk(
			MCT_Float1,
			0,
			0,
			*CodeChunk
			);
		return ResultIdx;
	}

	INT LensFlareRadialDistance()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		INT ResultIdx = INDEX_NONE;
		FString CodeChunk(TEXT("GetLensFlareRadialDistance(Parameters)"));
		ResultIdx = AddCodeChunk(
			MCT_Float1,
			0,
			0,
			*CodeChunk
			);
		return ResultIdx;
	}

	INT LensFlareRayDistance()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		INT ResultIdx = INDEX_NONE;
		FString CodeChunk(TEXT("GetLensFlareRayDistance(Parameters)"));
		ResultIdx = AddCodeChunk(
			MCT_Float1,
			0,
			0,
			*CodeChunk
			);
		return ResultIdx;
	}

	INT LensFlareSourceDistance()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		INT ResultIdx = INDEX_NONE;
		FString CodeChunk(TEXT("GetLensFlareSourceDistance(Parameters)"));
		ResultIdx = AddCodeChunk(
			MCT_Float1,
			0,
			0,
			*CodeChunk
			);
		return ResultIdx;
	}

	INT DynamicParameter()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		Material->bUsesDynamicParameter = TRUE;

		INT ResultIdx = INDEX_NONE;
		//@todo.SAS. Verify InParamaterIndex is in proper range...
		FString CodeChunk = FString::Printf(TEXT("GetDynamicParameter(Parameters)"));
		ResultIdx = AddCodeChunk(
			MCT_Float4,
			0,
			0,
			*CodeChunk
			);
		return ResultIdx;
	}

	INT LightmapUVs() 
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		Material->bUsesLightmapUVs = TRUE;

		INT ResultIdx = INDEX_NONE;
		FString CodeChunk = FString::Printf(TEXT("GetLightmapUVs(Parameters)"));
		ResultIdx = AddCodeChunk(
			MCT_Float2,
			0,
			0,
			*CodeChunk
			);
		return ResultIdx;
	}

	INT LightmassReplace(INT Realtime, INT Lightmass) { return Realtime; }

	INT ObjectOrientation() 
	{ 
		return AddInlinedCodeChunk(MCT_Float3,0,0,TEXT("ObjectOrientation"));	
	}

	INT WindDirectionAndSpeed() 
	{
		return AddInlinedCodeChunk(MCT_Float4,0,0,TEXT("WindDirectionAndSpeed"));	
	}

	INT FoliageImpulseDirection() 
	{
		return AddInlinedCodeChunk(MCT_Float3,0,0,TEXT("FoliageImpulseDirection"));	
	}

	INT FoliageNormalizedRotationAxisAndAngle() 
	{
		return AddInlinedCodeChunk(MCT_Float4,0,0,TEXT("FoliageNormalizedRotationAxisAndAngle"));	
	}

	INT RotateAboutAxis(INT NormalizedRotationAxisAndAngleIndex, INT PositionOnAxisIndex, INT PositionIndex)
	{
		if (NormalizedRotationAxisAndAngleIndex == INDEX_NONE
			|| PositionOnAxisIndex == INDEX_NONE
			|| PositionIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		else
		{
			return AddCodeChunk(
				MCT_Float3,
				0,
				0,
				TEXT("RotateAboutAxis(%s,%s,%s)"),
				*CoerceParameter(NormalizedRotationAxisAndAngleIndex,MCT_Float4),
				*CoerceParameter(PositionOnAxisIndex,MCT_Float3),
				*CoerceParameter(PositionIndex,MCT_Float3)
				);	
		}
	}

	INT TwoSidedSign() 
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		return AddInlinedCodeChunk(MCT_Float,0,0,TEXT("Parameters.TwoSidedSign"));	
	}

	INT WorldNormal() 
	{
		if (ShaderFrequency == SF_Pixel)
		{
			return Errorf(TEXT("Invalid node WorldNormal used in pixel shader input!"));
		}
		return AddInlinedCodeChunk(MCT_Float3,0,0,TEXT("TransformTangentVectorToWorld(Parameters.TangentToWorld,float3(0,0,1))"));	
	}

	INT DDX( INT X ) 
	{
		return AddCodeChunk(GetParameterType(X),0,GetTextureDependencyLength(X),TEXT("ddx(%s)"),GetParameterCode(X));	
	}

	INT DDY( INT X ) 
	{
		return AddCodeChunk(GetParameterType(X),0,GetTextureDependencyLength(X),TEXT("ddy(%s)"),GetParameterCode(X));	
	}

	INT AntialiasedTextureMask(INT Tex, INT UV, float Threshold, BYTE Channel)
	{
		INT ThresholdConst = Constant(Threshold);
		INT ChannelConst = Constant(Channel);

		return AddCodeChunk(MCT_Float, 
			0, 
			GetTextureDependencyLength(UV),
			TEXT("AntialiasedTextureMask(%s,%s,%s,%s)"), 
			GetParameterCode(Tex),
			GetParameterCode(UV),
			GetParameterCode(ThresholdConst),
			GetParameterCode(ChannelConst));
	}

	INT DepthOfFieldFunction(INT Depth, INT FunctionValueIndex)
	{
		if (ShaderFrequency == SF_Hull)
		{
			return Errorf(TEXT("Invalid node DepthOfFieldFunction used in hull shader input!"));
		}

		return AddCodeChunk(MCT_Float, 
			0, 
			GetTextureDependencyLength(Depth),
			TEXT("MaterialExpressionDepthOfFieldFunction(%s, %d)"), 
			GetParameterCode(Depth), FunctionValueIndex);
	}

	INT CustomExpression( class UMaterialExpressionCustom* Custom, TArray<INT>& CompiledInputs )
	{
		INT ResultIdx = INDEX_NONE;

		FString OutputTypeString;
		EMaterialValueType OutputType;
		switch( Custom->OutputType )
		{
		case CMOT_Float2:
			OutputType = MCT_Float2;
			OutputTypeString = TEXT("float2");
			break;
		case CMOT_Float3:
			OutputType = MCT_Float3;
			OutputTypeString = TEXT("float3");
			break;
		case CMOT_Float4:
			OutputType = MCT_Float4;
			OutputTypeString = TEXT("float4");
			break;
		default:
			OutputType = MCT_Float1;
			OutputTypeString = TEXT("float");
			break;
		}

		// Declare implementation function
		FString InputParamDecl;
		check( Custom->Inputs.Num() == CompiledInputs.Num() );
		for( INT i = 0; i < Custom->Inputs.Num(); i++ )
		{
			// skip over unnamed inputs
			if( Custom->Inputs(i).InputName.Len()==0 )
			{
				continue;
			}
			InputParamDecl += TEXT(",");
			switch(GetParameterType(CompiledInputs(i)))
			{
			case MCT_Float:
			case MCT_Float1:
				InputParamDecl += TEXT("float ");
				break;
			case MCT_Float2:
				InputParamDecl += TEXT("float2 ");
				break;
			case MCT_Float3:
				InputParamDecl += TEXT("float3 ");
				break;
			case MCT_Float4:
				InputParamDecl += TEXT("float4 ");
				break;
			case MCT_Texture2D:
				InputParamDecl += TEXT("sampler2D ");
				break;
			default:
				return Errorf(TEXT("Bad type %s for %s input %s"),DescribeType(GetParameterType(CompiledInputs(i))), *Custom->Description, *Custom->Inputs(i).InputName);
				break;
			}
			InputParamDecl += Custom->Inputs(i).InputName;
		}
		INT CustomExpressionIndex = CustomExpressionImplementations.Num();
		FString Code = Custom->Code;
		if( Code.InStr(TEXT("return")) == -1 )
		{
			Code = FString(TEXT("return "))+Code+TEXT(";");
		}
		Code.ReplaceInline(TEXT("\n"),TEXT("\r\n"));
		FString ImplementationCode = FString::Printf(TEXT("%s CustomExpression%d(FMaterial%sParameters Parameters%s)\r\n{\r\n%s\r\n}\r\n"), *OutputTypeString, CustomExpressionIndex, ShaderFrequency==SF_Vertex?TEXT("Vertex"):TEXT("Pixel"), *InputParamDecl, *Code);
		CustomExpressionImplementations.AddItem( ImplementationCode );

		// Add call to implementation function
		FString CodeChunk = FString::Printf(TEXT("CustomExpression%d(Parameters"), CustomExpressionIndex);
		for( INT i = 0; i < CompiledInputs.Num(); i++ )
		{
			// skip over unnamed inputs
			if( Custom->Inputs(i).InputName.Len()==0 )
			{
				continue;
			}
			CodeChunk += TEXT(",");
			CodeChunk += GetParameterCode(CompiledInputs(i));
		}
		CodeChunk += TEXT(")");

		ResultIdx = AddCodeChunk(
			OutputType,
			0,
			0,
			*CodeChunk
			);
		return ResultIdx;
	}

	INT OcclusionPercentage()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		INT ResultIdx = INDEX_NONE;
		FString CodeChunk(TEXT("GetOcclusionPercentage()"));
		ResultIdx = AddCodeChunk(
			MCT_Float1,
			0,
			0,
			*CodeChunk
			);
		return ResultIdx;
	}

	/**
	 * Adds code to return a random value shared by all geometry for any given instanced static mesh
	 *
	 * @return	Code index
	 */
	virtual INT PerInstanceRandom()
	{
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}
		else
		{
			// Currently, instance meshes transport random instance ID to the pixel shader
			// in the vertex color interpolator (as vertex color isn't supported on instanced
			// static meshes yet.)
			return AddInlinedCodeChunk(MCT_Float1, 0, 0, TEXT("Parameters.VertexColor.x"));
		}
	}
};
#endif

/**
* Compiles this material for Platform, storing the result in OutShaderMap
*
* @param StaticParameters - the set of static parameters to compile
* @param Platform - the platform to compile for
* @param OutShaderMap - the shader map to compile
* @param bForceCompile - force discard previous results 
* @param bDebugDump - Dump out the preprocessed and disassembled shader for debugging.
* @return - TRUE if compile succeeded or was not necessary (shader map for StaticParameters was found and was complete)
*/
UBOOL FMaterial::Compile(FStaticParameterSet* StaticParameters, EShaderPlatform Platform, TRefCountPtr<FMaterialShaderMap>& OutShaderMap, UBOOL bForceCompile, UBOOL bDebugDump)
{
#if !CONSOLE
	// Generate the material shader code.
	FUniformExpressionSet NewUniformExpressionSet;
	FHLSLMaterialTranslator MaterialTranslator(this,NewUniformExpressionSet,Platform);
	UBOOL bSuccess = MaterialTranslator.Translate();

	if(bSuccess)
	{
		const FString MaterialShaderCode = MaterialTranslator.GetMaterialShaderCode();
		bSuccess = CompileShaderMap(StaticParameters, Platform, NewUniformExpressionSet, OutShaderMap, MaterialShaderCode, bForceCompile, bDebugDump);
	}

	return bSuccess;
#else
	appErrorf(TEXT("Not supported."));
	return FALSE;
#endif
}

/**
* Compiles OutShaderMap using the shader code from MaterialShaderCode on Platform
*
* @param StaticParameters - the set of static parameters to compile
* @param Platform - the platform to compile for
* @param OutShaderMap - the shader map to compile
* @param MaterialShaderCode - a filled out instance of MaterialTemplate.usf to compile
* @param bForceCompile - force discard previous results 
* @param bDebugDump - Dump out the preprocessed and disassembled shader for debugging.
* @return - TRUE if compile succeeded or was not necessary (shader map for StaticParameters was found and was complete)
*/
UBOOL FMaterial::CompileShaderMap( 
	 const FStaticParameterSet* StaticParameters, 
	 EShaderPlatform Platform, 
	 const FUniformExpressionSet& UniformExpressionSet,
	 TRefCountPtr<FMaterialShaderMap>& OutShaderMap, 
	 const FString& MaterialShaderCode, 
	 UBOOL bForceCompile, 
	 UBOOL bDebugDump)
{
	FMaterialShaderMap* ExistingShaderMap = NULL;

	// if we want to force compile the material, there's no reason to check for an existing one
	if (bForceCompile)
	{
		UShaderCache::FlushId(*StaticParameters, Platform);
		if (ShaderMap)
		{
			ShaderMap->BeginRelease();
		}
		ShaderMap = NULL;
	}
	else
	{
		// see if it's already compiled
		ExistingShaderMap = FMaterialShaderMap::FindId(*StaticParameters, Platform);
	}

	OutShaderMap = ExistingShaderMap;
	if (!OutShaderMap)
	{
		// Create a shader map for the material on the given platform
		OutShaderMap = new FMaterialShaderMap;
	}

	UBOOL bSuccess = TRUE;
	UBOOL bRequiredCompile = FALSE;
	if(!ExistingShaderMap || !ExistingShaderMap->IsComplete(this, FALSE))
	{
		bRequiredCompile = TRUE;

		// Compile the shaders for the material.
		bSuccess = OutShaderMap->Compile(this,StaticParameters,*MaterialShaderCode,UniformExpressionSet,Platform,CompileErrors,bDebugDump);
		if (bSuccess)
		{
			//@todo - track down offenders and re-enable
			//check(OutShaderMap->IsUniformExpressionSetValid());
		}
	}

	if(bSuccess)
	{
		if (OutShaderMap->GetUniformExpressionSet().IsEmpty())
		{
			// The shader map's expression set was empty, it is legacy and should be overwritten with the newly generated set.
			OutShaderMap->SetUniformExpressions(UniformExpressionSet);
		}
		// Every FMaterial sharing the same shader map must generate the same uniform expression set to be used with the shared shader map.
		// Shader map sharing happens with material instances with the same base material and same set of static parameters,
		// Or with UMaterials that have been duplicated outside of the editor (filesystem copy) and have not been recompiled since.
		// Any code that changes the way uniform expressions are generated needs to bump the appropriate version version to discard outdated shader maps.
		else if (!(OutShaderMap->GetUniformExpressionSet() == UniformExpressionSet))
		{
			/*
			warnf(
				TEXT("Translated uniform expression set was different than the cached shader map with the same Id! \n")
				TEXT("	New: Base material %s, bRequiredCompile %u, ExpressionSet %s \n")
				TEXT("	Cached: Shadermap name %s, Id %s, ExpressionSet %s \n"),
				*GetBaseMaterialPathName(), 
				bRequiredCompile,
				*UniformExpressionSet.GetSummaryString(),
				*OutShaderMap->GetFriendlyName(), 
				*OutShaderMap->GetMaterialId().GetSummaryString(),
				*OutShaderMap->GetUniformExpressionSet().GetSummaryString()
				);*/
		}
		check(OutShaderMap->IsUniformExpressionSetValid());

		// Only initialize the shaders if no recompile was required or if we are not deferring shader compiling
		if (!bRequiredCompile || !DeferFinishCompiling() && !GShaderCompilingThreadManager->IsDeferringCompilation())
		{
			ShaderMap->BeginInit();
		}
	}
	else
	{
		// Clear the shader map if the compilation failed, to ensure that the incomplete shader map isn't used.
		OutShaderMap = NULL;
	}

	// Store that we have up date compilation output.
	bValidCompilationOutput = TRUE;

	return bSuccess;
}

/**
* Caches the material shaders for this material with no static parameters on the given platform.
*/
UBOOL FMaterial::CacheShaders(EShaderPlatform Platform, UBOOL bFlushExistingShaderMap)
{
	if (bFlushExistingShaderMap)
	{
		// Discard the ID and make a new one.
		Id = appCreateGuid();
	}

	FStaticParameterSet EmptySet(Id);
	return CacheShaders(&EmptySet, Platform, bFlushExistingShaderMap);
}

/**
* Caches the material shaders for the given static parameter set and platform
*/
UBOOL FMaterial::CacheShaders(FStaticParameterSet* StaticParameters, EShaderPlatform Platform, UBOOL bFlushExistingShaderMap, UBOOL bDebugDump)
{
	//flush the render command queue before changing the ShaderMap, since the rendering thread may be reading from it
	FlushRenderingCommands();

	if (bFlushExistingShaderMap)
	{
		FlushShaderMap();
	}

	if(!Id.IsValid())
	{
		// If the material doesn't have a valid ID, create a new one. 
		// This can happen if it is a new StaticPermutationResource, since it will never go through InitShaderMap().
		// On StaticPermutationResources, this Id is only used to track dirty state and not to find the matching shader map.
		Id = appCreateGuid();
	}

	// Reset the compile errors array.
	CompileErrors.Empty();

	// Release references to any existing shader map
	if (ShaderMap)
	{
		ShaderMap->BeginRelease();
		ShaderMap = NULL;
	}

	// Compile the material shaders for the current platform.
	return Compile(StaticParameters, Platform, ShaderMap, bFlushExistingShaderMap, bDebugDump);
}

/**
 * Should the shader for this material with the given platform, shader type and vertex 
 * factory type combination be compiled
 *
 * @param Platform		The platform currently being compiled for
 * @param ShaderType	Which shader is being compiled
 * @param VertexFactory	Which vertex factory is being compiled (can be NULL)
 *
 * @return TRUE if the shader should be compiled
 */
UBOOL FMaterial::ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
{
	return TRUE;
}

/** Evaluates uniform expression chains if needed and stores the results in this cache object. */
void FShaderFrequencyUniformExpressionValues::Update(
	const FShaderFrequencyUniformExpressions& UniformExpressions, 
	const FMaterialRenderContext& MaterialRenderContext, 
	const FMaterial* Material,
	UBOOL bForceUpdate)
{
	// Cache the pixel uniform expression values for this frame if they haven't already been cached
	if (bForceUpdate
		|| CachedFrameNumber != MaterialRenderContext.View->FrameNumber
		|| CachedFrameNumber == UINT_MAX)
	{
		CachedFrameNumber = MaterialRenderContext.View->FrameNumber;

		// Pack 4 scalar uniform expression values into the same vector so they can be set together,
		// Which saves shader constant registers and CPU time
		CachedScalarParameters.Empty((UniformExpressions.UniformScalarExpressions.Num() + 3) / 4);
		CachedScalarParameters.Add((UniformExpressions.UniformScalarExpressions.Num() + 3) / 4);

		INT ScalarIndex = 0;
		for (; ScalarIndex < UniformExpressions.UniformScalarExpressions.Num() - 4; ScalarIndex += 4)
		{
			//@todo - add a version of GetNumberValue which takes a FLOAT instead of FLinearColor
			FLinearColor Value0;
			FLinearColor Value1;
			FLinearColor Value2;
			FLinearColor Value3;
			UniformExpressions.UniformScalarExpressions(ScalarIndex + 0)->GetNumberValue(MaterialRenderContext, Value0);
			UniformExpressions.UniformScalarExpressions(ScalarIndex + 1)->GetNumberValue(MaterialRenderContext, Value1);
			UniformExpressions.UniformScalarExpressions(ScalarIndex + 2)->GetNumberValue(MaterialRenderContext, Value2);
			UniformExpressions.UniformScalarExpressions(ScalarIndex + 3)->GetNumberValue(MaterialRenderContext, Value3);
			CachedScalarParameters(ScalarIndex / 4) = FVector4(Value0.R, Value1.R, Value2.R, Value3.R);
		}

		// Handle the last float4 of values
		if (ScalarIndex < UniformExpressions.UniformScalarExpressions.Num())
		{
			FLinearColor Value0;
			FLinearColor Value1(0,0,0);
			FLinearColor Value2(0,0,0);
			FLinearColor Value3(0,0,0);
			UniformExpressions.UniformScalarExpressions(ScalarIndex + 0)->GetNumberValue(MaterialRenderContext, Value0);
			if (ScalarIndex + 1 < UniformExpressions.UniformScalarExpressions.Num())
			{
				UniformExpressions.UniformScalarExpressions(ScalarIndex + 1)->GetNumberValue(MaterialRenderContext, Value1);
				if (ScalarIndex + 2 < UniformExpressions.UniformScalarExpressions.Num())
				{
					UniformExpressions.UniformScalarExpressions(ScalarIndex + 2)->GetNumberValue(MaterialRenderContext, Value2);
					if (ScalarIndex + 3 < UniformExpressions.UniformScalarExpressions.Num())
					{
						UniformExpressions.UniformScalarExpressions(ScalarIndex + 3)->GetNumberValue(MaterialRenderContext, Value3);
					}
				}
			}
			CachedScalarParameters(ScalarIndex / 4) = FVector4(Value0.R, Value1.R, Value2.R, Value3.R);
		}

		CachedVectorParameters.Empty(UniformExpressions.UniformVectorExpressions.Num());
		CachedVectorParameters.Add(UniformExpressions.UniformVectorExpressions.Num());

		for (INT VectorIndex = 0; VectorIndex < UniformExpressions.UniformVectorExpressions.Num(); VectorIndex++)
		{
			checkAtCompileTime(sizeof(FLinearColor) == sizeof(FVector4),flinearcolor_and_fvector4_must_be_isomorphic);
			UniformExpressions.UniformVectorExpressions(VectorIndex)->GetNumberValue(
				MaterialRenderContext, 
				// we can drop this right into place
				*(FLinearColor*)&CachedVectorParameters(VectorIndex));
		}

		CachedTexture2DParameters.Empty(UniformExpressions.Uniform2DTextureExpressions.Num());
		CachedTexture2DParameters.Add(UniformExpressions.Uniform2DTextureExpressions.Num());

		for (INT VectorIndex = 0; VectorIndex < UniformExpressions.Uniform2DTextureExpressions.Num(); VectorIndex++)
		{
			const FTexture* Value = NULL;
			UniformExpressions.Uniform2DTextureExpressions(VectorIndex)->GetTextureValue(MaterialRenderContext,*Material,Value);
			if (!Value)
			{
				Value = GWhiteTexture;
			}
			CachedTexture2DParameters(VectorIndex) = Value;
		}
	}
}

//
// FColoredMaterialRenderProxy implementation.
//

const FMaterial* FColoredMaterialRenderProxy::GetMaterial() const
{
	return Parent->GetMaterial();
}

/**
 * Finds the shader matching the template type and the passed in vertex factory, asserts if not found.
 */
FShader* FMaterial::GetShader(FMeshMaterialShaderType* ShaderType, FVertexFactoryType* VertexFactoryType) const
{
	const FMeshMaterialShaderMap* MeshShaderMap = ShaderMap->GetMeshShaderMap(VertexFactoryType);
	FShader* Shader = MeshShaderMap ? MeshShaderMap->GetShader(ShaderType) : NULL;
	if (!Shader)
	{
		// Get the ShouldCache results that determine whether the shader should be compiled
		UBOOL bMaterialShouldCache = ShouldCache(GRHIShaderPlatform, ShaderType, VertexFactoryType);
		UBOOL bVFShouldCache = VertexFactoryType->ShouldCache(GRHIShaderPlatform, this, ShaderType);
		UBOOL bShaderShouldCache = ShaderType->ShouldCache(GRHIShaderPlatform, this, VertexFactoryType);
		FString MaterialUsage = GetMaterialUsageDescription();

		// Assert with detailed information if the shader wasn't found for rendering.  
		// This is usually the result of an incorrect ShouldCache function.
		appErrorf(
			TEXT("Couldn't find Shader %s for Material Resource %s!\n")
			TEXT("		With VF=%s, Platform=%s \n")
			TEXT("		ShouldCache: Mat=%u, VF=%u, Shader=%u \n")
			TEXT("		Material Usage = %s"),
			ShaderType->GetName(), 
			*GetFriendlyName(),
			VertexFactoryType->GetName(),
			ShaderPlatformToText(GRHIShaderPlatform),
			bMaterialShouldCache,
			bVFShouldCache,
			bShaderShouldCache,
			*MaterialUsage
			);
	}

	return Shader;
}

/** Rebuilds the information about all texture lookups. */
void FMaterial::RebuildTextureLookupInfo( UMaterial *Material )
{
	TextureLookups.Empty();

	INT NumExpressions = Material->Expressions.Num();
	for(INT ExpressionIndex = 0;ExpressionIndex < NumExpressions; ExpressionIndex++)
	{
		UMaterialExpression* Expression = Material->Expressions(ExpressionIndex);
		UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression);

		if(TextureSample)
		{
			FTextureLookup Lookup;
			Lookup.TexCoordIndex = 0;
			Lookup.TextureIndex = INDEX_NONE;
			Lookup.UScale = 1.0f;
			Lookup.VScale = 1.0f;

			//@TODO: Check to see if this texture lookup is actually used.

			if ( TextureSample->Coordinates.Expression )
			{
				UMaterialExpressionTextureCoordinate* TextureCoordinate =
					Cast<UMaterialExpressionTextureCoordinate>( TextureSample->Coordinates.Expression );
				UMaterialExpressionTerrainLayerCoords* TerrainTextureCoordinate =
					Cast<UMaterialExpressionTerrainLayerCoords>( TextureSample->Coordinates.Expression );

				if ( TextureCoordinate )
				{
					// Use the specified texcoord.
					Lookup.TexCoordIndex = TextureCoordinate->CoordinateIndex;
					Lookup.UScale = TextureCoordinate->UTiling;
					Lookup.VScale = TextureCoordinate->VTiling;
				}
				else if ( TerrainTextureCoordinate )
				{
					// Use the specified texcoord.
					Lookup.UScale = TerrainTextureCoordinate->MappingScale;
					Lookup.VScale = TerrainTextureCoordinate->MappingScale;
				}
				else
				{
					// Too complex texcoord expression, ignore.
					continue;
				}
			}

			UMaterialExpressionTextureSampleParameter2D* TextureSampleParameter = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression);
			UMaterialExpressionTextureSampleParameterNormal* TextureSampleParameterNormal = Cast<UMaterialExpressionTextureSampleParameterNormal>(Expression);

			// Find where the texture is stored in the Uniform2DTextureExpressions array.
			if ( TextureSampleParameter && TextureSampleParameter->Texture )
			{
				const INT ReferencedTextureIndex = UniformExpressionTextures.FindItemIndex(TextureSampleParameter->Texture);
				FMaterialUniformExpressionTextureParameter TextureExpression(TextureSampleParameter->ParameterName, TextureSampleParameter->Texture);
				TextureExpression.SetTextureIndex(ReferencedTextureIndex);
				Lookup.TextureIndex = FindExpression( GetUniform2DTextureExpressions(), TextureExpression );
			}
			else if ( TextureSampleParameterNormal && TextureSampleParameterNormal->Texture )
			{
				const INT ReferencedTextureIndex = UniformExpressionTextures.FindItemIndex(TextureSampleParameterNormal->Texture);
				FMaterialUniformExpressionTextureParameter TextureExpression(TextureSampleParameterNormal->ParameterName, TextureSampleParameterNormal->Texture);
				TextureExpression.SetTextureIndex(ReferencedTextureIndex);
				Lookup.TextureIndex = FindExpression( GetUniform2DTextureExpressions(), TextureExpression );
			}
			else if ( TextureSample->Texture )
			{
				const INT ReferencedTextureIndex = UniformExpressionTextures.FindItemIndex(TextureSample->Texture);
				FMaterialUniformExpressionTexture TextureExpression(TextureSample->Texture);
				TextureExpression.SetTextureIndex(ReferencedTextureIndex);
				Lookup.TextureIndex = FindExpression( GetUniform2DTextureExpressions(), TextureExpression );
			}

			if ( Lookup.TextureIndex >= 0 )
			{
				TextureLookups.AddItem( Lookup );
			}
		}
	}
}

/** Returns the index to the Expression in the Expressions array, or -1 if not found. */
INT FMaterial::FindExpression( const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >&Expressions, const FMaterialUniformExpressionTexture &Expression )
{
	for (INT ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ++ExpressionIndex)
	{
		if ( Expressions(ExpressionIndex)->IsIdentical(&Expression) )
		{
			return ExpressionIndex;
		}
	}
	return -1;
}

void FMaterial::AddLegacyTextures(const TArray<UTexture*>& InTextures) 
{ 
	// If there are legacy uniform expressions, gather all referenced textures from them on top of what was passed in.
	// This is necessary because the legacy method of maintaining texture references was updated in PostLoad,
	// But the new method only updates them after compiling, 
	// So it's possible that the passed in legacy textures don't contain all textures referenced by uniform expressions.
	if (LegacyUniformExpressions)
	{
		// Iterate over both the 2D textures and cube texture expressions.
		const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2] =
		{
			&LegacyUniformExpressions->PixelExpressions.Uniform2DTextureExpressions,
			&LegacyUniformExpressions->UniformCubeTextureExpressions
		};
		for(INT TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
		{
			const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& Expressions = *ExpressionsByType[TypeIndex];

			// Iterate over each of the texture expressions.
			for(INT ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
			{
				FMaterialUniformExpressionTexture* Expression = Expressions(ExpressionIndex);
				UTexture* Texture = Expression->GetLegacyReferencedTexture();
				if (Texture)
				{
					UniformExpressionTextures.AddUniqueItem(Texture);
				}
			}
		}
	}

	AddReferencedTextures(InTextures);
}

void FMaterial::AddReferencedTextures(const TArray<UTexture*>& InTextures)
{
	for (INT i = 0; i < InTextures.Num(); i++)
	{
		UniformExpressionTextures.AddUniqueItem(InTextures(i)); 
	}
}

TSet<FMaterial*> FMaterial::EditorLoadedMaterialResources;

/** Serialize a texture lookup info. */
void FMaterial::FTextureLookup::Serialize(FArchive& Ar)
{
	Ar << TexCoordIndex;
	Ar << TextureIndex;
	if( Ar.Ver() < VER_FONT_FORMAT_AND_UV_TILING_CHANGES )
	{
		// Legacy versions only stored a single scalar for texture tiling
		FLOAT UAndVScale = 1.0f;
		Ar << UAndVScale;
		UScale = UAndVScale;
		VScale = UAndVScale;
	}
	else
	{
		Ar << UScale;
		Ar << VScale;
	}
}

/*-----------------------------------------------------------------------------
	FColoredMaterialRenderProxy
-----------------------------------------------------------------------------*/

UBOOL FColoredMaterialRenderProxy::GetVectorValue(const FName& ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	if(ParameterName == NAME_Color)
	{
		*OutValue = Color;
		return TRUE;
	}
	else
	{
		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}
}

UBOOL FColoredMaterialRenderProxy::GetScalarValue(const FName& ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetScalarValue(ParameterName, OutValue, Context);
}

UBOOL FColoredMaterialRenderProxy::GetTextureValue(const FName& ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetTextureValue(ParameterName,OutValue,Context);
}

/*-----------------------------------------------------------------------------
	FLightingDensityMaterialRenderProxy
-----------------------------------------------------------------------------*/
static FName NAME_LightmapRes = FName(TEXT("LightmapRes"));

UBOOL FLightingDensityMaterialRenderProxy::GetVectorValue(const FName& ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	if (ParameterName == NAME_LightmapRes)
	{
		*OutValue = FLinearColor(LightmapResolution.X, LightmapResolution.Y, 0.0f, 0.0f);
		return TRUE;
	}
	return FColoredMaterialRenderProxy::GetVectorValue(ParameterName, OutValue, Context);
}

/*-----------------------------------------------------------------------------
	FScalarReplacementMaterialRenderProxy
-----------------------------------------------------------------------------*/

const FMaterial* FScalarReplacementMaterialRenderProxy::GetMaterial() const
{
	return Parent->GetMaterial();
}

UBOOL FScalarReplacementMaterialRenderProxy::GetVectorValue(const FName& ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetVectorValue(ParameterName, OutValue, Context);
}

UBOOL FScalarReplacementMaterialRenderProxy::GetScalarValue(const FName& ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
{
	if( ParameterName == ScalarName )
	{
		*OutValue = ScalarValue;
		return TRUE;
	}
	else
	{
		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}
}

UBOOL FScalarReplacementMaterialRenderProxy::GetTextureValue(const FName& ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetTextureValue(ParameterName,OutValue,Context);
}

/*-----------------------------------------------------------------------------
	FTexturedMaterialRenderProxy
-----------------------------------------------------------------------------*/

const FMaterial* FTexturedMaterialRenderProxy::GetMaterial() const
{
	return Parent->GetMaterial();
}

UBOOL FTexturedMaterialRenderProxy::GetVectorValue(const FName& ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	if(ParameterName == NAME_Color)
	{
		*OutValue = Color;
		return TRUE;
	}
	else
	{
		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}
}
	
UBOOL FTexturedMaterialRenderProxy::GetScalarValue(const FName& ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetScalarValue(ParameterName, OutValue, Context);
}

UBOOL FTexturedMaterialRenderProxy::GetTextureValue(const FName& ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
{
	if(ParameterName == NAME_Texture)
	{
		*OutValue = Texture;
		return TRUE;
	}
	else
	{
		return Parent->GetTextureValue(ParameterName,OutValue,Context);
	}
}

/*-----------------------------------------------------------------------------
	FFontMaterialRenderProxy
-----------------------------------------------------------------------------*/

const class FMaterial* FFontMaterialRenderProxy::GetMaterial() const
{
	return Parent->GetMaterial();
}

UBOOL FFontMaterialRenderProxy::GetVectorValue(const FName& ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetVectorValue(ParameterName, OutValue, Context);
}

UBOOL FFontMaterialRenderProxy::GetScalarValue(const FName& ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetScalarValue(ParameterName, OutValue, Context);
}

UBOOL FFontMaterialRenderProxy::GetTextureValue(const FName& ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
{
	// find the matching font parameter
	if( ParameterName == FontParamName &&
		Font->Textures.IsValidIndex(FontPage) )
	{
		// use the texture page from the font specified for the parameter
		UTexture2D* Texture = Font->Textures(FontPage);
		if( Texture && Texture->Resource )
		{
			*OutValue = Texture->Resource;
			return TRUE;
		}		
	}
	// try parent if not valid parameter
	return Parent->GetTextureValue(ParameterName,OutValue,Context);
}

/*-----------------------------------------------------------------------------
	FOverrideSelectionColorMaterialRenderProxy
-----------------------------------------------------------------------------*/

const FMaterial* FOverrideSelectionColorMaterialRenderProxy::GetMaterial() const
{
	return Parent->GetMaterial();
}

UBOOL FOverrideSelectionColorMaterialRenderProxy::GetVectorValue(const FName& ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	if( ParameterName == NAME_SelectionColor )
	{
		*OutValue = SelectionColor;
		return TRUE;
	}
	else
	{
		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}
}

UBOOL FOverrideSelectionColorMaterialRenderProxy::GetScalarValue(const FName& ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetScalarValue(ParameterName,OutValue,Context);
}

UBOOL FOverrideSelectionColorMaterialRenderProxy::GetTextureValue(const FName& ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
{
	return Parent->GetTextureValue(ParameterName,OutValue,Context);
}

/** Returns the number of samplers used in this material. */
INT FMaterialResource::GetSamplerUsage() const
{
	INT TextureParameters = 0;
	TArray<UTexture*> Textures;

	const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2] =
	{
		&GetUniform2DTextureExpressions(),
		&GetUniformCubeTextureExpressions()
	};

	for(INT TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
	{
		const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& Expressions = *ExpressionsByType[TypeIndex];

		// Iterate over each of the material's texture expressions.
		for(INT ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
		{
			UTexture* Texture = NULL;
			FMaterialUniformExpressionTexture* Expression = Expressions(ExpressionIndex);
			Expression->GetGameThreadTextureValue(Material,*this,Texture);

			// TextureParameter expressions always count as one sampler.
			if( Expression->GetType() == &FMaterialUniformExpressionTextureParameter::StaticType )
			{
				TextureParameters++;
			}
			else if( Texture )
			{
				Textures.AddUniqueItem(Texture);
			}
		}
	}

	return Textures.Num() + TextureParameters;
}

/** Returns a string that describes the material's usage for debugging purposes. */
FString FMaterialResource::GetMaterialUsageDescription() const
{
	check(Material);
	FString BaseDescription = GetLightingModelString(GetLightingModel()) + TEXT(", ") + GetBlendModeString(GetBlendMode());

	if (IsSpecialEngineMaterial())
	{
		BaseDescription += TEXT(", SpecialEngine");
	}
	if (IsTwoSided())
	{
		BaseDescription += TEXT(", TwoSided");
	}
	if (IsMasked())
	{
		BaseDescription += TEXT(", Masked");
	}
	if (IsDistorted())
	{
		BaseDescription += TEXT(", Distorted");
	}

	for (INT MaterialUsageIndex = 0; MaterialUsageIndex < MATUSAGE_MAX; MaterialUsageIndex++)
	{
		if (Material->GetUsageByFlag((EMaterialUsage)MaterialUsageIndex))
		{
			BaseDescription += FString(TEXT(", ")) + Material->GetUsageName((EMaterialUsage)MaterialUsageIndex);
		}
	}

	return BaseDescription;
}

/**
 * Get user source code for the material, with a list of code snippets to highlight representing the code for each MaterialExpression
 * @param OutSource - generated source code
 * @param OutHighlightMap - source code highlight list
 * @return - TRUE on Success
 */
UBOOL FMaterial::GetMaterialExpressionSource( FString& OutSource, TMap<UMaterialExpression*,INT>* OutExpressionCodeMap )
{
#if !CONSOLE
	class FViewSourceMaterialTranslator : public FHLSLMaterialTranslator
	{
		TMap<INT,FString> ParameterCodeMap[MP_MAX];
	public:
		FViewSourceMaterialTranslator(FMaterial* InMaterial,FUniformExpressionSet& InUniformExpressionSet,EShaderPlatform InPlatform)
		:	FHLSLMaterialTranslator(InMaterial,InUniformExpressionSet,InPlatform)
		{}

		// FHLSLMaterialTranslator interface
		virtual const TCHAR* GetParameterCode(INT Index)
		{
			// Tag the code section with the Index it came from.
			FString& TaggedCode = ParameterCodeMap[MaterialProperty].Set(Index, FString::Printf(TEXT("/*MARK_B%d*/%s/*MARK_E%d*/"), Index, FHLSLMaterialTranslator::GetParameterCode(Index), Index));
			return *TaggedCode;		
		}

		virtual FString GetFixedParameterCode(INT Index, EMaterialProperty InProperty)
		{
			// Tag the code section with the Index it came from.
			FString& TaggedCode = ParameterCodeMap[InProperty].Set(Index, FString::Printf(TEXT("/*MARK_B%d*/%s/*MARK_E%d*/"), Index, *FHLSLMaterialTranslator::GetFixedParameterCode(Index, InProperty), Index));
			return *TaggedCode;		
		}

		void GetExpressionCodeMap(TMap<UMaterialExpression*,INT>* OutExpressionCodeMap)
		{
			for (INT PropertyIndex = 0; PropertyIndex < MP_MAX; PropertyIndex++)
			{
				OutExpressionCodeMap[PropertyIndex] = ExpressionCodeMap[PropertyIndex];
			}
		}
	};

	FUniformExpressionSet TempSet;
	FViewSourceMaterialTranslator MaterialTranslator(this, TempSet, GRHIShaderPlatform);
	UBOOL bSuccess = MaterialTranslator.Translate();

	if( bSuccess )
	{
		// Generate the HLSL
		OutSource = MaterialTranslator.GetMaterialShaderCode();

		// Save the Expression Code map.
		MaterialTranslator.GetExpressionCodeMap(OutExpressionCodeMap);
	}
	return bSuccess;
#else
	appErrorf(TEXT("Not supported."));
	return FALSE;
#endif
}

/** Recompiles any materials in the EditorLoadedMaterialResources list if they are not complete. */
void FMaterial::UpdateEditorLoadedMaterialResources()
{
	for (TSet<FMaterial*>::TIterator It(EditorLoadedMaterialResources); It; ++It)
	{
		FMaterial* CurrentMaterial = *It;
		if (!CurrentMaterial->GetShaderMap() || !CurrentMaterial->GetShaderMap()->IsComplete(CurrentMaterial, TRUE))
		{
			CurrentMaterial->CacheShaders();
		}
	}
}
