# NKMM_MULTI_CURSOR 実装レポート

対象フラグ: `NKMM_MULTI_CURSOR`(新規、サブフラグ `NKMM_MULTICURSOR_MAX`)

## 背景

VS Code / Sublime Text 風の複数カーソル編集(同時にタイピング・削除・選択拡張が
できる機能)を桜エディタに追加した。Ctrl+Alt+↑/↓でのカーソル追加を起点に、
Alt+クリック、選択範囲の各行末への一括追加、カーソル同士の統合、Undo/Redoでの
復元まで、既存の単一カーソル実装(`CCaret`/`CViewSelect`)を無改変のまま流用する
方針で段階的に実装した。実装は2026-08-30〜08-31の2日間で計20回の設計・修正パス
を踏み、その後2026-09-03に`F_ADD_CURSORS_TO_LINE_ENDS`、2026-09-04にAlt+クリック
が追加された。設計判断の詳細と全パスの変更履歴は`docs/multicursor_design.html`
にまとめてあり、本レポートはその要約とファイル変更点の記録を兼ねる。

## 追加したファイル

- `changelog/NKMM_MULTI_CURSOR.md`(このファイル)
- `docs/multicursor_design.html` — データモデル・各メカニズム・既知の制約・
  全20パスの変更履歴を記録した設計リファレンス(実装と同時期に作成・更新)。

## 修正した既存ファイル

- **`sakura_core/my_config.h`** — `NKMM_MULTI_CURSOR`フラグ本体と
  `NKMM_MULTICURSOR_MAX(=10000)`(VS Codeの`editor.multiCursorLimit`既定に
  倣った追加カーソル上限)を追加。同じ場所に、密接に関わる別フラグ
  `NKMM_UNDO_RESTORE_SELECTION`(選択削除のUndoで選択状態も復元する機能。
  詳細は別レポート`project_undo_restore_selection_feature`側)も追加している。
- **`sakura_core/Funccode_x.hsrc`** — `F_ADD_CURSOR_UP`/`F_ADD_CURSOR_DOWN`/
  `F_MULTICURSOR_UNDO`/`F_ADD_CURSORS_TO_LINE_ENDS`の4コマンドを追加。
  `NKMM_FIX_MOVE_LINE`と同様、このファイルはプレーンテキストで
  `#ifdef`を解釈しないため無条件追加。
- **`sakura_core/cmd/CViewCommander.h`/`.cpp`** — 上記4コマンドの
  `HandleCommand`分岐、および移動系コマンドをマルチカーソル対応させる
  `DispatchMoveMultiCursor(bSelect, fnMoveOnce)`の宣言・配線。
- **`sakura_core/cmd/CViewCommander_Edit.cpp`** — 実装の中核。
  `ApplyToAllCursors`(全カーソルへの一括再生ループ)、
  `MergeOverlappingCursorsIfNeeded`(重なったカーソルの統合)、
  `AddCursorInDirection`(`Command_AddCursorUp`/`Down`の共通実装、
  第20パスで上下対称コードを統合)、`Command_MULTICURSOR_UNDO`、
  `RestoreMultiCursorAfterUndoRedo`(Undo/Redo後のカーソル復元、
  同じく第20パスで`Command_UNDO`/`Command_REDO`個別実装から1関数に統合)、
  `Command_AddCursorsToLineEnds`(2026-09-03追加)を実装。
- **`sakura_core/cmd/CViewCommander_ModeChange.cpp`/`CViewCommander_Search.cpp`/
  `CViewCommander_Select.cpp`** — 矩形選択開始時・検索/置換ダイアログ表示時
  (最終的に`OnKillFocus`一括処理へ統合、個別クリアは第20パスで撤去)・
  選択開始コマンドでのマルチカーソル解除を配線。
- **`sakura_core/view/CEditView.h`/`.cpp`** — 追加カーソル1個分の状態を持つ
  `SExtraCursor`構造体とその配列`m_vExtraCursors`、位置解決関数群
  (`ResolveExtraCursorFromBase`/`ResolveExtraCursor`/`ResolveExtraCursorAnchor`/
  `ResolveExtraCursorSelectAreaLine`)、`OnKillFocus`でのマルチカーソル解除、
  IME通知時の再描画拡張。
