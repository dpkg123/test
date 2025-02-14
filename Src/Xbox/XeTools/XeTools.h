/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

//#define _WIN32_DCOM
//#define INITGUID

#pragma managed(push, off)

#pragma pack(push,8)

#include <windows.h>
#include <objbase.h>
#include <assert.h>
#include <stdio.h>
#include <process.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <comutil.h>

using namespace std;

// This depends on the environment variable XEDK being setup by the XDK installer!
#include <xdk.h>
#include <xmaencoder.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <xgraphics.h>
#include <xdevkit.h>
#include <xbdm.h>

#pragma pack(pop)

#include "..\..\Engine\Inc\UnConsoleTools.h"
#include "..\..\Engine\Classes\PixelFormatEnum.uci"

template< class T > inline T Clamp( const T X, const T Min, const T Max )
{
	return X<Min ? Min : X<Max ? X : Max;
}

inline INT		appRound( FLOAT F )				{ return (INT) floorf(F + 0.5f); }

/**
 * Convert from Unreal to D3D format
 * These should mirror the GPixelFormats in XeD3DDevice.cpp
 * 
 * @param UnrealFormat - format to convert
 */
inline D3DFORMAT ConvertUnrealFormatToD3D( DWORD UnrealFormat )
{
	switch( UnrealFormat )
	{
	case PF_A8R8G8B8:
		return D3DFMT_A8R8G8B8;
	case PF_G8:
		return D3DFMT_L8;
	case PF_DXT1:
		return D3DFMT_DXT1;
	case PF_DXT3:
		return D3DFMT_DXT3;
	case PF_DXT5:
		return D3DFMT_DXT5;
	case PF_UYVY:
		return D3DFMT_UYVY;
	case PF_V8U8:
		return D3DFMT_V8U8;
	case PF_BC5:
		return D3DFMT_DXN;

	default:	
		return D3DFMT_UNKNOWN;
	}
}

/** mirrored in UnContentCookers.cpp */
enum TextureCookerCreateFlags
{
	// skip miptail packing
	TextureCreate_NoPackedMip = 1<<0,
	// convert to piecewise-linear gamma approximation
	TextureCreate_PWLGamma	  = 1<<1
};

/** Flags used for texture creation - mirrored in RHI.h */
enum ETextureCreateFlags
{
	// Texture is encoded in sRGB gamma space
	TexCreate_SRGB					= 1<<0,
	// Texture can be used as a resolve target
	TexCreate_ResolveTargetable		= 1<<1,
	// Texture is a depth stencil format that can be sampled
	TexCreate_DepthStencil			= 1<<2,
	// Texture will be created without a packed miptail
	TexCreate_NoMipTail				= 1<<3,
	// Texture will be created with an un-tiled format
	TexCreate_NoTiling				= 1<<4,
	// Texture that for a resolve target will only be written to/resolved once
	TexCreate_WriteOnce				= 1<<5,
	// Texture that may be updated every frame
	TexCreate_Dynamic				= 1<<6,
	// Texture that didn't go through the offline cooker
	TexCreate_Uncooked				= 1<<7,
};

// 5:6:5 DXT color
struct DXTColor565
{
	UINT16 B : 5;
	UINT16 G : 6;
	UINT16 R : 5;
};

struct DXTColor16
{
	union 
	{
		DXTColor565 Color565;
		UINT16 Value;
	};
};

// DXT1 storage block
struct DXTBlock
{
	DXTColor16 Color0;
	DXTColor16 Color1;
	UINT32 Indices;
};

// DXT3 and DXT5 have a 64 bit alpha block
#define DXT_ALPHA_SIZE	8

// DXT3 and DXT5 storage block
struct DXTBlock2
{
	BYTE PadAlpha[DXT_ALPHA_SIZE];
	DXTColor16 Color0;
	DXTColor16 Color1;
	UINT32 Indices;
};

/** Lookup tables for SRGB <-> Linear conversion for XBox 360/ PC */
// GBX:Zoner - Tables now do a direct remaping
extern INT SRGBToLinear_PC_Lookup_8bit[256];
extern INT SRGBToLinear_PC_Lookup_6bit[64];
extern INT SRGBToLinear_PC_Lookup_5bit[32];

