[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.


様々な簡単なシーンを描画して Edge Geometry ライブラリの機能を紹介
するサンプル


<サンプルプログラムのポイント>
  本サンプルは、edgegeomcompiler ツールによって処理されたシーンを読み込む
　方法および描画する方法を実現します。スキニング、ブレンドシェイプ、三角
　形のカリング、カスタム頂点圧縮そしてスタティックなシーンの最適化など、
　Edge Geometry の機能が使用されている様々な簡単なテストシーンを提供しま
　す。

<サンプルプログラム解説>
  edgegeomcompiler はシーンを処理する際、バイナリの .edge ファイルと読み
　易い C ヘッダファイルの2つの異なるフォーマットに出力を書き込みます。
　ヘッダファイルは、多様な Edge Geometry 構造体がどのようにして互いに組み
  合わされているか確認する際に役立ちます。また、データの整合性を確認する
　こともできます。
  このサンプルは両方のタイプのシーンを読み込むことができます。デフォルト
　では、このサンプルはバイナリシーンファイルを読み込みます（コマンドライ
　ンの引数として渡す）。ヘッダシーンファイルを読み込むには、main.cpp にあ
　る USE_SCENE_IN_HEADE の定義をコメントから外し、ヘッダーファイルへのパ
  スを直接指定してください。

  以下のシーンが含まれています。
  - bs_cube.edge: ブレンドシェイプを使用した簡単なキューブ
  - bs_cube2.edge: ブレンドシェイプを使用した簡単なキューブ
  - bs_skin_cyl.edge: ブレンドシェイプを使用しスキニングされたシリンダー
  - skin_cyl.edge: スキニングされたシリンダー
  - skin_cyl_customcompress.edge: 頂点データに対しカスタムの圧縮・解凍
				　コードを使用しスキニングされたシリンダー
  - teapot.edge: Edge のスタティックなジオメトリの最適化を利用した、サイズ
		 の大きいスタティックシーン

  簡易化のため、アニメーションマトリックスとブレンドシェイプのファクタは、
　PPU において手順に従って計算します。含まれているもの以外の任意にアニメー
　ション化されたシーンについては、サンプルが対応できないためです。描画はで
　きますが、正しくアニメーション化されない場合があります。

  使用している機能に関係なく、Edge Geometry SPURS ジョブを作成するコード
  は同じです。Edge Geometry での描画は、非常にデータ駆動型の処理になりま
　す。どの機能を有効にするかは、EdgeGeomSpuConfigInfo 構造体のコンテンツ
　によって大半が決まります。このため、Runtime コードは様々な用途に使用す
　ることができます。

<ファイル>
    Makefile           : Makefile
    bs_cube.dae        : ブレンドシェイプを使用したキューブシーン
			（Collada ソース）
    bs_cube.edge       : ブレンドシェイプを使用したキューブシーン
			（バイナリ Edge シーン）
    bs_cube.edge.h     : ブレンドシェイプを使用したキューブシーン
			（C ヘッダ Edge シーン）
    bs_cube2.dae       : ブレンドシェイプを使用した cube2 シーン
			（Collada ソース）
    bs_cube2.edge      : ブレンドシェイプを使用した cube2 シーン
			（バイナリ Edge シーン）
    bs_cube2.edge.h    : ブレンドシェイプを使用した cube2 シーン
			（C ヘッダ Edge シーン）
    bs_skin_cyl.dae    : ブレンドシェイプを使用しスキニングされた
			 シリンダーシーン（Collada ソース）
    bs_skin_cyl.edge   : ブレンドシェイプを使用しスキニングされた
			 シリンダーシーン（バイナリ Edge シーン）
    bs_skin_cyl.edge.h : ブレンドシェイプを使用しスキニングされた
			 シリンダーシーン（C ヘッダ Edge シーン）
    fpshader.cg        : フラグメントシェーダプログラム
    logo-color.bin     : 全シーンで使用するカラーテクスチャ（未加工の
			 ピクセルデータ）
    logo-normal.bin    : 全シーンで使用する法線マップテクスチャ（未加工
			 のピクセルデータ）
    main.cpp           : メインのサンプルソース
    main.h             : メインのサンプルヘッダ
    readme_e.txt       : Readme テキストファイル（英語版)
    readme_j.txt       : Readme テキストファイル（日本語版)
    skin_cyl.dae       : スキニングされたシリンダーシーン
			（Collada ソース）
    skin_cyl.edge      : スキニングされたシリンダーシーン
			（バイナリ Edge シーン）
    skin_cyl.edge.h    : スキニングされたシリンダーシーン
			（C ヘッダ Edge シーン）
    skin_cyl_customcompress.dae   : カスタム圧縮を使用したスキニングされ
				　　たシリンダーシーン（Collada ソース）
    skin_cyl_customcompress.edge  : カスタム圧縮を使用したスキニングされ
				　　たシリンダーシーン
				　　（バイナリ Edge シーン）
    skin_cyl_customcompress.edge.h :カスタム圧縮を使用したスキニングされ
				　　たシリンダーシーン
				　　（C ヘッダ Edge シーン）
    teapot.dae         : スタティックなティーポットシーン
			（Collada ソース）
    teapot.edge        : スタティックなティーポットシーン		
			（バイナリ Edge シーン）
    teapot.edge.h      : スタティックなティーポットシーン
			 （C ヘッダ Edge シーン）
    vpshader.cg        : 頂点シェーダプログラム
    spu/job_geom.cpp   : Edge Geometry SPURS ジョブソース
    spu/job_geom.mk    : Edge Geometry SPURS ジョブの makefile

<プログラムの実行方法>
    - コマンドラインに1つ引数を渡してサンプルを実行します。
　　　読み込むバイナリ .edge シーンファイルへのパスを指定します。
