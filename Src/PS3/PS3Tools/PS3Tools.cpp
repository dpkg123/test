/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#include "PS3Tools.h"
#include <locale.h>

#include <io.h>

#pragma warning( disable: 4189 )	//warning C4189: 'ident' : local variable is initialized but not referenced

typedef unsigned short uint16_t;
#define ALIGN( X, Y )	(((X) + (Y) - 1) & ~((Y) - 1))

inline bool IsPowerOfTwo( DWORD X )
{
	return ((X & (X - 1)) == 0);
}

inline DWORD FloorLog2( DWORD Value )
{
	DWORD Index = 0;
	_BitScanReverse( &Index, Value );
	return Index;
}

/** Interleaves the bits from A and B, in the form A[15]B[15]A[14]B[14]...A[1]B[1]A[0]B[0] */
inline DWORD InterleaveBits( DWORD A, DWORD B )
{
	A &= 0xffff;
	A = (A | (A << 8)) & 0x00ff00ff;
	A = (A | (A << 4)) & 0x0f0f0f0f;
	A = (A | (A << 2)) & 0x33333333;
	A = (A | (A << 1)) & 0x55555555;

	B &= 0xffff;
	B = (B | (B << 8)) & 0x00ff00ff;
	B = (B | (B << 4)) & 0x0f0f0f0f;
	B = (B | (B << 2)) & 0x33333333;
	B = (B | (B << 1)) & 0x55555555;

	return (A << 1) | B;
}

int CGtype2Size[][2] =
{
	#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_name, ncols, nrows, pc)	{ enum_name, ncols },
	#include <Cg/cg_datatypes.h>
};

int GetCGTypeSize( CGtype Type )
{
	const int NumCgTypes = sizeof(CGtype2Size)/sizeof(CGtype2Size[0]);
	int Index = int(Type) - CGtype2Size[0][0];
	if ( Index < 0 || Index >= NumCgTypes || CGtype2Size[Index][0] != Type )
		return 0;
	return (CGtype2Size[Index][1] > 0) ? CGtype2Size[Index][1] : 1;
}

// This struct must match the one in PS3RHIResources.h!
struct FVertexShaderInfo
{
	uint32_t	MicrocodeSize;
	uint32_t	NumDefaultValues;
	uint32_t	AttribMask;
	uint16_t	NumInstructions;
	uint16_t	NumRegisters;
};

// This struct must match the one in PS3RHIResources.h and PS3\SPU\...\PixelShaderPatching.cpp!
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

struct FShaderDefaultValue
{
	uint32_t	ConstantRegister;
	float		DefaultValue[4];
};

#define CT_ASSERT(X) extern char CompileTimeAssert[ (X) ? 1 : -1 ]

template <typename T>
T BYTESWAP( T Value )
{
	CT_ASSERT( sizeof(T) == 4 );
	DWORD &V = *((DWORD*) &Value);
	V = (((V)&0xff000000) >> 24) | (((V)&0x00ff0000) >> 8) | (((V)&0x0000ff00) << 8) | (((V)&0x000000ff) << 24);
	return *((T*) &V);
}
template <typename T>
T BYTESWAP16( T Value )
{
	CT_ASSERT( sizeof(T) == 2 );
	unsigned short &V = *((unsigned short*) &Value);
	V = (((V)&0xff00) >> 8) | (((V)&0x00ff) << 8);
	return *((T*) &V);
}

