# NKMM_COMMAND_PALETTE 実装レポート

対象フラグ: `NKMM_COMMAND_PALETTE`(新規)

関連する別フラグ・別文書:

- `NKMM_COMMAND_PALETTE_ROMAJI` / `NKMM_COMMAND_PALETTE_ROMAJI_KANJI` /
  `NKMM_COMMAND_PALETTE_ROMAJI_KANJI_JIS1TABLE` — 本パレットの絞り込みに
  使うローマ字あいまい検索・漢字読み展開の詳細は
  `changelog/NKMM_COMMAND_PALETTE_ROMAJI_KANJI_JIS1TABLE.md`を参照。
  本レポートは、日本語入力のマッチングの中身までは説明せず、パレット本体
  (呼び出し方・データソース・一覧描画・選択・アニメーション)を対象とする。

## 背景

VSCode等のクイックオープン/コマンドパレットに相当する機能が無く、コマンドを
実行するにはメニューを辿るか既存のキー割り当てを覚える必要があった。
`Shift+Ctrl+P`一発で「全コマンド」「現在開いているファイル」を1つの絞り込み
リストにまとめて検索し、Enterで即実行/切り替えできるダイアログを追加した。

既定の`Shift+Ctrl+P`は元々`F_PRINT_PREVIEW`(印刷プレビュー)に割り当て
済みだったため、このキーをコマンドパレットへ差し替え、印刷プレビュー側は
既定ショートカット無しにした(`sakura_core/func/CKeyBind.cpp`)。

## 追加したファイル

- `sakura_core/dlg/CDlgCommandPalette.h` / `.cpp` — ダイアログ本体
- `changelog/NKMM_COMMAND_PALETTE.md`(このファイル)

初回コミット(`4a093400a`)では上記に加えて、ローマ字あいまい検索エンジン
一式(`util/RomajiFuzzyMatch.hpp`、`util/CFuzzyMatchJp.h/.cpp`、
`util/CKanjiReadingDict.h/.cpp`)も同時に追加されているが、これらは
`NKMM_COMMAND_PALETTE_ROMAJI`側の文書で扱う。

## 修正した既存ファイル

- **`sakura_core/Funccode_x.hsrc`** — `F_COMMAND_PALETTE = 31338`を追加
  (`F_DLGWINLIST`直後の空き番号)。
- **`sakura_core/cmd/CViewCommander.h`** / **`.cpp`** —
  `Command_COMMAND_PALETTE()`の宣言・ディスパッチ。
- **`sakura_core/cmd/CViewCommander_Window.cpp`** —
  `Command_COMMAND_PALETTE()`本体。
- **`sakura_core/func/Funccode.cpp`** — `pnFuncList_Win[]`(コマンド一覧)に
  `F_COMMAND_PALETTE`を追加。
- **`sakura_core/func/CKeyBind.cpp`** — `Shift+Ctrl+P`を
  `F_PRINT_PREVIEW`から`F_COMMAND_PALETTE`へ差し替え。
- **`sakura_core/window/CEditWnd.h`** / **`.cpp`** —
  `m_cDlgCommandPalette`メンバの追加、`MessageLoop()`の
  `MyIsDialogMessage`列への追加(ダイアログ内のキー操作をアクセラレータより
  先に処理させるため)、親ウィンドウ移動・サイズ変更時の
  `FollowParentWindow()`呼び出し追加。
- **`sakura_core/sakura_rc.h`** / **`sakura_rc.rc`** —
  `IDD_DLG_COMMANDPALETTE`ダイアログテンプレート
  (`IDC_EDIT_COMMANDPALETTE_FILTER`絞り込みEdit +
  `IDC_LIST_COMMANDPALETTE`一覧)、コマンド名文字列リソースを追加。
  `sakura_rc.rc`はUTF-16LE(BOM付き)のため、CLAUDE.md記載の手順
  (Pythonでbytesとして読み込みUTF-16でデコード/エンコードして編集)に
  従っている。
- **`resource/MainMenu.ini`**(`35f90827c`) — メインメニューの「検索」
  メニューへコマンドパレット(funccode 31338)の項目とセパレータを追加し、
  メニューからも起動できるようにした。
- **`sakura_core/util/CSlideInAnimator.h`**(`a79a0f045`) — 元は
  `CDlgFind`とコマンドパレットにほぼ同一のスライドインアニメーション
  (ease-out、ウィンドウ位置固定)が二重実装されていたのを、値の時間補間だけを
  担う汎用クラスへ書き換えた(後述)。この共通化により`CDlgFind.cpp`側の
  スライド処理も同じクラスへ乗り換わっている。
- **`sakura_core/extmodule/CUxTheme.h`** / **`.cpp`**(`346320f6d`) —
  `CloseThemeData`APIラッパーを追加(選択行の半透明ハイライト用に開いた
  `HTHEME`をダイアログ破棄時に解放するため)。
- **`sakura_core/my_config.h`** — `NKMM_COMMAND_PALETTE`フラグと、隣接する
  `NKMM_COMMAND_PALETTE_ROMAJI`系フラグの定義・詳細コメントを追加。

