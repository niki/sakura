# 大規模ファイルでのアウトライン解析(ファンクション一覧)フリーズの調査と修正 20260811

対象フラグ: `NKMM_FIX_OUTLINE`(既存、新たに `sakura_core\my_config.h` の該当ブロックへ
今回の対応を追記)。新規フラグは追加していない。

対象ファイル:
- `sakura_core/outline/CDlgFuncList.cpp`(`SetTreeJava`, `SetData`, `SetListFlatVirtual`,
  `ShouldUseVirtualFlatList`, `GetActiveListHwnd`, `SortListView`, `GetData`,
  `OnNotify`[`LVN_GETDISPINFO`], `DispatchEvent`[`WM_APP_OUTLINE_CLEANUP_TREE`])
- `sakura_core/outline/CDlgFuncList.h`(メンバ追加)
- `sakura_core/sakura_rc.rc`(`IDC_LIST_FL_VIRTUAL`, Shift-JIS/CP932エンコーディング)
- `sakura_core/sakura_rc.h`(`IDC_LIST_FL_VIRTUAL`, `IDC_TREE_FL_CLEANUP_SCRATCH`)
- `sakura_core/my_config.h`(`WM_APP_OUTLINE_CLEANUP_TREE`の定義)
- `sakura_core/doc/CDocOutline.h`・`sakura_core/types/CType_Cpp.cpp`
  (`CDocOutline::ResolveOutlineType_C_CPP`、[修正7](#修正7-閉じた後にビジー
  トグルで閉じないの2件)参照)
- `sakura_core/cmd/CViewCommander_Outline.cpp`(`Command_FUNCLIST`、同上)

なお巨大ファイル解析中の進捗ダイアログは一度`sakura_core/types/CType_Cpp.cpp`・
`sakura_core/doc/CDocOutline.h`・`sakura_core/cmd/CViewCommander_Outline.cpp`に
実装したが後述の通り見送って削除した([修正4](#修正4実装したが見送り-巨大
ファイルの解析中の進捗ダイアログ)参照)。その後修正7で同じ3ファイルに
別目的(型解決の共通化)で再度手を入れている。

計測に使ったベンチマークスクリプト・生成テストファイルは `_dbg/outline_bench/`
にまとめてある(gitignore対象、リポジトリには追跡されない):
`gen_flat.js` / `gen_classes.js` / `gen_padded.js`(テストファイル生成)、
`bench_outline.js`(`Editor.Outline(1)` をマクロから叩いて壁時計時間を計測する
WSH/JScriptマクロ。使い方は `bench_search.js` と同じ形式:
`sakura.exe -M=bench_outline.js -MTYPE=file <path>`、`%BENCH_LABEL%` 環境変数で
CSVのラベル列を指定)、生成済みの `flat_2500.cpp` / `flat_10000.cpp` /
`flat_50k.cpp` / `classes_500x5.cpp` / `classes_2000x5.cpp` / `padded_350k.cpp`、
および実測結果 `bench_outline_results.csv`。

---

## 経緯

ユーザーから「大きなファイルを開いたときのアウトライン解析の改善」を依頼された。
まず `CDocOutline::MakeFuncList_C`(構文解析) / `CFuncInfoArr`(結果配列) /
`CDlgFuncList`(表示ダイアログ)のコードを読み、以下の複数のボトルネック候補を
洗い出した。

1. `CFuncInfoArr::AppendData` が1件ずつ `malloc`/`realloc`(容量倍加なし)
2. `CDlgFuncList::SetTreeJava` のクラスノード探索が `TreeView_GetNextSibling` +
   `TreeView_GetItemTextVector`(いずれも実`SendMessage`)による逐次比較
3. `TreeView_InsertItem`/`ListView_InsertItem` の呼び出しが `WM_SETREDRAW` で
   保護されておらず、ダイアログが既に表示中の再解析(`SHOW_RELOAD`)で
   再描画コストが乗る
4. UIの構築全体がUIスレッドで完全同期実行(非同期化していない)

ユーザーの意向で「まず計測してから決める」方針とし、実測ベースで進めた。

## 計測方法

`Editor.Outline(1)`(`F_OUTLINE` の `nAction=SHOW_RELOAD`)はダイアログが既に
開いていても強制的にフル再解析+再構築を行うため、マクロから呼んで
壁時計時間を測るだけで「ユーザーが体感するフリーズ時間」をそのまま計測できる
(検索の非同期化のときのような「マクロ呼び出し直後に結果を読むと非同期化で
stale値になる」問題が存在しない。アウトライン解析は現状完全に同期実行のため)。

Node.jsスクリプトで以下の合成C++ファイルを生成して計測した:

- `flat_2500.cpp` / `flat_10000.cpp` / `flat_50k.cpp`: クラスを持たないフラットな
  グローバル関数のみ(それぞれ2,500/10,000/50,000関数)
- `classes_500x5.cpp` / `classes_2000x5.cpp`: `ClassXNNNNN::methodM(...)` 形式の
  out-of-class定義で、500クラス×5メソッド=2,500関数、2,000クラス×5メソッド=
  10,000関数
- `padded_350k.cpp`: 関数はわずか20個だが、コメント行で35万行まで水増し
  (MakeFuncList_Cの純粋な行数コストを、関数数コストと切り分けるため)

## 発見1: 解析(MakeFuncList_C)自体は速い

