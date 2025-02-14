/*=============================================================================
	PS3RHIResources.h: PS3 resource RHI definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if USE_PS3_RHI

#ifndef __PS3RHIRESOURCES_H__
#define __PS3RHIRESOURCES_H__


class FPS3RHIVertexDeclaration : public FPS3RHIResource
{
public:
	FPS3RHIVertexDeclaration(const FVertexDeclarationElementList& Elements)
	: VertexElements(Elements)
	{}

#if USE_ALLOCATORFIXEDSIZEFREELIST
	/** Custom new/delete */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);
#endif

//protected:
	FVertexDeclarationElementList VertexElements;
};

typedef TPS3ResourceRef<class FPS3RHIVertexDeclaration> FVertexDeclarationRHIRef;
typedef FPS3RHIVertexDeclaration* FVertexDeclarationRHIParamRef;


// This struct must match the corresponding struct in PS3Tools.cpp!
struct FVertexShaderInfo
{
	uint32_t	MicrocodeSize;		/** The size of the microcode in bytes. */
	uint32_t	NumDefaultValues;	/** Number of default values in the shader (located right after 'Microcode') */
	uint32_t	AttribMask;			/** Vertex attribute input mask */
	uint16_t	NumInstructions;	/** Number of instructions in the shader */
	uint16_t	NumRegisters;		/** Number of temporary registers used in the shader */

//	uint8_t		Microcode[MicrocodeSize];
};

// This struct must match the corresponding struct in PS3Tools.cpp and PS3\SPU\...\PixelshaderPatching.cpp!
struct FPixelShaderInfo
{
	// These are from CellCgbFragmentProgramConfiguration, to be used with cellGcmSetFragmentProgramLoad() at run-time.
	uint32_t	AttributeInputMask;	/** Bitfield for the interpolators (I guess including FACE register, etc) */
	uint32_t	FragmentControl;	/** Control flags for the pixel shaders */
	uint16_t	TexCoordsInputMask;	/** Bitfield for the texcoord interpolators */
	uint16_t	TexCoords2D;		/** Bitfield for each tex2D/tex3D (set to 0xffff) */
	uint16_t	TexCoordsCentroid;	/** Set to 0 */
	uint16_t	RegisterCount;		/** Number of temp registers used */

	uint16_t	MicrocodeSize;		/** Size of the MicroCode[], in bytes */
	uint16_t	TextureMask;		/** Bit-mask of used texture units. Bit #0 represents texture unit #0, etc. */
	uint16_t	NumSamplers;		/** Number of used samplers (size of TextureUnits[]) */
	uint16_t	NumParameters;		/** Number of pixel shader parameters (size of NumOccurrances[]) */
	uint16_t	NumParamOffsets;	/** Number of offset in total (each parameter may have multiple offsets into the microcode) */

	uint8_t		Padding[6];			/** Make sure the struct is 32 bytes, so that the microcode is 16-byte aligned. */

//	uint8_t		Microcode[MicrocodeSize];		/** 16-byte aligned */
//	uint8_t		TextureUnits[NumSamplers];
//	uint8_t		NumOccurrances[NumParameters];
//	uint16_t	Offsets[NumParamOffsets];		/** 16-byte aligned */
};

// This struct must match the corresponding struct in PS3Tools.cpp!
struct FShaderDefaultValue
{
	DWORD	ConstantRegister;
	FLOAT	DefaultValue[4];
};


// Base class for PS3 pixel and vertex shaders
class FPS3RHIShader : public FPS3RHIGPUResource
{
public:
	FPS3RHIShader( void* InMicrocode );
	virtual ~FPS3RHIShader();

	void*	Microcode;			/** Pointer to the shader microcode */
};

// Vertex shader
class FPS3RHIVertexShader : public FPS3RHIShader
{
public:
	FPS3RHIVertexShader( FVertexShaderInfo* Info, void* InMicrocode );
	FPS3RHIVertexShader();
	virtual ~FPS3RHIVertexShader();

	/** Set up the FPS3RHIVertexShader to use a raw CGprogram */
	void Setup( CGprogram Program );

	FVertexShaderInfo		Info;
	FShaderDefaultValue*	DefaultValues;		/** Shader constants (literals,etc) that are automatically set by RHI. */
	BYTE					UsedAttributes[16];	/** 1 for each vertex attribute that is referenced by the vertex shader, otherwise 0. */
};

