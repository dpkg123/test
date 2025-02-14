/*=============================================================================
	PS3PixelShaderPatching.h: Pixel shader patching definitions shared between PPU and SPU.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PS3PIXELSHADERPATCHING_H__
#define __PS3PIXELSHADERPATCHING_H__

struct FBaseParameters
{
	QWORD TaskId;
};

struct FPixelShaderPatchingParameters
{
	UINT* CommandBufferHole;
	UINT CommandBufferSize;
	DWORD MicrocodeOut;
	FBaseParameters BaseParameters;
};

struct FPreVertexShaderCullingParameters
{
	FLOAT LocalViewProjectionMatrix[16]; // column-major order; transpose before passing to Edge!
	UINT* CommandBufferReserve;
	UINT CommandBufferReserveSize;
	UINT* OutputBufferInfo; // type is EdgeGeomOutputBufferInfo* 
	UINT IndexBufferDMAIndex;
	UINT VertexBufferDMAIndex;
	UINT VertexBufferStride;
	UINT NumTriangles;
	INT IndexBias; // <= 0 -- used to bias index buffer to 0-based
	UINT NumVertices;
	UBOOL bReverseCulling;
	FBaseParameters BaseParameters;
};

#endif
