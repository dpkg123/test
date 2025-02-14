/*=============================================================================
	PS3RHIShaders.cpp: PS3 RHI shaders implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3RHIPrivate.h"

#if USE_PS3_RHI

#define CT_ASSERT(X) extern char CompileTimeAssert[ (X) ? 1 : -1 ]

// Pull the shader cache stats into this context
STAT_MAKE_AVAILABLE_FAST(STAT_ShaderCompression_CompressedShaderMemory);
STAT_MAKE_AVAILABLE_FAST(STAT_ShaderCompression_UncompressedShaderMemory);


FPS3RHIShader::FPS3RHIShader(void* InMicrocode)
:	Microcode(InMicrocode)
{
}

FPS3RHIShader::~FPS3RHIShader()
{
}

FPS3RHIVertexShader::FPS3RHIVertexShader( )
:	FPS3RHIShader(NULL)
{
	appMemzero( &Info, sizeof(Info) );
	appMemzero( UsedAttributes, sizeof(UsedAttributes) );
	DefaultValues = NULL;
}

FPS3RHIVertexShader::FPS3RHIVertexShader( FVertexShaderInfo* InInfo, void* InMicrocode )
:	FPS3RHIShader( InMicrocode )
{
	Info = *InInfo;

	// Setup helper pointer.
	DefaultValues = (FShaderDefaultValue*) (((BYTE*)Microcode) + Info.MicrocodeSize);

	DWORD AttribMask = Info.AttribMask;
	for ( INT AttributeIndex=0; AttributeIndex < 16; ++AttributeIndex )
	{
		UsedAttributes[AttributeIndex] = (AttribMask & 0x01);
		AttribMask >>= 1;
	}
}

FPS3RHIVertexShader::~FPS3RHIVertexShader()
{
	// DefaultValues will be NULL only if we created this struct through a raw CGprogram,
	// in which case the memory is handled outside this struct.
	if ( DefaultValues != NULL )
	{
		DEC_DWORD_STAT_BY(STAT_VertexShaderMemory, Info.MicrocodeSize);
		DEC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, Info.MicrocodeSize);
		appFree( Microcode );
	}
}

void FPS3RHIVertexShader::Setup( CGprogram Program )
{
	cellGcmCgInitProgram( Program );
	cellGcmCgGetUCode( Program, &Microcode, &Info.MicrocodeSize );
	CellCgbVertexProgramConfiguration Config;
	cellGcmCgGetCgbVertexProgramConfiguration( Program, &Config );
	Info.AttribMask = Config.attributeInputMask;
	Info.NumInstructions = Config.instructionCount;
	Info.NumRegisters = Config.registerCount;

	DWORD AttribMask = Info.AttribMask;
	for ( INT AttributeIndex=0; AttributeIndex < 16; ++AttributeIndex )
	{
		UsedAttributes[AttributeIndex] = (AttribMask & 0x01);
		AttribMask >>= 1;
	}
}

FPS3RHIPixelShader::FPS3RHIPixelShader( BYTE* InBuffer, DWORD InBufferSize, void* InMicrocode )
:	FPS3RHIShader( InMicrocode )
{
	Buffer		= InBuffer;
	BufferSize	= InBufferSize;

	// Setup helper pointers.
	Info				= (FPixelShaderInfo*) Buffer;
	DWORD Offset		= sizeof(FPixelShaderInfo) + Info->MicrocodeSize;
	TextureUnits		= (BYTE*) (Buffer + Offset);
	Offset				+= Info->NumSamplers*sizeof(BYTE);
	NumOccurrances		= (BYTE*) (Buffer + Offset);
	Offset				= Align(Offset + Info->NumParameters*sizeof(BYTE), 16);
	Offsets				= (WORD*) (Buffer + Offset);
}

FPS3RHIPixelShader::~FPS3RHIPixelShader()
{
	DEC_DWORD_STAT_BY(STAT_PixelShaderMemory, BufferSize);
	DEC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, BufferSize);
	appFree(Buffer);
}


FPixelShaderRHIRef RHICreatePixelShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	BYTE *Data				= (BYTE*) Code.GetData();
	FPixelShaderInfo* Info	= (FPixelShaderInfo*) Data;
	DWORD TotalSize			= sizeof(FPixelShaderInfo) + Info->MicrocodeSize;
	TotalSize				+= Info->NumSamplers*sizeof(BYTE) + Info->NumParameters*sizeof(BYTE);
	TotalSize				= Align(TotalSize, 16) + Info->NumParamOffsets*sizeof(WORD);
	check( TotalSize == Code.Num() );

	// Allocate system memory for the whole block
	BYTE* TotalBuffer = (BYTE*) appMalloc(TotalSize, 128);
	appMemcpy(TotalBuffer, Data, TotalSize);
	INC_DWORD_STAT_BY(STAT_PixelShaderMemory, TotalSize);
	INC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, TotalSize);

	void* Microcode = TotalBuffer + sizeof(FPixelShaderInfo);
	return new FPS3RHIPixelShader(TotalBuffer, TotalSize, Microcode);
}

FVertexShaderRHIRef RHICreateVertexShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	BYTE *Data				= (BYTE*) Code.GetData();
	FVertexShaderInfo* Info	= (FVertexShaderInfo*) Data;
	void* InMicrocode		= Data + sizeof(FVertexShaderInfo);

	// Allocate host memory for the microcode and the default values following it, and copy the whole block.
	void *Microcode = appMalloc( Info->MicrocodeSize + Info->NumDefaultValues*sizeof(FShaderDefaultValue) );
	appMemcpy( Microcode, InMicrocode, Info->MicrocodeSize + Info->NumDefaultValues*sizeof(FShaderDefaultValue) );
	INC_DWORD_STAT_BY(STAT_VertexShaderMemory, Info->MicrocodeSize);
	INC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, Info->MicrocodeSize);

	return new FPS3RHIVertexShader( Info, Microcode );
}

/**
 * Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
 * @param VertexDeclaration - existing vertex decl
 * @param StreamStrides - optional stream strides
 * @param VertexShader - existing vertex shader
 * @param PixelShader - existing pixel shader
 */
FBoundShaderStateRHIRef RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclaration, 
	DWORD *StreamStrides, 
	FVertexShaderRHIParamRef VertexShader, 
	FPixelShaderRHIParamRef PixelShader )
{
	FBoundShaderStateRHIRef BoundShaderState;
	BoundShaderState.VertexDeclaration = VertexDeclaration;
	BoundShaderState.VertexShader = VertexShader;
	BoundShaderState.PixelShader = PixelShader;
	return BoundShaderState;
}


#endif // USE_PS3_RHI