#ifndef PS3MOD
BOOL APIENTRY DllMain( HANDLE /*hModule*/, DWORD ul_reason_for_call, LPVOID /*lpReserved*/ )
{
	// what is the reason this function is being called?
	switch( ul_reason_for_call ) 
	{
		// check for missing DLLs
		case DLL_PROCESS_ATTACH:
			{
				// make sure the PS3TMAPI.dll is there, without loading it
				TCHAR Path[MAX_PATH];
				bool bFoundDLL = false;
				if (GetEnvironmentVariable(L"SN_PS3_PATH", Path, MAX_PATH - 20) != 0)
				{
#if _WIN64
					wcscat_s(Path, MAX_PATH, L"\\bin\\PS3TMAPIx64.DLL");
#else
					wcscat_s(Path, MAX_PATH, L"\\bin\\PS3TMAPI.DLL");
#endif
					bFoundDLL = (GetFileAttributes(Path) != INVALID_FILE_ATTRIBUTES);
				}
				if ( !bFoundDLL )
				{
					OutputDebugString( L"We were unable to to find PS3TMAPI.DLL via the SN_PS3_PATH env var!  Please make certain SN_PS3_PATH is valid.\n" );
					return FALSE;
				}
			}			
			break;

		// do nothing here
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}
#endif

/**
 * Runs a child process without spawning a command prompt window for each one
 *
 * @param CommandLine The commandline of the child process to run
 * @param Errors Output buffer for any errors
 * @param ErrorsSize Size of the Errors buffer
 * @param ProcessReturnValue Optional out value that the process returned
 *
 * @return TRUE if the process was run (even if the process failed)
 */
bool RunChildProcess(const TCHAR* CommandLine, TCHAR* Errors, int ErrorsSize, DWORD* ProcessReturnValue)
{
	// run the command (and avoid a window popping up)
	SECURITY_ATTRIBUTES SecurityAttr; 
	SecurityAttr.nLength = sizeof(SecurityAttr); 
	SecurityAttr.bInheritHandle = TRUE; 
	SecurityAttr.lpSecurityDescriptor = NULL; 

	HANDLE StdOutRead, StdOutWrite;
	CreatePipe(&StdOutRead, &StdOutWrite, &SecurityAttr, 0);
	SetHandleInformation(StdOutRead, HANDLE_FLAG_INHERIT, 0);

	// set up process spawn structures
	STARTUPINFO StartupInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	StartupInfo.hStdOutput = StdOutWrite;
	StartupInfo.hStdError = StdOutWrite;
	PROCESS_INFORMATION ProcInfo;

	// kick off the child process
	if (!CreateProcess(NULL, (LPTSTR)CommandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &StartupInfo, &ProcInfo))
	{
		wsprintf(Errors, L"\nFailed to start process '%s'\n", CommandLine);
		return false;
	}

	bool bProcessComplete = false;
	TCHAR Buffer[1024 * 64];
	DWORD BufferSize = sizeof(Buffer);
	Errors[0] = 0;

//		FString CompileOutput;
	// wait until the precompiler is finished
	while (!bProcessComplete)
	{
		DWORD Reason = WaitForSingleObject(ProcInfo.hProcess, 0);

		// read up to 64k of error (that would be crazy amount of error, but whatever)
		DWORD SizeToRead;
		// See if we have some data to read
		PeekNamedPipe(StdOutRead, NULL, 0, NULL, &SizeToRead, NULL);
		while (SizeToRead > 0)
		{
			// read some output
			DWORD SizeRead;
			ReadFile(StdOutRead, &Buffer, min(SizeToRead, BufferSize - 1), &SizeRead, NULL);
			Buffer[SizeRead] = 0;

			// decrease how much we need to read
			SizeToRead -= SizeRead;

			// append the output to the 
			wcsncpy_s(Errors, ErrorsSize, Buffer, ErrorsSize - 1);
		}

		// when the process signals, its done
		if (Reason == WAIT_OBJECT_0)
		{
			// berak out of the loop
			bProcessComplete = true;
		}
	}

	// Get the return value
	DWORD ErrorCode;
	GetExitCodeProcess(ProcInfo.hProcess, &ErrorCode);

	// pass back the return code if desired
	if (ProcessReturnValue)
	{
		*ProcessReturnValue = ErrorCode;
	}

	// Close process and thread handles. 
	CloseHandle(ProcInfo.hProcess);
	CloseHandle(ProcInfo.hThread);
	CloseHandle(StdOutRead);
	CloseHandle(StdOutWrite);

	return true;
}


/**
 * Retrieves the location of the PS3 directory
 *
 * @param Path Buffer to fill out with the path data. Should be MAX_PATH big
 *
 * @return true if the function found the environment variable
 */
bool GetPS3Dir(TCHAR* Path)
{
	if (GetEnvironmentVariable(L"SCE_PS3_ROOT", Path, MAX_PATH - 40) != 0)
	{
		// Replace any forward slashes with backslashes
		TCHAR* C = Path;
		while (*C)
		{
			if (*C == '/')
			{
				*C = '\\';
			}
			if (C[0] == '\\' && C[1] == 0)	// Remove trailing backslash
			{
				*C = 0;
			}
			C++;
		}

		return true;
	}

	return false;
}

/**
 * Fills in the specified FPixelFormat struct for a texture format.
 *
 * @param	UnrealFormat	Texture format as a EPixelFormat
 * @param	OutPixelFormat	[out] Struct that will be filled in upon return
 * @return	TRUE if the specified texture format is supported by the platform
 */
bool FPS3TextureCooker::GetPixelFormat( DWORD UnrealFormat, FPixelFormat& OutPixelFormat )
{
	switch ( UnrealFormat )
	{
		case PF_A32B32G32R32F:
			OutPixelFormat = FPixelFormat(1,	1,	1,	16,	false);
			return true;
		case PF_A8R8G8B8:
			OutPixelFormat = FPixelFormat(1,	1,	1,	4,	true);
			return true;
		case PF_G8:
			OutPixelFormat = FPixelFormat(1,	1,	1,	1,	true);
			return true;
		case PF_G16:
			OutPixelFormat = FPixelFormat(1,	1,	1,	2,	false);
			return true;
		case PF_DXT1:
			OutPixelFormat = FPixelFormat(4,	4,	1,	8,	false);
			return true;
		case PF_DXT3:
			OutPixelFormat = FPixelFormat(4,	4,	1,	16,	false);
			return true;
		case PF_DXT5:
			OutPixelFormat = FPixelFormat(4,	4,	1,	16,	false);
			return true;
		case PF_UYVY:
			OutPixelFormat = FPixelFormat(2,	1,	1,	4,	false);
			return true;
		case PF_V8U8:
			OutPixelFormat = FPixelFormat(1,	1,	1,	2,	true);
			return true;
		default:	// PF_Unknown
			OutPixelFormat = FPixelFormat(1,	1,	1,	0,	false);
			return false;
	}
}

/**
 * Fills in the specified FPixelFormat struct for a texture format.
 *
 * @param	UnrealFormat	Texture format as a EPixelFormat
 * @param	OutPixelFormat	[out] Struct that will be filled in upon return
 * @return	TRUE if the specified UnrealFormat is supported by the platform
 */
void FPS3TextureCooker::Init( DWORD UnrealFormat, UINT Width, UINT Height, UINT /*NumMips*/, DWORD /*CreateFlags*/ )
{
	SavedFormat	= UnrealFormat;
	SavedWidth	= Width;
	SavedHeight	= Height;
	BitMask		= 0;
	BitShift	= 0;
	GetPixelFormat(UnrealFormat, PixelFormat);
}
UINT FPS3TextureCooker::GetPitch( DWORD MipIndex )
{
	// NOTE: We don't add any padding in cooked data

	// calculate by blocks, for things like DXT
	UINT MipSizeX = max( SavedWidth >> MipIndex, PixelFormat.BlockSizeX );
	UINT Pitch = (MipSizeX / PixelFormat.BlockSizeX) * PixelFormat.BlockBytes;
	return Pitch;
}
UINT FPS3TextureCooker::GetNumRows( DWORD MipIndex )
{
	UINT MipSizeY = max(SavedHeight >> MipIndex, PixelFormat.BlockSizeY) / PixelFormat.BlockSizeY;
	return MipSizeY;
}
UINT FPS3TextureCooker::GetMipSize( UINT Level ) 
{
	UINT MipSizeX = GetPitch( Level );
	UINT MipSizeY = GetNumRows( Level );
	UINT TotalSize = MipSizeX * MipSizeY;
	return TotalSize;
}

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
DWORD FPS3TextureCooker::GetSwizzleOffset( DWORD Width, DWORD Height, DWORD X, DWORD Y )
{
	DWORD Index;
	if ( Width == Height )
	{
		Index = InterleaveBits( Y, X );
	}
	else if ( Width > Height )
	{
		DWORD PackedBits = X & ~BitMask;
		DWORD InterleavedBits = InterleaveBits(Y, X & BitMask);
		Index = (PackedBits << BitShift) | InterleavedBits;
	}
	else
	{
		DWORD PackedBits = Y & ~BitMask;
		DWORD InterleavedBits = InterleaveBits(Y & BitMask, X);
		Index = (PackedBits << BitShift) | InterleavedBits;
	}
	return Index;
}
void FPS3TextureCooker::SwizzleTexture(BYTE* Src, BYTE* Dst, DWORD Width, DWORD Height, DWORD BytesPerPixel)
{
	DWORD NumBitsWidth = FloorLog2(Width);
	DWORD NumBitsHeight = FloorLog2(Height);
	if ( Width > Height )
	{
		// Interleave shared bits to the right, pack the leftovers to the left
		DWORD NumInterleavedBits = NumBitsHeight;
		BitShift = NumInterleavedBits;				// Amount to shift left to add the packedbits to the interleaved bits
		BitMask = (1 << NumInterleavedBits) - 1;	// Mask for the right-most (interleaved) bits in X
	}
	else if ( Height > Width )
	{
		// Interleave shared bits to the right, pack the leftovers to the left
		DWORD NumInterleavedBits = NumBitsWidth;
		BitShift = NumInterleavedBits;				// Amount to shift left to add the packedbits to the interleaved bits
		BitMask = (1 << NumInterleavedBits) - 1;	// Mask for the right-most (interleaved) bits in Y
	}

	for ( DWORD Y = 0; Y < Height; ++Y )
	{
		for ( DWORD X = 0; X < Width; ++X )
		{
			DWORD SrcTexelOffset = Y*Width + X;
			DWORD DstTexelOffset = GetSwizzleOffset( Width, Height, X, Y );
			memcpy( Dst + DstTexelOffset*BytesPerPixel, Src + SrcTexelOffset*BytesPerPixel, BytesPerPixel );
		}
	}
}
void FPS3TextureCooker::CookMip( UINT Level, void* Src, void* Dst, UINT /*SrcRowPitch*/ )
{
	if( SavedFormat == PF_V8U8 )
	{
		// Convert from two's compliment to regular normal representation.
		DWORD MipWidth = max( SavedWidth >> Level, 1 );
		DWORD MipHeight = max( SavedHeight >> Level, 1 );
		char* SignedSrc = (char*)Src;
		BYTE* NewSrc = new BYTE[MipWidth*MipHeight*2];
		BYTE* UnsignedNewSrc = NewSrc;
		for ( DWORD Y = 0; Y < MipHeight; ++Y )
		{
			for ( DWORD X = 0; X < MipWidth; ++X )
			{
				*(UnsignedNewSrc++) = (INT)(*(SignedSrc++)) + 128;
				*(UnsignedNewSrc++) = (INT)(*(SignedSrc++)) + 128;
			}
		}
		Src = NewSrc;
	}

	if ( IsPowerOfTwo(SavedWidth) && IsPowerOfTwo(SavedHeight) && PixelFormat.NeedToSwizzle )
	{
		DWORD MipWidth = max( SavedWidth >> Level, 1 );
		DWORD MipHeight = max( SavedHeight >> Level, 1 );
		SwizzleTexture( (BYTE*)Src, (BYTE*)Dst, MipWidth, MipHeight, PixelFormat.BlockBytes );
	}
	else
	{
		memcpy( Dst, Src, GetMipSize(Level) );
	}

	// Free temp memory
	if( SavedFormat == PF_V8U8 )
	{
		delete[] Src;
	}
}

/**
 * Constructor, initializing all member variables.
 */
FPS3SoundCooker::FPS3SoundCooker( void )
{
	int	i;

	ErrorString[0] = 0;

	for( i = 0; i < 4; i++ )
	{
		PS3RawData[i] = NULL;
		PS3RawDataSize[i] = 0;
	}

	TCHAR TempDir[MAX_PATH];
	GetTempPath( MAX_PATH - 1, TempDir );
	TCHAR TempFile[MAX_PATH];
	GUID Guid;
	CoCreateGuid(&Guid);
	wsprintf( TempFile, L"%s\\%08x%04x%04x%08x%08x", TempDir,Guid.Data1,Guid.Data2,Guid.Data3,*(DWORD *)(&Guid.Data4[0]),*(DWORD *)(&Guid.Data4[4]) );

	InputFile[0] = 0;
	OutputFile[0] = 0;
	InputSurroundFormat[0] = 0;
	OutputSurroundFormat[0] = 0;

	// add a backslash 
	wcscat_s( InputFile, MAX_PATH, TempFile);
	wcscat_s( OutputFile, MAX_PATH, TempFile);
	wcscat_s( InputSurroundFormat, MAX_PATH, TempFile );
	wcscat_s( OutputSurroundFormat, MAX_PATH, TempFile );

	// give us some usable extensions and indices
	wcscat_s( InputFile, MAX_PATH, L".wav" );
	wcscat_s( OutputFile, MAX_PATH, L".mp3" );
	wcscat_s( InputSurroundFormat, MAX_PATH, L"%%02d.wav" );
	wcscat_s( OutputSurroundFormat, MAX_PATH, L"%%02d.mp3" );

}

/**
 * Destructor, cleaning up allocations.
 */
FPS3SoundCooker::~FPS3SoundCooker( void )
{
	CleanUp();
	for( int Index = 0; Index < 4; Index++ )
	{
		if( PS3RawData[Index] )
		{
			free( PS3RawData[Index] );
			PS3RawData[Index] = NULL;
			PS3RawDataSize[Index] = 0;
		}
	}
}

/**
 * Clean up allocations and temp files
 */
void FPS3SoundCooker::CleanUp()
{
	int		Index;
	TCHAR	TempName[MAX_PATH];

	// delete temp files
	DeleteFile( InputFile );
	DeleteFile( OutputFile );

	for( Index = 0; Index < 4; Index++ )
	{
		wsprintf( TempName, InputSurroundFormat, Index );
		DeleteFile( TempName );

		wsprintf( TempName, OutputSurroundFormat, Index );
		DeleteFile( TempName );	
	}
}

/** 
 * Common init for surround and regular asset cooking
 */
bool FPS3SoundCooker::InitCook( TCHAR Path[MAX_PATH], WAVEFORMATEXTENSIBLE* Format, FSoundQualityInfo* QualityInfo )
{
	ErrorString[0] = 0;

	// Clear out any old data
	for( int Index = 0; Index < 4; Index++ )
	{
		if( PS3RawData[Index] )
		{
			free( PS3RawData[Index] );
			PS3RawData[Index] = NULL;
			PS3RawDataSize[Index] = 0;
		}
	}

	// Ensure we have the PS3 SDK installed
	if( !GetPS3Dir( Path ) )
	{
		wcscpy_s( ErrorString, 1024, L"SCE_PS3_ROOT envvar is not set! Run the PS3\\InstallSDK script to install the SDK.\n" );
		return( false );
	}

	// Set up the waveformat
	Format->Format.nChannels = ( WORD )QualityInfo->NumChannels;
	Format->Format.nSamplesPerSec = QualityInfo->SampleRate;
	Format->Format.nBlockAlign = ( WORD )( QualityInfo->NumChannels * sizeof( short ) );
	Format->Format.nAvgBytesPerSec = Format->Format.nBlockAlign * QualityInfo->SampleRate;
	Format->Format.wBitsPerSample = 16;
	Format->Format.wFormatTag = WAVE_FORMAT_PCM;

	return( true );
}

/**
 * Writes out a wav file from source data and format with no validation or error checking
 *
 * @param	WaveFile		File handle to write to
 * @param	SrcBuffer		Pointer to source buffer
 * @param	SrcBufferSize	Size in bytes of source buffer
 * @param	WaveFormat		Pointer to platform specific wave format description
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FPS3SoundCooker::WriteWaveFile( FILE *WaveFile, const BYTE* SrcBuffer, DWORD SrcBufferSize, void* WaveFormat )
{
	long	id;
	int		ChunkSize;
	int		HeaderSize;

	WAVEFORMATEXTENSIBLE* ExtFormat = ( WAVEFORMATEXTENSIBLE * )WaveFormat;
	WAVEFORMATEX* Format = ( WAVEFORMATEX * )&ExtFormat->Format;

	HeaderSize = sizeof( WAVEFORMATEX );
	ChunkSize = sizeof( id ) + sizeof( id ) + sizeof( int ) + sizeof( id ) + sizeof( int ) + SrcBufferSize + HeaderSize;

	id = 'FFIR';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &ChunkSize, sizeof( ChunkSize ), 1, WaveFile );

	id = 'EVAW';
	fwrite( &id, sizeof( id ), 1, WaveFile );	

	id = ' tmf';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &HeaderSize, sizeof( HeaderSize ), 1, WaveFile );
	fwrite( Format, HeaderSize, 1, WaveFile );

	id = 'atad';
	fwrite( &id, sizeof( id ), 1, WaveFile );		

	fwrite( &SrcBufferSize, sizeof( SrcBufferSize ), 1, WaveFile );
	fwrite( SrcBuffer, SrcBufferSize, 1, WaveFile );

	return( true );
}

/**
 * Writes out a stereo wav file from source data and format with no validation or error checking
 *
 * @param	WaveFile		File handle to write to
 * @param	SrcBuffer0		Pointer to source buffer
 * @param	SrcBuffer1		Pointer to source buffer
 * @param	SrcBufferSize	Size in bytes of source buffers
 * @param	WaveFormat		Pointer to platform specific wave format description
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FPS3SoundCooker::WriteStereoWaveFile( FILE *WaveFile, const BYTE* InSrcBuffer0, const BYTE* InSrcBuffer1, DWORD SrcBufferSize, void* WaveFormat )
{
	long	id;
	int		ChunkSize;
	int		HeaderSize;
	DWORD	i;
	short	NullSample = 0;

	WAVEFORMATEXTENSIBLE* ExtFormat = ( WAVEFORMATEXTENSIBLE * )WaveFormat;
	WAVEFORMATEX* Format = ( WAVEFORMATEX * )&ExtFormat->Format;

	const short* SrcBuffer0 = ( const short* )InSrcBuffer0;
	const short* SrcBuffer1 = ( const short* )InSrcBuffer1;

	HeaderSize = sizeof( WAVEFORMATEX );
	ChunkSize = sizeof( id ) + sizeof( id ) + sizeof( int ) + sizeof( id ) + sizeof( int ) + SrcBufferSize + HeaderSize;

	id = 'FFIR';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &ChunkSize, sizeof( ChunkSize ), 1, WaveFile );

	id = 'EVAW';
	fwrite( &id, sizeof( id ), 1, WaveFile );	

	id = ' tmf';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &HeaderSize, sizeof( HeaderSize ), 1, WaveFile );
	fwrite( Format, HeaderSize, 1, WaveFile );

	id = 'atad';
	fwrite( &id, sizeof( id ), 1, WaveFile );		

	fwrite( &SrcBufferSize, sizeof( SrcBufferSize ), 1, WaveFile );

	for( i = 0; i < SrcBufferSize / 2; i += 2 )
	{
		if( SrcBuffer0 )
		{
			fwrite( SrcBuffer0, sizeof( short ), 1, WaveFile );
			SrcBuffer0++;
		}
		else
		{
			fwrite( &NullSample, sizeof( short ), 1, WaveFile );
		}

		if( SrcBuffer1 )
		{
			fwrite( SrcBuffer1, sizeof( short ), 1, WaveFile );
			SrcBuffer1++;
		}
		else
		{
			fwrite( &NullSample, sizeof( short ), 1, WaveFile );
		}
	}

	return( true );
}

/**
 * Attempts to open a temp file 
 *
 * @param	Name			Name of temp file
 *
 * @return	FILE* that isn't NULL on success, NULL on failure
 */
FILE* FPS3SoundCooker::GetTempFile( const TCHAR* Name )
{
	// write the WAVE file to a temporary file on disk
	INT RetryCount = 0;
	FILE* WaveFile = NULL;
	while( ( ( WaveFile = _wfopen( Name, L"wb" ) ) == NULL) && ( RetryCount < 10 ) )
	{
		RetryCount++;
		Sleep( 100 );
	}

	if( !WaveFile )
	{
		// we failed after 10 retries, so just error out of this file
		OutputDebugString( L"Could not open temp$$$$.wav for writing after 10 retries...giving up!\n" );
	}

	return( WaveFile );
}

/** 
 * Reads in the output of the encoding process
 */
bool FPS3SoundCooker::ReadPS3File( const TCHAR* Filename, int Slot )
{
	// read in the new compressed file and replace the raw data with it
	FILE *PS3File = _wfopen( Filename, L"rb" );
	if( PS3File )
	{
		// get size of the file
		fseek( PS3File, 0, SEEK_END );
		PS3RawDataSize[Slot] = ftell( PS3File );
		fseek( PS3File, 0, SEEK_SET );

		// make sure it output something
		if( PS3RawDataSize[Slot] == 0 )
		{
			wcscpy_s( ErrorString, 1024, L"Compression returned a zero length file!\n" );
			fclose( PS3File );
			return( false );
		}

		// allocate space for the buffer that will be returned
		PS3RawData[Slot] = malloc( PS3RawDataSize[Slot] );

		// read in the data
		fread( PS3RawData[Slot], PS3RawDataSize[Slot], 1, PS3File );

		// close the file
		fclose( PS3File );
	}
	else
	{
		wcscpy_s( ErrorString, 1024, L"Unable to open output file for reading\n" );
		return( false );
	}

	return( true );
}

/** 
 * GetHardCodedQuality
 *
 * Potentially adjusts the encoding quality based on UT3 sound asset name and properties
 */
void FPS3SoundCooker::GetHardCodedQuality( WAVEFORMATEXTENSIBLE* Format, const wchar_t* Name, int Quality, int& VBRQuality, int& MaxBitrate )
{
	int NewQuality = 0;

	if( Format->Format.nChannels == 2 )
	{
		NewQuality = 60;
	}
	if( wcsstr( Name, L"A_Music" ) != 0 )
	{
		NewQuality = 70;
	}
	if( wcsstr( Name, L"A_Announcer" ) != 0 )
	{
		NewQuality = 50;
	}
	if( wcsstr( Name, L"A_Character" ) != 0 )
	{
		NewQuality = 20;
	}
	if( wcsstr( Name, L"BodyImpacts" ) != 0 )
	{
		NewQuality = 0;
	}
	if( wcsstr( Name, L"Efforts" ) != 0 )
	{
		NewQuality = 0;
	}
	if( wcsstr( Name, L"Footstep" ) != 0 )
	{
		NewQuality = 0;
	}
	if( NewQuality != 0 )
	{
		Quality = NewQuality;
	}

	if( Quality <= 20 )
	{
		VBRQuality = 7;
		MaxBitrate = 40;
	}
	else if( Quality <= 30 )
	{
		VBRQuality = 6;
		MaxBitrate = 40;
	}
	else if( Quality <= 40 )
	{
		VBRQuality = 6;
		MaxBitrate = 48;
	}
	else if( Quality <= 50 )
	{
		VBRQuality = 6;
		MaxBitrate = 64;
	}
	else if( Quality <= 60 )
	{
		VBRQuality = 6;
		MaxBitrate = 80;
	}
	else
	{
		VBRQuality = 6;
		MaxBitrate = 96;
	}
}

static bool GetMSEncLocation(TCHAR* Path, TCHAR* MSEncPath, TCHAR* ErrorString)
{
	swprintf_s( MSEncPath, MAX_PATH, L"%s\\host-win32\\bin\\MSEnc.exe", Path);
	if (GetFileAttributes(MSEncPath) == -1)
	{
		swprintf_s( MSEncPath, MAX_PATH, L"%s\\host-win32\\bin\\MSEnc\\MSEnc.exe", Path);
		if (GetFileAttributes(MSEncPath) == -1)
		{
			swprintf_s( ErrorString, 1024, L"MSEnc.exe could not be found at %S\\host-win32\\bin\\MSEnc.exe or %S\\host-win32\\bin\\MSEnc\\MSEnc.exe.\n", Path, Path );
			return false;
		}
	}

	return true;
}

/**
 * Cooks the source data for the platform and stores the cooked data internally.
 *
 * @param	SrcBuffer		Pointer to source buffer
 * @param	SrcBufferSize	Size in bytes of source buffer
 * @param	WaveFormat		Pointer to platform specific wave format description
 * @param	Compression		Quality value ranging from 1 [poor] to 100 [very good]
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FPS3SoundCooker::Cook( const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo )
{
	TCHAR Path[MAX_PATH];
	WAVEFORMATEXTENSIBLE Format = { 0 };

	if( !InitCook( Path, &Format, QualityInfo ) )
	{
		return( false );
	}

	FILE* WaveFile = GetTempFile( InputFile );
	if( WaveFile == NULL )
	{
		wcscpy_s( ErrorString, 1024, L"Could not prepare temp wav file\n" );
		return( false );
	}

	if( WaveFile )
	{
		if( !WriteWaveFile( WaveFile, SrcBuffer, QualityInfo->SampleDataSize, &Format ) )
		{
			wcscpy_s( ErrorString, 1024, L"Writing to temp$$$$.wav failed!\n" );
			fclose( WaveFile );
			CleanUp();
			return( false );
		}
		fclose( WaveFile );
	}

	// find where MSEnc lives
	TCHAR MSEncPath[MAX_PATH];
	if (!GetMSEncLocation(Path, MSEncPath, ErrorString))
	{
		return false;
	}

	TCHAR StrBuffer[MAX_PATH * 3];

	//
	// CNN - High numbers for Quality (exposed in the editor) means better quality - default is 40 in SoundNodeWave.uc
	// --sv 70 scales the input volume to 70% - this prevents clipping (static) when mixing lots of sounds
	// PS3 coooker will translate Quality into -V parameters and -B parameters for VBR MP3 encoding as follows:
	//
	// Quality	-V	-B	Description			Memory			Approx. Compression Ratio
	// --------	---	---	-------------------	---------------	-------------------------
	// 20		7	40	very low			very small		~17:1 compression
	// 30		6	40	low					small			~15:1 compression
	// 40		6	48	default (= ATRAC3)	medium			~13:1 compression
	// 50		6	64	high				large			~11:1 compression
	// 60		6	80	very high			very large		~9:1 compression
	// 70		6	96	stereo high			very large		~15:1 compression (stereo)
	//
	int VBRQuality = 6;
	int MaxBitrate = 40;

	// CNN - Hardcode initial default quality for now since we aren't sure if people have messed with the settings in the packages
	GetHardCodedQuality( &Format, ( const wchar_t* )QualityInfo->Name, QualityInfo->Quality, VBRQuality, MaxBitrate );

	swprintf_s( StrBuffer, MAX_PATH * 3, L"%s -x -mp3 32 -mp3vbr %d -loop -in %s -out %s", 
		MSEncPath, MaxBitrate, InputFile, OutputFile );
	
	DWORD ErrorCode;
	TCHAR ChildProcessErrors[4 * 1024];
	// spawn the child process and get back return information
	bool LaunchedOK = RunChildProcess( StrBuffer, ChildProcessErrors, sizeof( ChildProcessErrors ), &ErrorCode );

	if( !LaunchedOK )
	{
		wcscpy_s( ErrorString, 1024, L"MSEnc execution failed.  See below for more information.\n" );
		CleanUp();
		return false;
	}

	// did it succeed?
	if( ErrorCode == 0 )
	{
		if( !ReadPS3File( OutputFile, 0 ) )
		{
			// append the erorr from MSEnc to the error string
			wcscat_s( ErrorString, 1024, ChildProcessErrors );
			CleanUp();
			return( false );
		}
	}
	else
	{
		wcscpy_s( ErrorString, 1024, L"PS3 conversion failed!:\n" );
		CleanUp();
		return( false );
	}

	CleanUp();
	return( true );
}

/**
 * Cooks upto 8 mono files into a multistream file (eg. 5.1). The front left channel is required, the rest are optional.
 *
 * @param	SrcBuffers		Pointers to source buffers
 * @param	SrcBufferSize	Size in bytes of source buffer
 * @param	WaveFormat		Pointer to platform specific wave format description
 * @param	Compression		Quality value ranging from 1 [poor] to 100 [very good]
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FPS3SoundCooker::CookSurround( const BYTE* SrcBuffers[8], FSoundQualityInfo* QualityInfo )
{
	int		Index;
	TCHAR	Path[MAX_PATH];
	TCHAR	InName[MAX_PATH];
	TCHAR	OutName[MAX_PATH];
	TCHAR	StrBuffer[1024];

	WAVEFORMATEXTENSIBLE Format = { 0 };

	if( !InitCook( Path, &Format, QualityInfo ) )
	{
		return( false );
	}

	// Always write out pairs of channels
	Format.Format.nChannels = 2;
	Format.Format.nAvgBytesPerSec *= 2; 

	int VBRQuality = 6;
	int MaxBitrate = 40;

	GetHardCodedQuality( &Format, ( const wchar_t* )QualityInfo->Name, QualityInfo->Quality, VBRQuality, MaxBitrate );

	// Write out up to 4 stereo files
	for( Index = 0; Index < 4; Index++ )
	{
		if( SrcBuffers[Index * 2] || SrcBuffers[Index * 2 + 1] )
		{
			swprintf_s( InName, MAX_PATH, InputSurroundFormat, Index );
			FILE* InFile = GetTempFile( InName );
			if( InFile == NULL )
			{
				return( false );
			}

			if( !WriteStereoWaveFile( InFile, SrcBuffers[Index * 2], SrcBuffers[Index * 2 + 1], QualityInfo->SampleDataSize * 2, &Format ) )
			{
				fclose( InFile );
				wcscpy_s( ErrorString, 1024, L"Writing to temp$xx$.wav failed!\n" );
				CleanUp();
				return( false );
			}

			fclose( InFile );

			swprintf_s( OutName, MAX_PATH, OutputSurroundFormat, Index );
			DeleteFile( OutName );

			// find where MSEnc lives
			TCHAR MSEncPath[MAX_PATH];
			if (!GetMSEncLocation(Path, MSEncPath, ErrorString))
			{
				CleanUp();
				return false;
			}

			swprintf_s( StrBuffer, 1024, L"%s -x -mp3 32 -mp3vbr %d -loop -in %s -out %s", 
					MSEncPath, MaxBitrate, InName, OutName );

			DWORD ErrorCode;
			TCHAR ChildProcessErrors[4 * 1024];
			// spawn the child process and get back return information
			if( RunChildProcess( StrBuffer, ChildProcessErrors, sizeof( ChildProcessErrors ), &ErrorCode ) == false )
			{
				swprintf_s( ErrorString, 1024, L"MSEnc execution failed - %s\n", ChildProcessErrors );
				CleanUp();
				return( false );
			}

			if( ErrorCode == 0 )
			{
				if( !ReadPS3File( OutName, Index ) )
				{
					CleanUp();
					return( false );
				}
			}
			else
			{
				wcscpy_s( ErrorString, 1024, L"PS3 conversion failed!:\n" );
				CleanUp();
				return( false );
			}
		}
	}

	CleanUp();
	return( true );
}

/**
 * Returns the size of the cooked data in bytes.
 *
 * @return The size in bytes of the cooked data including any potential header information.
 */
UINT FPS3SoundCooker::GetCookedDataSize( void )
{
	DWORD Total = 0;

	for( int Index = 0; Index < 4; Index++ )
	{
		Total += PS3RawDataSize[Index];
	}

	return( Total + sizeof( PS3RawDataSize ) );
}

/**
 * Copies the cooked data into the passed in buffer of at least size GetCookedDataSize()
 *
 * @param DstBuffer	Buffer of at least GetCookedDataSize() bytes to copy data to.
 */
void FPS3SoundCooker::GetCookedData( BYTE* DstBuffer )
{
	// copy out the buffer
	DWORD* SizeBuffer = ( DWORD* )DstBuffer;
	DstBuffer += sizeof( PS3RawDataSize );

	for( int Index = 0; Index < 4; Index++ )
	{
		SizeBuffer[Index] = BYTESWAP( PS3RawDataSize[Index] );

		if( PS3RawData[Index] )
		{
			memcpy( DstBuffer, PS3RawData[Index], PS3RawDataSize[Index] );
			DstBuffer += PS3RawDataSize[Index];

			free( PS3RawData[Index] );
			PS3RawData[Index] = NULL;
			PS3RawDataSize[Index] = 0;
		}
	}
}

/** 
 * Decompresses the platform dependent format to raw PCM. Used for quality previewing.
 *
 * @param	SrcData			Uncompressed PCM data
 * @param	DstData			Uncompressed PCM data after being compressed		
 * @param	QualityInfo		All the information the compressor needs to compress the audio	
 */
INT FPS3SoundCooker::Recompress( const BYTE* SrcBuffer, BYTE*, FSoundQualityInfo* QualityInfo )
{
	// Cannot quality preview multichannel sounds
	if( QualityInfo->NumChannels > 2 )
	{
		return( -1 );
	}

	if( !Cook( SrcBuffer, QualityInfo ) )
	{
		return( -1 );
	}

	return( GetCookedDataSize() );
}

FPS3ShaderPrecompiler::FPS3ShaderPrecompiler()
{
	LibCgcDLL = NULL;
	cgcContext = NULL;
	cgcBinaryBin = NULL;
	cgcMessageBin = NULL;

#ifndef PS3MOD
	// Load libcgc.dll with an explicit path.
	TCHAR Path[MAX_PATH] = L"";

	// try to get the location of the PS3 SDK
	if (GetPS3Dir(Path))
	{
		// append the location of the lib
#if _WIN64
		wcscat_s(Path, MAX_PATH, L"\\host-win32\\Cg\\bin\\libcgc_x64.dll");
#else
		wcscat_s(Path, MAX_PATH, L"\\host-win32\\Cg\\bin\\libcgc.dll");
#endif
		// Try to load from the explicit path
		LibCgcDLL = LoadLibrary(Path);
	}
#endif

	if ( LibCgcDLL == NULL )
	{
#if _WIN64
		// Try to load from the standard search path
		LibCgcDLL = LoadLibrary( L"libcgc_x64.dll" );
#else
		LibCgcDLL = LoadLibrary( L"libcgc.dll" );
#endif
	}
	if ( LibCgcDLL == NULL )
	{
		OutputDebugString( L"We were unable to to find LIBCGC.DLL via the SCE_PS3_ROOT env var!  Please make sure SCE_PS3_ROOT is valid and you have LIBCGC.DLL.\n" );
	}

	if ( cgcContext == NULL )
	{
		cgcContext = sceCgcNewContext();
	}
	if ( cgcContext == NULL )
	{
		OutputDebugString( L"Could not init the Cg context!\n" );
	}
	if ( cgcBinaryBin == NULL )
	{
		cgcBinaryBin = sceCgcNewBin();
	}
	if ( cgcBinaryBin == NULL )
	{
		OutputDebugString( L"Could not init the Cg binary bin!\n" );
	}
	if ( cgcMessageBin == NULL )
	{
		cgcMessageBin = sceCgcNewBin();
	}
	if ( cgcMessageBin == NULL )
	{
		OutputDebugString( L"Could not init the Cg message bin!\n" );
	}
}

FPS3ShaderPrecompiler::~FPS3ShaderPrecompiler()
{
	if ( cgcMessageBin )
	{
		sceCgcDeleteBin( cgcMessageBin );
		cgcMessageBin = NULL;
	}
	if ( cgcBinaryBin )
	{
		sceCgcDeleteBin( cgcBinaryBin );
		cgcBinaryBin = NULL;
	}
	if ( cgcContext )
	{
		sceCgcDeleteContext( cgcContext );
		cgcContext = NULL;
	}

	if ( LibCgcDLL )
	{
		FreeLibrary( LibCgcDLL );
		LibCgcDLL = NULL;
	}
}

/**
 * Precompile the shader with the given name. Must be implemented
 *
 * @param ShaderPath		Pathname to the source shader file ("..\Engine\Shaders\Emissive.usf")
 * @param EntryFunction		Name of the startup function ("pixelShader")
 * @param bIsVertexShader	True if the vertex shader is being precompiled
 * @param CompileFlags		Default is 0, otherwise members of the D3DXSHADER enumeration
 * @param Definitions		Space separated string that contains shader defines ("FRAGMENT_SHADER=1 VERTEX_LIGHTING=0")
 * @param bDumpingShaderPDBs	True if shader PDB's should be saved to ShaderPDBPath
 * @param ShaderPDBPath			Path to save shader PDB's, can be on the local machine if not using runtime compiling.
 * @param BytecodeBuffer	Block of memory to fill in with the compiled bytecode
 * @param BytecodeSize		Size of the returned bytecode
 * @param ConstantBuffer	String buffer to return the shader definitions with [Name,RegisterIndex,RegisterCount] ("WorldToLocal,100,4 Scale,101,1"), NULL = Error case
 * @param Errors			String buffer any output/errors
 * 
 * @return true if successful
 */
bool FPS3ShaderPrecompiler::PrecompileShader(
	const char* ShaderPath, 
	const char* EntryFunction, 
	bool bIsVertexShader, 
	DWORD /*CompileFlags*/, 
	const char* Definitions, 
	const char* /*IncludeDirectory*/,
	char* const * /*IncludeFileNames*/,
	char* const * /*IncludeFileContents*/,
	int /*NumIncludes*/,
	bool bDumpingShaderPDBs,
	const char* ShaderPDBPath,
	unsigned char* BytecodeBuffer, 
	int& BytecodeSize, 
	char* ConstantBuffer, 
	char* Errors)
{
	if ( LibCgcDLL == NULL )
	{
		strcpy(Errors, "Libcgc.dll is not loaded - can't compile PS3 shaders!\n");
		return false;
	}

	char ShaderDirectory[MAX_PATH];
	char *FilenameStart = NULL;
	GetFullPathNameA( ShaderPath, MAX_PATH, ShaderDirectory, &FilenameStart );
	if ( FilenameStart )
		*FilenameStart = 0;

	// build the #define list (room for 128 options!)
	const char* Defines[128];
	char* CustomDefines = new char [strlen(Definitions) + 1];
	strcpy( CustomDefines, Definitions );
	int NumDefines = 0;
	Defines[NumDefines++] = "-contalloc";
	Defines[NumDefines++] = "-firstallocreg";
	Defines[NumDefines++] = "200";
	Defines[NumDefines++] = "-I";
	//@todo - implement the include handler for PS3
	Defines[NumDefines++] = ShaderDirectory;
	Defines[NumDefines++] = "-DPS3=1";
	Defines[NumDefines++] = "-DCOMPILER_CG=1";
	Defines[NumDefines++] = "-DSUPPORTS_DEPTH_TEXTURES=0";

	if ( bDumpingShaderPDBs )
	{
		Defines[NumDefines++] = "-capture-preprocess";
		Defines[NumDefines++] = "-capture-dir";
		Defines[NumDefines++] = ShaderPDBPath;
	}

	char* Token = strtok( CustomDefines, " " );
	while ( Token )
	{
		if ( NumDefines >= 128 )
		{
			delete [] CustomDefines;
			strcpy(Errors, "Too many #defines when compiling PS3 shader!\n");
			return false;
		}
		Defines[NumDefines++] = Token;
		Token = strtok( NULL, " " );
	}
	Defines[NumDefines++] = NULL;


	// Load the source code
	char *Source = new char[256*1024];	//256 KB
	FILE* SourceFile = fopen(ShaderPath, "rt");
	int SourceMaxLength = _filelength( _fileno(SourceFile) );
	if ( SourceMaxLength >= 256*1024 )
	{
		delete [] Source;
		delete [] CustomDefines;
		strcpy(Errors, "Error when compiling PS3 shader: Source code too large!\n");
		return false;
	}
	size_t SourceLength = fread(Source, 1, SourceMaxLength, SourceFile );
	fclose( SourceFile );
	Source[SourceLength] = 0;

	// Compile the program
	CGCstatus ErrorCode = sceCgcCompileString(cgcContext, Source, bIsVertexShader ? "sce_vp_rsx" : "sce_fp_rsx", EntryFunction, Defines, cgcBinaryBin, cgcMessageBin);
	delete [] Source;
	delete [] CustomDefines;
	if ( ErrorCode == SCECGC_OK )
	{
		size_t codeSize = sceCgcGetBinSize(cgcBinaryBin);
		char *Bytecode = new char[codeSize];
		memcpy(Bytecode, (char*)sceCgcGetBinData(cgcBinaryBin), codeSize);
		CGprogram Program = (CGprogram) Bytecode;

		// The CgBinaryProgram::totalSize variable is 8 bytes in...
		uint32_t ProgramSize = BYTESWAP(*((uint32_t*)(Bytecode + 8)));

		uint8_t* TempBuffer = new uint8_t[ProgramSize];
		memcpy( TempBuffer, Program, ProgramSize );

		// This fixes up the program (byteswapping, etc), so that it can be parsed on PC by the cellGcmCg* functions.
		cellGcmCgInitProgram(Program);

		// Get information about the CGprogram.
		uint32_t BinarySize			= cellGcmCgGetTotalBinarySize(Program);
		uint32_t NumParameters		= cellGcmCgGetCountParameter(Program);
		uint16_t NumInstructions	= (uint16_t) cellGcmCgGetInstructions(Program);
		uint32_t AttribMask			= cellGcmCgGetAttribOutputMask(Program);
		uint16_t NumRegisters		= (uint16_t) cellGcmCgGetRegisterCount(Program);
		int ConstantBufferSize		= 0;
		FVertexShaderInfo *VSHeader	= NULL;
		FPixelShaderInfo *PSHeader	= NULL;
		FShaderDefaultValue* DefaultValues = NULL;
		int NumDefaultValues		= 0;
		void *MicrocodeSrc			= NULL;
		uint8_t *MicrocodeDst		= NULL;
		uint32_t MicrocodeSize		= 0;
		uint8_t* TextureUnits		= NULL;
		uint16_t TexcoordMask		= 0;
		bool bOutputFromH0			= false;
		bool bDepthReplace			= false;
		bool bPixelKill				= false;
		cellGcmCgGetUCode(Program, &MicrocodeSrc, &MicrocodeSize);

		// Get pointer to the microcode and size of that part (for vertexshaders only)
		if ( bIsVertexShader )
		{
			VSHeader					= (FVertexShaderInfo*) BytecodeBuffer;
			VSHeader->MicrocodeSize		= BYTESWAP(MicrocodeSize);
			VSHeader->NumDefaultValues	= 0;
			VSHeader->AttribMask		= 0;
			VSHeader->NumInstructions	= BYTESWAP16(NumInstructions);
			VSHeader->NumRegisters		= BYTESWAP16(NumRegisters);
			BytecodeSize				= sizeof(FVertexShaderInfo);

			// Note, the microcode is in little-endian (byteswapped by cellGcmCgInitProgram) and
			// the RSX wants it in little-endian, but libGcm expects big-endian and will byteswap to
			// little-endian before copying it into the command buffer. So, we need to provide
			// big-endian microcode... :(
			MicrocodeDst = (uint8_t *) (BytecodeBuffer + BytecodeSize);
			uint32_t *Dest	= (uint32_t*) MicrocodeDst;
			uint32_t *Src	= (uint32_t*) MicrocodeSrc;
			for ( uint32_t Instruction=0; Instruction < (MicrocodeSize/4); ++Instruction )
			{
				Dest[Instruction] = BYTESWAP( Src[Instruction] );
			}
			BytecodeSize += int(MicrocodeSize);

			// Point to memory for extra vertex shader parameter info.
			DefaultValues = (FShaderDefaultValue*) (BytecodeBuffer + BytecodeSize);
			int NumDefaultValues = 0;
		}
		else
		{
			uint16_t MicrocodeSize2 = uint16_t(MicrocodeSize);
			PSHeader				= (FPixelShaderInfo*) BytecodeBuffer;
			PSHeader->MicrocodeSize	= BYTESWAP16( MicrocodeSize2 );
			PSHeader->TextureMask	= 0;
			BytecodeSize			= sizeof(FPixelShaderInfo);

			// Point to memory for the TextureUnits array
			TextureUnits			= BytecodeBuffer + MicrocodeSize + BytecodeSize;

			MicrocodeSrc			= TempBuffer + uint32_t(MicrocodeSrc) - uint32_t(Program);		MicrocodeDst = BytecodeBuffer + BytecodeSize;
			memcpy( MicrocodeDst, MicrocodeSrc, MicrocodeSize );
			BytecodeSize += int(MicrocodeSize);
		}


		delete [] TempBuffer;

		// Pixel shader info:
		// ParameterIndex for each used uniform
		struct ShaderParams
		{
			const char*	Name;
			int			ParameterIndex;
			int			NumElements;
		};
		ShaderParams PixelShaderParameters[256];
		int NumPixelShaderParameters = 0;	// Number of unique pixelshader parameter names (arrays counts as 1)
		int TotalPixelShaderParameters = 0;	// Total number of pixelshader parameters (including all array elements)

		// Sampler register for each used sampler
		ShaderParams Samplers[16];
		int NumSamplers = 0;

		for ( uint32_t ParameterIndex=0; ParameterIndex < NumParameters; ++ParameterIndex )
		{
			CGparameter Parameter = cellGcmCgGetIndexParameter(Program, ParameterIndex);
			CGbool IsReferenced = cellGcmCgGetParameterReferenced(Program, Parameter);
			const char *ConstantName = cellGcmCgGetParameterName(Program, Parameter);
			const char *Semantic = cellGcmCgGetParameterSemantic(Program, Parameter);

			// CG_HALF, CG_HALF[2-4], CG_HALF[1-4]x[1-4],
			// CG_FLOAT, CG_FLOAT[2-4], CG_FLOAT[1-4]x[1-4],
			// CG_SAMPLER[1-3]D, CG_SAMPLERRECT, CG_SAMPLERCUBE,
			// CG_BOOL, CG_BOOL[1-4], CG_BOOL[1-4]x[1-4]
			CGtype Type = cellGcmCgGetParameterType(Program, Parameter);

			// CG_IN, CG_OUT, CG_INOUT
			CGenum Direction = cellGcmCgGetParameterDirection(Program, Parameter);

			// CG_TEXUNIT[0-15], CG_ATTR[0-15], CG_TEX[0-7].
			CGresource Resource = cellGcmCgGetParameterResource(Program, Parameter);
			uint32_t ConstantRegister = cellGcmCgGetParameterResourceIndex(Program, Parameter);

			// CG_VARYING = interpolator/vertex_attr, CG_UNIFORM = shader parameter, CG_CONSTANT = constant (including internal Cg constants).
			CGenum Variability = cellGcmCgGetParameterVariability(Program, Parameter);

			// Default value.
			const float *DefaultValue = cellGcmCgGetParameterValues(Program, Parameter);

			// How many times it's referenced in a pixelshader.
			uint32_t EmbeddedCount = cellGcmCgGetEmbeddedConstantCount(Program, Parameter);

			// HACK: Convert the undocumented dp3 constant "_depth_factor" into a CG_CONSTANT.
			// This constant converts a D24S8 value to a scalar float, by default. Lets not support other depth formats.
			if ( !bIsVertexShader && Resource == CG_UNDEFINED && DefaultValue != NULL && strcmp(ConstantName, "_depth_factor") == 0 )
			{
				Variability = CG_CONSTANT;
			}

			int ResourceIndex, ConstantSize;

			if ( IsReferenced && Variability == CG_UNIFORM )
			{
				// Texture sampler?
				if ( Type == CG_SAMPLER1D || Type == CG_SAMPLER2D || Type == CG_SAMPLER3D || Type == CG_SAMPLERCUBE || Type == CG_SAMPLERRECT )
				{
					ConstantSize = 1;
					//						ResourceIndex = Resource - CG_TEXUNIT0;	// Texture slot		BUG!!!!! -contalloc doesn't work for arrays of samplers! :(
					if ( !bIsVertexShader )
					{
						// Generate a bitmask of used texture units so we can disable unused ones at run-time.
						PSHeader->TextureMask |= uint16_t( 1 << (uint32_t(Resource) - CG_TEXUNIT0) );
					}

					// Build up the Samplers[] info, for later processing.
					const char* Bracket = strchr(ConstantName, '[');
					int NumElements = 1;
					if ( Bracket )
					{
						NumElements = atoi( Bracket+1 ) + 1;
						int NameLength = int(Bracket - ConstantName) + 1;	// Include the '[' in the name, for comparison reasons
						bool bFound = false;
						for ( int PrevParam=0; PrevParam < NumSamplers; ++PrevParam )
						{
							if ( strncmp(ConstantName, Samplers[PrevParam].Name, NameLength) == 0 )
							{
								Samplers[ PrevParam ].NumElements = max( NumElements, Samplers[ PrevParam ].NumElements );
								bFound = true;
								continue;
							}
						}
						if ( bFound )
						{
							continue;
						}
					}
					Samplers[ NumSamplers ].Name = ConstantName;
					Samplers[ NumSamplers ].ParameterIndex = ParameterIndex;
					Samplers[ NumSamplers ].NumElements = NumElements;
					NumSamplers++;		//@TODO: Check for buffer overflow!
					continue;
				}
				// Otherwise it's a shader constant.
				else
				{
					ConstantSize = GetCGTypeSize( Type );

					// Only vertex shaders have constant registers.
					if ( bIsVertexShader )
					{
						// Rescale the constant address to be in bytes.
						ResourceIndex = ConstantRegister * sizeof(FLOAT) * 4;
						ConstantSize *= sizeof(FLOAT) * 4;
					}
					else
					{
						// Aggregate types are special :/
						ResourceIndex = (ConstantSize > 1) ? ParameterIndex+1 : ParameterIndex;

						// Build up the PixelShaderParameters[] info, for later processing.
						if ( EmbeddedCount > 0 )
						{
							const char* Bracket = strchr(ConstantName, '[');
							int NumElements = 1;
							if ( Bracket )
							{
								NumElements = ConstantSize * (atoi( Bracket+1 ) + 1);
								int NameLength = int(Bracket - ConstantName) + 1;	// Include the '[' in the name, for comparison reasons
								bool bFound = false;
								for ( int PrevParam=0; PrevParam < NumPixelShaderParameters; ++PrevParam )
								{
									if ( strncmp(ConstantName, PixelShaderParameters[PrevParam].Name, NameLength) == 0 )
									{
										int MaxNumElements = max( NumElements, PixelShaderParameters[ PrevParam ].NumElements );
										TotalPixelShaderParameters += MaxNumElements - PixelShaderParameters[ PrevParam ].NumElements;
										PixelShaderParameters[ PrevParam ].NumElements = MaxNumElements;
										bFound = true;
										continue;
									}
								}
								if ( bFound )
								{
									continue;
								}
							}
							PixelShaderParameters[ NumPixelShaderParameters ].Name = ConstantName;
							PixelShaderParameters[ NumPixelShaderParameters ].ParameterIndex = ParameterIndex - (NumElements - 1);
							PixelShaderParameters[ NumPixelShaderParameters ].NumElements = NumElements;
							NumPixelShaderParameters++;		//@TODO: Check for buffer overflow!
							TotalPixelShaderParameters++;
						}
						continue;
					}
				}

				if ( ConstantSize == 0 )
				{
					sprintf(Errors, "Error when compiling PS3 shader: Parameter \"%s : %s\" has 0 size!\n", ConstantName, Semantic);
					delete []Bytecode;
					return false;
				}

				ConstantBufferSize += sprintf(ConstantBuffer + ConstantBufferSize, "%s,%d,%d ", ConstantName, ResourceIndex, ConstantSize);
			}

			// Generate the vertex attribute input mask.
			if ( bIsVertexShader && IsReferenced && Variability == CG_VARYING && Direction == CG_IN )
			{
				VSHeader->AttribMask |= 1 << (uint32_t(Resource) - CG_ATTR0);
			}

			// Generate the pixel shader texcoord input mask.
			if ( !bIsVertexShader && IsReferenced && Variability == CG_VARYING )
			{
				if ( Direction == CG_IN && Resource >= CG_TEXCOORD0 && Resource <= CG_TEXCOORD15 )
				{
					TexcoordMask |= 1 << ((uint16_t(Resource) - CG_TEXCOORD0));
				}
				else if ( Direction == CG_OUT && Resource == CG_COLOR0 && Type == CG_HALF4 )
				{
					bOutputFromH0 = true;
				}
			}

			// Look for pixelkill.
			if ( !bIsVertexShader && Resource == CG_UNDEFINED && Variability == CG_VARYING && strncmp(ConstantName, "$kill", 5) == 0 )
			{
				bPixelKill = true;
			}

			// Is this a constant (i.e. not a shader parameter)?
			if ( IsReferenced && DefaultValue != NULL && Variability == CG_CONSTANT )
			{
				if ( bIsVertexShader )
				{
					// Vertex shaders: Save it off so that it can be set at run-time.
					DefaultValues[NumDefaultValues].ConstantRegister = BYTESWAP(ConstantRegister);
					DefaultValues[NumDefaultValues].DefaultValue[0]  = BYTESWAP(DefaultValue[0]);	// These floats are in little-endian, and the RSX wants them in little-endian...
					DefaultValues[NumDefaultValues].DefaultValue[1]  = BYTESWAP(DefaultValue[1]);	// HOWEVER, libGcm expects big-endian and will byteswap them... :(
					DefaultValues[NumDefaultValues].DefaultValue[2]  = BYTESWAP(DefaultValue[2]);
					DefaultValues[NumDefaultValues].DefaultValue[3]  = BYTESWAP(DefaultValue[3]);
					NumDefaultValues++;
				}
				else
				{
					// Pixel shaders: Patch the microcode right away.
					for ( uint32_t EmbeddedIndex=0; EmbeddedIndex < EmbeddedCount; ++EmbeddedIndex )
					{
						uint32_t EmbeddedOffset = cellGcmCgGetEmbeddedConstantOffset(Program, Parameter, EmbeddedIndex);
						uint32_t* Constant = (uint32_t*) DefaultValue;
						uint32_t* PatchPtr = (uint32_t*) (MicrocodeDst + EmbeddedOffset);
						uint32_t PS3Value[4];
						PS3Value[0] = BYTESWAP( Constant[0] );
						PS3Value[1] = BYTESWAP( Constant[1] );
						PS3Value[2] = BYTESWAP( Constant[2] );
						PS3Value[3] = BYTESWAP( Constant[3] );
						PatchPtr[0] = ((PS3Value[0] & 0xffff0000) >> 16) | ((PS3Value[0] & 0x0000ffff) << 16);
						PatchPtr[1] = ((PS3Value[1] & 0xffff0000) >> 16) | ((PS3Value[1] & 0x0000ffff) << 16);
						PatchPtr[2] = ((PS3Value[2] & 0xffff0000) >> 16) | ((PS3Value[2] & 0x0000ffff) << 16);
						PatchPtr[3] = ((PS3Value[3] & 0xffff0000) >> 16) | ((PS3Value[3] & 0x0000ffff) << 16);
					}
				}
			}
		}

		if ( bIsVertexShader )
		{
			BytecodeSize += NumDefaultValues * sizeof(FShaderDefaultValue);
			VSHeader->NumDefaultValues	= BYTESWAP( NumDefaultValues );
			VSHeader->AttribMask		= BYTESWAP( VSHeader->AttribMask );
		}
		else
		{
			PSHeader->TextureMask		= BYTESWAP16( PSHeader->TextureMask );

			// Setup info about the samplers.
			uint16_t NumSamplerRegisters = 0;
			for ( int ParamIndex=0; ParamIndex < NumSamplers; ++ParamIndex )
			{
				ConstantBufferSize += sprintf(ConstantBuffer + ConstantBufferSize, "%s,%d,%d ", Samplers[ParamIndex].Name, NumSamplerRegisters, Samplers[ParamIndex].NumElements);
				for ( int ElementIndex=0; ElementIndex < Samplers[ParamIndex].NumElements; ++ElementIndex )
				{
					CGparameter Parameter = cellGcmCgGetIndexParameter(Program, Samplers[ParamIndex].ParameterIndex + ElementIndex);
					CGbool IsReferenced = cellGcmCgGetParameterReferenced(Program, Parameter);
					if ( IsReferenced )
					{
						// CG_TEXUNIT[0-15], CG_ATTR[0-15], CG_TEX[0-7].
						CGresource Resource = cellGcmCgGetParameterResource(Program, Parameter);
						TextureUnits[ NumSamplerRegisters++ ] = uint8_t(Resource - CG_TEXUNIT0);
					}
					else
					{
						// Indicate that this sampler was optimized out by the Cg compiler.
						TextureUnits[ NumSamplerRegisters++ ] = 255;
					}
				}
			}
			PSHeader->NumSamplers		= BYTESWAP16(NumSamplerRegisters);
			BytecodeSize				+= NumSamplerRegisters*sizeof(TextureUnits[0]);

			// Setup info about the pixelshader constants.
			uint8_t* NumOccurrances = (uint8_t *) (BytecodeBuffer + BytecodeSize);
			BytecodeSize += TotalPixelShaderParameters*sizeof(NumOccurrances[0]);
			int BytecodeSizeAligned = ALIGN(BytecodeSize, 16);
			while ( BytecodeSize < BytecodeSizeAligned )
			{
				BytecodeBuffer[BytecodeSize++] = 0;
			}
			uint16_t* Offsets = (uint16_t *) (BytecodeBuffer + BytecodeSize);
			uint16_t NumOffsets = 0;
			uint32_t NumParameterRegisters = 0;
			for ( int ParamIndex=0; ParamIndex < NumPixelShaderParameters; ++ParamIndex )
			{
				ConstantBufferSize += sprintf(
					ConstantBuffer + ConstantBufferSize,
					"%s,%d,%d ",
					PixelShaderParameters[ParamIndex].Name,
					NumParameterRegisters * sizeof(FLOAT) * 4,
					PixelShaderParameters[ParamIndex].NumElements * sizeof(FLOAT) * 4
					);
				for ( int ElementIndex=0; ElementIndex < PixelShaderParameters[ParamIndex].NumElements; ++ElementIndex )
				{
					CGparameter Parameter = cellGcmCgGetIndexParameter(Program, PixelShaderParameters[ParamIndex].ParameterIndex + ElementIndex);
					int EmbeddedCount = cellGcmCgGetEmbeddedConstantCount(Program, Parameter);
					NumOccurrances[ NumParameterRegisters++ ] = uint8_t(EmbeddedCount);
					for ( int OccurranceIndex=0; OccurranceIndex < EmbeddedCount; ++OccurranceIndex )
					{
						uint16_t Offset = uint16_t(cellGcmCgGetEmbeddedConstantOffset(Program, Parameter, OccurranceIndex));
						Offsets[ NumOffsets++ ] = BYTESWAP16( Offset );
					}
				}
			}
			// NOTE: NumParameterRegisters should be identical to TotalPixelShaderParameters!
			uint16_t NumParams = uint16_t(TotalPixelShaderParameters);
			PSHeader->NumParameters = BYTESWAP16(NumParams);
			PSHeader->NumParamOffsets = BYTESWAP16(NumOffsets);
			BytecodeSize += NumOffsets*sizeof(Offsets[0]);

			// Setup Cgb flags.
			CellCgbFragmentProgramConfiguration Config;
			cellGcmCgGetCgbFragmentProgramConfiguration( Program, &Config, 1, 1, 0 );
			uint16_t RegisterCount16		= Config.registerCount;
			PSHeader->AttributeInputMask	= BYTESWAP(Config.attributeInputMask);
			PSHeader->FragmentControl		= BYTESWAP(Config.fragmentControl);
			PSHeader->TexCoordsInputMask	= BYTESWAP16(Config.texCoordsInputMask);
			PSHeader->TexCoords2D			= BYTESWAP16(Config.texCoords2D);
			PSHeader->TexCoordsCentroid		= BYTESWAP16(Config.texCoordsCentroid);
			PSHeader->RegisterCount			= BYTESWAP16(RegisterCount16);
		}

		// free the compiled code
		delete []Bytecode;
		return true;
	}
	else
	{
		int ErrorLength = sprintf( Errors, "Failed to compile PS3 %s:\n%s", bIsVertexShader ? "vertex shader" : "pixel shader", (char*)sceCgcGetBinData(cgcMessageBin) );

		return false;
	}
}

bool FPS3ShaderPrecompiler::PreprocessShader(
	const char* /*ShaderPath*/, 
	const char* /*Definitions*/, 
	const char* /*IncludeDirectory*/,
	char* const * /*IncludeFileNames*/,
	char* const * /*IncludeFileContents*/,
	int /*NumIncludes*/,
	unsigned char* /*ShaderText*/, 
	int& /*ShaderTextSize*/, 
	char* /*Errors*/)
{
	//@todo - implement for PS3
	return false;
}

bool FPS3ShaderPrecompiler::DisassembleShader(
	const DWORD* /*ShaderByteCode*/, 
	unsigned char* /*ShaderText*/, 
	int& /*ShaderTextSize*/)
{
	//@todo - implement for PS3
	return false;
}

bool FPS3ShaderPrecompiler::CreateShaderCompileCommandLine(
	const char* ShaderPath, 
	const char* IncludePath, 
	const char* EntryFunction, 
	bool bIsVertexShader, 
	DWORD CompileFlags,
	const char* Definitions, 
	char* CommandStr,
	bool bPreprocessed
	)
{
	if(ShaderPath == NULL)
	{
		return false;
	}

	CommandStr[0] = '\0';

	// set path to command line compiler
	strcat(CommandStr, "\"%SCE_PS3_ROOT%\\host-win32\\Cg\\bin\\sce-cgc\" ");
	strcat(CommandStr, ShaderPath);

	// add definitions
	strcat(CommandStr, " -DPS3=1 -DCOMPILER_CG=1 -DSUPPORTS_DEPTH_TEXTURES ");

	if(Definitions != NULL && !bPreprocessed)
	{
		strcat(CommandStr, Definitions);
	}

	if(EntryFunction != NULL)
	{
		// add the entry point reference
		strcat(CommandStr, " -e ");
		strcat(CommandStr, EntryFunction);
	}

	if(IncludePath != NULL && !bPreprocessed)
	{
		// add the include path
		strcat(CommandStr, " -I ");
		strcat(CommandStr, IncludePath);
	}

	// add the target instruction set
	strcat(CommandStr, bIsVertexShader ? " -p sce_vp_rsx" : " -p sce_fp_rsx");

	// add command to disassemble
	strcat(CommandStr, "\r\n\"%SCE_PS3_ROOT%\\host-win32\\Cg\\bin\\sce-cgcdisasm\" ucode.bin -o disassembly.txt");

	// add a newline and a pause
	strcat(CommandStr, "\r\n pause");

	return true;
}