- **`sakura_core/view/CEditView_Mouse.cpp`** — 装飾キー無しの通常クリックでの
  マルチカーソル解除、および`OnLBUTTONDOWN`/`OnLBUTTONUP`へのAlt+クリック
  (カーソル追加)のハードコード(2026-09-04追加)。
- **`sakura_core/view/CEditView_Paint.cpp`** — 追加カーソルのGDI手動描画
  (`FillRect`、OSキャレット点滅と非同期の常時表示)。2026-09-04、グリフ
  アトラス/カラーフォントのBitBlt転送より後ろへ描画順を移動する修正も
  同ファイルに含む(グリフアトラス有効時にカーソルが上書きされ不可視になる
  不具合の修正)。
- **`sakura_core/view/CEditView_Ime.cpp`** — マルチカーソル中のIME再変換無効化。
- **`sakura_core/view/CEditView_Command_New.cpp`** — `ReplaceData_CEditView`系
  関数への`bHadSelection`引数追加(`NKMM_UNDO_RESTORE_SELECTION`向け、
  マルチカーソルのUndo復元とも合流する)。
- **`sakura_core/view/colors/CColor_Found.h`/`.cpp`** — `CColor_Select`に
  extra用の選択ハイライトキャッシュ(`m_vExtraSelectCache`)を追加し、
  既存の1行分キャッシュの仕組みを複数化。
- **`sakura_core/COpe.h`** — `COpe::nCursorSlot`(Undo/Redo時にどのカーソル
  (プライマリ/どのextra)の編集かを識別するID。-1=マルチカーソル無関係、
  0=プライマリ、1以上=extraのindex+1)と`CReplaceOpe::bHadSelection`を追加。
- **`sakura_core/env/CommonSetting.h`/`CShareData.cpp`/`CShareData_IO.cpp`** —
  ini専用設定`bMultiCursorMergeOverlapping`(既定true)・
  `bUndoRestoreSelection`(既定true)の定義・既定値・入出力。両方とも
  ダイアログ項目は持たない(`m_bAutoColumnPaste`と同じ運用パターン)。
- **`sakura_core/func/CKeyBind.cpp`** — 既定キー割り当てに
  Ctrl+Alt+↑/↓ = カーソル追加(上/下)、Ctrl+Shift+U = 直近カーソル追加の
  取り消し、Shift+Alt+I = 選択範囲の各行末にカーソル追加、を追加。
  Ctrl+Alt+↑/↓は元々`F_UP2_BOX`/`F_DOWN2_BOX`(矩形選択の2行ジャンプ)の
  既定キーだったため、その2コマンドは既定キー無しになった
  (コマンド自体は残存、手動再割り当ては可能)。
- **`sakura_core/func/Funccode.cpp`** — 4コマンドをコマンド一覧
  (設定画面のキー割り当てタブに表示するための分類配列)に追加。
- **`sakura_core/sakura_rc.rc`** — 4コマンドの文字列リソースを追加
  (UTF-16LE、CLAUDE.md記載の手順で編集)。
- **`resource/MainMenu.ini`** — メインメニューへの4コマンド登録
  (プルダウンメニューはこの独自CSV形式の別リソースであることが実装中に判明)。
- **`keybind_presets/Default.key`** — ビルド時に`sakura_rc.rc`へRCDATAとして
  埋め込まれる静的な「既定」プリセット。`CKeyBind.cpp`の`KeyDataInit[]`とは
  別に手動同期が必要な5番目の登録ポイントで、マルチカーソル4コマンド分が
  未反映のままだった不具合を2026-09-03に発見・修正(自動検知の仕組みは無く、
  今後デフォルトキーを変える際は要注意)。

## 実装の詳細

### データモデル — 固定オフセット方式

追加カーソル1個分の状態は`CEditView::SExtraCursor`が持つ。絶対位置を直接持つ
フィールドは無く、すべてプライマリカーソルの対応する基準点からの**相対値**
(レイアウト単位)のみを保持する。

