# NKMM_CODE_FOLDING 実装レポート

対象フラグ: `NKMM_CODE_FOLDING`(新規)

## 背景

関数/構造体単位でエディタ本文を折りたたみ、ヘッダ行だけのアウトライン表示に
できるようにしてほしいという要望。VS Codeの「Fold All」相当の、文書全体を
まとめて折りたたむ/展開するトグル動作(Ctrl+Shift+[ / Ctrl+Shift+])として
実装した。個別の行単位での折りたたみ(カーソル行だけを開閉する方式)は
実装途中で「全体をfoldしてアウトライン一覧になるようにする」という
方向転換の指示を受け、最終的には不採用。

## 追加したファイル

- `sakura_core/docplus/CFoldManager.h` / `.cpp` — 行ごとの折りたたみ状態
  (`CLineFolded`: 開始行か/折りたたみ中か/非表示か/終了行/名前部分の桁位置)。
  既存の`CFuncListManager`等と同じdocplus流儀。
- `sakura_core/outline/CFoldRangeCalculator.h` / `.cpp` — アウトライン解析
  結果(`CFuncInfoArr`)から折りたたみ範囲(開始行〜終了行)を算出する。
  `CFuncInfo`には開始行しか記録されていないため、深さ(`m_nDepth`)ベースの
  スタック走査で「次に現れる同じか浅い深さの項目の直前」を終了行とみなす。
  この汎用アルゴリズムのおかげで、C/C++に限らず深さがフラット(常に0)な
  プレーンテキストのトピック解析でも「次の見出しの直前まで」が自然に
  折りたたみ範囲になる。
- `changelog/NKMM_CODE_FOLDING.md`(このファイル)

## 修正した既存ファイル

- **`sakura_core/my_config.h`** — `NKMM_CODE_FOLDING`フラグを追加。
- **`sakura_core/doc/logic/CDocLine.h`** — `MarkType`に`CLineFolded m_cFolded`
  を追加(フラグガード付き)。
- **`sakura_core/doc/CDocOutline.h` / `.cpp`** — `UpdateFoldRanges()`を新規追加。
  文書の既定アウトライン種別(`STypeConfig::m_eDefaultOutline`)に応じて
  `MakeFuncList_C`/`MakeFuncList_Java`/`MakeFuncList_Perl`/
  `MakeFuncList_VisualBasic`/`MakeFuncList_python`/`MakeFuncList_Erlang`/
  `MakeFuncList_PLSQL`/`MakeTopicList_cobol`/`MakeTopicList_asm`/
  `MakeTopicList_wztxt`/`MakeTopicList_html`(HTML/XML)/`MakeTopicList_tex`/
  `MakeTopicList_txt`(プレーンテキスト、デフォルトフォールバックも兼ねる)を
  呼び分け、`CFoldRangeCalculator`で範囲を算出して各ヘッダ行に
  `CFoldManager`でマークする。ブックマーク一覧/ファイルツリー/ルール
  ファイル種別(`OUTLINE_BOOKMARK`/`OUTLINE_FILETREE`/`OUTLINE_FILE`)は
  コード構造を表すものではないため対象外。
  併せて、関数/メソッド名部分だけをハイライトできるよう、`CFuncInfo::
  m_cmemFuncName`(`"Namespace::ClassName::funcName"`形式、`"::"`区切り)の
  最後の区切り以降(短い名前)を、ヘッダ行の実テキストから単純な部分文字列
  検索(正規表現不使用)で探し、見つかった桁位置/文字数を記録する。
- **`sakura_core/doc/CEditDoc.h` / `.cpp`** —
  `m_bFoldRangesReady`(初回のみ`UpdateFoldRanges()`を実行するガード)、
  `m_bOutlineFolded`(文書全体がアウトライン表示中かどうか)、
  `m_ptPreFoldCaretLogic`(アウトライン表示に入る直前のキャレット位置、
  Escで戻る用)を追加。`IsModificationForbidden()`に、アウトライン表示中は
  通常の「読み取り専用時の編集禁止コマンド一覧」(`EIsModificationForbidden`)
  を流用して編集操作を禁止するガードを追加(バイナリサーチ部分は
  `_IsInModificationForbiddenList()`として関数化し、通常の読み取り専用判定と
  共用)。
