/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __SHADERDRIVER_H__
#define __SHADERDRIVER_H__

#define NOEXIST_INPUT	uint8_t(255)

//J シェーダを抽象化する基底クラス。
class ShaderDriver
{
public:
	//J コンストラクタ
	ShaderDriver();

	//J デストラクタ
	virtual ~ShaderDriver();

	//J シェーダを初期化する。
	virtual void init(void);

	//J シェーダをバインドする。
	//J 派生クラスで追加したシェーダパラメータへの代入などは、派生クラス独自の
	//J apply()関数を実装してそこで行う。この関数は派生クラスのapply()の先頭か
	//J ら呼び出される。
	void bind();

	//J シェーダの使用が終わった時点でシェーダを無効にする。
	virtual void unbind(void);

	//J 頂点シェーダの頂点属性入力パラメータを取得する。
	CGresource getPosition(void) const
		{ return mVertexAttributeInputPosition; }

	CGresource getNormal(void) const
		{ return mVertexAttributeInputNormal; }

	CGresource getColor(void) const
		{ return mVertexAttributeInputColor; }

	CGresource getTexcoord(void) const
		{ return mVertexAttributeInputTexcoord; }

	CGresource getTangent(void) const
		{ return mVertexAttributeInputTangent; }

	CGresource getBinormal(void) const
		{ return mVertexAttributeInputBinormal; }

	CGresource getBlendWeight(void) const
		{ return mVertexAttributeInputBlendWeight; }

	CGresource getBlendIndex(void) const
		{ return mVertexAttributeInputBlendIndex; }

	//J デバッグ目的で強調表示したい頂点インデックスを設定。
	void setEmphasisVertexIndex(int index)
		{ mEmphasisVertexIndex = index; }

protected:
	//J 頂点シェーダ
	//J 頂点シェーダはシステムメモリに配置される。
	CGprogram		mCGVertexProgram;
	void			*mVertexProgramUCode;

	//J フラグメントシェーダ
	//J フラグメントシェーダはローカルメモリに配置され、オフセットを使って
	//J 指し示される。
	CGprogram		mCGFragmentProgram;
	void			*mFragmentProgramUCode;
	uint32_t		mFragmentProgramOffset;

	//J 頂点シェーダの頂点属性入力パラメータ。
	CGresource		mVertexAttributeInputPosition;
	CGresource		mVertexAttributeInputNormal;
	CGresource		mVertexAttributeInputColor;
	CGresource		mVertexAttributeInputTexcoord;
	CGresource		mVertexAttributeInputTangent;
	CGresource		mVertexAttributeInputBinormal;
	CGresource		mVertexAttributeInputBlendWeight;
	CGresource		mVertexAttributeInputBlendIndex;

	CGresource		mVertexAttributeInputVertexIndex;

	//J 派生クラスのシェーダで使用する頂点シェーダとフラグメントシェーダの
	//J コンパイル済みバイナリコードを、mCGVertexProgramとmCGFragmentProgram
	//J に設定する。
	//J 派生クラスで必ずオーバーライドする。
	//J このクラスのinit()関数から呼び出される。
	virtual void setPrograms() = 0;

	//J 派生クラスのシェーダで拡張されたシェーダパラメータを取得する。
	//J 派生クラスで必ずオーバーライドする。
	//J このクラスのinit()関数から呼び出される。
	virtual void setParameters() = 0;

	//J デバッグ目的で頂点シェーダに頂点番号を送るための頂点配列。
	static uint32_t	mVertexIndexBufferOffset;
	
	//J デバッグ目的で強調表示される頂点の番号。
	int mEmphasisVertexIndex;
	CGparameter		mEmphasisVertexIndexParam;
};

#endif