`padded_350k.cpp`(35万行/関数20個)は `Editor.Outline(1)` が34〜106msで完了した。
これにより「行数が多いこと」自体はボトルネックでないと確認できた。問題は
関数の「表示側」(`CDlgFuncList`)にあると判断し、以降はそちらに絞った。

## 発見2: クラスノード探索のO(n)がクラス数に比例して支配的

同じ関数数でもクラスに分散させるだけで大きく遅くなることを確認:

| ファイル | 関数数 | クラス数 | 1回目 | 2回目以降 |
|---|---|---|---|---|
| flat_2500.cpp | 2,500 | 0 | 138ms | 180-185ms |
| classes_500x5.cpp | 2,500 | 500 | 605ms | 1,008-1,030ms |
| flat_10000.cpp | 10,000 | 0 | 449ms | 819-829ms |
| classes_2000x5.cpp | 10,000 | 2,000 | 3,934ms | 5,684-5,710ms |

原因は `SetTreeJava` が「クラス名::メソッド」の分類先ノードを探す際、既存の
兄弟ノードを `TreeView_GetNextSibling` + `TreeView_GetItemTextVector`(いずれも
実`SendMessage`)で逐次比較していたこと。クラス数に比例して1関数あたりの
挿入コストが増える。

### 修正1

`(親ノード, クラス名) → 既存ノード` のハッシュマップ(`std::unordered_map`、
キー型 `SOutlineTreeNodeKey`)をこの関数内でキャッシュし、探索をO(1)化した。
表示上のラベル文字列(クラス名+"クラス"等の追加文字列)ではなく生のクラス名
だけをキーにしているが、マップへの登録を新規作成時のみ行う(検索一致時は
上書きしない)ことで、既存の「同名の既存ノードが複数あれば最初の項目を
親にする」という仕様は変えていない。

あわせて `TreeView_InsertItem` の `hInsertAfter` に常に `TVI_LAST` を渡していた
箇所も、`親ノード → 最後に追加した子ノード` のマップ(`mapLastChild`)を使う形に
統一した(Win32のTreeViewは `TVI_LAST` 指定時、内部で兄弟リストの末尾まで
たどるという既知の挙動があり、明示ハンドルを渡せばO(1)になるはず、という
仮説による)。

修正後の再計測(class-heavyケース):

| ファイル | 修正前 | 修正後 |
|---|---|---|
| classes_500x5.cpp | 605-1,030ms | 296-595ms(約2倍) |
| classes_2000x5.cpp | 3,934-5,710ms | 1,225-3,045ms(約2〜5倍) |

fine-grained計測(`SetTreeJava`内に一時的にQueryPerformanceCounterを追加、
計測後に削除)で、挿入ループ自体は55〜76msまで縮み、残り(約700〜800ms)は
`TreeView_Expand`(2,000個超のトップレベルノードを展開する処理)が支配的
であることを確認した。

## 発見3: フラットな巨大関数リストは改善しなかった(仮説が誤りだったと判明)

同じ修正版で `flat_50k.cpp`(50,000関数、クラスなし)を再計測したところ、
**12,418〜27,268msとほぼ変化なし**だった(修正前: 12,220〜19,744ms)。

`TVI_LAST` を明示ハンドルに置き換える修正1は、クラス探索を経由しない
「分類先クラスのないグローバル関数」の挿入(`htiGlobal`という単一の親への
大量追加)には効くはずだったが、効果が見られなかった。

一時的にfine-grained計測を追加して切り分けたところ、`SetTreeJava`の
挿入ループ自体が12.4〜15.7秒(支配的)、末尾の`TreeView_Expand`は0.36〜0.38秒
(軽微)と判明。つまり「探索コスト」でも「TVI_LASTの末尾探索」でもなく、
**`TreeView_InsertItem`自体が、有効な明示ハンドルを渡してもなお同一親への
挿入でO(n)的に遅くなる**ことを実測で確認した。

推定: Win32のTreeViewコントロールには、ListViewの`LVS_OWNERDATA`に相当する
仮想化モードが存在せず、同一親ノードへの数万件規模の子挿入を仮想化なしに
処理する構造的な限界がある可能性が高い。深追いはせず、次善の対応として
「この場合だけ別のコントロールを使う」方針に切り替えた。

## 修正2: クラス階層のないフラットな巨大リストは仮想ListViewで表示

`ShouldUseVirtualFlatList()` を新設し、以下の条件を両方満たす場合のみ
`SetTreeJava`(TreeView)でなく `SetListFlatVirtual()`(仮想ListView)を使うように
`SetData()` の分岐を変更した:

- 総関数数が3,000以上
- 「クラス名::メソッド」形式(名前に`::`を含む)の関数の割合が30%以下

判定はヒューリスティックであり、意図は「クラス階層としての価値がほとんど
ない(=ツリーが実質1個の"グローバル"ノードの下に大量の葉が並ぶだけになる)
場合のみ、ツリー表示を諦めてでも仮想化の恩恵を取る」というもの。クラスが
それなりにある実務的なC++ファイルは、比率が閾値を超えるため従来通り
TreeViewのまま(発見2の修正の恩恵をそのまま受ける)。

実装:

- `IDC_LIST_FL`(既存のリストビューコントロール、他のアウトライン種別
  (C言語/PL-SQL/ASM/Perl等)がフラット表示に流用しているのと同じコントロール)
  に対し、`SetListOwnerDataMode()` で `LVS_OWNERDATA` スタイルを動的に
  付け外しする(付け外しはアイテム数0の状態でのみ安全なため、`SetData()`
  冒頭の`ListView_DeleteAllItems`直後に必ず一度falseへ戻してから、
  必要な分岐でのみtrueにする)
