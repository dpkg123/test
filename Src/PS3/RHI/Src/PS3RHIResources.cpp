/*=============================================================================
	PS3RHIResources.cpp: PS3 RHI resources implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3RHIPrivate.h"
#include "AllocatorFixedSizeFreeList.h"
#include "BestFitAllocator.h"

#if USE_PS3_RHI

/**
* Helper class that creates a key for a vertex declaration
*/
class FVertexDeclarationKey
{
public:
	/**
	* Constructor
	*/
	FVertexDeclarationKey(const FVertexDeclarationElementList& InElements)
	{
		FVertexDeclarationElementList VertexElements;
		// Zero the vertex element list, so we can CRC it without being affected by garbage padding
		appMemzero(&VertexElements(0), VertexElements.GetAllocatedSize());
		// Manually specified assignment operator for FVertexElement ensures that padding isn't copied
		VertexElements = InElements;
		// CRC the entire list
		Hash = appMemCrc(&VertexElements(0), sizeof(FVertexElement) * VertexElements.Num()); 
	}

	/**
	* @return TRUE if the decls are the same
	* @param Other - instance to compare against
	*/
	UBOOL operator == (const FVertexDeclarationKey &Other) const
	{
		return Hash == Other.Hash;
	}
	friend DWORD GetTypeHash(const FVertexDeclarationKey &Key)
	{
		return Key.Hash;
	}

private:
	/** Hash for map lookups */
	DWORD Hash;
};

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
void DEBUG_DumpFixedSizePoolAllocatedSizes()
{
	extern TAllocatorFixedSizeFreeList<sizeof(FBestFitAllocator::FMemoryChunk), 20> GMemoryChunkFixedSizePool;
	extern TAllocatorFixedSizeFreeList<sizeof(FMallocGcm::FFreeEntry), 20> GGcmFreeEntryFixedSizePool;
	extern TAllocatorFixedSizeFreeList<sizeof(FPS3RHIVertexDeclaration), 10> GFPS3RHIVertexDeclarationFixedSizePool;
	extern TAllocatorFixedSizeFreeList<sizeof(FPS3RHISamplerState), 10> GFPS3RHISamplerStateFixedSizePool;
	extern TAllocatorFixedSizeFreeList<sizeof(FPS3RHIVertexBuffer), 10> GFPS3RHIVertexBufferFixedSizePool;
	extern TAllocatorFixedSizeFreeList<sizeof(FPS3RHIIndexBuffer), 10> GFPS3RHIIndexBufferFixedSizePool;

	GLog->Logf(TEXT("FixedSizePool for FBestFitAllocator::FMemoryChunk=%d"),GMemoryChunkFixedSizePool.GetAllocatedSize());
	GLog->Logf(TEXT("FixedSizePool for FMallocGcm::FFreeEntry=%d"),GGcmFreeEntryFixedSizePool.GetAllocatedSize());
	GLog->Logf(TEXT("FixedSizePool for FPS3RHIVertexDeclaration=%d"),GFPS3RHIVertexDeclarationFixedSizePool.GetAllocatedSize());
	GLog->Logf(TEXT("FixedSizePool for FPS3RHISamplerState=%d"),GFPS3RHISamplerStateFixedSizePool.GetAllocatedSize());
	GLog->Logf(TEXT("FixedSizePool for FPS3RHIVertexBuffer=%d"),GFPS3RHIVertexBufferFixedSizePool.GetAllocatedSize());
	GLog->Logf(TEXT("FixedSizePool for FPS3RHIIndexBuffer=%d"),GFPS3RHIIndexBufferFixedSizePool.GetAllocatedSize());		

	DWORD TotalAllocated = 0;
	TotalAllocated += GMemoryChunkFixedSizePool.GetAllocatedSize();
	TotalAllocated += GGcmFreeEntryFixedSizePool.GetAllocatedSize();
	TotalAllocated += GFPS3RHIVertexDeclarationFixedSizePool.GetAllocatedSize();
	TotalAllocated += GFPS3RHISamplerStateFixedSizePool.GetAllocatedSize();
	TotalAllocated += GFPS3RHIVertexBufferFixedSizePool.GetAllocatedSize();
	TotalAllocated += GFPS3RHIIndexBufferFixedSizePool.GetAllocatedSize();		
	GLog->Logf(TEXT("FixedSizePool Total Allocated=%d"),TotalAllocated);
}
#endif

