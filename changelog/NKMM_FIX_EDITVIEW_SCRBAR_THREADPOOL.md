# スクロールバーマーカーのキャッシュ作成/描画をスレッドプール化 20260810

対象フラグ: `NKMM_FIX_EDITVIEW_SCRBAR`(既存)配下の実装変更。新規フラグは追加していない。

対象ファイル:
- `sakura_core/view/CEditView.h`(`CEditView::ScrBarMarker`)
- `sakura_core/view/CEditView.cpp`(`ScrBarMarker::Build()`/`Draw()`/`WaitForBuild()`/
  `WaitForDraw()`/コンストラクタ・デストラクタ、および旧`SB_Marker_BuildThread`/
  `SB_Marker_DrawThread`を移植した`BuildWorkCallback`/`DrawWorkCallback`)

---

## 経緯

`NKMM_FIX_SCRBAR_MARKER_REPLACEALL_PERF.md`の調査で、`ScrBarMarker::Build()`が
キャッシュ再構築のたびに`_beginthreadex`でOSスレッドを新規生成し、起動直後に
`::Sleep(10)`で呼び出し元(UIスレッド)を無条件に最低10msブロックしていることが
分かっていた。当時の対策(描画抑制中は新規スレッドを起動しない、通常編集は
デバウンスタイマーで再構築頻度を落とす)により「編集1回につき必ず走る」状況は
解消済みだが、`Sleep(10)`とスレッド生成のコストそのものは`Build()`に残ったまま
だった。デバウンス発火のたびや、巨大ファイルで再構築が終わる前に次の要求が
来た場合など、依然としてこの経路を通る余地がある。

「編集のたびにスレッドを使い捨てるのではなく、常駐させてイベントで起こす
方式にすればどうか」という案を検討した結果、自前でスレッドを常駐させると
以下の懸念があることが分かった。

- `CEditView`(分割ウィンドウ/タブ)ごとに`ScrBarMarker`が1個あるため、
  常駐スレッドもビュー数だけ増える。`NKMM_FIX_SCRBAR_MARKER_REPLACEALL_PERF.md`
  追記6で「開いているウィンドウ数に応じて遅くなる未解決の現象」が報告されて
  おり、スレッド数の増加がそれと無関係か確認が要る。
- 起動・終了・再構築要求の取りこぼし防止を自前のイベント/フラグで組む必要が
  あり、実装・デバッグのコストが小さくない。

代わりにWindowsのスレッドプールAPI(`CreateThreadpoolWork`/
`SubmitThreadpoolWork`/`WaitForThreadpoolWorkCallbacks`/`CloseThreadpoolWork`)
に置き換えることにした。プロセス既定のスレッドプールを使うため、ビュー数が
増えてもスレッド数はOS側が適切にリサイクルし、ビューごとに専用スレッドが
常駐する事態を避けられる。

## 変更内容

### メンバの置き換え

`HANDLE hBuildThread_` / `bool bBuildThreadRunning_` / `HANDLE hDrawThread_` /
`bool bDrawThreadRunning_`等を、`PTP_WORK pBuildWork_` / `PTP_WORK pDrawWork_`
と`std::atomic<bool>`版のフラグ群に置き換えた。`PTP_WORK`は`ScrBarMarker`の
コンストラクタで1回だけ`CreateThreadpoolWork()`により作成し、デストラクタで
`CloseThreadpoolWork()`する。以降の再構築/再描画要求は同じワークオブジェクトへ
`SubmitThreadpoolWork()`するだけで、スレッドの生成・破棄は発生しない。

```cpp
// コンストラクタ
pBuildWork_ = ::CreateThreadpoolWork(&ScrBarMarker::BuildWorkCallback, pEditView_, nullptr);
pDrawWork_  = ::CreateThreadpoolWork(&ScrBarMarker::DrawWorkCallback,  pEditView_, nullptr);
```

`Build()`側のスレッド起動部分は次のようになった。`::Sleep(10)`は完全に削除した
(常駐ワーカーへ投入するだけなので、起動を待つ理由がそもそも無い)。

```cpp
if (vLines_.empty()) {
    bBuildThreadRunning_ = true;
    ::SubmitThreadpoolWork(pBuildWork_);
    SB_Marker_Trace(L"ScrBarMarker::Build (%d) start: %d", foo, vLines_.size());
}
```

### 再構築要求の取りこぼし防止(`bRebuildPending_`)

旧実装は`bCacheClear`時に`WaitForBuild(true)`で実行中のビルドを強制中断・
UIスレッドをブロックして待ってから作り直していた。常駐ワーカーへの投入方式
ではこのブロックは不要と判断し、実行中なら`bRebuildPending_`を立てるだけに
した。

