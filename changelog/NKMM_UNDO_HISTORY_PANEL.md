# NKMM_UNDO_HISTORY_PANEL 実装レポート

対象フラグ: `NKMM_UNDO_HISTORY_PANEL`(新規)

## 背景

Paint.NETの「履歴」パネルのように、Undo/Redoの操作履歴を一覧表示し、任意の
時点をクリックするだけでそこまでUndo/Redoをまとめてジャンプできる、常時表示の
フローティングパネルを追加した。トグルキーはF5(既存の「再描画」はShift+F5へ
退避)。6回のバージョンアップ(Ver.1〜Ver.6)を経ており、途中でウィンドウの
実装方式自体を1度全面的にやり直している。本レポートはその一連のコミット
(9209c7d61、5faff5540、f74677ab3、dfb62427a、0d707cf57、aa3e68b85)を通しで
読み、最終的にリポジトリに残っているコードの構成を軸にまとめたもの。

## 追加したファイル

- **`sakura_core/dlg/CDlgHistoryPanel.h`/`.cpp`** — パネル本体(Ver.1で新規
  作成、以後全バージョンで改修)。
- **`sakura_core/window/CBorderlessWnd.h`/`.cpp`** — Ver.3で
  `CDlgHistoryPanel`から「本物のボーダーレスウィンドウ」の共通機構を抽出した
  汎用基底クラス(後述)。`NKMM_UNDO_HISTORY_PANEL`のガード無しで常にビルド
  対象になっており、他機能からの再利用を前提にしている。
- **`changelog/NKMM_UNDO_HISTORY_PANEL.md`**(このファイル)

いずれも新規4ファイル(`CDlgHistoryPanel.h/.cpp`、`CBorderlessWnd.h/.cpp`)は
Ver.1・Ver.3のコミット内で`sakura/sakura.vcxproj`・
`sakura/sakura.vcxproj.filters`へのエントリ追加まで含めて既にコミットされて
おり、現状のvcxprojにも反映済みであることを確認した(追加で手動対応が必要な
項目は無い)。

## 修正した既存ファイル

- **`sakura_core/my_config.h`** — `NKMM_UNDO_HISTORY_PANEL`フラグ本体の定義
  とコメント。
- **`sakura_core/Funccode_x.hsrc`** — `F_SHOWUNDOHISTORYPANEL`を追加。
- **`sakura_core/func/Funccode.cpp`** — コマンド一覧・ヘルプID・
  `IsFuncEnable()`への登録。
- **`sakura_core/func/CKeyBind.cpp`** — デフォルトキー割り当てで
  `VK_F5`の1列目を`F_REDRAW`から`F_SHOWUNDOHISTORYPANEL`に差し替え、
  `F_REDRAW`は2列目(Shift+F5相当)へ退避。
- **`sakura_core/cmd/CViewCommander.h`/`.cpp`** —
  `Command_SHOWUNDOHISTORYPANEL()`のディスパッチ登録。
- **`sakura_core/cmd/CViewCommander_Settings.cpp`** —
  `Command_SHOWUNDOHISTORYPANEL()`本体。ini(`m_bDispUNDOHISTORYPANEL`)の
  トグルと`LayoutUndoHistoryPanel()`呼び出し、全ウィンドウへの
  `MYWM_BAR_CHANGE_NOTIFY`(`MYBCN_UNDOHISTORY`)通知を行う(他のバー表示/
  非表示コマンドと同じ流儀)。
- **`sakura_core/cmd/CViewCommander_Edit.cpp`** — `Command_UNDO()`/
  `Command_REDO()`の末尾でパネルが開いていれば
  `CDlgHistoryPanel::OnUndoStackChanged()`を呼び、一覧を再構築させる。
- **`sakura_core/window/CEditWnd.h`/`.cpp`** — `CDlgHistoryPanel`メンバー
  (`m_cDlgHistoryPanel`)の追加、`LayoutUndoHistoryPanel()`(表示/非表示の
  実処理)、`MessageLoop()`での`MyIsDialogMessage`登録、`SetActivePane()`での
  `ChangeView()`呼び出し(タブ内ペイン切替時の対象ビュー差し替え)、
  `DispatchEvent()`のウィンドウ移動/サイズ変更・最小化復元時の
  `FollowParentWindow()`呼び出し。
- **`sakura_core/env/CommonSetting.h`/`CShareData_IO.cpp`** —
  ini設定`m_bDispUNDOHISTORYPANEL`(次回起動時にパネルを表示するか)の追加と
  読み書き。
