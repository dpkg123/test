/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#pragma unmanaged

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "xgraphics.lib")
#pragma comment(lib, "xmaencoder.lib")

#include "XeTools.h"
#include <io.h>

#define ARRAY_COUNT( array )			( sizeof(array) / sizeof((array)[0]) )
#define VSNPRINTF(msg, len, fmt, ap)	vswprintf(msg, len, *FString(fmt).Replace(TEXT("%s"), TEXT("%ls")), ap);
#define VSNPRINTFA						vsnprintf

INT appGetVarArgsAnsi( char* Dest, SIZE_T /*DestSize*/, INT Count, const char*& Fmt, va_list ArgPtr )
{
	INT Result = VSNPRINTFA( Dest, Count, Fmt, ArgPtr );
	va_end( ArgPtr );
	return Result;
}

#define GET_VARARGS_ANSI(msg,msgsize,len,lastarg,fmt) {va_list ap; va_start(ap,lastarg);appGetVarArgsAnsi(msg,msgsize,len,fmt,ap);}

void OutputStringToLog(const char* Fmt, ...)
{
	char TempStr[8192];
	GET_VARARGS_ANSI(TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt);

	FILE* pf = fopen("c:\\XeTools.log", "at+");
	if (pf)
	{
		fprintf(pf, TempStr);
		fclose(pf);
	}
}

// The old PWL remapping code use a formula to do the work. The new code is based on this formula
// but uses a direct hand tweaked mapping table to reduce visual artifacts due to rounding errors
// at low color intensities. The approach used avoids purple/ green discoloration at low ranges.
#define USE_OLD_PWL_REMAPPING 0

#if USE_OLD_PWL_REMAPPING
/**
 * Convert color value from linear space to gamma space using 
 * PC sRGB method (exp calculated instead of lookup)
 * see: http://www.w3.org/Graphics/Color/sRGB
 * 
 * @param Value - value to convert (0 - 1023)
 *
 * @return - converted value (0 - 255)
 */
UINT LinearToSRGB_PC( UINT Value ) 
{
	FLOAT Linear = ((FLOAT)Value)/1023.0f;
	FLOAT SRGB;
	if( Linear < 0.0031308f ) 
	{
		// the linear portion of the curve
		SRGB = 12.92f * Linear;
	}
	else
	{
		// the exponential portion of the curve
		SRGB = 1.055f * pow(Linear, 0.41666f) - 0.055f;
	}
	return min(255, (UINT) (255.0f * SRGB + .5f));
}


/**
 * Convert color value from gamma space to linear space using 
 * PC sRGB method (exp calculated instead of lookup)
 * see: http://www.w3.org/Graphics/Color/sRGB
 * 
 * @param Value - value to convert (0 - 255)
 *
 * @return - converted value (0 - 1023)
 */
UINT SRGBToLinear_PC_8bit( UINT Value ) 
{
	FLOAT SRGB = ((float)Value)/255.0f;
	FLOAT Linear;
	if(SRGB <= 0.04045f)
	{
		// the linear portion of the curve
		Linear = SRGB / 12.92f;
	}
	else
	{
		// the exponential portion of the curve
		Linear = pow(((SRGB + 0.055f)/1.055f), 2.4f);
	}
	return Clamp<UINT>(appRound(Linear * 65535.f), 0, 65535);
}

UINT SRGBToLinear_PC_6bit( UINT Value ) 
{
	FLOAT SRGB = ((float)Value)/63.0f;
	FLOAT Linear;
	if(SRGB <= 0.04045f)
	{
		// the linear portion of the curve
		Linear = SRGB / 12.92f;
	}
	else
	{
		// the exponential portion of the curve
		Linear = pow(((SRGB + 0.055f)/1.055f), 2.4f);
	}
	return Clamp<UINT>(appRound(Linear * 65535.f), 0, 65535);
}

UINT SRGBToLinear_PC_5bit( UINT Value ) 
{
	FLOAT SRGB = ((float)Value)/31.0f;
	FLOAT Linear;
	if(SRGB <= 0.04045f)
	{
		// the linear portion of the curve
		Linear = SRGB / 12.92f;
	}
	else
	{
		// the exponential portion of the curve
		Linear = pow(((SRGB + 0.055f)/1.055f), 2.4f);
	}
	return Clamp<UINT>(appRound(Linear * 65535.f), 0, 65535);
}

/**
 * Convert color value from linear space to gamma space using 
 * Xbox 360 piecewise-linear approximation of sRGB method
 * 
 * @param Value - value to convert (0 - 1023)
 *
 * @return - converted value (0 - 255)
 */
UINT LinearToSRGB_Xenon( UINT IntValue ) 
{
	FLOAT Out;
	FLOAT Value = Clamp(IntValue / 65535.f, 0.f, 1.f);
	if (Value < 0.0625f)
	{
		Out = Value * 4.f;
	}
	else if (Value < 0.125f)
	{
		Out = 0.25f  + (Value - 0.0625f) * 2.f;
	}
	else if (Value < 0.5f)
	{
		Out = 0.375f + (Value - 0.125f);
	}
	else
	{
		Out = 0.75f + (Value - 0.5f) / 2.f;
	}
	INT IOut = appRound(Out * 65535.f);
	return IOut;
}

/**
 * Convert color value from gamma space to linear space using 
 * Xbox 360 piecewise-linear approximation of sRGB method
 * 
 * @param Value - value to convert (0 - 255)
 *
 * @return - converted value (0 - 1023)
 */
UINT SRGBToLinear_Xenon( UINT Value ) 
{
	int Out;
	if (Value < 64)
	{
		Out = Value;
	}
	else if (Value < 96)
	{
		Out = 64 + (Value-64)*2;
	}
	else if (Value < 192)
	{
		Out = 128 + (Value-96)*4;
	}
	else
	{
		Out = 513 + (Value-192)*8;
	}
	return Out;
}
#endif

/**
 * Associates texture parameters with cooker.
 *
 * @param UnrealFormat	Unreal pixel format
 * @param Width			Width of texture (in pixels)
 * @param Height		Height of texture (in pixels)
 * @param NumMips		Number of miplevels
 * @param CreateFlags	Platform-specific creation flags
 */
void FXenonTextureCooker::Init( DWORD UnrealFormat, UINT Width, UINT Height, UINT NumMips, DWORD CreateFlags )
{
	D3DFORMAT Format = ConvertUnrealFormatToD3D( UnrealFormat );
	DWORD D3DCreateFlags = 0;
	if( CreateFlags&TextureCreate_NoPackedMip ) 
	{
		D3DCreateFlags |= XGHEADEREX_NONPACKED;
	}

	// Create a dummy texture we can query the mip alignment from.
	XGSetTextureHeaderEx( 
		Width,							// Width
		Height,							// Height
		NumMips,						// Levels
		0,								// Usage
		Format,							// Format
		0,								// ExpBias
		D3DCreateFlags,					// Flags
		0,								// BaseOffset
		XGHEADER_CONTIGUOUS_MIP_OFFSET,	// MipOffset
		0,								// Pitch
		&DummyTexture,					// D3D texture
		NULL,							// unused
		NULL							// unused
		);
	// number of mip levels set in the texture header
	_NumMips = DummyTexture.Format.MaxMipLevel - DummyTexture.Format.MinMipLevel + 1;
	PWLGamma = ( ( (CreateFlags & TextureCreate_PWLGamma) != 0) &&
		( (Format == D3DFMT_A8R8G8B8) ||
		(Format == D3DFMT_DXT1) ||
		(Format == D3DFMT_DXT3) ||
		(Format == D3DFMT_DXT5) ) );
}

/**
 * Returns the platform specific size of a miplevel.
 *
 * @param	Level		Miplevel to query size for
 * @return	Returns	the size in bytes of Miplevel 'Level'
 */
UINT FXenonTextureCooker::GetMipSize( UINT Level ) 
{
	XGTEXTURE_DESC Desc;
	XGGetTextureDesc( &DummyTexture, Level, &Desc );
	return Desc.SlicePitch;
}

