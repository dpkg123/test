[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.


Edge Geometry を使用してアニメーション化されたサイズの大きいキャラクター
を描画するサンプル


<サンプルプログラムのポイント>
  本サンプルは、Edge の geom-sample のコンセプトに基づき、アニメーション
　化されたサイズの大きいマルチセグメントの elephant を読み込み、描画しま
  す。

<サンプルプログラム解説>
  簡易化のため、elephant のアニメーションマトリックスは、フラットで事前
  計算された配列から読み込みます。

<Files>
    Makefile           : Makefile
    elephant.dae       : elephant のシーン（Collada ソース）
    elephant.edge      : elephant のシーン（バイナリ Edge シーン）
    elephant.edge.h    : elephant のシーン（C ヘッダ Edge シーン）
    elephant-color.bin : カラーテクスチャ（未加工のピクセルデータ）
    elephant-matrices.bin : elephant の idle サイクル用に事前計算された
			    アニメーションマトリックス
    elephant-normal.bin : 法線マップテクスチャ（未加工のピクセルデータ）
    fpshader.cg        : フラグメントシェーダプログラム
    main.cpp           : メインサンプルソース
    main.h             : メインサンプルヘッダ
    readme_e.txt       : Readme テクストファイル（英語版)
    readme_j.txt       : Readme テクストファイル（日本語版)
    vpshader.cg        : 頂点シェーダプログラム
    spu/spumain.cpp    : Edge Geometry SPURS ジョブソース
    spu/spumain.mk     : Edge Geometry SPURS ジョブの makefile
