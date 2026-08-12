# NKMM_TABWND_SYNC_HIDE 修正レポート

対象フラグ: `NKMM_TABWND_SYNC_HIDE`（新規、`NKMM_FIX_TABWND`のサブオプション）
対象ファイル(主なもの):

- `sakura_core/window/CTabWnd.cpp`（`CTabWnd::ShowHideWindow()`）
- `sakura_core/my_config.h`（フラグ定義、TODOリストの該当項目を対応済みに更新）

---

## 背景

タブ(ウィンドウまとめ表示モード)を切り替えたときに、画面がちらつく（一瞬別のウィンドウが
重なって見える）現象が報告された。`my_config.h`のTODOリストにも
`20150804 タスクバーアイコンのちらつき — CTabWnd::ShowHideWindow()、SendMessageTimeout()と
TabWnd_ActivateFrameWindow()の関係`として既知の課題が残っていた。

## 原因

Sakuraエディタのタブは、実体としては1ファイルにつき1つの独立したトップレベルウィンドウ
(`CEditWnd`)であり、同じ位置に重なって配置されたうちの1つだけを可視にすることで
「タブ切替」を表現している。

タブをクリックした際の実処理は以下の経路をたどる。

```
OnTabLButtonDown/Up → ShowHideWindow(hwnd, TRUE)
  → TabWnd_ActivateFrameWindow(hwnd)   … ShowWindow(SW_SHOW)で新ウィンドウを表示
```

しかし、旧ウィンドウを隠す処理(`HideOtherWindows()`)はこの経路では呼ばれておらず、
別の非同期通知(`TWNT_ADD`/`TWNT_ORDER`、`CAppNodeManager`経由でウィンドウの
`WM_ACTIVATE`をトリガに送られる)が届いたときに初めて実行されていた
(`CTabWnd.cpp`の`OnTabWindowNotify()`内、`case TWNT_ORDER:`など)。

つまり「新ウィンドウを表示 → (メッセージの往復を待って非同期に)旧ウィンドウを隠す」
という順序になっており、このタイムラグの間、新旧2つのフルサイズウィンドウが重なって
存在する瞬間が生じ、それがちらつきとして視認されていたと考えられる。

なお、`AdjustWindowPlacement()`内の`Sleep(10)`(`NKMM_TABWND_FLICKER`、既存)は、
`SetWindowPlacement`で位置復元した直後の描画未完了に対する別種のちらつき対策であり、
本修正が対象にする「新旧ウィンドウの重なり」とは発生源が異なる。両者は独立して存在してよい。

## 対応

`CTabWnd::ShowHideWindow()`の`bDisp == TRUE`分岐で、`TabWnd_ActivateFrameWindow(hwnd)`
呼び出しの直後に`HideOtherWindows(hwnd)`を同期的に呼ぶよう変更した。

```cpp
TabWnd_ActivateFrameWindow( hwnd );

#if defined(NKMM_FIX_TABWND) && NKMM_TABWND_SYNC_HIDE == 1
    // 旧ウィンドウを隠す処理が TWNT_ORDER 通知の往復を待って非同期に行われると、
    // 新旧ウィンドウが重なって見えるちらつきが発生するため、ここで直ちに隠す。
    HideOtherWindows( hwnd );
#endif // NKMM_

m_pShareData->m_sFlags.m_bEditWndChanging = FALSE;
```

`HideOtherWindows(hwndExclude)`は指定ウィンドウ以外の同一グループのウィンドウを
`ShowWindow(SW_HIDE)`するだけの処理で、`TWNT_ADD`/`TWNT_ORDER`ハンドラからも
（別の`hwndExclude`で）呼ばれる冪等な処理のため、二重に実行されても害はない。

## 既知の制限・要検証事項

- `HideOtherWindows()`を非同期通知に頼らず同期化する対応が、連続高速タブ切替時の
  競合（`TWNT_ORDER`が交錯して画面がすべて消える等、`CTabWnd.cpp`のコメントに
  記載のある過去の不具合）を再発させないか、実機での確認は未実施。