- **`sakura_core/doc/layout/CLayoutMgr.h` / `CLayoutMgr_DoLayout.cpp`** —
  `ToggleFold(CLogicInt)`(指定行単体のトグル。最終的にコマンドからは
  未使用だが、将来のガターUI向けに残置)と`ToggleFoldAll(void)`
  (文書全体のトグル本体)を追加。
- **`sakura_core/doc/layout/CLayoutMgr.cpp`** — `LogicToLayout()`に、
  対象ロジック行に対応する`CLayout`が見つからない場合のフォールバックを
  2箇所追加(下記「見つけて修正した不具合」参照)。
- **`sakura_core/cmd/CViewCommander.h` / `.cpp`** — `Command_FOLD_TOGGLE()`/
  `Command_FOLD_CANCEL()`を宣言。`HandleCommand()`に、アウトライン表示中の
  Enter(`F_WCHAR`の`'\r'`/`'\n'`)・Tab(`F_INDENT_TAB`)を「ジャンプ」
  (`Command_FOLD_TOGGLE()`)、Esc(`F_CANCEL_MODE`)を「戻る」
  (`Command_FOLD_CANCEL()`)として横取りする分岐を追加。
- **`sakura_core/cmd/CViewCommander_Outline.cpp`** —
  `Command_FOLD_TOGGLE()`(文書全体のトグル+キャレット位置の調整+
  画面中央への移動)、`Command_FOLD_CANCEL()`(Esc用、折りたたみ前の
  キャレット位置へ戻る)、`CSuppressScrBarMarkerForFoldToggle`
  (RAIIガード、下記参照)を実装。
- **`sakura_core/view/CEditView.cpp`** — アウトライン表示中のダブルクリックを
  「ジャンプ」として扱う分岐を追加。`ScrBarMarker::BuildWorkCallback()`の
  キャッシュ配列サイズ算出を修正(下記参照)。
- **`sakura_core/view/colors/CColorStrategy.h` / `.cpp`**、
  **`sakura_core/view/colors/CColor_Found.h` / `.cpp`** —
  アウトライン表示中、ヘッダ行の関数/メソッド名部分を検索マーク
  (`COLORIDX_SEARCH`)と同じ色で表示する`CColor_OutlineHeader`を新規追加。
  既存の`CColor_Select`/`CColor_Found`と同じ「`BeginColorEx`で行頭の
  `CLayout`を参照して判定する」パターンを踏襲し、優先順位は
  選択範囲 > アウトラインヘッダ > 検索マーク > 通常の色分け。
- **`sakura_core/func/CKeyBind.cpp`** — `Ctrl+Shift+[` / `Ctrl+Shift+]`
  (どちらも`F_FOLD_TOGGLE`、既存の`Ctrl+[` / `Ctrl+]`=`F_BRACKETPAIR`の
  Shift+Ctrl列は空いていたため衝突なし)を追加。
- **`sakura_core/Funccode_x.hsrc`** — `F_FOLD_TOGGLE = 30991`を追加
  (`F_FILETREE`直後の空き番号帯)。このファイルはCP932(Shift-JIS)、
  BOM無しのプレーンテキストで、通常のUTF-8前提のツールで直接編集すると
  文字化けするため、Pythonの`bytes`相当の手順(PowerShellの
  `System.Text.Encoding.GetEncoding(932)`)でバイト単位に安全に追記した。
- **`sakura/sakura.vcxproj` / `.vcxproj.filters`** — 新規4ファイル
  (`CFoldManager.h/.cpp`, `CFoldRangeCalculator.h/.cpp`)を追加(ユーザー
  承認の上でこちらから追加。通常このプロジェクトファイルはユーザーが
  手動管理しているため、無断では編集しない方針)。
- **`docs/sakura_keybind_list.html`** — `F_FOLD_TOGGLE`の行と
  `FUNC_JP`辞書エントリを追加(既存の`F_SHOWUNDOHISTORYPANEL`追加時と
  同じ手順)。

## 実装の詳細

### フェーズ構成

1. **Phase1**: データモデル(`CFoldManager`)+アウトライン解析結果からの
   範囲算出ロジック(`CFoldRangeCalculator`)のみ。表示への影響なし。
