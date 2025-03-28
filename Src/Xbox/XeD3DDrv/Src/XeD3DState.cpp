/*=============================================================================
	D3DState.cpp: D3D state implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "XeD3DDrvPrivate.h"

#if USE_XeD3D_RHI

D3DTEXTUREADDRESS TranslateAddressMode(ESamplerAddressMode AddressMode)
{
	switch(AddressMode)
	{
	case AM_Clamp: return D3DTADDRESS_CLAMP;
	case AM_Mirror: return D3DTADDRESS_MIRROR;
	default: return D3DTADDRESS_WRAP;
	};
}

D3DCULL TranslateCullMode(ERasterizerCullMode CullMode)
{
	switch(CullMode)
	{
	case CM_CW: return D3DCULL_CW;
	case CM_CCW: return D3DCULL_CCW;
	default: return D3DCULL_NONE;
	};
}

D3DFILLMODE TranslateFillMode(ERasterizerFillMode FillMode)
{
	switch(FillMode)
	{
	case FM_Point: return D3DFILL_POINT;
	case FM_Wireframe: return D3DFILL_WIREFRAME;
	default: return D3DFILL_SOLID;
	};
}

D3DCMPFUNC TranslateCompareFunction(ECompareFunction CompareFunction)
{
	switch(CompareFunction)
	{
	case CF_Less: return D3DCMP_LESS;
	case CF_LessEqual: return D3DCMP_LESSEQUAL;
	case CF_Greater: return D3DCMP_GREATER;
	case CF_GreaterEqual: return D3DCMP_GREATEREQUAL;
	case CF_Equal: return D3DCMP_EQUAL;
	case CF_NotEqual: return D3DCMP_NOTEQUAL;
	case CF_Never: return D3DCMP_NEVER;
	default: return D3DCMP_ALWAYS;
	};
}

D3DSTENCILOP TranslateStencilOp(EStencilOp StencilOp)
{
	switch(StencilOp)
	{
	case SO_Zero: return D3DSTENCILOP_ZERO;
	case SO_Replace: return D3DSTENCILOP_REPLACE;
	case SO_SaturatedIncrement: return D3DSTENCILOP_INCRSAT;
	case SO_SaturatedDecrement: return D3DSTENCILOP_DECRSAT;
	case SO_Invert: return D3DSTENCILOP_INVERT;
	case SO_Increment: return D3DSTENCILOP_INCR;
	case SO_Decrement: return D3DSTENCILOP_DECR;
	default: return D3DSTENCILOP_KEEP;
	};
}

D3DBLENDOP TranslateBlendOp(EBlendOperation BlendOp)
{
	switch(BlendOp)
	{
	case BO_Subtract: return D3DBLENDOP_SUBTRACT;
	case BO_Min: return D3DBLENDOP_MIN;
	case BO_Max: return D3DBLENDOP_MAX;
    case BO_ReverseSubtract: return D3DBLENDOP_REVSUBTRACT;
	default: return D3DBLENDOP_ADD;
	};
}

D3DBLEND TranslateBlendFactor(EBlendFactor BlendFactor)
{
	switch(BlendFactor)
	{
	case BF_One: return D3DBLEND_ONE;
	case BF_SourceColor: return D3DBLEND_SRCCOLOR;
	case BF_InverseSourceColor: return D3DBLEND_INVSRCCOLOR;
	case BF_SourceAlpha: return D3DBLEND_SRCALPHA;
	case BF_InverseSourceAlpha: return D3DBLEND_INVSRCALPHA;
	case BF_DestAlpha: return D3DBLEND_DESTALPHA;
	case BF_InverseDestAlpha: return D3DBLEND_INVDESTALPHA;
	case BF_DestColor: return D3DBLEND_DESTCOLOR;
	case BF_InverseDestColor: return D3DBLEND_INVDESTCOLOR;
	default: return D3DBLEND_ZERO;
	};
}

FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	FD3DSamplerState* SamplerState = new FD3DSamplerState;
	SamplerState->AddressU = TranslateAddressMode(Initializer.AddressU);
	SamplerState->AddressV = TranslateAddressMode(Initializer.AddressV);
	SamplerState->AddressW = D3DTADDRESS_WRAP;
	switch(Initializer.Filter)
	{
	case SF_AnisotropicLinear:
		SamplerState->MinFilter = D3DTEXF_ANISOTROPIC;
		SamplerState->MagFilter	= D3DTEXF_LINEAR;
		SamplerState->MipFilter	= D3DTEXF_LINEAR;
		break;
	case SF_AnisotropicPoint:
		SamplerState->MinFilter = D3DTEXF_ANISOTROPIC;
		SamplerState->MagFilter	= D3DTEXF_LINEAR;
		SamplerState->MipFilter	= D3DTEXF_POINT;
		break;
	case SF_Trilinear:
		SamplerState->MinFilter = D3DTEXF_LINEAR;
		SamplerState->MagFilter	= D3DTEXF_LINEAR;
		SamplerState->MipFilter	= D3DTEXF_LINEAR;
		break;
	case SF_Bilinear:
		SamplerState->MinFilter = D3DTEXF_LINEAR;
		SamplerState->MagFilter	= D3DTEXF_LINEAR;
		SamplerState->MipFilter	= D3DTEXF_POINT;
		break;
	case SF_Point:
		SamplerState->MinFilter = D3DTEXF_POINT;
		SamplerState->MagFilter	= D3DTEXF_POINT;
		SamplerState->MipFilter	= D3DTEXF_POINT;
		break;
	}
	return SamplerState;
}

FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
    FD3DRasterizerState* RasterizerState = new FD3DRasterizerState;
	RasterizerState->CullMode = TranslateCullMode(Initializer.CullMode);
	RasterizerState->FillMode = TranslateFillMode(Initializer.FillMode);
	RasterizerState->DepthBias = 0;// GEMINI_TODO: needs a conversion formula
	RasterizerState->SlopeScaleDepthBias = Initializer.SlopeScaleDepthBias;
	return RasterizerState;
}

struct FShadowedRasterizerState : public FD3DRasterizerState
{
	FShadowedRasterizerState()
	{
		// Initialize states to invalid values so that they will be changed on first set
		FillMode = (D3DFILLMODE)-1;
		CullMode = (D3DCULL)-1;
		DepthBias = BIG_NUMBER;
		SlopeScaleDepthBias = BIG_NUMBER;
	}
};

// The current rasterizer state, tracked to reduce rasterizer state setting as an optimization
FShadowedRasterizerState ShadowedRasterizerState;

/**
 * Sets a rasterizer state using direct values instead of a cached object.  
 * This is preferable to calling RHISetRasterizerState( RHICreateRasterizerState(Initializer)) 
 * since at least some hardware platforms can optimize this path.
*/
void RHISetRasterizerStateImmediate(const FRasterizerStateInitializerRHI& ImmediateState)
{
	const D3DFILLMODE FillMode = TranslateFillMode(ImmediateState.FillMode);
	// Only call GDirect3DDevice->SetRenderState if the state has changed
	if (ShadowedRasterizerState.FillMode != FillMode)
	{
		GDirect3DDevice->SetRenderState(D3DRS_FILLMODE,FillMode);
		// Update the shadowed state
		ShadowedRasterizerState.FillMode = FillMode;
	}

	const D3DCULL CullMode = TranslateCullMode(ImmediateState.CullMode);
	if (ShadowedRasterizerState.CullMode != CullMode)
	{
		GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,CullMode);
		ShadowedRasterizerState.CullMode = CullMode;
	}
	
	extern FLOAT GDepthBiasOffset;
	if(GInvertZ)
	{
		// negate depth bias if we are using an inverted Z buffer
		const FLOAT DepthBias = -(ImmediateState.DepthBias + GDepthBiasOffset);
		const FLOAT SlopeScaleDepthBias = -ImmediateState.SlopeScaleDepthBias;

		if (ShadowedRasterizerState.DepthBias != ImmediateState.DepthBias)
		{
			GDirect3DDevice->SetRenderState(D3DRS_DEPTHBIAS,FLOAT_TO_DWORD(DepthBias));
			ShadowedRasterizerState.DepthBias = ImmediateState.DepthBias;
		}

		if (ShadowedRasterizerState.SlopeScaleDepthBias != ImmediateState.SlopeScaleDepthBias)
		{
			GDirect3DDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,FLOAT_TO_DWORD(SlopeScaleDepthBias));
			ShadowedRasterizerState.SlopeScaleDepthBias = ImmediateState.SlopeScaleDepthBias;
		}
	}
	else
	{
		if (ShadowedRasterizerState.DepthBias != ImmediateState.DepthBias + GDepthBiasOffset)
		{
			GDirect3DDevice->SetRenderState(D3DRS_DEPTHBIAS,FLOAT_TO_DWORD(ImmediateState.DepthBias + GDepthBiasOffset));
			ShadowedRasterizerState.DepthBias = ImmediateState.DepthBias + GDepthBiasOffset;
		}

		if (ShadowedRasterizerState.SlopeScaleDepthBias != ImmediateState.SlopeScaleDepthBias)
		{
			GDirect3DDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,FLOAT_TO_DWORD(ImmediateState.SlopeScaleDepthBias));
			ShadowedRasterizerState.SlopeScaleDepthBias = ImmediateState.SlopeScaleDepthBias;
		}
	}
}