- タブのドラッグ移動・ウィンドウの追加削除と組み合わせた場合の回帰確認も未実施。
- `NKMM_TABWND_FLICKER`の`Sleep(10)`は本修正後も不要になったとは限らないため、
  そのまま残してある。無効化する場合は別途実機での比較確認が必要。

## 動作確認について

タブを連続で切り替えた際に新旧ウィンドウが重なって見えるちらつきが解消されることを
確認する想定。ビルド確認（`CTabWnd.cpp`単体のコンパイル成功）のみ実施し、実機での
動作確認は未実施。

---

## 追記: ウィンドウまとめモード切替時の白フラッシュ調査(全8ラウンド) 20260807

対象フラグ: `NKMM_TABWND_FLICKER`(既存)。対象ファイル: 主に
`sakura_core/window/CTabWnd.cpp`(`AdjustWindowPlacement()`, `ShowHideWindow()`,
`TabWndProc`)、`sakura_core/util/window.cpp`(`ActivateFrameWindow()`,
`HideOtherGroupWindows()`)。

「タブ切替時にタイトルバー/メニュー/タブ帯が一瞬白くなる」という、上記の
`NKMM_TABWND_SYNC_HIDE`(新旧ウィンドウの重なり)とは別種のちらつきについて、
ユーザー報告を受けて8ラウンドにわたり原因を絞り込んだ記録。最終的に未解決の
1点を除き解消した。

### ラウンド1: UpdateWindow()の非同期範囲

`UpdateWindow(hwnd)`は`hwnd`自身しか同期再描画せず、エディットビュー等の子
ウィンドウの`WM_PAINT`や、タイトルバーの`WM_NCPAINT`がキューに残るため、
可視化直後の一瞬だけ中身が描画されない・タイトルバーが白くなる問題があった。
`RedrawWindow(..., RDW_ALLCHILDREN | RDW_FRAME)`で子ウィンドウと非クライアント
領域も同期再描画するよう修正。

### ラウンド2: DWM遷移アニメーション

ラウンド1だけではタイトルバーの白フラッシュが確率的に残った(DWM合成側の遷移
アニメーションが原因で、アプリ側の同期描画だけでは防げない)。切り替え中だけ
`DwmSetWindowAttribute(DWMWA_TRANSITIONS_FORCEDISABLED, TRUE)`でDWMの遷移
アニメーションを止めるよう追加修正。

### ラウンド3: 再レイアウトの抑止

それでもタイトルバー/メニュー/タブが毎回ちらつく(確率的でなく再現性あり)と
報告があり、`CEditWnd::OnSize2()`の再レイアウトを`WM_SETREDRAW`で抑止する
修正もあわせて実施。

### ラウンド4: TabCtrl_SetCurSelの2段階選択が真因と判明

上記でも直らず、ログを仕込んで実際のメッセージ列を採取したところ、切替の
たびに`AddEditWndList()`(`CAppNodeManager.cpp`)がMRU(最近アクティブ)
リストの並べ替えとして`TWNT_ORDER`をグループ全員へブロードキャストしており、
`CTabWnd::TabWindowNotify()`の`TWNT_ORDER`ハンドラが
`TabCtrl_SetCurSel(m_hwndTab,nScrollPos)`→`TabCtrl_SetCurSel(m_hwndTab,nIndex)`
と選択状態を2段階で同期変更していたのが真因と判明した(スクロール位置を
強制的にリセットしてから目的タブを選択するための実装だが、その間の
「誤った選択状態」が毎回一瞬そのまま画面に出ていた)。この2段階の選択変更を
`WM_SETREDRAW`で挟んで抑止し、最後に一度だけ`RedrawWindow()`で描き直すよう
修正した。タイトルバー/メニューのちらつきは、可視化に伴う一連の処理の体感
速度が上がったことで別要因(DWM等)が目立たなくなった。

### ラウンド5: Sleep(10)からDwmFlush()へ