/** Lookup tables for SRGB <-> Linear conversion for XBox 360/ PC */
#if USE_OLD_PWL_REMAPPING
INT SRGBToLinear_PC_Lookup_8bit[256];
INT SRGBToLinear_PC_Lookup_6bit[64];
INT SRGBToLinear_PC_Lookup_5bit[32];
#else
INT SRGBToLinear_PC_Lookup_8bit[256] = 
{
	0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,7,7,8,8,9,9,10,10,11,12,12,13,14,14,15,16,17,
	18,18,19,20,21,22,23,24,25,26,27,28,29,31,32,33,34,36,37,38,39,41,42,44,45,47,48,
	50,51,53,54,56,58,59,61,63,65,65,66,67,68,69,70,71,72,73,74,75,76,78,79,80,81,
	82,83,85,86,87,88,89,91,92,93,95,96,97,97,98,99,100,100,101,102,102,103,104,105,
	106,106,107,108,109,110,110,111,112,113,114,115,116,117,117,118,119,120,121,122,
	123,124,125,126,127,128,129,130,131,132,133,134,135,136,138,139,140,141,142,143,
	144,145,147,148,149,150,151,153,154,155,156,158,159,160,161,163,164,165,167,168,
	169,171,172,174,175,176,178,179,181,182,184,185,186,188,189,191,192,193,194,194,
	195,196,197,198,198,199,200,201,202,202,203,204,205,206,207,208,208,209,210,211,
	212,213,214,215,216,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,
	231,232,233,234,235,236,237,238,239,240,241,242,243,244,246,247,248,249,250,251,
	252,253,254,255
};
INT SRGBToLinear_PC_Lookup_6bit[64] = 
{
	0,1,2,2,2,3,4,4,4,5,6,7,8,9,11,13,14,15,17,19,20,21,22,23,24,25,26,27,28,28,28,29,30,31,
	32,33,34,35,37,39,40,41,42,43,45,47,48,49,50,51,52,52,52,53,54,55,56,57,58,59,60,61,62,63
};
INT SRGBToLinear_PC_Lookup_5bit[32] = 
{
	0,1,1,2,2,3,4,6,7,9,10,11,12,13,14,14,15,16,17,19,20,21,23,24,25,26,26,27,28,29,30,31
};
#endif

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
void FXenonTextureCooker::ConvertPWLGammaTexture( void* Src, void* Dst, UINT SrcRowPitch, UINT Width, UINT Height, D3DFORMAT Format )
{
#if USE_OLD_PWL_REMAPPING
	// Initialize lookup tables for faster math.
	static bool bAreLookupTablesInitialized = FALSE;
	if( !bAreLookupTablesInitialized )
	{
		for( INT i=0; i<256; i++ )
		{
			SRGBToLinear_PC_Lookup_8bit[i]	= Clamp<UINT>(appRound(LinearToSRGB_Xenon( SRGBToLinear_PC_8bit(i) ) / 65535.f * 255.f + 0.499f), 0, 255);
		}
		for (INT i=0; i<64; i++)
		{
			SRGBToLinear_PC_Lookup_6bit[i]	= Clamp<UINT>(appRound(LinearToSRGB_Xenon( SRGBToLinear_PC_6bit(i) ) / 65535.f * 63.f + 0.499f), 0, 63);
		}
		for (INT i=0; i<32; i++)
		{
			SRGBToLinear_PC_Lookup_5bit[i]	= Clamp<UINT>(appRound(LinearToSRGB_Xenon( SRGBToLinear_PC_5bit(i) ) / 65535.f * 31.f + 0.499f), 0, 31);
		}

		bAreLookupTablesInitialized=TRUE;
	}
#endif

	if(Format == D3DFMT_A8R8G8B8)
	{
		for(UINT Row = 0; Row < Height; Row++)
		{
			for(UINT Column = 0; Column < Width; Column++)
			{
				UINT DestIndex = Row * Width + Column;
				D3DCOLOR *SourceColor = (D3DCOLOR*)((BYTE*)Src + SrcRowPitch * Row + Column * sizeof(D3DCOLOR));
				D3DCOLOR *DestColor = (D3DCOLOR*)Dst + DestIndex;
				// convert and copy
				*DestColor = ConvertPWLGammaColor(*SourceColor);
			}
		}
	}
	else // this is a DXT texture
	{
		UINT AlphaPadSize = ((Format == D3DFMT_DXT1) ? 0 : DXT_ALPHA_SIZE);
		UINT BlockSize = sizeof(DXTBlock) + AlphaPadSize;

		// convert at least one texel
		Height = max(4,Height);
		Width = max(4,Width);

		// DXT stores blocks of 4x4 pixels
		for(UINT Row = 0; Row < Height/4; Row++)
		{
			for(UINT Column = 0; Column < Width/4; Column++)
			{
				UINT DestOffset = (Row * Width/4 + Column) * BlockSize;
				UINT SrcOffset = Row * SrcRowPitch + Column * BlockSize;
				DXTBlock *SourceBlock = (DXTBlock*) ((BYTE*)Src + SrcOffset + AlphaPadSize);
				DXTBlock *DestBlock = (DXTBlock*) ((BYTE*)Dst + DestOffset + AlphaPadSize);

				// convert and copy
				DestBlock->Color0 = ConvertPWLGammaColor(SourceBlock->Color0);
				DestBlock->Color1 = ConvertPWLGammaColor(SourceBlock->Color1);
				DestBlock->Indices = SourceBlock->Indices;

				if(AlphaPadSize)
				{
					// DXT3 or DXT5 have alpha bytes
					BYTE *SourceAlpha = (BYTE*)Src + SrcOffset;
					BYTE *DestAlpha = (BYTE*)Dst + DestOffset;
					// copy over
					memcpy(DestAlpha,SourceAlpha,AlphaPadSize);
				}
				else
				{
					// For DXT1 we need to maintain the C0/C1 relationship such that
					// if C0 > C1 it stays that way and if C0 <= C1 then it stays that way
					// some doc taken from 
					// http://www.opengl.org/registry/specs/EXT/texture_compression_s3tc.txt
					// There are 16 2-bit codes packed into "Indices".
					bool bNeedsColorSwap = FALSE;
					if( SourceBlock->Color0.Value > SourceBlock->Color1.Value )
					{
						if (DestBlock->Color0.Value < DestBlock->Color1.Value)
						{
							// source used this encoding:
							// RGB0,              if color0 > color1 and code(x,y) == 0
							// RGB1,              if color0 > color1 and code(x,y) == 1
							// (2*RGB0+RGB1)/3,   if color0 > color1 and code(x,y) == 2
							// (RGB0+2*RGB1)/3,   if color0 > color1 and code(x,y) == 3

							// we can swap to restore the color relationship
							// so, we need to swap code 0 with code 1
							// and code 2 with code 3

							DestBlock->Indices ^= 0x55555555;
							bNeedsColorSwap = TRUE;
						}
						else if(DestBlock->Color0.Value == DestBlock->Color1.Value)
						{
							// Hmmm, unequal source colors were turned into equal dest colors.
							// you can't swap colors to get the case back.

							// source used this encoding:
							// RGB0,              if color0 > color1 and code(x,y) == 0
							// RGB1,              if color0 > color1 and code(x,y) == 1
							// (2*RGB0+RGB1)/3,   if color0 > color1 and code(x,y) == 2
							// (RGB0+2*RGB1)/3,   if color0 > color1 and code(x,y) == 3

							// dest will be forced into this encoding:
							// RGB0,              if color0 <= color1 and code(x,y) == 0
							// RGB1,              if color0 <= color1 and code(x,y) == 1
							// (RGB0+RGB1)/2,     if color0 <= color1 and code(x,y) == 2
							// BLACK,             if color0 <= color1 and code(x,y) == 3

							// however, the dest colors are equal, so the problem is trivial
							// any of the source cases will work with any of the dest cases
							// _except_ code = 3, lets just use code = 0

							DestBlock->Indices = 0;
						}
					}
					else if(SourceBlock->Color0.Value <= SourceBlock->Color1.Value && 
						DestBlock->Color0.Value > DestBlock->Color1.Value)
					{
						// source used this encoding:
						// RGB0,              if color0 <= color1 and code(x,y) == 0
						// RGB1,              if color0 <= color1 and code(x,y) == 1
						// (RGB0+RGB1)/2,     if color0 <= color1 and code(x,y) == 2
						// BLACK,             if color0 <= color1 and code(x,y) == 3

						// swapping color can restore that relationship
						// the last two don't change, we only swap code 0 and code 1

						// create a mask on the high code bit to separate code = 0,1 from 2,3
						// then shift that down to the low code bit to flip case 0 and 1.
						DestBlock->Indices ^= (0xaaaaaaaa & ~DestBlock->Indices) >> 1;
						bNeedsColorSwap = TRUE;
					}
					if (bNeedsColorSwap)
					{
						UINT16 Temp = DestBlock->Color0.Value;
						DestBlock->Color0.Value = DestBlock->Color1.Value;
						DestBlock->Color1.Value = Temp;
					}
				}
			}
		}
	}
}


/**
 * Cooks the specified miplevel, and puts the result in Dst which is assumed to
 * be of at least GetMipSize size.
 *
 * @param Level			Miplevel to cook
 * @param Src			Src pointer
 * @param Dst			Dst pointer, needs to be of at least GetMipSize size
 * @param SrcRowPitch	Row pitch of source data
 */
void FXenonTextureCooker::CookMip( UINT Level, void* Src, void* Dst, UINT SrcRowPitch )
{
	XGTEXTURE_DESC Desc;
	XGGetTextureDesc( &DummyTexture, Level, &Desc );

	UINT	MipSize				= GetMipSize( Level );
	void*	IntermediateData	= new BYTE[MipSize];

	if(PWLGamma)
	{
		void* IntermediateData2 = new BYTE[MipSize];

		ConvertPWLGammaTexture(Src, IntermediateData2, SrcRowPitch, Desc.Width, Desc.Height, Desc.Format );

		// PC data is stored in Intel- endian format.
		XGEndianSwapSurface(
			IntermediateData,				// Destination pointer
			Desc.RowPitch,					// Destination pitch
			IntermediateData2,				// Source pointer
			SrcRowPitch,					// Source pitch
			Desc.Width,						// Width
			Desc.Height,					// Height
			Desc.Format						// Format
			);

		delete [] IntermediateData2;
	}
	else
	{
		// PC data is stored in Intel- endian format.
		XGEndianSwapSurface(
			IntermediateData,				// Destination pointer
			Desc.RowPitch,					// Destination pitch
			Src,							// Source pointer
			SrcRowPitch,					// Source pitch
			Desc.Width,						// Width
			Desc.Height,					// Height
			Desc.Format						// Format
			);
	}

	if( XGIsTiledFormat(Desc.Format) )
	{
		// Set area to be tiled
		RECT  Rect;
		SetRect( &Rect, 0, 0, Desc.WidthInBlocks, Desc.HeightInBlocks );

		// Tile intermediate data into the mip's data.
		XGTileSurface( 
			Dst,						// Destination pointer
			Desc.WidthInBlocks,			// Width (in blocks)
			Desc.HeightInBlocks,		// Height (in blocks)
			NULL,						// Start point
			IntermediateData,			// Source pointer
			Desc.RowPitch,				// Pitch
			&Rect,						// Tile rectangle
			Desc.BytesPerBlock			// Block size in bytes
			);
	}
	else
	{
		// Copy the intermediate endian swapped data back into the (potentially enlarged) mip data.
		memcpy( Dst, IntermediateData, MipSize );
	}

	delete [] IntermediateData;
}

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
INT FXenonTextureCooker::GetMipTailBase()
{
	XGMIPTAIL_DESC MipTailDesc;
	XGGetMipTailDesc(&DummyTexture, &MipTailDesc);

	return MipTailDesc.BaseLevel;
}

/**
 * Cooks all mip levels that reside in the packed miptail. Dst is assumed to 
 * be of size GetMipSize (size of the miptail level).
 *
 * @param Src - ptrs to mip data for each source mip level
 * @param SrcRowPitch - array of row pitch entries for each source mip level
 * @param Dst - ptr to miptail destination
 */
