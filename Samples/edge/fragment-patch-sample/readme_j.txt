[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<サンプルプログラムのポイント>
本サンプルは、Edge Geometry ジョブに追加されているユーザーコードを使用
して、SPU 上にフラグメントプログラムコンスタントをパッチする方法を実現
します。

<サンプルプログラム解説>

本プログラムはシーン内の各オブジェクト（128個の elephant）に対しフラグメ
ントプログラムコンスタントをパッチします。それぞれの elephant には事前に
パーティションされたセグメントが複数含まれているため（複数の Edge Geometry
ジョブに関連付けられる）、各オブジェクトの最初のセグメントに関連付けられ
ているジョブの最中にパッチを適用します。SPU 側では、Edge Geometry ジョブ
を修正しました。この修正により、パッチ情報を含む構造体およびフラグメント
プログラムのマイクロコードを DMA 転送し、パッチの値をメインメモリから SPU
のローカルストアに格納し、マイクロコード内の適切なオフセットにパッチ値を
コピーし、結果の ucode をビデオメモリ内の空き領域に DMA 転送し（ハイブリ
ッドバッファから割り当て）、適切なコマンドを追加して 
cellGcmSetUpdateFragmentProgramParameter() を呼び出してコマンドバッファの
パラメータを更新するようにしました。PPU 側では、オフィシャルツール内で事前
に計算されたコマンドバッファホールのサイズを拡張して、さらにコマンドを保持
できるようにしました（製品版では、ツール自身で処理するのが最適です）。  

<ファイル>
elephant.edge.h		: ジオメトリデータが含まれているヘッダファイル
elephant-color.bin	: カラーテクスチャが含まれているバイナリファイル
elephant-matrices.bin	: スキニングマトリックスが含まれているバイナリ
			　ファイル
elephant-normal.bin	: 法線テクスチャが含まれているバイナリファイル
fpshader.cg		: パッチするフラグメントシェーダプログラム
main.cpp		: メインプログラム
main.h			: メインプログラム用のヘッダファイル
Makefile		: Makefile
readme_e.txt		: Readme テキストファイル（英語版)
readme_j.txt		: Readme テキストファイル（日本語版)
vpshader.cg		: 頂点シェーダプログラム
spu/Makefile		: 全 SPU プログラムの Makefile
spu/spumain.cpp		: Edge Geometry 上のメインで、フラグメント
			　プログラムのパッチ用にさらにコードが付加され
			　ている
spu/spumain.mk		: Edge Geometry ジョブの Makefile

<プログラムの実行方法>
	- アナログスティックの左右と十字キーによってカメラを動かします。


<備考>