2. **Phase2**: `CLayoutMgr`での実際の非表示化(`ToggleFold`/`ToggleFoldAll`)。
3. **Phase3**: `Ctrl+Shift+[` / `Ctrl+Shift+]`のキーバインド化。
4. ユーザー指摘を受けての方向転換・追加要望への対応
   (個別行トグル→全体トグル、ヘッダ行以外も非表示化、編集禁止、
   Esc/Tab操作、ジャンプ時の中央表示、色分け修正、名前部分だけの
   ハイライト、他タイプへの対応拡大)。

### 非表示化の方式(なぜ`_DoLayout()`を直接呼ぶのか)

`CLayoutMgr::ToggleFoldAll()`は、`DoLayout_Range()`による部分再構築
(色分け継続状態の引き継ぎ等が絡み複雑)ではなく、単純さと確実性を優先して
`_DoLayout()`(全体再レイアウト)を呼ぶ方式にしている。折りたたみのON/OFFは
高頻度操作ではないため、O(n)のコストは許容している。

### 非表示化とコメント色分けの両立

`_DoLayout()`自体は折りたたみを一切意識せず、常に全行分のレイアウト/色分けを
通常通り生成する。これは複数行コメント等の色分け継続状態
(`colorPrev`/`exInfoPrev`)が、隠すべき行のテキストも実際に色分けエンジンへ
渡すことで初めて正しく引き継がれるため(最初は非表示行をレイアウト段階で
丸ごとスキップしていたが、それだと隠された行の中身(複数行コメントの開始等)が
色分けエンジンに一切渡されず、以降の色分けがずれるバグがあった)。
代わりに、通常通り全行分生成した「後」で、`ToggleFoldAll()`が非表示にすべき
行の`CLayout`ノードだけを連結リストから個別に取り除く(下記の
`_TextWidthMultisetErase`忘れの不具合はこの取り除き処理で発生した)。

### アウトライン表示中の操作

- **編集禁止**: `CEditDoc::IsModificationForbidden()`が、通常の読み取り
  専用時と同じ「編集禁止コマンド一覧」(`EIsModificationForbidden`)を
  アウトライン表示中にも流用する。カーソル移動や折りたたみトグル自体は
  この一覧に含まれないため引き続き操作できる。
- **キャレットはスコープのヘッダ行へ**: 折りたたみ時、キャレットが
  非表示になった行にいた場合、直近の手前の表示行(=元いたスコープの
  ヘッダ行)まで遡る。手前に表示行が無い(文書先頭付近)場合のみ、直後の
  表示行まで進める。
- **Enter/Tab = ジャンプ、Esc = 戻る**: Enter/Tabはアウトライン表示中の
  現在のキャレット位置(=選択中のヘッダ行)を維持したまま全展開する
  (`Command_FOLD_TOGGLE()`を流用)。Escは、アウトライン表示に入る直前の
  キャレット位置(`m_ptPreFoldCaretLogic`)へ戻す(`Command_FOLD_CANCEL()`、
  アウトライン内で別のヘッダ行に移動していても、常に元の位置へ戻る)。
  ダブルクリックもEnter/Tabと同じ「ジャンプ」として扱う。
- **ジャンプ時は画面中央へ**: 既存の`NKMM_FIX_CENTERING_CURSOR_JUMP`機構
  (`GetDllShareData().m_sFlags.m_nCenteringCursor++`をキャレット移動の
  直前でインクリメントする、タグジャンプ等と同じ手法)を流用。

### ヘッダ行のハイライト(検索マーク色、名前部分のみ)

折りたたみ中、ヘッダ行のうち関数/メソッド名の部分だけを検索マーク色
(`COLORIDX_SEARCH`)でハイライトする。既存の色分けアーキテクチャ
(`CColorStrategyPool`、`CColor_Select`/`CColor_Found`と同じ
`BeginColorEx`パターン)にそのまま乗せている。名前の桁位置は正規表現ではなく、
アウトライン解析が既に持っている名前情報(`CFuncInfo::m_cmemFuncName`)を
ヘッダ行の実テキスト内で単純な部分文字列検索するだけで求めている
(`CDocOutline::UpdateFoldRanges()`)。名前が見つからなかった場合は行全体を
ハイライトするフォールバックになる。

### 対応するアウトライン種別