/**
 * Convert 16bit color from PC gamma to  
 * Xbox 360 piecewise-linear approximated gamma
 * 
 * @param Color - color to convert
 *
 * @return - converted color
 */
inline DXTColor16 ConvertPWLGammaColor( DXTColor16 ColorIn )
{
	DXTColor16 ColorOut;

	UINT Red   = SRGBToLinear_PC_Lookup_5bit[ColorIn.Color565.R];
	UINT Green = SRGBToLinear_PC_Lookup_6bit[ColorIn.Color565.G];
	UINT Blue  = SRGBToLinear_PC_Lookup_5bit[ColorIn.Color565.B];

	ColorOut.Color565.R = Red;
	ColorOut.Color565.G = Green;
	ColorOut.Color565.B = Blue;

	return ColorOut;
}

/**
 * Convert 32bit color from PC gamma to  
 * Xbox 360 piecewise-linear approximated gamma
 * 
 * @param Color - color to convert
 *
 * @return - converted color
 */
inline D3DCOLOR ConvertPWLGammaColor( D3DCOLOR Color32 )
{
	UINT Red   = D3DCOLOR_GETRED(Color32);
	UINT Green = D3DCOLOR_GETGREEN(Color32);
	UINT Blue  = D3DCOLOR_GETBLUE(Color32);
	UINT Alpha = D3DCOLOR_GETALPHA(Color32);

	Red   = SRGBToLinear_PC_Lookup_8bit[Red];
	Green = SRGBToLinear_PC_Lookup_8bit[Green];
	Blue  = SRGBToLinear_PC_Lookup_8bit[Blue];

	return D3DCOLOR_ARGB(Alpha,Red,Green,Blue);
}

/**
 * Xenon version of sound cooker, performing XMA compression.
 */
class FXenonSoundCooker : public FConsoleSoundCooker
{
public:
	/**
	 * Constructor, initializing all member variables.
	 */
	FXenonSoundCooker( void );

	/**
	 * Destructor, cleaning up allocations.
	 */
	~FXenonSoundCooker( void );

	/**
	 * Merge 2 mono sounds into 1 stereo sound
	 */
	void* DualMonoToStereo( const BYTE* Left, const BYTE* Right, int NumSamples );

	/**
	 * Cooks the source data for the platform and stores the cooked data internally.
	 *
	 * @param	SrcBuffer		Pointer to source buffer
	 * @param	QualityInfo		All the information the compressor needs to compress the audio
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	virtual bool Cook( const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo );

	/**
	 * Cooks upto 8 mono files into a multichannel file (eg. 5.1). The front left channel is required, the rest are optional.
	 *
	 * @param	SrcBuffers		Pointers to source buffers
	 * @param	QualityInfo		All the information the compressor needs to compress the audio
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	virtual bool CookSurround( const BYTE* SrcBuffers[8], FSoundQualityInfo* QualityInfo );

	/**
	 * Returns the size of the cooked data in bytes.
	 *
	 * @return The size in bytes of the cooked data including any potential header information.
	 */
	virtual UINT GetCookedDataSize( void );

	/**
	 * Copies the cooked data into the passed in buffer of at least size GetCookedDataSize()
	 *
	 * @param CookedData		Buffer of at least GetCookedDataSize() bytes to copy data to.
	 */
	virtual void GetCookedData( BYTE* CookedData );

	/** 
	 * Decompresses the platform dependent format to raw PCM. Used for quality previewing.
	 *
	 * @param	SrcData			Uncompressed PCM data
	 * @param	DstData			Uncompressed PCM data after being compressed		
	 * @param	QualityInfo		All the information the compressor needs to compress the audio	
	 */
	virtual INT Recompress( const BYTE* SrcBuffer, BYTE* DestBuffer, FSoundQualityInfo* QualityInfo );

