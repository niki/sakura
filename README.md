Introduction  
------------

サクラエディタのリポジトリ([link](http://svn.code.sf.net/p/sakura-editor/code/sakura/trunk2))の[追っかけ](Publish/changes_from_r4011.txt). パッチ([link](#patchunicode))のマージ.  

普段使っていてたまったヘイト値をどかっと吐き出して修正しているのでそれなりに良くなっているかと思います.  
個人的な趣味でマクロとプラグインに **Lua** を使用できます. 将来的には **Python** も使えるようにしたいです.  

<details open><summary>修正は以下の開発環境を基準に快適に動かせる範囲で行っています.</summary>
- OS: Windows10 Home 1703<br>
- CPU: Celeron 3215U 1.7GHz<br>
- Memory: 8GB<br>
- Compiler: Visual Studio Community 2017<br>
- Heavy software used: MacType, ESET<br>
</details>


<br>

Download & Setup  
----------------

|ファイル名||主な変更内容|サイズ|日付||
|-|-|-|-|-|-|
|sakura-uzuki-2.46.zip|32bit|マークキャッシュ作成,描画のスレッド化|822KB|2017.7.9|[download](http://mimix.sakura.ne.jp/release/sakura-uzuki-2.46.zip)|
|keyword_pack.zip||patchunicode:#720対応|355KB|2017.6.16|[download](Publish/keyword_pack.zip)|

ダウンロードしたファイルをすでに使用しているサクラエディタに上書きしてください.  
`sakura.keywordset.csv`は実行ファイルと同じ場所に置いてください. 実行ファイル名が違う場合は`sakura`の部分を変更してください.  

変更履歴はコミットログ([link](https://github.com/uzuki3/sakura/commits/master))を参照してください. 細かい内容はmy_config.h([link](sakura_core/my_config.h)) を見てね.  


<br>

Changed  
-------
:star: - お気に入り<br>
:bug: - バグ, または直したほうがいいと思うもの  

**動作に影響する修正**

- 強調キーワードのセットファイル ([sakura.keywordset.csv](Publish/sakura.keywordset.csv))の使用
  <details><summary>more..</summary>
  <pre>
  起動時に列挙したキーワードファイルをインポートします.
  共通設定からの強調キーワード設定は可能ですが保存はされなくなりますので注意.
  また, `sakura.ini`には出力されなくなりますのでダイエットにもなります.
  ファイルがない場合は今まで通りの動作です.
  </pre>
  </details>
- 履歴を別ファイル (`sakura.recent`)に出力
- 起動時に存在しないファイル・フォルダ履歴を削除する. 消さないでグレー表示でもいいかも:memo:
- マクロ・プラグインに使用できる言語に `Lua`を追加:star:
  <details><summary>more..</summary>
  <pre>
  文字列をLuaコードとして評価する `eval`関数があります.
    e.g. `local n = eval("(1 + 2 + 3 + 4) * 3.14")`
  </pre>
  <img src="https://raw.github.com/wiki/uzuki3/sakura/images/sakura_201706041845.png" width="50%">
  </details>
- タイプ別設定一覧の「追加」から任意のタイプを追加できるようにする:star:
- デフォルト文字コードを UTF8にする
- カラー設定のインポートはカラー情報だけを適用させる. カラー設定を使い分けることってあるのかな:memo:
- 履歴 (検索, 置換, Grep)の値を変更
  <details><summary>more..</summary>
  <pre>
  検索キー: `20`
  置換キー: `20`
  Grepファイル: `10`
  Grepフォルダ: `20`
  </pre>
  </details>
- タブをダブルクリックで閉じられるようにする
- 選択タブのアクティブ化をマウス押下時に行いレスポンス向上
- 検索ダイアログをVisualStudioのような挙動にする
  <details><summary>more..</summary>
  <pre>
  入力中にもインクリメンタルに検索されます.
  ダイアログを開いたまま「次を検索」「前を検索」のショートカットが有効です.
  </pre>
  </details>
- Grep フォルダの指定BOXを３つに増やし, 除外フォルダを別ボックスで指定できるようにする
  <details><summary>more..</summary>
  <img src="https://raw.github.com/wiki/uzuki3/sakura/images/sakura_201706041815.png" width="50%">
  </details>
- Grep パターン変数を使用できるようにする
  <details><summary>more..</summary>
  <pre>
  レジストリ `HKEY_CURRENT_USER\Software\sakura-uzuki`への追加が必要です.
    `"$cpp"="*.c *.cpp *.cc *.cxx *.c++ *.h *.hpp"`
    `"$make"="makefile *.mak *.om OMakefile OMakeRoot"`
  上記のようにレジストリエントリを追加し、Grepのファイルに `$cpp`を指定すると `*.c *.cpp *.cc *.cxx *.c++ *.h *.hpp`で置き換えます.
  </pre>
  </details>
- 多重オープンの許可 (Shiftを押しながらファイルのドロップ)
- 折り返しモードをトグルで切り替えたときに「折り返さない」が処理されていないのを修正:bug:
- 垂直, 水平スクロールの挙動をメモ帳の挙動と同じにする
  <details><summary>more..</summary>
  <pre>
  垂直スクロールマージン１行
  水平スクロールマージン１, １６文字移動
  </pre>
  </details>
- 検索時に画面外へ移動するときカーソル行を中央表示
- タブ入力文字の切り替え機能
  <details><summary>more..</summary>
  <pre>
  (`S_ChangeTabWidth`マクロを修正, 負の値を設定するとタブと空白を相互に切り替えます)
  </pre>
  </details>
- 検索マーク切り替え, インクリメンタルサーチの際に検索ダイアログの「正規表現」が影響を受けてしまうのを修正:bug:
- メインメニューは常にデフォルトを使用する
  <details><summary>more..</summary>
  <pre>
  カスタマイズの効果が感じられなかったのと機能が追加されたときに増えるメニューが対応されず, 
  機能に気づかないことが多いのでいっそのこと固定にしちゃったほうがわかりやすいので.
  副作用でiniファイルに出力しないのでサイズが軽減されます.
  </pre>
  </details>
- Grep「現在編集中のファイルから検索」をチェックした時の状態を保持しないようにする
- 置換ダイアログの置換後テキストに置換前テキストを設定
- 正規表現検索のときに正規表現記号をクォート (`^abc$`を検索する場合 `\^abc\$`にする)
- ステータスバーのカスタマイズ
  <details><summary>more..</summary>
  <pre>
  カーソル移動時のちらつき抑制
  カラムの並べ替え
  左クリックでメニューを表示
  「? 行 ? 桁」→「Ln ? Col ?」に変更
  タイプ名を表示
  タブサイズを表示
  入力改行コードを主に使われているシステム名で表記
  </pre>
  </details>
- タグファイル作成時にフォルダの初期値を `tags`, `ctags.cnf`ファイルがあるところまで辿る


**表示に影響する修正**

- スクロールバーに検索結果とブックマーク, カーソル位置を表示:star:
  <details><summary>more..</summary>
  <img src="https://raw.github.com/wiki/uzuki3/sakura/images/sakura_201706092354.png" width="50%">
  </details>
- 行を中央揃えにする (行の間隔を上下に配分):bug:
- 半角スペースを `･`で表示
- NBSPも半角スペースとして `×`で表示
- タブ文字を線のみで描画 (Sublime Textみたいな)
- EOFのみの行 (起動時とか)にも行番号を表示
- コメント行の背景カラーを改行以降もその色で描画
- 空白, タブ, 改行, EOF, ノート線のカラーは現在のテキストカラーから自動で設定
- 数値の色付け判定を正規表現で行う (表現力の向上). 正規表現が使用できない場合は通常処理
- 選択範囲カラーは元のテキストカラーをそのまま使用する (Text:0%, Back:100%)
- 太字装飾の文字列を選択したときに選択範囲カラーの装飾の影響を受けないように修正:bug:
- カーソル行アンダーラインを行番号から引っ張る
- 行番号縦線を行番号の色で描画する:bug:
- 行番号が非表示でブックマークが表示のときブックマークは線で描画するように修正:bug:
- 行番号背景が行番号縦線をはみ出しているのを修正:bug:
- タブウィンドウ
  <details><summary>more..</summary>
  <pre>
  タブを閉じるボタンをグラフィカルにする
  間に選択タブがあると右側のエッヂがないバグを修正
  </pre>
  </details>
- リソース (ダイアログ)のフォントを `MS Shell Dlg`へ変更
- 変更, キーマクロ記録中のタブ名のカラーを変更
- アウトライン解析ダイアログのドッキング時はコントロールカラーのままにする
- ダイレクトジャンプ一覧の表示カラムを選別
- フォルダ選択ダイアログを今風にする
- 各種ダイアログを編集ウィンドウの中央に配置
- ルールファイルを設定してアウトライン解析をするとデフォルトが逆順になっているのを修正:bug:


**その他の修正**

- 偶数行背景はEOF以降は適用しない
- ノート線はEOF以降は適用しない
- ExtTextOutによる塗りつぶしをPatBltに変更
- WM_ERASEBKGNDの抑制


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
