# キー割り当てプリセット

サクラエディタの「共通設定」→「キー割り当て」→「インポート」で読み込める `.key` ファイル集です。
それぞれ、他のエディタ/IDEのキーマップに近い操作感になるよう、対応するサクラエディタの機能を
主要なショートカットに割り当てています。

## 対応表

| ファイル | 対象キーマップ |
|---|---|
| VSCode.key | Visual Studio Code |
| VisualStudio.key | Visual Studio（現行版の既定キーマップ） |
| VisualStudio6.key | Visual Studio 6 / Visual C++ 6 |
| VisualBasic6.key | Visual Basic 6 |
| ReSharper.key | ReSharper（Visual Studio 拡張） |
| SublimeText.key | Sublime Text |
| NotepadPlusPlus.key | Notepad++ |

## 使い方

共通設定ダイアログの「キー割り当て」ページで「インポート(I)」ボタンから該当ファイルを選択してください。
**インポートは、ファイルに記載されているキー(物理キー)単位で全モディファイア(Shift/Ctrl/Altの組み合わせ)を
まとめて上書きします。** そのキーに他の割り当てを残したい場合は、インポート前にエクスポートしてバックアップを
取ってください。

## 前提・限界

- サクラエディタはテキストエディタであり、コード補完・リファクタリング・デバッグ実行などのIDE機能は
  持っていません。そのため各キーマップの「ファイル操作・編集・検索・移動」に相当する部分だけを対象にしており、
  ビルド/実行/デバッグ/リファクタリング系のショートカットは含めていません。