- `SetListFlatVirtual()` は `ListView_InsertItem` を1件も呼ばず、
  `ListView_SetItemCountEx()` で件数だけを伝える。行のテキストは
  `OnNotify()` の `LVN_GETDISPINFO` ハンドラで、表示に必要な行が
  スクロールで見えるたびに `m_pcFuncInfoArr` から都度組み立てて返す
- 列幅は仮想リストでは `LVSCW_AUTOSIZE` が全項目走査を前提とし信頼できない
  ため、固定幅にした
- 列ヘッダクリックでの並び替え(`SortListView`)は、仮想リストでは
  `ListView_SortItems` が使えない(`LVS_OWNERDATA`非対応)ため、表示順序を
  保持する `m_vecVirtualListOrder`(`表示インデックス→m_pcFuncInfoArrの
  インデックス`)自体を `std::sort` する形に変更した。比較関数は既存の
  `CompareFunc_Asc`/`CompareFunc_Desc`(元々 `lParam` = 配列インデックスを
  前提にしている)をそのまま再利用できた。ソート前後で選択中の項目が
  物理位置でなく論理的に同じ関数を指し続けるよう、ソート前に選択中の
  `m_pcFuncInfoArr`インデックスを覚えておき、ソート後にその表示位置を
  再度選択する処理を追加している
- `GetData()`(現在選択中の項目取得)も、仮想リストでは `LVIF_PARAM` で
  `lParam` を読んでも値が保持されていないため、`m_vecVirtualListOrder`
  経由でインデックスを求めるよう分岐を追加した

### 修正後の再計測

| ファイル | 修正前(発見3時点) | 修正後 |
|---|---|---|
| flat_10000.cpp | 449-829ms | **31-82ms** |
| flat_50k.cpp | 12,220-27,268ms | **92-196ms** |

クラスが多いファイル(`classes_2000x5.cpp`、閾値に該当せず従来通りTreeView経由)
は1,069-2,373msのままで、回帰していないことも確認した。

## 修正3: 仮想リストが空表示になる不具合の修正

修正2を実機(手動でファイルを開いてEditor.Outlineを実行)で確認したところ、
「C++ メソッドツリー」ダイアログにリストビュー(行・桁・関数名の3列)は
表示されるが、行が1件も描画されない不具合が発生した(マクロによる自動計測
では所要時間としか見ておらず、この見た目上の不具合には気づけていなかった)。

原因は2つ重なっていた。

1. `LVS_OWNERDATA`スタイルはWin32のListViewコントロールにおいて、**コントロール
   生成後に動的に付け外しできない**(comctl32の既知の制約)。当初の実装は
   `SetListOwnerDataMode()`で既存の`IDC_LIST_FL`に対し`SetWindowLongPtr(GWL_STYLE, ...)`
   でこのビットを実行時に立てていたが、`GetWindowLongPtr`で読み返すとビットは
   立っているように見えるものの、コントロール内部は実際には仮想モードとして
   動作しておらず、`ListView_SetItemCountEx`で件数だけは設定されても
   `LVN_GETDISPINFO`が一切発行されないため、行が空のまま表示されていた。
2. `CDlgFuncList::OnNotify()`は関数冒頭で`hwndList = GetDlgItem(GetHwnd(), IDC_LIST_FL)`
   を固定的に取得しており、この値と`pnmh->hwndFrom`を比較して通知を振り分けて
   いた。上記1が仮に解決していたとしても、この比較が常に成立するため
   `LVN_GETDISPINFO`はそもそも処理される経路に乗っていなかった。

対応として、`LVS_OWNERDATA`を**リソース側(`sakura_rc.rc`)で生成時から
付与した専用コントロール`IDC_LIST_FL_VIRTUAL`**を新設し(`IDC_LIST_FL`と全く
同じ位置・サイズに重ねて配置)、実行時のスタイル切替はやめて表示/非表示の
切替のみに変更した。あわせて`CDlgFuncList::GetActiveListHwnd()`
(`m_bVirtualListMode`に応じて`IDC_LIST_FL`か`IDC_LIST_FL_VIRTUAL`のHWNDを返す)
を新設し、`OnNotify`・`GetData`・`SetFocus`・`OnContextMenu`など、どのリスト
コントロールが「今表示されているか」を意識する必要があった全箇所(`OnInitDialog`
での列作成・スタイル設定・フォント設定・`SyncColor`での配色・ドッキング時の
コントロール表示制御を含む)をこのヘルパー経由に統一した。

この過程で、`sakura_core/sakura_rc.rc`が本来Shift-JIS(CP932)エンコーディングの
ファイルであるにもかかわらず、通常の編集操作で一度UTF-8として保存され直し、
ファイル全体の日本語文字列が破損する事故が起きた(結果としてRCコンパイラが
無関係に見える行で`RC2104`エラーを出す形で発覆した)。`git checkout --`で
一旦ファイルを復元した上で、PowerShellから`[System.Text.Encoding]::GetEncoding(932)`
を明示指定してバイト単位で読み書きし、追加したいCONTROL行(純粋なASCII)だけを
挿入することで、既存の日本語文字列を一切破壊せずに済ませた。**この`.rc`
ファイルを今後編集する際は、テキストエディタでの素朴な保存では文字化けする
リスクがあることに注意**(このリポジトリの他の`.rc`/`.h`ファイルはUTF-8(BOM付き)
であることが多く、`sakura_rc.rc`だけが例外的にShift-JISである点も紛らわしい)。

