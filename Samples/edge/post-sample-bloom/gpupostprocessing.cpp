/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <cell/cell_fs.h>
#include <cell/gcm.h>
#include <cell/spurs.h>
#include <cell/sysmodule.h>
#include <sys/spu_initialize.h>
#include <ppu_intrinsics.h>
#include <spu_printf.h>
#include <cell/dbgfont.h>

#include "edge/post/edgepost.h"
#include "edge/post/edgepost_ppu.h"
#include "main.h"
#include "spu/postParams.h"
#include "../common/profile_bars.h"
#include "gcm/FWCellGCMWindow.h"
#include "gcmutil.h"

using namespace cell;
using namespace cell::Gcm;


static void MakeBufferFromMrt( CellGcmSurface& surface, const CellGcmSurface& source, uint32_t index )
{
	// setup surface structure
	memset( &surface, 0, sizeof(CellGcmSurface));
	surface.type = CELL_GCM_SURFACE_PITCH;
	surface.antialias = CELL_GCM_SURFACE_CENTER_1;
	surface.colorFormat = CELL_GCM_SURFACE_A8R8G8B8;
	surface.colorTarget = CELL_GCM_SURFACE_TARGET_0;
	for ( int j = 0; j < CELL_GCM_MRT_MAXCOUNT; ++j )
	{
		surface.colorLocation[j] = CELL_GCM_LOCATION_LOCAL;
		surface.colorPitch[j] = 64;
	}
	surface.colorOffset[0] = source.colorOffset[index];
	surface.colorPitch[0] = source.colorPitch[index];
	surface.depthFormat = source.depthFormat;
	surface.depthPitch = source.depthPitch;
	surface.width = source.width;
	surface.height = source.height;
	surface.x = source.x;
	surface.y = source.y;
}

void SampleApp::InitVRamBuffers( uint32_t width, uint32_t height )
{
	(void)width;
	(void)height;

	// create buffers from the Mrt target
	mRenderTarget.colorOffset[0] = mColorOffsets[0];
	MakeBufferFromMrt( mColorSurface[0], mRenderTarget, 0 );
	mRenderTarget.colorOffset[0] = mColorOffsets[1];
	MakeBufferFromMrt( mColorSurface[1], mRenderTarget, 0 );

	uint8_t* data;
	uint32_t pitch, offset, memsize;

	// Allocate a half resolution buffer in VRAM
	pitch = cellGcmGetTiledPitchSize( 640 * 4);
	memsize = pitch * (( 360 + 31) & ~31);
	memsize = ( memsize + 0xffff) & ~0xffff;
	data = (uint8_t*)cellGcmUtilAllocateLocalMemory( memsize, 0x10000);
	cellGcmAddressToOffset( data, &offset);

	// Make it into a tiled region
	cellGcmSetTileInfo( 6, CELL_GCM_LOCATION_LOCAL, offset, memsize, pitch, CELL_GCM_COMPMODE_DISABLED, 0, 0);
	cellGcmBindTile( 6);

	// Setup surface descriptor
	memset( &mHalfResVram, 0, sizeof(CellGcmSurface));
	mHalfResVram.type = CELL_GCM_SURFACE_PITCH;
	mHalfResVram.antialias = CELL_GCM_SURFACE_CENTER_1;
	mHalfResVram.colorFormat = CELL_GCM_SURFACE_A8R8G8B8;
	mHalfResVram.colorTarget = CELL_GCM_SURFACE_TARGET_0;
	for ( int j = 1; j < CELL_GCM_MRT_MAXCOUNT; ++j )
		mHalfResVram.colorPitch[j] = pitch;
	mHalfResVram.colorPitch[0] = pitch;
	mHalfResVram.colorOffset[0] = offset;
	mHalfResVram.depthFormat = CELL_GCM_SURFACE_Z24S8;
	mHalfResVram.depthPitch = pitch;
	mHalfResVram.width = 640;
	mHalfResVram.height = 360;

	// source texture used for tonemapping
	memset( &mOriginalTexture, 0, sizeof(CellGcmTexture));
	mOriginalTexture.format = CELL_GCM_TEXTURE_A8R8G8B8 | CELL_GCM_TEXTURE_LN;
	mOriginalTexture.mipmap = 1;
	mOriginalTexture.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
	mOriginalTexture.cubemap = CELL_GCM_FALSE;
	mOriginalTexture.remap = 
		CELL_GCM_TEXTURE_REMAP_REMAP << 14 |
		CELL_GCM_TEXTURE_REMAP_REMAP << 12 |
		CELL_GCM_TEXTURE_REMAP_REMAP << 10 |
		CELL_GCM_TEXTURE_REMAP_REMAP << 8 |
		CELL_GCM_TEXTURE_REMAP_FROM_B << 6 |
		CELL_GCM_TEXTURE_REMAP_FROM_G << 4 |
		CELL_GCM_TEXTURE_REMAP_FROM_R << 2 |
		CELL_GCM_TEXTURE_REMAP_FROM_A;
	mOriginalTexture.width = 1280;
	mOriginalTexture.height = 720;
	mOriginalTexture.depth = 1;
	mOriginalTexture.location = CELL_GCM_LOCATION_LOCAL;
	mOriginalTexture.pitch = mRenderTarget.colorPitch[0];
	mOriginalTexture.offset = 0;
}