```cpp
if (bBuildThreadRunning_) {
    if (bCacheClear) bRebuildPending_ = true;
    SB_Marker_Trace(L"ScrBarMarker::Build (%d) create wait...", foo);
    return;  // UIスレッドはブロックしない
}
```

`BuildWorkCallback`側は、1回のコールバック起動内でdo-whileにより複数パス
回せるようにした。1回のスキャンが終わった時点で`bRebuildPending_`が立って
いれば、最新のドキュメント状態でもう一度スキャンし直す。

```cpp
do {
    // ...ドキュメント全行スキャン(旧SB_Marker_BuildThread本体と同じロジック)...
} while (rSBMarker.bRebuildPending_.exchange(false));
```

「編集のたびに新規スレッドを起動する」旧実装と違い、ここでは常駐ワーカーが
ループするだけなので、複数の要求を1パスにまとめても速度面の不利益はない
(むしろスレッド生成回数が減る分有利)。

### WaitForBuild/WaitForDraw

`WaitForSingleObject(handle, INFINITE)` + `CloseHandle`を
`WaitForThreadpoolWorkCallbacks(work, fCancelPendingCallbacks)`に置き換えた。
`abort=true`の場合は未開始分をキャンセルしつつ、実行中のコールバックについては
(旧実装同様)中断フラグを見て自ら抜けるのを待つ点は変わらない。

```cpp
void CEditView::ScrBarMarker::WaitForBuild(bool abort)
{
    if (bBuildThreadRunning_) {
        if (abort) bExitRequestBuildThread_ = true;
        ::WaitForThreadpoolWorkCallbacks(pBuildWork_, abort ? TRUE : FALSE);
        bExitRequestBuildThread_ = false;
    }
}
```

外部(`CDlgFind.cpp`・`CEditView_Command.cpp`・`CEditView_Command_New.cpp`・
`CEditView.cpp`のWM_APP_SCRBAR_PAINTハンドラ)から呼ばれている
`SBMarker_->WaitForBuild()`/`WaitForDraw()`/`bBuildThreadRunning_`の
シグネチャ・意味はそのまま維持しているため、呼び出し側の変更は不要だった。

### 変更していないもの

- `m_CurRegexp`共有競合(`NKMM_FIX_SCRBAR_MARKER_REPLACEALL_PERF.md`参照)への
  対策(`CSuppressSrchKeyMarkForReplaceAll`、`ChangeCurRegexp()`前後の
  `WaitForBuild(true)`等)はスレッドの起動方式と無関係な問題のため、そのまま
  維持している。
- `SB_Marker_DrawThread`側の描画ロジック(OpenMP並列化、同一Y座標への重複
  描画スキップ、`bRestartRequestDrawThread_`による`goto start_thread`の
  やり直しループ)は、シグネチャと入出力(`_beginthreadex`の`void*`引数→
  `PTP_WORK`コールバックの`void* pv`)以外は変更していない。

## 動作確認について

`msbuild /t:sakura:ClCompile /p:SelectedFiles=..\sakura_core\view\CEditView.cpp`
によるファイル単位ビルドで、Release×Win32/x64ともに0エラー・0警告を確認済み
(Win32側はStdAfx.cppの再コンパイルも走ったが、そちらの警告はいずれも本変更と
無関係な既存のもの: `libs/silica`のsigned/unsigned比較警告、
`Funccode_enum.h`の文字コード警告)。Debug構成は既知の制約(`CStrictInteger`
関連の型変換エラー、本変更以前から存在)によりビルドが通らない。

実機での動作確認(スレッドプールへの投入が正しく行われるか、再構築要求の
取りこぼしが実際に起きないか、ウィンドウ多数オープン時にスレッド数が
増えなくなったかを含む)はまだ行っていない。特に以下は要注意:

- `CreateThreadpoolWork()`が失敗した場合(メモリ不足等)のエラーハンドリングは
  していない。`pBuildWork_`/`pDrawWork_`が`nullptr`のまま`SubmitThreadpoolWork()`
  に渡るとクラッシュする。旧実装の`_beginthreadex`失敗時も同様に未対応だった
  ため、既存と同水準のまま据え置いている。
- `bRebuildPending_`によるコアレッシング(取りこぼし防止)ロジックは新規実装
  であり、旧実装には無かった経路。実機での連続編集・巨大ファイルでの
  再構築中の追加要求パターンでの検証が要る。

## 追記: macro_bench/BenchmarkRegex.qjsによる回帰確認 20260810

フルビルド(Release x64、リンクまで含む)した`sakura.exe`に対し、
`macro_bench/BenchmarkRegex.qjs`(本変更の発端になった調査で使われた
ベンチマークそのもの)を実行して、`ReplaceAll`のタイミングにハング・
クラッシュ・大幅な悪化が無いか確認した。`InfoMsg`はモーダルダイアログで
自動化と相性が悪いため、出力だけファイル書き込みに変更した非対話版
(ロジックは無変更)を`-M=`オプションで実行した。