## 実装の詳細

### 呼び出しと単一インスタンス

`CViewCommander::Command_COMMAND_PALETTE()`は、検索ダイアログ
(`CDlgFind::Command_SEARCH_DIALOG`)と同じ流儀で、既にパレットが開いていれば
`ActivateFrameWindow()`でアクティブ化するだけにし、無ければ
`CDlgCommandPalette::DoModeless()`で新規に開く。モードレスダイアログとして
実装されており、開いたままエディタ側の操作(ウィンドウ移動・最小化等)にも
追従できる。

### データソース

一覧(`m_vAllRows`)は`BuildAllRows()`で構築する、大きく2系統のデータを1つの
フラットな配列にまとめたもの。

- **コマンド**(`ROWKIND_COMMAND`) — `CFuncLookup`(メインメニューが使う
  カテゴリ/項目列挙)を全カテゴリ・全項目走査し、`CKeyBind::GetKeyStr()`で
  現在のショートカットキー文字列を取得して名前の横に表示する。
  実行時は`WM_COMMAND`でその機能番号を転送する。
- **開いているファイル/最近使ったファイル**(`ROWKIND_WINDOW` /
  `ROWKIND_RECENT`) — `CAppNodeManager::GetOpenedWindowArr()`で、タブ=
  別プロセスをまたいで現在開いている全ウィンドウを列挙する。ウィンドウ切替は
  `hwndFile`を保持して`ActivateFrameWindow()`、最近使ったファイル
  (MRU)は`IDM_SELMRU + i`を`WM_COMMAND`で送り、`CEditWnd::OnCommand()`の
  既存のMRU処理に委ねて実行する。

読み取ったコードには、上記に加えて「@」接頭辞でのアウトライン解析結果
(`BuildOutlineRows()`)、「#」接頭辞でのブックマーク一覧
(`BuildBookmarkRows()`)への絞り込みモードも実装されており、いずれも
初回切り替え時にだけ遅延構築される。これらは同じ枠組み上の拡張だが、
初回実装コミットの範囲(コマンド・最近使ったファイル)を超えるため、
本レポートでは存在の確認のみに留める。

### 絞り込みパイプライン(概要)

`UpdateList()`が、フィルタEditの入力文字列(先頭の`>`/`edt `/`@`/`#`等の
接頭辞でモードを判定し、それを除いた実クエリ)を使って`m_vAllRows`を
走査し、一致した行の添字だけを`m_vMatchedRowIndices`に積み直す
(`LVS_OWNERDATA`のため、表示行は常にこの添字配列経由で
`m_vAllRows`を引く)。実際の文字列マッチ自体は`CFuzzyMatchJp.cpp`の
`FuzzyMatchJapanese()`(正規化+スコアリング付き部分列マッチ)に委ねており、
スコア降順に並べ替えて表示する。日本語入力(ローマ字/漢字読み展開)の
詳細は前述のROMAJI系文書を参照。

一覧描画は複数列のレポート形式ではなく、単一列(ヘッダ非表示)の
owner-draw(`NM_CUSTOMDRAW`、`OnListCustomDraw()`)で行っている。ヘッダ
非表示スタイル(`LVS_NOCOLUMNHEADER`)は生成後にランタイムでトグルすると
comctl32内部でレイアウトが破綻してクラッシュする問題があったため、
`sakura_rc.rc`のダイアログテンプレート側で生成時スタイルとして直接指定し、
実行時トグルは行っていない。同様に、`NMCUSTOMDRAW::rc`/`uItemState`は
`LVS_REPORT`表示下では信頼できない(comctl32のバージョン依存の癖)ため、
`ListView_GetItemRect()`で矩形を取り直し、選択状態も
`ListView_GetItemState()`を信用せず`m_nSelectedDispIndex`(選択に関わる
全経路が経由する`LivePreviewSelection()`で自前追跡)だけを根拠にしている。

### 選択行の見た目

コマンド行はアイコン無し+太字名+右寄せグレーのショートカットキー、
ウィンドウ/最近使ったファイル行はアイコン(`GetShellIconIndex()`、
`SHGetFileInfo`の共有システムアイコンを拡張子ごとにキャッシュ)+太字ファイル
名+グレーの格納フォルダで描く。

選択行の背景は、単色反転(`COLOR_HIGHLIGHT`)ではなく、エクスプローラーの
ファイル一覧と同じ半透明選択色にするため`"Explorer::ListView"`の
`HTHEME`(`CUxTheme::OpenThemeData`、テーマ非対応環境では従来の単色反転へ
フォールバック)を使い、`DrawThemeBackground(LVP_LISTITEM, LISS_SELECTED)`
で描く。半透明合成の上に繰り返し描くと色が濃く積み重なる(矢印キー押しっぱ
なしで選択行が変わらないまま再描画され続けるケースで実際に発生)ため、
描画前に必ず非選択時と同じ下地色(最終的に`RGB(247,250,252)` —
Visual Studio 2026のツリービュー/リストビュー同色をスポイト実測して採用)で
一度クリアしてから重ねている。

