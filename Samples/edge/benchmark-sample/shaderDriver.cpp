/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <cell/gcm.h>

using namespace cell::Gcm;

#include <gcmutil.h>
#include "shaderDriver.h"

//#define MAIN_MEMORY_FRAGMENT_PROGRAMS

uint32_t ShaderDriver::mVertexIndexBufferOffset = 0;
	
ShaderDriver::ShaderDriver() :
	mCGVertexProgram(NULL),
	mVertexProgramUCode(NULL),
	mCGFragmentProgram(NULL),
	mFragmentProgramUCode(NULL),
	mFragmentProgramOffset(0),
	mVertexAttributeInputPosition(CG_UNDEFINED),
	mVertexAttributeInputNormal(CG_UNDEFINED),
	mVertexAttributeInputColor(CG_UNDEFINED),
	mVertexAttributeInputTexcoord(CG_UNDEFINED),
	mVertexAttributeInputTangent(CG_UNDEFINED),
	mVertexAttributeInputBinormal(CG_UNDEFINED),
	mVertexAttributeInputBlendWeight(CG_UNDEFINED),
	mVertexAttributeInputBlendIndex(CG_UNDEFINED),
	mVertexAttributeInputVertexIndex(CG_UNDEFINED),
	mEmphasisVertexIndex(0)
{
}

ShaderDriver::~ShaderDriver()
{
}

void ShaderDriver::init()
{
	//J 変数mCGVertexProgramとmCGFragmentProgramとにこのシェーダで使用する
	//J シェーダのバイナリコードを設定する。
	setPrograms();

	if((!mCGVertexProgram) || (!mCGFragmentProgram))
	{
		assert(0);
		exit(1);
	}

	//J 頂点シェーダのバイナリコードを使用可能にセットアップする。
	unsigned int ucodeSize;
	void *ucode;
	cellGcmCgInitProgram(mCGVertexProgram);
	cellGcmCgGetUCode(mCGVertexProgram, &ucode, &ucodeSize);
	mVertexProgramUCode = ucode;

	//J フラグメントシェーダのバイナリコードを使用可能にセットアップする。
	//J シェーダのヘッダ部はシステムメモリ(mCGFragmentProgram)へ、マイクロ
	//J コード部はRSXのローカルメモリへ配置する。ヘッダ部をローカルメモリへ
	//J 配置すると、超低速なCell->RSXMemへのリードアクセスの原因になるので注意。
	cellGcmCgInitProgram(mCGFragmentProgram);
	cellGcmCgGetUCode(mCGFragmentProgram, &ucode, &ucodeSize);

#ifdef MAIN_MEMORY_FRAGMENT_PROGRAMS
	mFragmentProgramUCode = (void*)cellGcmUtilAllocateMainMemory(ucodeSize, 128);
#else
	mFragmentProgramUCode = (void*)cellGcmUtilAllocateLocalMemory(ucodeSize, 64);
#endif
	memcpy(mFragmentProgramUCode, ucode, ucodeSize);
	cellGcmAddressToOffset(mFragmentProgramUCode, &mFragmentProgramOffset);

	//J 派生クラスで拡張されたシェーダパラメータを初期化する。
	setParameters();

	//J 頂点シェーダの頂点属性入力パラメータを取得。
	CGparameter position = 
		cellGcmCgGetNamedParameter(mCGVertexProgram, "position");
	if(position)
	{
		mVertexAttributeInputPosition =
			cellGcmCgGetParameterResource(mCGVertexProgram, position);
	}

	CGparameter normal = 
		cellGcmCgGetNamedParameter(mCGVertexProgram, "normal");
	if(normal)
	{
		mVertexAttributeInputNormal =
			cellGcmCgGetParameterResource(mCGVertexProgram, normal);
	}

	CGparameter color = 
		cellGcmCgGetNamedParameter(mCGVertexProgram, "color");
	if(color)
	{
		mVertexAttributeInputColor =
			cellGcmCgGetParameterResource(mCGVertexProgram, color);
	}

	CGparameter texcoord = 
		cellGcmCgGetNamedParameter(mCGVertexProgram, "texcoord");
	if(texcoord)
	{
		mVertexAttributeInputTexcoord =
			cellGcmCgGetParameterResource(mCGVertexProgram, texcoord);
	}

	CGparameter tangent = 
		cellGcmCgGetNamedParameter(mCGVertexProgram, "tangent");
	if(tangent)
	{
		mVertexAttributeInputTangent =
			cellGcmCgGetParameterResource(mCGVertexProgram, tangent);
	}

	CGparameter binormal = 
		cellGcmCgGetNamedParameter(mCGVertexProgram, "binormal");
	if(binormal)
	{
		mVertexAttributeInputBinormal =
			cellGcmCgGetParameterResource(mCGVertexProgram, binormal);
	}

	CGparameter blendWeight = 
		cellGcmCgGetNamedParameter(mCGVertexProgram, "blendWeight");
	if(blendWeight)
	{
		mVertexAttributeInputBlendWeight =
			cellGcmCgGetParameterResource(mCGVertexProgram, blendWeight);
	}

	CGparameter blendIndex = 
		cellGcmCgGetNamedParameter(mCGVertexProgram, "blendIndex");
	if(blendIndex)
	{
		mVertexAttributeInputBlendIndex =
			cellGcmCgGetParameterResource(mCGVertexProgram, blendIndex);
	}

	//J デバッグ目的で頂点シェーダに頂点番号を送るための頂点配列を作成する。
	//J 頂点シェーダ入力に"int vertexIndex"を追加すると、このクラスが自動的に
	//J 必要なセットアップを行う。頂点シェーダ内で0から始まる頂点番号を取得す
	//J るには(vertexIndex + 32768)とする。
	CGparameter vertexIndex = 
		cellGcmCgGetNamedParameter(mCGVertexProgram, "vertexIndex");
	if(vertexIndex)
	{
		mVertexAttributeInputVertexIndex =
			cellGcmCgGetParameterResource(mCGVertexProgram, vertexIndex);
	}

	if(vertexIndex && (mVertexIndexBufferOffset == 0))
	{
		void *buffer = cellGcmUtilAllocateLocalMemory(65536*2, 128);
		cellGcmAddressToOffset(buffer, &mVertexIndexBufferOffset);

		short *p = (short *)buffer;
		for(int i = -32768; i < 32768; i++)
		{
			*(p++) = short(i);
		}
	}

	//J 何番の頂点を指定するかを代入する変数"gEmphasisVertexIndex"を取得する。
	//J 上のvertexIndexに入力される頂点番号と比較するために使用できる。
	mEmphasisVertexIndexParam = cellGcmCgGetNamedParameter(
		mCGVertexProgram,
		"gEmphasisVertexIndex");
}

