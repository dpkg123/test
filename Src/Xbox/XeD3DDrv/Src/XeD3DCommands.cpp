/*=============================================================================
	D3DCommands.cpp: D3D RHI commands implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "XeD3DDrvPrivate.h"

#if USE_XeD3D_RHI
#include "ChartCreation.h"

// Globals

/** TRUE when inside a [Begin/End]Vertices block */
static UBOOL GInBeginVertices = FALSE;

// This is the current render target Y offset, it needs to be 0 for View0
UINT GCurrentRenderTargetYOffset = 0;

extern UBOOL GD3DRenderingIsSuspended; 

extern INT XeGetRenderTargetColorExpBias(D3DFORMAT SurfaceD3DFormat, D3DFORMAT TextureD3DFormat);

/** TRUE if currently rendering to the scene color render target */
UBOOL GCurrentUsingSceneColorTarget = FALSE;

// Offset the base addresses for RT0, Depth, and HiZ to match with 
// an offset viewport based on the ViewportY and ResolveOffset
void OffsetRenderTargetBase(UINT ViewportSizeX, UINT ViewportY, UINT ResolveOffset)
{
	INT YOffset = 0 - GCurrentRenderTargetYOffset;
	D3DMULTISAMPLE_TYPE MultiSample = D3DMULTISAMPLE_NONE;

	if(ViewportY != 0)
	{
		if(GDirect3DDevice->m_DestinationPacket.SurfaceInfo.MsaaSamples == GPUSAMPLES_2X)
		{
			// for the MSAA case, the base address remains at 0
			MultiSample = D3DMULTISAMPLE_2_SAMPLES;
		}
		else
		{
			// for the non-MSAA case the base address has to be offset 
			// to draw to the appropriate bottom view
			YOffset = ViewportY - ResolveOffset - GCurrentRenderTargetYOffset;
		}
	}

	// only apply an offset if it is different than the current applied offset
	if( YOffset != 0 )
	{
		// get the tile sizes based on a positive YOffset
		INT DepthOffset	= (INT)XGSurfaceSize( ViewportSizeX, Abs(YOffset), D3DFMT_D24S8, MultiSample );
		INT ColorOffset	= (INT)XGSurfaceSize( ViewportSizeX, Abs(YOffset), D3DFMT_A8R8G8B8, MultiSample );
		INT HiZOffset = (INT)XGHierarchicalZSize( ViewportSizeX, Abs(YOffset), MultiSample );
		// lock the GPU registers
		GDirect3DDevice->GpuOwn(D3DTAG_DESTINATIONPACKET);
		GDirect3DDevice->GpuOwn(D3DTAG_HICONTROL);
		if( YOffset > 0 )
		{
			// increment GPU registers
			GDirect3DDevice->m_DestinationPacket.DepthInfo.DepthBase += DepthOffset;
			GDirect3DDevice->m_DestinationPacket.Color0Info.ColorBase += ColorOffset;
			GDirect3DDevice->m_ControlPacket.HiControl.HiBaseAddr += HiZOffset;
		}
		else
		{
			// decrement GPU registers
			GDirect3DDevice->m_DestinationPacket.DepthInfo.DepthBase = (UINT)Max<INT>(0,(INT)GDirect3DDevice->m_DestinationPacket.DepthInfo.DepthBase - (INT)DepthOffset);
			GDirect3DDevice->m_DestinationPacket.Color0Info.ColorBase = (UINT)Max<INT>(0,(INT)GDirect3DDevice->m_DestinationPacket.Color0Info.ColorBase - (INT)ColorOffset);
			GDirect3DDevice->m_ControlPacket.HiControl.HiBaseAddr = (UINT)Max<INT>(0,(INT)GDirect3DDevice->m_ControlPacket.HiControl.HiBaseAddr - (INT)HiZOffset);
		}
		// unlock GPU registers
		GDirect3DDevice->GpuDisown(D3DTAG_HICONTROL);
		GDirect3DDevice->GpuDisown(D3DTAG_DESTINATIONPACKET);
		// update global stored offset
		GCurrentRenderTargetYOffset += YOffset;
	}
}

/*-----------------------------------------------------------------------------
	Vertex state.
-----------------------------------------------------------------------------*/

struct FXBoxStream
{
	FXBoxStream()
		:	Stride(0)
	{
	}
	FVertexBufferRHIParamRef VertexBuffer;
	UINT	Stride;
	UINT	NumVerticesPerInstance;
};


#define MAX_VERTEX_STREAMS 16


UINT						GPendingNumInstances = 1;
FXBoxStream					GPendingStreams[MAX_VERTEX_STREAMS];
UINT						GUpdateStreamForInstancingMask = 0;

void RHISetStreamSource(UINT StreamIndex,FVertexBufferRHIParamRef VertexBuffer,UINT Stride,UBOOL bUseInstanceIndex,UINT NumVerticesPerInstance,UINT NumInstances)
{
	if ( NumInstances > 1 )
	{
		GPendingNumInstances = NumInstances;
		if ( bUseInstanceIndex )
		{
			// We can't actually set the stream at this point because we don't know if the shader has been hacked to 
			// support instancing or not. Thus we can't know if we need a stride of zero and fake instancing
			// or if we need a real stride.
			GPendingStreams[StreamIndex].VertexBuffer = VertexBuffer;
			GPendingStreams[StreamIndex].Stride = Stride;
			GPendingStreams[StreamIndex].NumVerticesPerInstance = NumVerticesPerInstance;
			GUpdateStreamForInstancingMask |= 1<<StreamIndex;
		}
	}
	else
	{
		bUseInstanceIndex = FALSE;

		// Were we using instancing before?
		if ( GPendingNumInstances > 1 )
		{
			// Reset the instanced vertex state.
			// clear data so that it doesn't cause future disruption
			UINT InstancingMask = GUpdateStreamForInstancingMask;
			for (UINT StreamIndex = 0; StreamIndex < MAX_VERTEX_STREAMS && InstancingMask; StreamIndex++)
			{
				if (InstancingMask & 1)
				{
					GPendingStreams[StreamIndex].VertexBuffer = 0;
				}
				InstancingMask >>= 1;
			}
			GUpdateStreamForInstancingMask = 0;
			GPendingNumInstances = 1;
		}
	}

	if ( bUseInstanceIndex == FALSE )
	{
		IDirect3DVertexBuffer9* D3DVertexBufferPtr = (IDirect3DVertexBuffer9*) VertexBuffer->Resource;
		GDirect3DDevice->SetStreamSource(StreamIndex,D3DVertexBufferPtr,0,Stride);
	}
}

void RHISetViewport(UINT MinX,UINT MinY,FLOAT MinZ,UINT MaxX,UINT MaxY,FLOAT MaxZ)
{
	D3DVIEWPORT9 CurrentViewport;
	GDirect3DDevice->GetViewport(&CurrentViewport);

	if (GInvertZ)
	{
		// Reverse the order of the depth values if the current active viewport is reversed
		MinZ = 1.0f - MinZ;
		MaxZ = 1.0f - MaxZ;
	}

	// for non-tiled split screen, we have to make sure View1's viewport
	// matches for both the MSAA and non-MSAA passes
	// this requires moving the base addresses for RT0, Depth, and HiZ
	if( GUseMSAASplitScreen && GCurrentUsingSceneColorTarget )
	{
		UINT ViewportY = MinY;
		UINT ResolveOffset = MinY % 32;
		UINT SizeX = MaxX - MinX;

		if(ResolveOffset != 0)
		{
			MaxY = ViewportY + ResolveOffset;
			MinY = ResolveOffset;
		}
		OffsetRenderTargetBase(SizeX, ViewportY, ResolveOffset);
	}
 
	D3DVIEWPORT9 Viewport = { MinX, MinY, MaxX - MinX, MaxY - MinY, MinZ, MaxZ };
	GDirect3DDevice->SetViewport(&Viewport);
}
void RHISetScissorRect(UBOOL bEnable,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY)
{
	// Defined in UnPlayer.cpp. Used here to disable scissors when doing highres screenshots.
	extern UBOOL GIsTiledScreenshot;
	bEnable = GIsTiledScreenshot ? FALSE : bEnable;

	if(bEnable)
	{
		// for non-tiled split screen, we have to make sure View1's scissor rect region
		// takes into account the viewport offsets for the MSAA and non-MSAA passes
		if( GUseMSAASplitScreen && 
			GCurrentUsingSceneColorTarget &&
			// detect if rendering scissor in bottom view region
			(INT)MinY >= (GScreenHeight/2) )
		{
			const UINT OffsetY = (GScreenHeight/2) - ((GScreenHeight/2) % 32);
			MinY -= OffsetY;
			MaxY -= OffsetY;
		}
	
		RECT ScissorRect;
		ScissorRect.left = MinX;
		ScissorRect.right = MaxX;
		ScissorRect.top = MinY;
		ScissorRect.bottom = MaxY;
		GDirect3DDevice->SetScissorRect(&ScissorRect);
	}
	GDirect3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE,bEnable);
}

/**
 * Set depth bounds test state.  
 * When enabled, incoming fragments are killed if the value already in the depth buffer is outside of [ClipSpaceNearPos, ClipSpaceFarPos]
 */
void RHISetDepthBoundsTest(UBOOL bEnable,const FVector4 &ClipSpaceNearPos,const FVector4 &ClipSpaceFarPos)
{
	//@todo: implement with user clip planes like D3D
}


/*-----------------------------------------------------------------------------
	Shader state.
-----------------------------------------------------------------------------*/

void RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderState)
{
	UBOOL bIsDoingInstancing = GPendingNumInstances > 1 && GUpdateStreamForInstancingMask;
	if ( bIsDoingInstancing == FALSE )
	{
		// No need to set vertex declaration when using bound shaders
		check(0 < BoundShaderState.BoundVertexShader->CacheRefCount);
		GDirect3DDevice->SetVertexDeclaration(NULL);
		GDirect3DDevice->SetVertexShader(BoundShaderState.BoundVertexShader->VertexShader);
		GDirect3DDevice->SetPixelShader(BoundShaderState.PixelShader);			
	}
	else
	{
		GDirect3DDevice->SetVertexDeclaration(BoundShaderState.VertexDeclaration);
		GDirect3DDevice->SetVertexShader(BoundShaderState.OriginalVertexShader);
		GDirect3DDevice->SetPixelShader(BoundShaderState.PixelShader);
	}
}

void RHISetSamplerStateOnly(FPixelShaderRHIParamRef PixelShader,UINT /*SamplerIndex*/,FSamplerStateRHIParamRef NewState)
{
	// Not yet implemented
	check(0);
}

void RHISetTextureParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	// Not yet implemented
	check(0);
}

void RHISetSurfaceParameter(FPixelShaderRHIParamRef PixelShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewTextureRHI)
{
	// Not yet implemented
	check(0);
}

// Current sampler states, tracked to minimize sampler state setting as an optimization
// One for each pixel shader sampler
FShadowedSamplerState ShadowedSamplerStates[16];

/**
 * Sets sampler state.
 *
 * @param PixelShader	The pixelshader using the sampler for the next drawcalls.
 * @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
 * @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
 * @param MipBias		Mip bias to use for the texture
 * @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
 * @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
 */