/*-----------------------------------------------------------------------------
	Rasterizer state.
-----------------------------------------------------------------------------*/

void RHISetRasterizerState(FRasterizerStateRHIParamRef NewState)
{
	extern FLOAT GDepthBiasOffset;

	if (ShadowedRasterizerState.FillMode != NewState->FillMode)
	{
		GDirect3DDevice->SetRenderState(D3DRS_FILLMODE,NewState->FillMode);
		ShadowedRasterizerState.FillMode = NewState->FillMode;
	}

	if (ShadowedRasterizerState.CullMode != NewState->CullMode)
	{
		GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,NewState->CullMode);
		ShadowedRasterizerState.CullMode = NewState->CullMode;
	}

	if(GInvertZ)
	{
		// negate depth bias if we are using an inverted Z buffer
		const FLOAT DepthBias = -(NewState->DepthBias + GDepthBiasOffset);
		const FLOAT SlopeScaleDepthBias = -NewState->SlopeScaleDepthBias;
		
		if (ShadowedRasterizerState.DepthBias != DepthBias)
		{
			GDirect3DDevice->SetRenderState(D3DRS_DEPTHBIAS,FLOAT_TO_DWORD(DepthBias));
			ShadowedRasterizerState.DepthBias = DepthBias;
		}

		if (ShadowedRasterizerState.SlopeScaleDepthBias != SlopeScaleDepthBias)
		{
			GDirect3DDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,FLOAT_TO_DWORD(SlopeScaleDepthBias));
			ShadowedRasterizerState.SlopeScaleDepthBias = SlopeScaleDepthBias;
		}
	}
	else
	{
		if (ShadowedRasterizerState.DepthBias != NewState->DepthBias + GDepthBiasOffset)
		{
			GDirect3DDevice->SetRenderState(D3DRS_DEPTHBIAS,FLOAT_TO_DWORD(NewState->DepthBias + GDepthBiasOffset));
			ShadowedRasterizerState.DepthBias = NewState->DepthBias + GDepthBiasOffset;
		}

		if (ShadowedRasterizerState.SlopeScaleDepthBias != NewState->SlopeScaleDepthBias)
		{
			GDirect3DDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,FLOAT_TO_DWORD(NewState->SlopeScaleDepthBias));
			ShadowedRasterizerState.SlopeScaleDepthBias = NewState->SlopeScaleDepthBias;
		}
	}
}

