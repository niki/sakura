Introduction  
------------

サクラエディタのリポジトリ([link](http://svn.code.sf.net/p/sakura-editor/code/sakura/trunk2))の[追っかけ](Publish/changes_from_r4011.txt). パッチ([link](#patchunicode))のマージ.  

普段使っていてたまったヘイト値をどかっと吐き出して修正しているのでそれなりに良くなっているかと思います.  
個人的な趣味でマクロとプラグインに **Lua** を使用できます. 将来的には **Python** も使えるようにしたいです.  

<details open><summary>修正は以下の開発環境を基準に快適に動かせる範囲で行っています.</summary>
<ul>
<li>OS: Windows10 Home 1703
<li>CPU: Celeron 3215U 1.7GHz
<li>Memory: 8GB
<li>Compiler: Visual Studio Community 2017
<li>Heavy software used: MacType, ESET
</details>


<br>

Download & Setup  
----------------

|ファイル名||主な変更内容|サイズ|日付||
|-|-|-|-|-|-|
|sakura-uzuki-2.47.zip|32bit||822KB|2017.7.9|[download](http://mimix.sakura.ne.jp/release/sakura-uzuki-2.47.zip)|
|sakura-uzuki-2.47-64.zip|64bit||980KB|2017.7.9|[download](http://mimix.sakura.ne.jp/release/sakura-uzuki-2.47-64.zip)|
|keyword_pack.zip||patchunicode:#720対応|355KB|2017.6.16|[download](Publish/keyword_pack.zip)|

ダウンロードしたファイルをすでに使用しているサクラエディタに上書きしてください.  
`sakura.keywordset.csv`は実行ファイルと同じ場所に置いてください. 実行ファイル名が違う場合は`sakura`の部分を変更してください.  

変更履歴はコミットログ([link](https://github.com/uzuki3/sakura/commits/master))を参照してください. 細かい内容はmy_config.h([link](sakura_core/my_config.h)) を見てね.  


<br>

Changed  
-------

<details open><summary>ファイ/設定/機能</summary>
<ul>
<li>マクロ・プラグインに使用できる言語に `Lua`を追加
<li>強調キーワードのセットファイル sakura.keywordset.csv の使用
<li>履歴を別ファイル (`sakura.recent`)に出力
<li>起動時に存在しないファイル・フォルダ履歴を削除する
<li>タイプ別設定一覧の「追加」から任意のタイプを追加できるようにする
<li>デフォルト文字コードを UTF8にする
<li>カラー設定のインポートはカラー情報だけを適用させる
<li>履歴 (検索, 置換, Grep)の値を変更
<li>多重オープンの許可 (Shiftを押しながらファイルのドロップ)
</details>

<details open><summary>表示/編集</summary>
<ul>
<li>スクロールバーに検索結果とブックマーク, カーソル位置を表示
<li>折り返しモードをトグルで切り替えたときに「折り返さない」が処理されていないのを修正 (bug?)
<li>垂直, 水平スクロールの挙動をメモ帳の挙動と同じにする (端でスクロールするようになります)
<li>検索時に画面外へ移動するときカーソル行を中央表示
<li>タブ入力文字の切り替え機能 (`S_ChangeTabWidth`マクロを修正, 負の値を設定するとタブと空白を相互に切り替えます)
<li>検索マーク切り替え, インクリメンタルサーチの際に検索ダイアログの「正規表現」が影響を受けてしまうのを修正 (bug?)
<li>行を中央揃えにする (行の間隔を上下に配分) (bug?)
<li>半角スペースを `･`で表示
<li>NBSPも半角スペースとして `×`で表示
<li>タブ文字を線のみで描画 (Sublime Textみたいな)
<li>EOFのみの行 (起動時とか)にも行番号を表示
<li>コメント行の背景カラーを改行以降もその色で描画
<li>空白, タブ, 改行, EOF, ノート線のカラーは現在のテキストカラーから自動で設定
<li>数値の色付け判定を正規表現で行う (表現力の向上). 正規表現が使用できない場合は通常処理
<li>選択範囲カラーは元のテキストカラーをそのまま使用し、背景のみ描画する
<li>太字装飾の文字列を選択したときに選択範囲カラーの装飾の影響を受けないように修正 (bug?)
<li>カーソル行アンダーラインを行番号から引っ張る
<li>行番号縦線を行番号の色で描画する (bug?)
<li>行番号が非表示でブックマークが表示のときブックマークは線で描画するように修正 (bug?)
<li>行番号背景が行番号縦線をはみ出しているのを修正 (bug?)
<li>偶数行背景はEOF以降は適用しない
<li>ノート線はEOF以降は適用しない
</details>

<details open><summary>UI</summary>
<ul>
<li>タブをダブルクリックで閉じられるようにする
<li>選択タブのアクティブ化をマウス押下時に行いレスポンス向上
<li>検索ダイアログをVisualStudioのような挙動にする
<li>Grep フォルダの指定BOXを３つに増やし, 除外フォルダを別ボックスで指定できるようにする
<li>Grep パターン変数を使用できるようにする (レジストリ `HKEY_CURRENT_USER\Software\sakura-uzuki`への追加が必要です)
<li>メインメニューは常にデフォルトを使用する
<li>Grep「現在編集中のファイルから検索」をチェックした時の状態を保持しないようにする
<li>置換ダイアログの置換後テキストに置換前テキストを設定
<li>正規表現検索のときに正規表現記号をクォート (`^abc$`を検索する場合 `\^abc\$`にする)
<li>ステータスバーのカスタマイズ
<li>タグファイル作成時にフォルダの初期値を `tags`, `ctags.cnf`ファイルがあるところまで辿る
<li>タブを閉じるボタンをグラフィカルにする
<li>間に選択タブがあると右側のエッヂがないバグを修正 (bug?)
<li>リソース (ダイアログ)のフォントを `MS Shell Dlg`へ変更
<li>変更, キーマクロ記録中のタブ名のカラーを変更
<li>アウトライン解析ダイアログのドッキング時はコントロールカラーのままにする
<li>ダイレクトジャンプ一覧の表示カラムを選別
<li>フォルダ選択ダイアログを今風にする
<li>各種ダイアログを編集ウィンドウの中央に配置
<li>ルールファイルを設定してアウトライン解析をするとデフォルトが逆順になっているのを修正:bug:
</details>

<details open><summary>その他</summary>
<ul>
<li>ExtTextOutによる塗りつぶしをPatBltに変更
<li>WM_ERASEBKGNDの抑制
</details>


Luaマクロ<br>
<img src="https://raw.github.com/wiki/uzuki3/sakura/images/sakura_201706041845.png" width="50%"><br>
Grep<br>
<img src="https://raw.github.com/wiki/uzuki3/sakura/images/sakura_201706041815.png" width="50%"><br>
検索マーク<br>
<img src="https://raw.github.com/wiki/uzuki3/sakura/images/sakura_201706092354.png" width="50%"><br>


<br>

**<a name="patchunicode">マージ済みパッチ**  

- [patchunicode:#1065](https://sourceforge.net/p/sakura-editor/patchunicode/1065/) ~~他のドキュメントから入力補完~~
- [patchunicode:#1050](https://sourceforge.net/p/sakura-editor/patchunicode/1050/) ~~エンコーディング名による文字コードの設定の修正~~
- [patchunicode:#1047](https://sourceforge.net/p/sakura-editor/patchunicode/1047/) プロポーショナル版で変更された単語単位移動を戻す
- [patchunicode:#1006](https://sourceforge.net/p/sakura-editor/patchunicode/1006/) 改行文字部分とそれより後ろのキャレット移動に関して
- [patchunicode:#830](https://sourceforge.net/p/sakura-editor/patchunicode/830/) ~~マクロの文字列コピーを減らす~~
- [patchunicode:#720](https://sourceforge.net/p/sakura-editor/patchunicode/720/) タイプ別設定の追加と強調キーワードの外部化


<br>

Cregit  
------
+ Lua ([link](http://www.lua.org/))  
  Copyright (C) 1994-2017 Lua.org, PUC-Rio  
  R. Ierusalimschy, L. H. de Figueiredo, W. Celes  
  [License](http://www.lua.org/license.html)  

---
(C) 2017, Uzuki.