- `nRelLine`/`nRelColumn` — プライマリの実キャレット位置からの相対行・相対桁。
  作成時に固定され、実際の編集・選択再生が走った回にのみ更新される。
- `nDesiredRelColumn` — プライマリの希望桁(`CCaret::m_nCaretPosX_Prev`)からの
  相対値。横移動・編集時のみ更新し、上下移動のクランプでは書き換わらない
  (第17パスで追加。無いと、短い行を通過する上下移動で実桁がそのまま新しい
  オフセットとして上書きされ続け、「位置がずれて戻らない」不具合になった)。
- `bHasSelection`・`nAnchorRelLine`/`nAnchorRelColumn` — そのカーソル自身の
  選択有無と、プライマリの選択アンカーからの相対値。

この方式に至るまでに、可変の「保留デルタ」方式(第6パス以前)で3カーソルの
往復移動をトレースしたところ順序が入れ替わる不具合があり、固定オフセット+
毎回その場で解決、という設計に全面変更した経緯がある。文書範囲外に解決された
カーソルは再生・描画の対象から単純に除外し、オフセット自体は変更しない。

### 位置の解決とコマンドの再生

`CEditView.cpp`の`ResolveExtraCursorFromBase`が「基準点+オフセット→行幅で
クランプ→`LineColumnToIndex`/`LineIndexToColumn`往復で文字境界へスナップ」を
共通処理し、`ResolveExtraCursor`(実位置)/`ResolveExtraCursorAnchor`(選択
アンカー)/`ResolveExtraCursorSelectAreaLine`(選択範囲の描画用)がそれぞれの
用途で呼ぶ。

移動系コマンドは`CViewCommander::DispatchMoveMultiCursor(bSelect, fnMoveOnce)`
で2経路に振り分ける。選択が絡まない素の移動は「高速パス」でプライマリを
1回動かすだけ(extraは次に参照される瞬間に自動追従)。選択が絡む場合
(Shift+矢印、または選択がある状態でのShiftなし矢印による収束)は
`ApplyToAllCursors(fnMoveOnce)`を呼び、プライマリ+全extraをドキュメント降順で
並べ、実カーソルへ1個ずつ「なりすまし」ながら`Command_WCHAR`/`Command_RIGHT`
等の既存の単一カーソル実装をそのまま再実行する。タイピング・BackSpace・
Delete・貼り付け・IME確定は常にこの一括再生ループを通る。

降順処理のため、プライマリより下の行にextraがあるとプライマリより先に処理され、
共有状態(`m_sSelectBgn`/`m_sSelect`/`m_nCaretPosX_Prev`)がextra自身の値で
上書きされる。そのため、ループ開始前に確定させた本来の値を、プライマリの番が
来るたびに明示的に復元する処理が必要になった(第14〜15パスで実際に踏んだ回帰)。

`CViewCommander::MergeOverlappingCursorsIfNeeded()`(第16パス)は
`ApplyToAllCursors`の末尾でのみ呼び、VS Codeの`CursorCollection.normalize()`
と同じ考え方で重なった/接したカーソルを1個に統合する。設定
`bMultiCursorMergeOverlapping`(既定true)でオフにできる。

### コマンドとキー割り当て

| F-code | 既定キー | 動作 |
|---|---|---|
| `F_ADD_CURSOR_UP` | Ctrl+Alt+↑ | 最上段カーソルの1行上・同じ桁に新規カーソルを追加 |
| `F_ADD_CURSOR_DOWN` | Ctrl+Alt+↓ | 最下段カーソルの1行下・同じ桁に新規カーソルを追加 |
| `F_MULTICURSOR_UNDO` | Ctrl+Shift+U | 直近に追加したカーソルを1個だけ取り消す(テキストのUndoとは別系統) |
| `F_ADD_CURSORS_TO_LINE_ENDS` | Shift+Alt+I | 選択範囲がまたがる各物理行の行末にカーソルを追加(VS Codeの`insertCursorAtEndOfEachLineSelected`相当) |