## 修正4(実装したが見送り): 巨大ファイルの解析中の進捗ダイアログ

ユーザーから「500MB級ではさすがに遅いだろうが、プログレスバーは出せるか」
という追加要望を受け、一度実装した。Grep/大量置換が使っている`CDlgCancel`
(`sakura_core/dlg/CDlgCancel.h/.cpp`、`IDD_OPERATIONRUNNING`)方式(同期ループ内で
100ms間隔ごとに`BlockingHook()`を呼びメッセージポンプを回す、協調的マルチタスク)
を`CDocOutline::MakeFuncList_C`(`CType_Cpp.cpp`)に組み込み、10万行超のファイルで
進捗表示とキャンセルを可能にした。`gen_flat.js`で生成した500,000関数/350万行の
ファイルに対し、`EnumWindows`+`BM_CLICK`でキャンセルボタンを自動クリックする
検証スクリプト(`_dbg/outline_bench/click_cancel.ps1`)を使い、通常1,200〜1,500ms
かかる解析を57msで中断できること、中断後も後続の解析が正常に動くことを実機で
確認済みだった。

しかしこの実装は`OUTLINE_C`/`OUTLINE_C_CPP`/`OUTLINE_CPP`(`MakeFuncList_C`)にしか
適用しておらず、`MakeFuncList_Java`/`MakeFuncList_Perl`/`MakeFuncList_python`など
言語別に分かれた他の解析関数には手を付けていなかった。ユーザーから
「型ごとに処理を組み込む必要があるならプログレスバーは無しでよい」との判断が
あり、C/C++型だけ特別扱いする非対称な実装を残すよりは全体を削除する方針とした。
`CDocOutline::MakeFuncList_C`の戻り値は`void`に戻し、`CViewCommander_Outline.cpp`
のキャンセル処理も削除済み。将来、全アウトライン種別に横断的な仕組みとして
再検討する場合の参考として、実装内容と実機確認の結果はこの節に残しておく。

## 修正5: クラスが多いファイルの「2回目以降の開閉が遅い」問題

ユーザーから「2回目以降開くまでが遅くなる。閉じる処理も遅くなっている?」と
報告があった。修正3(ハッシュマップ化)後の実測(`classes_2000x5.cpp`、
2,000クラス/10,000関数)を見返すと、確かに1回目1,030ms前後に対し2回目以降は
2,600〜3,000msへ跳ね上がっていた。原因調査のため`SetData()`内の
`ListView_DeleteAllItems`/`TreeView_DeleteAllItems`の前後に一時的に
`QueryPerformanceCounter`を仕込んで計測した(計測後に削除済み)。

結果、`ListView_DeleteAllItems`(未使用の`IDC_LIST_FL`に対して、常に空)は
0.1〜0.3msと無視できる一方、`TreeView_DeleteAllItems`が**1回目は0ms
(まだ何も入っていないため)、2回目以降は約1,270〜1,320ms**と判明した。つまり
「前回の解析結果(約12,000ノード: 2,000クラスノード+10,000メソッドノード)を
消す処理」自体が支配的コストだった。1回目が速く見えていたのは、単に
「消すものがまだ何もなかったから」に過ぎない。

`TreeView_InsertItem`と対称的に、Win32のTreeViewは大量ノードの一括削除も
遅い。念のため「`TreeView_DeleteAllItems`の代わりに`DestroyWindow`して
`CreateWindowEx`で新しいTreeViewを作り直す」という一般的な高速化手法も
実装して試したが、**むしろ悪化した**(削除相当の処理が1,410〜1,640msへ、
合計は4,000ms超へ増加)。ウィンドウ破棄そのものも内部的に同じ
`TVN_DELETEITEM`通知を全ノード分発行する経路を通るためと考えられる。この
アプローチはコミットせず元に戻した。

WM_SETREDRAWによる再描画抑制は既に効いている(2回目以降の遅さは再描画では
なく、ノードごとの内部的な後始末コストそのもの)ため、根本的な高速化手段が
見当たらなかった。

### 最初の対応(仮想リストへのフォールバック)とユーザーからの再指摘

最初は修正2と同じ考え方で、「クラス階層があってもノード数が多すぎるファイルは
仮想ListViewにフォールバックする」方向で対応した。`ShouldUseVirtualFlatList()`に、
メンバ比率に関係なく関数数が8,000を超えたら無条件で仮想リストを使う閾値
(`VIRTUAL_LIST_FORCE_FUNCS`)を追加し、`classes_2000x5.cpp`が205ms/69ms/54msまで
改善することを確認した。

しかしユーザーから「ツリー表示をフラットにしちゃうのはよくないのでは?」と
指摘があった。これはもっともで、2,000クラスのような正当なファイル構成に対して
「遅いので折りたたみ/展開機能を諦める」対症療法になっていた。この閾値は撤去し、
以下のダブルバッファリング方式に差し替えた。

### 最終対応: TreeViewのダブルバッファリング+非同期後始末

遅いのは「前回のツリー内容を消す」処理であり、「新しいツリーを表示する」処理
自体は修正1(ハッシュマップ化)で既に高速だった、という点に着目した。