void RHISetSamplerState(FPixelShaderRHIParamRef PixelShader,UINT TextureIndex,UINT /*SamplerIndex*/,FSamplerStateRHIParamRef InNewState,FTextureRHIParamRef NewTexture,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip,UBOOL bForceLinearMinFilter)
{
	// Only tracking pixel shader sampler state
	const UBOOL bShadowedSampler = TextureIndex < 16;

	const FD3DSamplerState* RESTRICT NewState = InNewState;

	CONSOLE_PREFETCH(NewState);
	CONSOLE_PREFETCH(NewTexture);

	if (bShadowedSampler)
	{
		FShadowedSamplerState* RESTRICT ShadowedSamplerState = &ShadowedSamplerStates[TextureIndex];
		const FD3DSamplerState* RESTRICT SamplerState = ShadowedSamplerState->SamplerState;

		if (LargestMip != ShadowedSamplerState->LargestMip)
		{
			GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MAXMIPLEVEL, (LargestMip < 0.0f) ? 0 : appTrunc(LargestMip) );
			ShadowedSamplerState->LargestMip = LargestMip;
		}

		if (SmallestMip != ShadowedSamplerState->SmallestMip)
		{
			GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MINMIPLEVEL, (SmallestMip < 0.0f) ? 13 : appTrunc(SmallestMip) );
			ShadowedSamplerState->SmallestMip = SmallestMip;
		}

		if (SamplerState != NewState)
		{
			if (!SamplerState)
			{
				GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MAGFILTER,NewState->MagFilter);
				GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MINFILTER,NewState->MinFilter);
				GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSU,NewState->AddressU);
				GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSV,NewState->AddressV);
				GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSW,NewState->AddressW);
			}
			else
			{
				if (NewState->MagFilter != SamplerState->MagFilter)
				{
					GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MAGFILTER,NewState->MagFilter);
				}
				if (NewState->MinFilter != SamplerState->MinFilter || bForceLinearMinFilter != ShadowedSamplerState->bForceLinearMinFilter)
				{
					GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MINFILTER,bForceLinearMinFilter ? D3DTEXF_LINEAR : NewState->MinFilter);
				}
				if (NewState->AddressU != SamplerState->AddressU)
				{
					GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSU,NewState->AddressU);
				}
				if (NewState->AddressV != SamplerState->AddressV)
				{
					GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSV,NewState->AddressV);
				}
				if (NewState->AddressW != SamplerState->AddressW)
				{
					GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSW,NewState->AddressW);
				}
			}
			ShadowedSamplerState->SamplerState = NewState;
			ShadowedSamplerState->bForceLinearMinFilter = bForceLinearMinFilter;
		}

		// this is a little tricky
		// SamplerState->MipFilter caches whatever was passed in last time
		// ShadowedSamplerState->MipFilter caches the results of the bias comparison
		if (MipBias != ShadowedSamplerState->MipBias || !SamplerState || NewState->MipFilter != SamplerState->MipFilter)
		{
			if (MipBias != ShadowedSamplerState->MipBias)
			{
				GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MIPMAPLODBIAS,FLOAT_TO_DWORD(MipBias));
			}
			const DWORD MipFilter = appIsNearlyEqual(appTruncFloat(MipBias), MipBias) ? NewState->MipFilter : D3DTEXF_LINEAR;
			if (MipFilter != ShadowedSamplerState->MipFilter)
			{
				GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MIPFILTER,MipFilter);
			}
			ShadowedSamplerState->MipFilter = MipFilter;
			ShadowedSamplerState->MipBias = MipBias;
		}
#if _DEBUG
		IDirect3DBaseTexture9* CurrentTexture = NULL;
		GDirect3DDevice->GetTexture(TextureIndex, &CurrentTexture);
		// Verify that the shadowed texture state matches what is currently set on the device
		// Code calling GDirect3DDevice->SetTexture directly needs to update ShadowedSamplerStates.
		//@todo - verify shadowed state set by GDirect3DDevice->SetSamplerState too
		checkSlow(ShadowedSamplerStates[TextureIndex].Texture == CurrentTexture);
		if (CurrentTexture)
		{
			CurrentTexture->Release();
		}
#endif
		IDirect3DBaseTexture9* D3DTexture = (IDirect3DBaseTexture9*)NewTexture->Resource;
		if (D3DTexture != ShadowedSamplerState->Texture)
		{
			GDirect3DDevice->SetTexture(TextureIndex,D3DTexture);
			ShadowedSamplerState->Texture = D3DTexture;
		}
	}
	else
	{
		IDirect3DBaseTexture9* D3DTexture = (IDirect3DBaseTexture9*)NewTexture->Resource;
		// Force linear mip-filter if MipBias has a fractional part.
		const DWORD MipFilter = appIsNearlyEqual(appTruncFloat(MipBias), MipBias) ? NewState->MipFilter : D3DTEXF_LINEAR;

		GDirect3DDevice->SetTexture(TextureIndex,D3DTexture);
		GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MAGFILTER,NewState->MagFilter);
		GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MINFILTER,NewState->MinFilter);
		GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSU,NewState->AddressU);
		GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSV,NewState->AddressV);
		GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_ADDRESSW,NewState->AddressW);
		GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MIPFILTER,MipFilter);
		GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MIPMAPLODBIAS,FLOAT_TO_DWORD(MipBias));
		GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MAXMIPLEVEL, (LargestMip < 0.0f) ? 0 : appTrunc(LargestMip) );
		GDirect3DDevice->SetSamplerState(TextureIndex,D3DSAMP_MINMIPLEVEL, (SmallestMip < 0.0f) ? 13 : appTrunc(SmallestMip) );
	}
}

void RHISetSamplerState(FVertexShaderRHIParamRef VertexShader,UINT TextureIndex,UINT /*SamplerIndex*/,FSamplerStateRHIParamRef InNewState,FTextureRHIParamRef NewTexture,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip,UBOOL bForceLinearMinFilter)
{
	// Not yet implemented
	check(0);
}

/** Sets all pixel shader textures to NULL. */
void UnsetPSTextures()
{
	// Reset all texture references, to ensure a reference to this render target doesn't remain set.
	for(UINT TextureIndex = 0;TextureIndex < 16;TextureIndex++)
	{
		if (ShadowedSamplerStates[TextureIndex].Texture)
		{
			GDirect3DDevice->SetTexture(TextureIndex, NULL);
			// Have to update the shadowed texture sampler state manually since we are calling GDirect3DDevice->SetTexture directly
			ShadowedSamplerStates[TextureIndex].Texture = NULL;
		}
		// these can go stale and the page can become unmapped, null them at the end of the frame to avoid that.
		ShadowedSamplerStates[TextureIndex].SamplerState = NULL;
	}
}

/** Sets all vertex shader textures to NULL. */
void UnsetVSTextures()
{
	for(UINT TextureIndex = D3DVERTEXTEXTURESAMPLER0; TextureIndex <= D3DVERTEXTEXTURESAMPLER3; TextureIndex++)
	{
		GDirect3DDevice->SetTexture(TextureIndex, NULL);
	}
}