void ShaderDriver::bind()
{
	//J 頂点シェーダとフラグメントシェーダをバインドする。
	cellGcmSetVertexProgram(mCGVertexProgram, mVertexProgramUCode);
#ifdef MAIN_MEMORY_FRAGMENT_PROGRAMS
	cellGcmSetFragmentProgramOffset(mCGFragmentProgram, mFragmentProgramOffset, 
		CELL_GCM_LOCATION_MAIN);
#else
	cellGcmSetFragmentProgramOffset(mCGFragmentProgram, mFragmentProgramOffset, 
		CELL_GCM_LOCATION_LOCAL);
#endif
	cellGcmSetFragmentProgramControl(mCGFragmentProgram, 1, 0, 0);

	//J デバッグ目的で頂点シェーダに頂点番号を送るための頂点配列を設定する。
	if(mVertexAttributeInputVertexIndex != CG_UNDEFINED)
	{
		cellGcmSetVertexDataArray(
			(mVertexAttributeInputVertexIndex - CG_ATTR0), 0, 2,
			1, CELL_GCM_VERTEX_S32K, CELL_GCM_LOCATION_LOCAL,
			mVertexIndexBufferOffset);
	}
	
	//J デバッグ目的で頂点番号を指定する変数"gEmphasisVertexIndex"を設定する。
	if(mEmphasisVertexIndexParam)
	{
		float v = float(mEmphasisVertexIndex);
		cellGcmSetVertexProgramParameter(
			mEmphasisVertexIndexParam, &v);
	}
}

void ShaderDriver::unbind(void)
{
	CGresource attrs[] =
	{
		mVertexAttributeInputPosition,
		mVertexAttributeInputNormal,
		mVertexAttributeInputColor,
		mVertexAttributeInputTexcoord,
		mVertexAttributeInputTangent,
		mVertexAttributeInputBinormal,
		mVertexAttributeInputBlendWeight,
		mVertexAttributeInputBlendIndex,
		mVertexAttributeInputVertexIndex,
	};
	for(int i = 0; i < 8; i++)
	{
		if(attrs[i] != CG_UNDEFINED)
		{
			cellGcmSetVertexDataArray(
				attrs[i] - CG_ATTR0,
				0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
		}
	}
}