- 前回のノード数(`m_nTreeItemCount`)が500を超える場合のみ、`SetData()`で
  `TreeView_DeleteAllItems`する代わりに、新しい`SysTreeView32`ウィンドウを
  `CreateWindowEx`で動的生成する(位置・サイズ・スタイル・フォントは古い方から
  複写)。
- 新しい方に`SetWindowLongPtr(GWLP_ID, IDC_TREE_FL)`でダイアログアイテムIDを
  与え、古い方は`IDC_TREE_FL_CLEANUP_SCRATCH`という未使用のスクラッチIDへ
  退避する。これにより`SetTreeJava`/`SetTree`/`SetTreeFile`をはじめ、
  `GetDlgItem(GetHwnd(), IDC_TREE_FL)`で探索している既存コードは一切変更せずに
  そのまま新しい方を見つけられる(IDC_LIST_FL_VIRTUALのときのような多数の
  呼び出し箇所の洗い出しが不要になった)。
- 新しいツリーへの構築(`SetTreeJava`等)は今まで通り同期的に行われるが、
  修正1のおかげで数千〜1万ノード程度なら数十〜数百msで終わる。
- 古い方(実データが残ったまま)の後始末(`TreeView_DeleteAllItems`+
  `DestroyWindow`)は、新しいメッセージ`WM_APP_OUTLINE_CLEANUP_TREE`
  (`WM_APP + 2504`)を`PostMessage`で送っておき、`CDlgFuncList::DispatchEvent`で
  非同期に処理する。`PostMessage`はキューに積むだけで即座に戻るため、
  `SetData()`はユーザーに新しい内容を見せるところまで高速に完了できる。
  古い方の実際の削除コストは変わらない(依然として同じノード数に比例した
  時間がかかる)が、それが発生するのはダイアログが新しい内容を表示し終えて
  アプリがメッセージループのアイドル状態に戻ってから。

念のため、`DestroyWindow`で直接作り直す(削除を待たずに単純に置き換える)
手法も試したが、**むしろ悪化した**(削除相当の処理が1,410〜1,640msへ、合計は
4,000ms超へ増加)。ウィンドウ破棄そのものも内部的に同じ`TVN_DELETEITEM`通知を
全ノード分発行する経路を通るためと考えられる。この結果が「非同期化しないと
意味がない」ことの裏付けにもなった。

#### 動作確認

`bench_outline.js`(3回連続で`Editor.Outline(1)`を呼ぶ)で計測したところ、
`classes_2000x5.cpp`は1,057ms/2,072ms/2,122msとなり、**数値上は改善して
いないように見える**。これは計測方法の限界であって回帰ではない: マクロの
各文の間ではWindowsメッセージポンプが回らないため(`Editor.Sleep()`は
生の`::Sleep()`を呼ぶだけでメッセージポンプを回さず、`WScript.Sleep`は
このマクロ実行系(QuickJSベース、`NKMM_FIX_QUICKJS_MACRO`)に存在せず
例外になる。実際に`WScript.Sleep`ありのテストスクリプトを書いたところ、
例外がスクリプトエラーダイアログとして表示され続けてプロセスが応答不能に
見える状態になった。これはプロダクトコードのハングではなくテストスクリプトの
不備で、`WScript`オブジェクト自体が未定義であることを別の最小スクリプトで
確認済み)、`PostMessage`で積んだ後始末が一度も処理されないまま次の
`Editor.Outline()`呼び出しに突入してしまう。

これを検証するため、`ExitAll()`を呼ばずに3回`Outline(1)`だけ呼んで終了する
マクロを使い、スクリプトが終わってアプリが通常のアイドル状態に戻った直後の
状態を確認した(一時的に`posted`/`processed`と`GetTickCount64`をCSVへ書き出す
ログを`SetData()`と`DispatchEvent`に仕込み、計測後に削除済み)。
結果、2件の`posted`エントリに対応する`processed`エントリが、スクリプト終了後
数秒以内に確実に記録された。つまり**非同期後始末の仕組み自体は正しく機能して
おり、マクロによる背中合わせの3連続呼び出しという計測方法自体が、実際の
対話的な操作(1回reloadしてから次のreloadまで人間の操作時間が入る)を
再現できていない**、というのが正しい結論。実際の対話操作では、reload後に
アプリがWindowsメッセージループへ戻った時点(ボタンクリック等のハンドラが
returnした直後)でほぼ即座に後始末が走り始めるため、体感としては「新しい
ツリーはすぐ表示されるが、その後1秒前後操作を受け付けない瞬間がある」という
形になり、「新しい内容が出るまで完全に固まる」という元の問題よりは改善される
はずである(このタイミングの詳細な体感計測は今回は行っていない)。

実機のスクリーンショットで、`classes_2000x5.cpp`が(仮想リストへフォール
バックせず)引き続きツリー表示(クラスごとの折りたたみ/展開)のまま
正しく描画されることも確認済み。

## 修正6: 実ファイル(50MB/41万行の小説テキスト)で発覚した2つの見落とし

ここまでの検証は全て合成した`.cpp`ファイルで行っていたが、ユーザーから実際に
存在するテスト用ファイル`Release64\dummy_novel_50mb.txt`(50MB、418,770行、
日本語の小説調テキスト)で試すよう指示があった。この種別は`OUTLINE_TEXT`
(プレーンテキストのトピック一覧)で、`SetTreeJava`ではなく別の関数
`SetTree()`を使う。実行したところ、1回目は1,043msと速かったが、**2回目以降が
29,000ms前後(約29秒)** という、修正3以前より悪いレベルの重さで再発した。

