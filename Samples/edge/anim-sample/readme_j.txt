[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<サンプルプログラムのポイント>

本サンプルは、ワイヤーフレームスケルトンのアニメーションを描画に 
Edge Animation を使用する場合の簡単な方法を示します。PPU コードは、
キャラクター毎に1つのブレンドツリーを構築し、通常の SPURS ジョブのセット
アップを表示します。SPU コードは、ブレンドツリーを処理し、メインメモリに
描き戻す前に、その結果を複数のワールドスペースマトリックスに変換します。

<サンプルプログラム解説>

以下に、メイン関数の詳細を示します。

SampleApp::onInit()

	SPURS を初期化します。
	Edge Animation を初期化します。SPU 毎に 128k の外部ポーズ
        キャッシュメモリが割り当てられます。本サンプル内では非常に単純な
　　　　ブレンドツリーを使用しているため、本来は外部ポーズキャッシュを
　　　　必要としません。このため、本サンプルは純粋にデモンストレーション
　　　　だけを目的に作成されています。
	スケルトンと4個のアニメーションバイナリをロードします（2つの
　　　　アイドルと walk と jog 各1つ）。
	各キャラクターには、ワールドマトリックス配列が割り当てられます。
　　　　これらの配列は、SPU ジョブによってフレーム毎に更新されます。

SampleApp::onUpdate()

	評価時間と最終ブレンドファクタが改善されました。キャラクターは、
　　　　idle から walk または jog へ、また idle へと徐々にブレンドが行わ
　　　　れます。
　　　　ブレンドツリー内で使用されている他の2つのブレンドファクタは（2つ
　　　　の idle または walk と jog ）、キャラクター毎に一定ですので注意し
        てください。

SampleApp::runAnimJobs()

	各キャラクターに対し、createBlendTree() と createAnimJob() を呼び
	出します。評価時間とブレンドファクタについては、ジッタ処理を行って
　　　　キャラクター毎に変化を与えます。ジョブはキックされ、このサンプル
	は結果が出るまでストールします。
    
SampleApp::createTestBlendTree()

	ブレンドファクタに渡されたものを使用して4個のアニメーションを
	ブレンドするシンプルなブレンドツリーを作成します。walk と jog の
	サイクルはそれぞれ異なり、通常はブレンディング中に同期されます
	が、簡易化のためにここでは採用していません。

SampleApp::createAnimJob()

	ブレンドツリーを評価する SPU ジョブを1つ作成し、複数のワールド
	マトリックスの配列に出力します。SPURS ジョブは以下のようにセット
	アップされます。

    入力バッファ	: ブレンドツリーのブランチとリーフの配列
    キャッシュバッファ	: スケルトンとグローバルのコンテキスト
　　　　　　　　　　　　（これらは全キャラクターで共有される)
    出力バッファ	: 評価済みのワールドマトリックス配列
    スクラッチバッファ	: Edge Animation 用に 128K が予約される
　　パラメータ		: ルートジョイント、ブレンドブランチカウント、
			　ブレンドリーフカウント、出力マトリックス配列の
			　有効アドレス

cellSpursJobMain2()

	このジョブのエントリポイント。ジョブパラメータと全バッファへの
	ポインタを取得します。processBlendTree() を呼び出してこの処理を行
	い、最終のワールドマトリックスをメインメモリに書き戻します。

processBlendTree()

	edgeAnimSpuInitialize() を呼び出して、Edge Animation SPU ライブラ
　　　　リを初期化します。また、edgeAnimProcessBlendTree() を呼び出して、
　　　　ブレンドツリーを処理します。 ブレンドツリーのルートはブランチなの
　　　　で、EDGE_ANIM_BLEND_TREE_INDEX_BRANCH をルートフラグとして渡しま
　　　　す。
	この段階では、評価済みのローカルスペースジョイントはポーズスタッ
　　　　クの一番上に存在します。
	エントリは、ワールドスペースジョイント用にポーズスタック上に
	プッシュされ、変換を行うために edgeAnimLocalJointsToWorldJoints()
　　　　が呼び出されます。
	edgeAnimJointsToMatrices4x4() を呼び出して、ワールドジョイントを
	マトリックスに変換します（出力バッファに直接行う）。
	最後に、ポーズスタックをリセットし、Edge Animation SPU ライブラリ
　　　　が完了します。


<ファイル>
main.cpp			: メイン PPU コード（ジョブセットアップ、
				　ブレンドツリーセットアップなどを含む）
spu/process_blend_tree.cpp	: メインアニメーション処理関数
				（スプリットして SPU ジョブまたは 
				 PC/win32版のいずれかから呼び出す）
spu/job_anim.cpp		: アニメーションジョブメイン関数
spu/job_anim.mk			: アニメーションジョブの makefile
assets/*.dae			: 実行時データの構築時に使用する Collada 
				  のソースファイル
assets.mk			: プライマリの makefile によって生成された
				　実行時データを構築する makefile
 
<プログラムの実行方法>
    - ファイルサービングのルートを self ディレクトリに設定します。
    - プログラムを実行します。
