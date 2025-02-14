[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<サンプルプログラムのポイント>

本サンプルは、アニメーション化およびスキン化されたキャラクターを表示する
ために Edge Animation と Edge Geometry の両方を使用した場合の例を示します。
<サンプルプログラム解説>

アニメーションジョブは、フレームのはじめに作成され（キャラクター毎に1つ）
キックオフされます。アニメーションジョブ自体は、anim-sample とほぼ同じで、
4x4 のワールドマトリックスではなく、3x4 のスキニングマトリックスを出力する
点が異なります。このマトリックスは、後で Edge Geometry が、3x4 のワールド
マトリックスおよび 3x4 インバースバインドポーズマトリックスの乗算を計算す
る際に使用します。（注：インバースバインドポーズ配列は、edgeGeomCompiler 
によって別ファイルとしてエクスポートされます。）

このサンプルは、アニメーションのジョブが終わるまでストールし、各キャラク
ターに関連付けられたスキニングマトリックスを使ってジオメトリジョブをキッ
クオフします。

アニメーションジョブとジオメトリジョブをセットアップおよび実行するコード
は、anim-sample と geom-sample とほぼ同じです。詳細については、これらのサ
ンプルを参照してください。

<ファイル>
main.cpp		: メイン PPU コード（ジョブセットアップ、ブレンド
			　ツリーセットアップなどを含む)
spu/job_anim.cpp	: アニメーションジョブ
spu/job_anim.mk		: アニメーションジョブの makefile
spu/job_geom.cpp	: ジオメトリジョブ
spu/job_geom.mk		: ジオメトリジョブの makefile
assets/*.dae		: 実行時データの構築時に使用する Collada 
			  のソースファイル
assets.mk		: プライマリの makefile によって生成された
			　実行時データを構築する makefile
 
<プログラムの実行方法>
    - ファイルサービングのルートを self ディレクトリに設定します。
    - プログラムを実行します。