原因は2つ、いずれも「`SetTreeJava`専用に書いた修正が、実は`SetTree`という
別の関数を使う他の全種別(プレーンテキスト・HTML・TeX・ルールファイル・
WZTXT・XML・Python・汎用ツリー等)には適用されていなかった」という見落とし
だった。

1. **`m_nTreeItemCount`が`SetTreeJava`でしか更新されていなかった**。修正5の
   ダブルバッファリングは`if( m_nTreeItemCount > TREE_RECREATE_THRESHOLD )`
   で発火するが、`SetTree()`はこの変数を一度も書き換えないため、常に0のまま
   ("SetTreeJavaを使う前回の値が残っているか、初期値の0")で、判定が
   絶対に成立しなかった。つまり`SetTree()`系の全種別は、修正5を実装した
   つもりでも実際には一切適用されず、`TreeView_DeleteAllItems`の素のコストを
   丸ごと受け続けていた。`SetTree()`の末尾に`SetTreeJava`と同様
   `m_nTreeItemCount = nFuncInfoArrNum;`を追加して修正。
2. **1を直しただけでは直らず、むしろ悪化した(29秒→29,931ms、誤差レベルで
   ほぼ変わらず)**。一時的な`QueryPerformanceCounter`計装(`SetTree`/
   `SortTree`/`SetDocLineFuncList`/`SetData`全体/ダブルバッファリング部分、
   すべて計測後に削除済み)で切り分けたところ、ダブルバッファリング部分
   (新しいTreeView生成)自体は2〜5ms未満と高速、`SetTree`本体も131〜209ms、
   `SortTree`も383〜479msといずれも高速なのに、`SetData()`全体としては
   依然29秒近くかかるという矛盾した結果が出た。原因は、新しく
   `CreateWindowEx`で作成したTreeViewに対して`WM_SETREDRAW(FALSE)`を
   一切適用していなかったこと。`SetData()`冒頭の`WM_SETREDRAW(FALSE)`は
   **古い(まもなく破棄される)ツリーのハンドルに対して**行われており、
   ダブルバッファリングで新規生成した別ウィンドウには何も効いていなかった。
   本来「非表示中でも内部の再描画関連処理は動くためWM_SETREDRAWで止める」
   という修正2由来の教訓([修正2](#修正2-クラス階層のないフラットな巨大
   リストは仮想listviewで表示)参照)を、自分自身の新しいコードに適用し
   忘れていた形。`CreateWindowEx`直後に`::SendMessage(hwndNewTree,
   WM_SETREDRAW, FALSE, 0)`を追加(終了時の`WM_SETREDRAW(TRUE)`は
   `hwndTree`変数を新しいウィンドウに差し替え済みのため対称に効く)。

再計測(`dummy_novel_50mb.txt`、55,836トピック):

| 回 | 修正6前(m_nTreeItemCount未更新) | m_nTreeItemCountのみ修正 | WM_SETREDRAWも修正(最終) |
|---|---|---|---|
| 1回目 | 1,050ms | 1,043ms | 1,056ms |
| 2回目 | 21,975ms | 29,058ms | **1,028ms** |
| 3回目 | 22,698ms | 29,239ms | **970ms** |

`m_nTreeItemCount`だけ直した中間状態がむしろ悪化したのは偶然ではなく、
「ダブルバッファリングが発火するようになった分、WM_SETREDRAWなしで
55,836件を構築するコストがそのまま乗った」ため。両方直して初めて
本来の効果(数十秒→1秒前後)が得られた。

この修正の後、既存の全合成テストファイル(`flat_2500`/`flat_10000`/
`flat_50k`/`classes_500x5`/`classes_2000x5`)も再計測し、いずれも1回目と
2回目以降で有意差がない(数十〜数百ms台で安定)ことを確認した。特に
`classes_500x5.cpp`は修正5時点では293ms→565ms→567msと変動があったが、
本修正後は282ms→237ms→244msとほぼ一定になった(このファイルもダブル
バッファリング発火対象だったため、同じWM_SETREDRAW漏れの影響を受けていた)。
実ファイルのスクリーンショットでもテキストトピックツリーが正しく描画される
ことを確認済み。

**教訓**: `SetTreeJava`のみを対象に検証・計測して「直った」と判断したが、
実際には同じ`CDlgFuncList`の中で構造的によく似た別の関数(`SetTree`)が
存在し、修正が横展開されていなかった。合成テストファイルは常にC/C++の
`SetTreeJava`経路を通るため、`SetTree`経路のバグには最後まで気づけなかった。
ユーザーが指定した実ファイルでのテストがなければ、この2つのバグは
見つからないまま残っていた可能性が高い。

## 修正7: 「閉じた後にビジー」「トグルで閉じない」の2件

修正6の後、ユーザーから2つの追加報告があった。「2回目以降開くまでが遅くなる。
閉じる処理も遅くなっている?」「解析ダイアログを閉じた後にビジー状態になる」。

### (a) ダイアログを閉じる操作自体が重い

