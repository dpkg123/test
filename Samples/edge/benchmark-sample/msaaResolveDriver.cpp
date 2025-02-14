/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <cell/gcm.h>

using namespace cell::Gcm;

#include "MSAAResolveDriver.h"

extern struct _CGprogram _binary_msaaResolveVp_vpo_start;
extern struct _CGprogram _binary_msaaResolveFp_fpo_start;

MSAAResolveDriver::MSAAResolveDriver()
{
}

MSAAResolveDriver::~MSAAResolveDriver()
{
}


void MSAAResolveDriver::apply(const CellGcmTexture *MSAATexture)
{
	//J 基底クラスのbind()関数によって、頂点シェーダとピクセルシェーダを
	//J バインドする。
	bind();

	uint8_t samplerIndex = (mMSAATexture - CG_TEXUNIT0);
	cellGcmSetTexture(
		samplerIndex,
		MSAATexture);
	cellGcmSetTextureControl(
		samplerIndex,
		CELL_GCM_TRUE,
		0,
		12 << 8,
		CELL_GCM_TEXTURE_MAX_ANISO_1);
	cellGcmSetTextureAddress(
		samplerIndex,
		CELL_GCM_TEXTURE_WRAP,
		CELL_GCM_TEXTURE_WRAP,
		CELL_GCM_TEXTURE_CLAMP_TO_EDGE,
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL,
		CELL_GCM_TEXTURE_ZFUNC_LESS,
		0);
	cellGcmSetTextureFilter(
		samplerIndex, 0,
		CELL_GCM_TEXTURE_LINEAR_LINEAR,
		CELL_GCM_TEXTURE_LINEAR,
		CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
}

void MSAAResolveDriver::setPrograms()
{
	//J このシェーダで使用するコンパイル済みのシェーダコードを設定する。
	mCGVertexProgram	= &_binary_msaaResolveVp_vpo_start;
	mCGFragmentProgram	= &_binary_msaaResolveFp_fpo_start;
}

void MSAAResolveDriver::setParameters()
{
	CGparameter MSAATexture = cellGcmCgGetNamedParameter(
		mCGFragmentProgram,
		"gMSAATexture");
	mMSAATexture = cellGcmCgGetParameterResource(
		mCGFragmentProgram,
		MSAATexture);
}