/**
* Sets vertex sampler state.
*
* @param SamplerIndex	Vertex texture sampler index.
* @param Texture		Texture to set
*/
void RHISetVertexTexture(UINT SamplerIndex,FTextureRHIParamRef NewTextureRHI)
{
	IDirect3DBaseTexture9* D3DTexture = (IDirect3DBaseTexture9*) NewTextureRHI->Resource;
	// On Xbox360 the Sampler Index already includes the D3DVERTEXTEXTURESAMPLER0 offset.
	GDirect3DDevice->SetTexture(SamplerIndex,D3DTexture);
	GDirect3DDevice->SetSamplerState(SamplerIndex, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	GDirect3DDevice->SetSamplerState(SamplerIndex, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	GDirect3DDevice->SetSamplerState(SamplerIndex, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
}


void RHISetMobileTextureSamplerState( FPixelShaderRHIParamRef PixelShader, const INT MobileTextureUnit, FSamplerStateRHIParamRef NewState, FTextureRHIParamRef NewTextureRHI, FLOAT MipBias, FLOAT LargestMip, FLOAT SmallestMip ) 
{
	// Only used on mobile platforms
} 


void RHISetMobileSimpleParams(EBlendMode InBlendMode)
{
	// Only used on mobile platforms
}

void RHISetMobileMaterialVertexParams(const FMobileMaterialVertexParams& InVertexParams)
{
	// Only used on mobile platforms
}

void RHISetMobileMaterialPixelParams(const FMobileMaterialPixelParams& InPixelParams)
{
	// Only used on mobile platforms
}


void RHISetMobileMeshVertexParams(const FMobileMeshVertexParams& InMeshParams)
{
	// Only used on mobile platforms
}


FLOAT RHIGetMobilePercentColorFade(void)
{
	// Only used on mobile platforms
	return 0.0f;
}


void RHISetMobileFogParams (const UBOOL bInEnabled, const FLOAT InFogStart, const FLOAT InFogEnd, const FColor& InFogColor)
{
	// Only used on mobile platforms
}

void RHISetMobileBumpOffsetParams(const UBOOL bInEnabled, const FLOAT InBumpEnd)
{
	// Only used on mobile platforms
}

void RHISetMobileTextureTransformOverride(TMatrix<3,3>& InOverrideTransform)
{
	// Only used on mobile platforms
}

void RHIResetTrackedPrimitive()
{
	// Not implemented yet
}

void RHIIncrementTrackedPrimitive(const INT InDelta)
{
	// Not implemented yet
}


void RHIClearSamplerBias()
{
	// Not used
}

static const UINT GXeNumBytesPerShaderRegister = sizeof(FLOAT) * 4;

void RHISetShaderParameter(FVertexShaderRHIParamRef VertexShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	RHISetVertexShaderParameter(VertexShader, BufferIndex, BaseIndex, NumBytes, NewValue, ParamIndex);
}

void RHISetVertexShaderParameter(FVertexShaderRHIParamRef VertexShader,UINT /*BufferIndex*/,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT /*ParamIndex*/)
{
	GDirect3DDevice->SetVertexShaderConstantF(
		BaseIndex / GXeNumBytesPerShaderRegister,
		(FLOAT*)GetPaddedShaderParameterValue(NewValue,NumBytes),
		(NumBytes + GXeNumBytesPerShaderRegister - 1) / GXeNumBytesPerShaderRegister
		);
}
/*
 * *** PERFORMANCE WARNING *****
 * This code is from Mikey W @ Xbox 
 * This function is to support single float array parameter in shader
 * This pads 3 more floats for each element - at the expensive of CPU/stack memory
 * Do not overuse this. If you can, use float4 in shader. 
 */
void RHISetVertexShaderFloatArray(FVertexShaderRHIParamRef VertexShader,UINT /*BufferIndex*/,UINT BaseIndex,UINT NumValues,const FLOAT* FloatValues, INT /*ParamIndex*/)
{
	// On D3D, this function takes an array of floats and pads them out to an array of vector4's before setting them as
	// vertex shader constants. This is the proper thing to do for Xenon, although please note the pros and cons
	// when using float arrays. The first con is that there's alot of wasted vertex shader constants that could
	// otherwise be saved if the float array was compressed. However, that would require a fair number of shader
	// instructions to decompress, so the overwhleming pro is that the accessing a float array in the shader is
	// trivial. Another con to be aware of is that the use of more shader constants contributes to "constant
	// waterfalling"...a potential performance problem that can not be predicted, but rather must be measured.

	// Temp storage space for the vector4 padded data. (Note: We might want to make this dynamic, but using
	// the stack is cheap and convenient.)
	const UINT GroupSize = 64;
	XMFLOAT4A pPaddedVectorValues[GroupSize];

	// Process a large set of shader constants in groups of a fixed size
	while( NumValues )
	{
		// Number of values to process this time through the loop
		UINT NumValuesThisGroup = Min( NumValues, GroupSize );
		UINT NumValuesThisGroupDiv4 = (NumValuesThisGroup+4-1)/4;

		// For performance, we use the XnaMath intrinsics. Furthermore, we prefer to use XMLoadFloat4A,
		// if we can guarantee pFloatValues is 16-byte aligned
		BOOL bAligned = (((UINT)FloatValues)%16)==0 ? TRUE : FALSE;
		if( bAligned )
		{
			XMFLOAT4A* pSrc  = (XMFLOAT4A*)FloatValues;  // pFloatValues is 16-byte aligned
			XMFLOAT4A* pDest = pPaddedVectorValues;

			// Load FLOAT values 4 at a time and store them out as padded FLOAT4 values
			for( UINT i=0; i<NumValuesThisGroupDiv4; i++ )
			{
				XMVECTOR V = XMLoadFloat4A( pSrc++ ); // pFloatValues is 16-byte aligned
				XMVectorGetXPtr( (FLOAT*)pDest++, V ); // Write V.x to first component of pDest
				XMVectorGetYPtr( (FLOAT*)pDest++, V ); // Write V.y to first component of pDest
				XMVectorGetZPtr( (FLOAT*)pDest++, V ); // Write V.w to first component of pDest
				XMVectorGetWPtr( (FLOAT*)pDest++, V ); // Write V.z to first component of pDest
			}
		}
		else
		{
			XMFLOAT4* pSrc  = (XMFLOAT4A*)FloatValues;  // pFloatValues is not aligned
			XMFLOAT4* pDest = pPaddedVectorValues;

			// Load FLOAT values 4 at a time and store them out as padded FLOAT4 values
			for( UINT i=0; i<NumValuesThisGroupDiv4-1; i++ )
			{
				XMVECTOR V = XMLoadFloat4( pSrc++ ); // pFloatValues is not aligned
				XMVectorGetXPtr( (FLOAT*)pDest++, V ); // Write V.x to first component of pDest
				XMVectorGetYPtr( (FLOAT*)pDest++, V ); // Write V.y to first component of pDest
				XMVectorGetZPtr( (FLOAT*)pDest++, V ); // Write V.w to first component of pDest
				XMVectorGetWPtr( (FLOAT*)pDest++, V ); // Write V.z to first component of pDest
			}

			// Random crash happened when I read pSrc
			// Only guess I have is overflow on reading - when not aligned
			// So I'm carefully reading the last values at the end. 
			// Get remainder, and do not overflow 
			UINT Remainder = NumValuesThisGroup-(NumValuesThisGroupDiv4-1)*4;

			if (Remainder > 0)
			{
				pDest->x = pSrc->x;
					
				if (Remainder > 1)
				{
					pDest++;
					pDest->x = pSrc->y;

					if (Remainder > 2)
					{
						pDest++;
						pDest->x = pSrc->z;

						if (Remainder > 3)
						{
							pDest++;
							pDest->x = pSrc->w;
						}
					}
				}
			}
		}

		// Set the newly padded vertex shader constants to the D3DDevice
		GDirect3DDevice->SetVertexShaderConstantF( BaseIndex / GXeNumBytesPerShaderRegister, (FLOAT*)pPaddedVectorValues, NumValuesThisGroup );
		// In case we need to continue processing more values, advance to the next group of values
		NumValues	-= NumValuesThisGroup;
		FloatValues	+= NumValuesThisGroup;
		BaseIndex   += NumValuesThisGroup*GXeNumBytesPerShaderRegister;
	}
}

void RHISetVertexShaderBoolParameter(FVertexShaderRHIParamRef VertexShader,UINT /*BufferIndex*/,UINT BaseIndex,UBOOL NewValue)
{
	GDirect3DDevice->SetVertexShaderConstantB(
		BaseIndex / GXeNumBytesPerShaderRegister,
		(BOOL*)&NewValue,
		1
		);
}

void RHISetShaderParameter(FPixelShaderRHIParamRef PixelShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{
	RHISetPixelShaderParameter(PixelShader, BufferIndex, BaseIndex, NumBytes, NewValue, ParamIndex);
}

void RHISetPixelShaderParameter(FPixelShaderRHIParamRef PixelShader,UINT /*BufferIndex*/,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT /*ParamIndex*/)
{
	GDirect3DDevice->SetPixelShaderConstantF(
		BaseIndex / GXeNumBytesPerShaderRegister,
		(FLOAT*)GetPaddedShaderParameterValue(NewValue,NumBytes),
		(NumBytes + GXeNumBytesPerShaderRegister - 1) / GXeNumBytesPerShaderRegister
		);
}

void RHISetPixelShaderBoolParameter(FPixelShaderRHIParamRef PixelShader,UINT /*BufferIndex*/,UINT BaseIndex,UBOOL NewValue)
{
	GDirect3DDevice->SetPixelShaderConstantB(
		BaseIndex / GXeNumBytesPerShaderRegister,
		(BOOL*)&NewValue,
		1
		);
}

void RHISetRenderTargetBias( FLOAT ColorBias )
{
	FVector4 ColorBiasParameter( ColorBias, 0, 0, 0 );
	GDirect3DDevice->SetPixelShaderConstantF(PSR_ColorBiasFactor,(FLOAT*)&ColorBiasParameter,1);
}

/**
 * Set engine shader parameters for the view.
 * @param View					The current view
 */
void RHISetViewParameters(const FSceneView& View)
{
	RHISetViewParametersWithOverrides(View, View.TranslatedViewProjectionMatrix, View.DiffuseOverrideParameter, View.SpecularOverrideParameter);
}

/**
 * Set engine shader parameters for the view.
 * @param View					The current view
 * @param ViewProjectionMatrix	Matrix that transforms from world space to projection space for the view
 * @param DiffuseOverride		Material diffuse input override
 * @param SpecularOverride		Material specular input override
 */
void RHISetViewParametersWithOverrides( const FSceneView& View, const FMatrix& ViewProjectionMatrix, const FVector4& DiffuseOverride, const FVector4& SpecularOverride )
{
	const FVector4 TranslatedViewOrigin = View.ViewOrigin + FVector4(View.PreViewTranslation,0);
	const FVector4 PreViewTranslation = View.PreViewTranslation;
	const FVector4 TemporalAAParameters = View.TemporalAAParameters.GetVector();

	GDirect3DDevice->SetVertexShaderConstantF( VSR_ViewProjMatrix, (const FLOAT*) &ViewProjectionMatrix, 4 );
	GDirect3DDevice->SetVertexShaderConstantF( VSR_ViewOrigin, (const FLOAT*) &TranslatedViewOrigin, 1 );
	GDirect3DDevice->SetVertexShaderConstantF( VSR_PreViewTranslation, (const FLOAT*) &PreViewTranslation, 1 );
	GDirect3DDevice->SetVertexShaderConstantF( VSR_TemporalAAParameters, (const FLOAT*) &TemporalAAParameters, 1 );
	GDirect3DDevice->SetPixelShaderConstantF( PSR_MinZ_MaxZ_Ratio, (const FLOAT*) &View.InvDeviceZToWorldZTransform, 1 );
	GDirect3DDevice->SetPixelShaderConstantF( PSR_ScreenPositionScaleBias, (const FLOAT*) &View.ScreenPositionScaleBias, 1 );
	GDirect3DDevice->SetPixelShaderConstantF( PSR_ViewOrigin, (const FLOAT*) &View.ViewOrigin, 1 );
}

/**
 * Control the GPR (General Purpose Register) allocation to help with hiding memory latency for certain shaders.
 * These must add up to 128 total GPRs.  There is some CPU/GPU overhead to calling this function.
 * See the SetShaderGPRAllocation XGK docs for more limitations.  
 * WARNING: Using a shader that needs more GPRs than are assigned to that part of the pipeline will cause a GPU hang.
 * @param NumVertexShaderRegisters - num of GPRs to allocate for the vertex shader (default is 64)
 * @param NumPixelShaderRegisters - num of GPRs to allocate for the pixel shader (default is 64)
 */
void RHISetShaderRegisterAllocation(UINT NumVertexShaderRegisters, UINT NumPixelShaderRegisters)
{
	check((NumVertexShaderRegisters + NumPixelShaderRegisters) == 128);
	check(NumVertexShaderRegisters >= 16 && NumPixelShaderRegisters >= 16);
	GDirect3DDevice->SetShaderGPRAllocation(0, NumVertexShaderRegisters, NumPixelShaderRegisters);
}

/*-----------------------------------------------------------------------------
	Output state.
-----------------------------------------------------------------------------*/

/**
 * Inverts depth comparison function if we GInvertZ is currently TRUE
 * @param CompareFunction - source depth compare function to convert
 * @return resulting converted D3DCMPFUNC
 */
D3DCMPFUNC ToggleCompareFunction(D3DCMPFUNC CompareFunction)
{
	if (!GInvertZ) 
	{
		return CompareFunction;
	}

	switch(CompareFunction)
	{
	case D3DCMP_LESS: return D3DCMP_GREATER;
	case D3DCMP_LESSEQUAL: return D3DCMP_GREATEREQUAL;
	case D3DCMP_GREATER: return D3DCMP_LESS;
	case D3DCMP_GREATEREQUAL: return D3DCMP_LESSEQUAL;
	default: return CompareFunction;
	};
}

void RHISetDepthState(FDepthStateRHIParamRef NewState)
{
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,NewState->bZEnable);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,NewState->bZWriteEnable);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,ToggleCompareFunction(NewState->ZFunc));
}

void RHISetStencilState(FStencilStateRHIParamRef NewState)
{
	GDirect3DDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE,NewState->bTwoSidedStencilMode);

	GDirect3DDevice->SetRenderState(D3DRS_STENCILENABLE,NewState->bStencilEnable);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILFUNC,NewState->StencilFunc);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILFAIL,NewState->StencilFail);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILZFAIL,NewState->StencilZFail);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILPASS,NewState->StencilPass);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILMASK,NewState->StencilReadMask);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILWRITEMASK,NewState->StencilWriteMask);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILREF,NewState->StencilRef);

	GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILFUNC,NewState->CCWStencilFunc);
	GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILFAIL,NewState->CCWStencilFail);
	GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILZFAIL,NewState->CCWStencilZFail);
	GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILPASS,NewState->CCWStencilPass);
	GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILWRITEMASK,NewState->StencilWriteMask);
	GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILMASK,NewState->StencilReadMask);
	GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILREF,NewState->StencilRef);
}

void RHISetBlendState(FBlendStateRHIParamRef NewState)
{
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,NewState->bAlphaBlendEnable);
	// enable high precision blending for 7e3 target only when alpha blending is enabled
	GDirect3DDevice->SetRenderState(D3DRS_HIGHPRECISIONBLENDENABLE,NewState->bAlphaBlendEnable);
	GDirect3DDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,NewState->bSeparateAlphaBlendEnable);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHATESTENABLE,NewState->bAlphaTestEnable);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHAFUNC,NewState->AlphaFunc);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHAREF,NewState->AlphaRef);

	D3DBLENDSTATE BlendState;
	BlendState.BlendOp        = NewState->ColorBlendOperation;
	BlendState.SrcBlend       = NewState->ColorSourceBlendFactor;
	BlendState.DestBlend      = NewState->ColorDestBlendFactor;
	BlendState.BlendOpAlpha   = NewState->AlphaBlendOperation;
	BlendState.SrcBlendAlpha  = NewState->AlphaSourceBlendFactor;
	BlendState.DestBlendAlpha = NewState->AlphaDestBlendFactor;
	GDirect3DDevice->SetBlendState(0,BlendState);
}

// Map the render target index to the appropriate D3DRS_COLORWRITEENABLEx state
static const D3DRENDERSTATETYPE MRT_HIGHPRECISIONBLENDENABLE[4] = {D3DRS_HIGHPRECISIONBLENDENABLE,D3DRS_HIGHPRECISIONBLENDENABLE1,D3DRS_HIGHPRECISIONBLENDENABLE2,D3DRS_HIGHPRECISIONBLENDENABLE3};

void RHISetMRTBlendState(FBlendStateRHIParamRef NewState, UINT TargetIndex)
{
	// enable high precision blending for 7e3 target only when alpha blending is enabled
	GDirect3DDevice->SetRenderState(MRT_HIGHPRECISIONBLENDENABLE[TargetIndex],NewState->bAlphaBlendEnable);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHATESTENABLE,NewState->bAlphaTestEnable);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHAFUNC,NewState->AlphaFunc);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHAREF,NewState->AlphaRef);

	D3DBLENDSTATE BlendState;
	BlendState.BlendOp        = NewState->ColorBlendOperation;
	BlendState.SrcBlend       = NewState->ColorSourceBlendFactor;
	BlendState.DestBlend      = NewState->ColorDestBlendFactor;
	BlendState.BlendOpAlpha   = NewState->AlphaBlendOperation;
	BlendState.SrcBlendAlpha  = NewState->AlphaSourceBlendFactor;
	BlendState.DestBlendAlpha = NewState->AlphaDestBlendFactor;
	GDirect3DDevice->SetBlendState(TargetIndex,BlendState);
}

