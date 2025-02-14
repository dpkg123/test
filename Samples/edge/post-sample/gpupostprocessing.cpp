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


static void MakeBuffer( CellGcmSurface& surface, uint32_t offset, 
		uint32_t pitch, uint32_t x, uint32_t y, uint32_t w, uint32_t h )
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
		surface.colorPitch[j] = pitch;
	}
	surface.colorOffset[0] = offset;
	surface.colorPitch[0] = pitch;
	surface.depthFormat = CELL_GCM_SURFACE_Z24S8;
	surface.depthPitch = pitch;
	surface.width = w;
	surface.height = h;
	surface.x = x;
	surface.y = y;
}

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

void SampleApp::InitGpuPostprocessing( uint32_t width, uint32_t height )
{
	// get pitch for tiled surface
	uint32_t pitch = cellGcmGetTiledPitchSize( width * 4);

	// calculate memory requirements
	uint32_t frameSize = pitch * (( height + 31) & ~31);
	uint32_t memSize = ( frameSize + 0xffff) & ~0xffff;

	printf( "Allocating GPU Postprocessing arena size in local (tiled) memory : %d\n", memSize );

	// allocate memory
	uint8_t* pTemp0 = (uint8_t*)cellGcmUtilAllocateLocalMemory( memSize, 0x10000);

	// set Tile info
	uint32_t offset;
	int ret = cellGcmAddressToOffset( pTemp0, &offset);
	assert( ret == CELL_OK );

#if 1
	ret = cellGcmSetTileInfo( 5, CELL_GCM_LOCATION_LOCAL, offset, memSize, pitch, CELL_GCM_COMPMODE_DISABLED, 0, 0);
	assert( ret == CELL_OK );

	ret = cellGcmBindTile( 5);
	assert( ret == CELL_OK );
#endif

	// create surfaces
	uint8_t* addr = pTemp0;
	cellGcmAddressToOffset( addr, &offset);
	assert( ret == CELL_OK );

	uint32_t w = width;
	uint32_t h = height;

	// full resolution
	MakeBuffer( mPostSurfaces_1, offset, pitch, 0, 0, w, h );

	// half resolution
	uint32_t w2 = w >> 1;
	uint32_t h2 = h >> 1;
	MakeBuffer( mPostSurfaces_2[0], offset, pitch, 0, 0, w2, h2 );
	MakeBuffer( mPostSurfaces_2[1], offset, pitch, w2, 0, w2, h2 );
	MakeBuffer( mPostSurfaces_2[2], offset, pitch, 0, h2, w2, h2 );
	MakeBuffer( mPostSurfaces_2[3], offset, pitch, w2, h2, w2, h2 );

	// quarter resolution
	uint32_t w4 = w / 4;
	uint32_t h4 = h / 4;
	for ( int y = 0; y < 3; ++y )
		for ( int x = 0; x < 3; ++x )
			MakeBuffer( mPostSurfaces_4[y*4+x], offset, pitch, w4 * x, h4 * y, w4, h4 );

	// 1/8 resolution
	uint32_t w8 = w4 >> 1;
	uint32_t h8 = h4 >> 1;
	for ( int y = 0; y < 8; ++y )
		for ( int x = 0; x < 8; ++x )
			MakeBuffer( mPostSurfaces_8[y*8+x], offset, pitch, w8 * x, h8 * y, w8, h8 );
	
	// 1/16 buffers used by bloom effect
	uint32_t w16 = w8 >> 1;
	uint32_t h16 = h8 >> 1;
	MakeBuffer( mPostSurfaces_16[0], offset, pitch, w16 * 14, h16 * 15, w16, h16 );
	MakeBuffer( mPostSurfaces_16[1], offset, pitch, w16 * 15, h16 * 15, w16, h16 );

	// create buffers from Mrt target
	MakeBufferFromMrt( mMotionAndDepthSurface, mMrtTarget, 1 );
	
	mMrtTarget.colorOffset[0] = mMrtColorOffsets[0];
	MakeBufferFromMrt( mColorSurface[0], mMrtTarget, 0 );
	mMrtTarget.colorOffset[0] = mMrtColorOffsets[1];
	MakeBufferFromMrt( mColorSurface[1], mMrtTarget, 0 );
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

static void SetTexture( int stage, const CellGcmSurface& surface, 
					bool bNormalized = true, uint32_t format = CELL_GCM_TEXTURE_A8R8G8B8 )
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

void SampleApp::SetupGpuResample( const CellGcmSurface& src, const CellGcmSurface& dst, bool bLinear )
{
	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );
	cellGcmSetFragmentProgram( mResampleFp.mCgProgram, mResampleFp.mUCodeOffset );
	
	// destination
	SetTarget( dst );

	// source texture
	if ( bLinear)
	{
		cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	} else {
		cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	}

	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 0, src );

	// fullscreen quad
	cellGcmSetVertexDataArray(0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray(0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::SetupDepthDownsample( const CellGcmSurface& src, const CellGcmSurface& dst )
{
	// NOTE: measured with GPAD. Twice as fast due to texture cache trashing
	cellGcmCgSetRegisterCount( mDepthdownsampleFp.mCgProgram, 10);

	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );
	cellGcmSetFragmentProgram( mDepthdownsampleFp.mCgProgram, mDepthdownsampleFp.mUCodeOffset );

	// destination
	SetTarget( dst );

	// source texture
	cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 0, src );

	// fullscreen quad
	cellGcmSetVertexDataArray(0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray(0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::SetupGpuBloomCapture( const CellGcmSurface& src, const CellGcmSurface& dst )
{
	CGparameter captureRange = cellGcmCgGetNamedParameter( mBloomCaptureFp.mCgProgram, "captureRange" );
	assert( captureRange );
	float v[] = {
		mPostParams.m_exposureLevel, 
		mPostParams.m_minLuminance, 
		1.0f/( mPostParams.m_maxLuminance - mPostParams.m_minLuminance ), 
		0.0f, 
	};
	cellGcmSetFragmentProgramParameter( mBloomCaptureFp.mCgProgram, captureRange, v, mBloomCaptureFp.mUCodeOffset );
	
	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );
	cellGcmSetFragmentProgram( mBloomCaptureFp.mCgProgram, mBloomCaptureFp.mUCodeOffset );

	// destination
	SetTarget( dst );
	
	cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 0, src );

	// fullscreen quad
	cellGcmSetVertexDataArray(0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray(0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::SetupGpuMotionBlur( const CellGcmSurface& src, const CellGcmSurface& motion, const CellGcmSurface& dst )
{
	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );
	cellGcmSetFragmentProgram( mMotionblurFp.mCgProgram, mMotionblurFp.mUCodeOffset );

	// destination
	SetTarget( dst );

	// source texture
	cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 0, src );

	// motion vectors
	cellGcmSetTextureFilter( 1, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 1, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 1,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 1, motion );

	// fullscreen quad
	cellGcmSetVertexDataArray( 0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray( 0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::SetupGpuFuzziness( const CellGcmSurface& depth, const CellGcmSurface& dst, bool bNear )
{
	float v[4], m[4];
	float k0 = mPostParams.m_nearSharp - mPostParams.m_nearFuzzy;
	float k1 = mPostParams.m_farFuzzy - mPostParams.m_farSharp;
	k0 = k0 == .0f ? .0f : 1.f / k0;
	k1 = k1 == .0f ? .0f : 1.f / k1;
	m[0] = m[1] = m[2] = m[3] = mPostParams.m_maxFuzziness;
	v[0] = mPostParams.m_nearSharp;
	v[1] = k0;
	v[2] = mPostParams.m_farSharp;
	v[3] = k1;

	// patch shaders
	if ( bNear)
	{
		CGparameter params0 = cellGcmCgGetNamedParameter( mNearFuzzinessFp.mCgProgram, "params0" );
		CGparameter maxFuzziness = cellGcmCgGetNamedParameter( mNearFuzzinessFp.mCgProgram, "maxFuzziness" );

		if ( params0)
			cellGcmSetFragmentProgramParameter( mNearFuzzinessFp.mCgProgram, params0, v, mNearFuzzinessFp.mUCodeOffset );

		if ( maxFuzziness)
			cellGcmSetFragmentProgramParameter( mNearFuzzinessFp.mCgProgram, maxFuzziness, m, mNearFuzzinessFp.mUCodeOffset );

		cellGcmSetFragmentProgram( mNearFuzzinessFp.mCgProgram, mNearFuzzinessFp.mUCodeOffset );
	} else {
		CGparameter params0 = cellGcmCgGetNamedParameter( mFarFuzzinessFp.mCgProgram, "params0" );
		CGparameter maxFuzziness = cellGcmCgGetNamedParameter( mFarFuzzinessFp.mCgProgram, "maxFuzziness" );
		
		if ( params0)
			cellGcmSetFragmentProgramParameter( mFarFuzzinessFp.mCgProgram, params0, v, mFarFuzzinessFp.mUCodeOffset );
		
		if ( maxFuzziness)
		cellGcmSetFragmentProgramParameter( mFarFuzzinessFp.mCgProgram, maxFuzziness, m, mFarFuzzinessFp.mUCodeOffset );
		
		cellGcmSetFragmentProgram( mFarFuzzinessFp.mCgProgram, mFarFuzzinessFp.mUCodeOffset );
	}

	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );

	// destination
	SetTarget( dst );
	
	// source texture id linear depth, use nearest sampling
	cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 0, depth );

	// fullscreen quad
	cellGcmSetVertexDataArray( 0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray( 0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::SetupGpuDof( const CellGcmSurface& src, const CellGcmSurface& fuzziness, const CellGcmSurface& dst )
{
	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );
	cellGcmSetFragmentProgram( mDofFp.mCgProgram, mDofFp.mUCodeOffset );

	// destination
	SetTarget( dst );

	// source texture
	//cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 0, src );

	// fuzziness
	//cellGcmSetTextureFilter( 1, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureFilter( 1, 0, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 1, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 1,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 1, fuzziness);

	// fullscreen quad
	cellGcmSetVertexDataArray( 0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray( 0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::SetupGpuGauss( const CellGcmSurface& src, const CellGcmSurface& dst, 
							  bool bHorizontal, float* fweights, uint32_t format )
{
	float v[4];
	v[0] = v[1] = v[2] = v[3] = bHorizontal ? 1.f / float(src.width) : 1.f / float(src.height);

	if ( bHorizontal )
	{
		CGparameter dx = cellGcmCgGetNamedParameter( mGauss7x1Fp.mCgProgram, "dx" );
		CGparameter weights = cellGcmCgGetNamedParameter( mGauss7x1Fp.mCgProgram, "weights" );
		assert( dx );
		assert( weights );

		cellGcmSetFragmentProgramParameter( mGauss7x1Fp.mCgProgram, dx, v, mGauss7x1Fp.mUCodeOffset );
		cellGcmSetFragmentProgramParameter( mGauss7x1Fp.mCgProgram, weights, fweights, mGauss7x1Fp.mUCodeOffset );
		cellGcmSetFragmentProgram( mGauss7x1Fp.mCgProgram, mGauss7x1Fp.mUCodeOffset );
	} else {
		CGparameter dx = cellGcmCgGetNamedParameter( mGauss1x7Fp.mCgProgram, "dx" );
		CGparameter weights = cellGcmCgGetNamedParameter( mGauss1x7Fp.mCgProgram, "weights" );
		assert( dx );
		assert( weights );

		cellGcmSetFragmentProgramParameter( mGauss1x7Fp.mCgProgram, dx, v, mGauss1x7Fp.mUCodeOffset );
		cellGcmSetFragmentProgramParameter( mGauss1x7Fp.mCgProgram, weights, fweights, mGauss1x7Fp.mUCodeOffset );
		cellGcmSetFragmentProgram( mGauss1x7Fp.mCgProgram, mGauss1x7Fp.mUCodeOffset );
	}

	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );

	// destination
	SetTarget( dst );

	// source texture
	if ( format == CELL_GCM_TEXTURE_X32_FLOAT )
		cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	else
		cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
		

	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 0, src, format );

	// fullscreen quad
	cellGcmSetVertexDataArray( 0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray( 0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::SetupGpuBloomComposite( const CellGcmSurface& src, const CellGcmSurface& bloom, const CellGcmSurface& dst )
{
	CGparameter bloomStrength = cellGcmCgGetNamedParameter( mBloomCompositeFp.mCgProgram, "bloomStrength" );
	
	float bs = mPostParams.m_bloomStrength * ( mPostParams.m_maxLuminance - mPostParams.m_minLuminance ) * mPostParams.m_exposureLevel;
	float v[] = { bs, bs, bs, bs};
	if ( bloomStrength )
		cellGcmSetFragmentProgramParameter( mBloomCompositeFp.mCgProgram, bloomStrength, v, mBloomCompositeFp.mUCodeOffset );

	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );
	cellGcmSetFragmentProgram( mBloomCompositeFp.mCgProgram, mBloomCompositeFp.mUCodeOffset );

	// destination
	SetTarget( dst );

	// source texture
	cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 0, src );

	// bloom texture
	cellGcmSetTextureFilter( 1, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 1, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 1,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 1, bloom );

	// fullscreen quad
	cellGcmSetVertexDataArray( 0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray( 0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::SetupGpuIlr( const CellGcmSurface& src, const CellGcmSurface& dst )
{
	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );
	cellGcmSetFragmentProgram( mIlrFp.mCgProgram, mIlrFp.mUCodeOffset );

	// destination
	SetTarget( dst );

	// source texture
	cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 0, src );

	// fullscreen quad
	cellGcmSetVertexDataArray( 0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray( 0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::SetupFinalCombine( const CellGcmSurface& src, const CellGcmSurface& accumulator, const CellGcmSurface& dst )
{
	float originAndScale[4] = { 0, 0, 1, 1};
	cellGcmSetVertexProgramConstants(0, 16, (const float*)&originAndScale);
	cellGcmSetVertexProgram( mResampleVp.mCgProgram, mResampleVp.mUCode );
	cellGcmSetFragmentProgram( mSpuCompositeFp.mCgProgram, mSpuCompositeFp.mUCodeOffset );

	// destination
	SetTarget( dst );

	// source texture
	cellGcmSetTextureFilter( 0, 0, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 0, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 0,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 0, src );

	// accumulator
	cellGcmSetTextureFilter( 1, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	cellGcmSetTextureControl( 1, CELL_GCM_TRUE, 0, 0, 0);
	cellGcmSetTextureAddress( 1,	CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, 
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	SetTexture( 1, accumulator );

	// fullscreen quad
	cellGcmSetVertexDataArray( 0, 0, 2*sizeof(float), 2, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, mQuadVertexDataOffset);
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_QUADS, 0, 4);
	cellGcmSetVertexDataArray( 0, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
}

void SampleApp::DoGpuPostprocess()
{

	// setup default states
	cellGcmSetBlendEnable( CELL_GCM_FALSE);
	cellGcmSetBlendEnableMrt( CELL_GCM_FALSE, CELL_GCM_FALSE, CELL_GCM_FALSE);
	cellGcmSetDepthTestEnable( CELL_GCM_FALSE);
	cellGcmSetCullFaceEnable( CELL_GCM_FALSE);
	cellGcmSetDepthMask( CELL_GCM_FALSE);
	cellGcmSetColorMask( CELL_GCM_COLOR_MASK_R | CELL_GCM_COLOR_MASK_G | CELL_GCM_COLOR_MASK_B | CELL_GCM_COLOR_MASK_A);
	cellGcmSetColorMaskMrt( 0 );

	CellGcmSurface colorSurface;
	MakeBufferFromMrt( colorSurface, mMrtTarget, 0 );

	//////////////////////////////////////////////////////////////////////////
	//// Depth of fields

	// downsample depth to 1/2 ( nearest candidate )
	SetupDepthDownsample( mMotionAndDepthSurface, mPostSurfaces_2[1] );
	ProfileBars::AddRsxMarker(0x0000ff00);

	// calculate near fuzziness downsampled to 1/4
	SetupGpuFuzziness( mPostSurfaces_2[1], mPostSurfaces_4[8], true );
	
	// blur near fuzziness to expand edges
	SetupGpuGauss( mPostSurfaces_4[8], mPostSurfaces_4[9], true, mDoubleWeights );
	SetupGpuGauss( mPostSurfaces_4[9], mPostSurfaces_4[8], false, mDoubleWeights );

	// calculate far fuzziness
	SetupGpuFuzziness( mPostSurfaces_2[1], mPostSurfaces_2[3], false );

	// add near and far fuzziness together
	cellGcmSetBlendEnable( CELL_GCM_TRUE);
	cellGcmSetBlendEquation( CELL_GCM_FUNC_ADD, CELL_GCM_FUNC_ADD );
	cellGcmSetBlendFunc( CELL_GCM_ONE, CELL_GCM_ONE, CELL_GCM_ONE, CELL_GCM_ONE);
	SetupGpuResample( mPostSurfaces_4[8], mPostSurfaces_2[3], false );
	cellGcmSetBlendEnable( CELL_GCM_FALSE);
	ProfileBars::AddRsxMarker(0x00ff0000);

	// downsample colors
	SetupGpuResample( colorSurface, mPostSurfaces_2[2] );
	ProfileBars::AddRsxMarker(0x00ffff00);

	// apply dof
	SetupGpuDof( mPostSurfaces_2[2], mPostSurfaces_2[3], mPostSurfaces_2[0] );
	ProfileBars::AddRsxMarker(0x0000ff00);

	//////////////////////////////////////////////////////////////////////////
	//// Motion blur

	//	motion blur
	SetupGpuMotionBlur( mPostSurfaces_2[0], mPostSurfaces_2[1], mPostSurfaces_2[2] );
	ProfileBars::AddRsxMarker(0x00ff0000);

	//////////////////////////////////////////////////////////////////////////
	//// Bloom

	// mPostSurfaces_2[2] is dof+motionblur, can't be touched

	// downsample to 1/4 and capture bloom color
	SetupGpuResample( mPostSurfaces_2[2], mPostSurfaces_4[10] );
	SetupGpuBloomCapture( mPostSurfaces_4[10], mPostSurfaces_4[0] );
	
	// Blur and downsample to 1/8
	SetupGpuGauss( mPostSurfaces_4[0], mPostSurfaces_4[10], true, mBloomWeights );
	SetupGpuGauss( mPostSurfaces_4[10], mPostSurfaces_4[0], false, mBloomWeights );
	SetupGpuResample( mPostSurfaces_4[0], mPostSurfaces_8[38] );

	// Blur and downsample to 1/16
	SetupGpuGauss( mPostSurfaces_8[38], mPostSurfaces_8[39], true, mBloomWeights );
	SetupGpuGauss( mPostSurfaces_8[39], mPostSurfaces_8[38], false, mBloomWeights );
	SetupGpuResample( mPostSurfaces_8[38], mPostSurfaces_16[0] );
	
	// Extra blur 1/8 buffer
	SetupGpuGauss( mPostSurfaces_8[38], mPostSurfaces_8[39], true, mBloomWeights );
	SetupGpuGauss( mPostSurfaces_8[39], mPostSurfaces_8[38], false, mBloomWeights );

	// Blur 1/16 buffer
	for ( int k = 0; k < 3; ++k )
	{
		SetupGpuGauss( mPostSurfaces_16[0], mPostSurfaces_16[1], true, mBloomWeights );
		SetupGpuGauss( mPostSurfaces_16[1], mPostSurfaces_16[0], false, mBloomWeights );
	}

	// Modulate upper levels
	// SetupConstModulate( pStage++,		80, 45,	pTmp0_4[2], 1.4f, pTmp0_4[2] );
	// SetupConstModulate( pStage++,		160, 90, pTmp0_4[0], 1.3f, pTmp0_4[0] );
	// SetupConstModulate( pStage++,		320, 180, pTmp1_4[1], 1.2f, pTmp1_4[1] );

	// use blending to accumulate levels
	cellGcmSetBlendEnable( CELL_GCM_TRUE);
	cellGcmSetBlendEquation( CELL_GCM_FUNC_ADD, CELL_GCM_FUNC_ADD );
	cellGcmSetBlendFunc( CELL_GCM_ONE, CELL_GCM_ONE, CELL_GCM_ZERO, CELL_GCM_ZERO);
	SetupGpuResample( mPostSurfaces_16[0], mPostSurfaces_8[38] );
	SetupGpuResample( mPostSurfaces_8[38], mPostSurfaces_4[0] );
	cellGcmSetBlendEnable( CELL_GCM_FALSE);

	// downsample bloom
	SetupGpuResample( mPostSurfaces_4[0], mPostSurfaces_8[39] );
	
	// Ilr
	SetupGpuIlr( mPostSurfaces_8[39], mPostSurfaces_8[38] );

	for ( int k = 0; k < 2; ++k )
	{
		SetupGpuGauss( mPostSurfaces_8[38], mPostSurfaces_8[39], true, mBloomWeights );
		SetupGpuGauss( mPostSurfaces_8[39], mPostSurfaces_8[38], false, mBloomWeights );
	}
	
	// upsample and add to bloom
	cellGcmSetBlendEnable( CELL_GCM_TRUE);
	cellGcmSetBlendEquation( CELL_GCM_FUNC_ADD, CELL_GCM_FUNC_ADD );
	cellGcmSetBlendFunc( CELL_GCM_ONE, CELL_GCM_ONE, CELL_GCM_ZERO, CELL_GCM_ZERO);
	SetupGpuResample( mPostSurfaces_8[38], mPostSurfaces_4[0] );
	cellGcmSetBlendEnable( CELL_GCM_FALSE);
	
	// composite bloom
	SetupGpuBloomComposite( mPostSurfaces_2[2], mPostSurfaces_4[0], mPostSurfaces_2[1]);
	ProfileBars::AddRsxMarker(0x00ffff00);

	//////////////////////////////////////////////////////////////////////////
	//// Final composition
	SetupFinalCombine( colorSurface, mPostSurfaces_2[1], mFrameTarget );

	ProfileBars::AddRsxMarker(0x0000ff00);
}