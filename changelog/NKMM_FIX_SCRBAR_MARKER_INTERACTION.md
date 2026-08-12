# スクロールバーマーカー: 競合修正・並列化・インタラクション機能・CPU対策 20260809-20260810

対象フラグ: `NKMM_FIX_EDITVIEW_SCRBAR`(既存)配下の実装変更・サブオプション追加。

対象ファイル(主なもの):
- `sakura_core/view/CEditView.h` / `CEditView.cpp`(`ScrBarMarker`, `SB_Marker_*`,
  `_SB_Marker_HitTestAndJump`, `_SB_Marker_HoverRedraw`)
- `sakura_core/view/CEditView_Scroll.cpp`(`VScrollBarWndProc`)
- `sakura_core/view/CEditView_Command.cpp`(`ChangeCurRegexp`)
- `sakura_core/view/CEditView_Command_New.cpp`(`ReplaceData_CEditView3`)
- `sakura_core/uiparts/CGraphics.h` / `.cpp`(`AlphaBlendMyRect`)

`NKMM_FIX_EDITVIEW_SCRBAR_HWIDTH_SKIP.md`(水平スクロールバー更新漏れ)、
`NKMM_FIX_EDITVIEW_SCRBAR_THREADPOOL.md`(スレッドプール化・mutex導入)、
`NKMM_FIX_SCRBAR_MARKER_REPLACEALL_PERF.md`(ReplaceAllが時々極端に遅くなる別件の
調査)とは別の一連の変更をまとめたもの。

---

## SB_Marker_BuildThreadとChangeCurRegexp()の競合クラッシュ修正 20260809

`SB_Marker_BuildThread`(バックグラウンドの検索ヒット行スキャン)は`IsFoundLine()`
経由で`CEditView::m_sSearchPattern`(Sunday-Quickスキップ配列を保持)をバック
グラウンドスレッドから読む。検索文字列入力のたびに`ChangeCurRegexp()`がUIスレッド
で`m_sSearchPattern.SetPattern()`を呼び、内部で`Reset()`して配列を解放してから
再構築するため、この2つが競合するとNULL/解放済みポインタを読んでクラッシュする。

500万行ファイルへのインクリメンタル検索(`NKMM_FIX_FIND_DIALOG`のデバウンス化で
連続検索が速くなったことで発生頻度が上がった)で実機クラッシュを確認・WinDbgの
ダンプ解析で原因特定済み(`CSearchAgent::SearchString`でのアクセス違反、スタックは
`SB_Marker_BuildThread`→`IsFoundLine`→`IsSearchString`→`SearchString`)。

`ChangeCurRegexp()`が`m_sSearchPattern`を書き換える直前に
`SBMarker_->WaitForBuild(true)`で実行中のビルドスレッドを中断・待機するよう修正
(ReplaceAll前の`CSuppressSrchKeyMarkForReplaceAll`と同じ考え方)。

対象: `sakura_core/view/CEditView_Command.cpp`(`ChangeCurRegexp`)

## WM_APP_SCRBAR_PAINTハンドラのブロッキング待ちを解消 20260809

上記の競合修正でビルドスレッドの再始動頻度が上がったことで、ビルド中にスクロール
すると、スクロールが発生させる描画要求のたびに`SBMarker_->WaitForBuild(false)`
(ビルド完了までブロック)に引っかかり、スクロールバー(実質画面全体)の更新が止まって
見える不具合が顕在化した。実機確認済み。ビルド中はこの描画要求を待たずに諦めるよう
変更。ビルド完了時にスレッド自身が`SB_Marker_DrawRequest()`で再描画を要求してくる
ので、それに任せれば十分(そもそもブロックして待つ必要がない設計だった)。

対象: `sakura_core/view/CEditView.cpp`(`WM_APP_SCRBAR_PAINT`ハンドラ)

## 通常編集とSB_Marker_BuildThreadの競合防止・ビルドの並列化 20260809

`CSuppressSrchKeyMarkForReplaceAll`はReplaceAll専用のガードで、通常のタイプ入力・
貼り付け・削除中に`SB_Marker_BuildThread`が巨大ファイルを走査していた場合の競合
(`CDocLineMgr`への読み取り中の書き換え)は未対策だった。編集の唯一の合流点
`CEditView::ReplaceData_CEditView3`の先頭で`SBMarker_->WaitForBuild(true)`を呼び、
編集前に確実に待避させるよう修正。

あわせて、「折り返しなし」かつ正規表現でない場合に限り、`SB_Marker_BuildThread`の
走査ループをOpenMPで並列化(`SB_Marker_DrawThread`と同じ手法)。折り返しありは
`LogicToLayout()`の共有ヒントキャッシュ、正規表現は`CEditView::m_CurRegexp`という
単一の共有エンジンインスタンスがあり、どちらも複数スレッドから同時に触ると競合する
ため対象外(既存の逐次パスにフォールバック)。500万行ファイルでのスクロールバー
マーク再構築(検索文字列入力のたびに再始動する)を高速化する目的。