void RHISetRenderTarget(FSurfaceRHIParamRef NewRenderTarget,FSurfaceRHIParamRef NewDepthStencilTarget)
{
	GDirect3DDevice->SetRenderTarget(0,NewRenderTarget);
	GDirect3DDevice->SetDepthStencilSurface(NewDepthStencilTarget);

	// detect if we're using scene color, assumption is that D3DFMT_A2B10G10R10F_EDRAM corresponds to scene color
	if( IsValidRef(NewRenderTarget) )
	{
		D3DSURFACE_DESC RenderTargetDesc;
		NewRenderTarget->GetDesc(&RenderTargetDesc);
		GCurrentUsingSceneColorTarget = RenderTargetDesc.Format == D3DFMT_A2B10G10R10F_EDRAM;
	}
	else
	{
		GCurrentUsingSceneColorTarget = FALSE;
	}

	// SetRenderTarget resets the render target GPU states
	GCurrentRenderTargetYOffset = 0;

	// Detect when the back buffer is being set, and set the correct viewport.
	if( (IDirect3DSurface9*) NewRenderTarget == GD3DBackBuffer && 
		GD3DDrawingViewport != NULL )
	{
		D3DVIEWPORT9 D3DViewport = { 0, 0, GD3DDrawingViewport->GetSizeX(), GD3DDrawingViewport->GetSizeY(), 0.0f, 1.0f };
		GDirect3DDevice->SetViewport(&D3DViewport);
	}

	// keep track of current InvertZ state
	// it is only set to TRUE if there is a valid Depth/Stencil and it is a floating point format depth buffer
	GInvertZ = (GUsesInvertedZ && IsValidRef(NewDepthStencilTarget) && (NewDepthStencilTarget->Format == D3DFMT_D24FS8));

	if (GInvertZ)
	{
		// Swap MinZ/MaxZ for the current viewport
		D3DVIEWPORT9 D3DViewport;
		GDirect3DDevice->GetViewport(&D3DViewport);
		Exchange<FLOAT>(D3DViewport.MinZ,D3DViewport.MaxZ);
		GDirect3DDevice->SetViewport(&D3DViewport);
	}

	// Reset all texture references, to ensure a reference to this render target doesn't remain set.
	UnsetPSTextures();
	UnsetVSTextures();

	// Keep track of current render target's color exp bias for shader params that use it. 
	// This should be 0 for all render targets except for the scene color
	GCurrentColorExpBias = IsValidRef(NewRenderTarget) ? NewRenderTarget.XeSurfaceInfo.GetColorExpBias() : 0;

	RHISetRenderTargetBias( appPow(2.0f,GCurrentColorExpBias ));
}
void RHISetMRTRenderTarget(FSurfaceRHIParamRef NewRenderTarget,UINT TargetIndex)
{
	GDirect3DDevice->SetRenderTarget(TargetIndex,NewRenderTarget);

	// Reset all texture references, to ensure a reference to this render target doesn't remain set.
	UnsetPSTextures();
	UnsetVSTextures();
}
void RHISetColorWriteEnable(UBOOL bEnable)
{
	DWORD EnabledStateValue = D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED;
	GDirect3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE,bEnable ? EnabledStateValue : 0);
}

// Map the render target index to the appropriate D3DRS_COLORWRITEENABLEx state
static const D3DRENDERSTATETYPE MRTColorWriteEnableStates[4] = {D3DRS_COLORWRITEENABLE,D3DRS_COLORWRITEENABLE1,D3DRS_COLORWRITEENABLE2,D3DRS_COLORWRITEENABLE3};

void RHISetMRTColorWriteEnable(UBOOL bEnable,UINT TargetIndex)
{
	DWORD EnabledStateValue = D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED;
	GDirect3DDevice->SetRenderState(MRTColorWriteEnableStates[TargetIndex],bEnable ? EnabledStateValue : 0);
}

void RHISetColorWriteMask(UINT ColorWriteMask)
{
	DWORD EnabledStateValue;
	EnabledStateValue  = (ColorWriteMask & CW_RED) ? D3DCOLORWRITEENABLE_RED : 0;
	EnabledStateValue |= (ColorWriteMask & CW_GREEN) ? D3DCOLORWRITEENABLE_GREEN : 0;
	EnabledStateValue |= (ColorWriteMask & CW_BLUE) ? D3DCOLORWRITEENABLE_BLUE : 0;
	EnabledStateValue |= (ColorWriteMask & CW_ALPHA) ? D3DCOLORWRITEENABLE_ALPHA : 0;
	GDirect3DDevice->SetRenderState( D3DRS_COLORWRITEENABLE, EnabledStateValue );
}

void RHISetMRTColorWriteMask(UINT ColorWriteMask,UINT TargetIndex)
{
	DWORD EnabledStateValue;
	EnabledStateValue  = (ColorWriteMask & CW_RED) ? D3DCOLORWRITEENABLE_RED : 0;
	EnabledStateValue |= (ColorWriteMask & CW_GREEN) ? D3DCOLORWRITEENABLE_GREEN : 0;
	EnabledStateValue |= (ColorWriteMask & CW_BLUE) ? D3DCOLORWRITEENABLE_BLUE : 0;
	EnabledStateValue |= (ColorWriteMask & CW_ALPHA) ? D3DCOLORWRITEENABLE_ALPHA : 0;
	GDirect3DDevice->SetRenderState( MRTColorWriteEnableStates[TargetIndex], EnabledStateValue );
}

void RHIBeginHiStencilRecord(UBOOL bCompareFunctionEqual, UINT RefValue)
{
	GDirect3DDevice->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );
	GDirect3DDevice->SetRenderState( D3DRS_HISTENCILWRITEENABLE, TRUE );
	GDirect3DDevice->SetRenderState( D3DRS_HISTENCILREF, RefValue );
	GDirect3DDevice->SetRenderState( D3DRS_HISTENCILFUNC, bCompareFunctionEqual ? D3DHSCMP_EQUAL : D3DHSCMP_NOTEQUAL );	
}

void RHIBeginHiStencilPlayback(UBOOL bFlush) 
{ 
	GDirect3DDevice->SetRenderState( D3DRS_HISTENCILENABLE, TRUE );
	GDirect3DDevice->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );
	if (bFlush)
	{
		// Note: Not flushing will cause difficult to debug artifacts.
		// Only pass in bFlush == FALSE if you know RHIBeginHiStencilPlayback(TRUE) has been called since the last hi stencil write.
		GDirect3DDevice->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS ); 
	}
}

void RHIEndHiStencil() 
{
	GDirect3DDevice->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );
	GDirect3DDevice->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );
}

/*-----------------------------------------------------------------------------
	Occlusion queries.
-----------------------------------------------------------------------------*/

void RHIBeginOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQuery)
{
	OcclusionQuery->Query->Issue(D3DISSUE_BEGIN);
}
void RHIEndOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQuery)
{
	OcclusionQuery->Query->Issue(D3DISSUE_END);
}

/*-----------------------------------------------------------------------------
	Primitive drawing.
-----------------------------------------------------------------------------*/

static D3DPRIMITIVETYPE GetD3DPrimitiveType(UINT PrimitiveType)
{
	switch(PrimitiveType)
	{
		case PT_TriangleList:			return D3DPT_TRIANGLELIST;
		case PT_TriangleStrip:			return D3DPT_TRIANGLESTRIP;
		case PT_LineList:				return D3DPT_LINELIST;
		case PT_QuadList:				return D3DPT_QUADLIST;
		default:						appErrorf(TEXT("Unknown primitive type: %u"),PrimitiveType);
	};
	return D3DPT_TRIANGLELIST;
}

static D3DTESSPRIMITIVETYPE GetD3DTessellatedPrimitiveType(UINT PrimitiveType)
{
	switch(PrimitiveType)
	{
		case PT_TessellatedQuadPatch:	return D3DTPT_QUADPATCH;
		default:						appErrorf(TEXT("Unknown tessellated primitive type: %u"),PrimitiveType);
	};
	return D3DTPT_QUADPATCH;
}

void RHIDrawPrimitive(UINT PrimitiveType,UINT BaseVertexIndex,UINT NumPrimitives)
{
	check(GD3DRenderingIsSuspended == FALSE);
	checkSlow( NumPrimitives > 0 );

	if ( NumPrimitives > 0 )
	{
	    if ( PrimitiveType != PT_TessellatedQuadPatch )
	    {
		    GDirect3DDevice->DrawPrimitive(
			    GetD3DPrimitiveType(PrimitiveType),
			    BaseVertexIndex,
			    NumPrimitives
			    );
	    }
	    else
	    {
		    GDirect3DDevice->DrawTessellatedPrimitive(
			    GetD3DTessellatedPrimitiveType(PrimitiveType),
			    BaseVertexIndex,
			    NumPrimitives
			    );
	    }
	}

	check(GUpdateStreamForInstancingMask == 0);
	check(GPendingNumInstances <= 1);
}

void RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives)
{
	check(GD3DRenderingIsSuspended == FALSE);
	checkSlow( NumPrimitives > 0 );
	IDirect3DIndexBuffer9* D3DIndexBufferPtr = (IDirect3DIndexBuffer9*) IndexBuffer->Resource;
	GDirect3DDevice->SetIndices(D3DIndexBufferPtr);
	UBOOL bIsDoingInstancing = GPendingNumInstances > 1 && GUpdateStreamForInstancingMask;

	// If the index buffer has been setup for instancing, then we assume the vertex factory has also
	// been hacked for instancing...otherwise we must do pure fake instancing.
	// If a shader has been hacked for instancing, it will still work drawing one instance per call.
	UINT MaxInstancesPerBatch = IndexBuffer->NumInstances;
	UBOOL bDoingHardwareInstancing = MaxInstancesPerBatch > 1;
	check(GPendingNumInstances);
	UINT InstancesToDraw = GPendingNumInstances;
	UINT BaseInstanceIndex = 0;
	UINT NumInstancesThisBatch = 1;

	IDirect3DVertexShader9* VertexShader = NULL;
	IDirect3DVertexDeclaration9* VertexDeclaration = NULL;
	GDirect3DDevice->GetVertexDeclaration(&VertexDeclaration);
	GDirect3DDevice->GetVertexShader(&VertexShader);
	if (!(VertexDeclaration && !VertexShader->IsBound() || !VertexDeclaration && VertexShader->IsBound()))
	{
		appErrorf(TEXT("Using a valid vertex declaration with a bound vertex shader or a NULL declaration with an unbound shader, VDecl %u, VS bound %u"), (INT)(VertexDeclaration != NULL), VertexShader->IsBound());
	}
	// D3D only requires us to release the reference to the vertex shader, not to the declaration
	VertexShader->Release();

	do
	{
		// Set the instance-indexed vertex streams with a base address of the current instance batch.
		if (bIsDoingInstancing)
		{
			UINT InstancingMask = GUpdateStreamForInstancingMask;
			for (UINT StreamIndex = 0; StreamIndex < MAX_VERTEX_STREAMS && InstancingMask ; StreamIndex++)
			{
				if (InstancingMask & 1)
				{
					IDirect3DVertexBuffer9* D3DVertexBufferPtr = (IDirect3DVertexBuffer9*) GPendingStreams[StreamIndex].VertexBuffer->Resource;
					GDirect3DDevice->SetStreamSource(
						StreamIndex,
						D3DVertexBufferPtr,
						GPendingStreams[StreamIndex].Stride * BaseInstanceIndex,
						bDoingHardwareInstancing ? GPendingStreams[StreamIndex].Stride : 0
						);
				}
				InstancingMask >>= 1;
			}
			NumInstancesThisBatch = Min<UINT>(InstancesToDraw,MaxInstancesPerBatch);
		}

		// Draw the current instance batch.
		if ( NumPrimitives > 0 )
		{
		    GDirect3DDevice->DrawIndexedPrimitive(
			    GetD3DPrimitiveType(PrimitiveType),
			    BaseVertexIndex,
			    MinIndex,
			    NumVertices,
			    StartIndex,
			    NumPrimitives * NumInstancesThisBatch
			    );
		}

		BaseInstanceIndex += NumInstancesThisBatch;
		InstancesToDraw -= NumInstancesThisBatch;
	}  while ( InstancesToDraw > 0 );
}