#if USE_ALLOCATORFIXEDSIZEFREELIST
TAllocatorFixedSizeFreeList<sizeof(FPS3RHIVertexDeclaration), 10> GFPS3RHIVertexDeclarationFixedSizePool(80);
void* FPS3RHIVertexDeclaration::operator new(size_t Size)
{
	checkSlow(Size == sizeof(FPS3RHIVertexDeclaration));
	return GFPS3RHIVertexDeclarationFixedSizePool.Allocate();
}
void FPS3RHIVertexDeclaration::operator delete(void *RawMemory)
{
	GFPS3RHIVertexDeclarationFixedSizePool.Free(RawMemory);
}
#endif

/**
* Maps a vertex declaration key to the resource
*/
class FVertexDeclarationCache
{
public:
	/**
	* Finds or creates a vertex declaration in the cache corresponding to DeclarationKey.
	*/
	FPS3RHIVertexDeclaration * GetVertexDeclaration(const FVertexDeclarationKey& DeclarationKey, const FVertexDeclarationElementList& Elements)
	{
		TPS3ResourceRef<FPS3RHIVertexDeclaration>* Value = VertexDeclarationMap.Find(DeclarationKey);
		if (!Value)
		{
			// Allocate a new vertex declaration if there wasn't a matching entry in the cache
			Value = &VertexDeclarationMap.Set(DeclarationKey, new FPS3RHIVertexDeclaration(Elements));
		}
		return *Value;
	}

private:
	/** Map from declaration key to resource */
	TMap<FVertexDeclarationKey, TPS3ResourceRef<FPS3RHIVertexDeclaration> > VertexDeclarationMap;
};

/** Global set of all vertex declarations */
FVertexDeclarationCache GVertexDeclarationCache;

FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	// Create the vertex declaration.
	FVertexDeclarationRHIRef VertexDeclaration;
	VertexDeclaration = GVertexDeclarationCache.GetVertexDeclaration(FVertexDeclarationKey(Elements), Elements);
	return VertexDeclaration;
}

FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements, FName DeclName)
{
	return RHICreateVertexDeclaration(Elements);
}

#if USE_ALLOCATORFIXEDSIZEFREELIST
TAllocatorFixedSizeFreeList<sizeof(FPS3RHISamplerState), 10> GFPS3RHISamplerStateFixedSizePool(1400);
void* FPS3RHISamplerState::operator new(size_t Size)
{
	checkSlow(Size == sizeof(FPS3RHISamplerState));
	return GFPS3RHISamplerStateFixedSizePool.Allocate();
}
void FPS3RHISamplerState::operator delete(void *RawMemory)
{
	GFPS3RHISamplerStateFixedSizePool.Free(RawMemory);
}
#endif

FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	FPS3RHISamplerState* SamplerState = new FPS3RHISamplerState;
	SamplerState->Anisotropic = (Initializer.Filter == SF_AnisotropicLinear || Initializer.Filter == SF_AnisotropicPoint) ? GPS3MaxAnisotropy : CELL_GCM_TEXTURE_MAX_ANISO_1;
	SamplerState->AddressU = TranslateUnrealAddressMode[Initializer.AddressU];
	SamplerState->AddressV = TranslateUnrealAddressMode[Initializer.AddressV];
	SamplerState->AddressW = CELL_GCM_TEXTURE_WRAP;
	SamplerState->MinFilter = TranslateUnrealMinFilterMode[Initializer.Filter];
	SamplerState->MagFilter	= TranslateUnrealMagFilterMode[Initializer.Filter];
	return SamplerState;
}

FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	FPS3RHIBlendState* BlendState = new FPS3RHIBlendState;
	BlendState->bAlphaBlendEnable = Initializer.ColorDestBlendFactor != BF_Zero || Initializer.ColorSourceBlendFactor != BF_One;
	BlendState->ColorBlendOperation = TranslateUnrealBlendOp[Initializer.ColorBlendOperation];
	BlendState->ColorSourceBlendFactor = TranslateUnrealBlendFactor[Initializer.ColorSourceBlendFactor];
	BlendState->ColorDestBlendFactor = TranslateUnrealBlendFactor[Initializer.ColorDestBlendFactor];
	BlendState->bSeparateAlphaBlendEnable = Initializer.AlphaDestBlendFactor != BF_Zero || Initializer.AlphaSourceBlendFactor != BF_One;
	BlendState->AlphaBlendOperation = TranslateUnrealBlendOp[Initializer.AlphaBlendOperation];
	BlendState->AlphaSourceBlendFactor = TranslateUnrealBlendFactor[Initializer.AlphaSourceBlendFactor];
	BlendState->AlphaDestBlendFactor = TranslateUnrealBlendFactor[Initializer.AlphaDestBlendFactor];
	BlendState->bAlphaTestEnable = Initializer.AlphaTest != CF_Always;
	BlendState->AlphaFunc = TranslateUnrealCompareFunction[Initializer.AlphaTest];
	BlendState->AlphaRef = Initializer.AlphaRef;
	return BlendState;
}

FDepthStateRHIRef RHICreateDepthState(const FDepthStateInitializerRHI& Initializer)
{
	FPS3RHIDepthState* DepthState = new FPS3RHIDepthState;
	DepthState->bEnableDepthTest = (Initializer.DepthTest != CF_Always) || Initializer.bEnableDepthWrite;
	DepthState->bEnableDepthWrite = Initializer.bEnableDepthWrite;
	DepthState->DepthTestFunc = TranslateUnrealCompareFunction[Initializer.DepthTest];
	return DepthState;
}

FStencilStateRHIRef RHICreateStencilState(const FStencilStateInitializerRHI& Initializer)
{
	FPS3RHIStencilState* StencilState = new FPS3RHIStencilState;
	StencilState->bStencilEnable = Initializer.bEnableFrontFaceStencil || Initializer.bEnableBackFaceStencil;
	StencilState->bTwoSidedStencilMode = Initializer.bEnableBackFaceStencil;

	StencilState->FrontFaceStencilFunc = TranslateUnrealCompareFunction[Initializer.FrontFaceStencilTest];
	StencilState->FrontFaceStencilFailStencilOp = TranslateUnrealStencilOp[Initializer.FrontFaceStencilFailStencilOp];
	StencilState->FrontFaceDepthFailStencilOp = TranslateUnrealStencilOp[Initializer.FrontFaceDepthFailStencilOp];
	StencilState->FrontFacePassStencilOp = TranslateUnrealStencilOp[Initializer.FrontFacePassStencilOp];

	StencilState->BackFaceStencilFunc = TranslateUnrealCompareFunction[Initializer.BackFaceStencilTest];
	StencilState->BackFaceStencilFailStencilOp = TranslateUnrealStencilOp[Initializer.BackFaceStencilFailStencilOp];
	StencilState->BackFaceDepthFailStencilOp = TranslateUnrealStencilOp[Initializer.BackFaceDepthFailStencilOp];
	StencilState->BackFacePassStencilOp = TranslateUnrealStencilOp[Initializer.BackFacePassStencilOp];

	StencilState->StencilReadMask = Initializer.StencilReadMask & 0xff;
	StencilState->StencilWriteMask = Initializer.StencilWriteMask & 0xff;
	StencilState->StencilRef = Initializer.StencilRef & 0xff;
	return StencilState;
}

FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	FPS3RHIRasterizerState* RasterizerState = new FPS3RHIRasterizerState;
	RasterizerState->bCullEnable	= Initializer.CullMode != CM_None;
	RasterizerState->FrontFace		= TranslateUnrealCullMode[Initializer.CullMode];
	RasterizerState->FillMode		= TranslateUnrealFillMode[Initializer.FillMode];
	RasterizerState->DepthBias		= Initializer.DepthBias;
	RasterizerState->SlopeScaleDepthBias = Initializer.SlopeScaleDepthBias;
	return RasterizerState;
}







/**
* Constructor
*/
FPS3RHIVertexBuffer::FPS3RHIVertexBuffer(DWORD InSize, DWORD UsageFlags)
: FPS3RHIDoubleBufferedGPUResource(InSize,
								   (UsageFlags & (RUF_Static|RUF_WriteOnly)) ? MR_Local : MR_Host,
								   AT_VertexBuffer,
								   (UsageFlags & RUF_AnyDynamic) ? TRUE : FALSE,
								   VERTEXBUFFER_ALIGNMENT)
{
}


/**
 * Constructor
 */
FPS3RHIVertexBuffer::FPS3RHIVertexBuffer(FResourceArrayInterface* ResourceArray)
: FPS3RHIDoubleBufferedGPUResource(ResourceArray)
{
}

#if USE_ALLOCATORFIXEDSIZEFREELIST
TAllocatorFixedSizeFreeList<sizeof(FPS3RHIVertexBuffer), 10> GFPS3RHIVertexBufferFixedSizePool(4200);
void* FPS3RHIVertexBuffer::operator new(size_t Size)
{
	checkSlow(Size == sizeof(FPS3RHIVertexBuffer));
	return GFPS3RHIVertexBufferFixedSizePool.Allocate();
}
void FPS3RHIVertexBuffer::operator delete(void *RawMemory)
{
	GFPS3RHIVertexBufferFixedSizePool.Free(RawMemory);
}
#endif

FVertexBufferRHIRef RHICreateVertexBuffer(UINT Size,FResourceArrayInterface* ResourceArray,DWORD InUsage)
{
	FPS3RHIVertexBuffer* VB;
	// if were given vertices, set them now
	if (ResourceArray)
	{
		// dynamic data shouldn't have the data set
		check(InUsage & RUF_Static);
		check(Size == ResourceArray->GetResourceDataSize());

		// create vertex buffer wrapper object around existing memory
		VB = new FPS3RHIVertexBuffer(ResourceArray);
	}
	else
	{
		// create a vertex buffer object, allocating memory
		VB = new FPS3RHIVertexBuffer(Size, InUsage);
	}

	return VB;
}

void* RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer,UINT Offset,UINT Size,UBOOL bReadOnlyInsteadOfWriteOnly)
{
	// make sure we are using a valid buffer
	VertexBuffer->SwapBuffers();

	return ((BYTE*)VertexBuffer->Data) + Offset;
}

void RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer)
{
}

/**
 * Checks if a vertex buffer is still in use by the GPU.
 * @param VertexBuffer - the RHI texture resource to check
 * @return TRUE if the texture is still in use by the GPU, otherwise FALSE
 */
UBOOL RHIIsBusyVertexBuffer(FVertexBufferRHIParamRef VertexBuffer)
{
	//@TODO: Implement GPU resource fence!
	return FALSE;
}

FTexture2DRHIRef RHICreateStereoFixTexture()
{
	return NULL;
}

void RHIUpdateStereoFixTexture(FTexture2DRHIParamRef TextureRHI)
{
}

/**
 * Constructor
 */
FPS3RHIIndexBuffer::FPS3RHIIndexBuffer(DWORD InSize, DWORD InStride, DWORD UsageFlags,DWORD InNumInstances)
: FPS3RHIDoubleBufferedGPUResource(InSize,
								   (UsageFlags & (RUF_Static|RUF_WriteOnly)) ? MR_Local : MR_Host,
								   AT_IndexBuffer,
								   (UsageFlags & RUF_AnyDynamic) ? TRUE : FALSE,
								   INDEXBUFFER_ALIGNMENT)