対象: `sakura_core/view/CEditView_Command_New.cpp`(`ReplaceData_CEditView3`),
`sakura_core/view/CEditView.cpp`(`SB_Marker_BuildThread`)

## SB_Marker_DrawThreadで同じY座標への重複描画をスキップ 20260809

スクロールバーの高さはせいぜい数百〜千数百pxしかないのに対し、ヒット数が数万〜
数十万件(500万行ファイルでよくある文字列を検索した場合等)になると、大半のヒットが
直前と同じピクセル行へ描画することになり、GDIの`FillRect`呼び出しが無駄に大量発生
して描画が極端に遅くなっていた。実機確認済み。行番号順に処理しているためY座標は
ほぼ単調増加であり、直前と同じYなら描画をスキップするよう変更(見た目の結果は
同じで、無駄な描画回数だけ減る)。

対象: `sakura_core/view/CEditView.cpp`(`SB_Marker_DrawThread`)

---

## スクロールバー上の検索/ブックマークのマークをクリックしたら該当行へジャンプ 20260810

`VScrollBarWndProc`(既存のホバー再描画フック)で`WM_LBUTTONDOWN`を追加で捕捉。
クリックYに最も近いマーク行(検索/ブックマーク、許容誤差数px)があれば
`CEditView::_SB_Marker_HitTestAndJump()`経由で`MoveCursorSelecting()`によりその行へ
ジャンプし、メッセージを消費する(既定のスクロールバー動作=ページスクロール/サム
位置ジャンプは行わない)。マークが無い位置のクリックは従来通り`CallWindowProc()`へ
素通しし、既定動作に任せる。

Y座標→レイアウト行の変換は`SB_Marker_DrawThread`(`DrawWorkCallback`)の
`fnLineToY`と同じ式を使用(表示側と当たり判定側の対応がズレないように)。

つまみ(現在の表示位置)の上をクリックした場合はマークジャンプより優先してつまみ
ドラッグさせる(`HitTest`冒頭で`xyThumbTop`/`xyThumbBottom`範囲内なら即`false`を
返す)。これが無いと、たまたまマークがつまみの位置に重なった時にスクロールバーを
つまんで動かせなくなってしまう。

対象: `sakura_core/view/CEditView.h`(`ScrBarMarker::HitTest`,
`CEditView::_SB_Marker_HitTestAndJump`), `sakura_core/view/CEditView.cpp`
(`ScrBarMarker::HitTest`実装, `_SB_Marker_HitTestAndJump`実装),
`sakura_core/view/CEditView_Scroll.cpp`(`VScrollBarWndProc`)

サブオプション: `NKMM_SCRBAR_MARKER_CLICK_JUMP`(`VScrollBarWndProc`のサブクラス化
`NKMM_SCRBAR_SYSTEM_STYLE`を前提とするため、そちらが無効な場合は動作しない)

## ホバー中もマーク位置が見えるように再描画を追加 20260810

従来は`WM_MOUSELEAVE`(ホバー解除)時のみマーカーを再描画しており、ホバー中は
Explorerテーマのフェードアニメーションでマークが消えたままになっていた
(クリックジャンプ機能を使うにも、マークがどこにあるか見えないと押しにくい)。

`WM_MOUSEMOVE`のたびに毎回再描画すると呼び出し過多になるため、`SetProp`/`GetProp`
でスクロールバーHWNDにホバー中フラグを持たせ、「非ホバー→ホバー」の遷移が起きた
最初の1回だけ`SB_Marker_CallPaint()`を呼ぶようにした。`Draw()`側は元々実行中の
再描画要求を`bRestartRequestDrawThread_`に集約するデバウンス機構を持つため、仮に
高頻度で呼んでも多重実行にはならないが、遷移時のみに絞ることでロック取得等の
無駄な呼び出し自体を減らしている。

実機確認したところ、Explorerテーマのフェードアニメーションはホバー開始から数百ms
かけて複数フレームに渡り自前描画を続けており、遷移時に1回再描画するだけでは
アニメーション後半のフレームでマークが再び消されてしまっていた。ホバー中
(`WM_MOUSEMOVE`〜`WM_MOUSELEAVE`)は`SetTimer()`で50ms間隔のタイマーを回し、
`WM_MOUSELEAVE`/`WM_DESTROY`で`KillTimer()`するように変更。高頻度に呼んでも
`Draw()`側のデバウンス機構(既述)により多重実行にはならないため安全。

対象: `sakura_core/view/CEditView_Scroll.cpp`(`VScrollBarWndProc`)

サブオプション: `NKMM_SCRBAR_MARKER_HOVER_REDRAW`(同じく`NKMM_SCRBAR_SYSTEM_STYLE`の
サブクラス化が前提)

## マーク描画を不透明な塗り潰しから半透明合成(AlphaBlend)に変更 20260810