修正5のダブルバッファリングは「新しい内容を表示するまで」の体感速度は
改善したが、ダイアログを**閉じる**とき(`WM_CLOSE`→`DestroyWindow`→
子ウィンドウの通常のカスケード破棄)には無関係だった。大量ノードを持つ
TreeViewがダイアログの子ウィンドウのままだと、破棄カスケードの中で
`TreeView_DeleteAllItems`相当のコスト(ノード数に比例、修正5参照)を
そのまま食う。`CDlgFuncList::OnDestroy()`を修正し、`m_nTreeItemCount > 500`
の場合は`SetParent(hwndTree, NULL)`でTreeViewを先にダイアログから切り離し、
最小限の自己破棄用ウィンドウプロシージャ(`OutlineOrphanTreeWndProc`、
`SetWindowLongPtr(GWLP_WNDPROC,...)`で差し替え)に付け替えた上で
`PostMessage(WM_APP_OUTLINE_CLEANUP_TREE)`により後始末を非同期化した。
ダイアログ自体はTreeViewの中身を待たずに閉じる。

### (b) F11(トグル)で閉じない/常に再解析扱いになる

`CViewCommander::Command_FUNCLIST()`のSHOW_NORMAL/SHOW_TOGGLE分岐は、
呼び出し時点の`nOutlineType`とダイアログが保持する`m_nOutlineType`を
`CheckListType()`(単純な`==`比較)で照合し、一致すれば「同じ種別なので
アクティブ化/閉じるだけ」、不一致なら「種別が違うので再解析」という
経路を取る。ところが`.cpp`ファイルのように`m_eDefaultOutline`が
`OUTLINE_C_CPP`(C/C++自動判別、未解決の値)である場合、この時点の
`nOutlineType`は常に`OUTLINE_C_CPP`のまま比較に使われる。一方
`m_nOutlineType`側は、開いた時に`MakeFuncList_C`が拡張子から実際に
解決した具体的な値(`OUTLINE_C`または`OUTLINE_CPP`)が保存されている。
そのため同じファイル・同じ種別で開き直しても`OUTLINE_C_CPP != OUTLINE_CPP`
で必ず不一致になり、「型が違うので再解析」の経路に落ちてしまい、
`SHOW_TOGGLE`(F11で閉じる)を押してもダイアログが閉じず、代わりに
毎回フルの再解析(`MakeFuncList_C`+ツリー再構築)が走っていた。実機の
デバッグトレースで`nOutlineType=1(OUTLINE_C_CPP)`に対し
`CheckListType=0`(不一致)になっていることを確認して特定した。

修正: `MakeFuncList_C`内にあった拡張子→具体型の解決ロジックを
`CDocOutline::ResolveOutlineType_C_CPP(nOutlineType, pszFileName)`として
切り出し、`Command_FUNCLIST()`冒頭(`OUTLINE_DEFAULT`の解決直後、
SHOW_NORMAL/SHOW_TOGGLE判定の直前)でも同じ解決を先に行うようにした。
この関数は`OUTLINE_C_CPP`以外の値には何もしない(素通し)ため、C/C++
以外の種別(プレーンテキスト、Java、Pythonなど)には影響しない。

### 動作確認

`classes_2000x5.cpp`で`Editor.Outline(1)`(開く)→`Editor.Outline(2)`
(SHOW_TOGGLEで閉じる)を計測:

| 項目 | 修正前 | 修正後 |
|---|---|---|
| 開く | 1,033ms | 1,055ms |
| 閉じる | (再解析扱いで実質フルコスト。修正前は`CheckListType=0`が実機トレースで確認済み) | **0ms**(即座に閉じる) |

あわせて以下も確認し、既存動作への副作用がないことを確認した:

- `Editor.Outline(0)`(SHOW_NORMAL)を同一ファイル・同一種別で連続2回
  呼んでも即座にアクティブ化されるのみ(3〜5ms、再解析は発生しない)。
- `.c`ファイル単体では`ResolveOutlineType_C_CPP`が`OUTLINE_C`(値0)に、
  `.cpp`ファイルでは`OUTLINE_CPP`(値16)に解決され、拡張子ごとに異なる
  具体値へ正しく分岐することを実機トレースで確認(異なる種別間では
  `CheckListType`が確実に不一致となり再解析経路が維持されることの根拠)。
- 既存の全合成テストファイル(`flat_2500`/`flat_10000`/`flat_50k`/
  `classes_500x5`/`classes_2000x5`)と実ファイル(`dummy_novel_50mb.txt`)で
  開く→閉じるの回帰確認を行い、閉じる時間はいずれも0〜1msで安定、
  プロセスも正常終了することを確認した。

sakura_core\doc\CDocOutline.h, sakura_core\types\CType_Cpp.cpp
(`ResolveOutlineType_C_CPP`), sakura_core\cmd\CViewCommander_Outline.cpp,
sakura_core\outline\CDlgFuncList.cpp(`OnDestroy`, `OutlineOrphanTreeWndProc`)

### 補足: `SetTreeJava()`のNKMMフラグ区切り漏れの是正

上記とは別に、修正1(`SetTreeJava()`のクラスノード探索ハッシュマップ化)を
見直したところ、ハッシュマップ/ラムダ(`mapClassNode`/`mapLastChild`/
`InsertChildFast`)の**宣言**は`#ifdef NKMM_FIX_OUTLINE`で囲われていたが、
それを使う**本体ロジック**(クラスノードの検索・生成、グローバルノード生成、
メソッドノード生成の3箇所)は無条件のまま書き換えられており、置き換え前の
`TreeView_GetNextSibling`+`TreeView_GetItemTextVector`による逐次探索コードは
`#else`に退避されず削除されていたことが判明した。

