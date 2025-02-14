[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<サンプルプログラムのポイント>

このサンプルは、Ray-Triangle 衝突テストの形式で、Edge Geometry ジョブに
ユーザーコードを追加する例を示します。

<サンプルプログラム解説>

このサンプルは、2種類の EdgeGeom ジョブを作成します。1つ目のジョブは、
geom-sample と elephant-sample と同じで、データの解凍、スキニング、カリン
グ、出力の圧縮、コマンドバッファホールを埋める作業を行います。2つ目のジョ
ブは、交差判定に特化したジョブです。データの解凍、ジオメトリのスキニング
を行い、これらの頂点を射線のローカル座標空間に変換して交差判定を行います。

セグメントから一番近い交差ポイントを計算した後、ジョブはアトミック処理を
利用してグローバルにメインメモリから一番近い交差ポイントを読み込みます。
必要であれば、セグメントに一番近い衝突ポイントをメインメモリに書き戻しま
す。一番近い交差ポイントには、法線に沿って線が引かれます。

<ファイル>
elephant.edge.h		: ジオメトリデータが含まれているヘッダファイル
elephant-color.bin	: カラーテクスチャが含まれているバイナリファイル
elephant-matrices.bin	: スキニングマトリックスが含まれているバイナリ
			　ファイル
elephant-normal.bin	: 法線テクスチャが含まれているバイナリファイル
fpshader.cg		: フラグメントシェーダプログラム
main.cpp		: メインプログラム
main.h			: メインプログラムのヘッダファイル
Makefile　　　　　	: Makefile
readme_e.txt		: Readme テキストファイル（英語版)
readme_j.txt		: Readme テキストファイル（日本語版）
vpshader.cg		: 頂点シェーダプログラム
spu/intersect.cpp	: 交差判定のファイル
spu/intersect.h		: 交差判定のヘッダ
spu/intersectmain.cpp	: ユーザーコード Edge Geometry 交差 ジョブのメイン
spu/intersectmain.mk	: ユーザーコード Edge Geometry 交差 ジョブの 
                          Makefile
spu/Makefile		: 全SPUプログラムの Makefile
spu/spumain.cpp		: Edge Geometry ジョブのメイン
spu/spumain.mk		: Edge Geometry ジョブの Makefile

<プログラムの実行方法>
	- アナログスティックの左右と十字キーによってカメラを動かします。

	- 交差判定で使用する射線はカメラのルックアットベクトルです。

<注>