void FXenonTextureCooker::CookMipTail( void** Src, UINT* SrcRowPitch, void* Dst )
{
	XGTEXTURE_DESC BaseTextureDesc;
	XGGetTextureDesc( &DummyTexture, 0, &BaseTextureDesc );
	//assert( BaseTextureDesc.Flags&XGTDESC_PACKED );

	XGMIPTAIL_DESC MipTailDesc;
	XGGetMipTailDesc( &DummyTexture, &MipTailDesc );		

	// Re-use for all mip tail surfaces
	INT MipTailStartLevel = GetMipTailBase();

	// For each mip perform an endian swap then pack it into MipTailData at the proper offset
	// Only iterate over the miptail levels
	for (INT i = MipTailStartLevel; i < _NumMips; i++) 
	{
		UINT  MipSize				= GetMipSize( i );
		void* IntermediateData		= new BYTE[MipSize];

		XGTEXTURE_DESC Desc;
		XGGetTextureDesc( &DummyTexture, i, &Desc );

		if(PWLGamma)
		{
			void*	IntermediateData2	= new BYTE[MipSize];

			ConvertPWLGammaTexture(
				Src[i - MipTailStartLevel],
				IntermediateData2,
				SrcRowPitch[i - MipTailStartLevel],
				Desc.Width,
				Desc.Height,
				Desc.Format 
				);

			// PC data is stored in Intel- endian format.
			XGEndianSwapSurface(
				IntermediateData,					// Destination pointer
				Desc.RowPitch,						// Destination pitch
				IntermediateData2,					// Source pointer
				SrcRowPitch[i - MipTailStartLevel],	// Source pitch
				Desc.Width,							// Width
				Desc.Height,						// Height
				Desc.Format							// Format
				);

			delete [] IntermediateData2;
		}
		else
		{
			// PC data is stored in Intel- endian format.
			XGEndianSwapSurface(
				IntermediateData,					// Destination pointer
				Desc.RowPitch,						// Destination pitch
				Src[i - MipTailStartLevel],			// Source pointer
				SrcRowPitch[i - MipTailStartLevel],	// Source pitch
				Desc.Width,							// Width
				Desc.Height,						// Height
				Desc.Format							// Format
				);
		}

		// Mip tail
		if (XGIsTiledFormat(Desc.Format))
		{
			UINT Offset =
				XGGetMipTailLevelOffset(
				BaseTextureDesc.Width,			// Width
				BaseTextureDesc.Height,			// Height
				1,								// Depth
				i,								// Mip level
				XGGetGpuFormat(Desc.Format),	// Format
				TRUE,							// Tiled (T/F)
				FALSE							// Border (T/F)
				);

			// Tile this mip directly into the packed mip tail
			XGTileTextureLevel(
				BaseTextureDesc.Width,			// Width
				BaseTextureDesc.Height,			// Height
				i,								// Mip level
				XGGetGpuFormat(Desc.Format),	// Destination format
				0,								// Flags
				(BYTE*)Dst + Offset,			// Destination pointer
				NULL,							// Start point
				IntermediateData,				// Source pointer
				Desc.RowPitch,					// Source pitch
				NULL							// Rect
				);
		} 
		else 
		{
			// Copy this linear mip into the correct offset
			UINT Offset =
				XGGetMipTailLevelOffset(
				BaseTextureDesc.Width,			// Width
				BaseTextureDesc.Height,			// Height
				1,								// Depth
				i,								// Mip level
				XGGetGpuFormat(Desc.Format),	// Format
				FALSE,							// Tiled (T/F)
				FALSE							// Border (T/F)
				);

			// Copy the IntermediateData into the correct location in the miptail
			XGCopySurface(
				(BYTE*)Dst + Offset,
				MipTailDesc.RowPitch,
				Desc.Width,
				Desc.Height,
				Desc.Format,
				NULL,
				IntermediateData,
				Desc.RowPitch,
				Desc.Format,
				NULL,
				0,
				0
				);
		}

		delete [] IntermediateData;
		IntermediateData = NULL;
	}
}

/**
 * Constructor, initializing all member variables.
 */
FXenonSoundCooker::FXenonSoundCooker( void )
	:	EncodedBufferSize( 0 )
	,	EncodedBufferFormatSize( 0 )
	,	SeekTableSize( 0 )
{
	EncodedBuffer = NULL;
	EncodedBufferFormat = NULL;
	SeekTable = NULL;
	if( GetEnvironmentVariable( TEXT("XEDK"), XDKPath, MAX_PATH ) != 0 )
	{
		// Replace any forward slashes with backslashes
		wchar_t* C = XDKPath;
		while( *C )
		{
			if( *C == '/' )
			{
				*C = '\\';
			}
			if( C[0] == '\\' && C[1] == 0 )
			{
				*C = 0;
			}

			C++;
		}
	}
}

/**
 * Destructor, cleaning up allocations.
 */
FXenonSoundCooker::~FXenonSoundCooker( void )
{
	FreeEncoderResources();
}

/**
 * Runs a child process without spawning a command prompt window for each one
 *
 * @param CommandLine The commandline of the child process to run
 *
 * @return TRUE if the process was run (even if the process failed)
 */
bool FXenonSoundCooker::RunChildProcess( const wchar_t* CommandLine )
{
	// run the command (and avoid a window popping up)
	SECURITY_ATTRIBUTES SecurityAttr; 
	SecurityAttr.nLength = sizeof( SecurityAttr ); 
	SecurityAttr.bInheritHandle = TRUE; 
	SecurityAttr.lpSecurityDescriptor = NULL; 

	HANDLE StdOutRead, StdOutWrite;
	CreatePipe( &StdOutRead, &StdOutWrite, &SecurityAttr, 0 );
	SetHandleInformation( StdOutRead, HANDLE_FLAG_INHERIT, 0 );

	// set up process spawn structures
	STARTUPINFO StartupInfo = { 0 };
	StartupInfo.cb = sizeof( StartupInfo );
	StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	StartupInfo.hStdOutput = StdOutWrite;
	StartupInfo.hStdError = StdOutWrite;
	PROCESS_INFORMATION ProcInfo;

	// kick off the child process
	if( !CreateProcess( NULL, ( LPTSTR )CommandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &StartupInfo, &ProcInfo ) )
	{
		wcscpy_s( ErrorString, 1024, L"Failed to start process" );
		return( false );
	}

	// Buffer for output
	bool bProcessComplete = false;
	char Buffer[1024 * 4];
	char ChildProcessErrors[1024 * 4];
	DWORD BufferSize = sizeof( Buffer );
	DWORD ErrorSize = sizeof( ChildProcessErrors );

	// wait until the precompiler is finished
	while( !bProcessComplete )
	{
		DWORD Reason = WaitForSingleObject( ProcInfo.hProcess, 0 );

		// read up to 64k of error (that would be crazy amount of error, but whatever)
		DWORD SizeToRead;
		// See if we have some data to read
		PeekNamedPipe( StdOutRead, NULL, 0, NULL, &SizeToRead, NULL );
		while( SizeToRead > 0 )
		{
			// read some output
			DWORD SizeRead;
			ReadFile( StdOutRead, &Buffer, min( SizeToRead, BufferSize - 1 ), &SizeRead, NULL );
			Buffer[SizeRead] = 0;

			// decrease how much we need to read
			SizeToRead -= SizeRead;

			// append the output to the 
			strncpy( ChildProcessErrors, Buffer, ErrorSize - 1 );
		}

		// when the process signals, its done
		if( Reason == WAIT_OBJECT_0 )
		{
			// berak out of the loop
			bProcessComplete = true;
		}
	}

	// Get the return value
	DWORD ErrorCode;
	GetExitCodeProcess( ProcInfo.hProcess, &ErrorCode );

	// Close process and thread handles. 
	CloseHandle( ProcInfo.hProcess );
	CloseHandle( ProcInfo.hThread );
	CloseHandle( StdOutRead );
	CloseHandle( StdOutWrite );

	// Conver the error to widechars
	MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, ChildProcessErrors, -1, ErrorString, 1024 );

	return( ErrorCode == S_OK );
}

/**
 * Attempts to open a temp file 
 *
 * @param	Extension			Extension of temp file
 *
 * @return	FILE* that isn't NULL on success, NULL on failure
 */
bool FXenonSoundCooker::GetTempFile( wchar_t* DestName, const wchar_t* Extension )
{
	wchar_t PathName[MAX_PATH];

	if( !GetTempPath( MAX_PATH, PathName ) )
	{
		wcscpy_s( ErrorString, 1024, L"Failed to get temp path" );
		return( false );
	}

	if( !GetTempFileName( PathName, TEXT("Xen"), 0, DestName ) )
	{
		wcscpy_s( ErrorString, 1024, L"Failed to get temp file name" );
		return( false );
	}

	wcscpy_s( DestName + ( wcslen( DestName ) - 4 ), MAX_PATH - ( wcslen( DestName ) - 4 ), Extension );
	return( true );
}

/**
 * Attempts to open a temp file 
 *
 * @param	Name			Name of file
 * @param	Options			Normally 'rb' or 'wb'
 *
 * @return	FILE* that isn't NULL on success, NULL on failure
 */
FILE* FXenonSoundCooker::OpenFile( const wchar_t* Name, const wchar_t* Options )
{
	// write the WAVE file to a temporary file on disk
	INT RetryCount = 0;
	FILE* WaveFile = NULL;
	while( ( ( WaveFile = _wfopen( Name, Options ) ) == NULL) && ( RetryCount < 10 ) )
	{
		RetryCount++;
		Sleep( 100 );
	}

	if( !WaveFile )
	{
		// we failed after 10 retries, so just error out of this file
		wcscpy_s( ErrorString, 1024, L"Could not open temp file for writing after 10 retries...giving up!" );
	}

	return( WaveFile );
}