「ミニマップのような半透明のつまみにマークを重ねたい」という要望に対し、つまみを
自前で再描画する(範囲判定・線の集合での再構築等)よりも簡単な方法として、GDIの
`AlphaBlend()`で合成するだけにした。`AlphaBlend()`は呼び出し時点で画面に既に
描かれている内容(つまみでもトラックでも)と自動的に合成してくれるため、つまみの
位置を判定したり自前で再描画したりする必要が無い。`CGraphics::AlphaBlendMyRect()`
を新設し、検索/ブックマーク/カーソル行のマーク描画を`FillSolidMyRect()`から
差し替えた。不透明度は`NKMM_SCRBAR_MARK_ALPHA`(既定220/255)。

対象: `sakura_core/uiparts/CGraphics.h` / `.cpp`(`AlphaBlendMyRect`),
`sakura_core/view/CEditView.cpp`(`DrawWorkCallback`)

## ホバー中のタイマー再描画でCPU使用率が跳ね上がる不具合を修正 20260810

実機確認したところ、`AlphaBlendMyRect`呼び出しごとのGDIオブジェクト生成
(`CGraphics`インスタンス生存中は使い回すよう修正済み、下記)を疑ったがそれだけでは
解消せず、真因は`ScrBarMarker::Draw()`内の`SetScrollInfo(..., TRUE)`だった。
`redraw=TRUE`はネイティブスクロールバーの全面再描画(Explorerテーマのホバー
アニメーション再トリガーを含む)を伴う重い呼び出しで、これを50ms間隔のホバー
タイマーから毎回呼んでいたため、再描画→テーマアニメーション再発火→マーク消失→
再描画...という負荷の高いループになっていた。

`Draw()`に`bUpdateScrollInfo`引数(既定`true`)を追加し、`false`なら
`GetScrollInfo`/`SetScrollInfo(TRUE)`/`GetScrollBarInfo`のブロックを丸ごとスキップ
するように変更(ホバー中は位置・範囲そのものは変化しないため不要)。ホバータイマー
専用の軽量エントリポイント`_SB_Marker_HoverRedraw()`を新設し、`WM_APP_SCRBAR_PAINT`
のメッセージキューも経由せず直接`Draw(false)`を呼ぶ。

あわせて、`AlphaBlendMyRect()`が呼び出しのたびに`CreateCompatibleDC`/
`CreateCompatibleBitmap`していたのを、`CGraphics`インスタンスの生存期間中1x1メモリ
DC/ビットマップを使い回す方式に変更(こちらも副次的な負荷要因)。

対象: `sakura_core/view/CEditView.h` / `.cpp`(`ScrBarMarker::Draw`,
`CEditView::_SB_Marker_HoverRedraw`), `sakura_core/view/CEditView_Scroll.cpp`
(`VScrollBarWndProc`), `sakura_core/uiparts/CGraphics.h` / `.cpp`
(`AlphaBlendMyRect`)

## 上記の対策でもホバー中のCPU使用率が高いままだった問題への追加対策 20260810

`SetScrollInfo(TRUE)`を止めた後も改善しなかったため再調査。`DrawWorkCallback()`が
呼ばれるたびに、色設定を`RegKey(NKMM_REGKEY).get_s()`でレジストリから都度読んで
いた(1回の`get_s()`で`RegOpenKeyEx`+`RegQueryValueEx`×2+`RegCloseKey`の計4回、3色で
12回のレジストリアクセス)ことと、ホバー中はアニメーションが収まった後も
`WM_MOUSELEAVE`までタイマーを回し続けていた(アニメーションは実際には数百msで
収まるはずが、ホバーしている間ずっと50ms間隔で`DrawWorkCallback()`のフル
パイプライン=OpenMPスキャン・`GetDC`/`ReleaseDC`・スレッドプール投入等を再実行して
いた)ことの2点を是正。

色キャッシュ(`clrSearchCache_`/`clrMarkCache_`/`clrCursorCache_`)を追加し、
コンストラクタと`BuildWorkCallback()`(実際にドキュメントが変化した時だけ走る経路)
でのみレジストリを読み直すように変更。`DrawWorkCallback()`はキャッシュを読むだけに
なった。

ホバー再描画タイマーに上限Tick数(`NKMM_SB_HOVER_REDRAW_MAX_TICKS`、既定8回=400ms)
を設け、バーストが終わったら(ホバーし続けていても)自動的に`KillTimer()`するように
変更。

対象: `sakura_core/view/CEditView.h` / `.cpp`(`ScrBarMarker::RefreshColorCache`,
`clrSearchCache_`/`clrMarkCache_`/`clrCursorCache_`), `sakura_core/view/
CEditView_Scroll.cpp`(`VScrollBarWndProc`)

## 動作確認について

いずれも実機での確認済み(コメント内に記載の通り、各修正は実機で発生現象を確認した
上で対応し、修正後の解消も実機で確認している)。自動テストは無し。
