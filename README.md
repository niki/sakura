## サクラエディタ２改造版
* 2.2.0.1をベースに[ココ](http://svn.code.sf.net/p/sakura-editor/code/sakura/trunk2)からマージ
* 基本的に元ソースは残しつつの修正
* 他のテキストエディタでよかったところを移植
* バグ?とか気になったところの修正
* いくつかの[パッチ](https://sourceforge.net/p/sakura-editor/patchunicode/)をマージ
* プロポーショナルフォント関連はスルー(個人的に使用しないため)

ベースリビジョンからのマージ情報は[こちら](https://github.com/rabbiteariris/sakura2201R/blob/master/changes_from_r4011.txt)

![](https://raw.github.com/wiki/rabbiteariris/sakura2201R/images/sakura0.png)
![](https://raw.github.com/wiki/rabbiteariris/sakura2201R/images/sakura1.png)

<br>

### ● 変更内容<br>
いくつかの設定はレジストリで変更できます.<br>
`[HKEY_CURRENT_USER\SOFTWARE\sakura_REI]` を参照してください.<br>

#### ・ファイル系

+ 設定情報の読み書きにレジストリを選択可能にする<br>
  レジストリから設定情報を読み込む場合は<br>
    `NoReadProfilesFromRegistry:dword:00000000`<br>
  レジストリに設定情報を書き込む場合は<br>
    `NoWriteProfilesToRegistry:dword:00000000`<br>
  としてください。
  レジストリキーがない場合はiniファイルから読み込みます.<br>
  アンインストールする際は `[HKEY_CURRENT_USER\SOFTWARE\sakura.ini]` を削除してください(キー名はプロファイル名で変わりますので注意).<br>

+ 履歴(検索、置換、Grep)の値を変更<br>
  検索 30→16 `RecentSearchKeyMax=dword:00000010`<br>
  置換 30→16 `RecentReplaceKeyMax=dword:00000010`<br>
  Grepファイル 30→8 `RecentGrepFileMax=dword:00000008`<br>
  Grepフォルダ 30→16 `RecentGrepFolderMax=dword:00000010`<br>

+ 多重オープンの許可<br>
  Shiftを押しながらファイルのドロップで同じファイルでも新しいウィンドウで開きます.<br>

#### ・操作,編集系

+ キャレットのサイズを変更可能に<br>
  レジストリにより何種類か変更できます `CaretType=dword:00000002`<br>
    `0`: 変更なし<br>
    `1-10`: サイズ<br>
    `11`: 1バイトコードの時は1px、2バイトコードの時は2px<br>
    `12`: 半角入力の時は1px、全角入力の時は2px<br>

+ 水平スクロールの挙動を変更, メモ帳の挙動と同じにする<br>
  ・スクロール開始マージンを `1` に変更。画面の端でスクロール開始<br>
  ・スクロール幅を `16` に設定。一度に大きく移動<br>

+ タブ入力文字の切り替え機能(タブ<->空白)<br>
  ・`S_ChangeTabWidth`マクロを修正, 負の値を設定すると相互に切り替えます<br>

#### ・表示系

+ EOFのみの行(起動時とか)にも行番号を表示<br>

+ 行を中央ぞろえにする<br>
  デフォルトでは上揃えになっていて行の間隔が下に付加されている.<br>
  ![](https://raw.github.com/wiki/rabbiteariris/sakura2201R/images/image004.png)

+ 半角空白文字を `･` で描画<br>
  Sublime Textみて、これだ！って思いました<br>
  ![](https://raw.github.com/wiki/rabbiteariris/sakura2201R/images/image005.png)

+ タブ文字を線のみで描画<br>
  Sublime Textみて(ry<br>
  ![](https://raw.github.com/wiki/rabbiteariris/sakura2201R/images/image006.png)

+ コメント行の背景カラーを改行以降も有効にする<br>
  行コメントとかブロックコメントの背景カラーを設定している場合にわかりやすくなります<br>
  Sublime Tex(ry<br>
  ![](https://raw.github.com/wiki/rabbiteariris/sakura2201R/images/image007.png)

+ 空白タブ、改行のカラーは現在のテキストカラーから自動で設定<br>
  コメント内の空白タブ、改行の色が色分けに影響を受けます.<br>
  Sub(ry<br>
  ![](https://raw.github.com/wiki/rabbiteariris/sakura2201R/images/image008.png)

+ 選択範囲を変更<br>
  テキストと背景のブレント率を設定できるように.<br>
  デフォルトではテキスト 0%、背景を 100% 選択カラーにします.<br>
  あと、太字装飾の文字列を選択したときに選択範囲カラーの装飾の影響を受けないように修正.<br>

+ カーソル行アンダーラインを行番号から引っ張る<br>

+ 折り返しの縦線は引かないようにした<br>

#### ・UI系

+ リソース(ダイアログ)のフォントを `9, "ＭＳ Ｐゴシック"` から `9, "MS Shell Dlg"` へ変更<br>

+ タブ名のカラーを変更<br>
  ・変更 `TabCaptionColorModified=dword:00d70000`<br>
  ・キーマクロ記録中 `TabCaptionColorRecMacro=dword:000000d8`<br>

+ 正規表現検索のときに正規表現記号をクォート<br>
  (`$10` を検索する場合 `\$10` にする)<br>

+ アウトライン解析ダイアログのフォントに設定フォントを使用<br>
  ドッキング時に背景カラーを使用しない(コントロール色のまま)<br>

+ ステータスバーにパスを表示.<br>
  タブサイズとタイプ名を表示.<br>
  改行コードに主に使われているシステム名を表記.<br>

+ Grepフォルダの指定を物理的に4つに増やした(`;` で区切ると履歴管理が面倒…)<br>

+ Grep「現在編集中のファイルから検索」をチェックした時の状態を保持しないようにする<br>
  現在編集中からのGrepって「今回だけ！」ってことが多いと思います.<br>

+ 置換ダイアログの置換後テキストに置換前テキストを設定<br>

+ ダイレクトジャンプ一覧のカラムを選別<br>

### ● バグっぽいのを修正<br>
+ 検索マーク切り替え、インクリメンタルサーチの際に検索ダイアログの「正規表現」が影響を受けないように<br>
  常時、正規表現で検索しているとコレ結構ストレスたまります<br>

+ カーソル移動時に描画が崩れる問題の仮対応<br>
  キーリピートの時間が速かったり、MacType使ってると負荷がかかってるみたいで描画が崩れたり行番号と本文の描画が同期してなかったりしてます<br>
  あんまりいい修正方法ではありませんが受けるストレスのほうが大事なので気にせず修正しました<br>
  この修正がこの改造版のすべてかと思います<br>