/** 
 * Convert the generated WAVEFORMATEX buffer format to the used WAVEFORMATEXTENSIBLE format
 *
 * @param BufferFormat		Old buffer format (already byte swapped)
 * @param BufferFormatSize	Size of old buffer format 
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FXenonSoundCooker::ConvertFormat( WAVEFORMATEX* BufferFormat )
{
	EncodedBufferFormatSize = sizeof( WAVEFORMATEXTENSIBLE );
	EncodedBufferFormat = ( WAVEFORMATEXTENSIBLE* )calloc( 1, EncodedBufferFormatSize );
	WAVEFORMATEXTENSIBLE* NewBufferFormat = ( WAVEFORMATEXTENSIBLE* )EncodedBufferFormat;

	XGEndianSwapData( &NewBufferFormat->Format.wFormatTag, &BufferFormat->wFormatTag, XGENDIAN_8IN16 );
	XGEndianSwapData( &NewBufferFormat->Format.nChannels, &BufferFormat->nChannels, XGENDIAN_8IN16 );
	XGEndianSwapData( &NewBufferFormat->Format.nSamplesPerSec, &BufferFormat->nSamplesPerSec, XGENDIAN_8IN32 );
	XGEndianSwapData( &NewBufferFormat->Format.nAvgBytesPerSec, &BufferFormat->nAvgBytesPerSec, XGENDIAN_8IN32 );
	XGEndianSwapData( &NewBufferFormat->Format.nBlockAlign, &BufferFormat->nBlockAlign, XGENDIAN_8IN16 );
	XGEndianSwapData( &NewBufferFormat->Format.wBitsPerSample, &BufferFormat->wBitsPerSample, XGENDIAN_8IN16 );

	WORD cbSize = sizeof( WAVEFORMATEXTENSIBLE ) - sizeof( WAVEFORMATEX );
	XGEndianSwapData( &NewBufferFormat->Format.cbSize, &cbSize, XGENDIAN_8IN16 );

	DWORD ChannelMask = 0;
	for( INT i = 0; i < BufferFormat->nChannels; i++ )
	{
		ChannelMask |= 1 << i;
	}
	XGEndianSwapData( &NewBufferFormat->dwChannelMask, &ChannelMask, XGENDIAN_8IN32 );

	return( true );
}

/** 
 * Byte swap the XMA2WAVEFORMATEX buffer format
 *
 * @param BufferFormat		Little endian buffer format
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FXenonSoundCooker::ConvertFormat( XMA2WAVEFORMATEX* BufferFormat )
{
	EncodedBufferFormatSize = sizeof( XMA2WAVEFORMATEX );
	EncodedBufferFormat = calloc( 1, EncodedBufferFormatSize );
	XMA2WAVEFORMATEX* NewBufferFormat = ( XMA2WAVEFORMATEX* )EncodedBufferFormat;

	XGEndianSwapData( &NewBufferFormat->wfx.wFormatTag, &BufferFormat->wfx.wFormatTag, XGENDIAN_8IN16 );
	XGEndianSwapData( &NewBufferFormat->wfx.nChannels, &BufferFormat->wfx.nChannels, XGENDIAN_8IN16 );
	XGEndianSwapData( &NewBufferFormat->wfx.nSamplesPerSec, &BufferFormat->wfx.nSamplesPerSec, XGENDIAN_8IN32 );
	XGEndianSwapData( &NewBufferFormat->wfx.nAvgBytesPerSec, &BufferFormat->wfx.nAvgBytesPerSec, XGENDIAN_8IN32 );
	XGEndianSwapData( &NewBufferFormat->wfx.nBlockAlign, &BufferFormat->wfx.nBlockAlign, XGENDIAN_8IN16 );
	XGEndianSwapData( &NewBufferFormat->wfx.wBitsPerSample, &BufferFormat->wfx.wBitsPerSample, XGENDIAN_8IN16 );
	XGEndianSwapData( &NewBufferFormat->wfx.cbSize, &BufferFormat->wfx.cbSize, XGENDIAN_8IN16 );

	XGEndianSwapData( &NewBufferFormat->NumStreams, &BufferFormat->NumStreams, XGENDIAN_8IN16 );
	XGEndianSwapData( &NewBufferFormat->ChannelMask, &BufferFormat->ChannelMask, XGENDIAN_8IN32 );
	XGEndianSwapData( &NewBufferFormat->SamplesEncoded, &BufferFormat->SamplesEncoded, XGENDIAN_8IN32 );
	XGEndianSwapData( &NewBufferFormat->BytesPerBlock, &BufferFormat->BytesPerBlock, XGENDIAN_8IN32 );
	XGEndianSwapData( &NewBufferFormat->PlayBegin, &BufferFormat->PlayBegin, XGENDIAN_8IN32 );
	XGEndianSwapData( &NewBufferFormat->PlayLength, &BufferFormat->PlayLength, XGENDIAN_8IN32 );
	XGEndianSwapData( &NewBufferFormat->LoopBegin, &BufferFormat->LoopBegin, XGENDIAN_8IN32 );
	XGEndianSwapData( &NewBufferFormat->LoopLength, &BufferFormat->LoopLength, XGENDIAN_8IN32 );

	NewBufferFormat->LoopCount = BufferFormat->LoopCount;
	NewBufferFormat->EncoderVersion = BufferFormat->EncoderVersion;

	XGEndianSwapData( &NewBufferFormat->BlockCount, &BufferFormat->BlockCount, XGENDIAN_8IN16 );
	return( true );
}

/**
 * Writes out a wav file from source data and format with no validation or error checking
 *
 * @param	WaveFile		File handle of wave file
 * @param	SrcBuffer		Pointer to source buffer
 * @param	QualityInfo		Pointer to platform specific wave format description
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FXenonSoundCooker::WriteWaveFile( FILE* WaveFile, const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo )
{
	long	id;
	int		ChunkSize;
	int		HeaderSize;

	WAVEFORMATEX WaveFormat = { 0 };

	WaveFormat.nChannels = ( WORD )QualityInfo->NumChannels;
	WaveFormat.nSamplesPerSec = QualityInfo->SampleRate;
	WaveFormat.nBlockAlign = ( WORD )( QualityInfo->NumChannels * sizeof( short ) );
	WaveFormat.nAvgBytesPerSec = WaveFormat.nBlockAlign * QualityInfo->SampleRate;
	WaveFormat.wBitsPerSample = 16;
	WaveFormat.wFormatTag = WAVE_FORMAT_PCM;

	HeaderSize = sizeof( WAVEFORMATEX );
	ChunkSize = sizeof( id ) + sizeof( id ) + sizeof( int ) + sizeof( id ) + sizeof( int ) + QualityInfo->SampleDataSize + HeaderSize;

	id = 'FFIR';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &ChunkSize, sizeof( ChunkSize ), 1, WaveFile );

	id = 'EVAW';
	fwrite( &id, sizeof( id ), 1, WaveFile );	

	id = ' tmf';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &HeaderSize, sizeof( HeaderSize ), 1, WaveFile );
	fwrite( &WaveFormat, HeaderSize, 1, WaveFile );

	id = 'atad';
	fwrite( &id, sizeof( id ), 1, WaveFile );		

	fwrite( &QualityInfo->SampleDataSize, sizeof( QualityInfo->SampleDataSize ), 1, WaveFile );
	fwrite( SrcBuffer, QualityInfo->SampleDataSize, 1, WaveFile );

	return( true );
}

/**
 * Reads in a wave file
 *
 * @param	WaveFile		File handle of wave file
 * @param	DestBuffer		Destination memory for pcm data
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FXenonSoundCooker::ReadWaveFile( FILE* WaveFile, BYTE* DestBuffer, FSoundQualityInfo* QualityInfo )
{
	long	id;
	DWORD	FileSize, ChunkSize;

	fseek( WaveFile, 0, SEEK_END );
	FileSize = ftell( WaveFile );
	fseek( WaveFile, 0, SEEK_SET );

	// 'RIFF'
	fread( &id, sizeof( id ), 1, WaveFile );
	fread( &ChunkSize, sizeof( ChunkSize ), 1, WaveFile );

	if( ChunkSize + 8 != FileSize )
	{
		wcscpy_s( ErrorString, 1024, L"ReadWaveFile: Riff size != file size" );
		return( false );
	}

	// 'WAVE'
	fread( &id, sizeof( id ), 1, WaveFile );
	if( id != 'EVAW' )
	{
		wcscpy_s( ErrorString, 1024, L"ReadXWMAFile: Could not find 'WAVE' tag" );
		return( false );	
	}

	// 'fmt'
	fread( &id, sizeof( id ), 1, WaveFile );

	fread( &ChunkSize, sizeof( ChunkSize ), 1, WaveFile );
	fseek( WaveFile, ChunkSize, SEEK_CUR );

	// 'data'
	fread( &id, sizeof( id ), 1, WaveFile );
	if( id != 'atad' )
	{
		wcscpy_s( ErrorString, 1024, L"ReadXWMAFile: Could not find 'data' tag" );
		return( false );	
	}

	fread( &ChunkSize, sizeof( ChunkSize ), 1, WaveFile );

	if( ChunkSize > QualityInfo->SampleDataSize )
	{
		ChunkSize = QualityInfo->SampleDataSize;
	}

	fread( DestBuffer, ChunkSize, 1, WaveFile );

	return( true );
}

/**
 * Writes out a xma file from the cooked data
 *
 * @param	WaveFile		File to write to
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FXenonSoundCooker::WriteXMA2File( FILE* WaveFile )
{
	long		id;
	DWORD		TotalSize;

	// XMA2 format is 
	// 'RIFF' size
	// 'WAVE' 'data' size <data>
	// 'XMA2' size <data> (44)
	// 'seek' size <data> 

	//          'WAVE'         'data'         size              'XMA2'         size              'seek'         size
	TotalSize = sizeof( id ) + sizeof( id ) + sizeof( DWORD ) + sizeof( id ) + sizeof( DWORD ) + sizeof( id ) + sizeof( DWORD );
	TotalSize += EncodedBufferSize + EncodedBufferFormatSize + SeekTableSize;

	id = 'FFIR';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &TotalSize, sizeof( TotalSize ), 1, WaveFile );

	id = 'EVAW';
	fwrite( &id, sizeof( id ), 1, WaveFile );	

	id = ' tmf';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &EncodedBufferFormatSize, sizeof( EncodedBufferFormatSize ), 1, WaveFile );
	fwrite( &XMA2BufferFormat, EncodedBufferFormatSize, 1, WaveFile );

	id = 'kees';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &SeekTableSize, sizeof( SeekTableSize ), 1, WaveFile );
	fwrite( SeekTable, SeekTableSize, 1, WaveFile );	

	id = 'atad';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &EncodedBufferSize, sizeof( EncodedBufferSize ), 1, WaveFile );
	fwrite( EncodedBuffer, EncodedBufferSize, 1, WaveFile );

	return( true );
}

/**
 * Writes out a xma file from the cooked data
 *
 * @param	WaveFile		File to write to
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FXenonSoundCooker::WriteXWMAFile( FILE* WaveFile )
{
	long		id;
	DWORD		TotalSize, FormatSize;

	// XWMA format is 
	// 'RIFF' size
	// 'XWMA' 'fmt ' size <data>
	// 'dpds' size <data>
	// 'data' size <data> 

	FormatSize = sizeof( WAVEFORMATEX );

	//          'XWMA'         'fmt '         size              'dpds'         size              'data'         size
	TotalSize = sizeof( id ) + sizeof( id ) + sizeof( DWORD ) + sizeof( id ) + sizeof( DWORD ) + sizeof( id ) + sizeof( DWORD );
	TotalSize += EncodedBufferSize + FormatSize + SeekTableSize;

	id = 'FFIR';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &TotalSize, sizeof( TotalSize ), 1, WaveFile );

	id = 'AMWX';
	fwrite( &id, sizeof( id ), 1, WaveFile );	

	id = ' tmf';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &FormatSize, sizeof( FormatSize ), 1, WaveFile );
	fwrite( &XWMABufferFormat, FormatSize, 1, WaveFile );

	id = 'sdpd';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &SeekTableSize, sizeof( SeekTableSize ), 1, WaveFile );
	fwrite( SeekTable, SeekTableSize, 1, WaveFile );	

	id = 'atad';
	fwrite( &id, sizeof( id ), 1, WaveFile );

	fwrite( &EncodedBufferSize, sizeof( EncodedBufferSize ), 1, WaveFile );
	fwrite( EncodedBuffer, EncodedBufferSize, 1, WaveFile );

	return( true );
}

/**
 * Reads the created xwma file
 *
 * @param	XWMAFile		FILE* to file
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FXenonSoundCooker::ReadXWMAFile( FILE* XWMAFile )
{
	long			id;
	DWORD			FileSize, ChunkSize;

	fseek( XWMAFile, 0, SEEK_END );
	FileSize = ftell( XWMAFile );
	fseek( XWMAFile, 0, SEEK_SET );

	// 'RIFF'
	fread( &id, sizeof( id ), 1, XWMAFile );
	fread( &ChunkSize, sizeof( ChunkSize ), 1, XWMAFile );

	if( ChunkSize + 8 != FileSize )
	{
		wcscpy_s( ErrorString, 1024, L"ReadXWMAFile: Riff size != file size" );
		return( false );
	}

	// 'XWMA'
	fread( &id, sizeof( id ), 1, XWMAFile );
	if( id != 'AMWX' )
	{
		wcscpy_s( ErrorString, 1024, L"ReadXWMAFile: Could not find 'XWMA' tag" );
		return( false );	
	}

	// 'fmt '
	fread( &id, sizeof( id ), 1, XWMAFile );

	fread( &EncodedBufferFormatSize, sizeof( EncodedBufferFormatSize ), 1, XWMAFile );
	if( EncodedBufferFormatSize != sizeof( WAVEFORMATEX ) )
	{
		wcscpy_s( ErrorString, 1024, L"ReadXWMAFile: Incorrect format size" );
		return( false );	
	}

	fread( &XWMABufferFormat, EncodedBufferFormatSize, 1, XWMAFile );

	// 'dpds'
	fread( &id, sizeof( id ), 1, XWMAFile );	
	if( id != 'sdpd' )
	{
		wcscpy_s( ErrorString, 1024, L"ReadXWMAFile: Could not find 'dpds' tag" );
		return( false );	
	}

	fread( &SeekTableSize, sizeof( SeekTableSize ), 1, XWMAFile );
	SeekTable = malloc( SeekTableSize );
	fread( SeekTable, SeekTableSize, 1, XWMAFile );

	// 'data'
	fread( &id, sizeof( id ), 1, XWMAFile );
	if( id != 'atad' )
	{
		wcscpy_s( ErrorString, 1024, L"ReadXWMAFile: Could not find 'data' tag" );
		return( false );	
	}

	fread( &EncodedBufferSize, sizeof( EncodedBufferSize ), 1, XWMAFile );
	EncodedBuffer = malloc( EncodedBufferSize );
	fread( EncodedBuffer, EncodedBufferSize, 1, XWMAFile );

	return( true );
}

/**
 * GetHardCodedQuality
 *
 * @param	QualityInfo		Pointer to platform specific wave format description
 *
 * @return	int				Requested bit rate
 */
