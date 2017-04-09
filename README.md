## サクラエディタ２改造版
* 2.2.0.1をベースに[ココ](http://svn.code.sf.net/p/sakura-editor/code/sakura/trunk2)からマージ<br>
  ベースリビジョンからのマージ情報は[こちら](https://github.com/calette/sakura2201c/blob/master/changes_from_r4011.txt)
* 基本的に元ソースは残しつつの修正
* 他のテキストエディタでよかったところなどを移植
* バグ?とか気になったところの修正
* いくつかの[パッチ](https://sourceforge.net/p/sakura-editor/patchunicode/)をマージ
* プロポーショナルフォント関連はスルー (個人的に使用していないため)

* 動作環境<br>
  + Windows10 RS1以降を対象としています (動作チェックができないので)
  + Visual Studio 2015 の [Visual C++ 再頒布可能パッケージ](https://www.microsoft.com/ja-jp/download/details.aspx?id=48145)が必要
* ビルド環境<br>
  + Windows10 RS1以前や Visual Studio 2015以外でビルドする際は適当にいじってください
  + TCMallocを使用しているので libtcmalloc_minimal.lib が必要です<br>
    [Google Performance Tools](https://github.com/gperftools/gperftools)をビルドしてください<br>
    生成した libtcmalloc_minimal.lib は sakura/ へコピーしてください

<img src="https://raw.github.com/wiki/calette/sakura2201c/images/sakura0.gif" width="865px">
<img src="https://raw.github.com/wiki/calette/sakura2201c/images/sakura1.gif" width="695px">

<br>

### ● 変更内容
いくつかの設定はレジストリで変更できます.<br>
`[HKEY_CURRENT_USER\SOFTWARE\sakura-calette]` を使用します.<br>
エントリが存在しない場合は作成してください.<br>

#### ・ビルド
+ MSVC2015を使用
+ 最適化オプションを O1 に設定
+ ランタイムライブラリを MD に設定
+ _WIN32_WINNT に 0x0A00 を設定
+ ターゲットプラットフォームバージョンに 10.0.14393.0 を設定
+ TCMalloc(Copyright (c) 2005, Google Inc.)を使用.<br>
  (ビルドするには libtcmalloc_minimal.lib を sakura/ 直下にコピーしてください)

#### ・ファイル系
+ 履歴 (検索、置換、Grep)の値を少なめに変更.

+ 多重オープンの許可 (Shiftを押しながらファイルのドロップ).

#### ・操作,編集系
+ キャレットのサイズを変更可能に.

+ 水平スクロールの挙動を変更, メモ帳の挙動と同じにする.

+ タブ入力文字の切り替え機能 (タブ<->空白).<br>
  ・`S_ChangeTabWidth`マクロを修正, 負の値を設定すると相互に切り替えます.

#### ・表示系
+ EOFのみの行 (起動時とか)にも行番号を表示.

+ 行を中央ぞろえにする.<br>
  デフォルトでは上揃えになっていて行の間隔が下に付加されている.<br>
  ![](https://raw.github.com/wiki/calette/sakura2201c/images/image004.gif)

+ 半角空白文字を `･` で描画.<br>
  Sublime Textみて、これだ！って思いました<br>
  ![](https://raw.github.com/wiki/calette/sakura2201c/images/image005.gif)

+ タブ文字を線のみで描画.<br>
  Sublime Textみて(ry<br>
  ![](https://raw.github.com/wiki/calette/sakura2201c/images/image006.gif)

+ コメント行の背景カラーを改行以降も有効にする.<br>
  行コメントとかブロックコメントの背景カラーを設定している場合にわかりやすくなります<br>
  Sublime Tex(ry<br>
  ![](https://raw.github.com/wiki/calette/sakura2201c/images/image007.gif)

+ 空白タブ、改行のカラーは現在のテキストカラーから自動で設定.<br>
  コメント内の空白タブ、改行の色が色分けに影響を受けます.<br>
  Sub(ry<br>
  ![](https://raw.github.com/wiki/calette/sakura2201c/images/image008.gif)

+ 選択範囲カラーのブレンド率を変更.

+ 太字装飾の文字列を選択したときに選択範囲カラーの装飾の影響を受けないように修正.

+ カーソル行アンダーラインを行番号から引っ張る.

#### ・UI系
+ リソース (ダイアログ)のフォントを `9, "ＭＳ Ｐゴシック"` から `9, "MS Shell Dlg"` へ変更.

+ タブ名のカラーを変更.

+ タブをダブルクリックで閉じる.

+ ウィンドウ一覧ポップアップの表示位置のカスタマイズ.

+ 正規表現検索のときに正規表現記号をクォート.<br>
  (`$10^` を検索する場合 `\$10\^` にする)

+ アウトライン解析ダイアログのフォントに設定フォントを使用.<br>
  ドッキング時に背景カラーを使用しない (コントロール色のまま).

+ ステータスバーにパスを表示.<br>
  タブサイズとタイプ名を表示.<br>
  改行コードに主に使われているシステム名を表記.

+ Grepフォルダの指定を物理的に4つに増やした (`;` で区切ると履歴管理が面倒…).

+ Grep「現在編集中のファイルから検索」をチェックした時の状態を保持しないようにする.<br>
  現在編集中からのGrepって「今回だけ！」ってことが多いと思います.

+ 置換ダイアログの置換後テキストに置換前テキストを設定.

+ ダイレクトジャンプ一覧の表示カラムを選別.

### ● バグっぽいのを修正
+ 検索マーク切り替え、インクリメンタルサーチの際に検索ダイアログの「正規表現」が影響を受けないように.<br>
  常時、正規表現で検索しているとコレ結構ストレスたまります.

+ カーソル移動時に描画が崩れる問題の仮対応.<br>
  キーリピートの時間が速かったり、MacType使ってると負荷がかかってるみたいで描画が崩れたり行番号と本文の描画が同期してなかったりして.ます<br>
  あんまりいい修正方法ではありませんが受けるストレスのほうが大事なので気にせず修正しました.<br>
  この修正がこの改造版のすべてかと思います.

### ● レジストリ詳細
[HKEY_CURRENT_USER\SOFTWARE\sakura-calette]<br>
存在しない場合は作成してください.<br>

+ CaretType (dword)<br>
    キャレットのサイズを…<br>
      `0`: 変更なし<br>
      `1-10`: 指定サイズ (default:2)<br>
      `11`: 1バイトコードの時は1px、2バイトコードの時は2px<br>
      `12`: 半角入力の時は1px、全角入力の時は2px<br>

+ DoubleClickClosesTab (dword)<br>
    タブをダブルクリックで<br>
      `0`: 閉じない<br>
      `1`: 閉じる (default)<br>

+ NoOutlineDockSystemColor (dword)<br>
    アウトライン解析ダイアログをドッキングしたときの背景カラーに…<br>
      `0`: システムカラーを使う (default)<br>
      `1`: システムカラーを使わない<br>

+ PlaceDialogWindowLeft (dword)<br>
    ダイアログの横表示位置を変更<br>
    次の場合は編集ウィンドウの横半分(1/2)の位置に表示される<br>
```
      21 (default)
      ||
      |+-- 分子
      +--- 分母
```

+ PlaceDialogWindowTop (dword)<br>
    ダイアログの縦表示位置を変更<br>
    次の場合は編集ウィンドウの4/7の位置に表示される<br>
```
      74 (default)
      ||
      |+-- 分子
      +--- 分母
```

+ RecentSearchKeyMax (dword)<br>
    検索履歴数を変更<br>
      `16`: (default)<br>

+ RecentGrepFileMax (dword)<br>
    Grep履歴数を変更<br>
      `8`: (default)<br>

+ RecentGrepFolderMax (dword)<br>
    Grepフォルダ履歴数を変更<br>
      `16`: (default)<br>

+ RecentReplaceKeyMax (dword)<br>
    置換履歴数を変更<br>
      `16`: (default)<br>

+ RegexpAutoQuote (dword)<br>
    検索・置換時に「正規表現」を使用する場合、文字列を…<br>
      `0`: クォートしない<br>
      `1`: クォートする (default)<br>

+ ReplaceTextToText (dword)<br>
    置換時に「置換前」テキストを「置換後」に…<br>
      `0`: 設定しない<br>
      `1`: 設定する (default)<br>

+ SelectAreaBlendPer (dword)<br>
    選択範囲カラーのブレンド率を設定<br>
      `1-8bit`: 背景色ブレンド率 [0-100] default:100<br>
      `9-16bit`: テキスト色ブレンド率 [0-100] default:0<br>

+ WhiteSpaceBlendPer (dword)<br>
    空白タブのテキストとのブレンド率を設定<br>
      `1-8bit`: ブレンド率 [0-100] default:30<br>

+ TabCaptionModifiedColor (dword)<br>
    変更時のタブ名のテキストカラーを設定, 形式は 0x00BBGGRR<br>
      `0x00d70000`: (default)<br>

+ TabCaptionRecMacroColor (dword)<br>
    キーマクロ記録時のタブ名のテキストカラーを設定, 形式は 0x00BBGGRR<br>
      `0x000000d8`: (default)<br>

+ WinListPopupTop (dword)<br>
    ウィンドウ一覧ポップアップの表示位置を変更 (左上基準)<br>
      `0xffffffff`: センタリング (default)<br>

+ WinListPopupLeft (dword)<br>
    ウィンドウ一覧ポップアップの表示位置を変更 (左上基準)<br>
      `0xffffffff`: センタリング (default)<br>

+ WinListPopupWidth (dword)<br>
    ウィンドウ一覧ポップアップの表示幅を変更 (タブアイコン表示のときのみ有効)<br>
      `400`: (default)<br>