void RHIDrawIndexedPrimitive_PreVertexShaderCulling(FIndexBufferRHIParamRef IndexBuffer,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives,const FMatrix& LocalToWorld,const void* PlatformMeshData)
{
	// Don't use pre-vertexshader-culling on 360.
	RHIDrawIndexedPrimitive(IndexBuffer,PrimitiveType,BaseVertexIndex,MinIndex,NumVertices,StartIndex,NumPrimitives);
}

/**
 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param NumVertices The number of vertices to be written
 * @param VertexDataStride Size of each vertex 
 * @param OutVertexData Reference to the allocated vertex memory
 */
void RHIBeginDrawPrimitiveUP(UINT PrimitiveType, UINT NumPrimitives, UINT NumVertices, UINT VertexDataStride, void*& OutVertexData)
{
	check(GD3DRenderingIsSuspended == FALSE);
	check(FALSE == GInBeginVertices);
	GInBeginVertices = TRUE;	

	HRESULT D3DResult = GDirect3DDevice->BeginVertices(GetD3DPrimitiveType(PrimitiveType), NumVertices, VertexDataStride, &OutVertexData);
	if (D3DResult != S_OK)
	{
		if (D3DResult == E_OUTOFMEMORY)
		{
			// Kick it, just in case it has stalled
			GDirect3DDevice->InsertFence();

			// Out of memory, so the command buffer is assumed full.
			if (GEngine->BeginUPTryCount == 0)
			{
				GEngine->BeginUPTryCount = 100000;
			}

			DWORD Time = 0;
			CLOCK_CYCLES(Time);
			// Busy loop, then retry X times before considering it a failure.
			INT AttemptCount = 0;
			while (AttemptCount < GEngine->BeginUPTryCount)
			{
				appSleep(0);

				// Try again
				D3DResult = GDirect3DDevice->BeginVertices(GetD3DPrimitiveType(PrimitiveType), NumVertices, VertexDataStride, &OutVertexData);
				if (D3DResult == S_OK)
				{
					// Fall out of the loop and return
					break;
				}
				else
				{
					 if (D3DResult != E_OUTOFMEMORY)
					 {
						FString ErrorCodeText( FString::Printf(TEXT("%08X"),(INT)D3DResult) );
						appErrorf(TEXT("%s failed at %s:%u with error %s"),TEXT("BeginVertices"),ANSI_TO_TCHAR(__FILE__),__LINE__,*ErrorCodeText);
					}
					AttemptCount++;
				}
			}

			UNCLOCK_CYCLES(Time);
			warnf(TEXT("BeginVertices: Attempted %6d times to get %8d bytes (%6d verts) and waited for %f seconds"), 
				AttemptCount, NumVertices * VertexDataStride, NumVertices, Time * GSecondsPerCycle);
			if (D3DResult != S_OK)
			{
				FString ErrorCodeText( FString::Printf(TEXT("%08X"),(INT)D3DResult) );
				appErrorf(TEXT("%s failed at %s:%u with error %s"),TEXT("BeginVertices"),ANSI_TO_TCHAR(__FILE__),__LINE__,*ErrorCodeText);
			}
		}
		else
		{
			FString ErrorCodeText( FString::Printf(TEXT("%08X"),(INT)D3DResult) );
			appErrorf(TEXT("%s failed at %s:%u with error %s"),TEXT("BeginVertices"),ANSI_TO_TCHAR(__FILE__),__LINE__,*ErrorCodeText);
		}
	}
	check(NumVertices == 0 || OutVertexData != NULL);
}

/**
 * Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
 */
void RHIEndDrawPrimitiveUP()
{
	check(GD3DRenderingIsSuspended == FALSE);
	check(TRUE == GInBeginVertices);
	GInBeginVertices = FALSE;

	GDirect3DDevice->EndVertices();
}

/**
* Calculates the number of vertices used by the number of primitives of a certain primitive type.
* Used to get the number of vertices for a drawprimup call or to get the number of indices for a drawindexedprimup call.
* @param PrimitiveType - EPrimitiveType value
* @param NumPrimitives - number of primitives used to calculate total vertices used
* @return total number of vertices/indices
*/
static INT GetPrimitiveTypeCount(INT PrimitiveType,INT NumPrimitives)
{
	switch (PrimitiveType)
	{
	case PT_TriangleList:
		return( NumPrimitives * 3 );
	case PT_TriangleStrip:	
		// This could change w/ degenerate indicators.
		return( NumPrimitives + 2 );
	case PT_LineList:		
		// This likely will never be used...
		return( NumPrimitives * 2 );
	case PT_QuadList:		
		// This likely will never be used...
		return(NumPrimitives * 4);
	default: 
		check( 0 && "invalid primitive type" );
		return(0);
	}
}

/** Gets set to TRUE when withing BeginTiling/EndTiling rendering */
static UBOOL GWithinBeginEndTilingBlock = FALSE;

/**
* Validates whether there is too much data for Draw*PrimitiveUP.
*
* @param NumVertices	Number of vertices
* @param VertexStride	Vertex stride
*
* @return TRUE if it is safe to call Draw*PrimitiveUP, FALSE otherwise
*/
static UBOOL ValidateDrawUPCall( INT NumVertices, INT VertexStride )
{
	UBOOL Result=TRUE;

	if( (NumVertices * VertexStride) > GDrawUPVertexCheckCount )
	{
		warnf(TEXT("Omitting DrawPrimitiveUP as call would use too much space in the ringbuffer! Size: %i Verts: %i Stride: %i "), 
			NumVertices * VertexStride, 
			NumVertices, 
			VertexStride );
		Result = FALSE;
	}
	// safeguard against running out of space in static command buffer used for tiled rendering
	else if( GWithinBeginEndTilingBlock )
	{
		DWORD TilingCommandBufferUsed=0;
		DWORD TilingCommandBufferRemaining=0;
		GDirect3DDevice->QueryBufferSpace(&TilingCommandBufferUsed,&TilingCommandBufferRemaining);
		if( (NumVertices * VertexStride) > (INT)TilingCommandBufferRemaining )
		{
			warnf(TEXT("Omitting DrawPrimitiveUP as call would use too much space in the ringbuffer! Size: %i Verts: %i Stride: %i TilingCommandBufferRemaining: %d"), 
				NumVertices * VertexStride, 
				NumVertices, 
				VertexStride,
				TilingCommandBufferRemaining);
			Result = FALSE;
		}
	}
	else if ( NumVertices <= 0 )
	{
		checkSlow( NumVertices > 0 );
		Result = FALSE;
	}

	return Result;
}

/**
* Validates whether there is too much data for Draw*PrimitiveUP.
*
* @param NumVertices	Number of vertices
* @param VertexStride	Vertex stride
* @param NumIndices		Number of Indices
* @param IndexStride	Index stride
*
* @return TRUE if it is safe to call Draw*PrimitiveUP, FALSE otherwise
*/
static UBOOL ValidateDrawIndexedUPCall( INT NumVertices, INT VertexStride, INT PrimitiveType, INT NumPrimitives, INT IndexStride )
{
	UBOOL Result=TRUE;

	INT NumIndices = GetPrimitiveTypeCount( PrimitiveType, NumPrimitives );	
	if ((NumIndices * IndexStride) > GDrawUPIndexCheckCount)
	{
		warnf(TEXT("Omitting DrawIndexedPrimitiveUP as call would use too much space in the ringbuffer! Size: %i Indices: %i Stride: %i "), 
			NumIndices * IndexStride, 
			NumIndices, 
			IndexStride);
		Result = FALSE; 
	}
	// safeguard against running out of space in static command buffer used for tiled rendering
	else if( GWithinBeginEndTilingBlock )
	{
		DWORD TilingCommandBufferUsed=0;
		DWORD TilingCommandBufferRemaining=0;
		GDirect3DDevice->QueryBufferSpace(&TilingCommandBufferUsed,&TilingCommandBufferRemaining);
		if( (NumVertices * VertexStride) > (INT)TilingCommandBufferRemaining )
		{
			warnf(TEXT("Omitting DrawIndexedPrimitiveUP as call would use too much space in the ringbuffer! Size: %i Verts: %i Stride: %i TilingCommandBufferRemaining: %d"), 
				NumVertices * VertexStride, 
				NumVertices, 
				VertexStride,
				TilingCommandBufferRemaining);
			Result = FALSE;
		}
	}
	else
	{
		Result = ValidateDrawUPCall( NumVertices, VertexStride);
	}

	return Result;
}

/**
 * Draw a primitive using the vertices passed in
 * VertexData is NOT created by BeginDrawPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param VertexData A reference to memory preallocate in RHIBeginDrawPrimitiveUP
 * @param VertexDataStride The size of one vertex
 */
void RHIDrawPrimitiveUP(UINT PrimitiveType, UINT NumPrimitives, const void* VertexData,UINT VertexDataStride)
{
	check(GD3DRenderingIsSuspended == FALSE);

	INT NumVertices = GetPrimitiveTypeCount( PrimitiveType, NumPrimitives );
	if( ValidateDrawUPCall( NumVertices, VertexDataStride ) )
	{
		GDirect3DDevice->DrawPrimitiveUP(
			GetD3DPrimitiveType(PrimitiveType),
			NumPrimitives,
			VertexData,
			VertexDataStride
			);
	}
}

/**
 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawIndexedPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param NumVertices The number of vertices to be written
 * @param VertexDataStride Size of each vertex
 * @param OutVertexData Reference to the allocated vertex memory
 * @param MinVertexIndex The lowest vertex index used by the index buffer
 * @param NumIndices Number of indices to be written
 * @param IndexDataStride Size of each index (either 2 or 4 bytes)
 * @param OutIndexData Reference to the allocated index memory
 */
void RHIBeginDrawIndexedPrimitiveUP(UINT PrimitiveType, UINT NumPrimitives, UINT NumVertices, UINT VertexDataStride, void*& OutVertexData, UINT MinVertexIndex, UINT NumIndices, UINT IndexDataStride, void*& OutIndexData)
{
	check(GD3DRenderingIsSuspended == FALSE);
	check(FALSE == GInBeginVertices);
	GInBeginVertices = TRUE;

	if( ValidateDrawIndexedUPCall( NumVertices, VertexDataStride, PrimitiveType, NumPrimitives, IndexDataStride ) )
	{
		HRESULT D3DResult = GDirect3DDevice->BeginIndexedVertices(GetD3DPrimitiveType(PrimitiveType), MinVertexIndex, NumVertices, 
			NumIndices, IndexDataStride == sizeof(WORD) ? D3DFMT_INDEX16 : D3DFMT_INDEX32, VertexDataStride, 
			&OutIndexData, &OutVertexData);
		if (D3DResult != S_OK)
		{
			if (D3DResult == E_OUTOFMEMORY)
			{
				// Kick it, just in case it has stalled
				GDirect3DDevice->InsertFence();

				// Out of memory, so the command buffer is assumed full.
				if (GEngine->BeginUPTryCount == 0)
				{
					GEngine->BeginUPTryCount = 100000;
				}

				// Busy loop, then retry X times before considering it a failure.
				DWORD Time = 0;
				CLOCK_CYCLES(Time);
				// Busy loop, then retry X times before considering it a failure.
				INT AttemptCount = 0;
				while (AttemptCount < GEngine->BeginUPTryCount)
				{
					appSleep(0);

					// Try again
					D3DResult = GDirect3DDevice->BeginIndexedVertices(GetD3DPrimitiveType(PrimitiveType), MinVertexIndex, NumVertices, 
						NumIndices, IndexDataStride == sizeof(WORD) ? D3DFMT_INDEX16 : D3DFMT_INDEX32, VertexDataStride, 
						&OutIndexData, &OutVertexData);
					if (D3DResult == S_OK)
					{
						// Fall out of the loop and return
						break;
					}
					else
					{
						 if (D3DResult != E_OUTOFMEMORY)
						 {
							FString ErrorCodeText( FString::Printf(TEXT("%08X"),(INT)D3DResult) );
							appErrorf(TEXT("%s failed at %s:%u with error %s"),TEXT("BeginIndexedVertices"),ANSI_TO_TCHAR(__FILE__),__LINE__,*ErrorCodeText);
						}
						AttemptCount++;
					}
				}

				UNCLOCK_CYCLES(Time);
				warnf(TEXT("BeginIndexedVertices: Attempted %6d times to get %8d bytes (%6d verts) and waited for %f seconds"), 
					AttemptCount, NumVertices * VertexDataStride, NumVertices, Time * GSecondsPerCycle);
				if (D3DResult != S_OK)
				{
					FString ErrorCodeText( FString::Printf(TEXT("%08X"),(INT)D3DResult) );
					appErrorf(TEXT("%s failed at %s:%u with error %s"),TEXT("BeginIndexedVertices"),ANSI_TO_TCHAR(__FILE__),__LINE__,*ErrorCodeText);
				}
			}
			else
			{
				FString ErrorCodeText( FString::Printf(TEXT("%08X"),(INT)D3DResult) );
				appErrorf(TEXT("%s failed at %s:%u with error %s"),TEXT("BeginIndexedVertices"),ANSI_TO_TCHAR(__FILE__),__LINE__,*ErrorCodeText);
			}
		}
	}
}