このファイル内の他の全ての変更は`#ifdef NKMM_FIX_OUTLINE`を未定義にすれば
元の実装に戻せる(トグル可能)よう書かれているが、この3箇所だけは
`NKMM_FIX_OUTLINE`を未定義にすると`mapClassNode`等が未定義識別子となり
**コンパイルエラーになる**という非対称な状態だった。

該当3箇所を、他の対応と同じ`#ifdef NKMM_FIX_OUTLINE / #else <元の実装> /
#endif`の形に修正(`TV_ITEM tvi;`宣言の復元含む)。`my_config.h`の
`#define NKMM_FIX_OUTLINE`を一時的にコメントアウトしてソリューション全体を
再ビルドし、エラーなくコンパイルできる(=フラグオフで元のO(n)実装に
正しく戻る)ことを確認した上でフラグを戻し、`classes_2000x5.cpp`で
性能に回帰がないこと(開く1,159ms、閉じる0ms)も再確認した。

sakura_core\outline\CDlgFuncList.cpp(`SetTreeJava`)

## 既知の制限・未検証事項

- `ShouldUseVirtualFlatList()` の閾値(3,000関数, メンバ比率30%)は経験則であり、
  「クラスが少数だが関数がとても多い」中間的なファイルでは、TreeView側に
  倒れて再びフリーズする可能性がある。より精密な判定(例: 実際に
  `htiGlobal`の子になる件数を先に数える等)は未実装。
- 仮想リストの列ヘッダクリックによる並び替え・キーボード操作・ダブル
  クリックでのタグジャンプは、マクロによる自動計測では検証できておらず、
  コードレビューのみに基づく。実機でのマウス操作による確認は未実施。
- `CFuncInfoArr::AppendData` の1件ずつ`malloc`/`realloc`(容量倍加なし)は、
  今回計測した範囲では支配的コストとして現れなかったため未対応のまま
  残している。将来、さらに大きい規模(数十万関数など)で問題になる場合は
  再検討が必要。
- `TREE_RECREATE_THRESHOLD`(前回ノード数500)を超えるとダブルバッファリング+
  非同期後始末の対象になるため、`classes_500x5.cpp`(3,000ノード)のような
  中規模ファイルも含め、ほぼ全てのクラス階層ファイルが対象になる想定。ただし
  「実際の対話操作での体感速度が改善している」ことの直接計測(マクロでは
  非同期部分を検証できないため、上述の通り間接的な確認に留まる)は未実施。
- `SetTreeJava`側で残っている `TreeView_Expand`(クラス数が非常に多い場合の
  展開コスト、`classes_2000x5.cpp`で約0.7〜0.8秒)は未対応。この展開処理は
  ダブルバッファリングの対象(新しいツリーへの構築、同期的)に含まれるため、
  引き続き新しい内容を表示するまでの時間に乗る。
- ダブルバッファリング中に、古いツリー(スクラッチID)の後始末が完了する前に
  同じ理由で3回目以降の作り直しが連続すると、複数の孤立したTreeViewウィンドウ
  (それぞれ`WM_APP_OUTLINE_CLEANUP_TREE`待ち)が一時的に共存する。理論上は
  各自が正しく後始末されるはずだが、非常に高頻度で連続reloadした場合の
  実機検証は行っていない。
- **未対応(既知の限界): `SetTree()`系種別(プレーンテキスト/HTML/TeX/
  WZTXT/XML/Python/ルールファイル/汎用ツリー等)は、フラットで件数が
  極端に多い場合のTreeView劣化に未対応**。実ファイル`dummy_novel_500mb.txt`
  (500MB、517万行)で検証したところ、約69万件(「」で始まる会話文の行を
  見出しとして検出、ほぼ全て同一深さのフラット構造)のトピックとなり、
  Outline(1)呼び出しが10分以上・CPU時間900秒超を消費してもメッセージ
  応答すら返さない状態を実機で確認した(プロセスをkillして確認)。
  原因は`SetTree()`自体ではなく(2016年の既存修正で`TVI_FIRST`挿入済み、
  ここはO(n)で問題ない)、その後`SetData()`から呼ばれる`SortTree()`→
  `TreeView_SortChildrenCB()`が、TVI_ROOT直下の約69万件の兄弟ノードを
  一括ソートする箇所にあると推測している(comctl32のTreeViewは子要素を
  連結リストで保持しており、`TreeView_SortChildrenCB`は非常に多い兄弟数
  では劣化することが知られている)。これは今回C/C++のフラットな巨大
  関数リストに対して行った対応(TreeViewでなくLVS_OWNERDATAの仮想リスト
  `SetListFlatVirtual`に逃がす)と同種の、Win32 TreeViewの構造的限界に
  `SetTree()`側でも達しているケースと考えられる。修正に必要な仮想リストの
  基盤(`IDC_LIST_FL_VIRTUAL`/`SetListFlatVirtual`/`GetActiveListHwnd`)は
  既に存在するため理論上は同じ手法で対応可能だが、対象が`SetTree()`を
  使う全種別に及び、種別ごとの検証コストが増えるため今回は見送り、
  既知の限界として記録するに留めた。500MB・数十万トピック級のような
  極端なケースでのみ顕在化する問題であり、これまで確認した範囲
  (50MB/55,836トピック以下)では問題ない。