	/**
	 * Queries for any warning or error messages resulting from the cooking phase
	 *
	 * @return Warning or error message string, or NULL if nothing to report
	 */
	virtual const wchar_t* GetCookErrorMessages( void ) const
	{
		return( ErrorString );
	}

private:
	/** 
	 * Convert the generated WAVEFORMATEX buffer format to the used WAVEFORMATEXTENSIBLE format
	 *
	 * @param BufferFormat		Old buffer format (already byte swapped)
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	bool ConvertFormat( WAVEFORMATEX* BufferFormat );

	/** 
 	 * Byte swap the XMA2WAVEFORMATEX buffer format
	 *
	 * @param BufferFormat	Little endian buffer format
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	bool ConvertFormat( XMA2WAVEFORMATEX* BufferFormat );

	/**
	 * Cooks the source data to XMA2
	 *
	 * @param	SrcBuffer		Pointer to source buffer
	 * @param	QualityInfo		All the info required about the wave for the compressor
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	bool CookXMA2( const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo );

	/**
	 * Cooks the source data to XWMA
	 *
	 * @param	SrcBuffer		Pointer to source buffer
	 * @param	QualityInfo		All the info required about the wave for the compressor
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	bool CookXWMA( const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo );

	/**
	 * Attempts to open a temp file 
	 *
	 * @param	DestName			Where to place the created name
	 * @param	Extension			Extension of temp file
	 *
	 * @return	FILE* that isn't NULL on success, NULL on failure
	 */
	bool GetTempFile( wchar_t* DestName, const wchar_t* Extension );

	/**
	 * Attempts to open a temp file 
	 *
	 * @param	Name			Name of file
	 * @param	Options			Normally 'rb' or 'wb'
	 *
	 * @return	FILE* that isn't NULL on success, NULL on failure
	 */
	FILE* OpenFile( const wchar_t* Name, const wchar_t* Options );

	/**
	 * Runs a child process without spawning a command prompt window for each one
	 *
	 * @param CommandLine The commandline of the child process to run
	 *
	 * @return TRUE if the process was run (even if the process failed)
	 */
	bool RunChildProcess( const wchar_t* CommandLine );

	/**
	 * Writes out a wav file from source data and format with no validation or error checking
	 *
	 * @param	WaveFile		File handle of wave file
	 * @param	SrcBuffer		Pointer to source buffer
	 * @param	QualityInfo		Pointer to platform specific wave format description
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	bool WriteWaveFile( FILE* WaveFile, const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo );

	/**
	 * Reads in a wave file
	 *
	 * @param	WaveFile		File handle of wave file
	 * @param	DestBuffer		Destination memory for pcm data
	 * @param	QualityInfo		Pointer to platform specific wave format description
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	bool ReadWaveFile( FILE* WaveFile, BYTE* DestBuffer, FSoundQualityInfo* QualityInfo );

	/**
	 * Writes out a xma file from the cooked data
	 *
	 * @param	WaveFile		File to write to
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	bool WriteXMA2File( FILE* WaveFile );

	/**
	 * Writes out a xma file from the cooked data
	 *
	 * @param	WaveFile		File to write to
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	bool WriteXWMAFile( FILE* WaveFile );

	/**
	 * Reads the created xwma file
	 *
	 * @param	XWMAFile		FILE* to file
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	bool ReadXWMAFile( FILE* XWMAFile );

	/**
	 * GetHardCodedQuality
	 *
	 * @param	QualityInfo		Pointer to platform specific wave format description
	 *
	 * @return	int				Requested bit rate
	 */
	int GetHardCodedQuality( FSoundQualityInfo* QualityInfo );

	/** 
	 * Helper function used to free resources allocated by XMA encoder.
     */
	void FreeEncoderResources( void );

	/** Encoded buffer data (allocated via malloc from within XMA encoder) */
	void*				EncodedBuffer;
	/** Size in bytes of encoded buffer */
	DWORD				EncodedBufferSize;
	/** Encoded buffer format (allocated via malloc from within XMA encoder) */
	void*				EncodedBufferFormat;
	/** Size in bytes of encoded buffer format */
	DWORD				EncodedBufferFormatSize;
	/** Seek table (allocated via malloc from within XMA encoder) */
	void*				SeekTable;
	/** Size in bytes of seek table */
	DWORD				SeekTableSize;

	// Temp storage for loaded XMA2 format (plus 4 streams)
	int					NumStreams;
	XMA2WAVEFORMATEX	XMA2BufferFormat;

	// Temp storage for loaded XWMA format
	WAVEFORMATEX		XWMABufferFormat;

	/** Path to the XDK */
	wchar_t				XDKPath[MAX_PATH];