/**
 * Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
 */
void RHIEndDrawIndexedPrimitiveUP()
{
	check(GD3DRenderingIsSuspended == FALSE);
	check(TRUE == GInBeginVertices);
	GInBeginVertices = FALSE;

	GDirect3DDevice->EndIndexedVertices();
}

/**
 * Draw a primitive using the vertices passed in as described the passed in indices. 
 * IndexData and VertexData are NOT created by BeginDrawIndexedPrimitveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param MinVertexIndex The lowest vertex index used by the index buffer
 * @param NumVertices The number of vertices in the vertex buffer
 * @param NumPrimitives THe number of primitives described by the index buffer
 * @param IndexData The memory preallocated in RHIBeginDrawIndexedPrimitiveUP
 * @param IndexDataStride The size of one index
 * @param VertexData The memory preallocate in RHIBeginDrawIndexedPrimitiveUP
 * @param VertexDataStride The size of one vertex
 */
void RHIDrawIndexedPrimitiveUP(UINT PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT NumPrimitives, const void* IndexData, UINT IndexDataStride, const void* VertexData, UINT VertexDataStride)
{
	check(GD3DRenderingIsSuspended == FALSE);

	if( ValidateDrawIndexedUPCall( NumVertices, VertexDataStride, PrimitiveType, NumPrimitives, IndexDataStride ) )
	{
		GDirect3DDevice->DrawIndexedPrimitiveUP(
			GetD3DPrimitiveType(PrimitiveType),
			MinVertexIndex,
			NumVertices,
			NumPrimitives,
			IndexData,
			IndexDataStride == sizeof(WORD) ? D3DFMT_INDEX16 : D3DFMT_INDEX32,
			VertexData,
			VertexDataStride
			);
	}
}

/**
 * Draw a sprite particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite particles
 */
void RHIDrawSpriteParticles(const FMeshElement& Mesh)
{
	check(GD3DRenderingIsSuspended == FALSE);

	check(Mesh.DynamicVertexData);
	FDynamicSpriteEmitterData* SpriteData = (FDynamicSpriteEmitterData*)(Mesh.DynamicVertexData);

	// Sort the particles if required
	INT ParticleCount = SpriteData->Source.ActiveParticleCount;

	// 'clamp' the number of particles actually drawn
	//@todo.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	INT StartIndex = 0;
	INT EndIndex = ParticleCount;
	if ((SpriteData->Source.MaxDrawCount >= 0) && (ParticleCount > SpriteData->Source.MaxDrawCount))
	{
		ParticleCount = SpriteData->Source.MaxDrawCount;
	}

	FParticleOrder* ParticleOrder = (FParticleOrder*)(Mesh.DynamicIndexData);

	// Render the particles are indexed tri-lists
	// Make sure we won't choke the buffer
	if (ValidateDrawUPCall( ParticleCount * 4, sizeof(FParticleSpriteVertex)))
	{
		// Get the memory from the device for copying the particle vertex/index data to
		void* OutVertexData = NULL;
		RHIBeginDrawPrimitiveUP(PT_QuadList, ParticleCount, ParticleCount * 4, Mesh.DynamicVertexStride, OutVertexData);
		check(OutVertexData);
		// Pack the data
		FParticleSpriteVertex* Vertices = (FParticleSpriteVertex*)OutVertexData;
		SpriteData->GetVertexAndIndexData(Vertices, NULL, ParticleOrder);
		// End the draw, which will submit the data for rendering
		RHIEndDrawPrimitiveUP();
	}
	else
	{
		warnf(TEXT("Failed to DrawSpriteParticles - too many vertices (%d)"), ParticleCount * 4);
	}
}

/**
 * Draw a sprite subuv particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite subuv particles
 */
void RHIDrawSubUVParticles(const FMeshElement& Mesh)
{
	check(GD3DRenderingIsSuspended == FALSE);

	check(Mesh.DynamicVertexData);
	FDynamicSubUVEmitterData* SubUVData = (FDynamicSubUVEmitterData*)(Mesh.DynamicVertexData);

	// Sort the particles if required
	INT ParticleCount = SubUVData->Source.ActiveParticleCount;

	// 'clamp' the number of particles actually drawn
	//@todo.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	INT StartIndex = 0;
	INT EndIndex = ParticleCount;
	if ((SubUVData->Source.MaxDrawCount >= 0) && (ParticleCount > SubUVData->Source.MaxDrawCount))
	{
		ParticleCount = SubUVData->Source.MaxDrawCount;
	}

	FParticleOrder* ParticleOrder = (FParticleOrder*)(Mesh.DynamicIndexData);

	// Render the particles are indexed tri-lists
	// Make sure we won't choke the buffer
	if (ValidateDrawUPCall( ParticleCount * 4, sizeof(FParticleSpriteSubUVVertex)))
	{
		// Get the memory from the device for copying the particle vertex/index data to
		void* OutVertexData = NULL;
		RHIBeginDrawPrimitiveUP(PT_QuadList, ParticleCount, ParticleCount * 4, Mesh.DynamicVertexStride, OutVertexData);
		check(OutVertexData);
		// Pack the data
		FParticleSpriteSubUVVertex* Vertices = (FParticleSpriteSubUVVertex*)OutVertexData;
		SubUVData->GetVertexAndIndexData(Vertices, NULL, ParticleOrder);
		// End the draw, which will submit the data for rendering
		RHIEndDrawPrimitiveUP();
	}
	else
	{
		warnf(TEXT("Failed to DrawSpriteParticles - too many vertices (%d)"), ParticleCount * 4);
	}
}


/**
 * Draw a point sprite particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite subuv particles
 */
void RHIDrawPointSpriteParticles(const FMeshElement& Mesh) 
{
	// Not implemented yet!
}


// Kick the rendering commands that are currently queued up in the GPU command buffer.
void RHIKickCommandBuffer()
{
	check(GD3DRenderingIsSuspended == FALSE);
	if ( !GWithinBeginEndTilingBlock )
	{
		// This will add a new fence and kick the command buffer.
		GDirect3DDevice->InsertFence();
	}
}

// Blocks the CPU until the GPU catches up and goes idle.
void RHIBlockUntilGPUIdle()
{
	check(GD3DRenderingIsSuspended == FALSE);
	XeBlockUntilGPUIdle();
}

/*-----------------------------------------------------------------------------
	Raster operations.
-----------------------------------------------------------------------------*/

void RHIClear(UBOOL bClearColor,const FLinearColor& Color,UBOOL bClearDepth,FLOAT Depth,UBOOL bClearStencil,DWORD Stencil)
{
	check(GD3DRenderingIsSuspended == FALSE);

	// Determine the clear flags.
	DWORD Flags = 0;
	if(bClearColor)
	{
		Flags |= D3DCLEAR_TARGET;
	}
	if(bClearDepth)
	{
		Flags |= D3DCLEAR_ZBUFFER;
	}
	if(bClearStencil)
	{
		Flags |= D3DCLEAR_STENCIL;
	}
	if (GInvertZ)
	{
		Depth = 1.f - Depth;
	}
	// Clear the render target/depth-stencil surfaces based on the flags.
	FColor QuantizedColor(Color.Quantize());
	GDirect3DDevice->Clear(0,NULL,Flags,D3DCOLOR_RGBA(QuantizedColor.R,QuantizedColor.G,QuantizedColor.B,QuantizedColor.A),Depth,Stencil);
}

extern const INT GNumTilingModes;
const INT GNumTilingModes = 5;
static const INT GMaxTiles = 3;
UINT GTilingMode = 1;

static D3DSurface *GMSAADepth[GNumTilingModes] = { NULL };
static D3DSurface *GMSAAColor[GNumTilingModes] = { NULL };
static D3DSurface *GMSAAPrepassDepth[GNumTilingModes] = { NULL };

static const D3DRECT GTilingRects[GNumTilingModes][GMaxTiles] =
{
	{ // 1280x720 4xAA
		{   0,   0,  1280, 256 },
		{   0, 256,  1280, 512 },
		{   0, 512,  1280, 720 },
	},
	{ // 1280x720 2xAA
		{   0, 0,   1280, 512 },
		{   0, 512, 1280, 720 },
	},
	{ // 1280x720 2xAA
		{   0, 0,   1280, 384 },
		{   0, 384, 1280, 720 },
	},
	{ // 1280x720 2xAA
		{   0, 0,   1280, 256 },
		{   0, 256, 1280, 512 },
		{   0, 512, 1280, 720 },
	},
	{ // 1280x720 1xAA
		{   0, 0,   1280, 720 },
	},
};

extern const TCHAR *GTilingDesc[];
const TCHAR *GTilingDesc[GNumTilingModes] = 
{
	TEXT("1280x720 4xAA, 3 tiles"),
	TEXT("1280x720 2xAA, 2 tiles"),
	TEXT("1280x720 2xAA, 2 tiles"),
	TEXT("1280x720 2xAA, 3 tiles"),
	TEXT("1280x720 1xAA, 1 tile"),
};

static FLOAT GViewportXOffsets[GNumTilingModes] = { -0.125f, -0.25f, -0.25f, -0.25f, 0.0f };
static FLOAT GViewportYOffsets[GNumTilingModes] = { -0.125f, 0.25f, 0.25f, 0.25f, 0.0f };

static INT GNumTiles[GNumTilingModes] = { 3, 2, 2, 3, 1 };
static UBOOL GUseOnePassZPass[GNumTilingModes] = { FALSE, TRUE, TRUE, TRUE, TRUE };

static const D3DMULTISAMPLE_TYPE GMultisampleType[GNumTilingModes] =
{
	D3DMULTISAMPLE_4_SAMPLES,
	D3DMULTISAMPLE_2_SAMPLES,
	D3DMULTISAMPLE_2_SAMPLES,
	D3DMULTISAMPLE_2_SAMPLES,
	D3DMULTISAMPLE_NONE,
};

static INT GetTileWidth(INT TilingMode)
{
	INT i, TileWidth = 0;
	for (i = 0; i < GNumTiles[TilingMode]; ++ i)
	{
		TileWidth = Max(TileWidth, (INT)(GTilingRects[TilingMode][i].x2 - GTilingRects[TilingMode][i].x1));
	}
	return TileWidth;
}

static INT GetTileHeight(INT TilingMode)
{
	INT i, TileHeight = 0;
	for (i = 0; i < GNumTiles[TilingMode]; ++ i)
	{
		TileHeight = Max(TileHeight, (INT)(GTilingRects[TilingMode][i].y2 - GTilingRects[TilingMode][i].y1));
	}
	return TileHeight;
}

static unsigned GetTilingScreenWidth(INT TilingMode)
{
	return GTilingRects[TilingMode][GNumTiles[TilingMode] - 1].x2;
}

