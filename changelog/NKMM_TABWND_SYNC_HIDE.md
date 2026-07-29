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
