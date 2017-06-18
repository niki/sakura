Introduction  
------------

サクラエディタ 修正版  
可能な限り最新版をベースに修正をしています  
また, 気になったライブラリなどの試験場  


Feature  
-------

<details><summary>MacTypeなどの描画負荷の高いソフトを併用した際に起こる描画崩れを軽減(したつもり)</summary></details>  
<details><summary>Luaをマクロ言語として組み込み</summary><img src="https://raw.github.com/wiki/uzuki3/sakura/images/sakura_201706041845.png" width="50%"></details>  
<details><summary>スクロールバーに検索結果やブックマークを表示</summary><img src="https://raw.github.com/wiki/uzuki3/sakura/images/sakura_201706092354.png" width="50%"></details>  
<details><summary>Grepフォルダの指定を４つに拡張</summary><img src="https://raw.github.com/wiki/uzuki3/sakura/images/sakura_201706041815.png" width="50%"></details>  
<details><summary>半角空白やタブなどの見た目をすっきりさせる (Sublime Textを模倣)</summary></details>  
<details><summary>sakura.iniの精査 (肥大化対策)</summary></details>  


Download  
--------

変更内容はコミットログを参照してください. 細かい内容は [my_config.h](https://github.com/uzuki3/sakura/raw/master/sakura_core/my_config.h) を見てね.  

+ [sakura-uzuki-2.37-32bit.zip](http://mimix.sakura.ne.jp/release/sakura-uzuki-2.37-32bit.zip) (766KB)  
+ 最新のキーワードセット [keyword_pack.zip](https://github.com/uzuki3/sakura/raw/master/Publish/keyword_pack.zip) (355KB)  

※ 動作には[Visual Studio 2017 Microsoft Visual C++ 再頒布可能パッケージ](https://www.visualstudio.com/ja/downloads/#other-ja)が必要です.  



Setup  
-----

ダウンロードしたファイルをすでに使用しているサクラエディタに上書きしてください.  
`sakura.keywordset.csv`は実行ファイルと同じ場所に置いてください. 実行ファイル名が違う場合は`sakura`の部分を変更してください.  


Build environment  
-----------------
+ 2.3.2.0をベースに[リポジトリ](http://svn.code.sf.net/p/sakura-editor/code/sakura/trunk2)の追っかけ. ベースリビジョンからのマージ情報は[こちら](https://github.com/uzuki3/sakura/raw/master/Publish/changes_from_r4011.txt). あと、[パッチ](https://sourceforge.net/p/sakura-editor/patchunicode/)のマージ  
+ MSVC2017でビルド (/O1=[/Og /Os /Oy /Ob2 /Gs /GF /Gy])  
+ Luaを使用 (マクロ, プラグイン)  
+ 挙動の制御 (共有フラグ)としてレジストリを使用しています  


Changed  
-------
※:bug:…バグ, または基本動作として疑問があるもの  

**ファイル系**  

|修正版|公式|
|-|-|
|[sakura.keywordset.csv](https://github.com/uzuki3/sakura/raw/master/Publish/sakura.keywordset.csv)<details><summary>強調キーワードのセットファイルの使用</summary>起動時に列挙したキーワードファイルをインポートします.<br>共通設定からの強調キーワード設定は可能ですが保存はされなくなりますので注意が必要です. 必要に応じてエクスポートしてください.<br>また, `sakura.ini`には出力されなくなります.<br>ファイルがない場合は今まで通りの動作になります.</details>|
|履歴を別ファイル (`sakura.recent`)に出力|`sakura.ini`に出力|
|起動時に存在しないファイル・フォルダ履歴を削除する||
|カラー設定のインポートはカラー情報だけを適用させる|すべての情報を適用|
|<details><summary>マクロ・プラグインに使用できる言語に `Lua`を追加</summary>文字列をLuaコードとして評価する `eval`関数があります.<br>e.g. `local n = eval("(1 + 2 + 3 + 4) * 3.14")`<br>今後はActiveXが使用できるようにしたいです.</details>||
|<details><summary>履歴 (検索, 置換, Grep)の値を少なめに変更</summary>- 検索キー: `16`<br>- 置換キー: `16`<br>- Grepファイル: `8`<br>- Grepフォルダ: `16`|- 検索キー: `30`<br>- 置換キー: `30`<br>- Grepファイル: `30`<br>- Grepフォルダ: `30`|
|多重オープンの許可 (Shiftを押しながらファイルのドロップ)|できない|

**表示系**  

|修正版|公式|
|-|-|
|スクロールバーに検索結果とブックマークを表示||
|:bug:行を中央揃えにする (行の間隔を上下に配分)|上揃え|
|半角スペースを `･`で表示|`o`の下半分を表示|
|NBSPも半角スペースとして `×`で表示|なし|
|タブ文字を線のみで描画 (Sublime Textみたいな)|矢印, または任意の文字|
|EOFのみの行 (起動時とか)にも行番号を表示|なし|
|コメント行の背景カラーを改行以降もその色で描画|なし|
|空白, タブ, 改行, EOFのカラーは現在のテキストカラーから自動で設定|個別にカラー設定|
|数値の色付け判定を正規表現で行う (表現力の向上)<br>正規表現が使用できない場合は通常処理|文字列解析|
|選択範囲カラーは元のテキストカラーをそのまま使用する<br>(Text:0%, Back:100%)|選択範囲カラーとブレンドされる|
|:bug:太字装飾の文字列を選択したときに選択範囲カラーの装飾の影響を受けないように修正|選択範囲カラー設定が使用される|
|カーソル行アンダーラインを行番号から引っ張る|入力エリアのみ|
|:bug:折り返しモードをトグルで切り替えたときに「折り返さない」が処理されていないのを修正|「指定桁で折り返す」のままになる|
|:bug:行番号縦線を行番号の色で描画する|行番号縦線はその行に変更があった場合, その行だけ変更色で縦線が引かれてしまう|
|:bug:行番号が非表示でブックマークが表示のときブックマークは線で描画するように修正|行番号非表示時のブックマーク表示がな|

**操作, 編集系**  

|修正版|公式|
|-|-|
|垂直, 水平スクロールの挙動をメモ帳の挙動と同じにする<br>垂直スクロールマージン１行.<br>水平スクロールマージン１, １６文字移動.|垂直スクロールマージン３行.<br>水平スクロールマージン１, １文字移動.|
|検索時に画面外へ移動するときカーソル行を中央表示|一番近い位置にカーソルが表示される|
|タブ入力文字の切り替え機能<br>(`S_ChangeTabWidth`マクロを修正, 負の値を設定するとタブと空白を相互に切り替えます)|できない|
|:bug:検索マーク切り替え, インクリメンタルサーチの際に検索ダイアログの「正規表現」が影響を受けてしまうのを修正||

**UI系**  

|修正版|公式|
|-|-|
|<details><summary>メインメニューは常にデフォルトを使用する</summary>カスタマイズの効果が感じられなかったのと機能が追加されたときに増えるメニューが対応されず, 機能に気づかないことが多いのでそれなら固定にしたほうが分かりやすいためです.<br>副作用でiniファイルに出力しないのでサイズが軽減されます.</details>|カスタマイズ可能|
|<details><summary>タブと編集ウィンドウのバグ修正とスタイル調整 (モダンに?)</summary>境界線を描画しない, タブを詰める.<br>タブを閉じるボタンをグラフィカルにする.<br>:bug:間に選択タブがあると右側のエッヂがないバグを修正.<br>エディット画面のスタイルから WS_EX_STATICEDGE を外し境界線を描かないようにする.</details>|メニュー下に境界線や編集画面に枠がある|
|リソース (ダイアログ)のフォントを `MS Shell Dlg`へ変更|`ＭＳ Ｐゴシック`|
|変更, キーマクロ記録中のタブ名のカラーを変更||
|タブをダブルクリックで閉じられるようにする||
|選択タブのアクティブ化をマウス押下時に行いレスポンス向上|マウス押上時にアクティブになる|
|Grep フォルダの指定BOXを４つに増やす|１つ|
|Grep 除外フォルダを別ボックスで指定できるようにする|ファイル条件でまとめて指定|
|Grep「現在編集中のファイルから検索」をチェックした時の状態を保持しないようにする|保持される|
|置換ダイアログの置換後テキストに置換前テキストを設定|前回のテキスト|
|正規表現検索のときに正規表現記号をクォート (`^abc$`を検索する場合 `\^abc\$`にする)||
|アウトライン解析ダイアログのドッキング時はコントロールカラーのままにする|背景カラーが使用される|
|<details><summary>ステータスバーのカスタマイズ</summary>:bug:カーソル移動時のちらつき抑制.<br>カラムの並べ替え.<br>左クリックでメニューを表示.<br>「? 行 ? 桁」→「Line ?, Column ?」に変更.<br>タイプ名を表示.<br>タブサイズを表示.<br>入力改行コードを主に使われているシステム名で表記.</details>||
|タグファイル作成時にフォルダの初期値を `tags`, `ctags.cnf`ファイルがあるところまで辿る|カレントフォルダ|
|ダイレクトジャンプ一覧の表示カラムを選別||
|フォルダ選択ダイアログを今風にする||
|各種ダイアログを編集ウィンドウの中央に配置||
|:bug:ルールファイルを設定してアウトライン解析をするとデフォルトが逆順になっているのを修正||


Cregit  
------
+ [Lua](http://www.lua.org/) [Lua: license](http://www.lua.org/license.html)  
  Copyright © 1994–2017 Lua.org, PUC-Rio.  
  

(C) 2017, Uzuki.