static unsigned GetTilingScreenHeight(INT TilingMode)
{
	return GTilingRects[TilingMode][GNumTiles[TilingMode] - 1].y2;
}

static void InitRenderTargets()
{
	static UBOOL InitializedRTs = FALSE;
	if (InitializedRTs)
	{
		return;
	}

	InitializedRTs = TRUE;

	D3DSURFACE_PARAMETERS SurfaceParams;
	appMemset(&SurfaceParams, 0, sizeof(D3DSURFACE_PARAMETERS));
	SurfaceParams.HierarchicalZBase = 0;

	INT i;
	for (i = 0; i < GNumTilingModes; ++ i)
	{
		DWORD TileWidth = GetTileWidth(i);
		DWORD TileHeight = GetTileHeight(i);

		SurfaceParams.Base = 0;
		GDirect3DDevice->CreateDepthStencilSurface(TileWidth, TileHeight, GUsesInvertedZ ? D3DFMT_D24FS8 : D3DFMT_D24S8, GMultisampleType[i], 0, 0, &GMSAADepth[i], &SurfaceParams);

		if (GUseOnePassZPass[i])
		{
			GDirect3DDevice->CreateDepthStencilSurface(GetTilingScreenWidth(i), GetTilingScreenHeight(i), GUsesInvertedZ ? D3DFMT_D24FS8 : D3DFMT_D24S8, GMultisampleType[i], 0, 0, &GMSAAPrepassDepth[i], &SurfaceParams);
		}
		else
		{
			GMSAAPrepassDepth[i] = GMSAADepth[i];
		}

		SurfaceParams.Base = GMSAADepth[i]->Size / GPU_EDRAM_TILE_SIZE;
		GDirect3DDevice->CreateRenderTarget(TileWidth, TileHeight, D3DFMT_A2B10G10R10F_EDRAM, GMultisampleType[i], 0, 0, &GMSAAColor[i], &SurfaceParams);
	}
}

extern UBOOL GInvertZ;

static void SetMainpassRenderTargets()
{
	InitRenderTargets();
	GDirect3DDevice->SetDepthStencilSurface(GMSAADepth[GTilingMode]);
	GDirect3DDevice->SetRenderTarget(0, GMSAAColor[GTilingMode]);
	GInvertZ = GUsesInvertedZ;

	GCurrentColorExpBias = XeGetRenderTargetColorExpBias(D3DFMT_A2B10G10R10F_EDRAM, (D3DFORMAT)GPixelFormats[PF_FloatRGB].PlatformFormat);
	RHISetRenderTargetBias( appPow(2.0f,GCurrentColorExpBias ));
}

/**
 * Used by non-tiling MSAA split screen; returns Screen Height / 2, and a valid offset to keep Resolve happy
 */
void RHIGetMSAAOffsets(UINT *HalfScreenY, UINT *ResolveOffset)
{
	check(GTilingMode==1 && "MSAA offsets only available in 1280x720, two tiled mode");
	UINT HalfY = GTilingRects[GTilingMode][1].y2 / 2;

	if (HalfScreenY)
	{
		*HalfScreenY = HalfY;
	}
	if (ResolveOffset)
	{
		// Xenon GPU limitation: (sourceY - destY) % 32 must be 0
		*ResolveOffset = HalfY % 32;
	}
}


/**
 * Configures surface settings for a depth-prepass before the MSAA block
 */
void RHIMSAAInitPrepass()
{
	SCOPED_DRAW_EVENT(EventMSAAInit)(DEC_SCENE_ITEMS,TEXT("MSAAInitPrepass"));

	InitRenderTargets();
	GDirect3DDevice->SetDepthStencilSurface(GMSAAPrepassDepth[GTilingMode]);
	GDirect3DDevice->SetRenderTarget(0, NULL);
	GInvertZ = GUsesInvertedZ;
	GCurrentColorExpBias = 0;

	RHISetRenderTargetBias( 1);
}

/**
 * Adds fractional offsets to the viewport to make fragment 0 of the MSAA buffers align with
 * a non-MSAA surface.  This is necessary since MSAA fragments are off-center; not doing so
 * causes depth inaccuracies when we try to restore the depth values later.
 */
void RHIMSAAFixViewport()
{
	GDirect3DDevice->GpuOwn(D3DTAG_VALUESPACKET);
	GDirect3DDevice->m_ValuesPacket.VportXOffset += GViewportXOffsets[GTilingMode];
	GDirect3DDevice->m_ValuesPacket.VportYOffset += GViewportYOffsets[GTilingMode];
	GDirect3DDevice->GpuDisown(D3DTAG_VALUESPACKET);
}

/**
 * Begins the block of MSAA rendering and clears the render targets accordingly.
 */
void RHIMSAABeginRendering(UBOOL bRequiresClear)
{
	check(GD3DRenderingIsSuspended == FALSE);

	SCOPED_DRAW_EVENT(EventMSAABeginRendering)(DEC_SCENE_ITEMS,TEXT("MSAABeginRendering"));

	// globally mark when using scene color as the current target
	GCurrentUsingSceneColorTarget=TRUE;

	SetMainpassRenderTargets();

	if (!GUseMSAASplitScreen)
	{
		DWORD Flags = GUseOnePassZPass[GTilingMode] ? (D3DTILING_ONE_PASS_ZPASS | D3DTILING_FIRST_TILE_INHERITS_DEPTH_BUFFER | D3DTILING_SKIP_FIRST_TILE_CLEAR) : 0;
		GDirect3DDevice->BeginTiling(Flags, GNumTiles[GTilingMode], GTilingRects[GTilingMode], NULL, GUsesInvertedZ ? 0.0f : 1.0f, 0);
		GWithinBeginEndTilingBlock = TRUE;
		if (bRequiresClear)
		{
			// Clear only the first tile if requested
			GDirect3DDevice->SetPredication(D3DPRED_TILE_RENDER(0));
			GDirect3DDevice->Clear( 0, NULL, D3DCLEAR_TARGET0, 0, 0.0f, 0);
			GDirect3DDevice->SetPredication(0);
		}
		RHIMSAAFixViewport();
	}
	else
	{
		if (bRequiresClear)
		{
			GDirect3DDevice->Clear( 0, NULL, D3DCLEAR_TARGET0, 0, 0.0f, 0);
		}
	}
}

/**
 * Ends the block of MSAA rendering and saves the results to textures.
 * @param DepthTexture Texture to copy fragment 0 of the depth data to
 * @param ColorTexture Texture to copy the MSAA color information to
 * @param ViewIndex Current view index, used by non-tiling MSAA split screen
 */
void RHIMSAAEndRendering(FTexture2DRHIParamRef DepthTexture, FTexture2DRHIParamRef ColorTexture, UINT ViewIndex)
{
	check(GD3DRenderingIsSuspended == FALSE);

	// globally mark when not using scene color as the current target
	GCurrentUsingSceneColorTarget=FALSE;

	SCOPED_DRAW_EVENT(EventMSAAEndRendering)(DEC_SCENE_ITEMS,TEXT("MSAAEndRendering"));
	INT ColorExpBias = XeGetRenderTargetColorExpBias(D3DFMT_A2B10G10R10F_EDRAM, (D3DFORMAT)GPixelFormats[PF_FloatRGB].PlatformFormat);
	D3DTexture *pDepthTexture = (D3DTexture*)DepthTexture->Resource;
	D3DTexture *pColorTexture = (D3DTexture*)ColorTexture->Resource;

	if (GUseMSAASplitScreen)
	{
		// Non-tiling MSAA split screen mode
		check(GTilingMode == 1 && "Non tiling MSAA split screen is only available in mode 1");
		D3DPOINT DestResolvePosition;
		D3DRECT SourceRect;
		UINT HalfScreenY;
		UINT ResolveOffset;
		
		RHIGetMSAAOffsets(&HalfScreenY, &ResolveOffset);

		// Must use 1280x720 for MSAA split screen
		SourceRect.x1 = 0;
		SourceRect.y1 = 0;
		SourceRect.x2 = GTilingRects[GTilingMode][1].x2;
		SourceRect.y2 = HalfScreenY;

		DestResolvePosition.x = 0;
		if (ViewIndex == 0)
		{
			DestResolvePosition.y = 0;
		}
		else
		{
			// Xenon GPU limitation: (sourceY - destY) % 32 must be 0
			UINT ResolveOffset = HalfScreenY % 32;
			HalfScreenY = GTilingRects[GTilingMode][1].y2 / 2;
			SourceRect.y1 += ResolveOffset;
			SourceRect.y2 += ResolveOffset;
			DestResolvePosition.y = HalfScreenY;
		}
		// remove render target offset if present
		OffsetRenderTargetBase(0, 0, 0);

		GDirect3DDevice->Resolve(
			D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS | D3DRESOLVE_EXPONENTBIAS(-ColorExpBias), 
			&SourceRect, 
			pColorTexture, 
			&DestResolvePosition, 
			0, 
			0, 
			NULL, 
			0.0f, 
			0, 
			NULL
			);

		// rest the Y offset so that fragment0 depth matches up with the non-msaa depth
		GDirect3DDevice->GpuOwn(D3DTAG_VALUESPACKET);
		GDirect3DDevice->m_ValuesPacket.VportYOffset -= GViewportYOffsets[GTilingMode];
		GDirect3DDevice->GpuDisown(D3DTAG_VALUESPACKET);

		GDirect3DDevice->Resolve(
			D3DRESOLVE_DEPTHSTENCIL | D3DRESOLVE_FRAGMENT0, 
			&SourceRect, 
			pDepthTexture, 
			&DestResolvePosition, 
			0, 
			0, 
			NULL, 
			GUsesInvertedZ ? 0.0f : 1.0f, 
			0, 
			NULL
			);
	}
	else
	{
		// Regular MSAA tiling mode
		INT TileNum;
		for	(TileNum = 0; TileNum <	GNumTiles[GTilingMode]; ++ TileNum)
		{
			GDirect3DDevice->SetPredication(D3DPRED_TILE_RENDER(TileNum));

			D3DPOINT destResolvePosition;
			destResolvePosition.x =	GTilingRects[GTilingMode][TileNum].x1;
			destResolvePosition.y =	GTilingRects[GTilingMode][TileNum].y1;
			GDirect3DDevice->Resolve(
				D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS | D3DRESOLVE_EXPONENTBIAS(-ColorExpBias), 
				&GTilingRects[GTilingMode][TileNum], 
				pColorTexture, 
				&destResolvePosition, 
				0, 
				0, 
				NULL, 
				0.0f, 
				0, 
				NULL
				);

	#if !FINAL_RELEASE
			GDirect3DDevice->Resolve(
				D3DRESOLVE_DEPTHSTENCIL | D3DRESOLVE_FRAGMENT0, 
				&GTilingRects[GTilingMode][TileNum], 
				pDepthTexture, 
				&destResolvePosition, 
				0, 
				0, 
				NULL, 
				GUsesInvertedZ ? 0.0f : 1.0f, 
				0, 
				NULL
				);
			D3DVIEWPORT9 tempViewport, tempViewport2;
			// save current viewport
			GDirect3DDevice->GetViewport(&tempViewport);
			tempViewport2.X = 0;
			tempViewport2.Y = 0;
			tempViewport2.Height = GetTilingScreenHeight(GTilingMode);
			tempViewport2.Width = GetTilingScreenWidth(GTilingMode);
			tempViewport2.MinZ = tempViewport.MinZ;
			tempViewport2.MaxZ = tempViewport.MaxZ;
			// set viewport to tiling extents for clear
			GDirect3DDevice->SetViewport(&tempViewport2);
			GDirect3DDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 0.0f, 0);
			// restore current viewport
			GDirect3DDevice->SetViewport(&tempViewport);
	#else
			GDirect3DDevice->Resolve(
				D3DRESOLVE_CLEARDEPTHSTENCIL | D3DRESOLVE_DEPTHSTENCIL | D3DRESOLVE_FRAGMENT0, 
				&GTilingRects[GTilingMode][TileNum], 
				pDepthTexture, 
				&destResolvePosition, 
				0, 
				0, 
				NULL, 
				GUsesInvertedZ ? 0.0f : 1.0f, 
				0, 
				NULL
				);
	#endif // !FINAL_RELEASE
		}
		GDirect3DDevice->SetPredication(0);

		// Query remaining space in command buffer to be able to see how close we are wrt to filling it up.
		DWORD CommandBufferBytesUsed = 0;
		DWORD CommandBufferBytesRemaining = 0;
		DWORD MaxCommandBufferBytesUsed = 0;
		DWORD MinCommandBufferBytesRemaining = INT_MAX;
		GDirect3DDevice->QueryBufferSpace( &CommandBufferBytesUsed, &CommandBufferBytesRemaining );
		MaxCommandBufferBytesUsed		= Max( MaxCommandBufferBytesUsed, CommandBufferBytesUsed );
		MinCommandBufferBytesRemaining	= Min( MinCommandBufferBytesRemaining, CommandBufferBytesRemaining );

		SET_DWORD_STAT( STAT_CommandBufferBytesRemaining, CommandBufferBytesRemaining / 1024 );
		SET_DWORD_STAT( STAT_CommandBufferBytesUsed, CommandBufferBytesUsed / 1024 );
		SET_DWORD_STAT( STAT_CommandBufferBytesMinRemaining, MinCommandBufferBytesRemaining / 1024 );
		SET_DWORD_STAT( STAT_CommandBufferBytesMaxUsed, MaxCommandBufferBytesUsed / 1024 );

		GDirect3DDevice->EndTiling(0, NULL, NULL, NULL, 0.0f, 0, NULL);
		GWithinBeginEndTilingBlock=FALSE;
		GDirect3DDevice->InsertFence();
	}
}