// Pixel shader
class FPS3RHIPixelShader : public FPS3RHIShader
{
public:
	FPS3RHIPixelShader( BYTE* Buffer, DWORD BufferSize, void* InMicrocode );
	virtual ~FPS3RHIPixelShader();

	BYTE*				Buffer;				/** Block of memory that contains everything */
	DWORD				BufferSize;			/** Total size of buffer */

	FPixelShaderInfo*	Info;				/** Pointer to the info struct (within Buffer) */
	BYTE *				TextureUnits;		/** Pointer to texture unit array (within Buffer) */
	BYTE *				NumOccurrances;		/** Pointer to num-of occurrences per parameter (within Buffer) */
	WORD *				Offsets;			/** Pointer to the offsets for each parameter (within Buffer) */
};

typedef TPS3ResourceRef<FPS3RHIPixelShader> FPixelShaderRHIRef;
typedef FPS3RHIPixelShader* FPixelShaderRHIParamRef;

typedef TPS3ResourceRef<FPS3RHIVertexShader> FVertexShaderRHIRef;
typedef FPS3RHIVertexShader* FVertexShaderRHIParamRef;

// not supported by the hardware but here for easier cross platform development
typedef void FGeometryShaderRHIRef;
typedef void* FGeometryShaderRHIParamRef;
typedef void FComputeShaderRHIRef;
typedef void* FComputeShaderRHIParamRef;

/**
* Combined shader state and vertex definition for rendering geometry. 
* Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
*/
struct FBoundShaderStateRHIRef
{
	FVertexDeclarationRHIRef VertexDeclaration;
	FVertexShaderRHIRef VertexShader;
	FPixelShaderRHIRef PixelShader;

	/**
	* @return TRUE if the sate object has a valid vs resource 
	*/
	friend UBOOL IsValidRef(const FBoundShaderStateRHIRef& Ref)
	{
		return IsValidRef(Ref.VertexShader);
	}

	void SafeRelease()
	{
		VertexDeclaration = NULL;
		VertexShader = NULL;
		PixelShader = NULL;
	}

	/**
	* Equality is based on vertex decl, vertex shader and pixel shader
	* @param Other - instance to compare against
	* @return TRUE if equal
	*/
	UBOOL operator==(const FBoundShaderStateRHIRef& Other) const
	{
		return (VertexDeclaration == Other.VertexDeclaration && VertexShader == Other.VertexShader && PixelShader == Other.PixelShader);
	}
};

typedef const FBoundShaderStateRHIRef& FBoundShaderStateRHIParamRef;

// Vertex buffer
class FPS3RHIVertexBuffer : public FPS3RHIDoubleBufferedGPUResource
{
public:
	/**
	 * Constructor
	 */
	FPS3RHIVertexBuffer(DWORD InSize, DWORD UsageFlags);

	/**
	 * Constructor for resource array (in place data)
	 */
	FPS3RHIVertexBuffer(FResourceArrayInterface* ResourceArray);

#if USE_ALLOCATORFIXEDSIZEFREELIST
	/** Custom new/delete */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);
#endif
};

typedef TPS3ResourceRef<FPS3RHIVertexBuffer> FVertexBufferRHIRef;
typedef FPS3RHIVertexBuffer* FVertexBufferRHIParamRef;


// Index buffer
class FPS3RHIIndexBuffer : public FPS3RHIDoubleBufferedGPUResource
{
public:

	/**
	 * Constructor
	 */
	FPS3RHIIndexBuffer(DWORD InSize, DWORD InStride, DWORD UsageFlags,DWORD InNumInstances);

	/**
	 * Constructor for resource array (in place data)
	 */
	FPS3RHIIndexBuffer(FResourceArrayInterface* ResourceArray, DWORD InStride,DWORD InNumInstances);

#if USE_ALLOCATORFIXEDSIZEFREELIST
	/** Custom new/delete */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);
#endif

	/** The number of instances of the base indices contained by this buffer. */
	DWORD NumInstances;

	/* 16 or 32-bit index data */
	BYTE DataType;
};

typedef TPS3ResourceRef<FPS3RHIIndexBuffer> FIndexBufferRHIRef;
typedef FPS3RHIIndexBuffer* FIndexBufferRHIParamRef;