比較のため、本変更(`CEditView.h`/`CEditView.cpp`/`my_config.h`)を
`git stash`で退避したビルドでも同じベンチマークを実行し、結果を突き合わせた。

- タイミング: 本変更後 Simple total=3188ms/avg=637.60ms、Alt
  total=5690ms/avg=1138.00ms。退避前(旧コード) Simple
  total=2915ms/avg=583.00ms、Alt total=5649ms/avg=1129.80ms。
  各1回の実行なのでノイズの範囲内とみられるが、明確な高速化は確認できて
  いない(そもそも`Command_REPLACE_ALL`のループ中は`GetDrawSwitch()==false`
  の早期returnで`ScrBarMarker::Build()`の大部分がスキップされる経路のため、
  この特定のベンチマークは今回の変更の効果を測るのに適していない)。

- **重大な副産物**: 保存した結果ドキュメントを確認したところ、`ReplaceAll`後に
  一部の行で部分文字列が重複挿入されているのを発見した。`git stash`による
  A/B比較で**本変更(ScrBarMarkerのスレッドプール化)とは完全に無関係**
  (前後でバイト単位で一致)であることを確認済み。原因は
  `CRegexFallback.cpp`(PCRE2フォールバックの置換実装)側の既存の不具合で、
  `bregonig.dll`が見つからずPCRE2フォールバックが有効な環境限定と判明した。
  本変更のスコープ外のため、詳細な調査結果は
  `changelog/NKMM_FIX_REGEXP_FALLBACK.md`の「追記: ReplaceAllの前置文字列
  二重挿入バグ(PCRE2フォールバック限定)」として切り出した。

## 追記: bRebuildPending_取りこぼしレースの修正(mutex導入) 20260810

対象ファイル: `sakura_core/view/CEditView.h`(`mtxBuildState_`追加)、
`sakura_core/view/CEditView.cpp`(`Build()`/`BuildWorkCallback`)。

### 問題

コードレビューで、`bRebuildPending_`によるコアレッシングにTOCTOU(check-then-act)
レースがあることに気付いた。`BuildWorkCallback`終了処理は次の2ステップだった。

```cpp
} while (rSBMarker.bRebuildPending_.exchange(false));  // (A) pending確認
...
rSBMarker.bBuildThreadRunning_ = false;                 // (B) running解除
```

(A)と(B)の間には(短いが)隙間があり、この隙間で`Build()`が呼ばれると

```cpp
if (bBuildThreadRunning_) {          // (A)より後、(B)より前なのでまだtrue
    if (bCacheClear) bRebuildPending_ = true;  // 立てるが…
    return;                           // …もう誰も見に来ない
}
```

となり、要求が黙って失われる。narrow windowだが、`bBuildThreadRunning_`と
`bRebuildPending_`が独立した`std::atomic<bool>`である限り原理的に埋まらない。
最悪の場合、その後の編集がなければスクロールバーマーカーが古いまま固まる。

### 修正

`mtxBuildState_`(`std::mutex`)を新設し、「running確認→pending設定/running設定」
(`Build()`側)と「pending確認→pending消費 or running解除」(`BuildWorkCallback`側)
をそれぞれ同じロックの中で不可分に行うようにした。

```cpp
// Build()
bool bNeedSubmit = false, bNeedDrawRequest = false;
{
    std::lock_guard<std::mutex> lock(mtxBuildState_);
    if (bBuildThreadRunning_) {
        if (bCacheClear) bRebuildPending_ = true;
    } else {
        if (bCacheClear) vLines_.clear();
        if (vLines_.empty()) { bBuildThreadRunning_ = true; bNeedSubmit = true; }
        else bNeedDrawRequest = true;
    }
}
if (bNeedSubmit) ::SubmitThreadpoolWork(pBuildWork_);
else if (bNeedDrawRequest) DrawRequest();
```

```cpp
// BuildWorkCallback (while条件)
} while ([&rSBMarker]() {
    std::lock_guard<std::mutex> lock(rSBMarker.mtxBuildState_);
    if (rSBMarker.bRebuildPending_) {
        rSBMarker.bRebuildPending_ = false;
        return true;   // もう1周
    }
    rSBMarker.bBuildThreadRunning_ = false;
    return false;
}());
```

`SubmitThreadpoolWork`自体はロックの外で呼ぶ(システムコールをロック保持中に
行わない)。呼び出し頻度が低い(デバウンス発火時・行数変化時のみ)ためロック
競合は問題にならない。`bBuildThreadRunning_`は他所(`CallPaint()`等)から
ロック無しでも読まれる箇所があるため、`std::atomic<bool>`のまま維持している
(そちらは「多少古い値を読んでも実害の無い best-effort な覗き見」であり、
今回問題にしたのはBuild()内の複合操作のみ)。

