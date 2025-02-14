/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <assert.h>
#include <Mmreg.h>

#include <cgc.h>
#include <cgutil.h>

#pragma warning(disable : 4311)
#pragma warning(disable : 4312)

#include "..\..\Engine\Inc\UnConsoleTools.h"
#include "..\..\Engine\Classes\PixelFormatEnum.uci"

/**
 * Runs a child process without spawning a command prompt window for each one
 *
 * @param CommandLine THe commandline of the child process to run
 * @param Errors Output buffer for any errors
 * @param ErrorsSize Size of the Errors buffer
 * @param ProcessReturnValue Optional out value that the process returned
 *
 * @return TRUE if the process was run (even if the process failed)
 */
bool RunChildProcess(const TCHAR* CommandLine, TCHAR* Errors, int ErrorsSize, DWORD* ProcessReturnValue=NULL);


/**
 * PS3 version of texture cooker
 */
struct FPS3TextureCooker : public FConsoleTextureCooker
{
	// FConsoleTextureCooker interface
	virtual void	Init( DWORD UnrealFormat, UINT Width, UINT Height, UINT NumMips, DWORD CreateFlags );
	virtual UINT	GetMipSize( UINT Level );
	virtual void	CookMip( UINT Level, void* Src, void* Dst, UINT SrcRowPitch );

	// Helper functions
	void			SwizzleTexture(BYTE* Src, BYTE* Dst, DWORD Width, DWORD Height, DWORD BytesPerPixel);
	UINT			GetPitch( DWORD MipIndex );
	UINT			GetNumRows( DWORD MipIndex );
	/**
	 *	Returns the offset for the pixel at coordinate (X,Y) relative the base address of a swizzled texture mipmap.
	 *	In a square texture, the bits in the X and Y coordinates are simply interleaved to create the offset.
	 *	In a rectangular texture, the shared lower bits are interleaved and the remainder bits are packed together on the left.
	 *
	 *	@param Width	Width of the mipmap in pixels
	 *	@param Height	Height of the mipmap in pixels
	 *	@param X		X-coordinate (U) in pixels
	 *	@param Y		Y-coordinate (V) in pixels
	 *	@return			Offset from the base address where this pixel should be stored (in pixels)
	 */
	DWORD			GetSwizzleOffset( DWORD Width, DWORD Height, DWORD U, DWORD V );

	/** Helper struct with information about a texture format. */
	struct FPixelFormat
	{
		FPixelFormat(UINT InBlockSizeX, UINT InBlockSizeY, UINT InBlockSizeZ, UINT InBlockBytes, bool bInNeedToSwizzle)
		:	BlockSizeX(InBlockSizeX), BlockSizeY(InBlockSizeY), BlockSizeZ(InBlockSizeZ), BlockBytes(InBlockBytes), NeedToSwizzle(bInNeedToSwizzle)
		{
		}
		FPixelFormat()
		:	BlockSizeX(0), BlockSizeY(0), BlockSizeZ(0), BlockBytes(0), NeedToSwizzle(false)
		{
		}
		UINT BlockSizeX, BlockSizeY, BlockSizeZ, BlockBytes;
		bool NeedToSwizzle;
	};

	/**
	 * Fills in the specified FPixelFormat struct for a texture format.
	 *
	 * @param	UnrealFormat	Texture format as a EPixelFormat
	 * @param	OutPixelFormat	[out] Struct that will be filled in upon return
	 * @return	TRUE if the specified texture format is supported by the platform
	 */
	static bool		GetPixelFormat( DWORD UnrealFormat, FPixelFormat& OutPixelFormat );

	// Unused functions
	virtual INT		GetMipTailBase() { return 0xFF; }
	virtual void	CookMipTail( void** /*Src*/, UINT* /*SrcRowPitch*/, void* /*Dst*/ ) {}

private:
	UINT			SavedWidth;
	UINT			SavedHeight;
	DWORD			SavedFormat;
	DWORD			BitMask;
	DWORD			BitShift;
	FPixelFormat	PixelFormat;
};


/**
 * PS3 version of sound cooker, performing compression.
 */
class FPS3SoundCooker : public FConsoleSoundCooker
{
public:
	/**
	 * Constructor, initializing all member variables.
	 */
	FPS3SoundCooker( void );

	/**
	 * Destructor, cleaning up allocations.
	 */
	~FPS3SoundCooker( void );

	/**
	 * Clean up allocations and temp files
	 */
	void CleanUp();

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
	/** Potentially adjusts the encoding quality based on UT3 sound asset name */
	void			GetHardCodedQuality( WAVEFORMATEXTENSIBLE* Format, const wchar_t* Name, int Quality, int& VBRQuality, int& MaxBitrate );

	bool			InitCook( TCHAR Path[MAX_PATH], WAVEFORMATEXTENSIBLE* Format, FSoundQualityInfo *QualityInfo );
	bool			WriteStereoWaveFile( FILE* WaveFile, const BYTE* SrcBuffer0, const BYTE* SrcBuffer1, DWORD SrcBufferSize, void* WaveFormat );
	bool			WriteWaveFile( FILE* WaveFile, const BYTE* SrcBuffer, DWORD SrcBufferSize, void* WaveFormat );
	FILE*			GetTempFile( const TCHAR* Name );
	bool			ReadPS3File( const TCHAR* Filename, int Slot );

	/** Encoded buffer data (allocated via malloc from within PS3 encoder) */
	void*			PS3RawData[4];
	/** Size in bytes of encoded buffer */
	int				PS3RawDataSize[4];

	/** Path to .wav file that is passed to the sound encoder */
	TCHAR			InputFile[MAX_PATH];

	/** Path the sound encoder writes out to */
	TCHAR			OutputFile[MAX_PATH];

	/** Path to .wav file that is passed to the sound encoder */
	TCHAR			InputSurroundFormat[MAX_PATH];

	/** Path the sound encoder writes out to */
	TCHAR			OutputSurroundFormat[MAX_PATH];

	/** Error string */
	TCHAR			ErrorString[1024];
};

/**
 * PS3 version of skeletal mesh cooker
 */
struct FPS3SkeletalMeshCooker : public FConsoleSkeletalMeshCooker
{
	virtual void Init(void);

	virtual void CookMeshElement(
		const FSkeletalMeshFragmentCookInfo& ElementInfo,
		FSkeletalMeshFragmentCookOutputInfo& OutInfo
		);
};

/**
 * PS3 version of static mesh cooker
 */
struct FPS3StaticMeshCooker : public FConsoleStaticMeshCooker
{
	virtual void Init(void);

	virtual void CookMeshElement(
		FMeshFragmentCookInfo& ElementInfo,
		FMeshFragmentCookOutputInfo& OutInfo
		);
};

/** PS3 version of shader precompiler. */
struct FPS3ShaderPrecompiler : public FConsoleShaderPrecompiler
{
public:
	HMODULE	LibCgcDLL;
	CGCcontext* cgcContext;
	CGCbin* cgcBinaryBin;
	CGCbin* cgcMessageBin;

	FPS3ShaderPrecompiler();
	~FPS3ShaderPrecompiler();
	virtual bool PrecompileShader(
		const char* ShaderPath, 
		const char* EntryFunction, 
		bool bIsVertexShader, 
		DWORD /*CompileFlags*/, 
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
	* @param CompileFlags		Default is 0
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