class FPS3RHISamplerState : public FPS3RHIResource
{
public:
	BYTE AddressU;
	BYTE AddressV;
	BYTE AddressW;
	BYTE MinFilter;
	BYTE MagFilter;
	BYTE Anisotropic;

#if USE_ALLOCATORFIXEDSIZEFREELIST
	/** Custom new/delete */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);
#endif
};

typedef TPS3ResourceRef<FPS3RHISamplerState> FSamplerStateRHIRef;
typedef FPS3RHISamplerState* FSamplerStateRHIParamRef;

class FPS3RHIBlendState : public FPS3RHIResource
{
public:
	UBOOL	bAlphaBlendEnable;
	WORD	ColorBlendOperation;
	WORD	ColorSourceBlendFactor;
	WORD	ColorDestBlendFactor;
	UBOOL	bSeparateAlphaBlendEnable;
	WORD	AlphaBlendOperation;
	WORD	AlphaSourceBlendFactor;
	WORD	AlphaDestBlendFactor;
	UBOOL	bAlphaTestEnable;
	WORD	AlphaFunc;
	BYTE	AlphaRef;
};

typedef TPS3ResourceRef<FPS3RHIBlendState> FBlendStateRHIRef;
typedef FPS3RHIBlendState* FBlendStateRHIParamRef;

class FPS3RHIDepthState : public FPS3RHIResource
{
public:
	UBOOL bEnableDepthTest;
	UBOOL bEnableDepthWrite;
	WORD DepthTestFunc;
};

typedef TPS3ResourceRef<FPS3RHIDepthState> FDepthStateRHIRef;
typedef FPS3RHIDepthState* FDepthStateRHIParamRef;

class FPS3RHIStencilState : public FPS3RHIResource
{
public:
	UBOOL	bStencilEnable;
	UBOOL	bTwoSidedStencilMode;
	WORD	FrontFaceStencilFunc;
	WORD	FrontFaceStencilFailStencilOp;
	WORD	FrontFaceDepthFailStencilOp;
	WORD	FrontFacePassStencilOp;
	WORD	BackFaceStencilFunc;
	WORD	BackFaceStencilFailStencilOp;
	WORD	BackFaceDepthFailStencilOp;
	WORD	BackFacePassStencilOp;
	DWORD	StencilReadMask;
	DWORD	StencilWriteMask;
	DWORD	StencilRef;
};

typedef TPS3ResourceRef<FPS3RHIStencilState> FStencilStateRHIRef;
typedef FPS3RHIStencilState* FStencilStateRHIParamRef;


class FPS3RHIRasterizerState : public FPS3RHIResource
{
public:
	WORD	FillMode;
	UBOOL	bCullEnable;
	WORD	FrontFace;
	FLOAT	DepthBias;
	FLOAT	SlopeScaleDepthBias;
};

typedef TPS3ResourceRef<FPS3RHIRasterizerState> FRasterizerStateRHIRef;
typedef FPS3RHIRasterizerState* FRasterizerStateRHIParamRef;


struct FPS3OcclusionBucket;

class FPS3RHIOcclusionQuery : public FPS3RHIResource
{
public:
	FPS3RHIOcclusionQuery();
	virtual ~FPS3RHIOcclusionQuery();

	FPS3OcclusionBucket*	Bucket;	/** Which bucket has the result (in host memory) of this query */
	INT						Index;	/** Index within the Bucket */

	DWORD					NumPixels;	/** When Bucket is NULL, the result is stored in this variable. */
};

typedef TPS3ResourceRef<FPS3RHIOcclusionQuery> FOcclusionQueryRHIRef;
typedef FPS3RHIOcclusionQuery* FOcclusionQueryRHIParamRef;


extern void PS3StartupOcclusionManager();
extern void PS3ShutdownOcclusionManager();
extern void PS3FlushOcclusionManager();


// GEMINI_TODO - implement shared PS3 resource
class FPS3RHISharedMemoryResource : public FPS3RHIResource
{
};
typedef TPS3ResourceRef<FPS3RHISharedMemoryResource> FSharedMemoryResourceRHIRef;
typedef FPS3RHISharedMemoryResource* FSharedMemoryResourceRHIParamRef;


#endif // __PS3RHIRESOURCES_H__

#endif // USE_PS3_RHI
