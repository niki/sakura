# 次を検索(F3・検索ダイアログのインクリメンタル検索)の非同期化 20260809-20260810

対象フラグ: `NKMM_FIX_ASYNC_SEARCH_NEXT`(新規)。

対象ファイル:
- `sakura_core/CSearchAgent.h` / `.cpp`
- `sakura_core/view/CEditView.h` / `.cpp`
- `sakura_core/cmd/CViewCommander.h`, `CViewCommander_Search.cpp`
- `sakura_core/view/CEditView_Command_New.cpp`

---

## 背景

デバウンス([NKMM_FIX_FIND_DIALOG_DEBOUNCE.md](NKMM_FIX_FIND_DIALOG_DEBOUNCE.md)参照)
だけでは、入力が止まるたびに走る「文書全体を線形走査して見つからないと判定する」
1回分(数百万行規模のファイルで実測150〜200ms)がUIスレッドをブロックし続けていた。
実機で500万行ファイルにて確認済み。

## 実装

`CSearchAgent::SearchWord()`の走査ループに中断フラグを追加し(同期呼び出しは
`nullptr`を渡せば従来通り)、`Command_SEARCH_NEXT()`の「検索開始位置の調整(選択中
テキストがある場合の特殊処理)」を伴わない単純なケース(選択なし・
`pcSelectLogic==NULL`・すべて置換実行中でない・正規表現でない・文書が一定行数を
超える)に限り、`CEditView::AsyncFindNext`でバックグラウンドスレッドに検索を回し、
結果が出たら`WM_APP_ASYNC_SEARCH_DONE`で戻してカーソル移動などのUI反映を行う。
上記以外(選択中の検索開始・すべて置換・正規表現・小さいファイル)は既存の同期
パスをそのまま使う(挙動変更なし)。

文書変更(タイプ入力/貼り付け/削除/Undo/Redo/すべて置換)との競合防止のため、
唯一に近い合流点である`CEditView::ReplaceData_CEditView3`の先頭で、実行中の検索
スレッドを中断・待機してから編集を進める(`ScrBarMarker`の`WaitForBuild(true)`と
同じ考え方)。検索スレッドが参照する検索パターン文字列・オプションは共有メンバを
直接参照せず、リクエスト時に独立コピーを作って渡す
(`CSearchStringPattern::SetPattern()`はポインタを保持するだけでコピーしないため、
共有バッファを渡すと次のキー入力で解放/書き換えされうる)。

`WM_APP_ASYNC_SEARCH_DONE`、非同期化する行数しきい値
`NKMM_ASYNC_SEARCH_NEXT_LINE_THRESHOLD`(既定200,000行)は`my_config.h`で定義。

## 既知の制約(修正保留・要検討): マクロからSearchNext()直後に結果を読めない

新旧2.3.2.0の検索速度をマクロ(WSH/JScript、`bench_search.js`)で比較しようとした
ところ、5,174,307行/500MBファイルで`Editor.SearchNext()`呼び出し直後に
`Editor.GetSelectLineFrom()`を読んでも常に0(未検出)が返ることが判明した。

原因: 上記の非同期化は`F_SEARCH_NEXT`自体(F3キー・マクロ問わず同じ
`Command_SEARCH_NEXT`経路)に掛かっているため、マクロから呼んでも対象が
`AsyncFindNext::Request`に回されてすぐ`return`する。マクロのJScript実行はUIスレッド
上で完全に同期的に進み、文の合間でメッセージポンプが回らない(`CWSH.cpp`内の
`PeekMessage`/`DispatchMessage`は「マクロ強制終了確認ダイアログ」監視用の別スレッド
にしかなく、通常のマクロ実行では出番がない)ため、バックグラウンドスレッド完了時に
飛ぶ`WM_APP_ASYNC_SEARCH_DONE`がマクロ実行中には一切ディスパッチされず、選択範囲
(カーソル移動)への反映がマクロから見えない。

影響: 20万行(`NKMM_ASYNC_SEARCH_NEXT_LINE_THRESHOLD`)を超える文書に対して
`SearchNext()`を呼び、直後に選択位置/ヒット文字列を読むマクロは、このフォークでは
検索結果を取得できなくなる(手元のF3操作は非同期完了後に正しく反映されるため
無症状)。対応する場合は`WM_APP_ASYNC_SEARCH_DONE`受信までブロックする同期版マクロ
API(例: `Editor.WaitForSearchNext()`相当)の追加を要検討。

## 追記: 速度比較の再測定と結論の訂正

上記調査中、真の検索完了時間(バックグラウンドスレッド内の`SearchWord()`実測)を
計測するため、`AsyncFindNextThreadProc`内に`QueryPerformanceCounter`で計測し
`bench_async_core_times.csv`へ書き出す一時的な計測コードを追加して測定した
(測定後に削除済み)。

初回の結果: 公式2.3.2.0(x86)がマクロ計測(同期)で230ms、フォーク(x64/x86)が実測
ネット検索時間で約110〜165ms、「検索アルゴリズム自体が1.5〜2倍速くなった」と結論
しかけたが、`NKMM_USE_MIMALLOC`/`NKMM_USE_MIMALLOC_OVERRIDE`を一時的に無効化して
同条件で再測定したところ約256ms(x64, n=10)まで悪化し、公式ビルドと同等かむしろ
遅い水準に戻った。

結論を訂正: `CSearchAgent::SearchWord()`の走査ループ自体
(`SearchString`/`CDocLine::GetDocLineStrWithEOL`/`GetNextLine`)は検索中に一切
ヒープ確保しておらず(スキップテーブルは`Request()`側でスレッド起動前に1回だけ
構築済み)、フォークと公式とで検索コード自体は実質同一。したがって観測された
速度差の大部分は「非同期化」でも「アルゴリズム改善」でもなく、5,174,307行分の
`CDocLine`/文字列バッファを読み込み時にどのアロケータ(mimalloc vs 既定のCRT
ヒープ)が確保したかによるメモリレイアウト(キャッシュ局所性)の差だった可能性が
高い。非同期化そのものの効果は「呼び出し元(UI/マクロ)を即座に解放する」ことに
限られ、走査自体の速さにはほぼ寄与していない。

なお公式2.3.2.0を同一ファイルで2回計測(230ms→235ms)しOSファイルキャッシュの
ウォームアップでは説明できないことは確認済み(`Editor.GoFileTop()`以降だけを計測
しており、その時点でファイルは既にドキュメント構造へ読み込み完了済みのため、
計測区間はディスクI/Oを経由しない)。

## 動作確認について

500万行ファイルでF3の非同期化・UIブロック解消を実機確認済み。マクロからの
`SearchNext()`直後の結果取得は既知の制約として上記の通り未対応。