int FXenonSoundCooker::GetHardCodedQuality( FSoundQualityInfo* QualityInfo )
{
	int BitRate = 48000;

	switch( QualityInfo->SampleRate )
	{
	case 22050:
	case 32000:
		if( QualityInfo->Quality < 10 )
		{
			if( QualityInfo->NumChannels == 1 )
			{
				BitRate = 20000;
			}
			else
			{
				BitRate = 32000;
			}
		}
		else if( QualityInfo->Quality < 50 )
		{
			BitRate = 48000;
		}
		else
		{
			BitRate = 192000;
		}
		break;

	case 44100:
		if( QualityInfo->Quality < 10 )
		{
			BitRate = 32000;
		}
		else if( QualityInfo->Quality < 40 )
		{
			BitRate = 48000;
		}
		else if( QualityInfo->Quality < 60 )
		{
			BitRate = 96000;
		}
		else
		{
			BitRate = 192000;
		}
		break;
	}

	return( BitRate );
}

/**
 * Cooks the source data to XMA2
 *
 * @param	SrcBuffer		Pointer to source buffer
 * @param	QualityInfo		All the info required about the wave for the compressor
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FXenonSoundCooker::CookXMA2( const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo )
{
	XMAENCODERSTREAM EncoderStream = { 0 };
	XMA2WAVEFORMATEX* BufferFormatPointer = NULL;

	EncoderStream.Format.Format.nChannels = ( WORD )QualityInfo->NumChannels;
	EncoderStream.Format.Format.nSamplesPerSec = QualityInfo->SampleRate;
	EncoderStream.Format.Format.nBlockAlign = ( WORD )( QualityInfo->NumChannels * sizeof( short ) );
	EncoderStream.Format.Format.nAvgBytesPerSec = EncoderStream.Format.Format.nBlockAlign * QualityInfo->SampleRate;
	EncoderStream.Format.Format.wBitsPerSample = 16;
	EncoderStream.Format.Format.wFormatTag = WAVE_FORMAT_PCM;

	EncoderStream.pBuffer = SrcBuffer;
	EncoderStream.BufferSize = QualityInfo->SampleDataSize;
	EncoderStream.LoopStart = 0;
	EncoderStream.LoopLength = QualityInfo->SampleDataSize / EncoderStream.Format.Format.nBlockAlign;

	if( EncoderStream.Format.Format.nChannels < 1 || EncoderStream.Format.Format.nChannels > 2 )
	{
		return( false );
	}

	// NULL the output variables as they are are freed if the encoding fails, even if they were not allocated
	if ((EncodedBuffer != NULL) || (EncodedBufferFormat != NULL) || (SeekTable != NULL))
	{
		if (EncodedBuffer != NULL)
			OutputStringToLog("FXenonSoundCooker::CookXMA2> EncodedBuffer is NOT NULL!\n");
		if (EncodedBufferFormat != NULL)
			OutputStringToLog("FXenonSoundCooker::CookXMA2> EncodedBufferFormat is NOT NULL!\n");
		if (SeekTable != NULL)
			OutputStringToLog("FXenonSoundCooker::CookXMA2> SeekTable is NOT NULL!\n");
	}

	EncodedBuffer = NULL;
	EncodedBufferFormat = NULL;
	SeekTable = NULL;

	// Convert the sound
	NumStreams = 1;
	HRESULT hr = XAudio2XMAEncoder(
		1,											// Number of streams
		&EncoderStream,								// Info about the passed in data
		QualityInfo->Quality,						// 1 to 100
		XMAENCODER_LOOP,							// Allow looping
		2,											// Block size (in KB) to pad to (xma data needs to be on a 2048 byte boundary)
		&EncodedBuffer,								// * [out] destination buffer
		&EncodedBufferSize,							// [out] size of encoded xma data
		&BufferFormatPointer,						// * [out] format of the output data
		&EncodedBufferFormatSize,					// [out] sizeof( XMA2WAVEFORMATEX )
		( DWORD** )&SeekTable,						// * [out]
		&SeekTableSize								// [out]
		);

	// Only 1 stream, so this is correct
	if( EncodedBufferFormatSize != sizeof( XMA2WAVEFORMATEX ) )
	{
		wcscpy_s( ErrorString, 1024, L"CookXMA2: Incorrect format size" );
		return( false );
	}

	memcpy( &XMA2BufferFormat, BufferFormatPointer, sizeof( XMA2WAVEFORMATEX ) );
	free( BufferFormatPointer );

	return( SUCCEEDED( hr ) );
}

/**
 * Cooks the source data to XWMA
 *
 * @param	SrcBuffer		Pointer to source buffer
 * @param	QualityInfo		All the info required about the wave for the compressor
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FXenonSoundCooker::CookXWMA( const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo )
{
	wchar_t InputFile[MAX_PATH];
	wchar_t OutputFile[MAX_PATH];
	wchar_t Command[MAX_PATH];

	// NULL the output variables as they are are freed if the encoding fails, even if they were not allocated
	if( ( EncodedBuffer != NULL ) || ( EncodedBufferFormat != NULL ) || ( SeekTable != NULL ) )
	{
		if( EncodedBuffer != NULL )
		{
			OutputStringToLog( "FXenonSoundCooker::CookXMA2> EncodedBuffer is NOT NULL!\n" );
		}

		if( EncodedBufferFormat != NULL )
		{
			OutputStringToLog( "FXenonSoundCooker::CookXMA2> EncodedBufferFormat is NOT NULL!\n" );
		}

		if( SeekTable != NULL )
		{
			OutputStringToLog( "FXenonSoundCooker::CookXMA2> SeekTable is NOT NULL!\n" );
		}
	}

	EncodedBuffer = NULL;
	EncodedBufferFormat = NULL;
	SeekTable = NULL;

	if( !GetTempFile( InputFile, TEXT(".wav") ) )
	{
		return( false );
	}

	if( !GetTempFile( OutputFile, TEXT(".xwma") ) )
	{
		return( false );
	}

	FILE* WaveFile = OpenFile( InputFile, TEXT("wb") );
	if( !WaveFile )
	{
		return( false );
	}

	if( !WriteWaveFile( WaveFile, SrcBuffer, QualityInfo ) )
	{
		return( false );
	}

	fclose( WaveFile );

	int BitRate = GetHardCodedQuality( QualityInfo );

	swprintf_s( Command, MAX_PATH, TEXT("%s\\bin\\win32\\xwmaencode.exe -b %d %s %s"), XDKPath, BitRate, InputFile, OutputFile );

	OutputStringToLog("Calling child process: %s\n", Command);
	if( !RunChildProcess( Command ) )
	{
		return( false );
	}

	FILE* XWMAFile = OpenFile( OutputFile, TEXT("rb") );
	if( !XWMAFile )
	{
		return( false );
	}

	if( !ReadXWMAFile( XWMAFile ) )
	{
		return( false );
	}

	fclose( XWMAFile );

	DeleteFile( InputFile );
	DeleteFile( OutputFile );

	return( true );
}

/**
 * Cooks the source data for the platform and stores the cooked data internally.
 *
 * @param	SrcBuffer		Pointer to source buffer
 * @param	QualityInfo		All the information the compressor needs to compress the audio
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FXenonSoundCooker::Cook( const BYTE* SrcBuffer, FSoundQualityInfo* QualityInfo )
{
	// Free buffer data from previous cooking runs.
	FreeEncoderResources();

	if( QualityInfo->bForceRealTimeDecompression && !QualityInfo->bLoopingSound )
	{
		if( QualityInfo->SampleRate == 22050 || QualityInfo->SampleRate == 32000 || QualityInfo->SampleRate == 44100 )
		{
			// Can do xwmaencode
			return( CookXWMA( SrcBuffer, QualityInfo ) );
		}
	}

	// Otherwise cook the standard way
	return( CookXMA2( SrcBuffer, QualityInfo ) );
}

/**
 * Merge 2 mono sounds into 1 stereo sound
 */