`F_ADD_CURSORS_TO_LINE_ENDS`は他の3コマンドと異なり、既存のマルチカーソル
状態に1個ずつ足すのではなく、プライマリ自身の選択(`m_sSelect`)だけを見て
「今の選択範囲」からマルチカーソル状態を丸ごと作り直す。既存の追加カーソルは
呼び出し時に全て破棄される。選択が無ければ何もしない(ErrorBeepのみ)。
複数行選択の終端がちょうど次の行の行頭(桁0)にある場合はその行を対象から
除く(Shift+↓で行末まで選択したときに触れていない次の行までカーソルが
増えるのを防ぐ、VS Codeと同じ扱い)。

Alt+クリック(2026-09-04追加)はF-codeを持たない。単発の左クリックは
`CKeyBind.cpp`のキー割り当てテーブル経由でディスパッチされる仕組みが無いため、
`CEditView_Mouse.cpp`の`OnLBUTTONDOWN`/`OnLBUTTONUP`に直接ハードコードして
いる。`OnLBUTTONDOWN`でキャレット移動前のプライマリ位置を退避し、
`OnLBUTTONUP`でドラッグが無かったと判明した時点でプライマリをその位置へ戻し、
クリック位置を新規`SExtraCursor`として追加する(既存のカーソルと重複する
場合は追加しない)。既存のextraはクリアされないため、繰り返しAlt+クリックで
カーソルを積み上げられる。

Ctrl+Alt+↑/↓は元々`F_UP2_BOX`/`F_DOWN2_BOX`(矩形選択の2行ジャンプ)の既定
キーだった。既存コマンドに`IsBoxSelecting()`で分岐させ二重役割を持たせる案は
キー割り当て設定画面での表示名不一致を理由にユーザーが却下したため、独立した
新規F-codeを新設した。矩形選択とマルチカーソルは相互排他で、どちらかの開始が
他方を解除する。

### 選択解除の条件

VS Codeの標準的な解除トリガーと突き合わせ、1つずつ実装した: Esc
(`Command_CANCEL_MODE`)、装飾キー無しの通常クリック、検索/置換ダイアログの
表示、フォーカスロス全般(最終的に`CEditView::OnKillFocus`へ一本化。ダイアログ
作成時の`WM_KILLFOCUS`と個別クリアが二重処理になっていたため、後者は第20パスで
撤去)、矩形選択の開始。「選択がある状態でのShiftなし矢印」は選択のみ収束し
カーソル自体は残るという単一カーソルの既存ロジックがそのまま正しく動くことを
確認済み。保存など特定コマンドでの解除は、ユーザー判断で対象外とした。

### 描画とIME

Win32のネイティブキャレットは1ウィンドウにつき1個までのため、プライマリは
既存のまま、追加カーソルは`CEditView_Paint.cpp`でGDIによる手動描画
(`FillRect`)にした。OSのキャレット点滅タイマーに同期させると消灯フェーズで
再描画されそのまま不可視になる不具合が出たため、常時表示に統一している。
色はドキュメントタイプの設定色(`COLORIDX_CARET`/`_IME`)をプライマリと同じ
ロジックで再導出する。2026-09-04には、グリフアトラス/カラーフォントの
BitBlt転送より後ろへ描画順を移動する修正も入れた(それより前に描くと、
グリフアトラスが有効な行でBitBltにカーソルの矩形が上書きされ、共通設定を
一度OKした後にだけ追加カーソルが見えなくなる不具合があった)。

選択ハイライトは`CColor_Select`にextra用キャッシュを追加して複数化した。
既知のコズメティックな差異として、extraの行末選択ハイライトが画面右端まで
伸びず実際の最終文字位置で止まる(選択されるテキスト自体は正しい)。

IMEは、`WM_IME_NOTIFY`受信時にextra分の再描画を追加した。IME再変換は
マルチカーソル中は無効化しており、矩形選択時の既存の制約と同じ扱い。文字
確定の挿入自体は通常のタイピングと同じ経路で全カーソルへ反映される。

