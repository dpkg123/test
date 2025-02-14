/*=============================================================================
	UnXenonSplash.cpp: Splash screen implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Core includes.
#include "CorePrivate.h"
#include "XeD3DDrvPrivate.h"

/** The vertex declaration for the splash screen shader. */
static D3DVertexDeclaration* SplashVertexDecl   = NULL;

/** Initialization for using the splash screen. */
void appXenonInitSplash()
{
#if USE_XeD3D_RHI
	// Create vertex declaration
	D3DVERTEXELEMENT9 Declaration[] = 
	{
		{ 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};
	verify( SUCCEEDED( GDirect3DDevice->CreateVertexDeclaration( Declaration, &SplashVertexDecl ) ) );
#endif  // USE_XeD3D_RHI
}

/**
 * Shows splash screen and optionally persists display so we can reboot.
 *
 * @param	SplashName	path to file containing splash screen
 * @param	bPersist	whether to persist display
 */
void appXenonShowSplash( const TCHAR* SplashName, UBOOL bPersist )
{
	FString SplashPath;
	// make sure the splash was found
	if (appGetSplashPath(SplashName, SplashPath) == FALSE)
	{
		return;
	}

#if USE_XeD3D_RHI
	// Load a texture into memory and create a texture from there.
	TArray<BYTE>		Data;
	IDirect3DTexture9*	Texture;
	if( appLoadFileToArray( Data, *SplashPath, GFileManager ) )
	{
		verify( SUCCEEDED( D3DXCreateTextureFromFileInMemory( GDirect3DDevice, &Data(0), Data.Num(), &Texture) ) );

		// Setup the splash screen vertices
		struct VERTEX
		{ 
			FLOAT sx, sy;
			FLOAT tu, tv;
		};
		VERTEX SplashVertices[4] = 
		{
			{ -1.0f,  1.0f,   0.0f, 0.0f },
			{  1.0f,  1.0f,   1.0f, 0.0f },
			{  1.0f, -1.0f,   1.0f, 1.0f },
			{ -1.0f, -1.0f,   0.0f, 1.0f },
		};

		// Set texture coordinates
		XGTEXTURE_DESC TextureDesc;
		XGGetTextureDesc( Texture, 0, &TextureDesc );
		SplashVertices[0].tu = 0.0f;   SplashVertices[0].tv = 0.0f;
		SplashVertices[1].tu = 1.0f;   SplashVertices[1].tv = 0.0f;
		SplashVertices[2].tu = 1.0f;   SplashVertices[2].tv = 1.0f;
		SplashVertices[3].tu = 0.0f;   SplashVertices[3].tv = 1.0f;

		// Grab the shaders from the global shader map
		TShaderMapRef<FSplashVertexShader> VertexShader(GetGlobalShaderMap());
		TShaderMapRef<FSplashPixelShader> PixelShader(GetGlobalShaderMap());

		// Set up state for rendering the splash screen.
		GDirect3DDevice->SetVertexDeclaration(SplashVertexDecl);
		GDirect3DDevice->SetVertexShader(VertexShader->GetVertexShader());
		GDirect3DDevice->SetTexture(0, Texture);
		// Have to update the shadowed texture sampler state manually since we are calling GDirect3DDevice->SetTexture directly
		extern FShadowedSamplerState ShadowedSamplerStates[16];
		ShadowedSamplerStates[0].Texture = Texture;
		GDirect3DDevice->SetPixelShader(PixelShader->GetPixelShader());
		GDirect3DDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
		GDirect3DDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
		GDirect3DDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
		GDirect3DDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
		// Have to update the shadowed texture sampler state manually since we are calling GDirect3DDevice->SetSamplerState directly
		ShadowedSamplerStates[0].SamplerState = NULL;

		GDirect3DDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
		GDirect3DDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
		GDirect3DDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
		GDirect3DDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
		GDirect3DDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		GDirect3DDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

		// Draw the splash image
		GDirect3DDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, SplashVertices, sizeof(SplashVertices[0]) );

		// Render the system UI and perform buffer swap.
		XePerformSwap( TRUE );

		// Make sure texture gets freed.
		GDirect3DDevice->SetTexture( 0, NULL );
		// Have to update the shadowed texture sampler state manually since we are calling GDirect3DDevice->SetTexture directly
		ShadowedSamplerStates[0].Texture = NULL;
		Texture->Release();
		Texture = NULL;

		// Keep the screen up even through reboots
		if( bPersist )
		{
			GDirect3DDevice->PersistDisplay( GD3DFrontBuffer, NULL );
		}
	}
#endif  // USE_XeD3D_RHI
}