void* FXenonSoundCooker::DualMonoToStereo( const BYTE* Left, const BYTE* Right, int NumSamples )
{
	short* Stereo = ( short* )malloc( NumSamples * 2 * sizeof( short ) );
	const short* LeftWork = ( const short* )Left;
	const short* RightWork = ( const short* )Right;
	short* Work = Stereo;

	for( int i = 0; i < NumSamples; i++ )
	{
		if( Left )
		{
			*Work++ = *LeftWork++;
		}
		else
		{
			*Work++ = 0;
		}

		if( Right )
		{
			*Work++ = *RightWork++;
		}
		else
		{
			*Work++ = 0;
		}
	}

	return( Stereo );
}

/**
 * Cooks upto 8 mono files into a multichannel file (eg. 5.1). The front left channel is required, the rest are optional.
 *
 * @param	SrcBuffers		Pointers to source buffers
 * @param	QualityInfo		All the information the compressor needs to compress the audio
 *
 * @return	TRUE if succeeded, FALSE otherwise
 */
bool FXenonSoundCooker::CookSurround( const BYTE* SrcBuffers[8], FSoundQualityInfo* QualityInfo )
{
	int	i;
	XMA2WAVEFORMATEX* BufferFormatPointer;

	// Free buffer data from previous cooking runs.
	FreeEncoderResources();

	XMAENCODERSTREAM EncoderStreams[4] = { 0 };

	NumStreams = 0;
	for( i = 0; i < 4; i++ )
	{
		if( SrcBuffers[i * 2] != NULL || SrcBuffers[i * 2 + 1] != NULL )
		{
			EncoderStreams[NumStreams].Format.Format.nChannels = ( WORD )QualityInfo->NumChannels * 2;
			EncoderStreams[NumStreams].Format.Format.nSamplesPerSec = QualityInfo->SampleRate;
			EncoderStreams[NumStreams].Format.Format.nBlockAlign = ( WORD )( EncoderStreams[NumStreams].Format.Format.nChannels * sizeof( short ) );
			EncoderStreams[NumStreams].Format.Format.nAvgBytesPerSec = EncoderStreams[NumStreams].Format.Format.nBlockAlign * QualityInfo->SampleRate;
			EncoderStreams[NumStreams].Format.Format.wBitsPerSample = 16;
			EncoderStreams[NumStreams].Format.Format.wFormatTag = WAVE_FORMAT_PCM;

			EncoderStreams[NumStreams].pBuffer = DualMonoToStereo( SrcBuffers[i * 2], SrcBuffers[( i * 2 ) + 1], QualityInfo->SampleDataSize / sizeof( short ) );
			EncoderStreams[NumStreams].BufferSize = QualityInfo->SampleDataSize * 2;
			EncoderStreams[NumStreams].LoopStart = 0;
			EncoderStreams[NumStreams].LoopLength = QualityInfo->SampleDataSize / sizeof( short );

			NumStreams++;
		}
	}

	HRESULT hr = XAudio2XMAEncoder(
		NumStreams,										// Number of streams
		EncoderStreams,									// Info about the passed in data
		QualityInfo->Quality,							// 1 to 100
		XMAENCODER_LOOP,								// Allow looping
		8,												// Block size (in KB) to pad to (xma data needs padded to 2048 bytes per stream)
		&EncodedBuffer,									// * [out] destination buffer
		&EncodedBufferSize,								// [out] size of encoded xma data
		&BufferFormatPointer,							// * [out] format of the output data
		&EncodedBufferFormatSize,						// [out] sizeof( XMA2WAVEFORMATEX )
		( DWORD** )&SeekTable,							// * [out]
		&SeekTableSize									// [out]
		);

	for( i = 0; i < NumStreams; i++ )
	{
		if( EncoderStreams[i].pBuffer )
		{
			free( ( void* )EncoderStreams[i].pBuffer );
		}
	}

	// Account for multiple streams
	if( EncodedBufferFormatSize != sizeof( XMA2WAVEFORMATEX ) )
	{
		wcscpy_s( ErrorString, 1024, L"CookSurround: Incorrect format size" );
		return( false );
	}

	memcpy( &XMA2BufferFormat, ( void* )BufferFormatPointer, EncodedBufferFormatSize );
	free( BufferFormatPointer );

	return( SUCCEEDED( hr ) );
}

/**
 * Returns the size of the cooked data in bytes.
 *
 * @return The size in bytes of the cooked data including any potential header information.
 */
UINT FXenonSoundCooker::GetCookedDataSize( void )
{
	UINT TotalSize = 0;

	if( EncodedBufferFormatSize == sizeof( XMA2WAVEFORMATEX ) )
	{
		// No seek table required for XMA2 playback
		TotalSize = sizeof( EncodedBufferFormatSize ) + sizeof( SeekTableSize ) + sizeof( EncodedBufferSize ) + sizeof( XMA2WAVEFORMATEX ) + EncodedBufferSize;
	}
	else if( EncodedBufferFormatSize == sizeof( WAVEFORMATEX ) )
	{
		TotalSize = sizeof( EncodedBufferFormatSize ) + sizeof( SeekTableSize ) + sizeof( EncodedBufferSize ) + sizeof( WAVEFORMATEXTENSIBLE ) + SeekTableSize + EncodedBufferSize;
	}

	return( TotalSize );
}

/**
 * Copies the cooked data into the passed in buffer of at least size GetCookedDataSize()
 *
 * @param DstBuffer	Buffer of at least GetCookedDataSize() bytes to copy data to.
 */
void FXenonSoundCooker::GetCookedData( BYTE* DstBuffer )
{
	if( EncodedBufferFormatSize == sizeof( XMA2WAVEFORMATEX ) )
	{
		// Convert to the required format
		ConvertFormat( &XMA2BufferFormat );

		// Seek data not required for XMA2 playback
		SeekTableSize = 0;
	}
	else if( EncodedBufferFormatSize == sizeof( WAVEFORMATEX ) )
	{
		// Convert to the required format
		ConvertFormat( &XWMABufferFormat );

		// Endian swap the XWMA cumulative offsets
		UINT32* CumulativeBytes = ( UINT32* )SeekTable;
		for( UINT32 i = 0; i < SeekTableSize / sizeof( UINT32 ); i++ )
		{
			XGEndianSwapData( CumulativeBytes + i, CumulativeBytes + i, XGENDIAN_8IN32 );
		}
	}
	else
	{
		return;
	}

	// EncodedBufferFormatSize
	XGEndianSwapData( DstBuffer, &EncodedBufferFormatSize, XGENDIAN_8IN32 );
	DstBuffer += sizeof( DWORD );
	// SeekTableSize
	XGEndianSwapData( DstBuffer, &SeekTableSize, XGENDIAN_8IN32 );
	DstBuffer += sizeof( DWORD );
	// EncodedBufferSize	
	XGEndianSwapData( DstBuffer, &EncodedBufferSize, XGENDIAN_8IN32 );
	DstBuffer += sizeof( DWORD );

	// EncodedBufferFormat
	memcpy( DstBuffer, EncodedBufferFormat, EncodedBufferFormatSize );
	DstBuffer += EncodedBufferFormatSize;
	// SeekTable
	memcpy( DstBuffer, SeekTable, SeekTableSize );
	DstBuffer += SeekTableSize;
	// EncodedBuffer
	memcpy( DstBuffer, EncodedBuffer, EncodedBufferSize );
	DstBuffer += EncodedBufferSize;

	// .... as this function is destructive
	FreeEncoderResources();
}

/** 
 * Decompresses the platform dependent format to raw PCM. Used for quality previewing.
 *
 * @param	SrcData			Uncompressed PCM data
 * @param	DstData			Uncompressed PCM data after being compressed		
 * @param	QualityInfo		All the information the compressor needs to compress the audio	
 */
INT FXenonSoundCooker::Recompress( const BYTE* SrcBuffer, BYTE* DestBuffer, FSoundQualityInfo* QualityInfo )
{
	wchar_t TempFile[MAX_PATH];
	wchar_t TempOutFile[MAX_PATH];
	wchar_t Command[1024];

	// Cannot quality preview multichannel sounds
	if( QualityInfo->NumChannels > 2 )
	{
		return( -1 );
	}

	// Cook an XMA2 or XWMA format file as normal
	if( !Cook( SrcBuffer, QualityInfo ) )
	{
		return( -1 );
	}

	if( !GetTempFile( TempFile, TEXT(".xma") ) )
	{
		return( -1 );
	}

	FILE* File = OpenFile( TempFile, TEXT("wb") );
	if( !File )
	{
		return( -1 );
	}

	// Write out the encoded file
	if( EncodedBufferFormatSize == sizeof( XMA2WAVEFORMATEX ) )
	{
		if( !WriteXMA2File( File ) )
		{
			return( -1 );
		}
	}
	else if( EncodedBufferFormatSize == sizeof( WAVEFORMATEX ) )
	{
		if( !WriteXWMAFile( File ) )
		{
			return( -1 );
		}
	}
	else
	{
		return( -1 );
	}

	fclose( File );

	if( !GetTempFile( TempOutFile, TEXT(".wav") ) )
	{
		return( -1 );
	}

	// Run the child process to convert back to awav
	if( EncodedBufferFormatSize == sizeof( XMA2WAVEFORMATEX ) )
	{
		swprintf_s( Command, 1024, TEXT("%s\\bin\\win32\\xma2encode.exe %s /DecodeToPCM %s"), XDKPath, TempFile, TempOutFile );
		OutputStringToLog("Calling child process: %s\n", Command);
		if( !RunChildProcess( Command ) )
		{
			return( -1 );
		}
	}
	else if( EncodedBufferFormatSize == sizeof( WAVEFORMATEX ) )
	{
		swprintf_s( Command, 1024, TEXT("%s\\bin\\win32\\xwmaencode.exe %s %s"), XDKPath, TempFile, TempOutFile );
		OutputStringToLog("Calling child process: %s\n", Command);
		if( !RunChildProcess( Command ) )
		{
			return( -1 );
		}
	}
	else
	{
		return( -1 );
	}

	File = OpenFile( TempOutFile, TEXT("rb") );
	if( !File )
	{
		return( -1 );
	}

	if( !ReadWaveFile( File, DestBuffer, QualityInfo ) )
	{
		return( -1 );
	}

	fclose( File );

	DeleteFile( TempFile );
	DeleteFile( TempOutFile );

	return( GetCookedDataSize() );
}