- **`sakura_core/config/system_constants.h`** — `MYWM_HISTORYPANEL_JUMP`
  (`WM_APP+228`。Ver.6で追加、後述のクラッシュ修正用)。
- **`sakura_core/COpe.h`/`COpeBlk.h`/`COpeBuf.h`/`.cpp`** —
  一覧のツールチップ/プレビュー文字列表示のための拡張(後述)。
- **`sakura_core/view/CEditView.cpp`/`CEditView_Command_New.cpp`** —
  操作生成時(`InsertData_CEditView`/`DeleteData2`/
  `ReplaceData_CEditView3`)にプレビュー用文字列を専用領域へコピーする処理。
- **`resource/MainMenu.ini`** — ウィンドウメニューに項目(funccode
  `31339` = `F_SHOWUNDOHISTORYPANEL`)を追加。
- **`sakura_core/sakura_rc.h`/`sakura_rc.rc`** — 文字列リソース・ダイアログ
  リソース関連の追加(Ver.1ではダイアログテンプレートIDD_DLG_UNDOHISTORYを
  追加していたが、Ver.2のCDialog全廃に伴いその後不要化)。`sakura_rc.rc`は
  UTF-16LE(BOM付き)のため、CLAUDE.md記載の手順(Pythonでbytesとして読み込み
  UTF-16でデコード/エンコード)で編集されている。
- **`docs/sakura_keybind_list.html`** — `F_SHOWUNDOHISTORYPANEL`の行を新規
  追加し、`F_REDRAW`の既定キーをF5からShift+F5に更新。v2.4.3プリセットとの
  F5衝突を既存の注記慣習に沿って明記。
- **`sakura/sakura.vcxproj`/`.vcxproj.filters`** — 新規4ファイルの登録
  (前述)。

## 実装の詳細

### Ver.1: CDialogベースの初期実装

最初のバージョンは`CDlgWindowListライク`な`CDialog(true)`(タイトルバー・
サイズ変更枠付きの可変ダイアログ機構)を使い、ダイアログテンプレート
(`IDD_DLG_UNDOHISTORY`)から生成する、通常のモードレスダイアログとして
実装された。この時点で以下の実装上の工夫・不具合対応が入っている。

- **オーナー誤補正バグ**: `CDialog::DoModeless()`内部の
  `ResolveDialogOwnerWindow()`(既存の`NKMM_FIX_DIALOG_OWNER`機構)は、渡された
  `hwndParent`がその時点で非表示だと`GetForegroundWindow()`へ「補正」して
  しまう。タブ切替直後などで本来のオーナー(自分が属する`CEditWnd`)が一時的に
  非表示だと、無関係な(別プロセスの可能性もある)前面ウィンドウがオーナーに
  され、以後`ShowOwnedPopups()`によるタブ切替時の自動非表示がそのオーナーには
  効かず、パネルが元のタブの位置に取り残されるという実バグが見つかった。
  `DoModeless()`直後に`GWLP_HWNDPARENT`を明示的に正しい`hwndParent`へ
  上書きすることで対処した(このパネル自身は所属する`CEditWnd`が常に明確に
  わかっているため、汎用救済策の`ResolveDialogOwnerWindow()`の結果を鵜呑みに
  しない)。
- **クリックによる暗黙アクティブ化**: `WM_MOUSEACTIVATE`を横取りして
  `MA_NOACTIVATE`を返すだけでは、一覧(`SysListView32`)が既定のクリック処理内
  で自分自身へ`::SetFocus()`する際にトップレベルウィンドウを暗黙にアクティブ
  化してしまう問題が残った。`WS_EX_NOACTIVATE`を追加で立てることで解消。
- ボタン(元に戻す/やり直し)については、非アクティブ状態からの最初のクリックが
  「アクティブ化だけ」で消費され、実際のコマンド実行(`BN_CLICKED`)まで届かない
  問題があり、`WM_MOUSEACTIVATE`の時点でカーソル位置がボタン上かを判定し、
  その場で`BN_CLICKED`を合成して即時実行し`MA_NOACTIVATEANDEAT`を返す、という
  手当てを行っていた(この手当て自体はVer.2の全面書き直しで別方式に置き換わる)。

### Ver.2: CDialog全廃、ボーダーレスウィンドウ化