FDepthStateRHIRef RHICreateDepthState(const FDepthStateInitializerRHI& Initializer)
{
	FD3DDepthState* DepthState = new FD3DDepthState;
	DepthState->bZEnable = Initializer.DepthTest != CF_Always || Initializer.bEnableDepthWrite;
	DepthState->bZWriteEnable = Initializer.bEnableDepthWrite;
	DepthState->ZFunc = TranslateCompareFunction(Initializer.DepthTest);
	return DepthState;
}

FStencilStateRHIRef RHICreateStencilState(const FStencilStateInitializerRHI& Initializer)
{
	FD3DStencilState* StencilState = new FD3DStencilState;
	StencilState->bStencilEnable = Initializer.bEnableFrontFaceStencil || Initializer.bEnableBackFaceStencil;
	StencilState->bTwoSidedStencilMode = Initializer.bEnableBackFaceStencil;
	StencilState->StencilFunc = TranslateCompareFunction(Initializer.FrontFaceStencilTest);
	StencilState->StencilFail = TranslateStencilOp(Initializer.FrontFaceStencilFailStencilOp);
	StencilState->StencilZFail = TranslateStencilOp(Initializer.FrontFaceDepthFailStencilOp);
	StencilState->StencilPass = TranslateStencilOp(Initializer.FrontFacePassStencilOp);
	StencilState->CCWStencilFunc = TranslateCompareFunction(Initializer.BackFaceStencilTest);
	StencilState->CCWStencilFail = TranslateStencilOp(Initializer.BackFaceStencilFailStencilOp);
	StencilState->CCWStencilZFail = TranslateStencilOp(Initializer.BackFaceDepthFailStencilOp);
	StencilState->CCWStencilPass = TranslateStencilOp(Initializer.BackFacePassStencilOp);
	StencilState->StencilReadMask = Initializer.StencilReadMask;
	StencilState->StencilWriteMask = Initializer.StencilWriteMask;
	StencilState->StencilRef = Initializer.StencilRef;
	return StencilState;
}

FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	FD3DBlendState* BlendState = new FD3DBlendState;
	BlendState->bAlphaBlendEnable = Initializer.ColorDestBlendFactor != BF_Zero || Initializer.ColorSourceBlendFactor != BF_One;
	BlendState->ColorBlendOperation = TranslateBlendOp(Initializer.ColorBlendOperation);
	BlendState->ColorSourceBlendFactor = TranslateBlendFactor(Initializer.ColorSourceBlendFactor);
	BlendState->ColorDestBlendFactor = TranslateBlendFactor(Initializer.ColorDestBlendFactor);
	BlendState->bSeparateAlphaBlendEnable =
		Initializer.AlphaDestBlendFactor != Initializer.ColorDestBlendFactor ||
		Initializer.AlphaSourceBlendFactor != Initializer.ColorSourceBlendFactor;
	BlendState->AlphaBlendOperation = TranslateBlendOp(Initializer.AlphaBlendOperation);
	BlendState->AlphaSourceBlendFactor = TranslateBlendFactor(Initializer.AlphaSourceBlendFactor);
	BlendState->AlphaDestBlendFactor = TranslateBlendFactor(Initializer.AlphaDestBlendFactor);
	BlendState->bAlphaTestEnable = Initializer.AlphaTest != CF_Always;
	BlendState->AlphaFunc = TranslateCompareFunction(Initializer.AlphaTest);
	BlendState->AlphaRef = Initializer.AlphaRef;
	return BlendState;
}

#endif