当初はC/C++限定のMVPとして実装したが、ユーザー指摘によりC/C++以外の
組み込みアウトライン解析方式(Java/Perl/Python/VB/PL-SQL/COBOL/アセンブラ/
階層付きテキスト/HTML/XML/TeX/プレーンテキスト)全般に対応を拡大した。
`CFoldRangeCalculator`のアルゴリズムが深さベースの汎用実装だったため、
ディスパッチの追加のみで対応できた。ブックマーク一覧・ファイルツリー・
ルールファイル種別・プラグイン提供の種別は対象外(プラグイン種別は
プレーンテキスト解析にフォールバック)。

プレーンテキストの折りたたみは、サクラエディタの既存のテキスト解析仕様
(`MakeTopicList_txt`)により、任意の見出し行ではなく「見出し記号」
(■○●等、共通設定の`m_szMidashiKigou`)で始まる行だけがトピックとして
認識される点に注意(今回新しく入れた制約ではない)。

## 見つけて修正した不具合

機能追加そのものとは別に、実装・検証の過程で以下の実際のバグを発見し
修正した。特に後半3件はクラッシュダンプ(`CrashDumps\*.dmp`)を解析して
根本原因を特定している。

1. **アウトライン終了行のオフバイワン** — `CFoldRangeCalculator`が
   `CFuncInfo::m_nFuncLineCRLF`(1オリジン)を0オリジンとして扱っていたため、
   関数ヘッダ行自身ではなく`{`の行でしか折りたたみが起動しなかった。
2. **キャレット位置の取得順序** — `Command_FOLD_TOGGLE()`が
   `_DoLayout()`を伴う`ToggleFoldAll()`の「後」にキャレットの論理位置を
   取得していたため、総レイアウト行数が激減した直後のレイアウト位置基準で
   再計算され、常にEOF行に丸められてしまっていた。
3. **`LogicToLayout()`の範囲外フォールバック** — `SearchLineByLayoutY()`が
   `NULL`を返した場合、即座に範囲外のY座標を設定して返していたため、
   呼び出し元は常に最終行(EOF)扱いになっていた。有効な最終レイアウト行に
   クランプして再検索するよう修正。
4. **`LogicToLayout()`の後方検索ループのNULL未チェック** —
   `while(pLayout->GetLogicLineNo() > ptLogic.GetY2())`のループに
   `pLayout`のNULLチェックが無く、対象行より手前が全て非表示の場合に
   `GetPrevLayout()`がリスト先頭で`NULL`を返した直後、次のループ条件判定で
   NULL参照する経路があった(F2/F3で非表示行にジャンプすると発生しうる)。
   NULLガードを追加し、手前に表示行が無ければ文書先頭の表示行へ
   フォールバックするよう修正。
5. **スクロールバーマーカーのバックグラウンドスレッドとの競合
   (ヒープ破壊、COMCTL32内で不可解にクラッシュ)** — `ToggleFoldAll()`内の
   `_DoLayout()`実行中、スクロールバーマーカーのバックグラウンド構築
   スレッド(`ScrBarMarker::BuildWorkCallback`、折り返しあり文書で有効)が
   並行して同じ`CLayout`連結リストを参照し、解放済みメモリへのアクセスで
   ヒープを破壊していた。`Command_REPLACE_ALL`で過去に一度発見・対策済みの
   同種の競合パターン(`CSuppressSrchKeyMarkForReplaceAll`)を参考に、
   `CSuppressScrBarMarkerForFoldToggle`というRAIIガードを追加し、
   `ToggleFoldAll()`実行中は全ビューの描画を一時停止しバックグラウンド
   スレッドを同期的に止めるようにした。
6. **`_TextWidthMultisetErase`呼び忘れによるヒープ破壊** —
   `ToggleFoldAll()`が非表示行の`CLayout`ノードを連結リストから取り除いて
   `delete`する際、`m_multisetTextWidth`(テキスト最大幅キャッシュ)からの
   登録解除を呼び忘れていた。`CLayoutMgr.h`のコメントで明記されている
   「CLayout削除時は必ずこのフックを通すこと」という契約に違反しており、
   解放済みポインタがキャッシュに残り続け、後で無関係なメモリ確保
   (mimalloc)がその領域に触れた時に不定動作を起こしていた。`delete`前に
   `_TextWidthMultisetErase()`を呼び、削除後に`m_vTextWidthTopK`
   キャッシュ(行番号ベースのため全体シフトで無効)もクリアするよう修正。