/**
 * Copies the contents of the depth texture back into EDRAM.
 * This also re-populates HiZ.
 * @param ColorTexture Texture containing color data (optionally NULL)
 * @param DepthTexture Texture containing depth data (requires valid texture)
 */
void RHIRestoreColorDepth(FTexture2DRHIParamRef ColorTexture, FTexture2DRHIParamRef DepthTexture)
{
	check(GD3DRenderingIsSuspended == FALSE);

	SCOPED_DRAW_EVENT(EventRestoreColorDepth)(DEC_SCENE_ITEMS,TEXT("RestoreColorDepth"));

	static UBOOL bInitialized = FALSE;
	static D3DVertexDeclaration* VertexDecl = NULL;

	if (!bInitialized)
	{
		D3DVERTEXELEMENT9 VertexElements[2];
		D3DVERTEXELEMENT9 VertexEnd = {0xFF,0,(DWORD)D3DDECLTYPE_UNUSED,0,0,0};
		VertexElements[1] = VertexEnd;
		VertexElements[0].Method = D3DDECLMETHOD_DEFAULT;
		VertexElements[0].Offset = 0;
		VertexElements[0].Stream = 0;
		VertexElements[0].Type = D3DDECLTYPE_FLOAT4;
		VertexElements[0].Usage = D3DDECLUSAGE_POSITION;
		VertexElements[0].UsageIndex = 0;

		VERIFYD3DRESULT(GDirect3DDevice->CreateVertexDeclaration(VertexElements, &VertexDecl));

		bInitialized = TRUE;
	}

	DWORD OldHalfPixelOffset; 
	DWORD OldColorWriteEnable;
	DWORD OldDepthFunc;
	DWORD OldDepthWriteEnable;
	DWORD OldDepthEnable;
	DWORD OldStencilEnable;
	DWORD OldMagFilter[2], OldMinFilter[2];
	D3DSURFACE_DESC CurrentDepthStencilDesc;
	D3DSURFACE_DESC TextureDesc;
	IDirect3DSurface9* CurrentDepthStencil = NULL;
	GDirect3DDevice->GetDepthStencilSurface( &CurrentDepthStencil );
	CurrentDepthStencil->GetDesc( &CurrentDepthStencilDesc );
	((D3DTexture*)DepthTexture->Resource)->GetLevelDesc( 0, &TextureDesc );

	// Clear the stencil values.
	RHIClear( FALSE, FLinearColor::Black, FALSE, 1.0f, TRUE, 0);

	GDirect3DDevice->GetRenderState(D3DRS_HALFPIXELOFFSET, &OldHalfPixelOffset);
	GDirect3DDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &OldColorWriteEnable);
	GDirect3DDevice->GetRenderState(D3DRS_ZFUNC, &OldDepthFunc);
	GDirect3DDevice->GetRenderState(D3DRS_ZWRITEENABLE, &OldDepthWriteEnable);
	GDirect3DDevice->GetRenderState(D3DRS_ZENABLE, &OldDepthEnable);
	GDirect3DDevice->GetRenderState(D3DRS_STENCILENABLE, &OldStencilEnable);
	GDirect3DDevice->GetSamplerState(0, D3DSAMP_MINFILTER, &OldMinFilter[0]);
	GDirect3DDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &OldMagFilter[0]);
	GDirect3DDevice->GetSamplerState(1, D3DSAMP_MINFILTER, &OldMinFilter[1]);
	GDirect3DDevice->GetSamplerState(1, D3DSAMP_MAGFILTER, &OldMagFilter[1]);

	GDirect3DDevice->SetRenderState(D3DRS_HALFPIXELOFFSET, TRUE);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);
	GDirect3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	GDirect3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	GDirect3DDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	GDirect3DDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	// Have to update the shadowed texture sampler state manually since we are calling GDirect3DDevice->SetSamplerState directly
	ShadowedSamplerStates[0].SamplerState = NULL;
	ShadowedSamplerStates[1].SamplerState = NULL;

	GDirect3DDevice->SetVertexDeclaration(VertexDecl);
	TShaderMapRef<FRestoreColorAndDepthVertexShader> VertexShader(GetGlobalShaderMap());
	GDirect3DDevice->SetVertexShader(VertexShader->GetVertexShader());

	TShaderMapRef<FRestoreDepthOnlyPixelShader> DepthOnlyPixelShader(GetGlobalShaderMap());
	TShaderMapRef<FRestoreColorAndDepthPixelShader> ColorAndDepthPixelShader(GetGlobalShaderMap());
	TShaderMapRef<FRestoreDownsamplingDepthOnlyPixelShader> DownsamplingDepthOnlyPixelShader(GetGlobalShaderMap());
	TShaderMapRef<FRestoreDownsamplingColorAndDepthPixelShader> DownsamplingColorAndDepthPixelShader(GetGlobalShaderMap());

	// Check if we need to restore color as well as depth.
	enum ERestoreOption
	{
		DEPTHONLY = 0,
		COLORANDDEPTH = 1
	};
	ERestoreOption ShaderIndex = DEPTHONLY;
	if ( ColorTexture )
	{
		FVector4 ColorBias( appPow(2.0f,GCurrentColorExpBias), 0.0f, 0.0f, 0.0f );
		ShaderIndex = COLORANDDEPTH;
		GDirect3DDevice->SetTexture(1, (D3DTexture*)ColorTexture->Resource);
		// Have to update the shadowed texture sampler state manually since we are calling GDirect3DDevice->SetTexture directly
		ShadowedSamplerStates[1].Texture = (D3DTexture*)ColorTexture->Resource;
		GDirect3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_ALPHA);
		GDirect3DDevice->SetPixelShaderConstantF( 0, (FLOAT*) &ColorBias, 1 );
	}
	else
	{
		GDirect3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
	}

	if ( TextureDesc.Width == CurrentDepthStencilDesc.Width )
	{
		GDirect3DDevice->SetPixelShader((ShaderIndex == DEPTHONLY) ? 
			DepthOnlyPixelShader->GetPixelShader() : 
			ColorAndDepthPixelShader->GetPixelShader());
	}
	else
	{
		check( TextureDesc.Width/2 == CurrentDepthStencilDesc.Width && TextureDesc.Height/2 == CurrentDepthStencilDesc.Height );
		GDirect3DDevice->SetPixelShader((ShaderIndex == DEPTHONLY) ? 
			DownsamplingDepthOnlyPixelShader->GetPixelShader() : 
			DownsamplingColorAndDepthPixelShader->GetPixelShader());
	}
	
	GDirect3DDevice->SetTexture(0, (D3DTexture*)DepthTexture->Resource);
	// Have to update the shadowed texture sampler state manually since we are calling GDirect3DDevice->SetTexture directly
	ShadowedSamplerStates[0].Texture = (D3DTexture*)DepthTexture->Resource;

	void *RawVertexData;
	VERIFYD3DRESULT(GDirect3DDevice->BeginVertices(D3DPT_RECTLIST, 3, sizeof(XMVECTOR), &RawVertexData));
	((XMVECTOR*)RawVertexData)[0] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	((XMVECTOR*)RawVertexData)[1] = XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
	((XMVECTOR*)RawVertexData)[2] = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
	VERIFYD3DRESULT(GDirect3DDevice->EndVertices());

	GDirect3DDevice->SetRenderState(D3DRS_HALFPIXELOFFSET, OldHalfPixelOffset);
	GDirect3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE, OldColorWriteEnable);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC, OldDepthFunc);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, OldDepthWriteEnable);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE, OldDepthEnable);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILENABLE, OldStencilEnable);
	GDirect3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, OldMinFilter[0]);
	GDirect3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, OldMagFilter[0]);
	GDirect3DDevice->SetSamplerState(1, D3DSAMP_MINFILTER, OldMinFilter[1]);
	GDirect3DDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, OldMagFilter[1]);
	// Have to update the shadowed texture sampler state manually since we are calling GDirect3DDevice->SetSamplerState directly
	ShadowedSamplerStates[0].SamplerState = NULL;
	ShadowedSamplerStates[1].SamplerState = NULL;
}

/*-----------------------------------------------------------------------------
	Functions to yield and regain rendering control from D3D
-----------------------------------------------------------------------------*/

/** Whether the device is currently suspended */
UBOOL GD3DRenderingIsSuspended = FALSE;
/** Whether the device should be suspended */
UBOOL GD3DRenderingShouldBeSuspended = FALSE;

void RHISuspendRendering()
{
	GD3DRenderingShouldBeSuspended = TRUE;
	if (!GD3DRenderingIsSuspended)
	{
		GD3DRenderingIsSuspended = TRUE;
		GDirect3DDevice->Suspend();
	}
}

void RHIResumeRendering()
{
	GD3DRenderingShouldBeSuspended = FALSE;
	if (GD3DRenderingIsSuspended)
	{
		GDirect3DDevice->Resume();
		GD3DRenderingIsSuspended = FALSE;
	}
}

UBOOL RHIIsRenderingSuspended()
{
	return GD3DRenderingIsSuspended;
}

/*-----------------------------------------------------------------------------
	Tessellation
-----------------------------------------------------------------------------*/

void RHISetTessellationMode( ETessellationMode TessellationMode, FLOAT MinTessellation, FLOAT MaxTessellation )
{
	D3DTESSELLATIONMODE Mode;
	switch ( TessellationMode )
	{
		case TESS_Discrete:		Mode = D3DTM_DISCRETE;   break;
		case TESS_PerEdge:		Mode = D3DTM_PEREDGE;    break;
		case TESS_Continuous:
		default:				Mode = D3DTM_CONTINUOUS; break;
	}
	GDirect3DDevice->SetRenderState( D3DRS_TESSELLATIONMODE, Mode );
	GDirect3DDevice->SetRenderState( D3DRS_MINTESSELLATIONLEVEL, FLOAT_TO_DWORD(MinTessellation) );
	GDirect3DDevice->SetRenderState( D3DRS_MAXTESSELLATIONLEVEL, FLOAT_TO_DWORD(MaxTessellation) );
}

/*-----------------------------------------------------------------------------
	Misc
-----------------------------------------------------------------------------*/

/*
 * Returns the total GPU time taken to render the last frame. Same metric as appCycles().
 */
DWORD RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

/*
 * Returns an approximation of the available memory that textures can use, which is video + AGP where applicable, rounded to the nearest MB, in MB.
 */
DWORD RHIGetAvailableTextureMemory()
{
	//not currently implemented on xenon
	return 0;
}

#endif // USE_XeD3D_RHI