中断(`bExitRequestBuildThread_`によるループの`break`)経路はこのロックを
経由しない別処理のままにした。中断は`WaitForBuild(true)`がUIスレッドを
ブロックして完了を待つ設計のため、その間はUIスレッドから新たな`Build()`が
呼ばれる余地が無く、レースの心配が無いため。

### 動作確認について(bRebuildPending_修正時点)

`msbuild /t:sakura:ClCompile /p:SelectedFiles=..\sakura_core\view\CEditView.cpp`
によるファイル単位ビルド(Release x64)で0エラー・0警告を確認後、フルビルド
(リンクまで)して`macro_bench/BenchmarkRegex.qjs`相当(Simple/Alt各5回)を
再実行し、結果ドキュメントに壊れが無いこと(1,435,395バイト、修正前と同一)、
タイミングに明確な悪化が無いこと(Simple total=1714ms、Alt total=1540ms、
mutex導入前とほぼ同水準)を確認した。

このテスト自体は`bBuildThreadRunning_==true`のときに`Build()`が割り込む
narrow windowを意図的には作れていない(タイミング依存のため)。今回の修正は
コードレビューで発見したレースを構造的に(ロックにより)閉じるアプローチで
あり、実行時に競合を再現させて確認する形は取っていない。

## 追記: 描画側(bRestartRequestDrawThread_)にも同型のレースを発見・修正 20260810

上記の`bRebuildPending_`修正は「今回のスレッドプール化で新規に追加した
コアレッシング機構」に対する対策だったが、`Draw()`/`DrawWorkCallback`の
`bRestartRequestDrawThread_`/`bDrawThreadRunning_`にも同種のレースが
理屈上存在することに気付いた。こちらは移植元の`SB_Marker_DrawThread`から
ロジックを変更しておらず、今回のスレッドプール化以前から存在した既存コード
だが、「簡単にできるなら直す」との方針で、同じ`mtxDrawState_`パターンを
適用した。

### 問題

`DrawWorkCallback`は描画1パスの間、`bRestartRequestDrawThread_`/
`bExitRequestDrawThread_`を2つの`#pragma omp for`ループの**内側**で
繰り返しチェックしているが、その後に続く「カーソル行」描画ブロックには
このチェックが無く、さらにループ終了後の`if (loop_break == ...)`判定は
ループ中に確定した`loop_break`の値を見るだけで再チェックはしない。この
「最後のチェック」から`bDrawThreadRunning_ = false`(`end_thread:`)までの
間に`Draw()`が呼ばれると、そこで立てた`bRestartRequestDrawThread_`は
誰にも消費されず、直近の再描画要求が失われる可能性があった。

### 修正

`Build()`/`BuildWorkCallback`と同じパターンで`mtxDrawState_`
(`std::mutex`)を新設。`Draw()`側は「running確認→restart設定/running設定」を、
`DrawWorkCallback`側は`loop_break == eLoopBreak_None`(完走)の場合に限り、
`end_thread:`へフォールスルーする直前で「restart確認→pending消費(再描画)
or running解除」を、それぞれ同じロックの中で不可分に行うようにした。

```cpp
// loop_break == eLoopBreak_None(完走)のときだけここに来る。
// eLoopBreak_Abortは既にgoto end_threadでこの区間を素通りしている
// (WaitForDraw(true)がUIスレッドをブロックするためレースの心配が無い)。
{
    std::lock_guard<std::mutex> lock(rSBMarker.mtxDrawState_);
    if (rSBMarker.bRestartRequestDrawThread_) {
        rSBMarker.bRestartRequestDrawThread_ = false;
        goto start_thread;   // 最新状態で描画し直す
    }
    rSBMarker.bDrawThreadRunning_ = false;
}
end_thread:
    ...
```

`Abort`経路(`bExitRequestDrawThread_`による中断)は`goto end_thread;`で
この区間を素通りするため対象外(`WaitForDraw(true)`がUIスレッドをブロック
して待つ設計のため、この経路にはBuild側と同じ理由でレースが起きない)。

### 動作確認について

`msbuild /t:sakura:ClCompile /p:SelectedFiles=..\sakura_core\view\CEditView.cpp`
によるファイル単位ビルド(Release x64)で0エラー・0警告を確認後、フルビルド
(リンクまで)して`macro_bench/BenchmarkRegex.qjs`相当を再実行し、結果
ドキュメントに壊れが無いこと(1,435,395バイト、修正前と同一)、タイミングに
明確な悪化が無いこと(Simple total=1626ms、Alt total=1485ms)を確認した。
`bRebuildPending_`修正時と同様、実行時に競合を再現させて確認する形では
なく、コードレビューで見つけたレースを構造的に閉じるアプローチ。