残っているごく短時間(1〜2フレーム程度)の空白は、`RedrawWindow(RDW_UPDATENOW)`
がGDI描画をアプリ側で完了させるだけで、DWMがそれを実際に画面へ合成・提示
するのを待たないことが一因。直後の`Sleep(10)`は「間に合うだろう」という
時間当てずっぽうでしかないため、DWMが次のフレームの合成・提示を終えるまで
実際に待つ`DwmFlush()`に置き換えた(`DwmFlush()`が使えない環境向けに
`Sleep(10)`はフォールバックとして残す)。

### ラウンド6: Ctrl+Tab切替はActivateFrameWindow()という別経路

`DwmFlush()`に変えても体感が変わらないと報告があり、ログにタイムスタンプを
再度仕込んで確認したところ、マウスでタブをクリックした場合は
`CTabWnd::ShowHideWindow()`経由で上記の修正が効くが、Ctrl+Tab等
(`F_NEXTWINDOW`/`F_PREVWINDOW`)によるタブ切替は`CControlTray::ActiveNextWindow`/
`ActivePrevWindow()` → `util/window.cpp`の`ActivateFrameWindow()`という
別経路を通ることが判明した。この関数は`ShowHideWindow()`と違って新ウィンドウ
表示後に旧ウィンドウを同期的に隠す処理が無く、`AddEditWndList()`発の
`TWNT_ORDER`通知の非同期往復待ちのままだった(`NKMM_TABWND_SYNC_HIDE`が
2026-07-29に対応したのは`ShowHideWindow()`側だけで、`ActivateFrameWindow()`は
未対応だった)。`ActivateFrameWindow()`にも同じ同期非表示処理
(`HideOtherGroupWindows()`、`util/window.cpp`)を追加した。

### ラウンド7: WM_ERASEBKGNDの二度塗り

あわせて、タブ項目は`TCS_OWNERDRAWFIXED`で`WM_DRAWITEM`が毎回全面を塗り
つぶすため不要なはずのデフォルト`WM_ERASEBKGND`(erase→redrawの二度塗り)を
`CTabWnd`用のサブクラスプロシージャ(`TabWndProc`)で無効化した。

### ラウンド8: 【未解決】Ctrl+Tab切替特有の一瞬の空白、および影の濃淡変化

上記修正後もログ上は`ShowHideWindow()`/`TWNT_ORDER`関連の処理が一切発生して
いない(`AddEditWndList()`は`WM_ACTIVATEAPP`からのみ呼ばれ、同一デスクトップ
内のプロセス間フォーカス移動でも今回のテストでは発火しなかった)にもかかわらず、
可視化から約100〜150ms後の1〜2フレーム(約15〜30ms)だけタブ帯が一瞬空白/欠落
するちらつきが高速連写キャプチャで再現し続けている。マウスクリックでの切替
(`TWNT_ORDER`経由)よりこちらの方が症状が大きい。`RedrawWindow`/`DwmFlush`/
`WM_ERASEBKGND`抑止のいずれでも解消せず、原因はまだ特定できていない。

ユーザーより「白くなるのと同時にウィンドウの影が濃くなる」との報告があった。
影の濃淡はDWMの非クライアント領域のアクティブ/非アクティブ描画
(`WM_NCACTIVATE`相当)に連動するため、白フラッシュは`SetForegroundWindow()`
によるアクティブ化のタイミングで起きている可能性が高いと判明した。ところが、
これまでの`DwmSetWindowAttribute(DWMWA_TRANSITIONS_FORCEDISABLED)`は
`CTabWnd::AdjustWindowPlacement()`内だけで有効→無効を完結させており、
`SetForegroundWindow()`(呼び出し元の`ShowHideWindow()`/`ActivateFrameWindow()`側で、
`AdjustWindowPlacement()`の後に呼ばれる)の時点では既に遷移アニメーションが
元に戻ってしまっていた＝保護範囲から漏れていたことが真因の可能性が高い。
DWM無効化・最終`RedrawWindow`・`DwmFlush`による合成待ちを
`AdjustWindowPlacement()`単体から`ShowHideWindow()`/`ActivateFrameWindow()`側に
引き上げ、「可視化開始～`SetForegroundWindow()`によるアクティブ化完了」までを
一括で保護するよう修正した。要追加調査として残っている項目は上記の
「Ctrl+Tab切替特有の一瞬の空白」のみ。