, NumInstances(InNumInstances)
, DataType(InStride == 2 ? CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16 : CELL_GCM_DRAW_INDEX_ARRAY_TYPE_32)
{
}

/**
* Constructor for resource array (in place data)
*/
FPS3RHIIndexBuffer::FPS3RHIIndexBuffer(FResourceArrayInterface* ResourceArray, DWORD InStride,DWORD InNumInstances)
: FPS3RHIDoubleBufferedGPUResource(ResourceArray)
, NumInstances(InNumInstances)
, DataType(InStride == 2 ? CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16 : CELL_GCM_DRAW_INDEX_ARRAY_TYPE_32)
{
}

#if USE_ALLOCATORFIXEDSIZEFREELIST
TAllocatorFixedSizeFreeList<sizeof(FPS3RHIIndexBuffer), 10> GFPS3RHIIndexBufferFixedSizePool(900);
void* FPS3RHIIndexBuffer::operator new(size_t Size)
{
	checkSlow(Size == sizeof(FPS3RHIIndexBuffer));
	return GFPS3RHIIndexBufferFixedSizePool.Allocate();
}
void FPS3RHIIndexBuffer::operator delete(void *RawMemory)
{
	GFPS3RHIIndexBufferFixedSizePool.Free(RawMemory);
}
#endif

FIndexBufferRHIRef RHICreateIndexBuffer(UINT Stride,UINT Size,FResourceArrayInterface* ResourceArray,DWORD InUsage)
{
	// create an index vertex buffer object, allocating memory
	FPS3RHIIndexBuffer* IB;

	// if were given vertices, set them now
	if (ResourceArray)
	{
		// dynamic data shouldn't have the data set
		check(InUsage & RUF_Static);
		check(Size == ResourceArray->GetResourceDataSize());

		IB = new FPS3RHIIndexBuffer(ResourceArray, Stride, 1);
	}
	else
	{
		IB = new FPS3RHIIndexBuffer(Size, Stride, InUsage, 1);
	}

	return IB;
}

FIndexBufferRHIRef RHICreateInstancedIndexBuffer(UINT Stride,UINT Size,DWORD InUsage,UINT& OutNumInstances)
{
	// If the index buffer is smaller than 2KB, replicate it to allow drawing multiple instances with one draw command.
	const UINT MinInstancingIndexBufferSize = 2048;
	OutNumInstances = 1;
	if (Size < MinInstancingIndexBufferSize)
	{
		OutNumInstances = (MinInstancingIndexBufferSize * 2) / Size;
		Size *= OutNumInstances;
	}

#if RHI_SUPPORTS_PREVERTEXSHADER_CULLING
	// We strip off RUF_Static to make sure the index buffer goes into host memory for 
	// Edge to be able to read it fast quickly
	InUsage &= ~RUF_Static;
#endif

	return new FPS3RHIIndexBuffer(Size, Stride, InUsage, OutNumInstances);
}

void* RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer,UINT Offset,UINT Size)
{
	// make sure we are using a valid buffer
	IndexBuffer->SwapBuffers();

	return ((BYTE*)IndexBuffer->Data) + Offset;
}

void RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer)
{
}


/**
 * Get the hash for this type
 * This defines a function declared in UnTemplate.h (see that file for explanation of its need)
 * @param Key - struct to hash
 * @return dword hash based on type
 */
DWORD GetTypeHash(const FBoundShaderStateRHIRef &Key)
{
	return PointerHash(
		(FPS3RHIVertexDeclaration *)Key.VertexDeclaration, 
		PointerHash((FPS3RHIVertexShader *)Key.VertexShader, 
		PointerHash((FPS3RHIPixelShader *)Key.PixelShader))
		);
}

#endif // USE_PS3_RHI