/** 
 * Helper function used to free resources allocated by XMA encoder.
 */
void FXenonSoundCooker::FreeEncoderResources( void )
{
	free( EncodedBuffer );
	free( EncodedBufferFormat );
	free( SeekTable );

	EncodedBuffer = NULL;
	EncodedBufferFormat = NULL;
	SeekTable = NULL;
	NumStreams = 0;
	ErrorString[0] = 0;
}

/**
 * Perform any skeletal mesh cooker initialization
 */
void FXenonSkeletalMeshCooker::Init(void)
{
}

/**
 * Cook the (16-bit) index buffer of a skeletal mesh
 *
 * @param InputIndices Input array of index data
 * @param NumTriangles Number of triangles represented by the index data - InputIndices will have 3 times this number of indices
 * @param OutputIndices Output array of optimized index data
 */
void FXenonSkeletalMeshCooker::CookMeshElement(const FSkeletalMeshFragmentCookInfo& ElementInfo, FSkeletalMeshFragmentCookOutputInfo& OutInfo)
{
	// no optimization needed, just copy the data over
	memcpy(OutInfo.NewIndices, ElementInfo.Indices, ElementInfo.NumTriangles * 3 * sizeof(DWORD) );
}

/**
* Perform any static mesh cooker initialization
*/
void FXenonStaticMeshCooker::Init()
{
}

/**
 * Cooks a mesh element.
 * @param ElementInfo - Information about the element being cooked
 * @param OutIndices - Upon return, contains the optimized index buffer.
 * @param OutPartitionSizes - Upon return, points to a list of partition sizes in number of triangles.
 * @param OutNumPartitions - Upon return, contains the number of partitions.
 * @param OutVertexIndexRemap - Upon return, points to a list of vertex indices which maps from output vertex index to input vertex index.
 * @param OutNumVertices - Upon return, contains the number of vertices indexed by OutVertexIndexRemap.
 */
void FXenonStaticMeshCooker::CookMeshElement(FMeshFragmentCookInfo& ElementInfo, FMeshFragmentCookOutputInfo& OutInfo)
{
	// no optimization needed, just copy the data over
	memcpy(OutInfo.NewIndices, ElementInfo.Indices, ElementInfo.NumTriangles * 3 * sizeof(WORD) );
}




/** generates/stores D3D macros used for shader compilation */
class FD3DMacroHelper
{
public:
	/** constructor (default) */
	FD3DMacroHelper() 
		: NumMacros(0) 
	{}

	/** 
	* constructor
	*
	* @param	DefinitionStr - shader definitions and values
	* format of string is as follows:
	* "-DTESTA=1-DTESTB=2-DTESTC=3"
	*/
	FD3DMacroHelper( const char* DefinitionStr )
		: NumMacros(0)
	{		
		// make a copy of str we can modify
		char* DefinitionStrCopy = strcpy( new char[strlen(DefinitionStr)+1], DefinitionStr );
		// add definition strings as D3DXMACROs
		char* strNext = strstr(DefinitionStrCopy, "-D");
		while( strNext )
		{
			// name of define
			char* strDefine	= strNext+2;
			// get the next definition
			strNext = strstr(strDefine, "-D");
			// value of define
			char* strValue = strstr(strDefine, "=");			
			if( strValue )
			{
				// terminate strs
				if( strNext ) 
				{
					*strNext = '\0';
				}
				*strValue = '\0';
				strValue += 1;
				// add the define/value pair
				AddMacro( strDefine, strValue );
			}
		}
		// add HLSL compile define
		AddMacro("COMPILER_HLSL", "1");
		// add XBOX=1 compile define
		AddMacro("XBOX", "1");
		// add SUPPORTS_DEPTH_TEXTURES=1 compile define
		AddMacro("SUPPORTS_DEPTH_TEXTURES", "1");
		// terminate macros
		Macros[NumMacros].Name = NULL;
		Macros[NumMacros].Definition = NULL;

		delete[] DefinitionStrCopy;
	}

	/** destructor */
	~FD3DMacroHelper()
	{
		// free any macro strings
		for( int Idx=0; Idx < NumMacros; Idx++ )
		{
			delete[] Macros[Idx].Definition;
			delete[] Macros[Idx].Name;
		}
		NumMacros=0;
	}

	/** retrieve ptr to the D3D macro array */
	D3DXMACRO* GetDataPtr()
	{
		return Macros;
	}

private:
	enum { MAXMACROS=256 };

	void AddMacro( const char* NameStr, const char* DefinitionStr )
	{
		// copy name of define
		Macros[NumMacros].Name = strcpy(new char[strlen(NameStr)+1], NameStr);
		// copy value of define
		Macros[NumMacros].Definition = strcpy(new char[strlen(DefinitionStr)+1], DefinitionStr);
		// total added
		NumMacros++; 
		assert(NumMacros<MAXMACROS);        
	}

	D3DXMACRO Macros[MAXMACROS];
	int NumMacros;
};

