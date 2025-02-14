/*=============================================================================
	D3DUtil.h: D3D RHI utility implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "XeD3DDrvPrivate.h"

#if USE_XeD3D_RHI 

//
// Stat declarations.
//

DECLARE_STATS_GROUP(TEXT("RHI"),STATGROUP_XeRHI);
DECLARE_DWORD_COUNTER_STAT(TEXT("CommandBuffer KB remaining"),STAT_CommandBufferBytesRemaining,STATGROUP_XeRHI);
DECLARE_DWORD_COUNTER_STAT(TEXT("CommandBuffer KB used"),STAT_CommandBufferBytesUsed,STATGROUP_XeRHI);
DECLARE_DWORD_COUNTER_STAT(TEXT("CommandBuffer Min KB remaining"),STAT_CommandBufferBytesMinRemaining,STATGROUP_XeRHI);
DECLARE_DWORD_COUNTER_STAT(TEXT("CommandBuffer Max KB used"),STAT_CommandBufferBytesMaxUsed,STATGROUP_XeRHI);
DECLARE_MEMORY_STAT(TEXT("Vertex Declaration Cache bytes"),STAT_VertexDeclarationCacheBytes,STATGROUP_XeRHI);

void VerifyD3DResult(HRESULT D3DResult,const ANSICHAR* Code,const ANSICHAR* Filename,UINT Line)
{
	if(FAILED(D3DResult))
	{
		FString ErrorCodeText( FString::Printf(TEXT("%08X"),(INT)D3DResult) );
		appErrorf(TEXT("%s failed at %s:%u with error %s"),ANSI_TO_TCHAR(Code),ANSI_TO_TCHAR(Filename),Line,*ErrorCodeText);
	}
}

/**
 * Adds a PIX event
 *
 * @note: UnXenon.cpp has the implementation for appBeginNamedEvent, which is identical code
 *
 * @param Color The color to draw the event as
 * @param Text The text displayed with the event
 */
void appBeginDrawEvent(const FColor& Color,const TCHAR* Text)
{
#if LINK_AGAINST_PROFILING_D3D_LIBRARIES
	PIXBeginNamedEvent(Color.DWColor(),TCHAR_TO_ANSI(Text));
#endif
}

/**
 * Ends the current PIX event
 *
 * @note: UnXenon.cpp has the implementation for appEndNamedEvent, which is identical code
 */
void appEndDrawEvent(void)
{
#if LINK_AGAINST_PROFILING_D3D_LIBRARIES
	PIXEndNamedEvent();
#endif
}

/**
 * Platform specific function for setting the value of a counter that can be
 * viewed in PIX.
 */
void appSetCounterValue(const TCHAR* CounterName, FLOAT Value)
{
#if LINK_AGAINST_PROFILING_D3D_LIBRARIES
	PIXAddNamedCounter( Value, TCHAR_TO_ANSI(CounterName) );
#endif
}

/*-----------------------------------------------------------------------------
	FGPUMemMove implementation.
-----------------------------------------------------------------------------*/

/** Constructor */
FGPUMemMove::FGPUMemMove()
:	VertexDecl( NULL )
,	MemCopyVertexShader( NULL )
{
}