	/** Error string */
	wchar_t				ErrorString[1024];
};

struct FXenonTextureCooker : public FConsoleTextureCooker
{
	/**
	 * Associates texture parameters with cooker.
	 *
	 * @param UnrealFormat	Unreal pixel format
	 * @param Width			Width of texture (in pixels)
	 * @param Height		Height of texture (in pixels)
	 * @param NumMips		Number of miplevels
	 * @param CreateFlags	Platform-specific creation flags
	 */
	virtual void Init( DWORD UnrealFormat, UINT Width, UINT Height, UINT NumMips, DWORD CreateFlags );

	/**
	 * Returns the platform specific size of a miplevel.
	 *
	 * @param	Level		Miplevel to query size for
	 * @return	Returns	the size in bytes of Miplevel 'Level'
	 */
	UINT GetMipSize( UINT Level );

	/**
	 * Cooks the specified miplevel, and puts the result in Dst which is assumed to
	 * be of at least GetMipSize size.
	 *
	 * @param Level			Miplevel to cook
	 * @param Src			Src pointer
	 * @param Dst			Dst pointer, needs to be of at least GetMipSize size
	 * @param SrcRowPitch	Row pitch of source data
	 */
	void CookMip( UINT Level, void* Src, void* Dst, UINT SrcRowPitch );

	/**
	 * Returns the index of the first mip level that resides in the packed miptail.
	 * Should always be a multiple of the tile size for this texture's format.
	 *
	 * Some common tile sizes:
	 * DXT1 tile = 8K
	 * DXT3/5 tile = 16K
	 * X8/A8R8G8B8 tile = 4K
	 * 
	 * @return index of first level of miptail 
	 */
	INT GetMipTailBase();

	/**
	 * Cooks all mip levels that reside in the packed miptail. Dst is assumed to 
	 * be of size GetMipSize (size of the miptail level).
	 *
	 * @param Src - ptrs to mip data for each source mip level
	 * @param SrcRowPitch - array of row pitch entries for each source mip level
	 * @param Dst - ptr to miptail destination
	 */
	void CookMipTail( void** Src, UINT* SrcRowPitch, void* Dst );

private:
	/**
	 * Converts texture memory PC gamma to  
	 * Xbox 360 piecewise-linear approximated gamma
	 *
	 * @param Src - ptr to source texture data
	 * @param Dst - ptr to destination texture data (can be same as source)
	 * @param SrcRowPitch - source data size for each row of color data
	 * @param Width		  - Width of texture (in pixels)
	 * @param Height	  - Height of texture (in pixels)
	 * @param Format	  - texture format
	 */
	static void ConvertPWLGammaTexture( void* Src, void* Dst, UINT SrcRowPitch, UINT Width, UINT Height, D3DFORMAT Format );

	D3DTexture DummyTexture;
	INT _NumMips;
	BOOL PWLGamma;
};

struct FXenonSkeletalMeshCooker : public FConsoleSkeletalMeshCooker
{
	void Init(void);

	virtual void CookMeshElement(const FSkeletalMeshFragmentCookInfo& ElementInfo, FSkeletalMeshFragmentCookOutputInfo& OutInfo);
};

/**
 * Xenon version of static mesh cooker
 */
struct FXenonStaticMeshCooker : public FConsoleStaticMeshCooker
{
	void Init(void);

	/**
	 * Cooks a mesh element.
	 * @param ElementInfo - Information about the element being cooked
	 * @param OutIndices - Upon return, contains the optimized index buffer.
	 * @param OutPartitionSizes - Upon return, points to a list of partition sizes in number of triangles.
	 * @param OutNumPartitions - Upon return, contains the number of partitions.
	 * @param OutVertexIndexRemap - Upon return, points to a list of vertex indices which maps from output vertex index to input vertex index.
	 * @param OutNumVertices - Upon return, contains the number of vertices indexed by OutVertexIndexRemap.
	 */
	virtual void CookMeshElement(FMeshFragmentCookInfo& ElementInfo, FMeshFragmentCookOutputInfo& OutInfo);
};

