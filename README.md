Introduction  
------------

可能な限り最新版をベースに修正をしています.  
普段使いながらヘイト値ためて, どかっと修正しているのでそれなりに良くなっているかと思います。


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

+ [sakura-uzuki-2.41-32bit.zip](http://mimix.sakura.ne.jp/release/sakura-uzuki-2.41-32bit.zip) (768KB)  

\- 変更履歴は[コミットログ](https://github.com/uzuki3/sakura/commits/master)を参照してください. 細かい内容は [my_config.h](sakura_core/my_config.h) を見てね.  
\- 動作には[Visual Studio 2017 Microsoft Visual C++ 再頒布可能パッケージ](https://www.visualstudio.com/ja/downloads/#other-ja)が必要です.  



Setup  
-----

ダウンロードしたファイルをすでに使用しているサクラエディタに上書きしてください.  
`sakura.keywordset.csv`は実行ファイルと同じ場所に置いてください. 実行ファイル名が違う場合は`sakura`の部分を変更してください.  

最新のキーワードファイル [keyword_pack.zip](Publish/keyword_pack.zip)  


Build environment  
-----------------
+ [サクラエディタのリポジトリ](http://svn.code.sf.net/p/sakura-editor/code/sakura/trunk2)の[追っかけ](Publish/changes_from_r4011.txt). [パッチのマージ](#patchunicode)  
+ MSVC2017でビルド (/O2 /Os)  
+ Luaを使用 (マクロ, プラグイン)  
+ 挙動の制御 (共有フラグ)としてレジストリを使用しています  


Changed  
-------
※:bug:…バグ, または直したほうがいいと思うもの  

**動作に影響する修正**

||内容|
|-|-|
||<details><summary>強調キーワードのセットファイル ([sakura.keywordset.csv](Publish/sakura.keywordset.csv))の使用</summary>起動時に列挙したキーワードファイルをインポートします.<br>共通設定からの強調キーワード設定は可能ですが保存はされなくなりますので注意.<br>また, `sakura.ini`には出力されなくなりますのでダイエットにもなります.<br>ファイルがない場合は今まで通りの動作です.</details>|
||履歴を別ファイル (`sakura.recent`)に出力|
||起動時に存在しないファイル・フォルダ履歴を削除する<br>:memo:消さないでグレー表示でもいいかも…|
|:star:|<details><summary>マクロ・プラグインに使用できる言語に `Lua`を追加</summary>:memo:文字列をLuaコードとして評価する `eval`関数があります.<br>　e.g. `local n = eval("(1 + 2 + 3 + 4) * 3.14")`<br>:memo:今後はActiveXが使用できるようにしたいです.</details>|
|:star:|タイプ別設定一覧の「追加」から任意のタイプを追加できるようにする|
||デフォルト文字コードを UTF8にする|
||カラー設定のインポートはカラー情報だけを適用させる<br>:memo:カラー設定を使い分けることってあるのかな|
||履歴 (検索, 置換, Grep)の値を変更<br><ul><li>検索キー: `20`<li>置換キー: `20`<li>Grepファイル: `10`<li>Grepフォルダ: `20`</ul>|
||タブをダブルクリックで閉じられるようにする|
||選択タブのアクティブ化をマウス押下時に行いレスポンス向上|
||Grep フォルダの指定BOXを４つに増やす|
||Grep 除外フォルダを別ボックスで指定できるようにする|
||<details><summary>Grep パターン変数を使用できるようにする</summary>レジストリ `HKEY_CURRENT_USER\Software\sakura-uzuki`への追加が必要です.<br>`"$cpp"="*.c *.cpp *.cc *.cxx *.c++ *.h *.hpp"`<br>`"$make"="makefile *.mak *.om OMakefile OMakeRoot"`<br>上記のようにレジストリエントリを追加し、Grepのファイルに `$cpp`を指定すると `*.c *.cpp *.cc *.cxx *.c++ *.h *.hpp`で置き換えます.</details>|
||多重オープンの許可 (Shiftを押しながらファイルのドロップ)|
|:bug:|折り返しモードをトグルで切り替えたときに「折り返さない」が処理されていないのを修正|
||垂直, 水平スクロールの挙動をメモ帳の挙動と同じにする<ul><li>垂直スクロールマージン１行<li>水平スクロールマージン１, １６文字移動</ul>|
||検索時に画面外へ移動するときカーソル行を中央表示|
||検索ダイアログで検索するときに文字入力の度に検索(インクリメンタルサーチ)<br>邪魔にならないようにダイアログレイアウトの調整|
||タブ入力文字の切り替え機能<br>(`S_ChangeTabWidth`マクロを修正, 負の値を設定するとタブと空白を相互に切り替えます)|
|:bug:|検索マーク切り替え, インクリメンタルサーチの際に検索ダイアログの「正規表現」が影響を受けてしまうのを修正|
||<details><summary>メインメニューは常にデフォルトを使用する</summary>:memo:カスタマイズの効果が感じられなかったのと機能が追加されたときに増えるメニューが対応されず, 機能に気づかないことが多いのでいっそのこと固定にしちゃったほうがわかりやすいので. 副作用でiniファイルに出力しないのでサイズが軽減されます.</details>|
||Grep「現在編集中のファイルから検索」をチェックした時の状態を保持しないようにする|
||置換ダイアログの置換後テキストに置換前テキストを設定|
||正規表現検索のときに正規表現記号をクォート (`^abc$`を検索する場合 `\^abc\$`にする)|
||<details><summary>ステータスバーのカスタマイズ</summary><ul><li>:bug:カーソル移動時のちらつき抑制<li>カラムの並べ替え<li>左クリックでメニューを表示<li>「? 行 ? 桁」→「Ln ? Col ?」に変更<li>タイプ名を表示<li>タブサイズを表示<li>入力改行コードを主に使われているシステム名で表記</ul></details>|
||タグファイル作成時にフォルダの初期値を `tags`, `ctags.cnf`ファイルがあるところまで辿る|


**表示に影響する修正**

||内容|
|-|-|
|:star:|スクロールバーに検索結果とブックマーク, カーソル位置を表示|
|:bug:|行を中央揃えにする (行の間隔を上下に配分)|
||半角スペースを `･`で表示|
||NBSPも半角スペースとして `×`で表示|
||タブ文字を線のみで描画 (Sublime Textみたいな)|
||EOFのみの行 (起動時とか)にも行番号を表示|
||コメント行の背景カラーを改行以降もその色で描画|
||空白, タブ, 改行, EOF, ノート線のカラーは現在のテキストカラーから自動で設定|
||数値の色付け判定を正規表現で行う (表現力の向上)<br>正規表現が使用できない場合は通常処理|
||選択範囲カラーは元のテキストカラーをそのまま使用する<br>(Text:0%, Back:100%)|
|:bug:|太字装飾の文字列を選択したときに選択範囲カラーの装飾の影響を受けないように修正|
||カーソル行アンダーラインを行番号から引っ張る|
|:bug:|行番号縦線を行番号の色で描画する|
|:bug:|行番号が非表示でブックマークが表示のときブックマークは線で描画するように修正|
|:bug:|行番号背景が行番号縦線をはみ出しているのを修正|
|:bug:|偶数行背景はEOF以降は適用しない|
|:bug:|ノート線はEOF以降は適用しない|
||<details><summary>タブと編集ウィンドウのバグ修正とスタイル調整 (モダンに?)</summary><ul><li>境界線を描画しない, タブを詰める.<li>タブを閉じるボタンをグラフィカルにする.<li>:bug:間に選択タブがあると右側のエッヂがないバグを修正.<li>エディット画面のスタイルから WS_EX_STATICEDGE を外し境界線を描かないようにする.</ul></details>|
||リソース (ダイアログ)のフォントを `MS Shell Dlg`へ変更|
||変更, キーマクロ記録中のタブ名のカラーを変更|
||アウトライン解析ダイアログのドッキング時はコントロールカラーのままにする|
||ダイレクトジャンプ一覧の表示カラムを選別|
||フォルダ選択ダイアログを今風にする|
||各種ダイアログを編集ウィンドウの中央に配置|
|:bug:|ルールファイルを設定してアウトライン解析をするとデフォルトが逆順になっているのを修正|



**<a name="patchunicode">マージ済みパッチ**  

|カテゴリ|内容|
|-|-|
||[patchunicode:#1065](https://sourceforge.net/p/sakura-editor/patchunicode/1065/) ~~他のドキュメントから入力補完~~|
||[patchunicode:#1050](https://sourceforge.net/p/sakura-editor/patchunicode/1050/) ~~エンコーディング名による文字コードの設定の修正~~|
||[patchunicode:#1047](https://sourceforge.net/p/sakura-editor/patchunicode/1047/) プロポーショナル版で変更された単語単位移動を戻す|
||[patchunicode:#1006](https://sourceforge.net/p/sakura-editor/patchunicode/1006/) 改行文字部分とそれより後ろのキャレット移動に関して|
||[patchunicode:#830](https://sourceforge.net/p/sakura-editor/patchunicode/830/) ~~マクロの文字列コピーを減らす~~|
||[patchunicode:#720](https://sourceforge.net/p/sakura-editor/patchunicode/720/) タイプ別設定の追加と強調キーワードの外部化|

Cregit  
------
+ Lua  
  http://www.lua.org/  [License](http://www.lua.org/license.html)
  Copyright (C) 1994-2017 Lua.org, PUC-Rio  
	R. Ierusalimschy, L. H. de Figueiredo, W. Celes  


(C) 2017, Uzuki.
