/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __MSAARESOLVEDRIVER__H
#define __MSAARESOLVEDRIVER__H

#include "shaderDriver.h"

class MSAAResolveDriver :
	public ShaderDriver
{
public:
	//J コンストラクタ
	MSAAResolveDriver();
	
	//J デストラクタ
	virtual ~MSAAResolveDriver();

	//J シェーダをバインドする。
	//J このシェーダで参照するパラメータやテクスチャは全てこの関数の
	//J 引数に指定する。
	void apply(const CellGcmTexture *MSAATexture);

protected:
	//J このシェーダで使用するコンパイル済みのシェーダコードを設定する。
	virtual void setPrograms();
	
	//J このシェーダで使用するシェーダパラメータを取得する。
	virtual void setParameters();

private:

	CGresource		mMSAATexture;
};

#endif