### Undo/Redoとの関係

編集の記録自体は新規実装が無い。`HandleCommand`が1コマンドにつき1個の
`COpeBlk`を開き、`ApplyToAllCursors`から呼ばれる`InsertData_CEditView`/
`DeleteData_CEditView`は開いているブロックに`COpe`を積むだけなので、N個の
カーソル分の編集が自動的に1回のUndoステップにまとまる。

ただし「戻した後にキャレットをどこへ置くか」には専用の復元機構が必要
だった。単一カーソル時代の`Command_UNDO`/`Command_REDO`は「ブロック内で
最後に処理したOpe」の位置を無条件にプライマリの位置として信頼する実装
だったが、`ApplyToAllCursors`が1ブロックにN個のカーソル分のOpeを積むと、
最後に処理したOpeが必ずしもプライマリのものとは限らなくなる(ドキュメント
降順処理のため、画面上の配置次第でどのカーソルが最後になるかが変わる)。
これによりUndo後にキャレットがプライマリでないextraの位置に復元される
不具合をユーザーが実際に報告し、`COpe::nCursorSlot`(編集直後に付与される
カーソル識別子)と、それを使って全カーソルを正式に復元する
`CViewCommander::RestoreMultiCursorAfterUndoRedo(pcOpeBlk, nOpeBlkNum, bIsUndo)`
を新設して解決した。この関数は`Command_UNDO`/`Command_REDO`共通で使う
(元は両コマンドに個別実装されていたほぼ同一のロジックだったが、第20パスで
1関数に統合)。

「選択削除のUndoで選択状態も復元する」機能自体は別の機能フラグ
`NKMM_UNDO_RESTORE_SELECTION`が担う(詳細は別レポート
`project_undo_restore_selection_feature`側)。マルチカーソルとの関わりは、
`RestoreMultiCursorAfterUndoRedo`が各スロットの`CReplaceOpe::bHadSelection`を
読み取り、プライマリ・extraそれぞれの選択を独立に復元する形で合流している
点のみで、この統合作業自体は本フラグ側の実装に含む。

## 既知の制約・対象外

- `nRelLine`はプライマリとの行数差を固定オフセットとして持つのみのため、
  マルチカーソルの編集操作を介さずウィンドウ幅変更等で折り返し行数そのものが
  変わると、その瞬間に意味がズレる可能性がある(未対応)。
- マウスでのカーソル**削除**、および「次の一致箇所を選択」(Ctrl+D相当)は
  対象外。
- マルチカーソル中のIME再変換は非対応(矩形選択と同じ制約)。
- カーソル統合が起きた回、統合に無関係な他カーソルの希望桁も一律リセット
  される。安全側(実桁に合わせる)にしか倒れないため実害は小さいと判断し、
  精緻な追跡は見送った。
- マルチカーソルのUndo復元直後、ステータスバーの選択文字数表示が一瞬古いまま
  残ることがある(原因を`CCaret::MoveCursor`内の`ShowCaretPosInfo`呼び出し順に
  一部特定したが未解消。実害は軽微)。
- `keybind_presets/Default.key`は`KeyDataInit[]`の既定値と別に手動同期する
  必要がある静的ファイルで、自動検知の仕組みは無い。今後マルチカーソル系の
  デフォルトキーを変更する際は要注意。

## 動作確認について

`docs/multicursor_design.html`の変更履歴(全20パス)によれば、各パスは
GUI上での実機確認(クリップボード読み取りによる選択・編集結果の検証を含む)
を経て次のパスに進んでおり、第17パスの時点で「ここまでの全機能に対する
回帰確認込みで検証済み」、第20パス(重複コード整理)でも「既存の全回帰
テストで統合前と完全一致することを確認済み」との記録がある。第18〜19パスの
Undo/Redo復元不具合は、折り返した3行構成での再現をユーザーが画面写真付きで
報告したものを修正・確認している。

本レポート作成時点では、上記の設計ドキュメントおよびgitコミット履歴の記録を
確認した以外の追加のビルド・実機テストは実施していない。