static void CreateTextureFromSurface( const CellGcmSurface& surface, CellGcmTexture& tex, 
					bool bNormalized = true, uint32_t format = CELL_GCM_TEXTURE_A8R8G8B8 )
{
	tex.format = format | CELL_GCM_TEXTURE_LN;
	tex.format |= bNormalized ? CELL_GCM_TEXTURE_NR : 0;
	tex.mipmap = 1;
	tex.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
	tex.cubemap = CELL_GCM_FALSE;
	tex.remap = 
			CELL_GCM_TEXTURE_REMAP_REMAP << 14 |
			CELL_GCM_TEXTURE_REMAP_REMAP << 12 |
			CELL_GCM_TEXTURE_REMAP_REMAP << 10 |
			CELL_GCM_TEXTURE_REMAP_REMAP << 8 |
			CELL_GCM_TEXTURE_REMAP_FROM_B << 6 |
			CELL_GCM_TEXTURE_REMAP_FROM_G << 4 |
			CELL_GCM_TEXTURE_REMAP_FROM_R << 2 |
			CELL_GCM_TEXTURE_REMAP_FROM_A;
	tex.width = surface.width;
	tex.height = surface.height;
	tex.depth = 1;
	tex.location = surface.colorLocation[0];
	tex._padding = 0;
	tex.pitch = surface.colorPitch[0];
	tex.offset = surface.colorOffset[0] + surface.x * 4 + surface.y * surface.colorPitch[0];
}

static void SetTexture( int stage, const CellGcmSurface& surface, bool bNormalized = true, uint32_t format = CELL_GCM_TEXTURE_A8R8G8B8 )
{
	CellGcmTexture texture;
	CreateTextureFromSurface( surface, texture, bNormalized, format );
	cellGcmSetTexture( stage, &texture );
}

void SampleApp::SetViewport( uint32_t w, uint32_t h )
{
	float scale[4],offset[4];

	const uint16_t x = 0;
	const uint16_t y = 0;
	const float min = 0.0f;
	const float max = 1.0f;
	scale[0] = w * 0.5f;
	scale[1] = h * -0.5f;
	scale[2] = (max - min) * 0.5f;
	scale[3] = 0.0f;
	offset[0] = x + scale[0];
	offset[1] = y + h * 0.5f;
	offset[2] = (max + min) * 0.5f;
	offset[3] = 0.0f;
	cellGcmSetViewport( x, y, w, h, min, max, scale, offset);
	cellGcmSetScissor( x, y, w, h);
}

void SampleApp::SetTarget( const CellGcmSurface& dst )
{
	cellGcmSetSurfaceWindow( &dst, CELL_GCM_WINDOW_ORIGIN_BOTTOM, CELL_GCM_WINDOW_PIXEL_CENTER_INTEGER );
	SetViewport( dst.width, dst.height );
}

void SampleApp::Tonemap( const CellGcmTexture& original, const CellGcmTexture& accumulator )
{
	// set tonemap parameters
	CGparameter tonemapParameters = cellGcmCgGetNamedParameter( mTonemapCompositeFp.mCgProgram, "parameters" );
	float bloomstrenght = mPostParams.m_bloomStrength * ( mPostParams.m_maxLuminance - mPostParams.m_minLuminance ) * mPostParams.m_exposureLevel;
	float v[] = { mAvgLuminance, mPostParams.m_whitePoint, mPostParams.m_middleGrey, bloomstrenght };
	cellGcmSetFragmentProgramParameter( mTonemapCompositeFp.mCgProgram, tonemapParameters, v, mTonemapCompositeFp.mUCodeOffset );

	// set shaders
	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );
	cellGcmSetFragmentProgram( mTonemapCompositeFp.mCgProgram, mTonemapCompositeFp.mUCodeOffset );

	// textures
	cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureFilter( 1, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureControl( 1, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	cellGcmSetTextureAddress( 1, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	cellGcmSetTexture( 0, &original );
	cellGcmSetTexture( 1, &accumulator );

	cellGcmSetSurfaceWindow( &mFrameTarget, CELL_GCM_WINDOW_ORIGIN_BOTTOM, CELL_GCM_WINDOW_PIXEL_CENTER_INTEGER );
	SetViewport( 1280, 720 );

	// Enable gamma corrected write
	cellGcmSetFragmentProgramGammaEnable( CELL_GCM_TRUE );

	// fullscreen quad
	cellGcmSetVertexDataArray( 0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray( 0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);

	// Disable gamma corrected write
	cellGcmSetFragmentProgramGammaEnable( CELL_GCM_FALSE );
}

void SampleApp::LuvDownsample( const CellGcmSurface& src, const CellGcmSurface& dst )
{
	// set shaders
	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );
	cellGcmSetFragmentProgram( mLuvDownsampleFp.mCgProgram, mLuvDownsampleFp.mUCodeOffset );

	// set target
	SetTarget( dst );

	// textures
	cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 0, src );

	// fullscreen quad
	cellGcmSetVertexDataArray( 0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray( 0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);

	// Disable gamma corrected write
	cellGcmSetFragmentProgramGammaEnable( CELL_GCM_FALSE );
}