Ver.1のCDialogベースのまま、見た目のタイトルバー・枠を消してボーダーレス化
しようとしたところ、`CDialog`(DLGPROC)自体がこの技法と根本的に相性が悪い
ことが判明し、`CDialog`依存を完全にやめて素の`CreateWindowEx`+自前`WNDPROC`
へ全面的に書き直された。

採用した技法は
[melak47/BorderlessWindow](https://github.com/melak47/BorderlessWindow)方式:
`WS_CAPTION | WS_THICKFRAME`スタイル自体は保持してOS標準の全辺リサイズ・Aero
スナップ・DWM合成影を活かしつつ、`WM_NCCALCSIZE`(クライアント矩形を提案
された「ウィンドウ矩形そのまま」にしてキャプション・枠の描画領域を確保させ
ない)と`WM_NCHITTEST`(外周はリサイズ判定、タイトル帯は`HTCAPTION`)を横取り
することで、OS標準の非クライアント領域の描画だけを消す。見た目のタイトル帯
(色付き背景+タイトル文字+閉じるボタン)は自前描画、ドラッグ移動はOS標準の
`HTCAPTION`ドラッグそのものに任せている(自前でドラッグループを回す必要が
無い)。タイトル帯の配色は決め打ちにせず`GetSysColor(COLOR_ACTIVECAPTION)`等
を都度問い合わせており、Windowsの「タイトルバーにアクセントカラーを表示する」
設定に自動追従する(既存の`CDlgFuncList`のドッキング時タイトル描画も同じ色を
使っており、決め打ちの独自色ではない)。

最大化時の保険として、`WM_NCCALCSIZE`でクライアント矩形をウィンドウ矩形の
まま返す実装だと万一最大化された場合にウィンドウ矩形自体がモニタ外(タスク
バーの下)まで含んでしまうため、`IsZoomed()`相当の判定でモニタの作業領域に
補正する処理も入れている。

### Ver.3: CBorderlessWndへの共通機構の抽出

Ver.2で書いたボーダーレスウィンドウの仕組みのうち、Undo履歴パネル固有でない
部分(非クライアント領域描画・ヒットテスト・アクティブ化抑制・閉じるボタン・
DWM影セットアップ・親ウィンドウへの追従等)を`window/CBorderlessWnd`
(`CWnd`派生)へ抽出し、`CDlgHistoryPanel`はそれを継承する形に整理した。
`CBorderlessWnd`は`NKMM_UNDO_HISTORY_PANEL`のガード無しで常にコンパイル対象
になっており、コードコメント上も「他機能でも再利用できるように」と明記されて
いる。派生クラスが実装すべき仮想関数は`GetWindowClassName()`/
`GetTitleText()`/`GetDefaultSize()`/`LayoutChildren()`/`OnCloseRequested()`
等で、閉じるボタンは`CreateCloseButton()`を呼べば描画・クリック処理を基底が
肩代わりする。

### 表示/非表示、タブ切替・最小化復元との同期

- F5(`Command_SHOWUNDOHISTORYPANEL`)がトグル。ini
  `m_bDispUNDOHISTORYPANEL`を反転させ`LayoutUndoHistoryPanel()`
  (`CEditWnd`)を呼ぶ。表示時は`m_cDlgHistoryPanel.DoModeless(...)`で生成、
  非表示時は`::DestroyWindow()`する(`CEditWnd`のメンバーとしての
  `CDlgHistoryPanel`オブジェクト自体は破棄されず、ウィンドウハンドルだけが
  作り直される)。
- タブ切替時の自動非表示/再表示は、サクラエディタの既存の
  `ShowOwnedPopups()`ベースの仕組み(オーナーウィンドウ経由)にそのまま乗る。
  Ver.1で見つかった「オーナー誤補正でこの仕組みが効かなくなる」バグは前述の
  通り対処済み。
- 本体ウィンドウの最小化/復元には`FollowParentWindow()`
  (`CEditWnd::DispatchEvent()`のウィンドウ移動/サイズ変更時に呼ばれる)で
  追従し、最小化中は位置を動かさず非表示相当にする。
- アクティブなタブ内でペイン(ビュー)が切り替わったときは`ChangeView()`で
  対象`CEditView`を差し替えて一覧を再構築する。

### 位置・サイズの決定

最終的な設計は次の通り(`CBorderlessWnd::ComputeBottomRightPosition()`/
`FollowParentWindow()`/`m_nWidth`・`m_nHeight`/`m_nOffsetX`・`m_nOffsetY`を
実際に読んで確認):

- **サイズ**: 初回表示時は既定値(`GetDefaultSize()`。Ver.1のダイアログ
  テンプレート初期サイズの縦横それぞれ半分に近い、コンパクトな値)。以後は
  `CBorderlessWnd`のメンバー`m_nWidth`/`m_nHeight`にユーザーがリサイズした
  値が記憶される。このメンバーは`CDlgHistoryPanel`オブジェクト自体
  (`CEditWnd`のメンバー変数)が生きている間保持されるため、F5でパネルの
  ウィンドウハンドルを破棄→再生成しても、同一セッション中は直前のサイズが
  維持される。ウィンドウを閉じてプロセス(タブ)を終了すれば記憶は消える
  (iniへの永続化は無い)。
- **位置**: 初回表示時は`ComputeBottomRightPosition()`で親ウィンドウ
  (エディタ本体)の右下(20pxマージン)に配置する。以後の追従は毎回この位置へ
  スナップし直すのではなく、パネル自身の`WM_MOVE`(ユーザーがタイトル帯を
  ドラッグした場合を含む)のたびに親ウィンドウとの相対オフセット
  (`m_nOffsetX`/`m_nOffsetY`)を記録し直し、`FollowParentWindow()`はその
  オフセットを保ったまま平行移動する、という「ユーザーがドラッグした位置を
  尊重する」相対追従方式になっている。
- **起動直後の再スナップ処理(実バグ修正)**: Ver.4で、
  `ComputeBottomRightPosition()`に`GetParentHwnd()`(=`CWnd`の
  `m_hwndParent`)を直接読ませていたところ、`CreateBorderlessWindow()`内での
  最初の呼び出し時点ではまだ`Create()`を呼んでおらず`m_hwndParent`が`NULL`の
  ままで、結果が毎回モニタ左上(0,0)にクランプされてしまう実バグが見つかり、
  呼び出し側から`hwndParent`を明示的に渡す形に修正した。さらに、起動直後の
  「前回の表示状態を復元する」経路(`CEditWnd::Create()`の子ウィンドウ生成中)
  では本体ウィンドウ自身がまだ最終的な位置・大きさになっていないことがあり、
  その時点の親矩形を基準に計算した既定位置のままオフセット追従を続けると、
  本体が本当の位置に落ち着いた後もパネルがずれた位置に留まり続ける
  (「F5を押しても反応が無いように見え、もう一度押すとようやく画面内に現れる」
  という紛らわしい挙動になる)ため、`m_bNeedsInitialResnap`フラグを設け、
  最初の`FollowParentWindow()`呼び出しでもう一度その時点の親矩形を取り直して
  1回だけ位置を補正し直すようにした。

### Undo/Redoジャンプ実行と、そこで見つかった実機バグ2件(Ver.6)

行クリックで`ExecuteJump()`が`Command_UNDO`/`Command_REDO`をN回ループ呼び出
しし、目的の時点までまとめてジャンプする。この経路で実機検証により2つの
バグが見つかり修正された。

1. **comctl32再入クラッシュ/ハング**: 一覧(`SysListView32`)の`NM_CLICK`
   ハンドラの中で同期的に`ExecuteJump()`〜`RefreshList()`
   (`ListView_SetItemCount`/`SetItemState`等)を呼ぶと、まだ処理中の
   `SysListView32`自身の`WM_LBUTTONUP`コールスタック上に再入することになり、
   comctl32内部でクラッシュ/ハングした。対処として`MYWM_HISTORYPANEL_JUMP`
   (`WM_APP+228`)を新設し、`NM_CLICK`ハンドラでは`PostMessage()`するだけに
   留め、実際のジャンプ処理は通知処理が完全に戻り切った後の別メッセージ
   (`DispatchEvent_WM_APP`)側で実行するようにした。
2. **複数ステップ飛びで描画が反映されないバグ**: `Command_UNDO`/`REDO`内部の
   再描画は「その1ステップ単独でキャレット行・折り返しが変わったか」という
   条件で全体再描画(`Call_OnPaint`)するかどうかを決めている
   (`CViewCommander_Edit.cpp`)。複数ステップをまとめて飛ぶ場合、途中の
   ステップは描画を止めた状態で実行され、最後の1ステップ(描画有効な唯一の
   ステップ)がその条件に当てはまらないと、データは正しく更新されているのに
   画面だけが古いまま、という表示不整合が発生した。ループ後に2ステップ以上
   飛ばした場合は無条件で`m_pcView->RedrawAll()`するよう修正した。

### 一覧のプレビュー文字列とツールチップ(Ver.5〜Ver.6)

一覧の各行には操作名に加え、実際に挿入/削除された文字列の先頭部分を1行に
まとめて表示する。`COpe`のUndo/Redoは「今ドキュメントに実データが無い側」
だけを保持するping-pong方式(Undo/Redoが実際にその方向へ適用された直後に
`clear()`される)のため、そのまま読むと「一度もUndoされていない」ブロックや
「一括Undoでスキップされた"間"のRedo待ちブロック」のプレビューが空になって
しまう。この対策として`COpe::cmemHistoryPreviewIns`/`cmemHistoryPreviewDel`
という専用領域を設け、`InsertData_CEditView`/`DeleteData2`/
`ReplaceData_CEditView3`が生成時に一度だけ独立してコピーしておく方式にした。

Ver.5では、この文字列をツールチップに組み立てる際に`auto_sprintf()`を使って
いたところ、実機でクラッシュが発生した。原因は`auto_sprintf_s()`がMSVC>=1400
では`tchar_snprintf_s()`経由で`tchar_vsprintf_s_imp()`に落ちるが、これは
`%s`フィールドがバッファに収まらない場合に切り詰めず
`_invalid_parameter_internal()`/`_invoke_watson()`でプロセスを異常終了させる
実装だったこと(`snprintf`という名前にもかかわらず`vsnprintf`相当の安全な
切り詰めになっていない既存コードの罠)。プレビュー文字列は最大240文字程度に
なり得るため確実にこの罠を踏んでいた。修正は`printf`系を使わず、必ず切り
詰められる`lstrcpyn()`と`CNativeW::AppendString(ptr,len)`だけで文字列を組み
立てる方式に変更した。

同じVer.5では、一覧のホバー行・現在位置行の描画も、単色反転ではなく
`CDlgCommandPalette`と同じ"Explorer::ListView"テーマ(`LISS_SELECTED`/
`LISS_HOT`)による半透明の選択色に変更している(テーマ非対応環境では
`COLOR_HIGHLIGHT`の単色反転にフォールバック)。ホバー追跡のため一覧
(`SysListView32`)自体をサブクラス化している。

### 対象コマンドと有効/無効

`Command_UNDO`/`Command_REDO`の末尾でパネルが開いていれば
`OnUndoStackChanged()`を呼び一覧を再構築する。編集操作自体
(`InsertData_CEditView`等)側にはパネルの有無に関する分岐は無く、Undo/Redo
バッファへの記録(`COpeBuf`)はパネルの有無に関係なく常に行われる。

## 動作確認について

各コミットのログから読み取れる範囲では、以下は実機(GUI)での確認を経て
修正されている。

- Ver.1: `CDialog`オーナー誤補正バグ、クリックによる暗黙アクティブ化・二度
  押し問題。
- Ver.6: 一覧クリックのcomctl32再入クラッシュ/ハング、複数ステップジャンプ
  時の描画不整合。
- Ver.5: `auto_sprintf()`のツールチップ組み立てで発生した実クラッシュ。

一方で、以下は各コミットの記述からは実機での最終確認が明言されておらず、
未確認/要フォローアップとして扱うべき項目として残っている。

- 本レポートの範囲では、実際のマウスドラッグによるリサイズ・タイトル帯
  ドラッグ移動そのものが「クリーンに動く」ことをGUI操作で最終検証した記録は
  見当たらない(位置・サイズ計算ロジックの実バグ修正はいずれも
  `WM_MOVE`/`WM_SIZE`通知やログからの推測ではなく実機で発見されたと明記
  されているため、少なくとも一部操作は行われているが、リサイズ・ドラッグの
  全パターンを網羅的になぞった記録は無い)。
- 最大化時のモニタ作業領域補正(`HistoryPanelAdjustMaximizedClientRect()`
  相当のロジック)は、このパネルには最大化ボタンが無く`Win+↑`等の間接的な
  最大化要求への保険として入れられたものであり、実際に最大化させて確認した
  記述は無い。
- 一連の変更のフルビルド結果(0エラー・0警告等)がコミットログ上に明記されて
  いるものは見当たらないため、本レポート作成時点であらためてビルドは行って
  いない。

いずれも実際にユーザーの手元でF5トグル・行クリックによるジャンプ・
リサイズ・タブ切替・最小化復元の一連の操作を試し、問題が無いか確認して
いただきたい。