絞り込みに一致した部分文字列は、選択有無に応じた文字色で3分割描画して
ハイライトする(VSCodeのクイックオープン風)。ここでのハイライト判定は表示名
に対する素直な部分文字列一致のみで、あいまい一致(ローマ字/漢字読み展開)の
一致箇所までは追っていない。

### 枠線・フォーカス表示

当初は`sakura_rc.rc`側の`WS_BORDER`(非クライアント側)に縁取りを任せて
いたが、太さが`GetSystemMetrics(SM_CXBORDER)`依存でDPI拡大率次第では
1論理pxを超えて太く見えることが分かった(`8b6d173b1`)。`WS_BORDER`を外し、
ダイアログ自体をサブクラス化(`PaletteDlgSubclassProc`)して`WM_PAINT`で
常に1デバイスピクセル幅の縁を`Rectangle()`で直描きする方式に変更した。

続けて(`bab5e569f`)、フィルタEditがフォーカスを持っている間だけ、絞り込み
一致ハイライトと同じ`COLOR_HOTLIGHT`でその周囲を強調枠として描くようにした。
強調枠はフィルタEdit自身の外側(親ダイアログのクライアント領域)に描くため、
`WM_SETFOCUS`/`WM_KILLFOCUS`側でフィルタEditの矩形を包む範囲を
`InvalidateRect(..., bErase=TRUE)`で無効化しないと、フォーカスが外れても
前回描いた枠が下辺の「取り残し」として残る不具合があり、それを踏まえた実装
になっている。

### スライドインアニメーションの汎用化(`a79a0f045`)

`CDlgFind::StartSlideAnimation()`と`CDlgCommandPalette::StartSlideAnimation()`
に、定数値・ease-out計算式までほぼ同一のスライドイン処理(上から下へ)が
二重実装されていたのを`util/CSlideInAnimator.h`の`CSlideInAnimator`に
共通化した。さらに、当初は「値の計算」と「値の反映(`SetWindowPos`)」が
クラス内で一体化していたため、コマンドパレットの一覧高さリサイズ
(リスト・ダイアログ本体という2つのウィンドウへ、位置ではなく高さとして
反映する必要がある)を同じ枠組みで扱おうとすると描画部までクラスに
持ち込む必要が出てきてしまう問題があった。そこで`Start()`が
`InitFunc`/`ApplyFunc`/`FinalizeFunc`(`std::function`)を受け取る形に
書き換え、クラス自体は「今どの値であるべきか」(開始値→目標値への
ease-out補間)の計算だけを担い、実際の反映は呼び出し側のラムダに委ねる
設計にした。この結果、`CDlgFind`側のスライドインと、コマンドパレットの
「開くときの上からのスライドイン」「絞り込み結果の件数に応じた一覧高さの
アニメーション(`m_cListHeightAnimator`、別インスタンスだが同じクラスを
使い回す)」の3箇所が同じ汎用クラスを共有している。

### コマンドの二重登録の解消(`8b6d173b1`)

`F_JUMP_DIALOG`(指定行へジャンプ)のように、同じコマンドが複数の
メニューカテゴリ(カーソル移動系・検索系など)に登録されているケースがある。
通常のプルダウンメニューではカテゴリごとに別メニューとして表示されるため
問題にならないが、コマンドパレットは全カテゴリを1つのフラットな一覧にまとめる
ため、そのままでは同じコマンドが複数回表示されてしまっていた。
`BuildAllRows()`で`std::set<EFunctionCode>`に登録済み機能番号を記録し、
既に出た機能番号は以後スキップすることで解消した。同時に、一覧の
`WS_BORDER`分の高さをウィンドウ矩形換算するときの補正
(`m_nListBorderHeight`)も入れ、行数から求めた高さで最終行がわずかに
見切れる不具合も修正している。

### 背景色の変更(`7303ddcfc`)

選択行の半透明合成の下地、および非選択行の背景を、当初の
`COLOR_WINDOW`(白)から、Visual Studio 2026のツリービュー/リストビューと
同じ配色(`RGB(247,250,252)`、スポイト実測)に変更した。白背景が周囲から
浮いて見えるという指摘に基づく調整。

## 動作確認について

読み取ったコミットメッセージの範囲では、フルビルドの成否について明記した
記述は確認できなかった(`NKMM_FIX_MOVE_LINE`等の他レポートのような
「msbuildで0エラー確認」という定型記述はこの一連のコミットには見当たらない)。
一方で、各fixコミットの内容(二重登録・枠のちらつき・フォーカス枠の
取り残し・選択色の濃淡蓄積・一覧の見切れ等)は、いずれも実機での見た目・
操作を確認した上でしか気づけない類の不具合であり、コミットが積み重なった
経緯自体が実機での試用とフィードバックを繰り返しながら実装されたことを
示している。本レポート作成時点で、ビルド確認や実機での新規動作確認は
行っていない。