- **ReSharper.key** は、ReSharperが素のVisual Studioから変更するショートカットの大半が「コード解析・
  ナビゲーション・リファクタリング」関連(Go to Everything、Alt+Enterのクイックフィックス等)で、
  サクラエディタには対応する機能がないため、基本的な内容は`VisualStudio.key`と同じです(ファイル/編集/
  検索の基本操作は拡張機能も変更しないため)。ただし「ヘッダ/ソースファイルの切り替え」(Alt+O、
  Ctrl+Shift+Gの代替キーも持つ)は[ReSharper C++が実際に実装している機能](https://www.jetbrains.com/help/resharper/Navigation_in_CPP.html)
  であることが確認できたため、サクラエディタの`F_OPEN_HfromtoC`(同名のC/C++ヘッダ(ソース)を開く)を
  Alt+O・Ctrl+Shift+Gに割り当て、`VisualStudio.key`との差分にしています。
  **VisualAssist.key は削除しました(20260823)。** 同様の趣旨(Alt+Oのみ)で追加していましたが、
  ReSharperと比べて知名度が低く、`VisualStudio.key`との差分もAlt+O 1行のみで独立したプリセットとして
  維持する価値が薄いと判断したため削除しました。
- **VisualStudio6.key** は、VC++6/VS6世代で確実に安定していたと確認できる基本操作のみに絞っています
  (Visual Studio 2005以降で追加された「すべて保存」「前后の場所へ移動」等は含めていません)。この世代・
  素のVisual Studioにはヘッダ/ソース切り替えの組み込み機能が無い(Visual Assist等の拡張機能が無いと
  使えない)と判断し、Alt+Oの割り当ては行っていません。
- 行コメントの切り替え(Ctrl+/ 等)、複数選択、行の移動(Alt+↑/↓)など、サクラエディタに対応する機能が
  存在しないショートカットは割り当てていません。
- **CSharp2005.key / VisualCpp2.key は削除しました。** 前者は`VisualStudio.key`と内容が完全に重複して
  おり独立したプリセットである意味がなかったため、後者はUIの選択肢に配線されておらず実質到達不能な上、
  対象(1990年代前半のVisual C++ 2)の当時の資料が乏しく確認精度も低かったため削除しました。
- **Grep(Ctrl+Shift+F)/Grep置換(Ctrl+Shift+H)の不具合を修正しました(20260823)。** VSCode.key/
  VisualStudio.key/ReSharper.keyのCtrl+Shift+Fは、検索ダイアログを開く`F_SEARCH_DIALOG`
  や置換ダイアログを開く`F_REPLACE_DIALOG`と対になるべきところ、誤って「ダイアログを開かず直前の設定で
  即実行する」`F_GREP`(ダイアログ版は`F_GREP_DIALOG`)を割り当てていました。Ctrl+Shift+Hに至っては
  何も割り当てられていませんでした。実際のVSCode/Visual Studioは共にCtrl+Shift+F=Find in Files、
  Ctrl+Shift+H=Replace in Filesでダイアログ/パネルを開く動作のため、`F_GREP_DIALOG`(Ctrl+Shift+F)・
  `F_GREP_REPLACE_DLG`(Ctrl+Shift+H)に修正しました。
- **VSCode.key に、公式のデフォルトキーバインド一覧(https://code.visualstudio.com/docs/reference/default-keybindings )
  と突き合わせて見つかった不足分を追加しました(20260823)。**
  - Ctrl+Shift+S(Save As...) → `F_FILESAVEAS_DIALOG`。既定値にはあった割り当てが、Sキー行がプリセットに
    含まれているせいで無言で消えていた(Ctrl+Shift+Sの不具合と同種)
  - Ctrl+Shift+O(Go to Symbol) → `F_OUTLINE`(アウトライン解析)
  - Ctrl+P(Quick Open) → `F_COMMAND_PALETTE`(最近使用したファイルも一覧に出るため代用可)
  - Ctrl+L(Select current line) → `F_SELECTLINE`
  - Ctrl+Shift+\(Jump to matching bracket) → `F_BRACKETPAIR`
  - Alt+Z(Toggle Word Wrap) → `F_WRAPWINDOWWIDTH`(折り返しモードを巡回するトグル)
  - 一方、Ctrl+D(複数選択)・Ctrl+/(行コメント切替)・Alt+↑↓(行の移動)・Ctrl+T(ワークスペース全体の
    シンボル検索)・Ctrl+Shift+M(診断一覧)など対応機能が無いものは引き続き未割り当て。また`Ctrl+K Ctrl+S`
    のような2打鍵チェイン系ショートカット(Save All等)は、サクラのキー割り当てが「1物理キー+モディファイア」
    までしか表現できない構造のため、原理的に再現できません
  - [日本語の紹介記事](https://qiita.com/12345/items/64f4372fbca041e949d0)とも突き合わせ、
    Ctrl+,(Open Settings) → `F_OPTION`(共通設定を開く)を追加しました(20260823)。カンマキーは
    元々全モディファイアが未割り当てだったため既存設定との衝突はありません。同記事の他の項目
    (編集/カーソル移動/選択/検索置換/ビュー表示/マルチカーソル/ファイル操作/エディタレイアウト/
    折りたたみ等)は、既にVSCode.keyに反映済みか、サクラに対応機能が無いか、2打鍵チェイン系で
    再現不可能なもののいずれかでした
- **SublimeText.key / NotepadPlusPlus.key を追加しました(20260823)。** IDEではなくテキストエディタを
  参考にした方が、デバッグ・リファクタリング等の対応不可能な項目をふるい落とす必要がなく機能が素直に
  対応するという判断から追加。
  - **SublimeText.key**: 実際に配布されている`Default (Windows).sublime-keymap`(JSON、
    [参照元](https://github.com/bradrobertson/sublime-packages/blob/master/Default/Default%20(Windows).sublime-keymap))
    の生データを直接突き合わせたため確認精度は高いです。Ctrl+L(SelectLine)、Ctrl+R(goto_symbol→Outline)、
    Ctrl+M(move_to brackets→BracketPair)、Ctrl+Shift+D(DuplicateLine)、Ctrl+Shift+K(DeleteLine)等。
    追加で[日本語の紹介記事](https://qiita.com/seafield1979/items/56d4833dae818dcf85ae)とも
    突き合わせ、Ctrl+P(Goto Anything→`F_COMMAND_PALETTE`)とCtrl+J(Join Lines→`F_MERGE`)を
    追加しました(20260823)。同記事のCtrl+;/Ctrl+-によるフォントサイズ変更は、他の一次資料と
    食い違い確証が持てなかったため採用していません(サクラの既定でもCtrl+マウスホイールで
    同等の操作ができるため実害は小さいと判断)。
  - **NotepadPlusPlus.key**: Notepad++はデフォルトキー割り当てが実行ファイルに埋め込み式で、Sublimeのような
    生の設定ファイルが無いため、コミュニティのショートカット一覧投稿([参照元](https://community.notepad-plus-plus.org/topic/12576/list-of-all-assigned-keyboard-shortcuts))と、
    Ctrl+L/Ctrl+Shift+Lの実際の内部コマンド名(SCILINECUT/SCILINEDELETE)が確認できた
    [別スレッド](https://community.notepad-plus-plus.org/topic/13077/keyboard-shortcut-to-delete-current-line)
    の2件で裏取りしています。SublimeText.keyより確認精度はやや低いため、収録項目は特に確度の高いものに
    絞りました(Find in Files相当のCtrl+Shift+F等、確証を得られなかったものは割り当てていません)。
- **VisualStudio.key / ReSharper.key に、公式のショートカット一覧
  ([Microsoft Learn](https://learn.microsoft.com/ja-jp/visualstudio/ide/default-keyboard-shortcuts-in-visual-studio?view=visualstudio))
  と突き合わせて見つかった不足分を追加しました(20260823)。両ファイルとも基本部分は同一のため
  同じ内容を反映しています。
  - Ctrl+W(Window.CloseDocumentWindow、Ctrl+F4の代替) → `F_FILECLOSE`。Wキーの既定値
    (Ctrl+W=単語選択)を上書きしますが、Ctrl+Alt+W(折り返し一時設定)は温存しています
  - Ctrl+L(Edit.LineCut) → `F_CUT_LINE`、Ctrl+Shift+L(Edit.LineDelete) → `F_DELETE_LINE`。
    Lキーの既定値(Ctrl+L=キーマクロ読み込み、Ctrl+Shift+L=キーマクロ実行)を上書きしますが、
    Alt+L(先頭空白削除)・Ctrl+Alt+L(小文字化)・Ctrl+Alt+Shift+L(大文字化)は温存しています
  - Ctrl+U(MakeLowercase)/Ctrl+Shift+U(MakeUppercase)は見送りました。Uキーの既定値
    (行の先頭までの切り取り/削除)がサクラ側で他に代替キーを持たない一意の機能のため、
    重複する大小文字変換(既にCtrl+Alt+L/Ctrl+Alt+Shift+Lにある)のために上書きする
    価値がないと判断しました
  - Ctrl+I(IncrementalSearch)・Ctrl+Shift+T(WordTranspose)・Ctrl+↑/↓(ScrollLine)等は
    対応する機能がサクラに無いため見送り。折りたたみ・複数キャレット・コード解析系
    (Ctrl+M系、Ctrl+K系のチェイン)は引き続き対象外
- **「カーソル行を上/下に移動」(`F_MOVE_LINE_UP`/`F_MOVE_LINE_DOWN`)をネイティブ化したのに伴い(20260823)、
  対応する各プリセットに割り当てを追加しました(20260824)。** サクラの既定値自体はUp/Downキーの
  Shift+Altスロットに割り当て済み(20260823のコミットで設定)ですが、各プリセットは対象エディタの
  実際のショートカットに合わせて別スロットへ明示的に割り当てています。
  - **VSCode.key**: Alt+↑/↓(`editor.action.moveLinesUpAction`/`DownAction`)。
    [公式デフォルトキーバインド一覧](https://code.visualstudio.com/docs/reference/default-keybindings)
    で確認。`.key`ファイルの各スロットは値0が「サクラの既定にフォールバック」を意味する
    (`CKeyBind::GetFuncCodeAt`、明示的な無効化ではない)ため、Up/DownどちらのAltスロットも、
    このプリセットを適用するとサクラの既定でAlt+↑/↓に入っている「(矩形選択)カーソル上下移動」
    (`F_UP_BOX`/`F_DOWN_BOX`)が上書きされます。矩形選択のカーソル移動はCtrl+Alt+↑/↓
    (`F_UP2_BOX`/`F_DOWN2_BOX`、２行ごと)にも別途割り当てられており、このプリセットでは
    上書きしていないため、矩形選択自体はキーボードだけでも引き続き操作できます。
  - **SublimeText.key / NotepadPlusPlus.key**: Ctrl+Shift+↑/↓(`swap_line_up`/`swap_line_down`)。
    SublimeTextは前述の[Default (Windows).sublime-keymap](https://github.com/bradrobertson/sublime-packages/blob/master/Default/Default%20(Windows).sublime-keymap)
    で確認。Notepad++は「Move Up/Down Current Line」(Edit > Line Operations)がCtrl+Shift+↑/↓に
    既定で割り当てられていることを、[コミュニティフォーラムのスレッド](https://community.notepad-plus-plus.org/topic/17831/alt-up-down-arrow-line-movement)
    で確認(この機能はv7.5.9で追加されたもので、以前に参照した2015年頃のショートカット一覧記事には
    載っていませんでした)。両プリセットとも、サクラの既定でCtrl+Shift+↑/↓に入っている
    「(範囲選択)カーソル上下移動(２行ごと)」(`F_UP2_SEL`/`F_DOWN2_SEL`)がこのプリセットの範囲では
    上書きされます。この機能はほかに代替キーを持たない一意の機能ですが、使用頻度の低いマイナーな
    移動系機能のため、対象エディタに合わせたMoveLine割り当てを優先しました。
  - **VisualStudio.key / ReSharper.key**: Alt+↑/↓(`Edit.MoveSelectedLinesUp`/`Down`、VS2013から)。
    [Microsoft Learnのデフォルトキーボードショートカット一覧](https://learn.microsoft.com/ja-jp/visualstudio/ide/default-keyboard-shortcuts-in-visual-studio?view=visualstudio)
    で確認。VSCode.keyと同様、サクラの既定の「(矩形選択)カーソル上下移動」(Alt+↑/↓)を上書きしますが、
    Ctrl+Alt+↑/↓側は残るため矩形選択自体は引き続きキーボードで操作できます。
  - **VisualStudio6.key / VisualBasic6.key には追加していません。** この機能はVisual Studio 2013で
    追加されたもので、VC++6/VS6・VB6(1990年代)の時点では存在しないため、既存の「post-2005機能は
    含めない」方針(前述)と整合的に対象外としました。
- **TeraPad.key は追加していません。** Web検索で確認した限り、断片的なブログ記事はあるものの
  ショートカット一覧を網羅した公式・準公式資料が見つからず、`VisualCpp2.key`と同様に確認精度が
  低くなる懸念が強いため保留しています。付属の`Keys.txt`(TeraPad本体に同梱)等、一次資料が
  手元にあれば作成できます。

## ファイル形式について

サクラエディタの `.key` ファイル(`KEYBIND_VERSION=SakuraKeyBind_Ver4`)形式で手書きしています。
文法上の詳細は`sakura_core/typeprop/CImpExpManager.cpp`(`CImpExpKeybind::Import`/`Export`)と
`sakura_core/env/CShareData_IO.cpp`(`IO_KeyBind`)を参照してソースコードから起こしたものです。

**`KEYBIND_COUNT`が100未満の「差分だけ」のファイル(このディレクトリの全プリセットが該当)は、
20260823まで`sakura_core/env/CShareData_IO.cpp`の`IO_KeyBind()`側の不具合で正しく動いて
いませんでした。** `Import()`は成功(true)を返すのに、既定テーブル上の位置が`KEYBIND_COUNT`
件目以降にあるキー(G/H/S/R等ほとんど)は一切上書きされず、たまたま位置が先頭寄りだったキー
(例: F4)だけ反映される、という気付きにくい壊れ方をしていました。原因は`IO_KeyBind()`終盤の
「`sKeyBind.m_nKeyNameArrNum = nKeyNameArrUsed;`」で、`nKeyNameArrUsed`が関数先頭
(既定値へのフォースより前、まだファイル自身の`KEYBIND_COUNT`が入っている状態)でキャプチャ
されており、既存キーの上書きではインクリメントされないため、100件未満の`.key`ファイルを
読み込むと必ず巻き戻っていたのが原因です。`IO_KeyBind()`側で修正済みのため、以後はこの
ディレクトリのような差分形式のファイルも正しく動作します(`KEYBIND_COUNT=100`のフル
エクスポート形式である`Default.key`はこの不具合の影響を受けていませんでした)。
文字コード・改行コードはCRLF+UTF-8(BOM有無はどちらでも動作します)で統一しています。

各`KeyBind[NNN]=`行の末尾(キー名の後ろ)にはTAB区切りで、8モディファイア分の割り当て機能名を
参考情報として追記しています(未割り当てのスロット=`_`)。実際のキー割り当て(機能コード8個)には
一切影響しません。この注釈はNKMM_FIX_KEYBIND_EXPORT_FUNCNAME機能でインポート時に読み飛ばされ、
元のキー名に復元されます。同機能が無効なビルドでインポートすると、キー名欄にこの注釈がそのまま
連結されて表示されるのでご注意ください(キー割り当ての動作自体は変わりません)。