/** Initializer to begin using this helper object. */
void FGPUMemMove::Initialize()
{
	// From GPUMemoryMove XDK sample.
	// It's using 8 memexports to avoid being set-up bound.
	// It's using ushort4 because 32-bit types has issues with bit-for-bit fetching/memexport (NaN handling).
	D3DVERTEXELEMENT9 D3DVertexElements[]	=
	{
		{ 0,  0,  D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0,  8,  D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
		{ 0,  16, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 2 },
		{ 0,  24, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 3 },
		{ 0,  32, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 4 },
		{ 0,  40, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 5 },
		{ 0,  48, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 6 },
		{ 0,  56, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 7 },
		D3DDECL_END()
	};

	VERIFYD3DRESULT( GDirect3DDevice->CreateVertexDeclaration( D3DVertexElements, &VertexDecl ) );
}

/** Must be called before calling MemMove(). */
void FGPUMemMove::BeginMemMove()
{
	if ( !IsInitialized() )
	{
		Initialize();
	}

	TShaderMapRef<FMemCopyVertexShader> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FNULLPixelShader> PixelShader(GetGlobalShaderMap());
	MemCopyVertexShader = *VertexShader;
	GDirect3DDevice->SetVertexDeclaration(VertexDecl);
	GDirect3DDevice->SetVertexShader(VertexShader->GetVertexShader());
	GDirect3DDevice->SetPixelShader(PixelShader->GetPixelShader());

	// Give max GPRs to the vertex shader (hasn't shown any improvement in practice)
#if DWTRIOVIZSDK
	// This is from Mikey W @ Xbox 
	GDirect3DDevice->SetShaderGPRAllocation( 0, 110, 18 );
#else
	GDirect3DDevice->SetShaderGPRAllocation( 0, 96, 32 );
#endif
}

/** Must be called after you're done calling MemMove(). */
void FGPUMemMove::EndMemMove()
{
	// Restore default GPR allocation
	GDirect3DDevice->SetShaderGPRAllocation( 0, 0, 0 );

	MemCopyVertexShader = NULL;
}

/**
 * Copies a block of memory using the GPU. The source and destination is allowed to overlap.
 * @param Dst		Destination base address. Must be aligned to 64 bytes.
 * @param Src		Source base address. Must be aligned to 64 bytes.
 * @param NumBytes	Number of bytes to copy. Must be a multiple of 64 bytes.
 */
void FGPUMemMove::MemMove( void* Dst, const void* Src, INT NumBytes )
{
	check( PTRINT(Src) % VertexSize == 0 );
	check( PTRINT(Dst) % VertexSize == 0 );
	check( NumBytes % VertexSize == 0 );

	// Alias the input memory as a vertex buffer
	IDirect3DVertexBuffer9 VertexBuffer;
	XGSetVertexBufferHeader( NumBytes, 0, 0, 0, &VertexBuffer );
	XGOffsetResourceAddress( &VertexBuffer, (void*) Src );
	GDirect3DDevice->SetStreamSource( 0, &VertexBuffer, 0, VertexSize );

	// Initialize the stream constant
	GPU_SET_MEMEXPORT_STREAM_CONSTANT( &MemExportStreamConstant,
		Dst,										// root address
		NumBytes / VertexElementSize,				// size in elements
		SURFACESWAP_LOW_RED,						// ARGB vs ABGR
		GPUSURFACENUMBER_UINTEGER,					// values are unsigned int
		GPUCOLORFORMAT_16_16_16_16,					// elements are 4 16-bit values
		GPUENDIAN128_8IN16 );						// byte-swapping for 16-bit values

	// Calculate difference between SrcIndex and DstIndex
	INT OffsetInVerts = ( PTRINT(Src) - PTRINT(Dst) ) / VertexSize;
	FVector4 fOffsetInVerts( 0 /*FLOAT(OffsetInVerts)*/, 0, 0, 0 );

	SetVertexShaderValue<FVector4>( MemCopyVertexShader->GetVertexShader(), MemCopyVertexShader->StreamConstantParameter, (FVector4&) MemExportStreamConstant.c );
	SetVertexShaderValue<FVector4>( MemCopyVertexShader->GetVertexShader(), MemCopyVertexShader->OffsetInVertsParameter, fOffsetInVerts );

	DWORD VertsLeftToMove = NumBytes / VertexSize;

	// Are we copying backwards?
	if ( PTRINT(Src) >= PTRINT(Dst) )
	{
		// Do a regular copy.
		DWORD SrcStartInVerts = 0 / VertexSize;

		while ( VertsLeftToMove > 0 )
		{
			const DWORD ChunkSizeInVerts = Min<DWORD>( VertsLeftToMove, Abs(OffsetInVerts) );

			// Ensures no side effects of memexports on earlier Draw calls using the same resource.
			GDirect3DDevice->BeginExport( 0, &VertexBuffer, D3DBEGINEXPORT_VERTEXSHADER );

			GDirect3DDevice->DrawPrimitive( D3DPT_POINTLIST, SrcStartInVerts, ChunkSizeInVerts );

			// Ensures no side effects of later Draw calls on memexports using the same resource.
			GDirect3DDevice->EndExport( 0, &VertexBuffer, 0 );

			SrcStartInVerts += ChunkSizeInVerts;
			VertsLeftToMove -= ChunkSizeInVerts;
		}
	}
	else
	{
		// Copying forwards.
		DWORD SrcStartInVerts = NumBytes / VertexSize;

		while ( VertsLeftToMove > 0 )
		{
			const DWORD ChunkSizeInVerts = Min<DWORD>( VertsLeftToMove, Abs(OffsetInVerts) );
			SrcStartInVerts -= ChunkSizeInVerts;
			VertsLeftToMove -= ChunkSizeInVerts;

			// Ensures no side effects of memexports on earlier Draw calls using the same resource.
			GDirect3DDevice->BeginExport( 0, &VertexBuffer, D3DBEGINEXPORT_VERTEXSHADER );

			GDirect3DDevice->DrawPrimitive( D3DPT_POINTLIST, SrcStartInVerts, ChunkSizeInVerts );

			// Ensures no side effects of later Draw calls on memexports using the same resource.
			GDirect3DDevice->EndExport( 0, &VertexBuffer, 0 );
		}
	}

	GDirect3DDevice->SetStreamSource( 0, NULL, 0, 0 );
}

/** Global helper object for (overlapping) memory copy using the GPU. */
FGPUMemMove GGPUMemMove;


#endif //USE_XeD3D_RHI