7. **スクロールバーマーカーのキャッシュ配列サイズの取り違え
   (ヒープバッファオーバーフロー)** — `ScrBarMarker::BuildWorkCallback()`が
   キャッシュ配列のサイズを`CLayoutMgr::GetLineCount()`(レイアウト行数=
   折りたたみで激減)で確保していたが、実際の書き込みは
   `CDocLineMgr`(論理行数=常に全行分)基準のインデックスで行っていたため、
   アウトライン表示中に検索する(F3や検索ダイアログでの検索実行)たびに
   配列の確保領域を超えてヒープに書き込んでいた。書き込んだ瞬間には
   症状が出ず、後で無関係なメモリ確保がその領域に触れた時に初めて
   クラッシュする性質のバグで、F2/F3のクラッシュ(#4)やCtrl+F後の検索
   実行時のクラッシュなど、一見バラバラに見えた複数の不具合報告の
   共通原因になっていた。キャッシュ配列のサイズを論理行数
   (`CDocLineMgr::GetLineCount()`)基準に統一して修正。
8. **名前部分ハイライトの区切り文字の取り違え** —
   `CFuncInfo::m_cmemFuncName`の区切り文字を(誤って)バックスラッシュだと
   思い込み`find_last_of(L"\\/")`で分割していたが、実際は`CType_Cpp.cpp`の
   `szNamespace`が生成する通り`"::"`(コロン2文字)区切りだった。
   `rfind(L"::")`に修正。

## スコープ外にしたもの・既知の制限

- マージン(行番号欄)への+/-アイコン表示・マウスクリックでの折りたたみは
  未実装(前例が全く無いUIのため、キーバインドのみで操作する方式に
  絞った)。
- 折りたたみ範囲の解析(`UpdateFoldRanges`)は文書を開いてから最初の1回だけ
  行う。以降の編集でアウトライン構造が変化しても自動では再解析しない。
- 個別行単位の折りたたみ(`ToggleFold`関数は実装済みだが、コマンドからは
  未使用)。
- ブックマーク一覧・ファイルツリー・ルールファイル種別・プラグイン提供の
  アウトライン種別は折りたたみ非対応。
- マクロ(`.qjs`/`.vbs`/`.pas`)から呼び出せるコマンドテーブルへの登録は
  行っていない。

## 動作確認について

`msbuild sakura.sln /t:sakura /p:Configuration=Release /p:Platform=x64`で
フルビルド(リンクまで)が0エラー・0警告で通ることを都度確認しながら実装した。

実機(`-PROF=`によるテスト専用プロファイル)での`WM_COMMAND`直接送信
(`SendMessage`/`PostMessage`)によるGUI自動操作で、以下を確認済み:

- 折りたたみ/展開のトグルが安定して繰り返し動作すること(複数回の
  fold/unfoldサイクルで内容が完全に元通りに復元されること)
- 折りたたみ中は編集操作(文字入力・削除・貼り付け等)が無視されること
- 折りたたみ時にキャレットが元いたスコープのヘッダ行へ移動すること
- Enter/Tab/ダブルクリックでのジャンプ、Escでの復帰(別の場所へ移動して
  いても元の位置へ戻ること)
- ジャンプ先が画面中央付近に表示されること(数千行規模のファイルで確認)
- ヘッダ行の関数/メソッド名部分だけが検索マーク色でハイライトされ、
  戻り値の型・クラス修飾子・引数リストはハイライトされないこと
- 複数行コメントをまたぐ折りたたみ/展開後も、コメント以降の色分けが
  ずれないこと
- C/C++に加えPython(ネストしたメソッドを含む)でも折りたたみが正しく
  動作すること、既存のC/C++での動作に回帰が無いこと
- F2(次のブックマーク)/F3(次を検索)/Ctrl+F(検索ダイアログ→実際に検索)を
  折りたたみ中に実行してもクラッシュしないこと(前述の不具合4・7の修正後)

Undo/Redoとの相互作用(折りたたみ状態自体はUndo/Redoの対象外)、複数ビュー
(分割ウィンドウ)での同時表示の細部、非常に大規模なファイル
(数十万行規模)でのアウトライン解析パフォーマンスについては、今回の
セッションでは重点的な確認を行っていない。