/** xenon platform shader precompiler */
struct FXenonShaderPrecompiler : public FConsoleShaderPrecompiler
{
	/**
	* Precompile the shader with the given name. Must be implemented
	*
	* @param ShaderPath			Pathname to the source shader file ("..\Engine\Shaders\Emissive.usf")
	* @param EntryFunction		Name of the startup function ("pixelShader")
	* @param bIsVertexShader	True if the vertex shader is being precompiled
	* @param CompileFlags		Default is 0, otherwise members of the D3DXSHADER enumeration
	* @param Definitions		Space separated string that contains shader defines ("FRAGMENT_SHADER=1 VERTEX_LIGHTING=0")
	* @param bDumpingShaderPDBs	True if shader PDB's should be saved to ShaderPDBPath
	* @param ShaderPDBPath		Path to save shader PDB's, can be on the local machine if not using runtime compiling.
	* @param BytecodeBuffer		Block of memory to fill in with the compiled bytecode
	* @param BytecodeSize		Size of the returned bytecode
	* @param ConstantBuffer		String buffer to return the shader definitions with [Name,RegisterIndex,RegisterCount] ("WorldToLocal,100,4 Scale,101,1"), NULL = Error case
	* @param Errors				String buffer any output/errors
	* 
	* @return true if successful
	*/
	virtual bool PrecompileShader(
		const char* ShaderPath, 
		const char* EntryFunction, 
		bool bIsVertexShader, 
		DWORD CompileFlags,
		const char* Definitions, 
		const char* IncludeDirectory,
		char* const * IncludeFileNames,
		char* const * IncludeFileContents,
		int NumIncludes,
		bool bDumpingShaderPDBs,
		const char* ShaderPDBPath,
		unsigned char* BytecodeBuffer, 
		int& BytecodeSize, 
		char* ConstantBuffer,
		char* Errors);

private:
	/**
	 * Preprocess the shader with the given name. Must be implemented
	 *
	 * @param ShaderPath		Pathname to the source shader file ("..\Engine\Shaders\BasePassPixelShader.usf")
	 * @param Definitions		Space separated string that contains shader defines ("FRAGMENT_SHADER=1 VERTEX_LIGHTING=0")
	 * @param ShaderText		Block of memory to fill in with the preprocessed shader output
	 * @param ShaderTextSize	Size of the returned text
	 * @param Errors			String buffer any output/errors
	 * 
	 * @return true if successful
	 */
	virtual bool PreprocessShader(
		const char* ShaderPath, 
		const char* Definitions, 
		const char* IncludeDirectory,
		char* const * IncludeFileNames,
		char* const * IncludeFileContents,
		int NumIncludes,
		unsigned char* ShaderText, 
		int& ShaderTextSize, 
		char* Errors);

	/**
	 * Disassemble the shader with the given byte code. Must be implemented
	 *
	 * @param ShaderByteCode	The null terminated shader byte code to be disassembled
	 * @param ShaderText		Block of memory to fill in with the preprocessed shader output
	 * @param ShaderTextSize	Size of the returned text
	 * 
	 * @return true if successful
	 */
	virtual bool DisassembleShader(
		const DWORD *ShaderByteCode, 
		unsigned char* ShaderText, 
		int& ShaderTextSize);

	/**
	 * Create a command line to compile the shader with the given parameters. Must be implemented
	 *
	 * @param ShaderPath		Pathname to the source shader file ("..\Engine\Shaders\BasePassPixelShader.usf")
	 * @param IncludePath		Pathname to extra include directory (can be NULL)
	 * @param EntryFunction		Name of the startup function ("Main") (can be NULL)
	 * @param bIsVertexShader	True if the vertex shader is being precompiled
	 * @param CompileFlags		Default is 0, otherwise members of the D3DXSHADER enumeration
	 * @param Definitions		Space separated string that contains shader defines ("FRAGMENT_SHADER=1 VERTEX_LIGHTING=0") (can be NULL)
	 * @param CommandStr		Block of memory to fill in with the null terminated command line string
	 * 
	 * @return true if successful
	 */
	virtual bool CreateShaderCompileCommandLine(
		const char* ShaderPath, 
		const char* IncludePath, 
		const char* EntryFunction, 
		bool bIsVertexShader, 
		DWORD CompileFlags,
		const char* Definitions, 
		char* CommandStr,
		bool bPreprocessed
		);
};

#pragma managed(pop)