/** Saves UPDB contents to the path at CompileParameters.UPDBPath */
bool SaveXenonPDB(D3DXSHADER_COMPILE_PARAMETERSA& CompileParameters)
{
	if (!CompileParameters.pUPDBBuffer)
	{
		return false;
	}

	HANDLE FileHandle = CreateFileA(CompileParameters.UPDBPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (FileHandle == INVALID_HANDLE_VALUE)
	{
		CompileParameters.pUPDBBuffer->Release();
		CompileParameters.pUPDBBuffer = NULL;
		return false;
	}

	DWORD BytesWritten = 0;
	DWORD NumBytes = CompileParameters.pUPDBBuffer->GetBufferSize();

	if (!WriteFile(FileHandle, CompileParameters.pUPDBBuffer->GetBufferPointer(), NumBytes, &BytesWritten, NULL))
	{
		CompileParameters.pUPDBBuffer->Release();
		CompileParameters.pUPDBBuffer = NULL;
		return false;
	}

	CloseHandle(FileHandle);

	CompileParameters.pUPDBBuffer->Release();
	CompileParameters.pUPDBBuffer = NULL;

	return true;
}


/**
* An implementation of the D3DX include interface
*/
class FXenonIncludeHandler : public ID3DXInclude
{
public:

	STDMETHOD(Open)(
		D3DXINCLUDE_TYPE /*Type*/,
		LPCSTR Name,
		LPCVOID /*ParentData*/,
		LPCVOID* Data,
		UINT* Bytes,
		LPSTR /*pFullPath*/,
		DWORD /*cbFullPath*/)
	{
		bool bFoundInclude = false;
		// Try to find the file in the in-memory includes first
		for (INT IncludeIndex = 0; IncludeIndex < NumIncludes; IncludeIndex++)
		{
			if (strstr(IncludeFileNames[IncludeIndex], Name) != NULL)
			{
				bFoundInclude = true;
				UINT DataLength = (UINT)strlen(IncludeFileContents[IncludeIndex]);
				CHAR* OutData = new CHAR[DataLength];
				memcpy(OutData, IncludeFileContents[IncludeIndex], DataLength);
				*Data = OutData;
				*Bytes = DataLength;
				break;
			}
		}

		if (!bFoundInclude)
		{
			FILE* IncludeFile = NULL;
			bool bPathIncluded = strchr(Name, '\\') != NULL;
#ifndef NDEBUG
			bool bAbsolutePath = strstr(Name, ":\\") != NULL;
#endif
			int FilenameLength = ( int )( (bPathIncluded ? 0 : strlen(IncludeDirectory)) + strlen(Name) + 1 );
			char* Filename = new char[FilenameLength];
			if (bPathIncluded)
			{
				assert(bAbsolutePath);
				strcpy(Filename, Name);
			}
			else
			{
				strcpy(Filename, IncludeDirectory);
				strcat(Filename, Name);
			}
		
			// Read the include from the shader directory
			errno_t ErrorCode = fopen_s(&IncludeFile, Filename, "rb");
			if (ErrorCode == 0 && IncludeFile != NULL)
			{
				int FileId = _fileno(IncludeFile);
				if (FileId >= 0)
				{
					long FileLength = _filelength(FileId);
					LPSTR IncludeFileContents = new CHAR[FileLength];
					size_t NumRead = fread(IncludeFileContents,sizeof(CHAR),FileLength,IncludeFile);
					if (NumRead == (size_t)FileLength)
					{
						*Data = IncludeFileContents;
						*Bytes = (UINT)NumRead;
						bFoundInclude = true;
					}
					else
					{
						delete [] IncludeFileContents;
					}
				}
				fclose(IncludeFile);
			}
			delete [] Filename;
		}

		return bFoundInclude ? S_OK : S_FALSE;
	}

	STDMETHOD(Close)(LPCVOID Data)
	{
		delete [] Data;
		return S_OK;
	}

	FXenonIncludeHandler(const char* InIncludeDirectory, char* const * InIncludeFileNames, char* const * InIncludeFileContents, int InNumIncludes) :
		IncludeDirectory(InIncludeDirectory),
		IncludeFileNames(InIncludeFileNames),
		IncludeFileContents(InIncludeFileContents),
		NumIncludes(InNumIncludes)
	{}

private:

	const char* IncludeDirectory;
	char* const * IncludeFileNames;
	char* const * IncludeFileContents;
	int NumIncludes;
};

bool FXenonShaderPrecompiler::PrecompileShader(
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
	char* Errors)
{
	bool Result=true;

#if _WIN64
	// Required when calling Xbox D3DX libraries from 64 bit windows
	D3DInitializeHeap();
#endif

	// init outputs
	ConstantBuffer[0] = '\0';
	Errors[0] = '\0';
	BytecodeSize = 0;

	// parse macro defines
	FD3DMacroHelper D3DMacroHelper(Definitions);

	// shader byte code and constant table
	ID3DXBuffer* ShaderBuf=NULL;
	ID3DXConstantTable* ConstantTable=NULL;
	ID3DXBuffer* ErrorBuf=NULL;
	FXenonIncludeHandler IncludeHandler(IncludeDirectory, IncludeFileNames, IncludeFileContents, NumIncludes);

	D3DXSHADER_COMPILE_PARAMETERSA CompileParameters;
	ZeroMemory(&CompileParameters, sizeof(CompileParameters));
	CompileParameters.Flags = D3DXSHADEREX_OPTIMIZE_UCODE | D3DXSHADEREX_REMOVE_UNUSED_PS_INPUTS;
	if (bDumpingShaderPDBs)
	{
		CompileParameters.Flags |= D3DXSHADEREX_GENERATE_UPDB;

		struct FGuid
		{
			DWORD A,B,C,D;
		};

		FGuid ShaderGUID;
		// Save this shader's PDB's with a unique filename
		//@todo - error handling
		CoCreateGuid((GUID*)&ShaderGUID);

		char UPDBPath[MAX_PATH];
		char GUIDTemp[MAX_PATH];

		strcpy(UPDBPath, ShaderPDBPath);
		_ultoa(ShaderGUID.A, GUIDTemp, 10);
		strcat(UPDBPath, GUIDTemp);
		_ultoa(ShaderGUID.B, GUIDTemp, 10);
		strcat(UPDBPath, GUIDTemp);
		_ultoa(ShaderGUID.C, GUIDTemp, 10);
		strcat(UPDBPath, GUIDTemp);
		_ultoa(ShaderGUID.D, GUIDTemp, 10);
		strcat(UPDBPath, GUIDTemp);
		strcat(UPDBPath, ".updb");

		CompileParameters.UPDBPath = UPDBPath;
	}

	// compile the shader
	HRESULT hr = D3DXCompileShaderFromFileExA(
		ShaderPath,
		D3DMacroHelper.GetDataPtr(),
		&IncludeHandler,
		EntryFunction,
		bIsVertexShader ? "vs_3_0" : "ps_3_0",
		CompileFlags,
		&ShaderBuf,
		&ErrorBuf,
		&ConstantTable,
		&CompileParameters
		);

	// return any errors
	if (FAILED(hr))
	{
		// use the given error string if present
		if (ErrorBuf)
		{
			strcpy(Errors, (char*)ErrorBuf->GetBufferPointer());
		}
		// otherwise, display an unknown error
		else
		{
			strcpy(Errors, "Unknown Xenon D3DX shader compile error");
		}
		Result = false;
	}
	else
	{
		if (bDumpingShaderPDBs)
		{
			SaveXenonPDB(CompileParameters);
		}

		// Determine how large the stripped shader will be.
		DWORD StrippedCodeSize = 0;
		XGMicrocodeDeleteConstantTable( ShaderBuf->GetBufferPointer(), NULL, 0, &StrippedCodeSize );

		// Strip the shader and place it in BytecodeBuffer.
		XGMicrocodeDeleteConstantTable( ShaderBuf->GetBufferPointer(), BytecodeBuffer, StrippedCodeSize, &StrippedCodeSize );
		BytecodeSize = StrippedCodeSize;

		// Read the constant table description.
		D3DXCONSTANTTABLE_DESC	Desc;
		ConstantTable->GetDesc(&Desc);

		// create constant output string
		char TempStr[64];
		for( UINT ConstantIdx=0; ConstantIdx < Desc.Constants; ConstantIdx++ )
		{
			// Read the constant description.
			D3DXCONSTANT_DESC ConstantDesc;
			UINT NumConstants = 1;
			ConstantTable->GetConstantDesc(
				ConstantTable->GetConstant(NULL,ConstantIdx),
				&ConstantDesc,
				&NumConstants
				);

			const bool bIsSampler =
				ConstantDesc.Type == D3DXPT_SAMPLER ||
				ConstantDesc.Type == D3DXPT_SAMPLER1D ||
				ConstantDesc.Type == D3DXPT_SAMPLER2D ||
				ConstantDesc.Type == D3DXPT_SAMPLER3D ||
				ConstantDesc.Type == D3DXPT_SAMPLERCUBE;
			const UINT RegisterSize = bIsSampler ? 1 : (sizeof(FLOAT) * 4);
			UINT RegisterIndex = ConstantDesc.RegisterIndex;

			if ( bIsVertexShader && bIsSampler )
			{
				RegisterIndex += D3DVERTEXTEXTURESAMPLER0;
			}

			// add constant string
			// eg. "WorldToLocal,100,4 Scale,101,1"

			// constant name
			strcat(ConstantBuffer, ConstantDesc.Name);
			strcat(ConstantBuffer, ",");

			// register index
			strcat(ConstantBuffer, _itoa(RegisterIndex * RegisterSize,TempStr,10));
			strcat(ConstantBuffer, ",");

			// register count
			strcat(ConstantBuffer, _itoa(ConstantDesc.RegisterCount * RegisterSize,TempStr,10));
			strcat(ConstantBuffer, " ");
		}
	}

	if( ShaderBuf )
	{
		ShaderBuf->Release();
	}
	if( ConstantTable )
	{
		ConstantTable->Release();
	}
	if( ErrorBuf )
	{
		ErrorBuf->Release();
	}

	return Result;
}

bool FXenonShaderPrecompiler::PreprocessShader(
	const char* ShaderPath, 
	const char* Definitions, 
	const char* IncludeDirectory,
	char* const * IncludeFileNames,
	char* const * IncludeFileContents,
	int NumIncludes,
	unsigned char* ShaderText, 
	int& ShaderTextSize, 
	char* Errors)
{
	bool Result=true;

#if _WIN64
	// Required when calling Xbox D3DX libraries from 64 bit windows
	D3DInitializeHeap();
#endif

	// init outputs
	Errors[0] = '\0';
	ShaderTextSize = 0;

	// parse macro defines
	FD3DMacroHelper D3DMacroHelper(Definitions);
	FXenonIncludeHandler IncludeHandler(IncludeDirectory, IncludeFileNames, IncludeFileContents, NumIncludes);

	// shader code
	ID3DXBuffer* ShaderBuf=NULL;
	ID3DXBuffer* ErrorBuf=NULL;

	// preprocess the shader
	HRESULT hr = D3DXPreprocessShaderFromFileA(
		ShaderPath,
		D3DMacroHelper.GetDataPtr(),
		&IncludeHandler,
		&ShaderBuf,
		&ErrorBuf);

	// return any errors
	if (FAILED(hr))
	{
		// use the given error string if present
		if (ErrorBuf)
		{
			strcpy(Errors, (char*)ErrorBuf->GetBufferPointer());
		}
		// otherwise, display an unknown error
		else
		{
			strcpy(Errors, "Unknown D3DX shader compile error");
		}
		Result = false;
	}
	else
	{
		// copy shader text
		ShaderTextSize = ShaderBuf->GetBufferSize();
		memcpy( ShaderText, ShaderBuf->GetBufferPointer(), ShaderTextSize );
	}

	if( ShaderBuf )
	{
		ShaderBuf->Release();
	}
	if( ErrorBuf )
	{
		ErrorBuf->Release();
	}

	return Result;
}

bool FXenonShaderPrecompiler::DisassembleShader(
	const DWORD *ShaderByteCode, 
	unsigned char* ShaderText, 
	int& ShaderTextSize)
{
	bool Result=false;

#if _WIN64
	// Required when calling Xbox D3DX libraries from 64 bit windows
	D3DInitializeHeap();
#endif

	// init outputs
	ShaderTextSize = 0;

	// shader asm code
	ID3DXBuffer* ShaderBuf=NULL;

	// disassemble the shader
	HRESULT hr = D3DXDisassembleShader(
		ShaderByteCode,
		false,
		NULL,
		&ShaderBuf);

	// return any errors
	if (SUCCEEDED(hr))
	{
		// copy byte code
		ShaderTextSize = ShaderBuf->GetBufferSize();
		memcpy( ShaderText, ShaderBuf->GetBufferPointer(), ShaderTextSize );
		Result = true;
	}

	if( ShaderBuf )
	{
		ShaderBuf->Release();
	}

	return Result;
}

bool FXenonShaderPrecompiler::CreateShaderCompileCommandLine(
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

	// fxc is our command line compiler
	// make sure we are using the XDK fxc and not the DX SDK one
	// surround with quotes since the XEDK path probably has spaces
	strcat(CommandStr, "\"%XEDK%\\bin\\win32\\fxc\" ");
	strcat(CommandStr, ShaderPath);

	// add definitions
	if(Definitions != NULL && !bPreprocessed)
	{
		// parse macro defines
		FD3DMacroHelper D3DMacroHelper(Definitions);
		D3DXMACRO* Macros = D3DMacroHelper.GetDataPtr();
		for (int i = 0; Macros[i].Name != NULL; i++)
		{
			strcat(CommandStr, " /D ");
			strcat(CommandStr, Macros[i].Name);
			strcat(CommandStr, "=");
			strcat(CommandStr, Macros[i].Definition);
		}
	}

	if(EntryFunction != NULL)
	{
		// add the entry point reference
		strcat(CommandStr, " /E");
		strcat(CommandStr, EntryFunction);
	}

	if(IncludePath != NULL && !bPreprocessed)
	{
		// add the include path
		strcat(CommandStr, " /I ");
		strcat(CommandStr, IncludePath);
	}

	// go through and add other switches
	if(CompileFlags & D3DXSHADER_PREFER_FLOW_CONTROL)
	{
		strcat(CommandStr, " /Gfp");
	}
	if(CompileFlags & D3DXSHADER_DEBUG)
	{
		strcat(CommandStr, " /Zi");
	}
	if(CompileFlags & D3DXSHADER_SKIPOPTIMIZATION)
	{
		strcat(CommandStr, " /Od");
	}
	if(CompileFlags & D3DXSHADER_AVOID_FLOW_CONTROL)
	{
		strcat(CommandStr, " /Gfa");
	}
	if(CompileFlags & D3DXSHADER_SKIPVALIDATION)
	{
		strcat(CommandStr, " /Vd");
	}

	strcat(CommandStr, " /XQremoveunusedpsinputs");

	// add the target instruction set
	strcat(CommandStr, bIsVertexShader ? " /Tvs_3_0" : " /Tps_3_0");

	// add a newline and a pause
	strcat(CommandStr, "\r\n pause");

	return true;
}
